/*
 * libSceCes's ces_mbcs_callback.c (Sony's internal/usermode/src/ces/libces/src/MBCS/ces_mbcs_callback.c,
 * the SDK's static library, built with SNC): the error callbacks of the conversions between a
 * multi-byte character set and the Unicode forms, and the sets a context's error operations
 * select. SNC's linker keeps only the routines a module calls; the tree has the ones libpaf
 * calls. Names of the routines and their parameters are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "../ces_context.h"
#include "../UCS/ces_ucs.h"
#include "ces_mbcs_callback.h"

/* above the highest code point of UTF-16, and so of the Unicode forms a replacement goes into */
#define CES_UCS_CODE_LIMIT	0x110000

/* the most bytes of a replacement character in a multi-byte character set */
#define CES_MBC_REPLACE_MAX	4

static int cesErrorCallbackMbcsToUtf16(const uint8_t *mbcs, uint32_t mbcsMax, uint32_t *mbcsLen,
				       uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
				       SceCesState *cbState);
static int cesErrorCallbackUtf16ToMbcs(const uint16_t *utf16buf, uint32_t utf16max,
				       uint32_t *utf16size, uint8_t *mbcsBuf, uint32_t mbcsMax,
				       uint32_t *mbcsLen, SceCesState *cbState);

int sceCesCallbackMbcsToUcsErrorReplace(const uint8_t *mbcsBuf, uint32_t mbcsMax, uint32_t *mbcsLen,
					uint32_t *ucs4buf, uint32_t ucs4max, uint32_t *ucs4Size,
					SceCesState *cbState)
{
	uint32_t ucode;

	ucode = cbState->replaceCode;
	if (ucode != 0 && ucode < CES_UCS_CODE_LIMIT) {
		if (ucs4Size != NULL)
			*ucs4Size = 1;
		if (ucs4buf != NULL && ucs4max != 0)
			*ucs4buf = ucode;
		cbState->setCharCount = 1;
	} else {
		cbState->codeLen = 0;
	}
	return SCE_CES_OK;
}

int sceCesCallbackMbcsToUtf16ErrorSkip(const uint8_t *mbcsBuf, uint32_t mbcsMax, uint32_t *mbcsLen,
				       uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
				       SceCesState *cbState)
{
	cbState->codeLen = 0;
	return SCE_CES_OK;
}

int sceCesCallbackUcsToMbcsErrorReplace(const uint32_t ucs4, uint8_t *mbcsBuf, uint32_t mbcsMax,
					uint32_t *mbcsLen, SceCesState *cbState)
{
	uint32_t cn;
	uint32_t n;

	n = 0;
	if (cbState->replaceMbc[0] != 0) {
		do
			n++;
		while (n < CES_MBC_REPLACE_MAX && cbState->replaceMbc[n] != 0);
	}
	if (mbcsLen != NULL)
		*mbcsLen = n;
	if (mbcsBuf != NULL && n <= mbcsMax) {
		for (cn = 0; cn < n; cn++)
			mbcsBuf[cn] = cbState->replaceMbc[cn];
	}
	return SCE_CES_OK;
}

int sceCesCallbackUtf16ToMbcsErrorSkip(const uint16_t *utf16str, uint32_t utf16max,
				       uint32_t *utf16Size, uint8_t *mbcsBuf, uint32_t mbcsMax,
				       uint32_t *mbcsLen, SceCesState *cbState)
{
	cbState->codeLen = 0;
	return SCE_CES_OK;
}

/* the nibbles of a context's error operations, one per kind of error */
#define CES_ERR_INVALID_ENCODE(operation)	((operation) & 0xf)
#define CES_ERR_ILLEGAL_CODE(operation)		(((operation) >> 4) & 0xf)
#define CES_ERR_OUT_OF_CODE_RANGE(operation)	(((operation) >> 8) & 0xf)
#define CES_ERR_UNASSIGNED_CODE(operation)	(((operation) >> 12) & 0xf)

static inline SceCesMbcsToUtf16ErrorCallback *cesSetupMbcsToUtf16(uint32_t operation,
								 SceCesMbcsToUcsErrorCallback **sub)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE) {
		*sub = sceCesCallbackMbcsToUcsErrorReplace;
		return cesErrorCallbackMbcsToUtf16;
	}
	*sub = NULL;
	if (operation == SCE_CES_ERR_OPERATION_SKIP)
		return sceCesCallbackMbcsToUtf16ErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return sceCesCallbackMbcsToUtf16ErrorSkip;
	return NULL;
}

