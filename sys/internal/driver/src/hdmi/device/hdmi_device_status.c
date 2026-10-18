/*
 * SceHdmi device layer: what the state machine asks about the device's state
 * (HdmiDevice.request and .pendingStatus under the "SceHdmiDeviceStatus"
 * mutex, the events it polls, the plug and PLL state). Internal names are
 * ours unless a comment says otherwise.
 */
#include <stddef.h>
#include <scetypes.h>
#include <threadmgr/threadmgr.h>
#include <fw.h>
#include "../hdmi_internal.h"

/* HdmiDevice.request */
#define HDMI_REQUEST_MODE	1	/* a video mode was set */
#define HDMI_REQUEST_RESET	2	/* the output is to start again */

/* HdmiDevice.pendingStatus */
#define HDMI_STATUS_SEEN	1
#define HDMI_STATUS_CHANGED	5

static SceUID statusMutex;

int hdmiDeviceStatusInitialize(HdmiDevice *device)
{
	statusMutex = 0;
	statusMutex = sceKernelCreateMutex("SceHdmiDeviceStatus", 0x2002, 0, NULL);
	return statusMutex < 0 ? SCE_HDMI_ERROR_DEVICE : 0;
}

int hdmiDeviceSmLock(void)
{
	SceUInt32 timeout = 0xffffffff;

	return sceKernelLockMutex(statusMutex, 1, &timeout);
}

int hdmiDeviceSmUnlock(void)
{
	return sceKernelUnlockMutex(statusMutex, 1);
}

int hdmiDeviceStatusIsStopRequested(HdmiDevice *device)
{
	SceUInt32 events;

	return hdmiDeviceSmPollEvent(&events, 0x40);
}

int hdmiDeviceStatusIsUnplugged(HdmiDevice *device)
{
	SceUInt32 events;

	return hdmiDeviceSmPollEvent(&events, 0x01);
}

int hdmiDeviceStatusHasRequest(HdmiDevice *device)
{
	SceUInt32 timeout = 0xffffffff;
	int ret;

	sceKernelLockMutex(statusMutex, 1, &timeout);
	ret = device->request != 0;
	sceKernelUnlockMutex(statusMutex, 1);
	return ret;
}

int hdmiDeviceStatusIsResetRequested(HdmiDevice *device)
{
	SceUInt32 timeout = 0xffffffff;
	int ret;

	sceKernelLockMutex(statusMutex, 1, &timeout);
	ret = device->request == HDMI_REQUEST_RESET;
	sceKernelUnlockMutex(statusMutex, 1);
	return ret;
}

int hdmiDeviceStatusIsModeRequested(HdmiDevice *device)
{
	SceUInt32 timeout = 0xffffffff;
	int ret;

	sceKernelLockMutex(statusMutex, 1, &timeout);
	ret = device->request == HDMI_REQUEST_MODE;
	sceKernelUnlockMutex(statusMutex, 1);
	return ret;
}

int hdmiDeviceStatusIsReady(HdmiDevice *device)
{
	SceUInt32 timeout = 0xffffffff;

	sceKernelLockMutex(statusMutex, 1, &timeout);
	sceKernelUnlockMutex(statusMutex, 1);
	return 1;
}

int hdmiDeviceStatusNeedsReauth(HdmiDevice *device)
{
	SceUInt32 timeout = 0xffffffff;

	sceKernelLockMutex(statusMutex, 1, &timeout);
	sceKernelUnlockMutex(statusMutex, 1);
	return 0;
}

/* Did the status change since the last call? */
int hdmiDeviceStatusIsChanged(HdmiDevice *device)
{
	SceUInt32 timeout = 0xffffffff;
	int status;

	sceKernelLockMutex(statusMutex, 1, &timeout);
	status = device->pendingStatus;
	device->pendingStatus = HDMI_STATUS_SEEN;
	sceKernelUnlockMutex(statusMutex, 1);
	return status == HDMI_STATUS_CHANGED;
}

int hdmiDeviceStatusGetPllStat(HdmiDevice *device)
{
	return HdmiDeviceHwGetPllStat(device);
}

int hdmiDeviceStatusGetPlgStat(HdmiDevice *device)
{
	return HdmiDeviceHwGetPlgStat(device);
}

