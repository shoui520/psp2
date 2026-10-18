#include <udcd/udcd_imports.h>
#include "udcd_internal.h"
#include <string.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/procevent.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/intrmgr.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <sysmem/sysmem.h>
#include <kernel/error.h>
#include <sysmem/suspend.h>

struct udcdState udcdState = { 0 };

void udcdUpdateStateEvent(int bus);

/* Original name unknown. Signal PHY readiness for the controller IRQ. */
int udcdPhyInterrupt(int interrupt, void *argp)
{
	volatile ScePervasive2Registers *registers;
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	unsigned int mask, ready;
	int bus;

	if (interrupt == 0xbe) {
		mask = 1;
		bus = 0;
	} else if (interrupt == 0xa4) {
		mask = 2;
		bus = 1;
	} else if (interrupt == 0xaa) {
		mask = 4;
		bus = 2;
	} else {
		mask = 0;
		bus = -1;
	}
	state = &udcdState.buses[bus];
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	registers = udcdState.registers;
	registers->usbPhyInterrupt = mask;
	ready = registers->usbPhyReady;
	if ((unsigned int)bus < 32 && ((ready >> bus) & 1))
		sceKernelSetEventFlag(state->phyReadyEvent, 1);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return -1;
}

/* Original names unknown. Initialize the registered bus callbacks. */
int udcdInitializeBusTwo(int args, void *argp)
{
	struct udcdBusState *state = &udcdState.buses[2];
	SceKernelIntrStatus status;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	state->suspendState = -1;
	state->cableState = SCE_UDCD_STATUS_CABLE_DISCONNECTED;
	state->stringDescriptorCount = 0;
	state->endpointCount = 1;
	state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
	state->controllerState = UDCD_CONTROLLER_STOPPED;
	state->resumeControllerState = UDCD_CONTROLLER_STOPPED;
	state->interfaceCount = 0;
	state->drivers = NULL;
	state->languageDescriptor = NULL;
	udcdUpdateStateEvent(2);
	state->endpoints[0].direction = 0xff;
	state->requestBlocked = 0;
	state->endpoints[0].type = 0;
	state->endpoints[0].requestTail = NULL;
	state->endpoints[0].requestHead = NULL;
	state->endpoints[0].endpoint = &state->controlEndpoint;
	state->controlEndpoint.transmittedBytes = 0;
	state->controlEndpoint.driverEndpointNumber = 0;
	state->controlEndpoint.endpointNumber = 0;
	udcdState.operations[2].initializeQueues(2);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return 0;
}

int udcdInitializeBusOne(int args, void *argp)
{
	struct udcdBusState *state = &udcdState.buses[1];
	SceKernelIntrStatus status;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	state->suspendState = -1;
	state->cableState = SCE_UDCD_STATUS_CABLE_DISCONNECTED;
	state->controllerState = UDCD_CONTROLLER_STOPPED;
	state->resumeControllerState = UDCD_CONTROLLER_STOPPED;
	state->interfaceCount = 0;
	state->drivers = NULL;
	state->languageDescriptor = NULL;
	state->stringDescriptorCount = 0;
	state->endpointCount = 1;
	state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
	udcdUpdateStateEvent(1);
	state->endpoints[0].endpoint = &state->controlEndpoint;
	state->endpoints[0].direction = 0xff;
	state->requestBlocked = 0;
	state->controlEndpoint.endpointNumber = 0;
	state->controlEndpoint.driverEndpointNumber = 0;
	state->controlEndpoint.transmittedBytes = 0;
	state->endpoints[0].type = 0;
	state->endpoints[0].requestTail = NULL;
	state->endpoints[0].requestHead = NULL;
	udcdState.operations[1].initializeQueues(1);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return 0;
}

