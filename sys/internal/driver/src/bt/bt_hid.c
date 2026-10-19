/* HID channel setup; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

void BtPollHidChallenge(void *argument);

int gBtHidCaptureCapacity;
uint8_t *gBtHidCaptureBuffer;
SceUID gBtHidCaptureProcessId;

void BtHidVuRequestTimedOut(void *argument)
{
	BtConnection *connection = argument;
	int eventId;

	connection->hid.requestTimer = 0;
	connection->flags &= ~0x800000;
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
	BtNotifyHidVuEvent(connection->deviceAddress, eventId, 131, NULL, 0);
}

void BtHidKeepaliveTimedOut(void *argument)
{
	BtConnection *connection = argument;

	connection->hid.keepaliveTimer = 0;
	BtBeginDisconnect(connection);
}

uintptr_t BtHidControlOpened(BtConnection *connection, BtChannel *channel)
{
	uintptr_t result = (uintptr_t)connection;

	if (!(channel->flags & 2)) {
		channel = BtFindChannel(connection, 20, 19);
		result = (uintptr_t)channel;
		if (channel) {
			channel->ops = &gBtHidInterruptOps;
			if (!(channel->flags & 2))
				result = BtSendL2capConnectionRequest(connection, channel);
		}
	}
	return result;
}

void BtHidInterruptClosed(BtConnection *connection, BtChannel *channel)
{
	(void)channel;
	channel = BtFindChannel(connection, 4, 17);
	if (channel)
		BtSendL2capDisconnectRequest(connection, channel);
	else
		BtDisconnectConnection(connection->stack, connection);
}

void BtPollHidStatus(void *argument)
{
	BtConnection *owner = argument;
	SceBtHidRequest *request = &owner->hid.statusRequest;
	SceBtHidRequest *tail;
	SceBtHidRequest **link;
	BtConnection *connection;
	BtHidQueue *queue;
	BtChannel *channel;
	uint8_t *buffer;
	uint32_t identity;
	unsigned int type;
	int psm;

	request->reportId = 6;
	request->type = 2;
	request->buffer = owner->hid.statusResponse;
	request->length = sizeof(owner->hid.statusResponse);
	request->internalFlags[1] = 255;
	request->internalFlags[0] = 255;
	request->argument = 0;
	connection = BtFindConnection(owner->stack, 1, owner->deviceAddress);
	if (!connection || !connection->device ||
	    !(connection->device->profiles & 0x40))
		goto schedule;
	buffer = request->buffer;
	if (!buffer)
		goto schedule;
	type = request->type;
	if (type <= 1) {
		psm = 19;
		queue = &connection->hid.interrupt;
	} else {
		if (type - 2 > 1)
			goto schedule;
		psm = 17;
		queue = &connection->hid.control;
	}
	identity = connection->device->vendorProductId;
	if (identity == 0x03290e6f)
		goto schedule;
	if (identity == 0x05c4054c && request->internalFlags[0] == 0) {
		switch (type) {
		case 1:
			if (*buffer != 17)
				goto schedule;
			break;
		case 2:
			if ((unsigned int)request->reportId - 5 > 1)
				goto schedule;
			break;
		case 3:
			goto schedule;
		}
	}
	if (type == 0) {
		tail = queue->readTail;
		link = tail ? &tail->next : &queue->readHead;
		request->prev = tail;
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
	if (request->type != 0 && !queue->active && queue->sendHead == request) {
		channel = BtFindChannel(connection, 36, psm);
		if (channel && (channel->flags & 0x1000))
			BtStartHidRequest(channel, queue);
	}
schedule:
	BtQueueTimer(&owner->hid.statusTimer, BtPollHidStatus, owner, 2000000);
}

void BtPollHidAuthenticationResponse(void *argument)
{
	BtConnection *owner = argument;
	SceBtHidRequest *request = &owner->hid.featureRequest;
	SceBtHidRequest *tail;
	SceBtHidRequest **link;
	BtConnection *connection;
	BtHidQueue *queue;
	BtChannel *channel;
	uint8_t *buffer;
	uint32_t identity;
	unsigned int type;
	int psm;

	request->reportId = 4;
	request->type = 2;
	request->argument = 2;
	request->buffer = owner->hid.featureResponse;
	request->length = sizeof(owner->hid.featureResponse);
	request->internalFlags[1] = 255;
	request->internalFlags[0] = 255;
	memset(owner->hid.featureResponse, 0, sizeof(owner->hid.featureResponse));
	connection = BtFindConnection(owner->stack, 1, owner->deviceAddress);
	if (!connection || !connection->device ||
	    !(connection->device->profiles & 0x40))
		goto schedule;
	buffer = request->buffer;
	if (!buffer)
		goto schedule;
	type = request->type;
	if (type <= 1) {
		queue = &connection->hid.interrupt;
		psm = 19;
	} else {
		if (type - 2 > 1)
			goto schedule;
		queue = &connection->hid.control;
		psm = 17;
	}
	identity = connection->device->vendorProductId;
	if (identity == 0x03290e6f)
		goto schedule;
	if (identity == 0x05c4054c && request->internalFlags[0] == 0) {
		switch (type) {
		case 1:
			if (*buffer != 17)
				goto schedule;
			break;
		case 2:
			if ((unsigned int)request->reportId - 5 > 1)
				goto schedule;
			break;
		case 3:
			goto schedule;
		}
	}
	if (request->type == 0) {
		tail = queue->readTail;
		link = tail ? &tail->next : &queue->readHead;
		request->prev = tail;
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
	if (request->type != 0 && !queue->active && queue->sendHead == request) {
		channel = BtFindChannel(connection, 36, psm);
		if (channel && (channel->flags & 0x1000))
			BtStartHidRequest(channel, queue);
	}
schedule:
	BtQueueTimer(&owner->hid.featureTimer, BtPollHidChallenge, owner, 2000000);
}

void BtPollHidChallenge(void *argument)
{
	BtConnection *owner = argument;
	BtStack *stack = owner->stack;
	SceBtHidRequest *request = &owner->hid.featureRequest;
	SceBtHidRequest *tail;
	SceBtHidRequest **link;
	BtConnection *connection;
	BtHidQueue *queue;
	BtChannel *channel;
	uint8_t *buffer;
	uint32_t identity;
	unsigned int type;
	uint8_t sequence;
	int psm;

	owner->hid.featureTimer = 0;
	request->argument = 0;
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
	connection = BtFindConnection(stack, 1, owner->deviceAddress);
	if (!connection || !connection->device ||
	    !(connection->device->profiles & 0x40))
		goto schedule;
	buffer = request->buffer;
	if (!buffer)
		goto schedule;
	type = request->type;
	if (type <= 1) {
		queue = &connection->hid.interrupt;
		psm = 19;
	} else {
		if (type - 2 > 1)
			goto schedule;
		queue = &connection->hid.control;
		psm = 17;
	}
	identity = connection->device->vendorProductId;
	if (identity == 0x03290e6f)
		goto schedule;
	if (identity == 0x05c4054c && request->internalFlags[0] == 0) {
		switch (type) {
		case 1:
			if (*buffer != 17)
				goto schedule;
			break;
		case 2:
			if ((unsigned int)request->reportId - 5 > 1)
				goto schedule;
			break;
		case 3:
			goto schedule;
		}
	}
	if (request->type == 0) {
		tail = queue->readTail;
		link = tail ? &tail->next : &queue->readHead;
		request->prev = tail;
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
	if (request->type != 0 && !queue->active && queue->sendHead == request) {
		channel = BtFindChannel(connection, 36, psm);
		if (channel && (channel->flags & 0x1000))
			BtStartHidRequest(channel, queue);
	}
schedule:
	BtQueueTimer(&owner->hid.featureTimer, BtPollHidAuthenticationResponse,
		     owner, 2000000);
}

void BtInitializeHidCapture(void)
{
	gBtHidCaptureProcessId = -1;
}

int BtSetHidCaptureBuffer(SceUID processId, uint8_t *buffer, int capacity)
{
	int invalid = buffer != NULL;

	if (capacity > 7)
		invalid = 0;
	if (invalid) {
		invalid = (int)0x802f1701;
	} else {
		gBtHidCaptureProcessId = processId;
		gBtHidCaptureBuffer = buffer;
		gBtHidCaptureCapacity = capacity;
	}
	return invalid;
}

int BtInitializeHidChannels(BtConnection *connection, BtChannel *channel)
{
	if (channel) {
		if (channel->psm != 17) {
			if (channel->psm == 19)
				channel->ops = &gBtHidInterruptOps;
			goto ready;
		}
	} else {
		channel = BtFindChannel(connection, 20, 17);
		if (!channel)
			return (int)0x802f0901;
	}
	channel->ops = &gBtHidControlOps;
ready:
	if (channel->flags & 2)
		return 0;
	return BtSendL2capConnectionRequest(connection, channel);
}

