/*
 * libSceCes's GB_ToUCS.c (Sony's internal/usermode/src/ces/libces/src/GB): one GB code to UCS-4:
 * a byte to 0x7F by the Roman set, 0x80 the euro sign where the profile has it, two bytes by
 * their area, GB18030's four bytes (byte, digit, byte, digit) by its four-byte table.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_gb_profile.h"

int sceCesGbToUcs4(const SceCesGbUcsProfile *prof, const uint8_t *gbBuf, uint32_t gbMax,
		   uint32_t *gbSize, uint32_t *ucs4)
{
	uint32_t ucode;
	int ret;

	if (prof == NULL) {
		ret = SCE_CES_ERROR_INVALID_PROFILE;
		goto error;
	}
	if (gbBuf != NULL && gbMax != 0) {
		uint8_t gbc1 = gbBuf[0];
		uint8_t gbc2;
		uint8_t gbc3;
		uint8_t gbc4;

		if (gbc1 <= 0x7f) {
			if (gbSize != NULL)
				*gbSize = 1;
			if (gbc1 == 0x0e && prof->cef_type == CES_GB_TYPE_EUC_CN)
				goto inv_encode;
			return sceCesSbcToUcs4(prof->Roma, gbc1, ucs4);
		}
		if (gbc1 <= 0x9f && prof->cef_type == CES_GB_TYPE_EUC_CN) {
			if (gbSize != NULL)
				*gbSize = 1;
			if (gbc1 == 0x8e || gbc1 == 0x8f)
				goto inv_encode;
			ret = SCE_CES_OK;
			ucode = gbc1;
			goto set_ucode;
		}
		if (gbc1 == 0x80) {
			if (gbSize != NULL)
				*gbSize = 1;
			if ((prof->types & CES_GB_TYPES_EURO) == 0)
				goto inv_encode;
			if (ucs4 != NULL)
				*ucs4 = CES_UCS_EURO_SIGN;
			return SCE_CES_OK;
		}
		if ((gbc1 == 0xa0 && prof->cef_type == CES_GB_TYPE_EUC_CN) || gbc1 == 0xff) {
			if (gbSize != NULL)
				*gbSize = 1;
			goto inv_encode;
		}
		if (gbMax < 2) {
			if (gbSize != NULL)
				*gbSize = 2;
			if (ucs4 != NULL)
				*ucs4 = 0;
			return SCE_CES_ERROR_SRC_BUFFER_END;
		}
		gbc2 = gbBuf[1];
		if (gbc2 >= 0x40) {
			uint32_t gbCode;

			if (gbSize != NULL)
				*gbSize = 2;
			if (gbc2 == 0x7f || gbc2 == 0xff)
				goto inv_encode;
			gbCode = gbc1 << 8 | gbc2;
			/* the areas: 1 0xA1-0xAF and 2 0xB0-0xF7 by second bytes 0xA1-0xFE (GB2312),
			 * 3 the first bytes 0x81-0xA0, 4 0xAA-0xFE and 5 0xA1-0xA9 by second bytes
			 * 0x40-0xA0 */
			if (gbc1 >= 0xa1) {
				if (gbc2 >= 0xa1) {
					if (gbc1 < 0xb0) {
						if (prof->gb2byte_1_to_ucs4 != NULL) {
							ret = prof->gb2byte_1_to_ucs4(prof, gbCode, &ucode);
							goto set_ucode;
						}
					} else {
						if (prof->gb2byte_2_to_ucs4 != NULL) {
							ret = prof->gb2byte_2_to_ucs4(prof, gbCode, &ucode);
							goto set_ucode;
						}
					}
					if (ucs4 != NULL)
						*ucs4 = 0;
					return SCE_CES_ERROR_INVALID_PROFILE;
				}
				if (gbc1 < 0xaa) {
					if (prof->gb2byte_5_to_ucs4 != NULL) {
						ret = prof->gb2byte_5_to_ucs4(prof, gbCode, &ucode);
						goto set_ucode;
					}
				} else {
					if (prof->gb2byte_4_to_ucs4 != NULL) {
						ret = prof->gb2byte_4_to_ucs4(prof, gbCode, &ucode);
						goto set_ucode;
					}
				}
			} else {
				if (prof->gb2byte_3_to_ucs4 != NULL) {
					ret = prof->gb2byte_3_to_ucs4(prof, gbCode, &ucode);
					goto set_ucode;
				}
			}
			if (ucs4 != NULL)
				*ucs4 = 0;
			return SCE_CES_ERROR_UNASSIGNED_CODE;
		}
		if (gbc2 >= 0x30 && gbc2 <= 0x39 && prof->gb4byte_to_ucs4 != NULL) {
			if (gbSize != NULL)
				*gbSize = 4;
			if (gbMax < 4) {
				if (ucs4 != NULL)
					*ucs4 = 0;
				if (gbMax >= 3 && gbBuf[2] == 0) {
					if (gbSize != NULL)
						*gbSize = 2;
					return SCE_CES_ERROR_INVALID_ENCODE;
				}
				return SCE_CES_ERROR_SRC_BUFFER_END;
			}
			gbc3 = gbBuf[2];
			gbc4 = gbBuf[3];
			if (gbc3 > 0x80 && gbc3 != 0xff && gbc4 >= 0x30 && gbc4 <= 0x39) {
				uint32_t gbCode = gbc1 << 24 | gbc2 << 16 | gbc3 << 8 | gbc4;

				ret = prof->gb4byte_to_ucs4(prof, gbCode, &ucode);
				goto set_ucode;
			}
			if (gbSize != NULL) {
				if (gbc3 == 0)
					*gbSize = 2;
				else if (gbc4 == 0)
					*gbSize = 3;
			}
			goto inv_encode;
		}
		if (gbSize != NULL) {
			if (gbc2 == 0)
				*gbSize = 1;
			else
				*gbSize = 2;
		}
		goto inv_encode;
	}
	goto inv_src;

inv_encode:
	if (ucs4 != NULL)
		*ucs4 = 0;
	return SCE_CES_ERROR_INVALID_ENCODE;

inv_src:
	ret = SCE_CES_ERROR_INVALID_SRC_BUFFER;
error:
	if (gbSize != NULL)
		*gbSize = 0;
	if (ucs4 != NULL)
		*ucs4 = 0;
	return ret;

set_ucode:
	if (ucs4 != NULL)
		*ucs4 = ucode;
	else if (ret == SCE_CES_OK)
		return SCE_CES_ERROR_INVALID_DST_BUFFER;
	return ret;
}
