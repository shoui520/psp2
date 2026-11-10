/*
 * libSceCes's Str_UTF16_UTF8.c (Sony's internal/usermode/src/ces/libces/src/UCS/Str_UTF16_UTF8.c,
 * the SDK's static library, built with SNC): UTF-16 strings to UTF-8. Names of the routines,
 * their parameters, locals and labels are Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "../ces_context.h"
#include "ces_ucs.h"
#include "ces_ucs_callback.h"
#include "ces_ucs_context.h"

#define CES_UCS_BOM		0xfeff
#define CES_UCS_BOM_SWAPPED	0xfffe

/* the longest UTF-8 sequence */
#define CES_UTF8_CODE_MAX	6

static int cesUtf16StrToUtf8Codes(SceCesUcsContext *ces, const uint16_t *utf16str,
				  uint32_t utf16max, uint32_t *utf16Len, uint8_t *utf8buf,
				  uint32_t utf8max, uint32_t *utf8Len)
{
	const uint16_t *utf16addr = utf16str;
	uint32_t srcLen = 0;
	uint32_t srcCount = 0;
	uint32_t dstLen = 0;
	uint32_t dstCount = 0;
	uint32_t dstOverLen = 0;
	uint32_t srcStrC = 0;
	uint32_t dstStrC = 0;
	uint32_t srcStrMax;
	uint32_t dstStrMax;
	uint32_t utf16size;
	int ret = SCE_CES_OK;
	CesUtf16ToUcsUtf8Func *funcUtf16ToUcsUtf8;
	CesUtf16ToUcsUtf8Func *func[2];
	int outputBom;
	int srcEndianSwap;
	uint32_t utf16N = 0;
	uint8_t utf8code[CES_UTF8_CODE_MAX];
	uint32_t ucs4 = 0;
	const SceCesUtf16ToUtf8ErrorCallbacks *callbacks = NULL;
	SceCesUtf16ToUtf8ErrorCallbacks Callbacks;
	uint32_t srcEndian0;

	if (ces != NULL) {
		outputBom = ces->UcsSetting.outputBom;
		if (ces->strContinue == 0) {
			ces->State.ctx.src.endian = ces->UcsSetting.utf16 >> 4;
		} else {
			if (outputBom == SCE_CES_OUTPUT_TOP_SET || outputBom == SCE_CES_OUTPUT_TOP_CUT)
				outputBom = SCE_CES_OUTPUT_ENABLE;
		}
		ces->strContinue = ces->strFrame;
	} else {
		outputBom = SCE_CES_OUTPUT_ENABLE;
	}

	func[0] = sceCesUtf16ToUcsUtf8;
	func[1] = sceCesUtf16beToUcsUtf8;
	srcEndianSwap = ces != NULL && ces->State.ctx.src.endian == CES_ENDIAN_BE;
	funcUtf16ToUcsUtf8 = func[srcEndianSwap];

	if (ces != NULL) {
		srcStrMax = ces->srcCharMax;
		dstStrMax = ces->dstCharMax;
		if (ces->errOperation != 0)
			callbacks = _sceCesSetupUtf16ToUtf8ErrorCallbacks(ces->errOperation, &Callbacks);
		else if (ces->UcsCallbacks != NULL)
			callbacks = &ces->UcsCallbacks->Utf16ToUtf8;
		srcEndian0 = ces->State.ctx.src.endian;
	} else {
		srcStrMax = 0xffffffff;
		dstStrMax = 0xffffffff;
		srcEndian0 = 0;
	}

	utf16size = sizeof(uint16_t);
	if (utf16max != 0)
		utf16size = utf16max;
	if (utf8buf == NULL)
		utf8max = 0;

loop:
	while (srcStrC < srcStrMax && dstStrC < dstStrMax) {
		uint32_t utf16C;
		uint32_t utf8N;
		uint32_t utf8C;

		if (dstStrC == 0 && outputBom == SCE_CES_OUTPUT_TOP_SET) {
			utf16C = 0;
			utf16N = 0;
			ucs4 = CES_UCS_BOM;
			ret = sceCesUcs4ToUtf8(CES_UCS_BOM, utf8code, CES_UTF8_CODE_MAX, &utf8N);
			outputBom = SCE_CES_OUTPUT_TOP_CUT;
		} else {
			if (*utf16addr == 0)
				break;
			ret = funcUtf16ToUcsUtf8(utf16addr, utf16size, &utf16N, &ucs4, utf8code,
						 CES_UTF8_CODE_MAX, &utf8N);
			utf16C = 1;
			if (ucs4 == CES_UCS_BOM_SWAPPED) {
				if (ces != NULL) {
					if (ces->UcsSetting.detectBom == SCE_CES_DETECT_DISABLE)
						goto convert;
					ces->State.ctx.src.endian = !ces->State.ctx.src.endian;
					if (dstOverLen == 0 && utf8max >= 3)
						srcEndian0 = ces->State.ctx.src.endian;
				}
				srcEndianSwap ^= 1;
				ucs4 = CES_UCS_BOM;
				utf8code[0] = 0xef;
				utf8code[1] = 0xbb;
				utf8code[2] = 0xbf;
				funcUtf16ToUcsUtf8 = func[srcEndianSwap];
				goto output_bom_check;
			}
			if (ucs4 == CES_UCS_BOM) {
output_bom_check:
				if (outputBom == SCE_CES_OUTPUT_DISABLE)
					goto skip;
				if (outputBom == SCE_CES_OUTPUT_TOP_CUT && srcStrC == 0)
					goto skip;
			}
		}
convert:
		if (ret == SCE_CES_OK) {
			utf8C = 1;
			goto write;
		} else {
			SceCesUtf16ToUtf8ErrorCallback *callback;

			if (callbacks == NULL)
				break;
			callback = NULL;
			utf8C = 0;
			if (ret == SCE_CES_ERROR_ILLEGAL_CODE) {
				callback = callbacks->IllegalCode;
				utf8C = 1;
			}
			if (callback == NULL)
				break;
			ces->State.returnValue = ret;
			ces->State.codeLen = utf8N;
			ces->State.setCharCount = utf8C;
			utf8N = 0;
			ret = callback(utf16addr, utf16max, &utf16N, utf8buf, utf8max, &utf8N,
				       &ces->State);
			if (ret != SCE_CES_OK)
				break;
			if (utf16N == 0 || (utf16max != 0 && utf16N > utf16max)) {
				ret = CES_ERROR_CALLBACK_SRC_LEN;
				goto end;
			}
			if (utf8N == 0) {
				if (ces->State.codeLen == 0)
					goto skip;
				utf8N = ces->State.codeLen;
				goto write;
			}
			utf8C = ces->State.setCharCount;
			goto next;
		}
write:
		if (utf8N <= utf8max) {
			uint32_t _ces_copy_code_count = utf8N;

			for (; _ces_copy_code_count != 0; _ces_copy_code_count--)
				utf8buf[_ces_copy_code_count - 1] = utf8code[_ces_copy_code_count - 1];
		}
next:
		if (utf8N <= utf8max) {
			utf8buf += utf8N;
			utf8max -= utf8N;
			dstCount += utf8C;
			dstLen += utf8N;
			srcLen += utf16N;
			srcCount += utf16C;
		} else {
			dstOverLen += utf8N;
			utf8max = 0;
		}
		dstStrC += utf8C;
		goto advance;
skip:
		if (dstOverLen == 0) {
			srcCount += utf16C;
			srcLen += utf16N;
		}
advance:
		utf16addr += utf16N;
		srcStrC += utf16C;
		if (utf16max == 0)
			goto loop;
		if (utf16N >= utf16max)
			break;
		utf16max -= utf16N;
		utf16size = utf16max;
	}
end:
	if (ces != NULL) {
		CesStrReport *report = ces->strReport;

		if (report != NULL) {
			report->srcCodeSize = sizeof(uint16_t);
			report->dstCodeSize = sizeof(uint8_t);
			report->lastCodeLen = utf16N;
			report->lastCharCode = ucs4;
			report->returnValue = ret;
			report->strSrcLen = utf16addr - utf16str;
			report->strDstLen = dstLen + dstOverLen;
			report->strSrcCount = srcStrC;
			report->strDstCount = dstStrC;
			report->cnvSrcCount = srcCount;
			report->cnvDstCount = dstCount;
		}
	}
	if (utf8buf == NULL) {
		srcLen = utf16addr - utf16str;
		dstLen = dstLen + dstOverLen;
	} else if (dstOverLen != 0) {
		ret = SCE_CES_ERROR_DST_BUFFER_END;
		if (ces != NULL)
			ces->State.ctx.src.endian = srcEndian0;
	}
	if (utf16Len != NULL)
		*utf16Len = srcLen;
	if (utf8Len != NULL)
		*utf8Len = dstLen;
	return ret;
}

