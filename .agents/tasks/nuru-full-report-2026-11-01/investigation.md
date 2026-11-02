# NURU Compiler — Investigation (Raw Material & Report Skeleton)

Investigation of the NURU compiler at `/home/error51/NURU`. READ-ONLY: no source was modified. Build/test artifacts were regenerated to verify status. Date: 2026-11-01.

## 0. Verification basis (what was run vs. only read)

Verified by executing the project's real build/test commands (tooling present: gcc 15.2.0, GNU `as`/`ld` 2.46, make):
- `make clean && make` → builds `./nuruc` (566,856 bytes), **exit 0**. Compilation emits `-Wformat` warnings (see §9), but no errors.
- `make test` → `test-native`: **53/53 native cases PASS**; `test-ast`: PASS (prints `FUNCTION_DECL (main)`); `test-codegen-e2e`: **FAIL** (bootstrap self-hosting end-to-end produces `20,10,4,0` vs expected `3,2,99`). So `make test` **exits non-zero overall** because of `test-codegen-e2e`.

Everything else below is from reading the source/docs directly. "Verified by running" is stated explicitly where it applies.

## 1. Repository map — what actually exists vs. empty placeholders

Real content lives only in: `compiler/` (C, 10 source pairs + driver), `bootstrap/` (4 `.nuru` files), `tests/native/` (53 case dirs + 2 shell scripts), `docs/` (5 spec `.md` files), `examples/` (6 `.nuru` files), `Makefile`, `README.md`, and the prebuilt `nuruc`/`build/` artifacts.

**Completely empty placeholder directories** (0 files anywhere beneath — `find -type f` count):
- `runtime/` (collections/, errors/, functions/, memory/, platform/linux/, platform/windows/, strings/, values/)
- `stdlib/` (collections/, core/, files/, math/, system/, text/)
- `scripts/`, `builds/`, `tools/nuru-cli/`
- `docs/decisions/`, `docs/roadmap/`
- `bootstrap/build-tools/`, `bootstrap/legacy-prototype/`
- `tests/lexer/`, `tests/compiler/`, `tests/parser/`, `tests/semantic/`, `tests/runtime/`, `tests/regression/` (only `tests/native/` has content)

This is a headline finding: the named `runtime/` and `stdlib/` subsystems are **empty directory skeletons**; the actual runtime is generated inline as x86-64 assembly by `compiler/nir/nuru_codegen.c` (see §3), and there is no stdlib implementation at all (see §4).

---

## 2. Compiler subsystem map (compiler/)

Pipeline (driver `compiler/driver/nuru.c`, `main`): read file → `lexer_init`/token dump → `parser_parse` → `semantic_analyze` → `irgen_lower` → `nir_optimize` (DISABLED) → `codegen_emit` → `nuru_assemble`(`as`) → `nuru_link`(`ld`). Entry of generated programs: `_start: call main; exit(0)`.

Line counts (verified): lexer.c 462/h 124; parser.c 927/h 22; ast.c 459/h 196; semantic.c 536/h 53; nir.c 241/h 146; irgen.c 583/h 22; codegen.c 1896/h 17; optimizer.c 132/h 9; object.c 15/h 12; driver nuru.c 269. Total ≈6,390.

### 2.1 driver/ (`nuru.c`) — Fully implemented
CLI parses `-t/-a/-s/-i/-o/-h` and input file; orchestrates the whole pipeline; writes assembly to `<out>.tmp`, assembles to `.o`, links to `<out>`, unlinks `.o`. **Critical evidence:** the optimizer call is commented out — `/* nir_optimize(ir); */` — so the optimizer is compiled but never invoked (see §2.8). Execution model: generated `_start` calls `main` then `exit(0)`; `main`'s return value is discarded (exit code is always 0 — diverges from language-spec §14/§15 which say main returns the exit status). Top-level non-function statements would dereference `irgen->current_func == NULL`; programs are effectively required to define `create main` (no `main` → `ld` undefined-reference failure).

