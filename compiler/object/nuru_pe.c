#include "nuru_pe.h"
#include <stdlib.h>
#include <string.h>

static void w16(unsigned char* p, unsigned v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}
static void w32(unsigned char* p, unsigned long v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}
static void w64(unsigned char* p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (unsigned char)((v >> (8 * i)) & 0xFF);
}
static unsigned u16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned long u32(const unsigned char* p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}
static unsigned long align_up(unsigned long v, unsigned long a) {
    return (v + a - 1) / a * a;
}

#define IMAGE_FILE_MACHINE_AMD64   0x8664
#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#define IMAGE_FILE_LARGE_ADDRESS_AWARE 0x0020
#define IMAGE_SUBSYSTEM_WINDOWS_CUI 3
#define IMAGE_DLLCHARACTERISTICS_NX_COMPAT 0x0100
#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define PE32P_MAGIC 0x20B

size_t nuru_pe_build(const unsigned char* text, size_t text_len,
                     uint32_t entry_rva, unsigned char** out) {
    if (!text || !out || text_len == 0) return 0;

    const unsigned long file_align = 0x200;
    const unsigned long sect_align = 0x1000;
    const unsigned long text_va = 0x1000;
    const unsigned long image_base = 0x140000000UL;

    const unsigned long pe_offset = 64;             /* e_lfanew */
    const unsigned long coff_size = 20;
    const unsigned long opt_size = 240;             /* PE32+ */
    const unsigned long sect_size = 40;
    const unsigned long headers_size = pe_offset + 4 + coff_size + opt_size + sect_size;
    const unsigned long headers_aligned = align_up(headers_size, file_align);

    const unsigned long text_raw_size = align_up((unsigned long)text_len, file_align);
    const unsigned long text_file_offset = headers_aligned;

    const unsigned long size_of_image = align_up(text_va + text_raw_size, sect_align);
    const unsigned long total = text_file_offset + text_raw_size;

    unsigned char* img = calloc(1, total);
    if (!img) return 0;

    /* DOS header */
    img[0] = 'M'; img[1] = 'Z';
    w16(img + 0x02, 0x90);          /* e_cblp */
    w16(img + 0x04, 3);             /* e_cp */
    w16(img + 0x08, 4);             /* e_cparhdr */
    w16(img + 0x0E, 0xFFFF);        /* e_maxalloc */
    w16(img + 0x10, 0xB8);          /* e_sp */
    w16(img + 0x18, 0x40);          /* e_lfarlc */
    w32(img + 0x3C, pe_offset);     /* e_lfanew */

    /* PE signature */
    img[pe_offset] = 'P'; img[pe_offset + 1] = 'E';
    img[pe_offset + 2] = 0; img[pe_offset + 3] = 0;

    /* COFF file header */
    unsigned char* cf = img + pe_offset + 4;
    w16(cf + 0, IMAGE_FILE_MACHINE_AMD64);
    w16(cf + 2, 1);                        /* NumberOfSections */
    w32(cf + 4, 0);                        /* TimeDateStamp */
    w32(cf + 8, 0);                        /* PointerToSymbolTable */
    w32(cf + 12, 0);                       /* NumberOfSymbols */
    w16(cf + 16, opt_size);                /* SizeOfOptionalHeader */
    w16(cf + 18, IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE);

    /* PE32+ optional header */
    unsigned char* oh = cf + coff_size;
    w16(oh + 0, PE32P_MAGIC);
    w16(oh + 2, 0);                        /* linker version */
    w32(oh + 4, (unsigned long)text_raw_size);   /* SizeOfCode */
    w32(oh + 8, 0);                        /* SizeOfInitializedData */
    w32(oh + 12, 0);                       /* SizeOfUninitializedData */
    w32(oh + 16, entry_rva);               /* AddressOfEntryPoint */
    w32(oh + 20, text_va);                 /* BaseOfCode */
    w64(oh + 24, image_base);              /* ImageBase */
    w32(oh + 32, sect_align);
    w32(oh + 36, file_align);
    w16(oh + 40, 6); w16(oh + 42, 0);     /* OS version */
    w16(oh + 44, 6); w16(oh + 46, 0);     /* subsystem version */
    w32(oh + 48, 0);                       /* Win32VersionValue */
    w32(oh + 52, size_of_image);
    w32(oh + 56, headers_aligned);
    w32(oh + 60, 0);                       /* CheckSum */
    w16(oh + 64, IMAGE_SUBSYSTEM_WINDOWS_CUI);
    w16(oh + 66, IMAGE_DLLCHARACTERISTICS_NX_COMPAT);
    w64(oh + 68, 0x100000);                /* stack reserve */
    w64(oh + 76, 0x1000);                  /* stack commit */
    w64(oh + 84, 0x100000);                /* heap reserve */
    w64(oh + 92, 0x1000);                  /* heap commit */
    w32(oh + 100, 0);                      /* LoaderFlags */
    w32(oh + 104, 16);                     /* NumberOfRvaAndSizes */
    /* 16 data directories (8 bytes each) remain zero */

    /* Section header (.text) */
    unsigned char* sh = oh + opt_size;
    memcpy(sh, ".text", 5);
    w32(sh + 8, (unsigned long)text_len);  /* VirtualSize */
    w32(sh + 12, text_va);
    w32(sh + 16, text_raw_size);
    w32(sh + 20, text_file_offset);
    w32(sh + 24, 0);                       /* PointerToRelocations */
    w32(sh + 28, 0);                       /* PointerToLinenumbers */
    w16(sh + 32, 0);                       /* NumberOfRelocations */
    w16(sh + 34, 0);                       /* NumberOfLinenumbers */
    w32(sh + 36, IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);

    /* section bytes */
    memcpy(img + text_file_offset, text, text_len);

    *out = img;
    return total;
}

int nuru_pe_validate(const unsigned char* img, size_t len) {
    if (!img || len < 256) return 0;
    if (img[0] != 'M' || img[1] != 'Z') return 0;
    unsigned long pe = u32(img + 0x3C);
    if (pe + 4 + 20 + 240 + 40 > len) return 0;
    if (img[pe] != 'P' || img[pe + 1] != 'E') return 0;
    if (u16(img + pe + 4) != IMAGE_FILE_MACHINE_AMD64) return 0;
    if (u16(img + pe + 6) != 1) return 0;
    if (u16(img + pe + 4 + 20) != PE32P_MAGIC) return 0;
    if (u16(img + pe + 4 + 20 + 64) != IMAGE_SUBSYSTEM_WINDOWS_CUI) return 0;
    /* section name + characteristics */
    const unsigned char* sh = img + pe + 4 + 20 + 240;
    if (memcmp(sh, ".text", 5) != 0) return 0;
    unsigned long ch = u32(sh + 36);
    if (!(ch & IMAGE_SCN_CNT_CODE)) return 0;
    return 1;
}
