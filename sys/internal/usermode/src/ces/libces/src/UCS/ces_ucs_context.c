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

int sceCesUcsContextInitCopy(SceCesUcsContext *context, const SceCesUcsContext *src)
{
	uint32_t *dst;
	const uint32_t *s;
	uint32_t n;

	if (context == NULL || src == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	n = sizeof(*context) / sizeof(uint32_t);
	dst = (uint32_t *)context + n;
	s = (const uint32_t *)src + n;
	for (; n != 0; n--)
		*--dst = *--s;
	return SCE_CES_OK;
}

int sceCesUcsSetUtf16StrEndian(SceCesUcsContext *context, SceCesEndianParam srcEndian,
			       SceCesEndianParam dstEndian)
{
	uint8_t val;

	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	switch (srcEndian) {
	case CES_ENDIAN_BE:
		val = 0x00;
		break;
	case CES_ENDIAN_LE:
	case CES_ENDIAN_SYS:
		val = 0x10;
		break;
	case CES_ENDIAN_STAY:
		val = context->UcsSetting.utf16 & 0xf0;
		break;
	default:
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	switch (dstEndian) {
	case CES_ENDIAN_BE:
		break;
	case CES_ENDIAN_LE:
	case CES_ENDIAN_SYS:
		val |= 0x01;
		break;
	case CES_ENDIAN_STAY:
		val |= context->UcsSetting.utf16 & 0x0f;
		break;
	default:
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	context->UcsSetting.utf16 = val;
	return SCE_CES_OK;
}

static __attribute__((noinline)) int cesUcsGetUtfStrEndian(uint8_t setting,
							   SceCesEndianParam *srcEndian,
							   SceCesEndianParam *dstEndian)
{
	if (srcEndian != NULL) {
		uint32_t hi = setting >> 4;
		if (hi == 0) {
			*srcEndian = CES_ENDIAN_BE;
		} else if (hi == 1) {
			*srcEndian = CES_ENDIAN_LE;
		} else {
			*srcEndian = CES_ENDIAN_LE;
			return SCE_CES_ERROR_INVALID_PARAMETER;
		}
	}
	if (dstEndian != NULL) {
		uint32_t lo = setting & 0xf;
		if (lo == 0) {
			*dstEndian = CES_ENDIAN_BE;
		} else if (lo == 1) {
			*dstEndian = CES_ENDIAN_LE;
		} else {
			*dstEndian = CES_ENDIAN_LE;
			return SCE_CES_ERROR_INVALID_PARAMETER;
		}
	}
	return SCE_CES_OK;
}

int sceCesUcsGetUtf16StrEndian(const SceCesUcsContext *context, SceCesEndianParam *srcEndian,
			       SceCesEndianParam *dstEndian)
{
	if (context == NULL || (srcEndian == NULL && dstEndian == NULL))
		return SCE_CES_ERROR_INVALID_PARAMETER;
	return cesUcsGetUtfStrEndian(context->UcsSetting.utf16, srcEndian, dstEndian);
}

int sceCesUcsSetEndianUtf16(SceCesUcsContext *context, SceCesEndianParam endian)
{
	return sceCesUcsSetUtf16StrEndian(context, endian, endian);
}

int sceCesUcsSetUtf32StrEndian(SceCesUcsContext *context, SceCesEndianParam srcEndian,
			       SceCesEndianParam dstEndian)
{
	uint8_t val;

	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	switch (srcEndian) {
	case CES_ENDIAN_BE:
		val = 0x00;
		break;
	case CES_ENDIAN_LE:
	case CES_ENDIAN_SYS:
		val = 0x10;
		break;
	case CES_ENDIAN_STAY:
		val = context->UcsSetting.utf32 & 0xf0;
		break;
	default:
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	switch (dstEndian) {
	case CES_ENDIAN_BE:
		break;
	case CES_ENDIAN_LE:
	case CES_ENDIAN_SYS:
		val |= 0x01;
		break;
	case CES_ENDIAN_STAY:
		val |= context->UcsSetting.utf32 & 0x0f;
		break;
	default:
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	context->UcsSetting.utf32 = val;
	return SCE_CES_OK;
}

int sceCesUcsGetUtf32StrEndian(const SceCesUcsContext *context, SceCesEndianParam *srcEndian,
			       SceCesEndianParam *dstEndian)
{
	if (context == NULL || (srcEndian == NULL && dstEndian == NULL))
		return SCE_CES_ERROR_INVALID_PARAMETER;
	return cesUcsGetUtfStrEndian(context->UcsSetting.utf32, srcEndian, dstEndian);
}

int sceCesUcsSetEndianUtf32(SceCesUcsContext *context, SceCesEndianParam endian)
{
	return sceCesUcsSetUtf32StrEndian(context, endian, endian);
}

int sceCesUcsSetPolicyDetectBom(SceCesUcsContext *context, int policy)
{
	if (context != NULL) {
		if (policy == 0) {
			context->UcsSetting.detectBom = 0;
			return SCE_CES_OK;
		}
		if (policy == 1) {
			context->UcsSetting.detectBom = 1;
			return SCE_CES_OK;
		}
	}
	return SCE_CES_ERROR_INVALID_PARAMETER;
}

int sceCesUcsSetPolicyOutputBom(SceCesUcsContext *context, int policy)
{
	if (context != NULL && (policy >= 0 && policy <= 3)) {
		context->UcsSetting.outputBom = (uint8_t)policy;
		return SCE_CES_OK;
	}
	return SCE_CES_ERROR_INVALID_PARAMETER;
}

int sceCesUcsBindCallbacks(SceCesUcsContext *context, const CesUcsCallbacks *callbacks)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->UcsCallbacks = callbacks;
	return SCE_CES_OK;
}

int sceCesUcsSetReplacementCharCode(SceCesUcsContext *context, uint32_t ucode)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	if (ucode > 0x10ffff || (ucode >= 0xd800 && ucode <= 0xdfff)) {
		context->State.replaceCode = 0;
		return SCE_CES_ERROR_ILLEGAL_CODE;
	}
	context->State.replaceCode = ucode;
	return SCE_CES_OK;
}

int sceCesUcsGetReplacementCharCode(const SceCesUcsContext *context, uint32_t *ucode)
{
	if (context == NULL || ucode == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	*ucode = context->State.replaceCode;
	return SCE_CES_OK;
}

int sceCesSetReplacementCharUCode(SceCesUcsContext *context, uint32_t ucode)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	if (context->OtherCesFuncs != NULL) {
		if (context->OtherCesFuncs->SetReplacementCharUCode == NULL)
			return SCE_CES_ERROR_INVALID_PARAMETER;
		context->OtherCesFuncs->SetReplacementCharUCode(context, ucode);
	}
	if (ucode > 0x10ffff || (ucode >= 0xd800 && ucode <= 0xdfff)) {
		context->State.replaceCode = 0;
		return SCE_CES_ERROR_ILLEGAL_CODE;
	}
	context->State.replaceCode = ucode;
	return SCE_CES_OK;
}

int sceCesSetUtf7EncodingForm(SceCesUcsContext *context, uint32_t form)
{
	if (context == NULL || form > 1)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->UcsSetting.utf7 = (uint8_t)form;
	return SCE_CES_OK;
}
