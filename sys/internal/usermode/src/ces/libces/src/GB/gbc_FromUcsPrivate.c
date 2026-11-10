/*
 * libSceCes's gbc_FromUcsPrivate.c (Sony's internal/usermode/src/ces/libces/src/GB): the GB code
 * of a private-use code point: the user-defined areas (U+E000-U+E765) where the profile maps them,
 * else the codes GB18030-2000 maps into the PUA.
 */
#include <stdint.h>
#include "ces_gb_profile.h"

/* by code point, runs of consecutive codes ended by an entry of code point 0 */
static const CesUcsDbcDB_t cesGB_UcsPrivateDB[] = {
	{ 0xe766, 0xa2ab }, { 0x0000, 0xa2b0 }, { 0xe76c, 0xa2e3 }, { 0xe76d, 0xa2e4 },
	{ 0xe76e, 0xa2ef }, { 0x0000, 0xa2f0 }, { 0xe770, 0xa2fd }, { 0x0000, 0xa2fe },
	{ 0xe772, 0xa4f4 }, { 0x0000, 0xa4fe }, { 0xe77d, 0xa5f7 }, { 0x0000, 0xa5fe },
	{ 0xe785, 0xa6b9 }, { 0x0000, 0xa6c0 }, { 0xe78d, 0xa6d9 }, { 0x0000, 0xa6df },
	{ 0xe794, 0xa6ec }, { 0x0000, 0xa6ed }, { 0xe796, 0xa6f3 }, { 0xe797, 0xa6f6 },
	{ 0x0000, 0xa6fe }, { 0xe7a0, 0xa7c2 }, { 0x0000, 0xa7d0 }, { 0xe7af, 0xa7f2 },
	{ 0x0000, 0xa7fe }, { 0xe7bc, 0xa896 }, { 0x0000, 0xa8a0 }, { 0xe7c7, 0xa8bc },
	{ 0xe7c8, 0xa8bf }, { 0xe7c9, 0xa8c1 }, { 0x0000, 0xa8c4 }, { 0xe7cd, 0xa8ea },
	{ 0x0000, 0xa8fe }, { 0xe7e2, 0xa958 }, { 0xe7e3, 0xa95b }, { 0xe7e4, 0xa95d },
	{ 0x0000, 0xa95f }, { 0xe7e7, 0xa989 }, { 0x0000, 0xa995 }, { 0xe7f4, 0xa997 },
	{ 0x0000, 0xa9a0 }, { 0xe7fe, 0xa9a1 }, { 0x0000, 0xa9a3 }, { 0xe801, 0xa9f0 },
	{ 0x0000, 0xa9fe }, { 0xe810, 0xd7fa }, { 0x0000, 0xd7fe }, { 0xe815, 0xfe50 },
	{ 0x0000, 0xfe7e }, { 0xe844, 0xfe80 }, { 0x0000, 0xfea0 },
};

uint32_t _sceCesGBCode_FromUcsPrivate(const SceCesGbUcsProfile *prof, uint32_t ucs4)
{
	uint32_t gbc;

	if (ucs4 < 0xe000)
		return 0;
	if (ucs4 <= 0xe765) {
		if ((prof->types & CES_GB_TYPES_UDA_UCS_PUA) != 0) {
			if (ucs4 <= 0xe233) {
				/* user-defined area 1: 0xAAA1-0xAFFE */
				uint32_t i = ucs4 - 0xe000;

				gbc = (uint16_t)(((i / 94) << 8) + i % 94 + 0xaaa1);
			} else if (ucs4 <= 0xe4c5) {
				/* user-defined area 2: 0xF8A1-0xFEFE */
				uint32_t i = ucs4 - 0xe234;

				gbc = (uint16_t)(((i / 94) << 8) + i % 94 + 0xf8a1);
			} else {
				/* user-defined area 3: 0xA140-0xA7A0, 0x7F left out */
				uint32_t i = ucs4 - 0xe4c6;

				gbc = ((i / 96) << 8) + i % 96 + 0xa141;
				if ((int8_t)gbc >= 0)
					gbc = ((i / 96 + 0xa1) << 8) + i % 96 + 0x40;
			}
		} else {
			gbc = 0;
		}
	} else if (ucs4 == 0xe7c7 && prof->types >= CES_GB_TYPES_EDITION_2005) {
		/* GB18030-2005 maps 0x8135F437 to U+1E3F and U+E7C7 back */
		gbc = 0x8135f437;
	} else {
		gbc = _sceCesUcsDbcDB_getDbc(cesGB_UcsPrivateDB, CES_DB_LAST(cesGB_UcsPrivateDB),
					     ucs4);
		/* code page 936 has none of 0xFE50 on */
		if (gbc >= 0xfe50 && prof->types < CES_GB_TYPES_EDITION_2000 &&
		    (prof->types & (CES_GB_TYPES_CP936 | CES_GB_TYPES_EURO)) ==
			    (CES_GB_TYPES_CP936 | CES_GB_TYPES_EURO))
			gbc = 0;
	}
	return gbc;
}
