/*
 * SceHdmi mode control (Sony's control_hdmi_mode.c): module start, the system
 * event handler, the transmitter's packet enables, HDCP/EDID modes, the chip
 * id and the CEC power. Internal names are ours unless a comment says otherwise.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/lowio/gpio.h>
#include <psp2kern/lowio/i2c.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <sysmem/sysevent.h>
#include "hdmi_internal.h"
#include <syscon/syscon.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/dipsw.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysclib.h>

#define HDMI_CHIP_ID_SIZE	10
#define HDMI_CHIP_ID_UNKNOWN	0xff	/* the init work has not run yet */

static SceUInt32 packetCtrl;
static SceUInt8 chipIdLength;
static int cecPowered;
static SceKernelFastMutex mutex;
static SceUInt8 chipId[HDMI_CHIP_ID_SIZE];

int controlHdmiSetPacketCtrl(SceUInt32 ctrl)
{
	packetCtrl = ctrl;
	if (sceHdmiDeviceSetPacketCtrl(ctrl) != 0)
		return 2;
	return 0;
}

int controlHdmiUpdatePacketCtrl(HdmiPacketCtrl ctrl)
{
	packetCtrl = (packetCtrl & ~ctrl.mask) | ctrl.set;
	if (sceHdmiDeviceSetPacketCtrl(packetCtrl) != 0)
		return 2;
	return 0;
}

static int _sceHdmiSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	int ret;

	if (resume != 0) {
		if (resume != 1)
			return 0;
	} else {
		if (eventid == SYSEVENT_SUSPEND_IDLE_ENTER)
			return sceHdmiDeviceSuspend();
		if (eventid == SYSEVENT_SUSPEND_IDLE_QUERY)
			return sceHdmiDeviceSuspendDone();
		return 0;
	}
	cecPowered = 0;
	if (eventid != SYSEVENT_RESUME_START)
		return 0;
	ret = ksceGpioSetPortMode(0, 15, 0);
	if (ret < 0) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(96);
#endif
		return ret;
	}
	ksceGpioPortClear(0, 15);
	ret = ksceI2cReset(0);
	if (ret < 0) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(108);
#endif
		return ret;
	}
	ret = sceHdmiDeviceResume();
	if (ret < 0) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(114);
#endif
		return ret;
	}
	ret = ksceI2cReset(1);
	if (ret < 0) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(120);
#endif
		return ret;
	}
	return ret;
}

static int initWork(void *arg)
{
	SceUInt8 length;

	if (ksceI2cReset(0) >= 0) {
		if (cecPowered == 0) {
			ksceSysconCtrlHdmiCecPower(1);
			cecPowered = 1;
		}
		if (ksceGpioSetPortMode(0, 15, 0) >= 0) {
			if ((ksceSysconGetHardwareInfo() & 0xffff00) < 0x102400)
				ksceGpioPortSet(0, 15);
			else
				ksceGpioPortClear(0, 15);
			if (_sceHdmiDeviceModuleStart() >= 0 && ksceI2cReset(1) >= 0 &&
			    sceHdmiDeviceGetChipId(chipId, &length) >= 0) {
				hdmiResetVicTiming();
				hdmiInfoFrameInitialize();
				if (!SceSblAIMgrForDriver_274663A0() ||
				    (SceQafMgrForDriver_382C71E8() != 0 && SceDipswForDriver_A98FC2FD(0xe5) == 0))
					sceHdmiDeviceSetHdcpMode(1);
				else
					sceHdmiDeviceSetHdcpMode(0);
				sceHdmiDeviceSetEdidMode(0);
				chipIdLength = length;
				return 1;
			}
		}
		if (cecPowered != 0) {
			ksceSysconCtrlHdmiCecPower(0);
			cecPowered = 0;
		}
	}
	chipIdLength = 0;
	return 1;
}

int controlHdmiModuleStart(void)
{
	SceUInt32 model;
	int i;

	cecPowered = 0;
	model = ksceSysconGetHardwareInfo() & 0xff0000;
	switch (model) {
	case 0x100000:
	case 0x310000:
	case 0x410000:
	case 0x510000:
	case 0x700000:
	case 0x720000:
		break;
	default:
		return 2;
	}
	for (i = 0; i < HDMI_CHIP_ID_SIZE; i++)
		chipId[i] = 0;
	chipIdLength = HDMI_CHIP_ID_UNKNOWN;
	eventHandleManagerInitialize();
	if (sceKernelInitializeFastMutex(&mutex, "SceHdmi", 2, 0) < 0 ||
	    sceKernelRegisterSysEventHandler("SceHdmi", _sceHdmiSysEventHandler, NULL) < 0)
		return 7;
	sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceHdmiInit", initWork, NULL);
	return 0;
}

int controlHdmiSetHdcpMode(int on)
{
	if (sceHdmiDeviceSetHdcpMode(on) != 0)
		return 2;
	return 0;
}

int controlHdmiSetEdidMode(int on)
{
	if (sceHdmiDeviceSetEdidMode(on) != 0)
		return 2;
	return 0;
}

int controlHdmiSetHdcpModeAgain(int on)
{
	if (sceHdmiDeviceSetHdcpMode(on) != 0)
		return 2;
	return 0;
}

int controlHdmiGetChipId(void *id, SceSize size, SceUInt8 *len)
{
	SceUInt8 length = chipIdLength;

	if (length == HDMI_CHIP_ID_UNKNOWN)
		return 3;
	if (length == 0)
		return 2;
	if (size < length)
		return 1;
	memcpy(id, chipId, length);
	*len = length;
	return 0;
}

int controlHdmiGetHdcpAksv(void)
{
	SceUInt8 aksv[5];

	if (sceHdmiDeviceGetHdcpAksv(aksv, 1) != 0)
		return 2;
	return 0;
}

int controlHdmiCecPowerOn(void)
{
	int ret;

	if (cecPowered != 0)
		return 0;
	ret = ksceSysconCtrlHdmiCecPower(1);
	cecPowered = 1;
	return ret;
}

int _sceHdmiLockMutex(void)
{
	if (lockFastMutex(&mutex) < 0)
		return 7;
	return 0;
}

int _sceHdmiUnlockMutex(void)
{
	if (sceKernelUnlockFastMutex(&mutex) < 0)
		return 7;
	return 0;
}
