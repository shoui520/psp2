/* HID transfer queues; original source filename unavailable. */
#include "bt_internal.h"
#include <psp2kern/kernel/threadmgr/thread.h>
#include <string.h>

BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t identifier);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtLockTransport(void);
int BtUnlockTransport(int result);
int BtSendL2capPacket(BtConnection *connection, int channel,
		     const char *format, ...);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
void BtHidVuRequestTimedOut(void *argument);
void BtHidRequestTimedOut(void *argument);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);
void BtPollHidAuthenticationResponse(void *argument);
int BtQueueHidTransfer(BtStack *stack, uint64_t deviceAddress,
		       SceBtHidRequest *request);
int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);
uintptr_t BtSetConnectedProfiles(BtConnection *connection, BtChannel *channel,
				 unsigned int profiles);
uintptr_t BtNotifyHidVuEvent(uint64_t deviceAddress, uint8_t eventId,
			   uint8_t status, const void *data, uint16_t length);
void BtHidKeepaliveTimedOut(void *argument);
void BtPollHidStatus(void *argument);
uintptr_t BtCompleteHidRequest(BtChannel *channel, BtHidQueue *queue,
			      SceBtHidRequest *request, int status);
int BtCancelTimer(unsigned int timer);
int BtBeginDisconnect(BtConnection *connection);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);

extern int gBtHidCaptureCapacity;
extern uint8_t *gBtHidCaptureBuffer;
extern SceUID gBtHidCaptureProcessId;

int BtStartHidRequest(BtChannel *channel, BtHidQueue *queue)
{
	BtConnection *connection = channel->connection;
	SceBtHidRequest *request = queue->sendHead;
	BtStack *stack = connection->stack;
	int headerLength, length, sendLength, result;

	if (!request) {
		queue->active = 0;
		return 0;
	}
	if (request->type == 2)
		headerLength = request->reportId ? 4 : 3;
	else if (request->type == 3)
		headerLength = request->reportId ? 2 : 1;
	else
		headerLength = 1;
	length = request->length;
	queue->active = 1;
	sendLength = channel->mtu - headerLength;
	if (sendLength >= length)
		sendLength = length;
	queue->sendPosition = sendLength;
	queue->lastPacketLength = headerLength + sendLength;
	if (request->type == 1) {
		if (connection->device->vendorProductId == 0x05c4054c) {
			const uint8_t *buffer = request->buffer;
			uint8_t *scratch = stack->scratch;
			uint32_t checksum;

			BtPack(scratch, 4096, "1111 11 #", 0xa2, 17, 0xc0, 0,
			       buffer[1], buffer[2] | 8, buffer + 3, 69);
			checksum = BtUpdateCrc32(0xffffffff, scratch, 75);
			result = BtSendL2capPacket(connection, channel->remoteCid,
					   "C # 4", 0x10000000, 1, scratch,
					   75, ~checksum);
		} else {
			result = BtSendL2capPacket(connection, channel->remoteCid,
					   "C1#", 0x10000000, 1, 0xa2,
					   request->buffer, sendLength);
		}
	} else if (request->type == 2) {
		result = BtSendL2capPacket(connection, channel->remoteCid,
				   "C1#2", 0x40, 1, 'K', &request->reportId,
				   request->reportId != 0, length);
	} else {
		result = BtSendL2capPacket(connection, channel->remoteCid,
				   "C1##", 0x40, 1, 'S', &request->reportId,
				   request->reportId != 0, request->buffer,
				   sendLength);
	}
	if (request->type != 1)
		BtQueueTimer(&connection->hid.requestTimer, BtHidRequestTimedOut,
			     channel, 1000000);
	return result;
}

