#include "nuru_object.h"
#include <stdio.h>
#include <stdlib.h>

int nuru_assemble(const char* asm_path, const char* obj_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "as -o \"%s\" \"%s\"", obj_path, asm_path);
    return system(cmd);
}

int nuru_link(const char* obj_path, const char* exe_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "ld -o \"%s\" \"%s\"", exe_path, obj_path);
    return system(cmd);
}
