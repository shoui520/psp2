/*
 * libSceCes's UCS routines (Sony's internal/usermode/src/ces/libces/src/UCS/ces_ucs.h): the one
 * character conversions between the Unicode forms the string conversions call, and the types of
 * the ones they pick by byte order. Named from the library's debug information (SDK 3.570.011
 * libSceCes.a); the SDK's public header (ces/libces_ucs.h) declares a part of them.
 */
#ifndef _CES_UCS_H
#define _CES_UCS_H

#include <stdint.h>
#include <ces/error.h>

typedef int CesUtf16ToUcs4Func(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
			       uint32_t *ucs4);
typedef int CesUtf16ToUcsUtf8Func(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
				  uint32_t *ucsCode, uint8_t *utf8buf, uint32_t utf8max,
				  uint32_t *utf8Size);

/* UTF-8 */
int sceCesUtf8ToUcs4(const uint8_t *utf8buf, uint32_t utf8max, uint32_t *convSize, uint32_t *ucs4);
int sceCesUcs4ToUtf8(uint32_t ucs4, uint8_t *utf8buf, uint32_t bufSize, uint32_t *utf8Size);

/* UTF-16 */
int sceCesUtf16ToUcs4(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16size,
		      uint32_t *ucs4);
int sceCesUcs4ToUtf16(uint32_t ucs4, uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Count);
int sceCesUtf16beToUcs4(const uint16_t *utf16be, uint32_t utf16max, uint32_t *utf16size,
			uint32_t *ucs4);
int sceCesUtf16leToUcs4(const uint16_t *utf16le, uint32_t utf16max, uint32_t *utf16size,
			uint32_t *ucs4);

/* UTF-16 and UTF-8 together */
int sceCesUtf16ToUcsUtf8(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
			 uint32_t *ucsCode, uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Size);
int sceCesUtf16beToUcsUtf8(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Size,
			   uint32_t *ucsCode, uint8_t *utf8buf, uint32_t utf8max,
			   uint32_t *utf8Size);
int sceCesUtf8ToUcsUtf16(const uint8_t *utf8buf, uint32_t utf8max, uint32_t *utf8Size,
			 uint32_t *ucsCode, uint16_t *utf16buf, uint32_t utf16max,
			 uint32_t *utf16Count);

#endif /* _CES_UCS_H */
