#undef __FILE_NAME__
#define __FILE_NAME__ "../robin/wlan/wlan_robin.c"
#include "../../wlanbt/wlanbt_internal.h"
#include <sysmem/debug.h>
#include <sysmem/sysevent.h>
#include <threadmgr/threadmgr.h>
#include <wlanbt/wlanbt.h>
#include <regmgr/regmgr.h>
#include <sysmem/sblaimgr.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <sysmem/suspend.h>
#include <syscon/syscon.h>
#include <string.h>

#if FW_DEBUG
static int gSuspendConfigurationQueued;
#endif

SceWlanBtRobinWlanContext gWlanContext;

#if !FW_DEBUG
static int gSuspendConfigurationQueued;
#endif
static int gSuspendWorkerRequested;

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, gPendingCommandSuspendWait, 3067, "_debugDumpStatus");
SCE_DBGINFO_DEFINE(static, gCurrentCommandSuspendWait, 3062, "_debugDumpStatus");
SCE_DBGINFO_DEFINE(static, gSuspendVisitorStatus, 3056, "_debugDumpStatus");
SCE_DBGINFO_DEFINE(static, gSuspendCommandStatus, 3058, "_debugDumpStatus");
SCE_DBGINFO_DEFINE(static, gSuspendTimeoutStatus, 3052, "_debugDumpStatus");
#endif

int wlanBtIsAvailable(void);
int WlanVisitorEnter(SceWlanVisitor *visitor);
SceWlanVisitor *WlanVisitorLeave(SceWlanVisitor *visitor);
SceWlanVisitor *WlanVisitorClose(SceWlanVisitor *visitor, int result);
SceWlanVisitor *WlanVisitorInitialize(SceWlanVisitor *visitor);
int wlanNotifyCallbacks(int argument);
int wlanSetConfiguration(int request);
int robinWlanTcmSetMulticastAddresses(SceWlanBtRobinWlanTcm *tcm,
				    const SceNetEtherAddr *addresses, unsigned int size);
int robinWlanTcmSetStationPowerSave(SceWlanBtRobinWlanTcm *tcm, int enabled);
int robinWlanTcmScan(SceWlanBtRobinWlanTcm *tcm, WlanScanConfig *configuration,
		    unsigned int capacity);
int robinWlanTcmAssociate(SceWlanBtRobinWlanTcm *tcm, const WlanBssDesc *descriptor,
			  const WlanConnectionSecurity *security,
			  const void *informationElements, unsigned int informationElementsSize);
int wlanIsSupportedChannel(int channel);
uint32_t *wlanEnableInformationElement(uint32_t *mask, unsigned int id);
int wlanBssHasPrivacy(const WlanBssDesc *descriptor);
int wlanBssHasWpa(const WlanBssDesc *descriptor);
int wlanBssHasRsn(const WlanBssDesc *descriptor);
int ksceUsbServEtherDisable(void);
int robinWlanBeginWakeOnConfiguration(SceWlanBtRobinWlanContext *context);
int robinWlanBeginHostSleepReset(SceWlanBtRobinWlanContext *context);
int ksceUsbServEtherEnable(void);
int robinWlanTcmStartBackgroundScan(SceWlanBtRobinWlanTcm *tcm,
				  const WlanBackgroundScanConfig *configuration,
				  unsigned int capacity);
int robinWlanTcmDisconnect(SceWlanBtRobinWlanTcm *tcm);
int robinWlanTcmStartAdhoc(SceWlanBtRobinWlanTcm *tcm,
			 WlanBssConnectionConfig *configuration, unsigned int capacity);
int robinWlanTcmJoinAdhoc(SceWlanBtRobinWlanTcm *tcm,
			WlanBssConnectionConfig *configuration, unsigned int capacity);
int robinWlanTcmStopAdhoc(SceWlanBtRobinWlanTcm *tcm);
int robinWlanTcmSetTransmitRate(SceWlanBtRobinWlanTcm *tcm, const void *request);
int robinWlanTcmReadSettings(SceWlanBtRobinWlanTcm *tcm, void *request);
int robinWlanTcmWriteSettings(SceWlanBtRobinWlanTcm *tcm, const void *request);
int robinWlanTcmGetCardInfo(SceWlanBtRobinWlanTcm *tcm, void *request);
int robinWlanTcmStopBackgroundScan(SceWlanBtRobinWlanTcm *tcm);
int robinWlanTcmReadBackgroundScan(SceWlanBtRobinWlanTcm *tcm,
				 WlanScanResult *scanResult, unsigned int capacity);
int robinWlanTcmSetManagementInformationElements(SceWlanBtRobinWlanTcm *tcm,
					       const void *informationElements, size_t size);
int robinWlanTcmConfigureOutput(SceWlanBtRobinWlanTcm *tcm, int output,
			       unsigned int value);
int robinWlanTcmReadInput(SceWlanBtRobinWlanTcm *tcm, int input, unsigned int *value);
int robinWlanTcmSetEnergyDetectionMacOffset(SceWlanBtRobinWlanTcm *tcm, int16_t offset);
int robinWlanTcmDisableEnergyDetectionMac(SceWlanBtRobinWlanTcm *tcm);
int wlanCopyInformationElement(const void *buffer, unsigned int length, unsigned int id,
			       void *output, unsigned int *outputSize, unsigned int *offset);
int WlanVisitorIsIdle(const SceWlanVisitor *visitor);
int robinWlanPowerOn(void);
int robinWlanPowerOff(void);
int robinWlanRestart(void);
int robinWlanRequestStandby(void);
int robinWlanStartDcm(SceWlanBtRobinWlanDcm *dcm);
int robinWlanShutdownDcm(SceWlanBtRobinWlanDcm *dcm, int keepFunction);
int robinWlanRequestCardWake(SceWlanBtRobinWlanDcm *dcm);
int robinWlanDcmInterruptRequest(SceWlanBtRobinWlanDcm *dcm);
int robinWlanBeginSetup(SceWlanBtRobinWlanContext *context);
int robinWlanBeginInitialize(SceWlanBtRobinWlanContext *context);
int robinWlanBeginShutdown(SceWlanBtRobinWlanContext *context);
int robinWlanBeginWakeOffConfiguration(SceWlanBtRobinWlanContext *context);
int robinWlanNotifyCommandCancellation(SceWlanBtRobinWlanContext *context);
int robinWlanProcessTransmit(SceWlanBtRobinWlanContext *context);
int robinWlanTransmitCommand(SceWlanBtRobinWlanContext *context);
int robinWlanRequestRssiUpdate(SceWlanBtRobinWlanContext *context);
int robinWlanBeginSleep(SceWlanBtRobinWlanContext *context);
int robinWlanBeginPowerSaveEnable(SceWlanBtRobinWlanContext *context);
int robinWlanBeginPowerSaveDisable(SceWlanBtRobinWlanContext *context);
int wlanBtGetWlanWakeSource(void);
int wlanBtGetBluetoothWakeSource(void);
int robinWlanInitializeTcm(SceWlanBtRobinWlanContext *context);
int robinWlanInitializeDcm(SceWlanBtRobinWlanDcm *dcm);
int _debugDumpStatus(int resume, int eventId, void *arguments, void *context);

int robinWlanDriverClose(struct NetDriver *driver, int argument)
{
	(void)driver;
	(void)argument;
	if (!wlanBtIsAvailable())
		return 0x80411682;
	return sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x1000000);
}

int robinWlanDriverSetMulticastAddresses(struct NetDriver *driver,
				       const unsigned char *addresses, unsigned int size)
{
	SceWlanBtRobinWlanContext *context = driver->private;
	SceWlanVisitor *visitor = &context->visitor;
	unsigned int status;
	int result = WlanVisitorEnter(visitor);

	if (result < 0)
		return result;
	if (addresses) {
		if (size % sizeof(SceNetEtherAddr))
			goto invalid;
	} else if (size) {
		goto invalid;
	}
	status = context->status;
	result = 0x80411683;
	if (!(status & 0x400020)) {
		if (status & 8)
			result = robinWlanTcmSetMulticastAddresses(&context->tcm,
						(const void *)addresses, size);
		else
			result = context->intError;
	}
out:
	WlanVisitorLeave(visitor);
	return result;
invalid:
	result = 0x80418001;
	goto out;
}

int robinWlanDriverKickTx(struct NetDriver *driver, int argument)
{
	SceWlanBtRobinWlanContext *context = driver->private;
	SceWlanVisitor *visitor = &context->visitor;
	struct NetDriverPacket *packet;
	int result = WlanVisitorEnter(visitor);

	(void)argument;
	if (result < 0)
		return result;
	if ((context->status & 0x1008) == 0x1008) {
		if (__atomic_fetch_add(&context->transmitRequests, 1, __ATOMIC_RELEASE) == 0)
			result = sceKernelSetEventFlag(context->workerEventFlag, 128);
	} else {
		while ((packet = driver->dequeueTx(driver)) != NULL)
			driver->releaseTx(driver, packet);
		result = context->intError;
	}
	WlanVisitorLeave(visitor);
	return result;
}

