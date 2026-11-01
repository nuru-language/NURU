# NURU Runtime and Memory Specification v0.1

## 1. Value Representation

### 1.1 Tagged Value Model

```
NuruValue (64-bit):
┌─────────────────────────────────┬─────────────────────────────────┐
│ bits 0-7:  type tag (8 bits)   │ bits 8-63: payload (56 bits)   │
└─────────────────────────────────┴─────────────────────────────────┘
```

### 1.2 Type Tags

```c
typedef enum {
    TAG_EMPTY      = 0x00,
    TAG_BOOLEAN    = 0x01,
    TAG_INTEGER    = 0x02,
    TAG_NUMBER     = 0x03,
    TAG_STRING     = 0x04,
    TAG_LIST       = 0x05,
    TAG_MAP        = 0x06,
    TAG_FUNCTION   = 0x07,
    TAG_ERROR      = 0x08,
    TAG_CFUNC      = 0x09,  /* C function */
    TAG_NOREF      = 0xFF,  /* no reference (gc marker) */
} ValueTag;
```

### 1.3 Payload Encoding

**Empty:** Tag=0, payload=0

**Boolean:** Tag=1, payload=0 (false) or 1 (true)

**Integer:** Tag=2, payload=two's complement 56-bit integer

**Number:** Tag=3, payload=IEEE 754 binary64 in remaining bits

**String:** Tag=4, payload=pointer to String object

**List:** Tag=5, payload=pointer to List object

**Map:** Tag=6, payload=pointer to Map object

**Function:** Tag=7, payload=pointer to Function object

**Error:** Tag=8, payload=pointer to Error object

**C Function:** Tag=9, payload=pointer to CFunction object

## 2. Object Layouts

### 2.1 String Object

```c
struct NuruString {
    uint32_t ref_count;
    uint32_t length;       /* character count */
    uint32_t capacity;    /* allocated bytes */
    char*    data;        /* UTF-8 encoded */
};
```

### 2.2 List Object

```c
struct NuruList {
    uint32_t ref_count;
    uint32_t count;
    uint32_t capacity;
    NuruValue* items;     /* array of values */
};
```

### 2.3 Map Object

```c
struct NuruMap {
    uint32_t ref_count;
    uint32_t count;
    NuruEntry* entries;   /* key-value pairs */
};

struct NuruEntry {
    NuruString* key;
    NuruValue value;
};
```

### 2.4 Function Object

```c
struct NuruFunction {
    uint32_t ref_count;
    uint8_t  type;        /* 0=native, 1=script */
    char*    name;
    uint8_t  arity;
    void*    code;         /* NIR or native address */
    NuruEnv* closure;     /* captured environment */
};
```

### 2.5 Error Object

```c
struct NuruError {
    uint32_t ref_count;
    char*    message;
    char*    file;
    uint32_t line;
    uint32_t column;
    NuruValue stack_trace;
};
```

## 3. Call Frames

```c
struct NuruCallFrame {
    NuruFunction* function;
    NuruValue*    locals;     /* local variables */
    NuruValue*    ip;        /* instruction pointer */
    NuruValue*    stack_base;
    NuruCallFrame* parent;
};
```

## 4. Environments

```c
struct NuruEnv {
    NuruEnv* parent;
    uint32_t size;
    uint32_t count;
    NuruBinding* bindings;  /* variable bindings */
};

struct NuruBinding {
    NuruString* name;
    NuruValue value;
    uint8_t mutable;
};
```

## 5. Garbage Collection

### 5.1 Mark-and-Sweep Collector

**Phase 1 - Mark:**
- Start from root set (globals, call frames, stack)
- Recursively mark reachable objects
- Mark closure environments

**Phase 2 - Sweep:**
- Scan all object allocations
- Free unmarked objects
- Update ref counts

### 5.2 Root Set

- Global environment
- Current call frame
- Local variables in current frame
- Operand stack
- Pending value registers

### 5.3 Memory Layout

```
┌─────────────────────────────────────────┐
│ Nuru VM State                           │
├─────────────────────────────────────────┤
│ Global Environment                       │
├─────────────────────────────────────────┤
│ Call Stack (max 1024 frames)             │
├─────────────────────────────────────────┤
│ Value Stack (for NIR execution)         │
├─────────────────────────────────────────┤
│ Heap (mark-and-sweep managed)            │
│   - Strings                             │
│   - Lists                               │
│   - Maps                                │
│   - Functions                           │
│   - Errors                              │
│   - Environments                        │
└─────────────────────────────────────────┘
```

## 6. Operators

### 6.1 Integer Operations

- add, subtract, multiply
- divide (error on zero)
- remainder (error on zero)
- bitwise operations (future)

### 6.2 Number Operations

- add, subtract, multiply, divide
- floor, ceil, round
- powers, roots
- trigonometric functions

### 6.3 Comparison Operations

All comparisons return boolean:
- `is` / `is not`
- `is above` / `is below`
- `is at least` / `is at most`

### 6.4 String Operations

- concatenation (+)
- length
- indexing
- slicing
- interpolation

### 6.5 List Operations

- length
- indexing
- slicing
- append
- remove

### 6.6 Map Operations

- get by key
- set by key
- has key
- remove key
- keys, values

## 7. Error Handling

### 7.1 Error Categories

```c
typedef enum {
    ERR_CATEGORY_SYSTEM,
    ERR_CATEGORY_SYNTAX,
    ERR_CATEGORY_RUNTIME,
    ERR_CATEGORY_TYPE,
    ERR_CATEGORY_MEMORY,
    ERR_CATEGORY_ASSERTION,
} ErrorCategory;
```

### 7.2 Error Properties

Every error carries:
- `message`: Human-readable description
- `category`: Error type
- `file`: Source file name
- `line`: Source line number
- `column`: Source column number

### 7.3 Attempt/Recover

```nuru
attempt {
    risky_operation
}
recover error {
    handle_error
}
```

## 8. Runtime ABI

### 8.1 Native Function Interface

```c
typedef NuruValue (*NativeFn)(NuruVM* vm, size_t argc, NuruValue* argv);

typedef struct {
    uint8_t type;          /* 0=native, 1=script */
    union {
        NativeFn native;
        void* script;
    } code;
    char* name;
    uint8_t arity;
    NuruEnv* closure;
} NuruFunction;
```

### 8.2 VM State

```c
struct NuruVM {
    NuruCallFrame* current_frame;
    NuruValue* stack;
    NuruValue* stack_top;
    NuruValue* stack_limit;
    NuruEnv*   globals;
    NuruGC*    gc;
    size_t     bytes_allocated;
    size_t     next_gc;
};
```

### 8.3 Platform Calling Convention

**Linux x86-64 (System V ABI):**
- Integer/pointer args: RDI, RSI, RDX, RCX, R8, R9
- Float args: XMM0-XMM7
- Return: RAX (int/ptr), XMM0 (float)

**Windows x86-64 (Microsoft):**
- Integer/pointer args: RCX, RDX, R8, R9
- Float args: XMM0-XMM3
- Return: RAX (int/ptr), XMM0 (float)