/*
 * From the transmit descriptor of a frame to the MAC: the request the
 * hardware reads is built from the descriptor (wlanTxRequestBuild() for a
 * data frame, wlanTxRequestSetup() for one whose request exists), handed
 * to a transmit ring or a window ring, and read back when the frame is done.
 * See robin/txring.h. The file name is inferred.
 */
#include <stddef.h>
#include "robin/coex.h"
#include "robin/ieee80211.h"
#include "robin/mac.h"
#include "robin/macrate.h"
#include "robin/mactx.h"
#include "robin/protection.h"
#include "robin/pspoll.h"
#include "robin/ratectl.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/stapssm.h"
#include "robin/subscribe.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wmm.h"
#include "robin/htba.h"

/* Not converted yet; provisional names. */
uint32_t *wlanPsFormatFrame(struct WlanConnection *connection, unsigned int multicast); /* 0x3d78c */
int macMgmtUpdateDescriptor(void);						/* 0x3bae4 */

/* Fields of the descriptor (the buffer data) the request is made from. */
#define TX_DATA_TYPE(data)	((data)[0])
#define TX_DATA_INFO(data)	((data)[1])
#define TX_DATA_RATE(data)	((data)[2])
#define TX_DATA_LIFETIME(data)	(*(int16_t *)((data) + 4))	/* time units; 0: the longest */
#define TX_DATA_LENGTH(data)	(*(uint16_t *)((data) + 6))
#define TX_DATA_FLAGS(data)	(*(uint32_t *)((data) + 8))
#define TX_DATA_CONTROL(data)	((uint32_t *)((data) + 12))
#define TX_DATA_SEQUENCE(data)	((const uint16_t *)((data) + 16))
#define TX_DATA_SELF(data)	((data) + 88)
#define TX_DATA_TIMESTAMP(data)	(*(uint32_t *)((data) + 100))

/* TX_DATA_FLAGS: the frame goes to the multicast rate table. */
#define TX_DATA_FLAG_MULTICAST	0x20000000u
/* WlanMacTxRequest.flags: the MAC chooses the rate from the table. */
#define MAC_TX_FLAG_RATE_TABLE	0x00001000u
#define MAC_TX_FLAG_PROBE	0x02000000u

/* WlanMacTxRequest.reserved1[1]: not acknowledged (as the MAC reports it). */
#define MAC_TX_DONE_FAILED	0x01
/* WlanMacTxRequest.reserved1[2]: the frame has a lifetime. */
#define MAC_TX_HAS_LIFETIME	0x80

#define REQUEST_FLAGS(request)	__aeabi_uread4((request)->flags)
#define FRAME_LENGTH(frame)	(((frame)[1] << 8) | (frame)[0])

/* The rate set the connection transmits in, with the retry limits the request is given. */
__attribute__((noinline))
int wlanTxRatesInit(struct WlanConnection *connection, uint8_t *data, struct WlanMacTxRequest *request)
{
	return wlanTxRateInit(TX_DATA_CONTROL(data), request, connection->shortRetryLimit,
			      connection->longRetryLimit);
}

/* Whether a frame of this length is long enough to be sent behind an RTS. */
int wlanTxUsesRts(struct WlanConnection *connection, unsigned int length, const uint8_t *peer)
{
	int qos;

	if (connection->interfaceType == WLAN_IF_IBSS || connection->mode == 2)
		qos = wlanPeerQosEnabled(connection, peer);
	else
		qos = wlanWmmEnabled();
	return length + (qos != 0 ? 30 : 28) > (unsigned int)(connection->rtsThreshold[1] << 8 | connection->rtsThreshold[0]);
}

/* Completes the request of a frame that already has one (a management frame or a retry). */
void wlanTxRequestSetup(struct WlanConnection *connection, uint8_t *data)
{
	struct WlanMacTxRequest *request = TX_DATA_REQUEST(data);
	const uint8_t *frame;
	unsigned int rate;
	int rts;

	__aeabi_uwrite4((uint32_t)data, request->buffer);
	frame = (const uint8_t *)TX_DATA_FRAME(data);
	rts = wlanTxUsesRts(connection, FRAME_LENGTH(frame), frame + 6);
	wlanTxRequestInit(request, (const uint8_t *)TX_DATA_FRAME(data), rts);
	request->type = TX_DATA_TYPE(data);
	request->info = TX_DATA_INFO(data);
	if (wlanCoexPsPollForced() && wlanPsPollUsesMacTimer() == 0) {
		wlanTxRatesInit(connection, data, request);
		TX_DATA_RETRIES(data) = request->mode;
		wlanTxControlRateSet(TX_DATA_CONTROL(data), 0);
	}
	if ((connection->interfaceType == WLAN_IF_AMP || connection->interfaceType == WLAN_IF_STATION ||
	     connection->interfaceType == WLAN_IF_AP) &&
	    (((const uint8_t *)__aeabi_uread4(request->frame))[2] >> 4) == 5) {
		__aeabi_uwrite4(REQUEST_FLAGS(request) | MAC_TX_FLAG_PROBE, request->flags);
		request->mode = 3;
		wlanTxControlRateSet(TX_DATA_CONTROL(data), 2);
	} else {
		wlanTxRatesInit(connection, data, request);
	}
	rate = TX_DATA_RATE(data);
	__aeabi_uwrite4(REQUEST_FLAGS(request) & ~MAC_TX_FLAG_RATE_TABLE, request->flags);
	__aeabi_uwrite4((uint32_t)wlanRateTableLookup(rate), request->rateInfo);
	__aeabi_uwrite4((uint32_t)data, request->host);
}

