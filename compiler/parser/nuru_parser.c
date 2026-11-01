#include "nuru_parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void error_at(Parser* parser, const char* message, int line, int column) {
    if (parser->had_error) return;
    parser->had_error = 1;
    parser->error_message = message;
    parser->error_line = line;
    parser->error_column = column;
}

static void error_at_current(Parser* parser, const char* message) {
    error_at(parser, message, parser->current.line, parser->current.column);
}

static char* strndup_(const char* start, size_t length) {
    char* result = (char*)malloc(length + 1);
    memcpy(result, start, length);
    result[length] = '\0';
    return result;
}

static void advance(Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = lexer_next_token(parser->lexer);

        if (parser->current.type == TOKEN_ERROR) {
            error_at_current(parser, parser->current.start);
            return;
        }

        if (parser->current.type == TOKEN_NEWLINE ||
            parser->current.type == TOKEN_HASH ||
            parser->current.type == TOKEN_MULTILINE_COMMENT_END) {
            continue;
        }

        break;
    }
}

static int match(Parser* parser, TokenType type) {
    if (parser->current.type != type) return 0;
    advance(parser);
    return 1;
}

static int check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static ASTNode** grow_array(ASTNode** arr, size_t* cap, size_t* count, ASTNode* item) {
    if (*count >= *cap) {
        *cap = *cap == 0 ? 8 : (*cap) * 2;
        arr = (ASTNode**)realloc(arr, sizeof(ASTNode*) * (*cap));
    }
    arr[(*count)++] = item;
    return arr;
}

static ASTNode** parse_statements(Parser* parser, int stop_token, size_t* out_count);

static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_or_expr(Parser* parser);
static ASTNode* parse_and_expr(Parser* parser);
static ASTNode* parse_equality_expr(Parser* parser);
static ASTNode* parse_comparison_expr(Parser* parser);
static ASTNode* parse_additive_expr(Parser* parser);
static ASTNode* parse_multiplicative_expr(Parser* parser);
static ASTNode* parse_unary_expr(Parser* parser);
static ASTNode* parse_primary_expr(Parser* parser);
static ASTNode* parse_postfix(Parser* parser);
static ASTNode** parse_expression_list(Parser* parser, size_t* out_count);
static ASTNode** parse_map_entries(Parser* parser, size_t* out_count);

static ASTNode* parse_declaration(Parser* parser) {
    Token start_token = parser->current;

    if (match(parser, TOKEN_MAKE)) {
        if (!match(parser, TOKEN_IDENTIFIER)) {
            error_at_current(parser, "Expected variable name after 'make'");
            return NULL;
        }

        char* name = strndup_(parser->previous.start, parser->previous.length);

        if (match(parser, TOKEN_BE)) {
            ASTNode* value = parse_expression(parser);
            if (parser->had_error) {
                free(name);
                return NULL;
            }
            return ast_create_declaration(name, value, start_token.line, start_token.column);
        }

        if (match(parser, TOKEN_CHANGEABLE)) {
            ASTNode* value = parse_expression(parser);
            if (parser->had_error) {
                free(name);
                return NULL;
            }
            return ast_create_declaration_changeable(name, value, start_token.line, start_token.column);
        }

        error_at_current(parser, "Expected 'be' or 'changeable' after variable name");
        free(name);
        return NULL;
    }

    if (match(parser, TOKEN_CREATE)) {
        if (!match(parser, TOKEN_IDENTIFIER)) {
            error_at_current(parser, "Expected function name after 'create'");
            return NULL;
        }

        char* name = strndup_(parser->previous.start, parser->previous.length);

        size_t param_cap = 0, param_count = 0;
        ASTNode** params = NULL;

        if (match(parser, TOKEN_USING)) {
            if (!check(parser, TOKEN_LBRACE)) {
                do {
                    if (!match(parser, TOKEN_IDENTIFIER)) {
                        error_at_current(parser, "Expected parameter name");
                        for (size_t i = 0; i < param_count; i++) ast_free(params[i]);
                        free(params);
                        free(name);
                        return NULL;
                    }
                    char* pname = strndup_(parser->previous.start, parser->previous.length);
                    ASTNode* param = ast_create_identifier(pname, parser->previous.line, parser->previous.column);
                    params = grow_array(params, &param_cap, &param_count, param);
                } while (match(parser, TOKEN_COMMA));
            }
        } else if (!check(parser, TOKEN_LBRACE)) {
            error_at_current(parser, "Expected 'using' or '{' after function name");
            free(name);
            free(params);
            return NULL;
        }

        if (!check(parser, TOKEN_LBRACE)) {
            error_at_current(parser, "Expected '{' before function body");
            for (size_t i = 0; i < param_count; i++) ast_free(params[i]);
            free(params);
            free(name);
            return NULL;
        }
        
        advance(parser); // consume opening brace

        size_t body_stmt_count = 0;
        ASTNode** body_stmts = parse_statements(parser, TOKEN_RBRACE, &body_stmt_count);
        if (!body_stmts && parser->had_error) {
            for (size_t i = 0; i < param_count; i++) ast_free(params[i]);
            free(params);
            free(name);
            return NULL;
        }
        advance(parser); // consume closing brace

        ASTNode* body = ast_create_block(body_stmts, body_stmt_count, start_token.line, start_token.column);
        return ast_create_function_decl(name, params, param_count, body, start_token.line, start_token.column);
    }

    if (match(parser, TOKEN_CHANGE)) {
        if (!match(parser, TOKEN_IDENTIFIER)) {
            error_at_current(parser, "Expected variable name after 'change'");
            return NULL;
        }

        char* name = strndup_(parser->previous.start, parser->previous.length);

        if (!match(parser, TOKEN_TO)) {
            error_at_current(parser, "Expected 'to' after variable name");
            free(name);
            return NULL;
        }

        ASTNode* value = parse_expression(parser);
        if (parser->had_error) {
            free(name);
            return NULL;
        }

        return ast_create_assignment(name, value, parser->previous.line, parser->previous.column);
    }

    return NULL;
}