int robinWlanDriverStart(struct NetDriver *driver, void *memoryBase)
{
	SceWlanBtRobinWlanContext *context = driver->private;
	SceWlanVisitor *visitor = &context->visitor;
	int result = WlanVisitorEnter(visitor);

	(void)memoryBase;
	if (result >= 0)
		WlanVisitorLeave(visitor);
	return result;
}

int robinWlanDriverStop(struct NetDriver *driver, void *memoryBase, int memorySize)
{
	SceWlanBtRobinWlanContext *context = driver->private;
	SceWlanVisitor *visitor = &context->visitor;
	int result = WlanVisitorEnter(visitor);

	(void)memoryBase;
	(void)memorySize;
	if (result >= 0)
		WlanVisitorLeave(visitor);
	return result;
}

int robinWlanDriverOpen(struct NetDriver *driver, int argument)
{
	SceWlanBtRobinWlanContext *context = driver->private;
	unsigned int status;
	int error;

	(void)argument;
	if (!wlanBtIsAvailable())
		return 0x80411682;
	lockFastMutex(&context->mutex);
	status = context->status;
	if ((status & 0x800000) || context->workPending) {
		context->driverNotifyPending = 1;
	} else if (status & 8) {
		sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x800000);
	} else {
		error = context->intError;
		context->driver.notify(&context->driver, NULL, NET_DRIVER_EVENT_STATUS,
				       NET_DRIVER_STATUS_ERROR, &error, sizeof(error));
	}
	sceKernelUnlockFastMutex(&context->mutex);
	return 0;
}

int _infraScanJoin(SceWlanBtRobinWlanTcm *tcm,
		   WlanConnectionConfig *configuration, unsigned int capacity)
{
	SceWlanBtRobinWlanContext *context = tcm->context;
	WlanScanConfig *scan;
	WlanScanResult *scanResult;
	WlanConnectionResult *output = (void *)configuration;
	const WlanBssDesc *descriptor, *selected;
	const uint8_t *informationElements;
	unsigned int count, channel, index, informationElementsSize, flags, ssidSize;
	int specificChannels = 1, secondPass = 0;
	int result;

	if (configuration->security.securityMode == 2) {
		if (context->securityPreference == 1)
			configuration->security.securityMode = 3;
		else if (context->securityPreference == 2)
			configuration->security.securityMode = 4;
	}
	scan = context->driver.allocate(&context->driver, 2048, 4);
	if (!scan)
		return 0x80418005;
	result = robinWlanTcmSetStationPowerSave(tcm, tcm->context->wakeEnabled != 0);
	if (result < 0)
		goto out;
	memset(scan, 0, sizeof(*scan));
prepareChannels:
	if (specificChannels) {
		count = 0;
		while (count < sizeof(configuration->channels) && configuration->channels[count]) {
			if (!wlanIsSupportedChannel(configuration->channels[count]))
				goto invalid;
			++count;
		}
		if (count) {
			memcpy(scan->channels, configuration->channels, count);
			goto prepareScan;
		}
	}
	{
		uint8_t channels[14] = { 0 };

		count = 0;
		for (channel = 1; channel != 15; ++channel)
			if (wlanIsSupportedChannel((uint8_t)channel))
				channels[count++] = channel;
		specificChannels = 0;
		memcpy(scan->channels, channels, count);
	}
prepareScan:
	scan->ssidCount = 1;
	ssidSize = configuration->ssid.size;
	scan->size = sizeof(*scan);
	scan->bufferSize = 2048;
	memcpy(scan->ssids[0].ssid, configuration->ssid.ssid, ssidSize);
	flags = scan->flags;
	ssidSize = configuration->ssid.size;
	scan->minScanTime = 100;
	scan->maxScanTime = 100;
	scan->bssType = 1;
	scan->flags = flags | 2;
	scan->ssids[0].size = ssidSize;
	scan->scanMode = 1;
	wlanEnableInformationElement(scan->informationElementMask, 45);
	wlanEnableInformationElement(scan->informationElementMask, 61);
	wlanEnableInformationElement(scan->informationElementMask, 48);
	wlanEnableInformationElement(scan->informationElementMask, 221);
	if (configuration->flags & 1) {
		count = configuration->scanInformationElementsSize;
		if (count > 1024)
			goto invalid;
		scan->vendorInformationElementsSize = count;
		scan->flags |= 1;
		memcpy(scan->vendorInformationElements, configuration + 1, count);
	}
	result = robinWlanTcmScan(tcm, scan, 2048);
	if (result < 0)
		goto out;
	scanResult = (void *)scan;
	if (!scanResult->bssNum) {
		if (!specificChannels) {
			result = 0x80411601;
			goto out;
		}
		memset(scan, 0, sizeof(*scan));
		specificChannels = 0;
		goto prepareChannels;
	}
	if ((uintptr_t)configuration + capacity < (uintptr_t)configuration + sizeof(*output))
		goto invalid;
tryCandidates:
	descriptor = (const void *)scanResult->bssDesc;
	selected = NULL;
	for (index = 0; index < scanResult->bssNum; ++index) {
		selected = descriptor;
		switch (configuration->security.securityMode) {
		case 0:
			if (!secondPass && (wlanBssHasPrivacy(descriptor) ||
					    wlanBssHasWpa(descriptor) || wlanBssHasRsn(descriptor)))
				goto rejected;
			break;
		case 1:
			if (!wlanBssHasPrivacy(descriptor))
				goto rejected;
			break;
		case 2:
			if (wlanBssHasWpa(descriptor))
				break;
			/* fall through */
		case 4:
			if (!wlanBssHasRsn(descriptor))
				goto rejected;
			break;
		case 3:
			if (!wlanBssHasWpa(descriptor))
				goto rejected;
			break;
		default:
			goto invalid;
		}
		informationElements = NULL;
		informationElementsSize = 0;
		if (configuration->flags & 2) {
			informationElementsSize = configuration->associationInformationElementsSize;
			if (informationElementsSize > 1024)
				goto invalid;
			informationElements = (const uint8_t *)(configuration + 1) +
				configuration->scanInformationElementsSize;
		}
		result = robinWlanTcmAssociate(tcm, descriptor, &configuration->security,
					      informationElements, informationElementsSize);
		if (!result)
			goto success;
		if (result == (int)0x80028005 && configuration->security.securityMode == 0 &&
		    (wlanBssHasPrivacy(descriptor) || wlanBssHasWpa(descriptor) ||
		     wlanBssHasRsn(descriptor))) {
rejected:
			result = 0x80411602;
		}
		descriptor = (const void *)((const uint8_t *)descriptor + descriptor->size);
	}
	if (result >= 0)
		goto success;
	if (configuration->security.securityMode == 0 && !secondPass) {
		secondPass = 1;
		goto tryCandidates;
	}
	goto out;
success:
	memcpy(&output->bss, selected, sizeof(output->bss));
out:
	tcm->context->driver.free(&tcm->context->driver, scan);
	return result;
invalid:
	result = 0x80418001;
	goto out;
}

int robinWlanRetryWakeOnConfiguration(SceWlanBtRobinWlanContext *context)
{
	uint64_t started, elapsed;
	int attempt = 0, result;

	ksceUsbServEtherDisable();
	started = sceKernelGetSystemTimeWide();
	do {
		result = _infraScanJoin(&context->tcm, &context->connection,
					context->connectionSize);
		elapsed = sceKernelGetSystemTimeWide() - started;
		if (result >= 0) {
			SceDebugForDriver_391B74B7("[SceWlanBt]: Set Wake On WLAN (MagicPacket) : OK (num=%d, %lldms)\n",
					 attempt, (long long)(elapsed / 1000));
			return robinWlanBeginWakeOnConfiguration(context);
		}
		if (elapsed > 10000000)
			break;
		++attempt;
	} while (attempt != 10);
	SceDebugForDriver_391B74B7("[SceWlanBt]: Set Wake On WLAN (MagicPacket) : NG (ret=0x%08x,num=%d, %lldms)\n",
			 result, attempt, (long long)(elapsed / 1000));
	__atomic_fetch_and(&context->status, ~0x80u, __ATOMIC_RELEASE);
	context->wakeFlags &= ~4u;
	return robinWlanBeginHostSleepReset(context);
}

