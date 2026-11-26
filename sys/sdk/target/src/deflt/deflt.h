#ifndef SCE_DEFLT_PRIVATE_H
#define SCE_DEFLT_PRIVATE_H

#include <libdeflt.h>

int sceZlibIsValid(const void *pSrc);
int sceZlibGetInfo(const void *pSrcZlib, unsigned char *pbCmf,
		   unsigned char *pbFlg, unsigned int *puiDictId,
		   const void **ppvData);
int sceZlibDecompress(void *pDst, unsigned int uiBufSize, const void *pSrcZlib,
		      unsigned int *puiAdler32);
const void *sceZlibGetCompressedData(const void *pSrcZlib);
int sceGzipIsValid(const void *pSrcGzip);
int sceGzipGetInfo(const void *pSrcGzip, const void **ppvExtra,
		   const char **ppszName, const char **ppszComment,
		   unsigned short *pusCrc, const void **ppvData);
int sceGzipDecompress(void *pDst, unsigned int uiBufSize, const void *pSrcGzip,
		      unsigned int *puiCrc32);
const char *sceGzipGetName(const void *pSrcGzip);
const char *sceGzipGetComment(const void *pSrcGzip);
const void *sceGzipGetCompressedData(const void *pSrcGzip);
int sceDeflateDecompress(void *pDst, unsigned int uiBufSize,
			const void *pSrcDeflate, const void **ppNext);
int sceDeflateDecompressPartial(void *pDst, unsigned int uiBufSize,
				const void *pSrcDeflate, const void **ppNext,
				SceDeflatePartialInputParam *cbInfo);

#endif
