# NURU

NURU is a native compiled programming language with a sentence-like syntax, built from scratch in C. The compiler lowers Nuru source through a full multi-stage pipeline to x86-64 assembly and produces standalone ELF executables (no VM, no runtime dependency beyond the OS).

## Quick start

```sh
make                 # build the compiler (produces ./nuruc)
make test            # run the native regression suite

./nuruc examples/hello/hello.nuru
./examples/hello/hello          # prints: Hello, Nuru!
```

### Usage

```sh
./nuruc [options] <file.nuru>
```

Options:

| Flag | Meaning |
|------|---------|
| `-t, --tokens` | dump the token stream |
| `-a, --ast` | dump the abstract syntax tree |
| `-s, --semantic` | show semantic-analysis result / errors |
| `-i, --ir` | dump the NIR intermediate representation (post-optimization) |
| `-o <file>` | output executable path (default: input path without `.nuru`) |
| `-h, --help` | usage |

Compilation runs the full pipeline and, unless `-i` is given, emits an executable at the output path.

## Language overview

Functions, control flow, and mutable state:

```nuru
create add using a, b {
    give a plus b
}

create main {
    make x changeable 0
    repeat 5 times {
        change x to x plus 1
    }
    when x is at least 5 {
        show "done"
    }
    otherwise {
        show "not yet"
    }
    show add with x, 10
}
```

Data structures and interpolation:

```nuru
create main {
    make cities be ["Kampala", "Jinja"]
    add "Gulu" to cities
    for each city in cities {
        show city
    }

    make person be map [
        name is "Amina",
        city is "Kampala"
    ]
    show "Hello {person.name}"
    show cities[0]        # list indexing: "Kampala"

    show "tab\there\nnewline"

    make name be ask            # read one line from stdin
    show "Hello {name}"

    make i changeable 0
    repeat 5 times {
        change i to i plus 1
        when i is 2 { skip }      # continue
        when i is 4 { stop }      # break
        show i
    }
}
```

See `docs/language/language-spec-v0.3.md` and `docs/language/grammar-spec-v0.3.md` for the full language and grammar.

## Compiler pipeline

```
Nuru source .nuru
   → Lexer          (compiler/lexer)
   → Parser         (compiler/parser)        → AST
   → Semantic       (compiler/semantic)      → validated AST + symbol tables
   → NIR lowering   (compiler/nir)           → stack-machine IR with basic blocks
   → Optimizer      (compiler/optimizer)     → constant folding, jump threading, trimming
   → Code generator  (compiler/nir/nuru_codegen.c) → x86-64 assembly
   → Object/Link    (compiler/object)        → ELF executable (via as + ld)
```

All compiler stages live under `compiler/`:

| Module | Files | Role |
|--------|-------|------|
| `lexer/` | `nuru_lexer.{c,h}` | tokenization, keywords, multi-word operators, string escapes |
| `parser/` | `nuru_parser.{c,h}` | recursive-descent parser, interpolation sub-parsing |
| `ast/` | `nuru_ast.{c,h}` | AST node definitions, constructors, printing |
| `semantic/` | `nuru_semantic.{c,h}` | scope/symbol table, static checks |
| `nir/` | `nuru_nir.{c,h}`, `nuru_irgen.c`, `nuru_codegen.c` | IR, AST→IR lowering, x86-64 codegen |
| `optimizer/` | `nuru_optimizer.{c,h}` | constant folding, jump threading, jump trimming, dead-tail truncation |
| `object/` | `nuru_object.{c,h}` | assembly and link stages (delegates to `as`/`ld`) |
| `driver/` | `nuru.c` | CLI, pipeline orchestration |

## Runtime

The generated programs are self-contained and use only Linux system calls (`write`, `read`, `exit`):

- Values are 64-bit words with an **8-bit type tag in the low byte** and a 56-bit payload (bits 8-63). Integers and booleans carry their value in the payload; strings, lists, and maps carry a pointer.
- Heap objects share a uniform header: `[ref_count][length/count][payload]`. String literals live in `.data` with the same layout and are exempt from reference counting via a literal-region range check.
- Memory is managed by **reference counting** (`_incref`/`_decref`) over a bump+free-list allocator: when an object's count reaches zero it is returned to the free list and its children are released recursively.
- `show` dispatches on the tag byte through `_print_value`; divide-by-zero raises a catchable error for `attempt`/`recover`.

