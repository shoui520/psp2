/*
 * libSceCes's MBCS profile (Sony's internal/usermode/src/ces/libces/src/MBCS/ces_mbcs_profile.h):
 * the head every multi-byte character set's profile starts with, its type and the two
 * conversions a caller of sceCesUcs4ToMbc and its kin goes through. Laid out and named from the
 * library's debug information (SDK 3.570.011 libSceCes.a). The SDK's public header
 * (ces/libces_mbcs.h) shows the same profile as an opaque sheet.
 */
#ifndef _CES_MBCS_PROFILE_H
#define _CES_MBCS_PROFILE_H

#include <stdint.h>
#include <ces/error.h>

struct SceCesMbcsUcsProfile;

typedef int _sceCesMbcsToUcs4Func(const struct SceCesMbcsUcsProfile *prof, const uint8_t *mbcsBuf,
				  uint32_t mbcsMax, uint32_t *mbcsSize, uint32_t *ucs4);
typedef int _sceCesUcs4ToMbcsFunc(uint32_t ucs4, const struct SceCesMbcsUcsProfile *prof,
				  uint8_t *mbcsBuf, uint32_t mbcsMax, uint32_t *mbcsSize);

typedef struct SceCesMbcsUcsProfile {
	int cef_type;
	int types;
	_sceCesMbcsToUcs4Func *mbcs_to_ucs4;
	_sceCesUcs4ToMbcsFunc *ucs4_to_mbcs;
} SceCesMbcsUcsProfile;

/*
 * UCS.c's sceCesUcs4CheckCode, as the library's conversions test their code in place: above
 * 0x7FFFFFFF a code is no UCS-4 encoding, a surrogate no character
 */
#define _SCE_CES_UCS4_CHECK_CODE(ucs4) \
	((ucs4) > 0x7fffffff ? SCE_CES_ERROR_INVALID_ENCODE : \
	 (ucs4) >= 0xd800 && (ucs4) <= 0xdfff ? SCE_CES_ERROR_ILLEGAL_CODE : SCE_CES_OK)

int sceCesMbcToUcs4(const SceCesMbcsUcsProfile *prof, const uint8_t *mbcsBuf, uint32_t mbcsMax,
		    uint32_t *mbcsSize, uint32_t *ucs4);
int sceCesMbcToUtf16(const SceCesMbcsUcsProfile *prof, const uint8_t *mbcsBuf, uint32_t mbcsMax,
		     uint32_t *mbcsSize, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len);
int sceCesUcs4ToMbc(uint32_t ucs4, const SceCesMbcsUcsProfile *prof, uint8_t *mbcsBuf,
		    uint32_t mbcsMax, uint32_t *mbcsSize);
int sceCesUtf32ToMbc(uint32_t utf32code, const SceCesMbcsUcsProfile *prof, uint8_t *mbcsBuf,
		     uint32_t mbcsMax, uint32_t *mbcsSize);

#endif /* _CES_MBCS_PROFILE_H */
