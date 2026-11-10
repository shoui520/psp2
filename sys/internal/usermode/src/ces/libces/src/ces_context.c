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