This implements the full tagged-value and reference-counting model of `docs/language/runtime-memory-spec-v0.1.md`.

## Testing

The native regression suite lives in `tests/native/`. Each case is a directory with `program.nuru` and `expected.txt`:

```
tests/native/cases/
├── add/            # functions, parameters, return
├── arithmetic/     # multi-op arithmetic
├── comparisons/    # all comparison operators
├── conditions/     # when/otherwise (true branch)
├── div_mod/        # integer division and remainder
├── else_branch/    # otherwise branch
├── escapes/        # string escape sequences
├── hello/          # string literal
├── interpolation/  # string interpolation
├── list_for_each/  # list literal + append + for-each
├── list_sum/       # integer list arithmetic
├── map_field/      # map literal + member access
├── nested_calls/   # function calling function
├── repeat_loop/    # repeat loop
└── while_loop/     # while loop with multi-word condition
```

Run with `make test` (or `make test-native`). The harness compiles each program, runs the binary, and diffs stdout/exit code against the expected output.

## Status

All documented language features are implemented and covered by native tests: functions, control flow (`when/otherwise`, `repeat`, `while`, `for each`, `stop`/`skip`, `attempt`/`recover`), arithmetic/comparison, mutable state, string interpolation, escapes, lists, maps, member access, append, `ask` (stdin), and the `len` built-in. The full pipeline — lexer through optimizer, codegen, and object/link stages — is wired end-to-end.

Built-in functions: `show`, `ask` (read a line from stdin), `read_file(path)` (read an entire file as a string, "" on error), `write_file(path, content)` (write exact bytes, returns 0 on success / -1 on error), `len` (string/list/map length), `abs`, `min`, `max`, `pow` (integer exponentiation), `to_number` (parse a string to an integer), `to_string` (integer → decimal string, handles negatives), `concat` (concatenate two strings), `substring(s, start, end)` (slice a string, with bounds clamping), `upper` (uppercase copy), `lower` (lowercase copy), `contains(s, sub)` (does `s` contain `sub` — returns 0/1), `index(s, sub)` (first position of `sub` in `s`, or -1), `ord` (first char code of a string), `chr` (integer → one-character string). String comparison (`is` / `is not`) works via literal interning (identical string constants share one descriptor).

The runtime implements the full tagged-value and reference-counting model of `docs/language/runtime-memory-spec-v0.1.md` plus catchable divide-by-zero.

### Self-hosting / bootstrap

Bootstrap is underway in `bootstrap/`. Stage 1, `bootstrap/lexer.nuru`, is a tokenizer for Nuru source written entirely in Nuru (identifiers, integers, strings, punctuation, newlines, EOF). It is covered by the native test `tests/native/cases/lexer`.

Work on bootstrap surfaced and fixed several compiler defects: empty list/map literals (`[]`, `map []`) were rejected by the parser; nested `when`/`otherwise` conditionals produced a broken control-flow graph (inner branches did not merge back to the enclosing continuation); string equality (`is`) was pointer/interning-based, so runtime-built strings never compared equal to literals; and the semantic analyzer rejected forward/mutual function references (now supported via a pre-declaration pass). All are resolved and covered by tests.

To support the later stages, the language gained: list indexing (`list[i]`), expression/call statements (`f with args` as a statement, with the result discarded), and the builtins `to_string`, `read_file`, `write_file`.

A recursive-descent expression-parser prototype (`bootstrap/parser_expr.nuru`) builds on the lexer but currently hits an unresolved codegen bug: functions that combine a `while` loop with a `change x to map [...]` whose entries read member accesses of locals corrupt the runtime stack, so it is not yet wired into the suite.

Still on the roadmap: the remaining compiler stages in Nuru (parser, semantic, code generation), plus language features those require (wider integer/string support, a module/import mechanism).
