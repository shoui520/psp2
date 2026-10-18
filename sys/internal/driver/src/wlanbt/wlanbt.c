/* Sony wlanbt.c: configuration and device power management. */
#include "wlanbt_internal.h"
#include <skeleton.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/debug.h>
#include <wlanbt/wlanbt.h>
#include <sysmem/pmmgr.h>
#include <sysmem/procevent.h>
#include <syscon/syscon.h>
#include <power/power.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/qafmgr.h>
#include <idstorage/idstorage.h>
#include <clockgen/clockgen.h>
#include <ss_mgr/ss_mgr.h>

int wlanBtInitializeRobin(void);
int wlanBtResumeRobin(unsigned int *wakeSources);
int wlanBtStartRobin(void);
int wlanBtStopRobin(void);
int robinWlanSwitchProcess(int eventId);
int robinWlanStartProcess(SceUID pid, int eventType);
int robinWlanKillProcess(SceUID pid);
int robinWlanExitProcess(SceUID pid);
int robinWlanSetConfiguration(int request, int force);
unsigned int robinWlanGetConfiguration(void);
int robinWlanGetWakeOnTargetProcess(void);
int robinInitialize(void);

static SceWlanBtRobinConfig gWlanBtConfig;
static SceWlan gWlan;

int wlanBtPrepareWakeWait(void)
{
	uint8_t interruptEnable;
	int result;

	ScePowerForDriver_F535D928(4, 1);
	SceSdifForDriver_DECE963B(SceSdifForDriver_6A8235FC(2), 4, 1,
				 &gWlanBtConfig.interruptEnable);
	interruptEnable = gWlanBtConfig.interruptEnable & ~1;
	SceSdifForDriver_5D65E66B(SceSdifForDriver_6A8235FC(2), 4, 1,
				 &interruptEnable);
	result = SceSdifForDriver_AB0222F2(SceSdifForDriver_6A8235FC(2));
	gWlanBtConfig.configuration &= ~0x100u;
	return result;
}

int module_start(SceSize arglen, const void *argp)
{
	uint8_t availability;
	SceProductMode productMode;
	int result;

	memset(&gWlanBtConfig, 0, sizeof(gWlanBtConfig));
	gWlanBtConfig.wlanState = 4;
	gWlanBtConfig.bluetoothState = 4;
	if (sceKernelInitializeFastMutex(&gWlanBtConfig.mutex,
					"SceWlanBtRobinConfig", 0x2000, NULL) < 0)
		return 2;
	result = sceSblSsGetNvsData(1, &availability, 1);
	if (result < 0)
		return result;
	if ((SceSysconForDriver_CBD6D8BC() & 0xff0000u) < 0x400000) {
		if (availability != 0) {
			if (availability == 1) {
				gWlanBtConfig.available = 0;
				goto initialize;
			}
			if (SceSblAIMgrForDriver_3B638885() == 0) {
				result = ScePmMgrForDriver_2AC815A2(&productMode);
				if (result < 0)
					return result;
				if (productMode == 0 &&
				    SceQafMgrForDriver_52B4E164() == 0)
					goto initialize;
			}
		}
		goto available;
	}
	if (availability != 1)
		goto available;
	gWlanBtConfig.available = 0;
	SceDebugForDriver_391B74B7("[SceWlanBt]: WLAN/BT is Not Available.\n");
initialize:
	if (wlanBtInitializeRobin() < 0)
		return 2;
	return 0;
available:
	gWlanBtConfig.available = 1;
	goto initialize;
}

int wlanBtIsAvailable(void)
{
	return gWlanBtConfig.available;
}

int wlanBtGetWlanWakeSource(void)
{
	return gWlanBtConfig.wakeSources & 1;
}

int wlanBtGetBluetoothWakeSource(void)
{
	return gWlanBtConfig.wakeSources & 2;
}

void wlanBtPowerOnDevice(void)
{
	int result;

	if (SceSysconForDriver_C0F215B7() != 1 ||
	    (result = SceSysconForDriver_A2E85DB9(0)) >= 0) {
		result = SceClockgenForDriver_7B28A724();
		if (result >= 0) {
			sceKernelDelayThread(5000);
			result = SceSysconForDriver_A2E85DB9(1);
			if (result >= 0) {
				result = SceSysconForDriver_40FF3898(16, 0);
				if (result >= 0) {
					sceKernelDelayThread(10000);
					result = SceSysconForDriver_40FF3898(16, 1);
					if (result >= 0)
						sceKernelDelayThread(10000);
				}
			}
		}
	}
	SCE_DBG_ASSERT(result >= 0, 346);
}

