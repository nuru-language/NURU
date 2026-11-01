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

- Values are 64-bit words; the high bit tags string/list/map pointers from integers.
- Strings are length-prefixed descriptors; lists and maps are heap objects with a length/count header.
- The heap is a 1 MiB managed region with a **conservative mark-and-sweep garbage collector** (`_gc_collect`): it marks reachable objects by scanning the machine stack (roots) and transitively through object payloads, then sweeps unmarked allocations back onto a free list that `_alloc` reuses. Collection triggers automatically when bump allocation exhausts the region.
- `show` dispatches on the tag bit through `_print_value`; divide-by-zero raises a catchable error for `attempt`/`recover`.

The memory model is a working subset of `docs/language/runtime-memory-spec-v0.1.md` (single-bit tagging rather than an 8-bit tag field; no reference counting).

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

Built-in functions: `show`, `ask` (read a line from stdin), `len` (string/list/map length), `abs`, `min`, `max`, `to_number` (parse a string to an integer), `concat` (concatenate two strings), `ord` (first char code of a string), `chr` (integer → one-character string). String comparison (`is` / `is not`) works via literal interning (identical string constants share one descriptor).

The runtime implements a working subset of `docs/language/runtime-memory-spec-v0.1.md`: tagged values, a managed heap with a conservative mark-and-sweep GC, and catchable divide-by-zero. Still on the roadmap: an 8-bit tag field and reference counting per the full spec, a wider standard library, and self-hosting/bootstrap.
