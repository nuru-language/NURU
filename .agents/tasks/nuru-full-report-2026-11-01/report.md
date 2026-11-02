# NURU Compiler — Full Project Report

**Date:** 2026-11-01  
**Workspace:** `/home/error51/NURU`  
**Source:** All findings are drawn from `investigation.md` (the completed investigation step). The dedicated `test-results.md` and `code-quality.md` files were not produced — the parallel `build-test` and `code-quality` workflow steps failed due to usage limits. All material for Dimensions 2 and 3 is sourced from `investigation.md` §§0, 6, 9 which contain actual test-run results and code-quality findings gathered during that step.

---

## 1. Project Status Report

### 1.1 Executive Summary

NURU is a compiled, dynamically-typed scripting language with an English-like syntax. Its compiler (`nuruc`) is a working, end-to-end native-code compiler written in C (~6,390 lines across 10 source pairs + driver). The full pipeline — lexer → parser → AST → semantic analysis → NIR IR generation → x86-64 code generation → `as`/`ld` → runnable ELF — is functional and verified: `make` builds clean (exit 0) and 53/53 end-to-end native test cases pass.

Key headline facts:
- The **optimizer** is fully implemented but commented out in the driver and never runs.
- The `runtime/` and `stdlib/` **directories are entirely empty**; the runtime is emitted as inline x86-64 assembly by the code generator, and there is no standard library at all.
- The **bootstrap self-hosting end-to-end demo** (`make test-codegen-e2e`) currently fails, causing `make test` to exit non-zero.
- Several language features are **silently broken**: logical `and`/`or` evaluate to the right operand only, float literals are silently truncated to integers, and `for each` over non-identifier iterables is silently dropped.
- The five `docs/` specification files describe a significantly more ambitious design (VM, mark-and-sweep GC, closures, Windows target, custom ELF/COFF emission, type checking) than what is actually implemented. Spec/impl drift is a consistent theme.

### 1.2 Subsystem Status Table

| Subsystem | Location | Status | One-line evidence |
|---|---|---|---|
| driver | `compiler/driver/nuru.c` | **Fully implemented** | CLI parses all flags; orchestrates full pipeline; optimizer call commented out; 269 lines |
| lexer | `compiler/lexer/nuru_lexer.{c,h}` | **Fully implemented** | Keywords, multi-word operators, numbers/floats, escapes, `#`/`###` comments; 462+124 lines |
| parser | `compiler/parser/nuru_parser.{c,h}` | **Fully implemented** | Full precedence ladder; all statement forms; interpolation via recursive sub-parser; 927+22 lines |
| AST | `compiler/ast/nuru_ast.{c,h}` | **Fully implemented** | All 30 node types; ownership-correct `ast_free`; shallow printer; 459+196 lines |
| semantic | `compiler/semantic/nuru_semantic.{c,h}` | **Fully implemented (scope only)** | Scope/structural checks pass; **no type system**; one dead header symbol; 536+53 lines |
| NIR (nir.c) | `compiler/nir/nuru_nir.{c,h}` | **Fully implemented** | Stack-machine IR; basic blocks; string interning; structural-only printer; 241+146 lines |
| NIR (irgen) | `compiler/nir/nuru_irgen.{c,h}` | **Mostly implemented** | All constructs lowered except: `and`/`or` silently dropped; `for each` identifier-iterable only; 583+22 lines |
| NIR (codegen) | `compiler/nir/nuru_codegen.{c,h}` | **Fully implemented** | Complete x86-64 emit + inline runtime (tagged values, refcounting, all builtins); floats truncated to int; OOM returns NULL; 1896+17 lines |
| optimizer | `compiler/optimizer/nuru_optimizer.{c,h}` | **Implemented but DISABLED** | Constant folding + jump threading implemented; `/* nir_optimize(ir); */` in driver; never runs |
| object | `compiler/object/nuru_object.{c,h}` | **Fully implemented (thin)** | Shells out to `as`/`ld`; no custom ELF/COFF; 15+12 lines |
| runtime | `runtime/` | **Empty directory** | All 8 subdirs have 0 files; runtime is emitted inline by `nuru_codegen.c` |
| stdlib | `stdlib/` | **Empty directory** | All 6 subdirs have 0 files; no modules, no import mechanism |
| bootstrap | `bootstrap/` | **Stages 1–3 working; Stage 4 e2e broken** | `lexer.nuru`/`parser*.nuru` pass their tests; `codegen.nuru` e2e fails |

