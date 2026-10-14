/* L2CAP signalling; original source filename unavailable. */
#include "bt_internal.h"

int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);

int BtSendL2capConnectionResponse(BtConnection *connection, BtChannel *channel,
				  int result, int status)
{
	if (!result)
		channel->flags |= 4;
	return BtSendL2capPacket(connection, 1, "112 2222", 3,
		channel->identifier, 8, channel->localCid, channel->remoteCid,
		result, status);
}

int BtSendL2capConfigurationRequest(BtConnection *connection, BtChannel *channel)
{
	uint8_t options[32];
	unsigned int identifier;
	int optionLength = 0;

	if (channel->mtu >= 0)
		optionLength = BtPack(options, 4, "112", 1, 2, channel->mtu);
	channel->flags |= 0x10;
	identifier = (uint8_t)(connection->stack->transactionId + 1);
	if (identifier) {
		connection->stack->transactionId = identifier;
	} else {
		connection->stack->transactionId = 1;
		identifier = 1;
	}
	return BtSendL2capPacket(connection, 1, "112 22#", 4, identifier,
				optionLength + 4, channel->remoteCid, 0,
				options, optionLength);
}

int BtSendL2capConnectionRequest(BtConnection *connection, BtChannel *channel)
{
	unsigned int identifier;

	channel->flags |= 1;
	identifier = (uint8_t)(connection->stack->transactionId + 1);
	if (identifier) {
		connection->stack->transactionId = identifier;
	} else {
		connection->stack->transactionId = 1;
		identifier = 1;
	}
	return BtSendL2capPacket(connection, 1, "112 22", 2, identifier, 4,
				channel->psm, channel->localCid);
}

int BtSendL2capDisconnectRequest(BtConnection *connection, BtChannel *channel)
{
	unsigned int identifier;

	channel->flags |= 0x100;
	identifier = (uint8_t)(connection->stack->transactionId + 1);
	if (identifier) {
		connection->stack->transactionId = identifier;
	} else {
		connection->stack->transactionId = 1;
		identifier = 1;
	}
	return BtSendL2capPacket(connection, 1, "112 22", 6, identifier, 4,
				channel->remoteCid, channel->localCid);
}

int BtSendEchoRequest(BtConnection *connection, const void *data,
		      unsigned int length)
{
	unsigned int identifier = (uint8_t)(connection->stack->transactionId + 1);

	if (identifier) {
		connection->stack->transactionId = identifier;
	} else {
		connection->stack->transactionId = 1;
		identifier = 1;
	}
	return BtSendL2capPacket(connection, 1, "112 #", 8, identifier,
				length, data, length);
}

uintptr_t BtReplyPendingL2capConnection(BtConnection *connection)
{
	BtChannel *channel;

	for (channel = connection->channels;
	     channel < connection->channels + 16; channel++) {
		if (channel->psm && (channel->flags & 6) == 2) {
			unsigned int identifier = channel->identifier;
			unsigned int localCid = channel->localCid;
			unsigned int remoteCid = channel->remoteCid;

			channel->flags |= 4;
			return BtSendL2capPacket(connection, 1, "112 2222", 3,
						identifier, 8, localCid, remoteCid, 0, 0);
		}
	}
	return (uintptr_t)connection;
}
