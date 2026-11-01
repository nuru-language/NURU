# NURU Compiler Architecture Specification v0.1

## 1. Overview

The Nuru compiler translates Nuru source code to native executables through a multi-stage pipeline.

## 2. Compiler Pipeline

```
Nuru Source (.nuru)
       │
       ▼
   ┌─────────┐
   │  Lexer  │
   └────┬────┘
        │ Tokens
        ▼
   ┌─────────┐
   │  Parser │
   └────┬────┘
        │ AST
        ▼
 ┌─────────────┐
 │   Semantic │
 │   Analysis │
 └─────┬──────┘
       │ Validated AST
       ▼
 ┌─────────────┐
 │ Name/Scope │
 │ Resolution  │
 └─────┬──────┘
       │
       ▼
 ┌──────────────────┐
 │      NIR         │
 │ (Intermediate    │
 │  Representation) │
 └─────┬────────────┘
       │ NIR CFG
       ▼
 ┌─────────────┐
 │  Optimizer  │
 └─────┬───────┘
       │ Optimized NIR
       ▼
 ┌─────────────┐
 │   Native    │
 │ Code Gen    │
 └─────┬───────┘
       │ Assembly/MC
       ▼
 ┌─────────────┐
 │   Object    │
 │  Generator  │
 └─────┬───────┘
       │ Object File
       ▼
 ┌─────────────┐
 │   Linker   │
 └─────┬───────┘
       │ Executable
       ▼
  Native Program
```

## 3. Directory Structure

```
compiler/
├── lexer/         # Tokenization
├── parser/        # AST generation
├── ast/           # AST node definitions
├── semantic/      # Static analysis
├── resolver/      # Name/scope resolution
├── nir/          # Intermediate representation
├── optimizer/     # Optimization passes
├── codegen/       # Native code generation
├── object/        # Object file format
└── driver/        # Compiler driver
```

## 4. Component Specifications

### 4.1 Lexer (compiler/lexer/)

**Input:** Raw source text
**Output:** Token stream
**Responsibilities:**
- Character-by-character scanning
- Token classification
- Source location tracking
- Error reporting

### 4.2 Parser (compiler/parser/)

**Input:** Token stream
**Output:** Abstract Syntax Tree
**Responsibilities:**
- Grammar parsing
- AST construction
- Syntax error reporting
- Operator precedence handling

### 4.3 AST (compiler/ast/)

**Input:** Parser output
**Output:** Typed AST nodes
**Responsibilities:**
- Node type definitions
- Node creation utilities
- AST traversal
- Node comparison

### 4.4 Semantic Analysis (compiler/semantic/)

**Input:** Raw AST
**Output:** Annotated AST with resolved types
**Responsibilities:**
- Variable declaration validation
- Duplicate declaration checking
- Scope boundary validation
- Function parameter validation
- Return statement validation

### 4.5 Resolver (compiler/resolver/)

**Input:** AST with semantic annotations
**Output:** AST with resolved names
**Responsibilities:**
- Name resolution
- Scope graph construction
- Closure analysis
- Import resolution (future)

### 4.6 NIR (compiler/nir/)

**Input:** Validated AST
**Output:** NIR control flow graph
**Responsibilities:**
- AST to NIR lowering
- Basic block construction
- Control flow graph
- Virtual register allocation
- NIR validation

### 4.7 Optimizer (compiler/optimizer/)

**Input:** NIR CFG
**Output:** Optimized NIR CFG
**Responsibilities:**
- Constant folding
- Dead code elimination
- Common subexpression elimination
- Control flow simplification
- Strength reduction

### 4.8 Code Generator (compiler/codegen/)

**Input:** Optimized NIR
**Output:** Machine code / assembly
**Responsibilities:**
- Instruction selection
- Register allocation
- Stack frame layout
- Calling convention
- Prologue/epilogue generation

### 4.9 Object Generator (compiler/object/)

**Input:** Machine code
**Output:** Relocatable object file
**Responsibilities:**
- Section construction
- Symbol table generation
- Relocation entries
- ELF64 (Linux) format
- COFF (Windows) format

### 4.10 Driver (compiler/driver/)

**Input:** Command line arguments
**Output:** Compiled executable
**Responsibilities:**
- Pipeline orchestration
- File I/O
- Error handling
- Build options

## 5. NIR Specification

### 5.1 NIR Structure

```c
struct NIRFunction {
    char* name;
    uint32_t id;
    uint32_t param_count;
    uint32_t local_count;
    List* basic_blocks;
};

struct NIRBasicBlock {
    uint32_t id;
    List* instructions;
    NIRBasicBlock* predecessor_true;
    NIRBasicBlock* predecessor_false;
    NIRBasicBlock* successor;
};

enum NIRInstructionType {
    // Constants
    NIR_LOAD_INT,
    NIR_LOAD_NUM,
    NIR_LOAD_STRING,
    NIR_LOAD_BOOL,
    NIR_LOAD_NIL,

    // Variables
    NIR_LOAD_LOCAL,
    NIR_STORE_LOCAL,
    NIR_LOAD_GLOBAL,
    NIR_STORE_GLOBAL,

    // Operations
    NIR_ADD,
    NIR_SUB,
    NIR_MUL,
    NIR_DIV,
    NIR_REM,

    // Comparisons
    NIR_CMP_EQ,
    NIR_CMP_NE,
    NIR_CMP_LT,
    NIR_CMP_GT,
    NIR_CMP_LE,
    NIR_CMP_GE,

    // Logical
    NIR_AND,
    NIR_OR,
    NIR_NOT,

    // Control flow
    NIR_JUMP,
    NIR_JUMP_IF_TRUE,
    NIR_JUMP_IF_FALSE,

    // Functions
    NIR_CALL,
    NIR_RETURN,

    // Collections
    NIR_NEW_LIST,
    NIR_NEW_MAP,
    NIR_LIST_GET,
    NIR_LIST_SET,
    NIR_MAP_GET,
    NIR_MAP_SET,

    // Functions
    NIR_NEW_FUNCTION,
    NIR_NEW_CLOSURE,
};
```

## 6. Code Generation Targets

### 6.1 x86-64 Linux

- Format: ELF64
- Calling: System V AMD64 ABI
- Relocations: RELA

### 6.2 x86-64 Windows

- Format: COFF/PE
- Calling: Microsoft x64 ABI
- Relocations: COFF relocation

## 7. Runtime Integration

The compiler generates calls to runtime helper functions for:
- Memory allocation
- Garbage collection
- String operations
- List operations
- Map operations
- Error handling
- I/O operations

## 8. Bootstrap Dependencies

- Initial build requires C compiler (gcc/clang)
- System linker (ld, link.exe) for initial bootstrap
- Target: eventual self-hosting