int sceCesUtf16StrGetUtf8Len(SceCesUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			     uint32_t *utf16Len, uint32_t *utf8Len)
{
	int ret;

	if (utf16str != NULL) {
		if (utf8Len != NULL) {
			ret = cesUtf16StrToUtf8Codes(ces, utf16str, utf16max, utf16Len, NULL, 0,
						     utf8Len);
			if (ret == SCE_CES_ERROR_INVALID_DST_BUFFER)
				ret = SCE_CES_OK;
			return ret;
		}
		ret = SCE_CES_ERROR_INVALID_PARAMETER;
	} else {
		if (utf8Len != NULL)
			*utf8Len = 0;
		ret = SCE_CES_ERROR_INVALID_SRC_BUFFER;
	}
	if (utf16Len != NULL)
		*utf16Len = 0;
	return _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint16_t), sizeof(uint8_t), ret);
}

int sceCesUtf16StrToUtf8Str(SceCesUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			    uint32_t *utf16Len, uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Len)
{
	int ret;
	uint32_t len;

	if (utf16str != NULL) {
		ret = cesUtf16StrToUtf8Codes(ces, utf16str, utf16max, utf16Len, utf8buf,
					     utf8max != 0 ? utf8max - 1 : 0, &len);
		if (utf8buf != NULL) {
			if (utf8Len != NULL)
				*utf8Len = len;
			if (utf8max != 0)
				utf8buf[len] = '\0';
			return ret;
		}
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	} else {
		if (utf8buf != NULL && utf8max != 0)
			utf8buf[0] = '\0';
		ret = _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint16_t), sizeof(uint8_t),
					    SCE_CES_ERROR_INVALID_SRC_BUFFER);
	}
	if (utf16Len != NULL)
		*utf16Len = 0;
	if (utf8Len != NULL)
		*utf8Len = 0;
	return ret;
}
