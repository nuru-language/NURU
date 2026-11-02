#define _DEFAULT_SOURCE
#include "nuru_irgen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static NIRBasicBlock* make_block(IRGen* irgen, const char* name) {
    return nir_block_create(irgen->current_func, name);
}

static void emit(IRGen* irgen, NIROpcode op, NIRValue* a, NIRValue* b, NIRBasicBlock* target) {
    nir_emit(irgen->current_block, op, a, b, target, irgen->current_line);
}

static void emit_call(IRGen* irgen, const char* name, NIRValue* argc) {
    nir_emit(irgen->current_block, NIR_CALL, nir_var_ref(name), argc, NULL, 0);
}

// Forward declarations
static void emit_expr(IRGen* irgen, ASTNode* node);
static void emit_stmt(IRGen* irgen, ASTNode* node);

static void emit_expr(IRGen* irgen, ASTNode* node) {
    if (!node) return;
    irgen->current_line = node->line;
    
    switch (node->type) {
        case AST_INTEGER_LITERAL:
            emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, node->integer_literal.value), NULL, NULL);
            break;
            
        case AST_NUMBER_LITERAL:
            emit(irgen, NIR_LOAD_CONST, nir_const_num(irgen->program, node->number_literal.value), NULL, NULL);
            break;
            
        case AST_STRING_LITERAL:
            emit(irgen, NIR_LOAD_CONST, nir_const_str(irgen->program, node->string_literal.value, node->string_literal.length), NULL, NULL);
            break;
            
        case AST_BOOLEAN_LITERAL:
            emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, node->boolean_literal.value ? 1 : 0), NULL, NULL);
            break;
            
        case AST_EMPTY_LITERAL:
            emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, 0), NULL, NULL);
            break;
            
        case AST_IDENTIFIER:
            emit(irgen, NIR_LOAD_VAR, nir_var_ref(node->identifier.name), NULL, NULL);
            break;
            
        case AST_BINARY_OP:
            {
                char* op = node->binary_op.operator;
                if (strcmp(op, "and") == 0 || strcmp(op, "or") == 0) {
                    NIRBasicBlock* false_block = make_block(irgen, "logf");
                    NIRBasicBlock* true_block = make_block(irgen, "logt");
                    NIRBasicBlock* end_block = make_block(irgen, "loge");

                    emit_expr(irgen, node->binary_op.left);

                    if (strcmp(op, "and") == 0) {
                        nir_emit(irgen->current_block, NIR_JUMP_IF_FALSE, NULL, NULL, false_block, 0);
                        emit_expr(irgen, node->binary_op.right);
                        nir_emit(irgen->current_block, NIR_JUMP_IF_FALSE, NULL, NULL, false_block, 0);
                        nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, true_block, 0);
                    } else {
                        nir_emit(irgen->current_block, NIR_JUMP_IF_TRUE, NULL, NULL, true_block, 0);
                        emit_expr(irgen, node->binary_op.right);
                        nir_emit(irgen->current_block, NIR_JUMP_IF_TRUE, NULL, NULL, true_block, 0);
                        nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, false_block, 0);
                    }

                    irgen->current_block = true_block;
                    emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, 1), NULL, NULL);
                    nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, end_block, 0);

                    irgen->current_block = false_block;
                    emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, 0), NULL, NULL);
                    nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, end_block, 0);

                    irgen->current_block = end_block;
                    break;
                }
                emit_expr(irgen, node->binary_op.left);
                emit_expr(irgen, node->binary_op.right);
                if (strcmp(op, "plus") == 0) emit(irgen, NIR_ADD, NULL, NULL, NULL);
                else if (strcmp(op, "minus") == 0) emit(irgen, NIR_SUB, NULL, NULL, NULL);
                else if (strcmp(op, "times") == 0) emit(irgen, NIR_MUL, NULL, NULL, NULL);
                else if (strcmp(op, "divided by") == 0) emit(irgen, NIR_DIV, NULL, NULL, NULL);
                else if (strcmp(op, "remainder of") == 0) emit(irgen, NIR_MOD, NULL, NULL, NULL);
                else if (strcmp(op, "is") == 0) emit(irgen, NIR_CMP_EQ, NULL, NULL, NULL);
                else if (strcmp(op, "is not") == 0) emit(irgen, NIR_CMP_NE, NULL, NULL, NULL);
                else if (strcmp(op, "is above") == 0) emit(irgen, NIR_CMP_GT, NULL, NULL, NULL);
                else if (strcmp(op, "is below") == 0) emit(irgen, NIR_CMP_LT, NULL, NULL, NULL);
                else if (strcmp(op, "is at least") == 0) emit(irgen, NIR_CMP_GE, NULL, NULL, NULL);
                else if (strcmp(op, "is at most") == 0) emit(irgen, NIR_CMP_LE, NULL, NULL, NULL);
            }
            break;
            
        case AST_UNARY_OP:
            emit_expr(irgen, node->unary_op.operand);
            {
                char* op = node->unary_op.operator;
                if (strcmp(op, "not") == 0) emit(irgen, NIR_NOT, NULL, NULL, NULL);
                else if (strcmp(op, "minus") == 0) emit(irgen, NIR_NEG, NULL, NULL, NULL);
            }
            break;
            
        case AST_FUNCTION_CALL:
            {
                if (strcmp(node->function_call.name, "ask") == 0) {
                    emit(irgen, NIR_READ_LINE, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "len") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_LEN, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "abs") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_ABS, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "min") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_MIN, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "max") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_MAX, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "to_number") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_TO_NUMBER, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "concat") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_CONCAT, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "ord") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_ORD, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "chr") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_CHR, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "pow") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_POW, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "contains") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_CONTAINS, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "substring") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit_expr(irgen, node->function_call.arguments[2]);
                    emit(irgen, NIR_SUBSTRING, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "upper") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_UPPER, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "lower") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_LOWER, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "index") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_INDEX, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "to_string") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_TO_STRING, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "reverse") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_REVERSE, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "remove") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_REMOVE, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "read_file") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit(irgen, NIR_READ_FILE, NULL, NULL, NULL);
                    break;
                }
                if (strcmp(node->function_call.name, "write_file") == 0) {
                    emit_expr(irgen, node->function_call.arguments[0]);
                    emit_expr(irgen, node->function_call.arguments[1]);
                    emit(irgen, NIR_WRITE_FILE, NULL, NULL, NULL);
                    break;
                }
                int argc = (int)node->function_call.arg_count;
                for (size_t i = 0; i < node->function_call.arg_count; i++) {
                    emit_expr(irgen, node->function_call.arguments[i]);
                }
                emit_call(irgen, node->function_call.name, nir_const_int(irgen->program, argc));
            }
            break;

        case AST_LIST_LITERAL:
            {
                size_t n = node->list_literal.count;
                for (size_t i = 0; i < n; i++) {
                    emit_expr(irgen, node->list_literal.items[i]);
                }
                emit(irgen, NIR_ALLOC_LIST, nir_const_int(irgen->program, (int64_t)n), NULL, NULL);
            }
            break;

        case AST_MAP_LITERAL:
            {
                size_t n = node->map_literal.count;
                for (size_t i = 0; i < n; i++) {
                    ASTNode* entry = node->map_literal.entries[i];
                    emit(irgen, NIR_LOAD_CONST,
                         nir_const_str(irgen->program, entry->map_entry.key, strlen(entry->map_entry.key)),
                         NULL, NULL);
                    emit_expr(irgen, entry->map_entry.value);
                }
                emit(irgen, NIR_ALLOC_MAP, nir_const_int(irgen->program, (int64_t)n), NULL, NULL);
            }
            break;

        case AST_MEMBER_ACCESS:
            emit_expr(irgen, node->member_access.object);
            emit(irgen, NIR_LOAD_MAP_FIELD, NULL,
                 nir_const_str(irgen->program, node->member_access.property, strlen(node->member_access.property)),
                 NULL);
            break;

        case AST_INDEX_ACCESS:
            emit_expr(irgen, node->index_access.object);
            emit_expr(irgen, node->index_access.index);
            emit(irgen, NIR_LOAD_ELT_EXPR, NULL, NULL, NULL);
            break;
            
        default:
            break;
    }
}

