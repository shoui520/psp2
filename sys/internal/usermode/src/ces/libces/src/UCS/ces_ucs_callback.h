/*
 * libSceCes's UCS error callbacks (Sony's internal/usermode/src/ces/libces/src/UCS/ces_ucs_callback.h):
 * for every conversion between the Unicode forms, the routine each kind of error calls, and the
 * set a context binds. Laid out and named from the library's debug information (SDK 3.570.011
 * libSceCes.a).
 */
#ifndef _CES_UCS_CALLBACK_H
#define _CES_UCS_CALLBACK_H

#include <stdint.h>
#include "../ces_callback.h"

typedef int SceCesUcs4ToCopyErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs4ToCopyErrorCallback *InvalidEncode;
	SceCesUcs4ToCopyErrorCallback *IllegalCode;
} SceCesUcs4ToCopyErrorCallbacks;

typedef int SceCesUcs4ToUcs2ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs4ToUcs2ErrorCallback *InvalidEncode;
	SceCesUcs4ToUcs2ErrorCallback *IllegalCode;
	SceCesUcs4ToUcs2ErrorCallback *OutOfCodeRange;
} SceCesUcs4ToUcs2ErrorCallbacks;

typedef int SceCesUcs4ToUtf8ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs4ToUtf8ErrorCallback *InvalidEncode;
	SceCesUcs4ToUtf8ErrorCallback *IllegalCode;
} SceCesUcs4ToUtf8ErrorCallbacks;

typedef int SceCesUcs4ToUtf16ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs4ToUtf16ErrorCallback *InvalidEncode;
	SceCesUcs4ToUtf16ErrorCallback *IllegalCode;
	SceCesUcs4ToUtf16ErrorCallback *OutOfCodeRange;
} SceCesUcs4ToUtf16ErrorCallbacks;

typedef int SceCesUcs4ToUtf32ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs4ToUtf32ErrorCallback *InvalidEncode;
	SceCesUcs4ToUtf32ErrorCallback *IllegalCode;
	SceCesUcs4ToUtf32ErrorCallback *OutOfCodeRange;
} SceCesUcs4ToUtf32ErrorCallbacks;

typedef int SceCesUcs2ToUcs4ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs2ToUcs4ErrorCallback *IllegalCode;
} SceCesUcs2ToUcs4ErrorCallbacks;

typedef int SceCesUcs2ToCopyErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs2ToCopyErrorCallback *IllegalCode;
} SceCesUcs2ToCopyErrorCallbacks;

typedef int SceCesUcs2ToUtf8ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs2ToUtf8ErrorCallback *IllegalCode;
} SceCesUcs2ToUtf8ErrorCallbacks;

typedef int SceCesUcs2ToUtf16ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs2ToUtf16ErrorCallback *IllegalCode;
} SceCesUcs2ToUtf16ErrorCallbacks;

typedef int SceCesUcs2ToUtf32ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs2ToUtf32ErrorCallback *IllegalCode;
} SceCesUcs2ToUtf32ErrorCallbacks;

typedef int SceCesUtf8ToUcs4ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf8ToUcs4ErrorCallback *InvalidEncode;
	SceCesUtf8ToUcs4ErrorCallback *IllegalCode;
} SceCesUtf8ToUcs4ErrorCallbacks;

typedef int SceCesUtf8ToUcs2ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf8ToUcs2ErrorCallback *InvalidEncode;
	SceCesUtf8ToUcs2ErrorCallback *IllegalCode;
	SceCesUtf8ToUcs2ErrorCallback *OutOfCodeRange;
} SceCesUtf8ToUcs2ErrorCallbacks;

typedef int SceCesUtf8ToCopyErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf8ToCopyErrorCallback *InvalidEncode;
	SceCesUtf8ToCopyErrorCallback *IllegalCode;
} SceCesUtf8ToCopyErrorCallbacks;

typedef int SceCesUtf8ToUtf16ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf8ToUtf16ErrorCallback *InvalidEncode;
	SceCesUtf8ToUtf16ErrorCallback *IllegalCode;
	SceCesUtf8ToUtf16ErrorCallback *OutOfCodeRange;
} SceCesUtf8ToUtf16ErrorCallbacks;

