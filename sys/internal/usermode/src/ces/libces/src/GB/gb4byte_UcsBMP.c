/*
 * libSceCes's gb4byte_UcsBMP.c (Sony's internal/usermode/src/ces/libces/src/GB): GB18030's
 * four-byte codes (byte 0x81-0xFE, digit, byte 0x81-0xFE, digit) and UCS-4. The BMP code points
 * no two-byte code has run in order from 0x81308130, by runs of consecutive code points; planes
 * 1-16 run from 0x90308130 without a gap.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_gb_profile.h"

/* a linear index's four-byte code from the code first (0x81308130, 0x90308130) */
#define CES_GB4_CODE(i, first) \
	((i) % 10 + ((i) / 10 % 126 << 8) + ((i) / 10 / 126 % 10 << 16) + \
	 ((i) / 10 / 126 / 10 % 126 << 24) + (first))

/* a four-byte code's linear index from first..fourth byte 0x81, 0x30, 0x81, 0x30 */
#define CES_GB4_INDEX(gbc1, gbc2, gbc3, gbc4) \
	(((((gbc1) - 0x81) * 10 + (gbc2) - 0x30) * 126 + (gbc3) - 0x81) * 10 + (gbc4) - 0x30)

/* runs by the index of the four-byte code from 0x81308130: U+3400-U+4DFF, CJK Extension A */
static const CesIndexUcsDB_t cesGB18030_4Byte_Ucs_3400_4DFF[] = {
	{ 0x3097, 0x3400 }, { 0x30de, 0x3448 }, { 0x3109, 0x3474 }, { 0x3233, 0x359f },
	{ 0x32a2, 0x360f }, { 0x32ad, 0x361b }, { 0x35aa, 0x3919 }, { 0x35ff, 0x396f },
	{ 0x365f, 0x39d1 }, { 0x366d, 0x39e0 }, { 0x3700, 0x3a74 }, { 0x37da, 0x3b4f },
	{ 0x38f9, 0x3c6f }, { 0x396a, 0x3ce1 }, { 0x3cdf, 0x4057 }, { 0x3de7, 0x4160 },
	{ 0x3fbe, 0x4338 }, { 0x4032, 0x43ad }, { 0x4036, 0x43b2 }, { 0x4061, 0x43de },
	{ 0x4159, 0x44d7 }, { 0x42ce, 0x464d }, { 0x42e2, 0x4662 }, { 0x43a3, 0x4724 },
	{ 0x43a8, 0x472a }, { 0x43fa, 0x477d }, { 0x440a, 0x478e }, { 0x45c3, 0x4948 },
	{ 0x45f5, 0x497b }, { 0x45f7, 0x497e }, { 0x45fb, 0x4984 }, { 0x45fc, 0x4987 },
	{ 0x4610, 0x499c }, { 0x4613, 0x49a0 }, { 0x4629, 0x49b8 }, { 0x48e8, 0x4c78 },
	{ 0x490f, 0x4ca4 }, { 0x497e, 0x4d1a }, { 0x4a12, 0x4daf }, { 0x4a19, 0x4db6 },
	{ 0x4a63, 0x0000 },
};

