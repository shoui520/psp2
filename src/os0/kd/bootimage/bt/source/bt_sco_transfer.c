/* Bluetooth SCO transmission; original source filename unavailable. */
#include <string.h>
#include "bt_internal.h"

BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			int identifier);
void BtWriteLe16(uint8_t *destination, unsigned int value);
int BtQueueTransmit(unsigned int packetType, const uint8_t *data, int length,
		    void (*completion)(void *argument), void *argument);

int BtSendScoPacket(BtConnection *connection, const void *data, int length,
		    int userMode)
{
	uint32_t profiles = connection->connectedProfiles;
	BtStack *stack = connection->stack;
	BtChannel *channel;
	int identifier;
	int packetLength;
	BtHciScoPacket *packet;

	if (profiles & 1)
		identifier = connection->hfpHandsfreeChannel;
	else if (profiles & 2)
		identifier = connection->hfpAudioGatewayChannel;
	else if (profiles & 0x10000)
		identifier = connection->hspHeadsetChannel;
	else if (profiles & 0x20000)
		identifier = connection->hspAudioGatewayChannel;
	else
		return (int)0x802f0807;
	channel = BtFindChannel(connection, 7, identifier);
	if (!channel)
		return (int)0x802f0807;
	if (userMode)
		return (int)0x802f0808;
	if ((channel->flags & 0x30000000) != 0x20000000)
		return (int)0x802f0809;
	if (length > 240)
		return (int)0x802f080a;
	packet = &stack->scoPacket;
	packetLength = length + sizeof(*packet);
	BtWriteLe16(packet->handleLe, connection->scoHandle);
	packet->length = length;
	memcpy(packet->payload, data, packetLength);
	return BtQueueTransmit(3, (uint8_t *)packet, packetLength, NULL, NULL);
}
