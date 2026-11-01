#include "nuru_ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return ptr;
}

static char* strdup_(const char* s) {
    size_t len = strlen(s) + 1;
    char* result = (char*)safe_malloc(len);
    memcpy(result, s, len);
    return result;
}

ASTNode* ast_create_node(ASTNodeType type, int line, int column) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

ASTNode* ast_create_program(ASTNode** statements, size_t count) {
    ASTNode* node = ast_create_node(AST_PROGRAM, 1, 1);
    node->program.statements = statements;
    node->program.statement_count = count;
    return node;
}

ASTNode* ast_create_declaration(const char* name, ASTNode* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_DECLARATION, line, column);
    node->declaration.name = strdup_(name);
    node->declaration.value = value;
    return node;
}

ASTNode* ast_create_declaration_changeable(const char* name, ASTNode* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_DECLARATION_CHANGEABLE, line, column);
    node->declaration_changeable.name = strdup_(name);
    node->declaration_changeable.value = value;
    return node;
}

ASTNode* ast_create_assignment(const char* name, ASTNode* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_ASSIGNMENT, line, column);
    node->assignment.name = strdup_(name);
    node->assignment.value = value;
    return node;
}

ASTNode* ast_create_append(ASTNode* value, const char* list_name, int line, int column) {
    ASTNode* node = ast_create_node(AST_APPEND, line, column);
    node->append.value = value;
    node->append.list_name = strdup_(list_name);
    return node;
}

ASTNode* ast_create_function_decl(const char* name, ASTNode** params, size_t param_count, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_FUNCTION_DECL, line, column);
    node->function_decl.name = strdup_(name);
    node->function_decl.parameters = params;
    node->function_decl.param_count = param_count;
    node->function_decl.body = body;
    return node;
}

ASTNode* ast_create_function_call(const char* name, ASTNode** args, size_t arg_count, int line, int column) {
    ASTNode* node = ast_create_node(AST_FUNCTION_CALL, line, column);
    node->function_call.name = strdup_(name);
    node->function_call.arguments = args;
    node->function_call.arg_count = arg_count;
    return node;
}

ASTNode* ast_create_block(ASTNode** statements, size_t count, int line, int column) {
    ASTNode* node = ast_create_node(AST_BLOCK, line, column);
    node->block.statements = statements;
    node->block.statement_count = count;
    return node;
}

ASTNode* ast_create_if(ASTNode* cond, ASTNode* then_br, ASTNode* else_br, int line, int column) {
    ASTNode* node = ast_create_node(AST_IF, line, column);
    node->if_stmt.condition = cond;
    node->if_stmt.then_branch = then_br;
    node->if_stmt.else_branch = else_br;
    return node;
}

ASTNode* ast_create_repeat(int64_t count, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_REPEAT, line, column);
    node->repeat.count = count;
    node->repeat.body = body;
    return node;
}

ASTNode* ast_create_while(ASTNode* cond, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_WHILE, line, column);
    node->while_stmt.condition = cond;
    node->while_stmt.body = body;
    return node;
}

ASTNode* ast_create_for_each(const char* var, ASTNode* iter, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_FOR_EACH, line, column);
    node->for_each.variable = strdup_(var);
    node->for_each.iterable = iter;
    node->for_each.body = body;
    return node;
}

ASTNode* ast_create_show(ASTNode* expr, int line, int column) {
    ASTNode* node = ast_create_node(AST_SHOW, line, column);
    node->show.expression = expr;
    return node;
}

ASTNode* ast_create_attempt(ASTNode* try_b, const char* err_var, ASTNode* rec_b, int line, int column) {
    ASTNode* node = ast_create_node(AST_ATTEMPT, line, column);
    node->attempt.try_block = try_b;
    node->attempt.error_var = strdup_(err_var);
    node->attempt.recover_block = rec_b;
    return node;
}

ASTNode* ast_create_return(ASTNode* expr, int line, int column) {
    ASTNode* node = ast_create_node(AST_RETURN, line, column);
    node->return_stmt.expression = expr;
    return node;
}

ASTNode* ast_create_identifier(const char* name, int line, int column) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER, line, column);
    node->identifier.name = strdup_(name);
    return node;
}

ASTNode* ast_create_integer(int64_t value, int line, int column) {
    ASTNode* node = ast_create_node(AST_INTEGER_LITERAL, line, column);
    node->integer_literal.value = value;
    return node;
}

ASTNode* ast_create_number(double value, int line, int column) {
    ASTNode* node = ast_create_node(AST_NUMBER_LITERAL, line, column);
    node->number_literal.value = value;
    return node;
}

ASTNode* ast_create_string(const char* value, size_t len, int line, int column) {
    ASTNode* node = ast_create_node(AST_STRING_LITERAL, line, column);
    node->string_literal.value = strdup_(value);
    node->string_literal.length = len;
    return node;
}

ASTNode* ast_create_interp_string(ASTNode** parts, size_t count, int line, int column) {
    ASTNode* node = ast_create_node(AST_INTERP_STRING, line, column);
    node->interp_string.parts = parts;
    node->interp_string.part_count = count;
    return node;
}

