# NURU Compiler — Deep Investigative Report

**Date:** 2026-11-01 (executed 2026-11-02)
**Basis:** Every source file read directly; build and test commands run and output recorded verbatim. No claim is taken from prior summaries without independent file-read verification.

---

## 1. Project Status

### 1.1 Executive Summary

NURU is a natively-compiled, dynamically-typed scripting language with an English-like syntax, implemented in C11. The compiler pipeline — lexer → parser → AST → semantic analysis → NIR IR → x86-64 codegen → `as`/`ld` — is wired end-to-end and produces working Linux ELF64 executables. All 53 native end-to-end tests pass. The `runtime/` and `stdlib/` directories are entirely empty; the runtime is instead emitted as inline x86-64 assembly by `nuru_codegen.c`. The optimizer is fully written but permanently commented out in the driver. The bootstrap self-hosting end-to-end test fails due to a stale expected-output file and a real indexing bug in the bootstrap code generator. Four features that the language spec describes (`and`/`or`, floats, `remove`, map bracket access, error objects) are either silently incorrect or completely absent.

### 1.2 Subsystem Status

| Subsystem | Status | Evidence |
|-----------|--------|----------|
| **driver** (`compiler/driver/nuru.c`) | Fully implemented | 269 lines; orchestrates full pipeline; optimizer call commented out at line 214: `/* nir_optimize(ir); */` |
| **lexer** (`compiler/lexer/nuru_lexer.c`, `.h`) | Fully implemented with dead code | 462 + 124 lines; scans all tokens, multi-word operators, floats, escapes, comments; `TOKEN_INTERPOLATION_START/END` and `LEX_STATE_STRING/STRING_ESCAPE/INTERPOLATION/COMMENT/MULTILINE_COMMENT` are declared but never emitted (lexer only ever uses `LEX_STATE_NORMAL`); `skip_whitespace` line 39: `peek_next==# && peek_next==#` is a tautology |
| **parser** (`compiler/parser/nuru_parser.c`, `.h`) | Fully implemented | 927 + 22 lines; full precedence ladder; all statements; interpolation parsed via recursive sub-`Parser` |
| **ast** (`compiler/ast/nuru_ast.c`, `.h`) | Fully implemented | 459 + 196 lines; 30 node types; ownership-correct recursive free; `ast_print` is shallow (only prints node type + primary field, only `INTERP_STRING` recurses) |
| **semantic** (`compiler/semantic/nuru_semantic.c`, `.h`) | Fully implemented for scope/structure; no type system | 536 + 53 lines; scope stack, forward-ref pre-pass, arity checks on builtins/user functions, immutability, loop-depth for stop/skip; `semantic_check_function` declared in `.h` but never defined anywhere |
| **nir/nir.c** (`compiler/nir/nuru_nir.c`, `.h`) | Fully implemented | 241 + 146 lines; stack-machine IR; string constant interning; `nir_print_program` prints operand pointers not values |
| **nir/irgen** (`compiler/nir/nuru_irgen.c`, `.h`) | Mostly implemented; two silent gaps | 583 + 22 lines; `and`/`or` binary ops are tokenized and parsed but the `AST_BINARY_OP` switch in `emit_expr` has no case for them — both operands are emitted, no opcode is emitted, result is `b`; `for each` silently drops the loop body if the iterable is not a bare `AST_IDENTIFIER` (line 443: `if (!iterable || iterable->type != AST_IDENTIFIER) break;`) |
| **nir/codegen** (`compiler/nir/nuru_codegen.c`, `.h`) | Fully implemented; float truncation bug; format-string UB | 1896 + 17 lines; emits x86-64 assembly + entire inline runtime; floats truncated to int at line 64: `(long long)(int64_t)instr->a->number_value`; 5 `-Wformat` warnings for unescaped `(%rsp)` |
| **optimizer** (`compiler/optimizer/nuru_optimizer.c`, `.h`) | Fully implemented; disabled | 132 + 9 lines; constant folding, jump threading, dead-code truncation, jump-to-next trimming; never called (driver line 214) |
| **object** (`compiler/object/nuru_object.c`, `.h`) | Fully implemented (thin) | 15 + 12 lines; `system("as …")` and `system("ld …")` wrappers only |
| **runtime** (`runtime/`) | EMPTY | `find /home/error51/NURU/runtime -type f` returns nothing; runtime is emitted inline by codegen |
| **stdlib** (`stdlib/`) | EMPTY | `find /home/error51/NURU/stdlib -type f` returns nothing; no module/import mechanism exists |
| **bootstrap** | Stages 1–3 working; stage 4 e2e broken | `bootstrap/lexer.nuru` (135 lines), `parser_expr.nuru` (399), `parser.nuru` (995): all corresponding native tests pass; `codegen.nuru` (1269 lines) compiles and runs but `make test-codegen-e2e` fails |

### 1.3 Pipeline Flow

Actual call sequence in `compiler/driver/nuru.c` `main()`, top to bottom:

1. `parse_args(&opts, argc, argv)` — parses `-t/-a/-s/-i/-o/-h` flags and the input filename.
2. `read_file(opts.input_file, &source_size)` — reads entire source into a heap buffer.
3. `lexer_init(&lexer, source, source_size)` — if `-t` flag: scan and print token stream, then continue.
4. `parser_init(&parser, source, source_size)` then `parser_parse(&parser)` → `ASTNode* ast`.
5. If `-a` flag: call `ast_print()` on top-level statements (shallow).
6. `semantic_init(&analyzer)` then `semantic_analyze(&analyzer, ast)` — scope/structural checks.
7. `irgen_create()` then `irgen_lower(irgen, ast)` → `NIRProgram* ir`.
8. **`/* nir_optimize(ir); */`** — optimizer disabled, skipped.
9. If `-i` flag: `nir_print_program(ir)`, then exit (does not codegen).
10. `codegen_create(tmpasm, ir)` then `codegen_emit(cg)` — writes `.tmp` assembly file.
11. `nuru_assemble(tmpasm, objfile)` — calls `system("as -o …")`.
12. `nuru_link(objfile, outfile)` — calls `system("ld -o …")`.
13. `unlink(objfile)` — removes `.o` file; `.tmp` assembly is kept.
14. Cleanup and exit 0.

