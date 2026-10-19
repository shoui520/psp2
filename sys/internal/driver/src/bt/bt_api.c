/* Sony Bluetooth API adapters; original source filename unavailable. */

#include <stdint.h>
#include <string.h>
#include <sysmem/sysroot_bt.h>
#include <intrmgr/syscall.h>
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <bt/bt_hid.h>
#include <bt/bt_event.h>
#include <bt/bt_registered.h>
#include <bt/bt_pairing.h>
#include <bt/bt_audio.h>
#include <bt/bt_test.h>
#include <sysmem/sysmem.h>
#include "bt_internal.h"
#include <bt/bt.h>

/* Name unknown; the final argument selects user-buffer copying. */
int GetRegisteredInfo(uint64_t deviceAddress, void *buffer, int maxCount,
		      int userMode);
int BtLockTransport(void);
int BtUnlockTransport(int result);
int BtGetHidReportDescriptor(BtStack *stack, uint64_t deviceAddress,
			     void *buffer, int capacity);
const char *BtLookupDeviceName(BtStack *stack, uint64_t deviceAddress);
int BtGetDeviceId(BtStack *stack, uint64_t deviceAddress, void *buffer,
		  int userMode);
BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress);
int BtDeleteRegisteredDevice(BtStack *stack, uint64_t deviceAddress);
int BtPushImage(BtConnection *connection, const char *path);
int BtSetImageReceivePath(const char *path);
int BtSendSerialData(BtConnection *connection, const void *data,
		     unsigned int length);
int BtSendEchoRequest(BtConnection *connection, const void *data,
		      unsigned int length);
int BtProcessInquiryResult(BtStack *stack, uint64_t deviceAddress,
			  uint8_t pageScanRepetitionMode, uint32_t deviceClass,
			  uint16_t clockOffset, int8_t rssi,
			  const uint8_t *extendedInquiryResponse);
BtChannel *BtFindChannel(BtConnection *connection, uint8_t selector,
			 int identifier);
uintptr_t BtNotifyHidVuEvent(uint64_t deviceAddress, uint8_t eventId,
			   uint8_t status, const void *data, uint16_t length);
int BtRequestHidVuReport(BtStack *stack, uint64_t deviceAddress, uint8_t command,
			 uint8_t reportId, const void *data, int length,
			 int userMode);
void BtConfigureAsynchronously(void *argument);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
uint64_t BtRecordInquiryProgress(BtStack *stack);
int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);
int BtFlushRegisteredDevices(BtStack *stack);
int BtCountRegisteredDevices(BtStack *stack);
BtDevice *BtGetOrCreateDevice(BtStack *stack, uint64_t deviceAddress,
			    unsigned int create);
int BtSetAvrcpTitle(BtStack *stack, const void *title, unsigned int length);
int BtSetAvrcpPlayStatus(BtStack *stack, int status);
int BtSendAvrcpButton(BtConnection *connection, int button);
int BtSendAvrcpVolume(BtConnection *connection, int volume);
int BtGetAvrcpVolume(BtConnection *connection);
int BtQueueHidTransfer(BtStack *stack, uint64_t deviceAddress,
		       SceBtHidRequest *request);
int BtGetAudioStatistics(void *buffer);
int BtAdjustAudioReceiveBuffer(int operation);
int BtSetHidCaptureBuffer(SceUID processId, uint8_t *buffer, int capacity);
int BtInjectHidPacket(BtStack *stack, const void *buffer, unsigned int length,
		      int userMode);
extern int gBtAudioUnderruns;
extern int gBtAudioOverruns;
extern int gBtPendingAudioPackets;
extern int gBtPanUserEnabled;
extern int gBtNetworkAccessPointEnabled;
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
int BtRegisterDevice(BtStack *stack, uint64_t deviceAddress);
int BtBeginDisconnect(BtConnection *connection);
int BtStartAudio(BtStack *stack, uint64_t deviceAddress, unsigned int profile);
int BtStopAudio(BtStack *stack, uint64_t deviceAddress, unsigned int profile);
int BtSendAudio(BtStack *stack, uint64_t deviceAddress, void *buffer,
		unsigned int size, int userMode);
int BtRecvAudio(BtStack *stack, uint64_t deviceAddress, void *buffer,
		unsigned int size, int userMode);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);

int SceBtForDriver_E9936446(uint64_t deviceAddress)
{
	BtConnection *connection;
	int result;

	if (!SceIntrmgrForDriver_182EE3E3()) {
		result = BtLockTransport();
		if (result < 0)
			return result;
	}
	connection = BtFindConnection(&gBtStack, 33, deviceAddress);
	if (!connection || !connection->device) {
		result = 1;
	} else if (connection->flags & 0x40) {
		result = 4;
	} else if (connection->flags & 0x100) {
		result = 3;
		if (connection->device->profiles & 0x2000000c)
			result = connection->activeProfiles & 0x2000000c ? 5 : 6;
	} else {
		result = 2;
	}
	if (SceIntrmgrForDriver_182EE3E3())
		return result;
	return BtUnlockTransport(result);
}

int SceBtForDriver_F86D25E2(uint64_t deviceAddress, void *buffer, int maxCount)
{
	return GetRegisteredInfo(deviceAddress, buffer, maxCount, 0);
}

static inline int BtSetConfiguration(int configuration)
{
	unsigned int mode;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result < 0)
		return result;
	result = gBtStack.deviceRegistryState;
	if (!result)
		return BtUnlockTransport((int)0x802f1501);
	if (result < 0)
		return BtUnlockTransport(result);
	switch (configuration) {
	case 0:
		mode = gBtStack.mode;
		if (mode & 1)
			mode &= ~0x80;
		gBtStack.mode = mode & ~1;
		break;
	case 1:
		mode = gBtStack.mode;
		if (!(mode & 1))
			mode &= ~0x80;
		gBtStack.mode = mode | 1;
		break;
	case 2:
		mode = gBtStack.mode;
		if ((mode & 0x82) == 0x82)
			mode = (mode & ~0x80) | 1;
		gBtStack.mode = mode & ~2;
		break;
	case 3:
		mode = gBtStack.mode;
		if ((mode & 3) == 1)
			mode = (mode & ~1) | 0x80;
		gBtStack.mode = mode | 2;
		break;
	default:
		return BtUnlockTransport((int)0x802f1502);
	}
	BtQueueTimer(NULL, BtConfigureAsynchronously, NULL, 0);
	return BtUnlockTransport(result);
}

int SceBtForDriver_EBDCC992(int configuration)
{
	return BtSetConfiguration(configuration);
}

int sceBtSetConfiguration(int configuration)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtSetConfiguration(configuration);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtGetConfiguration(void)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result < 0)
		return result;
	result = gBtStack.deviceRegistryState;
	if (!result)
		result = (int)0x802f1501;
	else if (result >= 0)
		result = (unsigned char)gBtStack.mode;
	return BtUnlockTransport(result);
}