ASTNode* ast_create_boolean(int value, int line, int column) {
    ASTNode* node = ast_create_node(AST_BOOLEAN_LITERAL, line, column);
    node->boolean_literal.value = value;
    return node;
}

ASTNode* ast_create_empty(int line, int column) {
    return ast_create_node(AST_EMPTY_LITERAL, line, column);
}

ASTNode* ast_create_list(ASTNode** items, size_t count, int line, int column) {
    ASTNode* node = ast_create_node(AST_LIST_LITERAL, line, column);
    node->list_literal.items = items;
    node->list_literal.count = count;
    return node;
}

ASTNode* ast_create_map(ASTNode** entries, size_t count, int line, int column) {
    ASTNode* node = ast_create_node(AST_MAP_LITERAL, line, column);
    node->map_literal.entries = entries;
    node->map_literal.count = count;
    return node;
}

ASTNode* ast_create_map_entry(const char* key, ASTNode* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_MAP_ENTRY, line, column);
    node->map_entry.key = strdup_(key);
    node->map_entry.value = value;
    return node;
}

ASTNode* ast_create_binary_op(const char* op, ASTNode* left, ASTNode* right, int line, int column) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, line, column);
    node->binary_op.operator = strdup_(op);
    node->binary_op.left = left;
    node->binary_op.right = right;
    return node;
}

ASTNode* ast_create_unary_op(const char* op, ASTNode* operand, int line, int column) {
    ASTNode* node = ast_create_node(AST_UNARY_OP, line, column);
    node->unary_op.operator = strdup_(op);
    node->unary_op.operand = operand;
    return node;
}

ASTNode* ast_create_member_access(ASTNode* obj, const char* prop, int line, int column) {
    ASTNode* node = ast_create_node(AST_MEMBER_ACCESS, line, column);
    node->member_access.object = obj;
    node->member_access.property = strdup_(prop);
    return node;
}

ASTNode* ast_create_index_access(ASTNode* obj, ASTNode* idx, int line, int column) {
    ASTNode* node = ast_create_node(AST_INDEX_ACCESS, line, column);
    node->index_access.object = obj;
    node->index_access.index = idx;
    return node;
}

void ast_free(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            for (size_t i = 0; i < node->program.statement_count; i++) {
                ast_free(node->program.statements[i]);
            }
            free(node->program.statements);
            break;
        case AST_DECLARATION:
            free(node->declaration.name);
            ast_free(node->declaration.value);
            break;
        case AST_DECLARATION_CHANGEABLE:
            free(node->declaration_changeable.name);
            ast_free(node->declaration_changeable.value);
            break;
        case AST_ASSIGNMENT:
            free(node->assignment.name);
            ast_free(node->assignment.value);
            break;
        case AST_FUNCTION_DECL:
            free(node->function_decl.name);
            for (size_t i = 0; i < node->function_decl.param_count; i++) {
                ast_free(node->function_decl.parameters[i]);
            }
            free(node->function_decl.parameters);
            ast_free(node->function_decl.body);
            break;
        case AST_FUNCTION_CALL:
            free(node->function_call.name);
            for (size_t i = 0; i < node->function_call.arg_count; i++) {
                ast_free(node->function_call.arguments[i]);
            }
            free(node->function_call.arguments);
            break;
        case AST_BLOCK:
            for (size_t i = 0; i < node->block.statement_count; i++) {
                ast_free(node->block.statements[i]);
            }
            free(node->block.statements);
            break;
        case AST_IF:
            ast_free(node->if_stmt.condition);
            ast_free(node->if_stmt.then_branch);
            ast_free(node->if_stmt.else_branch);
            break;
        case AST_REPEAT:
            ast_free(node->repeat.body);
            break;
        case AST_WHILE:
            ast_free(node->while_stmt.condition);
            ast_free(node->while_stmt.body);
            break;
        case AST_FOR_EACH:
            free(node->for_each.variable);
            ast_free(node->for_each.iterable);
            ast_free(node->for_each.body);
            break;
        case AST_SHOW:
            ast_free(node->show.expression);
            break;
        case AST_ATTEMPT:
            ast_free(node->attempt.try_block);
            free(node->attempt.error_var);
            ast_free(node->attempt.recover_block);
            break;
        case AST_STOP:
        case AST_SKIP:
            break;
        case AST_RETURN:
            ast_free(node->return_stmt.expression);
            break;
        case AST_IDENTIFIER:
            free(node->identifier.name);
            break;
        case AST_INTEGER_LITERAL:
        case AST_NUMBER_LITERAL:
        case AST_BOOLEAN_LITERAL:
        case AST_EMPTY_LITERAL:
            break;
        case AST_STRING_LITERAL:
            free(node->string_literal.value);
            break;
        case AST_INTERP_STRING:
            for (size_t i = 0; i < node->interp_string.part_count; i++) {
                ast_free(node->interp_string.parts[i]);
            }
            free(node->interp_string.parts);
            break;
        case AST_LIST_LITERAL:
            for (size_t i = 0; i < node->list_literal.count; i++) {
                ast_free(node->list_literal.items[i]);
            }
            free(node->list_literal.items);
            break;
        case AST_MAP_LITERAL:
            for (size_t i = 0; i < node->map_literal.count; i++) {
                ast_free(node->map_literal.entries[i]);
            }
            free(node->map_literal.entries);
            break;
        case AST_MAP_ENTRY:
            free(node->map_entry.key);
            ast_free(node->map_entry.value);
            break;
        case AST_BINARY_OP:
            free(node->binary_op.operator);
            ast_free(node->binary_op.left);
            ast_free(node->binary_op.right);
            break;
        case AST_UNARY_OP:
            free(node->unary_op.operator);
            ast_free(node->unary_op.operand);
            break;
        case AST_MEMBER_ACCESS:
            ast_free(node->member_access.object);
            free(node->member_access.property);
            break;
        case AST_INDEX_ACCESS:
            ast_free(node->index_access.object);
            ast_free(node->index_access.index);
            break;
        default:
            break;
    }

    free(node);
}