int robinWlanDriverControl(struct NetDriver *driver, unsigned int command,
			  int *argument, unsigned int capacity)
{
	SceWlanBtRobinWlanContext *context = driver->private;
	SceWlanVisitor *visitor = &context->visitor;
	SceWlanBtRobinWlanTcm *tcm;
	int result = WlanVisitorEnter(visitor);

	if (result < 0)
		return result;
	if (!(command & 0x01100000)) {
		result = 0x80418006;
		goto out;
	}
	result = context->ioctlError;
	if (result)
		goto out;
	if (command != 0x40180005 && command != 0x60180005 && command != 0x40180003) {
		unsigned int status = context->status;

		if (status & 0x400020) {
			result = 0x80411683;
			goto out;
		}
		if (context->workPending || !(status & 8)) {
			result = context->intError;
			goto out;
		}
	}
	tcm = &context->tcm;
	switch (command) {
	case 0x40110001: {
		WlanSignalInfo *signal = (void *)argument;

		if (context->status & 0x400) {
			uint64_t time = sceKernelGetSystemTimeWide();
			uint64_t previous = context->signalTimestamp;

			if (time < previous || previous + 3000000 < time) {
				context->signalTimestamp = time;
				result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x400000);
				if (result < 0)
					goto out;
			}
		}
		__builtin_memcpy(&signal->rssi, &context->signal.rssi,
				 sizeof(signal->rssi) + sizeof(signal->signalQuality));
		break;
	}
	case 0x40120003: {
		WlanHostSleepPeerList *list = (void *)argument;
		unsigned int enabled = context->wakeReasonFlags & 1;
		unsigned int count = context->peerCount;

		list->enabled = enabled;
		list->peerCount = count;
		memcpy(list->peers, context->peers, sizeof(list->peers));
		break;
	}
	case 0x40120004: {
		WlanDiscoveredSsidList *list = (void *)argument;
		unsigned int index, count;

		if (!(context->wakeReasonFlags & 2)) {
			list->available = 0;
			break;
		}
		count = context->discoveredSsidCount;
		list->available = 1;
		list->ssidCount = count;
		for (index = 0; index < context->discoveredSsidCount; ++index) {
			list->ssids[index].size = context->discoveredSsids[index].size;
			memcpy(list->ssids[index].ssid, context->discoveredSsids[index].ssid,
			       context->discoveredSsids[index].size);
		}
		break;
	}
	case 0x40180002:
		result = robinWlanTcmReadSettings(tcm, argument);
		break;
	case 0x40180003:
		((WlanScalarControl *)argument)->value = (context->status & 0x200) != 0;
		break;
	case 0x40180004:
		result = robinWlanTcmGetCardInfo(tcm, argument);
		break;
	case 0x40180005:
		((WlanScalarControl *)argument)->value = context->securityPreference;
		break;
	case 0x50100001:
		result = robinWlanTcmScan(tcm, (void *)argument, capacity);
		break;
	case 0x50100002:
		result = robinWlanTcmStartBackgroundScan(tcm, (const void *)argument, capacity);
		break;
	case 0x50100003:
		result = robinWlanTcmStopBackgroundScan(tcm);
		break;
	case 0x50100004:
		result = robinWlanTcmReadBackgroundScan(tcm, (void *)argument, capacity);
		break;
	case 0x50110001:
		memcpy(&context->connection, argument, sizeof(context->connection));
		context->connectionSize = capacity;
		result = _infraScanJoin(tcm, (void *)argument, capacity);
		if (result < 0)
			context->connectionSize = 0;
		else if (SceSblAIMgrForDriver_71608CA3())
			ksceUsbServEtherDisable();
		break;
	case 0x50110002:
		result = robinWlanTcmDisconnect(tcm);
		if (result >= 0)
			ksceUsbServEtherEnable();
		break;
	case 0x50120001:
		result = robinWlanTcmJoinAdhoc(tcm, (void *)argument, capacity);
		if (result >= 0)
			ksceUsbServEtherDisable();
		break;
	case 0x50120002:
		result = robinWlanTcmStopAdhoc(tcm);
		if (result >= 0)
			ksceUsbServEtherEnable();
		break;
	case 0x50120003:
		result = robinWlanTcmStartAdhoc(tcm, (void *)argument, capacity);
		if (result >= 0)
			ksceUsbServEtherDisable();
		break;
	case 0x50120004: {
		WlanConnectionConfig *configuration = (void *)argument;
		WlanConnectionResult *output = (void *)argument;
		WlanScanConfig *scan;
		WlanScanResult *scanResult;
		const WlanBssDesc *descriptor, *selected;
		WlanBssConnectionConfig *connection;
		WlanInformationElement *element;
		unsigned int count, channel, index, flags, ssidSize;
		unsigned int outputSize, offset, connectionSize;
		int specificChannels = 1;

		scan = tcm->context->driver.allocate(&tcm->context->driver, 2048, 4);
		if (!scan) {
			result = 0x80418005;
			break;
		}
prepareChannels:
		memset(scan, 0, sizeof(*scan));
		if (specificChannels) {
			count = 0;
			while (count < sizeof(configuration->channels) && configuration->channels[count]) {
				if (!wlanIsSupportedChannel(configuration->channels[count]))
					goto invalidAdhoc;
				++count;
			}
			if (count) {
				memcpy(scan->channels, configuration->channels, count);
				goto prepareScan;
			}
		}
		{
			uint8_t channels[14] = { 0 };

			count = 0;
			for (channel = 1; channel != 15; ++channel)
				if (wlanIsSupportedChannel((uint8_t)channel))
					channels[count++] = channel;
			specificChannels = 0;
			memcpy(scan->channels, channels, count);
		}
prepareScan:
		scan->ssidCount = 1;
		scan->size = sizeof(*scan);
		scan->bufferSize = 2048;
		memcpy(scan->ssids[0].ssid, configuration->ssid.ssid, configuration->ssid.size);
		flags = scan->flags;
		ssidSize = configuration->ssid.size;
		scan->scanMode = 1;
		scan->bssType = 2;
		scan->minScanTime = 100;
		scan->maxScanTime = 100;
		scan->flags = flags | 2;
		scan->ssids[0].size = ssidSize;
		wlanEnableInformationElement(scan->informationElementMask, 45);
		wlanEnableInformationElement(scan->informationElementMask, 61);
		wlanEnableInformationElement(scan->informationElementMask, 48);
		wlanEnableInformationElement(scan->informationElementMask, 221);
		result = robinWlanTcmScan(tcm, scan, 2048);
		if (result < 0)
			goto freeScan;
		scanResult = (void *)scan;
		if (!scanResult->bssNum) {
			if (!specificChannels) {
				result = 0x80411601;
				goto freeScan;
			}
			specificChannels = 0;
			goto prepareChannels;
		}
		if ((uintptr_t)argument + capacity < (uintptr_t)argument + sizeof(*output))
			goto invalidAdhoc;
		descriptor = (const void *)scanResult->bssDesc;
		selected = descriptor;
		for (index = 0; index < scanResult->bssNum; ++index) {
			selected = descriptor;
			switch (configuration->security.securityMode) {
			case 0:
				break;
			case 1:
				if (!wlanBssHasPrivacy(descriptor))
					goto rejected;
				break;
			case 2:
				if (wlanBssHasWpa(descriptor))
					break;
				/* fall through */
			case 4:
				if (!wlanBssHasRsn(descriptor))
					goto rejected;
				break;
			case 3:
				if (!wlanBssHasWpa(descriptor))
					goto rejected;
				break;
			default:
				goto invalidAdhoc;
			}
			connection = tcm->context->driver.allocate(&tcm->context->driver, 328, 4);
			if (!connection) {
				result = 0x80418005;
				goto freeScan;
			}
			memset(connection, 0, sizeof(*connection));
			flags = configuration->flags;
			ssidSize = descriptor->ssidSize;
			if (flags & 1)
				connection->flags |= 1;
			if (flags & 2)
				connection->flags |= 2;
			connection->bss.ssidSize = ssidSize;
			connection->security.authenticationMode = configuration->security.authenticationMode;
			connection->security.securityMode = configuration->security.securityMode;
			memcpy(connection->bss.ssid, descriptor->ssid, descriptor->ssidSize);
			connection->bss.size = sizeof(connection->bss);
			__builtin_memcpy(&connection->bss.bssid, &descriptor->bssid,
					 sizeof(connection->bss.bssid));
			connection->bss.bssMode = 2;
			connection->bss.channel = descriptor->channel;
			connection->bss.atimWindow = descriptor->atimWindow;
			connection->bss.capabilities = descriptor->capabilities;
			connection->bss.beaconInterval = descriptor->beaconInterval;
			__builtin_memcpy(connection->bss.supportedRates, descriptor->supportedRates,
					 sizeof(connection->bss.supportedRates));
			__builtin_memcpy(connection->bss.extendedRates, descriptor->extendedRates,
					 sizeof(connection->bss.extendedRates));
			offset = 0;
			connection->bss.informationElementsSize = 0;
			outputSize = 26;
			element = (void *)connection->bss.informationElements;
			connectionSize = sizeof(*connection);
			if (!wlanCopyInformationElement(descriptor->informationElements,
						 descriptor->informationElementsSize, 45,
						 element->data, &outputSize, &offset)) {
				element->id = 45;
				element->length = outputSize;
				connection->bss.informationElementsSize += (uint16_t)(outputSize + 2);
				connection->bss.size += (uint16_t)(outputSize + 2);
				connectionSize = sizeof(*connection) + outputSize + 2;
			}
			element = (void *)(connection->bss.informationElements +
					   connection->bss.informationElementsSize);
			offset = 0;
			outputSize = 22;
			if (!wlanCopyInformationElement(descriptor->informationElements,
						 descriptor->informationElementsSize, 61,
						 element->data, &outputSize, &offset)) {
				element->id = 61;
				element->length = outputSize;
				connection->bss.informationElementsSize += (uint16_t)(outputSize + 2);
				connection->bss.size += (uint16_t)(outputSize + 2);
				connectionSize += outputSize + 2;
			}
			result = robinWlanTcmJoinAdhoc(tcm, connection, connectionSize);
			tcm->context->driver.free(&tcm->context->driver, connection);
			if (!result)
				goto adhocSuccess;
			goto nextCandidate;
rejected:
			result = 0x80411602;
nextCandidate:
			descriptor = (const void *)((const uint8_t *)descriptor + descriptor->size);
		}
		if (result < 0)
			goto freeScan;
adhocSuccess:
		memcpy(&output->bss, selected, sizeof(output->bss));
		tcm->context->driver.free(&tcm->context->driver, scan);
		ksceUsbServEtherDisable();
		break;
invalidAdhoc:
		result = 0x80418001;
freeScan:
		tcm->context->driver.free(&tcm->context->driver, scan);
		break;
	}
	case 0x50180001: {
		WlanGpioControl *request = (void *)argument;
		unsigned int value;

		if (request->pin != 4 && (unsigned int)(request->pin - 15) > 1)
			goto unsupported;
		if (!request->operation) {
			result = robinWlanTcmReadInput(tcm, request->pin, &value);
			if (result >= 0)
				request->value = value;
		} else if (request->operation == 1) {
			value = request->value;
			if (value > 1)
				goto invalid;
			result = robinWlanTcmConfigureOutput(tcm, request->pin, value);
		} else {
			goto invalid;
		}
		break;
	}
	case 0x50180002: {
		WlanEnergyDetectionControl *request = (void *)argument;

		if (!request->operation)
			result = robinWlanTcmDisableEnergyDetectionMac(tcm);
		else if (request->operation == 1)
			result = robinWlanTcmSetEnergyDetectionMacOffset(tcm, (int16_t)request->offset);
		else
			goto invalid;
		break;
	}
	case 0x60120002: {
		WlanManagementInformationElements *request = (void *)argument;
		unsigned int size = request->informationElementsSize;
		WlanInformationElement *element = (void *)request->informationElements;

		if (capacity != size + sizeof(*request) || size - 1 > 255 ||
		    element->length != size - 2)
			goto invalid;
		result = robinWlanTcmSetManagementInformationElements(tcm, element, size);
		break;
	}
	case 0x60120003: {
		WlanWakeNetworkRequest *request = (void *)argument;
		const WlanWakeNetworkConfig *configuration;

		if (request) {
			configuration = &request->configuration;
			if (configuration->payloadSize > sizeof(context->wakePayload) ||
			    capacity != sizeof(*configuration) + configuration->payloadSize ||
			    (configuration->hostSleepDataSize &&
			     (unsigned int)(configuration->hostSleepDataSize - 4) > 2) ||
			    configuration->ssid.size > 32 || configuration->secondarySsid.size > 32)
				goto invalid;
			memcpy(&context->wakeNetwork, configuration, sizeof(*configuration));
			memcpy(context->wakePayload, request->payload, configuration->payloadSize);
			context->wakePayloadSize = configuration->payloadSize;
			context->wakeFlags |= 1;
			__atomic_fetch_or(&context->status, 0x80, __ATOMIC_RELEASE);
		} else {
			if (capacity)
				goto invalid;
			memset(&context->wakeNetwork, 0, sizeof(context->wakeNetwork));
			context->wakeFlags &= ~1;
			if (!context->wakeFlags)
				__atomic_fetch_and(&context->status, ~0x80, __ATOMIC_RELEASE);
		}
		break;
	}
	case 0x60120004: {
		WlanWakeAdhocConfig *configuration = (void *)argument;

		if (configuration) {
			if (capacity != sizeof(*configuration) || configuration->ssid.size > 32 ||
			    configuration->secondarySsid.size > 32)
				goto invalid;
			memcpy(&context->wakeAdhoc, configuration, sizeof(*configuration));
			context->wakeFlags |= 2;
			__atomic_fetch_or(&context->status, 0x80, __ATOMIC_RELEASE);
		} else {
			if (capacity)
				goto invalid;
			memset(&context->wakeAdhoc, 0, sizeof(context->wakeAdhoc));
			context->wakeFlags &= ~2;
			if (!context->wakeFlags)
				__atomic_fetch_and(&context->status, ~0x80, __ATOMIC_RELEASE);
		}
		break;
	}
	case 0x60180001:
		result = robinWlanTcmSetTransmitRate(tcm, argument);
		break;
	case 0x60180002:
		result = robinWlanTcmWriteSettings(tcm, argument);
		break;
	case 0x60180003:
		memcpy(&context->wakeScan, argument, sizeof(context->wakeScan));
		__atomic_fetch_or(&context->status, 0x80, __ATOMIC_RELEASE);
		context->wakeOwnerProcess = sceKernelGetProcessId();
		context->wakeFlags |= 0x80000000;
		break;
	case 0x60180005:
		context->securityPreference = ((WlanScalarControl *)argument)->value;
		break;
	default:
unsupported:
		result = 0x80418008;
		break;
	}
