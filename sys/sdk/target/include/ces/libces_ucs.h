/*
 * ces/libces_ucs.h: the character encoding library's Unicode checks and conversions, under Sony's
 * names (SDK Release 03.570.011, target/include/ces/libces_ucs.h). The SDK's static library
 * libSceCes links them into the modules that call them; only what the tree builds is declared.
 */
#ifndef _SCE_CES_LIBCES_UCS_H
#define _SCE_CES_LIBCES_UCS_H

#include <stdint.h>
#include <ces/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_CES_UTF32_CODE_OCTET	(4)
#define SCE_CES_UTF16_CODE_OCTET	(2)
#define SCE_CES_UTF8_CODE_OCTET		(1)
#define SCE_CES_UCS2_CODE_OCTET		(2)

/* one character code check */
int sceCesUcs2CheckCode(uint16_t ucs2);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_CES_LIBCES_UCS_H */