const char* ast_node_type_name(ASTNodeType type) {
    switch (type) {
        case AST_PROGRAM: return "PROGRAM";
        case AST_DECLARATION: return "DECLARATION";
        case AST_DECLARATION_CHANGEABLE: return "DECLARATION_CHANGEABLE";
        case AST_ASSIGNMENT: return "ASSIGNMENT";
        case AST_FUNCTION_DECL: return "FUNCTION_DECL";
        case AST_FUNCTION_CALL: return "FUNCTION_CALL";
        case AST_BLOCK: return "BLOCK";
        case AST_IF: return "IF";
        case AST_REPEAT: return "REPEAT";
        case AST_WHILE: return "WHILE";
        case AST_FOR_EACH: return "FOR_EACH";
        case AST_SHOW: return "SHOW";
        case AST_ATTEMPT: return "ATTEMPT";
        case AST_STOP: return "STOP";
        case AST_SKIP: return "SKIP";
        case AST_RETURN: return "RETURN";
        case AST_IDENTIFIER: return "IDENTIFIER";
        case AST_INTEGER_LITERAL: return "INTEGER";
        case AST_NUMBER_LITERAL: return "NUMBER";
        case AST_STRING_LITERAL: return "STRING";
        case AST_BOOLEAN_LITERAL: return "BOOLEAN";
        case AST_EMPTY_LITERAL: return "EMPTY";
        case AST_LIST_LITERAL: return "LIST";
        case AST_MAP_LITERAL: return "MAP";
        case AST_MAP_ENTRY: return "MAP_ENTRY";
        case AST_BINARY_OP: return "BINARY_OP";
        case AST_UNARY_OP: return "UNARY_OP";
        case AST_MEMBER_ACCESS: return "MEMBER_ACCESS";
        case AST_INDEX_ACCESS: return "INDEX_ACCESS";
        default: return "UNKNOWN";
    }
}

void ast_print(ASTNode* node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s", ast_node_type_name(node->type));

    switch (node->type) {
        case AST_DECLARATION:
        case AST_DECLARATION_CHANGEABLE:
            printf(" (%s)", node->declaration.name);
            break;
        case AST_ASSIGNMENT:
            printf(" (%s)", node->assignment.name);
            break;
        case AST_FUNCTION_DECL:
            printf(" (%s)", node->function_decl.name);
            break;
        case AST_FUNCTION_CALL:
            printf(" (%s)", node->function_call.name);
            break;
        case AST_IDENTIFIER:
            printf(" (%s)", node->identifier.name);
            break;
        case AST_INTEGER_LITERAL:
            printf(" (%" PRId64 ")", (int64_t)node->integer_literal.value);
            break;
        case AST_NUMBER_LITERAL:
            printf(" (%f)", node->number_literal.value);
            break;
        case AST_STRING_LITERAL:
            printf(" (\"%.*s\")", (int)node->string_literal.length, node->string_literal.value);
            break;
        case AST_INTERP_STRING:
            printf(" (interp)\n");
            for (size_t i = 0; i < node->interp_string.part_count; i++) {
                for (int k = 0; k < indent + 2; k++) printf("  ");
                ast_print(node->interp_string.parts[i], indent + 2);
            }
            break;
        case AST_BOOLEAN_LITERAL:
            printf(" (%s)", node->boolean_literal.value ? "true" : "false");
            break;
        case AST_BINARY_OP:
            printf(" (%s)", node->binary_op.operator);
            break;
        case AST_UNARY_OP:
            printf(" (%s)", node->unary_op.operator);
            break;
        case AST_FOR_EACH:
            printf(" (%s)", node->for_each.variable);
            break;
        case AST_REPEAT:
            printf(" (count=%" PRId64 ")", (int64_t)node->repeat.count);
            break;
        case AST_ATTEMPT:
            printf(" (error_var=%s)", node->attempt.error_var);
            break;
        default:
            break;
    }

    printf("\n");
}