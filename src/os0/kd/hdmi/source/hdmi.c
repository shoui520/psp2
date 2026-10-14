/*
 * SceHdmi: module start and the SceHdmiForDriver entry points.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/modulemgr.h>
#include "hdmi_internal.h"

int module_start(SceSize args, void *argp)
{
	int ret = controlHdmiModuleStart();

	switch (ret) {
	case 4:
	case 7:
		return SCE_KERNEL_START_FAILED;
	case 2:
		return SCE_KERNEL_START_NO_RESIDENT;
	default:
		return SCE_KERNEL_START_SUCCESS;
	}
}

int SceHdmiForDriver_8EB0FD9C(SceHdmiEdidInfo *info)
{
	return edidManagerGetInfoCopy(info);
}

int SceHdmiForDriver_5A212BB3(const SceHdmiOutputMode *mode)
{
	if (hdmiSetVideoMode(mode) != 0)
		return SCE_HDMI_ERROR_SET_MODE;
	return 0;
}

int SceHdmiForDriver_E66D5A4D(int mute)
{
	if (hdmiSetAvMute(mute) != 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int SceHdmiForDriver_99A3CABE(int mute)
{
	if (hdmiSetVideoMute(mute) != 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int SceHdmiForDriver_854EEB78(const SceHdmiAudioMode *mode)
{
	return hdmiSetAudioMode(mode);
}

int SceHdmiForDriver_08AFFB03(int mute)
{
	if (hdmiSetAudioMute(mute) != 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int SceHdmiForDriver_0F12364A(SceUInt32 event, HdmiEventCallback callback)
{
	return _sceHdmiRegisterEventCallback(event, callback);
}

int SceHdmiForDriver_E331EC90(SceUInt32 event)
{
	return _sceHdmiUnRegisterEventCallback(event);
}

int SceHdmiForDriver_0686FBD6(int on)
{
	if (controlHdmiSetHdcpMode(on) != 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int SceHdmiForDriver_F20529CC(int on)
{
	if (controlHdmiSetEdidMode(on) != 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int SceHdmiForDriver_FAB3A2E9(int on)
{
	if (controlHdmiSetHdcpModeAgain(on) != 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int SceHdmiForDriver_7AD0F6C9(void *id, SceSize size, SceUInt8 *len)
{
	int ret;

	if (len != NULL && id != NULL && size > 4) {
		ret = controlHdmiGetChipId(id, size, len);
		switch (ret) {
		case 1:
			return SCE_HDMI_ERROR_INVALID_ARGUMENT;
		case 2:
			return SCE_HDMI_ERROR_DEVICE;
		case 3:
			return SCE_HDMI_ERROR_BUSY;
		default:
			return 0;
		}
	}
	return SCE_HDMI_ERROR_INVALID_ARGUMENT;
}

int SceHdmiForDriver_FF5546D8(void)
{
	if (controlHdmiGetHdcpAksv() != 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int SceHdmiForDriver_B04F5B6A(void)
{
	return controlHdmiCecPowerOn();
}
