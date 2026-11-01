# NURU Language Specification v0.3

## 1. Overview

**Name:** Nuru
**Version:** 0.3
**Style:** Modern general-purpose programming language
**Typing:** Dynamic
**Execution:** Native compiled

## 2. Syntax Identity

Nuru uses sentence-like syntax with distinctive keyword-based operators.

### 2.1 Reserved Words

```
make         be          changeable   change
create       using       with         give
when         otherwise   repeat       while
for          each        in           show
attempt      recover     stop         skip
and          or          not          is
is not       is above    is below     is at least    is at most
plus         minus       times        divided by     remainder of
true         false       empty        map             list
```

### 2.2 Identifiers

- Begin with ASCII letter or underscore
- Continue with letters, digits, or underscores
- Case-sensitive
- No maximum length (practical limit ~255)

## 3. Type System

### 3.1 Primitive Types

| Type | Description |
|------|-------------|
| Empty | Null/nothing value |
| Boolean | true or false |
| Integer | Signed 64-bit integer |
| Number | IEEE 754 binary64 floating point |
| String | UTF-8 encoded text |
| Function | First-class function value |
| Error | Error value with message and stack |

### 3.2 Composite Types

| Type | Description |
|------|-------------|
| List | Ordered mutable collection, zero-indexed |
| Map | Key-value pairs, string keys, insertion order |

## 4. Variable Declarations

```nuru
make name be "Amina"
make age be 22
make savings be changeable 150000
```

- `make` declares a new binding
- `be` assigns the initial value
- `changeable` makes the binding mutable
- Default binding is immutable

## 5. Variable Update

```nuru
change savings to 200000
change savings to savings plus 5000
```

- Only `changeable` bindings can be reassigned
- `change` keyword followed by `to`

## 6. Function Declarations

```nuru
create greet using name {
    show "Hello {name}"
}

create add using a, b {
    give a plus b
}
```

- `create` keyword for function definition
- `using` for parameters
- Block delimited by braces
- `give` for return value

## 7. Function Calls

```nuru
make result be add with 10, 20
greet with "Nuru"
```

- Arguments use `with` keyword
- Parameters separated by commas
- `and` is NOT a separator (reserved for logical operations)

## 8. Control Flow

### 8.1 Conditions

```nuru
when age is at least 18 {
    show "Adult"
}
otherwise {
    show "Minor"
}
```

### 8.2 Repeat Loop

```nuru
repeat 5 times {
    show "Hello"
}
```

### 8.3 While Loop

```nuru
while count is below 10 {
    change count to count plus 1
}
```

### 8.4 For Each Loop

```nuru
for each city in cities {
    show city
}
```

## 9. Collections

### 9.1 Lists

```nuru
make cities be ["Kampala", "Entebbe", "Jinja"]
show cities at 0
add "Gulu" to cities
remove "Jinja" from cities
```

### 9.2 Maps

```nuru
make person be map [
    name is "Amina",
    age is 24,
    city is "Kampala"
]

show person.name
show person["city"]
```

## 10. Error Handling

```nuru
attempt {
    make result be perform_task
    show result
}
recover error {
    show error.message
}
```

## 11. Operators

### 11.1 Arithmetic

| Operator | Purpose |
|----------|---------|
| plus | Addition |
| minus | Subtraction |
| times | Multiplication |
| divided by | Division |
| remainder of | Remainder |

### 11.2 Comparison

| Operator | Purpose |
|----------|---------|
| is | Equality |
| is not | Inequality |
| is above | Greater than |
| is below | Less than |
| is at least | Greater than or equal |
| is at most | Less than or equal |

### 11.3 Logical

| Operator | Purpose |
|----------|---------|
| and | Logical AND |
| or | Logical OR |
| not | Logical NOT |

## 12. Comments

```nuru
# Single line comment

### Multi-line
comment ###
```

## 13. String Interpolation

```nuru
show "Hello {name}"
```

## 14. Entry Point

```nuru
create main {
    show "Nuru is running"
}
```

- Exactly one `main` function required for executables
- Takes zero parameters
- Returns Integer for exit status (optional)

## 15. Exit Codes

- 0 = success
- 1-255 = error status (platform-dependent)

## 16. Source File Extension

`.nuru`

## 17. Source Encoding

UTF-8

## 18. Line Endings

LF (\n) or CRLF (\r\n) - both accepted
