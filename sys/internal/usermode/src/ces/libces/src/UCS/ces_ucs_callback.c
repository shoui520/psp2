/*
 * libSceCes's ces_ucs_callback.c (Sony's internal/usermode/src/ces/libces/src/UCS/ces_ucs_callback.c,
 * the SDK's static library, built with SNC): the error callbacks of the conversions between the
 * Unicode forms, and the sets a context's error operations select. SNC's linker keeps only the
 * routines a module calls; the tree has the ones libpaf calls. Names of the routines and their
 * parameters are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "../ces_context.h"
#include "ces_ucs.h"
#include "ces_ucs_callback.h"

/* above the highest code point of UTF-16, and so of the Unicode forms a replacement goes into */
#define CES_UCS_CODE_LIMIT	0x110000

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

/* the callback an error operation (one nibble of a context's errOperation) selects, for a
   conversion into UTF-16 */
static inline void *cesErrorCallbackToUtf16(uint32_t operation)
{
	void *callback;

	if (operation == SCE_CES_ERR_OPERATION_REPLACE)
		callback = (void *)sceCesCallbackToUtf16ErrorReplace;
	else if (operation == SCE_CES_ERR_OPERATION_SKIP)
		callback = (void *)sceCesCallbackUcsErrorSkip;
	else if (operation == SCE_CES_ERR_OPERATION_OK)
		callback = (void *)sceCesCallbackUcsErrorOk;
	else
		callback = NULL;
	return callback;
}

/* the callback an error operation (one nibble of a context's errOperation) selects, for a
   conversion into UTF-8 */
static inline void *cesErrorCallbackToUtf8(uint32_t operation)
{
	void *callback;

	if (operation == SCE_CES_ERR_OPERATION_REPLACE)
		callback = (void *)sceCesCallbackToUtf8ErrorReplace;
	else if (operation == SCE_CES_ERR_OPERATION_SKIP)
		callback = (void *)sceCesCallbackUcsErrorSkip;
	else if (operation == SCE_CES_ERR_OPERATION_OK)
		callback = (void *)sceCesCallbackUcsErrorOk;
	else
		callback = NULL;
	return callback;
}

/* where each error's operation sits in errOperation */
#define CES_ERR_INVALID_ENCODE(operation)	((operation) & 0xf)
#define CES_ERR_ILLEGAL_CODE(operation)		(((operation) >> 4) & 0xf)
#define CES_ERR_OUT_OF_CODE_RANGE(operation)	(((operation) >> 8) & 0xf)

const SceCesUtf8ToUtf16ErrorCallbacks *_sceCesSetupUtf8ToUtf16ErrorCallbacks(uint32_t operation,
	SceCesUtf8ToUtf16ErrorCallbacks *callbacks)
{
	callbacks->InvalidEncode = (SceCesUtf8ToUtf16ErrorCallback *)cesErrorCallbackToUtf16(CES_ERR_INVALID_ENCODE(operation));
	callbacks->IllegalCode = (SceCesUtf8ToUtf16ErrorCallback *)cesErrorCallbackToUtf16(CES_ERR_ILLEGAL_CODE(operation));
	callbacks->OutOfCodeRange = (SceCesUtf8ToUtf16ErrorCallback *)cesErrorCallbackToUtf16(CES_ERR_OUT_OF_CODE_RANGE(operation));
	return callbacks;
}

const SceCesUtf16ToUtf8ErrorCallbacks *_sceCesSetupUtf16ToUtf8ErrorCallbacks(uint32_t operation,
	SceCesUtf16ToUtf8ErrorCallbacks *callbacks)
{
	callbacks->IllegalCode = (SceCesUtf16ToUtf8ErrorCallback *)cesErrorCallbackToUtf8(CES_ERR_ILLEGAL_CODE(operation));
	return callbacks;
}
