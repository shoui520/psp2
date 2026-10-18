/* Connection timers; original source filename unavailable. */
#include <sysmem/procevent.h>
#include <intrmgr/intrmgr.h>
#include <intrmgr/syscall.h>
#include <bt/bt_pairing.h>
#include "bt_internal.h"

extern BtStack gBtStack;
int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);
int BtSendL2capDisconnectRequest(BtConnection *connection, BtChannel *channel);
unsigned int BtCancelTimer(unsigned int timer);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
int BtInitializeHsp(BtConnection *connection, BtChannel *channel);
int BtInitializeAvdtp(BtConnection *connection, BtChannel *channel,
		      unsigned int profile);
int BtInitializeAvrcp(BtConnection *connection, BtChannel *channel);
int BtInitializeHidChannels(BtConnection *connection, BtChannel *channel);
int BtInitializeBnep(BtConnection *connection, BtChannel *channel,
		     unsigned int profile);
int BtInitializeImagingProfile(BtConnection *connection, BtChannel *channel,
			       unsigned int profile);
int BtInitializeImagingSession(BtConnection *connection, BtChannel *channel,
			       unsigned int profile);
int BtInitializeSerialPort(BtConnection *connection, BtChannel *channel);
int BtNotifyPendingProfilesConnected(BtConnection *connection, BtChannel *channel,
				     unsigned int profiles);
int BtNotifyBnepConnected(BtConnection *connection);
int BtInitializeSdpChannel(BtConnection *connection, BtChannel *channel);
uintptr_t BtReplyPendingL2capConnection(BtConnection *connection);
int BtRegisterDevice(BtStack *stack, uint64_t deviceAddress);
void BtNotifyConnectionComplete(BtConnection *connection, int status,
				uint32_t argument);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);
int BtLockTransport(void);
int BtUnlockTransport(int result);
BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			uint16_t identifier);
int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);

void BtDisconnectTimedOut(void *argument)
{
	BtConnection *connection = argument;
	unsigned int flags = connection->flags;

	connection->disconnectTimer = 0;
	if (!(flags & 0x2000)) {
		connection->flags = flags | 0x2000;
		BtSendHciCommand(&gBtStack, 0x406, "21",
				 connection->aclHandle, 19);
	}
}

int BtProcessResume(SceUID processId, int eventType,
		    ProcEventParam *param, int argument)
{
	(void)param;
	(void)argument;
	if (eventType == 0x10000 &&
	    gBtStack.inquiryCompletionProcessId == processId &&
	    gBtStack.state == 28 && BtLockTransport() == 0) {
		gBtStack.inquiryCompletionProcessId = -1;
		BtUnlockTransport(0);
	}
	return 0;
}

int BtProcessSuspend(SceUID processId, int eventType,
		     ProcEventParam *param, int argument)
{
	(void)param;
	(void)argument;
	if (eventType == 0x100 && gBtStack.inquiryProcessId == processId &&
	    BtLockTransport() == 0) {
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
		gBtStack.inquiryCompletionProcessId = gBtStack.inquiryProcessId;
		gBtStack.inquiryProcessId = -1;
		BtUnlockTransport(0);
	}
	return 0;
}

static inline int BtCleanupProcess(SceUID processId)
{
	BtEventSubscriber *subscriber;
	BtConnection *connection;

	if (BtLockTransport() < 0)
		return 0;
	subscriber = gBtStack.subscribers;
	do {
		if (subscriber->threadId && subscriber->processId == processId)
			subscriber->threadId = 0;
		subscriber++;
	} while (subscriber < gBtStack.subscribers + 8);
	if (gBtStack.hidVuSubscriber.threadId &&
	    gBtStack.hidVuSubscriber.processId == processId)
		gBtStack.hidVuSubscriber.threadId = 0;
	if (gBtStack.inquiryProcessId == processId) {
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
	}
	connection = gBtStack.connections;
	do {
		if (connection->echoResponseProcessId == processId) {
			connection->echoResponseProcessId = -1;
			connection->echoResponseBuffer = NULL;
			connection->echoResponseCapacity = 0;
		}
		connection++;
	} while (connection < gBtStack.connections + 7);
	BtUnlockTransport(0);
	return 0;
}

int BtProcessExit(SceUID processId, ProcEventParam *param, int argument)
{
	(void)param;
	(void)argument;
	return BtCleanupProcess(processId);
}

int BtProcessKill(SceUID processId, ProcEventParam *param, int argument)
{
	(void)param;
	(void)argument;
	return BtCleanupProcess(processId);
}

