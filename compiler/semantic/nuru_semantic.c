#define _DEFAULT_SOURCE
#include "nuru_semantic.h"
#include "../ast/nuru_ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Symbol* make_symbol(const char* name, SymbolKind kind) {
    Symbol* sym = (Symbol*)malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->kind = kind;
    sym->mutable = 0;
    sym->arity = 0;
    sym->next = NULL;
    return sym;
}

void semantic_init(SemanticAnalyzer* analyzer) {
    analyzer->global_scope = (Scope*)calloc(1, sizeof(Scope));
    analyzer->global_scope->parent = NULL;
    analyzer->global_scope->symbols = NULL;
    analyzer->global_scope->depth = 0;
    analyzer->current_scope = analyzer->global_scope;
    analyzer->error_count = 0;
    analyzer->errors = NULL;
    analyzer->error_capacity = 0;
    analyzer->in_function = false;
    analyzer->has_main = false;
    analyzer->loop_depth = 0;
}

void semantic_free(SemanticAnalyzer* analyzer) {
    if (analyzer->errors) {
        for (int i = 0; i < analyzer->error_count; i++) {
            free(analyzer->errors[i]);
        }
        free(analyzer->errors);
    }

    Scope* scope = analyzer->global_scope;
    while (scope) {
        Symbol* sym = scope->symbols;
        while (sym) {
            Symbol* next = sym->next;
            free(sym->name);
            free(sym);
            sym = next;
        }
        Scope* parent = scope->parent;
        free(scope);
        scope = parent;
    }
}

Symbol* scope_lookup(Scope* scope, const char* name) {
    while (scope) {
        Symbol* sym = scope->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
            sym = sym->next;
        }
        scope = scope->parent;
    }
    return NULL;
}

Symbol* scope_lookup_current(Scope* scope, const char* name) {
    Symbol* sym = scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }
    return NULL;
}

void scope_define(Scope* scope, Symbol* symbol) {
    symbol->next = scope->symbols;
    scope->symbols = symbol;
}

void semantic_error(SemanticAnalyzer* analyzer, const char* msg, int line, int col) {
    if ((size_t)analyzer->error_count >= analyzer->error_capacity) {
        analyzer->error_capacity = analyzer->error_capacity == 0 ? 8 : analyzer->error_capacity * 2;
        analyzer->errors = (char**)realloc(analyzer->errors, sizeof(char*) * analyzer->error_capacity);
    }
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "Semantic error at line %d, column %d: %s", line, col, msg);
    analyzer->errors[analyzer->error_count++] = strdup(buffer);
}

const char* semantic_get_error(SemanticAnalyzer* analyzer, int index) {
    if (index < 0 || index >= analyzer->error_count) return NULL;
    return analyzer->errors[index];
}

int semantic_get_error_count(SemanticAnalyzer* analyzer) {
    return analyzer->error_count;
}

static void analyze_node(SemanticAnalyzer* analyzer, ASTNode* node);

static void analyze_block(SemanticAnalyzer* analyzer, ASTNode* block) {
    Scope* outer = analyzer->current_scope;
    Scope* scope = (Scope*)calloc(1, sizeof(Scope));
    scope->parent = outer;
    scope->symbols = NULL;
    scope->depth = outer->depth + 1;
    analyzer->current_scope = scope;

    for (size_t i = 0; i < block->block.statement_count; i++) {
        analyze_node(analyzer, block->block.statements[i]);
    }

    analyzer->current_scope = outer;

    Symbol* sym = scope->symbols;
    while (sym) {
        Symbol* next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }
    free(scope);
}

static void analyze_declaration(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (node->type == AST_DECLARATION || node->type == AST_DECLARATION_CHANGEABLE) {
        const char* name = node->declaration.name;
        if (scope_lookup_current(analyzer->current_scope, name)) {
            semantic_error(analyzer, "Variable already declared in this scope", node->line, node->column);
            return;
        }

        Symbol* sym = make_symbol(name, SYMBOL_VARIABLE);
        sym->mutable = (node->type == AST_DECLARATION_CHANGEABLE);
        scope_define(analyzer->current_scope, sym);

        if (node->declaration.value) {
            analyze_node(analyzer, node->declaration.value);
        }
    }
}

