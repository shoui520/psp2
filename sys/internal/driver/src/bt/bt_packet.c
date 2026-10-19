/* Bluetooth packet adapters; original source filename unavailable. */
#include "bt_internal.h"

void BtDispatchHciEvent(BtStack *stack, uint8_t *packet, int length);
void BtDispatchAclPacket(BtStack *stack, uint8_t *packet, int length);
int BtReadLe16(const uint8_t *source);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtSendL2capPacket(BtConnection *connection, int channel,
		     const char *format, ...);
BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress);
void BtDeliverConnectionData(BtConnection *connection, uint32_t profile,
			    uint8_t *packet, unsigned int length);
uintptr_t BtCompleteAudioPacket(BtConnection *connection);
uintptr_t BtContinueHidControl(BtConnection *connection, unsigned int argument);
uintptr_t BtContinueHidInterrupt(BtConnection *connection, unsigned int argument);
uintptr_t BtContinueSerialPacket(BtConnection *connection);

uintptr_t BtNotifyPacketCompletion(BtConnection *connection, uint32_t profile,
				   unsigned int argument)
{
	if (profile & 8)
		return BtCompleteAudioPacket(connection);
	if (profile & 0x40)
		return BtContinueHidControl(connection, argument);
	if (profile & 0x10000000)
		return BtContinueHidInterrupt(connection, argument);
	if (profile & 0x300)
		return BtContinueSerialPacket(connection);
	return (uintptr_t)connection;
}

void BtDeliverMediaPacket(BtConnection *connection, BtChannel *channel,
			       uint8_t *packet, int length)
{
	(void)channel;
	BtDeliverConnectionData(connection, 4, packet, length);
}

void BtReceiveHciEvent(BtStack *stack, uint8_t *packet, int length)
{
	BtDispatchHciEvent(stack, packet, length);
}

BtConnection *BtReleaseOutstandingAclPackets(BtConnection *connection)
{
	int outstanding = connection->outstandingAclPackets;
	BtStack *stack = connection->stack;

	if (outstanding > 0)
		stack->outstandingAclPackets -= outstanding;
	return connection;
}

void BtReceiveAclPacket(BtStack *stack, uint8_t *packet, int length)
{
	BtDispatchAclPacket(stack, packet, length);
}

int BtInjectHidPacket(BtStack *stack, const void *buffer, unsigned int length,
		      int userMode)
{
	BtHidRawPacket *packet = &stack->hidRawPacket;
	uint64_t deviceAddress;
	BtConnection *connection;
	BtChannel *channel;
	int result;

	if (length - 8 > 0xff8)
		return (int)0x802f1701;
	result = BtCopyIn(buffer, stack->scratch, length, userMode);
	if (result < 0)
		return result;
	BtUnpack(packet->deviceAddress, sizeof(packet->deviceAddress), "E",
		 &deviceAddress);
	connection = BtFindConnection(stack, 1, deviceAddress);
	if (!connection)
		return (int)0x802f1703;
	if ((packet->psm & 0xfd) != 0x11)
		return (int)0x802f1702;
	channel = BtFindChannel(connection, 0x24, packet->psm);
	if (!channel)
		return (int)0x802f1702;
	return BtSendL2capPacket(channel->connection, channel->remoteCid, "#",
				packet->payload, length - 7);
}

void BtReceiveScoPacket(BtStack *stack, uint8_t *packet, int length)
{
	if (length > 2 && packet[2] == length - 3) {
		BtConnection *connection = BtFindConnection(stack, 3,
			BtReadLe16(packet) & 0xfff);

		if (connection)
			BtDeliverConnectionData(connection,
				0x20000000, packet, length);
	}
}
