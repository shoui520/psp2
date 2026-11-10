/*
 * libSceCes's UTF16_UTF8.c (Sony's internal/usermode/src/ces/libces/src/UCS/UTF16_UTF8.c, the
 * SDK's static library, built with SNC): one character from UTF-16 to UTF-8 and back, with the
 * code point between them. SNC's linker keeps only the routines a module calls; the tree has the
 * ones libpaf calls. Names of the routines, their parameters and locals are Sony's (the
 * library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_ucs.h"

int sceCesUtf16ToUcsUtf8(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
			 uint32_t *ucsCode, uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Size)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesUtf16ToUcs4(utf16buf, utf16max, utf16Size, &ucs4);
	if (ucsCode != NULL)
		*ucsCode = ucs4;
	if (ret == SCE_CES_ERROR_INVALID_SRC_BUFFER) {
		if (utf8Size != NULL)
			*utf8Size = 0;
		return ret;
	}
	if (ret == SCE_CES_ERROR_SRC_BUFFER_END) {
		/* a surrogate pair is four bytes of UTF-8 */
		if (utf8Size != NULL)
			*utf8Size = 4;
		return ret;
	}
	return sceCesUcs4ToUtf8(ucs4, utf8buf, utf8max, utf8Size);
}

int sceCesUtf8ToUcsUtf16(const uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Size,
			 uint32_t *ucsCode, uint16_t *utf16buf, uint32_t utf16max,
			 uint32_t *utf16Count)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesUtf8ToUcs4(utf8buf, utf8max, utf8Size, &ucs4);
	if (ucsCode != NULL)
		*ucsCode = ucs4;
	if (ret == SCE_CES_OK || ret == SCE_CES_ERROR_ILLEGAL_CODE) {
		int ret2;

		ret2 = sceCesUcs4ToUtf16(ucs4, utf16buf, utf16max, utf16Count);
		if (ret == SCE_CES_OK)
			return ret2;
		if (ret2 == SCE_CES_ERROR_DST_BUFFER_END)
			return ret2;
		return ret;
	}
	if (utf16Count != NULL)
		*utf16Count = 0;
	return ret;
}
