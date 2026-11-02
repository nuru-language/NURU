#define _DEFAULT_SOURCE
#include "nuru_import.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char** names;
    size_t count;
    size_t cap;
} IncludeSet;

static int is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int set_has(IncludeSet* s, const char* name) {
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->names[i], name) == 0) return 1;
    }
    return 0;
}

static int set_add(IncludeSet* s, const char* name) {
    if (s->count >= s->cap) {
        size_t ncap = s->cap == 0 ? 8 : s->cap * 2;
        char** tmp = realloc(s->names, sizeof(char*) * ncap);
        if (!tmp) return 0;
        s->names = tmp;
        s->cap = ncap;
    }
    s->names[s->count++] = strdup(name);
    return s->names[s->count - 1] != NULL;
}

/* If the line (len bytes, not newline-terminated) begins with `bring <name>`
 * (after optional whitespace, and followed only by whitespace or a comment),
 * set *start / *namelen to the name span and return 1. */
/* Parse a `bring` directive. Two forms:
 *   bring <module>
 *   bring <name> from <module>
 * On success sets *nstart/*nlen to the module identifier; if *selective is
 * set, *fstart/*flen hold the selected function name. */
static int parse_bring(const char* line, size_t len,
                       size_t* nstart, size_t* nlen,
                       int* selective, size_t* fstart, size_t* flen) {
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i + 5 > len || memcmp(line + i, "bring", 5) != 0) return 0;
    i += 5;
    if (i >= len || (line[i] != ' ' && line[i] != '\t')) return 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    size_t a = i;
    while (i < len && is_ident_char(line[i])) i++;
    if (i == a) return 0;
    size_t aend = i;

    size_t j = i;
    while (j < len && (line[j] == ' ' || line[j] == '\t')) j++;
    if (j + 4 <= len && memcmp(line + j, "from", 4) == 0) {
        size_t k = j + 4;
        while (k < len && (line[k] == ' ' || line[k] == '\t')) k++;
        size_t m = k;
        while (k < len && is_ident_char(line[k])) k++;
        if (k != m) {
            size_t q = k;
            while (q < len && (line[q] == ' ' || line[q] == '\t')) q++;
            if (q >= len || line[q] == '#' || line[q] == '\n' || line[q] == '\r') {
                *selective = 1;
                *fstart = a;
                *flen = aend - a;
                *nstart = m;
                *nlen = k - m;
                return 1;
            }
        }
    }

    size_t q = aend;
    while (q < len && (line[q] == ' ' || line[q] == '\t')) q++;
    if (q < len && line[q] != '#' && line[q] != '\n' && line[q] != '\r') return 0;
    *selective = 0;
    *nstart = a;
    *nlen = aend - a;
    return 1;
}

/* Copy the top-level definition of function `fname` from `src` to `out`. */
static void emit_function_block(FILE* out, const char* src, size_t len,
                                const char* fname, size_t fnamelen) {
    size_t start = 0;
    int in_fn = 0;
    int depth = 0;
    int opened = 0;
    while (start <= len) {
        size_t end = start;
        while (end < len && src[end] != '\n') end++;
        size_t linelen = end - start;
        size_t l = linelen;
        if (l > 0 && src[end - 1] == '\r') l--;

        if (!in_fn) {
            size_t p = 0;
            while (p < l && (src[start + p] == ' ' || src[start + p] == '\t')) p++;
            if (p + 6 <= l && memcmp(src + start + p, "create", 6) == 0) {
                size_t q = p + 6;
                while (q < l && (src[start + q] == ' ' || src[start + q] == '\t')) q++;
                if (q + fnamelen <= l &&
                    memcmp(src + start + q, fname, fnamelen) == 0 &&
                    (q + fnamelen >= l || !is_ident_char(src[start + q + fnamelen]))) {
                    in_fn = 1;
                }
            }
        }

        if (in_fn) {
            fwrite(src + start, 1, l, out);
            if (end < len) fputc('\n', out);
            for (size_t z = 0; z < l; z++) {
                if (src[start + z] == '{') { depth++; opened = 1; }
                else if (src[start + z] == '}') { if (depth > 0) depth--; }
            }
            if (opened && depth == 0) return;
        }

        if (end < len) start = end + 1;
        else break;
    }
}

static int read_file(const char* path, char** out, size_t* outlen) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    rewind(f);
    char* buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 0; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    *out = buf;
    *outlen = n;
    return 1;
}