void BtBeginHidChallenge(void *argument)
{
	BtConnection *owner = argument;
	BtStack *stack = owner->stack;
	SceBtHidRequest *request = &owner->hid.featureRequest;
	uint8_t sequence;

	BtGenerateRandomBlock(owner->hid.challenges[0]);
	BtGenerateRandomBlock(owner->hid.challenges[1]);
	request->type = 3;
	request->reportId = 3;
	sequence = owner->hid.sequence + 1;
	if (sequence <= 1)
		sequence = 2;
	owner->hid.sequence = sequence;
	owner->hid.challengeReport[0] = sequence;
	owner->hid.challengeReport[1] = 0;
	memcpy(owner->hid.challengeReport + 2, owner->hid.challenges,
	       sizeof(owner->hid.challenges));
	request->buffer = owner->hid.challengeReport;
	request->length = 34;
	BtPack(owner->hid.challengeReport + 34, 4, "4",
	       ~BtUpdateCrc32(0xb6b9b22d, owner->hid.challengeReport, 34));
	request->internalFlags[1] = 255;
	request->internalFlags[0] = 255;
	request->length += 4;
	request->argument = 1;
	BtQueueHidTransfer(stack, owner->deviceAddress, request);
	BtQueueTimer(&owner->hid.featureTimer, BtPollHidAuthenticationResponse,
		     owner, 2000000);
}

void BtHidRequestTimedOut(void *argument)
{
	BtChannel *channel = argument;
	BtConnection *connection = channel->connection;
	BtHidQueue *queue = &connection->hid.control;
	SceBtHidRequest *request = queue->sendHead;
	SceBtHidRequest *next;
	uint64_t deviceAddress;

	connection->hid.requestTimer = 0;
	if (!request)
		return;
	deviceAddress = connection->deviceAddress;
	next = request->next;
	if (request->type == 0) {
		queue->readHead = next;
		if (next)
			next->prev = NULL;
		else
			queue->readTail = NULL;
	} else {
		queue->sendHead = next;
		if (next)
			next->prev = NULL;
		else
			queue->sendTail = NULL;
	}
	request->next = NULL;
	request->prev = NULL;
	if ((request->type & 0xfd) == 0) {
		request->length = queue->receivePosition;
		queue->receivePosition = 0;
		if (request->type == 0) {
			BtNotifyEvent(10, 48, request->argument, deviceAddress);
			return;
		}
	}
	queue->sendPosition = 0;
	if (request->type == 1)
		BtNotifyEvent(11, 48, request->argument, deviceAddress);
	else if (request->internalFlags[0] == 0)
		BtNotifyEvent(12, 48, request->argument, deviceAddress);
	BtStartHidRequest(channel, queue);
}

