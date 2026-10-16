/* SceSysmem's common runtime representation of SHA-224 and SHA-256. */
#ifndef SCE_SHA2_H
#define SCE_SHA2_H

#include <stddef.h>
#include <stdint.h>

/* The public SHA-224 context calls the eighth result word pad2.  The kernel
 * compression routine consumes and updates all eight words for both hashes.
 */
typedef struct SceKernelSha2Context {
	uint32_t h[8];
	uint8_t reserved[8];
	uint64_t ullTotalLen;
	union {
		uint8_t buf[64];
		struct {
			uint8_t partial[63];
			uint8_t countAndComputed;
		} buffered;
	};
	uint32_t result[8];
} SceKernelSha2Context;

_Static_assert(sizeof(SceKernelSha2Context) == 144, "kernel SHA-2 context size");
_Static_assert(offsetof(SceKernelSha2Context, ullTotalLen) == 40 &&
	       offsetof(SceKernelSha2Context, buf) == 48 &&
	       offsetof(SceKernelSha2Context, buffered.countAndComputed) == 111 &&
	       offsetof(SceKernelSha2Context, result) == 112,
	       "kernel SHA-2 context layout");

#endif
