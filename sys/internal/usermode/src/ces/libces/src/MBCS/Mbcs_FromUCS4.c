/*
 * libSceCes's Mbcs_FromUCS4.c (Sony's internal/usermode/src/ces/libces/src/MBCS): one UCS-4 code
 * to a multi-byte character set's code, by the profile's own conversion.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_mbcs_profile.h"

int sceCesUcs4ToMbc(uint32_t ucs4, const SceCesMbcsUcsProfile *prof, uint8_t *mbcsBuf,
		    uint32_t mbcsMax, uint32_t *mbcsSize)
{
	if (prof != NULL && prof->ucs4_to_mbcs != NULL)
		return prof->ucs4_to_mbcs(ucs4, prof, mbcsBuf, mbcsMax, mbcsSize);
	{
		int ret = _SCE_CES_UCS4_CHECK_CODE(ucs4);

		if (mbcsSize != NULL)
			*mbcsSize = 0;
		if (ret != SCE_CES_OK)
			return ret;
		return SCE_CES_ERROR_INVALID_PROFILE;
	}
}
