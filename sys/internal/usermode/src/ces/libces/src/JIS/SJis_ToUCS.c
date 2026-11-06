/*
 * libSceCes's SJis_ToUCS.c (Sony's internal/usermode/src/ces/libces/src/JIS): one Shift_JIS code
 * to UCS-4, a profile's mbcs_to_ucs4, through the profile's conversion of the code's row.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"

int _sceCesSJisKutenToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
			   uint32_t *ucs4)
{
	uint32_t ucode;
	int ret;

	if (ku <= 15) {
		if (ku <= 8) {
			if (prof->row_1_8_to_ucs4 != NULL) {
				ret = prof->row_1_8_to_ucs4(prof, ku, ten, &ucode);
				goto set_ucode;
			}
		} else if (ku <= 12) {
			if (prof->row_9_12_to_ucs4 != NULL) {
				ret = prof->row_9_12_to_ucs4(prof, ku, ten, &ucode);
				goto set_ucode;
			}
		} else if (ku == 13) {
			if (prof->row_13_to_ucs4 != NULL) {
				ret = prof->row_13_to_ucs4(prof, ku, ten, &ucode);
				goto set_ucode;
			}
		} else {
			if (prof->row_14_15_to_ucs4 != NULL) {
				ret = prof->row_14_15_to_ucs4(prof, ku, ten, &ucode);
				goto set_ucode;
			}
		}
	} else if (ku <= 84) {
		if (prof->row_16_84_to_ucs4 != NULL) {
			ret = prof->row_16_84_to_ucs4(prof, ku, ten, &ucode);
			goto set_ucode;
		}
	} else if (ku <= 94) {
		if (prof->row_85_94_to_ucs4 != NULL) {
			ret = prof->row_85_94_to_ucs4(prof, ku, ten, &ucode);
			goto set_ucode;
		}
	} else if (ku <= 120) {
		if (prof->row_95_120_to_ucs4 != NULL) {
			ret = prof->row_95_120_to_ucs4(prof, ku, ten, &ucode);
			goto set_ucode;
		}
	}
	if (ucs4 != NULL)
		*ucs4 = 0;
	return SCE_CES_ERROR_UNASSIGNED_CODE;

set_ucode:
	if (ucs4 != NULL)
		*ucs4 = ucode;
	else if (ret == SCE_CES_OK)
		return SCE_CES_ERROR_INVALID_DST_BUFFER;
	return ret;
}

int sceCesSJisToUcs4(const SceCesJisUcsProfile *prof, const uint8_t *sjisBuf, uint32_t sjisMax,
		     uint32_t *sjisSize, uint32_t *ucs4)
{
	if (prof == NULL) {
		if (sjisSize != NULL)
			*sjisSize = 0;
		if (ucs4 != NULL)
			*ucs4 = 0;
		return SCE_CES_ERROR_INVALID_PROFILE;
	}
	if (sjisBuf != NULL && sjisMax != 0) {
		uint8_t sjc1 = sjisBuf[0], sjc2;

		if (sjc1 <= 0x7f) {
			if (sjisSize != NULL)
				*sjisSize = 1;
			return sceCesSbcToUcs4(prof->Roma, sjc1, ucs4);
		}
		/* the halfwidth katakana */
		if (sjc1 >= 0xa1 && sjc1 <= 0xdf) {
			if (sjisSize != NULL)
				*sjisSize = 1;
			if (prof->Kana == NULL) {
				if (ucs4 != NULL)
					*ucs4 = 0;
				return SCE_CES_ERROR_UNASSIGNED_CODE;
			}
			if (ucs4 == NULL)
				return SCE_CES_ERROR_INVALID_DST_BUFFER;
			*ucs4 = sjc1 + 0xfec0;
			return SCE_CES_OK;
		}
		if (sjc1 == 0x80 || sjc1 == 0xa0 || sjc1 > 0xfc) {
			if (sjisSize != NULL)
				*sjisSize = 1;
			if (ucs4 != NULL)
				*ucs4 = 0;
			return SCE_CES_ERROR_INVALID_ENCODE;
		}
		if (sjisSize != NULL)
			*sjisSize = 2;
		if (sjisMax < 2) {
			if (ucs4 != NULL)
				*ucs4 = 0;
			return SCE_CES_ERROR_SRC_BUFFER_END;
		}
		sjc2 = sjisBuf[1];
		if (sjc2 < 0x40 || sjc2 == 0x7f || sjc2 > 0xfc) {
			if (sjc2 == 0 && sjisSize != NULL)
				*sjisSize = 1;
			if (ucs4 != NULL)
				*ucs4 = 0;
			return SCE_CES_ERROR_INVALID_ENCODE;
		}
		/* the lead byte gives two rows, the trail byte the row and the cell */
		sjc1 = (sjc1 - (sjc1 <= 0x9f ? 0x81 : 0xc1)) * 2 + 1;
		if (sjc2 >= 0x9f) {
			sjc2 -= 0x9e;
			sjc1++;
		} else {
			sjc2 = sjc2 < 0x7f ? sjc2 - 0x3f : sjc2 - 0x40;
		}
		return _sceCesSJisKutenToUcs4(prof, sjc1, sjc2, ucs4);
	}
	if (sjisSize != NULL)
		*sjisSize = 0;
	if (ucs4 != NULL)
		*ucs4 = 0;
	return SCE_CES_ERROR_INVALID_SRC_BUFFER;
}
