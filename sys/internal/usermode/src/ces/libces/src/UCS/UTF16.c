/*
 * libSceCes's UTF16.c (Sony's internal/usermode/src/ces/libces/src/UCS/UTF16.c, the SDK's static
 * library, built with SNC): UTF-16 in the system's byte order, one character at a time. SNC's
 * linker keeps only the routines a module calls; the tree has the ones libpaf calls. Names of
 * the routines, their parameters and locals are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_ucs.h"

#define UTF16_IS_SURROGATE(c)		((c) >= 0xd800 && (c) <= 0xdfff)
#define UTF16_IS_LOW_SURROGATE(c)	((c) >= 0xdc00 && (c) <= 0xdfff)

/* one character: a code unit, or a surrogate pair; a lone surrogate is illegal */
static __attribute__((noinline)) int _cesUtf16ToUcs4(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
			   uint32_t *ucs4)
{
	uint32_t code;

	if (utf16buf == NULL || utf16max == 0) {
		if (utf16size != NULL)
			*utf16size = 0;
		if (ucs4 != NULL)
			*ucs4 = 0;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	code = utf16buf[0];
	if (!UTF16_IS_SURROGATE(code)) {
		if (ucs4 != NULL)
			*ucs4 = code;
		if (utf16size != NULL)
			*utf16size = 1;
		return SCE_CES_OK;
	}
	if (code < 0xdc00) {
		uint32_t code2;

		if (utf16max >= 2) {
			code2 = utf16buf[1];
			if (UTF16_IS_LOW_SURROGATE(code2)) {
				if (ucs4 != NULL)
					*ucs4 = ((code - 0xd800) << 10) + (code2 - 0xdc00) + 0x10000;
				if (utf16size != NULL)
					*utf16size = 2;
				return SCE_CES_OK;
			}
		} else {
			if (ucs4 != NULL)
				*ucs4 = 0;
			if (utf16size != NULL)
				*utf16size = 2;
			return SCE_CES_ERROR_SRC_BUFFER_END;
		}
	}
	if (ucs4 != NULL)
		*ucs4 = code;
	if (utf16size != NULL)
		*utf16size = 1;
	return SCE_CES_ERROR_ILLEGAL_CODE;
}

int sceCesUcs4GetUtf16CodeLen(uint32_t ucs4, uint32_t *utf16Count)
{
	if (ucs4 <= 0xffff) {
		int ret;

		ret = UTF16_IS_SURROGATE(ucs4) ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;
		if (utf16Count != NULL) {
			*utf16Count = 1;
			return ret;
		}
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	if (ucs4 <= 0x10ffff) {
		if (utf16Count != NULL)
			*utf16Count = 2;
		return SCE_CES_OK;
	}
	if (ucs4 <= 0x7fffffff) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_OUT_OF_CODE_RANGE;
	}
	if (utf16Count != NULL)
		*utf16Count = 0;
	return SCE_CES_ERROR_INVALID_ENCODE;
}

int sceCesUcs4ToUtf16(uint32_t ucs4, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Count)
{
	int ret;

	if (utf16buf != NULL && utf16max != 0) {
		if (ucs4 <= 0xffff) {
			if (utf16Count != NULL)
				*utf16Count = 1;
			utf16buf[0] = ucs4;
			if (!UTF16_IS_SURROGATE(ucs4))
				return SCE_CES_OK;
			return SCE_CES_ERROR_ILLEGAL_CODE;
		}
		if (ucs4 <= 0x10ffff) {
			if (utf16Count != NULL)
				*utf16Count = 2;
			if (utf16max >= 2) {
				ucs4 -= 0x10000;
				utf16buf[0] = 0xd800 + ((ucs4 >> 10) & 0x3ff);
				utf16buf[1] = 0xdc00 + (ucs4 & 0x3ff);
				return SCE_CES_OK;
			}
			return SCE_CES_ERROR_DST_BUFFER_END;
		}
		if (ucs4 <= 0x7fffffff) {
			if (utf16Count != NULL)
				*utf16Count = 0;
			return SCE_CES_ERROR_OUT_OF_CODE_RANGE;
		}
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_ENCODE;
	}
	/* no room: what the character would take */
	ret = sceCesUcs4GetUtf16CodeLen(ucs4, utf16Count);
	switch (ret) {
	case SCE_CES_OK:
	case SCE_CES_ERROR_INVALID_PARAMETER:
		ret = utf16buf != NULL ? SCE_CES_ERROR_DST_BUFFER_END : SCE_CES_ERROR_INVALID_DST_BUFFER;
		break;
	case SCE_CES_ERROR_ILLEGAL_CODE:
		if (utf16buf != NULL)
			ret = SCE_CES_ERROR_DST_BUFFER_END;
		break;
	}
	return ret;
}

int sceCesUtf16ToUcs4(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
		      uint32_t *ucs4)
{
	int ret;

	ret = _cesUtf16ToUcs4(utf16buf, utf16max, utf16size, ucs4);
	if (ucs4 == NULL) {
		if (ret == SCE_CES_OK)
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	return ret;
}

int sceCesUtf16CheckCode(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size)
{
	return _cesUtf16ToUcs4(utf16buf, utf16max, utf16size, NULL);
}

int sceCesUtf16ToCopyAndUcs(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
			    uint16_t *copyBuf, uint32_t copyMax, uint32_t *copyLen, uint32_t *ucs4)
{
	uint32_t n;
	int ret;

	ret = _cesUtf16ToUcs4(utf16buf, utf16max, &n, ucs4);
	if (n != 0) {
		if (copyBuf != NULL) {
			if (copyMax < n) {
				ret = SCE_CES_ERROR_DST_BUFFER_END;
			} else {
				copyBuf[0] = utf16buf[0];
				if (n == 2)
					copyBuf[1] = utf16buf[1];
			}
		} else {
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
		}
	}
	if (utf16size != NULL)
		*utf16size = n;
	if (copyLen != NULL)
		*copyLen = n;
	return ret;
}

int sceCesUtf16ToCopy(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
		      uint16_t *copyBuf, uint32_t copyMax, uint32_t *copyLen)
{
	uint32_t n;
	int ret;

	ret = _cesUtf16ToUcs4(utf16buf, utf16max, &n, NULL);
	if (n != 0) {
		if (copyBuf != NULL) {
			if (copyMax < n) {
				ret = SCE_CES_ERROR_DST_BUFFER_END;
			} else {
				copyBuf[0] = utf16buf[0];
				if (n == 2)
					copyBuf[1] = utf16buf[1];
			}
		} else {
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
		}
	}
	if (utf16size != NULL)
		*utf16size = n;
	if (copyLen != NULL)
		*copyLen = n;
	return ret;
}

int sceCesUcs2GetUtf16CodeLen(uint16_t ucs2, uint32_t *utf16Count)
{
	int ret = UTF16_IS_SURROGATE(ucs2) ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;

	if (utf16Count == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	*utf16Count = 1;
	return ret;
}

int sceCesUcs2ToUtf16(uint16_t ucs2, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Count)
{
	return sceCesUcs4ToUtf16(ucs2, utf16buf, utf16max, utf16Count);
}

int sceCesUtf16ToUcs2(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
		      uint16_t *ucs2)
{
	uint32_t ucs4;
	int ret;

	ret = _cesUtf16ToUcs4(utf16buf, utf16max, utf16size, &ucs4);
	if (ucs4 > 0xffff) {
		if (ucs2 != NULL)
			*ucs2 = 0;
		ret = SCE_CES_ERROR_OUT_OF_CODE_RANGE;
	} else if (ucs2 != NULL) {
		*ucs2 = (uint16_t)ucs4;
	} else if (ret == SCE_CES_OK) {
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	return ret;
}

int sceCesUtf32GetUtf16CodeLen(uint32_t utf32, uint32_t *utf16Count)
{
	if (utf32 > 0x10ffff) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_ENCODE;
	}
	if (utf32 > 0xffff) {
		if (utf16Count != NULL)
			*utf16Count = 2;
		return SCE_CES_OK;
	}
	if (utf16Count != NULL) {
		*utf16Count = 1;
		return UTF16_IS_SURROGATE(utf32) ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK;
	}
	return SCE_CES_ERROR_INVALID_PARAMETER;
}

int sceCesUtf32beGetUtf16CodeLen(const uint32_t *utf32be, uint32_t *utf16Count)
{
	const uint8_t *p;
	uint32_t v;

	if (utf32be == NULL) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	p = (const uint8_t *)utf32be;
	v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
	return sceCesUtf32GetUtf16CodeLen(v, utf16Count);
}

int sceCesUtf32leGetUtf16CodeLen(const uint32_t *utf32le, uint32_t *utf16Count)
{
	const uint8_t *p;
	uint32_t v;

	if (utf32le == NULL) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	p = (const uint8_t *)utf32le;
	v = ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
	return sceCesUtf32GetUtf16CodeLen(v, utf16Count);
}

int sceCesUtf32ToUtf16(uint32_t utf32, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Count)
{
	if (utf32 > 0x10ffff) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_ENCODE;
	}
	return sceCesUcs4ToUtf16(utf32, utf16buf, utf16max, utf16Count);
}

int sceCesUtf32beToUtf16(const uint32_t *utf32be, uint16_t *utf16buf, uint32_t utf16max,
			 uint32_t *utf16Count)
{
	const uint8_t *p;
	uint32_t v;

	if (utf32be == NULL) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	p = (const uint8_t *)utf32be;
	v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
	if (v > 0x10ffff) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_ENCODE;
	}
	return sceCesUcs4ToUtf16(v, utf16buf, utf16max, utf16Count);
}

int sceCesUtf32leToUtf16(const uint32_t *utf32le, uint16_t *utf16buf, uint32_t utf16max,
			 uint32_t *utf16Count)
{
	const uint8_t *p;
	uint32_t v;

	if (utf32le == NULL) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	p = (const uint8_t *)utf32le;
	v = ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
	if (v > 0x10ffff) {
		if (utf16Count != NULL)
			*utf16Count = 0;
		return SCE_CES_ERROR_INVALID_ENCODE;
	}
	return sceCesUcs4ToUtf16(v, utf16buf, utf16max, utf16Count);
}

int sceCesUtf16ToUtf32(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
		       uint32_t *utf32)
{
	int ret;

	ret = _cesUtf16ToUcs4(utf16buf, utf16max, utf16size, utf32);
	if (utf32 == NULL && ret == SCE_CES_OK)
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	return ret;
}

int sceCesUtf16ToUtf32be(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
			 uint32_t *utf32be)
{
	int ret;

	ret = _cesUtf16ToUcs4(utf16buf, utf16max, utf16size, utf32be);
	if (utf32be == NULL && ret == SCE_CES_OK)
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	if (utf32be != NULL) {
		uint32_t v = *utf32be;
		*utf32be = ((v & 0xffu) << 24) | ((v & 0xff00u) << 8) |
			   ((v & 0xff0000u) >> 8) | ((v & 0xff000000u) >> 24);
	}
	return ret;
}

int sceCesUtf16ToUtf32le(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
			 uint32_t *utf32le)
{
	int ret;

	ret = _cesUtf16ToUcs4(utf16buf, utf16max, utf16size, utf32le);
	if (utf32le == NULL && ret == SCE_CES_OK)
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	return ret;
}
