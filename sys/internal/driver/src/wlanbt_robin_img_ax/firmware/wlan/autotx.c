/*
 * Periodic frame sent on the host's behalf (host command 0x82, Marvell's
 * HostCmd_CMD_AUTO_TX): the record, its timer and the buffer the transmit task
 * takes. The filename is inferred.
 */
#include <stddef.h>
#include <wlanbt/wlanbt_protocol.h>
#include "robin/autotx.h"
#include "robin/conn.h"
#include "robin/rom.h"
#include "robin/txring.h"
#include "robin/wmm.h"

static struct WlanAutoTxRecord wlanAutoTx __attribute__((section(".itcm.bss")));		/* 0x5745e */
static struct WlanAutoTxRecord *wlanAutoTxActive __attribute__((section(".itcm.bss")));	/* 0x574e0 */
static uint8_t wlanAutoTxLastTick[8] __attribute__((section(".itcm.bss")));		/* 0x574e4 */

static void wlanAutoTxSetRemaining(uint64_t remaining)
{
	__aeabi_memcpy(wlanAutoTx.remaining, &remaining, 8);
}

static uint64_t wlanAutoTxGetRemaining(void)
{
	uint64_t remaining;

	__aeabi_memcpy(&remaining, wlanAutoTx.remaining, 8);
	return remaining;
}

/* The host command: a frame to send every interval, or, without a body, none. */
unsigned int hostCmdAutoTxSet(const uint8_t *body)
{
	if (body == NULL) {
		wlanAutoTx.frame.interval = 0;
		wlanAutoTxActive = NULL;
		return 0;
	}
	if ((uint16_t)(body[2] | body[3] << 8) == WLAN_AUTO_TX_TLV_TYPE) {
		__aeabi_memcpy(&wlanAutoTx.frame, body, (uint16_t)(body[4] | body[5] << 8) + 6);
		wlanAutoTxSetRemaining(1000u * wlanAutoTx.frame.interval);
		wlanAutoTxActive = &wlanAutoTx;
		return 0;
	}
	return 1;
}

unsigned int hostCmdAutoTxGet(uint8_t *body)
{
	if (body == NULL)
		return 1;
	if (wlanAutoTx.frame.interval != 0) {
		__aeabi_memcpy(body, &wlanAutoTx.frame, wlanAutoTx.frame.length + 6);
	} else {
		body[0] = 0;
		body[1] = 0;
		body[2] = WLAN_AUTO_TX_TLV_TYPE & 0xff;
		body[3] = WLAN_AUTO_TX_TLV_TYPE >> 8;
		body[4] = 0;
		body[5] = 0;
	}
	return 0;
}

/* No frame is configured. */
void wlanAutoTxInit(void)
{
	wlanAutoTx.frame.interval = 0;
}

/* The host gave a frame. */
int wlanAutoTxConfigured(void)
{
	return wlanAutoTxActive != NULL;
}

/* A frame is configured and the station is associated. */
int wlanAutoTxWanted(const struct WlanConnection *connection)
{
	return wlanAutoTxActive != NULL && (connection->flags2 & WLAN_CONN2_ASSOCIATED) != 0;
}

/*
 * Runs the timer by the time since the last call (milliseconds); 1 once the frame is due.
 */
int wlanAutoTxTick(void)
{
	int due = 0;
	uint64_t now = romGetTimerTimestamp();
	uint64_t last;
	uint64_t elapsed;
	uint64_t remaining;

	__aeabi_memcpy(&last, wlanAutoTxLastTick, 8);
	if (now < last)
		elapsed = ~last + now;
	else
		elapsed = now - last;
	elapsed /= 1000;
	if (wlanAutoTx.frame.interval != 0) {
		remaining = wlanAutoTxGetRemaining();
		if (elapsed >= remaining) {
			wlanAutoTxSetRemaining(0);
			due = 1;
		} else {
			wlanAutoTxSetRemaining(remaining - elapsed);
		}
	}
	__aeabi_memcpy(wlanAutoTxLastTick, &now, 8);
	return due;
}

/* The transmit packet the host would have sent: the descriptor, then the Ethernet frame. */
static void wlanAutoTxBuildFrame(const struct WlanAutoTxFrame *frame, OsaBuffer **result)
{
	struct WlanConnection *connection = wlanConnectionFirst(0);
	OsaBuffer *buffer;
	WlanTxDescriptor *descriptor;
	uint8_t *packet;
	unsigned int length;

	if (frame == NULL || result == NULL || connection == NULL)
		return;
	buffer = osa_BufferAlloc(&wlanBlockPool7, 300, 0);
	*result = buffer;
	if (buffer == NULL)
		return;
	buffer->context = connection;
	buffer->packetClass = 2;
	TX_DATA_TAG(buffer->data) = WLAN_AUTO_TX_TAG;
	buffer->dataOffset = 176;
	descriptor = (WlanTxDescriptor *)((uint8_t *)buffer + 176);
	descriptor->header.wire.bssType = connection->bssType;
	descriptor->header.wire.bssNumber = connection->bssNumber;
	descriptor->control = 0;
	descriptor->header.wire.packetOffset[0] = 15;
	descriptor->header.wire.packetOffset[1] = 0;
	length = frame->frameLength;
	descriptor->header.wire.packetLength[0] = length & 0xff;
	descriptor->header.wire.packetLength[1] = length >> 8;
	packet = (uint8_t *)descriptor + 15;
	if (length < 14) {
		__aeabi_memclr(packet + length, 14 - length);
		descriptor->header.wire.packetLength[0] = 14;
		descriptor->header.wire.packetLength[1] = 0;
	}
	descriptor->priority = wlanWmmEnabled() ? frame->priority : 0;
	descriptor->flags &= 0xf0;
	__aeabi_memcpy(packet, frame->destination, length);
	if ((frame->destination[0] | frame->destination[1] | frame->destination[2] |
	     frame->destination[3] | frame->destination[4] | frame->destination[5]) == 0)
		__aeabi_memcpy(packet, connection->localAddress, 6);
	if ((frame->source[0] | frame->source[1] | frame->source[2] |
	     frame->source[3] | frame->source[4] | frame->source[5]) == 0)
		__aeabi_memcpy(packet + 6, connection->interfaceAddress, 6);
}

/* The frame once its time is up, and the timer starts again; NULL while it waits. */
OsaBuffer *wlanAutoTxNextFrame(void)
{
	OsaBuffer *buffer = NULL;

	if (wlanAutoTx.frame.interval != 0 && wlanAutoTxGetRemaining() == 0) {
		wlanAutoTxBuildFrame(&wlanAutoTx.frame, &buffer);
		wlanAutoTxSetRemaining(1000u * wlanAutoTx.frame.interval);
		return buffer;
	}
	return NULL;
}