/* How many entries the window ring of a BSS can still take. */
unsigned int wlanTxWindowFreeCount(unsigned int window)
{
	uint32_t head = MAC_REG(0x930 + 8 * window);
	uint32_t tail = MAC_REG(0x934 + 8 * window) & 0x1f;
	unsigned int count = 0;

	while (((head + 4) & 0x1f) != tail) {
		head += 4;
		count++;
	}
	return count;
}

/* Puts a request on a window ring; 0 when the ring is full. */
int wlanTxWindowPush(struct WlanMacTxRequest *request, unsigned int length, unsigned int window)
{
	unsigned int posture;
	int result = 1;
	uint32_t head;

	posture = sysIrqDisable();
	if (length != 12) {
		for (;;)
			;
	}
	head = MAC_REG(0x930 + 8 * window);
	if (((head + 4) & 0x1f) == (MAC_REG(0x934 + 8 * window) & 0x1f)) {
		result = 0;
	} else {
		REG32(SOC_MAC_REGISTER_BASE | 0xc00 | (head & 0x1f) | (32 * window)) = (uint32_t)request;
		MAC_REG(0x930 + 8 * window) = (head + 4) & 0x3f;
	}
	sysIrqRestore(posture);
	return result;
}

/* Builds the request of a frame and puts it on the window ring of its BSS. */
int wlanTxWindowSubmit(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;

	if (TX_DATA_FRAME(data) == 0)
		return 0;
	if (connection == NULL) {
		sysIrqDisable();
		for (;;)
			;
	}
	wlanTxRequestSetup(connection, data);
	wlanTxInFlightAdd(buffer);
	if (wlanTxWindowPush(TX_DATA_REQUEST(data), TX_DATA_RING(data), connection->groupRing) != 0)
		return 1;
	wlanTxInFlightRemove(buffer);
	return 0;
}

/* The same for a frame whose request is made. */
int wlanTxWindowResubmit(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;

	wlanTxInFlightAdd(buffer);
	if (wlanTxWindowPush(TX_DATA_REQUEST(data), TX_DATA_RING(data), connection->groupRing) == 0) {
		wlanTxInFlightRemove(buffer);
		return 0;
	}
	return 1;
}

/* Sends a frame through its ring, waiting a tick at a time while the ring is full. */
int wlanTxReleaseFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
	unsigned int ring;

	if (connection == NULL) {
		sysIrqDisable();
		for (;;)
			;
	}
	if (TX_DATA_FRAME(data) == 0)
		return 0;
	wlanTxRequestSetup(connection, data);
	if ((((const uint8_t *)__aeabi_uread4(TX_DATA_REQUEST(data)->frame))[2] >> 4) == 9)
		ring = 1;
	else
		ring = TX_DATA_RING(data);
	for (;;) {
		wlanTxInFlightAdd(buffer);
		if (wlanTxCheckRing(connection, TX_DATA_REQUEST(data), ring) != 0)
			break;
		wlanTxInFlightRemove(buffer);
		osaTaskSleep(1);
	}
	return 1;
}

/* Gives the request the rate (or the rate table) its frame goes out at. */
void wlanTxRateSelect(struct WlanConnection *connection, uint8_t *data, struct WlanMacTxRequest *request)
{
	unsigned int rate = TX_DATA_RATE(data);

	if (REQUEST_FLAGS(request) & MAC_TX_FLAG_RATE_TABLE) {
		int converted = wlanHtGreenfieldWanted(connection);
		int verified = wlanHtShortGiWanted(connection, rate);

		wlanTxRequestRateSet(request, rate, verified, converted, (wlanProtection.flags >> 5) & 1);
	} else {
		__aeabi_uwrite4((uint32_t)wlanPsFormatFrame(connection, (TX_DATA_FLAGS(data) >> 29) & 1),
				request->rateInfo);
	}
}