/* Original name unknown. Poll the PHY with the native bounded busy delay. */
int udcdPollBusInitialized(unsigned int delay, int bus)
{
	volatile ScePervasive2Registers *registers;
	volatile unsigned int remaining;
	unsigned int mask;
	int attempts;

	if (bus < 0 || bus > 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	registers = udcdState.registers;
	mask = 1u << bus;
	for (attempts = 10; attempts; attempts--) {
		remaining = delay * 45;
		while (remaining)
			remaining--;
		if (registers->usbPhyReady & mask)
			break;
	}
	return registers->usbPhyReady & mask ? 0 : UDCD_ERROR_TIMEOUT;
}

int ksceUdcdWaitBusInitialized(unsigned int timeout, int bus)
{
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	int result;

	if (bus < 0 || bus > 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	state = &udcdState.buses[bus];
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (udcdState.registers->usbPhyReady & (1u << bus)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
		return 0;
	}
	sceKernelClearEventFlag(state->phyReadyEvent, ~1u);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	result = sceKernelWaitEventFlag(state->phyReadyEvent, 1, SCE_KERNEL_EVF_WAITMODE_OR, NULL,
				       timeout ? &timeout : NULL);
	return result == SCE_KERNEL_ERROR_WAIT_TIMEOUT ? UDCD_ERROR_TIMEOUT : result;
}

int ksceUdcdWaitStateInternal(SceUdcdWaitParam *parameter, unsigned int timeout,
			    int bus)
{
	struct udcdBusState *state;
	unsigned int bits = 0, pattern;
	unsigned int *remaining;
	int result;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	state = &udcdState.buses[bus];
	if (!parameter || ((unsigned int)parameter->controller & ~0x3000u) ||
	    ((unsigned int)parameter->status & ~0x700u) ||
	    ((unsigned int)parameter->cable & ~0x30u) ||
	    ((unsigned int)parameter->connection & ~0xfu))
		return UDCD_ERROR_INVALID_ARGUMENT;
	remaining = timeout ? &timeout : NULL;
	for (;;) {
		while (parameter->driverName &&
		       ksceUdcdGetDrvStateInternal(parameter->driverName, bus) != 2) {
			result = sceKernelWaitEventFlag(state->stateEvent, 0x80004000,
						       SCE_KERNEL_EVF_WAITMODE_OR |
						       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits,
						       remaining);
			if (state->waitError)
				return state->waitError;
			if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				return UDCD_ERROR_TIMEOUT;
			if (result < 0)
				return UDCD_ERROR_INTERRUPTED;
		}
		pattern = parameter->controller | parameter->status | parameter->cable |
			  parameter->connection;
		if (!pattern)
			return 0;
		result = sceKernelWaitEventFlag(state->stateEvent, pattern | 0x80004000,
					       SCE_KERNEL_EVF_WAITMODE_OR, &bits, remaining);
		if (state->waitError)
			return state->waitError;
		if (result < 0) {
			if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				return UDCD_ERROR_TIMEOUT;
			if (result == SCE_KERNEL_ERROR_WAIT_CANCEL ||
			    result == SCE_KERNEL_ERROR_THREAD_SUSPENDED ||
			    result == SCE_KERNEL_ERROR_THREAD_STOPPED)
				return UDCD_ERROR_INTERRUPTED;
			return result;
		}
		if (!(bits & 0x4000))
			return 0;
	}
}

int ksceUdcdWaitState(SceUdcdWaitParam *parameter, unsigned int timeout)
{
	return ksceUdcdWaitStateInternal(parameter, timeout, 2);
}

/* Original name unknown. Keep USB activity awake while its alarm is enabled. */
SceUInt32 udcdAlarmTick(void *argp)
{
	int bus = (int)(uintptr_t)argp;
	struct udcdBusState *state;
	unsigned int powerStatus;

	if (!bus)
		return UDCD_ERROR_INVALID_ARGUMENT;
	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	state = &udcdState.buses[bus];
	state->alarmTickTimer = -1;
	ScePowerForDriver_8C0D2051(&powerStatus);
	if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED && state->busPowered == 1 && state->chargingEnabled == 1 &&
	    (powerStatus & 0xf00) != 0x200) {
		SceKernelSuspendForDriver_E0489831(1);
		state->alarmTickTimer = SceThreadmgrForDriver_C58DF384("SceUdcdAlarmTickTimer",
			5000000, udcdAlarmTick, argp);
	}
	return 0;
}

/* Original name unknown. Publish state and update its timer/callback policy. */
static inline __attribute__((always_inline)) void udcdPublishState(int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	unsigned int pattern, changed;
	int configured;

	sceKernelClearEventFlag(state->stateEvent, 0);
	pattern = state->controllerState | state->activationState | state->cableState | state->connectionState;
	if (state->waitError)
		pattern |= UDCD_STATE_ERROR;
	sceKernelSetEventFlag(state->stateEvent, pattern);
	if (bus == 2 && state->busPowered == 1 && state->chargingEnabled == 1) {
		configured = state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED;
		if (state->alarmTickTimer > 0) {
			sceKernelCancelAlarm(state->alarmTickTimer);
			state->alarmTickTimer = -1;
		}
		if (configured)
			state->alarmTickTimer = SceThreadmgrForDriver_C58DF384("SceUdcdAlarmTickTimer",
				0, udcdAlarmTick, (void *)(uintptr_t)2);
	}
	changed = state->activationState & ~state->notifiedMask;
	if (state->callbackThread != -1 && (changed & state->callbackMask) &&
	    sceKernelSetEventWithNotifyCallback(state->callbackEvent, 0x1000000, 0, changed) >= 0)
		state->notifiedMask = changed;
}

void udcdUpdateStateEvent(int bus)
{
	udcdPublishState(bus);
}

int SceUdcdForDriver_EBB1E86B(void)
{
	int waiters;

	if (!udcdIsDriverBus(2))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	sceKernelCancelEventFlag(udcdState.buses[2].stateEvent, 0, &waiters);
	udcdPublishState(2);
	return 0;
}

int SceUdcdForDriver_DB7EF7AB(int bus)
{
	int waiters;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	sceKernelCancelEventFlag(udcdState.buses[bus].stateEvent, 0, &waiters);
	udcdPublishState(bus);
	return 0;
}

int ksceUdcdGetDeviceState(void)
{
	struct udcdBusState *state = &udcdState.buses[2];

	if (!udcdIsDriverBus(2))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	return state->activationState | state->controllerState | state->cableState | state->connectionState;
}

int ksceUdcdGetDeviceStateInternal(int bus)
{
	struct udcdBusState *state;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	state = &udcdState.buses[bus];
	return state->activationState | state->controllerState | state->cableState | state->connectionState;
}

int SceUdcdForDriver_78BF2A28(const struct udcdDeviceInformation *information)
{
	if (!udcdIsDriverBus(2))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	udcdState.buses[2].pendingEventDetail = information->eventDetail;
	return 0;
}

int SceUdcdForDriver_E4F32EDA(const struct udcdDeviceInformation *information, int bus)
{
	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	udcdState.buses[bus].pendingEventDetail = information->eventDetail;
	return 0;
}

void SceUdcdForDriver_B1454F11(const unsigned int *value)
{
	if (value)
		udcdState.buses[2].driverSetting = *value;
}

int ksceUdcdGetDeviceInfo(SceUdcdDeviceInfo *information)
{
	struct udcdDeviceInformation *record = (struct udcdDeviceInformation *)information;

	if (!udcdIsDriverBus(2))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	record->lastEvent = udcdState.buses[2].deviceInformation.lastEvent;
	record->eventDetail = udcdState.buses[2].deviceInformation.eventDetail;
	return 0;
}

int ksceUdcdGetDeviceInfoInternal(SceUdcdDeviceInfo *information, int bus)
{
	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	memcpy(information, &udcdState.buses[bus].deviceInformation,
	       sizeof(udcdState.buses[bus].deviceInformation));
	return 0;
}

void udcdMakeDeviceQualifier(const SceUdcdDeviceDescriptor *device,
			     SceUdcdDeviceQualifierDescriptor *qualifier);
int udcdSerializeConfiguration(const SceUdcdConfigDescriptor *config, void *buffer);

/* Original name unknown. Refresh advertised bus power and battery charging. */
static inline __attribute__((always_inline)) void udcdApplyUsbCharging(int bus)
{
	struct udcdBusState *state = &udcdState.buses[2];
	unsigned char attributes, maxPower;
	int enabled;

	if (bus != 2)
		return;
	if (state->controllerState != UDCD_CONTROLLER_STARTED) {
		enabled = state->chargingEnabled == 1;
	} else {
		enabled = state->busPowered == 1 && state->chargingEnabled == 1;
		if (state->cableState != SCE_UDCD_STATUS_CABLE_CONNECTED) {
			attributes = enabled ? 0x80 : 0xc0;
			maxPower = enabled ? 0xfa : 1;
			state->fullSpeedConfiguration->configDescriptors->bmAttributes = attributes;
			state->fullSpeedConfiguration->configDescriptors->bMaxPower = maxPower;
			udcdMakeDeviceQualifier(state->fullSpeedDevice, state->fullSpeedQualifier);
			if (udcdSerializeConfiguration(state->fullSpeedConfiguration->configDescriptors,
						       state->fullSpeedDescriptorData) < 0)
				return;
			if (state->highSpeedConfiguration) {
				state->highSpeedConfiguration->configDescriptors->bmAttributes = attributes;
				state->highSpeedConfiguration->configDescriptors->bMaxPower = maxPower;
				udcdMakeDeviceQualifier(state->highSpeedDevice, state->highSpeedQualifier);
				if (udcdSerializeConfiguration(
					state->highSpeedConfiguration->configDescriptors,
					state->highSpeedDescriptorData) < 0)
					return;
			}
			SceCpuForDriver_103872A5(state->fullSpeedDescriptorData, 0x1000);
		}
	}
	if (enabled)
		kscePowerBatteryEnableUsbCharging();
	else
		kscePowerBatteryDisableUsbCharging();
	ScePowerForDriver_F535D928(0x8000, enabled);
}

void udcdUpdateUsbCharging(int bus)
{
	udcdApplyUsbCharging(bus);
}

/* Original name unknown. Shut down a controller and release its descriptors. */
int udcdDeactivateBus(int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelIntrStatus status;
	SceUdcdDriver *driver;
	int result, index;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (state->controllerDriverState == UDCD_CONTROLLER_DRIVER_ABSENT) {
		result = SCE_UDCD_ERROR_ALREADY_DONE;
		goto unlock;
	}
	if (state->systemSuspended == 1) {
		result = UDCD_ERROR_SUSPENDED;
		goto unlock;
	}
	if (state->controllerState == UDCD_CONTROLLER_STOPPED) {
		result = SCE_UDCD_ERROR_INVALID_ARGUMENT;
		goto unlock;
	}
	state->deviceInformation.lastEvent = 0x20000;
	result = udcdState.operations[bus].stop(bus, &status);
	if (result < 0)
		goto unlock;
	result = SCE_UDCD_ERROR_WAIT_CANCEL;
	if (SceSysmemForDriver_009E1C61(state->descriptorMemBlock) < 0)
		goto unlock;
	state->descriptorMemBlock = 0;
	if (SceSysmemForDriver_009E1C61(state->deviceMemBlock) < 0)
		goto unlock;
	state->deviceMemBlock = 0;
	if (SceSysmemForDriver_009E1C61(state->fullSpeedConfigMemBlock) < 0)
		goto unlock;
	state->fullSpeedConfigMemBlock = 0;
	if (state->highSpeedConfiguration) {
		if (SceSysmemForDriver_009E1C61(state->highSpeedConfigMemBlock) < 0)
			goto unlock;
		state->highSpeedConfigMemBlock = 0;
	}
	state->controllerState = UDCD_CONTROLLER_STOPPED;
	state->resumeControllerState = UDCD_CONTROLLER_STOPPED;
	state->deviceInformation.eventDetail = state->pendingEventDetail;
	state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
	state->pendingEventDetail = 0;
	udcdUpdateStateEvent(bus);
	for (index = 1; index < 8; index++)
		state->endpoints[index].endpoint = NULL;
	for (driver = state->drivers; driver; driver = driver->link) {
		driver->endpoints[0].endpointNumber = 0;
		for (index = 1; index < driver->numEndpoints; index++)
			driver->endpoints[index].endpointNumber = 0;
	}
	state->languageDescriptor = NULL;
	state->stringDescriptorCount = 0;
	state->busPowered = 0;
	result = 0;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

int ksceUdcdDeactivateInternal(int bus)
{
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	unsigned int enabled;

	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	state = &udcdState.buses[bus];
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	enabled = state->activationEnabled;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return enabled ? udcdDeactivateBus(bus) : SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
}

int ksceUdcdDeactivate(void)
{
	struct udcdBusState *state = &udcdState.buses[2];
	SceKernelIntrStatus status;
	unsigned int enabled;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	enabled = state->activationEnabled;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return enabled ? udcdDeactivateBus(2) : SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
}

/* Original name unknown. Map each driver's endpoint aliases to hardware slots. */
int udcdAssignEndpointNumbers(int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDriver *driver;
	SceUdcdEndpoint *endpoint, *candidate;
	int occupied[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
	int index, alias, number, direction;

	for (driver = state->drivers; driver; driver = driver->link) {
		driver->endpoints[0].endpointNumber = 0;
		for (index = 1; index < driver->numEndpoints; index++) {
			endpoint = &driver->endpoints[index];
			for (alias = 1; alias < driver->numEndpoints; alias++) {
				candidate = &driver->endpoints[alias];
				if (candidate->driverEndpointNumber != endpoint->driverEndpointNumber ||
				    occupied[candidate->endpointNumber] == -1)
					continue;
				if (endpoint->direction != candidate->direction)
					return 0x80243305;
				endpoint->endpointNumber = candidate->endpointNumber;
				break;
			}
			if (endpoint->endpointNumber)
				continue;
			for (number = 1; number < 8; number++) {
				direction = number & 1 ? 0x80 : 0;
				if (occupied[number] == -1 && endpoint->direction == direction)
					break;
			}
			if (number == 8)
				return 0x80243308;
			endpoint->endpointNumber = number;
			occupied[number] = number;
			state->endpoints[number].endpoint = endpoint;
		}
	}
	return 0;
}

/* Original name unknown. A process dies: drop its state callback. */
int udcdProcessKill(SceUID pid, ProcEventParam *param, int unk)
{
	udcdUnregisterCallback(pid, 0);
	return 0;
}

/* Original name unknown. Build the combined full/high-speed configuration. */
SceUdcdConfiguration *udcdBuildConfiguration(int speed, const int *stringCounts,
					    int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdConfigurationAllocation *allocation;
	SceKernelAllocMemBlockKernelOpt options;
	SceUdcdConfiguration *source;
	SceUdcdDriver *driver;
	SceUdcdInterfaceSettings *settings, *driverSettings;
	SceUdcdInterfaceDescriptor *interface, *sourceInterface;
	SceUdcdEndpointDescriptor *endpoint, *sourceEndpoint;
	SceUdcdEndpoint *driverEndpoint;
	unsigned char *extra;
	unsigned int required, wireLength = 0;
	int interfaceCounts[9], descriptorCounts[9], endpointCounts[9];
	int drivers = 0, interfaces = 0, descriptors = 0, endpoints = 0;
	int index, settingIndex, descriptorIndex, endpointIndex, alias;
	int interfaceOffset = 0, stringOffset, number, direction;
	SceUID *memBlock;

	for (driver = state->drivers; driver; driver = driver->link) {
		source = speed == 1 ? driver->configuration : driver->configuration_hi;
		interfaceCounts[drivers] = source->configDescriptors->bNumInterfaces;
		wireLength += source->configDescriptors->wTotalLength;
		descriptorCounts[drivers] = 0;
		for (index = 0; index < interfaceCounts[drivers]; index++)
			descriptorCounts[drivers] += source->settings[index].numDescriptors;
		endpointCounts[drivers] = driver->numEndpoints - 1;
		interfaces += interfaceCounts[drivers];
		descriptors += descriptorCounts[drivers];
		endpoints += endpointCounts[drivers];
		if (++drivers == 9)
			return NULL;
	}
	wireLength += (1 - drivers) * 9;
	required = endpoints * 9 + descriptors * 15 + interfaces * 12 + 0x47 + wireLength;
	memBlock = speed == 1 ? &state->fullSpeedConfigMemBlock :
				&state->highSpeedConfigMemBlock;
	if (required > 0x1000)
		return NULL;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 0x200000;
	*memBlock = SceSysmemForDriver_C94850C9("SceUdcdConfigDesc", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW, 0x1000, &options);
	if (*memBlock < 0 || SceSysmemForDriver_A841EDDA(*memBlock, (void **)&allocation) < 0)
		return NULL;
	settings = (SceUdcdInterfaceSettings *)(allocation + 1);
	interface = (SceUdcdInterfaceDescriptor *)(settings + interfaces);
	endpoint = (SceUdcdEndpointDescriptor *)(interface + descriptors + 1);
	extra = endpoints ? (unsigned char *)(endpoint + endpoints + 1) :
			    (unsigned char *)endpoint;
	allocation->configuration.configDescriptors = &allocation->descriptor;
	allocation->configuration.settings = settings;
	allocation->configuration.interfaceDescriptors = interface;
	allocation->configuration.endpointDescriptors = endpoints ? endpoint : NULL;
	driver = state->drivers;
	source = speed == 1 ? driver->configuration : driver->configuration_hi;
	memcpy(&allocation->descriptor, source->configDescriptors, sizeof(allocation->descriptor));
	allocation->descriptor.bNumInterfaces = interfaces;
	allocation->descriptor.iConfiguration = 0;
	allocation->descriptor.wTotalLength = wireLength;
	if (state->busPowered == 1 && state->chargingEnabled == 1) {
		allocation->descriptor.bMaxPower = 0xfa;
		allocation->descriptor.bmAttributes = 0x80;
	} else {
		allocation->descriptor.bMaxPower = 1;
		allocation->descriptor.bmAttributes = 0xc0;
	}
	allocation->descriptor.settings = settings;
	stringOffset = state->stringDescriptorCount;
	for (index = 0; driver; driver = driver->link, index++) {
		source = speed == 1 ? driver->configuration : driver->configuration_hi;
		driverSettings = settings;
		memcpy(settings, source->settings, interfaceCounts[index] * sizeof(*settings));
		memcpy(interface, source->interfaceDescriptors,
		       descriptorCounts[index] * sizeof(*interface));
		if (endpointCounts[index])
			memcpy(endpoint, source->endpointDescriptors,
			       endpointCounts[index] * sizeof(*endpoint));
		for (settingIndex = 0; settingIndex < interfaceCounts[index]; settingIndex++, settings++) {
			settings->descriptors = interface;
			sourceInterface = source->settings[settingIndex].descriptors;
			if (!sourceInterface)
				return NULL;
			for (descriptorIndex = 0; descriptorIndex < (int)settings->numDescriptors;
			     descriptorIndex++, interface++, sourceInterface++) {
				interface->bInterfaceNumber += interfaceOffset;
				if (interface->iInterface)
					interface->iInterface += stringOffset;
				if (interface->extraLength) {
					interface->extra = extra;
					memcpy(extra, sourceInterface->extra, interface->extraLength);
					extra += interface->extraLength;
				}
				if (interface->bNumEndpoints)
					interface->endpoints = endpoint;
				sourceEndpoint = sourceInterface->endpoints;
				for (endpointIndex = 0; endpointIndex < interface->bNumEndpoints;
				     endpointIndex++, endpoint++, sourceEndpoint++) {
					number = endpoint->bEndpointAddress & 0xf;
					for (alias = 1; alias < driver->numEndpoints; alias++)
						if (driver->endpoints[alias].driverEndpointNumber == number)
							break;
					if (alias >= driver->numEndpoints)
						return NULL;
					driverEndpoint = &driver->endpoints[alias];
					direction = endpoint->bEndpointAddress & 0x80;
					if (driverEndpoint->direction == direction)
						number = driverEndpoint->endpointNumber;
					endpoint->bEndpointAddress = number | direction;
					if (endpoint->extraLength) {
						endpoint->extra = extra;
						memcpy(extra, sourceEndpoint->extra, endpoint->extraLength);
						extra += endpoint->extraLength;
					}
				}
			}
		}
		if (driver->configure)
			driver->configure(speed, stringOffset, driverSettings, driver->user_data);
		interfaceOffset += interfaceCounts[index];
		stringOffset += stringCounts[index];
	}
	interface->bLength = 0;
	if (endpoints)
		endpoint->bLength = 0;
	return &allocation->configuration;
}

/* The native templates include a second, empty 64-byte string slot. */
struct udcdDefaultString {
	SceUdcdStringDescriptor descriptor;
	SceUdcdStringDescriptor reserved;
};

static const struct udcdDefaultString udcdManufacturerString = {
	.descriptor = { 10, 3, { 'S', 'O', 'N', 'Y' } },
};

static const struct udcdDefaultString udcdProductString = {
	.descriptor = { 20, 3, { '"', 'P', 'S', ' ', 'V', 'i', 't', 'a', '"' } },
};

/* Original name unknown. Compose the device and driver string descriptors. */
int udcdBuildDeviceDescriptors(unsigned short productId, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDriver *driver;
	SceUdcdDeviceDescriptor *descriptor, *allocation;
	SceUdcdStringDescriptor *product = NULL, *serial = NULL, *string, *cursor;
	SceKernelAllocMemBlockKernelOpt options;
	unsigned short usbVersion = 0;
	unsigned char packetSize = 0;
	int stringCounts[8], driverCount = 0, strings = 0;
	int highSpeed = 1, descriptorBytes, index;

	for (driver = state->drivers; driver; driver = driver->link) {
		if (!driver->descriptor_hi || !driver->configuration_hi) {
			highSpeed = 0;
		} else {
			descriptor = driver->descriptor_hi;
			if (descriptor->bLength != 18 || descriptor->bDescriptorType != 1 ||
			    !descriptor->bNumConfigurations)
				return 0x80243301;
		}
		descriptor = driver->descriptor;
		if (descriptor->bLength != 18 || descriptor->bDescriptorType != 1 ||
		    !descriptor->bNumConfigurations ||
		    (descriptor->bMaxPacketSize0 != 8 && descriptor->bMaxPacketSize0 != 16 &&
		     descriptor->bMaxPacketSize0 != 32 && descriptor->bMaxPacketSize0 != 64))
			return 0x80243301;
		if (!driverCount || descriptor->bcdUSB < usbVersion)
			usbVersion = descriptor->bcdUSB;
		if (!driverCount || descriptor->bMaxPacketSize0 < packetSize)
			packetSize = descriptor->bMaxPacketSize0;
		if (driver->stringDescriptorProduct && !product && descriptor->iProduct == 2)
			product = driver->stringDescriptorProduct;
		if (driver->stringDescriptorSerial && !serial && descriptor->iSerialNumber == 3)
			serial = driver->stringDescriptorSerial;
		stringCounts[driverCount] = 0;
		for (string = driver->stringDescriptors; string && string->bLength; string++)
			stringCounts[driverCount]++;
		strings += stringCounts[driverCount++];
	}
	if (usbVersion < 0x200 || !highSpeed) {
		highSpeed = 0;
		state->highSpeedDevice = NULL;
		state->highSpeedConfiguration = NULL;
	}
	descriptorBytes = highSpeed ? 40 : 20;
	if (strings && (unsigned int)(descriptorBytes + strings * 64 + 0x100) > 0x1000)
		return SCE_UDCD_ERROR_WAIT_TIMEOUT;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 0x200000;
	state->deviceMemBlock = SceSysmemForDriver_C94850C9("SceUdcdDevDesc", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW,
						    0x1000, &options);
	if (state->deviceMemBlock < 0 ||
	    SceSysmemForDriver_A841EDDA(state->deviceMemBlock, (void **)&allocation) < 0)
		return SCE_UDCD_ERROR_WAIT_TIMEOUT;
	state->fullSpeedDevice = allocation;
	memcpy(allocation, state->drivers->descriptor, sizeof(*allocation));
	allocation->bcdUSB = usbVersion;
	allocation->bMaxPacketSize0 = packetSize;
	if (highSpeed) {
		state->highSpeedDevice = allocation + 1;
		memcpy(state->highSpeedDevice, state->drivers->descriptor_hi, sizeof(*allocation));
		for (driver = state->drivers->link; driver; driver = driver->link) {
			usbVersion = driver->descriptor_hi->bcdUSB;
			if (usbVersion < state->highSpeedDevice->bcdUSB)
				state->highSpeedDevice->bcdUSB = usbVersion;
		}
	}
	cursor = (SceUdcdStringDescriptor *)((unsigned char *)allocation + descriptorBytes);
	for (index = 0; index < 3; index++)
		state->builtInStringDescriptors[index] = cursor + index;
	cursor += 3;
	state->fullSpeedDevice->iManufacturer = 0;
	state->fullSpeedDevice->idVendor = 0x54c;
	state->fullSpeedDevice->idProduct = productId;
	state->fullSpeedDevice->iProduct = 0;
	state->stringDescriptorCount = 0;
	memcpy(state->builtInStringDescriptors[0], &udcdManufacturerString,
	       sizeof(udcdManufacturerString));
	if (product)
		memcpy(state->builtInStringDescriptors[1], product, sizeof(*product));
	else
		memcpy(state->builtInStringDescriptors[1], &udcdProductString,
		       sizeof(udcdProductString));
	state->fullSpeedDevice->iManufacturer = 1;
	state->fullSpeedDevice->iProduct = 2;
	state->stringDescriptorCount = 2;
	if (serial) {
		memcpy(state->builtInStringDescriptors[2], serial, sizeof(*serial));
		state->fullSpeedDevice->iSerialNumber = 3;
		state->stringDescriptorCount = 3;
	} else {
		state->fullSpeedDevice->iSerialNumber = 0;
	}
	if (highSpeed) {
		state->highSpeedDevice->idVendor = state->fullSpeedDevice->idVendor;
		state->highSpeedDevice->idProduct = state->fullSpeedDevice->idProduct;
		state->highSpeedDevice->iManufacturer = state->fullSpeedDevice->iManufacturer;
		state->highSpeedDevice->iProduct = state->fullSpeedDevice->iProduct;
		state->highSpeedDevice->iSerialNumber = state->fullSpeedDevice->iSerialNumber;
	}
	if (!strings) {
		state->stringDescriptors = NULL;
	} else {
		state->stringDescriptors = cursor;
		index = 0;
		for (driver = state->drivers; driver; driver = driver->link, index++) {
			if (stringCounts[index]) {
				memcpy(cursor, driver->stringDescriptors, stringCounts[index] * 64);
				cursor += stringCounts[index];
			}
		}
		cursor->bLength = 0;
	}
	state->languageDescriptor = state->stringDescriptorCount || state->stringDescriptors ?
		&udcdState.languageDescriptors[bus] : NULL;
	SceCpuForDriver_103872A5(state->fullSpeedDevice, 0x1000);
	state->fullSpeedConfiguration = udcdBuildConfiguration(1, stringCounts, bus);
	if (!state->fullSpeedConfiguration)
		return 0x80243302;
	if (!state->highSpeedDevice)
		return 0;
	state->highSpeedConfiguration = udcdBuildConfiguration(2, stringCounts, bus);
	return state->highSpeedConfiguration ? 0 : 0x80243302;
}

static inline __attribute__((always_inline)) void udcdInitializeControlTransfer(
	struct udcdBusState *state)
{
	SceUdcdDeviceRequest *transfer = &state->controlTransfer;

	transfer->endpoint = &state->controlEndpoint;
	transfer->data = NULL;
	transfer->size = 0;
	transfer->onComplete = NULL;
	transfer->next = NULL;
	transfer->unused = NULL;
	transfer->attributes = 0;
	transfer->transmitted = 0;
	transfer->isControlRequest = 1;
	transfer->returnCode = 1;
}

static inline __attribute__((always_inline)) int udcdQueueRequest(
	SceUdcdDeviceRequest *request, SceKernelIntrStatus *status, int bus,
	int receive)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDeviceRequest *tail, *next;
	struct udcdEndpointState *endpoint;
	int number;

	if (state->controllerState != UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	number = request->endpoint->endpointNumber;
	endpoint = &state->endpoints[number];
	tail = endpoint->requestTail;
	if (!number && tail)
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	request->transmitted = 0;
	request->returnCode = receive ? 3 : 2;
	if (!number) {
		endpoint->requestHead = request;
		endpoint->requestTail = request;
	} else {
		if (tail)
			tail->next = request;
		else
			endpoint->requestHead = request;
		tail = request;
		for (next = request->next; next; next = next->next) {
			next->transmitted = 0;
			next->returnCode = receive ? 3 : 2;
			tail = next;
		}
		endpoint->requestTail = tail;
	}
	if (receive)
		udcdState.operations[bus].receiveRequest(number, status, bus);
	else
		udcdState.operations[bus].submitRequest(number, status, bus);
	return 0;
}

static inline __attribute__((always_inline)) int udcdQueueControlTransfer(
	SceKernelIntrStatus *status, int bus)
{
	return udcdQueueRequest(&udcdState.buses[bus].controlTransfer, status, bus, 0);
}

/* Original name unknown. Answer GET_INTERFACE using the selected alternate. */
int udcdGetInterfaceRequest(const SceUdcdConfigDescriptor *config,
			  SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	unsigned int number = state->controlRequest.wIndex & 0xff;

	if (state->connectionState != SCE_UDCD_STATUS_CONNECTION_ESTABLISHED || !config->bNumInterfaces ||
	    number >= config->bNumInterfaces)
		return -1;
	state->controlTransfer.data = &config->settings[number].alternateSetting;
	state->controlTransfer.size = 1;
	return udcdQueueControlTransfer(status, bus);
}

static inline __attribute__((always_inline)) int udcdCallDriverRequest(
	SceUdcdDriver *driver, int recipient, int number,
	struct udcdBusState *state, SceKernelIntrStatus *status)
{
	int result;

	if (!driver->processRequest)
		return -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, *status);
	result = driver->processRequest(recipient, number, &state->controlRequest,
					driver->user_data);
	*status = SceCpuForDriver_D32ACE9E(&state->lock);
	return result;
}

/* Original name unknown. Route interface/endpoint/device control requests. */
int udcdDispatchDriverRequest(SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDriver *driver = state->drivers;
	SceUdcdEndpoint *endpoint;
	SceUdcdInterface *interface;
	unsigned int recipient = state->controlRequest.bmRequestType & 0x1f;
	unsigned int number;
	int index, result = -1, callbackResult;

	if (recipient == 1) {
		number = state->controlRequest.wIndex & 0xff;
		while (driver) {
			interface = driver->interface;
			if (interface->interfaceNumber + interface->numInterfaces >
			    (int)number)
				break;
			udcdCallDriverRequest(driver, 1, -1, state, status);
			driver = driver->link;
		}
		if (!driver)
			return -1;
		if (driver->processRequest) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, *status);
			result = driver->processRequest(1,
				number - interface->interfaceNumber, &state->controlRequest,
				driver->user_data);
			*status = SceCpuForDriver_D32ACE9E(&state->lock);
		}
	} else if (recipient == 2) {
		number = state->controlRequest.wIndex & 0xf;
		while (driver) {
			for (index = 0; index < driver->numEndpoints; index++) {
				endpoint = &driver->endpoints[index];
				if ((unsigned int)endpoint->endpointNumber == number)
					goto foundEndpoint;
			}
			udcdCallDriverRequest(driver, 2, -1, state, status);
			driver = driver->link;
		}
		return -1;
foundEndpoint:
		if (driver->processRequest) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, *status);
			result = driver->processRequest(2, endpoint->driverEndpointNumber,
						&state->controlRequest, driver->user_data);
			*status = SceCpuForDriver_D32ACE9E(&state->lock);
		}
	} else {
		for (; driver; driver = driver->link) {
			callbackResult = udcdCallDriverRequest(driver, recipient, 0,
							state, status);
			if (callbackResult >= 0)
				result = callbackResult;
		}
		return result;
	}
	for (driver = driver->link; driver; driver = driver->link)
		udcdCallDriverRequest(driver, recipient, -1, state, status);
	return result;
}