static ASTNode* parse_braced_block(Parser* parser) {
    if (!match(parser, TOKEN_LBRACE)) {
        error_at_current(parser, "Expected '{' to start block");
        return NULL;
    }
    size_t count = 0;
    ASTNode** stmts = parse_statements(parser, TOKEN_RBRACE, &count);
    if (!stmts) return NULL;
    advance(parser);
    return ast_create_block(stmts, count, parser->previous.line, parser->previous.column);
}

static ASTNode* parse_if_statement(Parser* parser) {
    Token start = parser->previous;

    ASTNode* condition = parse_expression(parser);
    if (!condition) return NULL;

    ASTNode* then_branch = parse_braced_block(parser);
    if (!then_branch) return NULL;

    ASTNode* else_branch = NULL;
    if (match(parser, TOKEN_OTHERWISE)) {
        else_branch = parse_braced_block(parser);
        if (!else_branch) return NULL;
    }

    return ast_create_if(condition, then_branch, else_branch, start.line, start.column);
}

static ASTNode* parse_repeat_statement(Parser* parser) {
    Token start = parser->previous;

    if (!match(parser, TOKEN_INTEGER)) {
        error_at_current(parser, "Expected integer count after 'repeat'");
        return NULL;
    }

    int64_t count = (int64_t)parser->previous.integer_value;

    if (!match(parser, TOKEN_TIMES)) {
        error_at_current(parser, "Expected 'times' after repeat count");
        return NULL;
    }

    ASTNode* body = parse_braced_block(parser);
    if (!body) return NULL;

    return ast_create_repeat(count, body, start.line, start.column);
}

static ASTNode* parse_while_statement(Parser* parser) {
    Token start = parser->previous;

    ASTNode* condition = parse_expression(parser);
    if (!condition) return NULL;

    ASTNode* body = parse_braced_block(parser);
    if (!body) return NULL;

    return ast_create_while(condition, body, start.line, start.column);
}

