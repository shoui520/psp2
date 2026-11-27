/*
 * libSceCes's ces_ucs_callback.c (Sony's internal/usermode/src/ces/libces/src/UCS/ces_ucs_callback.c,
 * the SDK's static library, built with SNC): the error callbacks of the conversions between the
 * Unicode forms, and the sets a context's error operations select. Names of the routines and
 * their parameters are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "../ces_context.h"
#include "ces_ucs.h"
#include "ces_ucs_callback.h"

#define CES_UCS_CODE_LIMIT	0x110000u
#define CES_UCS2_CODE_MAX	0xffffu
#define CES_UCS4_CODE_MAX	0x7fffffffu

int sceCesUcsCallbacksInit(CesUcsCallbacks *callbacks)
{
	uint32_t *p;
	uint32_t n;

	if (callbacks == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	n = sizeof(*callbacks) / sizeof(uint32_t);
	p = (uint32_t *)callbacks + n;
	for (; n != 0; n--)
		*--p = 0;
	return SCE_CES_OK;
}

#define DEFINE_SET_CB_IE_IC(Name, Field, CbType) \
int sceCesUcsCallbacksSet##Name(CesUcsCallbacks *callbacks, int errorType, CbType *cb) \
{ \
	if (callbacks == NULL) \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	switch (errorType) { \
	case SCE_CES_ERROR_INVALID_ENCODE: \
		callbacks->Field.InvalidEncode = cb; \
		break; \
	case SCE_CES_ERROR_ILLEGAL_CODE: \
		callbacks->Field.IllegalCode = cb; \
		break; \
	case SCE_CES_ERROR_ALL: \
		callbacks->Field.InvalidEncode = cb; \
		callbacks->Field.IllegalCode = cb; \
		break; \
	default: \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	} \
	return SCE_CES_OK; \
}

#define DEFINE_SET_CB_IE_IC_OOR(Name, Field, CbType) \
int sceCesUcsCallbacksSet##Name(CesUcsCallbacks *callbacks, int errorType, CbType *cb) \
{ \
	if (callbacks == NULL) \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	switch (errorType) { \
	case SCE_CES_ERROR_INVALID_ENCODE: \
		callbacks->Field.InvalidEncode = cb; \
		break; \
	case SCE_CES_ERROR_ILLEGAL_CODE: \
		callbacks->Field.IllegalCode = cb; \
		break; \
	case SCE_CES_ERROR_OUT_OF_CODE_RANGE: \
		callbacks->Field.OutOfCodeRange = cb; \
		break; \
	case SCE_CES_ERROR_ALL: \
		callbacks->Field.InvalidEncode = cb; \
		callbacks->Field.IllegalCode = cb; \
		callbacks->Field.OutOfCodeRange = cb; \
		break; \
	default: \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	} \
	return SCE_CES_OK; \
}

#define DEFINE_SET_CB_IC(Name, Field, CbType) \
int sceCesUcsCallbacksSet##Name(CesUcsCallbacks *callbacks, int errorType, CbType *cb) \
{ \
	if (callbacks == NULL) \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	switch (errorType) { \
	case SCE_CES_ERROR_ILLEGAL_CODE: \
	case SCE_CES_ERROR_ALL: \
		callbacks->Field.IllegalCode = cb; \
		break; \
	default: \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	} \
	return SCE_CES_OK; \
}

#define DEFINE_SET_CB_IC_OOR(Name, Field, CbType) \
int sceCesUcsCallbacksSet##Name(CesUcsCallbacks *callbacks, int errorType, CbType *cb) \
{ \
	if (callbacks == NULL) \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	switch (errorType) { \
	case SCE_CES_ERROR_ILLEGAL_CODE: \
		callbacks->Field.IllegalCode = cb; \
		break; \
	case SCE_CES_ERROR_OUT_OF_CODE_RANGE: \
		callbacks->Field.OutOfCodeRange = cb; \
		break; \
	case SCE_CES_ERROR_ALL: \
		callbacks->Field.IllegalCode = cb; \
		callbacks->Field.OutOfCodeRange = cb; \
		break; \
	default: \
		return SCE_CES_ERROR_INVALID_PARAMETER; \
	} \
	return SCE_CES_OK; \
}

DEFINE_SET_CB_IE_IC(Ucs4ToCopy, Ucs4ToCopy, SceCesUcs4ToCopyErrorCallback)
DEFINE_SET_CB_IE_IC_OOR(Ucs4ToUcs2, Ucs4ToUcs2, SceCesUcs4ToUcs2ErrorCallback)
DEFINE_SET_CB_IE_IC(Ucs4ToUtf8, Ucs4ToUtf8, SceCesUcs4ToUtf8ErrorCallback)
DEFINE_SET_CB_IE_IC_OOR(Ucs4ToUtf16, Ucs4ToUtf16, SceCesUcs4ToUtf16ErrorCallback)
DEFINE_SET_CB_IE_IC_OOR(Ucs4ToUtf32, Ucs4ToUtf32, SceCesUcs4ToUtf32ErrorCallback)

DEFINE_SET_CB_IC(Ucs2ToUcs4, Ucs2ToUcs4, SceCesUcs2ToUcs4ErrorCallback)
DEFINE_SET_CB_IC(Ucs2ToCopy, Ucs2ToCopy, SceCesUcs2ToCopyErrorCallback)
DEFINE_SET_CB_IC(Ucs2ToUtf8, Ucs2ToUtf8, SceCesUcs2ToUtf8ErrorCallback)
DEFINE_SET_CB_IC(Ucs2ToUtf16, Ucs2ToUtf16, SceCesUcs2ToUtf16ErrorCallback)
DEFINE_SET_CB_IC(Ucs2ToUtf32, Ucs2ToUtf32, SceCesUcs2ToUtf32ErrorCallback)

DEFINE_SET_CB_IE_IC(Utf8ToUcs4, Utf8ToUcs4, SceCesUtf8ToUcs4ErrorCallback)
DEFINE_SET_CB_IE_IC_OOR(Utf8ToUcs2, Utf8ToUcs2, SceCesUtf8ToUcs2ErrorCallback)
DEFINE_SET_CB_IE_IC(Utf8ToCopy, Utf8ToCopy, SceCesUtf8ToCopyErrorCallback)
DEFINE_SET_CB_IE_IC_OOR(Utf8ToUtf16, Utf8ToUtf16, SceCesUtf8ToUtf16ErrorCallback)
DEFINE_SET_CB_IE_IC_OOR(Utf8ToUtf32, Utf8ToUtf32, SceCesUtf8ToUtf32ErrorCallback)

DEFINE_SET_CB_IC(Utf16ToUcs4, Utf16ToUcs4, SceCesUtf16ToUcs4ErrorCallback)
DEFINE_SET_CB_IC_OOR(Utf16ToUcs2, Utf16ToUcs2, SceCesUtf16ToUcs2ErrorCallback)
DEFINE_SET_CB_IC(Utf16ToUtf8, Utf16ToUtf8, SceCesUtf16ToUtf8ErrorCallback)
DEFINE_SET_CB_IC(Utf16ToCopy, Utf16ToCopy, SceCesUtf16ToCopyErrorCallback)
DEFINE_SET_CB_IC(Utf16ToUtf32, Utf16ToUtf32, SceCesUtf16ToUtf32ErrorCallback)

DEFINE_SET_CB_IE_IC(Utf32ToUcs4, Utf32ToUcs4, SceCesUtf32ToUcs4ErrorCallback)
DEFINE_SET_CB_IE_IC_OOR(Utf32ToUcs2, Utf32ToUcs2, SceCesUtf32ToUcs2ErrorCallback)
DEFINE_SET_CB_IE_IC(Utf32ToUtf8, Utf32ToUtf8, SceCesUtf32ToUtf8ErrorCallback)
DEFINE_SET_CB_IE_IC(Utf32ToUtf16, Utf32ToUtf16, SceCesUtf32ToUtf16ErrorCallback)
DEFINE_SET_CB_IE_IC(Utf32ToCopy, Utf32ToCopy, SceCesUtf32ToCopyErrorCallback)

int sceCesCallbackToUtf32ErrorReplace(const void *srcStr, uint32_t srcMax, uint32_t *srcLen,
				      uint32_t *utf32buf, uint32_t utf32max, uint32_t *utf32Count,
				      SceCesState *cbState)
{
	uint32_t ucode;

	ucode = cbState->replaceCode;
	if (ucode != 0 && ucode < CES_UCS_CODE_LIMIT) {
		if (utf32Count != NULL)
			*utf32Count = 1;
		if (utf32buf != NULL && utf32max != 0)
			*utf32buf = ucode;
		cbState->setCharCount = 1;
	} else {
		cbState->codeLen = 0;
	}
	return SCE_CES_OK;
}

int sceCesCallbackToUtf16ErrorReplace(const void *srcStr, uint32_t srcMax, uint32_t *srcLen,
				      uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len,
				      SceCesState *cbState)
{
	uint32_t ucode;

	ucode = cbState->replaceCode;
	if (ucode != 0 && ucode < CES_UCS_CODE_LIMIT) {
		sceCesUcs4ToUtf16(ucode, utf16buf, utf16max, utf16Len);
		cbState->setCharCount = 1;
	} else {
		cbState->codeLen = 0;
	}
	return SCE_CES_OK;
}

int sceCesCallbackToUtf8ErrorReplace(const void *srcStr, uint32_t srcMax, uint32_t *srcLen,
				     uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8size,
				     SceCesState *cbState)
{
	uint32_t ucode;

	ucode = cbState->replaceCode;
	if (ucode != 0 && ucode < CES_UCS_CODE_LIMIT) {
		sceCesUcs4ToUtf8(ucode, utf8buf, utf8max, utf8size);
		cbState->setCharCount = 1;
	} else {
		cbState->codeLen = 0;
	}
	return SCE_CES_OK;
}

int sceCesCallbackToUcs4ErrorReplace(const void *srcStr, uint32_t srcMax, uint32_t *srcLen,
				     uint32_t *ucs4buf, uint32_t ucs4max, uint32_t *ucs4Count,
				     SceCesState *cbState)
{
	uint32_t ucode;

	ucode = cbState->replaceCode;
	if (ucode != 0 && ucode <= CES_UCS4_CODE_MAX) {
		if (ucs4Count != NULL)
			*ucs4Count = 1;
		if (ucs4buf != NULL && ucs4max != 0)
			*ucs4buf = ucode;
		cbState->setCharCount = 1;
	} else {
		cbState->codeLen = 0;
	}
	return SCE_CES_OK;
}

int sceCesCallbackToUcs2ErrorReplace(const void *srcStr, uint32_t srcMax, uint32_t *srcLen,
				     uint16_t *ucs2buf, uint32_t ucs2max, uint32_t *ucs2Count,
				     SceCesState *cbState)
{
	uint32_t ucode;

	ucode = cbState->replaceCode;
	if (ucode != 0 && ucode <= CES_UCS2_CODE_MAX) {
		if (ucs2Count != NULL)
			*ucs2Count = 1;
		if (ucs2buf != NULL && ucs2max != 0)
			*ucs2buf = ucode;
		cbState->setCharCount = 1;
	} else {
		cbState->codeLen = 0;
	}
	return SCE_CES_OK;
}

int sceCesCallbackUcsErrorSkip(const void *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
			       void *ucsBuf, uint32_t bufMax, uint32_t *bufLen,
			       SceCesState *cbState)
{
	cbState->codeLen = 0;
	return SCE_CES_OK;
}

int sceCesCallbackUcsErrorOk(const void *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
			     void *ucsBuf, uint32_t bufMax, uint32_t *bufLen,
			     SceCesState *cbState)
{
	return SCE_CES_OK;
}

int sceCesUcsCallbacksSetUcsErrorReturn(CesUcsCallbacks *callbacks, int errorType)
{
	if (callbacks == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	sceCesUcsCallbacksSetUcs4ToCopy(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs4ToUcs2(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs4ToUtf8(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs4ToUtf16(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs4ToUtf32(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs2ToUcs4(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs2ToCopy(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs2ToUtf8(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs2ToUtf16(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUcs2ToUtf32(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf8ToUcs4(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf8ToUcs2(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf8ToCopy(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf8ToUtf16(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf8ToUtf32(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf16ToUcs4(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf16ToUcs2(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf16ToUtf8(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf16ToCopy(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf16ToUtf32(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf32ToUcs4(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf32ToUcs2(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf32ToUtf8(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf32ToUtf16(callbacks, errorType, NULL);
	sceCesUcsCallbacksSetUtf32ToCopy(callbacks, errorType, NULL);
	return SCE_CES_OK;
}

int sceCesUcsCallbacksSetUcsErrorReplace(CesUcsCallbacks *callbacks, int errorType)
{
	if (callbacks == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	sceCesUcsCallbacksSetUcs4ToCopy(callbacks, errorType, (SceCesUcs4ToCopyErrorCallback *)sceCesCallbackToUcs4ErrorReplace);
	sceCesUcsCallbacksSetUcs4ToUcs2(callbacks, errorType, (SceCesUcs4ToUcs2ErrorCallback *)sceCesCallbackToUcs2ErrorReplace);
	sceCesUcsCallbacksSetUcs4ToUtf8(callbacks, errorType, (SceCesUcs4ToUtf8ErrorCallback *)sceCesCallbackToUtf8ErrorReplace);
	sceCesUcsCallbacksSetUcs4ToUtf16(callbacks, errorType, (SceCesUcs4ToUtf16ErrorCallback *)sceCesCallbackToUtf16ErrorReplace);
	sceCesUcsCallbacksSetUcs4ToUtf32(callbacks, errorType, (SceCesUcs4ToUtf32ErrorCallback *)sceCesCallbackToUtf32ErrorReplace);
	sceCesUcsCallbacksSetUcs2ToUcs4(callbacks, errorType, (SceCesUcs2ToUcs4ErrorCallback *)sceCesCallbackToUcs4ErrorReplace);
	sceCesUcsCallbacksSetUcs2ToCopy(callbacks, errorType, (SceCesUcs2ToCopyErrorCallback *)sceCesCallbackToUcs2ErrorReplace);
	sceCesUcsCallbacksSetUcs2ToUtf8(callbacks, errorType, (SceCesUcs2ToUtf8ErrorCallback *)sceCesCallbackToUtf8ErrorReplace);
	sceCesUcsCallbacksSetUcs2ToUtf16(callbacks, errorType, (SceCesUcs2ToUtf16ErrorCallback *)sceCesCallbackToUtf16ErrorReplace);
	sceCesUcsCallbacksSetUcs2ToUtf32(callbacks, errorType, (SceCesUcs2ToUtf32ErrorCallback *)sceCesCallbackToUtf32ErrorReplace);
	sceCesUcsCallbacksSetUtf8ToUcs4(callbacks, errorType, (SceCesUtf8ToUcs4ErrorCallback *)sceCesCallbackToUcs4ErrorReplace);
	sceCesUcsCallbacksSetUtf8ToUcs2(callbacks, errorType, (SceCesUtf8ToUcs2ErrorCallback *)sceCesCallbackToUcs2ErrorReplace);
	sceCesUcsCallbacksSetUtf8ToCopy(callbacks, errorType, (SceCesUtf8ToCopyErrorCallback *)sceCesCallbackToUtf8ErrorReplace);
	sceCesUcsCallbacksSetUtf8ToUtf16(callbacks, errorType, (SceCesUtf8ToUtf16ErrorCallback *)sceCesCallbackToUtf16ErrorReplace);
	sceCesUcsCallbacksSetUtf8ToUtf32(callbacks, errorType, (SceCesUtf8ToUtf32ErrorCallback *)sceCesCallbackToUtf32ErrorReplace);
	sceCesUcsCallbacksSetUtf16ToUcs4(callbacks, errorType, (SceCesUtf16ToUcs4ErrorCallback *)sceCesCallbackToUcs4ErrorReplace);
	sceCesUcsCallbacksSetUtf16ToUcs2(callbacks, errorType, (SceCesUtf16ToUcs2ErrorCallback *)sceCesCallbackToUcs2ErrorReplace);
	sceCesUcsCallbacksSetUtf16ToUtf8(callbacks, errorType, (SceCesUtf16ToUtf8ErrorCallback *)sceCesCallbackToUtf8ErrorReplace);
	sceCesUcsCallbacksSetUtf16ToCopy(callbacks, errorType, (SceCesUtf16ToCopyErrorCallback *)sceCesCallbackToUtf16ErrorReplace);
	sceCesUcsCallbacksSetUtf16ToUtf32(callbacks, errorType, (SceCesUtf16ToUtf32ErrorCallback *)sceCesCallbackToUtf32ErrorReplace);
	sceCesUcsCallbacksSetUtf32ToUcs4(callbacks, errorType, (SceCesUtf32ToUcs4ErrorCallback *)sceCesCallbackToUcs4ErrorReplace);
	sceCesUcsCallbacksSetUtf32ToUcs2(callbacks, errorType, (SceCesUtf32ToUcs2ErrorCallback *)sceCesCallbackToUcs2ErrorReplace);
	sceCesUcsCallbacksSetUtf32ToUtf8(callbacks, errorType, (SceCesUtf32ToUtf8ErrorCallback *)sceCesCallbackToUtf8ErrorReplace);
	sceCesUcsCallbacksSetUtf32ToUtf16(callbacks, errorType, (SceCesUtf32ToUtf16ErrorCallback *)sceCesCallbackToUtf16ErrorReplace);
	sceCesUcsCallbacksSetUtf32ToCopy(callbacks, errorType, (SceCesUtf32ToCopyErrorCallback *)sceCesCallbackToUtf32ErrorReplace);
	return SCE_CES_OK;
}

int sceCesUcsCallbacksSetUcsErrorSkip(CesUcsCallbacks *callbacks, int errorType)
{
	if (callbacks == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	sceCesUcsCallbacksSetUcs4ToCopy(callbacks, errorType, (SceCesUcs4ToCopyErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs4ToUcs2(callbacks, errorType, (SceCesUcs4ToUcs2ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs4ToUtf8(callbacks, errorType, (SceCesUcs4ToUtf8ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs4ToUtf16(callbacks, errorType, (SceCesUcs4ToUtf16ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs4ToUtf32(callbacks, errorType, (SceCesUcs4ToUtf32ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs2ToUcs4(callbacks, errorType, (SceCesUcs2ToUcs4ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs2ToCopy(callbacks, errorType, (SceCesUcs2ToCopyErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs2ToUtf8(callbacks, errorType, (SceCesUcs2ToUtf8ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs2ToUtf16(callbacks, errorType, (SceCesUcs2ToUtf16ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUcs2ToUtf32(callbacks, errorType, (SceCesUcs2ToUtf32ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf8ToUcs4(callbacks, errorType, (SceCesUtf8ToUcs4ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf8ToUcs2(callbacks, errorType, (SceCesUtf8ToUcs2ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf8ToCopy(callbacks, errorType, (SceCesUtf8ToCopyErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf8ToUtf16(callbacks, errorType, (SceCesUtf8ToUtf16ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf8ToUtf32(callbacks, errorType, (SceCesUtf8ToUtf32ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf16ToUcs4(callbacks, errorType, (SceCesUtf16ToUcs4ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf16ToUcs2(callbacks, errorType, (SceCesUtf16ToUcs2ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf16ToUtf8(callbacks, errorType, (SceCesUtf16ToUtf8ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf16ToCopy(callbacks, errorType, (SceCesUtf16ToCopyErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf16ToUtf32(callbacks, errorType, (SceCesUtf16ToUtf32ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf32ToUcs4(callbacks, errorType, (SceCesUtf32ToUcs4ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf32ToUcs2(callbacks, errorType, (SceCesUtf32ToUcs2ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf32ToUtf8(callbacks, errorType, (SceCesUtf32ToUtf8ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf32ToUtf16(callbacks, errorType, (SceCesUtf32ToUtf16ErrorCallback *)sceCesCallbackUcsErrorSkip);
	sceCesUcsCallbacksSetUtf32ToCopy(callbacks, errorType, (SceCesUtf32ToCopyErrorCallback *)sceCesCallbackUcsErrorSkip);
	return SCE_CES_OK;
}

int sceCesUcsCallbacksSetUcsErrorOk(CesUcsCallbacks *callbacks, int errorType)
{
	if (callbacks == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	sceCesUcsCallbacksSetUcs4ToCopy(callbacks, errorType, (SceCesUcs4ToCopyErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs4ToUcs2(callbacks, errorType, (SceCesUcs4ToUcs2ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs4ToUtf8(callbacks, errorType, (SceCesUcs4ToUtf8ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs4ToUtf16(callbacks, errorType, (SceCesUcs4ToUtf16ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs4ToUtf32(callbacks, errorType, (SceCesUcs4ToUtf32ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs2ToUcs4(callbacks, errorType, (SceCesUcs2ToUcs4ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs2ToCopy(callbacks, errorType, (SceCesUcs2ToCopyErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs2ToUtf8(callbacks, errorType, (SceCesUcs2ToUtf8ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs2ToUtf16(callbacks, errorType, (SceCesUcs2ToUtf16ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUcs2ToUtf32(callbacks, errorType, (SceCesUcs2ToUtf32ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf8ToUcs4(callbacks, errorType, (SceCesUtf8ToUcs4ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf8ToUcs2(callbacks, errorType, (SceCesUtf8ToUcs2ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf8ToCopy(callbacks, errorType, (SceCesUtf8ToCopyErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf8ToUtf16(callbacks, errorType, (SceCesUtf8ToUtf16ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf8ToUtf32(callbacks, errorType, (SceCesUtf8ToUtf32ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf16ToUcs4(callbacks, errorType, (SceCesUtf16ToUcs4ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf16ToUcs2(callbacks, errorType, (SceCesUtf16ToUcs2ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf16ToUtf8(callbacks, errorType, (SceCesUtf16ToUtf8ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf16ToCopy(callbacks, errorType, (SceCesUtf16ToCopyErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf16ToUtf32(callbacks, errorType, (SceCesUtf16ToUtf32ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf32ToUcs4(callbacks, errorType, (SceCesUtf32ToUcs4ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf32ToUcs2(callbacks, errorType, (SceCesUtf32ToUcs2ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf32ToUtf8(callbacks, errorType, (SceCesUtf32ToUtf8ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf32ToUtf16(callbacks, errorType, (SceCesUtf32ToUtf16ErrorCallback *)sceCesCallbackUcsErrorOk);
	sceCesUcsCallbacksSetUtf32ToCopy(callbacks, errorType, (SceCesUtf32ToCopyErrorCallback *)sceCesCallbackUcsErrorOk);
	return SCE_CES_OK;
}

static inline void *cesErrorCallbackToUcs4(uint32_t operation)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE)
		return (void *)sceCesCallbackToUcs4ErrorReplace;
	if (operation == SCE_CES_ERR_OPERATION_SKIP)
		return (void *)sceCesCallbackUcsErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return (void *)sceCesCallbackUcsErrorOk;
	return NULL;
}

static inline void *cesErrorCallbackToUcs2(uint32_t operation)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE)
		return (void *)sceCesCallbackToUcs2ErrorReplace;
	if (operation == SCE_CES_ERR_OPERATION_SKIP)
		return (void *)sceCesCallbackUcsErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return (void *)sceCesCallbackUcsErrorOk;
	return NULL;
}

static inline void *cesErrorCallbackToUtf8(uint32_t operation)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE)
		return (void *)sceCesCallbackToUtf8ErrorReplace;
	if (operation == SCE_CES_ERR_OPERATION_SKIP)
		return (void *)sceCesCallbackUcsErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return (void *)sceCesCallbackUcsErrorOk;
	return NULL;
}

static inline void *cesErrorCallbackToUtf16(uint32_t operation)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE)
		return (void *)sceCesCallbackToUtf16ErrorReplace;
	if (operation == SCE_CES_ERR_OPERATION_SKIP)
		return (void *)sceCesCallbackUcsErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return (void *)sceCesCallbackUcsErrorOk;
	return NULL;
}

static inline void *cesErrorCallbackToUtf32(uint32_t operation)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE)
		return (void *)sceCesCallbackToUtf32ErrorReplace;
	if (operation == SCE_CES_ERR_OPERATION_SKIP)
		return (void *)sceCesCallbackUcsErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return (void *)sceCesCallbackUcsErrorOk;
	return NULL;
}

static inline void *cesErrorCallbackToUtf7(uint32_t operation)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE || operation == SCE_CES_ERR_OPERATION_SKIP)
		return (void *)sceCesCallbackUcsErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return (void *)sceCesCallbackUcsErrorOk;
	return NULL;
}

#define CES_ERR_INVALID_ENCODE(operation)	((operation) & 0xf)
#define CES_ERR_ILLEGAL_CODE(operation)		(((operation) >> 4) & 0xf)
#define CES_ERR_OUT_OF_CODE_RANGE(operation)	(((operation) >> 8) & 0xf)

#define DEFINE_SETUP_IE_IC(Name, DstHelper) \
const SceCes##Name##ErrorCallbacks *_sceCesSetup##Name##ErrorCallbacks(uint32_t operation, \
	SceCes##Name##ErrorCallbacks *callbacks) \
{ \
	callbacks->InvalidEncode = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_INVALID_ENCODE(operation)); \
	callbacks->IllegalCode = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_ILLEGAL_CODE(operation)); \
	return callbacks; \
}

#define DEFINE_SETUP_IE_IC_OOR(Name, DstHelper) \
const SceCes##Name##ErrorCallbacks *_sceCesSetup##Name##ErrorCallbacks(uint32_t operation, \
	SceCes##Name##ErrorCallbacks *callbacks) \
{ \
	callbacks->InvalidEncode = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_INVALID_ENCODE(operation)); \
	callbacks->IllegalCode = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_ILLEGAL_CODE(operation)); \
	callbacks->OutOfCodeRange = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_OUT_OF_CODE_RANGE(operation)); \
	return callbacks; \
}

#define DEFINE_SETUP_IC(Name, DstHelper) \
const SceCes##Name##ErrorCallbacks *_sceCesSetup##Name##ErrorCallbacks(uint32_t operation, \
	SceCes##Name##ErrorCallbacks *callbacks) \
{ \
	callbacks->IllegalCode = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_ILLEGAL_CODE(operation)); \
	return callbacks; \
}

#define DEFINE_SETUP_IC_OOR(Name, DstHelper) \
const SceCes##Name##ErrorCallbacks *_sceCesSetup##Name##ErrorCallbacks(uint32_t operation, \
	SceCes##Name##ErrorCallbacks *callbacks) \
{ \
	callbacks->IllegalCode = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_ILLEGAL_CODE(operation)); \
	callbacks->OutOfCodeRange = (SceCes##Name##ErrorCallback *)DstHelper(CES_ERR_OUT_OF_CODE_RANGE(operation)); \
	return callbacks; \
}

DEFINE_SETUP_IE_IC(Ucs4ToCopy, cesErrorCallbackToUcs4)
DEFINE_SETUP_IE_IC_OOR(Ucs4ToUcs2, cesErrorCallbackToUcs2)
DEFINE_SETUP_IE_IC(Ucs4ToUtf8, cesErrorCallbackToUtf8)
DEFINE_SETUP_IE_IC_OOR(Ucs4ToUtf16, cesErrorCallbackToUtf16)
DEFINE_SETUP_IE_IC_OOR(Ucs4ToUtf32, cesErrorCallbackToUtf32)

DEFINE_SETUP_IC(Ucs2ToUcs4, cesErrorCallbackToUcs4)
DEFINE_SETUP_IC(Ucs2ToCopy, cesErrorCallbackToUcs2)
DEFINE_SETUP_IC(Ucs2ToUtf8, cesErrorCallbackToUtf8)
DEFINE_SETUP_IC(Ucs2ToUtf16, cesErrorCallbackToUtf16)
DEFINE_SETUP_IC(Ucs2ToUtf32, cesErrorCallbackToUtf32)

DEFINE_SETUP_IE_IC(Utf8ToUcs4, cesErrorCallbackToUcs4)
DEFINE_SETUP_IE_IC_OOR(Utf8ToUcs2, cesErrorCallbackToUcs2)
DEFINE_SETUP_IE_IC(Utf8ToCopy, cesErrorCallbackToUtf8)
DEFINE_SETUP_IE_IC_OOR(Utf8ToUtf16, cesErrorCallbackToUtf16)
DEFINE_SETUP_IE_IC_OOR(Utf8ToUtf32, cesErrorCallbackToUtf32)

DEFINE_SETUP_IC(Utf16ToUcs4, cesErrorCallbackToUcs4)
DEFINE_SETUP_IC_OOR(Utf16ToUcs2, cesErrorCallbackToUcs2)
DEFINE_SETUP_IC(Utf16ToUtf8, cesErrorCallbackToUtf8)
DEFINE_SETUP_IC(Utf16ToCopy, cesErrorCallbackToUtf16)
DEFINE_SETUP_IC(Utf16ToUtf32, cesErrorCallbackToUtf32)

DEFINE_SETUP_IE_IC(Utf32ToUcs4, cesErrorCallbackToUcs4)
DEFINE_SETUP_IE_IC_OOR(Utf32ToUcs2, cesErrorCallbackToUcs2)
DEFINE_SETUP_IE_IC(Utf32ToUtf8, cesErrorCallbackToUtf8)
DEFINE_SETUP_IE_IC(Utf32ToUtf16, cesErrorCallbackToUtf16)
DEFINE_SETUP_IE_IC(Utf32ToCopy, cesErrorCallbackToUtf32)

DEFINE_SETUP_IE_IC(Ucs4ToUtf7, cesErrorCallbackToUtf7)
DEFINE_SETUP_IC(Ucs2ToUtf7, cesErrorCallbackToUtf7)
DEFINE_SETUP_IE_IC(Utf8ToUtf7, cesErrorCallbackToUtf7)
DEFINE_SETUP_IC(Utf16ToUtf7, cesErrorCallbackToUtf7)
DEFINE_SETUP_IE_IC(Utf32ToUtf7, cesErrorCallbackToUtf7)

DEFINE_SETUP_IE_IC(Utf7ToUcs4, cesErrorCallbackToUcs4)
DEFINE_SETUP_IE_IC_OOR(Utf7ToUcs2, cesErrorCallbackToUcs2)
DEFINE_SETUP_IE_IC(Utf7ToUtf8, cesErrorCallbackToUtf8)
DEFINE_SETUP_IE_IC_OOR(Utf7ToUtf16, cesErrorCallbackToUtf16)
DEFINE_SETUP_IE_IC_OOR(Utf7ToUtf32, cesErrorCallbackToUtf32)