int SceBtForDriver_94C66A3B(void)
{
	return BtGetConfiguration();
}

int sceBtGetConfiguration(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtGetConfiguration();

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_AD31A43E(void)
{
	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	return gBtStack.lastError;
}

int sceBtGetLastError(void)
{
	return SceBtForDriver_AD31A43E();
}

int sceBtGetRegisteredInfo(uint64_t deviceAddress, void *buffer, int maxCount)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = GetRegisteredInfo(deviceAddress, buffer, maxCount, 1);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_E6F659E0(uint64_t deviceAddress)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result < 0)
		return result;
	if (BtFindConnection(&gBtStack, 33, deviceAddress))
		result = (int)0x802f0109;
	else
		result = BtDeleteRegisteredDevice(&gBtStack, deviceAddress);
	return BtUnlockTransport(result);
}

int sceBtDeleteRegisteredInfo(uint64_t deviceAddress)
{
	SceUInt32 saved = sceSyscallEnter();
	int result;

	if (SceIntrmgrForDriver_182EE3E3()) {
		result = (int)0x802f1901;
		goto out;
	}
	result = BtLockTransport();
	if (result < 0)
		goto out;
	if (BtFindConnection(&gBtStack, 33, deviceAddress))
		result = (int)0x802f0109;
	else
		result = BtDeleteRegisteredDevice(&gBtStack, deviceAddress);
	result = BtUnlockTransport(result);
out:
	sceSyscallLeave(saved);
	return result;
}

int sceBtGetConnectingInfo(uint64_t deviceAddress)
{
	SceUInt32 saved = sceSyscallEnter();
	BtConnection *connection;
	int result;

	if (!SceIntrmgrForDriver_182EE3E3()) {
		result = BtLockTransport();
		if (result < 0)
			goto out;
	}
	connection = BtFindConnection(&gBtStack, 33, deviceAddress);
	if (!connection || !connection->device) {
		result = 1;
	} else if (connection->flags & 0x40) {
		result = 4;
	} else if (connection->flags & 0x100) {
		result = 3;
		if (connection->device->profiles & 0x2000000c)
			result = connection->activeProfiles & 0x2000000c ? 5 : 6;
	} else {
		result = 2;
	}
	if (!SceIntrmgrForDriver_182EE3E3())
		result = BtUnlockTransport(result);
out:
	sceSyscallLeave(saved);
	return result;
}

static inline int BtStartInquiry(void)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	if (gBtStack.state == 28) {
		BtRecordInquiryProgress(&gBtStack);
		gBtStack.state = 30;
		result = BtSendHciCommand(&gBtStack, 0x401, "311", 0x9e8b33, 48, 0);
		gBtStack.inquiryCompletionProcessId = -1;
		gBtStack.inquiryProcessId = sceKernelGetProcessId();
	} else {
		result = (int)0x802f0201;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_9DC7E7E5(void)
{
	return BtStartInquiry();
}

int sceBtStartInquiry(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtStartInquiry();

	sceSyscallLeave(saved);
	return result;
}

static inline int BtStopInquiry(void)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	switch (gBtStack.state) {
	case 31:
		gBtStack.state = 33;
		break;
	case 32:
		gBtStack.state = 34;
		BtSendHciCommand(&gBtStack, 0x41a,
				 gBtDeviceAddressHciFormat, gBtStack.inquiryAddress);
		break;
	case 30:
		gBtStack.state = 33;
		BtSendHciCommand(&gBtStack, 0x402, gBtEmptyHciFormat);
		break;
	default:
		break;
	}
	gBtStack.inquiryProcessId = -1;
	gBtStack.inquiryCompletionProcessId = -1;
	return BtUnlockTransport(0);
}

int SceBtForDriver_6A1D9598(void)
{
	return BtStopInquiry();
}

int sceBtStopInquiry(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtStopInquiry();

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_633F784C(uint64_t deviceAddress, char *buffer)
{
	const char *name;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result)
		return result;
	name = BtLookupDeviceName(&gBtStack, deviceAddress);
	if (name)
		result = BtCopyOut(name, buffer, 121, 0);
	else
		result = (int)0x802f0c01;
	return BtUnlockTransport(result);
}

int sceBtGetDeviceName(uint64_t deviceAddress, char *buffer)
{
	SceUInt32 saved = sceSyscallEnter();
	const char *name;
	int result;

	if (SceIntrmgrForDriver_182EE3E3()) {
		result = (int)0x802f1901;
		goto out;
	}
	result = BtLockTransport();
	if (result)
		goto out;
	name = BtLookupDeviceName(&gBtStack, deviceAddress);
	if (name)
		result = BtCopyOut(name, buffer, 121, 1);
	else
		result = (int)0x802f0c01;
	result = BtUnlockTransport(result);
out:
	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_780A5557(uint64_t deviceAddress, void *buffer)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result)
		return result;
	return BtUnlockTransport(BtGetDeviceId(&gBtStack, deviceAddress, buffer, 0));
}

int sceBtGetDeviceId(uint64_t deviceAddress, void *buffer)
{
	SceUInt32 saved = sceSyscallEnter();
	int result;

	if (SceIntrmgrForDriver_182EE3E3()) {
		result = (int)0x802f1901;
		goto out;
	}
	result = BtLockTransport();
	if (result)
		goto out;
	result = BtUnlockTransport(BtGetDeviceId(&gBtStack, deviceAddress, buffer, 1));
out:
	sceSyscallLeave(saved);
	return result;
}

