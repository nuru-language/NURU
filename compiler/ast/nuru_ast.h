#ifndef NURU_AST_H
#define NURU_AST_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    AST_PROGRAM,
    AST_DECLARATION,
    AST_DECLARATION_CHANGEABLE,
    AST_ASSIGNMENT,
    AST_APPEND,
    AST_FUNCTION_DECL,
    AST_FUNCTION_CALL,
    AST_BLOCK,
    AST_IF,
    AST_REPEAT,
    AST_WHILE,
    AST_FOR_EACH,
    AST_SHOW,
    AST_ATTEMPT,
    AST_STOP,
    AST_SKIP,
    AST_RETURN,
    AST_IDENTIFIER,
    AST_INTEGER_LITERAL,
    AST_NUMBER_LITERAL,
    AST_STRING_LITERAL,
    AST_INTERP_STRING,
    AST_BOOLEAN_LITERAL,
    AST_EMPTY_LITERAL,
    AST_LIST_LITERAL,
    AST_MAP_LITERAL,
    AST_MAP_ENTRY,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_MEMBER_ACCESS,
    AST_INDEX_ACCESS,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    int line;
    int column;
    union {
        struct {
            struct ASTNode** statements;
            size_t statement_count;
        } program;
        struct {
            char* name;
            struct ASTNode* value;
        } declaration;
        struct {
            char* name;
            struct ASTNode* value;
        } declaration_changeable;
        struct {
            char* name;
            struct ASTNode* value;
        } assignment;
        struct {
            struct ASTNode* value;
            char* list_name;
        } append;
        struct {
            char* name;
            struct ASTNode** parameters;
            size_t param_count;
            struct ASTNode* body;
        } function_decl;
        struct {
            char* name;
            struct ASTNode** arguments;
            size_t arg_count;
        } function_call;
        struct {
            struct ASTNode** statements;
            size_t statement_count;
        } block;
        struct {
            struct ASTNode* condition;
            struct ASTNode* then_branch;
            struct ASTNode* else_branch;
        } if_stmt;
        struct {
            int64_t count;
            struct ASTNode* body;
        } repeat;
        struct {
            struct ASTNode* condition;
            struct ASTNode* body;
        } while_stmt;
        struct {
            char* variable;
            struct ASTNode* iterable;
            struct ASTNode* body;
        } for_each;
        struct {
            struct ASTNode* expression;
        } show;
        struct {
            struct ASTNode* try_block;
            char* error_var;
            struct ASTNode* recover_block;
        } attempt;
        struct {
            struct ASTNode* expression;
        } return_stmt;
        struct {
            char* name;
        } identifier;
        struct {
            int64_t value;
        } integer_literal;
        struct {
            double value;
        } number_literal;
        struct {
            char* value;
            size_t length;
        } string_literal;
        struct {
            struct ASTNode** parts;
            size_t part_count;
        } interp_string;
        struct {
            int value;
        } boolean_literal;
        struct {
            struct ASTNode** items;
            size_t count;
        } list_literal;
        struct {
            struct ASTNode** entries;
            size_t count;
        } map_literal;
        struct {
            char* key;
            struct ASTNode* value;
        } map_entry;
        struct {
            char* operator;
            struct ASTNode* left;
            struct ASTNode* right;
        } binary_op;
        struct {
            char* operator;
            struct ASTNode* operand;
        } unary_op;
        struct {
            struct ASTNode* object;
            char* property;
        } member_access;
        struct {
            struct ASTNode* object;
            struct ASTNode* index;
        } index_access;
    };
} ASTNode;

ASTNode* ast_create_node(ASTNodeType type, int line, int column);
ASTNode* ast_create_program(ASTNode** statements, size_t count);
ASTNode* ast_create_declaration(const char* name, ASTNode* value, int line, int column);
ASTNode* ast_create_declaration_changeable(const char* name, ASTNode* value, int line, int column);
ASTNode* ast_create_assignment(const char* name, ASTNode* value, int line, int column);
ASTNode* ast_create_append(ASTNode* value, const char* list_name, int line, int column);
ASTNode* ast_create_function_decl(const char* name, ASTNode** params, size_t param_count, ASTNode* body, int line, int column);
ASTNode* ast_create_function_call(const char* name, ASTNode** args, size_t arg_count, int line, int column);
ASTNode* ast_create_block(ASTNode** statements, size_t count, int line, int column);
ASTNode* ast_create_if(ASTNode* cond, ASTNode* then_br, ASTNode* else_br, int line, int column);
ASTNode* ast_create_repeat(int64_t count, ASTNode* body, int line, int column);
ASTNode* ast_create_while(ASTNode* cond, ASTNode* body, int line, int column);
ASTNode* ast_create_for_each(const char* var, ASTNode* iter, ASTNode* body, int line, int column);
ASTNode* ast_create_show(ASTNode* expr, int line, int column);
ASTNode* ast_create_attempt(ASTNode* try_b, const char* err_var, ASTNode* rec_b, int line, int column);
ASTNode* ast_create_return(ASTNode* expr, int line, int column);
ASTNode* ast_create_identifier(const char* name, int line, int column);
ASTNode* ast_create_integer(int64_t value, int line, int column);
ASTNode* ast_create_number(double value, int line, int column);
ASTNode* ast_create_string(const char* value, size_t len, int line, int column);
ASTNode* ast_create_interp_string(ASTNode** parts, size_t count, int line, int column);
ASTNode* ast_create_boolean(int value, int line, int column);
ASTNode* ast_create_empty(int line, int column);
ASTNode* ast_create_list(ASTNode** items, size_t count, int line, int column);
ASTNode* ast_create_map(ASTNode** entries, size_t count, int line, int column);
ASTNode* ast_create_map_entry(const char* key, ASTNode* value, int line, int column);
ASTNode* ast_create_binary_op(const char* op, ASTNode* left, ASTNode* right, int line, int column);
ASTNode* ast_create_unary_op(const char* op, ASTNode* operand, int line, int column);
ASTNode* ast_create_member_access(ASTNode* obj, const char* prop, int line, int column);
ASTNode* ast_create_index_access(ASTNode* obj, ASTNode* idx, int line, int column);

void ast_free(ASTNode* node);
const char* ast_node_type_name(ASTNodeType type);
void ast_print(ASTNode* node, int indent);

#endif /* NURU_AST_H */