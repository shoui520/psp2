/*
 * The TSPEC sessions of a connection (0x41436 - 0x41cb8): the traffic streams the host asks for
 * (HostCmd 0x6e), the ADDTS request that goes out in a time slot of its own, the answer that is
 * told to the host, and the medium time the admitted streams give to the queues. The names of the
 * firmware's placeholders are kept; see robin/tspec.h.
 */
#include <stddef.h>
#include "robin/apsta.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/mactx.h"
#include "robin/p2pgo.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/slot.h"
#include "robin/sync.h"
#include "robin/task.h"
#include "robin/tspec.h"
#include "robin/txring.h"
#include "robin/wlantask.h"
#include "robin/wmm.h"

extern int memcmp(const void *left, const void *right, size_t size);
extern void __aeabi_memclr(void *destination, size_t length);
extern void __aeabi_memclr4(void *destination, size_t length);
extern void __aeabi_memcpy(void *destination, const void *source, size_t length);
extern struct OsaBufferPool wlanReplyFramePool;			/* 0x4000e84 */

struct WlanAddtsPending wlanAddtsPending __attribute__((section(".itcm.bss")));	/* 0x59d44 */
uint8_t wlanTspecFlags __attribute__((section(".itcm.rw")));				/* 0x55414 */

#define WLAN_ACTION_CATEGORY_WMM	17
#define WLAN_WMM_ACTION_ADDTS_RESPONSE	1
#define WLAN_WMM_ACTION_DELTS		2
#define TSPEC_LENGTH			55
#define TSPEC_TSID(tspec)		(((tspec)[0] >> 1) & 0x0f)
#define TSPEC_USER_PRIORITY(tspec)	(((tspec)[1] >> 3) & 0x07)

/* Adds (mode 1, active; mode 2, suspended) or removes (mode 0) the TSPEC. Whether anything changed. */
int p2pDirectBuildEntry(const uint8_t *tspec, int mode)
{
	unsigned int time = p2pDirectConfigureEntry();
	unsigned int tsid = TSPEC_TSID(tspec);
	unsigned int priority = TSPEC_USER_PRIORITY(tspec);
	unsigned int count;
	int changed = 0;
	int flowBefore;
	int sessionBefore;
	uint8_t directions;
	int timeChanged;
	int flowChanged;
	int sessionChanged;

	count = p2pDirectValidateEntry(NULL);
	if (tsid >= 8)
		return 0;
	flowBefore = wlanWmm.infoIe[0][WLAN_WMM_IE_QOS_INFO];
	sessionBefore = wlanWmm.infoIe[2][WLAN_WMM_IE_QOS_INFO];
	if (mode == 1 || mode == 2) {
		struct WlanWmmQueue *queue;
		struct WlanWmmSlot *session;
		struct WlanWmmSlot *slot;
		unsigned int i;

		wlanTspecDirection(tspec, &directions);
		queue = &wlanWmm.queues[wlanAccessCategoryToQueue(wlanTidToAccessCategory(priority))];
		for (i = 0; i < 2; i = (uint8_t)(i + 1)) {
			slot = queue->slots[i];
			if (slot->directions & directions) {
				unsigned int old = TSPEC_TSID(slot->tspec);

				p2pDirectPrepareEntry(wlanWmm.sessions[old], 0);
				p2pDirectDispatchEntry(old);
				changed = 1;
			}
		}
		session = wlanWmm.sessions[tsid];
		if (session != NULL) {
			p2pDirectPrepareEntry(session, 0);
			p2pDirectDispatchEntry(tsid);
			changed = 1;
		}
		slot = &wlanWmm.slots[priority];
		wlanWmm.sessions[tsid] = slot;
		__aeabi_memcpy(slot, tspec, TSPEC_LENGTH);
		wlanWmm.sessions[tsid]->directions = directions;
		p2pDirectPrepareEntry(wlanWmm.sessions[tsid], mode);
		wlanWmm.sessions[tsid]->state = mode == 2 ? 5 : 4;
	} else {
		struct WlanWmmSlot *session = wlanWmm.sessions[tsid];

		if (session != NULL) {
			p2pDirectPrepareEntry(session, mode);
			p2pDirectDispatchEntry(tsid);
		}
	}
	timeChanged = p2pDirectConfigureEntry() != time;
	flowChanged = (uint8_t)memcmp(&flowBefore, &wlanWmm.infoIe[0][WLAN_WMM_IE_QOS_INFO], 1);
	sessionChanged = (uint8_t)memcmp(&sessionBefore, &wlanWmm.infoIe[2][WLAN_WMM_IE_QOS_INFO], 1);
	if (timeChanged) {
		unsigned int queueIndex;

		p2pDirectCheckEntry();
		for (queueIndex = 0; queueIndex < 4; queueIndex = (uint8_t)(queueIndex + 1))
			wlanWmmApplyQueue(queueIndex);
		if (wlanWmm.policingTimer == 0)
			p2pDirectParseEntry(0, 0);
	}
	return p2pDirectValidateEntry(NULL) != (int)count ||
	       (changed | timeChanged | flowChanged | sessionChanged) != 0;
}