uintptr_t BtCompleteHidRequest(BtChannel *channel, BtHidQueue *queue,
			      SceBtHidRequest *request, int status)
{
	BtConnection *connection = channel->connection;
	uint64_t deviceAddress = connection->deviceAddress;
	SceBtHidRequest *next = request->next;
	SceBtHidRequest **previous;
	BtStack *stack;
	int headerLength, length, sendLength;
	uintptr_t result;

	if (request->type) {
		queue->sendHead = next;
		previous = next ? &next->prev : &queue->sendTail;
	} else {
		queue->readHead = next;
		previous = next ? &next->prev : &queue->readTail;
	}
	*previous = NULL;
	request->next = NULL;
	request->prev = NULL;
	if (!(request->type & 0xfd)) {
		request->length = queue->receivePosition;
		queue->receivePosition = 0;
		if (!request->type)
			return BtNotifyEvent(10, status, request->argument, deviceAddress);
	}
	queue->sendPosition = 0;
	if (request->type == 1) {
		BtNotifyEvent(11, status, request->argument, deviceAddress);
	} else if (!request->internalFlags[0]) {
		BtNotifyEvent(12, status, request->argument, deviceAddress);
	} else if (request->argument == 0) {
		if (!status && request->length == 53) {
			int version, ignored;

			BtUnpack((const uint8_t *)request->buffer + 35, 8,
				 "2----2", &version, &ignored);
			if (version < 0x3100)
				connection->hid.fallback = 1;
			connection->hid.statusTimer =
				BtCancelTimer(connection->hid.statusTimer);
		}
	} else if (request->argument == 2 && !status) {
		BtHidAuthenticationBuffer *authentication =
			&connection->stack->hidAuthentication;
		BtHidAuthenticationResponse *response =
			&connection->hid.authenticationResponse;

		memcpy(authentication->message.prefix, gBtHidAuthenticationPrefix,
		       sizeof(authentication->message.prefix));
		memcpy(authentication->message.challenges, connection->hid.challenges,
		       sizeof(authentication->message.challenges));
		memcpy(authentication->message.nonce, response->nonce,
		       sizeof(authentication->message.nonce));
		BtSha1Digest((const uint8_t *)&authentication->message,
			     sizeof(authentication->message), authentication->digest);
		BtGenerateRandomBlock(authentication->message.prefix);
		BtGenerateRandomBlock(authentication->message.prefix + 16);
		if (response->reportId == 4 &&
		    response->sequence == connection->hid.sequence &&
		    response->status == 0 &&
		    memcmp(response->digest, authentication->digest,
			   sizeof(authentication->digest)) == 0 &&
		    connection->hid.challengeDeadline)
			connection->hid.retryCount++;
	}
	connection = channel->connection;
	request = queue->sendHead;
	stack = connection->stack;
	if (!request) {
		queue->active = 0;
		return (uintptr_t)stack;
	}
	if (request->type == 1)
		return BtStartHidRequest(channel, queue);
	if (request->type == 2)
		headerLength = request->reportId ? 4 : 3;
	else if (request->type == 3)
		headerLength = request->reportId ? 2 : 1;
	else
		headerLength = 1;
	length = request->length;
	queue->active = 1;
	sendLength = channel->mtu - headerLength;
	if (sendLength >= length)
		sendLength = length;
	queue->sendPosition = sendLength;
	queue->lastPacketLength = headerLength + sendLength;
	if (request->type == 2)
		result = BtSendL2capPacket(connection, channel->remoteCid,
					 "C1#2", 0x40, 1, 'K', &request->reportId,
					 request->reportId != 0, length);
	else
		result = BtSendL2capPacket(connection, channel->remoteCid,
					 "C1##", 0x40, 1, 'S', &request->reportId,
					 request->reportId != 0, request->buffer,
					 sendLength);
	if (request->type != 1)
		result = BtQueueTimer(&connection->hid.requestTimer,
				      BtHidRequestTimedOut, channel, 1000000);
	return result;
}