int BtDisconnectConnection(BtStack *stack, BtConnection *connection)
{
	unsigned int flags = connection->flags;
	BtChannel *channel = &connection->channels[15];

	if (!(flags & 0x40))
		return 0;
	for (;;) {
		if (channel->psm && (channel->flags & 0xc)) {
			if (channel->ops && channel->ops->requestDisconnect &&
			    channel->ops->requestDisconnect(connection, channel))
				return 0;
			return BtSendL2capDisconnectRequest(connection, channel);
		}
		if (channel == connection->channels)
			break;
		channel--;
	}
	if (flags & 0x2000)
		return 0;
	connection->flags = flags | 0x2000;
	return BtSendHciCommand(stack, 0x406, "21", connection->aclHandle, 19);
}

void BtStartProfiles(void *argument)
{
	BtConnection *connection = argument;
	unsigned int flags = connection->flags;
	unsigned int profiles, connected;

	connection->profileTimer = 0;
	connection->flags = flags | 0x400;
	profiles = connection->device->profiles;
	connected = connection->connectedProfiles;
	if (flags & 0x40) {
		BtDisconnectConnection(&gBtStack, connection);
		return;
	}
	connection->profileTimer = BtCancelTimer(0);
	if (!(connection->flags & 0x401)) {
		if (!connection->profileTimer)
			BtQueueTimer(&connection->profileTimer, BtStartProfiles,
				     connection, 3000000);
		return;
	}
	profiles &= ~connection->excludedProfiles;
	if ((profiles & 0x10000) && !(connected & 0x10000))
		BtInitializeHsp(connection, NULL);
	else if ((profiles & 4) && !(connected & 4))
		BtInitializeAvdtp(connection, NULL, 4);
	else if ((profiles & 8) && !(connected & 8))
		BtInitializeAvdtp(connection, NULL, 8);
	else if ((profiles & 0x10) && !(connected & 0x10))
		BtInitializeAvrcp(connection, NULL);
	else if ((profiles & 0x40) && !(connected & 0x40))
		BtInitializeHidChannels(connection, NULL);
	else if ((profiles & 0x200) && !(connected & 0x200))
		BtInitializeBnep(connection, NULL, 0x200);
	else if ((profiles & 0x1000) && !(connected & 0x1000))
		BtInitializeImagingProfile(connection, NULL, 0x1000);
	else if ((profiles & 0x2000) && !(connected & 0x2000))
		BtInitializeImagingSession(connection, NULL, 0x2000);
	else if ((profiles & 0x80000) && !(connected & 0x80000))
		BtInitializeSerialPort(connection, NULL);
}

int BtBeginDisconnect(BtConnection *connection)
{
	unsigned int flags;
	int result;

	if (!connection->aclHandle) {
		flags = connection->flags;
		result = flags & 1;
		if (!result) {
			connection->flags = flags | 0x200000;
			return result;
		}
		return BtSendHciCommand(&gBtStack, 0x408, "6",
					connection->deviceAddress);
	}
	flags = connection->flags;
	if (flags & 0x40)
		return 0;
	connection->flags = flags | 0x40;
	BtQueueTimer(&connection->disconnectTimer, BtDisconnectTimedOut,
		     connection, 3000000);
	if (connection->linkMode == 2) {
		BtSendHciCommand(&gBtStack, 0x804, "2", connection->aclHandle);
		connection->interval = 0;
	}
	return BtDisconnectConnection(&gBtStack, connection);
}

static inline int BtStartDisconnect(uint64_t deviceAddress)
{
	BtConnection *connection;
	BtChannel *channel;
	unsigned int flags;
	int result;

	if (SceIntrmgrForDriver_182EE3E3())
		return (int)0x802f1901;
	if ((gBtStack.mode & 0x38) != 8)
		return (int)0x802f1503;
	result = BtLockTransport();
	if (result)
		return result;
	connection = BtFindConnection(&gBtStack, 1, deviceAddress);
	if (!connection) {
		result = (int)0x802f0205;
		goto unlock;
	}
	flags = connection->flags;
	if ((flags & 0x1000) && connection->aclHandle &&
	    (connection->device->flags & 2) && !(flags & 0x800)) {
		channel = BtFindChannel(connection, 36, 17);
		if (channel) {
			connection->flags |= 0x800;
			BtSendL2capPacket(connection, channel->remoteCid, "1", 21);
			goto unlock;
		}
	}
	result = BtBeginDisconnect(connection);
unlock:
	return BtUnlockTransport(result);
}

int ksceBtStartDisconnect(unsigned int mac0, unsigned int mac1)
{
	return BtStartDisconnect((uint64_t)mac1 << 32 | mac0);
}

