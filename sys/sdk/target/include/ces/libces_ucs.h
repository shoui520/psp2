/*
 * ces/libces_ucs.h: the character encoding library's Unicode checks and conversions, under Sony's
 * names (SDK Release 03.570.011, target/include/ces/libces_ucs.h). The SDK's static library
 * libSceCes links them into the modules that call them; only what the tree builds is declared.
 */
#ifndef _SCE_CES_LIBCES_UCS_H
#define _SCE_CES_LIBCES_UCS_H

#include <stdint.h>
#include <ces/error.h>
#include <ces/libces_str.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_CES_UTF32_CODE_OCTET	(4)
#define SCE_CES_UTF16_CODE_OCTET	(2)
#define SCE_CES_UTF8_CODE_OCTET		(1)
#define SCE_CES_UCS2_CODE_OCTET		(2)

/* one character code check */
int sceCesUcs2CheckCode(uint16_t ucs2);
int sceCesUtf8CheckCode(const uint8_t *utf8addr, uint32_t utf8max, uint32_t *utf8Len);

/* character string conversion */
/* ================================================== */
typedef union SceCesUcsContext {

	SceCesContext cesStrCtx;
	SceCesContext cesUcsCtx;

} SceCesUcsContext;

int sceCesUcsContextInit(SceCesUcsContext *uctx);

#define sceCesGetUcsContext(ctx) ((SceCesUcsContext *)(void *)(&(ctx)->cesUcsCtx))

/* UTF-16 (string) */
int sceCesUtf16StrGetUtf8Len(SceCesUcsContext *uctx,
			     const uint16_t *utf16str, uint32_t utf16max, uint32_t *utf16Len,
			     uint32_t *utf8Len);
/* UTF-8 (string) */
int sceCesUtf8StrGetUtf16Len(SceCesUcsContext *uctx,
			     const uint8_t *utf8str, uint32_t utf8max, uint32_t *utf8Len,
			     uint32_t *utf16Len);

/* UTF-16 (string) */
int sceCesUtf16StrToUtf8Str(SceCesUcsContext *uctx,
			    const uint16_t *utf16str, uint32_t utf16max, uint32_t *utf16Len,
			    uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Len);
/* UTF-8 (string) */
int sceCesUtf8StrToUtf16Str(SceCesUcsContext *uctx,
			    const uint8_t *utf8str, uint32_t utf8max, uint32_t *utf8Len,
			    uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len);

/* ===================== */
/* for other CESs */
/* ===================== */
typedef union SceCesUcsProfileSheet {

	void *systemUse[32];

} SceCesUcsProfileSheet;

#ifdef __cplusplus
}
#endif

#endif /* _SCE_CES_LIBCES_UCS_H */
