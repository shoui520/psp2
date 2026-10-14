/*
 * The "wlan" mdev driver the host side opens: it queues frames and commands
 * for the MAC tasks and keeps the callback the host registers. The filename
 * is inferred.
 */
#include <stddef.h>
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/ieee80211.h"
#include "robin/mactx.h"
#include "robin/mdev.h"
#include "robin/rom.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wlan.h"
#include "robin/wlanrx.h"
#include "robin/wlantask.h"

/* Messages for the "MAC Tx" task, one ROM queue per message kind. */
struct WlanDriverQueue {
	uint32_t control[5];
};

static const char *wlanDriverName __attribute__((section(".itcm.rw"))) = "wlan";
WlanHostCallback wlanHostCallback __attribute__((section(".itcm.rw")));
struct WlanDriverQueue wlanDriverQueue4 __attribute__((section(".dtcm.bss")));
struct WlanDriverQueue wlanDriverQueue15 __attribute__((section(".dtcm.bss")));
struct WlanDriverQueue wlanDriverQueue1 __attribute__((section(".dtcm.bss")));
static struct MdevDriver wlanDriver __attribute__((section(".dtcm.bss")));

/* Appends a message to a ROM message queue (20 bytes of control data). */
int osa_MsgQueueSend(void *queue, void *message);

/* Bit n set: frames of 802.11 type n are passed to the host. */
uint16_t wlanRxFrameTypeMask __attribute__((section(".dtcm.rw")));

/* Command handlers of the driver, not converted yet. */
void wlanUpdateAntennaDiversity(const struct WlanRxStatus *status);		/* 0xaf6 */
void macTxFormatEntry(uintptr_t argument);					/* 0xbdf0, command 0x52 */
void macMgmtUpdateEntry(uintptr_t argument);					/* 0x36236, command 0x58 */
void macMgmtResetState(uintptr_t argument, unsigned int command, uintptr_t second,
		       int third);						/* 0x34ba6, command 0x59 */
void macTxQueryConfig(struct WlanConnection *connection);			/* 0x1365a */

/* Event callback of the driver: frames from the host. */
int wlanDriverEvent(void *device, OsaBuffer *buffer, uintptr_t argument, unsigned int event)
{
	struct OsaEventFlags *flags;

	switch (event) {
	case 1:
		osa_MsgQueueSend(&wlanDriverQueue1, buffer);
		wlanPostConnectionEvent(buffer->context, 2);
		return 0;
	case 4:
		if (wlanApPsEnabled())
			macTxQueryConfig(wlanConnectionByRole(2));
		osa_MsgQueueSend(&wlanDriverQueue4, buffer);
		break;
	case 15:
		osa_MsgQueueSend(&wlanDriverQueue15, buffer);
		break;
	default:
		return 0;
	}
	flags = macTxEventFlags;
	osaEventFlagsSet(flags, 1, 7);
	return 0;
}

/* Control callback of the driver. */
int wlanDriverControl(void *device, unsigned int command, uintptr_t argument, int third)
{
	switch (command) {
	case 0x20:
		wlanHostCallback = (WlanHostCallback)argument;
		break;
	case 0x52:
		macTxFormatEntry(argument);
		break;
	case 0x58:
		macMgmtUpdateEntry(argument);
		break;
	case 0x59:
		macMgmtResetState(argument, command, argument, third);
		break;
	}
	return 0;
}

/* Registers the "wlan" driver. */
int wlanDriverInit(void)
{
	wlanHostCallback = NULL;
	__aeabi_memclr4(&wlanDriverQueue4, sizeof(wlanDriverQueue4));
	__aeabi_memclr4(&wlanDriverQueue15, sizeof(wlanDriverQueue15));
	__aeabi_memclr4(&wlanDriverQueue1, sizeof(wlanDriverQueue1));
	wlanDriver.callbacks[0] = NULL;
	wlanDriver.callbacks[1] = NULL;
	wlanDriver.callbacks[2] = NULL;
	wlanDriver.callbacks[3] = NULL;
	wlanDriver.name = wlanDriverName;
	wlanDriver.callbacks[4] = (void *)wlanDriverEvent;
	wlanDriver.callbacks[5] = (void *)wlanDriverControl;
	return mdevRegisterDriver(&wlanDriver, (void *)wlanDriverControl);
}

