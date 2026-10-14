/* BNEP Ethernet frame transmission; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
unsigned int BtCancelTimer(unsigned int timer);

void BtCancelBnepTimers(BtConnection *connection)
{
	BtChannel *channel = connection->channels;
	BtChannel *end = connection->channels + 16;

	for (; channel < end; channel++) {
		if (channel->psm == 15)
			channel->protocol.bnep.retryTimer =
				BtCancelTimer(channel->protocol.bnep.retryTimer);
	}
}

int BtSendBnepFrame(BtConnection *connection, const void *data, int length,
		    int userMode)
{
	const BtEthernetFrame *frame = data;
	BtStack *stack = connection->stack;
	BtChannel *channel = BtFindChannel(connection, 4, 15);
	uint64_t destinationAddress;
	int payloadLength;
	int packetLength;

	if (!channel)
		return (int)0x802f0a03;
	if (length <= 13)
		return (int)0x802f0a04;
	BtUnpack(frame->ethernet.ether_dhost, 6, "E", &destinationAddress);
	payloadLength = length - sizeof(frame->ethernet);
	if (connection->deviceAddress == destinationAddress) {
		BtBnepCompressedPacket *packet =
			(BtBnepCompressedPacket *)stack->scratch;

		packet->packetType = 2;
		memcpy(packet->protocolBe, &frame->ethernet.ether_type,
		       sizeof(packet->protocolBe));
		memcpy(packet->payload, frame->payload, payloadLength);
		packetLength = payloadLength + sizeof(*packet);
	} else {
		BtBnepDestinationPacket *packet =
			(BtBnepDestinationPacket *)stack->scratch;

		packet->packetType = 4;
		memcpy(packet->destination, frame->ethernet.ether_dhost,
		       sizeof(packet->destination));
		memcpy(packet->protocolBe, &frame->ethernet.ether_type,
		       sizeof(packet->protocolBe));
		memcpy(packet->payload, frame->payload, payloadLength);
		packetLength = payloadLength + sizeof(*packet);
	}
	return BtSendL2capPacket(connection, channel->remoteCid, "C#", 0x200,
				userMode, stack->scratch, packetLength);
}
