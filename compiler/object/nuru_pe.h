#ifndef NURU_PE_H
#define NURU_PE_H

#include <stddef.h>
#include <stdint.h>

/*
 * Minimal PE32+ (x86-64) image builder.
 *
 * Produces a structurally valid Windows portable executable in memory from a
 * single code (.text) section. This is the container-format foundation for the
 * Windows backend; it is validated structurally on any host but, like all of
 * the Windows target, only truly executable on Windows.
 */

/* Build a PE32+ console image with one .text section.
 *   text        : machine-code bytes
 *   text_len    : length in bytes
 *   entry_rva   : RVA of the entry point (e.g. first byte of .text)
 *   out         : receives malloc'd image buffer (caller frees)
 * Returns the image size, or 0 on failure. */
size_t nuru_pe_build(const unsigned char* text, size_t text_len,
                     uint32_t entry_rva, unsigned char** out);

/* Structural validator for the PE image the builder emits. Returns 1 if the
 * image conforms to the expected layout (DOS stub, PE signature, COFF header,
 * PE32+ optional header, section table), else 0. */
int nuru_pe_validate(const unsigned char* img, size_t len);

#endif /* NURU_PE_H */
