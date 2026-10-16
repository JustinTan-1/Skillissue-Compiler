# SkillIssue C++ Compiler Implementation Plan

SkillIssue is a small, statically typed programming language with gaming slang for keywords. Build its compiler in C++20, generate readable LLVM intermediate representation (IR), and use Clang to produce native executables. The finished project should be easy to demonstrate: a funny source file becomes a real program, invalid programs receive useful diagnostics, and one optimization simplifies the generated code.

The engineering work is a compiler frontend and IR generator: lexical analysis, parsing, ownership of a syntax tree, symbol resolution, type checking, control flow, and constant folding. LLVM handles machine-code generation and linking through the existing toolchain. That division gives the project one clear purpose and a manageable first release. [Clang compiler architecture](https://clang.llvm.org/docs/UsersManual.html#terminology).

## First release scope

Support one source file containing one entry point, `mission main() -> int`. Programs can declare mutable variables, perform arithmetic, compare values, branch, loop, print, and return an exit status. Support two types: `int` and `bool`.

| SkillIssue keyword | Meaning |
| --- | --- |
| `mission` | Declare the main entry point |
| `loot` | Declare a variable |
| `grind` | Execute a while loop |
| `sus` | Execute an if branch |
| `nah` | Execute the else branch |
| `yap` | Print an integer or boolean followed by a newline |
| `gg` | Return an exit status |

The first release stays focused on these features. Additional functions, arrays, strings, floating-point values, pointers, classes, an interpreter, and a JIT belong to later versions. There is no simulator or virtual machine to build.

Use `.skill` for source files and `skillc` for the compiler executable. The basic demo is:

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

The executable prints `55` and exits successfully.

## Language rules

Write a short language specification before extending the parser. It should settle the following behavior so the compiler, generated programs, and tests agree.

- Identifiers and keywords are case-sensitive. Use ASCII identifiers, `//` comments, braces for blocks, and semicolons after declarations, assignments, print statements, and returns.
- Declarations require an explicit type and initializer: `loot score: int = 10;`. Variables are mutable. The initializer is checked before the new variable enters scope.
- Blocks have lexical scope. Reject duplicate names and shadowing of an active outer variable in this first release. Reject references after a variable's scope ends.
- Support `+`, `-`, `*`, `/`, unary `-`, numeric comparisons, equality, `!`, `&&`, and `||`. Parentheses override precedence. Arithmetic requires integers; logical operations require booleans; equality requires matching types.
- Conditions must be boolean. There are no implicit conversions between `int` and `bool`. Boolean literals are `true` and `false`.
- Evaluate operands from left to right. `&&` and `||` short-circuit, so their right operand is evaluated only when needed.
- `int` is a signed 64-bit value. Addition, subtraction, multiplication, and negation wrap in two's-complement arithmetic. Signed division truncates toward zero. Division by zero and minimum-integer divided by `-1` produce a runtime error.
- Reject integer literals outside the defined range; explicitly handle the minimum signed integer literal under unary minus. The compiler's constant evaluator must use unsigned arithmetic or another defined method for wrapping operations rather than overflowing a C++ signed integer.
- Require a final `gg` statement at the top level of `main`; nested or earlier `gg` statements are outside this first grammar. Its value must be an integer in the range 0 to 255, checked at runtime when necessary.
- `yap` prints integers as signed decimal text and booleans as `true` or `false`, each followed by a newline.

Define precedence from lowest to highest: `||`, `&&`, equality, ordering comparisons, addition/subtraction, multiplication/division, unary operators, and primary expressions. Binary operators are left-associative. Assignments are statements, not expressions.

## Architecture and C++ design

The compiler pipeline is:

```text
.skill source
    -> tokens with source spans
    -> abstract syntax tree
    -> symbol resolution and type checking
    -> optional constant folding
    -> LLVM IR file
    -> Clang and a small support library
    -> native executable
```

Keep the implementation in four main areas: `lexer`, `parser`, `semantic`, and `codegen`, with shared source locations, AST definitions, and diagnostics. Use a small command-line driver to connect them.

Use `enum class` for token kinds, operators, and types. Give AST nodes clear ownership through `std::unique_ptr`; keep source text alive when tokens refer to it through `std::string_view`. Represent scopes with a stack of symbol tables, and resolve references to stable symbol identifiers before code generation. This makes code generation independent of variable names and demonstrates deliberate C++ lifetime management.

Start with a recursive-descent parser and a small function for each expression-precedence level. Avoid introducing a parser generator or a separate custom intermediate representation for the first release.

Emit LLVM IR as text through a small typed emitter rather than linking the compiler against the LLVM C++ libraries. Track value types, unique temporary names, basic-block names, and whether the current block already has a terminator. Keep output in memory until compilation succeeds, then write the requested file. Set initial limits of 64 KiB of source and nesting depth 128 to make pathological parser inputs manageable.

## LLVM code generation

Map SkillIssue integers to LLVM `i64` and booleans to `i1`. Emit a native `i32 @main()` entry point; validate and narrow the final SkillIssue exit status before returning from it.

Allocate local variable storage in the function's entry block and implement access through loads and stores. Execute each initializer at its declaration site, including inside loops. Entry-block storage simplifies mutable variables and allows LLVM to promote suitable storage to registers later. [LLVM mutable variables tutorial](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl07.html).

Implement `sus` with condition, then, else, and merge blocks. Implement `grind` with condition, body, and exit blocks, returning to the condition after each body execution. Implement short-circuit operators with branches and a result merge, using a small phi node when required. Explain phi nodes as selecting a value according to the branch that reached a block.

For wrapping integer operations, emit ordinary integer arithmetic without `nsw` or `nuw` overflow assumptions. Route signed division through a checked support function initially. LLVM's raw signed division has undefined behavior for division by zero and signed division overflow, so the language's defined errors must be implemented explicitly. [LLVM integer addition](https://llvm.org/docs/LangRef.html#add-instruction), [LLVM signed division](https://llvm.org/docs/LangRef.html#sdiv-instruction).

A small C++ support file provides functions with C linkage for integer printing, boolean printing, checked division, and exit-status validation. Pass source line and column to checked operations so runtime errors identify the failing expression. Pass boolean print arguments as a widened integer with a documented signature to avoid depending on a C++ boolean ABI detail.

Let Clang parse and validate the emitted IR during integration tests. Add a native build at `-O0` first; use `-O2` later to check that optimizations preserve the defined program behavior. LLVM's object-code tutorial demonstrates the connection between generated code and linked C++ support functions. [LLVM native code and linking tutorial](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl08.html).

## Diagnostics and personality

A diagnostic must identify the file, line, column, actual problem, and source excerpt. Put an optional joke after the technical message:

```text
examples/broken.skill:8:9: error E010: unknown variable 'score'
    yap(score);
        ^^^^^
Skill issue: you haven't unlocked this variable.
```

Cover unknown variables, duplicate declarations, incorrect types, invalid conditions, missing tokens, unsupported syntax, and invalid literals. Report the first syntax error clearly before adding parser recovery. Give semantic errors stable identifiers so tests can check their meaning without depending on the joke.

Compiler failures return a nonzero exit code and do not write a new IR file. Runtime arithmetic failures report a source location to standard error and terminate with a documented failure status. Keep normal `yap` output on standard output.

## Implementation milestones

Budget approximately **60 to 90 focused hours**, or **6 to 9 weeks at 10 hours per week**, assuming comfort with basic C++, containers, RAII, and a debugger. Add learning time if these concepts or LLVM IR are new. Revise the estimates after the parser milestone.

| Milestone | Work | Completion evidence | Hours |
| --- | --- | --- | --- |
| 1 Build foundation | Create CMake targets, a CLI stub, unit tests, and a tiny support library. Verify the host Clang/linker configuration with a handwritten minimal IR program. | A native program built from IR prints `42`. C++ tests run. Toolchain versions are recorded. | 4 to 6 |
| 2 Tokenize the language | Implement tokens, keywords, literals, comments, source spans, and invalid-character handling. | A token dump matches the sample program; malformed characters and oversized literals have accurate locations. | 6 to 8 |
| 3 Parse the language | Implement AST ownership, statements, blocks, operator precedence, and an AST dump. | The sample parses; tests cover precedence, associativity, nesting, and missing punctuation. | 10 to 14 |
| 4 Validate programs | Implement scopes, symbols, type checking, initialization rules, and the main/return restrictions. | Invalid names and types are rejected before code generation; valid programs have resolved symbols and typed expressions. | 8 to 12 |
| 5 Compile straight-line programs | Emit integer/boolean values, local storage, arithmetic, printing, and final exit status. | `yap(42)` and variable/arithmetic programs compile into IR and native executables with correct output. Arithmetic faults report locations. | 14 to 20 |
| 6 Compile control flow | Emit conditionals, loops, nested blocks, and short-circuit boolean expressions. | The sum demo prints `55`; nested control flow works; short-circuit tests avoid unreachable division faults. | 10 to 16 |
| 7 Optimize and finish | Add constant folding, integration checks, documentation, example programs, and a short demo. | Folding changes generated IR while preserving behavior. A fresh checkout builds and reproduces the demo. | 8 to 14 |

Write relevant tests during each milestone. The first native executable appears in milestone 1 to resolve toolchain issues early; the first executable produced by your compiler appears in milestone 5.

## Constant folding and verification

Implement one optimization yourself: evaluate literal-only expressions at compile time. For example, lower `loot score: int = 6 * 7;` directly to a value of `42` instead of a multiplication instruction.

Preserve wrapping integer behavior, source locations, evaluation order, and short-circuit semantics. If a literal expression would cause a runtime error, leave it as a checked operation instead of folding it. Avoid algebraic rewrites involving variables in this release.

Use unit tests for tokens, AST structure, symbol resolution, types, diagnostics, and folding. Use CTest integration checks to compile example programs, invoke Clang with argument lists, run the executable with a timeout, and compare output and exit status. The compiler itself only writes IR; invoking an external compiler and running examples stays in the build/test workflow.

Include these meaningful cases:

- `1 + 2 * 3` produces `7`, while `(1 + 2) * 3` produces `9`.
- The sum from 1 to 10 prints `55`; a factorial demo prints `120`.
- Variable assignments in a loop persist across iterations; declarations execute at the correct point.
- A variable declared inside a block cannot be read outside it.
- Boolean conditions, equality, and nested branches preserve the expected output.
- `false && (1 / 0 == 0)` and `true || (1 / 0 == 0)` avoid the division error.
- Signed wrapping at integer boundaries matches the specification.
- Division by zero, division overflow, and an invalid exit status produce the documented runtime errors.
- The same programs produce matching output, status, and error locations with folding enabled/disabled and Clang at `-O0`/`-O2`.
- Missing semicolons, invalid types, source limits, and excessive nesting produce diagnostics instead of crashes.

Add a supported sanitizer configuration for the C++ compiler and tests. A bounded lexer/parser fuzzing target can be added during polish if it fits the remaining time. Keep it optional for the first release.

Show the optimization with IR before and after folding, while holding Clang's optimization level fixed. Describe it as fewer emitted operations unless a controlled benchmark establishes a runtime improvement. LLVM may perform the same folding independently, and constant examples are poor runtime benchmarks.

## Tools and repository

Use C++20, CMake, one test framework such as GoogleTest, and an installed Clang toolchain. The compiler emits text IR and does not need LLVM development libraries. Start with native Windows x64, matching the current workspace. Confirm that Clang can find the Windows SDK, linker, and C/C++ runtime before building language features. Linux support is a useful later portability check.

Pin or record the versions actually used during implementation. Ninja is optional; the CMake generator can match the available toolchain.

Proposed layout inside the current project folder:

```text
CMakeLists.txt
include/skillissue/
src/main.cpp
src/lexer.cpp
src/parser.cpp
src/semantic.cpp
src/codegen.cpp
src/constant_fold.cpp
runtime/skill_runtime.cpp
examples/hello.skill
examples/sum.skill
examples/factorial.skill
examples/branch.skill
examples/broken.skill
tests/
docs/language.md
docs/architecture.md
README.md
IMPLEMENTATION_PLAN.md
```

The intended workflow, once implemented, is:

```text
skillc examples/sum.skill -o sum.ll
clang++ sum.ll runtime/skill_runtime.cpp -o sum.exe
.\sum.exe
```

Useful compiler inspection options are `--dump-tokens`, `--dump-ast`, and `--no-fold`. Keep the compiler CLI focused on compilation and inspection. A one-command native build wrapper can come after the basic two-command workflow works reliably.

## Release demo and interview preparation

Prepare a two-minute demonstration: show the sum program, compile and run it, introduce a type error and show its diagnostic, then show an expression before and after constant folding.

The release is ready when a fresh checkout can reproduce that workflow, the language rules are documented, unit and integration tests pass, and the compiler's ownership and control-flow choices are easy to explain. Keep five small example programs with known output and at least one deliberately invalid example.

Use this interview explanation once the features are complete:

> I built SkillIssue, a small statically typed language with gaming slang for keywords. Its compiler is written in C++ and implements tokenization, parsing, scope resolution, type checking, and LLVM IR generation. It produces native programs using Clang, reports source-level errors, and folds constant expressions. The funny syntax makes the demo memorable, while the compiler has explicit integer and control-flow semantics.

Prepare answers about operator precedence, AST ownership, variable scope, stack storage, basic blocks, short-circuiting, wrapping arithmetic, and the distinction between your folding pass and LLVM optimization. Show one actual bug and the regression test that prevented it from returning.

Describe the project as a C++ compiler frontend with LLVM IR generation. A resume bullet can name those implemented features and mention native executable demos and correctness tests. Add measurements only after collecting them.

After this release, choose one extension: user-defined functions and recursion, an editor syntax-highlighting package, or a second native target. The current release already contains enough material for a focused compiler interview discussion.
