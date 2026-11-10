/*
 * libSceCes's Mbcs_ToUCS4.c (Sony's internal/usermode/src/ces/libces/src/MBCS): one code of a
 * multi-byte character set to UCS-4, by the profile's own conversion.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_mbcs_profile.h"

int sceCesMbcToUcs4(const SceCesMbcsUcsProfile *prof, const uint8_t *mbcsBuf, uint32_t mbcsMax,
		    uint32_t *mbcsSize, uint32_t *ucs4)
{
	if (prof == NULL || prof->mbcs_to_ucs4 == NULL) {
		if (mbcsSize != NULL)
			*mbcsSize = 0;
		if (ucs4 != NULL)
			*ucs4 = 0;
		return SCE_CES_ERROR_INVALID_PROFILE;
	}
	return prof->mbcs_to_ucs4(prof, mbcsBuf, mbcsMax, mbcsSize, ucs4);
}