static inline SceCesUtf16ToMbcsErrorCallback *cesSetupUtf16ToMbcs(uint32_t operation,
								 SceCesUcsToMbcsErrorCallback **sub)
{
	if (operation == SCE_CES_ERR_OPERATION_REPLACE) {
		*sub = sceCesCallbackUcsToMbcsErrorReplace;
		return cesErrorCallbackUtf16ToMbcs;
	}
	*sub = NULL;
	if (operation == SCE_CES_ERR_OPERATION_SKIP)
		return sceCesCallbackUtf16ToMbcsErrorSkip;
	if (operation == SCE_CES_ERR_OPERATION_OK)
		return sceCesCallbackUtf16ToMbcsErrorSkip;
	return NULL;
}

void _sceCesSetupMbcsToUtf16ErrorCallbacks(uint32_t operation,
					   SceCesMbcsToUtf16ErrorCallbacks *callbacks,
					   SceCesMbcsToUcsErrorCallbacks *subCallbacks)
{
	callbacks->InvalidEncode = cesSetupMbcsToUtf16(CES_ERR_INVALID_ENCODE(operation),
						       &subCallbacks->InvalidEncode);
	callbacks->UnassignedCode = cesSetupMbcsToUtf16(CES_ERR_UNASSIGNED_CODE(operation),
							&subCallbacks->UnassignedCode);
}

void _sceCesSetupUtf16ToMbcsErrorCallbacks(uint32_t operation,
					   SceCesUtf16ToMbcsErrorCallbacks *callbacks,
					   SceCesUcsToMbcsErrorCallbacks *subCallbacks)
{
	callbacks->InvalidEncode = cesSetupUtf16ToMbcs(CES_ERR_INVALID_ENCODE(operation),
						       &subCallbacks->InvalidEncode);
	callbacks->IllegalCode = cesSetupUtf16ToMbcs(CES_ERR_ILLEGAL_CODE(operation),
						     &subCallbacks->IllegalCode);
	callbacks->OutOfCodeRange = cesSetupUtf16ToMbcs(CES_ERR_OUT_OF_CODE_RANGE(operation),
							&subCallbacks->OutOfCodeRange);
	callbacks->UnassignedCode = cesSetupUtf16ToMbcs(CES_ERR_UNASSIGNED_CODE(operation),
							&subCallbacks->UnassignedCode);
}

static int cesErrorCallbackMbcsToUtf16(const uint8_t *mbcs, uint32_t mbcsMax, uint32_t *mbcsLen,
				       uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
				       SceCesState *cbState)
{
	int ret = cbState->returnValue;
	SceCesMbcsToUcsErrorCallback *callback = cbState->callback;

	if (callback != NULL) {
		uint32_t ucs;
		uint32_t size = 0;

		ret = callback(mbcs, mbcsMax, mbcsLen, &ucs, 1, &size, cbState);
		if (ret == SCE_CES_OK && size != 0 && utf16buf != NULL) {
			ret = sceCesUcs4ToUtf16(ucs, utf16buf, utf16max, utf16Size);
			if (ret == SCE_CES_ERROR_ILLEGAL_CODE)
				ret = SCE_CES_OK;
		}
	}
	return ret;
}

static int cesErrorCallbackUtf16ToMbcs(const uint16_t *utf16buf, uint32_t utf16max,
				       uint32_t *utf16size, uint8_t *mbcsBuf, uint32_t mbcsMax,
				       uint32_t *mbcsLen, SceCesState *cbState)
{
	int ret = cbState->returnValue;
	SceCesUcsToMbcsErrorCallback *callback = cbState->callback;

	if (callback != NULL) {
		uint32_t ucs4 = 0;

		if (ret == SCE_CES_ERROR_INVALID_ENCODE) {
			ucs4 = 0xffffffff;
		} else {
			if (utf16max == 0)
				utf16max = 2;
			if (cbState->ctx.src.endian == 0)
				sceCesUtf16beToUcs4(utf16buf, utf16max, utf16size, &ucs4);
			else
				sceCesUtf16leToUcs4(utf16buf, utf16max, utf16size, &ucs4);
		}
		ret = callback(ucs4, mbcsBuf, mbcsMax, mbcsLen, cbState);
	}
	return ret;
}