out:
	WlanVisitorLeave(visitor);
	return result;
invalid:
	result = 0x80418001;
	goto out;
}

void robinWlanUpdateConfiguration(SceWlanBtRobinWlanContext *context)
{
	unsigned int wakeRestoreMode = context->wakeRestoreMode;
	unsigned int status = context->status;
	unsigned int desired = context->desiredConfiguration;
	unsigned int notification;
	int error;

	if (!wakeRestoreMode) {
		if (status & 8) {
			if (!(desired & 1)) {
				error = 0x80411682;
				goto stop;
			}
			goto reconcilePowerSave;
		}
		if (desired & 1) {
			context->workPending = 1;
			sceKernelSetEventFlag(context->workerEventFlag, 4);
			desired = context->desiredConfiguration;
			goto reconcile;
		}
		error = 0x80411682;
		goto close;
	}
	if (status & 8) {
		if (desired & 2) {
			error = 0x80411681;
			goto stop;
		}
		context->wakeRestoreMode = 0;
		goto reconcilePowerSave;
	}
	if (desired & 2) {
		error = 0x80411681;
		goto close;
	}
	if (wakeRestoreMode == 1) {
		context->intError = 0x80411682;
		WlanVisitorClose(&context->visitor, 0x80411682);
		desired = context->desiredConfiguration;
	} else if (wakeRestoreMode == 2) {
		context->workPending = 1;
		sceKernelSetEventFlag(context->workerEventFlag, 4);
		desired = context->desiredConfiguration;
	}
	context->wakeRestoreMode = 0;
	goto reconcile;
stop:
	context->workPending = 1;
	context->intError = error;
	WlanVisitorClose(&context->visitor, error);
	sceKernelSetEventFlag(context->workerEventFlag, 16);
	desired = context->desiredConfiguration;
	goto reconcile;
close:
	context->intError = error;
	WlanVisitorClose(&context->visitor, error);
	desired = context->desiredConfiguration;
	goto reconcile;
reconcilePowerSave:
	if (context->currentConfiguration & 0x100) {
		if (!(desired & 0x100)) {
			context->workPending = 1;
			sceKernelSetEventFlag(context->workerEventFlag, 0x100000);
			desired = context->desiredConfiguration;
		}
	} else if (desired & 0x100) {
		context->workPending = 1;
		sceKernelSetEventFlag(context->workerEventFlag, 0x40000);
		desired = context->desiredConfiguration;
	}
reconcile:
	if (context->workPending) {
		context->nextConfiguration = desired;
		return;
	}
	status = context->status;
	context->currentConfiguration = desired;
	if (status & 8) {
		desired |= 8;
		context->currentConfiguration = desired;
	}
	if (status & 0x800000) {
		gWlanContext.currentStateMarker = 0x8b000000;
		__atomic_fetch_and(&context->status, ~0x800020, __ATOMIC_RELEASE);
	} else {
		notification = (desired & ~1) | (context->nextConfiguration & 1);
		if (context->status & 0x400020)
			notification |= 4;
		if ((int)status < 0)
			notification |= 0x40;
		if (context->shutdownState) {
			notification |= 0x200;
			if (context->shutdownState == 1)
				notification |= 1;
		}
		if (context->interfaceStopRequested)
			notification |= 0x400;
		if (context->wakeEnabled)
			notification |= 0x100;
		wlanNotifyCallbacks(notification);
	}
	if (context->driverNotifyPending) {
		if (context->status & 8) {
			sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x800000);
		} else {
			error = context->intError;
			context->driver.notify(&context->driver, NULL, NET_DRIVER_EVENT_STATUS,
					       NET_DRIVER_STATUS_ERROR, &error, sizeof(error));
		}
		context->driverNotifyPending = 0;
	}
}

