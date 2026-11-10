/*
 * libSceCes's GB_FromUCS.c (Sony's internal/usermode/src/ces/libces/src/GB): one UCS-4 code
 * point to GB: by the Roman set, the euro sign to 0x80 where the profile has it, else the
 * profile's two- or four-byte code.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_gb_profile.h"

int sceCesUcs4ToGb(uint32_t ucs4, const SceCesGbUcsProfile *prof, uint8_t *gbBuf, uint32_t gbMax,
		   uint32_t *gbSize)
{
	int ret;

	ret = _SCE_CES_UCS4_CHECK_CODE(ucs4);
	if (gbSize != NULL)
		*gbSize = 0;
	if (ret != SCE_CES_OK)
		return ret;
	if (prof != NULL) {
		uint32_t gbCode;
		uint8_t sbc;

		if (ucs4 == CES_UCS_EURO_SIGN && (prof->types & CES_GB_TYPES_EURO) != 0) {
			sbc = 0x80;
			ret = SCE_CES_OK;
		} else {
			ret = sceCesUcs4ToSbc(ucs4, prof->Roma, &sbc);
		}
		if (ret != SCE_CES_ERROR_UNASSIGNED_CODE) {
			if (gbSize != NULL)
				*gbSize = 1;
			if (ret == SCE_CES_OK) {
				if (gbBuf == NULL)
					return SCE_CES_ERROR_INVALID_DST_BUFFER;
				if (gbMax == 0)
					return SCE_CES_ERROR_DST_BUFFER_END;
				gbBuf[0] = sbc;
			}
			return ret;
		}
		if (prof->gbCode_from_ucs4 == NULL)
			return SCE_CES_ERROR_INVALID_PROFILE;
		gbCode = prof->gbCode_from_ucs4(prof, ucs4);
		if (gbCode != 0) {
			if (gbCode <= 0xffff) {
				if (gbSize != NULL)
					*gbSize = 2;
				if (gbBuf == NULL)
					return SCE_CES_ERROR_INVALID_DST_BUFFER;
				if (gbMax < 2)
					return SCE_CES_ERROR_DST_BUFFER_END;
				gbBuf[1] = gbCode;
				gbBuf[0] = gbCode >> 8;
			} else {
				if (gbSize != NULL)
					*gbSize = 4;
				if (gbBuf == NULL)
					return SCE_CES_ERROR_INVALID_DST_BUFFER;
				if (gbMax < 4)
					return SCE_CES_ERROR_DST_BUFFER_END;
				gbBuf[3] = gbCode;
				gbBuf[0] = gbCode >> 24;
				gbBuf[1] = gbCode >> 16;
				gbBuf[2] = gbCode >> 8;
			}
			return SCE_CES_OK;
		}
		return SCE_CES_ERROR_UNASSIGNED_CODE;
	}
	return SCE_CES_ERROR_INVALID_PROFILE;
}