/* An action frame of the connection towards the peer, for an ADDTS request or a DELTS. */
int p2pDirectComputeEntry(OsaBuffer **buffer)
{
	OsaBuffer *frame = wlanMgmtAllocFrame(wlanAddtsPending.connection, 300);
	struct WlanConnection *connection;
	WlanWirelessFrame *wireless;

	*buffer = frame;
	if (frame != NULL) {
		wireless = (WlanWirelessFrame *)((uint8_t *)frame + frame->dataOffset);
		connection = wlanAddtsPending.connection;
		wlanMgmtBuildFrame(frame, connection->interfaceAddress, connection->peerAddress, 13,
				   connection->localAddress);
		wireless->body[0] = WLAN_ACTION_CATEGORY_WMM;
		(*buffer)->dataOffset = 176;
		return 176;
	}
	return 0;
}

/* Sends the ADDTS request for the TSPEC (and the host's elements after it). */
int p2pDirectQueryEntry(const uint8_t *tspec, unsigned int length, const uint8_t *tail, unsigned int tailLength)
{
	OsaBuffer *buffer;
	uint8_t *frame;
	uint8_t *body;

	p2pDirectComputeEntry(&buffer);
	if (buffer == NULL)
		return -1;
	frame = (uint8_t *)buffer + buffer->dataOffset;
	if (++wlanAddtsPending.dialogToken == 0)
		wlanAddtsPending.dialogToken = 1;
	body = wlanTspecBuildAddts(buffer, tspec, length, wlanAddtsPending.dialogToken);
	if (tailLength != 0) {
		unsigned int size;

		__aeabi_memcpy(body + length + 12, tail, tailLength);
		size = ((frame[1] << 8) | frame[0]) + tailLength;
		frame[0] = size;
		frame[1] = size >> 8;
	}
	if ((wlanTspecFlags & WLAN_TSPEC_SEND_ALWAYS) == 0) {
		unsigned int bit = 1 << wlanTspecUapsdBit(TSPEC_USER_PRIORITY(body + 12));

		if (wlanWmm.infoIe[0][WLAN_WMM_IE_QOS_INFO] & bit)
			body[13] |= 4;
		else
			body[13] &= 0xfb;
	}
	return wlanMgmtSendFrame(buffer);
}

/* Whether the access category needs admission control. */
__attribute__((noinline)) int p2pDirectSelectEntry(unsigned int category)
{
	return wlanWmm.queues[wlanAccessCategoryToQueue(category)].edca.acm;
}

__attribute__((noinline)) int p2pDirectAdjustEntry(uint8_t tid)
{
	return p2pDirectSelectEntry(wlanTidToAccessCategory(tid));
}

/* Whether any access category needs admission control. */
int p2pDirectResetEntry(void)
{
	unsigned int category = 0;

	do {
		if (p2pDirectSelectEntry(category) != 0)
			return 1;
		category = (uint8_t)(category + 1);
	} while (category <= 3);
	return 0;
}

/* Sends the requests that wait (from the last slot down), at most count of them. */
int p2pDirectSyncEntry(int count)
{
	int sent = 0;
	int slot;

	for (slot = 7; slot >= 0; --slot) {
		struct WlanWmmSlot *session = &wlanWmm.slots[slot];

		if (session->state != 1)
			continue;
		if ((wlanTspecFlags & WLAN_TSPEC_SEND_ALWAYS) == 0 && p2pDirectAdjustEntry(slot) == 0)
			continue;
		p2pDirectQueryEntry(session->tspec, TSPEC_LENGTH, session->tail, session->tailLength);
		session->state = 2;
		if (++sent >= count)
			break;
	}
	return (uint8_t)sent;
}

