/* Incoming synchronous audio connection; original filename unavailable. */
#include "bt_internal.h"

int BtAcceptScoConnection(BtConnection *connection)
{
	unsigned int profiles = connection->connectedProfiles;
	unsigned int flags;
	int identifier;
	BtChannel *channel;

	if (profiles & 1)
		identifier = connection->hfpHandsfreeChannel;
	else if (profiles & 2)
		identifier = connection->hfpAudioGatewayChannel;
	else if (profiles & 0x10000)
		identifier = connection->hspHeadsetChannel;
	else if (profiles & 0x20000)
		identifier = connection->hspAudioGatewayChannel;
	else
		return (int)0x802f0803;
	channel = BtFindChannel(connection, 7, identifier);
	if (!channel)
		return (int)0x802f0803;
	flags = channel->flags;
	if (flags & 0x30000000)
		return (int)0x802f0804;
	channel->flags = (flags & ~0x30000000u) | 0x10000000;
	BtSendHciCommand(connection->stack, 0x0429, "6 44 2212",
			 connection->deviceAddress, 8000, 8000, 5, 96, 255, 964);
	return 0;
}