static void analyze_assignment(SemanticAnalyzer* analyzer, ASTNode* node) {
    const char* name = node->assignment.name;
    Symbol* sym = scope_lookup(analyzer->current_scope, name);

    if (!sym) {
        semantic_error(analyzer, "Assignment to undeclared variable", node->line, node->column);
        return;
    }

    if (sym->kind != SYMBOL_VARIABLE) {
        semantic_error(analyzer, "Cannot assign to non-variable", node->line, node->column);
        return;
    }

    if (!sym->mutable) {
        semantic_error(analyzer, "Cannot reassign immutable variable", node->line, node->column);
        return;
    }

    analyze_node(analyzer, node->assignment.value);
}

static void analyze_append(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (node->append.value) {
        analyze_node(analyzer, node->append.value);
    }
    Symbol* sym = scope_lookup(analyzer->current_scope, node->append.list_name);
    if (!sym) {
        semantic_error(analyzer, "Append to undeclared list", node->line, node->column);
    }
}

static void analyze_function_decl(SemanticAnalyzer* analyzer, ASTNode* node) {
    const char* name = node->function_decl.name;

    if (!scope_lookup_current(analyzer->current_scope, name)) {
        Symbol* sym = make_symbol(name, SYMBOL_FUNCTION);
        sym->arity = (int)node->function_decl.param_count;
        scope_define(analyzer->current_scope, sym);
    }

    bool was_in_function = analyzer->in_function;
    analyzer->in_function = true;

    Scope* outer = analyzer->current_scope;
    Scope* func_scope = (Scope*)calloc(1, sizeof(Scope));
    func_scope->parent = outer;
    func_scope->symbols = NULL;
    func_scope->depth = outer->depth + 1;
    analyzer->current_scope = func_scope;

    if (node->function_decl.parameters) {
        for (size_t i = 0; i < node->function_decl.param_count; i++) {
            ASTNode* param = node->function_decl.parameters[i];
            if (param->type == AST_IDENTIFIER) {
                Symbol* psym = make_symbol(param->identifier.name, SYMBOL_PARAMETER);
                psym->mutable = 0;
                scope_define(func_scope, psym);
            }
        }
    }

    if (node->function_decl.body) {
        analyze_block(analyzer, node->function_decl.body);
    }

    analyzer->current_scope = outer;
    analyzer->in_function = was_in_function;

    if (strcmp(name, "main") == 0) {
        analyzer->has_main = true;
    }

    Symbol* s = func_scope->symbols;
    while (s) {
        Symbol* next = s->next;
        free(s->name);
        free(s);
        s = next;
    }
    free(func_scope);
}

static void analyze_if(SemanticAnalyzer* analyzer, ASTNode* node) {
    analyze_node(analyzer, node->if_stmt.condition);

    if (node->if_stmt.then_branch) {
        if (node->if_stmt.then_branch->type == AST_BLOCK) {
            analyze_block(analyzer, node->if_stmt.then_branch);
        } else {
            analyze_node(analyzer, node->if_stmt.then_branch);
        }
    }

    if (node->if_stmt.else_branch) {
        if (node->if_stmt.else_branch->type == AST_BLOCK) {
            analyze_block(analyzer, node->if_stmt.else_branch);
        } else {
            analyze_node(analyzer, node->if_stmt.else_branch);
        }
    }
}

static void analyze_loop_body(SemanticAnalyzer* analyzer, ASTNode* body) {
    analyzer->loop_depth++;
    if (body && body->type == AST_BLOCK) {
        analyze_block(analyzer, body);
    } else if (body) {
        analyze_node(analyzer, body);
    }
    analyzer->loop_depth--;
}

static void analyze_repeat(SemanticAnalyzer* analyzer, ASTNode* node) {
    analyze_loop_body(analyzer, node->repeat.body);
}