static ASTNode* parse_for_each_statement(Parser* parser) {
    Token start = parser->previous;

    if (!match(parser, TOKEN_EACH)) {
        error_at_current(parser, "Expected 'each' after 'for'");
        return NULL;
    }

    if (!match(parser, TOKEN_IDENTIFIER)) {
        error_at_current(parser, "Expected loop variable name");
        return NULL;
    }

    char* var_name = strndup_(parser->previous.start, parser->previous.length);

    if (!match(parser, TOKEN_IN)) {
        error_at_current(parser, "Expected 'in' after loop variable");
        free(var_name);
        return NULL;
    }

    ASTNode* iterable = parse_expression(parser);
    if (!iterable) {
        free(var_name);
        return NULL;
    }

    ASTNode* body = parse_braced_block(parser);
    if (!body) {
        free(var_name);
        ast_free(iterable);
        return NULL;
    }

    return ast_create_for_each(var_name, iterable, body, start.line, start.column);
}

static ASTNode* parse_show_statement(Parser* parser) {
    Token start = parser->previous;

    ASTNode* expr = parse_expression(parser);
    if (!expr) return NULL;

    return ast_create_show(expr, start.line, start.column);
}

static ASTNode* parse_attempt_statement(Parser* parser) {
    Token start = parser->previous;

    ASTNode* try_block = parse_braced_block(parser);
    if (!try_block) return NULL;

    if (!match(parser, TOKEN_RECOVER)) {
        ast_free(try_block);
        error_at_current(parser, "Expected 'recover' after try block");
        return NULL;
    }

    if (!match(parser, TOKEN_IDENTIFIER)) {
        ast_free(try_block);
        error_at_current(parser, "Expected error variable name after 'recover'");
        return NULL;
    }

    char* err_name = strndup_(parser->previous.start, parser->previous.length);

    ASTNode* recover_block = parse_braced_block(parser);
    if (!recover_block) {
        ast_free(try_block);
        free(err_name);
        return NULL;
    }

    return ast_create_attempt(try_block, err_name, recover_block, start.line, start.column);
}

static ASTNode* parse_give_statement(Parser* parser) {
    Token start = parser->previous;

    ASTNode* expr = parse_expression(parser);
    if (!expr) return NULL;

    return ast_create_return(expr, start.line, start.column);
}

static ASTNode* parse_add_statement(Parser* parser) {
    Token start = parser->previous;

    ASTNode* value = parse_expression(parser);
    if (!value) return NULL;

    if (!match(parser, TOKEN_TO)) {
        error_at_current(parser, "Expected 'to' after value in add statement");
        ast_free(value);
        return NULL;
    }

    if (!match(parser, TOKEN_IDENTIFIER)) {
        error_at_current(parser, "Expected list name after 'to'");
        ast_free(value);
        return NULL;
    }

    char* lname = strndup_(parser->previous.start, parser->previous.length);
    return ast_create_append(value, lname, start.line, start.column);
}

static ASTNode* parse_statement(Parser* parser) {
    if (match(parser, TOKEN_WHEN)) return parse_if_statement(parser);
    if (match(parser, TOKEN_REPEAT)) return parse_repeat_statement(parser);
    if (match(parser, TOKEN_WHILE)) return parse_while_statement(parser);
    if (match(parser, TOKEN_FOR)) return parse_for_each_statement(parser);
    if (match(parser, TOKEN_SHOW)) return parse_show_statement(parser);
    if (match(parser, TOKEN_ATTEMPT)) return parse_attempt_statement(parser);
    if (match(parser, TOKEN_STOP)) return ast_create_node(AST_STOP, parser->previous.line, parser->previous.column);
    if (match(parser, TOKEN_SKIP)) return ast_create_node(AST_SKIP, parser->previous.line, parser->previous.column);
    if (match(parser, TOKEN_GIVE)) return parse_give_statement(parser);
    if (match(parser, TOKEN_ADD)) return parse_add_statement(parser);

    if (check(parser, TOKEN_LBRACE)) {
        advance(parser);
        size_t stmt_count = 0;
        ASTNode** stmts = parse_statements(parser, TOKEN_RBRACE, &stmt_count);
        if (!stmts) return NULL;
        advance(parser);
        return ast_create_block(stmts, stmt_count, parser->previous.line, parser->previous.column);
    }

    return parse_declaration(parser);
}

