/*
 * libSceCes's jis1990_Ext_FromUcs.c (Sony's internal/usermode/src/ces/libces/src/JIS): UCS-4 to
 * the NEC-selected IBM extensions, rows 89-92.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"

uint32_t _sceCesJisMenKuTenNECExFromUcs(const SceCesJisUcsProfile *prof, uint32_t ucs4)
{
	uint16_t jisMenKuten;
	int i;

	i = _sceCesSJisIBMIndexFromUcs(ucs4);
	if (i < 0)
		return 0;
	/* IBM's first 28 codes are NEC's cells 362-371 and 373-375 (row 92), the rest from cell 0 */
	if (i >= 28)
		i -= 28;
	else if (i < 10)
		i += 362;
	else if (i <= 20 || i >= 24)
		return 0;
	else
		i += 352;
	jisMenKuten = (1 << 14) + ((i / 94 + 89) << 7) + i % 94 + 1;
	return jisMenKuten;
}