### 1.3 Pipeline & Execution Model

The driver (`compiler/driver/nuru.c`) orchestrates the pipeline in this order:

```
source file
  → lexer_init / token scan
  → parser_parse          → ASTNode tree
  → semantic_analyze      → checked AST (scope only)
  → irgen_lower           → NIRProgram (stack-machine IR)
  → [nir_optimize — DISABLED]
  → codegen_emit          → <out>.tmp  (x86-64 assembly text)
  → nuru_assemble (as)    → <out>.o
  → nuru_link    (ld)     → <out>      (ELF64 executable)
  → unlink .o
```

Generated programs have a synthesized `_start` label (emitted by codegen) that calls `main` then `exit(0)`. The return value of `main` is discarded — exit code is always 0 regardless of what `main` returns.

The **value model** is 64-bit tagged words: integers are `(v<<8)|2` (tag 2); strings/lists/maps are `(ptr<<8)|4/5/6`; comparison results are raw `0`/`1` (no tag). Memory is managed with reference counting over a fixed **1 MB** static bump+free-list heap (`_gc_heap .zero 1048576`). Literal values between `_lit_lo`/`_lit_hi` are exempt from refcounting. `show` dispatches on the tag byte via `_print_value`, so it works uniformly for all value types.

The `attempt`/`recover` mechanism uses a single `_err_jmp` setjmp-style target; only divide-by-zero and mod-by-zero raise catchable errors. The error binding in `recover` is set to a boolean `1`, not an error object.

### 1.4 Runtime Status

The `runtime/` directory tree is **entirely empty** — every subdirectory (`collections/`, `errors/`, `functions/`, `memory/`, `platform/linux/`, `platform/windows/`, `strings/`, `values/`) contains zero files. There is no separate C runtime library.

Instead, the entire runtime is emitted as inline x86-64 assembly by `compiler/nir/nuru_codegen.c` at the end of every generated program. The emitted runtime is complete and functional for the implemented feature set:

**Implemented (verified by passing tests):**
- Tagged 64-bit values (int, string, list, map, empty/boolean)
- Uniform heap-object header: `[ref_count][length/count][inline payload]`
- Reference counting with recursive child release on free; literal-region exemption
- `show` dispatches on tag byte (`_print_value`/`_print_value_raw`)
- Full builtin set: `show`, `ask`, `len`, `abs`, `min`, `max`, `to_number`, `concat`, `ord`, `chr`, `pow`, `substring`, `upper`, `lower`, `index`, `to_string`, `reverse`, `read_file`, `write_file`
- `attempt`/`recover` (divide/mod-by-zero only)

**Not implemented (vs. `runtime-memory-spec-v0.1`):**
- No mark-and-sweep GC (refcounting only)
- No `NUMBER` float tag (floats truncated to int at codegen)
- No `FUNCTION`/`ERROR`/`CFUNC` value tags
- No closures or first-class functions
- No `Error` objects with `message`/`stack` fields
- No Windows platform support (Linux SysV ABI only)
- OOM returns NULL with no graceful recovery

### 1.5 Stdlib Status

The `stdlib/` directory tree is **entirely empty** — all six subdirectories (`collections/`, `core/`, `files/`, `math/`, `system/`, `text/`) contain zero files.

There is no standard library implementation. The only "library" surface available to Nuru programs is the hardcoded set of builtins emitted inline by `nuru_codegen.c` (listed in §1.4 above). There is no module system and no `import` mechanism; the bootstrap README and project README both list these as standing future work.

### 1.6 Bootstrap Status

`bootstrap/` contains four `.nuru` programs (2,798 lines total) that form a self-hosting compiler pipeline, each compiled by `nuruc` itself:

| Stage | File | Lines | Status |
|---|---|---|---|
| 1 — Lexer | `lexer.nuru` | 135 | **Working** — passes `tests/native/cases/lexer` |
| 2a — Expr parser | `parser_expr.nuru` | 399 | **Working** — passes `tests/native/cases/parser` |
| 2–3 — Parser + semantic | `parser.nuru` | 995 | **Working** — passes 9 parser/semantic test cases |
| 4 — Code generator | `codegen.nuru` | 1,269 | **Compiles and runs; e2e demo FAILS** |

