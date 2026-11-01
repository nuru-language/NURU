#ifndef NURU_NIR_H
#define NURU_NIR_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    NIR_LOAD_CONST,
    NIR_LOAD_VAR,
    NIR_STORE_VAR,
    NIR_ADD,
    NIR_SUB,
    NIR_MUL,
    NIR_DIV,
    NIR_MOD,
    NIR_NEG,
    NIR_NOT,
    NIR_CMP_EQ,
    NIR_CMP_NE,
    NIR_CMP_LT,
    NIR_CMP_GT,
    NIR_CMP_LE,
    NIR_CMP_GE,
    NIR_JUMP,
    NIR_JUMP_IF_FALSE,
    NIR_JUMP_IF_TRUE,
    NIR_CALL,
    NIR_RETURN,
    NIR_RETURN_VALUE,
    NIR_PRINT_INT,
    NIR_PRINT_STRING,
    NIR_PRINT_VALUE_RAW,
    NIR_PRINT_NEWLINE,
    NIR_READ_LINE,
    NIR_LEN,
    NIR_ABS,
    NIR_MIN,
    NIR_MAX,
    NIR_TO_NUMBER,
    NIR_CONCAT,
    NIR_ORD,
    NIR_CHR,
    NIR_POW,
    NIR_CONTAINS,
    NIR_SUBSTRING,
    NIR_UPPER,
    NIR_LOWER,
    NIR_INDEX,
    NIR_TO_STRING,
    NIR_READ_FILE,
    NIR_WRITE_FILE,
    NIR_ALLOC_LIST,
    NIR_LOAD_LIST_LEN,
    NIR_LOAD_ELT,
    NIR_LOAD_ELT_EXPR,
    NIR_ALLOC_MAP,
    NIR_LOAD_MAP_FIELD,
    NIR_APPEND,
    NIR_ERR_SAVE,
    NIR_ERR_SET,
    NIR_ERR_RESTORE,
    NIR_HALT,
} NIROpcode;

typedef enum {
    NIR_CONST_INT,
    NIR_CONST_NUM,
    NIR_CONST_STR,
    NIR_VAR,
} NIRValueKind;

typedef struct NIRValue {
    NIRValueKind kind;
    union {
        int64_t integer_value;
        double number_value;
        struct {
            const char* data;
            size_t length;
        } string_value;
        const char* name;
    };
} NIRValue;

struct NIRBasicBlock;

typedef struct NIRInstruction {
    NIROpcode op;
    NIRValue* a;
    NIRValue* b;
    struct NIRBasicBlock* target;
    int line;
} NIRInstruction;

typedef struct NIRBasicBlock {
    const char* name;
    NIRInstruction** instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    struct NIRBasicBlock* next;
} NIRBasicBlock;

typedef struct NIRFunction {
    const char* name;
    char** params;
    size_t param_count;
    char** locals;
    size_t local_count;
    size_t local_capacity;
    NIRBasicBlock* entry;
    NIRBasicBlock* tail;
    struct NIRProgram* program;
} NIRFunction;

typedef struct NIRProgram {
    NIRFunction** functions;
    size_t function_count;
    size_t function_capacity;
    NIRValue** constants;
    size_t constant_count;
    size_t constant_capacity;
    size_t block_counter;
} NIRProgram;

NIRProgram* nir_program_create(void);
void nir_program_free(NIRProgram* program);

NIRFunction* nir_function_create(NIRProgram* program, const char* name);
NIRBasicBlock* nir_block_create(NIRFunction* func, const char* name);
NIRInstruction* nir_emit(NIRBasicBlock* block, NIROpcode op, NIRValue* a, NIRValue* b, NIRBasicBlock* target, int line);

NIRValue* nir_const_int(NIRProgram* program, int64_t value);
NIRValue* nir_const_num(NIRProgram* program, double value);
NIRValue* nir_const_str(NIRProgram* program, const char* data, size_t length);
NIRValue* nir_var_ref(const char* name);

void nir_function_add_param(NIRFunction* func, const char* name);
void nir_function_add_local(NIRFunction* func, const char* name);
int nir_function_has_var(NIRFunction* func, const char* name);

const char* nir_opcode_name(NIROpcode op);
void nir_print_program(NIRProgram* program);

#endif