static ASTNode** parse_statements(Parser* parser, int stop_token, size_t* out_count) {
    ASTNode** statements = NULL;
    size_t count = 0;
    size_t cap = 0;

    while (!check(parser, stop_token) && !parser->had_error && parser->current.type != TOKEN_EOF) {
        ASTNode* stmt = parse_statement(parser);
        if (!stmt) {
            for (size_t i = 0; i < count; i++) ast_free(statements[i]);
            free(statements);
            *out_count = 0;
            return NULL;
        }
        statements = grow_array(statements, &cap, &count, stmt);
    }

    *out_count = count;
    return statements;
}

static ASTNode* parse_expression(Parser* parser) {
    return parse_or_expr(parser);
}

static ASTNode* parse_or_expr(Parser* parser) {
    ASTNode* left = parse_and_expr(parser);
    if (!left) return NULL;

    while (match(parser, TOKEN_OR)) {
        Token op = parser->previous;
        ASTNode* right = parse_and_expr(parser);
        if (!right) return NULL;
        left = ast_create_binary_op("or", left, right, op.line, op.column);
    }

    return left;
}

static ASTNode* parse_and_expr(Parser* parser) {
    ASTNode* left = parse_equality_expr(parser);
    if (!left) return NULL;

    while (match(parser, TOKEN_AND)) {
        Token op = parser->previous;
        ASTNode* right = parse_equality_expr(parser);
        if (!right) return NULL;
        left = ast_create_binary_op("and", left, right, op.line, op.column);
    }

    return left;
}

static ASTNode* parse_equality_expr(Parser* parser) {
    ASTNode* left = parse_comparison_expr(parser);
    if (!left) return NULL;

    while (match(parser, TOKEN_IS) || match(parser, TOKEN_IS_NOT)) {
        Token op = parser->previous;
        const char* op_str = op.type == TOKEN_IS_NOT ? "is not" : "is";
        ASTNode* right = parse_comparison_expr(parser);
        if (!right) return NULL;
        left = ast_create_binary_op(op_str, left, right, op.line, op.column);
    }

    return left;
}

static ASTNode* parse_comparison_expr(Parser* parser) {
    ASTNode* left = parse_additive_expr(parser);
    if (!left) return NULL;

    while (match(parser, TOKEN_IS_ABOVE) || match(parser, TOKEN_IS_BELOW) ||
           match(parser, TOKEN_IS_AT_LEAST) || match(parser, TOKEN_IS_AT_MOST)) {
        Token op = parser->previous;
        const char* op_str;
        switch (op.type) {
            case TOKEN_IS_ABOVE: op_str = "is above"; break;
            case TOKEN_IS_BELOW: op_str = "is below"; break;
            case TOKEN_IS_AT_LEAST: op_str = "is at least"; break;
            case TOKEN_IS_AT_MOST: op_str = "is at most"; break;
            default: op_str = "?"; break;
        }
        ASTNode* right = parse_additive_expr(parser);
        if (!right) return NULL;
        left = ast_create_binary_op(op_str, left, right, op.line, op.column);
    }

    return left;
}

static ASTNode* parse_additive_expr(Parser* parser) {
    ASTNode* left = parse_multiplicative_expr(parser);
    if (!left) return NULL;

    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        Token op = parser->previous;
        const char* op_str = op.type == TOKEN_PLUS ? "plus" : "minus";
        ASTNode* right = parse_multiplicative_expr(parser);
        if (!right) return NULL;
        left = ast_create_binary_op(op_str, left, right, op.line, op.column);
    }

    return left;
}

static ASTNode* parse_multiplicative_expr(Parser* parser) {
    ASTNode* left = parse_unary_expr(parser);
    if (!left) return NULL;

    while (match(parser, TOKEN_TIMES) || match(parser, TOKEN_DIVIDED_BY) || match(parser, TOKEN_REMAINDER_OF)) {
        Token op = parser->previous;
        const char* op_str;
        switch (op.type) {
            case TOKEN_TIMES: op_str = "times"; break;
            case TOKEN_DIVIDED_BY: op_str = "divided by"; break;
            case TOKEN_REMAINDER_OF: op_str = "remainder of"; break;
            default: op_str = "?"; break;
        }
        ASTNode* right = parse_unary_expr(parser);
        if (!right) return NULL;
        left = ast_create_binary_op(op_str, left, right, op.line, op.column);
    }

    return left;
}

