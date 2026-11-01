#ifndef NURU_OPTIMIZER_H
#define NURU_OPTIMIZER_H

#include "../nir/nuru_nir.h"

/* Runs peephole optimizations on the NIR program in place. */
void nir_optimize(NIRProgram* program);

#endif
