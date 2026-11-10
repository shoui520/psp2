/*
 * libSceCes's MbcsStr_FromUTF16.c (Sony's internal/usermode/src/ces/libces/src/MBCS/MbcsStr_FromUTF16.c,
 * the SDK's static library, built with SNC): UTF-16 strings to a multi-byte character set, by
 * the context's profile. Names of the routines, their parameters, locals and labels are Sony's
 * (the library's DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_mbcs_context.h"
#include "../UCS/ces_ucs.h"

#define CES_UCS_BOM		0xfeff
#define CES_UCS_BOM_SWAPPED	0xfffe

static int cesUtf16StrToMbcsCodes(SceCesMbcsUcsContext *ces, const uint16_t *utf16str,
				  uint32_t utf16max, uint32_t *utf16Len, uint8_t *mbcsBuf,
				  uint32_t mbcsMax, uint32_t *mbcsLen)
{
	const SceCesMbcsUcsProfile *prof = ces->Profile;
	const uint16_t *utf16addr = utf16str;
	uint32_t utf16size;
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
	int srcEndianSwap;
	CesUtf16ToUcs4Func *funcUtf16ToUcs4;
	CesUtf16ToUcs4Func *func[2];
	uint32_t utf16N = 0;
	uint32_t ucs4 = 0;
	const SceCesUtf16ToMbcsErrorCallbacks *callbacks = NULL;
	const SceCesUcsToMbcsErrorCallbacks *subCallbacks = NULL;
	SceCesUtf16ToMbcsErrorCallbacks Callbacks;
	SceCesUcsToMbcsErrorCallbacks SubCallbacks;
	uint32_t srcEndian0;

	srcStrMax = ces->srcCharMax;
	dstStrMax = ces->dstCharMax;
	if (ces->strContinue == 0)
		ces->State.ctx.src.endian = ces->UcsSetting.utf16 >> 4;
	ces->strContinue = ces->strFrame;
	func[0] = sceCesUtf16ToUcs4;
	func[1] = sceCesUtf16beToUcs4;
	srcEndian0 = ces->State.ctx.src.endian;
	srcEndianSwap = ces != NULL && ces->State.ctx.src.endian == CES_ENDIAN_BE;
	funcUtf16ToUcs4 = func[srcEndianSwap];
	ces->State.codeBuffer = NULL;
	if (ces->errOperation != 0) {
		_sceCesSetupUtf16ToMbcsErrorCallbacks(ces->errOperation, &Callbacks, &SubCallbacks);
		callbacks = &Callbacks;
		subCallbacks = &SubCallbacks;
	} else if (ces->Callbacks != NULL) {
		callbacks = &ces->Callbacks->Utf16ToMbcs;
		subCallbacks = &ces->Callbacks->UcsToMbcs;
	}

	utf16size = sizeof(uint16_t);
	if (utf16max != 0)
		utf16size = utf16max;

loop:
	while (srcStrC < srcStrMax && dstStrC < dstStrMax) {
		uint32_t mbcsN;

		ret = funcUtf16ToUcs4(utf16addr, utf16size, &utf16N, &ucs4);
		if (ret != SCE_CES_OK) {
			mbcsN = 0;
		} else {
			switch (ucs4) {
			case 0:
				goto end;
			case CES_UCS_BOM_SWAPPED:
				if (ces->UcsSetting.detectBom == SCE_CES_DETECT_DISABLE)
					goto convert;
				srcEndianSwap ^= 1;
				ces->State.ctx.src.endian = !ces->State.ctx.src.endian;
				funcUtf16ToUcs4 = func[srcEndianSwap];
				if (dstOverLen == 0)
					srcEndian0 = ces->State.ctx.src.endian;
				/* FALLTHROUGH */
			case CES_UCS_BOM:
				goto skip;
			default:
