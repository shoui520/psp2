/*
 * libSceCes's ces_ucs_context.c (Sony's internal/usermode/src/ces/libces/src/UCS/ces_ucs_context.c,
 * the SDK's static library, built with SNC): the UCS string conversion context. SNC's linker
 * keeps only the routines a module calls; the tree has the ones libpaf calls. Names of the
 * routines and their parameters are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_ucs_context.h"

/* a byte order setting: the source's in the high nibble, the destination's in the low one */
#define CES_UCS_ENDIAN_LE_LE	0x11

int sceCesUcsContextInit(SceCesUcsContext *context)
{
	uint32_t *p;
	uint32_t n;

	if (context != NULL) {
		n = sizeof(*context) / sizeof(uint32_t);
		p = (uint32_t *)context + n;
		for (; n != 0; n--)
			*--p = 0;
		_sceCesStrContextInit((CesStrContext *)context);
		context->UcsSetting.utf32 = CES_UCS_ENDIAN_LE_LE;
		context->UcsSetting.utf16 = CES_UCS_ENDIAN_LE_LE;
		context->UcsSetting.ucs4 = CES_UCS_ENDIAN_LE_LE;
		context->UcsSetting.ucs2 = CES_UCS_ENDIAN_LE_LE;
		context->UcsSetting.outputBom = CES_ENABLE;
		context->UcsSetting.detectBom = CES_ENABLE;
		context->UcsSetting.padding = 0;
		context->UcsSetting.utf7 = 0;
		context->State.returnValue = SCE_CES_OK;
		context->State.ctx.bitsValue = 1;
		context->State.codeBuffer = NULL;
		context->State.codeLen = 0;
		context->State.maxCharCount = 0;
		context->State.setCharCount = 0;
		context->State.replaceCode = 0xfffd;
		context->State.replaceCode2 = 0;
		context->State.replaceMbc[3] = 0;
		context->State.replaceMbc[2] = 0;
		context->State.replaceMbc[1] = 0;
		context->State.replaceMbc[0] = 0;
		context->State.callbackArg = NULL;
		context->State.callback = NULL;
		context->UcsCallbacks = NULL;
		return SCE_CES_OK;
	}
	return SCE_CES_ERROR_INVALID_PARAMETER;
}
