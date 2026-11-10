/*
 * libSceCes's UTF16BE_UTF8.c (Sony's internal/usermode/src/ces/libces/src/UCS/UTF16BE_UTF8.c,
 * the SDK's static library, built with SNC): one character from big-endian UTF-16 to UTF-8 and
 * back, with the code point between them. SNC's linker keeps only the routines a module calls;
 * the tree has the one libpaf calls. Names of the routines, their parameters and locals are
 * Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_ucs.h"

int sceCesUtf16beToUcsUtf8(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
			   uint32_t *ucsCode, uint8_t *utf8buf, uint32_t utf8max,
			   uint32_t *utf8Size)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesUtf16beToUcs4(utf16buf, utf16max, utf16Size, &ucs4);
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
