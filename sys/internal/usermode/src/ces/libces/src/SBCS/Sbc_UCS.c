/*
 * libSceCes's Sbc_UCS.c (Sony's internal/usermode/src/ces/libces/src/SBCS): one single-byte
 * code to UCS-4 and back, by the set's own functions or its table.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_sbcs_profile.h"

int sceCesSbcToUcs4(const SceCesSbcsUcsProfile *prof, uint8_t sbc, uint32_t *ucs4)
{
	uint32_t code;
	int ret;

	ret = _sceCesSbcToUcs4(prof, sbc, &code);
	if (ucs4 != NULL) {
		*ucs4 = code;
		return ret;
	}
	if (ret != SCE_CES_OK)
		return ret;
	return SCE_CES_ERROR_INVALID_DST_BUFFER;
}

int _sceCesSbcToUcs4(const SceCesSbcsUcsProfile *prof, uint8_t sbc, uint32_t *ucs4)
{
	const SceCesSbcs *sbcs;

	if (prof == NULL) {
		*ucs4 = 0;
		return SCE_CES_ERROR_INVALID_PROFILE;
	}
	sbcs = prof->sbcs;
	if (sbcs != NULL && sbc >= sbcs->baseCode) {
		const uint16_t *tbl = sbcs->Table;
		uint32_t base = sbcs->baseCode;
		uint32_t code;

		if (sbcs->getUCS != NULL) {
			code = sbcs->getUCS(sbcs, sbc);
		} else if (sbc - base < sbcs->tableCount) {
			code = tbl[sbc - base];
		} else {
			*ucs4 = 0;
			return SCE_CES_ERROR_INVALID_ENCODE;
		}
		if ((int)code > 0) {
			*ucs4 = code;
			return SCE_CES_OK;
		}
		*ucs4 = 0;
		if (code == 0)
			return SCE_CES_ERROR_UNASSIGNED_CODE;
		return SCE_CES_ERROR_INVALID_ENCODE;
	}
	*ucs4 = sbc;
	return SCE_CES_OK;
}

int sceCesUcs4ToSbc(uint32_t ucs4, const SceCesSbcsUcsProfile *prof, uint8_t *sbc)
{
	int ret = _SCE_CES_UCS4_CHECK_CODE(ucs4);

	if (ret == SCE_CES_OK)
		return _sceCesUcs4ToSbc(ucs4, prof, sbc);
	if (sbc != NULL)
		*sbc = 0;
	return ret;
}

int _sceCesUcs4ToSbc(uint32_t ucs4, const SceCesSbcsUcsProfile *prof, uint8_t *sbc)
{
	const SceCesSbcs *sbcs;

	if (prof == NULL) {
		if (sbc != NULL)
			*sbc = 0;
		return SCE_CES_ERROR_INVALID_PROFILE;
	}
	sbcs = prof->sbcs;
	if (sbcs == NULL) {
		if (ucs4 > 0xff) {
			if (sbc != NULL)
				*sbc = 0;
			return SCE_CES_ERROR_UNASSIGNED_CODE;
		}
	} else {
		uint32_t baseCode = sbcs->baseCode;

		if (ucs4 >= baseCode) {
			if (sbcs->getSBC != NULL) {
				uint8_t code = sbcs->getSBC(sbcs, ucs4);

				if (code == 0)
					goto unassigned;
				if (sbc == NULL)
					return SCE_CES_ERROR_INVALID_DST_BUFFER;
				*sbc = code;
				return SCE_CES_OK;
			} else {
				const uint16_t *tbl = sbcs->Table;
				const uint16_t *start = tbl;
				const uint16_t *end = tbl + sbcs->tableCount;

				do {
					if (*tbl == ucs4) {
						if (sbc == NULL)
							return SCE_CES_ERROR_INVALID_DST_BUFFER;
						*sbc = baseCode + (tbl - start);
						return SCE_CES_OK;
					}
				} while (++tbl < end);
			}
unassigned:
			if (sbc != NULL)
				*sbc = 0;
			return SCE_CES_ERROR_UNASSIGNED_CODE;
		}
	}
	if (sbc == NULL)
		return SCE_CES_ERROR_INVALID_DST_BUFFER;
	*sbc = ucs4;
	return SCE_CES_OK;
}
