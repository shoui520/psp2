/*
 * libSceCes's gb18030_FromUcs.c (Sony's internal/usermode/src/ces/libces/src/GB): a UCS-4 code
 * point's GB18030 code: the euro sign, else GBK's two bytes, else the profile's four bytes, else
 * the private-use mappings.
 */
#include <stddef.h>
#include <stdint.h>
#include "ces_gb_profile.h"

uint32_t _sceCesGB18030GetCodeFromUcs4(const SceCesGbUcsProfile *prof, uint32_t ucs4)
{
	uint32_t gbCode;

	if (ucs4 == CES_UCS_EURO_SIGN)
		return 0xa2e3;
	gbCode = _sceCesGBKGetCodeFromUcs4(prof, ucs4);
	if (gbCode == 0) {
		if (prof->gb4byte_from_ucs4 != NULL)
			gbCode = prof->gb4byte_from_ucs4(prof, ucs4);
		if (gbCode == 0)
			gbCode = _sceCesGBCode_FromUcsPrivate(prof, ucs4);
	}
	return gbCode;
}