int robinWlanReloadConfiguration(SceWlanBtRobinWlanContext *context)
{
	unsigned int desired;
	int value, wifiConfigured;

	lockFastMutex(&context->mutex);
	context->desiredConfiguration = 0;
	context->wakeRestoreMode = 0;
	if (wlanBtIsAvailable()) {
		if (sceRegMgrGetKeyInt("/CONFIG/NET", "wifi_flag", &value) >= 0 && value)
			context->desiredConfiguration |= 1;
		if (sceRegMgrGetKeyInt("/CONFIG/NET", "wifi_config", &value) >= 0)
			wifiConfigured = value != 0;
		else
			wifiConfigured = 0;
		if (sceRegMgrGetKeyInt("/CONFIG/NET", "wifi_power_save", &value) < 0 || value) {
			desired = context->desiredConfiguration | 0x100;
			context->desiredConfiguration = desired;
		} else {
			desired = context->desiredConfiguration;
		}
		context->wakeEnabled = (desired & 0x100) != 0;
		if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "flight_mode", &value) >= 0 && value) {
			desired = context->desiredConfiguration;
			context->desiredConfiguration = desired | 2;
			if ((desired & 1) && wifiConfigured) {
				context->wakeRestoreMode = 2;
				context->desiredConfiguration = (desired & ~1) | 2;
			}
		}
		if (!context->workPending)
			robinWlanUpdateConfiguration(context);
	} else {
		context->currentConfiguration = context->desiredConfiguration;
	}
	sceKernelUnlockFastMutex(&context->mutex);
	return 0;
}

int robinWlanCompleteConfiguration(SceWlanBtRobinWlanContext *context, int operation)
{
	unsigned int status, next;
	int error, request;

	lockFastMutex(&context->mutex);
	status = context->status;
	next = context->nextConfiguration;
	context->workPending = 0;
	context->currentConfiguration = next;
	if (status & 8) {
		context->intError = 0;
		if (operation == 1) {
			WlanVisitorInitialize(&context->visitor);
			if (context->workPending)
				goto unlock;
		}
	} else {
		error = context->intError;
		if (error == (int)0x80411681) {
			context->driver.notify(&context->driver, NULL, 0x100000, 0, NULL, 0);
			if (context->workPending)
				goto unlock;
		} else if (error == (int)0x80411682 && !(status & 0x400000)) {
			context->driver.notify(&context->driver, NULL, 0x200000, 0, NULL, 0);
			if (context->workPending)
				goto unlock;
		}
	}
	robinWlanUpdateConfiguration(context);
unlock:
	sceKernelUnlockFastMutex(&context->mutex);
	if (!(context->status & 0x20)) {
		request = context->deferredConfigurationRequest;
		if (request) {
			wlanSetConfiguration(request);
			context->deferredConfigurationRequest = 0;
		}
	}
	return 0;
}

int _workerEntry(SceSize args, void *argument)
{
	SceWlanBtRobinWlanContext *context;
	unsigned int events, status, flags, count, index;
	unsigned int ssidSize;
	int result = 0, matched, secondaryMatched;

	if (args != sizeof(context))
		return 0;
	context = *(SceWlanBtRobinWlanContext **)argument;
	for (;;) {
		events = 0;
		result = sceKernelWaitEventFlag(context->workerEventFlag, ~0u, 5, &events, NULL);
		if (result < 0)
			goto failed;
		if (events & 1) {
			__atomic_fetch_or(&context->status, 0x10000, __ATOMIC_RELEASE);
			result = robinWlanPowerOn();
			if (result < 0)
				goto failed;
			__atomic_fetch_or(&context->status, 2, __ATOMIC_RELEASE);
			__atomic_fetch_or(&context->status, 1, __ATOMIC_RELEASE);
			result = robinWlanStartDcm(&context->dcm);
			if (result < 0)
				goto failed;
			result = robinWlanBeginSetup(context);
			if (result < 0)
				goto failed;
		}
		if (events & 2) {
			__atomic_fetch_and(&context->status, ~0x10000, __ATOMIC_RELEASE);
			WlanVisitorInitialize(&context->visitor);
			context->driver.control = robinWlanDriverControl;
			context->driver.stop = robinWlanDriverStop;
			context->driver.start = robinWlanDriverStart;
			context->driver.multicast = robinWlanDriverSetMulticastAddresses;
			context->driver.type = 2;
			context->driver.open = robinWlanDriverOpen;
			context->driver.prefix = context->dcm.packetHeadroom;
			context->driver.close = robinWlanDriverClose;
			context->driver.kickTx = robinWlanDriverKickTx;
			context->driver.mtu = 1500;
			context->driver.private = context;
			context->driver.capabilities = 0xe0000;
			__builtin_memcpy(&context->driver.mac, &context->address,
					 sizeof(context->driver.mac));
			result = SceNetPsForDriver_1ABF937D(&context->driver);
			if (result < 0)
				goto failed;
			__atomic_fetch_or(&context->status, 4, __ATOMIC_RELEASE);
			result = robinWlanShutdownDcm(&context->dcm, 0);
			if (result < 0)
				goto failed;
			result = robinWlanPowerOff();
			if (result < 0)
				goto failed;
			__atomic_fetch_and(&context->status, ~2, __ATOMIC_RELEASE);
			if (!wlanBtIsAvailable())
				break;
			result = robinWlanReloadConfiguration(context);
			if (result < 0)
				goto failed;
		}
		if (events & 4) {
			__atomic_fetch_or(&context->status, 0x20000, __ATOMIC_RELEASE);
			if (robinWlanPowerOn() >= 0) {
				__atomic_fetch_or(&context->status, 2, __ATOMIC_RELEASE);
				result = robinWlanStartDcm(&context->dcm);
				if (result < 0)
					goto failed;
				result = robinWlanRequestCardWake(&context->dcm);
				if (result < 0)
					goto failed;
				result = robinWlanBeginInitialize(context);
				if (result < 0)
					goto failed;
			} else {
				__atomic_fetch_or(&context->status, 0x80000000, __ATOMIC_RELEASE);
				gWlanContext.ioctlError = 0x804116c0;
			}
		}
		if (events & 8) {
			__atomic_fetch_and(&context->status, ~0x20000, __ATOMIC_RELEASE);
			status = context->status;
			if (!(status & 0x40)) {
				context->transmitRequests = 0;
				__atomic_fetch_or(&context->status, 8, __ATOMIC_RELEASE);
				result = robinWlanCompleteConfiguration(context, 1);
			} else if (!(status & 0x80)) {
				gWlanContext.pendingStateMarker = 0x80000b00;
				result = robinWlanBeginHostSleepReset(context);
			} else {
				flags = context->wakeFlags;
				gWlanContext.pendingStateMarker = 0x80000a00;
				if ((flags & 4) && context->connectionSize) {
					sceKernelScheduleWorkTask(65571, "SceWlanBtRobinWlanWakeConfig",
						(SceKernelWorkQueueWorkFunction)robinWlanRetryWakeOnConfiguration,
						context);
					goto stopEvent;
				}
				result = robinWlanBeginWakeOnConfiguration(context);
			}
			if (result < 0)
				goto failed;
		}
stopEvent:
		if (events & 16) {
			if (!WlanVisitorIsIdle(&context->visitor)) {
				robinWlanNotifyCommandCancellation(context);
				result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 16);
				if (result < 0)
					goto failed;
				sceKernelDelayThread(10000);
			} else {
				__atomic_fetch_or(&context->status, 0x40000, __ATOMIC_RELEASE);
				__atomic_fetch_and(&context->status, ~8, __ATOMIC_RELEASE);
				if (!(context->status & 0x400000)) {
					result = robinWlanBeginShutdown(context);
					if (result < 0)
						goto failed;
				} else {
					sceKernelSetEventFlag(gWlanContext.workerEventFlag, 32);
				}
			}
		}
		if (events & 32) {
			__atomic_fetch_and(&context->status, ~0x140000, __ATOMIC_RELEASE);
			__atomic_fetch_and(&gWlanContext.status, ~0xc00, __ATOMIC_RELEASE);
			memset(&gWlanContext.bss, 0,
			       offsetof(SceWlanBtRobinWlanContext, wakeScan) -
			       offsetof(SceWlanBtRobinWlanContext, bss));
			result = robinWlanShutdownDcm(&context->dcm, 0);
			if (result < 0)
				goto failed;
			result = robinWlanPowerOff();
			if (result < 0)
				goto failed;
			__atomic_fetch_and(&context->status, ~2, __ATOMIC_RELEASE);
			ksceUsbServEtherEnable();
			result = robinWlanCompleteConfiguration(context, 2);
			if (result < 0)
				goto failed;
		}
		if (events & 0x200) {
			gWlanContext.pendingStateMarker = 0x80000600;
			if (context->status & 0x70000) {
				result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x200);
				sceKernelDelayThread(1000);
				if (result < 0)
					goto failed;
			} else {
				__atomic_fetch_and(&context->status, ~0x180010, __ATOMIC_RELEASE);
				__atomic_fetch_or(&context->status, 0x40, __ATOMIC_RELEASE);
				gWlanContext.pendingStateMarker = 0x80000700;
				if (robinWlanRestart() >= 0) {
					__atomic_fetch_or(&context->status, 2, __ATOMIC_RELEASE);
					gWlanContext.pendingStateMarker = 0x80000800;
					result = robinWlanStartDcm(&context->dcm);
					if (result < 0)
						goto failed;
					if (robinWlanRequestStandby() < 0) {
						__atomic_fetch_or(&context->status, 0x80000000, __ATOMIC_RELEASE);
						gWlanContext.ioctlError = 0x804116c0;
					}
					gWlanContext.pendingStateMarker = 0x80000900;
					result = robinWlanBeginInitialize(context);
					if (result < 0)
						goto failed;
				} else {
					__atomic_fetch_or(&context->status, 0x80000000, __ATOMIC_RELEASE);
					gWlanContext.ioctlError = 0x804116c0;
				}
			}
		}
		if (events & 0x400) {
			if (context->status & 0x10) {
				gWlanContext.pendingStateMarker = 0x80000f00;
				result = robinWlanShutdownDcm(&context->dcm, 1);
				if (result < 0)
					goto failed;
				result = ksceWlanBtSetConfiguration(13);
				if (result < 0)
					goto failed;
				__atomic_fetch_or(&context->status, 0x120, __ATOMIC_RELEASE);
				gWlanContext.pendingStateMarker = 0x80001000;
			} else {
				gWlanContext.pendingStateMarker = 0x80000e00;
				result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x400);
				sceKernelDelayThread(1000);
				if (result < 0)
					goto failed;
			}
		}
		if (events & 0x800) {
			if (context->status & 0x10) {
				gWlanContext.pendingStateMarker = 0x80001200;
				result = robinWlanShutdownDcm(&context->dcm, 1);
				if (result < 0)
					goto failed;
				gWlanContext.pendingStateMarker = 0x80001300;
				result = robinWlanPowerOff();
				if (result < 0)
					goto failed;
				__atomic_fetch_and(&context->status, ~2, __ATOMIC_RELEASE);
				result = ksceWlanBtSetConfiguration(14);
				if (result < 0)
					goto failed;
				__atomic_fetch_and(&context->status, ~0x100, __ATOMIC_RELEASE);
				__atomic_fetch_or(&context->status, 0x20, __ATOMIC_RELEASE);
				__atomic_fetch_and(&context->status, ~0x10, __ATOMIC_RELEASE);
				gWlanContext.pendingStateMarker = 0x80001400;
			} else {
				gWlanContext.pendingStateMarker = 0x80001100;
				result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x800);
				sceKernelDelayThread(1000);
				if (result < 0)
					goto failed;
			}
		}
		if (events & 0x100) {
			result = robinWlanTransmitCommand(context);
			if (result == (int)0x80418004) {
				result = sceKernelSetEventFlag(context->workerEventFlag, 0x10000);
				context->commandDeferred = 1;
				if (result < 0)
					goto failed;
			} else {
				if (result < 0)
					goto failed;
				context->commandDeferred = 0;
			}
		}
		if (events & 0x1000) {
			gWlanContext.currentStateMarker = 0x83000000;
			__atomic_fetch_or(&context->status, 0x200000, __ATOMIC_RELEASE);
			memset(&context->wakeReasonFlags, 0,
			       offsetof(SceWlanBtRobinWlanContext, wakeOwnerProcess) -
			       offsetof(SceWlanBtRobinWlanContext, wakeReasonFlags));
			if (robinWlanPowerOn() < 0) {
				__atomic_fetch_or(&context->status, 0x80000000, __ATOMIC_RELEASE);
				context->wakeFlags = 0;
				gWlanContext.ioctlError = 0x804116c0;
				__atomic_fetch_and(&context->status, ~0x200000, __ATOMIC_RELEASE);
				goto wakeResult;
			}
			__atomic_fetch_or(&context->status, 2, __ATOMIC_RELEASE);
			gWlanContext.currentStateMarker = 0x84000000;
			result = robinWlanStartDcm(&context->dcm);
			if (result < 0)
				goto failed;
			if ((ksceWlanBtGetConfiguration() & 0x100) || wlanBtGetWlanWakeSource() ||
			    wlanBtGetBluetoothWakeSource()) {
				result = robinWlanRequestCardWake(&context->dcm);
				if (result < 0)
					goto failed;
				__atomic_fetch_and(&context->status, ~0x10, __ATOMIC_RELEASE);
			}
			gWlanContext.currentStateMarker = 0x85000000;
			if (!(ksceWlanBtGetConfiguration() & 0x100)) {
				result = robinWlanBeginWakeOffConfiguration(context);
				if (result < 0)
					goto failed;
			} else {
				__atomic_fetch_and(&gWlanContext.status, ~0xc00, __ATOMIC_RELEASE);
				memset(&gWlanContext.bss, 0,
				       offsetof(SceWlanBtRobinWlanContext, wakeScan) -
				       offsetof(SceWlanBtRobinWlanContext, bss));
				sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x2000);
			}
		}
