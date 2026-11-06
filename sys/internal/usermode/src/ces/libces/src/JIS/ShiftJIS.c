/*
 * libSceCes's ShiftJIS.c (Sony's internal/usermode/src/ces/libces/src/JIS): JIS codes to
 * Shift_JIS codes. A lead byte holds two rows: plane 1's rows from 0x81 (rows 95-120 the vendor
 * extensions from 0xF0), JIS X 0213 plane 2's from 0xF0.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"

int sceCesJisToSJisCode(uint8_t men, uint8_t ku, uint8_t ten, uint32_t sjisType,
			uint16_t *sjisCode)
{
	uint16_t sjis;
	int flag;
	int ret = SCE_CES_OK;

	if (ku == 0 || ten == 0 || ten > 94)
		goto illegal;
	switch (men) {
	case 1:
		flag = !(ku & 1);
		if (ku <= 62) {
			sjis = (ku - 1) / 2 + 0x81;
		} else if (ku <= 120) {
			if (ku > 94 && (sjisType & CES_JIS_TYPE_VENDOR_EX) == 0)
				ret = SCE_CES_ERROR_ILLEGAL_CODE;
			sjis = (ku - 63) / 2 + 0xe0;
		} else {
			goto illegal;
		}
		break;
	case 2:
		if (sjisType < CES_JIS_TYPE_X0213_PLANE2) {
			if (sjisCode != NULL)
				*sjisCode = 0;
			return SCE_CES_ERROR_OUT_OF_CODE_RANGE;
		}
		if (ku >= 78) {
			if (ku > 94)
				goto illegal;
			sjis = (ku - 77) / 2 + 0xf4;
			flag = !(ku & 1);
		} else if (ku >= 12 && ku <= 15) {
			sjis = (ku - 11) / 2 + 0xf2;
			flag = !(ku & 1);
		} else {
			/* rows 1, 8, 3, 4 and 5 share the lead bytes 0xF0-0xF2 */
			if (ku == 8)
				ku = 2;
			else if (ku == 2 || ku > 5) {
				if (sjisCode != NULL)
					*sjisCode = 0;
				return SCE_CES_ERROR_UNASSIGNED_CODE;
			}
			sjis = (ku - 1) / 2 + 0xf0;
			flag = !(ku & 1);
		}
		break;
	default:
		goto illegal;
	}
	if (sjisCode != NULL) {
		if (flag)
			sjis = (sjis << 8) + ten + 0x9e;
		else
			sjis = (sjis << 8) + (ten < 0x40 ? 0x3f : 0x40) + ten;
		*sjisCode = sjis;
	}
	return ret;

illegal:
	if (sjisCode != NULL)
		*sjisCode = 0;
	return SCE_CES_ERROR_ILLEGAL_CODE;
}

int sceCesJisToSJis(uint8_t men, uint8_t ku, uint8_t ten, uint32_t sjisType, uint8_t *sjisBuf,
		    uint32_t sjisMax, uint32_t *sjisSize)
{
	uint16_t sjisCode;
	int ret;

	ret = sceCesJisToSJisCode(men, ku, ten, sjisType, &sjisCode);
	if (ret != SCE_CES_OK) {
		if (sjisSize != NULL)
			*sjisSize = 0;
		return ret;
	}
	if (sjisSize != NULL)
		*sjisSize = 2;
	if (sjisBuf == NULL)
		return SCE_CES_ERROR_INVALID_DST_BUFFER;
	if (sjisMax < 2)
		return SCE_CES_ERROR_DST_BUFFER_END;
	sjisBuf[0] = sjisCode >> 8;
	sjisBuf[1] = (uint8_t)sjisCode;
	return SCE_CES_OK;
}