/* Original name unknown. Dispatch the standard/class/vendor request type. */
int udcdDispatchControlRequest(SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	unsigned int type = state->controlRequest.bmRequestType & 0x60;

	if (type == 0x20 || type == 0x40)
		return udcdDispatchDriverRequest(status, bus) >> 31;
	if (type)
		return -1;
	return udcdState.operations[bus].processRequest(&state->controlRequest,
							status, bus);
}

/* Original name unknown. Broadcast the completion notification. */
static inline __attribute__((always_inline)) void udcdBroadcastControlRequest(
	SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDriver *driver = state->drivers;
	unsigned int recipient;

	if (state->connectionState != SCE_UDCD_STATUS_CONNECTION_ESTABLISHED)
		return;
	recipient = state->controlRequest.bmRequestType & 0x1f;
	for (; driver; driver = driver->link)
		udcdCallDriverRequest(driver, recipient, -1, state, status);
}

void udcdNotifyControlRequest(SceKernelIntrStatus *status, int bus)
{
	udcdBroadcastControlRequest(status, bus);
}

/* Original names/controller operation unknown. Registered per-bus callbacks. */
int udcdCallBusTwoOperation(int args, void *argp)
{
	SceKernelIntrStatus status;

	status = SceCpuForDriver_D32ACE9E(&udcdState.buses[2].lock);
	udcdState.operations[2].clearQueues(2);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&udcdState.buses[2].lock, status);
	return 0;
}

