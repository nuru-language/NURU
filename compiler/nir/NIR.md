# Niru — Current NIR Specification (Stage B)

This document formalizes the current stack-machine NIR produced by
`compiler/nir/nuru_irgen.c` and consumed by `compiler/nir/nuru_codegen.c`.
It is the working contract for the native backend. A control-flow-graph
redesign (NIR v0.2) is tracked separately.

## 1. Program structure

- **NIRProgram**: an ordered list of `NIRFunction`s and a constant pool.
- **NIRFunction**: name, parameters, locals, and a linked list of basic
  blocks beginning at `entry`.
- **NIRBasicBlock**: a name and an ordered list of `NIRInstruction`s. Blocks
  execute sequentially; control transfer happens only via explicit jump or
  return instructions (falls through to the next block in memory only when the
  optimizer removes a trailing jump to the next block).
- **NIRInstruction**: `op`, optional value operands `a`/`b`, an optional block
  `target`, and a source line number.

## 2. Value operands

- `NIR_CONST_INT` — 64-bit integer constant (`integer_value`).
- `NIR_CONST_NUM` — double constant (`number_value`); currently rejected at
  semantic analysis (no Number support yet).
- `NIR_CONST_STR` — interned string constant (`data`, `length`).
- `NIR_VAR` — a reference to a function parameter or local by name.

## 3. Evaluation stack

The runtime model is a per-function evaluation stack. Most instructions pop
operands and push a result; `LOAD_*` and call results push; `STORE_*`,
`PRINT_*`, `DISCARD`, and returns pop. The native backend implements this as
the machine stack. Ownership: a value on the stack is a live reference;
`LOAD_VAR` increments, `STORE_VAR` releases the previous slot value, and
`DISCARD`/`PRINT_*` release the popped value.

## 4. Opcode reference

Legend: `→` means "pushes"; `←` means "pops". `a`,`b` are operands; `t` is a
target block.

### Constants, variables
| Opcode | Operands | Stack effect | Semantics |
|---|---|---|---|
| `LOAD_CONST` | a | → const | Push `a` |
| `LOAD_VAR` | a=var | → value | Push a reference to `a` (incref) |
| `STORE_VAR` | a=var | ← | Pop and store into `a` (release old) |

### Arithmetic (all pop right, then left; push result)
| Opcode | Operands | Stack effect | Notes |
|---|---|---|---|
| `ADD` `SUB` `MUL` | | ←, ← → | Integer arithmetic; MUL checks overflow |
| `DIV` `MOD` | | ←, ← → | `_checked_div/_mod`: divide-by-zero raises error |
| `NEG` | | ← → | Unary minus (range-checked) |
| `NOT` | | ← → | Logical not → bool |

### Comparison (pop right, then left; push boolean)
| Opcode | Stack effect |
|---|---|
| `CMP_EQ` `CMP_NE` | ←, ← → |
| `CMP_LT` `CMP_GT` `CMP_LE` `CMP_GE` | ←, ← → |

### Control flow
| Opcode | Operands | Semantics |
|---|---|---|
| `JUMP` | t | Unconditional jump |
| `JUMP_IF_FALSE` | t | Pop; jump if falsy; else continue |
| `JUMP_IF_TRUE` | t | Pop; jump if truthy; else continue |
| `CALL` | a=fn, b=argc | Pop `argc` args, call, push result |
| `RETURN` | | Function epilogue (no value) |
| `RETURN_VALUE` | | Pop result, epilogue |
| `HALT` | | Exit process (status 0) |

### Output / I/O
| Opcode | Stack effect | Semantics |
|---|---|---|
| `PRINT_INT` | ← | Print value + newline |
| `PRINT_STRING` | ← | Print string + newline |
| `PRINT_VALUE_RAW` | ← | Print value without newline (interpolation) |
| `PRINT_NEWLINE` | | Write newline |
| `READ_LINE` | → | Push a line from stdin |
| `READ_FILE` | ← → | Push file contents as string |
| `WRITE_FILE` | ←, ← | Write contents to path; push status |

### Builtin helpers (pop arguments in order, push result)
| Opcode | Arity | Semantics |
|---|---|---|
| `LEN` | 1 | Length of string/list/map else 0 |
| `ABS` `TO_NUMBER` `TO_STRING` `UPPER` `LOWER` `REVERSE` | 1 | Unary |
| `MIN` `MAX` `ORD`(1) `CHR`(1) `POW` `CONCAT` `CONTAINS` `INDEX` | 2 | Binary |
| `SUBSTRING` | 3 | `(s, start, end)` |
| `READ_FILE` | 1 | see above |
| `WRITE_FILE` | 2 | see above |

`ORD`/`CHR` are arity-1 (mapping char↔code); the table above lists
`ORD`/`CHR` as arity 1.

### Collections
| Opcode | Stack effect | Semantics |
|---|---|---|
| `ALLOC_LIST` | ← n values → | Pop `n` items (reverse order), push a list |
| `LOAD_LIST_LEN` | (a=var) → | Push element count of var |
| `LOAD_ELT` | (a=var) ← → | Pop index; push element of var |
| `LOAD_ELT_EXPR` | ←, ← → | Pop container, pop index/key; dispatch list/map |
| `ALLOC_MAP` | ← 2n values → | Pop `n` key/value pairs, push a map |
| `LOAD_MAP_FIELD` | (b=key) ← → | Pop object; push field (map lookup or error.message) |
| `APPEND` | a=var, ← | Pop value, append to var (new list) |
| `REMOVE` | ←, ← → | Pop list, pop value; push new list without first match |

### Errors
| Opcode | Operands | Semantics |
|---|---|---|
| `ERR_SAVE` | a=var | Save `_err_jmp` into local |
| `ERR_SET` | t | Set `_err_jmp` to `t` (recover block) |
| `ERR_RESTORE` | a=var | Restore `_err_jmp` from local |
| `LOAD_ERR` | | Pop `_err_value` (clears it); push error value |
| `DISCARD` | | Pop and release |

## 5. Error behavior

The runtime uses a cooperative, non-unwinding handler:
- `_checked_div`/`_checked_mod` and the integer-overflow checks build an Error
  value, store it in `_err_value`, then jump to `_err_jmp` (set by `ERR_SET`
  to the enclosing `recover` block). If no handler is active, they fall back
  to returning `0`.
- The `attempt` lowering saves/restores `_err_jmp` around the try body; the
  `recover` block loads `_err_value` via `LOAD_ERR` into the error variable.

## 6. Optimizer

`nir_optimize` performs peephole passes: constant folding (three-instruction
pattern, guarded to 56-bit results), jump threading, truncation after
terminals, and jump-to-next removal. The optimizer must preserve stack effects
and error semantics; it is disabled with the driver's `-O0` flag.

## 7. Runtime helper calls

Backend builtins map to runtime helpers (see `runtime/runtime.h`): `_alloc`,
`_incref`, `_decref`, `_print_value`, `_append`, `_remove`, `_reverse`,
`_index_get`, `_str_eq_bool`, `_mk_error`, `_checked_div`, `_checked_mod`,
`_checked_mul`, `_raise_overflow`, `_check_int`, and the string/file helpers.
