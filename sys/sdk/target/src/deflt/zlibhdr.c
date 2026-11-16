/*
 * zlibhdr.c: the RFC 1950 (zlib) header of the SDK's libSceDeflt (zlibhdr.o, built by Sony with
 * GCC 4.8 -Os; the names are its DWARF's).
 */

#include "deflt.h"
#include <stddef.h>
#include <libdeflt.h>

/*
 * The header check (CMF * 256 + FLG is a multiple of 31), reduced by hand: 256 is 8 modulo 31,
 * and (us * 0x1085) >> 17 is us / 31 for every us up to 255 * 8 + 255. Non-zero for a bad
 * header.
 */
static inline int isInvalidHeader(const unsigned char *p)
{
	unsigned int us = (p[0] << 3) + p[1];
	unsigned int d31 = (us * 0x1085) >> 17;

	return us - d31 * 31;
}

static inline unsigned int read32BE(const void *p0)
{
	const unsigned int *p = p0;

#ifdef __SNC__
	return __builtin_rev(*p);
#else
	return __builtin_bswap32(*p);
#endif
}

int sceZlibIsValid(const void *pSrc)
{
	return !isInvalidHeader(pSrc);
}

int sceZlibGetInfo(const void *pSrcZlib, unsigned char *pbCmf,
		   unsigned char *pbFlg, unsigned int *puiDictId,
		   const void **ppvData)
{
	const unsigned char *p = pSrcZlib;
	unsigned char cmf = p[0], flg = p[1];

	if (isInvalidHeader(p))
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	if (pbCmf)
		*pbCmf = cmf;
	if (pbFlg)
		*pbFlg = flg;
	if (!(flg & 0x20)) {
		p += 2;
	} else {
		if (puiDictId)
			*puiDictId = read32BE(p + 2);
		p += 6;
	}
	if (ppvData)
		*ppvData = p;
	return 0;
}

int sceZlibDecompress(void *pDst, unsigned int uiBufSize, const void *pSrcZlib,
		      unsigned int *puiAdler32)
{
	unsigned char cmf, flg;
	const void *pDeflate, *pNext;
	int res;

	res = sceZlibGetInfo(pSrcZlib, &cmf, &flg, NULL, &pDeflate);
	if (res < 0)
		return res;
	if ((cmf & 0x0f) != 8 || (flg & 0x20))
		return SCE_DEFLATE_ERROR_NOT_SUPPORTED;
	res = sceDeflateDecompress(pDst, uiBufSize, pDeflate, &pNext);
	if (res >= 0 && puiAdler32)
		*puiAdler32 = read32BE(pNext);
	return res;
}

const void *sceZlibGetCompressedData(const void *pSrcZlib)
{
	const void *pvCompData;
	int res;

	res = sceZlibGetInfo(pSrcZlib, NULL, NULL, NULL, &pvCompData);
	if (res < 0)
		return NULL;
	return pvCompData;
}
