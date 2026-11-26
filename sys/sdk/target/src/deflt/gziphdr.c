/*
 * gziphdr.c: the RFC 1952 (gzip) header of the SDK's libSceDeflt (gziphdr.o, built by Sony with
 * GCC 4.8 -Os; the names are the SDK's).
 */

#include "deflt.h"
#include <stddef.h>
#include <stdint.h>
#include <libdeflt.h>

#define GZIP_MAGIC		0x8b1f
#define GZIP_CM_DEFLATE		8
#define GZIP_FLG_FHCRC		0x02
#define GZIP_FLG_FEXTRA		0x04
#define GZIP_FLG_FNAME		0x08
#define GZIP_FLG_FCOMMENT	0x10

typedef struct GzipHeader {
	uint16_t magic;
	uint8_t cm;
	uint8_t flg;
	uint32_t mtime;
	uint8_t xfl;
	uint8_t os;
	uint16_t xlen;
} __attribute__((packed)) GzipHeader;

typedef struct GzipTrailer {
	uint32_t crc32;
	uint32_t isize;
} GzipTrailer;

int sceGzipIsValid(const void *pSrcGzip)
{
	return ((const GzipHeader *)pSrcGzip)->magic == GZIP_MAGIC;
}

int sceGzipGetInfo(const void *pSrcGzip, const void **ppvExtra,
		   const char **ppszName, const char **ppszComment,
		   unsigned short *pusCrc, const void **ppvData)
{
	const GzipHeader *header = pSrcGzip;
	const uint8_t *p;
	uint8_t flags;

	if (ppvExtra != NULL)
		*ppvExtra = NULL;
	if (ppszName != NULL)
		*ppszName = NULL;
	if (ppszComment != NULL)
		*ppszComment = NULL;
	if (pusCrc != NULL)
		*pusCrc = 0;
	if (ppvData != NULL)
		*ppvData = NULL;
	if (pSrcGzip == NULL)
		return SCE_DEFLATE_ERROR_INVALID_POINTER;
	p = (const uint8_t *)&header->xlen;
	if (header->magic != GZIP_MAGIC)
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	flags = header->flg;
	if (flags & GZIP_FLG_FEXTRA) {
		unsigned int length = header->xlen;

		if (ppvExtra != NULL)
			*ppvExtra = p;
		p += length;
	}
	if (flags & GZIP_FLG_FNAME) {
		if (ppszName != NULL)
			*ppszName = (const char *)p;
		while (*p != '\0')
			p++;
		p++;
	}
	if (flags & GZIP_FLG_FCOMMENT) {
		if (ppszComment != NULL)
			*ppszComment = (const char *)p;
		while (*p != '\0')
			p++;
		p++;
	}
	if (flags & GZIP_FLG_FHCRC) {
		if (pusCrc != NULL)
			*pusCrc = *(const uint16_t *)p;
		p += 2;
	}
	if (ppvData != NULL)
		*ppvData = p;
	return 0;
}

int sceGzipDecompress(void *pDst, unsigned int uiBufSize,
		      const void *pSrcGzip, unsigned int *puiCrc32)
{
	const void *pDeflate;
	const GzipTrailer *trailer;
	int res;

	res = sceGzipGetInfo(pSrcGzip, NULL, NULL, NULL, NULL, &pDeflate);
	if (res < 0)
		return res;
	if (((const GzipHeader *)pSrcGzip)->cm != GZIP_CM_DEFLATE)
		return SCE_DEFLATE_ERROR_NOT_SUPPORTED;
	res = sceDeflateDecompress(pDst, uiBufSize, pDeflate,
				   (const void **)&trailer);
	if (res < 0)
		return res;
	if ((unsigned int)res != trailer->isize)
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	if (puiCrc32 != NULL)
		*puiCrc32 = trailer->crc32;
	return res;
}

const char *sceGzipGetName(const void *pSrcGzip)
{
	const char *name;

	sceGzipGetInfo(pSrcGzip, NULL, &name, NULL, NULL, NULL);
	return name;
}

const char *sceGzipGetComment(const void *pSrcGzip)
{
	const char *comment;

	sceGzipGetInfo(pSrcGzip, NULL, NULL, &comment, NULL, NULL);
	return comment;
}

const void *sceGzipGetCompressedData(const void *pSrcGzip)
{
	const void *data;

	sceGzipGetInfo(pSrcGzip, NULL, NULL, NULL, NULL, &data);
	return data;
}
