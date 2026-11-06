/*
 * ces/libces_mbcs.h: the character encoding library's multi-byte character sets, under Sony's
 * names (SDK Release 03.570.011, target/include/ces/libces_mbcs.h). Only what the tree builds is
 * declared.
 */
#ifndef _SCE_CES_LIBCES_MBCS_H
#define _SCE_CES_LIBCES_MBCS_H

#include <stdint.h>
#include <ces/libces_str.h>
#include <ces/libces_ucs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union SceCesMbcsUcsProfile {

	SceCesUcsProfileSheet ucsProfileSheet;

} SceCesMbcsUcsProfile;

#define sceCesGetMbcsUcsProfile(prof) (&(prof)->cesMbcsUcsPrf)

/* UCS -> MBC */
int sceCesUtf32ToMbc(uint32_t utf32code,
		     const SceCesMbcsUcsProfile *p, uint8_t *mbcBuf, uint32_t mbcMax, uint32_t *mbcLen);

/* character string conversion (MBCS <=> UCS) */
/* ================================================== */
typedef union SceCesMbcsUcsContext {

	SceCesContext    cesMbcsUcsCtx;
	SceCesUcsContext cesUcsCtx;
	SceCesContext    cesStrCtx;

} SceCesMbcsUcsContext;

int sceCesMbcsUcsContextInit(SceCesMbcsUcsContext *context, const SceCesMbcsUcsProfile *mbcsUcsPrf);

/* Gets UCS string length (from MBCS) */
int sceCesMbcsStrGetUtf16Len(SceCesMbcsUcsContext *context,
			     const uint8_t *mbcsStr, uint32_t mbcsMax, uint32_t *mbcsLen,
			     uint32_t *utf16Len);
/* Gets MBCS string length (from UCS) */
int sceCesUtf16StrGetMbcsLen(SceCesMbcsUcsContext *context,
			     const uint16_t *utf16str, uint32_t utf16max, uint32_t *utf16Len,
			     uint32_t *mbcsLen);

/* MBCS -> UCS (string) */
int sceCesMbcsStrToUtf16Str(SceCesMbcsUcsContext *context,
			    const uint8_t *mbcsStr, uint32_t mbcsMax, uint32_t *mbcsLen,
			    uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len);
/* UCS -> MBCS (string) */
int sceCesUtf16StrToMbcsStr(SceCesMbcsUcsContext *context,
			    const uint16_t *utf16str, uint32_t utf16max, uint32_t *utf16Len,
			    uint8_t *mbcsBuf, uint32_t mbcsMax, uint32_t *mbcsLen);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_CES_LIBCES_MBCS_H */
