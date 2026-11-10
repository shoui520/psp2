/*
 * libSceCes's GB profiles (Sony's internal/usermode/src/ces/libces/src/GB/ces_gb_profile.h): one
 * profile for GBK, GB18030 and EUC-CN, an MBCS profile head with the single-byte Roman set and the
 * conversions by GB area. Laid out and named from the library's debug information (SDK 3.570.011
 * libSceCes.a). A GB code is its bytes, first byte highest (0xB0A1, 0x81308130).
 */
#ifndef _CES_GB_PROFILE_H
#define _CES_GB_PROFILE_H

#include <stdint.h>
#include "../MBCS/ces_mbcs_profile.h"
#include "../SBCS/ces_sbcs_profile.h"
#include "../DBCS/DBCS.h"

struct SceCesGbUcsProfile;

/* a code of one GB area to UCS-4 */
typedef int _sceCesGbUcsFunc(const struct SceCesGbUcsProfile *prof, uint32_t gbc, uint32_t *ucs4);
/* a UCS-4 code point's GB code; 0 for none */
typedef uint32_t _sceCesGBCodeFormUcsFunc(const struct SceCesGbUcsProfile *prof, uint32_t ucs4);

/* the profiles' cef_type; the names are ours */
#define CES_GB_TYPE_EUC_CN	0x2180	/* EUC-CN: one and two bytes, 0x80-0xA0 single */
#define CES_GB_TYPE_GBK		0x6180	/* GBK: one and two bytes */
#define CES_GB_TYPE_GB18030	0xe180	/* GB18030: one, two and four bytes */

/*
 * the variant bits of types; the names are ours. Bits 16 on are the edition (the SDK's profiles:
 * 1 GB2312, 2 GBK, 4 GB18030-2000, 5 GB18030-2005; from 5 the four-byte range is BMP and beyond).
 */
#define CES_GB_TYPES_EURO		0x00000001	/* 0x80 is the euro sign */
#define CES_GB_TYPES_UDA_UCS_PUA	0x00000004	/* user-defined areas to the PUA */
#define CES_GB_TYPES_PUA		0x00000008	/* the codes GB18030-2000 maps into the PUA */
#define CES_GB_TYPES_4BYTE_BMP		0x00000010	/* four-byte codes of the BMP */
#define CES_GB_TYPES_4BYTE_SIP		0x00000020	/* four-byte codes of plane 2 */
#define CES_GB_TYPES_4BYTE_PLANES	0x00000040	/* four-byte codes of planes 1-16 */
#define CES_GB_TYPES_CP936		0x00000100	/* Microsoft's code page 936 */
#define CES_GB_TYPES_4BYTE_CJK_UIE_A	0x00000400	/* four-byte codes of CJK Extension A only */
#define CES_GB_TYPES_4BYTE_PLANES_ALL	0x00000070	/* all four-byte codes */
#define CES_GB_TYPES_4BYTE_RANGE	0x00000670
#define CES_GB_TYPES_EDITION_GB2312	0x00010000
#define CES_GB_TYPES_EDITION_GBK	0x00020000
#define CES_GB_TYPES_EDITION_2000	0x00040000
#define CES_GB_TYPES_EDITION_2005	0x00050000

/* the euro sign */
#define CES_UCS_EURO_SIGN	0x20ac

typedef struct SceCesGbUcsProfile {
	uint32_t cef_type;
	uint32_t types;
	_sceCesMbcsToUcs4Func *mbcs_to_ucs4;
	_sceCesUcs4ToMbcsFunc *ucs4_to_mbcs;
	const SceCesSbcsUcsProfile *Roma;
	_sceCesGbUcsFunc *gb2byte_1_to_ucs4;
	_sceCesGbUcsFunc *gb2byte_2_to_ucs4;
	_sceCesGbUcsFunc *gb2byte_3_to_ucs4;
	_sceCesGbUcsFunc *gb2byte_4_to_ucs4;
	_sceCesGbUcsFunc *gb2byte_5_to_ucs4;
	_sceCesGbUcsFunc *gb4byte_to_ucs4;
	_sceCesGBCodeFormUcsFunc *gbCode_from_ucs4;
	_sceCesGBCodeFormUcsFunc *gb4byte_from_ucs4;
} SceCesGbUcsProfile, SceCesEucCnUcsProfile;

/* the profiles */
SceCesGbUcsProfile *sceCesUcsProfileInitGb18030_2000(SceCesGbUcsProfile *prof);
int sceCesGbUcsProfileSetUdaMapping(SceCesGbUcsProfile *prof, int gbUda);
int sceCesGbUcsProfileSet4ByteCharRange(SceCesGbUcsProfile *prof, int gb4byte);

/* GB and UCS */
int sceCesGbToUcs4(const SceCesGbUcsProfile *prof, const uint8_t *gbBuf, uint32_t gbMax,
		   uint32_t *gbSize, uint32_t *ucs4);
int sceCesUcs4ToGb(uint32_t ucs4, const SceCesGbUcsProfile *prof, uint8_t *gbBuf, uint32_t gbMax,
		   uint32_t *gbSize);
int sceCesUtf16ToGb(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len,
		    const SceCesGbUcsProfile *prof, uint8_t *gbBuf, uint32_t gbMax, uint32_t *gbSize);

/* a two-byte GBK code's index (the GB2312 area first) and back */
uint16_t _sceCesGBGetIndex(uint16_t gbCode);
uint16_t _sceCesGBGetIndexCode(uint16_t index);

/* the two-byte areas to UCS-4 */
int _sceCesGB2byte1_ToUcs4(const SceCesGbUcsProfile *gbProf, uint32_t gbc, uint32_t *ucs4);
int _sceCesGB2byte2_ToUcs4(const SceCesGbUcsProfile *gbProf, uint32_t gbc, uint32_t *ucs4);
int _sceCesGB2byte3_ToUcs4(const SceCesGbUcsProfile *prof, uint32_t gbc, uint32_t *ucs4);
int _sceCesGB2byte4_ToUcs4(const SceCesGbUcsProfile *prof, uint32_t gbc, uint32_t *ucs4);
int _sceCesGB2byte5_ToUcs4(const SceCesGbUcsProfile *gbProf, uint32_t gbc, uint32_t *ucs4);
int _sceCesGB4byteToUcs4_UcsBmp(const SceCesGbUcsProfile *prof, uint32_t gbCode, uint32_t *ucs4);

/* UCS-4 to the areas' codes */
uint32_t _sceCesGB18030GetCodeFromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4);
uint32_t _sceCesGBKGetCodeFromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4);
uint32_t _sceCesGBCode_FromUcsPrivate(const SceCesGbUcsProfile *prof, uint32_t ucs4);
uint32_t _sceCesGBGet2byteSect1Code_FromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4);
uint32_t _sceCesGBGet2byteSect2Code_FromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4);
uint32_t _sceCesGBGet2byteSect3Code_FromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4);
uint32_t _sceCesGBGet2byteSect4Code_FromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4);
uint32_t _sceCesGBGet2byteSect5Code_FromUcs4(const SceCesGbUcsProfile *gbProf, uint32_t ucs4);
uint32_t _sceCesGBGet4byteCodeFromUcs4_UcsBmp(const SceCesGbUcsProfile *prof, uint32_t ucs4);

#endif /* _CES_GB_PROFILE_H */
