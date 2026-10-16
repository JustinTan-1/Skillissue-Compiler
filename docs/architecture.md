# SkillIssue Compiler Architecture

`src/main.cpp` handles files and options. The reusable `compile` API accepts an owned source name/text pair and returns owned IR and optional inspection strings. Compilation performs lexing, parsing, semantic checking, optional folding, and code generation in that order.

## Frontend

`lexer.cpp` returns tokens with a kind, source span, and borrowed lexeme. Those views refer to `Source::text`; the source outlives token use inside `compile`. The AST copies variable names and owns child expressions/statements with `std::unique_ptr`.

The public tokenizer rejects rvalue `Source` objects using a deleted overload. This prevents calls such as `tokenize(Source{...})` from returning views into an already-destroyed temporary. Compile-time tests cover mutable/const lvalues and temporaries. Callers must still keep their source alive and leave its text unchanged while using tokens. The higher-level `compile` API accepts temporaries safely because it returns owned strings.

`parser.cpp` uses one expression layer per precedence group. Binary expressions grow leftward, preserving left associativity. Prefix operators and parenthesized expressions recurse. Separate depth guards and an AST-depth check stop pathological inputs before recursive traversal becomes unsafe. The minimum signed integer literal is recognized under unary minus before converting its magnitude to a signed value.

`semantic.cpp` keeps a stack of scopes and a vector of stable symbol IDs. It checks declarations before inserting them, resolves every name, annotates expression types, and enforces the final top-level return rule. Sibling scopes can reuse a name while active outer names cannot be shadowed. Code generation receives a fully resolved, typed tree.

## Optimization

`constant_fold.cpp` evaluates literal-only subexpressions. Unsigned arithmetic and `std::bit_cast` provide defined wrapping semantics in C++. A division that would fault remains in the tree; folding does not turn it into a compiler error or remove its source location. The typed AST dump is taken before folding so inspection can compare source structure with optimized IR.

This is SkillIssue's own optimization. Clang's later optimization may perform equivalent folding. Reduced IR operations demonstrate the pass directly; runtime speedups require separate evidence.

## LLVM IR generation

`codegen.cpp` emits text through a small typed generator. Each symbol receives entry-block stack storage, while its initializer executes where the declaration appears. Temporaries and labels have unique names. The generator tracks whether a block has a terminator, catching incorrect instruction placement before writing IR.

Conditionals use then/else/merge blocks. Loops use condition/body/exit blocks. Logical AND/OR branch around their right operand and merge the boolean result using a phi node. Nested logical expressions can introduce intermediate blocks, so the generator captures the actual predecessor after emitting each operand.

The IR uses `i64` for integers, `i1` for booleans, opaque `ptr` for storage, and `i32 @main()` for the native entry point. Arithmetic has no LLVM overflow assumptions. Checked division and exit validation call support functions with the original expression's source location. Filenames are escaped as LLVM byte strings.

## Native execution and verification

`runtime/skill_runtime.cpp` provides four C-linkage functions for integer/boolean printing, checked division, and validated exit status. Boolean arguments are widened to `i32`; signed integers use `i64`. Clang links these functions with the emitted module using the host's ABI and runtime libraries.

The compiler has no LLVM library dependency and does not launch external processes. CMake integration tests invoke the compiler, Clang, and native programs as separate commands. Four suites compare the same observable results across SkillIssue folding settings and Clang optimization levels; Clang also validates every emitted module. Unit and CLI tests cover invalid programs and frontend behavior.

The mutation runner exercises compilation and diagnostics under AddressSanitizer and UndefinedBehaviorSanitizer. It uses a fixed random seed, a sorted input corpus, byte/keyword mutations, and a saved current input. Its harness compiles both folding modes and catches only expected `CompileError` exceptions. It never executes generated programs. This is deterministic mutation testing; the separate libFuzzer entry point is available for coverage-guided testing with a compatible toolchain.
