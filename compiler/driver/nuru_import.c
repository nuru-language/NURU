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
static int match_bring(const char* line, size_t len, size_t* start, size_t* namelen) {
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i + 5 > len || memcmp(line + i, "bring", 5) != 0) return 0;
    i += 5;
    if (i >= len || (line[i] != ' ' && line[i] != '\t')) return 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    size_t ns = i;
    while (i < len && is_ident_char(line[i])) i++;
    if (i == ns) return 0;
    size_t j = i;
    while (j < len && (line[j] == ' ' || line[j] == '\t')) j++;
    if (j < len && line[j] != '#' && line[j] != '\n' && line[j] != '\r') return 0;
    *start = ns;
    *namelen = i - ns;
    return 1;
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
            if (!expand_file(mpath, included, out, stdlib_dir, errbuf, errcap)) {
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
