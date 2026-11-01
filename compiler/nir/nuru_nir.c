#define _DEFAULT_SOURCE
#include "nuru_nir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

NIRProgram* nir_program_create(void) {
    NIRProgram* p = calloc(1, sizeof(NIRProgram));
    return p;
}

void nir_program_free(NIRProgram* program) {
    for (size_t i = 0; i < program->constant_count; i++) {
        free(program->constants[i]);
    }
    free(program->constants);
    
    for (size_t i = 0; i < program->function_count; i++) {
        NIRFunction* f = program->functions[i];
        NIRBasicBlock* b = f->entry;
        while (b) {
            for (size_t j = 0; j < b->instruction_count; j++) {
                free(b->instructions[j]);
            }
            free(b->instructions);
            NIRBasicBlock* next = b->next;
            free(b);
            b = next;
        }
        free(f->params);
        free(f->locals);
        free(f);
    }
    free(program->functions);
    free(program);
}

NIRFunction* nir_function_create(NIRProgram* program, const char* name) {
    NIRFunction* f = calloc(1, sizeof(NIRFunction));
    f->name = strdup(name);
    f->program = program;
    
    f->entry = nir_block_create(f, "entry");
    f->tail = f->entry;
    
    program->function_count++;
    program->functions = realloc(program->functions, sizeof(NIRFunction*) * program->function_count);
    program->functions[program->function_count - 1] = f;
    
    return f;
}

NIRBasicBlock* nir_block_create(NIRFunction* func, const char* name) {
    NIRBasicBlock* b = calloc(1, sizeof(NIRBasicBlock));
    char buf[128];
    snprintf(buf, sizeof(buf), "%s_%zu", name, func->program->block_counter++);
    b->name = strdup(buf);
    
    if (func->tail) {
        func->tail->next = b;
    }
    func->tail = b;
    
    return b;
}

NIRInstruction* nir_emit(NIRBasicBlock* block, NIROpcode op, NIRValue* a, NIRValue* b, NIRBasicBlock* target, int line) {
    if (block->instruction_count >= block->instruction_capacity) {
        block->instruction_capacity = block->instruction_capacity == 0 ? 8 : block->instruction_capacity * 2;
        block->instructions = realloc(block->instructions, sizeof(NIRInstruction*) * block->instruction_capacity);
    }
    
    NIRInstruction* instr = calloc(1, sizeof(NIRInstruction));
    instr->op = op;
    instr->a = a;
    instr->b = b;
    instr->target = target;
    instr->line = line;
    
    block->instructions[block->instruction_count++] = instr;
    return instr;
}

NIRValue* nir_const_int(NIRProgram* program, int64_t value) {
    NIRValue* v = calloc(1, sizeof(NIRValue));
    v->kind = NIR_CONST_INT;
    v->integer_value = value;
    
    program->constant_count++;
    program->constants = realloc(program->constants, sizeof(NIRValue*) * program->constant_count);
    program->constants[program->constant_count - 1] = v;
    return v;
}

NIRValue* nir_const_num(NIRProgram* program, double value) {
    NIRValue* v = calloc(1, sizeof(NIRValue));
    v->kind = NIR_CONST_NUM;
    v->number_value = value;
    
    program->constant_count++;
    program->constants = realloc(program->constants, sizeof(NIRValue*) * program->constant_count);
    program->constants[program->constant_count - 1] = v;
    return v;
}

NIRValue* nir_const_str(NIRProgram* program, const char* data, size_t length) {
    for (size_t i = 0; i < program->constant_count; i++) {
        NIRValue* existing = program->constants[i];
        if (existing->kind == NIR_CONST_STR &&
            existing->string_value.length == length &&
            strncmp(existing->string_value.data, data, length) == 0) {
            return existing;
        }
    }

    NIRValue* v = calloc(1, sizeof(NIRValue));
    v->kind = NIR_CONST_STR;
    v->string_value.data = data;
    v->string_value.length = length;
    
    program->constant_count++;
    program->constants = realloc(program->constants, sizeof(NIRValue*) * program->constant_count);
    program->constants[program->constant_count - 1] = v;
    return v;
}

NIRValue* nir_var_ref(const char* name) {
    NIRValue* v = calloc(1, sizeof(NIRValue));
    v->kind = NIR_VAR;
    v->name = strdup(name);
    return v;
}

