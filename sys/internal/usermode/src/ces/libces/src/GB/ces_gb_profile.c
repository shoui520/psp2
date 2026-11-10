/*
 * libSceCes's ces_gb_profile.c (Sony's internal/usermode/src/ces/libces/src/GB): a GB profile's
 * settings: the user-defined areas' mapping and GB18030's four-byte range.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_gb_profile.h"

/* the SDK's values (ces/libces_gb.h) */
#define SCE_CES_GB_UDA_MAPPING_NONE	(0)
#define SCE_CES_GB_UDA_MAPPING_UCS_PUA	(1)
#define SCE_CES_GB_4BYTE_UCS_CJK_UIE_A	(0)
#define SCE_CES_GB_4BYTE_UCS_BMP	(1)
#define SCE_CES_GB_4BYTE_UCS_CJK_UIE_AB	(2)
#define SCE_CES_GB_4BYTE_UCS_BMP_SIP	(3)
#define SCE_CES_GB_4BYTE_UCS_CODE_RANGE	(7)

int sceCesGbUcsProfileSetUdaMapping(SceCesGbUcsProfile *prof, int gbUda)
{
	if (prof != NULL &&
	    (prof->cef_type == CES_GB_TYPE_GBK || prof->cef_type == CES_GB_TYPE_GB18030)) {
		uint32_t types = prof->types & ~CES_GB_TYPES_UDA_UCS_PUA;

		switch (gbUda) {
		case SCE_CES_GB_UDA_MAPPING_UCS_PUA:
			types |= CES_GB_TYPES_UDA_UCS_PUA;
			/* FALLTHROUGH */
		case SCE_CES_GB_UDA_MAPPING_NONE:
			prof->types = types;
			return SCE_CES_OK;
		}
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	return SCE_CES_ERROR_INVALID_PROFILE;
}

int sceCesGbUcsProfileSet4ByteCharRange(SceCesGbUcsProfile *prof, int gb4byte)
{
	if (prof != NULL && prof->cef_type == CES_GB_TYPE_GB18030) {
		uint32_t types = prof->types;

		/* GB18030-2005 maps the four-byte codes of plane 2 */
		if (types >= CES_GB_TYPES_EDITION_2005 && gb4byte < SCE_CES_GB_4BYTE_UCS_BMP_SIP)
			return SCE_CES_ERROR_INVALID_PARAMETER;
		types &= ~CES_GB_TYPES_4BYTE_RANGE;
		switch (gb4byte) {
		case SCE_CES_GB_4BYTE_UCS_CJK_UIE_A:
			types |= CES_GB_TYPES_4BYTE_CJK_UIE_A;
			break;
		case SCE_CES_GB_4BYTE_UCS_BMP:
			types |= CES_GB_TYPES_4BYTE_BMP;
			break;
		case SCE_CES_GB_4BYTE_UCS_CJK_UIE_AB:
			types |= CES_GB_TYPES_4BYTE_SIP | CES_GB_TYPES_4BYTE_CJK_UIE_A;
			break;
		case SCE_CES_GB_4BYTE_UCS_BMP_SIP:
			types |= CES_GB_TYPES_4BYTE_SIP | CES_GB_TYPES_4BYTE_BMP;
			break;
		case SCE_CES_GB_4BYTE_UCS_CODE_RANGE:
			types |= CES_GB_TYPES_4BYTE_PLANES | CES_GB_TYPES_4BYTE_SIP |
				 CES_GB_TYPES_4BYTE_BMP;
			break;
		default:
			return SCE_CES_ERROR_INVALID_PARAMETER;
		}
		prof->types = types;
		return SCE_CES_OK;
	}
	return SCE_CES_ERROR_INVALID_PROFILE;
}