int sceBtStartDisconnect(unsigned int mac0, unsigned int mac1)
{
	SceUInt32 saved = sceSyscallEnter();
	int result = BtStartDisconnect((uint64_t)mac1 << 32 | mac0);

	sceSyscallLeave(saved);
	return result;
}

void BtNotifyConnectionComplete(BtConnection *connection, int status,
				uint32_t argument)
{
	unsigned int flags = connection->flags;
	unsigned int connected, profiles;

	if (flags & 0x100)
		return;
	connection->flags = flags | 0x100;
	if (!status) {
		connected = connection->connectedProfiles;
		profiles = connection->device->profiles;
		if (flags & 0x40) {
			BtDisconnectConnection(&gBtStack, connection);
		} else {
			connection->profileTimer =
				BtCancelTimer(connection->profileTimer);
			if (connection->flags & 0x401) {
				profiles &= ~connection->excludedProfiles;
				if ((profiles & 0x10000) && !(connected & 0x10000))
					BtInitializeHsp(connection, NULL);
				else if ((profiles & 4) && !(connected & 4))
					BtInitializeAvdtp(connection, NULL, 4);
				else if ((profiles & 8) && !(connected & 8))
					BtInitializeAvdtp(connection, NULL, 8);
				else if ((profiles & 0x10) && !(connected & 0x10))
					BtInitializeAvrcp(connection, NULL);
				else if ((profiles & 0x40) && !(connected & 0x40))
					BtInitializeHidChannels(connection, NULL);
				else if ((profiles & 0x200) && !(connected & 0x200))
					BtInitializeBnep(connection, NULL, 0x200);
				else if ((profiles & 0x1000) && !(connected & 0x1000))
					BtInitializeImagingProfile(connection, NULL, 0x1000);
				else if ((profiles & 0x2000) && !(connected & 0x2000))
					BtInitializeImagingSession(connection, NULL, 0x2000);
				else if ((profiles & 0x80000) && !(connected & 0x80000))
					BtInitializeSerialPort(connection, NULL);
			} else if (!connection->profileTimer) {
				BtQueueTimer(&connection->profileTimer,
					     BtStartProfiles, connection, 3000000);
			}
		}
	}
	BtNotifyEvent(5, status, argument, connection->deviceAddress);
}

uintptr_t BtSetConnectedProfiles(BtConnection *connection, BtChannel *channel,
				unsigned int profiles)
{
	unsigned int connected;
	unsigned int available;
	unsigned int deviceProfiles;
	uintptr_t result;

	if (profiles & 0x30003)
		profiles |= 0x20000000;
	connected = connection->connectedProfiles | profiles;
	connection->connectedProfiles = connected;
	if (connection->pendingStartProfiles & 0x2000000c & profiles) {
		result = BtNotifyPendingProfilesConnected(connection, channel, profiles);
		if (result)
			return result;
		connected = connection->connectedProfiles;
	}
	deviceProfiles = connection->device->profiles;
	if (connection->flags & 0x40) {
		result = BtDisconnectConnection(&gBtStack, connection);
	} else {
		result = BtCancelTimer(connection->profileTimer);
		connection->profileTimer = result;
		if (connection->flags & 0x401) {
			available = deviceProfiles & ~connection->excludedProfiles;
			result = available << 18;
			if ((available & 0x10000) && !(connected & 0x10000))
				result = BtInitializeHsp(connection, NULL);
			else if ((available & 4) && !(connected & 4))
				result = BtInitializeAvdtp(connection, NULL, 4);
			else if ((available & 8) && !(connected & 8))
				result = BtInitializeAvdtp(connection, NULL, 8);
			else if ((available & 0x10) && !(connected & 0x10))
				result = BtInitializeAvrcp(connection, NULL);
			else if ((available & 0x40) && !(connected & 0x40))
				result = BtInitializeHidChannels(connection, NULL);
			else if ((available & 0x200) && !(connected & 0x200))
				result = BtInitializeBnep(connection, NULL, 0x200);
			else if ((available & 0x1000) && !(connected & 0x1000))
				result = BtInitializeImagingProfile(connection, NULL, 0x1000);
			else if ((available & 0x2000) && !(connected & 0x2000))
				result = BtInitializeImagingSession(connection, NULL, 0x2000);
			else if ((available & 0x80000) && !(connected & 0x80000))
				result = BtInitializeSerialPort(connection, NULL);
		} else if (!result) {
			result = BtQueueTimer(&connection->profileTimer,
					      BtStartProfiles, connection, 3000000);
		}
	}
	if (profiles & 0x300)
		return BtNotifyBnepConnected(connection);
	return result;
}

