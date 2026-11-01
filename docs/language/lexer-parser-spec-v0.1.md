# NURU Lexer/Parser Specification v0.1

## 1. Lexer Architecture

### 1.1 Token Structure

```c
typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    int line;
    int column;
    union {
        uint64_t integer_value;
        double number_value;
    };
} Token;
```

### 1.2 Token Types

```c
typedef enum {
    TOKEN_EOF,
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
    TOKEN_CREATE,
    TOKEN_USING,
    TOKEN_WITH,
    TOKEN_GIVE,
    TOKEN_WHEN,
    TOKEN_OTHERWISE,
    TOKEN_REPEAT,
    TOKEN_TIMES,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_EACH,
    TOKEN_IN,
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

    /* Operators */
    TOKEN_ASSIGN,        /* = */
    TOKEN_LBRACE,        /* { */
    TOKEN_RBRACE,        /* } */
    TOKEN_LBRACKET,      /* [ */
    TOKEN_RBRACKET,      /* ] */
    TOKEN_LPAREN,        /* ( */
    TOKEN_RPAREN,        /* ) */
    TOKEN_COMMA,         /* , */
    TOKEN_DOT,           /* . */
    TOKEN_NEWLINE,

    /* Special */
    TOKEN_COMMENT,
    TOKEN_MULTILINE_COMMENT,
} TokenType;
```

### 1.3 Lexer States

```
STATE_NORMAL
STATE_STRING
STATE_STRING_ESCAPE
STATE_STRING_INTERPOLATION
STATE_COMMENT
STATE_MULTILINE_COMMENT
```

## 2. Lexer Rules

### 2.1 Whitespace Handling

- Spaces and tabs are skipped
- Newlines are preserved as TOKEN_NEWLINE (for statement separation)
- One newline minimum between statements

### 2.2 Comment Handling

```nuru
# Single line comment - skipped entirely

### Multi-line
comment ###
```

Single line comments: `#` to end of line
Multi-line comments: `###` to next `###`

### 2.3 String Rules

- Delimited by double quotes
- Escape sequences: `\n`, `\r`, `\t`, `\"`, `\\`, `\{`
- String interpolation: `{expression}` inside strings
- Unterminated strings produce error
- Invalid escape sequences produce error

### 2.4 Number Rules

Integers: `[0-9]+`
Numbers: `[0-9]+\.[0-9]+` or scientific notation
Numbers with exponent: `[0-9]+(\.[0-9]+)?[eE][+-]?[0-9]+`

### 2.5 Identifier Rules

Start: letter or underscore
Continue: letter, digit, or underscore

### 2.6 Reserved Word Matching

After matching an identifier, check against reserved word table:
```
make -> TOKEN_MAKE
be -> TOKEN_BE
...
```

## 3. Parser Architecture

### 3.1 AST Node Types

```c
typedef enum {
    AST_PROGRAM,
    AST_DECLARATION,
    AST_ASSIGNMENT,
    AST_FUNCTION_DECL,
    AST_FUNCTION_CALL,
    AST_PARAMETER,
    AST_ARGUMENT,
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
```

### 3.2 Precedence Table

| Precedence | Operator(s) | Associativity |
|------------|-------------|---------------|
| 1 | `or` | Left |
| 2 | `and` | Left |
| 3 | `is`, `is not` | Left |
| 4 | `is above`, `is below`, `is at least`, `is at most` | Left |
| 5 | `plus`, `minus` | Left |
| 6 | `times`, `divided by`, `remainder of` | Left |
| 7 | `not` | Right |
| 8 | Primary | - |

## 4. Parser Grammar

```
program        := statement* EOF

statement      := declaration
                | assignment
                | expr_statement
                | if_statement
                | repeat_statement
                | while_statement
                | for_each_statement
                | show_statement
                | attempt_statement
                | control_statement

declaration    := 'make' IDENTIFIER 'be' expression
                | 'make' IDENTIFIER 'be' 'changeable' expression
                | 'create' IDENTIFIER 'using' param_list block

assignment     := 'change' IDENTIFIER 'to' expression

expr_statement := expression

if_statement   := 'when' expression block
                | 'when' expression block 'otherwise' block

repeat_statement := 'repeat' INTEGER 'times' block

while_statement := 'while' expression block

for_each_statement := 'for' 'each' IDENTIFIER 'in' expression block

show_statement := 'show' expression

attempt_statement := 'attempt' block 'recover' IDENTIFIER block

control_statement := 'stop' | 'skip' | 'give' expression

block          := '{' statement* '}'

param_list     := IDENTIFIER (',' IDENTIFIER)*
arg_list       := expression (',' expression)*
expr_list      := expression (',' expression)*

expression     := or_expr

or_expr        := and_expr ('or' and_expr)*

and_expr       := equality_expr ('and' equality_expr)*

equality_expr  := comparison_expr (('is' | 'is not') comparison_expr)*

comparison_expr := additive_expr (('is above' | 'is below' | 'is at least' | 'is at most') additive_expr)*

additive_expr  := multiplicative_expr (('plus' | 'minus') multiplicative_expr)*

multiplicative_expr := unary_expr (('times' | 'divided by' | 'remainder of') unary_expr)*

unary_expr     := 'not' unary_expr | primary_expr

primary_expr   := literal
                | IDENTIFIER
                | IDENTIFIER 'with' arg_list
                | '(' expression ')'
                | list_literal
                | map_literal

literal        := INTEGER | NUMBER | STRING | 'true' | 'false' | 'empty'

list_literal   := '[' expr_list? ']'

map_literal    := 'map' '[' map_entry_list? ']'

map_entry      := IDENTIFIER 'is' expression

map_entry_list := map_entry (',' map_entry)*
```

## 5. Error Handling

### 5.1 Lexer Errors

- Invalid character
- Unterminated string
- Invalid escape sequence
- Malformed number
- Unterminated multiline comment

### 5.2 Parser Errors

- Unexpected token
- Expected token missing
- Invalid expression
- Invalid statement
- Invalid block

## 6. Source Location Tracking

All tokens and AST nodes track:
- Source file name
- Line number (1-indexed)
- Column number (1-indexed)

## 7. Memory Management

- Lexer allocates tokens from arena allocator
- Parser allocates AST nodes from pool allocator
- Tokens hold references to source text (no copy)
- String literals copy content into managed memory