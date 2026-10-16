# SkillIssue Language

SkillIssue programs contain exactly one `mission main() -> int { ... }`. Identifiers are case-sensitive ASCII letters, digits, and underscores, with a letter or underscore first. `//` comments continue to the next newline. Keywords cannot be variable names. Braces delimit blocks, and declarations, assignments, `yap`, and `gg` require semicolons.

## Statements

```text
loot count: int = 0;
loot ready: bool = true;
count = count + 1;
yap(count);
sus (ready) { yap(1); } nah { yap(0); }
grind (count < 10) { count = count + 1; }
{ loot local: int = 5; yap(local); }
gg 0;
```

Initializers are mandatory. Variables are mutable and visible only in their declaring block and nested blocks. A declaration enters scope after its initializer has been checked. Redeclaration and shadowing of active names are errors; separate sibling blocks can reuse a name. A declaration inside a loop executes its initializer on every iteration even though its storage is allocated once.

Exactly one `gg` is required as the final top-level statement. Nested or early returns, function parameters, and additional missions are unsupported. A valid `gg` value from 0 through 255 becomes the process exit status. Out-of-range values cause runtime error R003. A valid return of 2 shares the same process status as a runtime failure; runtime failures additionally report a diagnostic to stderr.

## Values and operators

`int` is signed 64-bit two's-complement. Decimal literals must fit, including the special unary-minus form `-9223372036854775808`. The positive magnitude `9223372036854775808` is invalid on its own. Addition, subtraction, multiplication, and unary negation wrap modulo 2^64, interpreted as signed. Integer division truncates toward zero, with errors for zero divisors and minimum-integer divided by -1.

`bool` has `true` and `false` values. No implicit conversion exists between integers and booleans. Conditions require booleans; arithmetic and ordering comparisons require integers; equality requires matching types.

From lowest to highest precedence:

| Operators | Meaning |
| --- | --- |
| `\|\|` | Short-circuit OR |
| `&&` | Short-circuit AND |
| `==`, `!=` | Equality |
| `<`, `<=`, `>`, `>=` | Signed ordering |
| `+`, `-` | Addition/subtraction |
| `*`, `/` | Multiplication/division |
| unary `-`, `!` | Negation/logical NOT |
| literals, names, `(expression)` | Primary expressions |

Binary operators associate to the left. Operands evaluate left to right. The right side of `false && expression` or `true || expression` does not execute; it is still parsed and type-checked. Assignments are statements, not expressions.

`yap` prints one integer or boolean followed by a newline. Boolean text is `true` or `false`. Strings, arrays, floats, pointers, and user-defined functions are outside version 0.1.

## Errors and limits

| Code | Meaning |
| --- | --- |
| E001 | Unexpected character or unsupported operator |
| E002 | Integer literal out of range |
| E003 | Source or nesting limit exceeded |
| E004 | Invalid syntax or entry point |
| E010 | Unknown/out-of-scope variable |
| E011 | Duplicate/shadowed declaration |
| E012 | Type mismatch |
| E013 | Invalid or missing final return |
| R001 | Division by zero |
| R002 | Signed division overflow |
| R003 | Exit status outside 0 to 255 |

Compiler errors exit with status 1 before opening an IR output file. Existing output is preserved when source validation fails. Runtime errors identify the source path, line, and column and exit with status 2, flushing earlier printed output. Other CLI or file I/O failures also exit with status 1.

Sources are limited to 64 KiB. Block nesting, parser expression recursion, and expression-tree depth are limited to 128; a long unparenthesized expression can therefore hit the tree-depth limit. These limits bound recursive traversal and destruction. Use UTF-8 text without a BOM; identifiers use the ASCII grammar above. CRLF and LF line endings are accepted. Infinite loops are legal language programs; the integration runner, rather than compiled programs, enforces a test timeout.
