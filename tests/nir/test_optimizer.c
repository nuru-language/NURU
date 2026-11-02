#include <stdio.h>
#include <assert.h>
#include "nir/nuru_nir.h"
#include "optimizer/nuru_optimizer.h"

static void test_constant_folding(void) {
    NIRProgram* p = nir_program_create();
    NIRFunction* f = nir_function_create(p, "fold");
    NIRBasicBlock* b = f->entry;
    nir_emit(b, NIR_LOAD_CONST, nir_const_int(p, 5), NULL, NULL, 0);
    nir_emit(b, NIR_LOAD_CONST, nir_const_int(p, 3), NULL, NULL, 0);
    nir_emit(b, NIR_ADD, NULL, NULL, NULL, 0);
    nir_emit(b, NIR_RETURN_VALUE, NULL, NULL, NULL, 0);
    nir_optimize(p);
    assert(b->instruction_count == 2);
    assert(b->instructions[0]->op == NIR_LOAD_CONST);
    assert(b->instructions[0]->a->kind == NIR_CONST_INT);
    assert(b->instructions[0]->a->integer_value == 8);
    assert(b->instructions[1]->op == NIR_RETURN_VALUE);
    nir_program_free(p);
    printf("PASS  optimizer_constant_folding\n");
}

static void test_div_by_zero_not_folded(void) {
    NIRProgram* p = nir_program_create();
    NIRFunction* f = nir_function_create(p, "guard");
    NIRBasicBlock* b = f->entry;
    nir_emit(b, NIR_LOAD_CONST, nir_const_int(p, 5), NULL, NULL, 0);
    nir_emit(b, NIR_LOAD_CONST, nir_const_int(p, 0), NULL, NULL, 0);
    nir_emit(b, NIR_DIV, NULL, NULL, NULL, 0);
    nir_emit(b, NIR_RETURN_VALUE, NULL, NULL, NULL, 0);
    nir_optimize(p);
    assert(b->instruction_count == 4);      /* must not fold, preserves runtime error */
    assert(b->instructions[2]->op == NIR_DIV);
    nir_program_free(p);
    printf("PASS  optimizer_div_zero_guard\n");
}

static void test_truncate_after_terminal(void) {
    NIRProgram* p = nir_program_create();
    NIRFunction* f = nir_function_create(p, "trunc");
    NIRBasicBlock* b = f->entry;
    nir_emit(b, NIR_LOAD_CONST, nir_const_int(p, 1), NULL, NULL, 0);
    nir_emit(b, NIR_RETURN_VALUE, NULL, NULL, NULL, 0);
    nir_emit(b, NIR_LOAD_CONST, nir_const_int(p, 99), NULL, NULL, 0);
    nir_optimize(p);
    assert(b->instruction_count == 2);
    assert(b->instructions[1]->op == NIR_RETURN_VALUE);
    nir_program_free(p);
    printf("PASS  optimizer_truncate_after_terminal\n");
}

static void test_jump_threading(void) {
    NIRProgram* p = nir_program_create();
    NIRFunction* f = nir_function_create(p, "thread");
    NIRBasicBlock* entry = f->entry;
    NIRBasicBlock* stub = nir_block_create(f, "stub");
    NIRBasicBlock* end = nir_block_create(f, "end");
    nir_emit(entry, NIR_JUMP, NULL, NULL, stub, 0);
    nir_emit(stub, NIR_JUMP, NULL, NULL, end, 0);
    nir_emit(end, NIR_HALT, NULL, NULL, NULL, 0);
    nir_optimize(p);
    assert(entry->instructions[0]->op == NIR_JUMP);
    assert(entry->instructions[0]->target == end);
    nir_program_free(p);
    printf("PASS  optimizer_jump_threading\n");
}

static void test_jump_to_next_removed(void) {
    NIRProgram* p = nir_program_create();
    NIRFunction* f = nir_function_create(p, "trim");
    NIRBasicBlock* a = f->entry;
    NIRBasicBlock* b = nir_block_create(f, "next");
    nir_emit(a, NIR_LOAD_CONST, nir_const_int(p, 1), NULL, NULL, 0);
    nir_emit(a, NIR_JUMP, NULL, NULL, b, 0);
    nir_emit(b, NIR_RETURN_VALUE, NULL, NULL, NULL, 0);
    nir_optimize(p);
    assert(a->instruction_count == 1);      /* trailing jump to next block removed */
    assert(a->instructions[0]->op == NIR_LOAD_CONST);
    nir_program_free(p);
    printf("PASS  optimizer_jump_to_next\n");
}

int main(void) {
    test_constant_folding();
    test_div_by_zero_not_folded();
    test_truncate_after_terminal();
    test_jump_threading();
    test_jump_to_next_removed();
    printf("optimizer unit tests: 5 passed\n");
    return 0;
}
