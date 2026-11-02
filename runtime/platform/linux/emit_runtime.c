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