static ASTNode* parse_unary_expr(Parser* parser) {
    if (match(parser, TOKEN_NOT)) {
        Token op = parser->previous;
        ASTNode* operand = parse_unary_expr(parser);
        if (!operand) return NULL;
        return ast_create_unary_op("not", operand, op.line, op.column);
    }

    if (match(parser, TOKEN_MINUS)) {
        Token op = parser->previous;
        ASTNode* operand = parse_unary_expr(parser);
        if (!operand) return NULL;
        return ast_create_unary_op("minus", operand, op.line, op.column);
    }

    return parse_postfix(parser);
}

static void grow_parts(ASTNode*** parts, size_t* count, size_t* cap, ASTNode* part) {
    if (*count >= *cap) {
        *cap = *cap == 0 ? 4 : (*cap) * 2;
        *parts = realloc(*parts, sizeof(ASTNode*) * (*cap));
    }
    (*parts)[(*count)++] = part;
}

static char* decode_string(const char* s, size_t len, size_t* out_len) {
    char* buf = malloc(len + 1);
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            char e = s[++i];
            switch (e) {
                case 'n': buf[w++] = '\n'; break;
                case 't': buf[w++] = '\t'; break;
                case 'r': buf[w++] = '\r'; break;
                case '"': buf[w++] = '"'; break;
                case '\\': buf[w++] = '\\'; break;
                case '{': buf[w++] = '{'; break;
                case '}': buf[w++] = '}'; break;
                default: buf[w++] = e; break;
            }
        } else {
            buf[w++] = s[i];
        }
    }
    buf[w] = '\0';
    *out_len = w;
    return buf;
}

static ASTNode* parse_interp_subexpr(const char* src, size_t len, int line, int col) {
    (void)line; (void)col;
    Parser p;
    parser_init(&p, src, len);
    ASTNode* expr = parser_parse_expression(&p);
    if (!expr || p.had_error) {
        if (expr) ast_free(expr);
        parser_free(&p);
        return NULL;
    }
    parser_free(&p);
    return expr;
}

static ASTNode* parse_interpolated_string(Parser* parser, const char* raw, size_t len, int line, int col) {
    (void)parser; (void)line; (void)col;
    ASTNode** parts = NULL;
    size_t count = 0, cap = 0;

    char* buf = malloc(len + 1);
    memcpy(buf, raw, len);
    buf[len] = '\0';

    size_t i = 0, seg = 0;
    while (i < len) {
        if (buf[i] == '\\' && i + 1 < len) {
            i += 2;
            continue;
        }
        if (buf[i] == '{') {
            if (i > seg) {
                char* lit = strndup_(buf + seg, i - seg);
                size_t dlen;
                char* dec = decode_string(lit, i - seg, &dlen);
                grow_parts(&parts, &count, &cap, ast_create_string(dec, dlen, line, col));
                free(lit);
                free(dec);
            }
            size_t j = i + 1;
            while (j < len && buf[j] != '}') j++;
            if (j >= len) {
                seg = i;
                break;
            }
            ASTNode* expr = parse_interp_subexpr(buf + i + 1, j - (i + 1), line, col);
            if (expr) {
                grow_parts(&parts, &count, &cap, expr);
            }
            i = j + 1;
            seg = i;
        } else {
            i++;
        }
    }

    if (seg < len) {
        char* lit = strndup_(buf + seg, len - seg);
        size_t dlen;
        char* dec = decode_string(lit, len - seg, &dlen);
        grow_parts(&parts, &count, &cap, ast_create_string(dec, dlen, line, col));
        free(lit);
        free(dec);
    }

    free(buf);
    return ast_create_interp_string(parts, count, line, col);
}

static ASTNode* parse_postfix(Parser* parser) {
    ASTNode* expr = parse_primary_expr(parser);
    if (!expr) return NULL;

    while (match(parser, TOKEN_DOT)) {
        if (!match(parser, TOKEN_IDENTIFIER)) {
            error_at_current(parser, "Expected property name after '.'");
            ast_free(expr);
            return NULL;
        }
        char* prop = strndup_(parser->previous.start, parser->previous.length);
        expr = ast_create_member_access(expr, prop, parser->previous.line, parser->previous.column);
    }

    while (match(parser, TOKEN_LBRACKET)) {
        ASTNode* idx = parse_expression(parser);
        if (!idx) {
            ast_free(expr);
            return NULL;
        }
        if (!match(parser, TOKEN_RBRACKET)) {
            error_at_current(parser, "Expected ']' after index");
            ast_free(expr);
            ast_free(idx);
            return NULL;
        }
        expr = ast_create_index_access(expr, idx, parser->previous.line, parser->previous.column);
    }

    return expr;
}

