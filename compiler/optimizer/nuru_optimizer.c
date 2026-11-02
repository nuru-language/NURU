#define _DEFAULT_SOURCE
#include "nuru_optimizer.h"
#include <stdlib.h>
#include <stdio.h>
static int fits_int56(int64_t v) {
    return v >= -((int64_t)1 << 55) && v <= ((int64_t)1 << 55) - 1;
}

static int fold_value(NIRValue* a, NIRValue* b, NIROpcode op, int64_t* out) {
    if (!a || !b || a->kind != NIR_CONST_INT || b->kind != NIR_CONST_INT) {
        return 0;
    }
    int64_t x = a->integer_value;
    int64_t y = b->integer_value;
    int64_t r = 0;
    switch (op) {
        case NIR_ADD: r = x + y; break;
        case NIR_SUB: r = x - y; break;
        case NIR_MUL: r = x * y; break;
        case NIR_DIV:
            if (y == 0) return 0;
            r = x / y; break;
        case NIR_MOD:
            if (y == 0) return 0;
            r = x % y; break;
        default:
            return 0;
    }
    if (!fits_int56(r)) return 0;
    *out = r;
    return 1;
}

static void optimize_block(NIRProgram* program, NIRBasicBlock* block) {
    if (block->instruction_count < 3) return;

    NIRInstruction** out = malloc(sizeof(NIRInstruction*) * block->instruction_count);
    size_t n = 0;

    size_t i = 0;
    while (i < block->instruction_count) {
        NIRInstruction* a = block->instructions[i];
        NIRInstruction* b = i + 1 < block->instruction_count ? block->instructions[i + 1] : NULL;
        NIRInstruction* c = i + 2 < block->instruction_count ? block->instructions[i + 2] : NULL;

        if (a->op == NIR_LOAD_CONST &&
            b && b->op == NIR_LOAD_CONST &&
            c && (c->op == NIR_ADD || c->op == NIR_SUB || c->op == NIR_MUL ||
                  c->op == NIR_DIV || c->op == NIR_MOD)) {
            int64_t folded;
            if (fold_value(a->a, b->a, c->op, &folded)) {
                NIRInstruction* merged = calloc(1, sizeof(NIRInstruction));
                merged->op = NIR_LOAD_CONST;
                merged->a = nir_const_int(program, folded);
                merged->line = a->line;
                out[n++] = merged;

                free(a);
                free(b);
                free(c);
                i += 3;
                continue;
            }
        }

        out[n++] = a;
        i++;
    }

    free(block->instructions);
    block->instructions = out;
    block->instruction_count = n;
    block->instruction_capacity = n;
}

static NIRBasicBlock* resolve_target(NIRBasicBlock* b, int depth) {
    if (!b || depth > 32) return b;
    if (b->instruction_count == 1 &&
        b->instructions[0]->op == NIR_JUMP &&
        b->instructions[0]->target) {
        return resolve_target(b->instructions[0]->target, depth + 1);
    }
    return b;
}

static void thread_jumps(NIRBasicBlock* block) {
    for (size_t i = 0; i < block->instruction_count; i++) {
        NIRInstruction* instr = block->instructions[i];
        if (instr->target) {
            instr->target = resolve_target(instr->target, 0);
        }
    }
}

static void trim_jumps_to_next(NIRBasicBlock* block) {
    if (block->instruction_count == 0) return;
    NIRInstruction* last = block->instructions[block->instruction_count - 1];
    if (last->op == NIR_JUMP && last->target && last->target == block->next) {
        free(last);
        block->instruction_count--;
    }
}

static int is_terminal(NIROpcode op) {
    return op == NIR_JUMP || op == NIR_RETURN || op == NIR_RETURN_VALUE || op == NIR_HALT;
}

static void truncate_after_terminal(NIRBasicBlock* block) {
    for (size_t i = 0; i < block->instruction_count; i++) {
        if (is_terminal(block->instructions[i]->op)) {
            for (size_t k = i + 1; k < block->instruction_count; k++) {
                free(block->instructions[k]);
            }
            block->instruction_count = i + 1;
            return;
        }
    }
}

void nir_optimize(NIRProgram* program) {
    for (size_t f = 0; f < program->function_count; f++) {
        NIRFunction* func = program->functions[f];
        NIRBasicBlock* block = func->entry;
        while (block) {
            optimize_block(program, block);
            block = block->next;
        }
        block = func->entry;
        while (block) {
            thread_jumps(block);
            truncate_after_terminal(block);
            block = block->next;
        }
        block = func->entry;
        while (block) {
            trim_jumps_to_next(block);
            block = block->next;
        }
    }
}
