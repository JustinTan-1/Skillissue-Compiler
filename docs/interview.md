# SkillIssue Interview Demo

Explain the project as a C++ compiler frontend that turns a small typed language into LLVM IR and then native executables using Clang. The funny keywords make the syntax memorable; the engineering is in parsing, scope/type validation, ownership, control flow, and preserving the language's integer behavior.

## Two minute demo

1. Show `examples/sum.skill`: `loot` declares, `grind` loops, `yap` prints, and `gg` exits. Run it and show `55`.
2. Use `--dump-ast` to explain precedence, resolved variables, and types.
3. Compile `examples/broken.skill` and show the precise unknown-variable diagnostic. Use `--serious` to show the same technical error without the joke.
4. Compile an expression such as `yap(6 * 7)` with folding on and off. Compare the emitted constant with the multiplication instruction.
5. Show the native tests, including short-circuit division errors and signed wraparound at both optimization levels.

## Questions to prepare

| Question | Concrete explanation |
| --- | --- |
| Why recursive descent? | The grammar is small; precedence layers and explicit ownership make behavior easy to inspect and test. |
| How do you handle scope? | Active scope maps resolve names to stable symbol IDs; declarations are checked before entering scope. |
| Who owns source text and AST nodes? | Source owns text; tokens borrow views during compilation; AST names are copied and children have unique ownership. |
| Why store locals in entry-block slots? | It simplifies mutable variables and loops; initializers still run at their declaration sites. LLVM can promote suitable storage later. |
| How does short-circuiting work? | Branch around the right operand and merge a boolean result with a phi node, recording actual predecessor blocks for nested expressions. |
| What happens on integer overflow? | Addition/subtraction/multiplication wrap by specification. Division errors are checked before performing the operation. |
| What did you implement versus LLVM? | The frontend, type checker, constant folder, and IR generator are yours; LLVM supplies native optimization and machine-code generation. |
| How did you check correctness? | Unit/CLI tests and real native programs, tested with folding on/off and Clang at O0/O2, including matching diagnostics and exit status. |

Discuss actual test results and bugs discovered during development. Do not claim handwritten machine-code generation, measured performance gains, or portability that has not been verified.
