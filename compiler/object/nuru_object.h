#ifndef NURU_OBJECT_H
#define NURU_OBJECT_H

/* Assemble an x86-64 assembly file into a relocatable object file
 * (delegates to the system assembler, `as`). Returns 0 on success. */
int nuru_assemble(const char* asm_path, const char* obj_path);

/* Link a relocatable object file into an executable
 * (delegates to the system linker, `ld`). Returns 0 on success. */
int nuru_link(const char* obj_path, const char* exe_path);

#endif