static int resolve_module(const char* name, size_t namelen,
                          const char* stdlib_dir, char* outpath, size_t outcap) {
    char modname[256];
    if (namelen >= sizeof(modname)) return 0;
    memcpy(modname, name, namelen);
    modname[namelen] = '\0';

    if (stdlib_dir) {
        snprintf(outpath, outcap, "%s/%s.nuru", stdlib_dir, modname);
        FILE* t = fopen(outpath, "rb");
        if (t) { fclose(t); return 1; }
    }
    snprintf(outpath, outcap, "%s.nuru", modname);
    FILE* t = fopen(outpath, "rb");
    if (t) { fclose(t); return 1; }
    return 0;
}

/* Selectively import one function definition from `path` (include-once). */
static int emit_selected(const char* path, IncludeSet* included, FILE* out,
                         const char* fname, size_t flen,
                         char* errbuf, size_t errcap) {
    if (set_has(included, path)) return 1;
    if (!set_add(included, path)) {
        snprintf(errbuf, errcap, "out of memory while resolving imports");
        return 0;
    }
    char* src = NULL;
    size_t len = 0;
    if (!read_file(path, &src, &len)) {
        snprintf(errbuf, errcap, "cannot read module file '%s'", path);
        return 0;
    }
    emit_function_block(out, src, len, fname, flen);
    free(src);
    return 1;
}

static int expand_file(const char* path, IncludeSet* included, FILE* out,
                       const char* stdlib_dir, char* errbuf, size_t errcap) {
    if (set_has(included, path)) return 1;
    if (!set_add(included, path)) {
        snprintf(errbuf, errcap, "out of memory while resolving imports");
        return 0;
    }

    char* src = NULL;
    size_t len = 0;
    if (!read_file(path, &src, &len)) {
        snprintf(errbuf, errcap, "cannot read module file '%s'", path);
        return 0;
    }

    size_t start = 0;
    int ok = 1;
    while (start <= len && ok) {
        size_t end = start;
        while (end < len && src[end] != '\n') end++;
        size_t linelen = end - start;
        /* strip trailing \r */
        if (linelen > 0 && src[end - 1] == '\r') linelen--;

        size_t ns = 0, nlen = 0, fs = 0, fl = 0;
        int sel = 0;
        if (parse_bring(src + start, linelen, &ns, &nlen, &sel, &fs, &fl)) {
            char mpath[512];
            if (!resolve_module(src + start + ns, nlen, stdlib_dir, mpath, sizeof(mpath))) {
                char modname[256];
                size_t mn = nlen;
                if (mn >= sizeof(modname)) mn = sizeof(modname) - 1;
                memcpy(modname, src + start + ns, mn);
                modname[mn] = '\0';
                snprintf(errbuf, errcap, "module '%s' not found (searched %s/, ./)",
                         modname, stdlib_dir ? stdlib_dir : ".");
                ok = 0;
                break;
            }
            if (sel) {
                if (!emit_selected(mpath, included, out, src + start + fs, fl,
                                   errbuf, errcap)) {
                    ok = 0;
                    break;
                }
            } else if (!expand_file(mpath, included, out, stdlib_dir, errbuf, errcap)) {
                ok = 0;
                break;
            }
        } else {
            fwrite(src + start, 1, linelen, out);
            if (end < len) fputc('\n', out);
        }
        if (end < len) start = end + 1;
        else break;
    }

    /* guarantee the included content ends on its own line */
    if (len > 0 && src[len - 1] != '\n') fputc('\n', out);

    free(src);
    return ok;
}

static int collect_rec(const char* path, const char* stdlib_dir, IncludeSet* inc,
                       char*** paths, size_t* count, size_t* cap,
                       char* errbuf, size_t errcap) {
    if (set_has(inc, path)) return 1;
    if (!set_add(inc, path)) {
        snprintf(errbuf, errcap, "out of memory while resolving imports");
        return 0;
    }
    char* src = NULL;
    size_t len = 0;
    if (!read_file(path, &src, &len)) {
        snprintf(errbuf, errcap, "cannot read module file '%s'", path);
        return 0;
    }

    if (*count >= *cap) {
        *cap = *cap == 0 ? 16 : *cap * 2;
        char** tmp = realloc(*paths, sizeof(char*) * *cap);
        if (!tmp) { free(src); snprintf(errbuf, errcap, "out of memory"); return 0; }
        *paths = tmp;
    }
    (*paths)[(*count)++] = strdup(path);

    size_t start = 0;
    int ok = 1;
    while (start <= len && ok) {
        size_t end = start;
        while (end < len && src[end] != '\n') end++;
        size_t linelen = end - start;
        if (linelen > 0 && src[end - 1] == '\r') linelen--;

        size_t ns = 0, nlen = 0, fs = 0, fl = 0;
        int sel = 0;
        if (parse_bring(src + start, linelen, &ns, &nlen, &sel, &fs, &fl)) {
            char mpath[512];
            if (!resolve_module(src + start + ns, nlen, stdlib_dir, mpath, sizeof(mpath))) {
                char modname[256];
                size_t mn = nlen;
                if (mn >= sizeof(modname)) mn = sizeof(modname) - 1;
                memcpy(modname, src + start + ns, mn);
                modname[mn] = '\0';
                snprintf(errbuf, errcap, "module '%s' not found (searched %s/, ./)",
                         modname, stdlib_dir ? stdlib_dir : ".");
                ok = 0;
                break;
            }
            if (!collect_rec(mpath, stdlib_dir, inc, paths, count, cap, errbuf, errcap)) {
                ok = 0;
                break;
            }
        }
        if (end < len) start = end + 1;
        else break;
    }
    free(src);
    return ok;
}

