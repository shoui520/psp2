/*
 * libSceCes's SJis_FromUTF16.c (Sony's internal/usermode/src/ces/libces/src/JIS): one UTF-16
 * code to Shift_JIS, through UCS-4. The libces libpaf links reads its UTF-16 as little-endian
 * (sceCesUtf16leToUcs4); the SDK 3.570 one uses sceCesUtf16ToUcs4.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"
#include "../UCS/ces_ucs.h"

int sceCesUtf16ToSJis(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len,
		      const SceCesJisUcsProfile *prof, uint8_t *sjisBuf, uint32_t sjisMax,
		      uint32_t *sjisSize)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesUtf16leToUcs4(utf16buf, utf16max, utf16Len, &ucs4);
	if (ret == SCE_CES_OK)
		return sceCesUcs4ToSJis(ucs4, prof, sjisBuf, sjisMax, sjisSize);
	if (sjisSize != NULL)
		*sjisSize = 0;
	return ret;
}
