/*
 * SceCtrl's controller sample, shared with SceDeci4pCtrlp (its ctrlp callback).
 */
#pragma once

#include <ctrl.h>

/*
 * A sample as the driver reads it from a pad (names unknown): SceCtrlData2's values, with the
 * sticks' wide-range values before the buttons' pressure.
 */
struct ctrlNativeData {
	SceUInt64 timeStamp;
	SceUInt32 buttons;
	SceUInt8 lx;
	SceUInt8 ly;
	SceUInt8 rx;
	SceUInt8 ry;
	SceUInt8 wideLx;		/* SCE_CTRL_MODE_DIGITALANALOG_WIDE */
	SceUInt8 wideLy;
	SceUInt8 wideRx;
	SceUInt8 wideRy;
	union {
		struct {		/* SceCtrlData2's */
			SceUInt8 up;
			SceUInt8 right;
			SceUInt8 down;
			SceUInt8 left;
			SceUInt8 l2;
			SceUInt8 r2;
			SceUInt8 l1;
			SceUInt8 r1;
			SceUInt8 triangle;
			SceUInt8 circle;
			SceUInt8 cross;
			SceUInt8 square;
		};
		SceUInt8 pressure[12];
	};
};

_Static_assert(sizeof(struct ctrlNativeData) == 0x20, "ctrlNativeData");

/*
 * A sample as the driver keeps it, then a second set of stick values that sampling mode 3
 * returns (filled from the wide-range values) and four bytes that are only cleared.
 */
struct ctrlNativeSample {
	struct ctrlNativeData sample;
	SceUInt8 extended[4];
	SceUInt8 reserved[4];
};