**Stage 4 detail:** `codegen.nuru` is a full self-hosting x86-64 code generator (`lex` → `parse_program` → `gen_program`). It compiles and runs under `nuruc` (the `codegen` native test case passes), but the end-to-end self-hosting demo (`tests/native/run_codegen_e2e.sh`, `make test-codegen-e2e`) fails. The demo's embedded sample exercises lists/indexing/append and should print `20,30,4,99`; the harness expects the stale value `3,2,99`, and the bootstrap-generated program actually prints `20,10,4,0` — indicating two separate problems: a stale test expectation and a real bug in `codegen.nuru`'s list-indexing code generation (`a[2]`→10 not 30; `b[3]`→0 not 99). A stray `tests/native/cases/cg_want.txt` containing `7,3,2` is leftover cruft from an even older expectation.

### 1.7 Docs vs. Implementation

Five spec files exist in `docs/language/` and `docs/architecture/`. All describe a more ambitious design than what is implemented.

| Spec file | Key divergence(s) |
|---|---|
| `compiler-architecture-v0.1.md` | Spec has separate `resolver/` and `codegen/` dirs — actual folds resolution into semantic and codegen into `nir/`. Spec NIR is register/CFG-oriented with virtual register allocation, `NIR_AND/OR`, `NIR_NEW_LIST/MAP`, `NIR_NEW_CLOSURE` — actual is a stack machine with none of these. Spec targets both ELF64 and COFF/PE — actual is Linux ELF64 only. Spec has custom ELF/COFF emission — actual shells out to `as`/`ld`. Spec §7 includes a garbage collector — actual uses refcounting. Spec semantic output = annotated AST with resolved types — actual has no type annotations. |
| `grammar-spec-v0.3.md` | Spec uses `at` for indexing (`expr 'at' expr`) — actual uses `[]`; no `at` token exists. Spec includes `remove "X" from list` — not in grammar and not implemented. Spec `unary` has no unary minus — actual adds it. Spec `literal` includes `'list' '[…]'` — `list` keyword is dead (lexed, never parsed). Spec says comparisons are non-left-associative — actual parses them left-associatively. Spec says interpolation is done in the lexer — actual does it in the parser. |
| `language-spec-v0.3.md` | Lists `Number` as a float type — floats are silently truncated to int. Lists `Function` as a first-class value — not implemented. Lists `Error` with `message`+`stack` fields — error binding is a boolean flag. Shows `person["city"]` map bracket access — only `.field` member access works. §14/15: main returns Integer exit status — main return is discarded, exit always 0. Lists are described as "mutable" — actual lists are value-semantics (`append` allocates new, rebinds). |
| `runtime-memory-spec-v0.1.md` | Specifies `NUMBER=3`, `FUNCTION=7`, `ERROR=8`, `CFUNC=9` tags — none implemented. Specifies per-type structs with `data*` pointer (`NuruString` with capacity, `NuruFunction` with closure, `NuruError` with message/file/line) — actual uses uniform inline-payload headers. Specifies a mark-and-sweep GC — actual uses refcounting + free-list. Specifies `NuruVM` with call frames and environments/closures — actual uses the native stack with no VM. Specifies both Linux SysV and Windows MS calling conventions — actual is Linux SysV only. |
| `lexer-parser-spec-v0.1.md` | Spec has `TOKEN_ASSIGN (=)`, `TOKEN_COMMENT` — actual has `TOKEN_HASH`/`MULTILINE_COMMENT_*`/`INTERPOLATION_*`, no `=`. Spec has `TOKEN_TIMES` twice (typo). Spec AST has `AST_PARAMETER`/`AST_ARGUMENT` — actual omits these, adds `AST_DECLARATION_CHANGEABLE`/`AST_INTERP_STRING`/`AST_APPEND`. Spec §7: lexer arena allocator, parser pool allocator — actual uses plain `malloc`/`strdup` per node. |

### 1.8 Feature Coverage & Known Gaps

Features **working** (verified by passing tests): integer arithmetic, comparisons, all loop forms (`while`, `repeat N times`, `for each` over identifier lists), conditionals, functions (recursive, forward-ref, nested calls), strings (escapes, interpolation, all string builtins), lists (creation, `append`, `len`, indexing, `for each`), maps (`.field` member access), I/O (`ask`, `read_file`, `write_file`), `attempt`/`recover` (divide/mod-by-zero), `gc_stress` (20k allocations exercising the free-list), all 18 builtins.

