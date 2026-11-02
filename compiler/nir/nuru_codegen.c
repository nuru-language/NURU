#define _DEFAULT_SOURCE
#include "nuru_codegen.h"
#include "emit_runtime.h"
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
                    fprintf(cg->out, "  mov $%d, _err_line(%%rip)\n", instr->line);
                    fprintf(cg->out, "  call _check_int\n");
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
                    fprintf(cg->out, "  mov $%d, _err_line(%%rip)\n", instr->line);
                    fprintf(cg->out, "  call _check_int\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_MUL:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rdx\n");
                    fprintf(cg->out, "  mov $%d, _err_line(%%rip)\n", instr->line);
                    fprintf(cg->out, "  call _checked_mul\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_DIV:
                    fprintf(cg->out, "  pop %%rcx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rcx\n");
                    fprintf(cg->out, "  mov $%d, _err_line(%%rip)\n", instr->line);
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
                    fprintf(cg->out, "  mov $%d, _err_line(%%rip)\n", instr->line);
                    fprintf(cg->out, "  call _checked_mod\n");
                    fprintf(cg->out, "  shl $8, %%rax\n");
                    fprintf(cg->out, "  or $2, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_REMOVE:
                    fprintf(cg->out, "  pop %%rsi\n");
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _remove\n");
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

                case NIR_LOAD_ERR:
                    fprintf(cg->out, "  mov _err_value(%%rip), %%rax\n");
                    fprintf(cg->out, "  xor %%ecx, %%ecx\n");
                    fprintf(cg->out, "  mov %%rcx, _err_value(%%rip)\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_NEG:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sar $8, %%rax\n");
                    fprintf(cg->out, "  neg %%rax\n");
                    fprintf(cg->out, "  mov $%d, _err_line(%%rip)\n", instr->line);
                    fprintf(cg->out, "  call _check_int\n");
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
                        int is_message = 0;
                        int is_line = 0;
                        if (instr->b && instr->b->kind == NIR_CONST_STR) {
                            if (instr->b->string_value.length == 7 &&
                                memcmp(instr->b->string_value.data, "message", 7) == 0) {
                                is_message = 1;
                            } else if (instr->b->string_value.length == 4 &&
                                       memcmp(instr->b->string_value.data, "line", 4) == 0) {
                                is_line = 1;
                            }
                        }
                        fprintf(cg->out, "  pop %%r11\n");               /* map or error */
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  and $0xff, %%eax\n");
                        fprintf(cg->out, "  cmp $7, %%rax\n");
                        fprintf(cg->out, "  jne .lf_map_%d\n", lbl);
                        /* error object */
                        fprintf(cg->out, "  shr $8, %%r11\n");
                        if (is_line) {
                            fprintf(cg->out, "  mov 24(%%r11), %%rax\n");
                            fprintf(cg->out, "  call _incref\n");
                        } else if (is_message) {
                            fprintf(cg->out, "  mov 8(%%r11), %%rax\n");
                            fprintf(cg->out, "  call _incref\n");
                        } else {
                            fprintf(cg->out, "  mov $2, %%rax\n");
                        }
                        fprintf(cg->out, "  jmp .lf_push_%d\n", lbl);
                        fprintf(cg->out, ".lf_map_%d:\n", lbl);
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
                        fprintf(cg->out, "  mov $2, %%rax\n");
                        fprintf(cg->out, "  jmp .lf_push_%d\n", lbl);
                        fprintf(cg->out, ".map_found_%d:\n", lbl);
                        fprintf(cg->out, "  lea (%%r14,%%r14), %%r15\n");
                        fprintf(cg->out, "  mov 8(%%r12,%%r15,8), %%rax\n");
                        fprintf(cg->out, "  call _incref\n");
                        fprintf(cg->out, ".lf_push_%d:\n", lbl);
                        fprintf(cg->out, "  push %%rax\n");
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  call _decref\n");
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
    if (!cg->out) {
        free(cg);
        return NULL;
    }
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
    fprintf(cg->out, "  mov %%rsp, _stack_low(%%rip)\n");
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
    
    nuru_emit_literal_data(cg->out, cg->program);

    fprintf(cg->out, "\n.text\n");
    
    nuru_emit_io_helpers(cg->out);

    nuru_emit_rt_helpers(cg->out);
    nuru_emit_gc_collector(cg->out);
    nuru_emit_gc_data(cg->out);
    
    return 0;
}
