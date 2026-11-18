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

#define WLAN_AUTO_TX_RECORDS	1	/* wlanAutoTxTick() runs over them */

static struct WlanAutoTxRecord wlanAutoTx[WLAN_AUTO_TX_RECORDS] __attribute__((section(".itcm.bss")));	/* 0x5745e */
static struct WlanAutoTxRecord *wlanAutoTxActive __attribute__((section(".itcm.bss")));	/* 0x574e0 */
static uint8_t wlanAutoTxLastTick[8] __attribute__((section(".itcm.bss")));		/* 0x574e4 */

/* The unaligned 64-bit counts are copied out at every use, a copy each. */
static inline uint64_t wlanAutoTxGet64(const uint8_t *bytes)
{
	uint64_t value;

	__aeabi_memcpy(&value, bytes, 8);
	return value;
}

static inline void wlanAutoTxSet64(uint8_t *bytes, uint64_t value)
{
	__aeabi_memcpy(bytes, &value, 8);
}

/* The host command: a frame to send every interval, or, without a body, none. */
unsigned int hostCmdAutoTxSet(const uint8_t *body)
{
	if (body == NULL) {
		wlanAutoTx[0].frame.interval = 0;
		wlanAutoTxActive = NULL;
		return 0;
	}
	if ((uint16_t)(body[2] | body[3] << 8) == WLAN_AUTO_TX_TLV_TYPE) {
		__aeabi_memcpy(&wlanAutoTx[0].frame, body, (uint16_t)(body[4] | body[5] << 8) + 6);
		wlanAutoTxSet64(wlanAutoTx[0].remaining, 1000 * wlanAutoTx[0].frame.interval);
		wlanAutoTxActive = &wlanAutoTx[0];
		return 0;
	}
	return 1;
}

unsigned int hostCmdAutoTxGet(uint8_t *body)
{
	if (body == NULL)
		return 1;
	if (wlanAutoTx[0].frame.interval != 0) {
		__aeabi_memcpy(body, &wlanAutoTx[0].frame, wlanAutoTx[0].frame.length + 6);
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
	wlanAutoTx[0].frame.interval = 0;
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
	static const uint8_t expired[8];
	int due = 0;
	unsigned int i;
	uint64_t now = romGetTimerTimestamp();
	uint64_t elapsed;

	if (now >= wlanAutoTxGet64(wlanAutoTxLastTick))
		elapsed = now - wlanAutoTxGet64(wlanAutoTxLastTick);
	else
		elapsed = ~wlanAutoTxGet64(wlanAutoTxLastTick) + now;
	elapsed /= 1000;
	for (i = 0; i < WLAN_AUTO_TX_RECORDS; i++) {
		struct WlanAutoTxRecord *record = &wlanAutoTx[i];

		if (record->frame.interval == 0)
			continue;
		if (elapsed >= wlanAutoTxGet64(record->remaining)) {
			__aeabi_memcpy(record->remaining, expired, 8);
			due = 1;
		} else {
			wlanAutoTxSet64(record->remaining, wlanAutoTxGet64(record->remaining) - elapsed);
		}
	}
	wlanAutoTxSet64(wlanAutoTxLastTick, now);
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
	(*result)->context = connection;
	(*result)->packetClass = 2;
	(*result)->dataOffset = sizeof(OsaBuffer);
	TX_DATA_TAG((*result)->data) = WLAN_AUTO_TX_TAG;
	(*result)->dataOffset = 176;
	descriptor = (WlanTxDescriptor *)((uint8_t *)*result + 176);
	descriptor->header.wire.bssType = 0;
	descriptor->header.wire.bssType = connection->bssType;
	descriptor->header.wire.bssNumber = connection->bssNumber;
	packet = (uint8_t *)descriptor + 15;
	__aeabi_uwrite4(0, &descriptor->control);
	descriptor->header.wire.packetOffset[0] = 15;
	descriptor->header.wire.packetOffset[1] = 0;
	length = frame->frameLength;
	descriptor->header.wire.packetLength[0] = length & 0xff;
	descriptor->header.wire.packetLength[1] = length >> 8;
	if (length < 14) {
		__aeabi_memclr(packet + frame->frameLength, 14 - frame->frameLength);
		descriptor->header.wire.packetLength[0] = 14;
		descriptor->header.wire.packetLength[1] = 0;
	}
	/* Without WMM the descriptor keeps the priority the buffer had. */
	if (wlanWmmEnabled())
		descriptor->priority = frame->priority;
	descriptor->flags &= 0xf0;
	__aeabi_memcpy(packet, frame->destination, frame->frameLength);
	if (frame->destination[0] == 0 && frame->destination[1] == 0 && frame->destination[2] == 0 &&
	    frame->destination[3] == 0 && frame->destination[4] == 0 && frame->destination[5] == 0)
		__aeabi_memcpy(packet, connection->localAddress, 6);
	if (frame->source[0] == 0 && frame->source[1] == 0 && frame->source[2] == 0 &&
	    frame->source[3] == 0 && frame->source[4] == 0 && frame->source[5] == 0)
		__aeabi_memcpy(packet + 6, connection->interfaceAddress, 6);
}

/* The frame once its time is up, and the timer starts again; NULL while it waits. */
OsaBuffer *wlanAutoTxNextFrame(void)
{
	OsaBuffer *buffer = NULL;

	if (wlanAutoTx[0].frame.interval != 0 && wlanAutoTxGet64(wlanAutoTx[0].remaining) == 0) {
		wlanAutoTxBuildFrame(&wlanAutoTx[0].frame, &buffer);
		wlanAutoTxSet64(wlanAutoTx[0].remaining, 1000 * wlanAutoTx[0].frame.interval);
		return buffer;
	}
	return NULL;
}
