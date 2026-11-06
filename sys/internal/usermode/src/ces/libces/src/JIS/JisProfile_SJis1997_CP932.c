/*
 * libSceCes's JisProfile_SJis1997_CP932.c (Sony's internal/usermode/src/ces/libces/src/JIS):
 * the profile of Microsoft's code page 932, Shift_JIS over JIS X 0208:1997 with NEC's row 13,
 * the NEC-selected and IBM extensions and Microsoft's mappings.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_jis_profile.h"

SceCesSJisUcsProfile *sceCesUcsProfileInitSJis1997Cp932(SceCesJisUcsProfile *prof)
{
	prof->cef_type = 0x4000;
	/* JIS X 0208, Microsoft's mappings, NEC's row 13 and the extensions (bit 2 unread here) */
	prof->types = 0x20107;
	prof->mbcs_to_ucs4 = (_sceCesMbcsToUcs4Func *)sceCesSJisToUcs4;
	prof->ucs4_to_mbcs = (_sceCesUcs4ToMbcsFunc *)sceCesUcs4ToSJis;
	prof->Roma = sceCesRefersUcsProfileAscii();
	prof->row_1_8_to_ucs4 = _sceCesSJisRow_1_8_ToUcs4;
	prof->row_9_12_to_ucs4 = NULL;
	prof->row_13_to_ucs4 = _sceCesSJisNECRow_13_ToUcs4;
	prof->row_14_15_to_ucs4 = NULL;
	prof->row_16_84_to_ucs4 = _sceCesSJisRow_16_84_ToUcs4;
	prof->row_85_94_to_ucs4 = _sceCesSJisRow_85_94_ToUcs4;
	prof->row_95_120_to_ucs4 = _sceCesSJisRow_95_120_ToUcs4;
	prof->jisExBits_from_ucs4 = _sceCesJisMenKuTenNECExFromUcs;
	prof->jisExBits_from_ucsCJKExB = NULL;
	prof->Kana = sceCesRefersUcsProfileJisX0201Katakana();
	prof->jisX0212_to_ucs4 = NULL;
	prof->jisX0212_from_ucs4 = NULL;
	return prof;
}
