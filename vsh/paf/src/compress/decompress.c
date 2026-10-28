#include "decompress.h"
#include "../std/stdlib.h"
#include <libdeflt.h>

int32_t Decompress(void *dst, size_t dstsize, const void *src, size_t srcsize,
		   DecompressType type)
{
	int32_t ret;

	if (type != DecompressType_Zlib) {
		ScePafStdc_D41FD265();
		return SCE_PAF_ERROR_DECOMPRESS_INVALID_FORMAT;
	}
	ret = sceZlibDecompress(dst, dstsize, src, NULL);
	if (ret == SCE_DEFLATE_ERROR_INVALID_SIZE)
		return SCE_PAF_ERROR_DECOMPRESS_INVALID_SIZE;
	if (ret == SCE_DEFLATE_ERROR_INVALID_FORMAT)
		return SCE_PAF_ERROR_DECOMPRESS_INVALID_FORMAT;
	return ret;
}
