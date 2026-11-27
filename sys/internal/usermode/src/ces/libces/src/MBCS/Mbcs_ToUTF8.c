/*
 * libSceCes's Mbcs_ToUTF8.c (Sony's internal/usermode/src/ces/libces/src/MBCS):
 * one code of a multi-byte character set to UTF-8, through UCS-4.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_mbcs_profile.h"
#include "../UCS/ces_ucs.h"

int sceCesMbcToUtf8(const SceCesMbcsUcsProfile *prof, const uint8_t *mbcsBuf, uint32_t mbcsMax,
		    uint32_t *mbcsSize, uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Len)
{
	uint32_t ucs4;
	int ret;

	ret = sceCesMbcToUcs4(prof, mbcsBuf, mbcsMax, mbcsSize, &ucs4);
	if (ret == SCE_CES_OK)
		return sceCesUcs4ToUtf8(ucs4, utf8buf, utf8max, utf8Len);
	if (utf8Len != NULL)
		*utf8Len = 0;
	return ret;
}
