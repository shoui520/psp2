/*
 * SceScreenShot's session with the screenshot service (an IPMI service of SceShell) and what the
 * library reads out of a screenshot file. No debug record names the types; the names are ours.
 */
#ifndef LIBSCESCREENSHOT_SCREENSHOT_INTERNAL_H
#define LIBSCESCREENSHOT_SCREENSHOT_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include "../../../../../lowlayer/src/ipmi/client.h"

/* the service's methods */
#define SCREENSHOT_METHOD_SET_PARAM			0x34560000
#define SCREENSHOT_METHOD_SEND_OVERLAY_DATA		0x34560001
#define SCREENSHOT_METHOD_SET_OVERLAY_OFFSET		0x34560002
#define SCREENSHOT_METHOD_ENABLE			0x34560003
#define SCREENSHOT_METHOD_DISABLE			0x34560004
#define SCREENSHOT_METHOD_ENABLE_NOTIFICATION		0x34560007
#define SCREENSHOT_METHOD_DISABLE_NOTIFICATION		0x34560008
#define SCREENSHOT_METHOD_START_CAPTURE			0x34560009
#define SCREENSHOT_METHOD_GET_CAPTURE_RESULT		0x3456000A
#define SCREENSHOT_METHOD_CANCEL_CAPTURE		0x3456000B

#define SCREENSHOT_HEAP_MEMBLOCK_TYPE			0x0C20D060

/* the byte order of the EXIF block's TIFF header */
#define SCREENSHOT_TIFF_BIG_ENDIAN			1
#define SCREENSHOT_TIFF_LITTLE_ENDIAN			2

/* the frames and texts of the maker note */
#define SCREENSHOT_FRAME_TXXX				0x54585858
#define SCREENSHOT_TEXT_ALBM				0x414C424D
#define SCREENSHOT_TEXT_GDSC				0x47445343

/* one connection to the screenshot service */
typedef struct ScreenShotSession {
	IPMI::Client *pClient;
	SceInt32 requestId;
	void *mspace;
	SceUID memblockId;
	void *pClientBuffer;
} ScreenShotSession;

typedef struct ScreenShotNotification {
	SceUInt32 type;
	char text[60];
} ScreenShotNotification;

/* an entry of an IFD, as the reader keeps it */
typedef struct ScreenShotIfdEntry {
	SceUInt16 tag;
	SceUInt16 type;
	SceUInt32 count;
	SceUInt32 value;
	SceUInt32 size;
} ScreenShotIfdEntry;

/* where the reader stands in a JPEG segment */
typedef struct ScreenShotMarkerState {
	SceInt64 start;
	SceUInt32 length;
	SceUInt32 ifdOffset;
} ScreenShotMarkerState;

static inline SceUInt32 screenShotSwap16(SceUInt32 value)
{
	return ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
}

static inline SceUInt32 screenShotSwap32(SceUInt32 value)
{
	return (((value >> 8) & 0xFF) << 16) | (value << 24) | (value >> 24) | (((value >> 16) & 0xFF) << 8);
}

#endif /* LIBSCESCREENSHOT_SCREENSHOT_INTERNAL_H */