int udcdCallBusOneOperation(int args, void *argp)
{
	SceKernelIntrStatus status;

	status = SceCpuForDriver_D32ACE9E(&udcdState.buses[1].lock);
	udcdState.operations[1].clearQueues(1);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&udcdState.buses[1].lock, status);
	return 0;
}

/* Original name unknown. Validate queued requests and obtain their DMA address. */
int udcdValidateRequests(SceUdcdDeviceRequest *request, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelVARange virtualRange;
	SceKernelPARange physicalRange;
	SceKernelPAVector vector;
	uintptr_t physicalAddress;
	int endpointNumber;

	if (state->controllerState != UDCD_CONTROLLER_STARTED || state->cableState != SCE_UDCD_STATUS_CABLE_CONNECTED ||
	    state->connectionState == SCE_UDCD_STATUS_CONNECTION_SUSPENDED || state->suspendState == UDCD_BUS_SUSPENDED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	if (state->requestBlocked == 1)
		return 0x80243408;
	if (!request)
		return 0x80243400;
	if (!request->endpoint)
		return 0x80243401;
	endpointNumber = request->endpoint->endpointNumber;
	if (!endpointNumber && request->next)
		return 0x80243402;
	do {
		if ((unsigned int)request->returnCode > 1)
			return SCE_UDCD_ERROR_INVALID_ARGUMENT;
		if (!request->size && request->isControlRequest == 1)
			return 0x80243405;
		if (endpointNumber) {
			if (!(request->attributes & SCE_UDCD_DEVICE_REQUEST_ATTR_PHYCONT)) {
				virtualRange.addr = (uintptr_t)request->data;
				virtualRange.size = request->size;
				vector.size = sizeof(vector);
				vector.capacity = 1;
				/* the original leaves nDataInVector unset: sysmem writes it */
				vector.total = 0;
				vector.pRanges = &physicalRange;
				if (sceKernelVARangeToPAVector(&virtualRange, &vector) < 0)
					return 0x80243407;
				if (vector.total > 1)
					return 0x80243404;
				request->physicalAddress = (void *)(uintptr_t)vector.pRanges->addr;
			} else {
				if (SceSysmemForDriver_8D160E65(request->data, &physicalAddress) < 0)
					return 0x80243407;
				request->physicalAddress = (void *)physicalAddress;
			}
		}
		request = request->next;
		if (!request)
			return 0;
	} while (request->endpoint->endpointNumber == endpointNumber);
	return 0x80243403;
}

/* Original names unknown. Read and write the activation gate under the bus lock. */
unsigned int udcdGetActivationEnabled(int bus)
{
	SceKernelIntrStatus status;
	unsigned int flag;

	status = SceCpuForDriver_D32ACE9E(&udcdState.buses[bus].lock);
	flag = udcdState.buses[bus].activationEnabled;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&udcdState.buses[bus].lock, status);
	return flag;
}

int udcdSetActivationEnabled(unsigned int flag, int bus)
{
	SceKernelIntrStatus status;

	status = SceCpuForDriver_D32ACE9E(&udcdState.buses[bus].lock);
	udcdState.buses[bus].activationEnabled = flag;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&udcdState.buses[bus].lock, status);
	return 0;
}

/* the event names of each bus */
static const char *const s_connectNames[3] = { "SceUdcd0Connect", "SceUdcd1Connect", "SceUdcd2Connect" };
static const char *const s_phyReadyNames[3] = { "SceUdcd0PhyReady", "SceUdcd1PhyReady", "SceUdcd2PhyReady" };

int module_start(SceSize args, void *argp)
{
	struct udcdBusState *state;
	SceKernelProcEventHandler handler;
	SceUdcdStringDescriptor *language;
	int bus;

	for (bus = 0; ; bus++) {
		state = &udcdState.buses[bus];
		state->controllerState = UDCD_CONTROLLER_STOPPED;
		state->resumeControllerState = UDCD_CONTROLLER_STOPPED;
		state->controllerDriverState = UDCD_CONTROLLER_DRIVER_ABSENT;
		state->suspendState = UDCD_BUS_SUSPENDED;
		state->cableState = SCE_UDCD_STATUS_CABLE_DISCONNECTED;
		state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
		state->activationEnabled = 1;
		state->deviceInformation.lastEvent = 8;
		state->deviceInformation.eventDetail = 0xffff;
		state->systemSuspended = 0;
		state->deviceMemBlock = 0;
		state->fullSpeedConfigMemBlock = 0;
		state->highSpeedConfigMemBlock = 0;
		state->descriptorMemBlock = 0;
		state->driverSetting = 0;
		state->alarmTickTimer = -1;
		state->callbackEvent = sceKernelCreateSimpleEvent(s_connectNames[bus], 0, 0, NULL);
		state->callbackProcess = -1;
		state->callbackThread = -1;
		state->lock = 0;
		state->phyReadyEvent = sceKernelCreateEventFlag(s_phyReadyNames[bus], 0x9000, 0, NULL);
		if (state->phyReadyEvent < 0)
			break;
		if (bus == 0)
			continue;
		if (bus == 2) {
			state->startController = udcdInitializeBusTwo;
			state->stopController = udcdCallBusTwoOperation;
		} else {
			state->startController = udcdInitializeBusOne;
			state->stopController = udcdCallBusOneOperation;
		}
		/* the language string descriptor: English (US) */
		language = &udcdState.languageDescriptors[bus];
		language->bLength = 4;
		language->bDescriptorType = 3;
		language->bString[0] = 0x409;
		state->chargingEnabled = 0;
		state->stateEvent = sceKernelCreateEventFlag(s_connectNames[bus], 0x9000, 0, NULL);
		if (state->stateEvent < 0)
			break;
		if (bus == 2) {
			udcdInitializeDriverList(1);
			udcdState.buses[1].controllerDriverState = UDCD_CONTROLLER_DRIVER_STOPPED;
			udcdInitializeDriverList(2);
			udcdState.buses[2].controllerDriverState = UDCD_CONTROLLER_DRIVER_STOPPED;
			if (udcdInitializeControllers() < 0)
				break;
			handler.size = sizeof(handler);
			handler.create = NULL;
			handler.exit = NULL;
			handler.kill = udcdProcessKill;
			handler.suspend = udcdProcessEvent;
			handler.resume = NULL;
			handler.notify = NULL;
			state->procEventHandler = sceKernelRegisterProcEventHandler("SceUdcd", &handler, 0);
			if (state->procEventHandler >= 0)
				state->mtpProcess = -1;
			return SCE_KERNEL_START_SUCCESS;
		}
	}
	return SCE_KERNEL_START_FAILED;
}

/* Original names unknown. Driver callbacks run with the bus lock released. */
void udcdAttachDrivers(SceKernelIntrStatus *intrStatus, int bus)
{
	SceUdcdDriver *driver;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&udcdState.buses[bus].lock,
					       *intrStatus);
	for (driver = udcdState.buses[bus].drivers; driver; driver = driver->link) {
		if (driver->attach)
			driver->attach(udcdState.buses[bus].usbVersion,
				       driver->user_data);
	}
	*intrStatus = SceCpuForDriver_D32ACE9E(
						&udcdState.buses[bus].lock);
}

void udcdDetachDrivers(SceKernelIntrStatus *intrStatus, int bus)
{
	SceUdcdDriver *driver;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&udcdState.buses[bus].lock,
					       *intrStatus);
	for (driver = udcdState.buses[bus].drivers; driver; driver = driver->link) {
		if (driver->detach)
			driver->detach(driver->user_data);
	}
	*intrStatus = SceCpuForDriver_D32ACE9E(
						&udcdState.buses[bus].lock);
}

/* Original name unknown. Wake waiters without retaining the transition bit. */
void udcdNotifyStateChange(int bus)
{
	sceKernelSetEventFlag(udcdState.buses[bus].stateEvent, 0x4000);
	sceKernelClearEventFlag(udcdState.buses[bus].stateEvent, ~0x4000U);
}

/* Original name unknown. Serialize descriptors without SDK pointer fields. */
int udcdSerializeConfiguration(const SceUdcdConfigDescriptor *config,
			       void *buffer)
{
	struct udcdWireConfiguration *header = buffer;
	unsigned char *cursor = (unsigned char *)(header + 1);
	const SceUdcdInterfaceSettings *settings;
	const SceUdcdInterfaceDescriptor *interface;
	const SceUdcdEndpointDescriptor *endpoint;
	struct udcdWireInterface *wireInterface;
	struct udcdWireEndpoint *wireEndpoint;
	int interfaceIndex, alternateIndex, endpointIndex;

	header->bLength = config->bLength;
	header->bDescriptorType = config->bDescriptorType;
	header->wTotalLength = config->wTotalLength;
	header->bNumInterfaces = config->bNumInterfaces;
	header->bConfigurationValue = config->bConfigurationValue;
	header->iConfiguration = config->iConfiguration;
	header->bmAttributes = config->bmAttributes;
	header->bMaxPower = config->bMaxPower;
	for (interfaceIndex = 0; interfaceIndex < config->bNumInterfaces;
	     interfaceIndex++) {
		settings = &config->settings[interfaceIndex];
		for (alternateIndex = 0;
		     alternateIndex < (int)settings->numDescriptors;
		     alternateIndex++) {
			interface = &settings->descriptors[alternateIndex];
			wireInterface = (struct udcdWireInterface *)cursor;
			wireInterface->bLength = interface->bLength;
			wireInterface->bDescriptorType = interface->bDescriptorType;
			wireInterface->bInterfaceNumber = interface->bInterfaceNumber;
			wireInterface->bAlternateSetting = interface->bAlternateSetting;
			wireInterface->bNumEndpoints = interface->bNumEndpoints;
			wireInterface->bInterfaceClass = interface->bInterfaceClass;
			wireInterface->bInterfaceSubClass = interface->bInterfaceSubClass;
			wireInterface->bInterfaceProtocol = interface->bInterfaceProtocol;
			wireInterface->iInterface = interface->iInterface;
			cursor = (unsigned char *)(wireInterface + 1);
			if (interface->extraLength) {
				if ((unsigned int)interface->extraLength > 0xfffe)
					return 0x80243304;
				cursor = memcpy(cursor, interface->extra,
						interface->extraLength);
				cursor += interface->extraLength;
			}
			for (endpointIndex = 0;
			     endpointIndex < interface->bNumEndpoints;
			     endpointIndex++) {
				endpoint = &interface->endpoints[endpointIndex];
				wireEndpoint = (struct udcdWireEndpoint *)cursor;
				wireEndpoint->bLength = endpoint->bLength;
				wireEndpoint->bDescriptorType = endpoint->bDescriptorType;
				wireEndpoint->bEndpointAddress = endpoint->bEndpointAddress;
				wireEndpoint->bmAttributes = endpoint->bmAttributes;
				wireEndpoint->wMaxPacketSize = endpoint->wMaxPacketSize;
				wireEndpoint->bInterval = endpoint->bInterval;
				cursor = (unsigned char *)(wireEndpoint + 1);
				if (endpoint->extraLength) {
					cursor = memcpy(cursor, endpoint->extra,
							endpoint->extraLength);
					cursor += endpoint->extraLength;
				}
			}
		}
	}
	return 0;
}

