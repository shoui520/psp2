/*
 * libSceCes's UTF8.c (Sony's internal/usermode/src/ces/libces/src/UCS/UTF8.c, the SDK's static
 * library, built with SNC): UTF-8 code checks and conversions. SNC's linker keeps only the
 * routines a module calls; the tree has the decoder and the check SceCommonDialog calls. The
 * names of the routines, their parameters and the decoder's locals are Sony's (the library's
 * DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include <ces/libces_ucs.h>

/* a byte that continues a UTF-8 sequence, 0x80 to 0xbf */
#define UTF8_IS_TRAIL(c)	((int8_t)(c) < (int8_t)0xc0)

/* a sequence of codeLen bytes that bufsize cuts short: the length up to the first byte that
   cannot continue it, else the end of the buffer */
static inline int cesUtf8Truncated(const uint8_t *utf8buf, uint32_t bufsize, uint32_t codeLen,
				   uint32_t *convSize, uint32_t *ucs4)
{
	uint32_t i;

	if (ucs4 != NULL)
		*ucs4 = 0;
	for (i = 1; i < codeLen; i++) {
		if (i >= bufsize)
			break;
		if (!UTF8_IS_TRAIL(utf8buf[i])) {
			if (convSize != NULL)
				*convSize = i + 1;
			return SCE_CES_ERROR_INVALID_ENCODE;
		}
	}
	if (convSize != NULL)
		*convSize = codeLen;
	return SCE_CES_ERROR_SRC_BUFFER_END;
}

/* one character of UTF-8 (up to six bytes, as ISO 10646 had them): its length and its code; a
   code a shorter sequence could have given, or a surrogate, is illegal */