/* Starts the clock of the frame's lifetime. */
void wlanTxPrepareRequest(uint8_t *data, struct WlanMacTxRequest *request)
{
	uint64_t now = romGetTimerTimestamp();

	TX_DATA_TIMESTAMP(data) = (uint32_t)now;
	wlanTxLifetimeSet(request, now, TX_DATA_LIFETIME(data));
	if (TX_DATA_LIFETIME(data) != 0)
		request->reserved1[2] |= MAC_TX_HAS_LIFETIME;
}

/* Builds the transmit request of a data frame from its descriptor. */
int wlanTxRequestBuild(struct WlanConnection *connection, uint8_t *data)
{
	struct WlanMacTxRequest *request = TX_DATA_REQUEST(data);
	uint32_t frame;

	__aeabi_memclr(request, sizeof(*request));
	__aeabi_uwrite4((uint32_t)data, request->buffer);
	frame = TX_DATA_FRAME(data);
	if (frame == 0)
		return 0;
	__aeabi_uwrite4(frame, request->frame);
	request->hwStatus = 0;
	request->type = TX_DATA_TYPE(data);
	request->info = TX_DATA_INFO(data);
	request->rate = (request->rate & 0xfc07) | ((TX_DATA_RATE(data) * 8) & 0x3ff);
	__aeabi_uwrite4(TX_DATA_FLAGS(data) & 0x0fffffff, request->flags);
	request->length = TX_DATA_LENGTH(data);
	wlanTxPrepareRequest(data, request);
	if (((((const uint8_t *)__aeabi_uread4(request->frame))[2] >> 2) & 3) == 1)
		wlanTxControlRateSet(TX_DATA_CONTROL(data), 0);
	if (wlanPsTriggerEnabled() && wlanCoexUseP2pSleepMode()) {
		wlanTxRatesInit(connection, data, request);
		TX_DATA_RETRIES(data) = request->mode;
		wlanTxControlRateSet(TX_DATA_CONTROL(data), 0);
	}
	wlanTxRatesInit(connection, data, request);
	wlanTxRateSelect(connection, data, request);
	request->sequence[0] = TX_DATA_SEQUENCE(data)[0];
	request->sequence[1] = TX_DATA_SEQUENCE(data)[1];
	__aeabi_uwrite4((uint32_t)data, request->host);
	return 1;
}

/*
 * Reads the next frame the MAC has finished into status and returns its
 * buffer; the result stays 1 (not acknowledged) when nothing is waiting.
 */
OsaBuffer *wlanTxCompletionFetch(struct WlanTxStatus *status)
{
	OsaBuffer *buffer = NULL;
	unsigned int posture;
	uint32_t tail;

	status->result = 1;
	status->data = NULL;
	posture = sysIrqDisable();
	tail = MAC_014_QUEUE_TAIL;
	if ((MAC_010_QUEUE_HEAD & 0x1ff) != (tail & 0x1ff)) {
		struct WlanMacTxRequest *done = *(struct WlanMacTxRequest **)((uint8_t)tail | SOC_MAC_REGISTER_BASE | 0xd00);

		if (done != NULL) {
			uint8_t *data = (uint8_t *)__aeabi_uread4(done->buffer);
			struct WlanConnection *connection;
			uint32_t flags;

			status->data = data;
			buffer = (OsaBuffer *)(data - 28);
			connection = buffer->context;
			flags = __aeabi_uread4(done->flags);
			status->flags = flags;
			if ((done->reserved1[1] & MAC_TX_DONE_FAILED) == 0)
				status->result = 0;
			if ((TX_DATA_FLAGS(data) & TX_DATA_FLAG_MULTICAST) == 0) {
				uint32_t rateInfo;

				status->type = done->mode;
				status->info = done->reserved1[0];
				if (flags & MAC_TX_FLAG_RATE_TABLE) {
					uint8_t rate = romLibAcquireState(done->rate & 1, (done->rate >> 2) & 1,
									  (done->rate >> 3) & 0x7f);

					status->rate = rate;
					status->finalRate = rate;
					status->reserved22 = ((const uint8_t *)done)[42] & 0xf;
					rateInfo = 0;
				} else {
					rateInfo = __aeabi_uread4(done->rateInfo);
				}
				status->rateInfo = rateInfo;
				if (done->reserved1[1] & MAC_TX_DONE_FAILED) {
					macMgmtUpdateDescriptor();
				} else {
					const uint8_t *sent = (const uint8_t *)TX_DATA_FRAME((uint8_t *)__aeabi_uread4(done->buffer));

					status->result = 0;
					macMgmtUpdateDescriptor();
					if (((sent[2] >> 2) & 3) == 2) {
						uint64_t start = __aeabi_uread4(&done->expiry[0]) |
								 (uint64_t)__aeabi_uread4(&done->expiry[4]) << 32;

						wlanSubscribeCheckLinkLatency(connection, start);
					}
				}
			}
		}
		MAC_014_QUEUE_TAIL = (tail + 4) & 0x1ff;
	}
	sysIrqRestore(posture);
	return buffer;
}
