/*
 * libSceCes's UTF32_UTF8.c (Sony's internal/usermode/src/ces/libces/src/UCS/UTF32_UTF8.c, the
 * SDK's static library, built with SNC): one character from UTF-8 to UTF-32. SNC's linker keeps
 * only the routines a module calls; the tree has the one apputil calls. Names of the routines and
 * their parameters are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_ucs.h"

/* the highest code point of UTF-32 */
#define CES_UTF32_CODE_MAX	0x10ffff

int sceCesUtf8ToUtf32(const uint8_t *utf8buf, uint32_t utf8max, uint32_t *convSize, uint32_t *utf32)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesUtf8ToUcs4(utf8buf, utf8max, convSize, &ucs4);
	if (ucs4 > CES_UTF32_CODE_MAX) {
		if (utf32 != NULL)
			*utf32 = ucs4;
		if (ret == SCE_CES_OK)
			ret = SCE_CES_ERROR_OUT_OF_CODE_RANGE;
	} else if (utf32 != NULL) {
		*utf32 = ucs4;
	} else if (ret == SCE_CES_OK) {
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	return ret;
}