/* Original name unknown. Bus zero is reserved for the controller itself. */
/* out of line in Sony's build */
__attribute__((noinline)) int udcdIsDriverBus(int bus)
{
	return bus == 1 || bus == 2;
}

/* Original name unknown. Validate the full/high-speed descriptor pair. */
int udcdValidateDriverDescriptors(const SceUdcdDriver *driver)
{
	const SceUdcdDeviceDescriptor *device = driver->descriptor;
	const SceUdcdConfiguration *config = driver->configuration;
	const SceUdcdEndpointDescriptor *endpoint;
	unsigned int packetSize, transferType;
	int index;

	if (!device || !config)
		return 0x80243306;
	packetSize = device->bMaxPacketSize0;
	if (packetSize != 8 && packetSize != 16 && packetSize != 32 &&
	    packetSize != 64)
		return 0x80243301;
	if (!config->configDescriptors || !config->settings)
		return 0x80243306;
	for (index = 0; index < config->configDescriptors->bNumInterfaces; index++) {
		if ((int)config->settings[index].numDescriptors > 2)
			return 0x80243303;
	}
	if (!config->interfaceDescriptors)
		return 0x80243306;
	endpoint = config->endpointDescriptors;
	if (endpoint) {
		for (; endpoint->bLength; endpoint++) {
			packetSize = endpoint->wMaxPacketSize;
			transferType = endpoint->bmAttributes & 3;
			if (transferType == 1) {
				if (packetSize > 1023)
					return 0x80243305;
			} else if (transferType == 3) {
				if (packetSize > 64)
					return 0x80243305;
			} else if (packetSize != 8 && packetSize != 16 &&
				   packetSize != 32 && packetSize != 64) {
				return 0x80243305;
			}
		}
	}
	if (!driver->descriptor_hi)
		return driver->configuration_hi ? 0x80243306 : 0;
	config = driver->configuration_hi;
	if (!config)
		return 0x80243306;
	if (device->bcdUSB < 0x200)
		return 0;
	if (driver->descriptor_hi->bMaxPacketSize0 != 64)
		return 0x80243301;
	if (!config->configDescriptors || !config->settings)
		return 0x80243306;
	for (index = 0; index < config->configDescriptors->bNumInterfaces; index++) {
		if ((int)config->settings[index].numDescriptors > 2)
			return 0x80243303;
	}
	if (!config->interfaceDescriptors)
		return 0x80243306;
	endpoint = config->endpointDescriptors;
	if (endpoint) {
		for (; endpoint->bLength; endpoint++) {
			packetSize = endpoint->wMaxPacketSize;
			transferType = endpoint->bmAttributes & 3;
			if (transferType == 2) {
				if (packetSize != 512)
					return 0x80243305;
			} else if (transferType == 1 || transferType == 3) {
				if (packetSize > 1024)
					return 0x80243305;
			} else if (packetSize != 64) {
				return 0x80243305;
			}
		}
	}
	return 0;
}

int SceUdcdForDriver_E9E75C49(int bus)
{
	int (*operation)(int bus) = udcdState.operations[bus].resume;
	int result;

	if (!operation)
		return SCE_UDCD_ERROR_INVALID_ARGUMENT;
	result = operation(bus);
	return result < 0 ? result : 0;
}

int SceUdcdForDriver_2A234CAF(int bus)
{
	int (*operation)(int bus) = udcdState.operations[bus].suspend;
	int result;

	if (!operation)
		return SCE_UDCD_ERROR_INVALID_ARGUMENT;
	result = operation(bus);
	return result < 0 ? result : 0;
}

/* Original name unknown. Build the standard USB device-qualifier descriptor. */
static inline __attribute__((always_inline)) void udcdFillDeviceQualifier(
	const SceUdcdDeviceDescriptor *device,
	SceUdcdDeviceQualifierDescriptor *qualifier)
{
	qualifier->bLength = 10;
	qualifier->bDescriptorType = 6;
	qualifier->bcdUSB = device->bcdUSB;
	qualifier->bDeviceClass = device->bDeviceClass;
	qualifier->bDeviceSubClass = device->bDeviceSubClass;
	qualifier->bDeviceProtocol = device->bDeviceProtocol;
	qualifier->bMaxPacketSize0 = device->bMaxPacketSize0;
	qualifier->bNumConfigurations = device->bNumConfigurations;
	qualifier->bReserved = 0;
}

void udcdMakeDeviceQualifier(const SceUdcdDeviceDescriptor *device,
			     SceUdcdDeviceQualifierDescriptor *qualifier)
{
	udcdFillDeviceQualifier(device, qualifier);
}

/* Original name unknown. Allocate one page for the USB descriptor responses. */
int udcdAllocateDescriptorData(int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelAllocMemBlockKernelOpt options;
	SceUdcdDeviceQualifierDescriptor *qualifier;
	unsigned char *base, *cursor;
	unsigned int required;
	int result;

	required = (state->fullSpeedConfiguration->configDescriptors->wTotalLength +
		    63U) & ~63U;
	if (state->highSpeedConfiguration) {
		required += (state->highSpeedConfiguration->configDescriptors->wTotalLength +
			     63U) & ~63U;
		required += 2 * sizeof(*qualifier);
	} else {
		required += sizeof(*qualifier);
	}
	if (required > 0x1000)
		return SCE_UDCD_ERROR_WAIT_TIMEOUT;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 0x200000;
	result = SceSysmemForDriver_C94850C9("SceUdcdGetDescData", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW,
				       0x1000, &options);
	state->descriptorMemBlock = result;
	if (result < 0 || SceSysmemForDriver_A841EDDA(result, (void **)&base) < 0)
		return SCE_UDCD_ERROR_WAIT_TIMEOUT;
	state->fullSpeedDescriptorData = base;
	cursor = base + ((state->fullSpeedConfiguration->configDescriptors->wTotalLength +
			 63U) & ~63U);
	if (state->highSpeedConfiguration) {
		state->highSpeedDescriptorData = cursor;
		cursor += (state->highSpeedConfiguration->configDescriptors->wTotalLength +
			   63U) & ~63U;
		qualifier = (SceUdcdDeviceQualifierDescriptor *)cursor;
		state->highSpeedQualifier = qualifier + 1;
	} else {
		qualifier = (SceUdcdDeviceQualifierDescriptor *)cursor;
	}
	state->fullSpeedQualifier = qualifier;
	udcdFillDeviceQualifier(state->fullSpeedDevice, qualifier);
	result = udcdSerializeConfiguration(state->fullSpeedConfiguration->configDescriptors,
					    state->fullSpeedDescriptorData);
	if (result < 0)
		return result;
	if (state->highSpeedConfiguration) {
		udcdFillDeviceQualifier(state->highSpeedDevice,
					state->highSpeedQualifier);
		result = udcdSerializeConfiguration(state->highSpeedConfiguration->configDescriptors,
						    state->highSpeedDescriptorData);
		if (result < 0)
			return result;
	}
	SceCpuForDriver_103872A5(state->fullSpeedDescriptorData, 0x1000);
	return 0;
}

/* Original name unknown. Answer GET_DESCRIPTOR and OTHER_SPEED_CONFIG. */
int udcdGetDescriptorRequest(SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDeviceRequest *transfer = &state->controlTransfer;
	SceUdcdStringDescriptor *string;
	struct udcdWireConfiguration *config;
	SceUdcdDriver *driver;
	unsigned int index = state->controlRequest.wValue & 0xff;
	unsigned int size = state->controlRequest.wLength;
	int remaining, result = -1;

	udcdInitializeControlTransfer(state);
	switch (state->controlRequest.wValue >> 8) {
	case 1:
		transfer->data = state->usbVersion == 2 ? state->highSpeedDevice :
							state->fullSpeedDevice;
		if (size > 18)
			size = 18;
		break;
	case 2:
		config = state->usbVersion == 2 ? state->highSpeedDescriptorData :
							state->fullSpeedDescriptorData;
		transfer->data = config;
		config->bDescriptorType = 2;
		goto configuration;
	case 3:
		if (index == 0xee) {
			for (driver = state->drivers; driver; driver = driver->link) {
				if (!strncmp(driver->driverName, "USB_MTP_Driver", 0x40) &&
				    driver->processRequest) {
					sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock,
										*status);
					result = driver->processRequest(
						state->controlRequest.bmRequestType & 0x1f,
						0, &state->controlRequest, driver->user_data);
					*status = SceCpuForDriver_D32ACE9E(
										&state->lock);
					return result;
				}
			}
			goto notify;
		}
		string = state->languageDescriptor;
		if (!string)
			goto notify;
		if (index) {
			if (index <= state->stringDescriptorCount) {
				string = state->builtInStringDescriptors[index - 1];
			} else {
				string = state->stringDescriptors;
				if (!string)
					goto notify;
				remaining = index - state->stringDescriptorCount - 1;
				while (remaining > 0) {
					if (!string->bLength)
						goto notify;
					string++;
					remaining--;
				}
				if (!string->bLength)
					goto notify;
			}
		}
		if (size > string->bLength)
			size = string->bLength;
		transfer->data = string;
		break;
	case 6:
		if (!state->highSpeedDevice)
			goto notify;
		transfer->data = state->usbVersion == 2 ? state->fullSpeedQualifier :
							state->highSpeedQualifier;
		if (size > 10)
			size = 10;
		break;
	case 7:
		if (!state->highSpeedConfiguration)
			goto notify;
		config = state->usbVersion == 2 ? state->fullSpeedDescriptorData :
							state->highSpeedDescriptorData;
		transfer->data = config;
		config->bDescriptorType = 7;
configuration:
		if (index >= 2)
			goto notify;
		if (size > config->wTotalLength)
			size = config->wTotalLength;
		SceCpuForDriver_103872A5(transfer->data, 0x20);
		break;
	default:
		return udcdDispatchDriverRequest(status, bus);
	}
	transfer->size = size;
	result = udcdQueueControlTransfer(status, bus);
	if (result < 0)
		return result;
notify:
	udcdBroadcastControlRequest(status, bus);
	return result;
}

static inline __attribute__((always_inline)) int udcdSubmitDriverRequest(
	SceUdcdDeviceRequest *request, int bus, int receive)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelIntrStatus status;
	int result;

	result = udcdValidateRequests(request, bus);
	if (result < 0)
		return result;
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	result = udcdQueueRequest(request, &status, bus, receive);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

