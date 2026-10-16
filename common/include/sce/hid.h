/* HID reports shared by the report producer and its clients. */
#pragma once

#include <stddef.h>
#include <psp2/types.h>

typedef struct SceHidKeyboardReport {
	SceUInt8 error;
	SceUInt8 modifiers[2];
	SceUInt8 keycodes[13];
	SceUInt64 timestamp;
} SceHidKeyboardReport;

typedef struct SceHidMouseReport {
	SceUInt8 buttons;
	SceUInt8 reserved;
	SceInt16 rel_x;
	SceInt16 rel_y;
	SceInt8 wheel;
	SceInt8 tilt;
	SceUInt64 timestamp;
} SceHidMouseReport;

typedef struct SceHidConsumerControlReport {
	SceUInt16 usages[4];
	SceUInt64 timestamp;
} SceHidConsumerControlReport;

_Static_assert(sizeof(SceHidKeyboardReport) == 24, "SceHidKeyboardReport");
_Static_assert(_Alignof(SceHidKeyboardReport) == 8, "HID keyboard alignment");
_Static_assert(offsetof(SceHidKeyboardReport, error) == 0, "HID keyboard error");
_Static_assert(offsetof(SceHidKeyboardReport, modifiers) == 1,
	       "HID keyboard modifiers");
_Static_assert(offsetof(SceHidKeyboardReport, keycodes) == 3,
	       "HID keyboard keycodes");
_Static_assert(offsetof(SceHidKeyboardReport, timestamp) == 16,
	       "HID keyboard timestamp");
_Static_assert(sizeof(SceHidMouseReport) == 16, "SceHidMouseReport");
_Static_assert(offsetof(SceHidMouseReport, timestamp) == 8,
	       "HID mouse timestamp");
_Static_assert(sizeof(SceHidConsumerControlReport) == 16,
	       "SceHidConsumerControlReport");
_Static_assert(offsetof(SceHidConsumerControlReport, timestamp) == 8,
	       "HID consumer-control timestamp");
