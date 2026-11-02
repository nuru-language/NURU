#ifndef NURU_IMPORT_H
#define NURU_IMPORT_H

#include <stddef.h>

/*
 * Expand `bring <module>` directives by substituting module file contents.
 *
 * A `bring` directive is a line whose first token is the identifier `bring`
 * followed by a module name (an identifier). It is replaced with the expanded
 * contents of the resolved module file. Modules are searched for in:
 *   1. <stdlib_dir>/<name>.nuru
 *   2. <name>.nuru (relative to the current directory)
 *
 * Each module file is included at most once per compilation (include-once).
 * Returns a newly allocated expanded source (caller frees) and sets *out_len,
 * or returns NULL on error with a message in errbuf (errcap bytes).
 */
char* nuru_expand_imports(const char* src, size_t len, size_t* out_len,
                          const char* stdlib_dir,
                          char* errbuf, size_t errcap);

#endif /* NURU_IMPORT_H */
