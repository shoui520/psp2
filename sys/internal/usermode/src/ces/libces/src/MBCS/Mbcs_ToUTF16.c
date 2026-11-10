/*
 * libSceCes's Mbcs_ToUTF16.c (Sony's internal/usermode/src/ces/libces/src/MBCS): one code of a
 * multi-byte character set to UTF-16, through UCS-4.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_mbcs_profile.h"
#include "../UCS/ces_ucs.h"

int sceCesMbcToUtf16(const SceCesMbcsUcsProfile *prof, const uint8_t *mbcsBuf, uint32_t mbcsMax,
		     uint32_t *mbcsSize, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesMbcToUcs4(prof, mbcsBuf, mbcsMax, mbcsSize, &ucs4);
	if (ret == SCE_CES_OK)
		return sceCesUcs4ToUtf16(ucs4, utf16buf, utf16max, utf16Len);
	if (utf16Len != NULL)
		*utf16Len = 0;
	return ret;
}
