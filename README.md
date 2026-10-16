# SkillIssue


[Language Guide](./docs/language.md)

A small programming language with gaming slang, compiled by a C++20 compiler into LLVM IR and native executables.

```text
mission main() -> int {
    loot total: int = 0;
    loot i: int = 1;
    grind (i <= 10) {
        total = total + i;
        i = i + 1;
    }
    yap(total);
    gg 0;
}
```

This program prints `55`. `sus`/`nah` provide conditionals; `bool` supports comparisons and short-circuit logic. Compiler errors explain the problem and then add a joke.

## Build and run on Windows

The current workspace has Visual Studio 2022 C++ tools and a portable Clang 22.1.8 toolchain in `.tools/llvm`. CMake is discovered through Visual Studio when it is absent from PATH. In PowerShell, from this project directory:

```powershell
.\scripts\build.ps1 -Test
.\scripts\run.ps1 examples/sum.skill
```

The run helper builds the compiler, writes IR under `build/programs`, links it with the small support library, and runs the executable. Try `examples/factorial.skill`, `examples/branch.skill`, or `examples/short_circuit.skill` too.

To build and run the sum demo through CMake:

```powershell
.\scripts\build.ps1 -Demo
```

For a fresh checkout, install Visual Studio 2022's Desktop development with C++ workload (including CMake) and an [official Clang toolchain](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8). Put `clang++` on PATH or pass its location with `-Clang`. `.tools` is ignored by source control; its portable compiler is a development dependency rather than part of SkillIssue.

## Use the compiler directly

```powershell
.\build\Debug\skillc.exe examples/sum.skill -o build/sum.ll
.\build\Debug\skillc.exe examples/sum.skill --dump-tokens --dump-ast -o build/sum.ll
.\build\Debug\skillc.exe examples/sum.skill --no-fold -o build/sum-unfolded.ll
.\build\Debug\skillc.exe examples/broken.skill --serious
& '.\.tools\llvm\bin\clang++.exe' build/sum.ll runtime/skill_runtime.cpp -o build/sum.exe
.\build\sum.exe
```

Without `-o`, the compiler writes a `.ll` file beside the source. `-o -` writes IR to stdout; it cannot be combined with inspection dumps. Dumps show the tokens and typed AST before folding. `--no-fold` disables SkillIssue's optimization; `--serious` suppresses compiler diagnostic jokes.

## Build with CMake on Linux or macOS

With a C++20 compiler, CMake, and `clang++` available:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --build build --target demo
```

The implementation uses standard C++20 and emits target-independent text IR. Native Windows x64 is the verified platform for this workspace. LLVM development libraries and a network connection are unnecessary for building the compiler. If Clang is unavailable, unit/CLI tests still run and native tests/demo are omitted. Pass `-DSKILLISSUE_CLANG=/path/to/clang++` to select the backend.

## What is implemented

- Handwritten lexer and recursive-descent parser with source locations and precedence.
- A syntax tree owned through `std::unique_ptr`, lexical scopes, resolved symbols, and strict `int`/`bool` checking.
- Text LLVM IR generation, entry-block local storage, loops, branches, and phi nodes for short-circuit expressions.
- Constant folding with defined 64-bit wrapping arithmetic and preservation of runtime faults.
- A small native support library for printing, checked signed division, and exit-status validation.
- Source excerpts and error codes, token/AST inspection, a serious diagnostic mode, resource limits, examples, and unit/CLI/native tests.
- A tokenizer API that rejects temporary source objects, plus sanitizer-backed malformed-input mutation tests.

The compiler produces LLVM IR. Clang and the host toolchain generate the machine code and executable. The support library provides a few native functions; it is not an interpreter or VM.

## Tests and sanitizers

The dependency-free unit runner checks lexing, AST precedence, scopes, types, folding, diagnostics, numeric boundaries, and resource limits. CLI tests check paths containing spaces, output preservation on compile errors, source overwrite protection, inspection, and option handling. Four native suites each run 26 programs, with folding on/off and Clang at `-O0`/`-O2`, comparing output, exit status, and runtime errors. When `llvm-as` is available, every module also passes its IR verifier. Every generated program has an execution timeout.

For the portable Windows Clang toolchain, run the compiler unit cases with AddressSanitizer and UndefinedBehaviorSanitizer using:

```powershell
.\scripts\sanitize.ps1
```

This builds a separate test executable and temporarily makes the sanitizer DLL available through the process PATH. MSVC STL container annotations are disabled in this helper so it works without the optional `stl_asan.lib` component; compiler code and heap accesses remain instrumented. Sanitizer findings make the run fail immediately.

For GCC/Clang on supported Unix platforms, configure `-DSKILLISSUE_SANITIZERS=ON` to enable both sanitizers. MSVC requires its C++ AddressSanitizer component and uses AddressSanitizer; build **RelWithDebInfo**, not Debug, to avoid `/RTC` incompatibility:

```powershell
cmake -S . -B build-asan -G 'Visual Studio 17 2022' -A x64 -DSKILLISSUE_SANITIZERS=ON
cmake --build build-asan --config RelWithDebInfo
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
```

The ASan runtime must be discoverable by the test process, typically through an x64 Visual Studio developer environment. A sanitizer run is separate from normal correctness testing.

For malformed-input testing under both sanitizers:

```powershell
.\scripts\fuzz.ps1 -Runs 25000 -Seconds 30 -Seed 1337
```

The Windows helper tests deterministic mutations of example programs, keyword/operator sequences, integer boundaries, and resource-limit seeds. It compiles each input with folding off and on, including inspection and diagnostic formatting. It stops at the run count or time limit and reports the actual count. Logs and the latest input are saved under `build/fuzz`. See [fuzz testing](docs/fuzzing.md) for replay and scope.

Verified in this workspace on native Windows x64: 48 compiler unit cases, the CLI checks, and all 104 native cases passed. Every native module also passed `llvm-as` verification. All 48 unit cases passed the separate Clang AddressSanitizer/UndefinedBehaviorSanitizer profile. A 30-second mutation campaign validated nine seeds and 5,044 mutated inputs with folding off/on and no sanitizer findings. The sum demo printed `55`, and the run helper produced factorial's `120`. Tools used: MSVC 19.44.35222, CMake 3.31.6, and Clang/LLVM 22.1.8.