The generated entry point is `_start: call main; mov $60, %rax; xor %rdi, %rdi; syscall` — main's return value is discarded; the process always exits 0.

### 1.4 Value Model

Source: `compiler/nir/nuru_codegen.c`, lines 58–70 and the runtime emitter section.

All values are 64-bit words. The low 8 bits are the type tag; bits 8–63 are the payload:

| Tag | Type | Payload encoding |
|-----|------|-----------------|
| `0x00` | Empty | — (raw 0 prints as "empty") |
| `0x01` | Boolean | payload = 0 (false) or 1 (true); used only by `_print_value_raw`; comparison ops produce raw 0/1 (no tag) |
| `0x02` | Integer | payload = signed 56-bit integer (`value << 8 \| 2`); extracted with `sar $8, %rax` |
| `0x04` | String | payload = pointer to heap object shifted left 8, ORed with 4 (`ptr << 8 \| 4`) |
| `0x05` | List | payload = pointer to heap object shifted left 8, ORed with 5 |
| `0x06` | Map | payload = pointer to heap object shifted left 8, ORed with 6 |

**Comparison results** are emitted as raw 0 or 1 (no tag byte) via `sete/setne/setl…` + `movzbl`. The `JUMP_IF_FALSE` handler checks the tag byte first: if tag == 2, it right-shifts and tests the integer; otherwise it tests the raw value directly.

**Heap object layout** (uniform for string/list/map):
```
offset 0:  [8 bytes] ref_count
offset 8:  [8 bytes] length/count (byte count for strings, element count for lists/maps)
offset 16: [inline payload] bytes for strings; tagged values for lists; interleaved key/value tagged pairs for maps
```

There is no `capacity` field or separate `data` pointer — all payloads are stored inline in a single heap block.

The heap is a fixed 1 MB static `.bss` region (`_gc_heap .zero 1048576`). Allocation is bump-pointer with a free-list for recycled blocks. Literals in `.data` between `_lit_lo` and `_lit_hi` are exempt from reference counting. On OOM, `_alloc` returns 0 (NULL) with no error signalling.

### 1.5 Runtime

There is no separate runtime library. `compiler/nir/nuru_codegen.c`'s `codegen_emit()` function emits every helper function as inline x86-64 assembly text via `fprintf`. The complete list:

| Helper | Description |
|--------|-------------|
| `_start` | Entry point: calls `main`, then `exit(0)` via `syscall 60` |
| `_print_int` | Converts a raw (untagged) integer in `%rdi` to decimal ASCII and calls `_write_string` |
| `_print_value_raw` | Dispatches on tag byte in `%rax`: integer → `_print_int`; boolean → literal "true"/"false"; string → `_write_string`; tag 0 → "empty" |
| `_print_value` | Calls `_print_value_raw` then emits a newline syscall; used for `show` |
| `_write_string` | `write(1, buf, len)` syscall wrapper |
| `_checked_div` | Integer division `%rax / %rcx`; on zero divisor, jumps to `_err_jmp` target or returns 0 |
| `_checked_mod` | Integer modulo `%rax % %rcx`; same zero-divisor behaviour as `_checked_div` |
| `_read_line` | Allocates 528-byte heap object, calls `read(0, …, 511)`, strips trailing newline, returns tagged string |
| `_concat` | Allocates new string of combined length, copies both inputs, decrefs both inputs, returns tagged string |
| `_contains` | Naive substring search (byte-by-byte); returns tagged integer 0 or 1 |
| `_append` | Allocates new list of count+1 elements, copies existing elements (incref each), stores new value at tail, returns tagged list |
| `_substring` | Extracts a slice with bounds clamping; returns tagged string |
| `_upper` | Copies string to new allocation, converts `a-z` to `A-Z`; returns tagged string |
| `_lower` | Copies string to new allocation, converts `A-Z` to `a-z`; returns tagged string |
| `_index` | Finds first occurrence of needle string in haystack; returns tagged integer index or -1 |
| `_to_string` | Converts a tagged value to a string representation; returns tagged string |
| `_reverse` | Reverses a string byte-by-byte; returns new tagged string |
| `_read_file` | Opens file with `open` syscall, `fstat` for size, `read` entire contents; returns tagged string |
| `_write_file` | Opens file with `open+O_CREAT+O_WRONLY+O_TRUNC`, writes string payload; returns tagged integer |
| `_parse_int` | Converts ASCII digit sequence to integer; used by `to_number` |
| `_alloc` | Bump-pointer allocator over `_gc_heap`; falls back to free-list; on OOM returns 0 |
| `_incref` | Increments ref_count field at `(ptr >> 8)`; skips if pointer is in literal range `_lit_lo`–`_lit_hi` or tag < 4 |
| `_decref` | Decrements ref_count; if zero, recursively decrefs children (list elements, map values), then returns block to free-list |

### 1.6 Bootstrap Stages

All bootstrap files live in `bootstrap/` and are compiled by the C-built `nuruc`.

**Stage 1 — `lexer.nuru` (135 lines)**
Tokenizer for Nuru source using `ord`, `substring`, `to_number`, and maps. Produces a list of token maps with `kind`/`value` fields. Covered by native test `lexer`. **Status: PASS.**

**Stage 2a — `parser_expr.nuru` (399 lines)**
Recursive-descent expression parser: arithmetic (`plus`/`minus`/`times`), comparisons (`is`/`is above`/…), list and map literals. Covered by native test `parser`. **Status: PASS.**

**Stage 2–3 — `parser.nuru` (995 lines)**
Statement-level parser for `make`/`change`/`show`/`give`, `create` functions, `when`/`otherwise`, `while`, `for each`, `repeat`, `attempt`/`recover`, `stop`/`skip`. Also includes a semantic checker `analyze_program` that collects declared names and reports undeclared references. Covered by native tests `parser_stmt`, `parser_fn`, `parser_control`, `parser_cmp`, `parser_loops`, `parser_repeat`, `parser_attempt`, `parser_literals`, `parser_semantic`. **Status: all PASS.**

