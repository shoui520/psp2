/*
 * libSceCes's MBCS context (Sony's internal/usermode/src/ces/libces/src/MBCS/ces_mbcs_context.h):
 * the string conversion context between a multi-byte character set and the Unicode forms. Laid
 * out and named from the library's debug information (SDK 3.570.011 libSceCes.a).
 */
#ifndef _CES_MBCS_CONTEXT_H
#define _CES_MBCS_CONTEXT_H

#include <stdint.h>
#include "../ces_context.h"
#include "../ces_callback.h"
#include "../UCS/ces_ucs_setting.h"
#include "../UCS/ces_ucs_context.h"
#include "../UCS/ces_ucs_callback.h"
#include "ces_mbcs_profile.h"
#include "ces_mbcs_callback.h"

typedef struct SceCesMbcsUcsContext {
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
	const SceCesMbcsUcsProfile *Profile;
	const SceCesMbcsUcsCallbacks *Callbacks;
	const SceCesOtherCesFuncs *OtherCesFuncs;
	CesUcsCallbacks *UcsCallbacks;
	uint32_t Reserve[9];
} SceCesMbcsUcsContext;

int _sceCesMbcsUcsContextInit(SceCesMbcsUcsContext *context, const SceCesMbcsUcsProfile *prof);
int sceCesMbcsUcsContextInit(SceCesMbcsUcsContext *context, const SceCesMbcsUcsProfile *prof);
int sceCesMbcsUcsSetMbcsReplacementCharUCode(SceCesMbcsUcsContext *context, uint32_t ucode);

/* UTF-16 strings to the character set and back */
int sceCesUtf16StrGetMbcsLen(SceCesMbcsUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			     uint32_t *utf16Len, uint32_t *mbcsLen);
int sceCesUtf16StrToMbcsStr(SceCesMbcsUcsContext *ces, const uint16_t *utf16str, uint32_t utf16max,
			    uint32_t *utf16Len, uint8_t *mbcsBuf, uint32_t mbcsMax, uint32_t *mbcsLen);
int sceCesMbcsStrGetUtf16Len(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr, uint32_t mbcsMax,
			     uint32_t *mbcsLen, uint32_t *utf16Len);
int sceCesMbcsStrToUtf16Str(SceCesMbcsUcsContext *ces, const uint8_t *mbcsStr, uint32_t mbcsMax,
			    uint32_t *mbcsLen, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len);

#endif /* _CES_MBCS_CONTEXT_H */
