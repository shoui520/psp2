#include "deflt.h"
#include <stddef.h>
#include <libdeflt.h>

int sceZlibIsValid(const void *pSrc)
{
	const unsigned char *p = pSrc;

	return ((p[0] << 8) + p[1]) % 31 == 0;
}

int sceZlibGetInfo(const void *pSrcZlib, unsigned char *pbCmf,
		   unsigned char *pbFlg, unsigned int *puiDictId,
		   const void **ppvData)
{
	const unsigned char *p = pSrcZlib;
	unsigned char cmf = p[0], flg = p[1];

	if (((cmf << 8) + flg) % 31)
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	if (pbCmf)
		*pbCmf = cmf;
	if (pbFlg)
		*pbFlg = flg;
	if (flg & 0x20) {
		if (puiDictId)
			*puiDictId = __builtin_rev(*(const unsigned int *)(p + 2));
		p += 6;
	} else {
		p += 2;
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
		*puiAdler32 = __builtin_rev(*(const unsigned int *)pNext);
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