### 2.2 lexer/ (`nuru_lexer.{c,h}`) — Fully implemented
Single-pass scanner: identifiers, integers, floats (`TOKEN_NUMBER` with `.` and `e`/`E`), strings with escapes (`\n \r \t \" \\ \{ \}`), `#` line comments, `### ... ###` multi-line comments, all keywords, and multi-word operators (`is not`, `is above/below/at least/at most`, `divided by`, `remainder of`) via `consume_next_word`. Line/column tracked. Errors: invalid char, unterminated string, unterminated escape, unterminated multiline comment.
Evidence of dead/vestigial code: `TokenType` includes `TOKEN_INTERPOLATION_START/END` and `LexerState` includes `LEX_STATE_STRING/STRING_ESCAPE/INTERPOLATION/COMMENT/MULTILINE_COMMENT`, but `lexer_next_token` only handles `LEX_STATE_NORMAL` (returns `EOF` otherwise) and never emits interpolation tokens — interpolation is done later in the parser. Minor: `skip_whitespace`'s `###` guard is `peek_next=='#' && peek_next=='#'` (redundant tautology) — single `#` and `###` still work, but `##` emits a stray `TOKEN_HASH`.

### 2.3 parser/ (`nuru_parser.{c,h}`) — Fully implemented
Recursive-descent with the full precedence ladder (or→and→equality→comparison→additive→multiplicative→unary→postfix→primary) matching grammar-spec §6. Statements: `make x be …`, `make x changeable …`, `change x to …`, `create f using … { }`, `when/otherwise`, `repeat n times`, `while`, `for each x in …`, `show`, `attempt/recover`, `stop`, `skip`, `give …`, `add … to …`, bare blocks, and expression-statements (`f with args` as a statement, result discarded — `AST_FUNCTION_CALL` + `NIR_DISCARD`). String interpolation is handled here via `parse_interpolated_string`, which recursively spins up a sub-`Parser`/`Lexer` on each `{expr}` substring (`parse_interp_subexpr`); escapes decoded in `decode_string`. `parse_postfix` handles `.member` then `[index]` as two *sequential* while-loops, so `a.b.c[0]` works but mixed `a[0].b` does not (index-then-member not chained). `ask` is parsed as a zero-arg call. `empty` → `AST_EMPTY_LITERAL`.

### 2.4 ast/ (`nuru_ast.{c,h}`) — Fully implemented
All 30 node types defined with a tagged-union `ASTNode`; constructors set fields and `strdup` names; `ast_free` recursively frees every node kind with correct ownership; `ast_print` is shallow (prints node type + primary field; only `INTERP_STRING` recurses into parts) — so `-a` dumps top-level statement names only (confirmed by running: `nuruc -a examples/hello/hello.nuru` prints just `FUNCTION_DECL (main)`). `AST_NUMBER_LITERAL` exists for floats.

### 2.5 semantic/ (`nuru_semantic.{c,h}`) — Fully implemented for scope/structural checks; NO type system
Scope stack with `Symbol` (`VARIABLE/FUNCTION/PARAMETER`, `mutable`, `arity`). `semantic_analyze` runs a pre-pass registering all top-level functions (forward/mutual references supported — confirmed by passing `forward_ref` test), then walks the AST. Checks: redeclaration in same scope, assignment to undeclared/immutable/non-variable, append to undeclared name, undeclared identifier use, `return` outside function, `stop`/`skip` outside loop (via `loop_depth`), and builtin/user-function arity against a hardcoded builtin table (`ask,len,abs,min,max,to_number,concat,ord,chr,pow,contains,substring,upper,lower,index,to_string,reverse,read_file,write_file`).
Gaps: **no type system** — symbols carry no type, there is no type inference or checking (diverges from architecture-spec §4.4 "annotated AST with resolved types"). `append` does not verify the target is a list. Comparison/arithmetic operand types are unchecked. `semantic_check_function` is declared in the header but **never defined** (dead declaration; nothing calls it).

### 2.6 nir/ — three files

**nuru_nir.{c,h}** — Fully implemented. Stack-machine IR with basic blocks linked by `next`; `NIRFunction` (params, locals); `NIRProgram` (functions, constants pool). String constants are **interned** by content+length (`nir_const_str`), which is the basis for `is`-on-strings being pointer/interning equality (README-confirmed). `nir_print_program` prints opcodes + block structure but operand values as raw pointers (`a=%p`), so the `-i` dump is structural, not human-readable.