int p2pDirectApplyEntry(void)
{
	return p2pDirectSyncEntry(1) == 0;
}

/* Tells the host the result of a request: the response (NULL: none came). */
int p2pDirectResolveEntry(const uint8_t *response, int length)
{
	OsaBuffer *message;

	do {
		message = osa_BufferAlloc(&wlanReplyFramePool, 2000, 0);
		osaTaskSleep(1);
	} while (message == NULL);
	message->context = wlanAddtsPending.connection;
	message->dataOffset = 176;
	__aeabi_memclr((uint8_t *)message + 176, 0x69);
	wlanWmmAddtsResult((uint8_t *)message + 176, response, length);
	if (osaMsgQueueSend(*wlanTaskHandles.cbSmeQueue, 4, (uintptr_t)message, 0) != 0)
		osa_BufferFree(message);
	else
		wlanPostConnectionEvent(wlanAddtsPending.connection, 128);
	return 1;
}

/* Tells the host the WMM status (the queues' parameters) and, with a TSPEC response, the response. */
int p2pDirectScheduleEntry(struct WlanConnection *connection, const uint8_t *source, const uint8_t *destination,
			   const uint8_t *body)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *event;
	uint8_t *tlv;

	if (wlanTxAllocFrame(&buffer, (struct WlanHostEventFrame **)&event, 0x190, WLAN_HOST_EVENT_WMM_STATUS) == 0)
		return 0;
	event->length = wlanWmmQueueStatus(connection, (uint8_t *)&event->code);
	tlv = (uint8_t *)event + 72;
	if (source != NULL)
		__aeabi_memcpy(tlv + 4, source, 6);
	if (destination != NULL)
		__aeabi_memcpy(tlv + 10, destination, 6);
	if (body != NULL) {
		tlv[0] = 0x8c;
		tlv[1] = 0x01;
		tlv[2] = 0x4f;
		tlv[3] = 0x01;
		__aeabi_memcpy(tlv + 16, body, 0x143);
		event->length += 339;
	}
	return wlanHostEventSend(connection, buffer);
}

/* An ADDTS response (the action 1, the status 0 means accepted) or a DELTS of the peer. */
int p2pDirectFormatEntry(struct WlanConnection *connection, const uint8_t *body, const uint8_t *frame)
{
	const uint8_t *tspec = body + 12;
	const uint8_t *peer = frame + 12;
	int mode;

	if (body[1] == WLAN_WMM_ACTION_ADDTS_RESPONSE) {
		if (body[3] != 0)
			return 1;
		wlanAddtsPending.dialogToken = 0;
		mode = 1;
	} else if (body[1] == WLAN_WMM_ACTION_DELTS) {
		mode = 0;
	} else {
		return 1;
	}
	if (p2pDirectBuildEntry(tspec, mode))
		p2pDirectScheduleEntry(wlanAddtsPending.connection, peer, frame + 6, body);
	return 1;
}

/* The streams the radio has lost (the connection went away) ask for their admission again. */
unsigned int p2pDirectTrackEntry(void)
{
	unsigned int slot;

	for (slot = 0; slot < 8; ++slot) {
		struct WlanWmmSlot *session = wlanWmm.sessions[slot];

		if (session != NULL && session->state == 4) {
			session->state = 1;
			session->mediumTime[0] = 0;
			session->mediumTime[1] = 0;
		}
	}
	return slot;
}

/* The admitted streams are suspended. */
unsigned int p2pDirectManageEntry(void)
{
	unsigned int slot;

	for (slot = 0; slot < 8; ++slot) {
		struct WlanWmmSlot *session = wlanWmm.sessions[slot];

		if (session != NULL && session->state == 4)
			session->state = 5;
	}
	return slot;
}

