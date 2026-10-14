/* BNEP channel setup; original source filename unavailable. */
#include "bt_internal.h"

BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtSendL2capConnectionRequest(BtConnection *connection, BtChannel *channel);

int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
void BtRetryBnep(void *argument);
int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);

int BtSendL2capDisconnectRequest(BtConnection *connection, BtChannel *channel);
unsigned int BtCancelTimer(unsigned int timer);
int BtDisconnectConnection(BtStack *stack, BtConnection *connection);

void BtBnepClosed(BtConnection *connection, BtChannel *channel)
{
	channel->protocol.bnep.retryTimer =
		BtCancelTimer(channel->protocol.bnep.retryTimer);
	BtDisconnectConnection(connection->stack, connection);
}

void BtBnepSetupTimedOut(void *argument)
{
	BtChannel *channel = argument;

	BtSendL2capDisconnectRequest(channel->connection, channel);
}

uintptr_t BtBnepOpened(BtConnection *connection, BtChannel *channel)
{
	uintptr_t result = (uintptr_t)connection;
	unsigned int remoteService;

	if (channel->flags & 2)
		return result;
	result = BtQueueTimer(&channel->protocol.bnep.retryTimer,
			     BtRetryBnep, channel, 5000000);
	channel->protocol.bnep.localService = 0x1115;
	switch (channel->protocol.bnep.profile) {
	case 0x200:
		remoteService = 0x1116;
		break;
	case 0x400:
		remoteService = 0x1117;
		break;
	case 0x100:
		remoteService = 0x1115;
		break;
	default:
		return result;
	}
	channel->protocol.bnep.remoteService = remoteService;
	return BtSendL2capPacket(connection, channel->remoteCid, "11 1HH",
				1, 1, 2, remoteService, 0x1115);
}


void BtRetryBnep(void *argument)
{
	BtChannel *channel = argument;
	unsigned int remoteService;

	BtQueueTimer(&channel->protocol.bnep.retryTimer,
		     BtBnepSetupTimedOut, channel, 5000000);
	channel->protocol.bnep.localService = 0x1115;
	switch (channel->protocol.bnep.profile) {
	case 0x200:
		remoteService = 0x1116;
		break;
	case 0x400:
		remoteService = 0x1117;
		break;
	case 0x100:
		remoteService = 0x1115;
		break;
	default:
		return;
	}
	channel->protocol.bnep.remoteService = remoteService;
	BtSendL2capPacket(channel->connection, channel->remoteCid, "11 1HH",
			  1, 1, 2, remoteService, 0x1115);
}

int BtInitializeBnep(BtConnection *connection, BtChannel *channel,
		     unsigned int profile)
{
	if (!channel) {
		channel = BtFindChannel(connection, 20, 15);
		if (!channel)
			return (int)0x802f0a01;
	}
	channel->ops = &gBtBnepOps;
	channel->protocol.bnep.profile = profile;
	if (channel->flags & 2)
		return 0;
	return BtSendL2capConnectionRequest(connection, channel);
}
