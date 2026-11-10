/*
 * libSceCes's UCS context (Sony's internal/usermode/src/ces/libces/src/UCS/ces_ucs_context.h):
 * the string conversion context between the Unicode forms, which the other character sets'
 * contexts extend. Laid out and named from the library's debug information (SDK 3.570.011
 * libSceCes.a); the SDK's public header (ces/libces_ucs.h) shows the same context as an opaque
 * sheet.
 */
#ifndef _CES_UCS_CONTEXT_H
#define _CES_UCS_CONTEXT_H

#include <stdint.h>
#include "../ces_context.h"
#include "../ces_callback.h"
#include "ces_ucs_setting.h"
#include "ces_ucs_callback.h"

typedef enum SceCesEndianParam {
	CES_ENDIAN_BE = 0,
	CES_ENDIAN_LE = 1,
	CES_ENDIAN_SYS = 2,
	CES_ENDIAN_STAY = 3
} SceCesEndianParam;

/* the conversions another character set adds to UCS's */
typedef struct SceCesUcsExtraFuncs {
	void *srcUcs4;
	void *srcUcs2;
	void *srcUtf32;
	void *srcUtf16;
	void *srcUtf8;
	void *dstUcs4;
	void *dstUcs2;
	void *dstUtf32;
	void *dstUtf16;
	void *dstUtf8;
} SceCesUcsExtraFuncs;

struct SceCesUcsContext;

typedef int SceCesSetReplacementCharUCodeFunc(struct SceCesUcsContext *ces, uint32_t ucode);
typedef int SceCesOtherStrGetUcsEncodingSizeFunc(struct SceCesUcsContext *ces, const void *src,
						  uint32_t srcMax, uint32_t *srcLen, int ucsType,
						  uint32_t *dstLen);
typedef int SceCesUcsStrGetOtherEncodingSizeFunc(struct SceCesUcsContext *ces, int ucsType,
						  const void *src, uint32_t srcMax,
						  uint32_t *srcLen, uint32_t *dstLen);
typedef int SceCesOtherStrToUcsStrFunc(struct SceCesUcsContext *ces, const void *src,
					uint32_t srcMax, uint32_t *srcLen, int ucsType, void *dst,
					uint32_t dstMax, uint32_t *dstLen);
typedef int SceCesUcsStrToOtherStrFunc(struct SceCesUcsContext *ces, int ucsType, const void *src,
					uint32_t srcMax, uint32_t *srcLen, void *dst, uint32_t dstMax,
					uint32_t *dstLen);

/* what a character set's context adds to the UCS conversions */
typedef struct SceCesOtherCesFuncs {
	int cesType;
	const SceCesUcsExtraFuncs *UcsExtra;
	SceCesSetReplacementCharUCodeFunc *SetReplacementCharUCode;
	SceCesSetReplacementCharUCodeFunc *SetReplacementCharUCode2;
	SceCesOtherStrGetUcsEncodingSizeFunc *OtherStrGetUcsEncodingSize;
	SceCesUcsStrGetOtherEncodingSizeFunc *UcsStrGetOtherEncodingSize;
	SceCesOtherStrToUcsStrFunc *OtherStrToUcsStr;
	SceCesUcsStrToOtherStrFunc *UcsStrToOtherStr;
} SceCesOtherCesFuncs;

typedef struct SceCesUcsContext {
	CesStrReport *strReport;
	uint32_t srcCharMax;
	uint32_t dstCharMax;
	uint32_t strContinue:1;
	uint32_t strFrame:1;
	uint32_t reserved:6;
	uint32_t escFilter:8;
	uint32_t errOperation:16;
	SceCesUcsSetting UcsSetting;
	SceCesState State;
	void *ConvertProfile;
	void *ConvertCallbacks;
	const SceCesOtherCesFuncs *OtherCesFuncs;
	const CesUcsCallbacks *UcsCallbacks;
	uint32_t Reserve[9];
} SceCesUcsContext;

int sceCesUcsContextInit(SceCesUcsContext *context);

/* UTF-16 strings to UTF-8 */
int sceCesUtf16StrGetUtf8Len(SceCesUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			     uint32_t *utf16Len, uint32_t *utf8Len);
int sceCesUtf16StrToUtf8Str(SceCesUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			    uint32_t *utf16Len, uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Len);

/* UTF-8 strings to UTF-16 */
int sceCesUtf8StrGetUtf16Len(SceCesUcsContext *ces, const uint8_t *utf8str, uint32_t utf8max,
			     uint32_t *utf8Size, uint32_t *utf16Len);
int sceCesUtf8StrToUtf16Str(SceCesUcsContext *ces, const uint8_t *utf8str, uint32_t utf8max,
			    uint32_t *utf8Size, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len);

#endif /* _CES_UCS_CONTEXT_H */
