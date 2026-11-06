/*
 * libSceCes's JisMenKuTen.c (Sony's internal/usermode/src/ces/libces/src/JIS): the JIS level of a
 * plane, row and cell: JIS X 0208's levels 1 and 2, JIS X 0213's levels 3 and 4.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"

/* rows 1-8: 3 in JIS X 0213's additions, else 1 */
int _sceCesJisRow_1_8_GetLevel(uint8_t ku, uint8_t ten)
{
	/* each row's end in the ranges that follow, then the ranges, first and last cell */
	static const uint8_t cesJIS2004Row1To8_lv3CharInfo[40] = {
	0x00, 0x0a, 0x12, 0x14, 0x16, 0x1a, 0x1e, 0x20, 0x0f, 0x19, 0x22, 0x29, 0x31, 0x3b, 0x4b, 0x51,
	0x5a, 0x5d, 0x01, 0x0f, 0x1a, 0x20, 0x3b, 0x40, 0x5b, 0x5e, 0x54, 0x5e, 0x57, 0x5e, 0x19, 0x20,
	0x39, 0x5e, 0x22, 0x30, 0x52, 0x5e, 0x21, 0x5e,
	};
	int i;
	int count;

	{
		int index = 0;
		const uint8_t *range;
		const uint8_t *info = cesJIS2004Row1To8_lv3CharInfo;

		i = ku - 1;
		if (i != 0)
			index = info[i - 1];
		count = (info[i] - index) / 2;
		range = &info[8 + index];
		for (; count != 0; count--, range += 2) {
			if (ten < range[0])
				return 1;
			if (ten <= range[1])
				return 3;
		}
	}
	return 1;
}

int sceCesJisGetLevel(uint8_t men, uint8_t ku, uint8_t ten, uint32_t *level)
{
	int ret = SCE_CES_OK;
	uint32_t lvTmp;

	if (level == NULL) {
		ret = SCE_CES_ERROR_INVALID_PARAMETER;
		level = &lvTmp;
	}
	if (ku == 0 || ten == 0 || ku > 94 || ten > 94)
		goto illegal;
	switch (men) {
	case 1:
		if (ku <= 8) {
			if (_sceCesJisRow_1_8_GetLevel(ku, ten) == 3) {
				if ((ku == 4 && ten >= 92) ||
				    (ku == 8 && ((ten >= 63 && ten <= 70) || ten >= 93)))
					ret = SCE_CES_ERROR_UNASSIGNED_CODE;
				*level = 3;
			} else {
				*level = 1;
			}
		} else if (ku < 16) {
			if ((ku == 12 && ten >= 84 && ten <= 92) ||
			    (ku == 13 && ((ten >= 56 && ten <= 62) || (ten >= 80 && ten <= 82) ||
					  (ten >= 84 && ten <= 87) || (ten >= 90 && ten <= 92))))
				ret = SCE_CES_ERROR_UNASSIGNED_CODE;
			*level = 3;
		} else if (ku <= 47) {
			if (ku == 47 && ten > 51)
				*level = 3;
			else
				*level = 1;
		} else if (ku <= 84) {
			if (ku == 84 && ten > 6)
				*level = 3;
			else
				*level = 2;
		} else {
			*level = 3;
		}
		return ret;
	case 2:
		*level = 4;
		if (ku < 16) {
			if (ku == 2 || ku == 6 || ku == 7 || ku == 9 || ku == 10 || ku == 11)
				ret = SCE_CES_ERROR_UNASSIGNED_CODE;
		} else if (ku <= 77 || (ku == 94 && ten >= 87)) {
			ret = SCE_CES_ERROR_UNASSIGNED_CODE;
		}
		return ret;
	}

illegal:
	*level = 0;
	return SCE_CES_ERROR_ILLEGAL_CODE;
}