**nuru_irgen.{c,h}** — Fully implemented with two notable lowering gaps. Lowers all expressions (incl. bool→int `1/0`, empty→`0`, map→`ALLOC_MAP`, member→`LOAD_MAP_FIELD`, index→`LOAD_ELT_EXPR`) and statements; builds correct CFGs for `if/else`, `while`, `repeat` (counter loop), `for each` (counter+`LOAD_LIST_LEN`+`LOAD_ELT`), `attempt/recover` (`ERR_SAVE`/`ERR_SET` setjmp-style target + `ERR_RESTORE`), and `stop/skip` (via fixed-depth `break/continue_blocks[32]`). Gap 1: **`and`/`or` are not lowered** — `emit_expr`'s `AST_BINARY_OP` switch has cases for `plus/minus/times/divided by/remainder of/is/is not/is above/below/at least/at most` but **none for `and`/`or`** (and `nir.h` has no `NIR_AND`/`NIR_OR` opcode); both operands are emitted then the operator is silently dropped, so `a and b` evaluates to `b` (a is left on the stack ignored). No test uses `and`/`or` (§6). Gap 2: **`for each` only lowers when the iterable is a bare `AST_IDENTIFIER`** — `if (!iterable || iterable->type != AST_IDENTIFIER) break;` silently drops the loop for any other iterable expression (matches bootstrap README note). Floats are lowered to `NIR_CONST_NUM`.

**nuru_codegen.{c,h}** — Fully implemented (the largest file, 1,896 lines). Emits x86-64 assembly via `fprintf`. This file **is the runtime**: it emits not only per-instruction code but the entire runtime helper set as inline assembly (`_print_int`, `_print_value`, `_print_value_raw`, `_write_string`, `_checked_div`/`_checked_mod` (divide/mod-by-zero → jump to `_err_jmp` for `attempt/recover`, else return 0), `_read_line` (ask), `_concat`, `_contains`, `_append`, `_substring`, `_upper`, `_lower`, `_index`, `_to_string`, `_read_file`/`_write_file` (open/read/write/close + fstat), `_reverse`, `_parse_int`, `_alloc`, `_incref`, `_decref`). Value model (verified): 64-bit tagged words — integer `(v<<8)|2` (tag 2), string/list/map pointers `(ptr<<8)|4|5|6`; comparison results are **raw 0/1 (no tag)** and conditionals test them via a tag-aware path. `PRINT_INT` and `PRINT_STRING` are byte-identical (both `call _print_value`, which dispatches on the runtime tag byte + newline) — so `show` works for any value type via runtime tag dispatch (README-accurate). Reference counting over a fixed **1 MB** static bump+free-list heap (`_gc_heap .zero 1048576`); literals in `.data` between `_lit_lo`/`_lit_hi` are exempt from refcounting; `_decref` to zero recursively releases list/map children then returns the block to the free list. `attempt/recover` error var is set to a constant `1` (a boolean flag), **not** an error object. Floats (`NIR_CONST_NUM`) are **truncated to int** at codegen (`(long long)(int64_t)number_value << 8 | 2`). OOM (`_alloc` bump past `_gc_hi`) returns `0` (NULL) with no graceful handling. Minor: several `fprintf` format strings use unescaped `(%rsp)`/`(%rsp)` (e.g. in `_read_file`/`_write_file`) producing `-Wformat` warnings; on glibc the unknown `%r` conversion is emitted literally so the assembly is correct (the `io` test passes), but it is undefined behavior and a portability hazard.

### 2.7 object/ (`nuru_object.{c,h}`) — Fully implemented (thin)
`nuru_assemble` → `system("as -o obj asm")`; `nuru_link` → `system("ld -o exe obj")`. 15 lines. Delegates to the system assembler/linker; no custom ELF/COFF emission (diverges from architecture-spec §4.9 "section construction, symbol table, relocations, ELF64/COFF"). `system()` shell-construction from file paths is a minor command-injection surface for untrusted paths; return value is the raw `system()` status.

