/*
 * libSceCes's JIS profiles (Sony's internal/usermode/src/ces/libces/src/JIS/ces_jis_profile.h):
 * one profile for Shift_JIS, EUC-JP and ISO-2022-JP, an MBCS profile head with the sets a
 * variant takes (its single-byte Roman and Katakana sets, ISO-2022's designations) and its
 * conversions by JIS row. Laid out and named from the library's debug information (SDK 3.570.011
 * libSceCes.a). A JIS code is men << 14 | ku << 7 | ten (plane, row and cell), a single-byte code
 * itself.
 */
#ifndef _CES_JIS_PROFILE_H
#define _CES_JIS_PROFILE_H

#include <stdint.h>
#include "../MBCS/ces_mbcs_profile.h"
#include "../SBCS/ces_sbcs_profile.h"

struct SceCesJisUcsProfile;

/* a row's cell to UCS-4 */
typedef int _sceCesSJisUcsFunc(const struct SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
			       uint32_t *ucs4);
/* a UCS-4 code point's JIS code; 0 for none */
typedef uint32_t _sceCesJisMenKuTenFromUcsFunc(const struct SceCesJisUcsProfile *prof,
					       uint32_t ucs4);

/* ISO-2022-JP's designations (64 bytes in Sony's header); the tree has no ISO-2022 code yet */
typedef struct SceCesJisIso2022Profile SceCesJisIso2022Profile;

/* the variant bits of types; the names are ours */
#define CES_JIS_TYPE_NEC_ROW13		0x00000001	/* NEC's row 13 */
#define CES_JIS_TYPE_NEC_IBM_EX		0x00000002	/* NEC-selected IBM extensions, rows 89-92 */
#define CES_JIS_TYPE_VENDOR_EX		0x0000000f	/* any vendor extension: rows 95-120 */
#define CES_JIS_TYPE_CP932		0x00000100	/* Microsoft's mappings of the symbols */
#define CES_JIS_TYPE_YEN_BACKSLASH	0x00001000	/* 0x5C is the backslash, not the yen sign */
#define CES_JIS_TYPE_TILDE		0x00002000	/* 0x7E is the tilde, not the overline */
/* the JIS edition, from bit 16 on: below X0213 level 3 and 4 characters are none */
#define CES_JIS_TYPE_X0208		0x00020000	/* level 2 kanji exist */
#define CES_JIS_TYPE_X0213		0x00030000	/* JIS X 0213 plane 1 */
#define CES_JIS_TYPE_X0213_PLANE2	0x00040000	/* JIS X 0213 plane 2 */

typedef struct SceCesJisUcsProfile {
	uint32_t cef_type;
	uint32_t types;
	_sceCesMbcsToUcs4Func *mbcs_to_ucs4;
	_sceCesUcs4ToMbcsFunc *ucs4_to_mbcs;
	const SceCesJisIso2022Profile *iso2022Profile;
	const SceCesSbcsUcsProfile *Roma;
	_sceCesSJisUcsFunc *row_1_8_to_ucs4;
	_sceCesSJisUcsFunc *row_9_12_to_ucs4;
	_sceCesSJisUcsFunc *row_13_to_ucs4;
	_sceCesSJisUcsFunc *row_14_15_to_ucs4;
	_sceCesSJisUcsFunc *row_16_84_to_ucs4;
	_sceCesSJisUcsFunc *row_85_94_to_ucs4;
	_sceCesSJisUcsFunc *row_95_120_to_ucs4;
	_sceCesJisMenKuTenFromUcsFunc *jisExBits_from_ucs4;
	_sceCesJisMenKuTenFromUcsFunc *jisExBits_from_ucsCJKExB;
	const SceCesSbcsUcsProfile *Kana;
	_sceCesSJisUcsFunc *jisX0212_to_ucs4;
	_sceCesJisMenKuTenFromUcsFunc *jisX0212_from_ucs4;
} SceCesJisUcsProfile, SceCesSJisUcsProfile, SceCesEucJpUcsProfile, SceCesIso2022JpUcsProfile;

/* JIS */
int sceCesJisGetLevel(uint8_t men, uint8_t ku, uint8_t ten, uint32_t *level);
int _sceCesJisRow_1_8_GetLevel(uint8_t ku, uint8_t ten);
int _sceCesUcs4ToJis(uint32_t ucs4, const SceCesJisUcsProfile *prof, uint32_t *jisPlaneRowCell);

/* Shift_JIS */
int sceCesJisToSJisCode(uint8_t men, uint8_t ku, uint8_t ten, uint32_t sjisType,
			uint16_t *sjisCode);
int sceCesJisToSJis(uint8_t men, uint8_t ku, uint8_t ten, uint32_t sjisType, uint8_t *sjisBuf,
		    uint32_t sjisMax, uint32_t *sjisSize);
int _sceCesSJisKutenToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
			   uint32_t *ucs4);
int sceCesSJisToUcs4(const SceCesJisUcsProfile *prof, const uint8_t *sjisBuf, uint32_t sjisMax,
		     uint32_t *sjisSize, uint32_t *ucs4);
int sceCesUcs4ToSJis(uint32_t ucs4, const SceCesJisUcsProfile *prof, uint8_t *sjisBuf,
		     uint32_t sjisMax, uint32_t *sjisSize);
int sceCesUtf16ToSJis(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len,
		      const SceCesJisUcsProfile *prof, uint8_t *sjisBuf, uint32_t sjisMax,
		      uint32_t *sjisSize);
SceCesSJisUcsProfile *sceCesUcsProfileInitSJis1997Cp932(SceCesJisUcsProfile *prof);

/* the rows, JIS X 0208/0213 to UCS */
int _sceCesSJis2004Row_1_8_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
				  uint32_t *ucs4);
int _sceCesSJisRow_1_8_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
			      uint32_t *ucs4);
int _sceCesSJis2004Row_13_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
				 uint32_t *ucs4);
int _sceCesSJisNECRow_13_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
				uint32_t *ucs4);
int _sceCesSJis2004Row_16_84_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
				    uint32_t *ucs4);
int _sceCesSJisRow_16_84_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
				uint32_t *ucs4);
int _sceCesSJisRow_85_94_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
				uint32_t *ucs4);
int _sceCesSJisRow_95_120_ToUcs4(const SceCesJisUcsProfile *prof, uint8_t ku, uint8_t ten,
				 uint32_t *ucs4);
uint32_t _sceCesJisUcsExBIndexToUcs4(int index);

/* UCS to JIS */
int _sceCesSJisIBMIndexFromUcs(uint32_t ucs4);
uint32_t _sceCesJisMenKuTenNECExFromUcs(const SceCesJisUcsProfile *prof, uint32_t ucs4);
uint16_t _sceCesJIS2004_Ucs_3000_30FF_To_JisMenkuten(uint32_t ucs4);
uint16_t _sceCesJIS2004_Ucs_4E00_9FFF_To_JisMenkuten(uint32_t ucs4);
uint16_t _sceCesJIS2004_Ucs_FF01_FF60_To_JisMenkuten(uint32_t ucs4);
uint16_t _sceCesJIS2004_Ucs_Other_ToJisMenKuten(uint32_t ucs4);

#endif /* _CES_JIS_PROFILE_H */
