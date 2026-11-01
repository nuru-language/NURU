#include "nuru_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_at_end(Lexer* lexer) {
    return lexer->current >= lexer->source_length;
}

static char peek(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->source[lexer->current];
}

static char peek_next(Lexer* lexer) {
    if (lexer->current + 1 >= lexer->source_length) return '\0';
    return lexer->source[lexer->current + 1];
}

static char advance(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    char c = lexer->source[lexer->current++];
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static void skip_whitespace(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lexer);
        } else if (c == '#') {
            if (peek_next(lexer) == '#' && peek_next(lexer) == '#') {
                break;
            }
            while (!is_at_end(lexer) && peek(lexer) != '\n') {
                advance(lexer);
            }
        } else {
            break;
        }
    }
}

static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    memset(&token, 0, sizeof(token));
    token.type = type;
    token.start = lexer->source + lexer->token_start_offset;
    token.length = lexer->current - lexer->token_start_offset;
    token.line = lexer->line;
    if (token.line < 1) token.line = 1;
    token.column = lexer->token_start_column;
    return token;
}

static Token make_error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    return make_error_token(lexer, message);
}

static TokenType identifier_type(Lexer* lexer) {
    size_t length = lexer->current - lexer->token_start_offset;
    const char* start = lexer->source + lexer->token_start_offset;

    if (length == 4) {
        if (strncmp(start, "make", 4) == 0) return TOKEN_MAKE;
        if (strncmp(start, "with", 4) == 0) return TOKEN_WITH;
        if (strncmp(start, "give", 4) == 0) return TOKEN_GIVE;
        if (strncmp(start, "when", 4) == 0) return TOKEN_WHEN;
        if (strncmp(start, "each", 4) == 0) return TOKEN_EACH;
        if (strncmp(start, "show", 4) == 0) return TOKEN_SHOW;
        if (strncmp(start, "stop", 4) == 0) return TOKEN_STOP;
        if (strncmp(start, "skip", 4) == 0) return TOKEN_SKIP;
        if (strncmp(start, "true", 4) == 0) return TOKEN_TRUE;
        if (strncmp(start, "list", 4) == 0) return TOKEN_LIST;
    }

    if (length == 2) {
        if (strncmp(start, "be", 2) == 0) return TOKEN_BE;
        if (strncmp(start, "to", 2) == 0) return TOKEN_TO;
        if (strncmp(start, "or", 2) == 0) return TOKEN_OR;
        if (strncmp(start, "in", 2) == 0) return TOKEN_IN;
    }

    if (length == 3) {
        if (strncmp(start, "not", 3) == 0) return TOKEN_NOT;
        if (strncmp(start, "for", 3) == 0) return TOKEN_FOR;
        if (strncmp(start, "and", 3) == 0) return TOKEN_AND;
        if (strncmp(start, "map", 3) == 0) return TOKEN_MAP;
        if (strncmp(start, "add", 3) == 0) return TOKEN_ADD;
        if (strncmp(start, "ask", 3) == 0) return TOKEN_ASK;
    }

    if (length == 5) {
        if (strncmp(start, "using", 5) == 0) return TOKEN_USING;
        if (strncmp(start, "while", 5) == 0) return TOKEN_WHILE;
        if (strncmp(start, "false", 5) == 0) return TOKEN_FALSE;
        if (strncmp(start, "empty", 5) == 0) return TOKEN_EMPTY;
        if (strncmp(start, "times", 5) == 0) return TOKEN_TIMES;
    }

    if (length == 6) {
        if (strncmp(start, "create", 6) == 0) return TOKEN_CREATE;
        if (strncmp(start, "repeat", 6) == 0) return TOKEN_REPEAT;
        if (strncmp(start, "change", 6) == 0) return TOKEN_CHANGE;
    }

    if (length == 10) {
        if (strncmp(start, "changeable", 10) == 0) return TOKEN_CHANGEABLE;
    }

    if (length == 7) {
        if (strncmp(start, "attempt", 7) == 0) return TOKEN_ATTEMPT;
        if (strncmp(start, "recover", 7) == 0) return TOKEN_RECOVER;
    }

    if (length == 9) {
        if (strncmp(start, "otherwise", 9) == 0) return TOKEN_OTHERWISE;
    }

    if (length == 6 && strncmp(start, "repeat", 6) == 0) return TOKEN_REPEAT;
    if (length == 4 && strncmp(start, "plus", 4) == 0) return TOKEN_PLUS;
    if (length == 5 && strncmp(start, "minus", 5) == 0) return TOKEN_MINUS;
    if (length == 2 && strncmp(start, "is", 2) == 0) return TOKEN_IS;

    return TOKEN_IDENTIFIER;
}

