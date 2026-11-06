/*
 * libSceCes's Mbcs_FromUTF32.c (Sony's internal/usermode/src/ces/libces/src/MBCS): one UTF-32
 * code to a multi-byte character set's code, through the profile.
 */
#include <stddef.h>
#include <stdint.h>
#include <ces/error.h>
#include "ces_mbcs_profile.h"

int sceCesUtf32ToMbc(uint32_t utf32code, const SceCesMbcsUcsProfile *prof, uint8_t *mbcsBuf,
		     uint32_t mbcsMax, uint32_t *mbcsSize)
{
	if (utf32code <= 0x10ffff)
		return sceCesUcs4ToMbc(utf32code, prof, mbcsBuf, mbcsMax, mbcsSize);
	if (mbcsSize != NULL)
		*mbcsSize = 0;
	return SCE_CES_ERROR_INVALID_ENCODE;
}