/* The suspended streams are admitted again. Whether there were any. */
int p2pDirectAcquireEntry(void)
{
	int resumed = 0;
	unsigned int slot;

	for (slot = 0; slot < 8; ++slot) {
		struct WlanWmmSlot *session = wlanWmm.sessions[slot];

		if (session != NULL && session->state == 5) {
			resumed = 1;
			p2pDirectPrepareEntry(session, 1);
			session->state = 4;
		}
	}
	p2pDirectCheckEntry();
	return resumed;
}

/* Whether the associated connection has a request to send (the sleep waits for it). */
int p2pDirectReleaseEntry(void)
{
	uint8_t toRequest = 0;

	if ((wlanAddtsPending.connection->flags2 & WLAN_CONN2_ASSOCIATED) == 0)
		return 0;
	if (wlanWmmEnabled() != 0)
		p2pDirectValidateEntry(&toRequest);
	return toRequest != 0;
}

/* The time slot of the request: what it does at each of its events. */
int p2pDirectSetupEntry(uint32_t handle, int event, void (*completion)(int result, int detail))
{
	switch (event) {
	case 3:
		wlanAddtsPending.dialogToken++;
		macTxSetPowerSave(0);
		if (wlanAddtsPending.dialogToken == 0)
			wlanAddtsPending.dialogToken = 1;
		if (wlanAddtsPending.pending != 0) {
			p2pDirectQueryEntry(wlanAddtsPending.tspec, TSPEC_LENGTH, wlanAddtsPending.tail,
					    wlanAddtsPending.tailLength);
			wlanAddtsPending.pending = 0;
			return 3;
		}
		if (p2pDirectSyncEntry(1) != 0)
			return 3;
		break;
	case 4:
		if (wlanAddtsPending.slot != handle)
			return 4;
		if (completion != NULL)
			completion(0, 0);
		break;
	case 6:
		p2pDirectAcquireEntry();
		p2pDirectScheduleEntry(wlanAddtsPending.connection, NULL, NULL, NULL);
		return 3;
	default:
		return 3;
	}
	wlanSlotAbort(handle, 6);
	wlanAddtsPending.slot = 0;
	return 3;
}

/* A frame or a transmit status in the request's slot: the ADDTS response takes the request's place. */
int p2pDirectClearEntry(uint32_t handle, const struct WlanTxStatus *status, const OsaBuffer *buffer,
			uint32_t second, uint32_t third, int (*result)(const uint8_t *body, int length))
{
	int consumed = 0;

	if (buffer != NULL) {
		const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;

		if ((frame[2] >> 4) == 13 && frame[32] == WLAN_ACTION_CATEGORY_WMM &&
		    frame[33] == WLAN_WMM_ACTION_ADDTS_RESPONSE) {
			/* the response is consumed; the status is looked at all the same (IDA 0x41ac6) */
			consumed = 1;
			if (frame[35] == 0) {
				if (!p2pDirectBuildEntry(frame + 44, 1))
					goto status;
				p2pDirectScheduleEntry(wlanAddtsPending.connection, frame + 12, frame + 6, frame + 32);
			}
			if (result != NULL && result(frame + 32, (frame[1] << 8) | frame[0]) != 0) {
				wlanSlotAbort(wlanAddtsPending.slot, 6);
				wlanAddtsPending.slot = 0;
			}
		}
	}
status:
	if (status != NULL) {
		unsigned int control = ((const uint8_t *)(uintptr_t)TX_DATA_FRAME(status->data))[2];

		if (((control >> 2) & 3) == 0 && (control >> 4) == 13)
			return 1;
	}
	return consumed;
}