static int consume_next_word(Lexer* lexer, const char* word) {
    size_t i = lexer->current;
    while (i < lexer->source_length && (lexer->source[i] == ' ' || lexer->source[i] == '\t')) {
        i++;
    }
    size_t len = strlen(word);
    if (i + len <= lexer->source_length &&
        strncmp(lexer->source + i, word, len) == 0 &&
        (i + len == lexer->source_length ||
         !(isalnum((unsigned char)lexer->source[i + len]) || lexer->source[i + len] == '_'))) {
        lexer->current = i + len;
        return 1;
    }
    return 0;
}

static Token scan_identifier(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (isalnum(c) || c == '_') {
            advance(lexer);
        } else {
            break;
        }
    }

    TokenType type = identifier_type(lexer);
    size_t first_len = lexer->current - lexer->token_start_offset;
    const char* first = lexer->source + lexer->token_start_offset;

    if (type == TOKEN_IS) {
        if (consume_next_word(lexer, "not")) {
            type = TOKEN_IS_NOT;
        } else if (consume_next_word(lexer, "above")) {
            type = TOKEN_IS_ABOVE;
        } else if (consume_next_word(lexer, "below")) {
            type = TOKEN_IS_BELOW;
        } else if (consume_next_word(lexer, "at")) {
            if (consume_next_word(lexer, "least")) {
                type = TOKEN_IS_AT_LEAST;
            } else if (consume_next_word(lexer, "most")) {
                type = TOKEN_IS_AT_MOST;
            }
        }
    } else if (type == TOKEN_IDENTIFIER) {
        if (first_len && strncmp(first, "divided", first_len < 7 ? first_len : 7) == 0 &&
            consume_next_word(lexer, "by")) {
            type = TOKEN_DIVIDED_BY;
        } else if (first_len && strncmp(first, "remainder", first_len < 9 ? first_len : 9) == 0 &&
                   consume_next_word(lexer, "of")) {
            type = TOKEN_REMAINDER_OF;
        }
    }

    Token token = make_token(lexer, type);
    return token;
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static Token scan_number(Lexer* lexer) {
    int is_float = 0;

    while (is_digit(peek(lexer))) {
        advance(lexer);
    }

    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        is_float = 1;
        advance(lexer);
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }

    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        is_float = 1;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }

    if (!is_float) {
        Token token = make_token(lexer, TOKEN_INTEGER);
        token.integer_value = strtoll(token.start, NULL, 10);
        return token;
    }

    Token token = make_token(lexer, TOKEN_NUMBER);
    token.number_value = atof(token.start);
    return token;
}

static Token scan_string(Lexer* lexer) {
    size_t start = lexer->current;

    // Skip the opening quote
    if (peek(lexer) == '"') {
        advance(lexer);
    }

    // Scan for the closing quote
    while (!is_at_end(lexer)) {
        char c = peek(lexer);

        if (c == '"') {
            advance(lexer);
            Token token = make_token(lexer, TOKEN_STRING);
            token.string_value = (char*)token.start + 1;
            token.string_length = lexer->current - lexer->token_start_offset - 2;
            return token;
        }

        if (c == '\n') {
            lexer->token_start_offset = start;
            return error_token(lexer, "Unterminated string");
        }

        if (c == '\\') {
            advance(lexer);
            if (is_at_end(lexer)) {
                lexer->token_start_offset = start;
                return error_token(lexer, "Unterminated string after escape");
            }
            char escape = peek(lexer);
            if (escape != 'n' && escape != 'r' && escape != 't' &&
                escape != '"' && escape != '\\' && escape != '{' && escape != '}') {
                lexer->token_start_offset = start;
                return error_token(lexer, "Invalid escape sequence");
            }
        }
        advance(lexer);
    }

    lexer->token_start_offset = start;
    return error_token(lexer, "Unterminated string");
}

static Token scan_multiline_comment(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        if (peek(lexer) == '#' && peek_next(lexer) == '#' &&
            lexer->current + 2 < lexer->source_length &&
            lexer->source[lexer->current + 2] == '#') {
            advance(lexer);
            advance(lexer);
            advance(lexer);
            return make_token(lexer, TOKEN_MULTILINE_COMMENT_END);
        }
        advance(lexer);
    }

    return error_token(lexer, "Unterminated multiline comment");
}

