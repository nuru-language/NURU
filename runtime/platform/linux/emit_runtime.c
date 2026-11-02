#include "emit_runtime.h"
#include <stdio.h>
#include "nir/nuru_nir.h"

void nuru_emit_gc_data(FILE* out) {
    fprintf(out, "\n.data\n");
    fprintf(out, ".align 8\n");
    fprintf(out, "_gc_heap:\n");
    fprintf(out, "  .zero 1048576\n");
    fprintf(out, "_gc_top:\n");
    fprintf(out, "  .quad _gc_heap\n");
    fprintf(out, "_gc_free:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "_gc_lo:\n");
    fprintf(out, "  .quad _gc_heap\n");
    fprintf(out, "_gc_hi:\n");
    fprintf(out, "  .quad _gc_heap + 1048576\n");
    fprintf(out, ".text\n");
}

void nuru_emit_literal_data(FILE* out, struct NIRProgram* program) {
    fprintf(out, "\n.data\n");
    fprintf(out, "_newline:\n");
    fprintf(out, "  .byte 10\n");
    fprintf(out, "_err_jmp:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "_err_value:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "_err_line:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "_lit_lo:\n");

    for (size_t i = 0; i < program->constant_count; i++) {
        if (program->constants[i]->kind == NIR_CONST_STR) {
            fprintf(out, "_str%d:\n", (int)i);
            fprintf(out, "  .quad 0\n");
            fprintf(out, "  .quad %zu\n", program->constants[i]->string_value.length);
            fprintf(out, "  .byte ");
            const unsigned char* d = (const unsigned char*)program->constants[i]->string_value.data;
            size_t l = program->constants[i]->string_value.length;
            for (size_t k = 0; k < l; k++) {
                fprintf(out, "%s%u", k ? "," : "", (unsigned)d[k]);
            }
            fprintf(out, "\n");
        }
    }

    fprintf(out, "_str_true:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "  .quad 4\n");
    fprintf(out, "  .byte 't','r','u','e'\n");
    fprintf(out, "_str_false:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "  .quad 5\n");
    fprintf(out, "  .byte 'f','a','l','s','e'\n");
    fprintf(out, "_str_empty:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "  .quad 5\n");
    fprintf(out, "  .byte 'e','m','p','t','y'\n");
    fprintf(out, "_str_err_divzero:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "  .quad 16\n");
    fprintf(out, "  .byte 'd','i','v','i','s','i','o','n',' ','b','y',' ','z','e','r','o'\n");
    fprintf(out, "_str_err_overflow:\n");
    fprintf(out, "  .quad 0\n");
    fprintf(out, "  .quad 16\n");
    fprintf(out, "  .byte 'i','n','t','e','g','e','r',' ','o','v','e','r','f','l','o','w'\n");
    fprintf(out, "_lit_hi:\n");
}

void nuru_emit_io_helpers(FILE* out) {
    fprintf(out, "\n.globl _print_int\n");
    fprintf(out, "_print_int:\n");
    fprintf(out, "  push %%rbp\n");
    fprintf(out, "  mov %%rsp, %%rbp\n");
    fprintf(out, "  sub $40, %%rsp\n");
    fprintf(out, "  mov %%rdi, %%rax\n");
    fprintf(out, "  xor %%r9, %%r9\n");
    fprintf(out, "  test %%rax, %%rax\n");
    fprintf(out, "  jns .pi_prep\n");
    fprintf(out, "  mov $1, %%r9\n");
    fprintf(out, "  neg %%rax\n");
    fprintf(out, ".pi_prep:\n");
    fprintf(out, "  mov $10, %%rcx\n");
    fprintf(out, "  lea 39(%%rsp), %%r8\n");
    fprintf(out, "  movb $0, (%%r8)\n");
    fprintf(out, ".print_loop:\n");
    fprintf(out, "  xor %%rdx, %%rdx\n");
    fprintf(out, "  div %%rcx\n");
    fprintf(out, "  add $'0', %%dl\n");
    fprintf(out, "  dec %%r8\n");
    fprintf(out, "  mov %%dl, (%%r8)\n");
    fprintf(out, "  test %%rax, %%rax\n");
    fprintf(out, "  jnz .print_loop\n");
    fprintf(out, "  test %%r9, %%r9\n");
    fprintf(out, "  jz .pi_nosign\n");
    fprintf(out, "  dec %%r8\n");
    fprintf(out, "  movb $'-', (%%r8)\n");
    fprintf(out, ".pi_nosign:\n");
    fprintf(out, "  lea 39(%%rsp), %%rsi\n");
    fprintf(out, "  sub %%r8, %%rsi\n");
    fprintf(out, "  mov %%r8, %%rdi\n");
    fprintf(out, "  call _write_string\n");
    fprintf(out, "  mov %%rbp, %%rsp\n");
    fprintf(out, "  pop %%rbp\n");
    fprintf(out, "  ret\n");

    fprintf(out, "\n.globl _print_value_raw\n");
    fprintf(out, "_print_value_raw:\n");
    fprintf(out, "  mov %%rax, %%rdx\n");
    fprintf(out, "  and $0xff, %%edx\n");
    fprintf(out, "  cmp $2, %%rdx\n");
    fprintf(out, "  je .raw_int\n");
    fprintf(out, "  cmp $1, %%rdx\n");
    fprintf(out, "  je .raw_bool\n");
    fprintf(out, "  cmp $4, %%rdx\n");
    fprintf(out, "  je .raw_str\n");
    fprintf(out, "  cmp $7, %%rdx\n");
    fprintf(out, "  je .raw_err\n");
    fprintf(out, "  cmp $0, %%rdx\n");
    fprintf(out, "  je .raw_empty\n");
    fprintf(out, "  jmp .raw_int\n");
    fprintf(out, ".raw_err:\n");
    fprintf(out, "  shr $8, %%rax\n");
    fprintf(out, "  mov 8(%%rax), %%rax\n");
    fprintf(out, "  jmp .raw_str\n");
    fprintf(out, ".raw_int:\n");
    fprintf(out, "  sar $8, %%rax\n");
    fprintf(out, "  mov %%rax, %%rdi\n");
    fprintf(out, "  call _print_int\n");
    fprintf(out, "  ret\n");
    fprintf(out, ".raw_bool:\n");
    fprintf(out, "  shr $8, %%rax\n");
    fprintf(out, "  test %%rax, %%rax\n");
    fprintf(out, "  jz .raw_bfalse\n");
    fprintf(out, "  lea _str_true(%%rip), %%rdi\n");
    fprintf(out, "  mov $4, %%rsi\n");
    fprintf(out, "  call _write_string\n");
    fprintf(out, "  ret\n");
    fprintf(out, ".raw_bfalse:\n");
    fprintf(out, "  lea _str_false(%%rip), %%rdi\n");
    fprintf(out, "  mov $5, %%rsi\n");
    fprintf(out, "  call _write_string\n");
    fprintf(out, "  ret\n");
    fprintf(out, ".raw_str:\n");
    fprintf(out, "  shr $8, %%rax\n");
    fprintf(out, "  mov 8(%%rax), %%rsi\n");
    fprintf(out, "  lea 16(%%rax), %%rdi\n");
    fprintf(out, "  call _write_string\n");
    fprintf(out, "  ret\n");
    fprintf(out, ".raw_empty:\n");
    fprintf(out, "  lea _str_empty(%%rip), %%rdi\n");
    fprintf(out, "  mov $5, %%rsi\n");
    fprintf(out, "  call _write_string\n");
    fprintf(out, "  ret\n");

    fprintf(out, "\n.globl _print_value\n");
    fprintf(out, "_print_value:\n");
    fprintf(out, "  call _print_value_raw\n");
    fprintf(out, "  mov $1, %%rax\n");
    fprintf(out, "  mov $1, %%rdi\n");
    fprintf(out, "  lea _newline(%%rip), %%rsi\n");
    fprintf(out, "  mov $1, %%rdx\n");
    fprintf(out, "  syscall\n");
    fprintf(out, "  ret\n");

    fprintf(out, "\n.globl _write_string\n");
    fprintf(out, "_write_string:\n");
    fprintf(out, "  mov %%rdi, %%r10\n");
    fprintf(out, "  mov %%rsi, %%rdx\n");
    fprintf(out, "  mov $1, %%rdi\n");
    fprintf(out, "  mov %%r10, %%rsi\n");
    fprintf(out, "  mov $1, %%rax\n");
    fprintf(out, "  syscall\n");
    fprintf(out, "  ret\n");
}