### 2.8 optimizer/ (`nuru_optimizer.{c,h}`) — Fully implemented but NOT wired in
`nir_optimize` implements constant folding (`LOAD_CONST LOAD_CONST ADD/SUB/MUL/DIV/MOD` → one `LOAD_CONST`, with div/mod zero-guard), jump threading (`resolve_target` follows single-JUMP chains, depth-limited 32), `truncate_after_terminal` (drops instructions after a true terminal — `is_terminal` recognizes only `JUMP/RETURN/RETURN_VALUE/HALT`, not conditional jumps, per the README fix), and `trim_jumps_to_next` (drop a trailing `JUMP` to the immediately-next block). All real. **But it is disabled in the driver** (`/* nir_optimize(ir); */` in `nuru.c`), so it never runs in the actual pipeline. This contradicts the README's claim that "the full pipeline — lexer through optimizer, codegen, and object/link stages — is wired end-to-end."

### 2.9 driver pipeline status summary
Working end-to-end (verified: 53/53 native tests): lexer → parser → semantic → irgen → codegen → as → ld → runnable ELF. Optimizer present but bypassed.

---

## 3. Runtime status

**The `runtime/` directory is entirely empty** (0 files in every subdirectory). The runtime is instead **emitted as inline x86-64 assembly by `compiler/nir/nuru_codegen.c`** — there is no separate C runtime library and no linked runtime. The emitted runtime is complete and functional (verified by the passing `gc_stress`, `io`, `attempt_catch`, `string_*`, `list_*`, `map_field` tests):
- Tagged 64-bit values (int=2, string=4, list=5, map=6; raw 0/1 for comparison results; tag 0 ⇒ "empty" when printed).
- Uniform heap-object header `[ref_count][length/count][payload…]` with inline payload (no separate capacity field, no `data` pointer — strings/lists/maps all carry data inline).
- Reference counting (`_incref`/`_decref`) over a fixed 1 MB bump+free-list allocator; literals exempted via a `_lit_lo`/`_lit_hi` range check; recursive child release on free.
- `show` dispatches on the tag byte via `_print_value`/`_print_value_raw` (README-accurate).
- `attempt/recover` via a single `_err_jmp` setjmp-style target; only divide/mod-by-zero raises a catchable error; the error binding is a boolean flag, not an error object.
- Full builtin set: `show, ask, len, abs, min, max, to_number, concat, ord, chr, pow, substring, upper, lower, index, to_string, reverse, read_file, write_file`.
Not implemented (vs. runtime-memory-spec): no VM/interpreter (native codegen instead), no mark-and-sweep GC (refcounting only), no `NUMBER`/float tag 3 (floats truncated to int), no `FUNCTION`/`ERROR`/`CFUNC` tags, no closures/environments, no Error objects with message/stack, no Windows platform (Linux SysV only).

## 4. Stdlib status

**The `stdlib/` directory is entirely empty** (0 files in collections/, core/, files/, math/, system/, text/). There is **no standard library implementation**. The only "library" surface is the hardcoded set of compiler builtins emitted inline by codegen (§3) and usable directly as `name with args`. There is no module/import mechanism (README/bootstrap README both list it as standing work), no `math`/`system`/`files`/`text`/`collections` library modules.

## 5. Bootstrap status

