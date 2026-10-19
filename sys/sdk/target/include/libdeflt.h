/*
 * libdeflt.h: the SDK's deflate (RFC 1951) partial-input decompression parameters (SDK Release
 * 03.570.011, target/include/libdeflt.h); the kernel's SceKernelUtilsForDriver takes the same.
 */
#ifndef SCE_LIBDEFLT_H
#define SCE_LIBDEFLT_H

struct SceDeflatePartialInputParam;

/* asked for more input once the decompressor reaches pBufEnd */
typedef const void *(SceDeflateDecompressPartialInputCallback)(
	struct SceDeflatePartialInputParam *param, unsigned int outputsize);

typedef struct SceDeflatePartialInputParam {
	unsigned int	size;			/* sizeof(SceDeflatePartialInputParam) */
	const void	*pBufEnd;		/* the end of the input given so far */
	void		*cookie;		/* the callback's */
	SceDeflateDecompressPartialInputCallback *callback;
} SceDeflatePartialInputParam;

#define SCE_DEFLATE_ERROR_INVALID_FORMAT	-2141847296	/* 0x80560100 */
#define SCE_DEFLATE_ERROR_INVALID_SIZE		-2141847295	/* 0x80560101 */
#define SCE_DEFLATE_ERROR_NOT_SUPPORTED		-2141847294	/* 0x80560102 */
#define SCE_DEFLATE_ERROR_INVALID_POINTER	-2141847293	/* 0x80560103 */
#define SCE_DEFLATE_ERROR_INVALID_CBINFO	-2141847292	/* 0x80560104 */

#endif /* SCE_LIBDEFLT_H */