static ASTNode* parse_identifier_or_call(Parser* parser) {
    Token name_token = parser->current;
    char* name = strndup_(name_token.start, name_token.length);
    advance(parser);

    if (match(parser, TOKEN_WITH)) {
        size_t arg_count = 0;
        ASTNode** args = parse_expression_list(parser, &arg_count);
        if (!args) {
            free(name);
            return NULL;
        }
        return ast_create_function_call(name, args, arg_count, name_token.line, name_token.column);
    }

    return ast_create_identifier(name, name_token.line, name_token.column);
}

static ASTNode* parse_primary_expr(Parser* parser) {
    Token start = parser->current;

    if (match(parser, TOKEN_LPAREN)) {
        ASTNode* inner = parse_expression(parser);
        if (!inner) return NULL;
        if (!match(parser, TOKEN_RPAREN)) {
            error_at_current(parser, "Expected ')' after expression");
            ast_free(inner);
            return NULL;
        }
        return inner;
    }

    if (match(parser, TOKEN_INTEGER)) {
        return ast_create_integer((int64_t)parser->previous.integer_value, start.line, start.column);
    }

    if (match(parser, TOKEN_NUMBER)) {
        return ast_create_number(parser->previous.number_value, start.line, start.column);
    }

    if (match(parser, TOKEN_STRING)) {
        const char* s = parser->previous.string_value;
        size_t slen = parser->previous.string_length;
        int has_interp = 0;
        for (size_t k = 0; k < slen; k++) {
            if (s[k] == '\\' && k + 1 < slen) { k++; continue; }
            if (s[k] == '{') { has_interp = 1; break; }
        }
        if (has_interp) {
            return parse_interpolated_string(parser, s, slen, start.line, start.column);
        }
        size_t dlen;
        char* dec = decode_string(s, slen, &dlen);
        ASTNode* strnode = ast_create_string(dec, dlen, start.line, start.column);
        free(dec);
        return strnode;
    }

    if (match(parser, TOKEN_TRUE)) {
        return ast_create_boolean(1, start.line, start.column);
    }

    if (match(parser, TOKEN_FALSE)) {
        return ast_create_boolean(0, start.line, start.column);
    }

    if (match(parser, TOKEN_EMPTY)) {
        return ast_create_empty(start.line, start.column);
    }

    if (match(parser, TOKEN_ASK)) {
        return ast_create_function_call("ask", NULL, 0, start.line, start.column);
    }

    if (match(parser, TOKEN_LBRACKET)) {
        size_t item_count = 0;
        ASTNode** items = parse_expression_list(parser, &item_count);
        if (!items && item_count != 0) return NULL;
        if (!match(parser, TOKEN_RBRACKET)) {
            for (size_t i = 0; i < item_count; i++) ast_free(items[i]);
            free(items);
            error_at_current(parser, "Expected ']' after list elements");
            return NULL;
        }
        return ast_create_list(items, item_count, start.line, start.column);
    }

    if (match(parser, TOKEN_MAP)) {
        if (!match(parser, TOKEN_LBRACKET)) {
            error_at_current(parser, "Expected '[' after 'map'");
            return NULL;
        }

        size_t entry_count = 0;
        ASTNode** entries = parse_map_entries(parser, &entry_count);
        if (!entries && entry_count != 0) return NULL;
        if (!match(parser, TOKEN_RBRACKET)) {
            for (size_t i = 0; i < entry_count; i++) ast_free(entries[i]);
            free(entries);
            error_at_current(parser, "Expected ']' after map entries");
            return NULL;
        }

        return ast_create_map(entries, entry_count, start.line, start.column);
    }

    if (match(parser, TOKEN_LPAREN)) {
        ASTNode* expr = parse_expression(parser);
        if (!expr) return NULL;
        if (!match(parser, TOKEN_RPAREN)) {
            error_at_current(parser, "Expected ')' after expression");
            return NULL;
        }
        return expr;
    }

    if (check(parser, TOKEN_IDENTIFIER)) {
        return parse_identifier_or_call(parser);
    }

    if (check(parser, TOKEN_LBRACE)) {
        size_t stmt_count = 0;
        ASTNode** stmts = parse_statements(parser, TOKEN_RBRACE, &stmt_count);
        if (!stmts) return NULL;
        advance(parser);
        return ast_create_block(stmts, stmt_count, start.line, start.column);
    }

    error_at_current(parser, "Expected expression");
    return NULL;
}