`bootstrap/` contains 4 real, working `.nuru` programs (2,798 lines total: `lexer.nuru` 135, `parser_expr.nuru` 399, `parser.nuru` 995, `codegen.nuru` 1,269) plus empty `build-tools/` and `legacy-prototype/`. Each is a normal Nuru program compiled by `nuruc`.
- **Stage 1 — `lexer.nuru`**: tokenizer for Nuru source (identifier/integer/string/punctuation/newline/EOF), implemented via `ord`/`substring`/`to_number`/`chr` and maps. Covered by passing `tests/native/cases/lexer`. Verified working (test passes).
- **Stage 2a — `parser_expr.nuru`**: recursive-descent expression parser (arithmetic + comparison precedence, list/map/string literals). Covered by passing `parser` case.
- **Stage 2–3 — `parser.nuru`**: statement-level parser + a semantic checker (`analyze_program` collects declared names, reports undeclared references). Covered by passing `parser_stmt`, `parser_fn`, `parser_control`, `parser_cmp`, `parser_loops`, `parser_repeat`, `parser_attempt`, `parser_literals`, `parser_semantic`. Verified working.
- **Stage 4 — `codegen.nuru`**: a full self-hosting compiler front-end + x86-64 code generator (`lex`→`parse_program`→`gen_program`, with `gen_expr`/`gen_block`/`gen_function` + `setcc_for`). Compiles and runs (passing `codegen` native case), **but the end-to-end self-hosting demo (`tests/native/run_codegen_e2e.sh`, `make test-codegen-e2e`) currently FAILS** (see §6). The embedded sample in `codegen.nuru`'s `main` exercises lists/indexing/append and should print `20,30,4,99`; the e2e harness expects the stale value `3,2,99`, and the actual generated program prints `20,10,4,0` — indicating both a **stale test expectation** and a **bug in the bootstrap code generator's list indexing** (`a[2]`→10 not 30; `b[3]`→0 not 99). A stray `tests/native/cases/cg_want.txt` (`7,3,2`) is leftover cruft from yet another older expectation.

## 6. Tests status