static void analyze_while(SemanticAnalyzer* analyzer, ASTNode* node) {
    analyze_node(analyzer, node->while_stmt.condition);
    analyze_loop_body(analyzer, node->while_stmt.body);
}

static void analyze_for_each(SemanticAnalyzer* analyzer, ASTNode* node) {
    analyze_node(analyzer, node->for_each.iterable);

    Symbol* sym = make_symbol(node->for_each.variable, SYMBOL_VARIABLE);
    sym->mutable = 1;
    scope_define(analyzer->current_scope, sym);

    analyze_loop_body(analyzer, node->for_each.body);
}

static void analyze_show(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (node->show.expression) {
        analyze_node(analyzer, node->show.expression);
    }
}

static void analyze_attempt(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (node->attempt.try_block) {
        analyze_block(analyzer, node->attempt.try_block);
    }

    if (node->attempt.error_var) {
        Symbol* sym = make_symbol(node->attempt.error_var, SYMBOL_VARIABLE);
        sym->mutable = 1;
        scope_define(analyzer->current_scope, sym);
    }

    if (node->attempt.recover_block) {
        analyze_block(analyzer, node->attempt.recover_block);
    }
}

static void analyze_return(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (!analyzer->in_function) {
        semantic_error(analyzer, "Return statement outside function", node->line, node->column);
    }

    if (node->return_stmt.expression) {
        analyze_node(analyzer, node->return_stmt.expression);
    }
}

static void analyze_function_call(SemanticAnalyzer* analyzer, ASTNode* node) {
    const char* name = node->function_call.name;

    static const struct { const char* name; size_t arity; } builtins[] = {
        { "ask", 0 }, { "len", 1 }, { "abs", 1 },
        { "min", 2 }, { "max", 2 }, { "to_number", 1 },
        { "concat", 2 }, { "ord", 1 }, { "chr", 1 },
        { "pow", 2 }, { "contains", 2 },
        { "substring", 3 }, { "upper", 1 }, { "lower", 1 }, { "index", 2 },
        { "to_string", 1 },
        { "read_file", 1 }, { "write_file", 2 },
    };
    for (size_t bi = 0; bi < sizeof(builtins)/sizeof(builtins[0]); bi++) {
        if (strcmp(name, builtins[bi].name) == 0) {
            if (node->function_call.arg_count != builtins[bi].arity) {
                semantic_error(analyzer, "argument count mismatch for builtin", node->line, node->column);
            }
            return;
        }
    }

    Symbol* sym = scope_lookup(analyzer->current_scope, name);

    if (!sym) {
        semantic_error(analyzer, "Call to undeclared function", node->line, node->column);
        return;
    }

    if (sym->kind != SYMBOL_FUNCTION) {
        semantic_error(analyzer, "Cannot call non-function", node->line, node->column);
        return;
    }

    size_t arg_count = node->function_call.arg_count;
    if (sym->arity > 0 && (int)arg_count != sym->arity) {
        semantic_error(analyzer, "Argument count mismatch", node->line, node->column);
    }

    if (node->function_call.arguments) {
        for (size_t i = 0; i < arg_count; i++) {
            analyze_node(analyzer, node->function_call.arguments[i]);
        }
    }
}

static void analyze_identifier(SemanticAnalyzer* analyzer, ASTNode* node) {
    const char* name = node->identifier.name;
    Symbol* sym = scope_lookup(analyzer->current_scope, name);

    if (!sym) {
        semantic_error(analyzer, "Use of undeclared identifier", node->line, node->column);
    }
}

static void analyze_binary_op(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (node->binary_op.left) {
        analyze_node(analyzer, node->binary_op.left);
    }
    if (node->binary_op.right) {
        analyze_node(analyzer, node->binary_op.right);
    }
}

static void analyze_unary_op(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (node->unary_op.operand) {
        analyze_node(analyzer, node->unary_op.operand);
    }
}