Features **silently broken** (no test covers them, silent wrong behavior):
- `and`/`or` — tokenized, parsed, passed by semantic, but `emit_expr` drops both operators; `a and b` evaluates to `b`.
- Float literals — lexed/parsed/lowered but codegen truncates to int silently.
- `for each` over non-identifier iterables — silently drops the loop body.

Features **not implemented** (not in compiler, no test): `remove … from` (list removal), map bracket access `m["k"]`, `error.message` / `error.stack` access, non-zero exit codes, closures/first-class functions, module imports, Windows target.

### 1.9 Build Status

| Command | Result |
|---|---|
| `make clean && make` | **Exit 0.** Builds `nuruc` (566,856 bytes). Emits `-Wformat` warnings from unescaped `(%rsp)` in `nuru_codegen.c` `fprintf` format strings — harmless on glibc (emits literally) but technically undefined behavior. |
| `make test-native` | **53/53 PASS** |
| `make test-ast` | **PASS** (prints `FUNCTION_DECL (main)`) |
| `make test-codegen-e2e` | **FAIL** — expected `3,2,99`, got `20,10,4,0` |
| `make test` | **Exits non-zero** (due to `test-codegen-e2e` failure) |

Toolchain: gcc 15.2.0, GNU `as`/`ld` 2.46, make. Build flags: `-Wall -Wextra -std=c11 -O2 -g`.

---

## 2. Test Report

### 2.1 Test Inventory & Harness

Only `tests/native/` contains test content. All other test directories are empty:

| Directory | Status |
|---|---|
| `tests/native/` | **53 test case directories + 2 shell scripts** |
| `tests/lexer/` | Empty |
| `tests/compiler/` | Empty |
| `tests/parser/` | Empty |
| `tests/semantic/` | Empty |
| `tests/runtime/` | Empty |
| `tests/regression/` | Empty |

Each test case in `tests/native/cases/<name>/` contains:
- `program.nuru` — the Nuru source file to compile and run
- `expected.txt` — expected stdout
- `input.txt` — stdin feed (only present in the `ask` test case)

The harness (`tests/native/run_native_tests.sh`) compiles each `program.nuru` with `nuruc`, runs the result (feeding `input.txt` via stdin where present), diffs stdout against `expected.txt`, and reports PASS/FAIL. There is no exit-code checking — consistent with the fact that `main`'s return is always discarded (exit is always 0).

`make test` runs three targets:
1. `test-native` — the 53 e2e case harness
2. `test-ast` — compiles `examples/hello/hello.nuru` with `-a`, checks output contains `FUNCTION_DECL`
3. `test-codegen-e2e` — the bootstrap self-hosting demo via `tests/native/run_codegen_e2e.sh`

### 2.2 Results

| Test suite | Result | Detail |
|---|---|---|
| `test-native` (53 cases) | **53/53 PASS** | All native e2e cases pass |
| `test-ast` | **PASS** | Prints `FUNCTION_DECL (main)` |
| `test-codegen-e2e` | **FAIL** | Expected `3,2,99`; got `20,10,4,0` |
| `make test` (overall) | **EXITS NON-ZERO** | Due to `test-codegen-e2e` failure |

*All results verified by running the actual build/test commands (gcc 15.2.0, GNU as/ld 2.46, make).*

### 2.3 Coverage Map by Feature

| Feature | Covering test case(s) |
|---|---|
| Integer arithmetic (+, -, *, /) | `arithmetic`, `add` |
| Comparisons (all operators) | `conditions`, `negatives` |
| Parentheses / grouping | `paren_group` |
| `when`/`otherwise` | `conditions`, `nested_if` |
| `repeat N times` | `repeat` |
| `while` loop | `while` |
| `for each … in` (identifier list) | `list_foreach`, `list_sum` |
| `stop` / `skip` | `loop_break`, `loop_continue` |
| Functions (define + call) | `add`, `factorial`, `fibonacci` |
| Recursive functions | `factorial`, `fibonacci` |
| Forward references | `forward_ref` |
| Nested function calls | `nested_calls` |
| Variables (`make`, `change`) | `arithmetic`, most cases |
| Immutable vs changeable | `variables` |
| String literals + escapes | `strings` |
| String interpolation | `string_interp` |
| String builtins (`concat`, `upper`, `lower`, `reverse`, `contains`, `index`, `substring`, `ord`, `chr`, `to_string`) | `string_builtins` |
| Number builtins (`abs`, `min`, `max`, `pow`, `to_number`) | `builtins` |
| `ask` (stdin input) | `ask` |
| `read_file` / `write_file` | `io` |
| List creation + indexing | `lists`, `list_index` |
| `append` | `list_append` |
| `len` | `list_len`, `string_len` |
| Map / `.field` access | `map_field` |
| `empty` literal | `empty_literal` |
| `attempt`/`recover` (error) | `attempt_catch` |
| `attempt`/`recover` (no error) | `attempt_noerror` |
| `div` / `mod` | `div_mod` |
| GC stress (20k allocs) | `gc_stress` |
| Char operations | `chars` |
| Bootstrap lexer | `lexer` |
| Bootstrap parser (exprs) | `parser` |
| Bootstrap parser (stmts/fns/control) | `parser_stmt`, `parser_fn`, `parser_control`, `parser_loops`, `parser_repeat`, `parser_attempt` |
| Bootstrap semantic analysis | `parser_semantic` |
| Bootstrap code generator | `codegen` |