/* U+0080-U+33FF */
static const CesIndexUcsDB_t cesGB18030_4Byte_Ucs_0080_33FF[] = {
	{ 0x0000, 0x0080 }, { 0x0024, 0x00a5 }, { 0x0026, 0x00a9 }, { 0x002d, 0x00b2 },
	{ 0x0032, 0x00b8 }, { 0x0051, 0x00d8 }, { 0x0059, 0x00e2 }, { 0x005f, 0x00eb },
	{ 0x0060, 0x00ee }, { 0x0064, 0x00f4 }, { 0x0067, 0x00f8 }, { 0x0068, 0x00fb },
	{ 0x0069, 0x00fd }, { 0x006d, 0x0102 }, { 0x007e, 0x0114 }, { 0x0085, 0x011c },
	{ 0x0094, 0x012c }, { 0x00ac, 0x0145 }, { 0x00af, 0x0149 }, { 0x00b3, 0x014e },
	{ 0x00d0, 0x016c }, { 0x0132, 0x01cf }, { 0x0133, 0x01d1 }, { 0x0134, 0x01d3 },
	{ 0x0135, 0x01d5 }, { 0x0136, 0x01d7 }, { 0x0137, 0x01d9 }, { 0x0138, 0x01db },
	{ 0x0139, 0x01dd }, { 0x0155, 0x01fa }, { 0x01ac, 0x0252 }, { 0x01bb, 0x0262 },
	{ 0x0220, 0x02c8 }, { 0x0221, 0x02cc }, { 0x022e, 0x02da }, { 0x02e5, 0x03a2 },
	{ 0x02e6, 0x03aa }, { 0x02ed, 0x03c2 }, { 0x02ee, 0x03ca }, { 0x0325, 0x0402 },
	{ 0x0333, 0x0450 }, { 0x0334, 0x0452 }, { 0x1ef2, 0x2011 }, { 0x1ef4, 0x2017 },
	{ 0x1ef5, 0x201a }, { 0x1ef7, 0x201e }, { 0x1efe, 0x2027 }, { 0x1f07, 0x2031 },
	{ 0x1f08, 0x2034 }, { 0x1f09, 0x2036 }, { 0x1f0e, 0x203c }, { 0x1f7e, 0x20ad },
	{ 0x1fd4, 0x2104 }, { 0x1fd5, 0x2106 }, { 0x1fd8, 0x210a }, { 0x1fe4, 0x2117 },
	{ 0x1fee, 0x2122 }, { 0x202c, 0x216c }, { 0x2030, 0x217a }, { 0x2046, 0x2194 },
	{ 0x2048, 0x219a }, { 0x20b6, 0x2209 }, { 0x20bc, 0x2210 }, { 0x20bd, 0x2212 },
	{ 0x20c0, 0x2216 }, { 0x20c4, 0x221b }, { 0x20c6, 0x2221 }, { 0x20c8, 0x2224 },
	{ 0x20c9, 0x2226 }, { 0x20ca, 0x222c }, { 0x20cc, 0x222f }, { 0x20d1, 0x2238 },
	{ 0x20d6, 0x223e }, { 0x20e0, 0x2249 }, { 0x20e3, 0x224d }, { 0x20e8, 0x2253 },
	{ 0x20f5, 0x2262 }, { 0x20f7, 0x2268 }, { 0x20fd, 0x2270 }, { 0x2122, 0x2296 },
	{ 0x2125, 0x229a }, { 0x2130, 0x22a6 }, { 0x2149, 0x22c0 }, { 0x219b, 0x2313 },
	{ 0x22e8, 0x246a }, { 0x22f2, 0x249c }, { 0x2356, 0x254c }, { 0x235a, 0x2574 },
	{ 0x2367, 0x2590 }, { 0x236a, 0x2596 }, { 0x2374, 0x25a2 }, { 0x2384, 0x25b4 },
	{ 0x238c, 0x25be }, { 0x2394, 0x25c8 }, { 0x2397, 0x25cc }, { 0x2399, 0x25d0 },
	{ 0x23ab, 0x25e6 }, { 0x23ca, 0x2607 }, { 0x23cc, 0x260a }, { 0x2402, 0x2641 },
	{ 0x2403, 0x2643 }, { 0x2c41, 0x2e82 }, { 0x2c43, 0x2e85 }, { 0x2c46, 0x2e89 },
	{ 0x2c48, 0x2e8d }, { 0x2c52, 0x2e98 }, { 0x2c61, 0x2ea8 }, { 0x2c63, 0x2eab },
	{ 0x2c66, 0x2eaf }, { 0x2c6a, 0x2eb4 }, { 0x2c6c, 0x2eb8 }, { 0x2c6f, 0x2ebc },
	{ 0x2c7d, 0x2ecb }, { 0x2da2, 0x2ffc }, { 0x2da6, 0x3004 }, { 0x2da7, 0x3018 },
	{ 0x2dac, 0x301f }, { 0x2dae, 0x302a }, { 0x2dc2, 0x303f }, { 0x2dc4, 0x3094 },
	{ 0x2dcb, 0x309f }, { 0x2dcd, 0x30f7 }, { 0x2dd2, 0x30ff }, { 0x2dd8, 0x312a },
	{ 0x2ece, 0x322a }, { 0x2ed5, 0x3232 }, { 0x2f46, 0x32a4 }, { 0x3030, 0x3390 },
	{ 0x303c, 0x339f }, { 0x303e, 0x33a2 }, { 0x3060, 0x33c5 }, { 0x3069, 0x33cf },
	{ 0x306b, 0x33d3 }, { 0x306d, 0x33d6 }, { 0x3097, 0x0000 },
};

