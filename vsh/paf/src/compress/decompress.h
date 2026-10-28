#ifndef PAF_COMPRESS_DECOMPRESS_H
#define PAF_COMPRESS_DECOMPRESS_H

#include <stddef.h>
#include <stdint.h>

typedef enum DecompressType {
	DecompressType_None = 0,
	DecompressType_Zlib = 1
} DecompressType;

#define Decompress ScePafMisc_5E39FCE1

int32_t Decompress(void *dst, size_t dstsize, const void *src, size_t srcsize,
		   DecompressType type);

#endif
