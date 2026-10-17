/*
 * SceKernelUtils: zlib (RFC 1950) and gzip (RFC 1952) headers around the
 * raw deflate decoder (ksceDeflateDecompress).
 */

#include <stdint.h>
#include <psp2kern/types.h>
#include <psp2kern/kernel/utils.h>
#include <kernel/error.h>

#define ZLIB_CM_DEFLATE		8
#define ZLIB_FLG_FDICT		0x20

#define GZIP_MAGIC		0x8b1f		/* 1f 8b, little endian */
#define GZIP_CM_DEFLATE		8
#define GZIP_FLG_FHCRC		0x02
#define GZIP_FLG_FEXTRA		0x04
#define GZIP_FLG_FNAME		0x08
#define GZIP_FLG_FCOMMENT	0x10

/* the zlib header; the dictionary id and the trailer are big endian */
typedef struct ZlibHeader {
	SceUInt8 cmf;
	SceUInt8 flg;
	SceUInt32 dictId;			/* only with FDICT */
} __attribute__((packed)) ZlibHeader;

typedef struct GzipHeader {
	SceUInt16 magic;
	SceUInt8 cm;
	SceUInt8 flg;
	SceUInt32 mtime;
	SceUInt8 xfl;
	SceUInt8 os;
	SceUInt16 xlen;				/* only with FEXTRA */
} __attribute__((packed)) GzipHeader;

/* the gzip trailer */
typedef struct GzipTrailer {
	SceUInt32 crc32;
	SceUInt32 isize;
} GzipTrailer;

int ksceZlibGetInfo(const void *src, unsigned char *cmf, unsigned char *flg,
		    unsigned int *dictid, const void **data)
{
	const ZlibHeader *header = src;
	SceUInt8 method = header->cmf, flags = header->flg;

	/* CMF and FLG as a big-endian halfword are a multiple of 31 */
	if (((SceUInt32)method << 8 | flags) % 31 != 0)
		return SCE_KERNEL_ERROR_ERROR;
	if (cmf != NULL)
		*cmf = method;
	if (flg != NULL)
		*flg = flags;
	if (flags & ZLIB_FLG_FDICT) {
		if (dictid != NULL)
			*dictid = __builtin_bswap32(header->dictId);
		src = (const SceUInt8 *)src + 6;
	} else {
		src = (const SceUInt8 *)src + 2;
	}
	if (data != NULL)
		*data = src;
	return 0;
}

int ksceZlibDecompress(void *dst, SceSize dst_size, const void *src, uint32_t *adler32)
{
	unsigned char cmf, flg;
	const void *data, *next;
	int ret;

	ret = ksceZlibGetInfo(src, &cmf, &flg, NULL, &data);
	if (ret < 0)
		return ret;
	if ((cmf & 0xf) != ZLIB_CM_DEFLATE || (flg & ZLIB_FLG_FDICT))
		return SCE_KERNEL_ERROR_ERROR;
	ret = ksceDeflateDecompress(dst, dst_size, data, &next);
	if (ret >= 0 && adler32 != NULL)
		*adler32 = __builtin_bswap32(*(const SceUInt32 *)next);
	return ret;
}

const void *ksceZlibGetCompressedData(const void *src)
{
	const void *data;

	if (ksceZlibGetInfo(src, NULL, NULL, NULL, &data) < 0)
		return NULL;
	return data;
}

int ksceGzipIsValid(const void *src)
{
	return ((const GzipHeader *)src)->magic == GZIP_MAGIC;
}

int ksceGzipGetInfo(const void *src, const void **extra, const char **name,
		    const char **comment, unsigned short *crc, const void **data)
{
	const GzipHeader *header = src;
	const SceUInt8 *p;
	SceUInt8 flags;

	if (extra != NULL)
		*extra = NULL;
	if (name != NULL)
		*name = NULL;
	if (comment != NULL)
		*comment = NULL;
	if (crc != NULL)
		*crc = 0;
	if (data != NULL)
		*data = NULL;
	if (src == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	p = (const SceUInt8 *)src + 10;
	if (header->magic != GZIP_MAGIC)
		return SCE_KERNEL_ERROR_ERROR;
	flags = header->flg;
	if (flags & GZIP_FLG_FEXTRA) {
		/* the extra field's start is its length; Sony skips only the field */
		if (extra != NULL)
			*extra = p;
		p += header->xlen;
	}
	if (flags & GZIP_FLG_FNAME) {
		if (name != NULL)
			*name = (const char *)p;
		while (*p != '\0')
			p++;
		p++;
	}
	if (flags & GZIP_FLG_FCOMMENT) {
		if (comment != NULL)
			*comment = (const char *)p;
		while (*p != '\0')
			p++;
		p++;
	}
	if (flags & GZIP_FLG_FHCRC) {
		if (crc != NULL)
			*crc = p[0] + (p[1] << 8);
		p += 2;
	}
	if (data != NULL)
		*data = p;
	return 0;
}

int ksceGzipDecompress(void *dst, SceSize dst_size, const void *src, uint32_t *crc32)
{
	const GzipTrailer *trailer;
	const void *data;
	int ret;

	ret = ksceGzipGetInfo(src, NULL, NULL, NULL, NULL, &data);
	if (ret < 0)
		return ret;
	if (((const GzipHeader *)src)->cm != GZIP_CM_DEFLATE)
		return SCE_KERNEL_ERROR_ERROR;
	ret = ksceDeflateDecompress(dst, dst_size, data, (const void **)&trailer);
	if (ret < 0)
		return ret;
	if (trailer->isize != (SceUInt32)ret)
		return SCE_KERNEL_ERROR_ERROR;
	if (crc32 != NULL)
		*crc32 = trailer->crc32;
	return ret;
}

const char *ksceGzipGetName(const void *src)
{
	const char *name;

	if (ksceGzipGetInfo(src, NULL, &name, NULL, NULL, NULL) < 0)
		return NULL;
	return name;
}

const char *ksceGzipGetComment(const void *src)
{
	const char *comment;

	if (ksceGzipGetInfo(src, NULL, NULL, &comment, NULL, NULL) < 0)
		return NULL;
	return comment;
}

const void *ksceGzipGetCompressedData(const void *src)
{
	const void *data;

	if (ksceGzipGetInfo(src, NULL, NULL, NULL, NULL, &data) < 0)
		return NULL;
	return data;
}
