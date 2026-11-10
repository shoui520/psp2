/*
 * libSceCes's string context (Sony's internal/usermode/src/ces/libces/src/ces_context.h): the
 * head every string conversion context starts with, and the report a conversion fills in. Laid
 * out and named from the library's debug information (SDK 3.570.011 libSceCes.a); the SDK's
 * public header (ces/libces_str.h) shows the context as an opaque sheet.
 */
#ifndef _CES_CONTEXT_H
#define _CES_CONTEXT_H

#include <stdint.h>
#include <ces/error.h>

/*
 * An error callback that reports no source consumed, or more than there is. No SDK header names
 * the code.
 */
#define CES_ERROR_CALLBACK_SRC_LEN	-2141454331	/* (0x805C0005) */

typedef enum SceCesEnable {
	CES_DISABLE = 0,
	CES_ENABLE = 1
} SceCesEnable;

typedef enum SceCesErrorOperation {
	SCE_CES_ERR_OPERATION_RETURN = 0,
	SCE_CES_ERR_OPERATION_OK = 1,
	SCE_CES_ERR_OPERATION_SKIP = 2,
	SCE_CES_ERR_OPERATION_REPLACE = 3
} SceCesErrorOperation;

/* what a string conversion did: the code units it read and wrote, and how it ended */
typedef struct CesStrReport {
	uint8_t srcCodeSize;
	uint8_t dstCodeSize;
	uint8_t reserved;
	uint8_t lastCodeLen;
	uint32_t lastCharCode;
	int32_t returnValue;
	uint32_t strSrcLen;
	uint32_t strDstLen;
	uint32_t strSrcCount;
	uint32_t strDstCount;
	uint32_t cnvSrcCount;
	uint32_t cnvDstCount;
} CesStrReport;

/* the head of every string conversion context */
typedef struct CesStrContext {
	CesStrReport *strReport;
	uint32_t srcCharMax;
	uint32_t dstCharMax;
	uint32_t strContinue:1;
	uint32_t strFrame:1;
	uint32_t reserved:6;
	uint32_t escFilter:8;
	uint32_t errOperation:16;
} CesStrContext;

void _sceCesStrReportReset(CesStrReport *report, uint32_t srcEncodeUnit, uint32_t dstEncodeUnit,
			   int ret);
void _sceCesStrContextInit(CesStrContext *strCtx);
int _sceCesStrResetReport(CesStrContext *ces, uint32_t srcEncodeUnit, uint32_t dstEncodeUnit,
			  int ret);

#endif /* _CES_CONTEXT_H */