static void emit_stmt(IRGen* irgen, ASTNode* node) {
    if (!node) return;
    irgen->current_line = node->line;
    
    switch (node->type) {
        case AST_DECLARATION:
        case AST_DECLARATION_CHANGEABLE:
            {
                char* name = node->type == AST_DECLARATION ? 
                    strdup(node->declaration.name) : strdup(node->declaration_changeable.name);
                nir_function_add_local(irgen->current_func, name);
                if (node->declaration.value) emit_expr(irgen, node->declaration.value);
                emit(irgen, NIR_STORE_VAR, nir_var_ref(name), NULL, NULL);
                free(name);
            }
            break;
            
        case AST_ASSIGNMENT:
            emit_expr(irgen, node->assignment.value);
            emit(irgen, NIR_STORE_VAR, nir_var_ref(node->assignment.name), NULL, NULL);
            break;

        case AST_APPEND:
            emit_expr(irgen, node->append.value);
            emit(irgen, NIR_APPEND, nir_var_ref(node->append.list_name), NULL, NULL);
            break;
            
        case AST_SHOW:
            if (!node->show.expression) {
                break;
            }
            if (node->show.expression->type == AST_INTERP_STRING) {
                for (size_t p = 0; p < node->show.expression->interp_string.part_count; p++) {
                    ASTNode* part = node->show.expression->interp_string.parts[p];
                    emit_expr(irgen, part);
                    emit(irgen, NIR_PRINT_VALUE_RAW, NULL, NULL, NULL);
                }
                emit(irgen, NIR_PRINT_NEWLINE, NULL, NULL, NULL);
            } else if (node->show.expression->type == AST_STRING_LITERAL) {
                emit_expr(irgen, node->show.expression);
                emit(irgen, NIR_PRINT_STRING, NULL, NULL, NULL);
            } else {
                emit_expr(irgen, node->show.expression);
                emit(irgen, NIR_PRINT_INT, NULL, NULL, NULL);
            }
            break;
            
        case AST_RETURN:
            if (node->return_stmt.expression) {
                emit_expr(irgen, node->return_stmt.expression);
                emit(irgen, NIR_RETURN_VALUE, NULL, NULL, NULL);
            } else {
                emit(irgen, NIR_RETURN, NULL, NULL, NULL);
            }
            break;

        case AST_FUNCTION_CALL:
            emit_expr(irgen, node);
            emit(irgen, NIR_DISCARD, NULL, NULL, NULL);
            break;
            
        case AST_FUNCTION_DECL:
            {
                char* name = strdup(node->function_decl.name);
                NIRFunction* old_func = irgen->current_func;
                NIRBasicBlock* old_block = irgen->current_block;
                irgen->current_func = nir_function_create(irgen->program, name);
                irgen->current_block = irgen->current_func->entry;
                
                for (size_t i = 0; i < node->function_decl.param_count; i++) {
                    char* pname = strdup(node->function_decl.parameters[i]->identifier.name);
                    nir_function_add_param(irgen->current_func, pname);
                    nir_function_add_local(irgen->current_func, pname);
                    free(pname);
                }
                
                if (node->function_decl.body) {
                    for (size_t i = 0; i < node->function_decl.body->block.statement_count; i++) {
                        emit_stmt(irgen, node->function_decl.body->block.statements[i]);
                    }
                }
                emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, 0), NULL, NULL);
                emit(irgen, NIR_RETURN_VALUE, NULL, NULL, NULL);
                
                irgen->current_func = old_func;
                irgen->current_block = old_block;
                free(name);
            }
            break;
            
        case AST_IF:
            {
                emit_expr(irgen, node->if_stmt.condition);
                NIRBasicBlock* cond_block = irgen->current_block;

                NIRBasicBlock* then_block = make_block(irgen, "then");
                NIRBasicBlock* else_block = make_block(irgen, "else");
                NIRBasicBlock* end_block = make_block(irgen, "end");

                nir_emit(cond_block, NIR_JUMP_IF_FALSE, NULL, NULL, else_block, 0);
                nir_emit(cond_block, NIR_JUMP, NULL, NULL, then_block, 0);

                irgen->current_block = then_block;
                if (node->if_stmt.then_branch) {
                    if (node->if_stmt.then_branch->type == AST_BLOCK) {
                        for (size_t i = 0; i < node->if_stmt.then_branch->block.statement_count; i++) {
                            emit_stmt(irgen, node->if_stmt.then_branch->block.statements[i]);
                        }
                    } else {
                        emit_stmt(irgen, node->if_stmt.then_branch);
                    }
                }
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, end_block, 0);

                irgen->current_block = else_block;
                if (node->if_stmt.else_branch) {
                    if (node->if_stmt.else_branch->type == AST_BLOCK) {
                        for (size_t i = 0; i < node->if_stmt.else_branch->block.statement_count; i++) {
                            emit_stmt(irgen, node->if_stmt.else_branch->block.statements[i]);
                        }
                    } else {
                        emit_stmt(irgen, node->if_stmt.else_branch);
                    }
                }
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, end_block, 0);

                irgen->current_block = end_block;
            }
            break;
            
        case AST_WHILE:
            {
                NIRBasicBlock* pre_block = irgen->current_block;
                NIRBasicBlock* cond_block = make_block(irgen, "cond");
                NIRBasicBlock* body_block = make_block(irgen, "body");
                NIRBasicBlock* end_block = make_block(irgen, "end");

                nir_emit(pre_block, NIR_JUMP, NULL, NULL, cond_block, 0);

                irgen->current_block = cond_block;
                emit_expr(irgen, node->while_stmt.condition);
                {
                    NIRBasicBlock* cblock = irgen->current_block;
                    nir_emit(cblock, NIR_JUMP_IF_FALSE, NULL, NULL, end_block, 0);
                    nir_emit(cblock, NIR_JUMP, NULL, NULL, body_block, 0);
                }

                irgen->current_block = body_block;
                irgen->break_blocks[irgen->loop_depth] = end_block;
                irgen->continue_blocks[irgen->loop_depth] = cond_block;
                irgen->loop_depth++;
                if (node->while_stmt.body->type == AST_BLOCK) {
                    for (size_t i = 0; i < node->while_stmt.body->block.statement_count; i++) {
                        emit_stmt(irgen, node->while_stmt.body->block.statements[i]);
                    }
                } else {
                    emit_stmt(irgen, node->while_stmt.body);
                }
                irgen->loop_depth--;
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, cond_block, 0);

                irgen->current_block = end_block;
            }
            break;
            
        case AST_REPEAT:
            {
                char counter[32];
                snprintf(counter, sizeof(counter), "repeat_counter_%d", irgen->temp_counter++);
                nir_function_add_local(irgen->current_func, counter);
                NIRBasicBlock* pre_block = irgen->current_block;
                emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, 0), NULL, NULL);
                emit(irgen, NIR_STORE_VAR, nir_var_ref(counter), NULL, NULL);

                NIRBasicBlock* cond_block = make_block(irgen, "cond");
                NIRBasicBlock* body_block = make_block(irgen, "body");
                NIRBasicBlock* step_block = make_block(irgen, "step");
                NIRBasicBlock* end_block = make_block(irgen, "end");

                nir_emit(pre_block, NIR_JUMP, NULL, NULL, cond_block, 0);

                irgen->current_block = cond_block;
                emit(irgen, NIR_LOAD_VAR, nir_var_ref(counter), NULL, NULL);
                emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, node->repeat.count), NULL, NULL);
                emit(irgen, NIR_CMP_LT, NULL, NULL, NULL);
                nir_emit(cond_block, NIR_JUMP_IF_FALSE, NULL, NULL, end_block, 0);
                nir_emit(cond_block, NIR_JUMP, NULL, NULL, body_block, 0);

                irgen->current_block = body_block;
                irgen->break_blocks[irgen->loop_depth] = end_block;
                irgen->continue_blocks[irgen->loop_depth] = step_block;
                irgen->loop_depth++;
                if (node->repeat.body->type == AST_BLOCK) {
                    for (size_t i = 0; i < node->repeat.body->block.statement_count; i++) {
                        emit_stmt(irgen, node->repeat.body->block.statements[i]);
                    }
                } else {
                    emit_stmt(irgen, node->repeat.body);
                }
                irgen->loop_depth--;
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, step_block, 0);

                irgen->current_block = step_block;
                emit(irgen, NIR_LOAD_VAR, nir_var_ref(counter), NULL, NULL);
                emit(irgen, NIR_LOAD_CONST, nir_const_int(irgen->program, 1), NULL, NULL);
                emit(irgen, NIR_ADD, NULL, NULL, NULL);
                emit(irgen, NIR_STORE_VAR, nir_var_ref(counter), NULL, NULL);
                nir_emit(step_block, NIR_JUMP, NULL, NULL, cond_block, 0);

                irgen->current_block = end_block;
            }
            break;

        case AST_FOR_EACH:
            {
                ASTNode* iterable = node->for_each.iterable;
                if (!iterable) {
                    break;
                }
                char iterbuf[32];
                snprintf(iterbuf, sizeof(iterbuf), "for_iter_%d", irgen->temp_counter++);
                nir_function_add_local(irgen->current_func, iterbuf);
                const char* listname = iterbuf;
                const char* var = node->for_each.variable;

                char counter[32];
                snprintf(counter, sizeof(counter), "for_i_%d", irgen->temp_counter++);
                nir_function_add_local(irgen->current_func, counter);
                if (!nir_function_has_var(irgen->current_func, var)) {
                    nir_function_add_local(irgen->current_func, var);
                }

                NIRBasicBlock* pre_block = irgen->current_block;
                emit_expr(irgen, iterable);
                nir_emit(pre_block, NIR_STORE_VAR, nir_var_ref(iterbuf), NULL, NULL, 0);
                nir_emit(pre_block, NIR_LOAD_CONST, nir_const_int(irgen->program, 0), NULL, NULL, 0);
                nir_emit(pre_block, NIR_STORE_VAR, nir_var_ref(counter), NULL, NULL, 0);

                NIRBasicBlock* cond_block = make_block(irgen, "cond");
                NIRBasicBlock* body_block = make_block(irgen, "body");
                NIRBasicBlock* step_block = make_block(irgen, "step");
                NIRBasicBlock* end_block = make_block(irgen, "end");

                nir_emit(pre_block, NIR_JUMP, NULL, NULL, cond_block, 0);

                irgen->current_block = cond_block;
                nir_emit(cond_block, NIR_LOAD_VAR, nir_var_ref(counter), NULL, NULL, 0);
                nir_emit(cond_block, NIR_LOAD_LIST_LEN, nir_var_ref(listname), NULL, NULL, 0);
                nir_emit(cond_block, NIR_CMP_LT, NULL, NULL, NULL, 0);
                nir_emit(cond_block, NIR_JUMP_IF_FALSE, NULL, NULL, end_block, 0);
                nir_emit(cond_block, NIR_JUMP, NULL, NULL, body_block, 0);

                irgen->current_block = body_block;
                nir_emit(body_block, NIR_LOAD_VAR, nir_var_ref(counter), NULL, NULL, 0);
                nir_emit(body_block, NIR_LOAD_ELT, nir_var_ref(listname), NULL, NULL, 0);
                nir_emit(body_block, NIR_STORE_VAR, nir_var_ref(var), NULL, NULL, 0);

                irgen->break_blocks[irgen->loop_depth] = end_block;
                irgen->continue_blocks[irgen->loop_depth] = step_block;
                irgen->loop_depth++;
                if (node->for_each.body->type == AST_BLOCK) {
                    for (size_t i = 0; i < node->for_each.body->block.statement_count; i++) {
                        emit_stmt(irgen, node->for_each.body->block.statements[i]);
                    }
                } else {
                    emit_stmt(irgen, node->for_each.body);
                }
                irgen->loop_depth--;
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, step_block, 0);

                irgen->current_block = step_block;
                nir_emit(step_block, NIR_LOAD_VAR, nir_var_ref(counter), NULL, NULL, 0);
                nir_emit(step_block, NIR_LOAD_CONST, nir_const_int(irgen->program, 1), NULL, NULL, 0);
                nir_emit(step_block, NIR_ADD, NULL, NULL, NULL, 0);
                nir_emit(step_block, NIR_STORE_VAR, nir_var_ref(counter), NULL, NULL, 0);
                nir_emit(step_block, NIR_JUMP, NULL, NULL, cond_block, 0);

                irgen->current_block = end_block;
            }
            break;

        case AST_STOP:
            if (irgen->loop_depth > 0) {
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL,
                         irgen->break_blocks[irgen->loop_depth - 1], 0);
            }
            break;

        case AST_ATTEMPT:
            {
                char save[32];
                snprintf(save, sizeof(save), "err_save_%d", irgen->temp_counter++);
                nir_function_add_local(irgen->current_func, save);

                NIRBasicBlock* pre = irgen->current_block;
                NIRBasicBlock* recover = make_block(irgen, "recover");
                NIRBasicBlock* after = make_block(irgen, "after");

                nir_emit(pre, NIR_ERR_SAVE, nir_var_ref(save), NULL, NULL, 0);
                nir_emit(pre, NIR_ERR_SET, NULL, NULL, recover, 0);

                irgen->current_block = pre;
                ASTNode* tb = node->attempt.try_block;
                if (tb && tb->type == AST_BLOCK) {
                    for (size_t i = 0; i < tb->block.statement_count; i++) {
                        emit_stmt(irgen, tb->block.statements[i]);
                    }
                } else if (tb) {
                    emit_stmt(irgen, tb);
                }
                nir_emit(irgen->current_block, NIR_ERR_RESTORE, nir_var_ref(save), NULL, NULL, 0);
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, after, 0);

                irgen->current_block = recover;
                if (node->attempt.error_var) {
                    nir_emit(recover, NIR_LOAD_ERR, NULL, NULL, NULL, 0);
                    nir_emit(recover, NIR_STORE_VAR, nir_var_ref(node->attempt.error_var), NULL, NULL, 0);
                }
                ASTNode* rb = node->attempt.recover_block;
                if (rb && rb->type == AST_BLOCK) {
                    for (size_t i = 0; i < rb->block.statement_count; i++) {
                        emit_stmt(irgen, rb->block.statements[i]);
                    }
                } else if (rb) {
                    emit_stmt(irgen, rb);
                }
                nir_emit(irgen->current_block, NIR_ERR_RESTORE, nir_var_ref(save), NULL, NULL, 0);
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL, after, 0);

                irgen->current_block = after;
            }
            break;

        case AST_SKIP:
            if (irgen->loop_depth > 0) {
                nir_emit(irgen->current_block, NIR_JUMP, NULL, NULL,
                         irgen->continue_blocks[irgen->loop_depth - 1], 0);
            }
            break;
            
        default:
            break;
    }
}

IRGen* irgen_create(void) {
    IRGen* irgen = calloc(1, sizeof(IRGen));
    irgen->program = nir_program_create();
    return irgen;
}

void irgen_free(IRGen* irgen) {
    nir_program_free(irgen->program);
    free(irgen);
}

NIRProgram* irgen_lower(IRGen* irgen, ASTNode* program) {
    for (size_t i = 0; i < program->program.statement_count; i++) {
        emit_stmt(irgen, program->program.statements[i]);
    }
    return irgen->program;
}
