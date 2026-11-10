/*
 * libSceCes's Str_UTF8_UTF16.c (Sony's internal/usermode/src/ces/libces/src/UCS/Str_UTF8_UTF16.c,
 * the SDK's static library, built with SNC): UTF-8 strings to UTF-16. Names of the routines,
 * their parameters, locals and labels are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "../ces_context.h"
#include "ces_ucs.h"
#include "ces_ucs_callback.h"
#include "ces_ucs_context.h"

#define CES_UCS_BOM		0xfeff

/* the longest UTF-8 sequence, and of UTF-16 */
#define CES_UTF8_CODE_MAX	6
#define CES_UTF16_CODE_MAX	2

static int cesUtf8StrToUtf16Codes(SceCesUcsContext *ces, const uint8_t *utf8str,
				  uint32_t utf8max, uint32_t *utf8Size, uint16_t *utf16buf,
				  uint32_t utf16max, uint32_t *utf16Len)
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
	int dstEndianSwap = 0;
	uint32_t utf8N = 0;
	uint16_t utf16[CES_UTF16_CODE_MAX];
	uint32_t ucs4 = 0;
	const SceCesUtf8ToUtf16ErrorCallbacks *callbacks = NULL;
	SceCesUtf8ToUtf16ErrorCallbacks Callbacks;

	if (ces != NULL) {
		outputBom = ces->UcsSetting.outputBom;
		if (ces->strContinue &&
		    (outputBom == SCE_CES_OUTPUT_TOP_SET || outputBom == SCE_CES_OUTPUT_TOP_CUT))
			outputBom = SCE_CES_OUTPUT_ENABLE;
		ces->strContinue = ces->strFrame;
		if ((ces->UcsSetting.utf16 & 0xf) != CES_ENDIAN_LE)
			dstEndianSwap = 1;
		srcStrMax = ces->srcCharMax;
		dstStrMax = ces->dstCharMax;
		ces->State.codeBuffer = utf16;
		if (ces->errOperation != 0)
			callbacks = _sceCesSetupUtf8ToUtf16ErrorCallbacks(ces->errOperation, &Callbacks);
		else if (ces->UcsCallbacks != NULL)
			callbacks = &ces->UcsCallbacks->Utf8ToUtf16;
	} else {
		srcStrMax = 0xffffffff;
		outputBom = SCE_CES_OUTPUT_ENABLE;
		dstStrMax = 0xffffffff;
	}

	utf8size = CES_UTF8_CODE_MAX;
	if (utf8max != 0)
		utf8size = utf8max;
	if (utf16buf == NULL)
		utf16max = 0;

