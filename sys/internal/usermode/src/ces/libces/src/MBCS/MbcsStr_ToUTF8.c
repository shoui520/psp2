/*
 * libSceCes's MbcsStr_ToUTF8.c (Sony's internal/usermode/src/ces/libces/src/MBCS):
 * strings of a multi-byte character set to UTF-8, by the context's profile.
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_mbcs_context.h"

#define CES_UCS_BOM             0xfeffU
#define CES_MBC_CODE_MAX        4U

extern int sceCesMbcToUtf8(const SceCesMbcsUcsProfile *prof, const uint8_t *mbcsBuf,
			   uint32_t mbcsMax, uint32_t *mbcsSize, uint8_t *utf8buf,
			   uint32_t utf8max, uint32_t *utf8Len);
extern void _sceCesSetupMbcsToUtf8ErrorCallbacks(uint32_t operation,
						 SceCesMbcsToUtf8ErrorCallbacks *callbacks,
						 SceCesMbcsToUcsErrorCallbacks *subCallbacks);

static int cesMbcsStrToUtf8Codes(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr,
				 uint32_t mbcsMax, uint32_t *mbcsLen, uint8_t *utf8buf,
				 uint32_t utf8max, uint32_t *utf8Len)
{
	const SceCesMbcsUcsProfile *prof = ces->Profile;
	const uint8_t *mbcsAddr = mbcsStr;
	uint32_t mbcsSize = (mbcsMax != 0U) ? mbcsMax : CES_MBC_CODE_MAX;
	uint32_t srcLen = 0U;
	uint32_t srcCount = 0U;
	uint32_t dstLen = 0U;
	uint32_t dstCount = 0U;
	uint32_t dstOverLen = 0U;
	uint32_t srcStrC = 0U;
	uint32_t dstStrC = 0U;
	uint32_t srcStrMax = ces->srcCharMax;
	uint32_t dstStrMax = ces->dstCharMax;
	uint32_t mbcsN = 0U;
	int ret = SCE_CES_OK;
	int outputBom = ces->UcsSetting.outputBom;
	const SceCesMbcsToUtf8ErrorCallbacks *callbacks = NULL;
	const SceCesMbcsToUcsErrorCallbacks *subCallbacks = NULL;
	SceCesMbcsToUtf8ErrorCallbacks localCallbacks;
	SceCesMbcsToUcsErrorCallbacks localSubCallbacks;

	if (ces->strContinue &&
	    (outputBom == SCE_CES_OUTPUT_TOP_SET || outputBom == SCE_CES_OUTPUT_TOP_CUT)) {
		outputBom = SCE_CES_OUTPUT_ENABLE;
	}
	ces->strContinue = ces->strFrame;
	ces->State.codeBuffer = NULL;

	if (ces->errOperation != 0U) {
		_sceCesSetupMbcsToUtf8ErrorCallbacks(ces->errOperation, &localCallbacks,
						     &localSubCallbacks);
		callbacks = &localCallbacks;
		subCallbacks = &localSubCallbacks;
	} else if (ces->Callbacks != NULL) {
		subCallbacks = &ces->Callbacks->MbcsToUcs;
		callbacks = &ces->Callbacks->MbcsToUtf8;
	}

	while (srcStrC < srcStrMax && dstStrC < dstStrMax) {
		uint32_t mbcsC;
		uint32_t utf8N = 0U;
		uint32_t utf8C = 0U;
		int skipStore = 0;

		if (dstStrC == 0U && outputBom == SCE_CES_OUTPUT_TOP_SET) {
			mbcsN = 0U;
			mbcsC = 0U;
			ret = sceCesUcs4ToUtf8(CES_UCS_BOM, utf8buf, utf8max, &utf8N);
		} else {
			if (*mbcsAddr == 0U)
				break;
			ret = sceCesMbcToUtf8(prof, mbcsAddr, mbcsSize, &mbcsN, utf8buf, utf8max,
					      &utf8N);
			mbcsC = 1U;
		}

		if (ret != SCE_CES_OK) {
			SceCesMbcsToUtf8ErrorCallback *callback = NULL;

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
				if (ret != SCE_CES_ERROR_INVALID_DST_BUFFER &&
				    ret != SCE_CES_ERROR_DST_BUFFER_END) {
					break;
				}
				utf8max = 0U;
				dstStrC++;
				dstOverLen += utf8N;
				skipStore = 1;
			} else {
				ces->State.returnValue = ret;
				ces->State.codeLen = 0U;
				ces->State.setCharCount = 0U;
				utf8N = 0U;
				ret = callback(mbcsAddr, mbcsMax, &mbcsN, utf8buf, utf8max, &utf8N,
					       &ces->State);
				if (ret != SCE_CES_OK)
					break;
				if (mbcsN == 0U || (mbcsMax != 0U && mbcsN > mbcsMax)) {
					ret = CES_ERROR_CALLBACK_SRC_LEN;
					break;
				}
				utf8C = ces->State.setCharCount;
				if (utf8N == 0U) {
					if (dstOverLen == 0U) {
						srcLen += mbcsN;
						srcCount += mbcsC;
					}
					skipStore = 1;
				}
			}
		} else {
			utf8C = 1U;
		}

		if (!skipStore) {
			if (utf8N <= utf8max) {
				utf8max -= utf8N;
				utf8buf += utf8N;
				dstCount += utf8C;
				dstLen += utf8N;
				srcLen += mbcsN;
				srcCount += mbcsC;
			} else {
				utf8max = 0U;
				dstOverLen += utf8N;
			}
			dstStrC += utf8C;
			ret = SCE_CES_OK;
		}

		mbcsAddr += mbcsN;
		srcStrC += mbcsC;
		if (mbcsMax != 0U) {
			if (mbcsN >= mbcsMax)
				break;
			mbcsMax -= mbcsN;
			mbcsSize = mbcsMax;
		}
	}

	{
		CesStrReport *report = ces->strReport;

		if (report != NULL) {
			report->srcCodeSize = sizeof(uint8_t);
			report->dstCodeSize = sizeof(uint8_t);
			report->lastCodeLen = (uint8_t)mbcsN;
			report->lastCharCode = 0U;
			report->returnValue = ret;
			report->strSrcLen = (uint32_t)(mbcsAddr - mbcsStr);
			report->strDstLen = dstLen + dstOverLen;
			report->strSrcCount = srcStrC;
			report->strDstCount = dstStrC;
			report->cnvSrcCount = srcCount;
			report->cnvDstCount = dstCount;
		}
	}

	if (utf8buf != NULL) {
		if (dstOverLen != 0U)
			ret = SCE_CES_ERROR_DST_BUFFER_END;
	} else {
		srcLen = (uint32_t)(mbcsAddr - mbcsStr);
		dstLen = dstLen + dstOverLen;
	}
	if (mbcsLen != NULL)
		*mbcsLen = srcLen;
	if (utf8Len != NULL)
		*utf8Len = dstLen;
	return ret;
}

int sceCesMbcsStrGetUtf8Len(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr, uint32_t mbcsMax,
			    uint32_t *mbcsLen, uint32_t *utf8Len)
{
	int ret;

	if (ces != NULL) {
		if (mbcsStr != NULL) {
			if (utf8Len != NULL) {
				ret = cesMbcsStrToUtf8Codes(ces, mbcsStr, mbcsMax, mbcsLen, NULL,
							    0U, utf8Len);
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
		*mbcsLen = 0U;
	if (utf8Len != NULL)
		*utf8Len = 0U;
	return _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint8_t), ret);
}

int sceCesMbcsStrToUtf8Str(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr, uint32_t mbcsMax,
			   uint32_t *mbcsLen, uint8_t *utf8buf, uint32_t utf8max,
			   uint32_t *utf8Len)
{
	int ret;

	if (ces == NULL) {
		ret = SCE_CES_ERROR_INVALID_PARAMETER;
		if (utf8buf != NULL && utf8max != 0U)
			utf8buf[0] = 0U;
		ret = _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint8_t),
					    ret);
	} else if (mbcsStr == NULL) {
		ret = SCE_CES_ERROR_INVALID_SRC_BUFFER;
		if (utf8buf != NULL && utf8max != 0U)
			utf8buf[0] = 0U;
		ret = _sceCesStrResetReport((CesStrContext *)ces, sizeof(uint8_t), sizeof(uint8_t),
					    ret);
	} else {
		uint32_t bufMax = (utf8max != 0U) ? (utf8max - 1U) : 0U;
		uint32_t utf8N = 0U;

		ret = cesMbcsStrToUtf8Codes(ces, mbcsStr, mbcsMax, mbcsLen, utf8buf, bufMax,
					    &utf8N);
		if (utf8buf != NULL) {
			if (utf8Len != NULL)
				*utf8Len = utf8N;
			if (utf8max != 0U)
				utf8buf[utf8N] = 0U;
			return ret;
		}
		ret = SCE_CES_ERROR_INVALID_DST_BUFFER;
	}

	if (mbcsLen != NULL)
		*mbcsLen = 0U;
	if (utf8Len != NULL)
		*utf8Len = 0U;
	return ret;
}
