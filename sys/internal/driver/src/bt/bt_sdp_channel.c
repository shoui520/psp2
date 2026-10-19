/* Bluetooth SDP channel methods; original source filename unavailable. */
#include "bt_internal.h"

uintptr_t BtSdpOpened(BtConnection *connection, BtChannel *channel)
{
	uintptr_t result = (uintptr_t)connection;
	uint8_t continuationState[1];
	uint16_t transactionId;

	if (!(channel->flags & 2)) {
		transactionId = connection->sdpTransactionId;
		continuationState[0] = 0;
		transactionId++;
		connection->sdpTransactionId = transactionId;
		result = BtSendL2capPacket(connection, channel->remoteCid,
			"1HH H1H H H1W #", 6, transactionId, 15,
			0x3503, 0x19, 0x100, 0x800, 0x3505, 0x0a, 0xffff,
			continuationState, 1);
	}
	return result;
}

int BtInitializeSdpChannel(BtConnection *connection, BtChannel *channel)
{
	uint32_t flags;

	if (!channel) {
		flags = connection->flags;
		if (flags & 0x10)
			return 0;
		connection->flags = flags | 0x10;
		channel = BtFindChannel(connection, 20, 1);
		if (!channel)
			return (int)0x802f0208;
	}
	flags = channel->flags;
	channel->ops = &gBtSdpOps;
	if (flags & 2)
		return 0;
	connection->stack->sdpReceive.length = 0;
	return BtSendL2capConnectionRequest(connection, channel);
}
