#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "object/nuru_pe.h"

static unsigned rd16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned long rd32(const unsigned char* p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

int main(void) {
    /* minimal x86-64 code: return 0 via rax */
    unsigned char code[] = { 0x31, 0xC0, 0xC3 };
    unsigned char* img = NULL;
    size_t n = nuru_pe_build(code, sizeof(code), 0x1000, &img);
    assert(n > 0 && img);

    /* structural checks */
    assert(nuru_pe_validate(img, n));

    assert(img[0] == 'M' && img[1] == 'Z');
    assert(rd32(img + 0x3C) == 64);            /* e_lfanew */
    size_t pe = 64;
    assert(img[pe] == 'P' && img[pe + 1] == 'E');
    assert(rd16(img + pe + 4) == 0x8664);      /* AMD64 */
    assert(rd16(img + pe + 6) == 1);           /* one section */
    assert(rd16(img + pe + 4 + 20) == 0x20B);  /* PE32+ magic */
    assert(rd16(img + pe + 4 + 20 + 64) == 3); /* console subsystem */
    assert(rd32(img + pe + 4 + 20 + 52) > 0);  /* SizeOfImage */

    /* .text section present, characteristics, and code bytes embedded */
    const unsigned char* sh = img + pe + 4 + 20 + 240;
    assert(memcmp(sh, ".text", 5) == 0);
    assert(rd32(sh + 36) & 0x00000020);        /* CNT_CODE */
    unsigned long raw_off = rd32(sh + 20);
    unsigned long raw_size = rd32(sh + 16);
    assert(raw_off + raw_size <= n);
    assert(memcmp(img + raw_off, code, sizeof(code)) == 0);

    free(img);
    printf("PASS  pe_structure\n");
    printf("pe tests: 1 passed\n");
    return 0;
}
