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
                        fprintf(cg->out, "  mov $%lld, %%rax\n", (long long)instr->a->integer_value);
                    } else if (instr->a->kind == NIR_CONST_NUM) {
                        fprintf(cg->out, "  mov $%lld, %%rax\n", (long long)(int64_t)instr->a->number_value);
                    } else if (instr->a->kind == NIR_CONST_STR) {
                        int idx = get_const_index(cg, instr->a);
                        fprintf(cg->out, "  lea _str%d(%%rip), %%rax\n", idx);
                        fprintf(cg->out, "  movabsq $0x8000000000000000, %%rcx\n");
                        fprintf(cg->out, "  orq %%rcx, %%rax\n");
                    }
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_LOAD_VAR:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rax\n", slot);
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_STORE_VAR:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  pop %%rax\n");
                        fprintf(cg->out, "  mov %%rax, -%d(%%rbp)\n", slot);
                    }
                    break;
                    
                case NIR_ADD:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  add %%rdx, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_SUB:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  sub %%rdx, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_MUL:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  imul %%rdx, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_DIV:
                    fprintf(cg->out, "  pop %%rcx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  call _checked_div\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_MOD:
                    fprintf(cg->out, "  pop %%rcx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  call _checked_mod\n");
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
                    fprintf(cg->out, "  neg %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_NOT:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  test %%rax, %%rax\n");
                    fprintf(cg->out, "  sete %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
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
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setl %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_GT:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setg %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_LE:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setle %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_CMP_GE:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  setge %%al\n");
                    fprintf(cg->out, "  movzbl %%al, %%eax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_JUMP:
                    if (instr->target) {
                        fprintf(cg->out, "  jmp .L%s_%s\n", func->name, instr->target->name);
                    }
                    break;
                    
                case NIR_JUMP_IF_FALSE:
                    if (instr->target) {
                        fprintf(cg->out, "  pop %%rax\n");
                        fprintf(cg->out, "  test %%rax, %%rax\n");
                        fprintf(cg->out, "  jz .L%s_%s\n", func->name, instr->target->name);
                    }
                    break;
                    
                case NIR_JUMP_IF_TRUE:
                    if (instr->target) {
                        fprintf(cg->out, "  pop %%rax\n");
                        fprintf(cg->out, "  test %%rax, %%rax\n");
                        fprintf(cg->out, "  jnz .L%s_%s\n", func->name, instr->target->name);
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
                    fprintf(cg->out, "  call _print_value\n");
                    break;
                    
                case NIR_PRINT_STRING:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  call _print_value\n");
                    break;
                    
                case NIR_PRINT_VALUE_RAW:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  call _print_value_raw\n");
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
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  bt $63, %%rax\n");
                    fprintf(cg->out, "  jnc .len_int_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov %%rax, %%rdx\n");
                    fprintf(cg->out, "  shlq $1, %%rdx\n");
                    fprintf(cg->out, "  shrq $1, %%rdx\n");
                    fprintf(cg->out, "  movabsq $0x01000000000000, %%rcx\n");
                    fprintf(cg->out, "  cmp %%rcx, %%rdx\n");
                    fprintf(cg->out, "  jae .len_int_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov (%%rdx), %%rax\n");
                    fprintf(cg->out, "  jmp .len_done_%d\n", cg->label_counter);
                    fprintf(cg->out, ".len_int_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  mov $0, %%rax\n");
                    fprintf(cg->out, ".len_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_ABS:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp $0, %%rax\n");
                    fprintf(cg->out, "  jge .abs_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  neg %%rax\n");
                    fprintf(cg->out, ".abs_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_MIN:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  jle .min_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov %%rdx, %%rax\n");
                    fprintf(cg->out, ".min_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_MAX:
                    fprintf(cg->out, "  pop %%rdx\n");
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  cmp %%rdx, %%rax\n");
                    fprintf(cg->out, "  jge .max_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov %%rdx, %%rax\n");
                    fprintf(cg->out, ".max_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_TO_NUMBER:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  bt $63, %%rax\n");
                    fprintf(cg->out, "  jnc .tn_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov %%rax, %%rdx\n");
                    fprintf(cg->out, "  shlq $1, %%rdx\n");
                    fprintf(cg->out, "  shrq $1, %%rdx\n");
                    fprintf(cg->out, "  movabsq $0x01000000000000, %%rcx\n");
                    fprintf(cg->out, "  cmp %%rcx, %%rdx\n");
                    fprintf(cg->out, "  jae .tn_done_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov (%%rdx), %%rsi\n");
                    fprintf(cg->out, "  add $8, %%rdx\n");
                    fprintf(cg->out, "  mov %%rdx, %%rdi\n");
                    fprintf(cg->out, "  call _parse_int\n");
                    fprintf(cg->out, ".tn_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_CONCAT:
                    fprintf(cg->out, "  pop %%rsi\n");
                    fprintf(cg->out, "  pop %%rdi\n");
                    fprintf(cg->out, "  call _concat\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;

                case NIR_ORD:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  bt $63, %%rax\n");
                    fprintf(cg->out, "  jnc .ord_zero_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov %%rax, %%rdx\n");
                    fprintf(cg->out, "  shlq $1, %%rdx\n");
                    fprintf(cg->out, "  shrq $1, %%rdx\n");
                    fprintf(cg->out, "  movabsq $0x01000000000000, %%rcx\n");
                    fprintf(cg->out, "  cmp %%rcx, %%rdx\n");
                    fprintf(cg->out, "  jae .ord_zero_%d\n", cg->label_counter);
                    fprintf(cg->out, "  mov (%%rdx), %%rcx\n");
                    fprintf(cg->out, "  test %%rcx, %%rcx\n");
                    fprintf(cg->out, "  jz .ord_zero_%d\n", cg->label_counter);
                    fprintf(cg->out, "  lea 8(%%rdx), %%rdx\n");
                    fprintf(cg->out, "  movzbl (%%rdx), %%eax\n");
                    fprintf(cg->out, "  jmp .ord_done_%d\n", cg->label_counter);
                    fprintf(cg->out, ".ord_zero_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  mov $0, %%rax\n");
                    fprintf(cg->out, ".ord_done_%d:\n", cg->label_counter);
                    fprintf(cg->out, "  push %%rax\n");
                    cg->label_counter++;
                    break;

                case NIR_CHR:
                    fprintf(cg->out, "  pop %%rax\n");
                    fprintf(cg->out, "  push %%rbx\n");
                    fprintf(cg->out, "  mov %%rax, %%rbx\n");
                    fprintf(cg->out, "  mov $9, %%rdi\n");
                    fprintf(cg->out, "  call _alloc\n");
                    fprintf(cg->out, "  mov %%rax, %%r11\n");
                    fprintf(cg->out, "  movq $1, (%%r11)\n");
                    fprintf(cg->out, "  lea 8(%%r11), %%r12\n");
                    fprintf(cg->out, "  movb %%bl, (%%r12)\n");
                    fprintf(cg->out, "  pop %%rbx\n");
                    fprintf(cg->out, "  movabsq $0x8000000000000000, %%rcx\n");
                    fprintf(cg->out, "  orq %%rcx, %%r11\n");
                    fprintf(cg->out, "  mov %%r11, %%rax\n");
                    fprintf(cg->out, "  push %%rax\n");
                    break;
                    
                case NIR_ALLOC_LIST:
                    {
                        long n = instr->a ? (long)instr->a->integer_value : 0;
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  mov $%ld, %%rdi\n", n);
                        fprintf(cg->out, "  call _alloc\n");
                        fprintf(cg->out, "  mov %%rax, %%r11\n");
                        fprintf(cg->out, "  mov $%ld, %%rcx\n", n);
                        fprintf(cg->out, "  mov %%rcx, (%%r11)\n");
                        if (n > 0) {
                            fprintf(cg->out, "  lea 8(%%r11), %%r12\n");
                            fprintf(cg->out, "  mov $%ld, %%rcx\n", n - 1);
                            fprintf(cg->out, ".store_list_loop_%d:\n", lbl);
                            fprintf(cg->out, "  pop %%rax\n");
                            fprintf(cg->out, "  mov %%rax, (%%r12,%%rcx,8)\n");
                            fprintf(cg->out, "  dec %%rcx\n");
                            fprintf(cg->out, "  jns .store_list_loop_%d\n", lbl);
                        }
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  movabsq $0x8000000000000000, %%rcx\n");
                        fprintf(cg->out, "  orq %%rcx, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_LOAD_LIST_LEN:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rax\n", slot);
                        fprintf(cg->out, "  shlq $1, %%rax\n");
                        fprintf(cg->out, "  shrq $1, %%rax\n");
                        fprintf(cg->out, "  mov (%%rax), %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_LOAD_ELT:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  pop %%rcx\n");          /* index */
                        fprintf(cg->out, "  mov -%d(%%rbp), %%rdx\n", slot); /* list */
                        fprintf(cg->out, "  shlq $1, %%rdx\n");
                        fprintf(cg->out, "  shrq $1, %%rdx\n");
                        fprintf(cg->out, "  mov 8(%%rdx,%%rcx,8), %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_ALLOC_MAP:
                    {
                        long n = instr->a ? (long)instr->a->integer_value : 0;
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  mov $%ld, %%rdi\n", n);
                        fprintf(cg->out, "  call _alloc\n");
                        fprintf(cg->out, "  mov %%rax, %%r11\n");
                        fprintf(cg->out, "  mov $%ld, %%rcx\n", n);
                        fprintf(cg->out, "  mov %%rcx, (%%r11)\n");
                        if (n > 0) {
                            fprintf(cg->out, "  lea 8(%%r11), %%r12\n");
                            fprintf(cg->out, "  mov $%ld, %%rcx\n", 2 * n - 1);
                            fprintf(cg->out, ".store_map_loop_%d:\n", lbl);
                            fprintf(cg->out, "  pop %%rax\n");
                            fprintf(cg->out, "  mov %%rax, (%%r12,%%rcx,8)\n");
                            fprintf(cg->out, "  dec %%rcx\n");
                            fprintf(cg->out, "  jns .store_map_loop_%d\n", lbl);
                        }
                        fprintf(cg->out, "  mov %%r11, %%rax\n");
                        fprintf(cg->out, "  movabsq $0x8000000000000000, %%rcx\n");
                        fprintf(cg->out, "  orq %%rcx, %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                    }
                    break;
                    
                case NIR_LOAD_MAP_FIELD:
                    {
                        int kidx = get_const_index(cg, instr->b);
                        int lbl = cg->label_counter++;
                        fprintf(cg->out, "  pop %%r11\n");               /* map */
                        fprintf(cg->out, "  shlq $1, %%r11\n");
                        fprintf(cg->out, "  shrq $1, %%r11\n");
                        fprintf(cg->out, "  mov (%%r11), %%rcx\n");     /* count */
                        fprintf(cg->out, "  lea 8(%%r11), %%r12\n");    /* entries */
                        fprintf(cg->out, "  lea _str%d(%%rip), %%r13\n", kidx);
                        fprintf(cg->out, "  movabsq $0x8000000000000000, %%rax\n");
                        fprintf(cg->out, "  orq %%rax, %%r13\n");       /* tagged key */
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
                        fprintf(cg->out, "  push %%rax\n");
                        fprintf(cg->out, "  jmp .map_done_%d\n", lbl);
                        fprintf(cg->out, ".map_found_%d:\n", lbl);
                        fprintf(cg->out, "  lea (%%r14,%%r14), %%r15\n");
                        fprintf(cg->out, "  mov 8(%%r12,%%r15,8), %%rax\n");
                        fprintf(cg->out, "  push %%rax\n");
                        fprintf(cg->out, ".map_done_%d:\n", lbl);
                    }
                    break;

                case NIR_APPEND:
                    {
                        int slot = get_var_slot(func, instr->a->name);
                        fprintf(cg->out, "  pop %%rax\n");
                        fprintf(cg->out, "  mov -%d(%%rbp), %%r11\n", slot);
                        fprintf(cg->out, "  shlq $1, %%r11\n");
                        fprintf(cg->out, "  shrq $1, %%r11\n");
                        fprintf(cg->out, "  mov (%%r11), %%rcx\n");
                        fprintf(cg->out, "  mov %%rax, 8(%%r11,%%rcx,8)\n");
                        fprintf(cg->out, "  inc %%rcx\n");
                        fprintf(cg->out, "  mov %%rcx, (%%r11)\n");
                    }
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
    fprintf(cg->out, "  mov $60, %%rax\n");
    fprintf(cg->out, "  xor %%rdi, %%rdi\n");
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
    
    for (size_t i = 0; i < cg->program->constant_count; i++) {
        if (cg->program->constants[i]->kind == NIR_CONST_STR) {
            fprintf(cg->out, "_str%d:\n", (int)i);
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
    fprintf(cg->out, "  mov %%rax, %%rdi\n");
    fprintf(cg->out, "  bt $63, %%rdi\n");
    fprintf(cg->out, "  jnc .raw_int\n");
    fprintf(cg->out, "  mov %%rdi, %%rdx\n");
    fprintf(cg->out, "  shlq $1, %%rdx\n");
    fprintf(cg->out, "  shrq $1, %%rdx\n");
    fprintf(cg->out, "  movabsq $0x01000000000000, %%rcx\n");
    fprintf(cg->out, "  cmp %%rcx, %%rdx\n");
    fprintf(cg->out, "  jae .raw_int\n");
    fprintf(cg->out, "  mov %%rdx, %%rdi\n");
    fprintf(cg->out, "  mov (%%rdi), %%rsi\n");
    fprintf(cg->out, "  add $8, %%rdi\n");
    fprintf(cg->out, "  call _write_string\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".raw_int:\n");
    fprintf(cg->out, "  call _print_int\n");
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
    fprintf(cg->out, "  mov $512, %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  mov %%rax, %%rbx\n");
    fprintf(cg->out, "  lea 8(%%rbx), %%r12\n");
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
    fprintf(cg->out, "  mov %%rax, (%%rbx)\n");
    fprintf(cg->out, "  jmp .rl_tag\n");
    fprintf(cg->out, ".rl_zero:\n");
    fprintf(cg->out, "  movq $0, (%%rbx)\n");
    fprintf(cg->out, ".rl_tag:\n");
    fprintf(cg->out, "  movabsq $0x8000000000000000, %%rcx\n");
    fprintf(cg->out, "  orq %%rcx, %%rbx\n");
    fprintf(cg->out, "  mov %%rbx, %%rax\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _concat\n");
    fprintf(cg->out, "_concat:\n");
    fprintf(cg->out, "  mov %%rdi, %%r8\n");
    fprintf(cg->out, "  mov %%rsi, %%r9\n");
    fprintf(cg->out, "  shlq $1, %%r8\n");
    fprintf(cg->out, "  shrq $1, %%r8\n");
    fprintf(cg->out, "  shlq $1, %%r9\n");
    fprintf(cg->out, "  shrq $1, %%r9\n");
    fprintf(cg->out, "  mov (%%r8), %%r10\n");
    fprintf(cg->out, "  mov (%%r9), %%r11\n");
    fprintf(cg->out, "  lea 8(%%r8), %%r8\n");
    fprintf(cg->out, "  lea 8(%%r9), %%r9\n");
    fprintf(cg->out, "  push %%r8\n");
    fprintf(cg->out, "  push %%r9\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  push %%r11\n");
    fprintf(cg->out, "  lea 8(%%r10,%%r11), %%rdi\n");
    fprintf(cg->out, "  call _alloc\n");
    fprintf(cg->out, "  pop %%r11\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r9\n");
    fprintf(cg->out, "  pop %%r8\n");
    fprintf(cg->out, "  mov %%rax, %%rdx\n");
    fprintf(cg->out, "  lea (%%r10,%%r11), %%rcx\n");
    fprintf(cg->out, "  mov %%rcx, (%%rdx)\n");
    fprintf(cg->out, "  lea 8(%%rdx), %%rdx\n");
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
    fprintf(cg->out, "  sub $8, %%rdx\n");
    fprintf(cg->out, "  movabsq $0x8000000000000000, %%rcx\n");
    fprintf(cg->out, "  orq %%rcx, %%rdx\n");
    fprintf(cg->out, "  mov %%rdx, %%rax\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.globl _parse_int\n");
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
    fprintf(cg->out, "  movq $0, _gc_didcollect(%%rip)\n");
    fprintf(cg->out, "  lea 32(%%r9), %%rax\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".af_bump:\n");
    fprintf(cg->out, "  mov _gc_top(%%rip), %%r9\n");
    fprintf(cg->out, "  lea (%%r9,%%r8), %%r10\n");
    fprintf(cg->out, "  cmp _gc_hi(%%rip), %%r10\n");
    fprintf(cg->out, "  jbe .af_bumpok\n");
    fprintf(cg->out, "  mov _gc_didcollect(%%rip), %%rax\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jnz .af_fail\n");
    fprintf(cg->out, "  movq $1, _gc_didcollect(%%rip)\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  mov %%rdi, %%rbx\n");
    fprintf(cg->out, "  call _gc_collect\n");
    fprintf(cg->out, "  mov %%rbx, %%rdi\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  jmp _alloc\n");
    fprintf(cg->out, ".af_bumpok:\n");
    fprintf(cg->out, "  mov _gc_all(%%rip), %%rax\n");
    fprintf(cg->out, "  mov %%rax, 0(%%r9)\n");
    fprintf(cg->out, "  mov %%r9, _gc_all(%%rip)\n");
    fprintf(cg->out, "  mov %%r8, 8(%%r9)\n");
    fprintf(cg->out, "  movq $0, 16(%%r9)\n");
    fprintf(cg->out, "  mov %%r10, _gc_top(%%rip)\n");
    fprintf(cg->out, "  movq $0, _gc_didcollect(%%rip)\n");
    fprintf(cg->out, "  lea 32(%%r9), %%rax\n");
    fprintf(cg->out, "  ret\n");
    fprintf(cg->out, ".af_fail:\n");
    fprintf(cg->out, "  xor %%rax, %%rax\n");
    fprintf(cg->out, "  ret\n");

    /* mark helper: rax=word; marks containing chunk; sets r15 if newly marked */
    fprintf(cg->out, "\n.globl _gc_mark_word\n");
    fprintf(cg->out, "_gc_mark_word:\n");
    fprintf(cg->out, "  push %%r9\n");
    fprintf(cg->out, "  push %%r10\n");
    fprintf(cg->out, "  push %%r11\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  shlq $1, %%rax\n");
    fprintf(cg->out, "  shrq $1, %%rax\n");
    fprintf(cg->out, "  cmp _gc_lo(%%rip), %%rax\n");
    fprintf(cg->out, "  jb .gmw_no\n");
    fprintf(cg->out, "  cmp _gc_hi(%%rip), %%rax\n");
    fprintf(cg->out, "  jae .gmw_no\n");
    fprintf(cg->out, "  mov _gc_all(%%rip), %%r9\n");
    fprintf(cg->out, ".gmw_loop:\n");
    fprintf(cg->out, "  test %%r9, %%r9\n");
    fprintf(cg->out, "  jz .gmw_no\n");
    fprintf(cg->out, "  lea 32(%%r9), %%r10\n");
    fprintf(cg->out, "  cmp %%r10, %%rax\n");
    fprintf(cg->out, "  jb .gmw_next\n");
    fprintf(cg->out, "  mov 8(%%r9), %%r11\n");
    fprintf(cg->out, "  lea (%%r10,%%r11), %%r11\n");
    fprintf(cg->out, "  cmp %%r11, %%rax\n");
    fprintf(cg->out, "  jb .gmw_found\n");
    fprintf(cg->out, ".gmw_next:\n");
    fprintf(cg->out, "  mov 0(%%r9), %%r9\n");
    fprintf(cg->out, "  jmp .gmw_loop\n");
    fprintf(cg->out, ".gmw_found:\n");
    fprintf(cg->out, "  mov 16(%%r9), %%rax\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jnz .gmw_no\n");
    fprintf(cg->out, "  movq $1, 16(%%r9)\n");
    fprintf(cg->out, "  mov $1, %%r15\n");
    fprintf(cg->out, ".gmw_no:\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%r11\n");
    fprintf(cg->out, "  pop %%r10\n");
    fprintf(cg->out, "  pop %%r9\n");
    fprintf(cg->out, "  ret\n");

    /* conservative mark-and-sweep */
    fprintf(cg->out, "\n.globl _gc_collect\n");
    fprintf(cg->out, "_gc_collect:\n");
    fprintf(cg->out, "  push %%rbx\n");
    fprintf(cg->out, "  push %%r12\n");
    fprintf(cg->out, "  push %%r13\n");
    fprintf(cg->out, "  push %%r14\n");
    fprintf(cg->out, "  push %%r15\n");
    /* mark roots: scan stack */
    fprintf(cg->out, "  mov %%rsp, %%r9\n");
    fprintf(cg->out, "  lea 1048576(%%rsp), %%r10\n");
    fprintf(cg->out, ".gc_rootscan:\n");
    fprintf(cg->out, "  cmp %%r10, %%r9\n");
    fprintf(cg->out, "  jae .gc_transitive\n");
    fprintf(cg->out, "  mov (%%r9), %%rax\n");
    fprintf(cg->out, "  call _gc_mark_word\n");
    fprintf(cg->out, "  add $8, %%r9\n");
    fprintf(cg->out, "  jmp .gc_rootscan\n");
    /* transitive marking */
    fprintf(cg->out, ".gc_transitive:\n");
    fprintf(cg->out, "  mov $8, %%r14\n");
    fprintf(cg->out, ".gc_pass:\n");
    fprintf(cg->out, "  xor %%r15, %%r15\n");
    fprintf(cg->out, "  mov _gc_all(%%rip), %%r9\n");
    fprintf(cg->out, ".gc_scanobj:\n");
    fprintf(cg->out, "  test %%r9, %%r9\n");
    fprintf(cg->out, "  jz .gc_pass_check\n");
    fprintf(cg->out, "  cmpq $0, 16(%%r9)\n");
    fprintf(cg->out, "  je .gc_obj_next\n");
    fprintf(cg->out, "  lea 32(%%r9), %%r12\n");
    fprintf(cg->out, "  mov 8(%%r9), %%r13\n");
    fprintf(cg->out, "  lea (%%r12,%%r13), %%r13\n");
    fprintf(cg->out, ".gc_wordloop:\n");
    fprintf(cg->out, "  cmp %%r13, %%r12\n");
    fprintf(cg->out, "  jae .gc_obj_next\n");
    fprintf(cg->out, "  mov (%%r12), %%rax\n");
    fprintf(cg->out, "  call _gc_mark_word\n");
    fprintf(cg->out, "  add $8, %%r12\n");
    fprintf(cg->out, "  jmp .gc_wordloop\n");
    fprintf(cg->out, ".gc_obj_next:\n");
    fprintf(cg->out, "  mov 0(%%r9), %%r9\n");
    fprintf(cg->out, "  jmp .gc_scanobj\n");
    fprintf(cg->out, ".gc_pass_check:\n");
    fprintf(cg->out, "  test %%r15, %%r15\n");
    fprintf(cg->out, "  jz .gc_sweep\n");
    fprintf(cg->out, "  dec %%r14\n");
    fprintf(cg->out, "  jnz .gc_pass\n");
    /* sweep */
    fprintf(cg->out, ".gc_sweep:\n");
    fprintf(cg->out, "  mov _gc_all(%%rip), %%r9\n");
    fprintf(cg->out, ".gc_sweep_loop:\n");
    fprintf(cg->out, "  test %%r9, %%r9\n");
    fprintf(cg->out, "  jz .gc_done\n");
    fprintf(cg->out, "  mov 16(%%r9), %%rax\n");
    fprintf(cg->out, "  movq $0, 16(%%r9)\n");
    fprintf(cg->out, "  test %%rax, %%rax\n");
    fprintf(cg->out, "  jnz .gc_sweep_next\n");
    fprintf(cg->out, "  mov _gc_free(%%rip), %%rax\n");
    fprintf(cg->out, "  mov %%rax, 24(%%r9)\n");
    fprintf(cg->out, "  mov %%r9, _gc_free(%%rip)\n");
    fprintf(cg->out, ".gc_sweep_next:\n");
    fprintf(cg->out, "  mov 0(%%r9), %%r9\n");
    fprintf(cg->out, "  jmp .gc_sweep_loop\n");
    fprintf(cg->out, ".gc_done:\n");
    fprintf(cg->out, "  pop %%r15\n");
    fprintf(cg->out, "  pop %%r14\n");
    fprintf(cg->out, "  pop %%r13\n");
    fprintf(cg->out, "  pop %%r12\n");
    fprintf(cg->out, "  pop %%rbx\n");
    fprintf(cg->out, "  ret\n");

    fprintf(cg->out, "\n.data\n");
    fprintf(cg->out, ".align 8\n");
    fprintf(cg->out, "_gc_heap:\n");
    fprintf(cg->out, "  .zero 1048576\n");
    fprintf(cg->out, "_gc_top:\n");
    fprintf(cg->out, "  .quad _gc_heap\n");
    fprintf(cg->out, "_gc_all:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, "_gc_free:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, "_gc_lo:\n");
    fprintf(cg->out, "  .quad _gc_heap\n");
    fprintf(cg->out, "_gc_hi:\n");
    fprintf(cg->out, "  .quad _gc_heap + 1048576\n");
    fprintf(cg->out, "_gc_didcollect:\n");
    fprintf(cg->out, "  .quad 0\n");
    fprintf(cg->out, ".text\n");
    
    return 0;
}