void BtReceiveHidInterrupt(BtConnection *connection, BtChannel *channel,
			      uint8_t *packet, int length)
{
	uintptr_t result = (uintptr_t)connection;
	unsigned int identity;
	BtHidQueue *queue;
	SceBtHidRequest *request;
	uint8_t *buffer;
	unsigned int header;
	int received;

	if (gBtHidCaptureBuffer && gBtHidCaptureCapacity >= length) {
		BtPack(connection->stack->hidRawPacket.deviceAddress, 6, "E",
		       connection->deviceAddress);
		connection->stack->hidRawPacket.psm = 19;
		memcpy(connection->stack->hidRawPacket.payload, packet, length);
		BtCopyOutProcess(&connection->stack->hidRawPacket,
				 gBtHidCaptureBuffer, length + 7,
				 gBtHidCaptureProcessId);
		{
			BtNotifyEvent(20, 2, length + 7, connection->deviceAddress);
			return;
		}
	}
	identity = connection->device->vendorProductId;
	if (identity == 0x0306054c) {
		result = BtQueueTimer(&connection->hid.keepaliveTimer,
				      BtHidKeepaliveTimedOut, connection, 1800000000);
	} else if (identity == 0x05c4054c) {
		if (connection->hid.challengeDeadline) {
			uint64_t now = BtGetSystemTimeWide();
			uint64_t deadline = connection->hid.challengeDeadline;

			result = now;
			if (now < deadline)
				goto receive;
			if (deadline) {
				unsigned int retryCount = connection->hid.retryCount;

				connection->hid.challengeDeadline = 0;
				if (!retryCount) {
					connection->hid.fallback = 1;
					return;
				}
			}
		}
		if (connection->hid.fallback)
			return;
	}
receive:
	if (length <= 0)
		return;
	header = packet[0];
	if ((header & 0xef) != 0xa1)
		return;
	identity = connection->device->vendorProductId;
	if (identity == 0x03290e6f) {
		if (header == 0xa1 && length <= connection->hidVuReportCapacity + 1)
			result = BtNotifyHidVuEvent(connection->deviceAddress, 9,
						  128, packet + 1, length - 1);
		return;
	}
	queue = &connection->hid.interrupt;
	request = queue->readHead;
	if (!request)
		return;
	received = queue->receivePosition;
	result = received;
	if ((int32_t)(request->length - (uint32_t)received) < length - 1) {
			BtCompleteHidRequest(channel, queue, request, 16);
			return;
		}
	if (identity == 0x05c4054c) {
		if (length != 79 || header != 0xa1 || packet[1] != 17 ||
		    !(packet[2] & 0x80))
			return;
		result = BtUpdateCrc32(0xffffffff, packet, 79);
		if (result != 0xdebb20e3)
			return;
		buffer = request->buffer;
		result = (uintptr_t)buffer;
		if (!buffer)
			goto clear;
		buffer[0] = 17;
		result = (uintptr_t)memcpy(buffer + 1, packet + 4, 75);
		queue->receivePosition += 76;
	} else {
		buffer = request->buffer;
		if (!buffer)
			goto clear;
		result = (uintptr_t)memcpy(buffer + received, packet + 1, length - 1);
		queue->receivePosition += length - 1;
	}
	if (length < channel->mtu)
		result = BtCompleteHidRequest(channel, queue, request, 0);
	return;
clear:
	queue->readHead = NULL;
	queue->readTail = NULL;
	return;
}

