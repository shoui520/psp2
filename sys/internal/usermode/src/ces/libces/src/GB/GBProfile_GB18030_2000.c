/*
 * libSceCes's GBProfile_GB18030_2000.c (Sony's internal/usermode/src/ces/libces/src/GB): the
 * profile of GB18030-2000, its four-byte codes over the BMP.
 */
#include <stddef.h>
#include <stdint.h>
#include "ces_gb_profile.h"

SceCesGbUcsProfile *sceCesUcsProfileInitGb18030_2000(SceCesGbUcsProfile *prof)
{
	prof->types = CES_GB_TYPES_EDITION_2000 | CES_GB_TYPES_4BYTE_BMP | CES_GB_TYPES_PUA |
		      CES_GB_TYPES_UDA_UCS_PUA;
	prof->cef_type = CES_GB_TYPE_GB18030;
	prof->mbcs_to_ucs4 = (_sceCesMbcsToUcs4Func *)sceCesGbToUcs4;
	prof->ucs4_to_mbcs = (_sceCesUcs4ToMbcsFunc *)sceCesUcs4ToGb;
	prof->Roma = sceCesRefersUcsProfileAscii();
	prof->gb2byte_1_to_ucs4 = _sceCesGB2byte1_ToUcs4;
	prof->gb2byte_2_to_ucs4 = _sceCesGB2byte2_ToUcs4;
	prof->gb2byte_3_to_ucs4 = _sceCesGB2byte3_ToUcs4;
	prof->gb2byte_4_to_ucs4 = _sceCesGB2byte4_ToUcs4;
	prof->gb2byte_5_to_ucs4 = _sceCesGB2byte5_ToUcs4;
	prof->gb4byte_to_ucs4 = _sceCesGB4byteToUcs4_UcsBmp;
	prof->gbCode_from_ucs4 = _sceCesGB18030GetCodeFromUcs4;
	prof->gb4byte_from_ucs4 = _sceCesGBGet4byteCodeFromUcs4_UcsBmp;
	return prof;
}