/* Pool the frames handed to the host are allocated from. */
extern struct OsaBufferPool wlanPacketBufferPool;

/*
 * Turns the 802.11 data frame a received buffer holds into an Ethernet frame
 * for the host and updates the SDIO packet header in front of it. The buffer
 * must hold more than the 96 bytes of status that precede the frame.
 */
int wlanDecodeDataFrame(OsaBuffer *buffer, WlanPacketHeader *header, const void *wirelessHeader)
{
	int16_t offset = (int16_t)(header->wire.packetOffset[0] |
				   (header->wire.packetOffset[1] << 8));
	uint8_t *payload = (uint8_t *)header + offset + 36;
	struct ether_header *output;
	uint32_t length = buffer->length;
	uint16_t type;

	if (length <= 0x60)
		return 0;
	buffer->length = length - 0x60;
	type = wlanConvertWirelessToEthernet(buffer, &output, wirelessHeader, payload,
					     wlanContextUsesEthernetFrames(buffer->context));
	header->wire.packetType[0] = type;
	header->wire.packetType[1] = type >> 8;
	offset = (uint8_t *)output - (uint8_t *)header;
	header->wire.packetOffset[0] = offset;
	header->wire.packetOffset[1] = offset >> 8;
	return 1;
}

/*
 * Wraps an 802.11 control frame (2-byte length, then the frame) into an SDIO
 * packet of type 0xe5 for the connection and passes it to the host.
 */
int wlanSubmitControlFrame(struct WlanConnection *context, const void *frame)
{
	const uint8_t *source = frame;
	unsigned int length = (source[1] << 8) | source[0];
	OsaBuffer *buffer;
	WlanPacketHeader *packet;
	unsigned int posture;

	length += 32;
	buffer = osa_BufferAlloc(&wlanPacketBufferPool, (uint16_t)(length + 50), 0);
	if (buffer == NULL)
		return 0;
	buffer->length = length + 18;
	buffer->dataOffset = 32;
	packet = (WlanPacketHeader *)((uint8_t *)buffer + 32);
	__aeabi_memclr(packet, 0x12);
	packet->wire.bssType = context->bssType;
	packet->wire.bssNumber = context->bssNumber;
	packet->wire.packetOffset[0] = 0x12;
	packet->wire.packetOffset[1] = 0;
	packet->wire.packetType[0] = 0xe5;
	packet->wire.packetType[1] = 0;
	packet->wire.packetLength[0] = length;
	packet->wire.packetLength[1] = length >> 8;
	buffer->dataOffset = 50;
	__aeabi_memcpy((uint8_t *)buffer + 50, frame, length);
	buffer->dataOffset = 32;
	if (wlanApPsEnabled())
		wlanRaiseEvent(5);
	posture = sysIrqDisable();
	wlanHostCallback(wlanDevice, buffer, 0, 5);
	sysIrqRestore(posture);
	return 1;
}

/* SDIO receive descriptor followed by one more halfword. */
struct WlanHostRxPacket {
	WlanRxDescriptor descriptor;
	uint16_t frameCount;
};

#define WLAN_HOST_RX_FRAME_PLAIN	1
#define WLAN_HOST_RX_FRAME_QOS		2

/*
 * Prepares the SDIO receive packet for a received 802.11 frame: the packet
 * type, the kind of header, and the frame's header copied in front of the
 * data, with the QoS control taken from the receive status. The buffer
 * length is reduced by the status that precedes the frame.
 */