void BtReceiveHidControl(BtConnection *connection, BtChannel *channel,
			    uint8_t *packet, int length)
{
	uintptr_t result = (uintptr_t)connection;
	BtHidQueue *queue;
	SceBtHidRequest *request;
	uint8_t *buffer;
	int received;

	if (gBtHidCaptureBuffer && gBtHidCaptureCapacity >= length) {
		BtPack(connection->stack->hidRawPacket.deviceAddress, 6, "E",
		       connection->deviceAddress);
		connection->stack->hidRawPacket.psm = 17;
		memcpy(connection->stack->hidRawPacket.payload, packet, length);
		BtCopyOutProcess(&connection->stack->hidRawPacket,
				 gBtHidCaptureBuffer, length + 7,
				 gBtHidCaptureProcessId);
		{
			BtNotifyEvent(20, 2, length + 7, connection->deviceAddress);
			return;
		}
	}
	if (length <= 0)
		return;
	if (connection->device->vendorProductId == 0x03290e6f) {
		unsigned int eventId;
		unsigned int timer;
		unsigned int header;

		switch (connection->hidVuCommand) {
		case 'I':
			eventId = 4;
			break;
		case 'K':
			eventId = 5;
			break;
		case 'R':
			eventId = 6;
			break;
		case 'S':
			eventId = 7;
			break;
		default:
			return;
		}
		timer = connection->hid.requestTimer;
		connection->flags &= ~0x800000;
		connection->hid.requestTimer = BtCancelTimer(timer);
		header = packet[0];
		if (!(header & 0xf0)) {
				BtNotifyHidVuEvent(connection->deviceAddress,
						  eventId, header, NULL, 0);
				return;
			}
		if ((connection->hidVuCommand == 'I' && header != 0xa1) ||
		    (connection->hidVuCommand != 'I' &&
		     (connection->hidVuCommand != 'K' || header != 0xa3))) {
				BtNotifyHidVuEvent(connection->deviceAddress,
						  eventId, 130, NULL, 0);
				return;
			}
		if (length > connection->hidVuReportCapacity + 1) {
				BtNotifyHidVuEvent(connection->deviceAddress,
						  eventId, 129, NULL, 0);
				return;
			}
		{
			BtNotifyHidVuEvent(connection->deviceAddress, eventId,
					 128, packet + 1, length - 1);
			return;
		}
	}
	if ((packet[0] & 0xf0) == 0x10 && (packet[0] & 15) == 5) {
		connection->flags |= 0x800;
		result = BtBeginDisconnect(connection);
	}
	queue = &connection->hid.control;
	request = queue->sendHead;
	if (!request) {
		queue->active = 0;
		return;
	}
	if (connection->device->vendorProductId == 0x05c4054c &&
	    packet[0] == 0xa3) {
		result = BtUpdateCrc32(0xffffffff, packet, length);
		if (result != 0xdebb20e3)
			return;
	}
	result = BtCancelTimer(connection->hid.requestTimer);
	connection->hid.requestTimer = result;
	if (request->type != 2 || (packet[0] & 0xef) != 0xa3) {
		if (!(packet[0] & 0xf0))
			result = BtCompleteHidRequest(channel, queue, request, 0);
		return;
	}
	received = queue->receivePosition;
	result = received;
	if ((int32_t)(request->length + (uint32_t)received) < length - 1) {
			BtCompleteHidRequest(channel, queue, request, 16);
			return;
		}
	buffer = request->buffer;
	if (!buffer) {
		queue->sendTail = NULL;
		queue->sendHead = NULL;
		queue->active = 0;
		return;
	}
	result = (uintptr_t)memcpy(buffer + received, packet + 1, length - 1);
	queue->receivePosition += length - 1;
	if (length < channel->mtu)
		result = BtCompleteHidRequest(channel, queue, request, 0);
	return;
}

int BtRequestHidVuReport(BtStack *stack, uint64_t deviceAddress,
		       uint8_t command, uint8_t reportId, const void *data,
		       int length, int userMode)
{
	SceUID threadId = sceKernelGetThreadId();
	BtHidVuSubscriber *subscriber;
	BtConnection *connection;
	BtChannel *channel;
	int result = BtLockTransport();

	if (result)
		return result;
	for (subscriber = &stack->hidVuSubscriber;
	     subscriber < &stack->hidVuSubscriber + 1; subscriber++) {
		if (subscriber->threadId == threadId)
			break;
	}
	result = 0x802f4003;
	if (subscriber == &stack->hidVuSubscriber + 1)
		goto unlock;
	connection = BtFindConnection(stack, 1, deviceAddress);
	result = 0x802f4004;
	if (!connection)
		goto unlock;
	result = 0x802f4001;
	if (connection->device->vendorProductId != 0x03290e6f)
		goto unlock;
	result = 0x802f4005;
	if (connection->flags & 0x800000)
		goto unlock;
	channel = BtFindChannel(connection, 4, command == 0xa2 ? 19 : 17);
	result = 0x802f4006;
	if (!channel)
		goto unlock;
	if ((command & 0xfd) == 0x49) {
		BtSendL2capPacket(connection, channel->remoteCid, "1#2", command,
				 &reportId, reportId ? 1 : 0,
				 connection->hidVuReportCapacity);
		connection->flags |= 0x800000;
	} else {
		result = 0x802f4007;
		if (connection->hidVuReportCapacity < length)
			goto unlock;
		result = BtCopyIn(data, stack->scratch, length, userMode);
		if (result) {
			result = 0x802f4008;
			goto unlock;
		}
		BtSendL2capPacket(connection, channel->remoteCid, "C1#",
				 command == 0xa2 ? 0x10000000 : 0x40,
				 1, command, stack->scratch, length);
		connection->flags |= 0x800000;
		if (command == 0xa2)
			goto unlock;
	}
	BtQueueTimer(&connection->hid.requestTimer, BtHidVuRequestTimedOut,
		     connection, 1000000);
	connection->hidVuCommand = command;
	result = 0;
unlock:
	return BtUnlockTransport(result);
}

