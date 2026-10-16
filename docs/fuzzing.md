# SkillIssue Fuzz Testing

The validated Windows profile uses deterministic input mutations with AddressSanitizer and UndefinedBehaviorSanitizer. Its purpose is to find compiler crashes, invalid memory accesses, unexpected exceptions, and failures while formatting errors. Both constant-folding modes, token/AST inspection, and diagnostic formatting are exercised for each input.

## Run a bounded campaign

```powershell
.\scripts\fuzz.ps1 -Runs 25000 -Seconds 30 -Seed 1337
```

The helper builds a separate instrumented executable, copies the current example programs into `build/fuzz/corpus`, and adds seeds for excessive nesting, excessive source size, and missing closing syntax. The dictionary contains keywords, operators, and integer boundaries. Existing corpus files are preserved.

The C++ runner tests the seeds, then edits source using byte changes, insertion, deletion, duplication, and dictionary replacement. Each candidate is limited to 65,537 bytes so it can test the compiler's 64 KiB boundary. It stops at the first reached run/time limit. The reported count is the number of mutations actually tested, not the requested upper bound.

`build/fuzz/last-run.log` records the result. `build/fuzz/artifacts/current-input.skill` is saved before each compilation and contains the latest candidate, including after a successful run. On an unexpected exception or sanitizer crash, it retains the failing input. These generated files stay under the ignored build directory.

## Reproduce a candidate

With the same corpus and dictionary, a fixed seed reproduces the mutation sequence. To replay a known prefix, choose its run count and allow enough time:

```powershell
.\scripts\fuzz.ps1 -Runs 5044 -Seconds 120 -Seed 1337
```

You can also replay a saved candidate directly through the instrumented harness:

```powershell
New-Item -ItemType Directory -Force build/fuzz/replay | Out-Null
Copy-Item build/fuzz/artifacts/current-input.skill build/fuzz/replay/candidate.skill
$env:Path = '.\.tools\llvm\lib\clang\22\lib\windows;' + $env:Path
.\build\fuzz\mutation_fuzz.exe build/fuzz/replay build/fuzz/replay-artifacts fuzz/skillissue.dict 1 10 1337
```

The replay runner exercises the unmodified seed before applying its one mutation. An ordinary compiler diagnostic is an expected result, so it is caught rather than reported as a fuzz failure. The normal compiler can display that diagnostic using `skillc <candidate> --serious`.

The PowerShell helpers restore their previous process PATH after making sanitizer DLLs available. The manual replay command above changes PATH only for the current terminal; use the toolchain's reported `-print-runtime-dir` if its version or location differs.

## Coverage guided harness

`fuzz/compiler_fuzz.cpp` also exports the conventional `LLVMFuzzerTestOneInput` entry point. On a compatible Clang/libFuzzer toolchain it can be linked with the six compiler-library source files using `-fsanitize=fuzzer,address,undefined`, as described in the [LLVM libFuzzer guide](https://llvm.org/docs/LibFuzzer.html). Supply `fuzz/skillissue.dict` as the mutation dictionary.

The coverage-instrumented Windows build used during development stopped at a breakpoint in the compiler's exception-cleanup path when handling a normal invalid program. The normal sanitizer tests and deterministic mutation profile pass that input. Coverage-guided Windows fuzzing is therefore unverified; use the validated mutation runner in this workspace.

## Verified result and limits

The Windows campaign validated nine seeds and 5,044 mutated inputs in a 30-second budget with seed 1337. Both folding modes passed with no sanitizer findings or unexpected exceptions. All 48 compiler unit cases and 104 native test runs also passed separately.

The mutation runner is not coverage-guided and does not execute generated code or compare arbitrary programs against a reference interpreter. It supplements the native correctness tests and does not establish that every possible source program is safe or semantically correct. MSVC STL container annotations are disabled in this portable sanitizer profile, as in the unit sanitizer helper; compiler code and heap accesses remain instrumented.
