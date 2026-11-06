/*
 * libSceCes's ASCII.c (Sony's internal/usermode/src/ces/libces/src/SBCS/ISO_646): ASCII as a
 * single-byte character set, the codes below 0x80 their own UCS code points.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "../ces_sbcs_profile.h"

static uint32_t cesAscii_getUcs(const SceCesSbcs *iso646, uint8_t sbc)
{
	if (sbc <= 0x7f)
		return sbc;
	return (uint32_t)-1;
}

static uint8_t cesAscii_getSbc(const SceCesSbcs *iso646, uint32_t ucs)
{
	if (ucs <= 0x7f)
		return (uint8_t)ucs;
	return 0;
}

static const SceCesSbcs cesAscii = {
	cesAscii_getUcs,
	cesAscii_getSbc,
	0x20,
	0,
};

static const SceCesSbcsUcsProfile cesSbcsUcsProfile_ASCII = {
	0x04420000,
	0,
	(_sceCesMbcsToUcs4Func *)sceCesSbcsToUcs4,
	(_sceCesUcs4ToMbcsFunc *)sceCesUcs4ToSbcs,
	&cesAscii,
};

const SceCesSbcsUcsProfile *sceCesRefersUcsProfileAscii(void)
{
	return &cesSbcsUcsProfile_ASCII;
}
