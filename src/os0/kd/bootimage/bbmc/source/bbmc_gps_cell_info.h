#pragma once

#include <stddef.h>
#include <psp2/types.h>

typedef struct BbmcGpsCellInfo {
	SceUInt32 radioInterface;
	SceUInt8 cellPresent;
	SceUInt8 measurementPresent;
	SceUInt32 cellId;
	SceUInt32 timingAdvance;
	SceUInt16 lac;
	SceUInt16 mcc;
	SceUInt16 mnc;
	SceInt16 signalStrength;
	SceUInt16 primaryScramblingCode;
	/* The complete API result is cleared; these bytes are never populated. */
	SceUInt8 reserved[302];
} BbmcGpsCellInfo;

_Static_assert(sizeof(BbmcGpsCellInfo) == 328, "GPS cell information size");
_Static_assert(__alignof__(BbmcGpsCellInfo) == 4, "GPS cell information alignment");
_Static_assert(offsetof(BbmcGpsCellInfo, radioInterface) == 0, "radio interface");
_Static_assert(offsetof(BbmcGpsCellInfo, cellPresent) == 4, "cell availability");
_Static_assert(offsetof(BbmcGpsCellInfo, measurementPresent) == 5, "measurement availability");
_Static_assert(offsetof(BbmcGpsCellInfo, cellId) == 8, "cell identifier");
_Static_assert(offsetof(BbmcGpsCellInfo, timingAdvance) == 12, "timing advance");
_Static_assert(offsetof(BbmcGpsCellInfo, lac) == 16, "location area code");
_Static_assert(offsetof(BbmcGpsCellInfo, mcc) == 18, "mobile country code");
_Static_assert(offsetof(BbmcGpsCellInfo, mnc) == 20, "mobile network code");
_Static_assert(offsetof(BbmcGpsCellInfo, signalStrength) == 22, "signal strength");
_Static_assert(offsetof(BbmcGpsCellInfo, primaryScramblingCode) == 24, "scrambling code");
_Static_assert(offsetof(BbmcGpsCellInfo, reserved) == 26, "unpopulated result bytes");
