/*
 * SceGxm: small helpers shared by the program code.
 */

#include "gxm_internal.h"

/* bytes per component */
uint32_t gxmAttributeFormatGetSize(SceUInt8 format)
{
	switch (format) {
	case SCE_GXM_ATTRIBUTE_FORMAT_U8:
	case SCE_GXM_ATTRIBUTE_FORMAT_S8:
	case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
	case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
		return 1;
	case SCE_GXM_ATTRIBUTE_FORMAT_U16:
	case SCE_GXM_ATTRIBUTE_FORMAT_S16:
	case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
	case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
	case SCE_GXM_ATTRIBUTE_FORMAT_F16:
		return 2;
	case SCE_GXM_ATTRIBUTE_FORMAT_F32:
	case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED:
		return 4;
	default:
		GXM_DBG_FAIL(91, "unrecognised vertex attribute format");
		return 0;
	}
}
