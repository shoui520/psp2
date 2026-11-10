/*
 * libSceCes's MBCS error callbacks (Sony's internal/usermode/src/ces/libces/src/MBCS/ces_mbcs_callback.h):
 * for every conversion between a multi-byte character set and the Unicode forms, the routine
 * each kind of error calls, and the set a context binds. Laid out and named from the library's
 * debug information (SDK 3.570.011 libSceCes.a).
 */
#ifndef _CES_MBCS_CALLBACK_H
#define _CES_MBCS_CALLBACK_H

#include <stdint.h>
#include "../ces_callback.h"

typedef int SceCesMbcsToUcsErrorCallback(const uint8_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesMbcsToUcsErrorCallback *InvalidEncode;
	SceCesMbcsToUcsErrorCallback *IllegalCode;
	SceCesMbcsToUcsErrorCallback *OutOfCodeRange;
	SceCesMbcsToUcsErrorCallback *UnassignedCode;
} SceCesMbcsToUcsErrorCallbacks;

typedef int SceCesUcsToMbcsErrorCallback(uint32_t ucs4, uint8_t *mbcsBuf, uint32_t mbcsMax,
				uint32_t *mbcsLen, SceCesState *cbState);
typedef struct {
	SceCesUcsToMbcsErrorCallback *InvalidEncode;
	SceCesUcsToMbcsErrorCallback *IllegalCode;
	SceCesUcsToMbcsErrorCallback *OutOfCodeRange;
	SceCesUcsToMbcsErrorCallback *UnassignedCode;
} SceCesUcsToMbcsErrorCallbacks;

typedef int SceCesMbcsToUcs4ErrorCallback(const uint8_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesMbcsToUcs4ErrorCallback *InvalidEncode;
	SceCesMbcsToUcs4ErrorCallback *UnassignedCode;
} SceCesMbcsToUcs4ErrorCallbacks;

typedef int SceCesMbcsToUcs2ErrorCallback(const uint8_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesMbcsToUcs2ErrorCallback *InvalidEncode;
	SceCesMbcsToUcs2ErrorCallback *UnassignedCode;
	SceCesMbcsToUcs2ErrorCallback *OutOfCodeRange;
} SceCesMbcsToUcs2ErrorCallbacks;

typedef int SceCesMbcsToUtf8ErrorCallback(const uint8_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesMbcsToUtf8ErrorCallback *InvalidEncode;
	SceCesMbcsToUtf8ErrorCallback *UnassignedCode;
} SceCesMbcsToUtf8ErrorCallbacks;

typedef int SceCesMbcsToUtf16ErrorCallback(const uint8_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint16_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesMbcsToUtf16ErrorCallback *InvalidEncode;
	SceCesMbcsToUtf16ErrorCallback *UnassignedCode;
} SceCesMbcsToUtf16ErrorCallbacks;

typedef int SceCesMbcsToUtf32ErrorCallback(const uint8_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint32_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesMbcsToUtf32ErrorCallback *InvalidEncode;
	SceCesMbcsToUtf32ErrorCallback *UnassignedCode;
} SceCesMbcsToUtf32ErrorCallbacks;

typedef int SceCesUcs4ToMbcsErrorCallback(const uint32_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesUcs4ToMbcsErrorCallback *InvalidEncode;
	SceCesUcs4ToMbcsErrorCallback *IllegalCode;
	SceCesUcs4ToMbcsErrorCallback *OutOfCodeRange;
	SceCesUcs4ToMbcsErrorCallback *UnassignedCode;
} SceCesUcs4ToMbcsErrorCallbacks;

typedef int SceCesUcs2ToMbcsErrorCallback(const uint16_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesUcs2ToMbcsErrorCallback *InvalidEncode;
	SceCesUcs2ToMbcsErrorCallback *IllegalCode;
	SceCesUcs2ToMbcsErrorCallback *OutOfCodeRange;
	SceCesUcs2ToMbcsErrorCallback *UnassignedCode;
} SceCesUcs2ToMbcsErrorCallbacks;

typedef int SceCesUtf8ToMbcsErrorCallback(const uint8_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesUtf8ToMbcsErrorCallback *InvalidEncode;
	SceCesUtf8ToMbcsErrorCallback *IllegalCode;
	SceCesUtf8ToMbcsErrorCallback *OutOfCodeRange;
	SceCesUtf8ToMbcsErrorCallback *UnassignedCode;
} SceCesUtf8ToMbcsErrorCallbacks;

typedef int SceCesUtf16ToMbcsErrorCallback(const uint16_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesUtf16ToMbcsErrorCallback *InvalidEncode;
	SceCesUtf16ToMbcsErrorCallback *IllegalCode;
	SceCesUtf16ToMbcsErrorCallback *OutOfCodeRange;
	SceCesUtf16ToMbcsErrorCallback *UnassignedCode;
} SceCesUtf16ToMbcsErrorCallbacks;

typedef int SceCesUtf32ToMbcsErrorCallback(const uint32_t *src, uint32_t srcMax, uint32_t *srcLen,
				uint8_t *buf, uint32_t bufMax, uint32_t *bufLen,
				SceCesState *cbState);
typedef struct {
	SceCesUtf32ToMbcsErrorCallback *InvalidEncode;
	SceCesUtf32ToMbcsErrorCallback *IllegalCode;
	SceCesUtf32ToMbcsErrorCallback *OutOfCodeRange;
	SceCesUtf32ToMbcsErrorCallback *UnassignedCode;
} SceCesUtf32ToMbcsErrorCallbacks;

typedef struct SceCesMbcsUcsCallbacks {
	SceCesMbcsToUcsErrorCallbacks MbcsToUcs;
	SceCesUcsToMbcsErrorCallbacks UcsToMbcs;
	SceCesMbcsToUcs4ErrorCallbacks MbcsToUcs4;
	SceCesMbcsToUcs2ErrorCallbacks MbcsToUcs2;
	SceCesMbcsToUtf8ErrorCallbacks MbcsToUtf8;
	SceCesMbcsToUtf16ErrorCallbacks MbcsToUtf16;
	SceCesMbcsToUtf32ErrorCallbacks MbcsToUtf32;
	SceCesUcs4ToMbcsErrorCallbacks Ucs4ToMbcs;
	SceCesUcs2ToMbcsErrorCallbacks Ucs2ToMbcs;
	SceCesUtf8ToMbcsErrorCallbacks Utf8ToMbcs;
	SceCesUtf16ToMbcsErrorCallbacks Utf16ToMbcs;
	SceCesUtf32ToMbcsErrorCallbacks Utf32ToMbcs;
	void *reserved[21];
} SceCesMbcsUcsCallbacks;

#endif /* _CES_MBCS_CALLBACK_H */