uintptr_t BtContinueHidInterrupt(BtConnection *connection)
{
	BtChannel *channel;
	BtHidQueue *queue;
	SceBtHidRequest *request;
	uintptr_t result;
	int sendPosition, sendLength, type, mtu;

	if (connection->device->vendorProductId == 0x03290e6f) {
		connection->flags &= ~0x800000;
		return BtNotifyHidVuEvent(connection->deviceAddress, 8, 0,
					  NULL, 0);
	}
	channel = BtFindChannel(connection, 4, 19);
	result = (uintptr_t)channel;
	if (!channel)
		return result;
	queue = &connection->hid.interrupt;
	request = queue->sendHead;
	if (!request) {
		queue->active = 0;
		return result;
	}
	sendPosition = queue->sendPosition;
	sendLength = (int32_t)(request->length - (uint32_t)sendPosition);
	type = request->type;
	mtu = channel->mtu;
	if (mtu <= sendLength)
		sendLength = mtu - 1;
	if ((type & 0xfd) != 1)
		return result;
	if (sendLength > 0 || mtu == queue->lastPacketLength) {
		result = BtSendL2capPacket(connection, channel->remoteCid,
					  "C1#", 0x10000000, 1, 0xb2,
					  (uint8_t *)request->buffer + sendPosition,
					  sendLength);
		queue->lastPacketLength = sendLength + 1;
		queue->sendPosition += sendLength;
	} else if (type == 1) {
		result = BtCompleteHidRequest(channel, queue, request, 0);
	}
	return result;
}

uintptr_t BtContinueHidControl(BtConnection *connection)
{
	BtChannel *channel;
	BtHidQueue *queue;
	SceBtHidRequest *request;
	uintptr_t result = (uintptr_t)connection;
	int sendPosition, sendLength, type, mtu;

	if (connection->device->vendorProductId == 0x03290e6f)
		return result;
	channel = BtFindChannel(connection, 4, 17);
	result = (uintptr_t)channel;
	if (!channel)
		return result;
	queue = &connection->hid.control;
	request = queue->sendHead;
	if (!request) {
		queue->active = 0;
		return result;
	}
	sendPosition = queue->sendPosition;
	sendLength = (int32_t)(request->length - (uint32_t)sendPosition);
	type = request->type;
	mtu = channel->mtu;
	if (mtu <= sendLength)
		sendLength = mtu - 1;
	if ((type & 0xfd) != 1)
		return result;
	if (sendLength > 0 || mtu == queue->lastPacketLength) {
		result = BtSendL2capPacket(connection, channel->remoteCid,
					  "C1#", 0x10000000, 1, 0xb3,
					  (uint8_t *)request->buffer + sendPosition,
					  sendLength);
		queue->lastPacketLength = sendLength + 1;
		queue->sendPosition += sendLength;
	} else if (type == 1) {
		result = BtCompleteHidRequest(channel, queue, request, 0);
	}
	return result;
}