/* U+9FA6-U+FFFF */
static const CesIndexUcsDB_t cesGB18030_4Byte_Ucs_9FA6_FFFF[] = {
	{ 0x4a63, 0x9fa6 }, { 0x82bd, 0xe76c }, { 0x82be, 0xe7c8 }, { 0x82bf, 0xe7e7 },
	{ 0x82cc, 0xe815 }, { 0x82cd, 0xe819 }, { 0x82d2, 0xe81f }, { 0x82d9, 0xe827 },
	{ 0x82dd, 0xe82d }, { 0x82e1, 0xe833 }, { 0x82e9, 0xe83c }, { 0x82f0, 0xe844 },
	{ 0x8300, 0xe856 }, { 0x830e, 0xe865 }, { 0x93d5, 0xf92d }, { 0x9421, 0xf97a },
	{ 0x943c, 0xf996 }, { 0x948d, 0xf9e8 }, { 0x9496, 0xf9f2 }, { 0x94b0, 0xfa10 },
	{ 0x94b1, 0xfa12 }, { 0x94b2, 0xfa15 }, { 0x94b5, 0xfa19 }, { 0x94bb, 0xfa22 },
	{ 0x94bc, 0xfa25 }, { 0x94be, 0xfa2a }, { 0x98c4, 0xfe32 }, { 0x98c5, 0xfe45 },
	{ 0x98c9, 0xfe53 }, { 0x98ca, 0xfe58 }, { 0x98cb, 0xfe67 }, { 0x98cc, 0xfe6c },
	{ 0x9961, 0xff5f }, { 0x99e2, 0xffe6 }, { 0x99fc, 0x0000 },
};

/* a four-byte code's code point by a table of runs; 0 for none */
uint32_t _sceCesGBIndexDB_getUcsFromGB4byte(const CesIndexUcsDB_t *dbTop,
					    const CesIndexUcsDB_t *dbLast, uint32_t gbCode)
{
	if (dbTop != NULL) {
		uint8_t gbc1 = gbCode >> 24;
		uint8_t gbc2 = gbCode >> 16;
		uint8_t gbc3 = gbCode >> 8;
		uint8_t gbc4 = gbCode;
		uint32_t i = CES_GB4_INDEX(gbc1, gbc2, gbc3, gbc4);

		return _sceCesIndexUcsDB_getUcs4(dbTop, dbLast, i);
	}
	return 0;
}

/* a code point's four-byte code by a table of runs; 0 for none */
uint32_t _sceCesGBIndexDB_getGB4byteFromUcs(const CesIndexUcsDB_t *dbTop,
					    const CesIndexUcsDB_t *dbLast, uint32_t ucs4)
{
	if (dbTop != NULL) {
		uint32_t i = _sceCesIndexUcsDB_getIndex(dbTop, dbLast, ucs4);

		if ((int)i >= 0) {
			uint32_t gbc = CES_GB4_CODE(i, 0x81308130);

			return gbc;
		}
	}
	return 0;
}

