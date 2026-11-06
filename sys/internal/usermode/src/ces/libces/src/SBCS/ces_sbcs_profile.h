/*
 * libSceCes's single-byte character sets (Sony's
 * internal/usermode/src/ces/libces/src/SBCS/ces_sbcs_profile.h): a set maps its codes from
 * baseCode on through its own functions or a table of UCS code points; below baseCode a code is
 * its own UCS code point. Laid out and named from the library's debug information (SDK
 * 3.570.011 libSceCes.a).
 */
#ifndef _CES_SBCS_PROFILE_H
#define _CES_SBCS_PROFILE_H

#include <stdint.h>
#include "../MBCS/ces_mbcs_profile.h"

struct SceCesSbcs;

/* a code's UCS code point; 0 for none, -1 for no code of the set */
typedef uint32_t cesSBCGetUcs4Func(const struct SceCesSbcs *sbcs, uint8_t sbc);
/* a UCS code point's code; 0 for none */
typedef uint8_t cesUcs4GetSBCFunc(const struct SceCesSbcs *sbcs, uint32_t ucs);

typedef struct SceCesSbcs {
	cesSBCGetUcs4Func *getUCS;
	cesUcs4GetSBCFunc *getSBC;
	uint16_t baseCode;
	uint16_t tableCount;		/* without getUCS: Table's codes from baseCode on */
	uint16_t Table[];
} SceCesSbcs, SceCesSbcsUcs;

/* an SBCS profile is an MBCS profile with its set */
typedef struct SceCesSbcsUcsProfile {
	uint32_t csType;
	uint32_t types;
	_sceCesMbcsToUcs4Func *mbcs_to_ucs4;
	_sceCesUcs4ToMbcsFunc *ucs4_to_mbcs;
	const SceCesSbcs *sbcs;
} SceCesSbcsUcsProfile;

int sceCesSbcsToUcs4(const SceCesSbcsUcsProfile *prof, const uint8_t *sbcsBuf, uint32_t sbcsMax,
		     uint32_t *sbcsSize, uint32_t *ucs4);
int sceCesUcs4ToSbcs(uint32_t ucs4, const SceCesSbcsUcsProfile *prof, uint8_t *sbcsBuf,
		     uint32_t sbcsMax, uint32_t *sbcsSize);

int _sceCesSbcToUcs4(const SceCesSbcsUcsProfile *prof, uint8_t sbc, uint32_t *ucs4);
int sceCesSbcToUcs4(const SceCesSbcsUcsProfile *prof, uint8_t sbc, uint32_t *ucs4);
int _sceCesUcs4ToSbc(uint32_t ucs4, const SceCesSbcsUcsProfile *prof, uint8_t *sbc);
int sceCesUcs4ToSbc(uint32_t ucs4, const SceCesSbcsUcsProfile *prof, uint8_t *sbc);

const SceCesSbcsUcsProfile *sceCesRefersUcsProfileAscii(void);
const SceCesSbcsUcsProfile *sceCesRefersUcsProfileJisX0201Katakana(void);

#endif /* _CES_SBCS_PROFILE_H */