### 2.4 The Curated-Subset Caveat

The 53/53 pass rate does **not** reflect full language coverage. The test suite is curated to the *implemented and working* subset of the language. Grep over all `program.nuru` files confirms:

- **Zero** test files contain decimal float literals
- **Zero** test files use `and` or `or` as operators (all hits are inside comments)
- **Zero** test files use `remove … from`
- **Zero** test files use map bracket access (`m["key"]`)
- **Zero** test files expect a non-zero exit code
- **Zero** test files test `error.message` access inside `recover`

The 53/53 pass rate tells you the working subset works reliably. It says nothing about the completeness of the language implementation. Features in the green zone and features in the broken/missing zone both show zero test failures — because broken features have no tests.

### 2.5 Failures & Regressions

**`test-codegen-e2e` (the only failing test):**

The harness (`run_codegen_e2e.sh`) uses `nuruc` to compile `bootstrap/codegen.nuru`, then runs the resulting binary, and diffs its output against a hardcoded expected value.

- Expected: `3,2,99`
- Actual: `20,10,4,0`

There are two distinct problems:

1. **Stale test expectation.** The value `3,2,99` is outdated — it no longer matches the sample program embedded in `codegen.nuru`'s `main`. A stray `tests/native/cases/cg_want.txt` containing `7,3,2` is evidence of an even older expectation, showing the expected value has been revised at least twice without keeping the harness in sync.

2. **Real bug in `codegen.nuru`'s list index code generation.** The embedded sample creates `a = [20, 30, 4]` and `b = [1, 2, 99]`. The correct output should be `20,30,4,99`. Instead the output is `20,10,4,0`:
   - `a[2]` yields `10` instead of `30` — wrong list-element index generation
   - `b[3]` yields `0` instead of `99` — out-of-bounds or wrong base address

   This is a bug in the self-hosting code generator's x86-64 emit for list indexing, not in the `nuruc` compiler itself (the native `lists`/`list_index` tests pass).

**Fix required:** correct the list-index codegen in `bootstrap/codegen.nuru` AND update the e2e expected value once the bug is fixed. Also delete the stray `cg_want.txt`.

### 2.6 Test Infrastructure Gaps

- **No unit tests for any compiler stage.** All testing is end-to-end (compile + run + diff). A bug anywhere in the pipeline can only be located by manual bisection.
- **No lexer unit tests** despite `tests/lexer/` existing as a placeholder.
- **No parser/semantic/IR unit tests** for the same reason.
- **No exit-code testing.** The harness only checks stdout; since `main`'s return is always discarded this is consistent, but it means the "exit code" feature is completely untestable with the current harness.
- **No negative tests.** No cases check that `nuruc` rejects invalid input with a useful error message.
- **No float, `and`/`or`, `remove`, map-bracket, or error-object tests** — the suite is curated away from broken features rather than using `xfail` or skip markers.
- **No regression suite.** Bug fixes are not tracked with dedicated regression cases.

---

## 3. Code Quality / Review Report

### 3.1 Architecture Assessment

**Strengths:**
- Stage separation is clean and explicit. Each stage (`lexer`, `parser`, `ast`, `semantic`, `nir/`, `optimizer`, `object`) lives in its own subdirectory with a corresponding `.c`/`.h` pair. Dependencies are clearly unidirectional.
- The driver (`nuru.c`) is minimal and readable at 269 lines. The pipeline sequence is obvious from reading it.
- AST node ownership is correct and consistent. `ast_free` handles every node kind, `strdup` is used uniformly for owned strings, and the tagged-union design is straightforward.
- The NIR design as a simple stack machine (rather than SSA/register allocation) is a pragmatic choice that keeps `niru_irgen.c` and `nuru_codegen.c` straightforward for a single-target, single-pass compiler.
- String interning in NIR (`nir_const_str`) means `is`-comparison on strings works correctly as pointer equality — a clean design choice that avoids a separate string-equals runtime function.
- The inline-emitted runtime is consistent and complete for the supported value model. All builtins follow the same tagged-value calling convention.