int _sceCesGB4byteToUcs4_UcsBmp(const SceCesGbUcsProfile *prof, uint32_t gbCode, uint32_t *ucs4)
{
	const CesIndexUcsDB_t *dbTop = NULL;
	const CesIndexUcsDB_t *dbLast;

	if (gbCode <= 0x8139ee38) {
		/* U+0080-U+33FF */
		if ((prof->types & CES_GB_TYPES_4BYTE_BMP) == 0) {
			*ucs4 = 0;
			return SCE_CES_ERROR_UNASSIGNED_CODE;
		}
		/* GB18030-2005 maps 0x8135F437 to U+E7C7 */
		if (gbCode == 0x8135f437 && prof->types >= CES_GB_TYPES_EDITION_2005) {
			*ucs4 = 0xe7c7;
			return SCE_CES_OK;
		}
		dbTop = cesGB18030_4Byte_Ucs_0080_33FF;
		dbLast = CES_INDEX_DB_LAST(cesGB18030_4Byte_Ucs_0080_33FF);
	} else if (gbCode <= 0x82358f32) {
		/* U+3400-U+4DFF; 0x82358739 on is U+4DB6, past CJK Extension A */
		if (gbCode >= 0x82358739 && (prof->types & CES_GB_TYPES_4BYTE_CJK_UIE_A) != 0) {
			*ucs4 = 0;
			return SCE_CES_ERROR_UNASSIGNED_CODE;
		}
		dbTop = cesGB18030_4Byte_Ucs_3400_4DFF;
		dbLast = CES_INDEX_DB_LAST(cesGB18030_4Byte_Ucs_3400_4DFF);
	} else if (gbCode <= 0x8431a439) {
		/* U+9FA6-U+FFFF */
		if ((prof->types & CES_GB_TYPES_4BYTE_BMP) == 0) {
			*ucs4 = 0;
			return SCE_CES_ERROR_UNASSIGNED_CODE;
		}
		dbTop = cesGB18030_4Byte_Ucs_9FA6_FFFF;
		dbLast = CES_INDEX_DB_LAST(cesGB18030_4Byte_Ucs_9FA6_FFFF);
	} else if (gbCode >= 0x90308130 && gbCode <= 0xe3329a35) {
		/* planes 1-16 (U+10000-U+10FFFF); plane 2 from 0x95328236 */
		uint32_t types = prof->types;

		if ((types & CES_GB_TYPES_4BYTE_PLANES_ALL) == CES_GB_TYPES_4BYTE_PLANES_ALL ||
		    ((types & CES_GB_TYPES_4BYTE_SIP) != 0 && gbCode >= 0x95328236 &&
		     gbCode <= ((types & CES_GB_TYPES_4BYTE_CJK_UIE_A) != 0 ? 0x9835f336 :
										0x9a348431))) {
			uint8_t gbc1 = gbCode >> 24;
			uint8_t gbc2 = gbCode >> 16;
			uint8_t gbc3 = gbCode >> 8;
			uint8_t gbc4 = gbCode;
			uint32_t i = CES_GB4_INDEX(gbc1 - 0x0f, gbc2, gbc3, gbc4);

			*ucs4 = i + 0x10000;
			return SCE_CES_OK;
		}
	}
	*ucs4 = _sceCesGBIndexDB_getUcsFromGB4byte(dbTop, dbLast, gbCode);
	if (*ucs4 != 0)
		return SCE_CES_OK;
	return SCE_CES_ERROR_UNASSIGNED_CODE;
}

uint32_t _sceCesGBGet4byteCodeFromUcs4_UcsBmp(const SceCesGbUcsProfile *prof, uint32_t ucs4)
{
	const CesIndexUcsDB_t *dbTop = NULL;
	const CesIndexUcsDB_t *dbLast;

	if (ucs4 <= 0x33ff) {
		if ((prof->types & CES_GB_TYPES_4BYTE_BMP) == 0)
			return 0;
		dbTop = cesGB18030_4Byte_Ucs_0080_33FF;
		dbLast = CES_INDEX_DB_LAST(cesGB18030_4Byte_Ucs_0080_33FF);
	} else if (ucs4 <= 0x4dff) {
		/* U+4DB6 on are past CJK Extension A */
		if (ucs4 >= 0x4db6 && (prof->types & CES_GB_TYPES_4BYTE_CJK_UIE_A) != 0)
			return 0;
		dbTop = cesGB18030_4Byte_Ucs_3400_4DFF;
		dbLast = CES_INDEX_DB_LAST(cesGB18030_4Byte_Ucs_3400_4DFF);
	} else if (ucs4 <= 0xffff) {
		if ((prof->types & CES_GB_TYPES_4BYTE_BMP) == 0)
			return 0;
		dbTop = cesGB18030_4Byte_Ucs_9FA6_FFFF;
		dbLast = CES_INDEX_DB_LAST(cesGB18030_4Byte_Ucs_9FA6_FFFF);
	} else if (ucs4 <= 0x10ffff) {
		uint32_t types = prof->types;

		/* plane 2 to U+2A6D6 (CJK Extension B) or U+2FFFF */
		if ((types & CES_GB_TYPES_4BYTE_PLANES_ALL) == CES_GB_TYPES_4BYTE_PLANES_ALL ||
		    ((types & CES_GB_TYPES_4BYTE_SIP) != 0 && ucs4 >= 0x20000 &&
		     ucs4 <= ((types & CES_GB_TYPES_4BYTE_CJK_UIE_A) != 0 ? 0x2a6d6 : 0x2ffff))) {
			uint32_t i = ucs4 - 0x10000;
			uint32_t gbc = CES_GB4_CODE(i, 0x90308130);

			return gbc;
		}
		return 0;
	}
	/* none past U+10FFFF: no table */
	return _sceCesGBIndexDB_getGB4byteFromUcs(dbTop, dbLast, ucs4);
}
