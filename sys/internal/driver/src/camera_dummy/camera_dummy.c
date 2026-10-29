/*
 * SceCameraDummy: the camera driver for units without cameras.
 *
 * The user-visible SceCamera syscalls are the real driver's (camera_user.c,
 * compiled from SceCamera's folder); the SceCameraForDriver layer underneath
 * reports that no camera is mounted.
 */

#include <sysmem/sysmem.h>
#include <camera/camera.h>
#include "../camera/camera_config.h"

int module_start(SceSize args, void *argp)
{
	return 0;
}

int SceCameraForDriver_45EB69C9(void)
{
	return 0;
}

int SceCameraForDriver_F7A0C99F(void)
{
	return 0;
}

int sceCameraOpen(int dev, SceCameraInfo *info)	/* open */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int sceCameraClose(int dev)				/* close */
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

int sceCameraRead(int dev, SceCameraRead *read)	/* read */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_C2E71BE8(int dev, int attr, int *value, int flags)	/* get attribute */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_FE3C170C(int dev, int attr, int *value, int flags)
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

int sceCameraIsActive(int dev)				/* is active */
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_EB1CC2CA(SceUID processId, int *devices)
{
	return SCE_CAMERA_ERROR_NOT_MOUNTED;
}

int SceCameraForDriver_EE3D4B1C(int dev, SceFVector3 *location)		/* device location */
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

/* SceCamera_99B5FB62's driver call (camera.c's name), not exported */
int cameraSetAlternateFrameBuffers(unsigned int mode)
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
