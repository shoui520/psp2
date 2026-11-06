/*
 * ces/libces_jis.h: the character encoding library's JIS character sets, under Sony's names (SDK
 * Release 03.570.011, target/include/ces/libces_jis.h). Only what the tree builds is declared.
 */
#ifndef _SCE_CES_LIBCES_JIS_H
#define _SCE_CES_LIBCES_JIS_H

#include <stdint.h>
#include <ces/libces_mbcs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== */
/* JISCS <=> UCS */
/* ===================== */
typedef union SceCesJiscsUcsProfile {

	SceCesUcsProfileSheet ucsProfileSheet;

} SceCesJiscsUcsProfile;

#define sceCesGetJiscsUcsProfile(prof) (&(prof)->cesJiscsUcsPrf)

/* ===================== */
/* Shift_JIS <=> UCS */
/* ===================== */
typedef union SceCesSJisUcsProfile {

	SceCesUcsProfileSheet  ucsProfileSheet;
	SceCesMbcsUcsProfile   cesMbcsUcsPrf;
	SceCesJiscsUcsProfile  cesJiscsUcsPrf;

} SceCesSJisUcsProfile;

SceCesSJisUcsProfile *sceCesUcsProfileInitSJis1997Cp932(SceCesUcsProfileSheet *sheet);
#define sceCesUcsProfileInitSJis  sceCesUcsProfileInitSJis1997Cp932

#ifdef __cplusplus
}
#endif

#endif /* _SCE_CES_LIBCES_JIS_H */
