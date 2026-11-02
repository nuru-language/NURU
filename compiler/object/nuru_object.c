#include "nuru_object.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

static int run_tool(char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    if (!WIFEXITED(status)) {
        fprintf(stderr, "error: %s terminated abnormally\n", argv[0]);
        return -1;
    }
    int code = WEXITSTATUS(status);
    if (code != 0) {
        fprintf(stderr, "error: %s exited with status %d\n", argv[0], code);
    }
    return code;
}

int nuru_assemble(const char* asm_path, const char* obj_path) {
    char* argv[] = { "as", "-o", (char*)obj_path, (char*)asm_path, NULL };
    return run_tool(argv);
}

int nuru_link(const char* obj_path, const char* exe_path) {
    char* argv[] = { "ld", "-o", (char*)exe_path, (char*)obj_path, NULL };
    return run_tool(argv);
}
