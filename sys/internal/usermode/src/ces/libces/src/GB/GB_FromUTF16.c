/*
 * libSceCes's GB_FromUTF16.c (Sony's internal/usermode/src/ces/libces/src/GB): one UTF-16 code to
 * GB, through UCS-4. The libces libpaf links reads its UTF-16 as little-endian
 * (sceCesUtf16leToUcs4); the SDK 3.570 one uses sceCesUtf16ToUcs4.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_gb_profile.h"
#include "../UCS/ces_ucs.h"

int sceCesUtf16ToGb(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len,
		    const SceCesGbUcsProfile *prof, uint8_t *gbBuf, uint32_t gbMax, uint32_t *gbSize)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesUtf16leToUcs4(utf16buf, utf16max, utf16Len, &ucs4);
	if (ret == SCE_CES_OK)
		return sceCesUcs4ToGb(ucs4, prof, gbBuf, gbMax, gbSize);
	if (gbSize != NULL)
		*gbSize = 0;
	return ret;
}
