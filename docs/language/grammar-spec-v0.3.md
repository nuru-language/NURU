# NURU Grammar Specification v0.3

## 1. Overview

This document defines the formal grammar for Nuru v0.3 using EBNF notation.

## 2. Source Text

```
source       ::= statement* EOF
```

## 3. Tokens

### 3.1 Identifiers

```
IDENTIFIER   ::= LETTER (LETTER | DIGIT | '_')*
LETTER       ::= 'a'..'z' | 'A'..'Z' | '_'
DIGIT        ::= '0'..'9'
```

### 3.2 Literals

```
INTEGER      ::= '0'..'9'+
NUMBER       ::= INTEGER '.' INTEGER
              | INTEGER [eE] [+-]? INTEGER
STRING       ::= '"' (ESCAPE | ~["\])* '"'
ESCAPE       ::= '\\' ('n' | 'r' | 't' | '"' | '\\' | '{')
INTERPOLATION:: '{' expression '}'
```

### 3.3 Reserved Words

```
make         ::= 'make'
be           ::= 'be'
changeable   ::= 'changeable'
change       ::= 'change'
to           ::= 'to'
create       ::= 'create'
using        ::= 'using'
with         ::= 'with'
give         ::= 'give'
when         ::= 'when'
otherwise    ::= 'otherwise'
repeat       ::= 'repeat'
times        ::= 'times'
while        ::= 'while'
for          ::= 'for'
each         ::= 'each'
in           ::= 'in'
show         ::= 'show'
attempt      ::= 'attempt'
recover      ::= 'recover'
stop         ::= 'stop'
skip         ::= 'skip'
and          ::= 'and'
or           ::= 'or'
not          ::= 'not'
is           ::= 'is'
is_not       ::= 'is not'
is_above     ::= 'is above'
is_below     ::= 'is below'
is_at_least  ::= 'is at least'
is_at_most   ::= 'is at most'
plus         ::= 'plus'
minus        ::= 'minus'
times_op     ::= 'times'
divided_by   ::= 'divided by'
remainder_of ::= 'remainder of'
true         ::= 'true'
false        ::= 'false'
empty        ::= 'empty'
map          ::= 'map'
list         ::= 'list'
```

### 3.4 Punctuation

```
LBRACE       ::= '{'
RBRACE       ::= '}'
LBRACKET     ::= '['
RBRACKET     ::= ']'
LPAREN       ::= '('
RPAREN       ::= ')'
COMMA        ::= ','
DOT          ::= '.'
NEWLINE      ::= '\n' | '\r\n'
COMMENT      ::= '#' ~[\n\r]*
MULTILINE_COMMENT ::= '###' .*? '###'
```

## 4. Statements

```
statement    ::= declaration
              | assignment
              | append_stmt
              | function_call
              | if_statement
              | repeat_statement
              | while_statement
              | for_each_statement
              | show_statement
              | attempt_statement
              | stop_statement
              | skip_statement
              | return_statement
              | block

declaration  ::= 'make' IDENTIFIER 'be' expression
              | 'make' IDENTIFIER 'be' 'changeable' expression
              | 'create' IDENTIFIER 'using' parameter_list block

assignment   ::= 'change' IDENTIFIER 'to' expression

append_stmt  ::= 'add' expression 'to' IDENTIFIER

function_call::= IDENTIFIER 'with' argument_list
              | IDENTIFIER 'with' expression

if_statement ::= 'when' expression block
              | 'when' expression block 'otherwise' block

repeat_statement ::= 'repeat' INTEGER 'times' block

while_statement ::= 'while' expression block

for_each_statement ::= 'for' 'each' IDENTIFIER 'in' expression block

show_statement ::= 'show' expression

attempt_statement ::= 'attempt' block 'recover' IDENTIFIER block

stop_statement ::= 'stop'

skip_statement ::= 'skip'

return_statement ::= 'give' expression

block        ::= '{' statement* '}'
```

## 5. Expressions

```
expression   ::= logical_or

logical_or   ::= logical_and ('or' logical_and)*

logical_and  ::= equality ('and' equality)*

equality     ::= comparison (('is' | 'is not') comparison)*

comparison   ::= term (('is above' | 'is below' | 'is at least' | 'is at most') term)*

term         ::= factor (('plus' | 'minus') factor)*

factor       ::= unary (('times' | 'divided by' | 'remainder of') unary)*

unary        ::= 'not' unary
              | primary

primary      ::= literal
              | IDENTIFIER
              | function_call
              | '(' expression ')'
              | list_literal
              | map_literal

literal      ::= INTEGER
              | NUMBER
              | STRING
              | 'true'
              | 'false'
              | 'empty'
              | 'list' '[' expression_list? ']'
              | 'map' '[' map_entry_list? ']'

list_literal ::= '[' expression_list? ']'

map_literal  ::= 'map' '[' map_entry_list? ']'

map_entry    ::= IDENTIFIER 'is' expression

expression_list ::= expression (',' expression)*

argument_list ::= expression (',' expression)*

parameter_list ::= IDENTIFIER (',' IDENTIFIER)*

map_entry_list ::= map_entry (',' map_entry)*

member_access ::= expression '.' IDENTIFIER
index_access  ::= expression 'at' expression
```

## 6. Precedence (lowest to highest)

1. `or`
2. `and`
3. `is` / `is not`
4. `is above` / `is below` / `is at least` / `is at most`
5. `plus` / `minus`
6. `times` / `divided by` / `remainder of`
7. `not`
8. Primary expressions

## 7. Associativity

- All binary operators are left-associative except comparison operators
- `not` is right-associative

## 8. Grammar Notes

### 8.1 Comma vs `and`

Commas separate function parameters and arguments. `and` is reserved for logical operations only.

**Correct:**
```nuru
create add using a, b { give a plus b }
make result be add with 10, 20
```

**Incorrect:**
```nuru
create add using a and b { }
```

### 8.2 Statement Termination

Statements are separated by newlines. Explicit semicolons are optional.

### 8.3 Block Scope

Blocks create new scope. Variables declared inside a block are not visible outside.

### 8.4 String Interpolation

String interpolation is parsed as part of string tokenization:
```
"Hello {name}"
```
The `{name}` is a placeholder evaluated at runtime.

## 9. Reserved Words List

```
make, be, changeable, change, to, add,
create, using, with, give,
when, otherwise,
repeat, times, while, for, each, in,
show,
attempt, recover,
stop, skip,
and, or, not,
is, is not, is above, is below, is at least, is at most,
plus, minus, times, divided by, remainder of,
true, false, empty,
map, list
```

## 10. Implementation Requirements

- Case-sensitive parsing
- Unicode support for identifiers (future)
- UTF-8 source encoding
- Track source location (file, line, column) for all tokens and nodes