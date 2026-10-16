/* Initial chaining values for the two SHA-2 digest sizes. */
#ifndef SYSMEM_SHA2_INTERNAL_H
#define SYSMEM_SHA2_INTERNAL_H

#include <stdint.h>

extern const uint32_t sha224InitialState[8];
extern const uint32_t sha256InitialState[8];
extern const uint32_t sha256RoundConstants[64];

const unsigned char *Sha256Transform(const unsigned char *plain,
	unsigned int blocks, uint32_t state[8]);

#endif
