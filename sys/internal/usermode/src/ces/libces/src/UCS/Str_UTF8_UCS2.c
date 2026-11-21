/*
 * libSceCes's Str_UTF8_UCS2.c (Sony's internal/usermode/src/ces/libces/src/UCS/Str_UTF8_UCS2.c,
 * the SDK's static library, built with SNC): UTF-8 strings to UCS-2. SNC's linker keeps only the
 * routines a module calls; the tree has the ones apputil calls. Names of the routines, their
 * parameters, locals and labels are Sony's (the library's DWARF).
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

static int cesUtf8StrToUcs2Array(SceCesUcsContext *ces, const uint8_t *utf8str, uint32_t utf8max,
				 uint32_t *utf8Len, uint16_t *ucs2buf, uint32_t ucs2max,
				 uint32_t *ucs2Count)
{
	const uint8_t *utf8addr = utf8str;
	uint32_t utf8size;
	uint32_t srcLen = 0;
	uint32_t srcCount = 0;
	uint32_t dstLen = 0;
	uint32_t dstOverLen = 0;
	uint32_t srcStrC = 0;
	uint32_t dstStrC = 0;
	uint32_t srcStrMax;
	uint32_t dstStrMax;
	int ret = SCE_CES_OK;
	int outputBom;
	uint32_t utf8N = 0;
	uint16_t ucs2 = 0;
	const SceCesUtf8ToUcs2ErrorCallbacks *callbacks = NULL;
	SceCesUtf8ToUcs2ErrorCallbacks Callbacks;

	if (ces != NULL) {
		outputBom = ces->UcsSetting.outputBom;
		if (ces->strContinue &&
		    (outputBom == SCE_CES_OUTPUT_TOP_SET || outputBom == SCE_CES_OUTPUT_TOP_CUT))
			outputBom = SCE_CES_OUTPUT_ENABLE;
		ces->strContinue = ces->strFrame;
		srcStrMax = ces->srcCharMax;
		dstStrMax = ces->dstCharMax;
		ces->State.codeBuffer = &ucs2;
		if (ces->errOperation != 0)
			callbacks = _sceCesSetupUtf8ToUcs2ErrorCallbacks(ces->errOperation, &Callbacks);
		else if (ces->UcsCallbacks != NULL)
			callbacks = &ces->UcsCallbacks->Utf8ToUcs2;
	} else {
		srcStrMax = 0xffffffff;
		outputBom = SCE_CES_OUTPUT_ENABLE;
		dstStrMax = 0xffffffff;
	}

	utf8size = CES_UTF8_CODE_MAX;
	if (utf8max != 0)
		utf8size = utf8max;
	if (ucs2buf == NULL)
		ucs2max = 0;

loop:
	while (srcStrC < srcStrMax && dstStrC < dstStrMax) {
		uint32_t utf8C;
		uint32_t ucs2N = 0;

		ret = sceCesUtf8ToUcs2(utf8addr, utf8size, &utf8N, &ucs2);
		if (dstStrC == 0 && outputBom == SCE_CES_OUTPUT_TOP_SET) {
			utf8C = 1;
			if (ucs2 != CES_UCS_BOM) {
				utf8N = 0;
				ucs2 = CES_UCS_BOM;
				utf8C = 0;
			}
		} else if (ret != SCE_CES_OK) {
			SceCesUtf8ToUcs2ErrorCallback *callback;

			if (callbacks == NULL)
				break;
			callback = NULL;
			switch (ret) {
			case SCE_CES_ERROR_ILLEGAL_CODE:
				callback = callbacks->IllegalCode;
				ucs2N = 1;
				break;
			case SCE_CES_ERROR_OUT_OF_CODE_RANGE:
				callback = callbacks->OutOfCodeRange;
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
			ces->State.codeLen = ucs2N;
			ucs2N = 0;
			ret = callback(utf8addr, utf8max, &utf8N, ucs2buf, ucs2max, &ucs2N, &ces->State);
			if (ret != SCE_CES_OK)
				break;
			if (utf8N == 0 || (utf8max != 0 && utf8N > utf8max)) {
				ret = CES_ERROR_CALLBACK_SRC_LEN;
				goto end;
			}
			utf8C = 1;
			if (ucs2N != 0)
				goto next;
			if (ces->State.codeLen == 0)
				goto skip;
			ucs2N = ces->State.codeLen;
			goto write;
		} else {
			if (ucs2 == 0)
				break;
			utf8C = 1;
			if (ucs2 == CES_UCS_BOM) {
				if (outputBom == SCE_CES_OUTPUT_DISABLE)
					goto skip;
				if (outputBom == SCE_CES_OUTPUT_TOP_CUT && srcStrC == 0)
					goto skip;
			}
		}
		ucs2N = 1;
write:
		if (ucs2max != 0)
			*ucs2buf = ucs2;
next:
		if (ucs2N <= ucs2max) {
			ucs2buf += ucs2N;
			dstLen += ucs2N;
			ucs2max -= ucs2N;
			srcLen += utf8N;
			srcCount += utf8C;
		} else {
			ucs2max = 0;
			dstOverLen += ucs2N;
		}
		dstStrC += ucs2N;
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
			report->dstCodeSize = sizeof(uint16_t);
			report->lastCodeLen = utf8N;
			report->lastCharCode = ucs2;
			report->returnValue = ret;
			report->strSrcLen = utf8addr - utf8str;
			report->strDstLen = dstLen + dstOverLen;
			report->strSrcCount = srcStrC;
			report->strDstCount = dstStrC;
			report->cnvSrcCount = srcCount;
			report->cnvDstCount = dstLen;
		}
	}
	if (ucs2buf != NULL) {
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
	if (ucs2Count != NULL)
		*ucs2Count = dstLen;
	return ret;
}

int sceCesUtf8StrGetUcs2Len(SceCesUcsContext *ces, const uint8_t *utf8str, uint32_t utf8max,
			    uint32_t *utf8Len, uint32_t *ucs2Len)
{
	int ret;

	if (utf8str != NULL) {
		if (ucs2Len != NULL) {
			ret = cesUtf8StrToUcs2Array(ces, utf8str, utf8max, utf8Len, NULL, 0, ucs2Len);
			if (ret == SCE_CES_ERROR_INVALID_DST_BUFFER)
				ret = SCE_CES_OK;
			return ret;
		}
		ret = SCE_CES_ERROR_INVALID_PARAMETER;
	} else {
		if (ucs2Len != NULL)
			*ucs2Len = 0;
		ret = SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	if (utf8Len != NULL)
		*utf8Len = 0;
	return _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint16_t), ret);
}
