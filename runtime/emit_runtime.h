#ifndef NURU_EMIT_RUNTIME_H
#define NURU_EMIT_RUNTIME_H

#include <stdio.h>

/*
 * Runtime-emission interface. These functions write the runtime's embedded
 * assembly/data into the generated source file. They are the extraction target
 * for the runtime currently emitted inline by compiler/nir/nuru_codegen.c.
 * See runtime/README.md for the runtime architecture.
 */

/* Emit the garbage-collector data section (heap, bump pointer, free list). */
void nuru_emit_gc_data(FILE* out);

/* Emit the program literal table (string constants + runtime string literals). */
void nuru_emit_literal_data(FILE* out, struct NIRProgram* program);

#endif /* NURU_EMIT_RUNTIME_H */
