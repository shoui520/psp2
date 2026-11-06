/*
 * libSceCes's JIS_X_0201_Katakana.c (Sony's internal/usermode/src/ces/libces/src/JIS): JIS X
 * 0201's katakana half as a single-byte character set, 0x21-0x5F the halfwidth katakana
 * U+FF61-U+FF9F.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "../SBCS/ces_sbcs_profile.h"

static uint32_t cesJisX0201Katakana_getUcs(const SceCesSbcs *sbcs, uint8_t sbc)
{
	if (sbc >= 0x21 && sbc <= 0x5f)
		return sbc + 0xff40;
	if (sbc == 0x20 || sbc == 0x7f)
		return sbc;
	if (sbc >= 0x80)
		return (uint32_t)-1;
	return 0;
}

static uint8_t cesJisX0201Katakana_getSbc(const SceCesSbcs *sbcs, uint32_t ucs)
{
	if (ucs >= 0xff61 && ucs <= 0xff9f)
		return (uint8_t)(ucs - 0xff40);
	if (ucs == 0x20 || ucs == 0x7f)
		return (uint8_t)ucs;
	return 0;
}

static const SceCesSbcs cesJisX0201Katakana = {
	cesJisX0201Katakana_getUcs,
	cesJisX0201Katakana_getSbc,
	0x20,
	0,
};

static const SceCesSbcsUcsProfile cesSbcsUcsProfile_JISX0201Katakana = {
	0x04490000,
	0,
	(_sceCesMbcsToUcs4Func *)sceCesSbcsToUcs4,
	(_sceCesUcs4ToMbcsFunc *)sceCesUcs4ToSbcs,
	&cesJisX0201Katakana,
};

const SceCesSbcsUcsProfile *sceCesRefersUcsProfileJisX0201Katakana(void)
{
	return &cesSbcsUcsProfile_JISX0201Katakana;
}