/* Original name unknown. Retire the first request and advance its endpoint. */
static inline __attribute__((always_inline)) void udcdRetireRequest(
	struct udcdEndpointState *endpoint, int result,
	SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDeviceRequest *request = endpoint->requestHead;
	SceUdcdDeviceRequest *next;
	SceUdcdEndpoint *driverEndpoint;

	if (!request)
		return;
	next = request->next;
	if (next) {
		endpoint->requestHead = next;
	} else {
		endpoint->requestHead = NULL;
		endpoint->requestTail = NULL;
		if (result == -2)
			result = -3;
	}
	if (state->controllerState == UDCD_CONTROLLER_STARTED) {
		if (endpoint->direction == 0x80 ||
		    (endpoint->direction && request->returnCode == 2))
			udcdState.operations[bus].completeSend(
				request->endpoint->endpointNumber, status, bus);
		else
			udcdState.operations[bus].completeReceive(
				request->endpoint->endpointNumber, status, bus);
		next = endpoint->requestHead;
	}
	driverEndpoint = request->endpoint;
	request->returnCode = result;
	driverEndpoint->transmittedBytes += request->transmitted;
	request->next = NULL;
	if (next && result != -2 && state->controllerState == UDCD_CONTROLLER_STARTED &&
	    state->suspendState != UDCD_BUS_SUSPENDED) {
		if (next->returnCode == 3)
			udcdState.operations[bus].receiveRequest(
				driverEndpoint->endpointNumber, status, bus);
		else
			udcdState.operations[bus].submitRequest(
				driverEndpoint->endpointNumber, status, bus);
	}
	if (request->onComplete) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, *status);
		request->onComplete(request);
		*status = SceCpuForDriver_D32ACE9E(&state->lock);
	}
}

void udcdCompleteRequest(struct udcdEndpointState *endpoint, int result,
			 SceKernelIntrStatus *status, int bus)
{
	udcdRetireRequest(endpoint, result, status, bus);
}

/* Original name unknown. Cancel every request for one endpoint. */
static inline __attribute__((always_inline)) int udcdDrainEndpointRequests(
	int number, SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdEndpointState *endpoint = &state->endpoints[number];
	int result;

	state->requestBlocked = 1;
	if (!number) {
		if (endpoint->requestHead)
			udcdCompleteRequest(endpoint, -2, status, bus);
	} else if (state->controllerState == UDCD_CONTROLLER_STARTED && state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED &&
		   state->connectionState != SCE_UDCD_STATUS_CONNECTION_SUSPENDED && state->suspendState != UDCD_BUS_SUSPENDED) {
		if (endpoint->requestHead) {
			result = udcdState.operations[bus].cancelRequest(number, -2, bus);
			if (result == (int)SCE_UDCD_ERROR_DRIVER_IN_PROGRESS) {
				udcdState.operations[bus].abortEndpoint(number, bus);
				state->requestBlocked = 0;
				return result;
			}
			while (endpoint->requestHead)
				udcdCompleteRequest(endpoint, -2, status, bus);
		}
	} else {
		while (endpoint->requestHead)
			udcdRetireRequest(endpoint, -2, status, bus);
	}
	state->requestBlocked = 0;
	return 0;
}

int udcdCancelEndpointRequests(int number, SceKernelIntrStatus *status, int bus)
{
	return udcdDrainEndpointRequests(number, status, bus);
}