size_t nuru_collect_imports(const char* src, size_t len, const char* stdlib_dir,
                            char*** out_paths, char* errbuf, size_t errcap) {
    IncludeSet included = {0};
    size_t cap = 16, count = 0;
    char** paths = malloc(sizeof(char*) * cap);
    if (!paths) {
        snprintf(errbuf, errcap, "out of memory");
        return (size_t)-1;
    }

    size_t start = 0;
    int ok = 1;
    while (start <= len && ok) {
        size_t end = start;
        while (end < len && src[end] != '\n') end++;
        size_t linelen = end - start;
        if (linelen > 0 && src[end - 1] == '\r') linelen--;

        size_t ns = 0, namelen = 0;
        if (match_bring(src + start, linelen, &ns, &namelen)) {
            char mpath[512];
            if (!resolve_module(src + start + ns, namelen, stdlib_dir, mpath, sizeof(mpath))) {
                char modname[256];
                if (namelen >= sizeof(modname)) namelen = sizeof(modname) - 1;
                memcpy(modname, src + start + ns, namelen);
                modname[namelen] = '\0';
                snprintf(errbuf, errcap, "module '%s' not found (searched %s/, ./)",
                         modname, stdlib_dir ? stdlib_dir : ".");
                ok = 0;
                break;
            }
            if (!collect_rec(mpath, stdlib_dir, &included, &paths, &count, &cap,
                             errbuf, errcap)) {
                ok = 0;
                break;
            }
        }
        if (end < len) start = end + 1;
        else break;
    }

    for (size_t i = 0; i < included.count; i++) free(included.names[i]);
    free(included.names);

    if (!ok) {
        for (size_t i = 0; i < count; i++) free(paths[i]);
        free(paths);
        return (size_t)-1;
    }
    *out_paths = paths;
    return count;
}

char* nuru_expand_imports(const char* src, size_t len, size_t* out_len,
                          const char* stdlib_dir,
                          char* errbuf, size_t errcap) {
    IncludeSet included = {0};
    char* outbuf = NULL;
    size_t outsz = 0;
    FILE* out = open_memstream(&outbuf, &outsz);
    if (!out) {
        snprintf(errbuf, errcap, "cannot allocate output stream");
        return NULL;
    }

    /* treat the main source as an anonymous root; process it line by line */
    size_t start = 0;
    int ok = 1;
    while (start <= len && ok) {
        size_t end = start;
        while (end < len && src[end] != '\n') end++;
        size_t linelen = end - start;
        if (linelen > 0 && src[end - 1] == '\r') linelen--;

        size_t ns = 0, namelen = 0;
        if (match_bring(src + start, linelen, &ns, &namelen)) {
            char mpath[512];
            if (!resolve_module(src + start + ns, namelen, stdlib_dir, mpath, sizeof(mpath))) {
                char modname[256];
                if (namelen >= sizeof(modname)) namelen = sizeof(modname) - 1;
                memcpy(modname, src + start + ns, namelen);
                modname[namelen] = '\0';
                snprintf(errbuf, errcap, "module '%s' not found (searched %s/, ./)",
                         modname, stdlib_dir ? stdlib_dir : ".");
                ok = 0;
                break;
            }
            if (!expand_file(mpath, &included, out, stdlib_dir, errbuf, errcap)) {
                ok = 0;
                break;
            }
        } else {
            fwrite(src + start, 1, linelen, out);
            if (end < len) fputc('\n', out);
        }
        if (end < len) start = end + 1;
        else break;
    }

    /* newline-terminate the expanded output if the root source lacked one */
    if (len > 0 && src[len - 1] != '\n') fputc('\n', out);

    fclose(out);

    for (size_t i = 0; i < included.count; i++) free(included.names[i]);
    free(included.names);

    if (!ok) {
        free(outbuf);
        return NULL;
    }
    if (out_len) *out_len = outsz;
    return outbuf; /* caller frees */
}