void wlanBtPowerDownDevice(void)
{
	int result;

	SceSysconForDriver_40FF3898(16, 0);
	SceSysconForDriver_A2E85DB9(0);
	result = SceClockgenForDriver_B6F0A532();
	sceKernelDelayThread(500000);
	SCE_DBG_ASSERT(result >= 0, 372);
}

void wlanBtCompleteWakeWait(void)
{
	uint8_t interruptEnable;
	uint32_t wakeupFactor;
	unsigned int wakeSources;

	if (gWlanBtConfig.wlanState != 4 ||
	    gWlanBtConfig.bluetoothState != 4)
		return;
	ScePowerForDriver_F535D928(4, 0);
	gWlanBtConfig.wakeSources = 0;
	if (!(gWlanBtConfig.configuration & 0xc0))
		return;
	if (SceSysconForDriver_C0F215B7() == 0) {
		gWlanBtConfig.configuration = 0;
		return;
	}
	interruptEnable = gWlanBtConfig.interruptEnable;
	SceSdifForDriver_5D65E66B(SceSdifForDriver_6A8235FC(2), 4, 1,
				 &interruptEnable);
	wakeupFactor = 0;
	SceSysconForDriver_CF5B2F2F(&wakeupFactor);
	wakeupFactor &= 0x7f;
	if (wakeupFactor == 2) {
		wakeSources = 0;
		if ((gWlanBtConfig.configuration & 0xc0) == 0xc0) {
			wakeSources = 3;
			wlanBtResumeRobin(&wakeSources);
		} else if (gWlanBtConfig.configuration & 0x40) {
			wakeSources = 1;
			wlanBtResumeRobin(&wakeSources);
		} else {
			if (gWlanBtConfig.configuration & 0x80)
				wakeSources = 2;
			wlanBtResumeRobin(&wakeSources);
		}
		gWlanBtConfig.wakeSources = wakeSources;
		if (wakeSources)
			gWlanBtConfig.configuration &= ~0xf8u;
		else {
			wlanBtPowerDownDevice();
			gWlanBtConfig.configuration = 0;
		}
	} else {
		int result;

		SceSysconForDriver_40FF3898(16, 0);
		SceSysconForDriver_A2E85DB9(0);
		result = SceClockgenForDriver_B6F0A532();
		sceKernelDelayThread(500000);
		SCE_DBG_ASSERT_FUNC(result >= 0, 372, "wlanBtPowerDownDevice");
		gWlanBtConfig.configuration = 0;
	}
}

int wlanBtNotifyMonitor(unsigned int event, unsigned int argument,
		       unsigned int size, const void *data)
{
	if (gWlanBtConfig.monitor)
		return (*gWlanBtConfig.monitor)(event, argument, size, data);
	return 0;
}

unsigned int sceWlanBtGetConfiguration(void)
{
	unsigned int configuration;
	int result;

	result = lockFastMutex(&gWlanBtConfig.mutex);
	SCE_DBG_ASSERT_FUNC(result == 0, 428, "sceWlanBtGetConfiguration");
	configuration = gWlanBtConfig.configuration;
	result = sceKernelUnlockFastMutex(&gWlanBtConfig.mutex);
	SCE_DBG_ASSERT_FUNC(result == 0, 433, "sceWlanBtGetConfiguration");
	return configuration;
}

