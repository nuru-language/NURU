#ifndef NURU_CODEGEN_H
#define NURU_CODEGEN_H

#include <stdio.h>
#include "nuru_nir.h"

typedef struct {
    NIRProgram* program;
    FILE* out;
    int label_counter;
} CodeGen;

CodeGen* codegen_create(const char* filename, NIRProgram* program);
void codegen_free(CodeGen* cg);
int codegen_emit(CodeGen* cg);

#endif
