/*
 * SceCameraDummy: the camera driver for units without cameras.
 *
 * The user-visible SceCamera syscalls validate their arguments and copy the
 * caller's structures in and out exactly as the real driver does; the
 * SceCameraForDriver layer underneath reports that no camera is mounted.
 */

#include <sysmem/sysmem.h>
#include <camera/camera.h>

/* the device argument: 0 (front) or 1 (back) */
#define CAMERA_DEVICE_VALID(dev)	(((dev) & ~1u) == 0)

/* attribute numbers of the generic get/set calls */
#define CAMERA_ATTR_SATURATION		0x0a
#define CAMERA_ATTR_BRIGHTNESS		0x0b
#define CAMERA_ATTR_CONTRAST		0x0c
#define CAMERA_ATTR_SHARPNESS		0x0d
#define CAMERA_ATTR_REVERSE		0x0e
#define CAMERA_ATTR_EFFECT		0x0f
#define CAMERA_ATTR_EV			0x10
#define CAMERA_ATTR_ZOOM		0x11
#define CAMERA_ATTR_ANTIFLICKER		0x12
#define CAMERA_ATTR_ISO			0x14
#define CAMERA_ATTR_WHITEBALANCE	0x16
#define CAMERA_ATTR_BACKLIGHT		0x17
#define CAMERA_ATTR_NIGHTMODE		0x18
#define CAMERA_ATTR_NOISEREDUCTION	0x19
#define CAMERA_ATTR_AUTOCONTROLHOLD	0x1a
#define CAMERA_ATTR_EXPOSURECEILING	0x1b
#define CAMERA_ATTR_IMAGEQUALITY	0x1c

#define CAMERA_OPEN_PARAM_MIN		0x28
#define CAMERA_OPEN_PARAM_MAX		0x30
#define CAMERA_READ_PARAM_MIN		0x18
#define CAMERA_READ_PARAM_MAX		0x48

/* ---- SceCameraForDriver: no camera ---- */

int SceCameraForDriver_45EB69C9(void)
{
	return 0;
}

int SceCameraForDriver_F7A0C99F(void)
{
	return 0;
}

int SceCameraForDriver_E487870A(int dev, void *param)		/* open */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_0E9B4B9E(int dev)				/* close */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_5338BE86(int dev)				/* start */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_8C3354E5(int dev)				/* stop */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_2D12FDF3(int dev, void *param)		/* read */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_C2E71BE8(int dev, int attr, int *value, int unk)	/* get attribute */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_FE3C170C(int dev, int attr, int *value, int unk)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_D56A12BD(int dev, int attr, int value)	/* set attribute */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_B312CE90(int dev, int attr, int value)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_E81EADE1(SceUID processId)
{
	return 0;
}

