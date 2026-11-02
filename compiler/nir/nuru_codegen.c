#define _DEFAULT_SOURCE
#include "nuru_codegen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int get_const_index(CodeGen* cg, NIRValue* val) {
    for (size_t i = 0; i < cg->program->constant_count; i++) {
        if (cg->program->constants[i] == val) return (int)i;
    }
    return -1;
}

static int get_var_slot(NIRFunction* func, const char* name) {
    for (size_t i = 0; i < func->param_count; i++) {
        if (strcmp(func->params[i], name) == 0) return (int)(i + 1) * 8;
    }
    for (size_t i = 0; i < func->local_count; i++) {
        if (strcmp(func->locals[i], name) == 0) return (int)(func->param_count + i + 1) * 8;
    }
    return 0;
}

static void emit_func(CodeGen* cg, NIRFunction* func) {
    char label[256];
    snprintf(label, sizeof(label), ".%s", func->name);
    
    fprintf(cg->out, "\n.globl %s\n", func->name);
    fprintf(cg->out, "%s:\n", func->name);
    
    fprintf(cg->out, "  push %%rbp\n");
    fprintf(cg->out, "  mov %%rsp, %%rbp\n");
    
    int frame_size = (int)(func->param_count + func->local_count) * 8 + 16;
    fprintf(cg->out, "  sub $%d, %%rsp\n", frame_size);
    fprintf(cg->out, "  mov $%d, %%r10\n", frame_size);
    fprintf(cg->out, "  lea (%%rsp), %%r11\n");
    fprintf(cg->out, "  xor %%eax, %%eax\n");
    fprintf(cg->out, ".frame_zero_%s:\n", func->name);
    fprintf(cg->out, "  mov %%rax, (%%r11)\n");
    fprintf(cg->out, "  add $8, %%r11\n");
    fprintf(cg->out, "  sub $8, %%r10\n");
    fprintf(cg->out, "  jnz .frame_zero_%s\n", func->name);
    
    const char* arg_regs[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    for (size_t i = 0; i < func->param_count && i < 6; i++) {
        fprintf(cg->out, "  mov %s, -%d(%%rbp)\n", 
                arg_regs[i], (int)(i + 1) * 8);
    }
    
    NIRBasicBlock* block = func->entry;
    while (block) {
        fprintf(cg->out, "\n.L%s_%s:\n", func->name, block->name);
        
        for (size_t i = 0; i < block->instruction_count; i++) {
            NIRInstruction* instr = block->instructions[i];
            
            switch (instr->op) {
                case NIR_LOAD_CONST:
                    if (instr->a->kind == NIR_CONST_INT) {
                        long long v = instr->a->integer_value;
                        fprintf(cg->out, "  mov $%lld, %%rax\n", (long long)((v << 8) | 2));
                    } else if (instr->a->kind == NIR_CONST_NUM) {
                        long long v = (long long)(int64_t)instr->a->number_value;
                        fprintf(cg->out, "  mov $%lld, %%rax\n", (long long)((v << 8) | 2));
                    } else if (instr->a->kind == NIR_CONST_STR) {
                        int idx = get_const_index(cg, instr->a);
                        fprintf(cg->out, "  lea _str%d(%%rip), %%rax\n", idx);
                        fprintf(cg->out, "  shl $8, %%rax\n");
                        fprintf(cg->out, "  or $4, %%rax\n");
                    }
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_LOAD_VAR:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rax\n", slot);
                        fprintf(cg->out, "  call _incref\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_STORE_VAR:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  pop %%rax\n");
                        fprintf(cg->out, "  mov %%rax, %%r11\n");
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rax\n", slot);
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, "  mov %%r11, -%d(%%rbp)\n", slot);
                    }
                    break;
                    
                case NIR_ADD:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  add %%rdx, %%rax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_SUB:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  sub %%rdx, %%rax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_MUL:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  imul %%rdx, %%rax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_DIV:
                    fprintf(cg->out, "  pop %%rcx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rcx\n");
                    fprintf(cg->out, "  call _checked_div\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_MOD:
                    fprintf(cg->out, "  pop %%rcx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rcx\n");
                    fprintf(cg->out, "  call _checked_mod\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_ERR_SAVE:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  mov _err_jmp(%%rip), %%rax\n");
                        fprintf(cg->out, "  mov %%rax, -%d(%%rbp)\n", slot);
                    }
                    break;

                case NIR_ERR_SET:
                    if (instr->target) {
                        fprintf(cg->out, "  lea .L%s_%s(%%rip), %%rax\n", func->name, instr->target->name);
                        fprintf(cg->out, "  mov %%rax, _err_jmp(%%rip)\n");
                    }
                    break;

                case NIR_ERR_RESTORE:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rax\n", slot);
                        fprintf(cg->out, "  mov %%rax, _err_jmp(%%rip)\n");
                    }
                    break;
                    
                case NIR_NEG:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  neg %%rax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_NOT:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  test %%rax, %%rax\n");
                    fprintf(cg->out, "  sete %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_EQ:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  sete %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_NE:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setne %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_LT:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setl %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_GT:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setg %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_LE:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setle %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_GE:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setge %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_JUMP:
                    if (instr->target) {
                        fprintf(cg->out, "  jmp .L%s_%s\n", func->name, instr->target->name);
                    }
                    break;
                    
                case NIR_JUMP_IF_FALSE:
                    if (instr->target) {
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                        fprintf(cg->out, "  mov %%rax, %%rdx\n");
                        fprintf(cg->out, "  and $0xff, %%edx\n");
                        fprintf(cg->out, "  cmp $2, %%rdx\n");
                        fprintf(cg->out, "  jne .Lfi_%d\n", lbl);
                        fprintf(cg->out, "  sar $8, %%rax\n");
                        fprintf(cg->out, "  test %%rax, %%rax\n");
                        fprintf(cg->out, "  jnz .Lft_%d\n", lbl);
                        fprintf(cg->out, "  jmp .Lff_%d\n", lbl);
                        fprintf(cg->out, ".Lfi_%d:\n", lbl);
                        fprintf(cg->out, "  test %%rax, %%rax\n");
                        fprintf(cg->out, "  jnz .Lft_%d\n", lbl);
                        fprintf(cg->out, ".Lff_%d:\n", lbl);
                        fprintf(cg->out, "  mov (%%rsp), %%rax\n");
                        fprintf(cg->out, "  add $8, %%rsp\n");
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, "  jmp .L%s_%s\n", func->name, instr->target->name);
                        fprintf(cg->out, ".Lft_%d:\n", lbl);
                        fprintf(cg->out, "  add $8, %%rsp\n");
                    }
                    break;
                    
                case NIR_JUMP_IF_TRUE:
                    if (instr->target) {
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                        fprintf(cg->out, "  mov %%rax, %%rdx\n");
                        fprintf(cg->out, "  and $0xff, %%edx\n");
                        fprintf(cg->out, "  cmp $2, %%rdx\n");
                        fprintf(cg->out, "  jne .Lti_%d\n", lbl);
                        fprintf(cg->out, "  sar $8, %%rax\n");
                        fprintf(cg->out, "  test %%rax, %%rax\n");
                        fprintf(cg->out, "  jnz .Ltt_%d\n", lbl);
                        fprintf(cg->out, "  jmp .Ltf_%d\n", lbl);
                        fprintf(cg->out, ".Lti_%d:\n", lbl);
                        fprintf(cg->out, "  test %%rax, %%rax\n");
                        fprintf(cg->out, "  jz .Ltf_%d\n", lbl);
                        fprintf(cg->out, ".Ltt_%d:\n", lbl);
                        fprintf(cg->out, "  mov (%%rsp), %%rax\n");
                        fprintf(cg->out, "  add $8, %%rsp\n");
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, "  jmp .L%s_%s\n", func->name, instr->target->name);
                        fprintf(cg->out, ".Ltf_%d:\n", lbl);
                        fprintf(cg->out, "  add $8, %%rsp\n");
                    }
                    break;
                    
                case NIR_CALL:
                    {
                        NIRFunction* target_func = NULL;
                        for (size_t k = 0; k < cg->program->function_count; k++) {
                            if (strcmp(cg->program->functions[k]->name, instr->a->name) == 0) {
                                target_func = cg->program->functions[k];
                                break;
                            }
                        }
                        
                        size_t argc = instr->b ? (size_t)instr->b->integer_value : 0;
                        if (target_func) argc = target_func->param_count;
                        if (argc > 6) argc = 6;
                        
                        const char* regs[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
                        for (size_t k = 0; k < argc; k++) {
                            fprintf(cg->out, "  pop %s\n", regs[argc - 1 - k]);
                        }
                        
                        fprintf(cg->out, "  call %s\n", instr->a->name);
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_RETURN:
                    fprintf(cg->out, "  mov %%rbp, %%rsp\n");
                    fprintf(cg->out, "  pop %%rbp\n");
                    fprintf(cg->out, "  ret\n");
                    break;
                    
                case NIR_RETURN_VALUE:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  mov %%rbp, %%rsp\n");
                    fprintf(cg->out, "  pop %%rbp\n");
                    fprintf(cg->out, "  ret\n");
                    break;
                    
                case NIR_PRINT_INT:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  mov %%rax, %%r12\n");
                    fprintf(cg->out, "  call _print_value\n");
                    fprintf(cg->out, "  mov %%r12, %%rax\n");
                    fprintf(cg->out, "  call _decref\n");
                    break;
                    
                case NIR_PRINT_STRING:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  mov %%rax, %%r12\n");
                    fprintf(cg->out, "  call _print_value\n");
                    fprintf(cg->out, "  mov %%r12, %%rax\n");
                    fprintf(cg->out, "  call _decref\n");
                    break;
                    
                case NIR_PRINT_VALUE_RAW:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  mov %%rax, %%r12\n");
                    fprintf(cg->out, "  call _print_value_raw\n");
                    fprintf(cg->out, "  mov %%r12, %%rax\n");
                    fprintf(cg->out, "  call _decref\n");
                    break;
                    
                case NIR_PRINT_NEWLINE:
                    fprintf(cg->out, "  mov $1, %%rax\n");
                    fprintf(cg->out, "  mov $1, %%rdi\n");
                    fprintf(cg->out, "  lea _newline(%%rip), %%rsi\n");
                    fprintf(cg->out, "  mov $1, %%rdx\n");
                    fprintf(cg->out, "  syscall\n");
                    break;
                    
                case NIR_READ_LINE:
                    fprintf(cg->out, "  call _read_line\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_LEN:
                    {
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%r11\n");
                        fprintf(cg->out, "  push %%r11\n");
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  mov %%rax, %%rdx\n");
                        fprintf(cg->out, "  and $0xff, %%edx\n");
                        fprintf(cg->out, "  cmp $4, %%rdx\n");
                        fprintf(cg->out, "  je .len_obj_%d\n", lbl);
                        fprintf(cg->out, "  cmp $5, %%rdx\n");
                        fprintf(cg->out, "  je .len_obj_%d\n", lbl);
                        fprintf(cg->out, "  cmp $6, %%rdx\n");
                        fprintf(cg->out, "  je .len_obj_%d\n", lbl);
                        fprintf(cg->out, "  xor %%eax, %%eax\n");
                        fprintf(cg->out, "  jmp .len_done_%d\n", lbl);
                        fprintf(cg->out, ".len_obj_%d:\n", lbl);
                        fprintf(cg->out, "  shr $8, %%rax\n");
                        fprintf(cg->out, "  mov 8(%%rax), %%rax\n");
                        fprintf(cg->out, ".len_done_%d:\n", lbl);
                        fprintf(cg->out, "  shl $8, %%rax\n");
                        fprintf(cg->out, "  or $2, %%rax\n");
                        fprintf(cg->out, "  mov %%rax, %%r12\n");
                        fprintf(cg->out, "  mov (%%rsp), %%rax\n");
                        fprintf(cg->out, "  add $8, %%rsp\n");
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, "  mov %%r12, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;

                case NIR_ABS:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  cmp $0, %%rax\n");
                    fprintf(cg->out, "  jge .abs_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  neg %%rax\n");
                    fprintf(cg->out, ".abs_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_MIN:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  jle .min_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov %%rdx, %%rax\n");
                    fprintf(cg->out, ".min_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_MAX:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  jge .max_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov %%rdx, %%rax\n");
                    fprintf(cg->out, ".max_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_TO_NUMBER:
                    {
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%r11\n");
                        fprintf(cg->out, "  push %%r11\n");
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  mov %%rax, %%rdx\n");
                        fprintf(cg->out, "  and $0xff, %%edx\n");
                        fprintf(cg->out, "  cmp $4, %%rdx\n");
                        fprintf(cg->out, "  jne .tn_zero_%d\n", lbl);
                        fprintf(cg->out, "  shr $8, %%rax\n");
                        fprintf(cg->out, "  mov 8(%%rax), %%rsi\n");
                        fprintf(cg->out, "  lea 16(%%rax), %%rdi\n");
                        fprintf(cg->out, "  call _parse_int\n");
                        fprintf(cg->out, "  shl $8, %%rax\n");
                        fprintf(cg->out, "  or $2, %%rax\n");
                        fprintf(cg->out, "  jmp .tn_done_%d\n", lbl);
                        fprintf(cg->out, ".tn_zero_%d:\n", lbl);
                        fprintf(cg->out, "  mov $2, %%eax\n");
                        fprintf(cg->out, ".tn_done_%d:\n", lbl);
                        fprintf(cg->out, "  mov %%rax, %%r12\n");
                        fprintf(cg->out, "  mov (%%rsp), %%rax\n");
                        fprintf(cg->out, "  add $8, %%rsp\n");
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, "  mov %%r12, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;

                case NIR_CONCAT:
                    fprintf(cg->out, "  pop %%rsi\n");
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _concat\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_ORD:
                    {
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%r11\n");
                        fprintf(cg->out, "  push %%r11\n");
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  mov %%rax, %%rdx\n");
                        fprintf(cg->out, "  and $0xff, %%edx\n");
                        fprintf(cg->out, "  cmp $4, %%rdx\n");
                        fprintf(cg->out, "  jne .ord_zero_%d\n", lbl);
                        fprintf(cg->out, "  shr $8, %%rax\n");
                        fprintf(cg->out, "  mov 8(%%rax), %%rcx\n");
                        fprintf(cg->out, "  test %%rcx, %%rcx\n");
                        fprintf(cg->out, "  jz .ord_zero_%d\n", lbl);
                        fprintf(cg->out, "  lea 16(%%rax), %%rdx\n");
                        fprintf(cg->out, "  movzbl (%%rdx), %%eax\n");
                        fprintf(cg->out, "  shl $8, %%rax\n");
                        fprintf(cg->out, "  or $2, %%rax\n");
                        fprintf(cg->out, "  jmp .ord_done_%d\n", lbl);
                        fprintf(cg->out, ".ord_zero_%d:\n", lbl);
                        fprintf(cg->out, "  mov $2, %%eax\n");
                        fprintf(cg->out, ".ord_done_%d:\n", lbl);
                        fprintf(cg->out, "  mov %%rax, %%r12\n");
                        fprintf(cg->out, "  mov (%%rsp), %%rax\n");
                        fprintf(cg->out, "  add $8, %%rsp\n");
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, "  mov %%r12, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;

                case NIR_CHR:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  push %%rbx\n");
                    fprintf(cg->out, "  mov %%rax, %%rbx\n");
                    fprintf(cg->out, "  mov $17, %%rdi\n");
                    fprintf(cg->out, "  call _alloc\n");
                    fprintf(cg->out, "  mov %%rax, %%r11\n");
                    fprintf(cg->out, "  movq $1, (%%r11)\n");
                    fprintf(cg->out, "  movq $1, 8(%%r11)\n");
                    fprintf(cg->out, "  lea 16(%%r11), %%r12\n");
                    fprintf(cg->out, "  movb %%bl, (%%r12)\n");
                    fprintf(cg->out, "  pop %%rbx\n");
                    fprintf(cg->out, "  shl $8, %%r11\n");
                    fprintf(cg->out, "  or $4, %%r11\n");
                    fprintf(cg->out, "  mov %%r11, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_POW:
                    {
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%rsi\n");
                        fprintf(cg->out, "  pop %%rdi\n");
                        fprintf(cg->out, "  sar $8, %%rsi\n");
                        fprintf(cg->out, "  sar $8, %%rdi\n");
                        fprintf(cg->out, "  mov $1, %%rax\n");
                        fprintf(cg->out, "  mov %%rsi, %%rcx\n");
                        fprintf(cg->out, "  test %%rcx, %%rcx\n");
                        fprintf(cg->out, "  jle .pow_done_%d\n", lbl);
                        fprintf(cg->out, ".pow_loop_%d:\n", lbl);
                        fprintf(cg->out, "  imul %%rdi, %%rax\n");
                        fprintf(cg->out, "  dec %%rcx\n");
                        fprintf(cg->out, "  jnz .pow_loop_%d\n", lbl);
                        fprintf(cg->out, ".pow_done_%d:\n", lbl);
                        fprintf(cg->out, "  shl $8, %%rax\n");
                        fprintf(cg->out, "  or $2, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;

                case NIR_CONTAINS:
                    fprintf(cg->out, "  pop %%rsi\n");
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _contains\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_SUBSTRING:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rsi\n");
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _substring\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_UPPER:
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _upper\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_LOWER:
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _lower\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_INDEX:
                    fprintf(cg->out, "  pop %%rsi\n");
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _index\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_TO_STRING:
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _to_string\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_REVERSE:
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _reverse\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_READ_FILE:
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _read_file\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_WRITE_FILE:
                    fprintf(cg->out, "  pop %%rsi\n");
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _write_file\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_ALLOC_LIST:
                    {
                        long n = instr->a ? (long)instr->a->integer_value : 0;
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  mov $%ld, %%rdi\n", 16 + n * 8);
                        fprintf(cg->out, "  call _alloc\n");
                        fprintf(cg->out, "  mov %%rax, %%r11\n");
                        fprintf(cg->out, "  movq $1, (%%r11)\n");
                        fprintf(cg->out, "  mov $%ld, %%rcx\n", n);
                        fprintf(cg->out, "  mov %%rcx, 8(%%r11)\n");
                        if (n > 0) {
                            fprintf(cg->out, "  lea 16(%%r11), %%r12\n");
                            fprintf(cg->out, "  mov $%ld, %%rcx\n", n - 1);
                            fprintf(cg->out, ".store_list_loop_%d:\n", lbl);
                            fprintf(cg->out, "  pop %%rax\n");
                            fprintf(cg->out, "  mov %%rax, (%%r12,%%rcx,8)\n");
                            fprintf(cg->out, "  dec %%rcx\n");
                            fprintf(cg->out, "  jns .store_list_loop_%d\n", lbl);
                        }
                        fprintf(cg->out, "  shl $8, %%r11\n");
                        fprintf(cg->out, "  or $5, %%r11\n");
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_LOAD_LIST_LEN:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rax\n", slot);
                        fprintf(cg->out, "  shr $8, %%rax\n");
                        fprintf(cg->out, "  mov 8(%%rax), %%rax\n");
                        fprintf(cg->out, "  shl $8, %%rax\n");
                        fprintf(cg->out, "  or $2, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_LOAD_ELT:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  pop %%rcx\n");          /* index */
                        fprintf(cg->out, "  sar $8, %%rcx\n");
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rdx\n", slot); /* list */
                        fprintf(cg->out, "  shr $8, %%rdx\n");
                        fprintf(cg->out, "  mov 16(%%rdx,%%rcx,8), %%rax\n");
                        fprintf(cg->out, "  call _incref\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;

                case NIR_LOAD_ELT_EXPR:
                    fprintf(cg->out, "  pop %%rsi\n");          /* index (key for maps) */
                    fprintf(cg->out, "  pop %%rdi\n");          /* container (list/map) */
                    fprintf(cg->out, "  call _index_get\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_ALLOC_MAP:
                    {
                        long n = instr->a ? (long)instr->a->integer_value : 0;
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  mov $%ld, %%rdi\n", 16 + n * 16);
                        fprintf(cg->out, "  call _alloc\n");
                        fprintf(cg->out, "  mov %%rax, %%r11\n");
                        fprintf(cg->out, "  movq $1, (%%r11)\n");
                        fprintf(cg->out, "  mov $%ld, %%rcx\n", n);
                        fprintf(cg->out, "  mov %%rcx, 8(%%r11)\n");
                        if (n > 0) {
                            fprintf(cg->out, "  lea 16(%%r11), %%r12\n");
                            fprintf(cg->out, "  mov $%ld, %%rcx\n", 2 * n - 1);
                            fprintf(cg->out, ".store_map_loop_%d:\n", lbl);
                            fprintf(cg->out, "  pop %%rax\n");
                            fprintf(cg->out, "  mov %%rax, (%%r12,%%rcx,8)\n");
                            fprintf(cg->out, "  dec %%rcx\n");
                            fprintf(cg->out, "  jns .store_map_loop_%d\n", lbl);
                        }
                        fprintf(cg->out, "  shl $8, %%r11\n");
                        fprintf(cg->out, "  or $6, %%r11\n");
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_LOAD_MAP_FIELD:
                    {
                        int kidx = get_const_index(cg, instr->b);
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%r11\n");               /* map */
                        fprintf(cg->out, "  shr $8, %%r11\n");
                        fprintf(cg->out, "  mov 8(%%r11), %%rcx\n");     /* count */
                        fprintf(cg->out, "  lea 16(%%r11), %%r12\n");    /* entries */
                        fprintf(cg->out, "  lea _str%d(%%rip), %%r13\n", kidx);
                        fprintf(cg->out, "  shl $8, %%r13\n");
                        fprintf(cg->out, "  or $4, %%r13\n");             /* tagged key */
                        fprintf(cg->out, "  xor %%r14, %%r14\n");
                        fprintf(cg->out, ".map_search_loop_%d:\n", lbl);
                        fprintf(cg->out, "  cmp %%r14, %%rcx\n");
                        fprintf(cg->out, "  je .map_not_found_%d\n", lbl);
                        fprintf(cg->out, "  lea (%%r14,%%r14), %%r15\n");
                        fprintf(cg->out, "  mov (%%r12,%%r15,8), %%rax\n");
                        fprintf(cg->out, "  cmp %%r13, %%rax\n");
                        fprintf(cg->out, "  je .map_found_%d\n", lbl);
                        fprintf(cg->out, "  inc %%r14\n");
                        fprintf(cg->out, "  jmp .map_search_loop_%d\n", lbl);
                        fprintf(cg->out, ".map_not_found_%d:\n", lbl);
                        fprintf(cg->out, "  mov $0, %%rax\n");
                        fprintf(cg->out, "  jmp .map_push_%d\n", lbl);
                        fprintf(cg->out, ".map_found_%d:\n", lbl);
                        fprintf(cg->out, "  lea (%%r14,%%r14), %%r15\n");
                        fprintf(cg->out, "  mov 8(%%r12,%%r15,8), %%rax\n");
                        fprintf(cg->out, "  call _incref\n");
                        fprintf(cg->out, ".map_push_%d:\n", lbl);
                        fprintf(cg->out, "  push %%rax\n");
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, ".map_done_%d:\n", lbl);
                    }
                    break;

                case NIR_APPEND:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  pop %%rsi\n");
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rdi\n", slot);
                        fprintf(cg->out, "  call _append\n");
                        fprintf(cg->out, "  mov %%rax, %%r11\n");
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rax\n", slot);
                        fprintf(cg->out, "  call _decref\n");
                        fprintf(cg->out, "  mov %%r11, -%d(%%rbp)\n", slot);
                    }
                    break;

                case NIR_DISCARD:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  call _decref\n");
                    break;

                case NIR_HALT:
                    fprintf(cg->out, "  mov $60, %%rax\n");
                    fprintf(cg->out, "  xor %%rdi, %%rdi\n");
                    fprintf(cg->out, "  syscall\n");
                    break;
            }
        }
        block = block->next;
    }
    
    fprintf(cg->out, "\n");
}

CodeGen* codegen_create(const char* filename, NIRProgram* program) {
    CodeGen* cg = calloc(1, sizeof(CodeGen));
    cg->program = program;
    cg->out = fopen(filename, "w");
    return cg;
}

void codegen_free(CodeGen* cg) {
    if (cg) {
        if (cg->out) fclose(cg->out);
        free(cg);
    }
}

int codegen_emit(CodeGen* cg) {
    fprintf(cg->out, ".text\n");
    
    NIRFunction* main_func = NULL;
    for (size_t i = 0; i < cg->program->function_count; i++) {
        if (strcmp(cg->program->functions[i]->name, "main") == 0) {
            main_func = cg->program->functions[i];
            break;
        }
    }
    
    // Entry point
    fprintf(cg->out, "\n.globl _start\n");
    fprintf(cg->out, "_start:\n");
    fprintf(cg->out, "  call main\n");
    fprintf(cg->out, "  mov %%rax, %%rdi\n");
    fprintf(cg->out, "  and $0xff, %%edi\n");
    fprintf(cg->out, "  cmp $2, %%rdi\n");
    fprintf(cg->out, "  jne .Lexit_zero\n");
    fprintf(cg->out, "  mov %%rax, %%rdi\n");
    fprintf(cg->out, "  sar $8, %%rdi\n");
    fprintf(cg->out, "  jmp .Lexit_done\n");
    fprintf(cg->out, ".Lexit_zero:\n");
    fprintf(cg->out, "  xor %%edi, %%edi\n");
    fprintf(cg->out, ".Lexit_done:\n");
    fprintf(cg->out, "  mov $60, %%rax\n");
    fprintf(cg->out, "  syscall\n");

    if (main_func) {
        emit_func(cg, main_func);
    }
    
    for (size_t i = 0; i < cg->program->function_count; i++) {
        if (cg->program->functions[i] != main_func) {
            emit_func(cg, cg->program->functions[i]);
        }
    }
    
    fprintf(cg->out, "\n.data\n");
    fprintf(cg->out, "_newline:\n");
    fprintf(cg->out, "  .byte 10\n");
    fprintf(cg->out, "_err_jmp:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, "_lit_lo:\n");
    
    for (size_t i = 0; i < cg->program->constant_count; i++) {
        if (cg->program->constants[i]->kind == NIR_CONST_STR) {
            fprintf(cg->out, "_str%d:\n", (int)i);
            fprintf(cg->out, "  .quad 0\n");
            fprintf(cg->out, "  .quad %zu\n", cg->program->constants[i]->string_value.length);
            fprintf(cg->out, "  .byte ");
            const unsigned char* d = (const unsigned char*)cg->program->constants[i]->string_value.data;
            size_t l = cg->program->constants[i]->string_value.length;
            for (size_t k = 0; k < l; k++) {
                fprintf(cg->out, "%s%u", k ? "," : "", (unsigned)d[k]);
            }
            fprintf(cg->out, "\n");
        }
    }
    
    fprintf(cg->out, "_lit_hi:\n");
    fprintf(cg->out, "_str_true:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, "  .quad 4\n");
    fprintf(cg->out, "  .byte 't','r','u','e'\n");
    fprintf(cg->out, "_str_false:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, "  .quad 5\n");
    fprintf(cg->out, "  .byte 'f','a','l','s','e'\n");
    fprintf(cg->out, "_str_empty:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, "  .quad 5\n");
    fprintf(cg->out, "  .byte 'e','m','p','t','y'\n");
    
    fprintf(cg->out, "\n.text\n");
    
    fprintf(cg->out, "\n.globl _print_int\n");
    fprintf(cg->out, "_print_int:\n");
    fprintf(cg->out, "  push %%rbp\n");
    fprintf(cg->out, "  mov %%rsp, %%rbp\n");
    fprintf(cg->out, "  sub $40, %%rsp\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  xor %%r9, %%r9\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jns .pi_prep\n");
    fprintf(cg->out, "  mov $1, %%r9\n");
    fprintf(cg->out, "  neg %%rax\n");
    fprintf(cg->out, ".pi_prep:\n");
    fprintf(cg->out, "  mov $10, %%rcx\n");
    fprintf(cg->out, "  lea 39(%%rsp), %%r8\n");
    fprintf(cg->out, "  movb $0, (%%r8)\n");
    fprintf(cg->out, ".print_loop:\n");
    fprintf(cg->out, "  xor %%rdx, %%rdx\n");
    fprintf(cg->out, "  div %%rcx\n");
    fprintf(cg->out, "  add $'0', %%dl\n");
    fprintf(cg->out, "  dec %%r8\n");
    fprintf(cg->out, "  mov %%dl, (%%r8)\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jnz .print_loop\n");
    fprintf(cg->out, "  test %%r9, %%r9\n");
    fprintf(cg->out, "  jz .pi_nosign\n");
    fprintf(cg->out, "  dec %%r8\n");
    fprintf(cg->out, "  movb $'-', (%%r8)\n");
    fprintf(cg->out, ".pi_nosign:\n");
    fprintf(cg->out, "  lea 39(%%rsp), %%rsi\n");
    fprintf(cg->out, "  sub %%r8, %%rsi\n");
    fprintf(cg->out, "  mov %%r8, %%rdi\n");
    fprintf(cg->out, "  call _write_string\n");
    fprintf(cg->out, "  mov %%rbp, %%rsp\n");
    fprintf(cg->out, "  pop %%rbp\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _print_value_raw\n");
    fprintf(cg->out, "_print_value_raw:\n");
    fprintf(cg->out, "  mov %%rax, %%rdx\n");
    fprintf(cg->out, "  and $0xff, %%edx\n");
    fprintf(cg->out, "  cmp $2, %%rdx\n");
    fprintf(cg->out, "  je .raw_int\n");
    fprintf(cg->out, "  cmp $1, %%rdx\n");
    fprintf(cg->out, "  je .raw_bool\n");
    fprintf(cg->out, "  cmp $4, %%rdx\n");
    fprintf(cg->out, "  je .raw_str\n");
    fprintf(cg->out, "  cmp $0, %%rdx\n");
    fprintf(cg->out, "  je .raw_empty\n");
    fprintf(cg->out, "  jmp .raw_int\n");
    fprintf(cg->out, ".raw_int:\n");
    fprintf(cg->out, "  sar $8, %%rax\n");
    fprintf(cg->out, "  mov %%rax, %%rdi\n");
    fprintf(cg->out, "  call _print_int\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".raw_bool:\n");
    fprintf(cg->out, "  shr $8, %%rax\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jz .raw_bfalse\n");
    fprintf(cg->out, "  lea _str_true(%%rip), %%rdi\n");
    fprintf(cg->out, "  mov $4, %%rsi\n");
    fprintf(cg->out, "  call _write_string\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".raw_bfalse:\n");
    fprintf(cg->out, "  lea _str_false(%%rip), %%rdi\n");
    fprintf(cg->out, "  mov $5, %%rsi\n");
    fprintf(cg->out, "  call _write_string\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".raw_str:\n");
    fprintf(cg->out, "  shr $8, %%rax\n");
    fprintf(cg->out, "  mov 8(%%rax), %%rsi\n");
    fprintf(cg->out, "  lea 16(%%rax), %%rdi\n");
    fprintf(cg->out, "  call _write_string\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".raw_empty:\n");
    fprintf(cg->out, "  lea _str_empty(%%rip), %%rdi\n");
    fprintf(cg->out, "  mov $5, %%rsi\n");
    fprintf(cg->out, "  call _write_string\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _print_value\n");
    fprintf(cg->out, "_print_value:\n");
    fprintf(cg->out, "  call _print_value_raw\n");
    fprintf(cg->out, "  mov $1, %%rax\n");
    fprintf(cg->out, "  mov $1, %%rdi\n");
    fprintf(cg->out, "  lea _newline(%%rip), %%rsi\n");
    fprintf(cg->out, "  mov $1, %%rdx\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _write_string\n");
    fprintf(cg->out, "_write_string:\n");
    fprintf(cg->out, "  mov %%rdi, %%r10\n");
    fprintf(cg->out, "  mov %%rsi, %%rdx\n");
    fprintf(cg->out, "  mov $1, %%rdi\n");
    fprintf(cg->out, "  mov %%r10, %%rsi\n");
    fprintf(cg->out, "  mov $1, %%rax\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _checked_div\n");
    fprintf(cg->out, "_checked_div:\n");
    fprintf(cg->out, "  test %%rcx, %%rcx\n");
    fprintf(cg->out, "  jz .cd_zero\n");
    fprintf(cg->out, "  xor %%rdx, %%rdx\n");
    fprintf(cg->out, "  idiv %%rcx\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".cd_zero:\n");
    fprintf(cg->out, "  mov _err_jmp(%%rip), %%rdx\n");
    fprintf(cg->out, "  test %%rdx, %%rdx\n");
    fprintf(cg->out, "  jz .cd_crash\n");
    fprintf(cg->out, "  jmp *%%rdx\n");
    fprintf(cg->out, ".cd_crash:\n");
    fprintf(cg->out, "  mov $0, %%rax\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _checked_mod\n");
    fprintf(cg->out, "_checked_mod:\n");
    fprintf(cg->out, "  test %%rcx, %%rcx\n");
    fprintf(cg->out, "  jz .cm_zero\n");
    fprintf(cg->out, "  xor %%rdx, %%rdx\n");
    fprintf(cg->out, "  idiv %%rcx\n");
    fprintf(cg->out, "  mov %%rdx, %%rax\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".cm_zero:\n");
    fprintf(cg->out, "  mov _err_jmp(%%rip), %%rdx\n");
    fprintf(cg->out, "  test %%rdx, %%rdx\n");
    fprintf(cg->out, "  jz .cm_crash\n");
    fprintf(cg->out, "  jmp *%%rdx\n");
    fprintf(cg->out, ".cm_crash:\n");
    fprintf(cg->out, "  mov $0, %%rax\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _read_line\n");
    fprintf(cg->out, "_read_line:\n");
    fprintf(cg->out, "  mov $528, %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  movq $1, (%%rbx)\n");
    fprintf(cg->out, "  lea 16(%%rbx), %%r12\n");
    fprintf(cg->out, "  mov $0, %%rdi\n");
    fprintf(cg->out, "  mov %%r12, %%rsi\n");
    fprintf(cg->out, "  mov $511, %%rdx\n");
    fprintf(cg->out, "  mov $0, %%rax\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  cmp $0, %%rax\n");
    fprintf(cg->out, "  jle .rl_zero\n");
    fprintf(cg->out, "  lea -1(%%r12,%%rax), %%rcx\n");
    fprintf(cg->out, "  cmpb $10, (%%rcx)\n");
    fprintf(cg->out, "  jne .rl_len\n");
    fprintf(cg->out, "  dec %%rax\n");
    fprintf(cg->out, ".rl_len:\n");
    fprintf(cg->out, "  mov %%rax, 8(%%rbx)\n");
    fprintf(cg->out, "  jmp .rl_tag\n");
    fprintf(cg->out, ".rl_zero:\n");
    fprintf(cg->out, "  movq $0, 8(%%rbx)\n");
    fprintf(cg->out, ".rl_tag:\n");
    fprintf(cg->out, "  shl $8, %%rbx\n");
    fprintf(cg->out, "  or $4, %%rbx\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _concat\n");
    fprintf(cg->out, "_concat:\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  push %%rsi\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  mov %%rsi, %%r9\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r9\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  mov 8(%%r9), %%r11\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  lea 16(%%r9), %%r9\n");
    fprintf(cg->out, "  push %%r8\n");
    fprintf(cg->out, "  push %%r9\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  push %%r11\n");
    fprintf(cg->out, "  lea 16(%%r10,%%r11), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r11\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r9\n");
    fprintf(cg->out, "  pop %%r8\n");
    fprintf(cg->out, "  mov %%rax, %%rdx\n");
    fprintf(cg->out, "  movq $1, (%%rdx)\n");
    fprintf(cg->out, "  lea (%%r10,%%r11), %%rcx\n");
    fprintf(cg->out, "  mov %%rcx, 8(%%rdx)\n");
    fprintf(cg->out, "  lea 16(%%rdx), %%rdx\n");
    fprintf(cg->out, "  xor %%rcx, %%rcx\n");
    fprintf(cg->out, ".cpA:\n");
    fprintf(cg->out, "  cmp %%r10, %%rcx\n");
    fprintf(cg->out, "  jae .cpB\n");
    fprintf(cg->out, "  movb (%%r8,%%rcx), %%al\n");
    fprintf(cg->out, "  movb %%al, (%%rdx,%%rcx)\n");
    fprintf(cg->out, "  inc %%rcx\n");
    fprintf(cg->out, "  jmp .cpA\n");
    fprintf(cg->out, ".cpB:\n");
    fprintf(cg->out, "  xor %%rsi, %%rsi\n");
    fprintf(cg->out, ".cpBloop:\n");
    fprintf(cg->out, "  cmp %%r11, %%rsi\n");
    fprintf(cg->out, "  jae .cpDone\n");
    fprintf(cg->out, "  movb (%%r9,%%rsi), %%al\n");
    fprintf(cg->out, "  movb %%al, (%%rdx,%%rcx)\n");
    fprintf(cg->out, "  inc %%rcx\n");
    fprintf(cg->out, "  inc %%rsi\n");
    fprintf(cg->out, "  jmp .cpBloop\n");
    fprintf(cg->out, ".cpDone:\n");
    fprintf(cg->out, "  sub $16, %%rdx\n");
    fprintf(cg->out, "  mov %%rdx, %%rbx\n");
    fprintf(cg->out, "  mov 8(%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov (%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  add $16, %%rsp\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $4, %%rax\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _contains\n");
    fprintf(cg->out, "_contains:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  push %%r15\n");
    fprintf(cg->out, "  xor %%ebx, %%ebx\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  mov %%rsi, %%r9\n");
    fprintf(cg->out, "  shr $8, %%r9\n");
    fprintf(cg->out, "  mov 8(%%r9), %%r11\n");
    fprintf(cg->out, "  lea 16(%%r9), %%r9\n");
    fprintf(cg->out, "  xor %%eax, %%eax\n");
    fprintf(cg->out, "  test %%r11, %%r11\n");
    fprintf(cg->out, "  jz .ct_found\n");
    fprintf(cg->out, "  cmp %%r11, %%r10\n");
    fprintf(cg->out, "  jb .ct_done\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, ".ct_outer:\n");
    fprintf(cg->out, "  mov %%r10, %%r13\n");
    fprintf(cg->out, "  sub %%r12, %%r13\n");
    fprintf(cg->out, "  cmp %%r11, %%r13\n");
    fprintf(cg->out, "  jb .ct_done\n");
    fprintf(cg->out, "  xor %%r14, %%r14\n");
    fprintf(cg->out, ".ct_inner:\n");
    fprintf(cg->out, "  cmp %%r11, %%r14\n");
    fprintf(cg->out, "  jae .ct_found\n");
    fprintf(cg->out, "  movb (%%r8,%%r12,1), %%al\n");
    fprintf(cg->out, "  movb (%%r9,%%r14,1), %%dl\n");
    fprintf(cg->out, "  cmp %%dl, %%al\n");
    fprintf(cg->out, "  jne .ct_next\n");
    fprintf(cg->out, "  inc %%r14\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .ct_inner\n");
    fprintf(cg->out, ".ct_next:\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .ct_outer\n");
    fprintf(cg->out, ".ct_found:\n");
    fprintf(cg->out, "  mov $1, %%ebx\n");
    fprintf(cg->out, ".ct_done:\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov %%rsi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov %%ebx, %%eax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $2, %%rax\n");
    fprintf(cg->out, "  pop %%r15\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _append\n");
    fprintf(cg->out, "_append:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r9\n");
    fprintf(cg->out, "  lea 1(%%r10), %%r11\n");
    fprintf(cg->out, "  push %%r9\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  push %%r11\n");
    fprintf(cg->out, "  lea 88(,%%r10,8), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r11\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r9\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  movq $1, (%%rbx)\n");
    fprintf(cg->out, "  mov %%r11, 8(%%rbx)\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, ".ap_cp:\n");
    fprintf(cg->out, "  cmp %%r10, %%r12\n");
    fprintf(cg->out, "  jae .ap_val\n");
    fprintf(cg->out, "  mov (%%r9,%%r12,8), %%rax\n");
    fprintf(cg->out, "  call _incref\n");
    fprintf(cg->out, "  mov %%rax, 16(%%rbx,%%r12,8)\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .ap_cp\n");
    fprintf(cg->out, ".ap_val:\n");
    fprintf(cg->out, "  mov %%rsi, %%rax\n");
    fprintf(cg->out, "  mov %%rax, 16(%%rbx,%%r10,8)\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $5, %%rax\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _substring\n");
    fprintf(cg->out, "_substring:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  mov %%rsi, %%r9\n");
    fprintf(cg->out, "  sar $8, %%r9\n");
    fprintf(cg->out, "  mov %%rdx, %%r11\n");
    fprintf(cg->out, "  sar $8, %%r11\n");
    fprintf(cg->out, "  push %%r9\n");
    fprintf(cg->out, "  push %%r11\n");
    fprintf(cg->out, "  mov %%r11, %%r13\n");
    fprintf(cg->out, "  sub %%r9, %%r13\n");
    fprintf(cg->out, "  test %%r13, %%r13\n");
    fprintf(cg->out, "  jns .ss_alloc\n");
    fprintf(cg->out, "  xor %%r13, %%r13\n");
    fprintf(cg->out, ".ss_alloc:\n");
    fprintf(cg->out, "  lea 17(%%r13), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  pop %%r11\n");
    fprintf(cg->out, "  pop %%r9\n");
    fprintf(cg->out, "  pop %%rdi\n");
    fprintf(cg->out, "  movq $1, (%%rbx)\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, "  cmp %%r12, %%r9\n");
    fprintf(cg->out, "  jge .ss_sok\n");
    fprintf(cg->out, "  xor %%r9, %%r9\n");
    fprintf(cg->out, ".ss_sok:\n");
    fprintf(cg->out, "  cmp %%r10, %%r9\n");
    fprintf(cg->out, "  jle .ss_sok2\n");
    fprintf(cg->out, "  mov %%r10, %%r9\n");
    fprintf(cg->out, ".ss_sok2:\n");
    fprintf(cg->out, "  cmp %%r12, %%r11\n");
    fprintf(cg->out, "  jge .ss_eok\n");
    fprintf(cg->out, "  xor %%r11, %%r11\n");
    fprintf(cg->out, ".ss_eok:\n");
    fprintf(cg->out, "  cmp %%r10, %%r11\n");
    fprintf(cg->out, "  jle .ss_eok2\n");
    fprintf(cg->out, "  mov %%r10, %%r11\n");
    fprintf(cg->out, ".ss_eok2:\n");
    fprintf(cg->out, "  mov %%r11, %%r13\n");
    fprintf(cg->out, "  sub %%r9, %%r13\n");
    fprintf(cg->out, "  test %%r13, %%r13\n");
    fprintf(cg->out, "  jns .ss_len\n");
    fprintf(cg->out, "  xor %%r13, %%r13\n");
    fprintf(cg->out, ".ss_len:\n");
    fprintf(cg->out, "  mov %%r13, 8(%%rbx)\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, ".ss_cp:\n");
    fprintf(cg->out, "  cmp %%r13, %%r12\n");
    fprintf(cg->out, "  jae .ss_done\n");
    fprintf(cg->out, "  movb (%%r8,%%r9,1), %%al\n");
    fprintf(cg->out, "  movb %%al, 16(%%rbx,%%r12,1)\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  inc %%r9\n");
    fprintf(cg->out, "  jmp .ss_cp\n");
    fprintf(cg->out, ".ss_done:\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $4, %%rax\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _upper\n");
    fprintf(cg->out, "_upper:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  lea 17(%%r10), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%rdi\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  movq $1, (%%rbx)\n");
    fprintf(cg->out, "  mov %%r10, 8(%%rbx)\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, ".up_loop:\n");
    fprintf(cg->out, "  cmp %%r10, %%r12\n");
    fprintf(cg->out, "  jae .up_done\n");
    fprintf(cg->out, "  movb (%%r8,%%r12,1), %%al\n");
    fprintf(cg->out, "  cmpb $'a', %%al\n");
    fprintf(cg->out, "  jb .up_store\n");
    fprintf(cg->out, "  cmpb $'z', %%al\n");
    fprintf(cg->out, "  ja .up_store\n");
    fprintf(cg->out, "  sub $32, %%al\n");
    fprintf(cg->out, ".up_store:\n");
    fprintf(cg->out, "  movb %%al, 16(%%rbx,%%r12,1)\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .up_loop\n");
    fprintf(cg->out, ".up_done:\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $4, %%rax\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _lower\n");
    fprintf(cg->out, "_lower:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  lea 17(%%r10), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%rdi\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  movq $1, (%%rbx)\n");
    fprintf(cg->out, "  mov %%r10, 8(%%rbx)\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, ".lo_loop:\n");
    fprintf(cg->out, "  cmp %%r10, %%r12\n");
    fprintf(cg->out, "  jae .lo_done\n");
    fprintf(cg->out, "  movb (%%r8,%%r12,1), %%al\n");
    fprintf(cg->out, "  cmpb $'A', %%al\n");
    fprintf(cg->out, "  jb .lo_store\n");
    fprintf(cg->out, "  cmpb $'Z', %%al\n");
    fprintf(cg->out, "  ja .lo_store\n");
    fprintf(cg->out, "  add $32, %%al\n");
    fprintf(cg->out, ".lo_store:\n");
    fprintf(cg->out, "  movb %%al, 16(%%rbx,%%r12,1)\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .lo_loop\n");
    fprintf(cg->out, ".lo_done:\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $4, %%rax\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _index\n");
    fprintf(cg->out, "_index:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  push %%r15\n");
    fprintf(cg->out, "  mov $-1, %%ebx\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  mov %%rsi, %%r9\n");
    fprintf(cg->out, "  shr $8, %%r9\n");
    fprintf(cg->out, "  mov 8(%%r9), %%r11\n");
    fprintf(cg->out, "  lea 16(%%r9), %%r9\n");
    fprintf(cg->out, "  test %%r11, %%r11\n");
    fprintf(cg->out, "  jz .ix_found\n");
    fprintf(cg->out, "  cmp %%r11, %%r10\n");
    fprintf(cg->out, "  jb .ix_done\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, ".ix_outer:\n");
    fprintf(cg->out, "  mov %%r12, %%r15\n");
    fprintf(cg->out, "  mov %%r10, %%r13\n");
    fprintf(cg->out, "  sub %%r12, %%r13\n");
    fprintf(cg->out, "  cmp %%r11, %%r13\n");
    fprintf(cg->out, "  jb .ix_done\n");
    fprintf(cg->out, "  xor %%r14, %%r14\n");
    fprintf(cg->out, ".ix_inner:\n");
    fprintf(cg->out, "  cmp %%r11, %%r14\n");
    fprintf(cg->out, "  jae .ix_found\n");
    fprintf(cg->out, "  movb (%%r8,%%r12,1), %%al\n");
    fprintf(cg->out, "  movb (%%r9,%%r14,1), %%dl\n");
    fprintf(cg->out, "  cmp %%dl, %%al\n");
    fprintf(cg->out, "  jne .ix_next\n");
    fprintf(cg->out, "  inc %%r14\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .ix_inner\n");
    fprintf(cg->out, ".ix_next:\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .ix_outer\n");
    fprintf(cg->out, ".ix_found:\n");
    fprintf(cg->out, "  mov %%r15d, %%ebx\n");
    fprintf(cg->out, ".ix_done:\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov %%rsi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  movslq %%ebx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $2, %%rax\n");
    fprintf(cg->out, "  pop %%r15\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _to_string\n");
    fprintf(cg->out, "_to_string:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  sar $8, %%rax\n");
    fprintf(cg->out, "  xor %%r8, %%r8\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jns .ts_nonneg\n");
    fprintf(cg->out, "  mov $1, %%r8\n");
    fprintf(cg->out, "  neg %%rax\n");
    fprintf(cg->out, ".ts_nonneg:\n");
    fprintf(cg->out, "  sub $48, %%rsp\n");
    fprintf(cg->out, "  lea 47(%%rsp), %%r9\n");
    fprintf(cg->out, "  movb $0, (%%r9)\n");
    fprintf(cg->out, "  mov $10, %%rcx\n");
    fprintf(cg->out, "  xor %%r10, %%r10\n");
    fprintf(cg->out, ".ts_loop:\n");
    fprintf(cg->out, "  xor %%rdx, %%rdx\n");
    fprintf(cg->out, "  div %%rcx\n");
    fprintf(cg->out, "  add $'0', %%dl\n");
    fprintf(cg->out, "  dec %%r9\n");
    fprintf(cg->out, "  mov %%dl, (%%r9)\n");
    fprintf(cg->out, "  inc %%r10\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jnz .ts_loop\n");
    fprintf(cg->out, "  test %%r8, %%r8\n");
    fprintf(cg->out, "  jz .ts_nosign\n");
    fprintf(cg->out, "  dec %%r9\n");
    fprintf(cg->out, "  movb $'-', (%%r9)\n");
    fprintf(cg->out, "  inc %%r10\n");
    fprintf(cg->out, ".ts_nosign:\n");
    fprintf(cg->out, "  push %%r9\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  lea 32(%%r10), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r9\n");
    fprintf(cg->out, "  mov %%rax, %%r12\n");
    fprintf(cg->out, "  movq $1, (%%r12)\n");
    fprintf(cg->out, "  mov %%r10, 8(%%r12)\n");
    fprintf(cg->out, "  xor %%rcx, %%rcx\n");
    fprintf(cg->out, ".ts_cp:\n");
    fprintf(cg->out, "  cmp %%r10, %%rcx\n");
    fprintf(cg->out, "  jae .ts_done\n");
    fprintf(cg->out, "  movb (%%r9,%%rcx,1), %%al\n");
    fprintf(cg->out, "  movb %%al, 16(%%r12,%%rcx,1)\n");
    fprintf(cg->out, "  inc %%rcx\n");
    fprintf(cg->out, "  jmp .ts_cp\n");
    fprintf(cg->out, ".ts_done:\n");
    fprintf(cg->out, "  add $48, %%rsp\n");
    fprintf(cg->out, "  mov %%r12, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $4, %%rax\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _read_file\n");
    fprintf(cg->out, "_read_file:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  push %%r15\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  push %%r8\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  lea 1(%%r10), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r8\n");
    fprintf(cg->out, "  mov %%rax, %%r9\n");
    fprintf(cg->out, "  xor %%rcx, %%rcx\n");
    fprintf(cg->out, ".rf_cp:\n");
    fprintf(cg->out, "  cmp %%r10, %%rcx\n");
    fprintf(cg->out, "  jae .rf_cpdone\n");
    fprintf(cg->out, "  movb (%%r8,%%rcx,1), %%al\n");
    fprintf(cg->out, "  movb %%al, (%%r9,%%rcx,1)\n");
    fprintf(cg->out, "  inc %%rcx\n");
    fprintf(cg->out, "  jmp .rf_cp\n");
    fprintf(cg->out, ".rf_cpdone:\n");
    fprintf(cg->out, "  movb $0, (%%r9,%%r10,1)\n");
    fprintf(cg->out, "  mov $2, %%rax\n");
    fprintf(cg->out, "  mov %%r9, %%rdi\n");
    fprintf(cg->out, "  xor %%rsi, %%rsi\n");
    fprintf(cg->out, "  xor %%rdx, %%rdx\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  js .rf_empty\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  sub $152, %%rsp\n");
    fprintf(cg->out, "  mov $5, %%rax\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  mov %%rsp, %%rsi\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jne .rf_closerem\n");
    fprintf(cg->out, "  mov 48(%%rsp), %%r10\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  lea 32(%%r10), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  mov %%rax, %%r12\n");
    fprintf(cg->out, "  movq $1, (%%r12)\n");
    fprintf(cg->out, "  mov %%r10, 8(%%r12)\n");
    fprintf(cg->out, "  mov $0, %%rax\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  lea 16(%%r12), %%rsi\n");
    fprintf(cg->out, "  mov %%r10, %%rdx\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  mov $3, %%rax\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  add $152, %%rsp\n");
    fprintf(cg->out, "  jmp .rf_mkval\n");
    fprintf(cg->out, ".rf_closerem:\n");
    fprintf(cg->out, "  mov $3, %%rax\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  add $152, %%rsp\n");
    fprintf(cg->out, ".rf_empty:\n");
    fprintf(cg->out, "  mov $16, %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  mov %%rax, %%r12\n");
    fprintf(cg->out, "  movq $1, (%%r12)\n");
    fprintf(cg->out, "  movq $0, 8(%%r12)\n");
    fprintf(cg->out, ".rf_mkval:\n");
    fprintf(cg->out, "  mov (%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  add $8, %%rsp\n");
    fprintf(cg->out, "  mov %%r12, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $4, %%rax\n");
    fprintf(cg->out, "  pop %%r15\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _write_file\n");
    fprintf(cg->out, "_write_file:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  push %%r15\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  push %%rsi\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  push %%r8\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  lea 1(%%r10), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r8\n");
    fprintf(cg->out, "  mov %%rax, %%r9\n");
    fprintf(cg->out, "  xor %%rcx, %%rcx\n");
    fprintf(cg->out, ".wf_cp:\n");
    fprintf(cg->out, "  cmp %%r10, %%rcx\n");
    fprintf(cg->out, "  jae .wf_cpdone\n");
    fprintf(cg->out, "  movb (%%r8,%%rcx,1), %%al\n");
    fprintf(cg->out, "  movb %%al, (%%r9,%%rcx,1)\n");
    fprintf(cg->out, "  inc %%rcx\n");
    fprintf(cg->out, "  jmp .wf_cp\n");
    fprintf(cg->out, ".wf_cpdone:\n");
    fprintf(cg->out, "  movb $0, (%%r9,%%r10,1)\n");
    fprintf(cg->out, "  mov (%%rsp), %%rax\n");
    fprintf(cg->out, "  mov %%rax, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  push %%r8\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  mov $2, %%rax\n");
    fprintf(cg->out, "  mov %%r9, %%rdi\n");
    fprintf(cg->out, "  mov $0x241, %%rsi\n");
    fprintf(cg->out, "  mov $0644, %%rdx\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r8\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  js .wf_fail\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  mov $1, %%rax\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  mov %%r8, %%rsi\n");
    fprintf(cg->out, "  mov %%r10, %%rdx\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  mov $3, %%rax\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  syscall\n");
    fprintf(cg->out, "  mov $2, %%r12\n");
    fprintf(cg->out, "  jmp .wf_done\n");
    fprintf(cg->out, ".wf_fail:\n");
    fprintf(cg->out, "  mov $-254, %%r12\n");
    fprintf(cg->out, ".wf_done:\n");
    fprintf(cg->out, "  mov 8(%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov (%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  add $16, %%rsp\n");
    fprintf(cg->out, "  mov %%r12, %%rax\n");
    fprintf(cg->out, "  pop %%r15\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _reverse\n");
    fprintf(cg->out, "_reverse:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  push %%r15\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  and $0xff, %%eax\n");
    fprintf(cg->out, "  cmp $5, %%rax\n");
    fprintf(cg->out, "  je .rv_list\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  lea 17(%%r10), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%rdi\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  movq $1, (%%rbx)\n");
    fprintf(cg->out, "  mov %%r10, 8(%%rbx)\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  xor %%r12, %%r12\n");
    fprintf(cg->out, ".rv_sloop:\n");
    fprintf(cg->out, "  cmp %%r10, %%r12\n");
    fprintf(cg->out, "  jae .rv_sdone\n");
    fprintf(cg->out, "  mov %%r10, %%r13\n");
    fprintf(cg->out, "  sub %%r12, %%r13\n");
    fprintf(cg->out, "  dec %%r13\n");
    fprintf(cg->out, "  movb (%%r8,%%r13,1), %%al\n");
    fprintf(cg->out, "  movb %%al, 16(%%rbx,%%r12,1)\n");
    fprintf(cg->out, "  inc %%r12\n");
    fprintf(cg->out, "  jmp .rv_sloop\n");
    fprintf(cg->out, ".rv_sdone:\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $4, %%rax\n");
    fprintf(cg->out, "  jmp .rv_out\n");
    fprintf(cg->out, ".rv_list:\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r12\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r13\n");
    fprintf(cg->out, "  lea 16(,%%r12,8), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  movq $1, (%%rbx)\n");
    fprintf(cg->out, "  mov %%r12, 8(%%rbx)\n");
    fprintf(cg->out, "  xor %%r14, %%r14\n");
    fprintf(cg->out, ".rv_lloop:\n");
    fprintf(cg->out, "  cmp %%r12, %%r14\n");
    fprintf(cg->out, "  jae .rv_ldone\n");
    fprintf(cg->out, "  mov %%r12, %%r15\n");
    fprintf(cg->out, "  sub %%r14, %%r15\n");
    fprintf(cg->out, "  dec %%r15\n");
    fprintf(cg->out, "  mov (%%r13,%%r15,8), %%rax\n");
    fprintf(cg->out, "  call _incref\n");
    fprintf(cg->out, "  mov %%rax, 16(%%rbx,%%r14,8)\n");
    fprintf(cg->out, "  inc %%r14\n");
    fprintf(cg->out, "  jmp .rv_lloop\n");
    fprintf(cg->out, ".rv_ldone:\n");
    fprintf(cg->out, "  mov (%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  add $8, %%rsp\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  shl $8, %%rax\n");
    fprintf(cg->out, "  or $5, %%rax\n");
    fprintf(cg->out, ".rv_out:\n");
    fprintf(cg->out, "  pop %%r15\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "_parse_int:\n");
    fprintf(cg->out, "  mov %%rdi, %%r10\n");
    fprintf(cg->out, "  mov %%rsi, %%r11\n");
    fprintf(cg->out, "  xor %%rcx, %%rcx\n");
    fprintf(cg->out, "  mov $1, %%r8\n");
    fprintf(cg->out, "  xor %%r9, %%r9\n");
    fprintf(cg->out, ".pl_skip:\n");
    fprintf(cg->out, "  cmp %%r11, %%r9\n");
    fprintf(cg->out, "  jae .pl_done\n");
    fprintf(cg->out, "  movb (%%r10,%%r9), %%al\n");
    fprintf(cg->out, "  cmpb $' ', %%al\n");
    fprintf(cg->out, "  jne .pl_sign\n");
    fprintf(cg->out, "  inc %%r9\n");
    fprintf(cg->out, "  jmp .pl_skip\n");
    fprintf(cg->out, ".pl_sign:\n");
    fprintf(cg->out, "  cmpb $'-', %%al\n");
    fprintf(cg->out, "  jne .pl_digits\n");
    fprintf(cg->out, "  mov $-1, %%r8\n");
    fprintf(cg->out, "  inc %%r9\n");
    fprintf(cg->out, ".pl_digits:\n");
    fprintf(cg->out, "  cmp %%r11, %%r9\n");
    fprintf(cg->out, "  jae .pl_done\n");
    fprintf(cg->out, "  movb (%%r10,%%r9), %%al\n");
    fprintf(cg->out, "  sub $'0', %%al\n");
    fprintf(cg->out, "  cmpb $9, %%al\n");
    fprintf(cg->out, "  ja .pl_done\n");
    fprintf(cg->out, "  imul $10, %%rcx\n");
    fprintf(cg->out, "  movsbl %%al, %%eax\n");
    fprintf(cg->out, "  add %%rax, %%rcx\n");
    fprintf(cg->out, "  inc %%r9\n");
    fprintf(cg->out, "  jmp .pl_digits\n");
    fprintf(cg->out, ".pl_done:\n");
    fprintf(cg->out, "  imul %%r8, %%rcx\n");
    fprintf(cg->out, "  mov %%rcx, %%rax\n");
    fprintf(cg->out, "  ret\n");

    /* Exact string equality over two tagged strings. rdi, rsi = tagged.
     * Returns 1 (tagged) if both are byte-identical strings, else 0. */
    fprintf(cg->out, "\n.globl _str_eq_bool\n");
    fprintf(cg->out, "_str_eq_bool:\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  shr $8, %%r8\n");
    fprintf(cg->out, "  mov %%rsi, %%r9\n");
    fprintf(cg->out, "  shr $8, %%r9\n");
    fprintf(cg->out, "  mov 8(%%r8), %%r10\n");
    fprintf(cg->out, "  mov 8(%%r9), %%r11\n");
    fprintf(cg->out, "  cmp %%r11, %%r10\n");
    fprintf(cg->out, "  jne .seq_false\n");
    fprintf(cg->out, "  lea 16(%%r8), %%r8\n");
    fprintf(cg->out, "  lea 16(%%r9), %%r9\n");
    fprintf(cg->out, "  xor %%rcx, %%rcx\n");
    fprintf(cg->out, ".seq_loop:\n");
    fprintf(cg->out, "  cmp %%r10, %%rcx\n");
    fprintf(cg->out, "  jae .seq_true\n");
    fprintf(cg->out, "  movb (%%r8,%%rcx), %%al\n");
    fprintf(cg->out, "  cmpb (%%r9,%%rcx), %%al\n");
    fprintf(cg->out, "  jne .seq_false\n");
    fprintf(cg->out, "  inc %%rcx\n");
    fprintf(cg->out, "  jmp .seq_loop\n");
    fprintf(cg->out, ".seq_true:\n");
    fprintf(cg->out, "  mov $1, %%rax\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".seq_false:\n");
    fprintf(cg->out, "  xor %%eax, %%eax\n");
    fprintf(cg->out, "  ret\n");

    /* Container element lookup with runtime dispatch. rdi = container (tagged),
     * rsi = index/key (tagged). For lists the index must be an integer; for maps
     * the key must be a string. Returns the element (incref'd) or 0 when the
     * index/key is invalid or missing. Owns and releases both inputs. */
    fprintf(cg->out, "\n.globl _index_get\n");
    fprintf(cg->out, "_index_get:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  push %%r15\n");
    fprintf(cg->out, "  push %%rdi\n");
    fprintf(cg->out, "  push %%rsi\n");
    fprintf(cg->out, "  mov %%rdi, %%rax\n");
    fprintf(cg->out, "  and $0xff, %%eax\n");
    fprintf(cg->out, "  cmp $5, %%rax\n");
    fprintf(cg->out, "  je .ig_list\n");
    fprintf(cg->out, "  cmp $6, %%rax\n");
    fprintf(cg->out, "  je .ig_map\n");
    fprintf(cg->out, "  jmp .ig_zero\n");
    fprintf(cg->out, ".ig_list:\n");
    fprintf(cg->out, "  mov 8(%%rsp), %%rax\n");
    fprintf(cg->out, "  shr $8, %%rax\n");
    fprintf(cg->out, "  mov (%%rsp), %%rcx\n");
    fprintf(cg->out, "  sar $8, %%rcx\n");
    fprintf(cg->out, "  test %%rcx, %%rcx\n");
    fprintf(cg->out, "  js .ig_zero\n");
    fprintf(cg->out, "  mov 8(%%rax), %%rdx\n");
    fprintf(cg->out, "  cmp %%rdx, %%rcx\n");
    fprintf(cg->out, "  jge .ig_zero\n");
    fprintf(cg->out, "  mov 16(%%rax,%%rcx,8), %%rax\n");
    fprintf(cg->out, "  call _incref\n");
    fprintf(cg->out, "  jmp .ig_finish\n");
    fprintf(cg->out, ".ig_map:\n");
    fprintf(cg->out, "  mov 8(%%rsp), %%rax\n");
    fprintf(cg->out, "  shr $8, %%rax\n");
    fprintf(cg->out, "  mov 8(%%rax), %%r13\n");
    fprintf(cg->out, "  lea 16(%%rax), %%r12\n");
    fprintf(cg->out, "  xor %%r15, %%r15\n");
    fprintf(cg->out, ".ig_search:\n");
    fprintf(cg->out, "  cmp %%r13, %%r15\n");
    fprintf(cg->out, "  jge .ig_zero\n");
    fprintf(cg->out, "  lea (%%r15,%%r15), %%rax\n");
    fprintf(cg->out, "  mov (%%r12,%%rax,8), %%rbx\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  mov (%%rsp), %%rsi\n");
    fprintf(cg->out, "  call _str_eq_bool\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jnz .ig_found\n");
    fprintf(cg->out, "  inc %%r15\n");
    fprintf(cg->out, "  jmp .ig_search\n");
    fprintf(cg->out, ".ig_found:\n");
    fprintf(cg->out, "  lea (%%r15,%%r15), %%rax\n");
    fprintf(cg->out, "  mov 8(%%r12,%%rax,8), %%rax\n");
    fprintf(cg->out, "  call _incref\n");
    fprintf(cg->out, "  jmp .ig_finish\n");
    fprintf(cg->out, ".ig_zero:\n");
    fprintf(cg->out, "  mov $0, %%rax\n");
    fprintf(cg->out, ".ig_finish:\n");
    fprintf(cg->out, "  mov %%rax, %%r8\n");
    fprintf(cg->out, "  mov 8(%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  mov (%%rsp), %%rax\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  add $16, %%rsp\n");
    fprintf(cg->out, "  mov %%r8, %%rax\n");
    fprintf(cg->out, "  pop %%r15\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _alloc\n");
    fprintf(cg->out, "_alloc:\n");
    fprintf(cg->out, "  add $15, %%rdi\n");
    fprintf(cg->out, "  and $-16, %%rdi\n");
    fprintf(cg->out, "  lea 32(%%rdi), %%r8\n");
    /* free-list first-fit */
    fprintf(cg->out, "  mov _gc_free(%%rip), %%r9\n");
    fprintf(cg->out, "  xor %%r10, %%r10\n");
    fprintf(cg->out, ".af_loop:\n");
    fprintf(cg->out, "  test %%r9, %%r9\n");
    fprintf(cg->out, "  jz .af_bump\n");
    fprintf(cg->out, "  cmp 8(%%r9), %%r8\n");
    fprintf(cg->out, "  jbe .af_take\n");
    fprintf(cg->out, "  mov %%r9, %%r10\n");
    fprintf(cg->out, "  mov 24(%%r9), %%r9\n");
    fprintf(cg->out, "  jmp .af_loop\n");
    fprintf(cg->out, ".af_take:\n");
    fprintf(cg->out, "  test %%r10, %%r10\n");
    fprintf(cg->out, "  jz .af_head\n");
    fprintf(cg->out, "  mov 24(%%r9), %%rax\n");
    fprintf(cg->out, "  mov %%rax, 24(%%r10)\n");
    fprintf(cg->out, "  jmp .af_use\n");
    fprintf(cg->out, ".af_head:\n");
    fprintf(cg->out, "  mov 24(%%r9), %%rax\n");
    fprintf(cg->out, "  mov %%rax, _gc_free(%%rip)\n");
    fprintf(cg->out, ".af_use:\n");
    fprintf(cg->out, "  movq $0, 16(%%r9)\n");
    fprintf(cg->out, "  lea 32(%%r9), %%rax\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".af_bump:\n");
    fprintf(cg->out, "  mov _gc_top(%%rip), %%r9\n");
    fprintf(cg->out, "  lea (%%r9,%%r8), %%r10\n");
    fprintf(cg->out, "  cmp _gc_hi(%%rip), %%r10\n");
    fprintf(cg->out, "  jbe .af_bumpok\n");
    fprintf(cg->out, "  jmp .af_fail\n");
    fprintf(cg->out, ".af_bumpok:\n");
    fprintf(cg->out, "  mov %%r8, 8(%%r9)\n");
    fprintf(cg->out, "  mov %%r10, _gc_top(%%rip)\n");
    fprintf(cg->out, "  lea 32(%%r9), %%rax\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".af_fail:\n");
    fprintf(cg->out, "  xor %%rax, %%rax\n");
    fprintf(cg->out, "  ret\n");

    /* reference counting */
    fprintf(cg->out, "\n.globl _incref\n");
    fprintf(cg->out, "_incref:\n");
    fprintf(cg->out, "  push %%rdx\n");
    fprintf(cg->out, "  mov %%rax, %%rdx\n");
    fprintf(cg->out, "  and $0xff, %%edx\n");
    fprintf(cg->out, "  cmp $4, %%rdx\n");
    fprintf(cg->out, "  je .inc_hdr\n");
    fprintf(cg->out, "  cmp $5, %%rdx\n");
    fprintf(cg->out, "  je .inc_hdr\n");
    fprintf(cg->out, "  cmp $6, %%rdx\n");
    fprintf(cg->out, "  je .inc_hdr\n");
    fprintf(cg->out, "  jmp .inc_done\n");
    fprintf(cg->out, ".inc_hdr:\n");
    fprintf(cg->out, "  mov %%rax, %%rdx\n");
    fprintf(cg->out, "  shr $8, %%rdx\n");
    fprintf(cg->out, "  lea _lit_lo(%%rip), %%rcx\n");
    fprintf(cg->out, "  cmp %%rcx, %%rdx\n");
    fprintf(cg->out, "  jb .inc_heap\n");
    fprintf(cg->out, "  lea _lit_hi(%%rip), %%rcx\n");
    fprintf(cg->out, "  cmp %%rcx, %%rdx\n");
    fprintf(cg->out, "  jae .inc_heap\n");
    fprintf(cg->out, "  jmp .inc_done\n");
    fprintf(cg->out, ".inc_heap:\n");
    fprintf(cg->out, "  incq (%%rdx)\n");
    fprintf(cg->out, ".inc_done:\n");
    fprintf(cg->out, "  pop %%rdx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _decref\n");
    fprintf(cg->out, "_decref:\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jz .dr_done\n");
    fprintf(cg->out, "  mov %%rax, %%rdx\n");
    fprintf(cg->out, "  and $0xff, %%edx\n");
    fprintf(cg->out, "  cmp $4, %%rdx\n");
    fprintf(cg->out, "  je .dr_obj\n");
    fprintf(cg->out, "  cmp $5, %%rdx\n");
    fprintf(cg->out, "  je .dr_obj\n");
    fprintf(cg->out, "  cmp $6, %%rdx\n");
    fprintf(cg->out, "  je .dr_obj\n");
    fprintf(cg->out, "  jmp .dr_done\n");
    fprintf(cg->out, ".dr_obj:\n");
    fprintf(cg->out, "  shr $8, %%rax\n");
    fprintf(cg->out, "  lea _lit_lo(%%rip), %%rcx\n");
    fprintf(cg->out, "  cmp %%rcx, %%rax\n");
    fprintf(cg->out, "  jb .dr_heap\n");
    fprintf(cg->out, "  lea _lit_hi(%%rip), %%rcx\n");
    fprintf(cg->out, "  cmp %%rcx, %%rax\n");
    fprintf(cg->out, "  jae .dr_heap\n");
    fprintf(cg->out, "  jmp .dr_done\n");
    fprintf(cg->out, ".dr_heap:\n");
    fprintf(cg->out, "  cmpq $0, (%%rax)\n");
    fprintf(cg->out, "  jle .dr_done\n");
    fprintf(cg->out, "  decq (%%rax)\n");
    fprintf(cg->out, "  jg .dr_done\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  lea -32(%%rax), %%rbx\n");
    fprintf(cg->out, "  push %%rdx\n");
    fprintf(cg->out, "  cmp $5, %%rdx\n");
    fprintf(cg->out, "  je .dr_list\n");
    fprintf(cg->out, "  cmp $6, %%rdx\n");
    fprintf(cg->out, "  je .dr_map\n");
    fprintf(cg->out, "  jmp .dr_pushfree\n");
    fprintf(cg->out, ".dr_list:\n");
    fprintf(cg->out, "  mov 8(%%rax), %%rcx\n");
    fprintf(cg->out, "  lea 16(%%rax), %%r12\n");
    fprintf(cg->out, "  xor %%r13, %%r13\n");
    fprintf(cg->out, "  jmp .dr_lcheck\n");
    fprintf(cg->out, ".dr_lbody:\n");
    fprintf(cg->out, "  mov (%%r12,%%r13,8), %%rax\n");
    fprintf(cg->out, "  push %%rcx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rcx\n");
    fprintf(cg->out, "  inc %%r13\n");
    fprintf(cg->out, ".dr_lcheck:\n");
    fprintf(cg->out, "  cmp %%r13, %%rcx\n");
    fprintf(cg->out, "  ja .dr_lbody\n");
    fprintf(cg->out, "  jmp .dr_pushfree\n");
    fprintf(cg->out, ".dr_map:\n");
    fprintf(cg->out, "  mov 8(%%rax), %%rcx\n");
    fprintf(cg->out, "  lea 16(%%rax), %%r12\n");
    fprintf(cg->out, "  xor %%r13, %%r13\n");
    fprintf(cg->out, "  jmp .dr_mcheck\n");
    fprintf(cg->out, ".dr_mbody:\n");
    fprintf(cg->out, "  lea (%%r13,%%r13), %%r14\n");
    fprintf(cg->out, "  mov (%%r12,%%r14,8), %%rax\n");
    fprintf(cg->out, "  push %%rcx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rcx\n");
    fprintf(cg->out, "  mov 8(%%r12,%%r14,8), %%rax\n");
    fprintf(cg->out, "  push %%rcx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  call _decref\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rcx\n");
    fprintf(cg->out, "  inc %%r13\n");
    fprintf(cg->out, ".dr_mcheck:\n");
    fprintf(cg->out, "  cmp %%r13, %%rcx\n");
    fprintf(cg->out, "  ja .dr_mbody\n");
    fprintf(cg->out, ".dr_pushfree:\n");
    fprintf(cg->out, "  pop %%rdx\n");
    fprintf(cg->out, "  mov _gc_free(%%rip), %%rax\n");
    fprintf(cg->out, "  mov %%rax, 24(%%rbx)\n");
    fprintf(cg->out, "  mov %%rbx, _gc_free(%%rip)\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, ".dr_done:\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.data\n");
    fprintf(cg->out, ".align 8\n");
    fprintf(cg->out, "_gc_heap:\n");
    fprintf(cg->out, "  .zero 1048576\n");
    fprintf(cg->out, "_gc_top:\n");
    fprintf(cg->out, "  .quad _gc_heap\n");
    fprintf(cg->out, "_gc_free:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, "_gc_lo:\n");
    fprintf(cg->out, "  .quad _gc_heap\n");
    fprintf(cg->out, "_gc_hi:\n");
    fprintf(cg->out, "  .quad _gc_heap + 1048576\n");
    fprintf(cg->out, ".text\n");
    
    return 0;
}