/* The request of the host: the TSPEC, in a time slot of its own or at once. */
int hostCmd006e(struct WlanConnection *connection, char *body, int length)
{
	unsigned int tsid = TSPEC_TSID((uint8_t *)body + 15);
	struct WlanSlotParams params;
	int result;

	if (wlanAddtsPending.slot != 0) {
		body[0] = 1;
		return -1;
	}
	if (tsid >= 8) {
		body[0] = 3;
		return -1;
	}
	if ((connection->flags2 & WLAN_CONN2_ASSOCIATED) != 0 && wlanWmmEnabled() != 0 &&
	    ((wlanTspecFlags & WLAN_TSPEC_SEND_ALWAYS) != 0 ||
	     p2pDirectAdjustEntry(TSPEC_USER_PRIORITY((uint8_t *)body + 15)) != 0)) {
		__aeabi_memclr4(&params, 0x38);
		params.callback = (int (*)(uint32_t, int, uint32_t))p2pDirectSetupEntry;
		params.context = (uint32_t)(uintptr_t)p2pDirectClearEntry;
		params.argument = (uint32_t)(uintptr_t)p2pDirectResolveEntry;
		params.duration = 1000 * __aeabi_uread4(body + 1);
		params.flags |= 6;
		params.connection = connection;
		wlanAddtsPending.connection = connection;
		__aeabi_memcpy(wlanAddtsPending.tspec, body + 15, TSPEC_LENGTH);
		wlanAddtsPending.tailLength = length - 70;
		if ((uint8_t)length != 70)
			__aeabi_memcpy(wlanAddtsPending.tail, body + 70, (uint8_t)(length - 70));
		result = 1;
		wlanAddtsPending.pending = 1;
		wlanAddtsPending.slot = wlanSlotSubmit(&params);
		if (wlanAddtsPending.slot == 0)
			result = -1;
	} else if (p2pDirectBuildEntry((uint8_t *)body + 15, 1)) {
		wlanWmm.sessions[tsid]->state = 1;
		body[0] = 4;
		result = 0;
	} else {
		body[0] = 3;
		result = -1;
	}
	if (result == -1)
		body[0] = 1;
	return result;
}

/* The connection joined: the streams that wait are requested in a slot, or the status told to the host. */
void p2pDirectVerifyEntry(struct WlanConnection *connection)
{
	struct WlanSlotParams params;
	uint8_t toRequest = 0;
	unsigned int count;

	if (!(connection->flags2 & WLAN_CONN2_ASSOCIATED) || wlanWmmEnabled() == 0)
		return;
	if (p2pDirectResetEntry() == 0 && (wlanTspecFlags & WLAN_TSPEC_SEND_ALWAYS) == 0)
		return;
	p2pDirectValidateEntry(&toRequest);
	count = toRequest;
	if (count != 0) {
		__aeabi_memclr4(&params, 0x38);
		__aeabi_memclr4(&wlanAddtsPending, 0x60);
		params.callback = (int (*)(uint32_t, int, uint32_t))p2pDirectSetupEntry;
		params.context = (uint32_t)(uintptr_t)p2pDirectClearEntry;
		params.argument = (uint32_t)(uintptr_t)p2pDirectApplyEntry;
		params.connection = connection;
		params.duration = 250000 * count;
		wlanAddtsPending.slot = wlanSlotSubmit(&params);
		wlanAddtsPending.connection = connection;
	} else if (p2pDirectAcquireEntry() != 0) {
		p2pDirectScheduleEntry(connection, NULL, NULL, NULL);
	}
}

/* The status of a stream for the host (HostCmd 0x5d). */
unsigned int hostCmd005d(uint8_t *body)
{
	struct WlanWmmSlot *session = wlanWmm.sessions[body[0] & 7];
	const uint8_t *directions = NULL;
	int valid = 0;

	if (session != NULL) {
		valid = session->state == 4;
		directions = &session->directions;
	}
	return wlanWmmTsStatus(body, (const uint8_t *)session, directions, valid);
}

/* The host removes a stream: the DELTS goes out when the connection is associated. */
int hostCmd006f(uint8_t *body, int length)
{
	int result = 0;
	OsaBuffer *buffer;

	p2pDirectBuildEntry(body + 11, 0);
	body[0] = 4;
	if ((wlanAddtsPending.connection->flags2 & WLAN_CONN2_ASSOCIATED) != 0 && wlanWmmEnabled() != 0) {
		unsigned int tspecLength = body[2];	/* HostCmd_DS_WMM_DELTS_REQ: result, reason, TSPEC length, TSPEC */

		p2pDirectComputeEntry(&buffer);
		if (buffer != NULL) {
			wlanTspecBuildDelts(buffer, body + 3, tspecLength);
			result = wlanMgmtSendFrame(buffer);
		} else {
			result = -1;
		}
		if (result == 0)
			body[0] = 0;
		p2pDirectScheduleEntry(wlanAddtsPending.connection, NULL, NULL, NULL);
	}
	return result;
}
