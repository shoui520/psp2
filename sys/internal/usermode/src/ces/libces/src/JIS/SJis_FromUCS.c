/*
 * libSceCes's SJis_FromUCS.c (Sony's internal/usermode/src/ces/libces/src/JIS): one UCS-4 code
 * to Shift_JIS, a profile's ucs4_to_mbcs.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"

int sceCesUcs4ToSJis(uint32_t ucs4, const SceCesJisUcsProfile *prof, uint8_t *sjisBuf,
		     uint32_t sjisMax, uint32_t *sjisSize)
{
	int ret;

	if (prof != NULL) {
		uint32_t jisMenKuTen = 0;

		ret = _sceCesUcs4ToJis(ucs4, prof, &jisMenKuTen);
		if (ret == SCE_CES_OK) {
			uint8_t men = (jisMenKuTen >> 14) & 0x7f;
			uint8_t ku = (jisMenKuTen >> 7) & 0x7f;
			uint8_t ten = jisMenKuTen & 0x7f;

			if (men != 0)
				return sceCesJisToSJis(men, ku, ten, prof->types, sjisBuf, sjisMax,
						       sjisSize);
			/* a single byte, but for the C1 controls */
			if (ku <= 1 && (jisMenKuTen < 0x80 || jisMenKuTen > 0x9f)) {
				if (sjisSize != NULL)
					*sjisSize = 1;
				if (sjisBuf == NULL)
					return SCE_CES_ERROR_INVALID_DST_BUFFER;
				if (sjisMax == 0)
					return SCE_CES_ERROR_DST_BUFFER_END;
				*sjisBuf = (uint8_t)jisMenKuTen;
				return SCE_CES_OK;
			}
			ret = SCE_CES_ERROR_OUT_OF_CODE_RANGE;
		}
		if (sjisSize != NULL)
			*sjisSize = 0;
		return ret;
	}
	if (sjisSize != NULL)
		*sjisSize = 0;
	ret = _SCE_CES_UCS4_CHECK_CODE(ucs4);
	if (ret == SCE_CES_OK)
		ret = SCE_CES_ERROR_INVALID_PROFILE;
	return ret;
}
