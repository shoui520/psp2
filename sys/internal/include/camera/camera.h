/*
 * SceCamera (os0:kd/bootimage/camera, camera_dummy): the driver's exports beside the SDK's
 * (camera.h), and the size of the read parameter it takes.
 */

#pragma once

#include <scetypes.h>
#include <camera.h>

/* the SDK's read parameter, 0x48 bytes since the exposure fields; the earlier SDK revision is smaller */
_Static_assert(sizeof(SceCameraRead) == 0x48, "SceCameraRead size");
_Static_assert(__builtin_offsetof(SceCameraRead, sizeIBase) == 0x30 &&
	       __builtin_offsetof(SceCameraRead, pvIBase) == 0x3c,
	       "SceCameraRead layout");

int SceCameraForDriver_0E9B4B9E(int devnum);
int SceCameraForDriver_22E70D83(int devnum);
int SceCameraForDriver_45EB69C9(void);
int SceCameraForDriver_5338BE86(int devnum);
int SceCameraForDriver_8C3354E5(int devnum);
int SceCameraForDriver_C2E71BE8(int devnum, int attribute, int *pValue, int flags);
/* vitasdk: ksceCameraIsActive */
int SceCameraForDriver_EB1CC2CA(SceUID processId, int *devices);
int SceCameraForDriver_E81EADE1(SceUID processId);
int SceCameraForDriver_F7A0C99F(void);
int SceCameraForDriver_FE3C170C(int devnum, int attribute, int *pValue, int flags);
int sceCameraGetImageQuality(int devnum, int *pLevel);
int sceCameraGetNoiseReduction(int devnum, int *pLevel);
int sceCameraGetSharpnessOff(int devnum, int *pLevel);
int sceCameraSetImageQuality(int devnum, int level);
int sceCameraSetNoiseReduction(int devnum, int level);
int sceCameraSetSharpnessOff(int devnum, int level);