static inline int BtReplyPinCode(unsigned int mac0, unsigned int mac1,
				unsigned char *code, SceSize length, int userMode)
{
	unsigned char pin[16];
	uint64_t deviceAddress = (uint64_t)mac1 << 32 | mac0;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	if (code && length <= sizeof(pin)) {
		result = BtCopyIn(code, pin, length, userMode);
		if (result >= 0)
			result = BtSendHciCommand(&gBtStack, 0x40d, "6P",
						 deviceAddress, length, pin);
	} else {
		BtSendHciCommand(&gBtStack, 0x40e, gBtDeviceAddressHciFormat,
				 deviceAddress);
		result = length <= sizeof(pin) ? 0 : (int)0x802f0206;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_BB69A62A(unsigned int mac0, unsigned int mac1,
		       unsigned char *code, SceSize length)
{
	return BtReplyPinCode(mac0, mac1, code, length, 0);
}

int sceBtReplyPinCode(unsigned int mac0, unsigned int mac1,
		      unsigned char *code, SceSize length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtReplyPinCode(mac0, mac1, code, length, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtReplyUserConfirmation(unsigned int mac0, unsigned int mac1,
					int accepted)
{
	uint64_t deviceAddress = (uint64_t)mac1 << 32 | mac0;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	if (BtFindConnection(&gBtStack, 1, deviceAddress)) {
		if (accepted)
			result = BtSendHciCommand(&gBtStack, 0x42c,
						 gBtDeviceAddressHciFormat, deviceAddress);
		else
			result = BtSendHciCommand(&gBtStack, 0x42d,
						 gBtDeviceAddressHciFormat, deviceAddress);
	} else {
		result = (int)0x802f0207;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_B5FA291C(unsigned int mac0, unsigned int mac1, int accepted)
{
	return BtReplyUserConfirmation(mac0, mac1, accepted);
}

int sceBtReplyUserConfirmation(unsigned int mac0, unsigned int mac1, int accepted)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtReplyUserConfirmation(mac0, mac1, accepted);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtSetInquiryScan(int enabled)
{
	unsigned int scanEnable;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	if (enabled)
		scanEnable = gBtStack.scanEnable | 1;
	else
		scanEnable = gBtStack.scanEnable & ~1;
	if (scanEnable != gBtStack.scanEnable) {
		gBtStack.scanEnable = scanEnable;
		BtSendHciCommand(&gBtStack, 0xc1a, "1", scanEnable);
	}
	if (!(scanEnable & 1))
		BtFlushRegisteredDevices(&gBtStack);
	return BtUnlockTransport(0);
}

int SceBtForDriver_4F5A852E(int enabled)
{
	return BtSetInquiryScan(enabled);
}

int sceBtSetInquiryScan(int enabled)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtSetInquiryScan(enabled);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtStartConnect(uint64_t deviceAddress)
{
	BtDevice *device;
	BtConnection *connection;
	unsigned int flags;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	if (gBtStack.state != 28) {
		result = (int)0x802f0204;
		goto unlock;
	}
	device = BtGetOrCreateDevice(&gBtStack, deviceAddress, 0);
	if (!device) {
		device = BtRestoreDeviceFromHistory(&gBtStack, deviceAddress);
		if (!device) {
			result = BtCountRegisteredDevices(&gBtStack) > 30 ?
				(int)0x802f0209 : (int)0x802f0202;
			goto unlock;
		}
	}
	flags = device->flags;
	if (flags & 0x10000) {
		result = (int)0x802f020e;
		goto unlock;
	}
	if (!(flags & 0x1000))
		device->flags = flags & ~0x2000;
	else if ((device->profiles & 0x40) && !(flags & 1)) {
		result = (int)0x802f0203;
		goto unlock;
	}
	connection = BtFindConnection(&gBtStack, 17, deviceAddress);
	if (!connection) {
		result = (int)0x802f020a;
		goto unlock;
	}
	flags = connection->flags;
	connection->device = device;
	gBtStack.connectingAddress = deviceAddress;
	connection->flags = flags | 1;
	connection->packetType = 0x3318;
	gBtStack.state = 40;
	result = BtSendHciCommand(&gBtStack, 0x405, "621-21", deviceAddress,
				 0x3318, device->pageScanRepetitionMode,
				 device->clockOffset, 0);
unlock:
	return BtUnlockTransport(result);
}

int SceBtForDriver_6059113A(unsigned int mac0, unsigned int mac1)
{
	return BtStartConnect((uint64_t)mac1 << 32 | mac0);
}

int sceBtStartConnect(unsigned int mac0, unsigned int mac1)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtStartConnect((uint64_t)mac1 << 32 | mac0);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtRegisterCallback(SceUID callback, uint64_t eventMask,
				    SceUID processId)
{
	BtEventSubscriber *subscriber;
	SceUID threadId;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	threadId = sceKernelGetThreadId();
	result = BtLockTransport();
	if (result != 0)
		return result;
	subscriber = gBtStack.subscribers;
	do {
		if (subscriber->threadId == threadId) {
			subscriber->callbackId = callback;
			subscriber->eventMask = eventMask;
			goto unlock;
		}
		subscriber++;
	} while (subscriber < gBtStack.subscribers + 8);
	subscriber = gBtStack.subscribers;
	while (subscriber->threadId) {
		if (++subscriber >= gBtStack.subscribers + 8) {
			result = (int)0x802f0c02;
			goto unlock;
		}
	}
	subscriber->threadId = threadId;
	subscriber->callbackId = callback;
	subscriber->eventMask = eventMask;
	subscriber->readPosition = 0;
	subscriber->droppedEvents = 0;
	subscriber->count = 0;
	subscriber->writePosition = 0;
	subscriber->processId = processId;
	subscriber->capacity = 16;
unlock:
	return BtUnlockTransport(result);
}

int SceBtForDriver_120AC1F7(SceUID callback, uint64_t eventMask)
{
	return BtRegisterCallback(callback, eventMask, -1);
}

int sceBtRegisterCallback(SceUID callback, uint64_t eventMask)
{
	SceUID processId = sceKernelGetProcessId();
	SceUID kernelCallback;
	SceUInt32 saved;
	int result;

	if (processId < 0)
		return processId;
	kernelCallback = SceSysmemForDriver_45D22597(processId, callback);
	if (kernelCallback < 0)
		return kernelCallback;
	saved = sceSyscallEnter();
	result = BtRegisterCallback(kernelCallback, eventMask, processId);
	sceSyscallLeave(saved);
	return result;
}

static inline int BtUnregisterCallback(void)
{
	BtEventSubscriber *subscriber;
	SceUID threadId;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	threadId = sceKernelGetThreadId();
	result = BtLockTransport();
	if (result != 0)
		return result;
	subscriber = gBtStack.subscribers;
	while (subscriber->threadId != threadId) {
		if (++subscriber >= gBtStack.subscribers + 8) {
			result = (int)0x802f0c03;
			goto unlock;
		}
	}
	subscriber->threadId = 0;
	subscriber->callbackId = 0;
unlock:
	return BtUnlockTransport(result);
}

int SceBtForDriver_BF72E1BE(SceUID callback)
{
	(void)callback;
	return BtUnregisterCallback();
}

int sceBtUnregisterCallback(SceUID callback)
{
	SceUInt32 saved = sceSyscallEnter();
	int result;

	(void)callback;
	result = BtUnregisterCallback();
	sceSyscallLeave(saved);
	return result;
}

static inline int BtReadEvent(SceBtEvent *events, int maxCount, int userMode)
{
	BtEventSubscriber *subscriber;
	SceBtEvent event;
	SceUID threadId;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	threadId = sceKernelGetThreadId();
	result = BtLockTransport();
	if (result != 0)
		return result;
	subscriber = gBtStack.subscribers;
	while (subscriber->threadId != threadId) {
		if (++subscriber >= gBtStack.subscribers + 8) {
			result = (int)0x802f0c03;
			goto unlock;
		}
	}
	if (subscriber->droppedEvents) {
		subscriber->droppedEvents = 0;
		result = (int)0x802f0c04;
		goto unlock;
	}
	if (subscriber->count > 0 && maxCount != 0) {
		do {
			const SceBtEvent *queued =
				&subscriber->events[subscriber->readPosition];
			int copied;
			int readPosition;

			event.reserved = 0;
			event.id = queued->id;
			event.status = queued->status;
			event.argument = queued->argument;
			event.mac0 = queued->mac0;
			event.mac1 = queued->mac1;
			copied = BtCopyOut(&event, events, sizeof(event), userMode);
			if (copied < 0) {
				result = copied;
				break;
			}
			events++;
			result++;
			readPosition = subscriber->readPosition + 1;
			subscriber->count--;
			if (readPosition < subscriber->capacity)
				subscriber->readPosition = readPosition;
			else
				subscriber->readPosition = 0;
		} while (subscriber->count > 0 && result != maxCount);
	}
unlock:
	return BtUnlockTransport(result);
}

int SceBtForDriver_5ABB9A9D(SceBtEvent *events, int maxCount)
{
	return BtReadEvent(events, maxCount, 0);
}

int sceBtReadEvent(SceBtEvent *events, int maxCount)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtReadEvent(events, maxCount, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtCopyAvrcpTitle(const void *title, unsigned int length,
				 int userMode)
{
	int result;

	if (length > 1024)
		return (int)0x802f1201;
	result = BtLockTransport();
	if (result)
		return result;
	result = BtCopyIn(title, gBtStack.scratch, length, userMode);
	if (result >= 0) {
		gBtStack.scratch[length] = 0;
		result = BtSetAvrcpTitle(&gBtStack, gBtStack.scratch, length);
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_91EF55AD(const void *title, unsigned int length)
{
	return BtCopyAvrcpTitle(title, length, 0);
}

int sceBtAvrcpSetTitle(const void *title, unsigned int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtCopyAvrcpTitle(title, length, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtApplyAvrcpPlayStatus(int status)
{
	int result = BtLockTransport();

	if (result)
		return result;
	switch (status) {
	case 0:
		result = BtSetAvrcpPlayStatus(&gBtStack, 0);
		break;
	case 1:
		result = BtSetAvrcpPlayStatus(&gBtStack, 1);
		break;
	case 2:
		result = BtSetAvrcpPlayStatus(&gBtStack, 2);
		break;
	case 3:
		result = BtSetAvrcpPlayStatus(&gBtStack, 3);
		break;
	case 4:
		result = BtSetAvrcpPlayStatus(&gBtStack, 4);
		break;
	default:
		result = (int)0x802f1202;
		break;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_C5E7349C(int status)
{
	return BtApplyAvrcpPlayStatus(status);
}

int sceBtAvrcpSetPlayStatus(int status)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtApplyAvrcpPlayStatus(status);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtAvrcpSendButton(uint64_t deviceAddress, int button)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (connection)
		result = BtSendAvrcpButton(connection, button);
	else
		result = (int)0x802f0604;
	return BtUnlockTransport(result);
}

int SceBtForDriver_EC5F8519(uint64_t deviceAddress, int button)
{
	return BtAvrcpSendButton(deviceAddress, button);
}

int sceBtAvrcpSendButton(uint64_t deviceAddress, int button)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtAvrcpSendButton(deviceAddress, button);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtAvrcpSendVolume(uint64_t deviceAddress, int volume)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (connection)
		result = BtSendAvrcpVolume(connection, volume);
	else
		result = (int)0x802f0604;
	return BtUnlockTransport(result);
}

int SceBtForDriver_7689DA3D(uint64_t deviceAddress, int volume)
{
	return BtAvrcpSendVolume(deviceAddress, volume);
}

int sceBtAvrcpSendVolume(uint64_t deviceAddress, int volume)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtAvrcpSendVolume(deviceAddress, volume);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtAvrcpReadVolume(uint64_t deviceAddress)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (connection)
		result = BtGetAvrcpVolume(connection);
	else
		result = (int)0x802f0604;
	return BtUnlockTransport(result);
}

int SceBtForDriver_C9C70056(uint64_t deviceAddress)
{
	return BtAvrcpReadVolume(deviceAddress);
}

int sceBtAvrcpReadVolume(uint64_t deviceAddress)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtAvrcpReadVolume(deviceAddress);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_06CB4ABA(uint64_t deviceAddress, void *buffer,
			       unsigned int capacity)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result)
		return result;
	return BtUnlockTransport(BtGetHidReportDescriptor(&gBtStack,
		deviceAddress, buffer, capacity));
}

int SceBtForDriver_F9DCEC77(uint32_t mac0, uint32_t mac1, SceBtHidRequest *request)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	if (request) {
		request->internalFlags[1] = 0;
		request->internalFlags[0] = 0;
	}
	result = BtQueueHidTransfer(&gBtStack,
		((uint64_t)mac1 << 32) | mac0, request);
	return BtUnlockTransport(result);
}

int SceBtForDriver_10D68FAE(const void *peerAddress, const void *deviceClass,
			  const void *previousHostAddress, unsigned int vendorId,
			  unsigned int productId, unsigned int deviceVersion,
			  const char *name, void *hostAddressOut, void *linkKeyOut)
{
	uint64_t address;
	uint64_t previousHost;
	uint32_t classValue;
	uint64_t hostAddress;
	uint64_t oldestAddress;
	BtDevice *device;
	BtConnection *connection;
	unsigned int product = (uint16_t)productId;
	int retries = 0;
	int result;

	BtUnpack(peerAddress, 6, gBtDeviceAddressHciFormat, &address);
	BtUnpack(deviceClass, 3, "3", &classValue);
	BtUnpack(previousHostAddress, 6, gBtDeviceAddressHciFormat, &previousHost);
	if (vendorId == 0x54c && product == 0x9cc)
		product = 0x5c4;
	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	while (!gBtStack.deviceAddress || !gBtStack.deviceRegistryState) {
		if (retries == 30)
			return (int)0x802f1802;
		retries++;
		BtDelayThread(1000000);
	}
	if (retries == 30)
		return (int)0x802f1802;
	if (!(gBtStack.mode & 0x10000))
		return (int)0x802f1801;
	result = BtLockTransport();
	if (result)
		return result;
	device = BtGetOrCreateDevice(&gBtStack, address, 1);
	if (!device) {
		oldestAddress = gBtStack.deviceTail->deviceAddress;
		if ((vendorId != 0x54c || (product != 0x5c4 && product != 0x268)) &&
		    BtCountRegisteredDevices(&gBtStack) > 30) {
			result = (int)0x802f1803;
			goto unlock;
		}
		connection = BtFindConnection(&gBtStack, 33, oldestAddress);
		if (connection) {
			result = BtBeginDisconnect(connection);
			if (result)
				goto unlock;
			connection->device->flags |= 0x10000;
			retries = 50;
			for (;;) {
				BtUnlockTransport(0);
				BtDelayThread(100000);
				result = BtLockTransport();
				if (result)
					return result;
				if (!BtFindConnection(&gBtStack, 33, oldestAddress))
					break;
				if (!--retries) {
					result = (int)0x802f1804;
					goto unlock;
				}
			}
		}
		if (BtCountRegisteredDevices(&gBtStack) > 31)
			BtDeleteRegisteredDevice(&gBtStack,
				gBtStack.deviceTail->deviceAddress);
		device = BtGetOrCreateDevice(&gBtStack, address, 1);
		if (!device) {
			BtNotifyEvent(30, 0, product | (vendorId << 16), address);
			result = (int)0x802f1803;
			goto unlock;
		}
	}
	hostAddress = gBtStack.deviceAddress;
	if (hostAddress == previousHost &&
	    (device->flags & 0x21000) == 0x1000 &&
	    device->deviceClass == classValue && device->vendorId == vendorId &&
	    device->productId == product && device->version == (uint16_t)deviceVersion) {
		result = strncmp(device->name, name, 120);
		if (!result)
			goto unlock;
	}
	if (device->flags & 0x1000) {
		device->flags &= ~0x1000;
		if (device->next)
			device->next->prev = device->prev;
		else
			gBtStack.deviceTail = device->prev;
		if (device->prev)
			device->prev->next = device->next;
		else
			gBtStack.deviceHead = device->next;
	}
	BtPack(hostAddressOut, 6, gBtDeviceAddressHciFormat, hostAddress);
	BtGenerateRandomBlock(linkKeyOut);
	device->version = deviceVersion;
	device->deviceClass = classValue;
	memcpy(device->linkKey, linkKeyOut, sizeof(device->linkKey));
	device->vendorId = vendorId;
	device->productId = product;
	device->profiles = 0x40;
	device->eirIdentity.fields.vendorIdSource = 2;
	strncpy(device->name, name, 120);
	device->flags = 0x2000;
	device->generation++;
	BtRegisterDevice(&gBtStack, address);
	result = 1;
unlock:
	return BtUnlockTransport(result);
}

int sceBtHidGetReportDescriptor(uint64_t deviceAddress, void *buffer,
			      unsigned int capacity)
{
	(void)deviceAddress;
	(void)buffer;
	(void)capacity;
	return (int)0x802f090a;
}

/* The original user export rejects every request without inspecting it. */
int sceBtHidTransfer(uint64_t deviceAddress, struct _SceBtHidRequest *request)
{
	(void)deviceAddress;
	(void)request;
	return (int)0x802f090a;
}

/* The original consumes no arguments; its public argument contract is unknown. */
int sceBtPairingOOB(void)
{
	return (int)0x802f090a;
}

static inline int BtStartAudioApi(uint64_t deviceAddress, int profile)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	return BtUnlockTransport(BtStartAudio(&gBtStack, deviceAddress, profile));
}

int SceBtForDriver_8D47CABD(uint64_t deviceAddress, int profile)
{
	return BtStartAudioApi(deviceAddress, profile);
}

int sceBtStartAudio(uint64_t deviceAddress, int profile)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtStartAudioApi(deviceAddress, profile);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtStopAudioApi(uint64_t deviceAddress, int profile)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	return BtUnlockTransport(BtStopAudio(&gBtStack, deviceAddress, profile));
}

int SceBtForDriver_CAE5A9F3(uint64_t deviceAddress, int profile)
{
	return BtStopAudioApi(deviceAddress, profile);
}

int sceBtStopAudio(uint64_t deviceAddress, int profile)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtStopAudioApi(deviceAddress, profile);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtSendAudioApi(uint64_t deviceAddress, void *buffer,
				 unsigned int size, int userMode)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	return BtUnlockTransport(BtSendAudio(&gBtStack, deviceAddress,
					   buffer, size, userMode));
}

int SceBtForDriver_47F19727(uint64_t deviceAddress, void *buffer, SceSize size)
{
	return BtSendAudioApi(deviceAddress, buffer, size, 0);
}

int sceBtSendAudio(uint64_t deviceAddress, void *buffer, SceSize size)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtSendAudioApi(deviceAddress, buffer, size, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtRecvAudioApi(uint64_t deviceAddress, void *buffer,
				 unsigned int size, int userMode)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	return BtUnlockTransport(BtRecvAudio(&gBtStack, deviceAddress,
					   buffer, size, userMode));
}

int SceBtForDriver_CD7C0492(uint64_t deviceAddress, void *buffer, SceSize size)
{
	return BtRecvAudioApi(deviceAddress, buffer, size, 0);
}

int sceBtRecvAudio(uint64_t deviceAddress, void *buffer, SceSize size)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtRecvAudioApi(deviceAddress, buffer, size, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtAudioFrequencyApi(uint64_t deviceAddress)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	result = connection ? connection->avdtp.receiveSampleRate :
		(int)0x802f0d0f;
	return BtUnlockTransport(result);
}

int SceBtForDriver_DA20DCC8(uint64_t deviceAddress)
{
	return BtAudioFrequencyApi(deviceAddress);
}

int sceBtFreqAudio(uint64_t deviceAddress)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtAudioFrequencyApi(deviceAddress);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_08632086(int protection)
{
	if ((unsigned int)protection - 1 > 1)
		gBtStack.contentProtection = 0;
	else
		gBtStack.contentProtection = protection;
	return 0;
}

int sceBtSetContentProtection(int protection)
{
	if ((unsigned int)protection - 1 > 1)
		gBtStack.contentProtection = 0;
	else
		gBtStack.contentProtection = protection;
	return 0;
}

static inline int BtPushImageApi(uint64_t deviceAddress, const void *path,
				unsigned int length, int userMode)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	if (length >= sizeof(gBtStack.scratch))
		return (int)0x802f0f10;
	result = BtLockTransport();
	if (result)
		return result;
	result = BtCopyIn(path, gBtStack.scratch, length, userMode);
	if (result >= 0) {
		gBtStack.scratch[length] = 0;
		connection = BtFindConnection(&gBtStack, 1, deviceAddress);
		result = connection ? BtPushImage(connection,
						(const char *)gBtStack.scratch) :
			(int)0x802f0f01;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_7FF77792(uint64_t deviceAddress, const void *path, unsigned int length)
{
	return BtPushImageApi(deviceAddress, path, length, 0);
}

int sceBtPushBip(uint64_t deviceAddress, const void *path, unsigned int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtPushImageApi(deviceAddress, path, length, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtReceiveImageApi(const void *path, unsigned int length,
				   int userMode)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	if (length >= sizeof(gBtStack.scratch))
		return (int)0x802f0f10;
	result = BtLockTransport();
	if (result)
		return result;
	result = BtCopyIn(path, gBtStack.scratch, length, userMode);
	if (result >= 0) {
		gBtStack.scratch[length] = 0;
		result = BtSetImageReceivePath((const char *)gBtStack.scratch);
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_E8E95977(const void *path, unsigned int length)
{
	return BtReceiveImageApi(path, length, 0);
}

int sceBtRecvBip(const void *path, unsigned int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtReceiveImageApi(path, length, 1);

	sceSyscallLeave(saved);
	return result;
}

/* These originals consume no arguments and reject the unavailable operation. */
int SceBtForDriver_21C788E2(void)
{
	return (int)0x802f1008;
}

int sceBtPushOpp(void)
{
	SceUInt32 saved = sceSyscallEnter();

	sceSyscallLeave(saved);
	return (int)0x802f1008;
}

int SceBtForDriver_8008AA2C(void)
{
	return (int)0x802f1008;
}

int sceBtRecvOpp(void)
{
	SceUInt32 saved = sceSyscallEnter();

	sceSyscallLeave(saved);
	return (int)0x802f1008;
}

static inline int BtSendSerialApi(uint64_t deviceAddress, const void *data,
				unsigned int length, int userMode)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	if (length > sizeof(gBtStack.scratch))
		return (int)0x802f1603;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (connection) {
		result = BtCopyIn(data, gBtStack.scratch, length, userMode);
		if (result >= 0)
			result = BtSendSerialData(connection, gBtStack.scratch, length);
	} else {
		result = (int)0x802f1602;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_92241DFB(uint64_t deviceAddress, const void *data, unsigned int length)
{
	return BtSendSerialApi(deviceAddress, data, length, 0);
}

int sceBtSendSpp(uint64_t deviceAddress, const void *data, unsigned int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtSendSerialApi(deviceAddress, data, length, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtReceiveSerialApi(uint64_t deviceAddress, void *data,
				   int length, int userMode)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (!connection)
		result = (int)0x802f1602;
	else if (connection->serialReceive.data &&
		 connection->serialReceive.capacity > 0)
		result = BtRingRead(&connection->serialReceive, data, length,
				    userMode);
	else
		result = 0;
	return BtUnlockTransport(result);
}

int SceBtForDriver_C09D9E7C(uint64_t deviceAddress, void *data, int length)
{
	return BtReceiveSerialApi(deviceAddress, data, length, 0);
}

int sceBtRecvSpp(uint64_t deviceAddress, void *data, int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtReceiveSerialApi(deviceAddress, data, length, 1);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_6E49D11D(void)
{
	return (int)0x802f1301;
}

int sceBtHfpRequest(void)
{
	SceUInt32 saved = sceSyscallEnter();

	sceSyscallLeave(saved);
	return (int)0x802f1301;
}

int SceBtForDriver_7BC66D6D(void)
{
	return (int)0x802f1301;
}

int sceBtHfpGetCurrentPhoneNumber(void)
{
	SceUInt32 saved = sceSyscallEnter();

	sceSyscallLeave(saved);
	return (int)0x802f1301;
}

static inline int BtGetLinkInfoApi(uint64_t deviceAddress, void *buffer,
				 int userMode)
{
	BtLinkInfoForTest info;
	BtConnection *connection;
	uint64_t now;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (!connection)
		return BtUnlockTransport((int)0x802f1401);
	info.l2capReceivePackets = connection->statistics.l2capReceivePackets;
	info.l2capReceiveBytes = connection->statistics.l2capReceiveBytes;
	info.l2capTransmitPackets = connection->statistics.l2capTransmitPackets;
	info.l2capTransmitBytes = connection->statistics.l2capTransmitBytes;
	info.aclTransmitPackets = connection->statistics.aclTransmitPackets;
	info.aclTransmitBytes = connection->statistics.aclTransmitBytes;
	info.aclReceivePackets = connection->statistics.aclReceivePackets;
	info.aclReceiveBytes = connection->statistics.aclReceiveBytes;
	info.interval = connection->interval;
	info.linkQuality = connection->linkQuality;
	info.rssi = connection->rssi;
	info.afhMode = connection->afhMode;
	memcpy(info.afhMap, connection->afhMap, sizeof(info.afhMap));
	info.linkState = (connection->linkMode & 3) |
		((connection->flags & 0x200) ? 4 : 0);
	result = BtCopyOut(&info, buffer, sizeof(info), userMode);
	if (result >= 0) {
		now = BtGetSystemTimeWide();
		if (!connection->statistics.queryTime ||
		    connection->statistics.queryTime + 1000000 < now) {
			connection->statistics.queryTime = now + 1000000;
			BtSendHciCommand(&gBtStack, 0x1403, "2", connection->aclHandle);
		}
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_2C9DD7BB(uint64_t deviceAddress, void *buffer)
{
	return BtGetLinkInfoApi(deviceAddress, buffer, 0);
}

int sceBtGetInfoForTest(uint64_t deviceAddress, void *buffer)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtGetLinkInfoApi(deviceAddress, buffer, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtSendEchoApi(uint64_t deviceAddress, const void *data,
			      unsigned int length, int userMode)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	if (length > sizeof(gBtStack.scratch)) {
		result = (int)0x802f1403;
	} else {
		connection = BtFindConnection(&gBtStack, 1, deviceAddress);
		if (connection) {
			result = BtCopyIn(data, gBtStack.scratch, length, userMode);
			if (result >= 0)
				result = BtSendEchoRequest(connection, gBtStack.scratch,
						   length);
		} else {
			result = (int)0x802f1402;
		}
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_E3A66814(uint64_t deviceAddress, const void *data,
				   unsigned int length)
{
	return BtSendEchoApi(deviceAddress, data, length, 0);
}

int sceBtSendL2capEchoRequestForTest(uint64_t deviceAddress, const void *data,
				  unsigned int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtSendEchoApi(deviceAddress, data, length, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtSetEchoBufferApi(uint64_t deviceAddress, uint8_t *buffer,
				    int capacity, SceUID processId)
{
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (connection) {
		connection->echoResponseBuffer = buffer;
		connection->echoResponseCapacity = capacity;
		connection->echoResponseProcessId = processId;
	} else {
		result = (int)0x802f1404;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_0B7D8E0E(uint64_t deviceAddress,
					 uint8_t *buffer, int capacity)
{
	return BtSetEchoBufferApi(deviceAddress, buffer, capacity, -1);
}

int sceBtSetL2capEchoResponseBufferForTest(uint64_t deviceAddress,
					uint8_t *buffer, int capacity)
{
	SceUInt32 saved = sceSyscallEnter();
	SceUID processId = sceKernelGetProcessId();
	int result = BtSetEchoBufferApi(deviceAddress, buffer, capacity, processId);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtSetTestInquiryApi(const void *resultBuffer, int userMode)
{
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	result = BtCopyIn(resultBuffer, &gBtTestInquiryResult,
			  sizeof(gBtTestInquiryResult), userMode);
	if (result >= 0)
		BtProcessInquiryResult(&gBtStack, gBtTestInquiryResult.deviceAddress,
			gBtTestInquiryResult.pageScanRepetitionMode,
			gBtTestInquiryResult.deviceClass,
			gBtTestInquiryResult.clockOffset, gBtTestInquiryResult.rssi,
			gBtTestInquiryResult.extendedInquiryResponse);
	return BtUnlockTransport(result);
}

int SceBtForDriver_1C26F878(const void *resultBuffer)
{
	return BtSetTestInquiryApi(resultBuffer, 0);
}

int sceBtSetInquiryResultForTest(const void *resultBuffer)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtSetTestInquiryApi(resultBuffer, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtRegisterHidVuApi(SceUID callback, int vendorId, int productId,
				   SceUID processId)
{
	BtHidVuSubscriber *subscriber = &gBtStack.hidVuSubscriber;
	BtConnection *connection;
	SceUID threadId;
	int result;

	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	if (vendorId != 0xe6f || productId != 0x329)
		return (int)0x802f4001;
	threadId = sceKernelGetThreadId();
	result = BtLockTransport();
	if (result)
		return result;
	if (subscriber->threadId == threadId) {
		subscriber->callbackId = callback;
		subscriber->vendorId = 0xe6f;
		subscriber->productId = 0x329;
	} else if (subscriber->threadId) {
		result = (int)0x802f4002;
	} else {
		subscriber->threadId = threadId;
		subscriber->callbackId = callback;
		subscriber->vendorId = 0xe6f;
		subscriber->productId = 0x329;
		subscriber->capacity = 8;
		subscriber->writePosition = 0;
		subscriber->readPosition = 0;
		subscriber->count = 0;
		subscriber->processId = processId;
		for (connection = gBtStack.connections;
		     connection < gBtStack.connections + 7; connection++) {
			BtDevice *device;
			BtChannel *channel;

			if (!connection->deviceAddress)
				continue;
			device = connection->device;
			if (!device || device->vendorId != 0xe6f ||
			    device->productId != 0x329)
				continue;
			BtNotifyHidVuEvent(connection->deviceAddress, 1, 0, NULL, 0);
			channel = BtFindChannel(connection, 4, 0x13);
			if (channel && (channel->flags & 0x1000))
				BtNotifyHidVuEvent(connection->deviceAddress, 3, 0, NULL, 0);
		}
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_AB57F8C1(SceUID callback, int vendorId, int productId)
{
	return BtRegisterHidVuApi(callback, vendorId, productId, -1);
}

int sceBtHidVuRegisterCallback(SceUID callback, int vendorId, int productId)
{
	SceUID processId = sceKernelGetProcessId();
	SceUID kernelCallback;
	SceUInt32 saved;
	int result;

	if (processId < 0)
		return processId;
	kernelCallback = SceSysmemForDriver_45D22597(processId, callback);
	if (kernelCallback < 0)
		return kernelCallback;
	saved = sceSyscallEnter();
	result = BtRegisterHidVuApi(kernelCallback, vendorId, productId, processId);
	sceSyscallLeave(saved);
	return result;
}

static inline int BtUnregisterHidVuApi(void)
{
	SceUID threadId = sceKernelGetThreadId();
	int result = BtLockTransport();

	if (result)
		return result;
	if (gBtStack.hidVuSubscriber.threadId == threadId) {
		gBtStack.hidVuSubscriber.processId = 0;
		gBtStack.hidVuSubscriber.threadId = 0;
	} else {
		result = (int)0x802f4003;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_1AB31C7D(void)
{
	return BtUnregisterHidVuApi();
}

int sceBtHidVuUnregisterCallback(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtUnregisterHidVuApi();

	sceSyscallLeave(saved);
	return result;
}

static inline int BtReadHidVuEventsApi(void *buffer, int maxCount, int userMode)
{
	BtHidVuSubscriber *subscriber = &gBtStack.hidVuSubscriber;
	BtHidVuEvent *output = buffer;
	SceUID threadId = sceKernelGetThreadId();
	int result = BtLockTransport();

	if (result)
		return result;
	if (subscriber->threadId != threadId)
		return BtUnlockTransport((int)0x802f4003);
	while (subscriber->count > 0 && result != maxCount) {
		int copied = BtCopyOut(&subscriber->events[subscriber->readPosition],
				       output++, sizeof(*output), userMode);
		int position;

		if (copied < 0) {
			result = copied;
			break;
		}
		result++;
		position = subscriber->readPosition + 1;
		subscriber->count--;
		if (position >= subscriber->capacity)
			position = 0;
		subscriber->readPosition = position;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_3333A878(void *buffer, int maxCount)
{
	return BtReadHidVuEventsApi(buffer, maxCount, 0);
}

int sceBtHidVuReadEvent(void *buffer, int maxCount)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtReadHidVuEventsApi(buffer, maxCount, 1);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_3BBC1EE1(uint64_t deviceAddress, uint8_t reportId)
{
	return BtRequestHidVuReport(&gBtStack, deviceAddress, 'I', reportId,
				  NULL, 0, 0);
}

int sceBtHidVuGetReportInput(uint64_t deviceAddress, uint8_t reportId)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtRequestHidVuReport(&gBtStack, deviceAddress, 'I', reportId,
					NULL, 0, 1);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_CA9B885C(uint64_t deviceAddress, uint8_t reportId)
{
	return BtRequestHidVuReport(&gBtStack, deviceAddress, 'K', reportId,
				  NULL, 0, 0);
}

int sceBtHidVuGetReportFeature(uint64_t deviceAddress, uint8_t reportId)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtRequestHidVuReport(&gBtStack, deviceAddress, 'K', reportId,
					NULL, 0, 1);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_AB2032A3(uint64_t deviceAddress, const void *data, int length)
{
	return BtRequestHidVuReport(&gBtStack, deviceAddress, 'R', 0,
				  data, length, 0);
}

int sceBtHidVuSetReportOutput(uint64_t deviceAddress, const void *data, int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtRequestHidVuReport(&gBtStack, deviceAddress, 'R', 0,
					data, length, 1);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_014A1D28(uint64_t deviceAddress, const void *data, int length)
{
	return BtRequestHidVuReport(&gBtStack, deviceAddress, 'S', 0,
				  data, length, 0);
}

int sceBtHidVuSetReportFeature(uint64_t deviceAddress, const void *data, int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtRequestHidVuReport(&gBtStack, deviceAddress, 'S', 0,
					data, length, 1);

	sceSyscallLeave(saved);
	return result;
}

int SceBtForDriver_D2449052(uint64_t deviceAddress, const void *data, int length)
{
	return BtRequestHidVuReport(&gBtStack, deviceAddress, 0xa2, 0,
				  data, length, 0);
}

int sceBtHidVuSetInterruptOutput(uint64_t deviceAddress, const void *data, int length)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtRequestHidVuReport(&gBtStack, deviceAddress, 0xa2, 0,
					data, length, 1);

	sceSyscallLeave(saved);
	return result;
}

static inline int BtGetTestStatusApi(int selector, void *buffer, int size,
				     int userMode)
{
	uint64_t deviceAddress;
	unsigned int streamingEnabled;
	BtAudioQueueStatistics queueStatistics;
	unsigned char audioStatistics[80];
	BtConnection *connection;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result)
		return result;
	switch (selector) {
	case 0:
		if (size != 8)
			goto invalid;
		result = BtCopyOut(&gBtStack.deviceAddress, buffer, size, userMode);
		break;
	case 1:
		streamingEnabled = !!(gBtStack.mode & 0x800);
		if (size != 4)
			goto invalid;
		result = BtCopyOut(&streamingEnabled, buffer, size, userMode);
		break;
	case 2:
		if (size != sizeof(queueStatistics))
			goto invalid;
		queueStatistics.underruns = gBtAudioUnderruns;
		queueStatistics.overruns = gBtAudioOverruns;
		queueStatistics.pendingPackets = gBtPendingAudioPackets;
		result = BtCopyOut(&queueStatistics, buffer, size, userMode);
		break;
	case 3:
		if (size != 4)
			goto invalid;
		result = BtCopyOut(&gBtStack.scoPacketLength, buffer, size, userMode);
		break;
	case 6:
		if (!(gBtStack.mode & 0x1000)) {
			result = (int)0x802f1704;
			break;
		}
		if (size != 8)
			goto invalid;
		result = BtCopyIn(buffer, &deviceAddress, size, userMode);
		if (result)
			break;
		if (deviceAddress) {
			connection = BtFindConnection(&gBtStack, 1, deviceAddress);
			if (!connection) {
				result = (int)0x802f1703;
				break;
			}
			if (connection->flags & 0x400000)
				goto unsupported;
			result = BtSendHciCommand(&gBtStack, 0xfc2c, "2",
						 connection->aclHandle);
		} else {
			result = BtSendHciCommand(&gBtStack, 0xfc2c, "2", 0);
		}
		break;
	case 9:
		if (size != sizeof(audioStatistics) || !buffer)
			goto invalid;
		result = BtGetAudioStatistics(audioStatistics);
		if (!result)
			result = BtCopyOut(audioStatistics, buffer,
					   sizeof(audioStatistics), userMode);
		break;
	default:
unsupported:
		result = (int)0x802f1702;
		break;
invalid:
		result = (int)0x802f1701;
		break;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_83781649(int selector, void *buffer, int size)
{
	if (selector == 7)
		return gBtStack.state;
	return BtGetTestStatusApi(selector, buffer, size, 0);
}

int sceBtGetStatusForTest(int selector, void *buffer, int size)
{
	SceUInt32 saved;
	int result;

	if (selector == 7)
		return gBtStack.state;
	saved = sceSyscallEnter();
	result = BtGetTestStatusApi(selector, buffer, size, 1);
	sceSyscallLeave(saved);
	return result;
}

static inline int BtSetTestStatusApi(int selector, void *buffer, int size,
				     int userMode)
{
	union {
		uint64_t address;
		uint32_t word;
		uint8_t byte;
	} value;
	BtInquiryScanActivity scan;
	BtVendorConnectionSetting *setting = buffer;
	BtConnection *connection;
	uint8_t vendorSetting;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	result = BtLockTransport();
	if (result)
		return result;
	switch (selector) {
	case 1:
		if (size != 4)
			goto invalid;
		result = BtCopyIn(buffer, &value.word, size, userMode);
		if (!result) {
			unsigned int mode = gBtStack.mode & ~0x800;

			gBtStack.mode = mode;
			if (value.word)
				gBtStack.mode = mode | 0x800;
		}
		break;
	case 6:
		if (!(gBtStack.mode & 0x1000))
			goto disabled;
		result = BtSetHidCaptureBuffer(userMode ? sceKernelGetProcessId() : -1,
					      buffer, size);
		break;
	case 7:
		if (!(gBtStack.mode & 0x1000))
			goto disabled;
		result = BtInjectHidPacket(&gBtStack, buffer, size, userMode);
		break;
	case 15:
		if (!(gBtStack.mode & 0x1000))
			goto disabled;
		if (size != 1 || !buffer)
			goto unsupported;
		result = BtCopyIn(buffer, &value.byte, 1, userMode);
		if (!result) {
			gBtStack.scanEnable = value.byte;
			BtSendHciCommand(&gBtStack, 0x0c1a, "1", value.byte);
		}
		break;
	case 19:
		if (!(gBtStack.mode & 0x1000))
			goto disabled;
		if (size != sizeof(*setting))
			goto invalid;
		result = BtCopyIn(setting->deviceAddress, &value.address,
				  sizeof(value.address), userMode);
		if (result)
			break;
		result = BtCopyIn(&setting->value, &vendorSetting, 1, userMode);
		if (result)
			break;
		if (value.address) {
			gBtStack.vendorConnectionSetting = vendorSetting;
			connection = BtFindConnection(&gBtStack, 1, value.address);
			if (!connection) {
				result = (int)0x802f1703;
				break;
			}
			if (connection->flags & 0x400000)
				goto unsupported;
			result = BtSendHciCommand(&gBtStack, 0xfc2b, "21",
						 connection->aclHandle, vendorSetting);
		} else {
			result = BtSendHciCommand(&gBtStack, 0xfc2b, "21", 0,
						 vendorSetting);
		}
		break;
	case 22:
		if (size != 1)
			goto invalid;
		result = BtCopyIn(buffer, &value.byte, size, userMode);
		if (!result)
			gBtNetworkAccessPointEnabled = value.byte;
		break;
	case 23:
		if (size != 1)
			goto invalid;
		result = BtCopyIn(buffer, &value.byte, size, userMode);
		if (!result)
			gBtPanUserEnabled = value.byte;
		break;
	case 24:
		if (size != 4)
			goto invalid;
		result = BtCopyIn(buffer, &value.word, size, userMode);
		if (!result)
			result = BtAdjustAudioReceiveBuffer(value.word);
		break;
	case 28:
		if (size)
			goto invalid;
		if (!(gBtStack.mode & 0x1000))
			goto disabled;
		result = BtSendHciCommand(&gBtStack, 0x1803, gBtEmptyHciFormat);
		break;
	case 34:
		if (!(gBtStack.mode & 0x1000))
			goto disabled;
		if (size != sizeof(scan))
			goto invalid;
		result = BtCopyIn(buffer, &scan, size, userMode);
		if (!result) {
			if ((uint16_t)(scan.interval - 18) > 0xfee ||
			    (unsigned int)scan.window - 17 >= 0xff0 ||
			    scan.interval < scan.window)
				goto unsupported;
			result = BtSendHciCommand(&gBtStack, 0x0c1e, "22",
						 scan.interval, scan.window);
		}
		break;
	case 35:
		if (!(gBtStack.mode & 0x1000))
			goto disabled;
		if (size != 1)
			goto invalid;
		result = BtCopyIn(buffer, &gBtStack.suppressInquiryResult, size,
				  userMode);
		break;
	default:
unsupported:
		result = (int)0x802f1702;
		break;
invalid:
		result = (int)0x802f1701;
		break;
disabled:
		result = (int)0x802f1704;
		break;
	}
	return BtUnlockTransport(result);
}

int SceBtForDriver_9C195411(int selector, void *buffer, int size)
{
	return BtSetTestStatusApi(selector, buffer, size, 0);
}

int sceBtSetStatusForTest(int selector, void *buffer, int size)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtSetTestStatusApi(selector, buffer, size, 1);

	sceSyscallLeave(saved);
	return result;
}