void nir_function_add_param(NIRFunction* func, const char* name) {
    func->param_count++;
    func->params = realloc(func->params, sizeof(char*) * func->param_count);
    func->params[func->param_count - 1] = strdup(name);
}

void nir_function_add_local(NIRFunction* func, const char* name) {
    if (func->local_count >= func->local_capacity) {
        func->local_capacity = func->local_capacity == 0 ? 8 : func->local_capacity * 2;
        func->locals = realloc(func->locals, sizeof(char*) * func->local_capacity);
    }
    func->locals[func->local_count++] = strdup(name);
}

int nir_function_has_var(NIRFunction* func, const char* name) {
    for (size_t i = 0; i < func->param_count; i++) {
        if (strcmp(func->params[i], name) == 0) return 1;
    }
    for (size_t i = 0; i < func->local_count; i++) {
        if (strcmp(func->locals[i], name) == 0) return 1;
    }
    return 0;
}

const char* nir_opcode_name(NIROpcode op) {
    switch (op) {
        case NIR_LOAD_CONST: return "LOAD_CONST";
        case NIR_LOAD_VAR: return "LOAD_VAR";
        case NIR_STORE_VAR: return "STORE_VAR";
        case NIR_ADD: return "ADD";
        case NIR_SUB: return "SUB";
        case NIR_MUL: return "MUL";
        case NIR_DIV: return "DIV";
        case NIR_MOD: return "MOD";
        case NIR_NEG: return "NEG";
        case NIR_NOT: return "NOT";
        case NIR_CMP_EQ: return "CMP_EQ";
        case NIR_CMP_NE: return "CMP_NE";
        case NIR_CMP_LT: return "CMP_LT";
        case NIR_CMP_GT: return "CMP_GT";
        case NIR_CMP_LE: return "CMP_LE";
        case NIR_CMP_GE: return "CMP_GE";
        case NIR_JUMP: return "JUMP";
        case NIR_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case NIR_JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case NIR_CALL: return "CALL";
        case NIR_RETURN: return "RETURN";
        case NIR_RETURN_VALUE: return "RETURN_VALUE";
        case NIR_PRINT_INT: return "PRINT_INT";
        case NIR_PRINT_STRING: return "PRINT_STRING";
        case NIR_PRINT_VALUE_RAW: return "PRINT_VALUE_RAW";
        case NIR_PRINT_NEWLINE: return "PRINT_NEWLINE";
        case NIR_READ_LINE: return "READ_LINE";
        case NIR_LEN: return "LEN";
        case NIR_ABS: return "ABS";
        case NIR_MIN: return "MIN";
        case NIR_MAX: return "MAX";
        case NIR_TO_NUMBER: return "TO_NUMBER";
        case NIR_CONCAT: return "CONCAT";
        case NIR_ORD: return "ORD";
        case NIR_CHR: return "CHR";
        case NIR_ALLOC_LIST: return "ALLOC_LIST";
        case NIR_LOAD_LIST_LEN: return "LOAD_LIST_LEN";
        case NIR_LOAD_ELT: return "LOAD_ELT";
        case NIR_ALLOC_MAP: return "ALLOC_MAP";
        case NIR_LOAD_MAP_FIELD: return "LOAD_MAP_FIELD";
        case NIR_APPEND: return "APPEND";
        case NIR_ERR_SAVE: return "ERR_SAVE";
        case NIR_ERR_SET: return "ERR_SET";
        case NIR_ERR_RESTORE: return "ERR_RESTORE";
        case NIR_HALT: return "HALT";
        default: return "UNKNOWN";
    }
}

void nir_print_program(NIRProgram* program) {
    printf("=== NIR ===\n");
    for (size_t i = 0; i < program->function_count; i++) {
        NIRFunction* f = program->functions[i];
        printf("FUNCTION %s (%zu params, %zu locals)\n", f->name, f->param_count, f->local_count);
        NIRBasicBlock* b = f->entry;
        while (b) {
            printf("  BLOCK %s\n", b->name);
            for (size_t j = 0; j < b->instruction_count; j++) {
                NIRInstruction* instr = b->instructions[j];
                printf("    %s", nir_opcode_name(instr->op));
                if (instr->a) printf(" a=%p", (void*)instr->a);
                if (instr->b) printf(" b=%p", (void*)instr->b);
                if (instr->target) printf(" ->%s", instr->target->name);
                printf("\n");
            }
            b = b->next;
        }
    }
    printf("=== END NIR ===\n");
}
