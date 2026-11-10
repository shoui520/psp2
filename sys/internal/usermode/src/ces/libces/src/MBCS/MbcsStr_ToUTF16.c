/*
 * libSceCes's MbcsStr_ToUTF16.c (Sony's internal/usermode/src/ces/libces/src/MBCS/MbcsStr_ToUTF16.c,
 * the SDK's static library, built with SNC): strings of a multi-byte character set to UTF-16,
 * by the context's profile. Names of the routines, their parameters, locals and labels are
 * Sony's (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_mbcs_context.h"

#define CES_UCS_BOM		0xfeff

/* the longest code of a multi-byte character set, and of UTF-16 */
#define CES_MBC_CODE_MAX	4
#define CES_UTF16_CODE_MAX	2

static int cesMbcsStrToUtf16Codes(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr,
				  uint32_t mbcsMax, uint32_t *mbcsLen, uint16_t *utf16buf,
				  uint32_t utf16max, uint32_t *utf16Len)
{
	const SceCesMbcsUcsProfile *prof = ces->Profile;
	const uint8_t *mbcsAddr = mbcsStr;
	uint32_t mbcsSize;
	uint32_t srcLen = 0;
	uint32_t srcCount = 0;
	uint32_t dstLen = 0;
	uint32_t dstCount = 0;
	uint32_t dstOverLen = 0;
	uint32_t srcStrC = 0;
	uint32_t dstStrC = 0;
	uint32_t srcStrMax;
	uint32_t dstStrMax;
	uint32_t mbcsN = 0;
	int ret = SCE_CES_OK;
	int outputBom;
	int dstEndianSwap = 0;
	const SceCesMbcsToUtf16ErrorCallbacks *callbacks = NULL;
	const SceCesMbcsToUcsErrorCallbacks *subCallbacks = NULL;
	SceCesMbcsToUtf16ErrorCallbacks Callbacks;
	SceCesMbcsToUcsErrorCallbacks SubCallbacks;

	outputBom = ces->UcsSetting.outputBom;
	if (ces->strContinue &&
	    (outputBom == SCE_CES_OUTPUT_TOP_SET || outputBom == SCE_CES_OUTPUT_TOP_CUT))
		outputBom = SCE_CES_OUTPUT_ENABLE;
	ces->strContinue = ces->strFrame;
	srcStrMax = ces->srcCharMax;
	dstStrMax = ces->dstCharMax;
	ces->State.codeBuffer = NULL;
	if (ces->errOperation != 0) {
		_sceCesSetupMbcsToUtf16ErrorCallbacks(ces->errOperation, &Callbacks, &SubCallbacks);
		callbacks = &Callbacks;
		subCallbacks = &SubCallbacks;
	} else if (ces->Callbacks != NULL) {
		subCallbacks = &ces->Callbacks->MbcsToUcs;
		callbacks = &ces->Callbacks->MbcsToUtf16;
	}
	if ((ces->UcsSetting.utf16 & 0xf) != CES_ENDIAN_LE)
		dstEndianSwap = 1;

	mbcsSize = CES_MBC_CODE_MAX;
	if (mbcsMax != 0)
		mbcsSize = mbcsMax;

loop:
	while (srcStrC < srcStrMax && dstStrC < dstStrMax) {
		uint32_t mbcsC;
		uint16_t *utf16addr;
		uint32_t utf16N;
		uint32_t utf16C;
		uint16_t utf16[CES_UTF16_CODE_MAX];
		uint32_t utf16size;

		utf16size = CES_UTF16_CODE_MAX;
		utf16addr = utf16;
		if (utf16max != 0) {
			utf16addr = utf16buf;
			utf16size = utf16max;
		}
		if (dstStrC == 0 && outputBom == SCE_CES_OUTPUT_TOP_SET) {
			mbcsN = 0;
			mbcsC = 0;
			utf16addr[0] = CES_UCS_BOM;
			utf16N = 1;
			utf16C = 1;
		} else {
			if (*mbcsAddr == 0)
				break;
			ret = sceCesMbcToUtf16(prof, mbcsAddr, mbcsSize, &mbcsN, utf16addr, utf16size,
					       &utf16N);
			if (ret != SCE_CES_OK) {
				SceCesMbcsToUtf16ErrorCallback *callback = NULL;

				if (callbacks != NULL) {
					if (ret == SCE_CES_ERROR_UNASSIGNED_CODE) {
						callback = callbacks->UnassignedCode;
						ces->State.callback = subCallbacks->UnassignedCode;
					} else if (ret == SCE_CES_ERROR_INVALID_ENCODE) {
						callback = callbacks->InvalidEncode;
						ces->State.callback = subCallbacks->InvalidEncode;
					}
				}
				if (callback == NULL) {
					if (ret != SCE_CES_ERROR_DST_BUFFER_END)
						break;
					utf16max = 0;
					dstStrC++;
					dstOverLen += utf16N;
					srcStrC++;
					mbcsAddr += mbcsN;
					goto next;
				}
				ces->State.returnValue = ret;
				ces->State.codeLen = 0;
				ces->State.setCharCount = 0;
				utf16N = 0;
				ret = callback(mbcsAddr, mbcsMax, &mbcsN, utf16buf, utf16max, &utf16N,
					       &ces->State);
				utf16C = ces->State.setCharCount;
				if (ret != SCE_CES_OK)
					break;
				if (mbcsN == 0 || (mbcsMax != 0 && mbcsN > mbcsMax)) {
					ret = CES_ERROR_CALLBACK_SRC_LEN;
					goto end;
				}
				mbcsC = 1;
				if (utf16N == 0)
					goto skip;
			} else {
				utf16C = 1;
				mbcsC = 1;
			}
		}
swap:
		if (utf16N <= utf16max && dstEndianSwap) {
			uint16_t *utf16_ = utf16addr;
			uint16_t code_;
			uint32_t count_;

			for (count_ = utf16N; count_ != 0; count_--) {
				code_ = *utf16_;
				*utf16_++ = code_ << 8 | code_ >> 8;
			}
		}
		if (utf16N <= utf16max) {
			utf16max -= utf16N;
			utf16buf += utf16N;
			dstCount += utf16C;
			dstLen += utf16N;
			srcLen += mbcsN;
			srcCount += mbcsC;
		} else {
			utf16max = 0;
			dstOverLen += utf16N;
		}
		dstStrC += utf16C;
		mbcsAddr += mbcsN;
		srcStrC += mbcsC;
		goto next;
skip:
		mbcsAddr += mbcsN;
		if (dstOverLen == 0) {
			srcLen += mbcsN;
			srcCount++;
		}
		srcStrC++;
next:
		if (mbcsMax == 0)
			goto loop;
		if (mbcsN >= mbcsMax)
			break;
		mbcsMax -= mbcsN;
		mbcsSize = mbcsMax;
	}
end:
	{
		CesStrReport *report = ces->strReport;

		if (report != NULL) {
			report->srcCodeSize = sizeof(uint8_t);
			report->dstCodeSize = sizeof(uint16_t);
			report->lastCodeLen = mbcsN;
			report->lastCharCode = 0;
			report->returnValue = ret;
			report->strSrcLen = mbcsAddr - mbcsStr;
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
		srcLen = mbcsAddr - mbcsStr;
		dstLen = dstLen + dstOverLen;
		if (ret == SCE_CES_OK)
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}
	if (mbcsLen != NULL)
		*mbcsLen = srcLen;
	if (utf16Len != NULL)
		*utf16Len = dstLen;
	return ret;
}

int sceCesMbcsStrGetUtf16Len(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr, uint32_t mbcsMax,
			     uint32_t *mbcsLen, uint32_t *utf16Len)
{
	int ret;

	if (ces != NULL) {
		if (mbcsStr != NULL) {
			if (utf16Len != NULL) {
				ret = cesMbcsStrToUtf16Codes(ces, mbcsStr, mbcsMax, mbcsLen, NULL, 0,
							     utf16Len);
				if (ret == SCE_CES_ERROR_INVALID_DST_BUFFER)
					ret = SCE_CES_OK;
				return ret;
			}
			ret = SCE_CES_ERROR_INVALID_PARAMETER;
		} else {
			ret = SCE_CES_ERROR_INVALID_SRC_BUFFER;
		}
	} else {
		ret = SCE_CES_ERROR_INVALID_PARAMETER;
	}
	if (mbcsLen != NULL)
		*mbcsLen = 0;
	if (utf16Len != NULL)
		*utf16Len = 0;
	return _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint16_t), ret);
}

int sceCesMbcsStrToUtf16Str(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr, uint32_t mbcsMax,
			    uint32_t *mbcsLen, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len)
{
	int ret;

	if (ces != NULL) {
		if (mbcsStr != NULL) {
			uint32_t bufMax = 0;
			uint32_t utf16N;

			if (utf16max != 0)
				bufMax = utf16max - 1;
			ret = cesMbcsStrToUtf16Codes(ces, mbcsStr, mbcsMax, mbcsLen, utf16buf, bufMax,
						     &utf16N);
			if (utf16buf != NULL) {
				if (utf16Len != NULL)
					*utf16Len = utf16N;
				if (utf16max != 0)
					utf16buf[utf16N] = 0;
				return ret;
			}
			ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
		} else {
			ret = SCE_CES_ERROR_INVALID_SRC_BUFFER;
			goto length0;
		}
	} else {
		ret = SCE_CES_ERROR_INVALID_PARAMETER;
length0:
		if (utf16buf != NULL && utf16max != 0)
			utf16buf[0] = 0;
		ret = _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint16_t),
					    ret);
	}
	if (mbcsLen != NULL)
		*mbcsLen = 0;
	if (utf16Len != NULL)
		*utf16Len = 0;
	return ret;
}
