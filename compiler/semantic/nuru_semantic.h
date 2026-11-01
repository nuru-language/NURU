#ifndef NURU_SEMANTIC_H
#define NURU_SEMANTIC_H

#include "../ast/nuru_ast.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER
} SymbolKind;

typedef struct Symbol {
    char* name;
    SymbolKind kind;
    int mutable;
    int arity;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    struct Scope* parent;
    Symbol* symbols;
    int depth;
} Scope;

typedef struct {
    Scope* global_scope;
    Scope* current_scope;
    int error_count;
    char** errors;
    size_t error_capacity;
    bool in_function;
    bool has_main;
    int loop_depth;
} SemanticAnalyzer;

void semantic_init(SemanticAnalyzer* analyzer);
void semantic_free(SemanticAnalyzer* analyzer);

int semantic_analyze(SemanticAnalyzer* analyzer, ASTNode* program);
int semantic_check_function(SemanticAnalyzer* analyzer, ASTNode* func);

Symbol* scope_lookup(Scope* scope, const char* name);
Symbol* scope_lookup_current(Scope* scope, const char* name);
void scope_define(Scope* scope, Symbol* symbol);

void semantic_error(SemanticAnalyzer* analyzer, const char* msg, int line, int col);
const char* semantic_get_error(SemanticAnalyzer* analyzer, int index);
int semantic_get_error_count(SemanticAnalyzer* analyzer);

#endif /* NURU_SEMANTIC_H */