uintptr_t BtClearConnectedProfiles(BtConnection *connection,
				   BtChannel *channel, unsigned int profiles)
{
	uintptr_t result = (uintptr_t)&connection->hsp;

	(void)channel;
	connection->connectedProfiles &= ~profiles;
	return result;
}

int BtAdvanceConnection(BtConnection *connection)
{
	BtStack *stack = connection->stack;
	BtConnection *other;
	BtDevice *device;
	BtChannel *channel;
	uint32_t flags = connection->flags;
	uint32_t identity, profiles;
	int index;

	if (!(flags & 0x10) && !connection->device->profiles) {
		BtInitializeSdpChannel(connection, NULL);
		return 0;
	}
	if (!(flags & 0x200)) {
		device = connection->device;
		if (!device->name[0]) {
			stack->connectingAddress = connection->deviceAddress;
			stack->state = 50;
			BtSendHciCommand(stack, 0x419, "61-2", connection->deviceAddress, 2, 0);
			return 0;
		}
		if (device->profiles == 0x40 && !(device->flags & 0x40) &&
		    !(connection->remoteFeatures & UINT64_C(0x8000000000000)))
			goto ready;
		if (!(flags & 2)) {
			BtSendHciCommand(stack, 0x411, "2", connection->aclHandle);
			return 0;
		}
		if (!(flags & 4)) {
			BtSendHciCommand(stack, 0x413, "21", connection->aclHandle, 1);
			return 0;
		}
ready:
		BtReplyPendingL2capConnection(connection);
		flags = connection->flags;
		if (flags & 1) {
			channel = BtFindChannel(connection, 0x24, 1);
			if (channel && (channel->flags & 1))
				return 0;
			flags = connection->flags;
		}
		if (!(flags & 0x100)) {
			device = connection->device;
			identity = device->vendorProductId;
			if ((stack->mode & 0x10000) && identity == 0x0306054c) {
				if (stack->exclusiveControllerCount) {
					for (index = 0; index < 7; index++) {
						other = &stack->connections[index];
						if (other != connection && other->deviceAddress &&
						    !(other->flags & 0x40) &&
						    other->device->vendorProductId == 0x0306054c)
							BtBeginDisconnect(other);
					}
				}
				stack->exclusiveControllerCount++;
				connection->flags |= 0x10000;
			} else if ((stack->mode & 0x10000) &&
				   (identity == 0x0268054c || identity == 0x05c4054c)) {
				if (stack->limitedConnectionCount > 3)
					goto tooManyConnections;
				stack->limitedConnectionCount++;
				connection->flags |= 0x20000;
			} else {
				profiles = device->profiles;
				if (profiles & 0x10018) {
					if (stack->limitedConnectionCount > 3)
						goto tooManyConnections;
					if (stack->audioConnectionCount) {
						for (index = 0; index < 7; index++) {
							other = &stack->connections[index];
							if (other != connection && other->deviceAddress &&
							    (other->device->profiles & 0x10018))
								BtBeginDisconnect(other);
						}
					}
					stack->limitedConnectionCount++;
					stack->audioConnectionCount++;
					connection->flags |= 0x60000;
				} else {
					if (profiles != 0x40) {
						BtBeginDisconnect(connection);
						BtNotifyConnectionComplete(connection, 0, 0);
						return 0;
					}
					if (stack->limitedConnectionCount > 3)
						goto tooManyConnections;
					if (stack->genericHidConnectionCount > 1) {
						BtBeginDisconnect(connection);
						BtNotifyConnectionComplete(connection, 0, 0x802f3402);
						return 0;
					}
					stack->limitedConnectionCount++;
					stack->genericHidConnectionCount++;
					connection->flags |= 0xa0000;
				}
			}
		}
		BtRegisterDevice(stack, connection->deviceAddress);
		BtNotifyConnectionComplete(connection, 0, connection->device->profiles);
		return 1;
	}
	if (!(flags & 0x100000)) {
		identity = connection->device->vendorProductId;
		if (identity == 0x0306054c || identity == 0x0268054c ||
		    identity == 0x03d5054c || identity == 0x042f054c ||
		    identity == 0x05c4054c || identity == 0x03290e6f) {
			connection->flags = flags | 0x100000;
			BtSendHciCommand(stack, 0x80b, "61", connection->deviceAddress, 0);
		} else {
			BtBeginDisconnect(connection);
		}
	}
	return 0;

tooManyConnections:
	BtBeginDisconnect(connection);
	BtNotifyConnectionComplete(connection, 0, 0x802f3401);
	return 0;
}