wakeResult:
		if (events & 0x2000) {
			gWlanContext.currentStateMarker = 0x87000000;
			if (context->peerCount > 1) {
				context->wakeReasonFlags |= 1;
				context->firstNetworkId = context->wakeNetwork.networkId;
			} else {
				if (context->discoveredSsidCount) {
					ssidSize = context->wakeNetwork.secondarySsid.size;
					count = gWlanContext.discoveredSsidCount;
					matched = 0;
					if (ssidSize && count)
						for (index = 0; index < 10 && index < count; ++index)
							if (!memcmp(gWlanContext.discoveredSsids[index].ssid,
								    context->wakeNetwork.secondarySsid.ssid, ssidSize)) {
								matched = 1;
								break;
							}
					if (matched) {
						context->wakeReasonFlags |= 1;
						context->firstNetworkId = context->wakeNetwork.networkId;
					} else {
						ssidSize = context->wakeAdhoc.secondarySsid.size;
						count = gWlanContext.discoveredSsidCount;
						matched = 0;
						if (ssidSize && count)
							for (index = 0; index < 10 && index < count; ++index)
								if (!memcmp(gWlanContext.discoveredSsids[index].ssid,
									    context->wakeAdhoc.secondarySsid.ssid, ssidSize)) {
									matched = 1;
									break;
								}
						secondaryMatched = matched;
						if (!matched) {
							ssidSize = context->wakeScan.ssids[0].size;
							count = gWlanContext.discoveredSsidCount;
							if (ssidSize && count)
								for (index = 0; index < 10 && index < count; ++index)
									if (!memcmp(gWlanContext.discoveredSsids[index].ssid,
										    context->wakeScan.ssids[0].ssid, ssidSize)) {
										matched = 1;
										break;
									}
						}
						if (secondaryMatched || !matched) {
							context->wakeReasonFlags |= 2;
							context->secondNetworkId = context->wakeAdhoc.networkId;
						}
					}
				} else {
					SceSblAIMgrForDriver_71608CA3();
				}
				context->wakeFlags = 0;
			}
			__atomic_fetch_or(&context->status, 0x200, __ATOMIC_RELEASE);
			if (context->status & 0x200)
				__atomic_fetch_and(&context->status, ~0x80, __ATOMIC_RELEASE);
			__atomic_fetch_and(&context->status, ~0x200000, __ATOMIC_RELEASE);
			gWlanContext.currentStateMarker = 0x88000000;
			result = robinWlanShutdownDcm(&context->dcm, 0);
			if (result < 0)
				goto failed;
			gWlanContext.currentStateMarker = 0x89000000;
			result = robinWlanPowerOff();
			if (result < 0)
				goto failed;
			__atomic_fetch_and(&context->status, ~2, __ATOMIC_RELEASE);
			gWlanContext.currentStateMarker = 0x8a000000;
			result = robinWlanReloadConfiguration(context);
			if (result < 0)
				goto failed;
		}
		if ((events & 0x40) && (context->status & 2) && (context->status & 0x60) != 0x60)
			robinWlanDcmInterruptRequest(&context->dcm);
		if ((events & 0x80) && (context->status & 8))
			robinWlanProcessTransmit(context);
		if ((events & 0x400000) && (context->status & 0x400008) == 8)
			robinWlanRequestRssiUpdate(context);
		if (events & 0x4000) {
			status = context->status;
			if (!(status & 0x40020)) {
				if ((events & ~0xc00) == 0x4000) {
					result = robinWlanBeginSleep(context);
					if (result < 0)
						goto failed;
				} else if (status & 0x48) {
					result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x4000);
					if (result < 0)
						goto failed;
				}
			}
		}
		if (events & 0x8000) {
			__atomic_fetch_and(&context->status, ~0x80000, __ATOMIC_RELEASE);
			context->status |= 0x10;
		}
		if ((events & 0x10000) && (context->status & 0x60) != 0x60) {
			__atomic_fetch_or(&context->status, 0x100000, __ATOMIC_RELEASE);
			result = robinWlanRequestCardWake(&context->dcm);
			if (result < 0)
				goto failed;
		}
		if (events & 0x20000) {
			__atomic_fetch_and(&context->status, ~0x100010, __ATOMIC_RELEASE);
			if (context->transmitDeferred) {
				result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 128);
				if (result < 0)
					goto failed;
			}
			if (context->commandDeferred) {
				result = sceKernelSetEventFlag(gWlanContext.workerEventFlag, 256);
				if (result < 0)
					goto failed;
			}
		}
		if (events & 0x40000) {
			if (context->status & 0x400)
				result = robinWlanBeginPowerSaveEnable(context);
			else
				result = robinWlanCompleteConfiguration(context, 3);
			if (result < 0)
				goto failed;
		}
		if (events & 0x80000) {
			result = robinWlanCompleteConfiguration(context, 3);
			if (result < 0)
				goto failed;
		}
		if (events & 0x100000) {
			if (context->status & 0x400)
				result = robinWlanBeginPowerSaveDisable(context);
			else
				result = robinWlanCompleteConfiguration(context, 4);
			if (result < 0)
				goto failed;
		}
		if (events & 0x200000) {
			result = robinWlanCompleteConfiguration(context, 4);
			if (result < 0)
				goto failed;
		}
		if (events & 0x800000) {
			__atomic_fetch_or(&context->status, 0x1000, __ATOMIC_RELEASE);
			context->driver.notify(&context->driver, NULL, NET_DRIVER_EVENT_STATUS,
					       NET_DRIVER_STATUS_STARTED, NULL, 0);
		}
		if (events & 0x1000000) {
			__atomic_fetch_and(&context->status, ~0x1000, __ATOMIC_RELEASE);
			context->driver.notify(&context->driver, NULL, NET_DRIVER_EVENT_STATUS,
					       NET_DRIVER_STATUS_STOPPED, NULL, 0);
		}
		if (events & 0x2000000) {
			if (context->status & 2)
				robinWlanShutdownDcm(&context->dcm, 0);
			ksceWlanBtSetConfiguration(17);
			__atomic_fetch_and(&context->status, 0xffc0ec25, __ATOMIC_RELEASE);
			__atomic_fetch_and(&gWlanContext.status, ~0xc00, __ATOMIC_RELEASE);
			memset(&gWlanContext.bss, 0,
			       offsetof(SceWlanBtRobinWlanContext, wakeScan) -
			       offsetof(SceWlanBtRobinWlanContext, bss));
			__atomic_fetch_or(&context->status, 0x20, __ATOMIC_RELEASE);
		}
	}
	return result;
