#ifndef PAF_COMPRESS_DECOMPRESS_H
#define PAF_COMPRESS_DECOMPRESS_H

#include <stddef.h>
#include <stdint.h>

#define sceZlibDecompress ScePafLowlayer_D7DAA151
#include <deflt.h>

#define SCE_PAF_ERROR_DECOMPRESS_INVALID_FORMAT (-2135992576) /* 0x80AF5700 */
#define SCE_PAF_ERROR_DECOMPRESS_INVALID_SIZE (-2135992572) /* 0x80AF5704 */

typedef enum DecompressType {
	DecompressType_None = 0,
	DecompressType_Zlib = 1
} DecompressType;

#define Decompress ScePafMisc_5E39FCE1

int32_t Decompress(void *dst, size_t dstsize, const void *src, size_t srcsize,
		   DecompressType type);

#endif
