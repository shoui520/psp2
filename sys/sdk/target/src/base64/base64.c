/*
 * libSceBase64 (the SDK's static library, base64.o): base64 encoding and decoding.
 */
#include <libbase64.h>
#include <stddef.h>

#define X	-1	/* not a base64 character */

static const char s_base64encTbl[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* each character's 6 bits; '=' decodes as 0 */
static const signed char s_base64decTbl[256] = {
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  62, X,  X,  X,  63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, X,  X,  X,  0,  X,  X,
	X,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, X,  X,  X,  X,  X,
	X,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
	X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
};

/* encodes uiSiz bytes, padded with '=' and NUL-terminated; returns the characters written */
SceInt32 sceBase64Encoder(char *pDst, const void *pSrc, SceUInt32 uiSiz)
{
	const SceUChar8 *src = pSrc;
	SceInt32 len = 0;
	SceUInt32 bits;

	if (pDst == NULL || pSrc == NULL)
		return SCE_BASE64_ERROR_INVALID_VALUE;
	while (uiSiz != 0) {
		bits = src[0] << 16;
		if (uiSiz > 1) {
			bits |= src[1] << 8;
			if (uiSiz > 2) {
				bits |= src[2];
				pDst[0] = s_base64encTbl[bits >> 18];
				pDst[1] = s_base64encTbl[(bits >> 12) & 0x3f];
				pDst[2] = s_base64encTbl[(bits >> 6) & 0x3f];
				pDst[3] = s_base64encTbl[bits & 0x3f];
				pDst += 4;
				src += 3;
				uiSiz -= 3;
				len += 4;
				continue;
			}
			pDst[0] = s_base64encTbl[bits >> 18];
			pDst[1] = s_base64encTbl[(bits >> 12) & 0x3f];
			pDst[2] = s_base64encTbl[(bits >> 6) & 0x3f];
		} else {
			pDst[0] = s_base64encTbl[bits >> 18];
			pDst[1] = s_base64encTbl[(bits >> 12) & 0x3f];
			pDst[2] = '=';
		}
		pDst[3] = '=';
		pDst += 4;
		len += 4;
		break;
	}
	*pDst = '\0';
	return len;
}

/* decodes whole groups of four characters up to the first '=', NUL-terminated; returns the bytes */
SceInt32 sceBase64Decoder(void *pDst, const char *pSrc, SceUInt32 uiSiz)
{
	SceUChar8 *dst = pDst;
	const SceChar8 *src = (const SceChar8 *)pSrc;	/* signed: bytes from 0x80 index before the table */
	SceInt32 len = 0;
	SceUInt32 bits;
	int c;

	if (pSrc == NULL || pDst == NULL)
		return SCE_BASE64_ERROR_INVALID_VALUE;
	for (; uiSiz >= 4; uiSiz -= 4, src += 4) {
		c = s_base64decTbl[src[0]];
		if (c < 0)
			return SCE_BASE64_ERROR_INVALID_FORMAT;
		bits = c << 18;
		c = s_base64decTbl[src[1]];
		if (c < 0)
			return SCE_BASE64_ERROR_INVALID_FORMAT;
		bits |= c << 12;
		*dst++ = bits >> 16;
		len++;
		if (src[2] == '=')
			break;
		c = s_base64decTbl[src[2]];
		if (c < 0)
			return SCE_BASE64_ERROR_INVALID_FORMAT;
		bits |= c << 6;
		*dst++ = bits >> 8;
		len++;
		if (src[3] == '=')
			break;
		c = s_base64decTbl[src[3]];
		if (c < 0)
			return SCE_BASE64_ERROR_INVALID_FORMAT;
		bits |= c;
		*dst++ = bits;
		len++;
	}
	*dst = '\0';
	return len;
}
