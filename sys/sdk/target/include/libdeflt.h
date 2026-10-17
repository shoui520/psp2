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

#endif /* SCE_LIBDEFLT_H */
