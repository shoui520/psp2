/*
 * libSceCes's Jis_FromUCS.c (Sony's internal/usermode/src/ces/libces/src/JIS): one UCS-4 code to
 * its JIS code under a profile: the Roman set, the vendor extensions, JIS X 0213's tables with
 * the variant's mappings, then JIS X 0212.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"

int _sceCesUcs4ToJis(uint32_t ucs4, const SceCesJisUcsProfile *prof, uint32_t *jisPlaneRowCell)
{
	uint32_t jisMenKuTen = 0;
	int ret;

	ret = _SCE_CES_UCS4_CHECK_CODE(ucs4);
	if (ret != SCE_CES_OK) {
		*jisPlaneRowCell = 0;
		return ret;
	}
	if (prof == NULL) {
		*jisPlaneRowCell = 0;
		return SCE_CES_ERROR_INVALID_PROFILE;
	}
	if (ucs4 > 0xffff) {
		if (prof->jisExBits_from_ucsCJKExB != NULL)
			jisMenKuTen = prof->jisExBits_from_ucsCJKExB(prof, ucs4);
		ret = SCE_CES_ERROR_UNASSIGNED_CODE;
	} else {
		{
			uint8_t sbc;

			ret = sceCesUcs4ToSbc(ucs4, prof->Roma, &sbc);
			if (ret != SCE_CES_ERROR_UNASSIGNED_CODE) {
				jisMenKuTen = sbc;
				goto write;
			}
		}
		/* the C1 controls are their own codes */
		if (ucs4 >= 0x80 && ucs4 <= 0x9f) {
			jisMenKuTen = ucs4;
			goto write;
		}
		if (prof->jisExBits_from_ucs4 != NULL) {
			jisMenKuTen = prof->jisExBits_from_ucs4(prof, ucs4);
			if (jisMenKuTen != 0)
				goto write;
		}
		if (ucs4 < 0x3000) {
			switch (ucs4) {
			case 0x5c:	/* REVERSE SOLIDUS */
				if (prof->types & CES_JIS_TYPE_YEN_BACKSLASH)
					jisMenKuTen = 0x40a0;
				goto write;
			case 0xa5:	/* YEN SIGN */
				if (!(prof->types & CES_JIS_TYPE_YEN_BACKSLASH))
					jisMenKuTen = 0x40cf;
				goto write;
			case 0x7e:	/* TILDE */
				if (!(prof->types & CES_JIS_TYPE_TILDE))
					goto write;
				if (prof->types >= CES_JIS_TYPE_X0213 && prof->jisX0212_from_ucs4 == NULL) {
					jisMenKuTen = 0x4112;
					goto write;
				}
				goto stepX0212;
			case 0x203e:	/* OVERLINE */
				if (!(prof->types & CES_JIS_TYPE_TILDE))
					jisMenKuTen = 0x4091;
				goto write;
			case 0x2211:	/* N-ARY SUMMATION */
				if (prof->types & CES_JIS_TYPE_NEC_ROW13)
					jisMenKuTen = 0x46d4;
				goto write;
			}
			if (prof->types & CES_JIS_TYPE_CP932) {
				if (ucs4 == 0x2015) {	/* HORIZONTAL BAR */
					jisMenKuTen = 0x409d;
					goto write;
				}
				if (ucs4 == 0x2225) {	/* PARALLEL TO */
					jisMenKuTen = 0x40a2;
					goto write;
				}
			}
		} else if (ucs4 <= 0x30ff) {
			jisMenKuTen = _sceCesJIS2004_Ucs_3000_30FF_To_JisMenkuten(ucs4);
		} else if (ucs4 >= 0x4e00) {
			if (ucs4 <= 0x9fff) {
				jisMenKuTen = _sceCesJIS2004_Ucs_4E00_9FFF_To_JisMenkuten(ucs4);
			} else if (ucs4 >= 0xff01) {
				if (ucs4 <= 0xff60) {
					if (prof->types & CES_JIS_TYPE_CP932) {
						if (ucs4 == 0xff0d) {	/* FULLWIDTH HYPHEN-MINUS */
							jisMenKuTen = 0x40bd;
							goto write;
						}
						if (ucs4 == 0xff5e) {	/* FULLWIDTH TILDE */
							jisMenKuTen = 0x40a1;
							goto write;
						}
					} else if (ucs4 == 0xff3c &&
						   (prof->types & CES_JIS_TYPE_YEN_BACKSLASH)) {
						goto write;
					}
					jisMenKuTen = _sceCesJIS2004_Ucs_FF01_FF60_To_JisMenkuten(ucs4);
				} else if (ucs4 <= 0xff9f) {
					/* the halfwidth katakana */
					if (prof->Kana != NULL) {
						jisMenKuTen = (uint8_t)(ucs4 - 0xfec0);
						goto write;
					}
				} else {
					if (prof->types & CES_JIS_TYPE_CP932) {
						if (ucs4 == 0xffe0)		/* FULLWIDTH CENT SIGN */
							jisMenKuTen = 0x40d1;
						else if (ucs4 == 0xffe1)	/* FULLWIDTH POUND SIGN */
							jisMenKuTen = 0x40d2;
						else if (ucs4 == 0xffe2)	/* FULLWIDTH NOT SIGN */
							jisMenKuTen = 0x412c;
					}
					if (ucs4 == 0xffe3)		/* FULLWIDTH MACRON */
						jisMenKuTen = 0x4091;
					else if (ucs4 == 0xffe5)	/* FULLWIDTH YEN SIGN */
						jisMenKuTen = 0x40cf;
					goto write;
				}
			}
		}
		if (jisMenKuTen == 0)
			jisMenKuTen = _sceCesJIS2004_Ucs_Other_ToJisMenKuten(ucs4);
	}
	if (jisMenKuTen != 0) {
		uint8_t men = (uint8_t)(jisMenKuTen >> 14);
		uint8_t ku = (jisMenKuTen >> 7) & 0x7f;
		uint8_t ten = jisMenKuTen & 0x7f;

		if (men == 1) {
			/* before JIS X 0213 its level 3 characters are none, but NEC's row 13 */
			if (prof->types < CES_JIS_TYPE_X0213 &&
			    (!(prof->types & CES_JIS_TYPE_NEC_ROW13) || ku != 13 || ten == 31 ||
			     ten == 55 || ten >= 93)) {
				uint32_t lv;

				if (sceCesJisGetLevel(men, ku, ten, &lv) == SCE_CES_OK &&
				    (lv >= 3 || (prof->types < CES_JIS_TYPE_X0208 && lv >= 2))) {
					jisMenKuTen = 0;
					goto stepX0212;
				}
			}
			goto write;
		}
		if (men != 2 || prof->types >= CES_JIS_TYPE_X0213_PLANE2)
			goto write;
		jisMenKuTen = 0;
	}

stepX0212:
	if (prof->jisX0212_from_ucs4 != NULL)
		jisMenKuTen = prof->jisX0212_from_ucs4(prof, ucs4);
write:
	*jisPlaneRowCell = jisMenKuTen;
	if (jisMenKuTen != 0)
		ret = SCE_CES_OK;
	return ret;
}
