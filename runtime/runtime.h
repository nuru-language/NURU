#ifndef NURU_RUNTIME_H
#define NURU_RUNTIME_H

/*
 * Nuru runtime interface.
 *
 * These prototypes describe the runtime helper ABI that the native code
 * generator emits calls to (see runtime/README.md for the value representation
 * and ownership conventions). The implementations are currently emitted inline
 * as x86-64 assembly by compiler/nir/nuru_codegen.c.
 *
 * Calling convention (System V, Linux x86-64): integer/pointer arguments in
 * rdi, rsi, rdx, rcx, r8, r9; result in rax; callee-saved registers preserved.
 * A returned object carries a fresh reference (see README section 4).
 */

typedef int64_t NuruValue;   /* tagged 64-bit value */

/* Allocation and reference counting. */
void*   rt_alloc(int64_t size);          /* returns object header pointer */
NuruValue rt_incref(NuruValue v);        /* returns v */
void    rt_decref(NuruValue v);

/* Printing and I/O. */
void    rt_print_value(NuruValue v);
void    rt_print_value_raw(NuruValue v);
void    rt_write_string(const char* s, int64_t len);
NuruValue rt_read_line(void);

/* String helpers. */
NuruValue rt_concat(NuruValue a, NuruValue b);
NuruValue rt_substring(NuruValue s, NuruValue start, NuruValue end);
NuruValue rt_upper(NuruValue s);
NuruValue rt_lower(NuruValue s);
NuruValue rt_index(NuruValue haystack, NuruValue needle);
NuruValue rt_contains(NuruValue haystack, NuruValue needle);
NuruValue rt_to_string(NuruValue n);
NuruValue rt_parse_int(const char* s, int64_t len);

/* Collections. */
NuruValue rt_append(NuruValue list, NuruValue value);
NuruValue rt_remove(NuruValue list, NuruValue value);
NuruValue rt_reverse(NuruValue v);       /* string or list */
NuruValue rt_index_get(NuruValue container, NuruValue index_or_key);

/* Errors. */
NuruValue rt_mk_error(NuruValue message, NuruValue category);

/* Arithmetic with runtime error checks. */
int64_t rt_checked_div(int64_t a, int64_t b);
int64_t rt_checked_mod(int64_t a, int64_t b);

/* File I/O. */
NuruValue rt_read_file(NuruValue path);
NuruValue rt_write_file(NuruValue path, NuruValue contents);

#endif /* NURU_RUNTIME_H */
