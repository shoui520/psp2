/*
 * SceHdmi: copying and clearing the EDID information. GCC kept the calls to
 * edid_info.c's accessors, so this is a file of its own; no debug record names
 * it, the name is ours.
 */
#include <psp2kern/types.h>
#include "hdmi_internal.h"
#include <sysmem/sysclib.h>

#define SCE_HDMI_ERROR_NO_EDID		0x80370008
#define SCE_HDMI_ERROR_LOCK		0x8037000c
#define SCE_HDMI_ERROR_STATUS		0x8037000d

int edidManagerGetInfoCopy(SceHdmiEdidInfo *info)
{
	int ret;

	if (info == NULL)
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	if (_sceHdmiLockMutex() != 0)
		return SCE_HDMI_ERROR_LOCK;
	if (edidManagerIsValid() == 0) {
		memset(info, 0, sizeof(*info));
		ret = SCE_HDMI_ERROR_NO_EDID;
	} else {
		memcpy(info, &edidManager.info, sizeof(*info));
		ret = 0;
	}
	if (sceHdmiDeviceCheckStatus() != 0)
		ret = SCE_HDMI_ERROR_STATUS;
	_sceHdmiUnlockMutex();
	return ret;
}

int edidManagerClearInfo(void)
{
	memset(&edidManager.info, 0, sizeof(edidManager.info));
	return 0;
}