typedef int SceCesUtf8ToUtf32ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf8ToUtf32ErrorCallback *InvalidEncode;
	SceCesUtf8ToUtf32ErrorCallback *IllegalCode;
	SceCesUtf8ToUtf32ErrorCallback *OutOfCodeRange;
} SceCesUtf8ToUtf32ErrorCallbacks;

typedef int SceCesUtf16ToUcs4ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf16ToUcs4ErrorCallback *IllegalCode;
} SceCesUtf16ToUcs4ErrorCallbacks;

typedef int SceCesUtf16ToUcs2ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf16ToUcs2ErrorCallback *IllegalCode;
	SceCesUtf16ToUcs2ErrorCallback *OutOfCodeRange;
} SceCesUtf16ToUcs2ErrorCallbacks;

typedef int SceCesUtf16ToUtf8ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf16ToUtf8ErrorCallback *IllegalCode;
} SceCesUtf16ToUtf8ErrorCallbacks;

typedef int SceCesUtf16ToCopyErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf16ToCopyErrorCallback *IllegalCode;
} SceCesUtf16ToCopyErrorCallbacks;

typedef int SceCesUtf16ToUtf32ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf16ToUtf32ErrorCallback *IllegalCode;
} SceCesUtf16ToUtf32ErrorCallbacks;

typedef int SceCesUtf32ToUcs4ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf32ToUcs4ErrorCallback *InvalidEncode;
	SceCesUtf32ToUcs4ErrorCallback *IllegalCode;
} SceCesUtf32ToUcs4ErrorCallbacks;

typedef int SceCesUtf32ToUcs2ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf32ToUcs2ErrorCallback *InvalidEncode;
	SceCesUtf32ToUcs2ErrorCallback *IllegalCode;
	SceCesUtf32ToUcs2ErrorCallback *OutOfCodeRange;
} SceCesUtf32ToUcs2ErrorCallbacks;

typedef int SceCesUtf32ToUtf8ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf32ToUtf8ErrorCallback *InvalidEncode;
	SceCesUtf32ToUtf8ErrorCallback *IllegalCode;
} SceCesUtf32ToUtf8ErrorCallbacks;

typedef int SceCesUtf32ToUtf16ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf32ToUtf16ErrorCallback *InvalidEncode;
	SceCesUtf32ToUtf16ErrorCallback *IllegalCode;
} SceCesUtf32ToUtf16ErrorCallbacks;

typedef int SceCesUtf32ToCopyErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf32ToCopyErrorCallback *InvalidEncode;
	SceCesUtf32ToCopyErrorCallback *IllegalCode;
} SceCesUtf32ToCopyErrorCallbacks;

typedef int SceCesUcs4ToUtf7ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs4ToUtf7ErrorCallback *InvalidEncode;
	SceCesUcs4ToUtf7ErrorCallback *IllegalCode;
} SceCesUcs4ToUtf7ErrorCallbacks;

typedef int SceCesUcs2ToUtf7ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUcs2ToUtf7ErrorCallback *IllegalCode;
} SceCesUcs2ToUtf7ErrorCallbacks;

typedef int SceCesUtf8ToUtf7ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf8ToUtf7ErrorCallback *InvalidEncode;
	SceCesUtf8ToUtf7ErrorCallback *IllegalCode;
} SceCesUtf8ToUtf7ErrorCallbacks;

typedef int SceCesUtf16ToUtf7ErrorCallback(const uint16_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf16ToUtf7ErrorCallback *IllegalCode;
} SceCesUtf16ToUtf7ErrorCallbacks;

typedef int SceCesUtf32ToUtf7ErrorCallback(const uint32_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf32ToUtf7ErrorCallback *InvalidEncode;
	SceCesUtf32ToUtf7ErrorCallback *IllegalCode;
} SceCesUtf32ToUtf7ErrorCallbacks;

typedef int SceCesUtf7ToUcs4ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf7ToUcs4ErrorCallback *InvalidEncode;
	SceCesUtf7ToUcs4ErrorCallback *IllegalCode;
} SceCesUtf7ToUcs4ErrorCallbacks;

typedef int SceCesUtf7ToUcs2ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf7ToUcs2ErrorCallback *InvalidEncode;
	SceCesUtf7ToUcs2ErrorCallback *IllegalCode;
	SceCesUtf7ToUcs2ErrorCallback *OutOfCodeRange;
} SceCesUtf7ToUcs2ErrorCallbacks;

