/* Connection and channel lookup; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress)
{
	BtConnection *end = stack->connections + 7;
	BtConnection *connection;
	BtConnection *freeConnection = stack->connections;
	uint64_t firstAddress;

	if (!deviceAddress)
		return NULL;
	switch (selector & 7) {
	case 2:
		connection = stack->connections;
		do {
			if (connection->aclHandle == (uint16_t)deviceAddress)
				goto found;
		} while (++connection < end);
		if (!(selector & 0x10))
			return NULL;
		while (freeConnection->deviceAddress) {
			if (++freeConnection >= end)
				return NULL;
		}
		freeConnection->aclHandle = deviceAddress;
		break;
	case 3:
		connection = stack->connections;
		do {
			if (connection->scoHandle == (uint16_t)deviceAddress)
				goto found;
		} while (++connection < end);
		return NULL;
	case 1:
		firstAddress = freeConnection->deviceAddress;
		connection = freeConnection;
		if (firstAddress == deviceAddress)
			goto found;
		while (++connection < end) {
			if (connection->deviceAddress == deviceAddress)
				goto found;
		}
		if (!(selector & 0x10))
			return NULL;
		if (firstAddress) {
			do {
				if (++freeConnection >= end)
					return NULL;
			} while (freeConnection->deviceAddress);
		}
		freeConnection->deviceAddress = deviceAddress;
		break;
	default:
		return NULL;
	}
	freeConnection->stack = stack;
	freeConnection->linkState = 1;
	memset(freeConnection->channels, 0, sizeof(freeConnection->channels));
	return freeConnection;
found:
	return selector & 0x10 ? NULL : connection;
}

BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier)
{
	BtChannel *end = connection->channels + 16;
	BtStack *stack = connection->stack;
	BtChannel *channel = connection->channels;
	uint16_t localCid;
	int mtu;

	if (!identifier)
		return NULL;
	switch (selector & 7) {
	case 4:
		if (selector & 0x10) {
			localCid = stack->nextChannelId;
			if (localCid <= 63) {
				stack->nextChannelId = 65;
				localCid = 64;
			} else {
				stack->nextChannelId = localCid + 1;
			}
			while (channel->psm) {
				if (++channel >= end)
					return NULL;
			}
			memset(channel, 0, sizeof(*channel));
			channel->connection = connection;
			mtu = identifier == 15 ? 1519 : 672;
			channel->psm = identifier;
			channel->localCid = localCid;
			channel->mtu = mtu;
			return channel;
		}
		do {
			if (channel->psm == identifier)
				return channel;
		} while (++channel < end);
		return NULL;
	case 5:
		do {
			if (channel->localCid == identifier)
				goto found;
		} while (++channel < end);
		return NULL;
	case 6:
		do {
			if (channel->remoteCid == identifier)
				goto found;
		} while (++channel < end);
		return NULL;
	case 7:
		do {
			if (channel->psm == 3 &&
			    channel->protocol.rfcomm.dlci == (uint32_t)identifier)
				goto found;
		} while (++channel < end);
		return NULL;
	default:
		return NULL;
	}
found:
	return selector & 0x10 ? NULL : channel;
}
