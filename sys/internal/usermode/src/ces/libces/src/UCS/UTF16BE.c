/*
 * libSceCes's UTF16BE.c (Sony's internal/usermode/src/ces/libces/src/UCS/UTF16BE.c, the SDK's
 * static library, built with SNC): Big-endian UTF-16, one character at a time. SNC's linker
 * keeps only the routines a module calls; the tree has the ones libpaf calls. Names of the
 * routines, their parameters and locals are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_ucs.h"

#define UTF16_IS_SURROGATE(c)		((c) >= 0xd800 && (c) <= 0xdfff)
#define UTF16_IS_LOW_SURROGATE(c)	((c) >= 0xdc00 && (c) <= 0xdfff)

/* one character, big-endian: a code unit, or a surrogate pair; a lone surrogate is illegal */
static __attribute__((noinline)) int _cesUtf16beToUcs4(const uint16_t *utf16be, uint32_t utf16max, uint32_t *utf16size,
			   uint32_t *ucs4)
{
	const uint8_t *utf16;
	uint32_t code;

	if (utf16be == NULL || utf16max == 0) {
		if (utf16size != NULL)
			*utf16size = 0;
		if (ucs4 != NULL)
			*ucs4 = 0;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	utf16 = (const uint8_t *)utf16be;
	code = utf16[0] << 8 | utf16[1];
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
			code2 = utf16[2] << 8 | utf16[3];
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

int sceCesUtf16beToUcs4(const uint16_t *utf16be, uint32_t utf16max, uint32_t *utf16size,
		      uint32_t *ucs4)
{
	int ret;

	ret = _cesUtf16beToUcs4(utf16be, utf16max, utf16size, ucs4);
	if (ucs4 == NULL) {
		if (ret == SCE_CES_OK)
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	return ret;
}
