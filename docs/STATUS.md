# Nuru Project Status

Consolidated engineering status (compiler, runtime, tooling, modules).

## Build & test

```sh
make            # builds ./nuruc (zero warnings required)
make test       # full suite: native + AST + bootstrap e2e + safe-io + negative
                #   + optimizer unit + optimized/unoptimized compare + CLI
```

Current suite (all green on a clean `make clean && make test`):

| Suite | Count | Notes |
|-------|-------|-------|
| Native execution | 70 | `tests/native/cases/*` |
| Bootstrap codegen e2e | 1 | `run_codegen_e2e.sh` (self-hosting demo) |
| Safe external-tool invocation | 2 | filenames with metacharacters, missing `as` |
| Compile-time diagnostic (negative) | 5 | float literal, missing module, top-level stmt, duplicate function, unwritable output |
| Optimizer unit | 5 | folding, div0-guard, truncation, threading, jump-to-next |
| Optimized vs unoptimized compare | 70 | identical stdout + exit code |
| CLI integration | 7 | version/build/run/check/new/test/module-path |

## Compiler pipeline

```
Nuru source -> import expander (bring) -> lexer -> parser -> AST
  -> semantic (scopes, top-level rules, int-literal range, duplicate fns)
  -> NIR (stack machine, basic blocks) -> optimizer (default on, -O0 to disable)
  -> x86-64 codegen -> as/ld (safe execvp) -> ELF executable
```

## Runtime

Self-contained executables (no external runtime). See `runtime/README.md` for
the full value representation, object layouts, allocator and refcount model.

- Tagged 64-bit values: 0 empty, 1 bool, 2 integer (signed 56-bit), 4 string,
  5 list, 6 map, 7 error.
- Integer literals outside `2^55` are rejected at compile time; arithmetic
  overflow raises a catchable `"integer overflow"` error.
- Errors are real values (tag 7) with `.message`; `recover` receives them and
  `show` prints the message.
- `attempt`/`recover` uses `_err_jmp`/`_err_value`; divide-by-zero and overflow
  are catchable.
- Builtins: `len`, `abs`, `min`, `max`, `pow`, `to_number`, `to_string`,
  `concat`, `substring`, `upper`, `lower`, `contains`, `index`, `ord`, `chr`,
  `reverse` (string or list), `remove` (list), `read_file`, `write_file`,
  `ask`, `show`. Ordinal string equality is available via `stdlib/text.nuru`
  (`str_eq`).

## Modules (`bring`)

```nuru
bring math        # stdlib/math.nuru (or ./math.nuru)
bring greet       # resolved via manifest module_path, see below
```

- Search: `<stdlib_dir>/<name>.nuru` then `./<name>.nuru`; `--stdlib DIR`/`-S`
  sets the directory (default `stdlib`).
- Include-once per compilation; recursive `bring` supported; missing modules
  and duplicate top-level functions are clear compile-time errors.
- stdlib: `core` (triple), `math` (square/cube/clamp), `text`
  (str_eq/starts_with/ends_with), `collections` (sum/list_has), `geo`
  (distance_sq), `files` (read_text/write_text).

## Tooling

`nuru` CLI:
```
nuru version | new NAME | build [-o OUT] FILE|DIR | run FILE|DIR
nuru check FILE|DIR | test [DIR] | lock [DIR] | doctor | clean | help
```

- `new NAME` scaffolds `src/main.nuru`, `tests/`, `examples/`, `nuru.toml`,
  `README.md`.
- `build DIG` reads `nuru.toml` `[project] name` → emits `build/<name>`;
  `[project] module_path` → module search dir; `run`/`check`/`test` honor the
  same resolution.

## Test conventions

- Native cases: `tests/native/cases/<name>/{program.nuru,expected.txt[,exit_code]}`
- Negative (must-fail) checks: `tests/native/run_negative_tests.sh`
- Optimizer parity: `tests/native/run_opt_compare.sh`
- CLI: `tests/integration/run_cli_tests.sh`

## Roadmap status

- **Phase 9 — complete.** `nuru.lock` (SHA-256 per module) with `nuru lock`
  / `lock --check`, and `--locked` gating on build/run/check.
- **Phase 5 — nearly complete.** All runtime helper emissions now live behind
  `runtime/emit_runtime.h` / `runtime/platform/linux/emit_runtime.c` (verified
  byte-identical against the pre-extraction build). Mark-and-sweep GC:
  algorithm implemented and tested as a host-side collector
  (`tests/nir/test_gc.c`, covering reachability, cycle reclamation, and
  reachable-cycle preservation) per `runtime/GC.md`; integration into the
  emitted assembly runtime remains.
- **Phase 8 — in progress.** Module visibility: selective imports
  (`bring <name> from <module>`) implemented; un-imported module symbols are
  not visible. Namespace-qualified references (e.g. `m.f`) remain future work.
- **Phase 7**: Windows x86-64 PE backend (needs a Windows runner).
- **Phase 10**: extend bootstrap codegen to full self-hosting.
