# Nuru Runtime

This directory documents the runtime that every Nuru executable embeds. The
runtime is emitted by the native code generator (`compiler/nir/nuru_codegen.c`)
as inline x86-64 assembly inside each generated executable, so a Nuru program
is fully self-contained and has no dependency on any external runtime or
interpreted language.

## 1. Value representation

A Nuru value is a single 64-bit word: a tagged payload.

```
value = (payload << 8) | tag
```

Tags currently defined:

| Tag | Meaning       | Payload |
|-----|---------------|---------|
| 0   | empty         | unused (0) |
| 1   | boolean       | 0 or 1 |
| 2   | integer       | signed 64-bit integer |
| 4   | string        | pointer to string object |
| 5   | list          | pointer to list object |
| 6   | map           | pointer to map object |
| 7   | error         | pointer to error object |

For object kinds (4–7), the payload is the address of the object header. For
integer and boolean, the payload is the value itself. `0` (all bits clear)
denotes the empty value.

## 2. Object layout

All heap objects share a 16-byte header followed by type-specific data. The
object pointer returned by allocation points at the header (offset 0).

| Offset | Field |
|--------|-------|
| +0     | reference count (int64) |
| +8     | length/count (int64) |
| +16    | type-specific data |

- **String** (tag 4): `+8` = byte length (UTF-8); bytes at `+16`.
- **List** (tag 5): `+8` = element count; each element is a tagged value at
  `+16 + i*8`.
- **Map** (tag 6): `+8` = entry count; entries are stored as contiguous
  key/value pairs: key at `+16 + 2i*8`, value at `+16 + (2i+1)*8`. Keys are
  case-sensitive strings.
- **Error** (tag 7): `+8` = message (tagged string); `+16` = category
  (tagged integer).

## 3. Allocation

`_alloc(rdi = request_size)` returns an object pointer in `rax`.

- Objects are carved from a single 1 MiB static arena (`_gc_heap`) via a bump
  pointer (`_gc_top`), and freed blocks are recycled through a free list
  (`_gc_free`) using first-fit selection.
- Request sizes are aligned up to 16 bytes; a 32-byte block header sits before
  the returned object pointer (used by the free list).
- Allocation failure returns `0` (the runtime does not yet recover from
  allocation exhaustion; this is a known limitation).

Object pointers are treated as heap objects only when they fall between
`_lit_lo` and `_lit_hi`. String literals stored in the program's `.data`
section live inside that range and are exempt from reference counting.

## 4. Memory management: reference counting

The runtime currently uses **reference counting** (not tracing garbage
collection).

- `_incref(rax)` increments the refcount of a heap object (tags 4–7);
  literals are ignored.
- `_decref(rax)` decrements; when it reaches zero the object is released:
  list elements and map key/value children are recursively released, and the
  block is pushed onto the free list.

Ownership convention used by the generated code: functions and builtins that
return a freshly built object transfer a single reference; callers that store
it into a variable or push it on the evaluation stack take over that
reference. `LOAD_VAR`/`STORE_VAR` increment/decrement accordingly.

The language's value semantics make cyclic structures effectively
unconstructible through normal operations (append/remove/reverse return new
collections rather than mutating shared ones), so reference counting is
sound for the currently expressible programs.

## 5. Runtime helper ABI (System V, Linux x86-64)

Helpers are the compiler–runtime interface. Arguments in `rdi/rsi/rdx/rcx`,
return in `rax`. Callee-saved registers are preserved. Ownership follows
section 4.

Core helpers: `_alloc`, `_incref`, `_decref`, `_print_value`,
`_print_value_raw`, `_write_string`, `_read_line`, `_concat`, `_substring`,
`_upper`, `_lower`, `_index`, `_contains`, `_to_string`, `_parse_int`,
`_append`, `_remove`, `_reverse`, `_index_get`, `_str_eq_bool`,
`_mk_error`, `_checked_div`, `_checked_mod`, `_read_file`, `_write_file`.

## 6. Platform

- **Linux x86-64 / ELF64**: current backend (this runtime).
- **Windows x86-64 / PE**: planned; `platform/windows/` will hold the PE
  runtime interface when implemented.

A separate, Nuru-owned runtime library (with tracing GC and a clean C
interface) is the long-term target; this document is the contract that
extraction will target.
