/* RFCOMM channel setup; original source filename unavailable. */
#include <stdarg.h>
#include "bt_internal.h"

BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtSendL2capConnectionRequest(BtConnection *connection, BtChannel *channel);

int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
int BtDisconnectConnection(BtStack *stack, BtConnection *connection);
int BtSendL2capDisconnectRequest(BtConnection *connection, BtChannel *channel);

/* Control is the first unnamed word; only UIH frames have payload arguments. */
int BtSendRfcommFrame(BtConnection *connection, BtChannel *channel,
		      unsigned int address, ...)
{
	BtRfcommChannelState *state = &channel->protocol.rfcomm;
	BtRingBuffer *transmit = &state->transmit;
	const uint8_t *data;
	va_list arguments;
	int control, kind, length, result = 0;
	int chunk, available, creditLength, frameControl;
	unsigned int flags, frameAddress = address;

	va_start(arguments, address);
	control = va_arg(arguments, int);
	kind = control & ~0x10;
	switch (kind) {
	case 0x43:
	case 0x2f:
	case 0x63:
		result = BtSendL2capPacket(connection, channel->remoteCid,
					 "<11L%", frameAddress, control, 0);
		goto out;
	case 0xef:
		break;
	default:
		goto out;
	}
	frameAddress &= ~2;
	if (!(channel->flags & 2))
		frameAddress |= 2;
	data = va_arg(arguments, const uint8_t *);
	length = va_arg(arguments, int);
	if (!(frameAddress & 0xfc)) {
		result = BtSendL2capPacket(connection, channel->remoteCid,
					 "<11>L#%", frameAddress, 0xef, length,
					 data, length);
		goto out;
	}
	if (data && length > 0) {
		result = BtRingWrite(transmit, data, length, 0);
		if (result < length)
			goto out;
	}
	chunk = transmit->used;
	for (;;) {
		if (chunk >= channel->mtu - 5)
			chunk = channel->mtu - 6;
		available = transmit->capacity - transmit->readPosition;
		if (chunk >= available)
			chunk = available;
		flags = state->flags;
		if (chunk <= 0 || (!(flags & 0x400) &&
		    (state->transmitCredits || (flags & 0x1000)) &&
		    (flags & 0x2000))) {
			creditLength = state->pendingCredits;
			frameControl = 0xef;
			if (creditLength) {
				creditLength = !(state->flags & 0x1000);
				frameControl = creditLength ? 0xff : 0xef;
			}
			result = BtSendL2capPacket(connection, channel->remoteCid,
				"<11>L##%", frameAddress, frameControl, chunk,
				&state->pendingCredits, creditLength,
				transmit->data + transmit->readPosition, chunk);
			state->pendingCredits = 0;
			if (chunk > 0) {
				if (state->transmitCredits)
					state->transmitCredits--;
				BtRingConsume(transmit, chunk);
			}
		} else {
			if (!state->pendingCredits || (flags & 0x1000))
				goto out;
			result = BtSendL2capPacket(connection, channel->remoteCid,
				"<11>L##%", frameAddress, 0xff, 0,
				&state->pendingCredits, 1,
				transmit->data + transmit->readPosition, 0);
			state->pendingCredits = 0;
		}
		if (result < 0)
			break;
		chunk = transmit->used;
		if (chunk <= 0)
			break;
	}
out:
	va_end(arguments);
	return result;
}

void BtRfcommClosed(BtConnection *connection, BtChannel *channel)
{
	if (connection->connectingProfile == channel->protocol.rfcomm.profile)
		BtClearConnectedProfiles(connection, NULL, connection->connectingProfile);
	BtDisconnectConnection(connection->stack, connection);
}

uintptr_t BtRequestRfcommDisconnect(BtConnection *connection, BtChannel *channel)
{
	unsigned int address;

	if (channel->protocol.rfcomm.profile & connection->excludedProfiles) {
		BtSendL2capDisconnectRequest(connection, channel);
		return 1;
	}
	channel->protocol.rfcomm.flags |= 0x20;
	address = (uint8_t)(channel->protocol.rfcomm.dlci << 2) | 3;
	if (channel->flags & 2)
		address &= 0xfd;
	BtSendRfcommFrame(connection, channel, address, 0x53);
	return 1;
}

uintptr_t BtRfcommOpened(BtConnection *connection, BtChannel *channel)
{
	if (!(channel->flags & 2)) {
		channel->protocol.rfcomm.flags |= 1;
		return BtSendRfcommFrame(connection, channel, 3, 0x3f);
	}
	return (uintptr_t)connection;
}

int BtInitializeRfcomm(BtConnection *connection, BtChannel *channel,
		       unsigned int profile, unsigned int dlci)
{
	if (!channel) {
		channel = BtFindChannel(connection, 20, 3);
		if (!channel)
			return (int)0x802f0701;
	}
	channel->ops = &gBtRfcommOps;
	BtRingReset(&channel->protocol.rfcomm.transmit);
	channel->protocol.rfcomm.transmit.capacity = 512;
	channel->protocol.rfcomm.transmit.data = channel->protocol.rfcomm.data;
	if (channel->flags & 2)
		return 0;
	channel->protocol.rfcomm.profile = profile;
	channel->protocol.rfcomm.dlci = dlci;
	return BtSendL2capConnectionRequest(connection, channel);
}