failed:
	gWlanContext.ioctlError = 0x804116c1;
	return result;
}

int robinInitializeWlanContext(void)
{
	SceWlanBtRobinWlanContext *context = &gWlanContext;
	int result;

	memset(&gWlanContext, 0, sizeof(gWlanContext));
	gWlanContext.dcm.context = &gWlanContext;
	gWlanContext.tcm.context = &gWlanContext;
	gWlanContext.tcm.contextAlias = &gWlanContext;
	result = sceKernelInitializeFastMutex(&context->mutex,
					     "SceWlanBtRobinWlanContext", 0x2000, 0);
	if (result < 0)
		goto out;
	result = robinWlanInitializeTcm(context);
	if (result < 0)
		goto out;
	result = robinWlanInitializeDcm(&context->dcm);
	if (result < 0)
		goto out;
	result = sceKernelCreateEventFlag("SceWlanBtRobinWlanWorker", 0, 0, NULL);
	if (result < 0)
		goto out;
	context->workerEventFlag = result;
	result = sceKernelCreateThread("SceNetDrvWlan", _workerEntry, 62, 8192, 0, 0xf0000, NULL);
	if (result < 0)
		goto out;
	context->worker = result;
	result = sceKernelStartThread(result, sizeof(context), &context);
	if (result < 0)
		goto out;
	WlanVisitorClose(&context->visitor, 0x80411682);
	result = sceKernelSetEventFlag(context->workerEventFlag, 1);
	if (result < 0)
		goto out;
	result = sceKernelRegisterSysEventHandler("SceWlanBtRobin", _debugDumpStatus, context);
	if (result < 0)
		goto out;
	context->sysEventHandler = result;
	result = 0;
out:
	return result;
}

int robinWlanNotifyWorker(unsigned int flags)
{
	return sceKernelSetEventFlag(gWlanContext.workerEventFlag, flags);
}

int robinWlanSetIoctlError(int error)
{
	gWlanContext.ioctlError = error;
	return error;
}

uint32_t robinWlanRecordCurrentState(unsigned int state, unsigned int stage)
{
	uint32_t marker = (stage << 16) | 0x80000000;

	marker |= state << 24;

	gWlanContext.currentStateMarker = marker;
	return marker;
}

uint32_t robinWlanRecordPendingState(unsigned int state, unsigned int stage)
{
	uint32_t marker = stage | 0x80000000 | (state << 8);

	gWlanContext.pendingStateMarker = marker;
	return marker;
}

int robinWlanGetBssid(uint8_t bssid[6])
{
	uint32_t prefix;

	__builtin_memcpy(&prefix, gWlanContext.bss.bssid.data, sizeof(prefix));
	__builtin_memcpy(bssid, &prefix, sizeof(prefix));
	__builtin_memcpy(bssid + sizeof(prefix),
			 gWlanContext.bss.bssid.data + sizeof(prefix),
			 sizeof(gWlanContext.bss.bssid) - sizeof(prefix));
	return prefix;
}

int robinWlanNeedsDeauthentication(void)
{
	return (gWlanContext.wakeFlags & 4) != 0;
}

unsigned int robinWlanGetConfiguration(void)
{
	unsigned int result, next;

	lockFastMutex(&gWlanContext.mutex);
	next = gWlanContext.nextConfiguration & 1;
	result = (gWlanContext.currentConfiguration & ~1) | next;
	if (gWlanContext.workPending) {
		if (next)
			result |= 0x10;
		else
			result |= 0x20;
	}
	if (gWlanContext.status & 0x400020)
		result |= 4;
	if ((int)gWlanContext.status < 0)
		result |= 0x40;
	if (gWlanContext.shutdownState) {
		result |= 0x200;
		if (gWlanContext.shutdownState == 1)
			result |= 1;
	}
	if (gWlanContext.interfaceStopRequested)
		result |= 0x400;
	if (gWlanContext.wakeEnabled)
		result |= 0x100;
	sceKernelUnlockFastMutex(&gWlanContext.mutex);
	return result;
}

int robinWlanSetConfiguration(int request, int force)
{
	SceWlanBtRobinWlanContext *context = &gWlanContext;
	int result = 0;

	if ((int)context->status < 0)
		return 0x804116c0;
	if (!force) {
		if ((context->status & 0x400004) != 4)
			return 0;
		if (context->status & 0x20) {
			if ((request & ~2) == 8 || (unsigned int)request - 2 <= 2)
				context->deferredConfigurationRequest = request;
			return 0;
		}
	}
	lockFastMutex(&context->mutex);
	switch (request) {
	case 0:
		context->wakeRestoreMode = 0;
		context->desiredConfiguration &= ~1;
		break;
	case 1:
		if (context->shutdownState) {
			result = 0x80418001;
			goto out;
		}
		context->wakeRestoreMode = 0;
		context->desiredConfiguration |= 1;
		break;
	case 2:
		context->desiredConfiguration &= ~2;
		if (context->wakeRestoreMode == 2)
			context->desiredConfiguration |= 1;
		context->wakeRestoreMode = 0;
		break;
	case 3:
		context->wakeRestoreMode = (context->currentConfiguration & 8) ? 2 : 1;
		context->desiredConfiguration = (context->desiredConfiguration & ~1) | 2;
		break;
	case 4:
		if (context->currentConfiguration & 1) {
			context->shutdownState = 1;
			context->desiredConfiguration &= ~1;
		} else {
			context->shutdownState = 2;
		}
		context->interfaceStopRequested = 1;
		if (context->status & 4)
			context->driver.notify(&context->driver, NULL, 0, 0xc0, NULL, 0);
		break;
	case 6:
		if (context->currentConfiguration & 0x100)
			context->desiredConfiguration &= ~0x100;
		context->wakeEnabled = 0;
		break;
	case 7:
		if (!(context->currentConfiguration & 0x100))
			context->desiredConfiguration |= 0x100;
		context->wakeEnabled = 1;
		break;
	case 9:
		if (context->shutdownState == 1)
			context->desiredConfiguration |= 1;
		context->shutdownState = 0;
		break;
	case 10:
		if (context->status & 4)
			context->driver.notify(&context->driver, NULL, 0, 0x80, NULL, 0);
		context->interfaceStopRequested = 1;
		break;
	case 11:
		context->interfaceStopRequested = 0;
		break;
	default:
		break;
	}
	if (!context->workPending)
		robinWlanUpdateConfiguration(context);
out:
	sceKernelUnlockFastMutex(&context->mutex);
	return result;
}