int sceWlanBtSetConfiguration(int request)
{
	unsigned int configuration;
	int result;
	int lockResult;

	lockResult = lockFastMutex(&gWlanBtConfig.mutex);
	SCE_DBG_ASSERT_FUNC(lockResult == 0, 446, "sceWlanBtSetConfiguration");
	if (SceSysconForDriver_9ADD60D2()) {
		result = 0x80418004;
		goto done;
	}
	result = 0;
	switch (request) {
	case 1:
		if (!(gWlanBtConfig.configuration & 1)) {
			result = wlanBtStartRobin();
			if (result >= 0)
				gWlanBtConfig.configuration |= 1;
		}
		break;
	case 2:
		if (gWlanBtConfig.configuration & 1) {
			wlanBtStopRobin();
			gWlanBtConfig.configuration = 0;
		}
		break;
	case 3:
		if (gWlanBtConfig.configuration & 8)
			wlanBtCompleteWakeWait();
		if (gWlanBtConfig.configuration & 1)
			gWlanBtConfig.configuration |= 2;
		else {
			result = wlanBtStartRobin();
			if (result == 0) {
				gWlanBtConfig.configuration |= 3;
				if (gWlanBtConfig.wlanState == 4 &&
				    gWlanBtConfig.bluetoothState == 4)
					gWlanBtConfig.configuration |= 0x100;
			}
		}
		gWlanBtConfig.wlanState = 1;
		k_sceEventLogPut(20001, 1, 0, &result, sizeof(result));
		break;
	case 4:
		configuration = gWlanBtConfig.configuration;
		if (configuration & 1) {
			gWlanBtConfig.configuration = configuration & ~0x102u;
			if (!(configuration & 4)) {
				wlanBtStopRobin();
				gWlanBtConfig.configuration = 0;
			}
		}
		gWlanBtConfig.wlanState = 4;
		k_sceEventLogPut(20001, 2, 0, &result, sizeof(result));
		break;
	case 5:
		if (!gWlanBtConfig.available) {
			result = 0x80418007;
			break;
		}
		if (gWlanBtConfig.configuration & 8)
			wlanBtCompleteWakeWait();
		if (gWlanBtConfig.configuration & 1)
			gWlanBtConfig.configuration |= 4;
		else {
			result = wlanBtStartRobin();
			if (result == 0) {
				gWlanBtConfig.configuration |= 5;
				if (gWlanBtConfig.wlanState == 4 &&
				    gWlanBtConfig.bluetoothState == 4)
					gWlanBtConfig.configuration |= 0x100;
			}
		}
		gWlanBtConfig.bluetoothState = 1;
		k_sceEventLogPut(20001, 3, 0, &result, sizeof(result));
		break;
	case 6:
		if (!gWlanBtConfig.available) {
			result = 0x80418007;
			break;
		}
		configuration = gWlanBtConfig.configuration;
		if (configuration & 1) {
			gWlanBtConfig.configuration = configuration & ~0x104u;
			if (!(configuration & 2)) {
				wlanBtStopRobin();
				gWlanBtConfig.configuration = 0;
			}
		}
		gWlanBtConfig.bluetoothState = 4;
		k_sceEventLogPut(20001, 4, 0, &result, sizeof(result));
		break;
	case 7:
		gWlanBtConfig.wlanState = 2;
		if (gWlanBtConfig.bluetoothState == 2)
			goto restartRobin;
		break;
	case 8:
		gWlanBtConfig.wlanState = 3;
		if (gWlanBtConfig.bluetoothState == 3)
			gWlanBtConfig.configuration |= 0x10;
		break;
	case 10:
		gWlanBtConfig.bluetoothState = 2;
		if (gWlanBtConfig.wlanState == 2)
			goto restartRobin;
		break;
	case 11:
		gWlanBtConfig.bluetoothState = 3;
		if (gWlanBtConfig.wlanState == 3)
			gWlanBtConfig.configuration |= 0x10;
		break;
	case 13:
		gWlanBtConfig.wlanState = 4;
		gWlanBtConfig.configuration |= 0x40;
		if (gWlanBtConfig.bluetoothState == 4)
			goto goWakeWaitState;
		break;
	case 14:
		gWlanBtConfig.wlanState = 4;
		gWlanBtConfig.configuration &= ~0x40u;
		if (gWlanBtConfig.bluetoothState == 4)
			goto goWakeWaitState;
		break;
	case 15:
		gWlanBtConfig.bluetoothState = 4;
		gWlanBtConfig.configuration |= 0xa0;
		if (gWlanBtConfig.wlanState == 4)
			goto goWakeWaitState;
		break;
	case 16:
		gWlanBtConfig.bluetoothState = 4;
		gWlanBtConfig.configuration =
			(gWlanBtConfig.configuration & ~0x80u) | 0x20;
		if (gWlanBtConfig.wlanState == 4)
			goto goWakeWaitState;
		break;
	case 17:
		gWlanBtConfig.wlanState = 4;
		gWlanBtConfig.forceOffFlags |= 1;
		gWlanBtConfig.configuration &= ~0x40u;
		if (gWlanBtConfig.bluetoothState == 4)
			goto goWakeWaitState;
		break;
	case 18:
		gWlanBtConfig.bluetoothState = 4;
		gWlanBtConfig.configuration =
			(gWlanBtConfig.configuration & ~0x80u) | 0x20;
		gWlanBtConfig.forceOffFlags |= 2;
		if (gWlanBtConfig.wlanState == 4)
			goto goWakeWaitState;
		break;
	default:
		break;
	}
	goto done;

restartRobin:
	wlanBtStopRobin();
	if (wlanBtStartRobin() < 0) {
		ScePowerForDriver_F535D928(4, 0);
		gWlanBtConfig.wlanState = 4;
		gWlanBtConfig.bluetoothState = 4;
		gWlanBtConfig.configuration = 0;
	} else
		gWlanBtConfig.configuration |= 9;
	goto done;

goWakeWaitState:
	if (gWlanBtConfig.forceOffFlags) {
		SCE_PRINTF_WITH_INFO_FUNC(0, 260, "_goWakeWaitState",
			"[SceWlanBt]: Force device off (0x%02x)\n",
			gWlanBtConfig.forceOffFlags);
		ScePowerForDriver_F535D928(4, 0);
		wlanBtStopRobin();
		gWlanBtConfig.configuration = 0;
		gWlanBtConfig.forceOffFlags = 0;
	} else if (gWlanBtConfig.configuration & 0xc0)
		wlanBtPrepareWakeWait();
	else {
		ScePowerForDriver_F535D928(4, 0);
		wlanBtStopRobin();
		gWlanBtConfig.configuration = 0;
	}
done:
	lockResult = sceKernelUnlockFastMutex(&gWlanBtConfig.mutex);
	SCE_DBG_ASSERT_FUNC(lockResult == 0, 615, "sceWlanBtSetConfiguration");
	return result;
}

