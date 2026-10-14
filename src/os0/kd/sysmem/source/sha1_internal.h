/* Internal SHA-1 compression interface; the block is already byte-swapped. */
#ifndef SYSMEM_SHA1_INTERNAL_H
#define SYSMEM_SHA1_INTERNAL_H

#include <stdint.h>

uint32_t *Sha1Transform(uint32_t block[16], uint32_t state[5]);

#endif