static int cesUtf8ToUcs4(const uint8_t *utf8buf, uint32_t bufsize, uint32_t *convSize, uint32_t *ucs4)
{
	const uint8_t *utf8;
	uint32_t code;
	uint8_t c1, c2, c3, c4, c5;
	int ret;

	utf8 = utf8buf;
	if (utf8buf == NULL || bufsize == 0) {
		if (convSize != NULL)
			*convSize = 0;
		if (ucs4 != NULL)
			*ucs4 = 0;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	code = *utf8;
	if (code < 0x80) {
		if (convSize != NULL)
			*convSize = 1;
		if (ucs4 != NULL)
			*ucs4 = code;
		return SCE_CES_OK;
	}
	if (code < 0xc0) {
		if (convSize != NULL)
			*convSize = 0;
		if (ucs4 != NULL)
			*ucs4 = 0;
		return SCE_CES_ERROR_INVALID_ENCODE;
	} else if (code < 0xe0) {
		if (bufsize < 2)
			return cesUtf8Truncated(utf8buf, bufsize, 2, convSize, ucs4);
		c1 = *++utf8;
		if (!UTF8_IS_TRAIL(c1))
			goto invalid;
		code = (code & 0x1f) << 6 | (c1 & 0x3f);
		if (convSize != NULL)
			*convSize = 2;
		ret = code < 0x80 ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;
	} else if (code < 0xf0) {
		if (bufsize < 3)
			return cesUtf8Truncated(utf8buf, bufsize, 3, convSize, ucs4);
		c1 = *++utf8;
		if (!UTF8_IS_TRAIL(c1))
			goto invalid;
		c2 = *++utf8;
		if (!UTF8_IS_TRAIL(c2))
			goto invalid;
		code = ((code & 0x0f) << 6 | (c1 & 0x3f)) << 6 | (c2 & 0x3f);
		if (convSize != NULL)
			*convSize = 3;
		ret = code < 0x800 || (code >= 0xd800 && code <= 0xdfff) ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;
	} else if (code < 0xf8) {
		if (bufsize < 4)
			return cesUtf8Truncated(utf8buf, bufsize, 4, convSize, ucs4);
		c1 = *++utf8;
		if (!UTF8_IS_TRAIL(c1))
			goto invalid;
		c2 = *++utf8;
		if (!UTF8_IS_TRAIL(c2))
			goto invalid;
		c3 = *++utf8;
		if (!UTF8_IS_TRAIL(c3))
			goto invalid;
		code = (((code & 0x07) << 6 | (c1 & 0x3f)) << 6 | (c2 & 0x3f)) << 6 | (c3 & 0x3f);
		if (convSize != NULL)
			*convSize = 4;
		ret = code < 0x10000 ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;
	} else if (code < 0xfc) {
		if (bufsize < 5)
			return cesUtf8Truncated(utf8buf, bufsize, 5, convSize, ucs4);
		c1 = *++utf8;
		if (!UTF8_IS_TRAIL(c1))
			goto invalid;
		c2 = *++utf8;
		if (!UTF8_IS_TRAIL(c2))
			goto invalid;
		c3 = *++utf8;
		if (!UTF8_IS_TRAIL(c3))
			goto invalid;
		c4 = *++utf8;
		if (!UTF8_IS_TRAIL(c4))
			goto invalid;
		code = ((((code & 0x03) << 6 | (c1 & 0x3f)) << 6 | (c2 & 0x3f)) << 6 | (c3 & 0x3f)) << 6
		       | (c4 & 0x3f);
		if (convSize != NULL)
			*convSize = 5;
		ret = code < 0x200000 ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;
	} else if (code < 0xfe) {
		if (bufsize < 6)
			return cesUtf8Truncated(utf8buf, bufsize, 6, convSize, ucs4);
		c1 = *++utf8;
		if (!UTF8_IS_TRAIL(c1))
			goto invalid;
		c2 = *++utf8;
		if (!UTF8_IS_TRAIL(c2))
			goto invalid;
		c3 = *++utf8;
		if (!UTF8_IS_TRAIL(c3))
			goto invalid;
		c4 = *++utf8;
		if (!UTF8_IS_TRAIL(c4))
			goto invalid;
		c5 = *++utf8;
		if (!UTF8_IS_TRAIL(c5))
			goto invalid;
		code = (((((code & 0x01) << 6 | (c1 & 0x3f)) << 6 | (c2 & 0x3f)) << 6 | (c3 & 0x3f)) << 6
			| (c4 & 0x3f)) << 6 | (c5 & 0x3f);
		if (convSize != NULL)
			*convSize = 6;
		ret = code < 0x4000000 ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;
	} else {
		goto invalid;
	}
	if (ucs4 != NULL)
		*ucs4 = code;
	return ret;

invalid:
	if (convSize != NULL)
		*convSize = utf8 - utf8buf;
	if (ucs4 != NULL)
		*ucs4 = 0;
	return SCE_CES_ERROR_INVALID_ENCODE;
}

int sceCesUtf8CheckCode(const uint8_t *utf8buf, uint32_t bufsize, uint32_t *codeSize)
{
	return cesUtf8ToUcs4(utf8buf, bufsize, codeSize, NULL);
}

int sceCesUtf8ToUcs4(const uint8_t *utf8buf, uint32_t utf8max, uint32_t *convSize, uint32_t *ucs4)
{
	int ret;

	ret = cesUtf8ToUcs4(utf8buf, utf8max, convSize, ucs4);
	if (ucs4 == NULL) {
		if (ret == SCE_CES_OK)
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	return ret;
}

/* one character of n bytes: the lead byte, then six bits in each trailing byte */
void sceCesUcs4ToUtf8_1byte(uint32_t ucs4, uint8_t *utf8)
{
	if (utf8 != NULL)
		utf8[0] = ucs4 & 0x7f;
}

void sceCesUcs4ToUtf8_2byte(uint32_t ucs4, uint8_t *utf8)
{
	if (utf8 != NULL) {
		utf8[1] = 0x80 | (ucs4 & 0x3f);
		utf8[0] = 0xc0 | ((ucs4 >> 6) & 0x1f);
	}
}

void sceCesUcs4ToUtf8_3byte(uint32_t ucs4, uint8_t *utf8)
{
	if (utf8 != NULL) {
		utf8[2] = 0x80 | (ucs4 & 0x3f);
		utf8[1] = 0x80 | ((ucs4 >> 6) & 0x3f);
		utf8[0] = 0xe0 | ((ucs4 >> 12) & 0x0f);
	}
}

void sceCesUcs4ToUtf8_4byte(uint32_t ucs4, uint8_t *utf8)
{
	if (utf8 != NULL) {
		utf8[3] = 0x80 | (ucs4 & 0x3f);
		utf8[2] = 0x80 | ((ucs4 >> 6) & 0x3f);
		utf8[1] = 0x80 | ((ucs4 >> 12) & 0x3f);
		utf8[0] = 0xf0 | ((ucs4 >> 18) & 0x07);
	}
}

void sceCesUcs4ToUtf8_5byte(uint32_t ucs4, uint8_t *utf8)
{
	if (utf8 != NULL) {
		utf8[4] = 0x80 | (ucs4 & 0x3f);
		utf8[3] = 0x80 | ((ucs4 >> 6) & 0x3f);
		utf8[2] = 0x80 | ((ucs4 >> 12) & 0x3f);
		utf8[1] = 0x80 | ((ucs4 >> 18) & 0x3f);
		utf8[0] = 0xf8 | ((ucs4 >> 24) & 0x03);
	}
}

void sceCesUcs4ToUtf8_6byte(uint32_t ucs4, uint8_t *utf8)
{
	if (utf8 != NULL) {
		utf8[5] = 0x80 | (ucs4 & 0x3f);
		utf8[4] = 0x80 | ((ucs4 >> 6) & 0x3f);
		utf8[3] = 0x80 | ((ucs4 >> 12) & 0x3f);
		utf8[2] = 0x80 | ((ucs4 >> 18) & 0x3f);
		utf8[1] = 0x80 | ((ucs4 >> 24) & 0x3f);
		utf8[0] = 0xfc | ((ucs4 >> 30) & 0x01);
	}
}

/* the bytes a character takes, or 0 for a code no UTF-8 sequence holds */
#define UTF8_CODE_LEN(ucs4) \
	((ucs4) <= 0x7f ? 1 : (ucs4) <= 0x7ff ? 2 : (ucs4) <= 0xffff ? 3 : \
	 (ucs4) <= 0x1fffff ? 4 : (ucs4) <= 0x3ffffff ? 5 : (ucs4) <= 0x7fffffff ? 6 : 0)

#define UCS4_IS_SURROGATE(c)	((c) >= 0xd800 && (c) <= 0xdfff)

/* one character into UTF-8; a buffer too short gets nothing, and n says what it needed */
int sceCesUcs4ToUtf8(uint32_t ucs4, uint8_t *utf8buf, uint32_t bufSize, uint32_t *utf8Size)
{
	int n;

	if (utf8buf != NULL && bufSize != 0) {
		if (ucs4 <= 0x7f) {
			utf8buf[0] = ucs4;
			n = 1;
		} else if (ucs4 <= 0x7ff) {
			if (bufSize >= 2) {
				sceCesUcs4ToUtf8_2byte(ucs4, utf8buf);
				n = 2;
			} else {
				n = -2;
			}
		} else if (ucs4 <= 0xffff) {
			if (bufSize >= 3) {
				sceCesUcs4ToUtf8_3byte(ucs4, utf8buf);
				n = 3;
			} else {
				n = -3;
			}
		} else if (ucs4 <= 0x1fffff) {
			if (bufSize >= 4) {
				sceCesUcs4ToUtf8_4byte(ucs4, utf8buf);
				n = 4;
			} else {
				n = -4;
			}
		} else if (ucs4 <= 0x3ffffff) {
			if (bufSize >= 5) {
				sceCesUcs4ToUtf8_5byte(ucs4, utf8buf);
				n = 5;
			} else {
				n = -5;
			}
		} else if (ucs4 <= 0x7fffffff) {
			if (bufSize >= 6) {
				sceCesUcs4ToUtf8_6byte(ucs4, utf8buf);
				n = 6;
			} else {
				n = -6;
			}
		} else {
			n = 0;
		}
	} else {
		n = UTF8_CODE_LEN(ucs4);
		if (n != 0) {
			if (utf8Size != NULL)
				*utf8Size = n;
			if (utf8buf != NULL)
				return SCE_CES_ERROR_DST_BUFFER_END;
			if (UCS4_IS_SURROGATE(ucs4))
				return SCE_CES_ERROR_ILLEGAL_CODE;
			return SCE_CES_ERROR_INVALID_DST_BUFFER;
		}
	}
	if (n > 0) {
		if (utf8Size != NULL)
			*utf8Size = n;
		if (UCS4_IS_SURROGATE(ucs4))
			return SCE_CES_ERROR_ILLEGAL_CODE;
		return SCE_CES_OK;
	}
	if (n < 0) {
		if (utf8Size != NULL)
			*utf8Size = -n;
		return SCE_CES_ERROR_DST_BUFFER_END;
	}
	if (utf8Size != NULL)
		*utf8Size = 0;
	return SCE_CES_ERROR_INVALID_ENCODE;
}