int _debugDumpStatus(int resume, int eventId, void *arguments, void *registeredContext)
{
	SceWlanBtRobinWlanContext *context = registeredContext;
	uint64_t now;
	int result = 0, wol;

	if (!(context->status & 1) || ksceSysconIsDownLoaderMode() || !wlanBtIsAvailable())
		return 0;
	if (!resume) {
		switch (eventId) {
		case SYSEVENT_SUSPEND_IDLE_ENTER:
			if (((struct PowerSuspendEvent *)arguments)->sleepMode) {
				__atomic_fetch_and(&context->status, ~0x80, __ATOMIC_RELEASE);
			} else if (SceSblAIMgrForDriver_71608CA3()) {
				if (!context->wakeFlags && (context->status & 0x400)) {
					context->wakeFlags = 4;
					__atomic_fetch_or(&context->status, 0x80, __ATOMIC_RELEASE);
				}
			} else if ((int)context->wakeFlags < 0 && !context->wakeScan.ssidCount) {
				context->wakeFlags |= 4;
				__atomic_fetch_or(&context->status, 0x80, __ATOMIC_RELEASE);
			}
			gWlanContext.pendingStateMarker = 0x80000100;
			WlanVisitorClose(&context->visitor, 0x80411683);
			__atomic_fetch_or(&context->status, 0x400000, __ATOMIC_RELEASE);
			gSuspendConfigurationQueued = 0;
			gSuspendWorkerRequested = 0;
			context->suspendTimestamp = sceKernelGetSystemTimeWide();
			context->suspendTimeoutReported = 0;
			break;
		case SYSEVENT_SUSPEND_IDLE:
			if ((int)context->status < 0)
				break;
			if (context->status & 0x800000) {
				gWlanContext.pendingStateMarker = 0x80000200;
				break;
			}
			if (!gSuspendConfigurationQueued) {
				context->savedConfiguration = wlanGetConfiguration() & 3;
				gWlanContext.pendingStateMarker = 0x80000300;
				sceRegMgrSetKeyInt("/CONFIG/NET", "wifi_config", context->wakeRestoreMode == 2);
				if (SceSblAIMgrForDriver_71608CA3() && (context->wakeFlags & 4) &&
				    sceRegMgrGetKeyInt("/CONFIG/NET/", "wol_config", &wol) >= 0 && !wol) {
					context->wakeFlags &= ~4;
					__atomic_fetch_and(&context->status, ~0x80, __ATOMIC_RELEASE);
				}
				gWlanContext.pendingStateMarker = 0x80000400;
				result = robinWlanSetConfiguration(0, 1);
				if (result < 0)
					gWlanContext.ioctlError = 0x804116c3;
				gSuspendConfigurationQueued = 1;
			}
			if (!context->workPending && (wlanGetConfiguration() & 0x201) != 1 &&
			    !gSuspendWorkerRequested && !(context->status & 0x40)) {
				gWlanContext.pendingStateMarker = 0x80000500;
				result = sceKernelSetEventFlag(context->workerEventFlag, 0x200);
				if (result < 0)
					gWlanContext.ioctlError = 0x804116c3;
				gSuspendWorkerRequested = 1;
			}
			break;
		case SYSEVENT_SUSPEND_IDLE_QUERY:
			if (gSuspendConfigurationQueued && (context->status & 0x20)) {
				__atomic_fetch_and(&context->status, ~0x400200, __ATOMIC_RELEASE);
				break;
			}
			result = context->currentStateMarker | context->pendingStateMarker;
			if (!result)
				break;
			now = sceKernelGetSystemTimeWide();
			if (!gWlanContext.suspendTimeoutReported &&
			    now >= gWlanContext.suspendTimestamp + 20000000 &&
			    now < gWlanContext.suspendTimestamp + 30000000) {
				gWlanContext.suspendTimeoutReported = 1;
#if FW_DEBUG
				SceDebugForDriver_D9703808(1, 0, &gSuspendTimeoutStatus,
					"[SceWlanBt]: delta=%lldms, status=0x%08x, intError=0x%08x\n",
					(now - gWlanContext.suspendTimestamp) / 1000,
					gWlanContext.status, gWlanContext.ioctlError);
				SceDebugForDriver_D9703808(1, 0, &gSuspendVisitorStatus,
					"[SceWlanBt]: gk cnt=%d, ec=0x%08x, available=%d\n",
					gWlanContext.visitor.count, gWlanContext.visitor.result,
					gWlanContext.visitor.accepting);
				SceDebugForDriver_D9703808(1, 0, &gSuspendCommandStatus,
					"[SceWlanBt]: curCmdDesc=%p, pendingCmdDesc=%p\n",
					gWlanContext.dcm.currentCommand, gWlanContext.dcm.pendingCommands);
				if (gWlanContext.dcm.currentCommand)
					SceDebugForDriver_D9703808(1, 0, &gCurrentCommandSuspendWait,
						"[SceWlanBt]: curCmdCode=%04x, wait=%lldms\n",
						gWlanContext.dcm.currentCommand->commandId,
						(now - gWlanContext.dcm.currentCommand->timestamp) / 1000);
				if (gWlanContext.dcm.pendingCommands)
					SceDebugForDriver_D9703808(1, 0, &gPendingCommandSuspendWait,
						"[SceWlanBt]: pendingCmdCode=%04x, wait=%lldms\n",
						gWlanContext.dcm.pendingCommands->commandId,
						(now - gWlanContext.dcm.pendingCommands->timestamp) / 1000);
#endif
				sceKernelSetEventFlag(gWlanContext.workerEventFlag, 0x2000000);
			}
			break;
		default:
			break;
		}
	} else if (resume == 1) {
		if (eventId == SYSEVENT_RESUME_START) {
			gWlanContext.currentStateMarker = 0x81000000;
			gWlanContext.ioctlError = 0;
			memset(&context->connection, 0, sizeof(context->connection));
			context->connectionSize = 0;
			__atomic_fetch_and(&context->status, 0x7ffffebf, __ATOMIC_RELEASE);
			__atomic_fetch_or(&context->status, 0x800000, __ATOMIC_RELEASE);
			context->nextConfiguration = context->savedConfiguration;
			context->deferredConfigurationRequest = 0;
			result = sceKernelSetEventFlag(context->workerEventFlag, 0x1000);
			if (result < 0)
				gWlanContext.ioctlError = 0x804116c2;
			gWlanContext.currentStateMarker = 0x82000000;
		} else if (eventId == SYSEVENT_RESUME_IDLE_QUERY &&
			   (int)context->status >= 0 && (context->status & 0x200000) && context->wakeFlags) {
			result = context->currentStateMarker | context->pendingStateMarker;
		}
	}
	return result;
}

int robinWlanGetWakeOnTargetProcess(void)
{
	int processId;

	lockFastMutex(&gWlanContext.mutex);
	processId = gWlanContext.secondNetworkId;
	if (!processId) {
		processId = gWlanContext.firstNetworkId;
		if (!processId) {
			processId = gWlanContext.wakeOwnerProcess;
			if (!processId)
				processId = -1;
		}
	}
	sceKernelUnlockFastMutex(&gWlanContext.mutex);
	return processId;
}

int robinWlanStartProcess(SceUID pid, int eventType)
{
	if (eventType == 0x40000) {
		if (pid == gWlanContext.wakeNetwork.networkId) {
			memset(&gWlanContext.wakeNetwork, 0, sizeof(gWlanContext.wakeNetwork));
			gWlanContext.wakeFlags &= ~1;
		} else if (pid == gWlanContext.wakeAdhoc.networkId) {
			memset(&gWlanContext.wakeAdhoc, 0, sizeof(gWlanContext.wakeAdhoc));
			gWlanContext.wakeFlags &= ~2;
		}
		if (!gWlanContext.wakeFlags)
			__atomic_fetch_and(&gWlanContext.status, ~0x80, __ATOMIC_RELEASE);
	}
	return 0;
}

int robinWlanExitProcess(SceUID pid)
{
	if (pid == gWlanContext.wakeNetwork.networkId) {
		memset(&gWlanContext.wakeNetwork, 0, sizeof(gWlanContext.wakeNetwork));
		gWlanContext.wakeFlags &= ~1;
	} else if (pid == gWlanContext.wakeAdhoc.networkId) {
		memset(&gWlanContext.wakeAdhoc, 0, sizeof(gWlanContext.wakeAdhoc));
		gWlanContext.wakeFlags &= ~2;
	}
	if (gWlanContext.wakeFlags)
		return 0;
	__atomic_fetch_and(&gWlanContext.status, ~0x80, __ATOMIC_RELEASE);
	return 0;
}

int robinWlanKillProcess(SceUID pid)
{
	if (pid == gWlanContext.wakeNetwork.networkId) {
		memset(&gWlanContext.wakeNetwork, 0, sizeof(gWlanContext.wakeNetwork));
		gWlanContext.wakeFlags &= ~1;
	} else if (pid == gWlanContext.wakeAdhoc.networkId) {
		memset(&gWlanContext.wakeAdhoc, 0, sizeof(gWlanContext.wakeAdhoc));
		gWlanContext.wakeFlags &= ~2;
	}
	if (gWlanContext.wakeFlags)
		return 0;
	__atomic_fetch_and(&gWlanContext.status, ~0x80, __ATOMIC_RELEASE);
	return 0;
}

int robinWlanSwitchProcess(int eventId)
{
	if (eventId == 16)
		return sceKernelChangeThreadCpuAffinityMask(gWlanContext.worker, 0x80000);
	if (eventId == 17)
		return sceKernelChangeThreadCpuAffinityMask(gWlanContext.worker, 0xf0000);
	return 0;
}