static void analyze_node(SemanticAnalyzer* analyzer, ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            for (size_t i = 0; i < node->program.statement_count; i++) {
                analyze_node(analyzer, node->program.statements[i]);
            }
            break;

        case AST_DECLARATION:
        case AST_DECLARATION_CHANGEABLE:
            analyze_declaration(analyzer, node);
            break;

        case AST_ASSIGNMENT:
            analyze_assignment(analyzer, node);
            break;

        case AST_APPEND:
            analyze_append(analyzer, node);
            break;

        case AST_FUNCTION_DECL:
            analyze_function_decl(analyzer, node);
            break;

        case AST_FUNCTION_CALL:
            analyze_function_call(analyzer, node);
            break;

        case AST_BLOCK:
            analyze_block(analyzer, node);
            break;

        case AST_IF:
            analyze_if(analyzer, node);
            break;

        case AST_REPEAT:
            analyze_repeat(analyzer, node);
            break;

        case AST_WHILE:
            analyze_while(analyzer, node);
            break;

        case AST_FOR_EACH:
            analyze_for_each(analyzer, node);
            break;

        case AST_SHOW:
            analyze_show(analyzer, node);
            break;

        case AST_ATTEMPT:
            analyze_attempt(analyzer, node);
            break;

        case AST_RETURN:
            analyze_return(analyzer, node);
            break;

        case AST_STOP:
        case AST_SKIP:
            if (analyzer->loop_depth == 0) {
                semantic_error(analyzer,
                    node->type == AST_STOP ? "'stop' used outside a loop" : "'skip' used outside a loop",
                    node->line, node->column);
            }
            break;

        case AST_IDENTIFIER:
            analyze_identifier(analyzer, node);
            break;

        case AST_INTEGER_LITERAL:
        case AST_NUMBER_LITERAL:
        case AST_STRING_LITERAL:
        case AST_BOOLEAN_LITERAL:
        case AST_EMPTY_LITERAL:
            break;

        case AST_INTERP_STRING:
            if (node->interp_string.parts) {
                for (size_t i = 0; i < node->interp_string.part_count; i++) {
                    analyze_node(analyzer, node->interp_string.parts[i]);
                }
            }
            break;

        case AST_LIST_LITERAL:
            if (node->list_literal.items) {
                for (size_t i = 0; i < node->list_literal.count; i++) {
                    analyze_node(analyzer, node->list_literal.items[i]);
                }
            }
            break;

        case AST_MAP_LITERAL:
            if (node->map_literal.entries) {
                for (size_t i = 0; i < node->map_literal.count; i++) {
                    analyze_node(analyzer, node->map_literal.entries[i]);
                }
            }
            break;

        case AST_MAP_ENTRY:
            analyze_node(analyzer, node->map_entry.value);
            break;

        case AST_BINARY_OP:
            analyze_binary_op(analyzer, node);
            break;

        case AST_UNARY_OP:
            analyze_unary_op(analyzer, node);
            break;

        case AST_MEMBER_ACCESS:
            analyze_node(analyzer, node->member_access.object);
            break;

        case AST_INDEX_ACCESS:
            analyze_node(analyzer, node->index_access.object);
            analyze_node(analyzer, node->index_access.index);
            break;

        default:
            break;
    }
}

int semantic_analyze(SemanticAnalyzer* analyzer, ASTNode* program) {
    if (!program) return 1;

    if (program->type == AST_PROGRAM) {
        for (size_t i = 0; i < program->program.statement_count; i++) {
            ASTNode* s = program->program.statements[i];
            if (s->type == AST_FUNCTION_DECL && !scope_lookup_current(analyzer->current_scope, s->function_decl.name)) {
                Symbol* sym = make_symbol(s->function_decl.name, SYMBOL_FUNCTION);
                sym->arity = (int)s->function_decl.param_count;
                scope_define(analyzer->current_scope, sym);
            }
        }
    }

    analyze_node(analyzer, program);

    if (analyzer->error_count > 0) {
        return 1;
    }

    return 0;
}
