#include "emit_runtime.h"
#include <stdio.h>

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