static ASTNode** parse_expression_list(Parser* parser, size_t* out_count) {
    ASTNode** items = NULL;
    size_t count = 0;
    size_t cap = 0;

    if (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_NEWLINE) &&
        parser->current.type != TOKEN_EOF && !check(parser, TOKEN_RBRACKET)) {
        do {
            ASTNode* item = parse_expression(parser);
            if (!item) {
                for (size_t i = 0; i < count; i++) ast_free(items[i]);
                free(items);
                *out_count = 0;
                return NULL;
            }
            items = grow_array(items, &cap, &count, item);
        } while (match(parser, TOKEN_COMMA));
    }

    *out_count = count;
    return items;
}

static ASTNode** parse_map_entries(Parser* parser, size_t* out_count) {
    ASTNode** entries = NULL;
    size_t count = 0;
    size_t cap = 0;

    if (!check(parser, TOKEN_RBRACKET)) {
        do {
            if (!match(parser, TOKEN_IDENTIFIER)) {
                for (size_t i = 0; i < count; i++) ast_free(entries[i]);
                free(entries);
                *out_count = 0;
                return NULL;
            }

            char* key = strndup_(parser->previous.start, parser->previous.length);

            if (!match(parser, TOKEN_IS)) {
                for (size_t i = 0; i < count; i++) ast_free(entries[i]);
                free(entries);
                free(key);
                *out_count = 0;
                return NULL;
            }

            ASTNode* value = parse_expression(parser);
            if (!value) {
                for (size_t i = 0; i < count; i++) ast_free(entries[i]);
                free(entries);
                free(key);
                *out_count = 0;
                return NULL;
            }

            ASTNode* entry = ast_create_map_entry(key, value, parser->previous.line, parser->previous.column);
            entries = grow_array(entries, &cap, &count, entry);
        } while (match(parser, TOKEN_COMMA));
    }

    *out_count = count;
    return entries;
}

void parser_init(Parser* parser, const char* source, size_t length) {
    parser->lexer = (Lexer*)malloc(sizeof(Lexer));
    lexer_init(parser->lexer, source, length);

    parser->current.type = TOKEN_EOF;
    parser->current.start = NULL;
    parser->current.length = 0;
    parser->current.line = 0;
    parser->current.column = 0;

    parser->previous.type = TOKEN_EOF;
    parser->previous.start = NULL;
    parser->previous.length = 0;
    parser->previous.line = 0;
    parser->previous.column = 0;

    parser->had_error = 0;
    parser->error_message = NULL;

    advance(parser);
}

ASTNode* parser_parse(Parser* parser) {
    size_t count = 0;
    ASTNode** stmts = parse_statements(parser, TOKEN_EOF, &count);

    if (!stmts || parser->had_error) {
        if (stmts) {
            for (size_t i = 0; i < count; i++) ast_free(stmts[i]);
            free(stmts);
        }
        return NULL;
    }

    return ast_create_program(stmts, count);
}

ASTNode* parser_parse_expression(Parser* parser) {
    ASTNode* expr = parse_expression(parser);
    if (parser->had_error) {
        if (expr) ast_free(expr);
        return NULL;
    }
    return expr;
}

void parser_free(Parser* parser) {
    if (parser->lexer) {
        free(parser->lexer);
        parser->lexer = NULL;
    }
}

const char* parser_get_error(Parser* parser) {
    if (!parser->had_error) return NULL;

    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Parse error at line %d, column %d: %s",
             parser->error_line, parser->error_column, parser->error_message);
    return buffer;
}