Only `tests/native/` has content: 53 case directories (each `program.nuru` + `expected.txt`; only `ask` has `input.txt`; **no** case sets a non-zero `exit_code` — consistent with main's return being ignored) plus `run_native_tests.sh` (compile→run→diff) and `run_codegen_e2e.sh` (bootstrap self-hosting demo). All other `tests/` subdirs (`lexer/compiler/parser/semantic/runtime/regression`) are empty — there are **no unit tests** for any individual compiler stage; testing is end-to-end only.
Verified by running: **53/53 native cases pass**, `test-ast` passes, `test-codegen-e2e` **fails** ⇒ `make test` exits non-zero.
Coverage spans: arithmetic/comparisons/conditions, all loops (+break/continue), functions (+nested calls/forward refs), strings (escapes/interpolation/concat/substring/upper/lower/reverse/contains/index/ord/chr/to_string/compare), collections (empty list/list-for-each/list-sum/map-field/index/indexing/len/append), I/O (ask/read_file/write_file), error handling (attempt catch/no-error), builtins, gc_stress (20k allocations exercising the free-list), negatives, paren grouping, nested if, pow, div_mod, chars.
**Coverage gap (meta-finding):** the suite is curated to the *implemented* subset and systematically excludes broken/unimplemented features — no test uses float literals, logical `and`/`or`, `remove … from`, map bracket access `m["k"]`, `error.message`, or non-zero exit codes. Grep over all `program.nuru` confirmed: zero decimal literals; all `and`/`or` hits are inside comments; no `remove`; every `["` is a list literal. So the 53/53 pass rate reflects the working subset, not feature completeness.

## 7. Build system

`Makefile` (root): `CC=gcc`, `CFLAGS=-Wall -Wextra -std=c11 -O2 -g -Icompiler`, globs `compiler/*/*.c` and `compiler/*/*/*.c` into `build/*.o`, links `nuruc`. Targets: `all` (`nuruc`), `clean`, `test` (= `test-native` + `test-ast` + `test-codegen-e2e`), `test-native`, `test-codegen-e2e`, `test-ast`. `nuruc` (root) is a prebuilt ELF binary (also rebuilt by `make`). No `nuruc` script — the task's mention of a root `nuruc` script is the prebuilt binary. Build is straightforward and works (verified exit 0).

---

## 8. docs/ intended design vs. actual implementation (per spec)

`docs/` has 5 spec files (architecture-v0.1, grammar-v0.3, language-v0.3, runtime-memory-v0.1, lexer-parser-v0.1) plus empty `decisions/` and `roadmap/`. Comparison:

**architecture/compiler-architecture-v0.1.md** — aspirational; partially realized:
- Spec pipeline has a separate **Name/Scope Resolution** stage and `compiler/resolver/` dir. Actual: name/scope resolution is folded into `semantic/`; no `resolver/` dir.
- Spec has a separate `compiler/codegen/` dir. Actual: codegen lives in `compiler/nir/nuru_codegen.c`.
- Spec NIR (§5) is register/CFG-oriented (`predecessor_true/false/successor`, "virtual register allocation", `NIR_AND`/`NIR_OR`, `NIR_NEW_LIST/MAP`+`LIST_GET/SET`/`MAP_GET/SET`, `NIR_NEW_FUNCTION`/`NIR_NEW_CLOSURE`). Actual NIR is a simpler **stack machine** (`NIR_LOAD_CONST`/`LOAD_VAR`/`STORE_VAR`, no local/global split, no `AND`/`OR`, no list/map SET, no closures/first-class functions).
- Spec codegen targets **both x86-64 Linux (ELF64) and Windows (COFF/PE, MS ABI)**. Actual: Linux ELF64 only (Windows is an empty `runtime/platform/windows/` stub).
- Spec object generator does custom ELF/COFF emission (sections/symbols/relocations). Actual: shells out to `as`/`ld`.
- Spec §7 "Runtime Integration" lists a garbage collector. Actual: refcounting only, no GC.
- Spec §4.4 semantic output = "annotated AST with resolved types". Actual: no type annotations.

**language/grammar-spec-v0.3.md**:
- Spec defines `index_access ::= expression 'at' expression` (word-based indexing). Actual: **bracket `[]` indexing**; there is no `at` token in the lexer at all. (README's own overview uses `cities[0]`, contradicting this spec.)
- `remove "X" from list` appears in language-spec §9.1 but is **absent from the grammar** and **not implemented** (the two specs disagree).
- Spec `unary ::= 'not' unary | primary` — no unary minus; actual **adds unary `minus`** (extra).
- Spec `literal` includes `'list' '[' … ']'`; actual implements only bare `[ … ]` and `map [ … ]` — the `list` keyword is **dead** (lexed, never used by the parser).
- Spec §7 says comparison operators are **not** left-associative; actual parses them left-associatively.
- Spec §8.4 says interpolation is parsed during tokenization; actual parses it in the parser.

**language/language-spec-v0.3.md**:
- Lists types Number (float), Function (first-class), Error (with message+stack). Actual: floats truncated to int; no first-class functions; no Error objects (error is a boolean flag).
- Lists List as "mutable"; actual lists are **value-semantics** (`append` allocates a new list, rebinds the variable).
- §9.2 shows `person["city"]` (map bracket access) and §10 `error.message`. Actual: only `.field` member access works; bracket-on-map mis-routes to list-element indexing; `error.message` would member-access an integer.
- §14/§15: "main returns Integer for exit status." Actual: main return is discarded; exit is always 0.

**language/runtime-memory-spec-v0.1.md** — describes a VM-era design, largely diverged:
- Tags include `NUMBER=3, FUNCTION=7, ERROR=8, CFUNC=9, NOREF=0xFF`. Actual implements only EMPTY/BOOLEAN/INTEGER/STRING/LIST/MAP; no float/function/error/cfunc tags.
- Per-type structs (`NuruString` with capacity+`data*`, `NuruFunction` with closure, `NuruError` with message/file/line/column/stack_trace), call frames, environments/closures, `NuruVM` state, and a **mark-and-sweep GC**. Actual: uniform inline-payload header, native stack (no call frames/VM), no closures, no Error objects, **refcounting + free-list instead of mark-and-sweep**. The README's statement that the runtime "implements the full tagged-value and reference-counting model of runtime-memory-spec-v0.1" is only half-accurate: the tagged-value idea matches, but the spec actually specifies **mark-and-sweep GC**, not refcounting.
- §8.3 lists both Linux SysV and Windows MS calling conventions. Actual: Linux SysV only.

**language/lexer-parser-spec-v0.1.md**:
- Token enum includes `TOKEN_ASSIGN (=)`, `TOKEN_COMMENT`; has `TOKEN_TIMES` twice (typo). Actual: no `=`, uses `TOKEN_HASH`/`MULTILINE_COMMENT_*`/`INTERPOLATION_*`, no duplicate.
- Spec lists lexer states incl. `STATE_STRING_INTERPOLATION`; actual `LexerState` enum has them but only `NORMAL` is used (vestigial).
- AST node types include `AST_PARAMETER`/`AST_ARGUMENT`; actual omits these (params are `AST_IDENTIFIER`; args are raw expressions) and adds `AST_DECLARATION_CHANGEABLE`/`AST_INTERP_STRING`/`AST_APPEND`.
- §6 source location = "file name, line, column". Actual: line/column only (no filename — single-file compiler).
- §7 memory = "arena allocator (lexer), pool allocator (parser)". Actual: plain `malloc`/`strdup` per node; no arena/pool.

---

## 9. Cross-cutting findings, known bugs, and code-quality notes

Confirmed defects / divergences:
1. **Optimizer disabled** in the driver (`/* nir_optimize(ir); */`), contradicting README's "wired end-to-end" claim.
2. **`and`/`or` not lowered** in IR — tokenized, parsed, semantically passed, but `emit_expr` drops them; `a and b` yields `b`. No test covers them.
3. **Floats broken** — lexed/parsed/lowered to `NIR_CONST_NUM`, but codegen truncates to int; no float test.
4. **`for each` over non-identifier iterables silently dropped** in IR.
5. **Bootstrap self-hosting e2e fails** (`make test-codegen-e2e`): stale expected `3,2,99` plus a real bootstrap-codegen indexing bug (`20,10,4,0`). Stray `tests/native/cases/cg_want.txt` is cruft.
6. **No type checking** (semantic = scope/structural only); main return ignored (exit always 0); map bracket access and `remove` not implemented; lists immutable (vs spec "mutable").
7. **`semantic_check_function` declared but undefined** (dead header symbol).
8. **`-Wformat` warnings** from unescaped `(%rsp)` in codegen `fprintf`s — harmless on glibc (emits literally) but undefined behavior / portability hazard.
9. **Fixed 1 MB heap, OOM returns NULL** with no graceful handling.
10. **`system()`-based as/ld** with shell-built commands (minor command-injection surface for untrusted paths).

Code-quality observations (for Dimension 3): ownership-correct manual memory management in AST/NIR; consistent tagged-value discipline; large hand-written assembly runtime in `codegen.c` is the highest-risk/most-complex code; vestigial enums/states (`INTERPOLATION_*`, `LexerState` non-NORMAL states, `list` keyword) should be pruned; IR printer prints operand pointers not values (weak `-i` debug output); AST printer is shallow (weak `-a` debug output); no per-stage unit tests (only e2e).

---

## 10. Proposed report skeleton (three dimensions + subsections)

This `investigation.md` is the raw material for **Dimension 1** and provides the skeleton for Dimensions 2 and 3.

### Dimension 1 — Project Status Report
1.1 Executive summary (one-paragraph headline: a working native-compiled language pipeline end-to-end; optimizer present but disabled; runtime/stdlib are empty dirs with the runtime actually emitted inline; bootstrap self-hosting e2e currently broken).
1.2 Subsystem status table (compiler/: driver, lexer, parser, ast, semantic, nir-nir/irgen/codegen, optimizer, object — status + 1-line evidence each, from §2).
1.3 Pipeline & execution model (driver flow, `_start: call main`, tagged-value runtime, refcounting + free-list, as/ld).
1.4 Runtime status (empty `runtime/` dir; inline-emitted runtime in codegen; what's implemented vs not — from §3).
1.5 Stdlib status (empty `stdlib/`; builtin-only surface; no modules/imports — §4).
1.6 Bootstrap status (stages 1–3 working; stage 4 e2e broken — §5).
1.7 Docs vs. implementation (intended-vs-actual deltas per spec — §8).
1.8 Feature coverage & known gaps (and/or, floats, for-each iterable, remove, map bracket, error.message, closures, Windows, GC, type checking — §9).
1.9 Build status (make builds; `make test` fails on codegen-e2e; warnings — §7, §0).

### Dimension 2 — Test Report
2.1 Test inventory & harness (`tests/native/` only; 53 cases; e2e script; empty per-stage dirs — §6).
2.2 Results (53/53 native PASS; AST PASS; codegen-e2e FAIL → `make test` non-zero — §0/§6).
2.3 Coverage map by feature (table mapping features → covering cases; note absence of float/and-or/remove/map-bracket/exit-code tests — §6).
2.4 The curated-subset caveat (pass rate reflects working subset, not completeness — §6 meta-finding).
2.5 Failures & regressions (codegen-e2e root cause: stale expectation + bootstrap indexing bug; stray `cg_want.txt` — §5/§6).
2.6 Test-infrastructure gaps (no unit tests; no exit-code coverage; e2e-only — §6).

### Dimension 3 — Code Quality / Review Report
3.1 Architecture assessment (clean stage separation in compiler/; NIR as stack machine vs spec's register/CFG design; runtime-as-emitted-assembly choice; driver simplicity).
3.2 Correctness risks (§9 items: optimizer disabled, and/or, floats, for-each, main-return, map bracket; `_print_value_raw` tag-0 ⇒ "empty" quirk for false comparison results).
3.3 Memory safety (ownership-correct AST/NIR free; refcounting correctness incl. literal-region exemption & recursive release; OOM-returns-NULL; frame zeroing on entry per README fix).
3.4 Maintainability (vestigial enums/states/keywords; dead `semantic_check_function`; shallow AST/IR printers; large monolithic codegen; duplicated `PRINT_INT`/`PRINT_STRING`).
3.5 Portability (Linux ELF64 only; glibc-specific `%r` literal emission; ASCII-only identifiers; fixed 1 MB heap).
3.6 Security (system()-based as/ld with shell-built paths; single-file compiler).
3.7 Doc hygiene (stale expectations; spec/impl disagreements across 5 docs; README claims vs reality — optimizer "wired", runtime "implements spec").
3.8 Recommendations (wire optimizer or remove from pipeline/docs; implement or remove and/or; fix float codegen or reject floats early; fix bootstrap codegen + update e2e expectation; prune dead tokens/states/symbols; add per-stage unit tests; resolve spec drift).

---

## 11. Headline summary (per subsystem)

- **driver**: fully implemented; orchestrates the full pipeline; optimizer call commented out; `main` return ignored.
- **lexer**: fully implemented; keywords, multi-word operators, numbers/floats, escapes, comments; vestigial interpolation tokens/states.
- **parser**: fully implemented; full precedence ladder, all statements, interpolation via recursive sub-parsing; mixed member/index chaining limited.
- **ast**: fully implemented; all node types, ownership-correct free; shallow printer.
- **semantic**: fully implemented for scope/structural checks; **no type system**; one dead header symbol.
- **nir (nir.c)**: fully implemented; stack-machine IR, basic blocks, string interning; structural-only printer.
- **nir (irgen)**: fully implemented with gaps — **`and`/`or` not lowered**, `for each` identifier-iterable only.
- **nir (codegen)**: fully implemented; complete x86-64 codegen + inline-emitted runtime (tagged values, refcounting, all builtins, attempt/recover); floats truncated to int; OOM returns NULL; format-string portability hazard.
- **optimizer**: fully implemented but **not wired** (commented out in driver).
- **object**: fully implemented (thin `as`/`ld` wrappers; no custom ELF/COFF).
- **runtime**: `runtime/` dir empty; runtime emitted inline by codegen; complete for the implemented value model; no GC/closures/Error objects/floats/Windows.
- **stdlib**: `stdlib/` dir empty; no standard library; builtin-only surface; no modules/imports.
- **bootstrap**: stages 1–3 (lexer/parser/semantic) working and tested; stage 4 codegen compiles/runs but **self-hosting e2e is broken** (stale expectation + indexing bug).
- **tests**: `tests/native/` only (53 e2e cases, all pass) + codegen-e2e (fails); no per-stage unit tests; suite curated to the working subset.
- **docs**: 5 specs describe a more ambitious design (VM, mark-sweep GC, closures, floats, Windows, custom object emission, type checking) than what is implemented; notable spec/impl drift.