**Weaknesses:**
- `nuru_codegen.c` at 1,896 lines is a monolith. It performs three conceptually distinct jobs: instruction-level code emission, flow-control scaffolding (function prologue/epilogue, branch/loop structure), and the entire runtime library. These should be separated.
- The optimizer is fully implemented but entirely disconnected from the pipeline. Its presence creates a false impression of optimization in the README.
- The AST printer (`ast_print`) is shallow — it prints only the top-level node type and primary field, with recursion only for `INTERP_STRING`. The `-a` flag is nearly useless for debugging complex programs.
- The NIR printer (`nir_print_program`) prints operand values as raw pointers (`a=%p`), making the `-i` dump unreadable for anything beyond confirming block structure. Neither debug flag is useful in practice.
- The spec/impl gap (§1.7) means the docs are actively misleading — a new contributor reading the architecture doc would form incorrect expectations about NIR design, codegen targets, and the type system.

### 3.2 Correctness Risks

Confirmed defects, ordered by severity:

| # | Finding | Severity | Detail |
|---|---|---|---|
| 1 | **`and`/`or` silently evaluate to right operand** | High | `emit_expr` in `nuru_irgen.c` has no case for `and`/`or`; both operands are emitted, the operator is dropped, leaving just the right-hand value. `a and b` is semantically equivalent to `b`. No error, no warning. |
| 2 | **Bootstrap codegen list-index bug** | High | `codegen.nuru`'s list-index code generation produces wrong addresses (`a[2]`→10 not 30; `b[3]`→0 not 99), breaking the e2e demo and likely any program `codegen.nuru` generates that uses list indexing. |
| 3 | **Floats silently truncated to int** | High | `NIR_CONST_NUM` is lowered from float AST nodes but codegen casts to `(long long)(int64_t)` before tagging. `3.14` becomes `3`. No warning at any stage. |
| 4 | **`for each` over non-identifier iterables silently dropped** | High | `emit_stmt` in `nuru_irgen.c`: `if (!iterable || iterable->type != AST_IDENTIFIER) break;` — the loop is emitted as an empty no-op if the iterable is any expression other than a bare variable name. |
| 5 | **Main return value discarded; exit always 0** | Medium | The generated `_start` calls `main` then immediately `exit(0)`, discarding `main`'s return value. Programs cannot return meaningful exit codes. |
| 6 | **`error` in `recover` is a boolean, not an Error object** | Medium | The error binding in `recover` is set to the integer `1` (tag 2). Accessing `error.message` would dereference a tagged integer as a pointer — a segfault or garbage read. |
| 7 | **Map bracket access mis-routes to list element** | Medium | `m["key"]` parses as `AST_INDEX`, which in codegen emits list-element indexing. String keys on maps are not supported; the spec's `person["city"]` example silently produces garbage. |
| 8 | **`remove … from` not implemented** | Medium | The `remove` keyword is not in the parser (not in grammar-spec either, but it is in language-spec §9.1). Programs using it would fail at parse time — at least it fails loudly. |
| 9 | **Optimizer disabled but documented as active** | Low | README states "the full pipeline — lexer through optimizer, codegen, and object/link stages — is wired end-to-end." This is false; `nir_optimize` is commented out in `nuru.c`. |
| 10 | **`semantic_check_function` declared but undefined** | Low | Declared in `nuru_semantic.h`, never defined in `nuru_semantic.c`, never called anywhere. Dead symbol; will produce a linker error if anything ever calls it. |

### 3.3 Memory Safety

**Positive findings:**
- `ast_free` handles all 30 node kinds correctly, recursively freeing children and calling `free()` on every `strdup`-owned string. No obvious AST leak.
- NIR deallocation follows the same ownership pattern. `nir_free_program` frees all functions, basic blocks, instructions, and the constants pool.
- The tagged-value runtime uses reference counting with correct recursive child release: `_decref` to zero releases list elements and map values before returning the block to the free list.
- Literal values (between `_lit_lo`/`_lit_hi`) are correctly excluded from refcounting via the range check in `_incref`/`_decref`.

