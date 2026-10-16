/* Bluetooth profile data delivery; original source filename unavailable. */
#include "bt_internal.h"

int BtReceiveHsp(BtConnection *connection, const char *packet, int length);
void BtReceiveAudioPacket(BtConnection *connection, unsigned int profile,
			  const uint8_t *packet, int length);
int BtReceiveImageResponse(BtConnection *connection, const uint8_t *packet,
			   int length);
int BtReceiveImage(BtConnection *connection, uint8_t *packet, int length);
int BtReceiveNetworkPacket(BtStack *stack, uint64_t deviceAddress,
			   const void *data, unsigned int length);
int BtReceiveSerialPort(BtConnection *connection, const uint8_t *packet,
			int length);

void BtDeliverConnectionData(BtConnection *connection, unsigned int profile,
			     uint8_t *packet, int length)
{
	if (profile & 0x10000)
		BtReceiveHsp(connection, (const char *)packet, length);
	else if (profile & 0x2000000c)
		BtReceiveAudioPacket(connection, profile, packet, length);
	else if (profile & 0x2000)
		BtReceiveImageResponse(connection, packet, length);
	else if (profile & 0x1000)
		BtReceiveImage(connection, packet, length);
	else if (profile & 0x300)
		BtReceiveNetworkPacket(&gBtStack, connection->deviceAddress,
				       packet, length);
	else if (profile & 0x80000)
		BtReceiveSerialPort(connection, packet, length);
}
