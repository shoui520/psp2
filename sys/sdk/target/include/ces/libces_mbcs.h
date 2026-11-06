/*
 * ces/libces_mbcs.h: the character encoding library's multi-byte character sets, under Sony's
 * names (SDK Release 03.570.011, target/include/ces/libces_mbcs.h). Only what the tree builds is
 * declared.
 */
#ifndef _SCE_CES_LIBCES_MBCS_H
#define _SCE_CES_LIBCES_MBCS_H

#include <stdint.h>
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

#ifdef __cplusplus
}
#endif

#endif /* _SCE_CES_LIBCES_MBCS_H */
