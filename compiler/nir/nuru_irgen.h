#ifndef NURU_IRGEN_H
#define NURU_IRGEN_H

#include "nuru_nir.h"
#include "../ast/nuru_ast.h"

typedef struct {
    NIRProgram* program;
    NIRFunction* current_func;
    NIRBasicBlock* current_block;
    int temp_counter;
    NIRBasicBlock* break_blocks[32];
    NIRBasicBlock* continue_blocks[32];
    int loop_depth;
    int current_line;
} IRGen;

IRGen* irgen_create(void);
void irgen_free(IRGen* irgen);

NIRProgram* irgen_lower(IRGen* irgen, ASTNode* program);

#endif