int SceCameraForDriver_93477709(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_B241F11F(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_36C164BE(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_6443EEDB(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_DFBBA264(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_22E70D83(int dev)				/* is active */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_EB1CC2CA(SceUID processId, int *devices)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_EE3D4B1C(int dev, void *location)		/* device location */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

void SceCameraForDriver_22D51107(void)
{
}

int SceCameraForDriver_96C63737(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_D0810455(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

/* name unknown: driver call of SceCamera_99B5FB62, not exported */
static int __attribute__((noinline)) cameraDriver99B5FB62(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_D259707C(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_3590BB9B(void)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

/* ---- SceCamera: user syscalls ---- */

/* copy a size-prefixed parameter structure in, call the driver, copy it back */
#define CAMERA_PARAM_CALL(dev, uparam, min, max, driver) ({			\
	int __buf[(max) / 4 + 2];						\
	int __ret, __res;							\
									\
	if (!CAMERA_DEVICE_VALID(dev) || (uparam) == NULL)			\
		return SCE_CAMERA_ERROR_PARAM;					\
	__ret = sceKernelCopyFromUser(__buf, (uparam), 4);			\
	if (__ret < 0)								\
		return __ret;							\
	if ((SceUInt32)__buf[0] - (min) > (max) - (min))			\
		return SCE_CAMERA_ERROR_PARAM;					\
	__ret = sceKernelCopyFromUser(__buf, (uparam), __buf[0]);		\
	if (__ret < 0)								\
		return __ret;							\
	__res = driver((dev), __buf);						\
	__ret = sceKernelCopyToUser((uparam), __buf, __buf[0]);		\
	__ret < 0 ? __ret : __res;						\
})

int sceCameraOpen(int dev, SceCameraInfo *info)
{
	return CAMERA_PARAM_CALL(dev, info, CAMERA_OPEN_PARAM_MIN, CAMERA_OPEN_PARAM_MAX,
				 SceCameraForDriver_E487870A);
}

int sceCameraClose(int dev)
{
	if (!CAMERA_DEVICE_VALID(dev))
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_0E9B4B9E(dev);
}

int sceCameraStart(int dev)
{
	if (!CAMERA_DEVICE_VALID(dev))
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_5338BE86(dev);
}

int sceCameraStop(int dev)
{
	if (!CAMERA_DEVICE_VALID(dev))
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_8C3354E5(dev);
}

int sceCameraRead(int dev, SceCameraRead *read)
{
	return CAMERA_PARAM_CALL(dev, read, CAMERA_READ_PARAM_MIN, CAMERA_READ_PARAM_MAX,
				 SceCameraForDriver_2D12FDF3);
}

int sceCameraIsActive(int dev)
{
	if (!CAMERA_DEVICE_VALID(dev))
		return SCE_CAMERA_ERROR_PARAM;
	return SceCameraForDriver_22E70D83(dev);
}

int sceCameraGetDeviceLocation(int dev, SceFVector3 *location)
{
	int buf[3];
	int ret;

	if (!CAMERA_DEVICE_VALID(dev) || location == NULL)
		return SCE_CAMERA_ERROR_PARAM;
	ret = sceKernelCopyFromUser(buf, location, sizeof(buf));
	if (ret < 0)
		return ret;
	ret = SceCameraForDriver_EE3D4B1C(dev, buf);
	if (ret < 0)
		return ret;
	ret = sceKernelCopyToUser(location, buf, sizeof(buf));
	return ret < 0 ? ret : 0;
}

/* sceCameraGet<Attr>(dev, int *value): the value is copied in, then out */
#define CAMERA_GET(name, attr, driver)						\
int name(int dev, int *value)							\
{										\
	int v, ret;								\
										\
	if (!CAMERA_DEVICE_VALID(dev) || value == NULL)				\
		return SCE_CAMERA_ERROR_PARAM;					\
	ret = sceKernelCopyFromUser(&v, value, sizeof(v));			\
	if (ret < 0)								\
		return ret;							\
	ret = driver(dev, (attr), &v, 0);					\
	if (ret < 0)								\
		return ret;							\
	ret = sceKernelCopyToUser(value, &v, sizeof(v));			\
	return ret < 0 ? ret : 0;						\
}

#define CAMERA_SET(name, attr, driver)						\
int name(int dev, int value)							\
{										\
	if (!CAMERA_DEVICE_VALID(dev))						\
		return SCE_CAMERA_ERROR_PARAM;					\
	return driver(dev, (attr), value);					\
}

CAMERA_GET(sceCameraGetSaturation, CAMERA_ATTR_SATURATION, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetSaturation, CAMERA_ATTR_SATURATION, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetBrightness, CAMERA_ATTR_BRIGHTNESS, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetBrightness, CAMERA_ATTR_BRIGHTNESS, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetContrast, CAMERA_ATTR_CONTRAST, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetContrast, CAMERA_ATTR_CONTRAST, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetSharpness, CAMERA_ATTR_SHARPNESS, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetSharpness, CAMERA_ATTR_SHARPNESS, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetReverse, CAMERA_ATTR_REVERSE, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetReverse, CAMERA_ATTR_REVERSE, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetEffect, CAMERA_ATTR_EFFECT, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetEffect, CAMERA_ATTR_EFFECT, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetEV, CAMERA_ATTR_EV, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetEV, CAMERA_ATTR_EV, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetZoom, CAMERA_ATTR_ZOOM, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetZoom, CAMERA_ATTR_ZOOM, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetAntiFlicker, CAMERA_ATTR_ANTIFLICKER, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetAntiFlicker, CAMERA_ATTR_ANTIFLICKER, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetISO, CAMERA_ATTR_ISO, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetISO, CAMERA_ATTR_ISO, SceCameraForDriver_D56A12BD)

/* gain is not supported through these calls */
int sceCameraGetGain(int dev, int *gain)
{
	if (!CAMERA_DEVICE_VALID(dev))
		return SCE_CAMERA_ERROR_PARAM;
	return gain != NULL ? SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN : SCE_CAMERA_ERROR_PARAM;
}

int sceCameraSetGain(int dev, int gain)
{
	if (!CAMERA_DEVICE_VALID(dev))
		return SCE_CAMERA_ERROR_PARAM;
	return SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN;
}

CAMERA_GET(sceCameraGetWhiteBalance, CAMERA_ATTR_WHITEBALANCE, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetWhiteBalance, CAMERA_ATTR_WHITEBALANCE, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetBacklight, CAMERA_ATTR_BACKLIGHT, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetBacklight, CAMERA_ATTR_BACKLIGHT, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetNightmode, CAMERA_ATTR_NIGHTMODE, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetNightmode, CAMERA_ATTR_NIGHTMODE, SceCameraForDriver_D56A12BD)

int SceCamera_99B5FB62(void)
{
	return cameraDriver99B5FB62();
}

CAMERA_GET(sceCameraGetNoiseReduction, CAMERA_ATTR_NOISEREDUCTION, SceCameraForDriver_FE3C170C)
CAMERA_SET(sceCameraSetNoiseReduction, CAMERA_ATTR_NOISEREDUCTION, SceCameraForDriver_B312CE90)
CAMERA_GET(sceCameraGetSharpnessOff, CAMERA_ATTR_SHARPNESS, SceCameraForDriver_FE3C170C)
CAMERA_SET(sceCameraSetSharpnessOff, CAMERA_ATTR_SHARPNESS, SceCameraForDriver_B312CE90)
CAMERA_GET(sceCameraGetAutoControlHold, CAMERA_ATTR_AUTOCONTROLHOLD, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetAutoControlHold, CAMERA_ATTR_AUTOCONTROLHOLD, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetExposureCeiling, CAMERA_ATTR_EXPOSURECEILING, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetExposureCeiling, CAMERA_ATTR_EXPOSURECEILING, SceCameraForDriver_D56A12BD)
CAMERA_GET(sceCameraGetImageQuality, CAMERA_ATTR_IMAGEQUALITY, SceCameraForDriver_C2E71BE8)
CAMERA_SET(sceCameraSetImageQuality, CAMERA_ATTR_IMAGEQUALITY, SceCameraForDriver_D56A12BD)

int module_start(SceSize args, void *argp)
{
	return 0;
}
