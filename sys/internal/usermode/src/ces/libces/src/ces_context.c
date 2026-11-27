/*
 * libSceCes's ces_context.c (Sony's internal/usermode/src/ces/libces/src/ces_context.c, the
 * SDK's static library, built with SNC): the string context every conversion starts from and
 * the report it fills in. SNC's linker keeps only the routines a module calls; the tree has the
 * ones libpaf calls. Names of the routines and their parameters are Sony's (the library's
 * DWARF).
 */
#include <stdint.h>
#include <stddef.h>
#include "ces_context.h"

void _sceCesStrReportReset(CesStrReport *report, uint32_t srcEncodeUnit, uint32_t dstEncodeUnit,
			   int ret)
{
	if (report == NULL)
		return;
	report->srcCodeSize = srcEncodeUnit;
	report->dstCodeSize = dstEncodeUnit;
	report->lastCodeLen = 0;
	report->reserved = 0;
	report->returnValue = ret;
	report->strSrcLen = 0;
	report->strDstLen = 0;
	report->strSrcCount = 0;
	report->strDstCount = 0;
	report->cnvSrcCount = 0;
	report->cnvDstCount = 0;
}

void _sceCesStrContextInit(CesStrContext *strCtx)
{
	strCtx->srcCharMax = 0xffffffff;
	strCtx->dstCharMax = 0xffffffff;
	strCtx->strContinue = 0;
	strCtx->strFrame = 0;
	strCtx->escFilter = 0;
	strCtx->errOperation = 0;
	strCtx->strReport = NULL;
}

int _sceCesStrResetReport(CesStrContext *ces, uint32_t srcEncodeUnit, uint32_t dstEncodeUnit,
			  int ret)
{
	if (ces != NULL)
		_sceCesStrReportReset(ces->strReport, srcEncodeUnit, dstEncodeUnit, ret);
	return ret;
}

int _sceCesStrSetReport(CesStrContext *ces, CesStrReport *report)
{
	if (ces == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	ces->strReport = report;
	return SCE_CES_OK;
}

int sceCesSetStrBegin(CesStrContext *context)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->strFrame = 1;
	return SCE_CES_OK;
}

int sceCesSetStrLast(CesStrContext *context)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->strContinue = 1;
	context->strFrame = 0;
	return SCE_CES_OK;
}

int sceCesSetStrEnd(CesStrContext *context)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->strContinue = 0;
	context->strFrame = 0;
	return SCE_CES_OK;
}

int sceCesSetStrContinue(CesStrContext *context)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->strContinue = 1;
	return SCE_CES_OK;
}

int sceCesSetStrMaxCharCount(CesStrContext *context, uint32_t srcMax, uint32_t dstMax)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->srcCharMax = (srcMax != 0) ? srcMax : 0xffffffffu;
	context->dstCharMax = (dstMax != 0) ? dstMax : 0xffffffffu;
	return SCE_CES_OK;
}

int _sceCesStrSetFilterEscSeq(CesStrContext *ces, uint32_t enable)
{
	if (ces == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	if (enable == CES_DISABLE) {
		ces->escFilter = 0;
	} else if (enable == CES_ENABLE) {
		ces->escFilter = 1;
	} else {
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	return SCE_CES_OK;
}

int sceCesUnsetErrorOperation(CesStrContext *context)
{
	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->errOperation = 0;
	return SCE_CES_OK;
}

int sceCesSetErrorOperation(CesStrContext *context, int errorType, int errorOperation)
{
	uint32_t shift;

	if (context == NULL)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	switch (errorType) {
	case SCE_CES_ERROR_INVALID_ENCODE:
		shift = 0;
		break;
	case SCE_CES_ERROR_ILLEGAL_CODE:
		shift = 4;
		break;
	case SCE_CES_ERROR_OUT_OF_CODE_RANGE:
		shift = 8;
		break;
	case SCE_CES_ERROR_UNASSIGNED_CODE:
		shift = 12;
		break;
	default:
		return SCE_CES_ERROR_INVALID_PARAMETER;
	}
	if ((uint32_t)errorOperation >= 4)
		return SCE_CES_ERROR_INVALID_PARAMETER;
	context->errOperation = (context->errOperation & ~(0xfu << shift)) | ((uint32_t)errorOperation << shift);
	return SCE_CES_OK;
}
