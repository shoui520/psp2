#ifndef SCE_DEFLT_PRIVATE_H
#define SCE_DEFLT_PRIVATE_H

int sceZlibIsValid(const void *pSrc);
int sceZlibGetInfo(const void *pSrcZlib, unsigned char *pbCmf,
		   unsigned char *pbFlg, unsigned int *puiDictId,
		   const void **ppvData);
int sceZlibDecompress(void *pDst, unsigned int uiBufSize, const void *pSrcZlib,
		      unsigned int *puiAdler32);
const void *sceZlibGetCompressedData(const void *pSrcZlib);
int sceDeflateDecompress(void *pDst, unsigned int uiBufSize,
			const void *pSrcDeflate, const void **ppNext);

#endif
