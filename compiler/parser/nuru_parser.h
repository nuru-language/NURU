#ifndef NURU_PARSER_H
#define NURU_PARSER_H

#include "../lexer/nuru_lexer.h"
#include "../ast/nuru_ast.h"

typedef struct {
    Lexer* lexer;
    Token current;
    Token previous;
    int had_error;
    const char* error_message;
    int error_line;
    int error_column;
} Parser;

void parser_init(Parser* parser, const char* source, size_t length);
ASTNode* parser_parse(Parser* parser);
ASTNode* parser_parse_expression(Parser* parser);
void parser_free(Parser* parser);
const char* parser_get_error(Parser* parser);

#endif /* NURU_PARSER_H */