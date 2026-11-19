/*
 * libSceCes's Str_UTF8_Copy.c (Sony's internal/usermode/src/ces/libces/src/UCS/Str_UTF8_Copy.c,
 * the SDK's static library, built with SNC): UTF-8 strings copied as UTF-8, each character
 * checked on the way. Names of the routines, their parameters and labels are Sony's (the
 * library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "../ces_context.h"
#include "ces_ucs.h"
#include "ces_ucs_callback.h"
#include "ces_ucs_context.h"

#define CES_UCS_BOM		0xfeff

/* the longest UTF-8 sequence */
#define CES_UTF8_CODE_MAX	6

static int cesUtf8StrToCopyCodes(SceCesUcsContext *ces, const uint8_t *utf8str,
				 uint32_t utf8max, uint32_t *utf8Len, uint8_t *copyBuf,
				 uint32_t copyMax, uint32_t *copyLen)
{
	const uint8_t *utf8addr = utf8str;
	uint32_t utf8size;
	uint32_t srcLen = 0;
	uint32_t srcCount = 0;
	uint32_t dstLen = 0;
	uint32_t dstCount = 0;
	uint32_t dstOverLen = 0;
	uint32_t srcStrC = 0;
	uint32_t dstStrC = 0;
	uint32_t srcStrMax;
	uint32_t dstStrMax;
	int ret = SCE_CES_OK;
	int outputBom;
	uint32_t utf8N = 0;
	uint8_t copy[CES_UTF8_CODE_MAX];
	uint32_t ucs4 = 0;
	const SceCesUtf8ToCopyErrorCallbacks *callbacks = NULL;
	SceCesUtf8ToCopyErrorCallbacks Callbacks;

	if (ces != NULL) {
		outputBom = ces->UcsSetting.outputBom;
		if (ces->strContinue &&
		    (outputBom == SCE_CES_OUTPUT_TOP_SET || outputBom == SCE_CES_OUTPUT_TOP_CUT))
			outputBom = SCE_CES_OUTPUT_ENABLE;
		ces->strContinue = ces->strFrame;
		srcStrMax = ces->srcCharMax;
		dstStrMax = ces->dstCharMax;
		ces->State.codeBuffer = copy;
		if (ces->errOperation != 0)
			callbacks = _sceCesSetupUtf8ToCopyErrorCallbacks(ces->errOperation, &Callbacks);
		else if (ces->UcsCallbacks != NULL)
			callbacks = &ces->UcsCallbacks->Utf8ToCopy;
	} else {
		srcStrMax = 0xffffffff;
		outputBom = SCE_CES_OUTPUT_ENABLE;
		dstStrMax = 0xffffffff;
	}

	utf8size = CES_UTF8_CODE_MAX;
	if (utf8max != 0)
		utf8size = utf8max;
	if (copyBuf == NULL)
		copyMax = 0;

loop:
	while (srcStrC < srcStrMax && dstStrC < dstStrMax) {
		uint32_t utf8C;
		uint32_t copyN;
		uint32_t copyC;

		ret = sceCesUtf8ToCopyAndUcs(utf8addr, utf8size, &utf8N, copy, CES_UTF8_CODE_MAX,
					     &copyN, &ucs4);
		if (dstStrC == 0 && outputBom == SCE_CES_OUTPUT_TOP_SET) {
			utf8C = 1;
			if (ucs4 != CES_UCS_BOM) {
				utf8N = 0;
				ucs4 = CES_UCS_BOM;
				copy[0] = 0xef;
				copy[1] = 0xbb;
				copy[2] = 0xbf;
				copyN = 3;
				utf8C = 0;
			}
			copyC = 1;
		} else if (ret != SCE_CES_OK) {
			SceCesUtf8ToCopyErrorCallback *callback;

			if (callbacks == NULL)
				break;
			callback = NULL;
			copyC = 0;
			switch (ret) {
			case SCE_CES_ERROR_ILLEGAL_CODE:
				callback = callbacks->IllegalCode;
				copyC = 1;
				break;
			case SCE_CES_ERROR_INVALID_ENCODE:
				callback = callbacks->InvalidEncode;
				break;
			}
			if (callback == NULL)
				break;
			if (utf8N == 0)
				utf8N = 1;
			ces->State.returnValue = ret;
			ces->State.setCharCount = copyC;
			ces->State.codeLen = copyN;
			copyN = 0;
			ret = callback(utf8addr, utf8max, &utf8N, copyBuf, copyMax, &copyN,
				       &ces->State);
			if (ret != SCE_CES_OK)
				break;
			if (utf8N == 0 || (utf8max != 0 && utf8N > utf8max)) {
				ret = CES_ERROR_CALLBACK_SRC_LEN;
				goto end;
			}
			utf8C = 1;
			if (copyN != 0) {
				copyC = ces->State.setCharCount;
				goto next;
			}
			if (ces->State.codeLen == 0)
				goto skip;
			copyN = ces->State.codeLen;
		} else {
			if (ucs4 == 0)
				break;
			utf8C = 1;
			if (ucs4 == CES_UCS_BOM) {
				if (outputBom == SCE_CES_OUTPUT_DISABLE)
					goto skip;
				if (outputBom == SCE_CES_OUTPUT_TOP_CUT && srcStrC == 0)
					goto skip;
			}
			copyC = 1;
		}
write:
		if (copyN <= copyMax) {
			uint32_t _ces_copy_code_count = copyN;

			for (; _ces_copy_code_count != 0; _ces_copy_code_count--)
				copyBuf[_ces_copy_code_count - 1] = copy[_ces_copy_code_count - 1];
		}
next:
		if (copyN <= copyMax) {
			copyBuf += copyN;
			copyMax -= copyN;
			dstCount += copyC;
			dstLen += copyN;
			srcLen += utf8N;
			srcCount += utf8C;
		} else {
			copyMax = 0;
			dstOverLen += copyN;
		}
		dstStrC += copyC;
		goto advance;
skip:
		if (dstOverLen == 0) {
			srcCount += utf8C;
			srcLen += utf8N;
		}
advance:
		utf8addr += utf8N;
		srcStrC += utf8C;
		if (utf8max == 0)
			goto loop;
		if (utf8N >= utf8max)
			break;
		utf8max -= utf8N;
		utf8size = utf8max;
	}
end:
	if (ces != NULL) {
		CesStrReport *report = ces->strReport;

		if (report != NULL) {
			report->srcCodeSize = sizeof(uint8_t);
			report->dstCodeSize = sizeof(uint8_t);
			report->lastCodeLen = utf8N;
			report->lastCharCode = ucs4;
			report->returnValue = ret;
			report->strSrcLen = utf8addr - utf8str;
			report->strDstLen = dstLen + dstOverLen;
			report->strSrcCount = srcStrC;
			report->strDstCount = dstStrC;
			report->cnvSrcCount = srcCount;
			report->cnvDstCount = dstCount;
		}
	}
	if (copyBuf != NULL) {
		if (dstOverLen != 0)
			ret = SCE_CES_ERROR_DST_BUFFER_END;
	} else {
		srcLen = utf8addr - utf8str;
		dstLen = dstLen + dstOverLen;
		if (ret == SCE_CES_OK)
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	if (utf8Len != NULL)
		*utf8Len = srcLen;
	if (copyLen != NULL)
		*copyLen = dstLen;
	return ret;
}

int sceCesUtf8StrToCopyStr(SceCesUcsContext *ces, const uint8_t *utf8str, uint32_t utf8max,
			   uint32_t *utf8Len, uint8_t *copyStr, uint32_t copyMax, uint32_t *copyLen)
{
	int ret;

	if (utf8str != NULL) {
		uint32_t copyCount;
		uint32_t max = 0;

		if (copyMax != 0)
			max = copyMax - 1;
		ret = cesUtf8StrToCopyCodes(ces, utf8str, utf8max, utf8Len, copyStr, max,
					    &copyCount);
		if (copyStr != NULL) {
			if (copyLen != NULL)
				*copyLen = copyCount;
			if (copyMax != 0)
				copyStr[copyCount] = 0;
			return ret;
		}
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	} else {
		if (copyStr != NULL && copyMax != 0)
			copyStr[0] = 0;
		ret = _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint8_t),
					    SCE_CES_ERROR_INVALID_SRC_BUFFER);
	}
	if (utf8Len != NULL)
		*utf8Len = 0;
	if (copyLen != NULL)
		*copyLen = 0;
	return ret;
}
