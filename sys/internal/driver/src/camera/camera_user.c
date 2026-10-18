/* SceCamera user entry points, shared with SceCameraDummy. */

#include <camera.h>
#include <sysmem/sysmem.h>
#include <camera/camera.h>

int SceCameraForDriver_E487870A(int devnum, SceCameraInfo *pInfo);
int SceCameraForDriver_2D12FDF3(int devnum, SceCameraRead *pRead);
int SceCameraForDriver_EE3D4B1C(int devnum, SceFVector3 *pLocation);
int SceCameraForDriver_D56A12BD(int devnum, int attribute, int value);
/* Name unknown; same user wrapper family as SceCameraDummy. */
int cameraSetAlternateFrameBuffers(unsigned int mode);
int SceCameraForDriver_B312CE90(int devnum, int attribute, int value);

int sceCameraOpen(int devnum, SceCameraInfo *pInfo)
{
	SceCameraInfo *info;
	SceSize size;
	int result, driverResult;

	if ((devnum & ~1u) != 0 || pInfo == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	info = __builtin_alloca(sizeof(*info));
	result = sceKernelCopyFromUser(info, pInfo, sizeof(info->sizeThis));
	if (result < 0)
		return result;
	size = info->sizeThis;
	if (size - 0x28 > 8)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(info, pInfo, size);
	if (result < 0)
		return result;
	driverResult = SceCameraForDriver_E487870A(devnum, info);
	result = sceKernelCopyToUser(pInfo, info, size);
	return result < 0 ? result : driverResult;
}

int sceCameraClose(int devnum)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_0E9B4B9E(devnum);
}

int sceCameraStart(int devnum)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_5338BE86(devnum);
}

int sceCameraStop(int devnum)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_8C3354E5(devnum);
}

int sceCameraRead(int devnum, SceCameraRead *pRead)
{
	SceCameraRead *read;
	SceSize size;
	int result, driverResult;

	if ((devnum & ~1u) != 0 || pRead == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	/* The kernel accepts the extended 0x48-byte read parameter. */
	read = __builtin_alloca(0x48);
	result = sceKernelCopyFromUser(read, pRead, sizeof(read->sizeThis));
	if (result < 0)
		return result;
	size = read->sizeThis;
	if (size - 0x18 > 0x30)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(read, pRead, size);
	if (result < 0)
		return result;
	driverResult = SceCameraForDriver_2D12FDF3(devnum, read);
	result = sceKernelCopyToUser(pRead, read, size);
	return result < 0 ? result : driverResult;
}

int sceCameraIsActive(int devnum)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_22E70D83(devnum);
}

int sceCameraGetDeviceLocation(int devnum, SceFVector3 *pLocation)
{
	SceFVector3 location;
	int result;

	if ((devnum & ~1u) != 0 || pLocation == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&location, pLocation, sizeof(location));
	if (result < 0)
		return result;
	result = SceCameraForDriver_EE3D4B1C(devnum, &location);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLocation, &location, sizeof(location));
	return result < 0 ? result : 0;
}

int sceCameraGetSaturation(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x0a, &level, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetSaturation(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x0a, level);
}

int sceCameraGetBrightness(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x0b, &level, 0);
	if (result < 0)
		return result;
	level = (unsigned char)level;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetBrightness(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x0b, level);
}

int sceCameraGetContrast(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x0c, &level, 0);
	if (result < 0)
		return result;
	level = (unsigned char)level;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetContrast(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x0c, level);
}

int sceCameraGetSharpness(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x0d, &level, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetSharpness(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x0d, level);
}

int sceCameraGetReverse(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x0e, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetReverse(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x0e, mode);
}

int sceCameraGetEffect(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x0f, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetEffect(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x0f, mode);
}

int sceCameraGetEV(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x10, &level, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetEV(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x10, level);
}

int sceCameraGetZoom(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x11, &level, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetZoom(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x11, level);
}

int sceCameraGetAntiFlicker(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x12, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetAntiFlicker(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x12, mode);
}

int sceCameraGetISO(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x14, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetISO(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x14, mode);
}

int sceCameraGetGain(int devnum, int *pMode)
{
	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	return SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN;
}

int sceCameraSetGain(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN;
}

int sceCameraGetWhiteBalance(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x16, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetWhiteBalance(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x16, mode);
}

int sceCameraGetBacklight(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x17, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetBacklight(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x17, mode);
}

int sceCameraGetNightmode(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x18, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetNightmode(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x18, mode);
}

int SceCamera_99B5FB62(unsigned int mode)
{
	return cameraSetAlternateFrameBuffers(mode);
}

int sceCameraGetNoiseReduction(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_FE3C170C(devnum, 0x19, &level, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetNoiseReduction(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_B312CE90(devnum, 0x19, level);
}

int sceCameraGetSharpnessOff(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_FE3C170C(devnum, 0x0d, &level, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetSharpnessOff(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_B312CE90(devnum, 0x0d, level);
}

int sceCameraGetAutoControlHold(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x1a, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetAutoControlHold(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x1a, mode);
}

int sceCameraGetExposureCeiling(int devnum, int *pMode)
{
	int mode, result;

	if ((devnum & ~1u) != 0 || pMode == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&mode, pMode, sizeof(mode));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x1b, &mode, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pMode, &mode, sizeof(mode));
	return result < 0 ? result : 0;
}

int sceCameraSetExposureCeiling(int devnum, int mode)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x1b, mode);
}

int sceCameraGetImageQuality(int devnum, int *pLevel)
{
	int level, result;

	if ((devnum & ~1u) != 0 || pLevel == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	result = sceKernelCopyFromUser(&level, pLevel, sizeof(level));
	if (result < 0)
		return result;
	result = SceCameraForDriver_C2E71BE8(devnum, 0x1c, &level, 0);
	if (result < 0)
		return result;
	result = sceKernelCopyToUser(pLevel, &level, sizeof(level));
	return result < 0 ? result : 0;
}

int sceCameraSetImageQuality(int devnum, int level)
{
	if ((devnum & ~1u) != 0)
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_D56A12BD(devnum, 0x1c, level);
}