**Risks and gaps:**
- **OOM returns NULL with no graceful handling.** `_alloc` returns `0` (NULL) when the bump pointer exceeds `_gc_hi`. The caller receives a null tagged pointer, which will be dereferenced on the next operation — a segfault. A program that allocates aggressively will crash silently rather than raising a recoverable error.
- **Fixed 1 MB heap.** The static heap size is hardcoded. There is no growth mechanism and no way for programs to configure it.
- **`system()`-based assembly/link** builds shell command strings from file paths without sanitization (§3.6 for security context; memory-safety aspect: `nuru_assemble`/`nuru_link` use fixed-size buffers — overflow-safe only if paths are short).
- **No arena/pool allocator.** The spec called for arena allocation in the lexer and pool allocation in the parser; actual uses `malloc`/`strdup` per node. In practice this is correct (no double-free, no use-after-free visible in the source), but it is slower and harder to bulk-free during error recovery.
- **`parse_interp_subexpr`** spins up a new `Parser` and `Lexer` on the heap for each interpolated substring. These are stack-allocated in `parse_interpolated_string`, correctly freed on exit.

### 3.4 Maintainability

**Vestigial / dead code:**
- `TokenType` includes `TOKEN_INTERPOLATION_START` and `TOKEN_INTERPOLATION_END`; `LexerState` includes `LEX_STATE_STRING`, `LEX_STATE_STRING_ESCAPE`, `LEX_STATE_INTERPOLATION`, `LEX_STATE_COMMENT`, `LEX_STATE_MULTILINE_COMMENT` — but `lexer_next_token` only ever enters `LEX_STATE_NORMAL` (returns EOF for any other state). These states were superseded by parser-level interpolation but were never removed.
- `TOKEN_LIST` (`list` keyword) is lexed and appears in the keyword table but the parser never produces an `AST_LIST_LITERAL` from it. The token is dead.
- `semantic_check_function` is declared in `nuru_semantic.h` but never defined or called (§3.2 item 10).
- `skip_whitespace`'s `###` guard is `peek_next=='#' && peek_next=='#'` — a tautology (the second condition is identical to the first). Multiline comments still work, but the guard is logically incorrect and could mask a future bug.

**Structural concerns:**
- `nuru_codegen.c` at 1,896 lines mixes code emission with the complete runtime library. Splitting the emitted-runtime helpers into a separate `nuru_runtime_emit.c` would make both files maintainable.
- `PRINT_INT` and `PRINT_STRING` in the codegen emit identical code (`call _print_value` with newline handling) — they are byte-for-byte duplicates. This is harmless but misleading; a single `NIR_SHOW` opcode would be cleaner.
- The IR operand printer (`nir_print_program`) prints pointers not values, making IR dumps useless beyond confirming block count. Fixing this would significantly improve debuggability.
- The AST printer's shallowness (only top-level node + `INTERP_STRING` recursion) means `-a` is nearly useless for programs with more than one statement.

### 3.5 Portability

- **Linux x86-64 only.** The emitted assembly uses Linux SysV ABI (`syscall` for I/O, `rdi/rsi/rdx` argument passing), Linux syscall numbers (`sys_read=0`, `sys_write=1`, `sys_open=2`, `sys_close=3`, `sys_exit=60`), and ELF64 linking via `ld`. Nothing in `nuru_codegen.c` is platform-abstracted.
- **Windows is a declared non-starter.** `runtime/platform/windows/` is an empty directory stub.
- **`-Wformat` portability hazard.** Several `fprintf` calls in the emitted `_read_file`/`_write_file` helpers use `(%rsp)` inside format strings without escaping the `%`. GCC/glibc treats unknown `%r` as a literal `%r`, so the emitted assembly is correct on glibc, but this is undefined behavior under the C standard and will miscompile with other libc implementations.
- **ASCII-only identifiers.** The lexer's `is_identifier_start`/`is_identifier_continue` checks for `[a-zA-Z_]`/`[a-zA-Z0-9_]`. No Unicode support.
- **`int` for lengths/indices.** Several internal buffers and loop counters use `int` rather than `size_t`. Not a correctness issue for typical programs but a latent bug for very long strings or large data.

### 3.6 Security

