/*
 * libSceCes's gbk_FromUcs.c (Sony's internal/usermode/src/ces/libces/src/GB): a UCS-4 code
 * point's two-byte GBK code, from the areas that hold its block.
 */
#include <stdint.h>
#include "ces_gb_profile.h"

uint32_t _sceCesGBKGetCodeFromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4)
{
	uint32_t gbCode = 0;

	if (ucs4 < 0x4e00) {
		/* symbols */
		gbCode = _sceCesGBGet2byteSect1Code_FromUcs4(prof, ucs4);
		if (gbCode == 0)
			gbCode = _sceCesGBGet2byteSect5Code_FromUcs4(prof, ucs4);
		if (gbCode == 0)
			gbCode = _sceCesGBGet2byteSect4Code_FromUcs4(prof, ucs4);
	} else if (ucs4 <= 0x9fff) {
		/* the CJK unified ideographs: GB2312's first, then area 3 to U+72DB, area 4 on */
		gbCode = _sceCesGBGet2byteSect2Code_FromUcs4(prof, ucs4);
		if (gbCode == 0) {
			if (ucs4 < 0x72dc)
				gbCode = _sceCesGBGet2byteSect3Code_FromUcs4(prof, ucs4);
			else
				gbCode = _sceCesGBGet2byteSect4Code_FromUcs4(prof, ucs4);
		}
	} else if (ucs4 >= 0xf900 && ucs4 < 0xffff) {
		/* compatibility ideographs and forms */
		gbCode = _sceCesGBGet2byteSect1Code_FromUcs4(prof, ucs4);
		if (gbCode == 0)
			gbCode = _sceCesGBGet2byteSect5Code_FromUcs4(prof, ucs4);
		if (gbCode == 0)
			gbCode = _sceCesGBGet2byteSect4Code_FromUcs4(prof, ucs4);
	}
	return gbCode;
}
