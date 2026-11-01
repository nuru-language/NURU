#ifndef NURU_LEXER_H
#define NURU_LEXER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_ERROR,

    /* Literals */
    TOKEN_INTEGER,
    TOKEN_NUMBER,
    TOKEN_STRING,

    /* Identifiers */
    TOKEN_IDENTIFIER,

    /* Reserved Words */
    TOKEN_MAKE,
    TOKEN_BE,
    TOKEN_CHANGEABLE,
    TOKEN_CHANGE,
    TOKEN_TO,
    TOKEN_ADD,
    TOKEN_CREATE,
    TOKEN_USING,
    TOKEN_WITH,
    TOKEN_GIVE,
    TOKEN_WHEN,
    TOKEN_OTHERWISE,
    TOKEN_REPEAT,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_EACH,
    TOKEN_IN,
    TOKEN_ASK,
    TOKEN_SHOW,
    TOKEN_ATTEMPT,
    TOKEN_RECOVER,
    TOKEN_STOP,
    TOKEN_SKIP,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_IS,
    TOKEN_IS_NOT,
    TOKEN_IS_ABOVE,
    TOKEN_IS_BELOW,
    TOKEN_IS_AT_LEAST,
    TOKEN_IS_AT_MOST,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_TIMES,
    TOKEN_DIVIDED_BY,
    TOKEN_REMAINDER_OF,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_EMPTY,
    TOKEN_MAP,
    TOKEN_LIST,

    /* Operators & Punctuation */
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_NEWLINE,
    TOKEN_HASH,
    TOKEN_MULTILINE_COMMENT_START,
    TOKEN_MULTILINE_COMMENT_END,

    /* Special tokens for interpolation */
    TOKEN_INTERPOLATION_START,
    TOKEN_INTERPOLATION_END,
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    int line;
    int column;
    union {
        uint64_t integer_value;
        double number_value;
        struct {
            char* string_value;
            size_t string_length;
        };
    };
} Token;

typedef enum {
    LEX_STATE_NORMAL,
    LEX_STATE_STRING,
    LEX_STATE_STRING_ESCAPE,
    LEX_STATE_INTERPOLATION,
    LEX_STATE_COMMENT,
    LEX_STATE_MULTILINE_COMMENT,
} LexerState;

typedef struct {
    const char* source;
    size_t source_length;
    size_t current;
    size_t token_start_offset;
    int line;
    int column;
    int token_start_column;
    LexerState state;
    int interpolation_depth;
    Token error_token;
} Lexer;

void lexer_init(Lexer* lexer, const char* source, size_t length);
Token lexer_next_token(Lexer* lexer);
const char* token_type_name(TokenType type);

#endif /* NURU_LEXER_H */
