# Bootstrap (self-hosting)

This directory contains stages of the Nuru compiler **written in Nuru itself**.
Each stage is a normal Nuru program compiled by the C-compiled `nuruc`.

| File | Stage | What it does |
|------|-------|--------------|
| `lexer.nuru` | 1 | Tokenizes Nuru source into identifier/integer/string/punctuation/newline/EOF tokens. |
| `parser_expr.nuru` | 2a | Recursive-descent expression parser (arithmetic + comparison precedence, `plus`/`minus`/`times`, `is`/`is not`/`is above`/…, list/map/string literals). |
| `parser.nuru` | 2–3 | Statement-level parser + semantic checker (`analyze_program`): parses `make`/`show`/`give`, `create` functions, `when`/`otherwise`, `while`, `for each`, `repeat`, `attempt`/`recover`, `stop`/`skip`, and reports undeclared references. |
| `codegen.nuru` | 4 | Emits x86-64 assembly for a Nuru subset: integer variables (`make`/`change`), arithmetic, `when`/`otherwise`, `while`, `repeat`, and `create f using … { … give … }` function calls, plus a run-time `print_int`. The assembly assembles with `as` and links with `ld`. |

## End-to-end demonstration

`tests/native/run_codegen_e2e.sh` runs the complete self-hosting loop:

1. Compile `bootstrap/codegen.nuru` with `nuruc`.
2. Run it to emit assembly for a sample program (variables + loop + branch).
3. `as` + `ld` the generated assembly into an executable.
4. Run it and check the output.

```sh
make test            # includes test-native, test-ast, test-codegen-e2e
./tests/native/run_codegen_e2e.sh
```

## Notes and constraints discovered while bootstrapping

- Nuru string literals treat `{`/`}` as interpolation; source samples containing braces must escape them (`\{`, `\}`).
- Lists/maps are value-semantics: accumulate state by returning and re-threading accumulator lists, not by mutating passed-in lists.
- `is` on strings is pointer/interning-based; compare runtime-built strings with `str_eq` (ordinal).
- A function call's argument list consumes trailing tokens, so `f with x is 1` parses as `f(x is 1)`; wrap comparison results in parentheses (`(f with x) is 1`) or hoist to a temporary.
- `for each` over a *member-access* iterable (e.g. `for each a in x.args`) does not iterate — the iterable must be a plain variable (`make lst be x.args; for each a in lst`).
- Zero-argument user functions are not callable as expressions yet.
- `repeat <n> times` is handled in the parser via a dedicated count parser (stops at `times` to avoid clashing with multiplication).

## Remaining standing work

- Code generator: function calls, collections (lists/maps), `repeat`, `for each`.
- A module/import mechanism.
- Wider integer/string support (floats, string formatting).
