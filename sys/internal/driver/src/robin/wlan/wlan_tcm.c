/* TCM method family; filename corroborated by prototype names-only records. */
#include "../../wlanbt/wlanbt_internal.h"
#include <string.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <threadmgr/threadmgr.h>
#include <wlanbt/wlanbt.h>
#if FW_DEBUG
#include <sysmem/fortify.h>
#endif

/* 802.11b rates in 500 kb/s units; 1 and 2 Mb/s are basic rates. */
static const uint8_t gWakeAdhocSupportedRates[] = { 0x80 | 2, 0x80 | 4, 11, 22 };

#if FW_DEBUG
uint32_t gWlanInitializeStage;
uint32_t gWlanHostSleepResetStage;
uint32_t gWlanPowerSaveEnableStage;
uint32_t gWlanSleepStage;
uint32_t gWlanWakeOnStage;
uint32_t gWlanHostSleepPeerIndex;
uint32_t gWlanShutdownStage;
uint32_t gWlanWakeOffStage;
uint32_t gWlanPowerSaveDisableStage;
uint32_t gWlanSetupStage;
#else
uint32_t gWlanWakeOffStage;
uint32_t gWlanSleepStage;
uint32_t gWlanInitializeStage;
uint32_t gWlanWakeOnStage;
uint32_t gWlanPowerSaveEnableStage;
uint32_t gWlanHostSleepResetStage;
uint32_t gWlanHostSleepPeerIndex;
uint32_t gWlanShutdownStage;
uint32_t gWlanSetupStage;
uint32_t gWlanPowerSaveDisableStage;
#endif

int robinWlanNotifyWorker(unsigned int flags);
int robinWlanSetIoctlError(int error);
uint32_t robinWlanRecordPendingState(unsigned int state, unsigned int stage);
int robinWlanReleaseCommand(SceWlanBtRobinWlanContext *context,
			   SceWlanBtRobinWlanCommand *command);
