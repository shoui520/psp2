/*
 * SceCamera (os0:kd/bootimage/camera, camera_dummy): error codes, as used so far.
 */

#pragma once

#include <scetypes.h>

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

int SceCameraForDriver_0E9B4B9E(int devnum);
int SceCameraForDriver_22E70D83(int devnum);
int SceCameraForDriver_45EB69C9(void);
int SceCameraForDriver_5338BE86(int devnum);
int SceCameraForDriver_8C3354E5(int devnum);
int SceCameraForDriver_C2E71BE8(int devnum, int attribute, int *pValue, int flags);
int SceCameraForDriver_F7A0C99F(void);
int SceCameraForDriver_FE3C170C(int devnum, int attribute, int *pValue, int flags);
int sceCameraClose(int devnum);
int sceCameraGetAntiFlicker(int devnum, int *pMode);
int sceCameraGetAutoControlHold(int devnum, int *pMode);
int sceCameraGetBacklight(int devnum, int *pMode);
int sceCameraGetBrightness(int devnum, int *pLevel);
int sceCameraGetContrast(int devnum, int *pLevel);
int sceCameraGetEV(int devnum, int *pLevel);
int sceCameraGetEffect(int devnum, int *pMode);
int sceCameraGetExposureCeiling(int devnum, int *pMode);
int sceCameraGetGain(int devnum, int *pMode);
int sceCameraGetISO(int devnum, int *pMode);
int sceCameraGetImageQuality(int devnum, int *pLevel);
int sceCameraGetNightmode(int devnum, int *pMode);
int sceCameraGetNoiseReduction(int devnum, int *pLevel);
int sceCameraGetReverse(int devnum, int *pMode);
int sceCameraGetSaturation(int devnum, int *pLevel);
int sceCameraGetSharpness(int devnum, int *pLevel);
int sceCameraGetSharpnessOff(int devnum, int *pLevel);
int sceCameraGetWhiteBalance(int devnum, int *pMode);
int sceCameraGetZoom(int devnum, int *pLevel);
int sceCameraIsActive(int devnum);
int sceCameraSetAntiFlicker(int devnum, int mode);
int sceCameraSetAutoControlHold(int devnum, int mode);
int sceCameraSetBacklight(int devnum, int mode);
int sceCameraSetBrightness(int devnum, int level);
int sceCameraSetContrast(int devnum, int level);
int sceCameraSetEV(int devnum, int level);
int sceCameraSetEffect(int devnum, int mode);
int sceCameraSetExposureCeiling(int devnum, int mode);
int sceCameraSetGain(int devnum, int mode);
int sceCameraSetISO(int devnum, int mode);
int sceCameraSetImageQuality(int devnum, int level);
int sceCameraSetNightmode(int devnum, int mode);
int sceCameraSetNoiseReduction(int devnum, int level);
int sceCameraSetReverse(int devnum, int mode);
int sceCameraSetSaturation(int devnum, int level);
int sceCameraSetSharpness(int devnum, int level);
int sceCameraSetSharpnessOff(int devnum, int level);
int sceCameraSetWhiteBalance(int devnum, int mode);
int sceCameraSetZoom(int devnum, int level);
int sceCameraStart(int devnum);
int sceCameraStop(int devnum);