void lexer_init(Lexer* lexer, const char* source, size_t length) {
    lexer->source = source;
    lexer->source_length = length;
    lexer->current = 0;
    lexer->token_start_offset = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_start_column = 1;
    lexer->state = LEX_STATE_NORMAL;
    lexer->interpolation_depth = 0;
}

static Token next_token_normal(Lexer* lexer) {
    skip_whitespace(lexer);

    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }

    lexer->token_start_offset = lexer->current;
    lexer->token_start_column = lexer->column;

    char c = peek(lexer);

    if (c == '#' && peek_next(lexer) == '#' && !is_at_end(lexer) &&
        lexer->current + 2 < lexer->source_length &&
        lexer->source[lexer->current + 2] == '#') {
        advance(lexer);
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_MULTILINE_COMMENT_START);
    }

    if (c == '#') {
        while (!is_at_end(lexer) && peek(lexer) != '\n') {
            advance(lexer);
        }
        return make_token(lexer, TOKEN_HASH);
    }

    if (c == '\n') {
        advance(lexer);
        return make_token(lexer, TOKEN_NEWLINE);
    }

    if (isalpha(c) || c == '_') {
        return scan_identifier(lexer);
    }

    if (is_digit(c)) {
        return scan_number(lexer);
    }

    if (c == '"') {
        return scan_string(lexer);
    }

    advance(lexer);

    switch (c) {
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case '[': return make_token(lexer, TOKEN_LBRACKET);
        case ']': return make_token(lexer, TOKEN_RBRACKET);
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case '.': return make_token(lexer, TOKEN_DOT);
        case '-': return make_token(lexer, TOKEN_MINUS);
        case '+': return make_token(lexer, TOKEN_PLUS);
        default:
            break;
    }

    char message[64];
    snprintf(message, sizeof(message), "Unexpected character: '%c'", c);
    return error_token(lexer, message);
}

Token lexer_next_token(Lexer* lexer) {
    if (lexer->state == LEX_STATE_NORMAL) {
        Token token = next_token_normal(lexer);

        if (token.type == TOKEN_MULTILINE_COMMENT_START) {
            return scan_multiline_comment(lexer);
        }

        return token;
    }

    return make_token(lexer, TOKEN_EOF);
}

const char* token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_INTEGER: return "INTEGER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_MAKE: return "make";
        case TOKEN_BE: return "be";
        case TOKEN_CHANGEABLE: return "changeable";
        case TOKEN_CHANGE: return "change";
        case TOKEN_TO: return "to";
        case TOKEN_ADD: return "add";
        case TOKEN_CREATE: return "create";
        case TOKEN_USING: return "using";
        case TOKEN_WITH: return "with";
        case TOKEN_GIVE: return "give";
        case TOKEN_WHEN: return "when";
        case TOKEN_OTHERWISE: return "otherwise";
        case TOKEN_REPEAT: return "repeat";
        case TOKEN_TIMES: return "times";
        case TOKEN_WHILE: return "while";
        case TOKEN_FOR: return "for";
        case TOKEN_EACH: return "each";
        case TOKEN_IN: return "in";
        case TOKEN_SHOW: return "show";
        case TOKEN_ASK: return "ask";
        case TOKEN_ATTEMPT: return "attempt";
        case TOKEN_RECOVER: return "recover";
        case TOKEN_STOP: return "stop";
        case TOKEN_SKIP: return "skip";
        case TOKEN_AND: return "and";
        case TOKEN_OR: return "or";
        case TOKEN_NOT: return "not";
        case TOKEN_IS: return "is";
        case TOKEN_IS_NOT: return "is not";
        case TOKEN_IS_ABOVE: return "is above";
        case TOKEN_IS_BELOW: return "is below";
        case TOKEN_IS_AT_LEAST: return "is at least";
        case TOKEN_IS_AT_MOST: return "is at most";
        case TOKEN_PLUS: return "plus";
        case TOKEN_MINUS: return "minus";
        case TOKEN_DIVIDED_BY: return "divided by";
        case TOKEN_REMAINDER_OF: return "remainder of";
        case TOKEN_TRUE: return "true";
        case TOKEN_FALSE: return "false";
        case TOKEN_EMPTY: return "empty";
        case TOKEN_MAP: return "map";
        case TOKEN_LIST: return "list";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_COMMA: return ",";
        case TOKEN_DOT: return ".";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_HASH: return "#";
        case TOKEN_MULTILINE_COMMENT_START: return "###";
        case TOKEN_MULTILINE_COMMENT_END: return "###";
        default: return "UNKNOWN";
    }
}