int robinWlanRequestFunctionInitialize(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanSetTransmitBuffer(SceWlanBtRobinWlanContext *context, uint16_t size,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanEnableAutoDeepSleep(SceWlanBtRobinWlanContext *context, uint16_t timeout,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestIbssCoalescing(SceWlanBtRobinWlanContext *context, int enabled,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanSetAmsduAggregation(SceWlanBtRobinWlanContext *context, int enabled,
	uint16_t currentBufferSize,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanSetInactivityTimeout(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanSubscribeEvents(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestCoexistence(SceWlanBtRobinWlanContext *context, uint8_t mode,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestDisableEnergyDetectionMac(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanConfirmSleep(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestEnableStationPowerSave(SceWlanBtRobinWlanContext *context,
	uint16_t nullPacketInterval, uint16_t beaconMissTimeout,
	uint16_t adhocWakePeriod, uint16_t mode, SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestDisableStationPowerSave(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanSetSdioPullControl(SceWlanBtRobinWlanContext *context,
	uint16_t pullUpDelay, uint16_t pullDownDelay, uint8_t pullControl, uint8_t pullFlags,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanConfigureHostSleep(SceWlanBtRobinWlanContext *context,
	uint32_t conditions, uint8_t gpio, uint8_t gap,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestMacAddress(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanCompleteSetup(SceWlanBtRobinWlanContext *context,
			   SceWlanBtRobinWlanCommand *command);
int robinWlanCompleteWakeOnConfiguration(SceWlanBtRobinWlanContext *context,
					 SceWlanBtRobinWlanCommand *command);

int robinWlanScan(SceWlanBtRobinWlanContext *context, WlanScanConfig *configuration,
		 unsigned int capacity);

int robinWlanReadBackgroundScan(SceWlanBtRobinWlanContext *context,
			       WlanScanResult *result, unsigned int capacity);
int robinWlanSetBackgroundScan(
	SceWlanBtRobinWlanContext *context, uint8_t enabled,
	const SceWlanBtScanSsid *ssids, unsigned int ssidCount,
	const SceWlanBtScanSsid *wildcardSsids, unsigned int wildcardCount,
	uint8_t bssType, uint8_t channelsPerScan, uint32_t scanInterval,
	const uint8_t *channels, unsigned int channelCount, uint8_t scanMode,
	uint16_t minScanTime, uint16_t maxScanTime);

int robinWlanEnableStationPowerSave(
	SceWlanBtRobinWlanContext *context, uint16_t nullPacketInterval,
	uint16_t beaconMissTimeout, uint16_t adhocWakePeriod, uint16_t mode);
int robinWlanDisableStationPowerSave(SceWlanBtRobinWlanContext *context);

int robinWlanSetManagementInformationElements(
	SceWlanBtRobinWlanContext *context, const void *informationElements, size_t size);

int robinWlanSetMulticastAddresses(SceWlanBtRobinWlanContext *context,
				  unsigned int count, const SceNetEtherAddr *addresses);
int robinWlanSetControl(SceWlanBtRobinWlanContext *context, uint16_t control);
int robinWlanSetBssMode(SceWlanBtRobinWlanContext *context, uint8_t connectionType);
int robinWlanSetTransmitRate(SceWlanBtRobinWlanContext *context, unsigned int rate);
int robinWlanSetHtTransmit(SceWlanBtRobinWlanContext *context,
			  uint16_t capabilities, uint16_t information);
int robinWlanGetRssi(SceWlanBtRobinWlanContext *context, WlanSignalInfo *output);
int robinWlanDeauthenticate(SceWlanBtRobinWlanContext *context);
int mrvlExecAssociate(SceWlanBtRobinWlanContext *context,
	const WlanBssDesc *descriptor, const WlanConnectionSecurity *security,
	const void *informationElements, unsigned int informationElementsSize,
	unsigned int *htEnabled);
int robinWlanSetIbssCoalescing(SceWlanBtRobinWlanContext *context, int enabled);
int robinWlanStartAdhoc(SceWlanBtRobinWlanContext *context,
		      WlanBssConnectionConfig *configuration, unsigned int capacity);
int robinWlanJoinAdhoc(SceWlanBtRobinWlanContext *context,
		     WlanBssConnectionConfig *configuration);
int robinWlanStopAdhoc(SceWlanBtRobinWlanContext *context);
typedef struct SceWlanBtRobinWlanAddBlockAckMessage SceWlanBtRobinWlanAddBlockAckMessage;
typedef struct SceWlanBtRobinWlanDeleteBlockAckMessage SceWlanBtRobinWlanDeleteBlockAckMessage;
int robinWlanRespondAddBlockAck(SceWlanBtRobinWlanContext *context,
	const SceWlanBtRobinWlanAddBlockAckMessage *input,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
void robinWlanReadDeleteBlockAckReason(
	const SceWlanBtRobinWlanDeleteBlockAckMessage *message,
	uint8_t *initiator, uint16_t *reason);
void robinWlanReadDeleteBlockAckResult(
	const SceWlanBtRobinWlanDeleteBlockAckMessage *message,
	SceNetEtherAddr *peer, uint8_t *result, uint8_t *parameterFlags);
int robinWlanRequestDeleteBlockAck(SceWlanBtRobinWlanContext *context,
	uint8_t result, const SceNetEtherAddr *peer, uint16_t parameters,
	uint16_t reason,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));

int robinWlanRequestRssi(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanCompleteAsyncCommand(SceWlanBtRobinWlanContext *context,
				  SceWlanBtRobinWlanCommand *command);

int robinWlanWriteMemory(SceWlanBtRobinWlanContext *context, uint32_t address,
			 uint32_t value);

int robinWlanReadMemory(SceWlanBtRobinWlanContext *context, uint32_t address,
			 uint32_t *output);

int robinWlanSetEnergyDetectionMacOffset(SceWlanBtRobinWlanContext *context,
				       int16_t offset);
int robinWlanDisableEnergyDetectionMac(SceWlanBtRobinWlanContext *context);

int robinWlanRequestSetup(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanValidateSetupResponse(const void *response, int responseSize);
int robinWlanReadMacAddressResponse(SceWlanBtRobinWlanCommand *command,
				   SceNetEtherAddr *address);
int robinWlanRequestDeauthentication(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestStopAdhoc(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestFunctionShutdown(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
uint32_t robinWlanRecordCurrentState(unsigned int state, unsigned int stage);
int robinWlanRequestHostSleepAddressData(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanReadHostSleepAddresses(SceWlanBtRobinWlanContext *context,
				  const void *buffer, unsigned int size);
int robinWlanRequestHostSleepDataForAddress(SceWlanBtRobinWlanContext *context,
	const SceNetEtherAddr *address,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanReadHostSleepPeerData(SceWlanBtRobinWlanContext *context,
				 const void *buffer, unsigned int size, int index);
int robinWlanClearHostSleepAddressData(SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestBackgroundScanQuery(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanReadBackgroundScanResponse(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand *command, WlanScanResult *result, unsigned int capacity);
int robinWlanRequestBackgroundScan(
	SceWlanBtRobinWlanContext *context, uint8_t enabled,
	const SceWlanBtScanSsid *ssids, unsigned int ssidCount,
	const SceWlanBtScanSsid *wildcardSsids, unsigned int wildcardCount,
	uint8_t bssType, uint8_t channelsPerScan, uint32_t scanInterval,
	const uint8_t *channels, unsigned int channelCount, uint8_t scanMode,
	uint16_t minScanTime, uint16_t maxScanTime, SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));

int robinWlanCompleteSetup(SceWlanBtRobinWlanContext *context,
			   SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanCommand *output = NULL;
	int commandResult = 0, result;

	if (!command)
		gWlanSetupStage = 1;
	switch (gWlanSetupStage) {
	case 1:
		commandResult = robinWlanRequestMacAddress(context, &output, robinWlanCompleteSetup);
		gWlanSetupStage = 2;
		break;
	case 2:
		commandResult = robinWlanReadMacAddressResponse(command, &context->address);
		if (commandResult >= 0) {
			commandResult = robinWlanRequestSetup(context, robinWlanCompleteSetup);
			gWlanSetupStage = 3;
		}
		break;
	case 3:
		commandResult = robinWlanValidateSetupResponse(command->responsePayload,
						     command->responseSize);
		if (commandResult >= 0)
			commandResult = robinWlanNotifyWorker(2);
		break;
	default:
		break;
	}
	result = commandResult;
	if (command)
		result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
		result = robinWlanSetIoctlError(0x804116c4);
	return result;
}

int robinWlanCompleteInitialization(SceWlanBtRobinWlanContext *context,
				    SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanCommand *output;
	int commandResult, result;

	if (!command)
		gWlanInitializeStage = 1;
	switch (gWlanInitializeStage) {
	case 1:
		commandResult = robinWlanRequestFunctionInitialize(context,
						robinWlanCompleteInitialization);
		gWlanInitializeStage = 2;
		break;
	case 2:
		commandResult = robinWlanSetTransmitBuffer(context, 0x800,
						 robinWlanCompleteInitialization);
		gWlanInitializeStage = 3;
		break;
	case 3:
		commandResult = robinWlanEnableAutoDeepSleep(context, 100,
						   robinWlanCompleteInitialization);
		gWlanInitializeStage = 4;
		break;
	case 4:
		output = NULL;
		commandResult = robinWlanRequestIbssCoalescing(context, 1, &output,
						robinWlanCompleteInitialization);
		gWlanInitializeStage = 5;
		break;
	case 5:
		commandResult = robinWlanSetAmsduAggregation(context, 0, 0,
						   robinWlanCompleteInitialization);
		gWlanInitializeStage = 6;
		break;
	case 6:
		commandResult = robinWlanSetInactivityTimeout(context,
						robinWlanCompleteInitialization);
		gWlanInitializeStage = 7;
		break;
	case 7:
		commandResult = robinWlanSubscribeEvents(context,
						robinWlanCompleteInitialization);
		gWlanInitializeStage = 8;
		break;
	case 8:
		output = NULL;
		commandResult = robinWlanRequestCoexistence(context, 0, &output,
						robinWlanCompleteInitialization);
		gWlanInitializeStage = 10;
		break;
	case 9:
		output = NULL;
		commandResult = robinWlanRequestDisableEnergyDetectionMac(context, &output,
						robinWlanCompleteInitialization);
		gWlanInitializeStage = 10;
		break;
	case 10:
		commandResult = robinWlanNotifyWorker(8);
		break;
	default:
		commandResult = 0;
		break;
	}
	result = commandResult;
	if (command)
		result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
		result = robinWlanSetIoctlError(0x804116c5);
	return result;
}

int robinWlanCompleteShutdown(SceWlanBtRobinWlanContext *context,
			      SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanCommand *output;
	int commandResult = 0, result;

	if (!command) {
		if (context->status & 0x400) {
			gWlanShutdownStage = 1;
			goto deauthenticate;
		}
		if (context->status & 0x800) {
			gWlanShutdownStage = 2;
			goto stopAdhoc;
		}
		gWlanShutdownStage = 3;
		goto shutdown;
	}
	switch (gWlanShutdownStage) {
	case 1:
	deauthenticate:
		output = NULL;
		commandResult = robinWlanRequestDeauthentication(context, &output,
							robinWlanCompleteShutdown);
		gWlanShutdownStage = 3;
		break;
	case 2:
	stopAdhoc:
		output = NULL;
		commandResult = robinWlanRequestStopAdhoc(context, &output, robinWlanCompleteShutdown);
		gWlanShutdownStage = 3;
		break;
	case 3:
	shutdown:
		commandResult = robinWlanRequestFunctionShutdown(context, robinWlanCompleteShutdown);
		gWlanShutdownStage = 4;
		break;
	case 4:
		commandResult = robinWlanNotifyWorker(0x20);
		break;
	default:
		break;
	}
	result = commandResult;
	if (command)
		result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
		result = robinWlanSetIoctlError(0x804116c6);
	return result;
}

int robinWlanCompletePowerSaveEnable(SceWlanBtRobinWlanContext *context,
				     SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanCommand *output;
	int commandResult, result;

	if (command) {
		if (gWlanPowerSaveEnableStage != 1) {
			if (gWlanPowerSaveEnableStage == 2)
				commandResult = robinWlanNotifyWorker(0x80000);
			else
				commandResult = 0;
			goto release;
		}
	} else {
		gWlanPowerSaveEnableStage = 1;
	}
	result = robinWlanRequestEnableStationPowerSave(context, 0, 5, 0, 1,
					     &output, robinWlanCompletePowerSaveEnable);
	commandResult = result;
	gWlanPowerSaveEnableStage = 2;
	if (!command) {
		if (result >= 0)
			return result;
		goto error;
	}
release:
	result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
error:
		result = robinWlanSetIoctlError(0x804116c7);
	return result;
}

int robinWlanCompletePowerSaveDisable(SceWlanBtRobinWlanContext *context,
				      SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanCommand *output;
	int commandResult, result;

	if (command) {
		if (gWlanPowerSaveDisableStage != 1) {
			if (gWlanPowerSaveDisableStage == 2)
				commandResult = robinWlanNotifyWorker(0x200000);
			else
				commandResult = 0;
			goto release;
		}
	} else {
		gWlanPowerSaveDisableStage = 1;
	}
	result = robinWlanRequestDisableStationPowerSave(context, &output,
						robinWlanCompletePowerSaveDisable);
	commandResult = result;
	gWlanPowerSaveDisableStage = 2;
	if (!command) {
		if (result >= 0)
			return result;
		goto error;
	}
release:
	result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
error:
		result = robinWlanSetIoctlError(0x804116c7);
	return result;
}

int robinWlanCompleteHostSleepReset(SceWlanBtRobinWlanContext *context,
				   SceWlanBtRobinWlanCommand *command)
{
	int result, commandResult;
	unsigned int stage;

	if (command) {
		stage = gWlanHostSleepResetStage;
	} else {
		stage = 1;
		gWlanHostSleepResetStage = 1;
	}
	result = robinWlanRecordPendingState(13, stage);
	switch (gWlanHostSleepResetStage) {
	case 1:
		result = robinWlanSetSdioPullControl(context, 0, 0, 4, 15,
						robinWlanCompleteHostSleepReset);
		commandResult = result;
		gWlanHostSleepResetStage = 2;
		break;
	case 2:
		result = robinWlanConfigureHostSleep(context, 0, 16, 0,
						robinWlanCompleteHostSleepReset);
		commandResult = result;
		gWlanHostSleepResetStage = 3;
		break;
	case 3:
		result = robinWlanNotifyWorker(0x800);
		commandResult = result;
		break;
	default:
		commandResult = 0;
		break;
	}
	if (command)
		result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
		return robinWlanSetIoctlError(0x804116c9);
	return result;
}

int robinWlanCompleteWakeOffConfiguration(SceWlanBtRobinWlanContext *context,
					SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanCommand *output;
	WlanScanResult *scan;
	WlanBssDesc *descriptor;
	uint8_t *cursor;
	unsigned int flags, count, i;
	int commandResult = 0, result;

	if (!command) {
		flags = context->wakeFlags;
		if (flags) {
			if (flags & 3)
				gWlanWakeOffStage = 1;
			else if ((flags & 4) && (context->status & 0x400))
				gWlanWakeOffStage = 2;
			else
				gWlanWakeOffStage = 3;
		} else if (!context->wakeOwnerProcess) {
			gWlanWakeOffStage = (ksceWlanBtGetConfiguration() & 0x100) ? 9 : 8;
		} else {
			gWlanWakeOffStage = 3;
		}
	}
	robinWlanRecordCurrentState(6, gWlanWakeOffStage);
	switch (gWlanWakeOffStage) {
	case 1:
		output = NULL;
		commandResult = robinWlanRequestStopAdhoc(context, &output,
							robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 3;
		break;
	case 2:
		output = NULL;
		commandResult = robinWlanRequestDeauthentication(context, &output,
							robinWlanCompleteWakeOffConfiguration);
		__atomic_fetch_and(&context->status, ~0x400, __ATOMIC_RELEASE);
		context->blockAckTransmitState = 0;
		context->signal.signalQuality = 0;
		context->blockAckReceivePending = 0;
		context->deleteBlockAckPending = 0;
		context->signal.rssi = -100;
		gWlanWakeOffStage = 8;
		break;
	case 3:
		commandResult = robinWlanRequestHostSleepAddressData(context,
							robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 4;
		break;
	case 4:
		robinWlanReadHostSleepAddresses(context, command->responsePayload, command->responseSize);
		if (context->peerCount) {
			gWlanHostSleepPeerIndex = 0;
			commandResult = robinWlanRequestHostSleepDataForAddress(context,
				&context->peers[0].address, robinWlanCompleteWakeOffConfiguration);
			gWlanWakeOffStage = 5;
		} else {
			output = NULL;
			commandResult = robinWlanRequestBackgroundScanQuery(context, &output,
							robinWlanCompleteWakeOffConfiguration);
			gWlanWakeOffStage = 7;
		}
		break;
	case 5:
		if (robinWlanReadHostSleepPeerData(context, command->responsePayload,
						 command->responseSize, gWlanHostSleepPeerIndex) < 0) {
			for (i = 0; i < sizeof(context->peers[0].address.data); ++i)
				context->peers[gWlanHostSleepPeerIndex].address.data[i] = 0;
		}
		++gWlanHostSleepPeerIndex;
		if (gWlanHostSleepPeerIndex == context->peerCount) {
			commandResult = robinWlanClearHostSleepAddressData(context,
							robinWlanCompleteWakeOffConfiguration);
			gWlanWakeOffStage = 6;
		} else {
			commandResult = robinWlanRequestHostSleepDataForAddress(context,
				&context->peers[gWlanHostSleepPeerIndex].address,
				robinWlanCompleteWakeOffConfiguration);
		}
		break;
	case 6:
		output = NULL;
		commandResult = robinWlanRequestBackgroundScanQuery(context, &output,
							robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 7;
		break;
	case 7:
		if (context->wakeFlags || context->wakeOwnerProcess) {
			scan = context->driver.allocate(&context->driver, 2048, 4);
			if (scan) {
				robinWlanReadBackgroundScanResponse(context, command, scan, 2048);
				count = 0;
				cursor = scan->bssDesc;
				if (scan->bssNum) {
					do {
						unsigned int ssidSize;

						descriptor = (WlanBssDesc *)cursor;
						ssidSize = descriptor->ssidSize;
						context->discoveredSsids[count].size = ssidSize;
						memcpy(context->discoveredSsids[count].ssid,
						       descriptor->ssid, ssidSize);
						cursor += descriptor->size;
						++count;
					} while (count < scan->bssNum && count != 10);
				}
				context->discoveredSsidCount = count;
				context->driver.free(&context->driver, scan);
			}
		}
		commandResult = robinWlanRequestBackgroundScan(context, 0, NULL, 0, NULL, 0,
						0, 0, 0, NULL, 0, 0, 0, 0, &output,
						robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 8;
		break;
	case 8:
		commandResult = robinWlanRequestFunctionShutdown(context,
							robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 9;
		break;
	case 9:
		commandResult = robinWlanNotifyWorker(0x2000);
		break;
	default:
		break;
	}
	result = commandResult;
	if (command)
		result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
		result = robinWlanSetIoctlError(0x804116ca);
	return result;
}

int robinWlanReadAddBlockAckResult(const SceWlanBtRobinWlanCommand *command,
				 uint8_t *result);
int robinWlanReadRssiResponse(SceWlanBtRobinWlanCommand *command, WlanSignalInfo *output);

int robinWlanCompleteAsyncCommand(SceWlanBtRobinWlanContext *context,
				  SceWlanBtRobinWlanCommand *command)
{
	int result = (intptr_t)context;
	unsigned int commandId;

	if (command) {
		commandId = command->commandId;
		if (commandId == 0xce) {
			uint8_t status = 0;

			robinWlanReadAddBlockAckResult(command, &status);
			if (status == 2) {
				context->transmittedPackets = 0;
				context->blockAckTransmitState = 0;
			}
		} else if (commandId <= 0xce) {
			if (commandId == 0xa4)
				robinWlanReadRssiResponse(command, &context->signal);
		} else if (commandId == 0xcf) {
			context->blockAckReceivePending = 0;
		} else if (commandId == 0xd0) {
			context->deleteBlockAckPending = 0;
		}
		result = robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanCompleteSleep(SceWlanBtRobinWlanContext *context,
			   SceWlanBtRobinWlanCommand *command)
{
	int commandResult, result;

	if (!command) {
		gWlanSleepStage = 1;
	} else if (gWlanSleepStage != 1) {
		if (gWlanSleepStage == 2)
			commandResult = robinWlanNotifyWorker(0x8000);
		else
			commandResult = 0;
		goto release;
	}
	commandResult = robinWlanConfirmSleep(context, robinWlanCompleteSleep);
	gWlanSleepStage = 2;
	result = commandResult;
	if (!command)
		goto check;
release:
	result = robinWlanReleaseCommand(context, command);
check:
	if (commandResult < 0)
		return robinWlanSetIoctlError(0x804116c7);
	return result;
}

int robinWlanSetMacFilter(SceWlanBtRobinWlanContext *context,
	const SceNetEtherAddr *address,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanSetHostSleepData(SceWlanBtRobinWlanContext *context,
	const void *data, size_t size,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanSetHostSleepAddressData(SceWlanBtRobinWlanContext *context,
	const SceNetEtherAddr *address, const void *data, size_t size,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestBssMode(SceWlanBtRobinWlanContext *context, uint8_t connectionType,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int robinWlanRequestStartAdhoc(SceWlanBtRobinWlanContext *context,
	WlanBssConnectionConfig *configuration, unsigned int capacity,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));
int wlanIsSupportedChannel(int channel);

int robinWlanCompleteWakeOnConfiguration(SceWlanBtRobinWlanContext *context,
					 SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanCommand *output;
	int commandResult = 0, result;

	if (!command) {
		if (context->wakeFlags & 4)
			gWlanWakeOnStage = 1;
		else if (context->wakeFlags & 1)
			gWlanWakeOnStage = 3;
		else
			gWlanWakeOnStage = (context->wakeFlags & 2) ? 5 : 10;
	}
	robinWlanRecordPendingState(12, gWlanWakeOnStage);
	switch (gWlanWakeOnStage) {
	case 1:
		commandResult = robinWlanSetMacFilter(context, &context->address,
						    robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 2;
		break;
	case 2:
		output = NULL;
		commandResult = robinWlanRequestEnableStationPowerSave(context, 60, 50, 0, 1,
			&output, robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 11;
		break;
	case 3:
		commandResult = robinWlanSetHostSleepData(context, &context->wakePayload[2],
			context->wakeNetwork.hostSleepDataSize, robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 4;
		break;
	case 4:
		commandResult = robinWlanSetHostSleepAddressData(context, &context->address,
			context->wakePayload, context->wakePayloadSize,
			robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 5;
		break;
	case 5:
		output = NULL;
		commandResult = robinWlanRequestBssMode(context, 1, &output,
			robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 6;
		break;
	case 6:
		output = NULL;
		commandResult = robinWlanRequestEnableStationPowerSave(context, 0, 5, 255, 1,
			&output, robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 7;
		break;
	case 7:
		output = NULL;
		commandResult = robinWlanRequestIbssCoalescing(context, 0, &output,
			robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 8;
		break;
	case 8: {
		WlanBssConnectionConfig configuration;

		output = NULL;
		memset(&configuration, 0, sizeof(configuration));
		configuration.bss.size = sizeof(configuration);
		if (context->wakeAdhoc.ssid.size) {
			configuration.bss.ssidSize = context->wakeAdhoc.ssid.size;
			memcpy(configuration.bss.ssid, context->wakeAdhoc.ssid.ssid,
			       context->wakeAdhoc.ssid.size);
		} else if (context->wakeNetwork.ssid.size) {
			configuration.bss.ssidSize = context->wakeNetwork.ssid.size;
			memcpy(configuration.bss.ssid, context->wakeNetwork.ssid.ssid,
			       context->wakeNetwork.ssid.size);
		}
		configuration.bss.bssMode = 2;
		configuration.bss.channel = (context->wakeFlags & 2) ?
			context->wakeAdhoc.channel : context->wakeNetwork.channel;
		configuration.bss.capabilities = 0x22;
		memcpy(configuration.bss.supportedRates, gWakeAdhocSupportedRates,
		       sizeof(gWakeAdhocSupportedRates));
		configuration.bss.beaconInterval = 110;
		configuration.bss.atimWindow = 2;
		commandResult = robinWlanRequestStartAdhoc(context, &configuration,
			sizeof(configuration), &output, robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 9;
		break;
	}
	case 9: {
		SceWlanBtScanSsid ssids[2], wildcardSsids[2];
		unsigned int ssidCount = 0, wildcardCount = 0;
		uint8_t channel;

		output = NULL;
		channel = (context->wakeFlags & 2) ? context->wakeAdhoc.channel :
			context->wakeNetwork.channel;
		memset(ssids, 0, sizeof(ssids));
		memset(wildcardSsids, 0, sizeof(wildcardSsids));
		if (context->wakeNetwork.secondarySsid.size) {
			if (context->wakeNetwork.flags & 1) {
				wildcardSsids[0].size = context->wakeNetwork.secondarySsid.size;
				memcpy(wildcardSsids[0].ssid, context->wakeNetwork.secondarySsid.ssid,
				       context->wakeNetwork.secondarySsid.size);
				wildcardCount = 1;
			} else {
				ssids[0].size = context->wakeNetwork.secondarySsid.size;
				memcpy(ssids[0].ssid, context->wakeNetwork.secondarySsid.ssid,
				       context->wakeNetwork.secondarySsid.size);
				ssidCount = 1;
			}
		}
		if (context->wakeAdhoc.secondarySsid.size) {
			if (context->wakeAdhoc.flags & 1) {
				wildcardSsids[wildcardCount].size = context->wakeAdhoc.secondarySsid.size;
				memcpy(wildcardSsids[wildcardCount].ssid,
				       context->wakeAdhoc.secondarySsid.ssid,
				       context->wakeAdhoc.secondarySsid.size);
				++wildcardCount;
			} else {
				ssids[ssidCount].size = context->wakeAdhoc.secondarySsid.size;
				memcpy(ssids[ssidCount].ssid, context->wakeAdhoc.secondarySsid.ssid,
				       context->wakeAdhoc.secondarySsid.size);
				++ssidCount;
			}
		}
		commandResult = robinWlanRequestBackgroundScan(context, 1, ssids, ssidCount,
			wildcardSsids, wildcardCount, 2, 1, 3000, &channel, 1, 1, 100, 100,
			&output, robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 11;
		break;
	}
	case 10: {
		unsigned int channelCount = 0;
		uint8_t scanMode;

		output = NULL;
		while (channelCount != 14 && context->wakeScan.channels[channelCount] &&
		       wlanIsSupportedChannel(context->wakeScan.channels[channelCount]))
			++channelCount;
		scanMode = context->wakeScan.scanMode == 2 ? 3 : 2;
		commandResult = robinWlanRequestBackgroundScan(context, 1,
			context->wakeScan.ssids, context->wakeScan.ssidCount, NULL, 0,
			context->wakeScan.bssType, 14, context->wakeScan.scanInterval,
			context->wakeScan.channels, channelCount, scanMode,
			context->wakeScan.minScanTime, context->wakeScan.maxScanTime,
			&output, robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 11;
		break;
	}
	case 11:
		commandResult = robinWlanSetSdioPullControl(context, 0, 0, 4, 15,
			robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 12;
		break;
	case 12: {
		unsigned int conditions, networkConditions, alternateConditions;

		if (context->wakeFlags & 4) {
			conditions = 0;
			networkConditions = 16;
			alternateConditions = 32;
		} else {
			conditions = 4;
			networkConditions = 20;
			alternateConditions = 36;
		}
		if (context->wakeFlags & 1)
			conditions = (context->wakeNetwork.flags & 2) ?
				alternateConditions : networkConditions;
		commandResult = robinWlanConfigureHostSleep(context, conditions, 16, 255,
			robinWlanCompleteWakeOnConfiguration);
		gWlanWakeOnStage = 13;
		break;
	}
	case 13:
		commandResult = robinWlanNotifyWorker(0x400);
		break;
	}
	result = commandResult;
	if (command)
		result = robinWlanReleaseCommand(context, command);
	if (commandResult < 0)
		result = robinWlanSetIoctlError(0x804116c8);
	return result;
}

int robinWlanInitializeTcm(SceWlanBtRobinWlanContext *context)
{
	int result, eventFlag;

	result = ksceKernelInitializeFastMutex(&context->tcm.mutex,
					       "SceWlanBtRobinWlanTcm", 0x2000, NULL);
	if (result >= 0) {
		eventFlag = ksceKernelCreateEventFlag("SceWlanBtRobinWlanTcm", 0, 0, NULL);
		if (eventFlag < 0)
			return eventFlag;
		context->tcm.eventFlag = eventFlag;
		context->signal.rssi = -100;
		context->signal.signalQuality = 0;
	}
	return result;
}

int robinWlanBeginSetup(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *output = NULL;
	int result;

	gWlanSetupStage = 1;
	result = robinWlanRequestMacAddress(context, &output, robinWlanCompleteSetup);
	gWlanSetupStage = 2;
	if (result < 0)
		robinWlanSetIoctlError(0x804116c4);
	return 0;
}

int robinWlanBeginInitialize(SceWlanBtRobinWlanContext *context)
{
	int result;

	gWlanInitializeStage = 1;
	result = robinWlanRequestFunctionInitialize(context, robinWlanCompleteInitialization);
	gWlanInitializeStage = 2;
	if (result < 0)
		robinWlanSetIoctlError(0x804116c5);
	return 0;
}

int robinWlanBeginShutdown(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *output;
	int result;

	if (context->status & 0x400) {
		output = NULL;
		gWlanShutdownStage = 1;
		result = robinWlanRequestDeauthentication(context, &output,
							robinWlanCompleteShutdown);
		gWlanShutdownStage = 3;
	} else if (context->status & 0x800) {
		output = NULL;
		gWlanShutdownStage = 2;
		result = robinWlanRequestStopAdhoc(context, &output,
						 robinWlanCompleteShutdown);
		gWlanShutdownStage = 3;
	} else {
		gWlanShutdownStage = 3;
		result = robinWlanRequestFunctionShutdown(context, robinWlanCompleteShutdown);
		gWlanShutdownStage = 4;
	}
	if (result < 0)
		robinWlanSetIoctlError(0x804116c6);
	return 0;
}

int robinWlanBeginPowerSaveEnable(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *output;
	int result;

	gWlanPowerSaveEnableStage = 1;
	result = robinWlanRequestEnableStationPowerSave(context, 0, 5, 0, 1,
					     &output, robinWlanCompletePowerSaveEnable);
	gWlanPowerSaveEnableStage = 2;
	if (result < 0)
		robinWlanSetIoctlError(0x804116c7);
	return 0;
}

int robinWlanBeginPowerSaveDisable(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *output;
	int result;

	gWlanPowerSaveDisableStage = 1;
	result = robinWlanRequestDisableStationPowerSave(context, &output,
						robinWlanCompletePowerSaveDisable);
	gWlanPowerSaveDisableStage = 2;
	if (result < 0)
		robinWlanSetIoctlError(0x804116c7);
	return 0;
}

int robinWlanBeginWakeOnConfiguration(SceWlanBtRobinWlanContext *context)
{
	robinWlanCompleteWakeOnConfiguration(context, NULL);
	return 0;
}

int robinWlanBeginHostSleepReset(SceWlanBtRobinWlanContext *context)
{
	int result;

	gWlanHostSleepResetStage = 1;
	robinWlanRecordPendingState(13, 1);
	switch (gWlanHostSleepResetStage) {
	case 1:
		result = robinWlanSetSdioPullControl(context, 0, 0, 4, 15,
						 robinWlanCompleteHostSleepReset);
		gWlanHostSleepResetStage = 2;
		break;
	case 2:
		result = robinWlanConfigureHostSleep(context, 0, 16, 0,
						robinWlanCompleteHostSleepReset);
		gWlanHostSleepResetStage = 3;
		break;
	case 3:
		result = robinWlanNotifyWorker(0x800);
		break;
	default:
		return 0;
	}
	if (result < 0)
		robinWlanSetIoctlError(0x804116c9);
	return 0;
}

int robinWlanBeginWakeOffConfiguration(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *command = NULL, *output;
	WlanScanResult *scan;
	WlanBssDesc *descriptor;
	uint8_t *cursor;
	unsigned int flags, count, i;
	int result = 0;

	/* Sony inlines the NULL-command entry of the completion handler here. */
	flags = context->wakeFlags;
	if (flags) {
		if (flags & 3)
			gWlanWakeOffStage = 1;
		else if ((flags & 4) && (context->status & 0x400))
			gWlanWakeOffStage = 2;
		else
			gWlanWakeOffStage = 3;
	} else if (!context->wakeOwnerProcess) {
		gWlanWakeOffStage = (ksceWlanBtGetConfiguration() & 0x100) ? 9 : 8;
	} else {
		gWlanWakeOffStage = 3;
	}
	robinWlanRecordCurrentState(6, gWlanWakeOffStage);
	switch (gWlanWakeOffStage) {
	case 1:
		output = NULL;
		result = robinWlanRequestStopAdhoc(context, &output,
			robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 3;
		break;
	case 2:
		output = NULL;
		result = robinWlanRequestDeauthentication(context, &output,
			robinWlanCompleteWakeOffConfiguration);
		__atomic_fetch_and(&context->status, ~0x400, __ATOMIC_RELEASE);
		context->blockAckTransmitState = 0;
		context->signal.signalQuality = 0;
		context->blockAckReceivePending = 0;
		context->deleteBlockAckPending = 0;
		context->signal.rssi = -100;
		gWlanWakeOffStage = 8;
		break;
	case 3:
		result = robinWlanRequestHostSleepAddressData(context,
			robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 4;
		break;
	case 4:
		robinWlanReadHostSleepAddresses(context, command->responsePayload, command->responseSize);
		if (context->peerCount) {
			gWlanHostSleepPeerIndex = 0;
			result = robinWlanRequestHostSleepDataForAddress(context,
				&context->peers[0].address, robinWlanCompleteWakeOffConfiguration);
			gWlanWakeOffStage = 5;
		} else {
			output = NULL;
			result = robinWlanRequestBackgroundScanQuery(context, &output,
				robinWlanCompleteWakeOffConfiguration);
			gWlanWakeOffStage = 7;
		}
		break;
	case 5:
		if (robinWlanReadHostSleepPeerData(context, command->responsePayload,
			command->responseSize, gWlanHostSleepPeerIndex) < 0) {
			for (i = 0; i < sizeof(context->peers[0].address.data); ++i)
				context->peers[gWlanHostSleepPeerIndex].address.data[i] = 0;
		}
		++gWlanHostSleepPeerIndex;
		if (gWlanHostSleepPeerIndex == context->peerCount) {
			result = robinWlanClearHostSleepAddressData(context,
				robinWlanCompleteWakeOffConfiguration);
			gWlanWakeOffStage = 6;
		} else {
			result = robinWlanRequestHostSleepDataForAddress(context,
				&context->peers[gWlanHostSleepPeerIndex].address,
				robinWlanCompleteWakeOffConfiguration);
		}
		break;
	case 6:
		output = NULL;
		result = robinWlanRequestBackgroundScanQuery(context, &output,
			robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 7;
		break;
	case 7:
		if (context->wakeFlags || context->wakeOwnerProcess) {
			scan = context->driver.allocate(&context->driver, 2048, 4);
			if (scan) {
				robinWlanReadBackgroundScanResponse(context, command, scan, 2048);
				count = 0;
				cursor = scan->bssDesc;
				if (scan->bssNum) {
					do {
						unsigned int ssidSize;

						descriptor = (WlanBssDesc *)cursor;
						ssidSize = descriptor->ssidSize;
						context->discoveredSsids[count].size = ssidSize;
						memcpy(context->discoveredSsids[count].ssid,
						       descriptor->ssid, ssidSize);
						cursor += descriptor->size;
						++count;
					} while (count < scan->bssNum && count != 10);
				}
				context->discoveredSsidCount = count;
				context->driver.free(&context->driver, scan);
			}
		}
		result = robinWlanRequestBackgroundScan(context, 0, NULL, 0, NULL, 0,
			0, 0, 0, NULL, 0, 0, 0, 0, &output, robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 8;
		break;
	case 8:
		result = robinWlanRequestFunctionShutdown(context,
			robinWlanCompleteWakeOffConfiguration);
		gWlanWakeOffStage = 9;
		break;
	case 9:
		result = robinWlanNotifyWorker(0x2000);
		break;
	}
	if (result < 0)
		robinWlanSetIoctlError(0x804116ca);
	return 0;
}

int robinWlanNotifyCommandCancellation(SceWlanBtRobinWlanContext *context)
{
	return ksceKernelSetEventFlag(context->tcm.eventFlag, 8);
}

int robinWlanTcmSetMulticastAddresses(SceWlanBtRobinWlanTcm *tcm,
				     const SceNetEtherAddr *addresses, unsigned int size)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	if (!addresses) {
		result = robinWlanSetMulticastAddresses(tcm->context, 32, NULL);
		if (result < 0)
			goto out;
	} else if (size / sizeof(*addresses) <= 32) {
		result = robinWlanSetMulticastAddresses(tcm->context,
						      size / sizeof(*addresses), addresses);
		if (result < 0)
			goto out;
	} else {
		tcm->control |= 0x100;
		result = robinWlanSetControl(tcm->context, tcm->control);
		goto out;
	}
	tcm->control &= 0xfeff;
	result = robinWlanSetControl(tcm->context, tcm->control);
out:
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmScan(SceWlanBtRobinWlanTcm *tcm, WlanScanConfig *configuration,
		    unsigned int capacity)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	result = robinWlanScan(tcm->context, configuration, capacity);
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int wlanMapBssType(uint8_t bssType);

int robinWlanTcmStartBackgroundScan(SceWlanBtRobinWlanTcm *tcm,
				  const WlanBackgroundScanConfig *configuration,
				  unsigned int capacity)
{
	unsigned int channelCount = 0;
	unsigned int ssidCount, wildcardCount;
	const SceWlanBtScanSsid *ssids, *wildcardSsids;
	uint8_t bssType;
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	if (configuration->ssidCount > 10)
		goto invalid;
	bssType = wlanMapBssType(configuration->bssType);
	if (!bssType)
		goto invalid;
	while (configuration->channels[channelCount] &&
	       wlanIsSupportedChannel(configuration->channels[channelCount])) {
		if (++channelCount == 14)
			break;
	}
	if (!channelCount)
		goto invalid;
	if (configuration->flags & 4) {
		ssids = NULL;
		ssidCount = 0;
		wildcardSsids = configuration->ssids;
		wildcardCount = configuration->ssidCount;
	} else {
		ssids = configuration->ssids;
		ssidCount = configuration->ssidCount;
		wildcardSsids = NULL;
		wildcardCount = 0;
	}
	result = robinWlanSetBackgroundScan(tcm->context, 1, ssids, ssidCount,
		wildcardSsids, wildcardCount, bssType, configuration->channelsPerScan,
		configuration->scanInterval, configuration->channels, channelCount,
		configuration->scanMode == 2, configuration->minScanTime,
		configuration->maxScanTime);
	if (result >= 0) {
		tcm->informationElementMaskEnabled = configuration->flags & 2;
		if (tcm->informationElementMaskEnabled)
			__builtin_memcpy(tcm->informationElementMask,
				configuration->informationElementMask,
				sizeof(tcm->informationElementMask));
		else
			memset(tcm->informationElementMask, 0, sizeof(tcm->informationElementMask));
	}
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
invalid:
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return 0x80418001;
}

int robinWlanTcmStopBackgroundScan(SceWlanBtRobinWlanTcm *tcm)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	result = robinWlanSetBackgroundScan(tcm->context, 0, NULL, 0, NULL, 0,
					  0, 0, 0, NULL, 0, 0, 0, 0);
	if (result >= 0)
		result = robinWlanReadBackgroundScan(tcm->context, NULL, 0);
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmReadBackgroundScan(SceWlanBtRobinWlanTcm *tcm,
				 WlanScanResult *scanResult, unsigned int capacity)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	result = robinWlanReadBackgroundScan(tcm->context, scanResult, capacity);
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmAssociate(SceWlanBtRobinWlanTcm *tcm,
			const WlanBssDesc *descriptor,
			const WlanConnectionSecurity *security,
			const void *informationElements,
			unsigned int informationElementsSize)
{
	int retries;
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	retries = tcm->context->status & 0xc00;
	if (retries) {
		result = 0x80418002;
		goto unlock;
	}
	memset(&tcm->context->bss, 0,
	       offsetof(SceWlanBtRobinWlanContext, wakeScan) -
	       offsetof(SceWlanBtRobinWlanContext, bss));
	result = ksceKernelClearEventFlag(tcm->eventFlag, 0);
	if (result < 0)
		goto unlock;
	result = robinWlanSetBssMode(tcm->context, 0);
	if (result < 0)
		goto unlock;
	for (;;) {
		result = mrvlExecAssociate(tcm->context, descriptor, security,
			informationElements, informationElementsSize, &tcm->context->htEnabled);
		if (result >= 0)
			break;
		if (retries > 1 || result != (int)0x80411605)
			goto unlock;
		++retries;
	}
	result = robinWlanSetTransmitRate(tcm->context, 0x100);
	if (result < 0)
		goto deauthenticate;
	tcm->control |= 0x13;
	result = robinWlanSetControl(tcm->context, tcm->control);
	if (result < 0)
		goto deauthenticate;
	switch (security->securityMode) {
	case 1:
		tcm->control |= 8;
		result = robinWlanSetControl(tcm->context, tcm->control);
		if (result < 0)
			goto deauthenticate;
		break;
	case 2:
	case 3:
	case 4: {
		SceUInt timeout = 30000000;
		unsigned int events = 0;

		result = ksceKernelWaitEventFlag(tcm->eventFlag, 15, 5, &events, &timeout);
		if (result < 0)
			goto deauthenticate;
		if (events & 2) {
			result = 0x80411607;
			goto deauthenticate;
		}
		if (events & 4) {
			result = 0x80411608;
			goto deauthenticate;
		}
		if (events & 8) {
			result = 0x80411683;
			goto deauthenticate;
		}
	}
		/* fall through */
	case 0:
		tcm->control &= ~8;
		result = robinWlanSetControl(tcm->context, tcm->control);
		if (result < 0)
			goto deauthenticate;
		break;
	default:
		result = 0x80418008;
		goto deauthenticate;
	}
	if (tcm->context->htEnabled) {
		SceWlanBtRobinWlanContext *context;

		result = robinWlanSetHtTransmit(tcm->context, 32, 0);
		if (result < 0)
			goto deauthenticate;
		context = tcm->context;
		context->blockAckPacketThreshold = (sceKernelGetSystemTimeWide() & 15) + 16;
	}
	__atomic_fetch_or(&tcm->context->status, 0x400, __ATOMIC_RELEASE);
	memcpy(&tcm->context->bss, descriptor, sizeof(*descriptor));
	result = robinWlanGetRssi(tcm->context, &tcm->context->signal);
	{
		SceWlanBtRobinWlanContext *context = tcm->context;

		context->signalTimestamp = sceKernelGetSystemTimeWide();
	}
	if (result >= 0)
		goto unlock;
deauthenticate:
	robinWlanDeauthenticate(tcm->context);
unlock:
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmDisconnect(SceWlanBtRobinWlanTcm *tcm)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	result = tcm->context->status & 0x400;
	if (result) {
		result = robinWlanDeauthenticate(tcm->context);
		if (result >= 0) {
			SceWlanBtRobinWlanContext *context;

			__atomic_fetch_and(&tcm->context->status, ~0x400, __ATOMIC_RELEASE);
			context = tcm->context;
			context->blockAckTransmitState = 0;
			context->signal.signalQuality = 0;
			context->blockAckReceivePending = 0;
			context->deleteBlockAckPending = 0;
			context->signal.rssi = -100;
		}
	}
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmSetStationPowerSave(SceWlanBtRobinWlanTcm *tcm, int enabled)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	if (enabled)
		result = robinWlanEnableStationPowerSave(tcm->context, 0, 5, 0, 1);
	else
		result = robinWlanDisableStationPowerSave(tcm->context);
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmStartAdhoc(SceWlanBtRobinWlanTcm *tcm,
			 WlanBssConnectionConfig *configuration, unsigned int capacity)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	if (tcm->context->status & 0xc00) {
		result = 0x80418002;
		goto unlock;
	}
	tcm->control |= 0x13;
	result = robinWlanSetControl(tcm->context, tcm->control);
	if (result < 0)
		goto unlock;
	tcm->control &= ~8;
	result = robinWlanSetControl(tcm->context, tcm->control);
	if (result < 0)
		goto unlock;
	result = robinWlanSetBssMode(tcm->context, 1);
	if (result < 0)
		goto unlock;
	result = robinWlanSetIbssCoalescing(tcm->context, !(configuration->flags & 2));
	if (result < 0)
		goto unlock;
	result = robinWlanStartAdhoc(tcm->context, configuration, capacity);
	if (result < 0)
		goto unlock;
	result = robinWlanSetTransmitRate(tcm->context, (configuration->flags & 1) ? 3 : 257);
	if (result < 0)
		robinWlanStopAdhoc(tcm->context);
	else
		__atomic_fetch_or(&tcm->context->status, 0x800, __ATOMIC_RELEASE);
unlock:
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmJoinAdhoc(SceWlanBtRobinWlanTcm *tcm,
			WlanBssConnectionConfig *configuration, unsigned int capacity)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	if (tcm->context->status & 0xc00) {
		result = 0x80418002;
		goto unlock;
	}
	tcm->control |= 0x13;
	result = robinWlanSetControl(tcm->context, tcm->control);
	if (result < 0)
		goto unlock;
	tcm->control &= ~8;
	result = robinWlanSetControl(tcm->context, tcm->control);
	if (result < 0)
		goto unlock;
	result = robinWlanSetBssMode(tcm->context, 1);
	if (result < 0)
		goto unlock;
	result = robinWlanSetIbssCoalescing(tcm->context, !(configuration->flags & 2));
	if (result < 0)
		goto unlock;
	result = robinWlanJoinAdhoc(tcm->context, configuration);
	if (result < 0)
		goto unlock;
	result = robinWlanSetTransmitRate(tcm->context, (configuration->flags & 1) ? 3 : 257);
	if (result < 0)
		robinWlanStopAdhoc(tcm->context);
	else
		__atomic_fetch_or(&tcm->context->status, 0x800, __ATOMIC_RELEASE);
unlock:
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmStopAdhoc(SceWlanBtRobinWlanTcm *tcm)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	result = tcm->context->status & 0x800;
	if (result) {
		robinWlanSetTransmitRate(tcm->context, 0x100);
		result = robinWlanStopAdhoc(tcm->context);
		if (result >= 0)
			__atomic_fetch_and(&tcm->context->status, ~0x800, __ATOMIC_RELEASE);
	}
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanTcmSetManagementInformationElements(SceWlanBtRobinWlanTcm *tcm,
					       const void *informationElements, size_t size)
{
	int result;

	ksceKernelLockFastMutex(&tcm->mutex);
	result = robinWlanSetManagementInformationElements(tcm->context,
							 informationElements, size);
	ksceKernelUnlockFastMutex(&tcm->mutex);
	return result;
}

int robinWlanBeginSleep(SceWlanBtRobinWlanContext *context)
{
	int result;

	gWlanSleepStage = 1;
	result = robinWlanConfirmSleep(context, robinWlanCompleteSleep);
	gWlanSleepStage = 2;
	if (result < 0)
		robinWlanSetIoctlError(0x804116c7);
	return 0;
}

/* Variable-length event message carried by SDIO message type 3. */
typedef struct WlanEventMessage {
	uint16_t eventId;
	uint8_t callerMetadata[2];
	uint8_t payload[];
} WlanEventMessage;

_Static_assert(sizeof(WlanEventMessage) == 4, "event message header");
_Static_assert(offsetof(WlanEventMessage, payload) == 4, "event message payload");

void robinWlanTcmEventReceive(SceWlanBtRobinWlanContext *context, const void *input)
{
	const WlanEventMessage *event = input;
	unsigned int eventId = event->eventId & 0xfff;
	struct NetDriver *driver = &context->driver;

	if (!driver)
		return;
	switch (eventId) {
	case 3:
		if (context->status & 0x200000)
			context->wakeReasonFlags |= 8;
		else
			driver->notify(driver, NULL, NET_DRIVER_EVENT_LINK_UP, 0, NULL, 0);
		break;
	case 8:
		ksceKernelSetEventFlag(context->tcm.eventFlag, 2);
		driver->notify(driver, NULL, NET_DRIVER_EVENT_LINK_DOWN, 0, NULL, 0);
		break;
	case 9:
		ksceKernelSetEventFlag(context->tcm.eventFlag, 4);
		driver->notify(driver, NULL, 8, 0, NULL, 0);
		break;
	case 10:
		robinWlanNotifyWorker(0x20000);
		break;
	case 11:
		robinWlanNotifyWorker(0x20000);
		robinWlanNotifyWorker(0x4000);
		break;
	case 24:
		driver->notify(driver, NULL, 16, 0, NULL, 0);
		break;
	case 25:
		if (!(context->status & 0x800))
			driver->notify(driver, NULL, 4, 0, NULL, 0);
		break;
	case 43:
		ksceKernelSetEventFlag(context->tcm.eventFlag, 1);
		break;
	case 51:
		if ((context->status & 8) && !context->blockAckReceivePending) {
			if (!robinWlanRespondAddBlockAck(context, (const void *)event->payload,
						       robinWlanCompleteAsyncCommand))
				context->blockAckReceivePending = 1;
		}
		break;
	case 52:
		if (context->status & 0x400) {
			uint8_t initiator;
			uint16_t reason;

			robinWlanReadDeleteBlockAckReason((const void *)event->payload,
							&initiator, &reason);
			if (initiator != 1)
				context->blockAckTransmitState = reason == 37 ? 2 : 0;
		}
		break;
	case 55:
		if ((context->status & 8) && !context->deleteBlockAckPending) {
			SceNetEtherAddr peer;
			uint8_t result;
			uint8_t parameterFlags;

			robinWlanReadDeleteBlockAckResult((const void *)event->payload,
							&peer, &result, &parameterFlags);
			if (!robinWlanRequestDeleteBlockAck(context, 0, &peer,
					(result << 12) | (parameterFlags << 11), 39,
					robinWlanCompleteAsyncCommand))
				context->deleteBlockAckPending = 1;
		}
		break;
	case 79:
		if (context->status & 0x200000)
			context->wakeReasonFlags |= 4;
		break;
	default:
		break;
	}
}

int robinWlanRequestTransmit(SceWlanBtRobinWlanDcm *dcm);
int robinWlanRequestAddBlockAck(SceWlanBtRobinWlanContext *context,
	const SceNetEtherAddr *peer, uint16_t bufferSize, uint16_t tid,
	uint8_t policy, uint8_t amsdu, uint16_t timeout, uint16_t startingSequence,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *));

int robinWlanProcessTransmit(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *output;

	if ((context->status & 0x400) && context->htEnabled &&
	    !context->blockAckTransmitState &&
	    context->transmittedPackets > context->blockAckPacketThreshold) {
		output = NULL;
		if (robinWlanRequestAddBlockAck(context, &context->bss.bssid,
			32, 0, 1, 0, 0, 0, &output, robinWlanCompleteAsyncCommand) == 0)
			context->blockAckTransmitState = 1;
	}
	return robinWlanRequestTransmit(&context->dcm);
}

int robinWlanRequestRssiUpdate(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *command = NULL;

	return robinWlanRequestRssi(context, &command, robinWlanCompleteAsyncCommand);
}

/* This operation consumes a halfword rate, unlike the word scalar controls. */
typedef struct WlanTransmitRateControl {
	uint8_t callerMetadata[8];
	uint16_t rate;
} WlanTransmitRateControl;

_Static_assert(sizeof(WlanTransmitRateControl) == 10, "transmit-rate operation");
_Static_assert(offsetof(WlanTransmitRateControl, rate) == 8, "transmit rate");

int robinWlanTcmSetTransmitRate(SceWlanBtRobinWlanTcm *tcm, const void *input)
{
	const WlanTransmitRateControl *request = input;

	return robinWlanSetTransmitRate(tcm->context, request->rate);
}

typedef struct WlanRetryLimitControl {
	uint8_t callerMetadata[8];
	uint16_t shortRetryLimit;
	uint16_t longRetryLimit;
} WlanRetryLimitControl;

_Static_assert(sizeof(WlanRetryLimitControl) == 12, "retry-limit operation");
_Static_assert(offsetof(WlanRetryLimitControl, shortRetryLimit) == 8, "short retry limit");
_Static_assert(offsetof(WlanRetryLimitControl, longRetryLimit) == 10, "long retry limit");

int robinWlanReadSetting(SceWlanBtRobinWlanContext *context, uint16_t settingId,
			void *data, unsigned int size);
int robinWlanWriteSetting(SceWlanBtRobinWlanContext *context, uint16_t settingId,
			 const void *data, unsigned int size);

int robinWlanTcmReadSettings(SceWlanBtRobinWlanTcm *tcm, void *input)
{
	WlanRetryLimitControl *request = input;
	uint8_t value = 0;
	int result;

	result = robinWlanReadSetting(tcm->context, 6, &value, sizeof(value));
	if (result >= 0) {
		uint8_t shortRetryLimit = value;
		SceWlanBtRobinWlanContext *context = tcm->context;

		value = 0;
		request->shortRetryLimit = shortRetryLimit;
		result = robinWlanReadSetting(context, 7, &value, sizeof(value));
		if (result >= 0)
			request->longRetryLimit = value;
	}
	return result;
}

int robinWlanTcmWriteSettings(SceWlanBtRobinWlanTcm *tcm, const void *input)
{
	const WlanRetryLimitControl *request = input;
	uint8_t value = request->shortRetryLimit;
	int result;

	result = robinWlanWriteSetting(tcm->context, 6, &value, sizeof(value));
	if (result >= 0) {
		value = request->longRetryLimit;
		result = robinWlanWriteSetting(tcm->context, 7, &value, sizeof(value));
	}
	return result;
}

typedef struct WlanCardInfo {
	uint8_t callerMetadata[8];
	uint32_t cardRevision;
	SceNetEtherAddr address;
	uint8_t reserved[2];
	uint32_t addressSize;
} WlanCardInfo;

_Static_assert(sizeof(WlanCardInfo) == 24, "card information operation");
_Static_assert(offsetof(WlanCardInfo, cardRevision) == 8, "card revision");
_Static_assert(offsetof(WlanCardInfo, address) == 12, "card MAC");
_Static_assert(offsetof(WlanCardInfo, addressSize) == 20, "card MAC size");

int robinWlanGetMacAddress(SceWlanBtRobinWlanContext *context, SceNetEtherAddr *address);
int robinGetCardRevision(void);

int robinWlanTcmGetCardInfo(SceWlanBtRobinWlanTcm *tcm, void *input)
{
	WlanCardInfo *information = input;
	int result;

	result = robinWlanGetMacAddress(tcm->context, &information->address);
	if (result >= 0) {
		information->addressSize = sizeof(information->address);
		information->cardRevision = robinGetCardRevision();
	}
	return result;
}

int robinWlanTcmConfigureOutput(SceWlanBtRobinWlanTcm *tcm, int output,
			       unsigned int value)
{
	int result;

	result = robinWlanWriteMemory(tcm->context, 0x80002200, 1U << output);
	if (result < 0)
		return result;
	switch (output) {
	case 15:
		result = robinWlanWriteMemory(tcm->context, 0x90000204, 0x4000);
		if (result >= 0) {
			result = robinWlanWriteMemory(tcm->context, 0x90000208, 0x7fff);
			if (result >= 0)
				return robinWlanWriteMemory(tcm->context, 0x9000020c, value << 15);
		}
		break;
	case 16:
		result = robinWlanWriteMemory(tcm->context, 0x9000025c, 1);
		if (result >= 0) {
			result = robinWlanWriteMemory(tcm->context, 0x90000264, 0xfffe);
			if (result >= 0)
				return robinWlanWriteMemory(tcm->context, 0x90000268, value);
		}
		break;
	case 4:
		result = robinWlanWriteMemory(tcm->context, 0x90000200, 0x100);
		if (result >= 0) {
			result = robinWlanWriteMemory(tcm->context, 0x90000208, 0xffef);
			if (result >= 0)
				return robinWlanWriteMemory(tcm->context, 0x9000020c, value << 4);
		}
		break;
	default:
		return 0x80418008;
	}
	return result;
}

int robinWlanTcmReadInput(SceWlanBtRobinWlanTcm *tcm, int input, unsigned int *value)
{
	unsigned int mask = 1U << input;
	unsigned int sample, address;
	int result;

	result = robinWlanWriteMemory(tcm->context, 0x80002200, mask);
	if (result < 0)
		return result;
	if (input == 15) {
		address = 0x90000204;
	} else if (input == 16) {
		result = robinWlanWriteMemory(tcm->context, 0x9000025c, 0);
		if (result < 0)
			return result;
		result = robinWlanWriteMemory(tcm->context, 0x90000264, 0xffff);
		if (result < 0)
			return result;
		address = 0x9000026c;
		goto read;
	} else if (input == 4) {
		address = 0x90000200;
	} else {
		return 0x80418008;
	}
	result = robinWlanWriteMemory(tcm->context, address, 0);
	if (result < 0)
		return result;
	result = robinWlanWriteMemory(tcm->context, 0x90000208, 0xffff);
	if (result < 0)
		return result;
	address = 0x90000210;
read:
	result = robinWlanReadMemory(tcm->context, address, &sample);
	if (result >= 0)
		*value = (mask & sample) != 0;
	return result;
}

int robinWlanTcmSetEnergyDetectionMacOffset(SceWlanBtRobinWlanTcm *tcm, int16_t offset)
{
	return robinWlanSetEnergyDetectionMacOffset(tcm->context, offset);
}

int robinWlanTcmDisableEnergyDetectionMac(SceWlanBtRobinWlanTcm *tcm)
{
	return robinWlanDisableEnergyDetectionMac(tcm->context);
}