- **`system()`-based assembler/linker invocation.** `nuru_assemble` and `nuru_link` in `nuru_object.c` construct shell command strings by interpolating file paths directly:
  ```c
  system("as -o <obj> <asm>");
  system("ld -o <exe> <obj>");
  ```
  If the output file path contains shell metacharacters (spaces, semicolons, backticks, `$(...)`), this is a command-injection vector. The compiler is a local tool and the paths are typically controlled by the user, so the practical risk is low, but it is not correct to use `system()` with user-supplied paths. `execvp` with an argument array would be safe.
- **Return value of `system()` not checked for partial failure.** If `as` or `ld` fails, the driver proceeds as if it succeeded and reports success to the user.
- **No sandboxing of compiled programs.** This is expected for a native compiler, but programs have full syscall access (the generated `_read_file`/`_write_file` open arbitrary paths).
- **Fixed-size internal buffers.** Some buffers in the codegen and driver use `char buf[256]` or `char buf[512]` for path construction. Very long paths could overflow these, though in practice the compiler is invoked with paths the user controls.

### 3.7 Doc Hygiene

- **README claims the optimizer is "wired end-to-end."** It is not — `nir_optimize` is commented out in the driver.
- **README states the runtime "implements the full tagged-value and reference-counting model of runtime-memory-spec-v0.1."** This is only half-accurate: the tagged-value idea is drawn from the spec, but the spec actually specifies mark-and-sweep GC, not reference counting.
- **Five spec files all diverge from the implementation** in material ways (§1.7). A new contributor reading the docs would form incorrect expectations about the NIR design, type system, codegen targets, and memory model.
- **Stale test expectation** in `run_codegen_e2e.sh` (`3,2,99` is not the right expected value for the current `codegen.nuru` sample).
- **Stray `tests/native/cases/cg_want.txt`** (`7,3,2`) is cruft from a previous iteration and should be deleted.
- **`docs/decisions/`** and **`docs/roadmap/`** are empty. The architecture doc is labeled `v0.1` with no successor.

### 3.8 Recommendations (Priority Order)

| Priority | Recommendation |
|---|---|
| **P0** | Fix `codegen.nuru` list-index code generation bug; update `run_codegen_e2e.sh` expected value once fixed; delete `cg_want.txt`. This is the only thing making `make test` exit non-zero. |
| **P0** | Fix `and`/`or` lowering in `nuru_irgen.c`. Add `NIR_AND`/`NIR_OR` opcodes (or lower to short-circuit branch sequences) and add test cases. This is a silent correctness bug affecting a fundamental language feature. |
| **P1** | Fix float handling: either implement float codegen properly (add tag 3, emit float arithmetic) or add an early-error in the semantic pass or parser that rejects float literals with a clear message. Silent truncation is the worst outcome. |
| **P1** | Fix `for each` over non-identifier iterables: remove the `AST_IDENTIFIER`-only guard and emit the correct IR for any iterable expression. |
| **P1** | Wire in the optimizer: remove the `/* nir_optimize(ir); */` comment, or explicitly delete the optimizer and update the README to reflect its absence. Either is acceptable; the current state (present but silently disabled) is not. |
| **P2** | Fix `error` binding in `recover`: emit a proper error object (even a minimal struct with a `message` field) so `error.message` works without a segfault. |
| **P2** | Fix `system()`-based `as`/`ld` invocation: replace with `fork`+`execvp` using argument arrays to eliminate the command-injection surface and add proper failure detection. |
| **P2** | Add per-stage unit tests (at minimum for the lexer and parser) so individual-stage regressions are caught without full e2e pipeline runs. |
| **P3** | Prune dead code: remove `TOKEN_INTERPOLATION_START/END`, unused `LexerState` values, `TOKEN_LIST`, and the `semantic_check_function` declaration. Fix the tautological `###` guard. |
| **P3** | Improve debug output: fix the NIR printer to show operand values (not pointers) and make the AST printer recursive. These are the two main debugging tools; in their current state they provide almost no useful information. |
| **P3** | Resolve spec/impl drift: either update the five spec docs to reflect the actual implementation or add a `docs/decisions/` ADR explaining the deliberate departures (stack machine instead of register IR, refcounting instead of GC, etc.). |
| **P3** | Add OOM error handling in the generated runtime: `_alloc` should jump to the current `_err_jmp` target (or a dedicated OOM handler) rather than returning NULL. |

---

*End of report. All findings are based on actually reading the source and running the build/test commands. Test results in §2 reflect an actual `make test` run; all other claims cite specific source files and line counts verified by inspection.*