int SceWlanBtForDriver_C5AA6F43(SceWlanBtMonitorCallback *monitor)
{
	gWlanBtConfig.monitor = monitor;
	return 0;
}

int sceWlanBtDetachMonitor(SceWlanBtMonitorCallback *monitor)
{
	SCE_DBG_ASSERT_FUNC(gWlanBtConfig.monitor == monitor, 630,
			    "sceWlanBtDetachMonitor");
	gWlanBtConfig.monitor = NULL;
	return 0;
}
int wlanBtSwitchProcess(int eventId, int eventType, void *param, int flags)
{
	return robinWlanSwitchProcess(eventId);
}

int wlanBtStartProcess(SceUID pid, int eventType,
		       ProcEventParam *param, int flags)
{
	return robinWlanStartProcess(pid, eventType);
}

int wlanBtKillProcess(SceUID pid, ProcEventParam *param, int flags)
{
	return robinWlanKillProcess(pid);
}

int wlanBtExitProcess(SceUID pid, ProcEventParam *param, int flags)
{
	return robinWlanExitProcess(pid);
}

int wlanBtPowerEvent(int eventId, uint32_t flags, void *argument)
{
	int result = eventId;

	if (eventId != 0x10021)
		return result;
	if ((flags & 0x30) == 0x30) {
		if (!gWlan.powerSuspended) {
			result = robinWlanSetConfiguration(4, 0);
			gWlan.powerSuspended = 1;
			gWlan.powerRestricted = 1;
		}
	} else if (flags & 0x20) {
		if (gWlan.powerSuspended) {
			robinWlanSetConfiguration(10, 0);
			gWlan.powerRestricted = 1;
			result = robinWlanSetConfiguration(9, 0);
			gWlan.powerSuspended = 0;
		} else if (!gWlan.powerRestricted) {
			result = robinWlanSetConfiguration(10, 0);
			gWlan.powerRestricted = 1;
		}
	} else {
		if (gWlan.powerSuspended) {
			result = robinWlanSetConfiguration(9, 0);
			gWlan.powerSuspended = 0;
		}
		if (gWlan.powerRestricted) {
			result = robinWlanSetConfiguration(11, 0);
			gWlan.powerRestricted = 0;
		}
	}
	return result;
}

unsigned int wlanGetConfiguration(void)
{
	return robinWlanGetConfiguration();
}

int wlanSetConfiguration(int request)
{
	return robinWlanSetConfiguration(request, 0);
}