typedef int SceCesUtf7ToUtf8ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf7ToUtf8ErrorCallback *InvalidEncode;
	SceCesUtf7ToUtf8ErrorCallback *IllegalCode;
} SceCesUtf7ToUtf8ErrorCallbacks;

typedef int SceCesUtf7ToUtf16ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf7ToUtf16ErrorCallback *InvalidEncode;
	SceCesUtf7ToUtf16ErrorCallback *IllegalCode;
	SceCesUtf7ToUtf16ErrorCallback *OutOfCodeRange;
} SceCesUtf7ToUtf16ErrorCallbacks;

typedef int SceCesUtf7ToUtf32ErrorCallback(const uint8_t *ucsAddr, uint32_t ucsMax, uint32_t *ucsLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen, SceCesState *cbState);
typedef struct {
	SceCesUtf7ToUtf32ErrorCallback *InvalidEncode;
	SceCesUtf7ToUtf32ErrorCallback *IllegalCode;
	SceCesUtf7ToUtf32ErrorCallback *OutOfCodeRange;
} SceCesUtf7ToUtf32ErrorCallbacks;

typedef struct CesUcsCallbacks {
	SceCesUcs4ToCopyErrorCallbacks Ucs4ToCopy;
	SceCesUcs4ToUcs2ErrorCallbacks Ucs4ToUcs2;
	SceCesUcs4ToUtf8ErrorCallbacks Ucs4ToUtf8;
	SceCesUcs4ToUtf16ErrorCallbacks Ucs4ToUtf16;
	SceCesUcs4ToUtf32ErrorCallbacks Ucs4ToUtf32;
	SceCesUcs2ToUcs4ErrorCallbacks Ucs2ToUcs4;
	SceCesUcs2ToCopyErrorCallbacks Ucs2ToCopy;
	SceCesUcs2ToUtf8ErrorCallbacks Ucs2ToUtf8;
	SceCesUcs2ToUtf16ErrorCallbacks Ucs2ToUtf16;
	SceCesUcs2ToUtf32ErrorCallbacks Ucs2ToUtf32;
	SceCesUtf8ToUcs4ErrorCallbacks Utf8ToUcs4;
	SceCesUtf8ToUcs2ErrorCallbacks Utf8ToUcs2;
	SceCesUtf8ToCopyErrorCallbacks Utf8ToCopy;
	SceCesUtf8ToUtf16ErrorCallbacks Utf8ToUtf16;
	SceCesUtf8ToUtf32ErrorCallbacks Utf8ToUtf32;
	SceCesUtf16ToUcs4ErrorCallbacks Utf16ToUcs4;
	SceCesUtf16ToUcs2ErrorCallbacks Utf16ToUcs2;
	SceCesUtf16ToUtf8ErrorCallbacks Utf16ToUtf8;
	SceCesUtf16ToCopyErrorCallbacks Utf16ToCopy;
	SceCesUtf16ToUtf32ErrorCallbacks Utf16ToUtf32;
	SceCesUtf32ToUcs4ErrorCallbacks Utf32ToUcs4;
	SceCesUtf32ToUcs2ErrorCallbacks Utf32ToUcs2;
	SceCesUtf32ToUtf8ErrorCallbacks Utf32ToUtf8;
	SceCesUtf32ToUtf16ErrorCallbacks Utf32ToUtf16;
	SceCesUtf32ToCopyErrorCallbacks Utf32ToCopy;
	SceCesUcs4ToUtf7ErrorCallbacks Ucs4ToUtf7;
	SceCesUcs2ToUtf7ErrorCallbacks Ucs2ToUtf7;
	SceCesUtf8ToUtf7ErrorCallbacks Utf8ToUtf7;
	SceCesUtf16ToUtf7ErrorCallbacks Utf16ToUtf7;
	SceCesUtf32ToUtf7ErrorCallbacks Utf32ToUtf7;
	SceCesUtf7ToUcs4ErrorCallbacks Utf7ToUcs4;
	SceCesUtf7ToUcs2ErrorCallbacks Utf7ToUcs2;
	SceCesUtf7ToUtf8ErrorCallbacks Utf7ToUtf8;
	SceCesUtf7ToUtf16ErrorCallbacks Utf7ToUtf16;
	SceCesUtf7ToUtf32ErrorCallbacks Utf7ToUtf32;
	void *reserved[11];
} CesUcsCallbacks;

#endif /* _CES_UCS_CALLBACK_H */
