/*
 * libSceCes's ces_mbcs_context.c (Sony's internal/usermode/src/ces/libces/src/MBCS/ces_mbcs_context.c,
 * the SDK's static library, built with SNC): the string conversion context between a multi-byte
 * character set and the Unicode forms. SNC's linker keeps only the routines a module calls; the
 * tree has the ones libpaf calls. Names of the routines, their parameters and the table are
 * Sony's (the library's DWARF). libpaf links an older build of the library than SDK 3.570.011:
 * its table carries the replacement character routine, which 3.570.011's leaves empty.
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_mbcs_context.h"

/* a byte order setting: the source's in the high nibble, the destination's in the low one */
#define CES_UCS_ENDIAN_LE_LE	0x11

/* the type of the character set a context's table converts, a multi-byte one */
#define CES_TYPE_MBCS		0x80

/* the most bytes of a replacement character */
#define CES_MBC_REPLACE_MAX	4

static const SceCesOtherCesFuncs _cesMbcsFuncs = {
	.cesType = CES_TYPE_MBCS,
	.SetReplacementCharUCode =
		(SceCesSetReplacementCharUCodeFunc *)sceCesMbcsUcsSetMbcsReplacementCharUCode,
};

int _sceCesMbcsUcsContextInit(SceCesMbcsUcsContext *context, const SceCesMbcsUcsProfile *prof)
{
	uint32_t *p;
	uint32_t n;

	if (context != NULL) {
		n = sizeof(*context) / sizeof(uint32_t);
		p = (uint32_t *)context + n;
		for (; n != 0; n--)
			*--p = 0;
		if (prof != NULL && prof->cef_type != 0) {
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
			context->Profile = prof;
			context->Callbacks = NULL;
			context->OtherCesFuncs = &_cesMbcsFuncs;
			return SCE_CES_OK;
		}
	}
	return SCE_CES_ERROR_INVALID_PARAMETER;
}

int sceCesMbcsUcsSetMbcsReplacementCharUCode(SceCesMbcsUcsContext *context, uint32_t ucode)
{
	if (context != NULL && context->Profile != NULL) {
		uint32_t mbcLen;
		int ret;

		ret = sceCesUtf32ToMbc(ucode, context->Profile, context->State.replaceMbc,
				      CES_MBC_REPLACE_MAX, &mbcLen);
		{
			uint32_t code = 0;
			int i;

			if (ret != SCE_CES_OK) {
				context->State.replaceMbc[0] = 0;
			} else {
				for (i = 0; i < CES_MBC_REPLACE_MAX; i++) {
					if (mbcLen == 0)
						break;
					mbcLen--;
					code = code << 8 | context->State.replaceMbc[i];
				}
			}
			context->State.replaceCode2 = code;
		}
		return ret;
	}
	return SCE_CES_ERROR_INVALID_PARAMETER;
}