int BtQueueHidTransfer(BtStack *stack, uint64_t deviceAddress,
		       SceBtHidRequest *request)
{
	BtConnection *connection = BtFindConnection(stack, 1, deviceAddress);
	BtDevice *device;
	BtHidQueue *queue;
	BtChannel *channel;
	SceBtHidRequest *tail, **link;
	unsigned int type;
	int psm;

	if (!connection)
		return 0x802f0904;
	device = connection->device;
	if (!device || !(device->profiles & 0x40))
		return 0x802f0905;
	if (!request->buffer)
		return 0x802f0907;
	type = request->type;
	if (type <= 1) {
		queue = &connection->hid.interrupt;
		psm = 19;
	} else {
		if (type - 2 > 1)
			return 0x802f0906;
		queue = &connection->hid.control;
		psm = 17;
	}
	if (device->vendorProductId == 0x03290e6f)
		return 0x802f090c;
	if (device->vendorProductId == 0x05c4054c &&
	    request->internalFlags[0] == 0) {
		if (type == 0)
			goto enqueueRead;
		if (type == 1) {
			if (*(uint8_t *)request->buffer != 17)
				return 0x802f090b;
		} else if (type == 2) {
			if ((unsigned int)request->reportId - 5 > 1)
				return 0x802f090b;
		} else if (type == 3) {
			return 0x802f090b;
		}
	}
	if (type == 0) {
enqueueRead:
		tail = queue->readTail;
		request->prev = tail;
		link = tail ? &tail->next : &queue->readHead;
		*link = request;
		request->next = NULL;
		queue->readTail = request;
	} else {
		tail = queue->sendTail;
		link = tail ? &tail->next : &queue->sendHead;
		request->prev = tail;
		*link = request;
		request->next = NULL;
		queue->sendTail = request;
	}
	if (request->type && !queue->active && queue->sendHead == request) {
		channel = BtFindChannel(connection, 36, psm);
		if (channel && (channel->flags & 0x1000))
			return BtStartHidRequest(channel, queue);
	}
	return 0;
}

uintptr_t BtHidInterruptOpened(BtConnection *connection, BtChannel *channel)
{
	BtDevice *device = connection->device;
	unsigned int identity = device->vendorProductId;
	BtChannel *control;
	uintptr_t result;
	int latency = 0;

	if (identity == 0x05c4054c) {
		SceBtHidRequest *request = &connection->hid.statusRequest;

		BtQueueTimer(&connection->hid.featureTimer, BtBeginHidChallenge,
			     connection, 2000000);
		connection->hid.challengeDeadline = BtGetSystemTimeWide() + 35000000;
		request->type = 2;
		request->internalFlags[1] = 255;
		request->internalFlags[0] = 255;
		request->reportId = 6;
		request->buffer = connection->hid.statusResponse;
		request->length = 53;
		request->argument = 0;
		BtQueueHidTransfer(connection->stack, connection->deviceAddress,
				   request);
		BtQueueTimer(&connection->hid.statusTimer, BtPollHidStatus,
			     connection, 2000000);
		latency = 18;
	} else if (identity == 0x03d5054c || identity == 0x0268054c ||
		   identity == 0x042f054c || identity == 0x03290e6f) {
		latency = 18;
	} else if (identity == 0x0306054c) {
		BtQueueTimer(&connection->hid.keepaliveTimer,
			     BtHidKeepaliveTimedOut, connection, 1800000000);
		latency = 72;
	}
	if (connection->stack->controllerType == 1) {
		connection->avdtp.controllerSleepState = 2;
		BtSendHciCommand(connection->stack, 0xfc24, "21",
				 connection->aclHandle, 2);
	}
	if (latency)
		BtSendHciCommand(connection->stack, 0x0803, "22222",
				 connection->aclHandle, latency, latency, 1, 0);
	result = BtSetConnectedProfiles(connection, NULL, 0x40);
	if (!connection->hid.control.active) {
		control = BtFindChannel(connection, 4, 17);
		result = (uintptr_t)control;
		if (control)
			result = BtStartHidRequest(control, &connection->hid.control);
	}
	if (!connection->hid.interrupt.active)
		result = BtStartHidRequest(channel, &connection->hid.interrupt);
	if (device->vendorProductId == 0x03290e6f)
		result = BtNotifyHidVuEvent(connection->deviceAddress, 3, 0, NULL, 0);
	return result;
}
