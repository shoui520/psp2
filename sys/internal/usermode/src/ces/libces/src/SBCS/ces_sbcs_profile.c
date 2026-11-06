/*
 * libSceCes's ces_sbcs_profile.c (Sony's internal/usermode/src/ces/libces/src/SBCS): a
 * single-byte character set's conversions as an MBCS profile's, one byte a code.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_sbcs_profile.h"

int sceCesSbcsToUcs4(const SceCesSbcsUcsProfile *prof, const uint8_t *sbcsBuf, uint32_t sbcsMax,
		     uint32_t *sbcsSize, uint32_t *ucs4)
{
	if (prof == NULL || sbcsBuf == NULL || sbcsMax == 0) {
		if (sbcsSize != NULL)
			*sbcsSize = 0;
		if (ucs4 != NULL)
			*ucs4 = 0;
		if (prof == NULL)
			return SCE_CES_ERROR_INVALID_PROFILE;
		return SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	if (sbcsSize != NULL)
		*sbcsSize = 1;
	return sceCesSbcToUcs4(prof, *sbcsBuf, ucs4);
}

int sceCesUcs4ToSbcs(uint32_t ucs4, const SceCesSbcsUcsProfile *prof, uint8_t *sbcsBuf,
		     uint32_t sbcsMax, uint32_t *sbcsSize)
{
	int ret;
	uint8_t sbc;

	ret = sceCesUcs4ToSbc(ucs4, prof, &sbc);
	if (ret != SCE_CES_OK) {
		if (sbcsSize != NULL)
			*sbcsSize = 0;
		return ret;
	}
	if (sbcsSize != NULL)
		*sbcsSize = 1;
	if (sbcsBuf == NULL)
		return SCE_CES_ERROR_INVALID_DST_BUFFER;
	if (sbcsMax == 0)
		return SCE_CES_ERROR_DST_BUFFER_END;
	*sbcsBuf = sbc;
	return ret;
}
