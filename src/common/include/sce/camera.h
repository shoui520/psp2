/*
 * SceCamera (os0:kd/bootimage/camera, camera_dummy): error codes, as used so far.
 */

#pragma once

#include <psp2kern/types.h>

/* The extended Sony camera-read parameter; the earlier SDK revision is smaller. */
typedef struct SceCameraReadV2 {
	SceSize sizeThis;
	SceInt32 dwMode;
	SceInt32 dwExposureTimeMode;
	SceInt32 dwStatus;
	SceUInt64 qwFrame;
	SceUInt64 qwTimestamp;
	SceUInt32 dwExposureTime;
	SceUInt32 dwExposureTimeGap;
	SceUInt32 dwRaw8Format;
	SceUInt32 padding;
	SceSize sizeIBase;
	SceSize sizeUBase;
	SceSize sizeVBase;
	void *pvIBase;
	void *pvUBase;
	void *pvVBase;
} SceCameraReadV2;

_Static_assert(sizeof(SceCameraReadV2) == 0x48, "SceCameraReadV2 size");
_Static_assert(__builtin_offsetof(SceCameraReadV2, sizeIBase) == 0x30 &&
	       __builtin_offsetof(SceCameraReadV2, pvIBase) == 0x3c,
	       "SceCameraReadV2 layout");

#define SCE_CAMERA_ERROR_PARAM			0x802E0000
#define SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN	0x802E000C
#define SCE_CAMERA_ERROR_NOT_MOUNTED		0x802E0010