int ksceUdcdReqCancelAll(SceUdcdEndpoint *endpoint)
{
	struct udcdBusState *state = &udcdState.buses[2];
	SceKernelIntrStatus status;
	int result;

	if (!endpoint)
		return 0x80243401;
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (state->controllerState == UDCD_CONTROLLER_STARTED)
		result = udcdDrainEndpointRequests(endpoint->endpointNumber, &status, 2);
	else
		result = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

/* Original name unknown. Cancel a queued request; absent requests succeed. */
static inline __attribute__((always_inline)) int udcdUnlinkRequest(
	SceUdcdDeviceRequest *request, SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdEndpointState *endpoint;
	SceUdcdDeviceRequest *previous, *current;
	SceUdcdEndpoint *driverEndpoint;
	int number, result;

	if (state->controllerState != UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	if (!request)
		return 0x80243400;
	if (!request->endpoint)
		return 0x80243401;
	number = request->endpoint->endpointNumber;
	endpoint = &state->endpoints[number];
	previous = endpoint->requestHead;
	if (!previous)
		return 0;
	if (previous == request) {
		if (state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED && state->connectionState != SCE_UDCD_STATUS_CONNECTION_SUSPENDED &&
		    state->suspendState != UDCD_BUS_SUSPENDED) {
			result = udcdState.operations[bus].cancelRequest(number, -1, bus);
			if (result == (int)SCE_UDCD_ERROR_DRIVER_IN_PROGRESS)
				return result;
		}
		udcdCompleteRequest(endpoint, -1, status, bus);
		return 0;
	}
	for (current = previous->next; current; current = current->next) {
		if (current == request) {
			driverEndpoint = request->endpoint;
			previous->next = request->next;
			request->returnCode = -1;
			driverEndpoint->transmittedBytes += request->transmitted;
			request->next = NULL;
			/* Sony leaves requestTail unchanged for a non-head cancellation. */
			if (request->onComplete) {
				sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock,
								*status);
				request->onComplete(request);
				*status = SceCpuForDriver_D32ACE9E(&state->lock);
			}
			return 0;
		}
		previous = current;
	}
	return 0;
}

int udcdCancelRequest(SceUdcdDeviceRequest *request,
		      SceKernelIntrStatus *status, int bus)
{
	return udcdUnlinkRequest(request, status, bus);
}

/* Original names unknown. Locked request-cancellation exports. */
int SceUdcdForDriver_AA5FD90E(SceUdcdDeviceRequest *request)
{
	struct udcdBusState *state = &udcdState.buses[2];
	SceKernelIntrStatus status;
	int result;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	result = udcdUnlinkRequest(request, &status, 2);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

int SceUdcdForDriver_03781649(SceUdcdDeviceRequest *request, int bus)
{
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	int result;

	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	state = &udcdState.buses[bus];
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	result = udcdUnlinkRequest(request, &status, bus);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

int ksceUdcdReqSend(SceUdcdDeviceRequest *request)
{
	return udcdSubmitDriverRequest(request, 2, 0);
}

int ksceUdcdReqSendInternal(SceUdcdDeviceRequest *request, int bus)
{
	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	return udcdSubmitDriverRequest(request, bus, 0);
}

int ksceUdcdReqRecv(SceUdcdDeviceRequest *request)
{
	return udcdSubmitDriverRequest(request, 2, 1);
}

int ksceUdcdReqRecvInternal(SceUdcdDeviceRequest *request, int bus)
{
	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	return udcdSubmitDriverRequest(request, bus, 1);
}

/* Original name unknown. Replace an interface's active endpoint setting. */
int udcdSetInterfaceSetting(unsigned int number, int alternate,
			    SceUdcdConfigDescriptor *config,
			    SceKernelIntrStatus *status, int bus)
{
	SceUdcdInterfaceSettings *settings = config->settings;
	SceUdcdInterfaceDescriptor *descriptor, *oldDescriptor;
	SceUdcdEndpointDescriptor *endpoint;
	int interfaceIndex, settingIndex, endpointIndex, result;
	unsigned int endpointNumber;

	for (interfaceIndex = 0; interfaceIndex < config->bNumInterfaces;
	     interfaceIndex++, settings++) {
		descriptor = settings->descriptors;
		if (descriptor->bInterfaceNumber != number)
			continue;
		if (alternate < 0) {
			alternate = 0;
		} else {
			if ((int)settings->numDescriptors <= 0)
				continue;
			for (settingIndex = 0; settingIndex < (int)settings->numDescriptors;
			     settingIndex++) {
				oldDescriptor = &descriptor[settingIndex];
				if (oldDescriptor->bAlternateSetting != settings->alternateSetting)
					continue;
				endpoint = oldDescriptor->endpoints;
				for (endpointIndex = 0;
				     endpointIndex < oldDescriptor->bNumEndpoints;
				     endpointIndex++, endpoint++) {
					endpointNumber = endpoint->bEndpointAddress & 0xf;
					udcdCancelEndpointRequests(endpointNumber, status, bus);
					udcdState.operations[bus].abortEndpoint(endpointNumber, bus);
				}
				break;
			}
			descriptor = settings->descriptors;
		}
		for (settingIndex = 0; settingIndex < (int)settings->numDescriptors;
		     settingIndex++, descriptor++) {
			if (descriptor->bAlternateSetting != (unsigned int)alternate)
				continue;
			endpoint = descriptor->endpoints;
			settings->alternateSetting = alternate;
			for (endpointIndex = 0; endpointIndex < descriptor->bNumEndpoints;
			     endpointIndex++, endpoint++) {
				result = udcdState.operations[bus].configureEndpoint(endpoint, bus);
				if (result < 0)
					return result;
			}
			return 0;
		}
	}
	return 0x80243fff;
}

/* Original name unknown. Initialize every interface to alternate zero. */
int udcdInitializeInterfaceSettings(SceUdcdConfigDescriptor *config,
				    SceKernelIntrStatus *status, int bus)
{
	int number, result;

	for (number = 0; number < config->bNumInterfaces; number++) {
		result = udcdSetInterfaceSetting(number, -1, config, status, bus);
		if (result < 0)
			return result;
	}
	return 0;
}

/* Original name unknown. Assign composite interfaces and activate the bus. */
static inline __attribute__((always_inline)) int udcdActivateBusImpl(
	unsigned int productId, unsigned int policy, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelIntrStatus status;
	SceUdcdDriver *driver, *previous, *selected, *last;
	SceUdcdInterface *interface;
	int occupied[8] = { -2, -2, -2, -2, -2, -2, -2, -2 };
	int index, count, number, nextNumber, result;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (state->controllerDriverState == UDCD_CONTROLLER_DRIVER_ABSENT) {
		result = SCE_UDCD_ERROR_ALREADY_DONE;
		goto unlock;
	}
	if (state->systemSuspended == 1) {
		result = UDCD_ERROR_SUSPENDED;
		goto unlock;
	}
	if (state->controllerDriverState == UDCD_CONTROLLER_DRIVER_STOPPED) {
		result = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
		goto unlock;
	}
	if (state->controllerState == UDCD_CONTROLLER_STARTED) {
		result = SCE_UDCD_ERROR_INVALID_ARGUMENT;
		goto unlock;
	}
	if (!state->drivers) {
		result = UDCD_ERROR_DRIVER_NOT_REGISTERED;
		goto unlock;
	}
	if (policy > 1) {
		result = 0x80243300;
		goto unlock;
	}
	state->busPowered = policy;
	if (state->drivers->link) {
		for (driver = state->drivers; driver; driver = driver->link) {
			if (driver->descriptor->bDeviceClass) {
				result = 0x80243301;
				goto unlock;
			}
		}
	}
	for (index = 1; index < 8; index++) {
		state->endpoints[index].endpoint = NULL;
		state->endpoints[index].requestHead = NULL;
		state->endpoints[index].requestTail = NULL;
	}
	for (driver = state->drivers; driver; driver = driver->link) {
		driver->endpoints[0].endpointNumber = 0;
		for (index = 1; index < driver->numEndpoints; index++)
			driver->endpoints[index].endpointNumber = 0;
	}
	for (driver = state->drivers; driver; driver = driver->link)
		driver->interface->interfaceNumber = -1;
	result = 0x80243303;
	for (driver = state->drivers; driver; driver = driver->link) {
		interface = driver->interface;
		if (interface->expectNumber == -1)
			continue;
		number = interface->expectNumber;
		count = interface->numInterfaces;
		for (index = 0; index < count && index < 8; index++) {
			if (occupied[number + index] != -2)
				goto cleanup;
			occupied[number + index] = number;
		}
		interface->interfaceNumber = number;
	}
	for (number = 0; number < (int)state->interfaceCount; number++) {
		if (occupied[number] != -2)
			continue;
		for (driver = state->drivers; driver; driver = driver->link) {
			interface = driver->interface;
			if (interface->expectNumber != -1 || interface->interfaceNumber != -1)
				continue;
			count = interface->numInterfaces;
			if (count <= 0) {
				interface->interfaceNumber = number;
				goto cleanup;
			}
			for (index = 1; index < count && index < 8; index++) {
				if (occupied[number + index] != -2)
					break;
			}
			if (index < count && index < 8)
				continue;
			for (index = 0; index < count && index < 8; index++)
				occupied[number + index] = number + index;
			interface->interfaceNumber = number;
			break;
		}
		if (occupied[number] == -2)
			goto cleanup;
	}
	last = NULL;
	nextNumber = 0;
	do {
		selected = last ? last->link : state->drivers;
		if (selected->interface->interfaceNumber != nextNumber) {
			previous = selected;
			selected = selected->link;
			while (selected->interface->interfaceNumber != nextNumber) {
				previous = selected;
				selected = selected->link;
			}
			previous->link = selected->link;
			if (last) {
				selected->link = last->link;
				last->link = selected;
			} else {
				selected->link = state->drivers;
				state->drivers = selected;
			}
		}
		nextNumber += selected->interface->numInterfaces;
		last = selected;
	} while (last->link);
	result = udcdAssignEndpointNumbers(bus);
	if (result < 0)
		goto cleanup;
	result = udcdBuildDeviceDescriptors(productId, bus);
	if (result < 0)
		goto cleanup;
	result = udcdAllocateDescriptorData(bus);
	if (result < 0)
		goto cleanup;
	udcdInitializeControlTransfer(state);
	if (state->deviceInformation.lastEvent != 8)
		state->deviceInformation.lastEvent = 2;
	result = udcdState.operations[bus].start(bus, &status);
	if (result < 0)
		goto cleanup;
	udcdNotifyStateChange(bus);
	if (bus == 2 && ksceUdcdGetDrvState("USB_MTP_Driver") == 2)
		udcdState.buses[2].mtpProcess = -1;
	result = 0;
	goto unlock;
cleanup:
	if (state->descriptorMemBlock) {
		SceSysmemForDriver_009E1C61(state->descriptorMemBlock);
		state->descriptorMemBlock = 0;
	}
	if (state->deviceMemBlock) {
		SceSysmemForDriver_009E1C61(state->deviceMemBlock);
		state->deviceMemBlock = 0;
	}
	if (state->fullSpeedConfigMemBlock) {
		SceSysmemForDriver_009E1C61(state->fullSpeedConfigMemBlock);
		state->fullSpeedConfigMemBlock = 0;
	}
	if (state->highSpeedConfigMemBlock) {
		SceSysmemForDriver_009E1C61(state->highSpeedConfigMemBlock);
		state->highSpeedConfigMemBlock = 0;
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

int udcdActivateBus(unsigned int productId, unsigned int policy, int bus)
{
	return udcdActivateBusImpl(productId, policy, bus);
}

static inline __attribute__((always_inline)) int udcdActivateIfEnabled(
	unsigned int productId, unsigned int policy, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelIntrStatus status;
	unsigned int enabled;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	enabled = state->activationEnabled;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return enabled ? udcdActivateBusImpl(productId, policy, bus) : SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
}

int ksceUdcdActivate(unsigned int productId)
{
	return udcdActivateIfEnabled(productId, 1, 2);
}

int ksceUdcdActivateInternal(unsigned int productId, unsigned int policy, int bus)
{
	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	return udcdActivateIfEnabled(productId, policy, bus);
}

/* Native restart policy is inlined into both reactivation entry points. */
static inline __attribute__((always_inline)) void udcdReactivateBus(void)
{
	struct udcdBusState *state = &udcdState.buses[2];
	unsigned int policy, information;
	unsigned short productId;

	udcdSetActivationEnabled(0, 2);
	policy = state->busPowered;
	information = state->pendingEventDetail;
	productId = state->fullSpeedDevice->idProduct;
	udcdDeactivateBus(2);
	udcdActivateBus(productId, policy, 2);
	state->pendingEventDetail = information;
	udcdSetActivationEnabled(1, 2);
}

int SceUdcdForDriver_28BFEC45(void)
{
	udcdReactivateBus();
	return 0;
}

int SceUdcdForDriver_2D91F8C3(unsigned int enabled)
{
	struct udcdBusState *state = &udcdState.buses[2];

	state->chargingEnabled = enabled;
	if (state->controllerState == UDCD_CONTROLLER_STARTED && state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED && state->activationState != SCE_UDCD_STATUS_IS_CHARGING)
		udcdReactivateBus();
	else
		udcdApplyUsbCharging(2);
	return 0;
}

/* Original name unknown. Start a registered driver and move it to the bus. */
static inline __attribute__((always_inline)) int udcdStartDriverImpl(
	const char *name, unsigned int args, void *argp, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDriver *driver, *previous, *tail;
	unsigned int driverState;
	int result;

	if (!name || strnlen(name, 64) == 64)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_NAME;
	if (SceIntrmgrForDriver_182EE3E3())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	driverState = state->controllerDriverState;
	if (driverState == UDCD_CONTROLLER_DRIVER_ABSENT)
		return SCE_UDCD_ERROR_ALREADY_DONE;
	if (!strncmp(name, "USBDeviceControllerDriver", 64)) {
		if (driverState == UDCD_CONTROLLER_DRIVER_STARTED)
			return SCE_UDCD_ERROR_INVALID_ARGUMENT;
		result = state->startController(args, argp);
		if (result < 0)
			return result;
		state->controllerDriverState = UDCD_CONTROLLER_DRIVER_STARTED;
		return 0;
	}
	if (driverState == UDCD_CONTROLLER_DRIVER_STOPPED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	if (state->controllerState == UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	previous = NULL;
	for (driver = udcdState.registeredDrivers[bus]; driver; driver = driver->link) {
		if (!strncmp(name, driver->driverName, 64))
			break;
		previous = driver;
	}
	if (!driver) {
		for (driver = state->drivers; driver; driver = driver->link) {
			if (!strncmp(name, driver->driverName, 64))
				return SCE_UDCD_ERROR_INVALID_ARGUMENT;
		}
		return UDCD_ERROR_DRIVER_NOT_REGISTERED;
	}
	result = driver->start(args, argp, driver->user_data);
	if (result < 0)
		return result;
	result = udcdValidateDriverDescriptors(driver);
	if (result < 0) {
		driver->stop(0, NULL, driver->user_data);
		return result;
	}
	if (previous)
		previous->link = driver->link;
	else
		udcdState.registeredDrivers[bus] = driver->link;
	tail = state->drivers;
	if (!tail) {
		driver->link = NULL;
		state->drivers = driver;
	} else {
		while (tail->link)
			tail = tail->link;
		tail->link = driver;
		driver->link = NULL;
	}
	if (driver->interface->expectNumber < 0)
		driver->interface->expectNumber = -1;
	state->interfaceCount += driver->interface->numInterfaces;
	state->endpointCount += driver->numEndpoints - 1;
	return 0;
}

int udcdStartDriver(const char *name, unsigned int args, void *argp, int bus)
{
	return udcdStartDriverImpl(name, args, argp, bus);
}

int ksceUdcdStart(const char *name, int args, void *argp)
{
	if (!udcdIsDriverBus(2))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!udcdGetActivationEnabled(2))
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	return udcdStartDriverImpl(name, args, argp, 2);
}

int ksceUdcdStartInternal(const char *name, int args, void *argp, int bus)
{
	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!udcdGetActivationEnabled(bus))
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	return udcdStartDriverImpl(name, args, argp, bus);
}

/* Original name unknown. Stop a bus driver and return it to the registry. */
static inline __attribute__((always_inline)) int udcdStopDriverImpl(
	const char *name, unsigned int args, void *argp, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceUdcdDriver *driver, *previous, *tail;
	unsigned int driverState;
	int result;

	if (!name || strnlen(name, 64) == 64)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_NAME;
	if (SceIntrmgrForDriver_182EE3E3())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	driverState = state->controllerDriverState;
	if (driverState == UDCD_CONTROLLER_DRIVER_ABSENT)
		return SCE_UDCD_ERROR_ALREADY_DONE;
	if (state->controllerState == UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_MEMORY_EXHAUSTED;
	if (!strncmp(name, "USBDeviceControllerDriver", 64)) {
		if (driverState == UDCD_CONTROLLER_DRIVER_STOPPED)
			return SCE_UDCD_ERROR_INVALID_ARGUMENT;
		if (state->drivers)
			return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
		result = state->stopController(args, argp);
		if (result < 0)
			return result;
		state->controllerDriverState = UDCD_CONTROLLER_DRIVER_STOPPED;
		return 0;
	}
	if (driverState == UDCD_CONTROLLER_DRIVER_STOPPED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	previous = NULL;
	for (driver = state->drivers; driver; driver = driver->link) {
		if (!strncmp(name, driver->driverName, 64))
			break;
		previous = driver;
	}
	if (!driver) {
		for (driver = udcdState.registeredDrivers[bus]; driver; driver = driver->link) {
			if (!strncmp(name, driver->driverName, 64))
				return SCE_UDCD_ERROR_INVALID_ARGUMENT;
		}
		return UDCD_ERROR_DRIVER_NOT_REGISTERED;
	}
	result = driver->stop(args, argp, driver->user_data);
	if (result < 0)
		return result;
	if (previous)
		previous->link = driver->link;
	else
		state->drivers = driver->link;
	tail = udcdState.registeredDrivers[bus];
	if (!tail) {
		driver->link = NULL;
		udcdState.registeredDrivers[bus] = driver;
	} else {
		while (tail->link)
			tail = tail->link;
		tail->link = driver;
		driver->link = NULL;
	}
	state->endpointCount += 1 - driver->numEndpoints;
	state->interfaceCount -= driver->interface->numInterfaces;
	if (bus == 2 && !strncmp(name, "USB_MTP_Driver", 64) &&
	    sceKernelIsUserModeThread())
		state->mtpProcess = sceKernelGetProcessId();
	return 0;
}

int udcdStopDriver(const char *name, unsigned int args, void *argp, int bus)
{
	return udcdStopDriverImpl(name, args, argp, bus);
}

int ksceUdcdStop(const char *name, int args, void *argp)
{
	if (!udcdIsDriverBus(2))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!udcdGetActivationEnabled(2))
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	return udcdStopDriverImpl(name, args, argp, 2);
}

int ksceUdcdStopInternal(const char *name, int args, void *argp, int bus)
{
	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!udcdGetActivationEnabled(bus))
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	return udcdStopDriverImpl(name, args, argp, bus);
}

int ksceUdcdStopCurrentInternal(int bus)
{
	struct udcdBusState *state;
	int result;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	result = udcdDeactivateBus(bus);
	if (result < 0 && result != (int)SCE_UDCD_ERROR_INVALID_ARGUMENT)
		return result;
	state = &udcdState.buses[bus];
	while (state->drivers) {
		result = udcdStopDriverImpl(state->drivers->driverName, 0, NULL, bus);
		if (result < 0 && result != (int)SCE_UDCD_ERROR_INVALID_ARGUMENT)
			return result;
	}
	result = udcdStopDriver("USBDeviceControllerDriver", 0, NULL, bus);
	return result >= 0 || result == (int)SCE_UDCD_ERROR_INVALID_ARGUMENT ? 0 : result;
}

int ksceUdcdStartCurrentInternal(int unused, int bus)
{
	SceUdcdDriver *driver;
	int result;

	(void)unused;
	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	result = udcdStartDriver("USBDeviceControllerDriver", 0, NULL, bus);
	if (result < 0)
		return result;
	while (udcdState.registeredDrivers[bus]) {
		result = udcdStartDriverImpl(
			udcdState.registeredDrivers[bus]->driverName, 0, NULL, bus);
		if (result < 0)
			goto stop_controller;
	}
	result = udcdActivateBus(0x4e4, 0, bus);
	if (result >= 0)
		return 0;
	for (driver = udcdState.registeredDrivers[bus]; driver; driver = driver->link) {
		const char *name = driver->driverName;

		if (udcdIsDriverBus(bus) && udcdGetActivationEnabled(bus))
			udcdStopDriver(name, 0, NULL, bus);
	}
stop_controller:
	if (udcdIsDriverBus(bus) && udcdGetActivationEnabled(bus))
		udcdStopDriver("USBDeviceControllerDriver", 0, NULL, bus);
	return result;
}

/* Original name unknown. Restore MTP after its tracked process exits. */
int udcdProcessEvent(SceUID process, int event, ProcEventParam *param, int flags)
{
	struct udcdBusState *state = &udcdState.buses[2];
	SceKernelIntrStatus status;
	unsigned int information;
	int result;

	if (event == 0x202)
		return udcdState.processEventResult;
	if (event != 0x200 && event != 0x201)
		return 0;
	if (process == state->mtpProcess && ksceUdcdGetDrvState("USB_MTP_Driver") == 1 &&
	    ksceUdcdGetDrvState("USBStor_Driver") != 2) {
		if (event == 0x200) {
			status = SceCpuForDriver_D32ACE9E(&state->lock);
			state->activationEnabled = 0;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
		} else {
			result = ksceUdcdStopCurrentInternal(2);
			if (result < 0) {
				udcdState.processEventResult = result;
				return 0;
			}
			udcdStartDriver("USBDeviceControllerDriver", 0, NULL, 2);
			udcdStartDriver("USB_MTP_Driver", 0, NULL, 2);
			information = state->pendingEventDetail;
			udcdActivateBus(0x4e4, state->busPowered, 2);
			state->pendingEventDetail = information;
			status = SceCpuForDriver_D32ACE9E(&state->lock);
			state->activationEnabled = 1;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
		}
	}
	udcdState.processEventResult = event == 0x200 ? -1 : 0;
	return 0;
}

static inline __attribute__((always_inline)) int udcdChangeEndpointState(
	SceUdcdEndpoint *endpoint, int bus, int stall)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelIntrStatus status;
	int number, result;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (!endpoint) {
		result = 0x80243401;
	} else {
		number = endpoint->endpointNumber;
		if (!number) {
			result = 0x80243402;
		} else if (state->controllerState == UDCD_CONTROLLER_STARTED && state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED &&
			   state->connectionState != SCE_UDCD_STATUS_CONNECTION_SUSPENDED) {
			if (stall)
				udcdState.operations[bus].stallEndpoint(number, bus);
			else
				udcdState.operations[bus].abortEndpoint(number, bus);
			udcdCancelEndpointRequests(number, &status, bus);
			result = 0;
		} else {
			result = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

int ksceUdcdClearFIFOInternal(SceUdcdEndpoint *endpoint, int bus)
{
	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	return udcdChangeEndpointState(endpoint, bus, 0);
}

int ksceUdcdStallInternal(SceUdcdEndpoint *endpoint, int bus)
{
	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	return udcdChangeEndpointState(endpoint, bus, 1);
}

int ksceUdcdClearFIFO(SceUdcdEndpoint *endpoint)
{
	return udcdChangeEndpointState(endpoint, 2, 0);
}

int ksceUdcdStall(SceUdcdEndpoint *endpoint)
{
	return udcdChangeEndpointState(endpoint, 2, 1);
}

/* Original name unknown. Cancel all requests on the caller's endpoint. */
int SceUdcdForDriver_B1C2EA5A(SceUdcdEndpoint *endpoint, int bus)
{
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	int result;

	if (bus != 1 && bus != 2)
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!endpoint)
		return 0x80243401;
	state = &udcdState.buses[bus];
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (state->controllerState == UDCD_CONTROLLER_STARTED)
		result = udcdCancelEndpointRequests(endpoint->endpointNumber, &status, bus);
	else
		result = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return result;
}

int ksceUdcdRegisterInternal(SceUdcdDriver *driver, int bus)
{
	SceUdcdDriver *head, *current;
	const char *name;
	int interfaces;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!driver)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_DRIVER;
	name = driver->driverName;
	if (!name || strnlen(name, 0x40) == 0x40)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_NAME;
	if (!driver->start || !driver->stop)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_FUNCS;
	interfaces = driver->interface->numInterfaces;
	if (driver->interface->expectNumber >= 0)
		interfaces += driver->interface->expectNumber;
	if (interfaces > 8)
		return UDCD_ERROR_TOO_MANY_INTERFACES;
	head = udcdState.registeredDrivers[bus];
	if (!head) {
		driver->bus = bus;
		driver->link = NULL;
		udcdState.registeredDrivers[bus] = driver;
		return 0;
	}
	for (current = head; current; current = current->link)
		if (!strncmp(name, current->driverName, 0x40))
			return SCE_UDCD_ERROR_INVALID_ARGUMENT;
	for (current = head; current->link; current = current->link)
		;
	current->link = driver;
	/* Sony sets bus only when inserting into an empty registered list. */
	driver->link = NULL;
	return 0;
}

int ksceUdcdRegister(SceUdcdDriver *driver)
{
	return ksceUdcdRegisterInternal(driver, 2);
}

int ksceUdcdUnregisterInternal(SceUdcdDriver *driver, int bus)
{
	SceUdcdDriver *current, *previous = NULL;
	const char *name;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!driver)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_DRIVER;
	name = driver->driverName;
	if (!name || strnlen(name, 0x40) == 0x40)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_NAME;
	if (udcdState.buses[bus].controllerState == UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_MEMORY_EXHAUSTED;
	for (current = udcdState.registeredDrivers[bus]; current;
	     previous = current, current = current->link) {
		if (strncmp(name, current->driverName, 0x40))
			continue;
		if (previous)
			previous->link = current->link;
		else
			udcdState.registeredDrivers[bus] = current->link;
		return 0;
	}
	for (current = udcdState.buses[bus].drivers; current; current = current->link)
		if (!strncmp(name, current->driverName, 0x40))
			return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	return UDCD_ERROR_DRIVER_NOT_REGISTERED;
}

int ksceUdcdUnregister(SceUdcdDriver *driver)
{
	return ksceUdcdUnregisterInternal(driver, 2);
}

static inline __attribute__((always_inline)) int udcdGetDriverState(
	const char *name, int bus)
{
	SceUdcdDriver *driver;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (!name || strnlen(name, 0x40) == 0x40)
		return SCE_UDCD_ERROR_USBDRIVER_INVALID_NAME;
	for (driver = udcdState.registeredDrivers[bus]; driver; driver = driver->link)
		if (!strncmp(name, driver->driverName, 0x40))
			return 1;
	for (driver = udcdState.buses[bus].drivers; driver; driver = driver->link)
		if (!strncmp(name, driver->driverName, 0x40))
			return 2;
	return -1;
}

int ksceUdcdGetDrvState(const char *name)
{
	return udcdGetDriverState(name, 2);
}

int ksceUdcdGetDrvStateInternal(const char *name, int bus)
{
	return udcdGetDriverState(name, bus);
}

/* Original name unknown. Reset the registered driver list during bus setup. */
int udcdInitializeDriverList(int bus)
{
	udcdState.registeredDrivers[bus] = NULL;
	return 0;
}

/* Original names unknown. Controller request-state operations. */
void udcdFinishControllerSend(int number, SceKernelIntrStatus *status, int bus)
{
	struct udcdControllerEndpoint *endpoint = &udcdState.controllers[bus].endpoints[number];

	if (endpoint->flags & 1) {
		endpoint->flags &= ~1;
		endpoint->controlFlags &= ~0x20;
	}
}

void udcdFinishControllerReceive(int number, SceKernelIntrStatus *status, int bus)
{
	struct udcdControllerEndpoint *endpoint = &udcdState.controllers[bus].endpoints[number];

	if (endpoint->flags & 1) {
		endpoint->flags &= ~1;
		endpoint->controlFlags &= ~0x20;
	}
}

int udcdCancelControllerRequest(int number, int result, int bus)
{
	struct udcdControllerEndpoint *endpoint = &udcdState.controllers[bus].endpoints[number];
	unsigned int flags = endpoint->controlFlags;

	if (!(flags & 0x23)) {
		endpoint->controlFlags = flags & ~3;
		return 0;
	}
	endpoint->controlFlags = flags | (result == -1 ? 1 : 2);
	return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
}

int udcdIsControllerEndpointStalled(int number, int bus)
{
	return udcdState.controllers[bus].endpoints[number].flags >> 7;
}

int udcdInitializeControllerQueues(int bus)
{
	int number;

	for (number = 0; number < 9; number++) {
		udcdState.controllers[bus].endpoints[number].queue =
			&udcdState.buses[bus].endpoints[number];
		udcdState.controllers[bus].endpoints[number].controlFlags &= ~0x20;
	}
	return 0;
}

int udcdClearControllerQueues(int bus)
{
	int number;

	for (number = 0; number < 9; number++)
		udcdState.controllers[bus].endpoints[number].queue = NULL;
	for (number = 0; number < 9; number++)
		udcdState.controllers[bus].endpoints[number].controlFlags &= ~0x20;
	return 0;
}

/* Original name unknown. Answer GET_CONFIGURATION. */
int udcdGetConfigurationRequest(SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	int result;

	udcdInitializeControlTransfer(state);
	if (state->controlRequest.wLength != 1 || state->controlRequest.wValue ||
	    state->controlRequest.wIndex)
		return -1;
	state->controlTransfer.size = 1;
	state->controlTransfer.data = udcdState.controlResponse;
	udcdState.controlResponse[0] = state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED;
	result = udcdQueueControlTransfer(status, bus);
	if (result < 0)
		return result;
	udcdBroadcastControlRequest(status, bus);
	return 0;
}

/* Original name unknown. Answer device/interface/endpoint GET_STATUS. */
int udcdGetStatusRequest(SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	unsigned int recipient, endpoint, attributes;
	int result;

	udcdInitializeControlTransfer(state);
	if (state->controlRequest.wValue || state->controlRequest.wLength != 2)
		return -1;
	state->controlTransfer.size = 2;
	if (state->connectionState == UDCD_CONNECTION_ADDRESSED && state->controlRequest.wIndex)
		return -1;
	recipient = state->controlRequest.bmRequestType & 0x1f;
	if (recipient == 1) {
		if (!state->controlRequest.wIndex)
			return -1;
		udcdState.controlResponse[0] = 0;
	} else if (!recipient) {
		if (state->controlRequest.wIndex)
			return -1;
		attributes = state->fullSpeedConfiguration->configDescriptors->bmAttributes;
		udcdState.controlResponse[0] = (attributes >> 6) & 1;
		if (state->fullSpeedConfiguration->configDescriptors->bmAttributes & 0x20)
			udcdState.controlResponse[0] |= 2;
	} else if (recipient == 2) {
		endpoint = state->controlRequest.wIndex & 0xf;
		if (endpoint) {
			if (endpoint > 7 || !state->endpoints[endpoint].endpoint)
				return -1;
			result = udcdState.operations[bus].isEndpointStalled(endpoint, bus);
			udcdState.controlResponse[0] = result != 0;
		} else {
			udcdState.controlResponse[0] = 0;
		}
	} else {
		return -1;
	}
	state->controlTransfer.data = udcdState.controlResponse;
	udcdState.controlResponse[1] = 0;
	result = udcdQueueControlTransfer(status, bus);
	if (result < 0)
		return result;
	udcdBroadcastControlRequest(status, bus);
	return 0;
}