**Stage 4 — `codegen.nuru` (1269 lines)**
A full self-hosting compiler front-end plus x86-64 code generator: `lex` → `parse_program` → `gen_program`, with `gen_expr`/`gen_block`/`gen_function` and a `setcc_for` helper for comparison opcodes. Compiles and runs (native test `codegen` passes), but the **end-to-end self-hosting demo** (`tests/native/run_codegen_e2e.sh`, `make test-codegen-e2e`) **FAILS**.

Root cause of e2e failure (verified by running):
- The e2e script hardcodes expected output `3\n2\n99` in a heredoc.
- The actual generated program prints `20\n10\n4\n0`.
- This reflects two problems: (1) the expected value in the test script is stale (it does not match `codegen.nuru`'s current sample program), and (2) there is a real bug in the bootstrap code generator's list indexing — `a[2]` produces 10 instead of the expected 30, and `b[3]` produces 0 instead of 99.
- Additionally, `tests/native/cases/cg_want.txt` contains `7\n3\n2`, a leftover from a third, even older expected value — pure cruft, no script references it.

### 1.7 Spec vs. Implementation

#### `docs/architecture/compiler-architecture-v0.1.md`

| Spec claim | Actual implementation |
|------------|-----------------------|
| §3: separate `compiler/resolver/` directory for Name/Scope Resolution | Does not exist; resolution is folded into `semantic/` |
| §3: separate `compiler/codegen/` directory | Codegen lives in `compiler/nir/nuru_codegen.c` |
| §5 NIR: register-oriented CFG with `predecessor_true/false/successor`, virtual registers, `NIR_LOAD_LOCAL/STORE_LOCAL`, `NIR_LOAD_GLOBAL/STORE_GLOBAL`, `NIR_AND`, `NIR_OR`, `NIR_NEW_LIST`, `NIR_LIST_GET/SET`, `NIR_MAP_GET/SET`, `NIR_NEW_FUNCTION`, `NIR_NEW_CLOSURE` | Actual NIR is a stack machine with `NIR_LOAD_CONST/LOAD_VAR/STORE_VAR`; no local/global split; no `AND`/`OR` opcodes; no `LIST_SET`/`MAP_SET`; no closure/function opcodes |
| §6: targets both x86-64 Linux ELF64 and x86-64 Windows COFF/PE | Linux ELF64 only; `runtime/platform/windows/` is empty |
| §4.9 object generator: section construction, symbol table, relocation, ELF64/COFF | Shells out to `as` and `ld` via `system()` |
| §7 runtime: garbage collection | Reference counting + free-list only; no GC |
| §4.4 semantic output: "annotated AST with resolved types" | No type annotations; symbols carry no type |
| README claim: "the full pipeline — lexer through optimizer, codegen, and object/link stages — is wired end-to-end" | Optimizer is commented out in driver (line 214) |

#### `docs/language/grammar-spec-v0.3.md`

| Spec claim | Actual implementation |
|------------|-----------------------|
| §5 `index_access ::= expression 'at' expression` (word-based indexing) | Actual: bracket `[]` indexing; no `at` token in the lexer |
| §4 `remove … from list` (mentioned in language-spec §9.1) | Not present in grammar spec and not implemented |
| §5 `unary ::= 'not' unary \| primary` (no unary minus) | Actual parser adds unary `minus` (`parse_unary_expr` handles `TOKEN_MINUS`) |
| §5 `literal` includes `'list' '[' … ']'` (list keyword) | `TOKEN_LIST` is lexed but the parser never uses it for list literals; bare `[…]` is the actual syntax |
| §7 comparison operators: "not left-associative" | Actual: left-associative (standard while-loop in `parse_comparison_expr`) |
| §8.4 "String interpolation is parsed as part of string tokenization" | Actual: interpolation is parsed in the parser (`parse_interpolated_string` in `nuru_parser.c`) |

#### `docs/language/language-spec-v0.3.md`

| Spec claim | Actual implementation |
|------------|-----------------------|
| §3.1 type `Number` (IEEE 754 float), `Function` (first-class), `Error` (with message+stack) | Floats are truncated to int at codegen (codegen.c line 64); no first-class functions; error binding is integer `1`, not an Error object |
| §9.1 Lists are "mutable"; `remove "X" from list` | Lists have value semantics (append creates a new list); `remove` is not implemented anywhere |
| §9.2 `person["city"]` (map bracket access) | `AST_INDEX_ACCESS` lowers to `NIR_LOAD_ELT_EXPR` which treats the object as a list; bracket access on a map reads a list element, not a map entry |
| §10 `error.message` | Error binding is integer `1`; `.message` member-access on an integer reads a garbage heap pointer |
| §14/§15 "main returns Integer for exit status" | main's return value is discarded; exit code is always 0 (`_start` does `xor %rdi, %rdi; syscall`) |

#### `docs/language/runtime-memory-spec-v0.1.md`

| Spec claim | Actual implementation |
|------------|-----------------------|
| §1.2 tags include `NUMBER=0x03`, `FUNCTION=0x07`, `ERROR=0x08`, `CFUNC=0x09`, `NOREF=0xFF` | Actual: no float/function/error/cfunc tags exist |
| §2.1–2.5 per-type structs with `capacity` + `data*` pointer, separate `NuruFunction` with closure, `NuruError` with message/file/line/column | Actual: uniform inline-payload header; no capacity field; no data pointer; no closures; no Error objects |
| §3 `NuruCallFrame` structures, environments, closures | Actual: native x86-64 call stack; no VM call frames or environments |
| §5 mark-and-sweep GC | Actual: reference counting + bump-pointer free-list; no mark-and-sweep |
| §8.3 Windows MS calling convention | Linux SysV only |

#### `docs/language/lexer-parser-spec-v0.1.md`

| Spec claim | Actual implementation |
|------------|-----------------------|
| `TOKEN_ASSIGN (=)` in token enum | No `=` operator in actual lexer |
| `TOKEN_COMMENT` in token enum | Actual: `TOKEN_HASH` (skipped); `TOKEN_MULTILINE_COMMENT_START/END` |
| `TOKEN_TIMES` appears twice (duplicate in spec) | Actual: single occurrence, no duplicate |
| Lexer states `STATE_STRING_INTERPOLATION` active | Actual `LexerState` enum includes these but only `LEX_STATE_NORMAL` is ever used in `lexer_next_token` |
| AST node types `AST_PARAMETER`, `AST_ARGUMENT` | Not present; params are `AST_IDENTIFIER`; args are raw expressions |
| Actual missing from spec: `AST_DECLARATION_CHANGEABLE`, `AST_INTERP_STRING`, `AST_APPEND` | Present in actual code but absent from spec |
| §6 source location: "file name, line, column" | Actual: line and column only; no filename tracked (single-file compiler) |
| §7 "arena allocator (lexer), pool allocator (parser)" | Actual: plain `malloc`/`strdup` per node throughout |

### 1.8 Known Broken Features

| Feature | Bug description | Source location |
|---------|----------------|-----------------|
| `and`/`or` operators | Both operands emitted but no opcode generated; `a and b` evaluates to `b`; `a or b` evaluates to `b` | `nuru_irgen.c` `emit_expr()`, `AST_BINARY_OP` switch — no case for `"and"` or `"or"` |
| Float literals | Lexed as `TOKEN_NUMBER`, parsed as `AST_NUMBER_LITERAL`, lowered to `NIR_CONST_NUM`, then truncated to `int64_t` at codegen | `nuru_codegen.c` line 64: `(long long)(int64_t)instr->a->number_value` |
| `for each` over non-identifier iterables | Loop body silently dropped | `nuru_irgen.c` lines 443–444: `if (!iterable || iterable->type != AST_IDENTIFIER) break;` |
| Map bracket access `m["key"]` | `AST_INDEX_ACCESS` → `NIR_LOAD_ELT_EXPR` treats object as list; reads wrong offset | `nuru_irgen.c` `AST_INDEX_ACCESS` case emits `NIR_LOAD_ELT_EXPR`; `nuru_codegen.c` `NIR_LOAD_ELT_EXPR` does `mov 16(%rdx,%rcx,8)` treating it as a list |
| `error.message` | Error variable bound to integer `1`; `.message` member-access would dereference `1 >> 8 = 0` as a heap pointer, likely segfault | `nuru_irgen.c` `AST_ATTEMPT` case: `nir_emit(recover, NIR_LOAD_CONST, nir_const_int(irgen->program, 1), …)` |
| Exit codes | `main`'s return value always discarded | `nuru_codegen.c` `codegen_emit()`: `_start` does `xor %rdi, %rdi; syscall` unconditionally |
| Optimizer | Constant folding and jump threading never run | `nuru.c` line 214: `/* nir_optimize(ir); */` |
| Bootstrap e2e self-hosting | Stale expected output + real indexing bug in `codegen.nuru` | `tests/native/run_codegen_e2e.sh` hardcoded `3\n2\n99`; actual output `20\n10\n4\n0` |

### 1.9 Build Status

Command run: `make clean && make` in `/home/error51/NURU`

**Exit code: 0** (success)

Full output (verbatim):
```
rm -rf build nuruc
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/ast/nuru_ast.o compiler/ast/nuru_ast.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/driver/nuru.o compiler/driver/nuru.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/lexer/nuru_lexer.o compiler/lexer/nuru_lexer.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/nir/nuru_codegen.o compiler/nir/nuru_codegen.c
compiler/nir/nuru_codegen.c: In function 'codegen_emit':
compiler/nir/nuru_codegen.c:1082:31: warning: unknown conversion type character 'r' in format [-Wformat=]
 1082 |     fprintf(cg->out, "  mov (%rsp), %%rax\n");
compiler/nir/nuru_codegen.c:1546:31: warning: unknown conversion type character 'r' in format [-Wformat=]
 1546 |     fprintf(cg->out, "  mov (%rsp), %%rax\n");
compiler/nir/nuru_codegen.c:1589:31: warning: unknown conversion type character 'r' in format [-Wformat=]
 1589 |     fprintf(cg->out, "  mov (%rsp), %%rax\n");
compiler/nir/nuru_codegen.c:1619:32: warning: unknown conversion type character 'r' in format [-Wformat=]
 1619 |     fprintf(cg->out, "  mov 8(%rsp), %%rax\n");
compiler/nir/nuru_codegen.c:1621:31: warning: unknown conversion type character 'r' in format [-Wformat=]
 1621 |     fprintf(cg->out, "  mov (%rsp), %%rax\n");
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/nir/nuru_irgen.o compiler/nir/nuru_irgen.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/nir/nuru_nir.o compiler/nir/nuru_nir.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/object/nuru_object.o compiler/object/nuru_object.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/optimizer/nuru_optimizer.o compiler/optimizer/nuru_optimizer.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/parser/nuru_parser.o compiler/parser/nuru_parser.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o build/semantic/nuru_semantic.o compiler/semantic/nuru_semantic.c
gcc -Wall -Wextra -std=c11 -O2 -g -Icompiler -c -o nuruc build/ast/nuru_ast.o build/driver/nuru.o ...
```

5 `-Wformat` warnings in `nuru_codegen.c` — all from format strings like `"  mov (%rsp), %%rax\n"` where `%r` is an unknown conversion character. On glibc these emit the literal text `%r`, which happens to be valid x86-64 AT&T syntax, so the assembly is functionally correct. However this is **undefined behavior in C** and will break on alternative libc implementations.

---

## 2. Test Report

### 2.1 All 53 Test Cases — PASS/FAIL

Command run: `make test` in `/home/error51/NURU`. Output recorded verbatim.

| # | Test name | Result |
|---|-----------|--------|
| 1 | add | **PASS** |
| 2 | arithmetic | **PASS** |
| 3 | ask | **PASS** |
| 4 | attempt_catch | **PASS** |
| 5 | attempt_noerror | **PASS** |
| 6 | builtins | **PASS** |
| 7 | call_stmt | **PASS** |
| 8 | chars | **PASS** |
| 9 | codegen | **PASS** |
| 10 | comparisons | **PASS** |
| 11 | concat | **PASS** |
| 12 | conditions | **PASS** |
| 13 | contains | **PASS** |
| 14 | div_mod | **PASS** |
| 15 | else_branch | **PASS** |
| 16 | empty_list | **PASS** |
| 17 | escapes | **PASS** |
| 18 | forward_ref | **PASS** |
| 19 | gc_stress | **PASS** |
| 20 | hello | **PASS** |
| 21 | index | **PASS** |
| 22 | indexing | **PASS** |
| 23 | interpolation | **PASS** |
| 24 | io | **PASS** |
| 25 | len | **PASS** |
| 26 | lexer | **PASS** |
| 27 | list_for_each | **PASS** |
| 28 | list_sum | **PASS** |
| 29 | loop_break | **PASS** |
| 30 | loop_continue | **PASS** |
| 31 | map_field | **PASS** |
| 32 | negatives | **PASS** |
| 33 | nested_calls | **PASS** |
| 34 | nested_if | **PASS** |
| 35 | paren_grouping | **PASS** |
| 36 | parser | **PASS** |
| 37 | parser_attempt | **PASS** |
| 38 | parser_cmp | **PASS** |
| 39 | parser_control | **PASS** |
| 40 | parser_fn | **PASS** |
| 41 | parser_literals | **PASS** |
| 42 | parser_loops | **PASS** |
| 43 | parser_repeat | **PASS** |
| 44 | parser_semantic | **PASS** |
| 45 | parser_stmt | **PASS** |
| 46 | pow | **PASS** |
| 47 | repeat_loop | **PASS** |
| 48 | reverse | **PASS** |
| 49 | string_compare | **PASS** |
| 50 | substring | **PASS** |
| 51 | to_string | **PASS** |
| 52 | upper | **PASS** |
| 53 | while_loop | **PASS** |

**Native tests summary: 53 passed, 0 failed.**

Additionally: `test-ast` PASS (`nuruc -a examples/hello/hello.nuru` prints `FUNCTION_DECL (main)`).

`test-codegen-e2e`: **FAIL** — exit code 2, `make test` exits non-zero overall.

### 2.2 Codegen E2E — Actual vs. Expected Output

Command run: `./tests/native/run_codegen_e2e.sh` (inside `make test`).

**Expected** (from hardcoded heredoc in `run_codegen_e2e.sh`):
```
3
2
99
```

**Actual** (from the generated program):
```
20
10
4
0
```

The diff reported by the script:
```diff
--- /tmp/tmp.WjSAruLx9u/want.txt
+++ /tmp/tmp.WjSAruLx9u/out.txt
@@ -1,3 +1,4 @@
-3
-2
-99
+20
+10
+4
+0
```

The mismatch has two sources:
1. **Stale expected value.** The sample program in `codegen.nuru` was updated since the e2e script was written. The script was never updated to match.
2. **Bootstrap codegen indexing bug.** The sample in `codegen.nuru`'s `main` builds lists `a` and `b` and indexes them. Lines producing `10` and `0` indicate off-by-one or tag-stripping errors in the bootstrap code generator's list-index emission.

### 2.3 Coverage Gaps

Features with **zero test coverage** (verified by grepping all `tests/native/cases/*/program.nuru`):

| Feature | Evidence of gap |
|---------|----------------|
| `and` / `or` logical operators | No `program.nuru` uses `and` or `or` outside comments; grep returns empty |
| Float literals (e.g. `3.14`) | No `program.nuru` contains a decimal point in a number literal |
| `remove X from list` | No `program.nuru` uses `remove` |
| Map bracket access `m["key"]` | No `program.nuru` uses `["` on a map variable |
| `error.message` / error properties | No `program.nuru` accesses fields on error variable |
| Non-zero exit codes | No test case has an `exit_code` file; test harness defaults to 0 |
| `for each` over non-identifier iterable (e.g. `for each x in f with y`) | No test case exercises this path |
| Optimizer passes (constant folding, jump threading) | Optimizer is disabled; no test can exercise it |
| Mixed member/bracket chaining (e.g. `a[0].field`) | Not exercised; `parse_postfix` handles member then bracket in separate loops |

### 2.4 Test Infrastructure

**`tests/native/run_native_tests.sh`:**
- Iterates over all subdirectories of `tests/native/cases/` alphabetically.
- For each case: compiles `program.nuru` with `nuruc`; optionally supplies `input.txt` as stdin; diffs stdout against `expected.txt`; checks exit code against `exit_code` file (defaults to 0).
- Reports PASS/FAIL per case; exits non-zero if any case fails.

**`tests/native/run_codegen_e2e.sh`:**
- Compiles `bootstrap/codegen.nuru` with `nuruc`.
- Runs the resulting binary to emit assembly to stdout.
- Assembles and links the assembly with `as`/`ld`.
- Runs the resulting binary and diffs against a hardcoded heredoc (`3\n2\n99`).

**Makefile targets:**
- `make test` = `test-native` + `test-ast` + `test-codegen-e2e`
- `make test-native` = runs `run_native_tests.sh`
- `make test-ast` = `nuruc -a examples/hello/hello.nuru` (visual check only; always passes)
- `make test-codegen-e2e` = runs `run_codegen_e2e.sh`; currently fails

**No per-stage unit tests exist.** `tests/lexer/`, `tests/parser/`, `tests/semantic/`, `tests/compiler/`, `tests/runtime/`, `tests/regression/` are all empty directories.

---

## 3. Code Quality

### 3.1 Per-File Metrics and Assessment

| File | Lines | Assessment |
|------|-------|------------|
| `compiler/driver/nuru.c` | 269 | Clean driver; arg parsing, file reading, pipeline orchestration, and cleanup are straightforward. Two defects: optimizer commented out (line 214), and the `.tmp` assembly file is described as kept but the variable `asmfile` is freed unused. `derive_output_name` uses `snprintf` with a fixed +5 buffer which is just barely safe for `.tmp` but fragile. |
| `compiler/lexer/nuru_lexer.c` | 462 | Well-structured single-pass scanner. The `skip_whitespace` tautology bug (line 39: `peek_next == '#' && peek_next == '#'` always checks the same value twice; should be `peek(lexer)==peek_next(lexer)=='#'`) means `##` emits a stray `TOKEN_HASH` but `###` still works because a different code path handles it in `next_token_normal`. `TOKEN_INTERPOLATION_START/END` are never emitted. `LEX_STATE_*` beyond NORMAL are dead. |
| `compiler/lexer/nuru_lexer.h` | 124 | Well-typed header. `TOKEN_INTERPOLATION_START/END` and non-NORMAL `LexerState` values are vestigial. |
| `compiler/parser/nuru_parser.c` | 927 | Good recursive-descent structure. String interpolation uses a recursive sub-`Parser` — works correctly but is expensive (allocates a new Lexer/Parser per interpolated expression). `parse_postfix` handles member access then bracket access in two sequential `while` loops: `while(DOT) … while(LBRACKET) …` — this means `a.b.c` and `a[0][1]` work, but `a[0].b` does not chain (bracket loop has already consumed; restarting member loop cannot occur). Duplicate `TOKEN_LPAREN` check in `parse_primary_expr` (the `(expr)` case appears twice). |
| `compiler/parser/nuru_parser.h` | 22 | Minimal; clean. |
| `compiler/ast/nuru_ast.c` | 459 | Correct and complete. Every `ast_create_*` `strdup`s names and owns them. `ast_free` handles every node type with matching ownership. `ast_print` is a minimal stub — doesn't recurse into children beyond `INTERP_STRING`, making `-a` output nearly useless for debugging complex programs. |
| `compiler/ast/nuru_ast.h` | 196 | Defines all 30 node types as a tagged union. Uses a union without a discriminant for `declaration`/`declaration_changeable` (they have structurally identical union members, so `node->declaration.name` and `node->declaration_changeable.name` are aliased — harmless but confusing). |
| `compiler/semantic/nuru_semantic.c` | 536 | Correct scope stack with pre-pass for forward references. Builtin arity table covers all 19 builtins. `analyze_for_each` adds the loop variable to the *current* scope before the loop body — technically correct, but means the variable leaks into the enclosing scope. `semantic_check_function` is declared in the header but has no definition anywhere in the codebase. |
| `compiler/semantic/nuru_semantic.h` | 53 | Declares `semantic_check_function` which is never defined. |
| `compiler/nir/nuru_nir.c` | 241 | Clean implementation of the stack-machine IR. `nir_var_ref` allocates a new `NIRValue` for every use (`strdup` of the name) — these are never freed (they're not tracked in `program->constants`). Minor memory leak per variable reference. |
| `compiler/nir/nuru_nir.h` | 146 | Complete and accurate. No `NIR_AND`/`NIR_OR` opcodes (consistent with the `and`/`or` being silently dropped by irgen). |
| `compiler/nir/nuru_irgen.c` | 583 | The `and`/`or` gap (no case in the `AST_BINARY_OP` switch) and the `for each` identifier-only restriction are the two functional bugs. The `emit_stmt` for `AST_DECLARATION`/`AST_DECLARATION_CHANGEABLE` accesses `node->declaration.name` for both node types (works because the union layout is identical). `irgen_lower` does not emit a synthesized `IRGen` entry function — it lowers all top-level statements directly, which means any top-level non-function statement runs at load time with `irgen->current_func == NULL`, causing a crash; in practice all test programs wrap everything in `create main { }`. |
| `compiler/nir/nuru_irgen.h` | 22 | Clean. |
| `compiler/nir/nuru_codegen.c` | 1896 | The largest and most complex file. The hand-written x86-64 assembly emission is generally correct and consistent. Notable issues: (1) `NIR_PRINT_INT` and `NIR_PRINT_STRING` are byte-identical — both call `_print_value` — the distinction is meaningless; (2) 5 `-Wformat` warnings from unescaped `(%rsp)` format strings (UB in C); (3) `NIR_LOAD_MAP_FIELD` compares tagged key pointers — only works with interned string constants, not runtime-built strings; (4) `_concat` pops both input strings from the stack incorrectly: after computing the new string it accesses the saved stack values at `mov 8(%rsp), %rax` (offset 8 from current rsp) and `mov (%rsp), %rax` — this is the `-Wformat` line and the assembly reads from rsp, not from a C-level stack argument, so whether the save/restore of rdi/rsi is correct depends on the assembly rather than C; (5) `_append` size calculation `lea 88(,%r10,8), %rdi` — `88 = 16 (header) + 9*8 (capacity buffer)` — this over-allocates by a fixed amount each time but is safe. |
| `compiler/nir/nuru_codegen.h` | 17 | Minimal. |
| `compiler/optimizer/nuru_optimizer.c` | 132 | Well-written. Constant folding, jump threading, dead-code truncation, and jump-to-next trimming are all correct. `is_terminal` does not include `NIR_JUMP_IF_FALSE`/`NIR_JUMP_IF_TRUE` (conditional jumps are not terminals — correct) but also doesn't include `NIR_ERR_SAVE`/`NIR_ERR_RESTORE` — these are never generated after a real terminal, so no issue in practice. The optimizer is disabled so these subtleties are moot. |
| `compiler/optimizer/nuru_optimizer.h` | 9 | Declares `nir_optimize`. |
| `compiler/object/nuru_object.c` | 15 | Two functions, both shell out to `system()`. The return value of `system()` is propagated directly, which is the raw `waitpid` status — callers compare it to `0`, which works for normal cases but is not technically correct (should use `WEXITSTATUS`). |
| `compiler/object/nuru_object.h` | 12 | Correct declarations. |

### 3.2 Confirmed Bugs

| # | File | Line(s) | Description | Impact |
|---|------|---------|-------------|--------|
| 1 | `nuru_irgen.c` | `emit_expr`, `AST_BINARY_OP` switch (no `"and"`/`"or"` case) | `a and b` and `a or b` discard `a` and evaluate to `b`; no compile-time warning | Any program using `and`/`or` produces silently wrong results |
| 2 | `nuru_codegen.c` | line 64 | `NIR_CONST_NUM` handler: `(long long)(int64_t)number_value` truncates float to int | All float literals silently become integers |
| 3 | `nuru_irgen.c` | lines 443–444 | `for each` over non-identifier iterable silently breaks out of the `AST_FOR_EACH` case without emitting any code | Programs with `for each x in f()` or `for each x in lst.items` emit nothing for that loop |
| 4 | `nuru_codegen.c` | `NIR_LOAD_ELT_EXPR` case | Used for `a[i]` on any object; only correct for lists | Bracket indexing on a map produces wrong (list-semantics) results |
| 5 | `nuru_irgen.c` | `AST_ATTEMPT`, recover block | Error variable bound to integer constant `1` | `error.message` and any field access on error variable will dereference a near-null pointer (likely segfault) |
| 6 | `nuru.c` | line 214 | `/* nir_optimize(ir); */` | Optimizer never runs; contradicts README claim |
| 7 | `nuru_codegen.c` | `codegen_emit`, `_start` | `xor %rdi, %rdi; syscall` always exits 0, discarding `main`'s return value | Programs cannot communicate exit status to the shell |
| 8 | `nuru_lexer.c` | line 39 | `peek_next(lexer) == '#' && peek_next(lexer) == '#'` — both sides call the same function; always equivalent to `peek_next(lexer) == '#'` (never checks third `#`) | `##x` in source emits a stray `TOKEN_HASH` instead of being treated as a single-line comment; `###` still works through a separate code path |
| 9 | `nuru_semantic.h` | declaration of `semantic_check_function` | Function declared but not defined anywhere; calling it would produce a link error | Dead declaration; no current caller, so no runtime effect |
| 10 | `nuru_codegen.c` | lines 1082, 1546, 1589, 1619, 1621 | `fprintf` format strings `"  mov (%rsp), %%rax\n"` — `%r` is an unknown conversion character | Undefined C behavior; accidentally works on glibc which emits the literal `%r` text, producing valid assembly |
| 11 | `tests/native/run_codegen_e2e.sh` | heredoc | Expected output `3\n2\n99` is stale; actual output is `20\n10\n4\n0` | `make test` always exits non-zero |
| 12 | `bootstrap/codegen.nuru` | list indexing in `main` | Bootstrap code generator produces wrong values for `a[2]` (10 vs 30) and `b[3]` (0 vs 99) | Self-hosting e2e demo broken |
| 13 | `nuru_nir.c` | `nir_var_ref` | Allocates `NIRValue` with `strdup` but does not add it to `program->constants`; these are never freed | Memory leak per variable reference in NIR; benign for short-lived compiler runs |

### 3.3 Dead Code

| Symbol | File | Line(s) | Description |
|--------|------|---------|-------------|
| `TOKEN_INTERPOLATION_START` / `TOKEN_INTERPOLATION_END` | `nuru_lexer.h` | 78–79 | Declared in enum; never emitted by `lexer_next_token`; not handled in parser |
| `LEX_STATE_STRING` / `LEX_STATE_STRING_ESCAPE` / `LEX_STATE_INTERPOLATION` / `LEX_STATE_COMMENT` / `LEX_STATE_MULTILINE_COMMENT` | `nuru_lexer.h` | 100–105 | Non-NORMAL states declared in `LexerState` enum; `lexer_next_token` returns `TOKEN_EOF` for any non-NORMAL state; never transitioned to |
| `Lexer.interpolation_depth` | `nuru_lexer.h` | field in `Lexer` struct | Field initialized to 0, never read or written |
| `Lexer.error_token` | `nuru_lexer.h` | field in `Lexer` struct | Field declared; never used |
| `TOKEN_LIST` | `nuru_lexer.h` / `nuru_lexer.c` | 62 / ~118 | Lexed as a keyword; not used by parser for list literal parsing (bare `[…]` is the actual syntax) |
| `semantic_check_function` | `nuru_semantic.h` | declaration | Declared as `int semantic_check_function(SemanticAnalyzer*, ASTNode*)` but never defined anywhere in the codebase |
| `NIR_JUMP_IF_TRUE` | `nuru_nir.h` / `nuru_nir.c` | opcode | Defined in enum and has a `nir_opcode_name` case; emitted nowhere in `nuru_irgen.c`; handled in `nuru_codegen.c` (the code is present) but never triggered |
| `nir_print_program` operand values | `nuru_nir.c` | ~200 | Prints `a=%p b=%p` (raw pointers); not human-readable; `-i` flag output is structurally correct but useless for debugging values |
| Optimizer passes | `nuru_optimizer.c` | entire file | All four optimization passes are compiled in but never called (`driver/nuru.c` line 214) |
| `asmfile` variable | `nuru.c` | ~235 | Allocated as `outfile + ".s"` string but the variable is only freed, never used (the `.tmp` path is what gets written) |

### 3.4 Memory Safety

**Allocations assessed:**

- `ast_create_*` functions in `nuru_ast.c`: use `safe_malloc` (exits on NULL); `strdup_` is a safe wrapper; `ast_free` correctly frees all owned strings and recursively frees child nodes. **Correct.**

- `parser.c` `strndup_`: manually allocates `length+1` bytes; correct. `grow_array` uses `realloc`; correct.

- `nuru_nir.c` `nir_var_ref`: calls `strdup(name)` and allocates a `NIRValue` but does **not** add it to `program->constants`. These are leaked every time a variable is referenced in the IR. For a short-lived compiler run this is benign, but it would matter in a long-running process. **Minor leak.**

- `nuru_nir.c` `nir_program_free`: frees constants and instructions but **does not free `NIRValue.name`** for `NIR_VAR` values in instructions' `a`/`b` fields. `nir_var_ref` returns dynamically-allocated values not tracked in `constants`. **Confirmed leak.**

- `nuru_irgen.c` `IRGen` allocation: `irgen_create` uses `calloc`; `irgen_free` calls `nir_program_free` then `free(irgen)`. **Correct.**

- `nuru_codegen.c` `codegen_create`/`codegen_free`: opens file in create, closes in free. **Correct.**

- `nuru_optimizer.c` `optimize_block`: frees old instructions `a`, `b`, `c` when folded and allocates `merged`. Uses `malloc` for the output array. The `block->instructions` pointer is replaced and the old array freed. **Correct.**

- `nuru.c` heap strings (`outfile`, `asmfile`, `objfile`, `tmpasm`): all freed before return. `source` is freed. `ast` is freed. `analyzer` is freed via `semantic_free`. `irgen` is freed. **Correct.** Exception: on the early-return code paths (assemble/link failure), `outfile`/`asmfile`/`objfile`/`tmpasm` are **not freed** (resource leak on error path, lines ~240–255).

- Heap in generated programs: `_alloc` returns NULL on OOM; callers do not check the return value. Any subsequent pointer dereference of a null-tagged value will crash. **Potential crash on OOM in generated programs.**

### 3.5 Security: `system()` calls in `object.c`

`compiler/object/nuru_object.c` contains:

```c
int nuru_assemble(const char* asm_path, const char* obj_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "as -o \"%s\" \"%s\"", obj_path, asm_path);
    return system(cmd);
}

int nuru_link(const char* obj_path, const char* exe_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "ld -o \"%s\" \"%s\"", exe_path, obj_path);
    return system(cmd);
}
```

**Assessment:**

1. **Shell injection via filenames.** The paths are shell-quoted with double quotes, but a filename containing a double quote, backtick, `$(…)`, or other shell metacharacter would break out of the quoting and allow arbitrary command execution. Example: if `asm_path` is `foo").s; rm -rf ~; echo "`, the resulting shell command executes `rm -rf ~`. This is the classic `system()`-with-untrusted-string vulnerability. The paths originate from CLI arguments and `derive_output_name`, which does not sanitize.

2. **`system()` invokes `/bin/sh -c`**, which inherits the process's full environment including `PATH`, `LD_PRELOAD`, etc. A compromised environment could cause a different `as` or `ld` to run.

3. **Return value semantics.** `system()` returns the raw `waitpid` status. Callers compare to `0`, but `system()` returns 0 only if the child exited with status 0 *and* `waitpid` succeeded. The correct check is `WIFEXITED(ret) && WEXITSTATUS(ret) == 0`. Currently a child that is killed by a signal returns non-zero and is correctly treated as failure, but the error message is "Assembly failed" with no further detail.

4. **Impact:** This is a **low-severity** issue for the current intended use (a local developer tool compiling trusted source files). It would be **high-severity** if `nuruc` were ever exposed as a server-side compilation service.

**Recommendation:** Replace `system()` with `execvp()` + `fork()` + `waitpid()`, passing the arguments as a properly-typed array. This eliminates shell injection entirely.

### 3.6 Recommendations (Ordered by Impact)

1. **Fix `and`/`or` in `nuru_irgen.c`.** Add `NIR_AND`/`NIR_OR` opcodes to `nuru_nir.h` and implement them in `nuru_codegen.c` (short-circuit or eager; both are straightforward on the stack machine). This is the most user-visible silent correctness bug.

2. **Fix or explicitly reject floats.** Either implement float support throughout the pipeline (add `TAG_NUMBER=0x03`, use `xmm` registers in codegen) or add a parser/semantic error for `AST_NUMBER_LITERAL` nodes, preventing silent truncation. The silent truncation (`3.14` → `3`) is a trap.

3. **Fix the bootstrap codegen e2e test.** Two steps: (a) fix the list-indexing bug in `bootstrap/codegen.nuru` (examine `gen_expr` for index access); (b) update `run_codegen_e2e.sh`'s heredoc to match `codegen.nuru`'s current sample program. Delete `tests/native/cases/cg_want.txt`.

4. **Wire the optimizer.** Remove the `/* nir_optimize(ir); */` comment to enable it, add an e2e test that verifies a constant-folded program, and update the README. The optimizer is already correct.

5. **Fix the `for each` restriction.** `nuru_irgen.c` line 443: evaluate the iterable into a temporary variable and use that; remove the `AST_IDENTIFIER`-only guard.

6. **Replace `system()` in `object.c`** with `fork`/`execvp`/`waitpid` to eliminate shell injection.

7. **Fix `_start` to propagate exit code.** Change the emitted `_start` from `xor %rdi, %rdi; syscall` to `pop %rdi; mov $60, %rax; syscall` (use `main`'s return in `%rax` as the exit code).

8. **Fix the 5 `-Wformat` warnings in `nuru_codegen.c`.** Double the `%` to `%%` in strings like `"  mov (%rsp), %%rax\n"` (they should be `"  mov (%%rsp), %%rax\n"`). This eliminates UB and makes the code correct by construction.

9. **Remove dead code.** Prune `TOKEN_INTERPOLATION_START/END`, non-NORMAL `LexerState` values, `TOKEN_LIST` (or implement it), `semantic_check_function` declaration, the `asmfile` variable in `nuru.c`.

10. **Add unit tests for individual stages.** The current test suite is 100% end-to-end. Per-stage tests for the lexer, parser, semantic analyzer, and optimizer would catch regressions much earlier. The empty `tests/lexer/`, `tests/parser/`, `tests/semantic/` directories are already scaffolded for this purpose.

11. **Fix memory leaks in NIR.** Track `NIRValue` allocations from `nir_var_ref` in the program's constant pool, or add a separate free list, so `nir_program_free` releases them.

12. **Add tests for broken features.** Once fixed, add test cases for `and`/`or`, floats, non-zero exit codes, `for each` over expressions, and map bracket access. The current 53/53 pass rate reflects only the working subset.

---

*Report generated by read-only investigation: all claims verified against actual source files and test run output.*
