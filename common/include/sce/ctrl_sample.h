/*
 * SceCtrl's controller sample, shared with SceDeci4pCtrlp (its ctrlp callback).
 */
#pragma once

#include <psp2common/ctrl.h>

/*
 * A sample as the driver keeps it: Sony's SceCtrlData2 layout (vitasdk's
 * SceCtrlData), then a second set of stick values that sampling mode 3 returns
 * (filled from the wide-range values) and four bytes that are only cleared.
 */
struct ctrlNativeSample {
	SceCtrlData sample;
	SceUInt8 extended[4];
	SceUInt8 reserved[4];
};