loop:
	while (srcStrC < srcStrMax && dstStrC < dstStrMax) {
		uint32_t utf8C;
		uint32_t utf16N;
		uint32_t utf16C;

		ret = sceCesUtf8ToUcsUtf16(utf8addr, utf8size, &utf8N, &ucs4, utf16,
					   CES_UTF16_CODE_MAX, &utf16N);
		if (dstStrC == 0 && outputBom == SCE_CES_OUTPUT_TOP_SET) {
			utf8C = 1;
			if (ucs4 != CES_UCS_BOM) {
				utf8N = 0;
				ucs4 = CES_UCS_BOM;
				utf16[0] = CES_UCS_BOM;
				utf16N = 1;
				utf8C = 0;
			}
		} else if (ret != SCE_CES_OK) {
			SceCesUtf8ToUtf16ErrorCallback *callback;

			if (callbacks == NULL)
				break;
			callback = NULL;
			switch (ret) {
			case SCE_CES_ERROR_ILLEGAL_CODE:
				callback = callbacks->IllegalCode;
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
			ces->State.setCharCount = 1;
			ces->State.codeLen = utf16N;
			utf16N = 0;
			ret = callback(utf8addr, utf8max, &utf8N, utf16buf, utf16max, &utf16N,
				       &ces->State);
			if (ret != SCE_CES_OK)
				break;
			if (utf8N == 0 || (utf8max != 0 && utf8N > utf8max)) {
				ret = CES_ERROR_CALLBACK_SRC_LEN;
				goto end;
			}
			utf8C = 1;
			if (utf16N != 0) {
				utf16C = ces->State.setCharCount;
				if (dstEndianSwap && utf16N <= utf16max) {
					uint16_t *utf16_ = utf16buf;
					uint16_t code_;
					uint32_t count_;

					for (count_ = utf16N; count_ != 0; count_--) {
						code_ = *utf16_;
						*utf16_++ = code_ << 8 | code_ >> 8;
					}
				}
				goto next;
			}
			if (ces->State.codeLen == 0)
				goto skip;
			utf16N = ces->State.codeLen;
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
		}
write:
		utf16C = 1;
		if (utf16N <= utf16max) {
			if (dstEndianSwap) {
				uint16_t *utf16_ = utf16buf;
				uint16_t *utf16rv_ = utf16;
				uint16_t code_;
				uint32_t count_;

				for (count_ = utf16N; count_ != 0; count_--) {
					code_ = *utf16rv_++;
					*utf16_++ = code_ << 8 | code_ >> 8;
				}
			} else {
				uint32_t _ces_copy_code_count = utf16N;

				for (; _ces_copy_code_count != 0; _ces_copy_code_count--)
					utf16buf[_ces_copy_code_count - 1] = utf16[_ces_copy_code_count - 1];
			}
		}
next:
		if (utf16N <= utf16max) {
			utf16buf += utf16N;
			utf16max -= utf16N;
			dstCount += utf16C;
			dstLen += utf16N;
			srcLen += utf8N;
			srcCount += utf8C;
		} else {
			utf16max = 0;
			dstOverLen += utf16N;
		}
		dstStrC += utf16C;
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
	if (utf16buf != NULL) {
		if (dstOverLen != 0)
			ret = SCE_CES_ERROR_DST_BUFFER_END;
	} else {
		srcLen = utf8addr - utf8str;
		dstLen = dstLen + dstOverLen;
		if (ret == SCE_CES_OK)
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	if (utf8Size != NULL)
		*utf8Size = srcLen;
	if (utf16Len != NULL)
		*utf16Len = dstLen;
	return ret;
}

int sceCesUtf8StrGetUtf16Len(SceCesUcsContext *ces, const uint8_t *utf8str, uint32_t utf8max,
			     uint32_t *utf8Size, uint32_t *utf16Len)
{
	int ret;

	if (utf8str != NULL) {
		if (utf16Len != NULL) {
			ret = cesUtf8StrToUtf16Codes(ces, utf8str, utf8max, utf8Size, NULL, 0,
						     utf16Len);
			if (ret == SCE_CES_ERROR_INVALID_DST_BUFFER)
				ret = SCE_CES_OK;
			return ret;
		}
		ret = SCE_CES_ERROR_INVALID_PARAMETER;
	} else {
		if (utf16Len != NULL)
			*utf16Len = 0;
		ret = SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	if (utf8Size != NULL)
		*utf8Size = 0;
	return _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint16_t), ret);
}

int sceCesUtf8StrToUtf16Str(SceCesUcsContext *ces, const uint8_t *utf8str, uint32_t utf8max,
			    uint32_t *utf8Size, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len)
{
	int ret;

	if (utf8str != NULL) {
		uint32_t utf16Count;
		uint32_t max = 0;

		if (utf16max != 0)
			max = utf16max - 1;
		ret = cesUtf8StrToUtf16Codes(ces, utf8str, utf8max, utf8Size, utf16buf, max,
					     &utf16Count);
		if (utf16buf != NULL) {
			if (utf16Len != NULL)
				*utf16Len = utf16Count;
			if (utf16max != 0)
				utf16buf[utf16Count] = 0;
			return ret;
		}
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	} else {
		if (utf16buf != NULL && utf16max != 0)
			utf16buf[0] = 0;
		ret = _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint16_t),
					    SCE_CES_ERROR_INVALID_SRC_BUFFER);
	}
	if (utf8Size != NULL)
		*utf8Size = 0;
	if (utf16Len != NULL)
		*utf16Len = 0;
	return ret;
}
