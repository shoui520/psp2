/* Bluetooth SDIO receive polling; original source filename unavailable. */
#include "bt_transport.h"

void BtPollTransport(void *argument)
{
	BtTransport *transport = argument;
	BtHciEventPacket *event;
	uint8_t units[1];
	int receivedLength, transferLength, packetLength, payloadLength;
	unsigned int packetType;

	if (transport->stack->mode & 4) {
		transport->wakeupTimer = 0;
		return;
	}
	BtQueueTimer(&transport->wakeupTimer, BtPollTransport, transport, 1000000);
	if (SceSdifForDriver_D0F78D9B(transport->function, 98, 1, units, 1) < 0)
		return;
	receivedLength = units[0] << transport->receiveUnitShift;
	if ((unsigned int)(receivedLength - 1) >= 2048)
		return;
	transferLength = receivedLength;
	if (receivedLength > 128)
		transferLength = (receivedLength + 127) & ~127;
	if (SceSdifForDriver_D0F78D9B(transport->function, transport->ioPort,
		transferLength, gBtReceivePacket.bytes, 2) < 0 || receivedLength <= 3)
		return;
	packetLength = BtReadLe24(gBtReceivePacket.packet.length);
	if (receivedLength < packetLength)
		return;
	payloadLength = packetLength - 4;
	if (payloadLength <= 0)
		return;
	packetType = gBtReceivePacket.packet.packetType;
	if (packetType == 3) {
		BtReceiveScoPacket(transport->stack, gBtReceivePacket.packet.payload,
			payloadLength);
	} else if (packetType <= 3) {
		if (packetType == 2)
			BtReceiveAclPacket(transport->stack, gBtReceivePacket.packet.payload,
				payloadLength);
	} else if (packetType == 4 || packetType == 254) {
		event = (BtHciEventPacket *)gBtReceivePacket.packet.payload;
		if (payloadLength > 3 && event->eventCode == 255 &&
		    event->parameterLength > 1 &&
		    ((event->parameters[0] == 32 && event->parameters[1] == 1) ||
		     (event->parameters[0] == 90 && event->parameters[1] == 0))) {
			transport->asleep = 1;
			transport->wakeupPending = 0;
			BtReceiveHciEvent(transport->stack,
				gBtReceivePacket.packet.payload, payloadLength);
			goto cancelTimer;
		}
		BtReceiveHciEvent(transport->stack, gBtReceivePacket.packet.payload,
			payloadLength);
	}
	if (transport->asleep) {
		transport->asleep = 0;
		BtPumpTransmit(transport);
	}
cancelTimer:
	transport->wakeupTimer = BtCancelTimer(transport->wakeupTimer);
}