int wlanRegisterCallback(SceUID callbackId)
{
	SceUID threadId = sceKernelGetThreadId();
	int slot;
	int result;

	lockFastMutex(&gWlan.mutex);
	if (gWlan.callbacks[0].threadId == threadId) {
		result = 0x80418002;
		goto done;
	}
	if (!gWlan.callbacks[0].threadId)
		slot = 0;
	else {
		if (gWlan.callbacks[1].threadId == threadId) {
			result = 0x80418002;
			goto done;
		}
		if (!gWlan.callbacks[1].threadId)
			slot = 1;
		else {
			result = 0x80418005;
			goto done;
		}
	}
	gWlan.callbacks[slot].threadId = threadId;
	gWlan.callbacks[slot].callbackId = callbackId;
	result = 0;
done:
	sceKernelUnlockFastMutex(&gWlan.mutex);
	return result;
}

int wlanUnregisterCallback(void)
{
	SceUID threadId = sceKernelGetThreadId();
	int slot;

	lockFastMutex(&gWlan.mutex);
	if (gWlan.callbacks[0].threadId == threadId)
		slot = 0;
	else if (gWlan.callbacks[1].threadId == threadId)
		slot = 1;
	else
		goto done;
	gWlan.callbacks[slot].threadId = 0;
	gWlan.callbacks[slot].callbackId = 0;
done:
	sceKernelUnlockFastMutex(&gWlan.mutex);
	return 0;
}

int wlanGetWakeOnTargetProcess(void)
{
	return robinWlanGetWakeOnTargetProcess();
}

int wlanBtInitializeRobin(void)
{
	SceProductMode productMode;
	int result;
	SceUID handlerId;

	productMode = 0;
	memset(&gWlan, 0, sizeof(gWlan));
	if (sceIdStorageLookup(0x110, 0, &gWlan.region, sizeof(gWlan.region)) < 0) {
		gWlan.region.channelMask = 0x7ff;
		gWlan.region.flags = 0;
	} else
		gWlan.region.channelMask &= 0x1fff;
	if (ScePmMgrForDriver_2AC815A2(&productMode) < 0)
		productMode = 0;
	else if (productMode == 1)
		gWlan.region.channelMask = 0x1fff;
	result = sceKernelInitializeFastMutex(&gWlan.mutex, "SceWlan", 0x2000, NULL);
	if (result < 0)
		return result;
	if (robinInitialize() < 0)
		return 2;
	gWlan.procEventHandler.resume = wlanBtStartProcess;
	gWlan.procEventHandler.notify = wlanBtSwitchProcess;
	gWlan.procEventHandler.exit = wlanBtExitProcess;
	gWlan.procEventHandler.kill = wlanBtKillProcess;
	gWlan.procEventHandler.create = NULL;
	gWlan.procEventHandler.suspend = NULL;
	gWlan.procEventHandler.size = sizeof(gWlan.procEventHandler);
	handlerId = sceKernelRegisterProcEventHandler("SceWlan", &gWlan.procEventHandler, 0);
	if (handlerId < 0)
		return 2;
	gWlan.procEventHandlerId = handlerId;
	return ScePowerForDriver_B24263F0(0, (PowerProcessFlagsCallback)wlanBtPowerEvent, NULL);
}

int wlanIsSupportedChannel(int channel)
{
	int index = channel - 1;

	if ((uint8_t)index > 12)
		return 0;
	return (gWlan.region.channelMask >> index) & 1;
}

int wlanIsActiveScanChannel(int channel)
{
	int index = channel - 1;

	if ((uint8_t)index > 12)
		return 0;
	if (!((gWlan.region.channelMask >> index) & 1))
		return 0;
	if (gWlan.region.flags & 1)
		return (unsigned int)(channel - 12) > 1;
	return 1;
}

int wlanNotifyCallbacks(int argument)
{
	lockFastMutex(&gWlan.mutex);
	if (gWlan.callbacks[0].threadId) {
		sceKernelNotifyCallback(gWlan.callbacks[0].callbackId, argument);
		if (!gWlan.callbacks[1].threadId)
			goto done;
		goto notifySecond;
	}
	if (gWlan.callbacks[1].threadId)
		goto notifySecond;
	goto done;

notifySecond:
	sceKernelNotifyCallback(gWlan.callbacks[1].callbackId, argument);
done:
	sceKernelUnlockFastMutex(&gWlan.mutex);
	return 0;
}