convert:
				ret = sceCesUcs4ToMbc(ucs4, prof, mbcsBuf, mbcsMax, &mbcsN);
				break;
			}
		}
		if (ret == SCE_CES_OK) {
			mbcsMax -= mbcsN;
			mbcsBuf += mbcsN;
			srcCount++;
			dstLen += mbcsN;
			dstCount++;
			dstStrC++;
			srcLen += utf16N;
		} else {
			SceCesUtf16ToMbcsErrorCallback *callback = NULL;

			if (callbacks != NULL) {
				if (ret == SCE_CES_ERROR_UNASSIGNED_CODE) {
					callback = callbacks->UnassignedCode;
					ces->State.callback = subCallbacks->UnassignedCode;
				} else if (ret == SCE_CES_ERROR_ILLEGAL_CODE) {
					callback = callbacks->IllegalCode;
					ces->State.callback = subCallbacks->IllegalCode;
				}
			}
			if (callback == NULL) {
				if (ret != SCE_CES_ERROR_DST_BUFFER_END &&
				    ret != SCE_CES_ERROR_INVALID_DST_BUFFER)
					break;
				mbcsMax = 0;
				dstOverLen += mbcsN;
				dstStrC++;
			} else {
				if (utf16N == 0)
					utf16N = 1;
				ces->State.returnValue = ret;
				ces->State.codeLen = 0;
				ces->State.setCharCount = 0;
				mbcsN = 0;
				ret = callback(utf16addr, utf16max, &utf16N, mbcsBuf, mbcsMax, &mbcsN,
					       &ces->State);
				if (ret != SCE_CES_OK)
					break;
				{
					uint32_t mbcsC = ces->State.setCharCount;

					if (utf16N == 0 || (utf16max != 0 && utf16N > utf16max)) {
						ret = CES_ERROR_CALLBACK_SRC_LEN;
						goto end;
					}
					if (mbcsN <= mbcsMax) {
						mbcsMax -= mbcsN;
						mbcsBuf += mbcsN;
						dstLen += mbcsN;
						srcLen += utf16N;
						srcCount++;
						dstCount += mbcsC;
					} else {
						mbcsMax = 0;
						dstOverLen += mbcsN;
					}
					dstStrC += mbcsC;
				}
			}
		}
		goto next;
skip:
		if (dstOverLen == 0) {
			srcLen += utf16N;
			srcCount++;
		}
next:
		srcStrC++;
		utf16addr += utf16N;
		if (utf16max == 0)
			goto loop;
		if (utf16N >= utf16max)
			break;
		utf16max -= utf16N;
		utf16size = utf16max;
	}
end:
	{
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
	if (mbcsBuf != NULL) {
		if (dstOverLen != 0) {
			ret = SCE_CES_ERROR_DST_BUFFER_END;
			ces->State.ctx.src.endian = srcEndian0;
		}
	} else {
		dstLen = dstLen + dstOverLen;
		srcLen = utf16addr - utf16str;
	}
	if (utf16Len != NULL)
		*utf16Len = srcLen;
	if (mbcsLen != NULL)
		*mbcsLen = dstLen;
	return ret;
}

int sceCesUtf16StrGetMbcsLen(SceCesMbcsUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			     uint32_t *utf16Len, uint32_t *mbcsLen)
{
	int ret;

	if (ces != NULL) {
		if (utf16str != NULL) {
			if (mbcsLen != NULL) {
				ret = cesUtf16StrToMbcsCodes(ces, utf16str, utf16max, utf16Len, NULL, 0,
							     mbcsLen);
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
	if (utf16Len != NULL)
		*utf16Len = 0;
	if (mbcsLen != NULL)
		*mbcsLen = 0;
	return _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint16_t), sizeof(uint8_t), ret);
}

int sceCesUtf16StrToMbcsStr(SceCesMbcsUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			    uint32_t *utf16Len, uint8_t *mbcsBuf, uint32_t mbcsMax, uint32_t *mbcsLen)
{
	int ret;

	if (ces != NULL) {
		if (utf16str != NULL) {
			uint32_t bufMax = 0;
			uint32_t mbcsN;

			if (mbcsMax != 0)
				bufMax = mbcsMax - 1;
			ret = cesUtf16StrToMbcsCodes(ces, utf16str, utf16max, utf16Len, mbcsBuf, bufMax,
						     &mbcsN);
			if (mbcsBuf != NULL) {
				if (mbcsLen != NULL)
					*mbcsLen = mbcsN;
				if (mbcsMax != 0)
					mbcsBuf[mbcsN] = '\0';
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
		if (mbcsBuf != NULL && mbcsMax != 0)
			mbcsBuf[0] = '\0';
		ret = _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint16_t), sizeof(uint8_t),
					    ret);
	}
	if (utf16Len != NULL)
		*utf16Len = 0;
	if (mbcsLen != NULL)
		*mbcsLen = 0;
	return ret;
}