int wlanBuildHostRxPacket(OsaBuffer *buffer, struct WlanHostRxPacket *packet,
			  const struct WlanRxStatus *status, const WlanWirelessFrame *frame)
{
	WlanPacketHeader *header = &packet->descriptor.header;
	int fourAddresses = (frame->frameControl[1] & IEEE80211_FC1_FROMDS) &&
			    (frame->frameControl[1] & IEEE80211_FC1_TODS);
	unsigned int headerLength = 24;
	uint8_t *data;
	uint8_t *destination;
	uint8_t *copied;

	header->wire.packetType[0] = 1;
	header->wire.packetType[1] = 0;
	if (((frame->frameControl[0] >> 2) & 3) == 2 && (frame->frameControl[0] & IEEE80211_FC0_QOS))
		packet->descriptor.reserved = WLAN_HOST_RX_FRAME_QOS;
	else
		packet->descriptor.reserved = WLAN_HOST_RX_FRAME_PLAIN;
	data = (uint8_t *)packet + (int16_t)(header->wire.packetOffset[0] |
					     (header->wire.packetOffset[1] << 8)) + 36;
	if (fourAddresses)
		headerLength = 30;
	if (packet->descriptor.reserved == WLAN_HOST_RX_FRAME_QOS)
		headerLength += 2;
	destination = data - headerLength;
	packet->frameCount = 1;
	__aeabi_memcpy(destination, frame->frameControl, headerLength);
	if (packet->descriptor.reserved == WLAN_HOST_RX_FRAME_QOS) {
		copied = destination + headerLength - 32;
		copied[30] = status->qosControl[0];
		copied[31] = status->qosControl[1];
		buffer->length += 2;
	}
	buffer->length -= 72;
	if (fourAddresses)
		buffer->length += 6;
	header->wire.packetOffset[0] = (uint8_t *)destination - (uint8_t *)packet;
	header->wire.packetOffset[1] = ((uint8_t *)destination - (uint8_t *)packet) >> 8;
	return 1;
}

/*
 * Hands a received 802.11 frame to the host: the frame header is copied
 * aside, the antenna diversity sees the receive status, and the SDIO packet
 * header in front of the frame is filled in. Frames whose type is not
 * enabled are dropped.
 */
void wlanDeliverRxFrame(OsaBuffer *buffer, struct WlanRxStatus *status)
{
	struct WlanConnection *connection;
	struct WlanHostRxPacket *packet;
	WlanWirelessFrame frame;
	unsigned int posture;
	uint16_t length;

	if (status == NULL)
		return;
	connection = buffer->context;
	__aeabi_memcpy(&frame, (uint8_t *)buffer + buffer->dataOffset, sizeof(frame));
	if (!((1 << ((frame.frameControl[0] >> 2) & 3)) & wlanRxFrameTypeMask)) {
		wlanReleasePacket(buffer);
		return;
	}
	wlanUpdateAntennaDiversity(status);
	packet = (struct WlanHostRxPacket *)((uint8_t *)buffer + 32);
	length = buffer->dataOffset - 32;
	buffer->dataOffset = 32;
	packet->descriptor.header.wire.packetOffset[0] = length;
	packet->descriptor.header.wire.packetOffset[1] = length >> 8;
	packet->descriptor.sequence = ((frame.sequenceControl[1] << 8 | frame.sequenceControl[0]) >> 4);
	packet->descriptor.header.wire.bssType = connection->bssType;
	packet->descriptor.header.wire.bssNumber = connection->bssNumber;
	packet->descriptor.snr = wlanClampSnr(connection->snr);
	packet->descriptor.noiseFloor = wlanClampNoise(connection->noise);
	packet->descriptor.priority = 0xff;
	wlanBuildHostRxPacket(buffer, packet, status, &frame);
	packet->descriptor.header.wire.packetType[0] = 5;
	packet->descriptor.header.wire.packetType[1] = 0;
	packet->descriptor.header.wire.packetLength[0] = buffer->length;
	packet->descriptor.header.wire.packetLength[1] = buffer->length >> 8;
	wlanRxDecodeRate(status, &packet->descriptor.htInfo, &packet->descriptor.rate);
	posture = sysIrqDisable();
	wlanHostCallback(wlanDevice, buffer, 0, 5);
	sysIrqRestore(posture);
}
