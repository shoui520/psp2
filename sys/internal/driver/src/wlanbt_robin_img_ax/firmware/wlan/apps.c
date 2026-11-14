/*
 * Frames an access point keeps for peers in power save (0x12918..0x131d4).
 * A frame for a dozing peer goes into one of its queues (the U-APSD queue
 * when its access category is delivery-enabled) and sets its bit in the
 * TIM; a PS-Poll or a trigger frame sends the frames again, the More Data
 * bit telling the peer whether more wait. Group-addressed frames wait while
 * any peer dozes and go out after the DTIM beacon. The stations share 22
 * entries, each limited to its quota. The filename is inferred; no source
 * filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/apps.h"
#include "robin/conn.h"
#include "robin/list.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/pspoll.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlanrx.h"
#include "robin/wlan.h"

#define LE16(bytes)	((uint16_t)((bytes)[0] | ((bytes)[1] << 8)))

/* Frame control: the type bits of byte 0, the More Data and Order bits of byte 1. */
#define WLAN_FC_TYPE(fc)		(((fc) >> 2) & 3)
#define WLAN_FC_SUBTYPE(fc)		((fc) >> 4)
#define WLAN_FC1_MORE_DATA		0x20
#define WLAN_FC1_ORDER			0x80
#define WLAN_SUBTYPE_PROBE_RESPONSE	5

/* The rings that send group frames after the DTIM beacon, and their flag. */
#define WLAN_TX_RING_GROUP		12

/* TX_DATA_TAG: the last group frame after a DTIM beacon; a service period's frames. */
#define WLAN_TX_TAG_LAST_GROUP		20
#define WLAN_TX_TAG_SP_LAST		49
#define WLAN_TX_TAG_SP			50

/* QoS Control: end of service period. */
#define WLAN_QOS_EOSP			0x10
#define WLAN_FC1_POWER_MGMT		0x10
#define WLAN_FC_SUBTYPE_QOS		0x80
#define WLAN_TYPE_DATA			2

/* The host event that reports a peer whose buffers are full. */
#define WLAN_EVENT_PS_BUFFER_FULL	85
#define TLV_TYPE_PS_BUFFER_FULL		0x0194

/* wpaSuppPrepareDescriptor(): an EAPOL frame. */
#define WLAN_FRAME_CLASS_EAPOL		3

/* What the host is told when a peer's buffers are full. */
struct WlanApPsFullEvent {
	uint16_t type;
	uint16_t length;
	uint8_t peer[6];
	uint8_t reported;
	uint8_t buffered;
};

struct WlanApPsLimits wlanApPsLimits __attribute__((section(".itcm.rw"))) = {
	.reportFull = 1,
	.maxBuffered = WLAN_AP_PS_ENTRIES,
};

static struct WlanApPsEntry wlanApPsEntries[WLAN_AP_PS_ENTRIES] __attribute__((section(".itcm.bss")));
static struct RobinList wlanApPsFree __attribute__((section(".itcm.bss")));
static struct RobinList wlanApPsUsed __attribute__((section(".itcm.bss")));

/* Not converted yet; provisional names. */
extern uint16_t wlanMgmtFrameSize;					/* 0x54b8e */
void macMgmtInspectPacket(struct WlanConnection *connection, unsigned int aid, int set); /* 0x3a06e */
unsigned int macTxNotifyParseTable(unsigned int ring);			/* 0x318cc */
int macTxNotifyQueryTable(OsaBuffer *buffer);				/* 0x3199e */
int macTxNotifyComputeTable(OsaBuffer *buffer);				/* 0x31952 */
void macTxApplyConfig(struct WlanConnection *connection);		/* 0x13978 */
void macTxQueryConfig(struct WlanConnection *connection);		/* 0x1365a */
int wpaSuppPrepareDescriptor(OsaBuffer *buffer);			/* 0x1891a */
void cbProcPrepareFrame(struct WlanConnection *connection, unsigned int more,
			unsigned int tid);				/* 0x1ea6c */

static inline __attribute__((always_inline)) uint8_t *wlanApPsData(OsaBuffer *buffer)
{
	return (uint8_t *)buffer + buffer->dataOffset;
}

static inline __attribute__((always_inline)) WlanWirelessFrame *wlanApPsFrame(uint8_t *data)
{
	return (WlanWirelessFrame *)TX_DATA_FRAME(data);
}

static inline __attribute__((always_inline)) void wlanApPsUpdateQuota(unsigned int stations)
{
	if (wlanApPsLimits.maxBuffered > 1)
		wlanApPsLimits.quota = (wlanApPsLimits.maxBuffered - 1) / (int)stations;
	else
		wlanApPsLimits.quota = 0;
}

/* 0x12918 */
int wlanApPsHasBuffered(const struct WlanConnectionRole *role)
{
	return role->psQueue.count != 0 ||
	       ((~role->uapsd.deliveryEnabled & 0x0f) == 0 && role->uapsdQueue.count != 0);
}

/* 0x12936 */
int wlanApPsBufferCommand(struct WlanConnection *connection, struct WlanApPsBufferConfig *config)
{
	(void)connection;
	if (LE16(config->action) != 1) {
		config->maxBuffered = wlanApPsLimits.maxBuffered;
		config->reportFull = wlanApPsLimits.reportFull;
		return 0;
	}
	if (config->maxBuffered > WLAN_AP_PS_ENTRIES) {
		wlanApPsLimits.maxBuffered = WLAN_AP_PS_ENTRIES;
		config->maxBuffered = WLAN_AP_PS_ENTRIES;
	} else {
		wlanApPsLimits.maxBuffered = config->maxBuffered;
	}
	wlanApPsLimits.reportFull = config->reportFull;
	if (wlanApPsLimits.maxBuffered <= 1) {
		wlanApPsLimits.quota = 0;
		return 0;
	}
	if (wlanApPsLimits.stations != 0)
		wlanApPsLimits.quota = (wlanApPsLimits.maxBuffered - 1) / (int)wlanApPsLimits.stations;
	return 0;
}

/* 0x12982 */
void wlanApPsResetGroup(struct WlanConnectionInfo *info)
{
	info->dozingStations = 0;
	info->groupQueue.count = 0;
	info->groupQueue.head = NULL;
	info->groupQueue.tail = NULL;
}

/* 0x1299a */
void wlanApPsFreeEntry(struct WlanApPsEntry *entry)
{
	robinListPushHead(&wlanApPsFree, robinListRemove(&wlanApPsUsed, &entry->node));
}

/* 0x129b0: take the entry out of its queue. */
int wlanApPsUnlink(struct WlanApPsEntry *entry)
{
	struct WlanApPsEntry *it;

	for (it = entry->queue->head; it != NULL; it = it->next) {
		if (it != entry)
			continue;
		if (it->prev != NULL)
			it->prev->next = it->next;
		else
			it->queue->head = it->next;
		if (it->next != NULL)
			it->next->prev = it->prev;
		else
			it->queue->tail = it->prev;
		return 0;
	}
	return 0;
}

/* 0x129ea: drop a kept frame. */
void wlanApPsDrop(struct WlanApPsEntry *entry)
{
	if (entry == NULL)
		return;
	entry->queue->count--;
	wlanCompletePacket(entry->buffer, 0);
	wlanApPsUnlink(entry);
	wlanApPsFreeEntry(entry);
}

/* 0x12a0e */
void wlanApPsFlushGroup(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	while (info->groupQueue.head != NULL)
		wlanApPsDrop(info->groupQueue.head);
}

/* 0x12a2a */
void wlanApPsResetUapsd(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	role->uapsdQueue.count = 0;
	role->uapsdQueue.head = NULL;
	role->uapsdQueue.tail = NULL;
	__aeabi_memclr4(&role->uapsd, sizeof(role->uapsd));
}

/* 0x12a44 */
void wlanApPsResetStation(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	role->peerPowerSave = 0;
	role->psQueue.count = 0;
	role->psQueue.head = NULL;
	role->psQueue.tail = NULL;
	wlanApPsResetUapsd(connection);
}

/* 0x12a60 */
int wlanApPsInit(void)
{
	struct WlanApPsEntry *entry;
	int i;

	robinListInit(&wlanApPsFree);
	robinListInit(&wlanApPsUsed);
	for (i = 0; i < WLAN_AP_PS_ENTRIES; i++) {
		entry = &wlanApPsEntries[i];
		entry->node.next = NULL;
		entry->node.prev = NULL;
		robinListPushHead(&wlanApPsFree, &entry->node);
	}
	return 0;
}

/* 0x12a92 */
void wlanApPsFlushUapsd(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	while (role->uapsdQueue.head != NULL)
		wlanApPsDrop(role->uapsdQueue.head);
}

/* 0x12aa8: the peer left; drop what it was kept. */
void wlanApPsStationGone(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	UINT posture;
	unsigned int stations;

	posture = sysIrqDisable();
	if (role->psCounted) {
		role->psCounted = 0;
		stations = --wlanApPsLimits.stations;
		if (stations != 0)
			wlanApPsUpdateQuota(stations);
	}
	if (role->peerPowerSave == 1) {
		role->peerPowerSave = 0;
		info->dozingStations--;
	}
	while (role->psQueue.head != NULL)
		wlanApPsDrop(role->psQueue.head);
	wlanApPsFlushUapsd(connection);
	macMgmtInspectPacket(connection, connection->associationId, 0);
	sysIrqRestore(posture);
}

/* 0x12b2e */
int wlanApPsReportFull(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanApPsFullEvent event;

	event.type = TLV_TYPE_PS_BUFFER_FULL;
	event.length = 8;
	event.reported = role->fullReported;
	event.buffered = role->psQueue.count + role->uapsdQueue.count;
	__aeabi_memcpy(event.peer, connection->peerAddress, sizeof(event.peer));
	return wlanHostEventSendData(connection, WLAN_EVENT_PS_BUFFER_FULL, &event, sizeof(event));
}

/* 0x12b6e: the peer has room again; tell the host once. */
void wlanApPsCheckRoom(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	wlanConnectionInfo(connection);
	if ((unsigned int)(wlanApPsLimits.quota >> 1) <
	    (uint16_t)(role->psQueue.count + role->uapsdQueue.count))
		return;
	if (role->fullReported == 1) {
		role->fullReported = 0;
		wlanApPsReportFull(connection);
	}
}

/* 0x12ba8: send the first frame of a queue; more: frames follow it. */
int wlanApPsSendNext(struct WlanConnection *connection, struct WlanApPsQueue *queue, int more)
{
	struct WlanApPsEntry *entry = queue->head;
	struct WlanConnectionRole *role;
	OsaBuffer *buffer;
	WlanWirelessFrame *frame;
	uint8_t *data;
	uint32_t remaining;
	int result = 0;

	role = wlanConnectionInfoSlot(connection);
	if (entry == NULL)
		return result;
	buffer = entry->buffer;
	data = wlanApPsData(buffer);
	frame = wlanApPsFrame(data);
	remaining = --queue->count;
	if (frame->address1[0] & 1)
		remaining = more;
	if (remaining != 0 || role->fullReported)
		frame->frameControl[1] |= WLAN_FC1_MORE_DATA;
	else
		frame->frameControl[1] &= ~WLAN_FC1_MORE_DATA;
	if (TX_DATA_RING(data) == WLAN_TX_RING_GROUP) {
		if (WLAN_FC_TYPE(frame->frameControl[0]) != 0)
			result = macTxNotifyQueryTable(buffer);
		else
			result = macTxNotifyComputeTable(buffer);
		if (queue->count == 0)
			macMgmtInspectPacket(connection, 0, 0);
	} else {
		wlanApPsCheckRoom(connection);
		if (WLAN_FC_TYPE(frame->frameControl[0]) != 0)
			result = wlanTxResubmit(buffer);
		else
			result = wlanTxReleaseFrame(buffer);
		if (!wlanApPsHasBuffered(role))
			macMgmtInspectPacket(connection, connection->associationId, 0);
	}
	if (result == 0)
		wlanCompletePacket(buffer, 0);
	wlanApPsUnlink(entry);
	wlanApPsFreeEntry(entry);
	return result;
}

/* 0x12c76 */
void wlanApPsSendAll(struct WlanConnection *connection, struct WlanApPsQueue *queue)
{
	while (queue->count != 0)
		wlanApPsSendNext(connection, queue, 0);
}

/* 0x12c90: send the group frames, after the DTIM beacon (dtim) or when no peer dozes. */
void wlanApPsSendGroup(struct WlanConnection *connection, int dtim)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanApPsQueue *queue = &info->groupQueue;
	unsigned int room = 0;
	int markLast = 0;
	int dataFollows = 0;
	int more;
	uint8_t ring;
	uint8_t *data;

	if (dtim) {
		if (wlanApPsEnabled() && info->groupDeliveryMode == 2 && macTxPoolsIdle() &&
		    info->groupDeliveryBusy == 0) {
			if (info->groupDeliveryFlags & 1) {
				if (info->dataAllowed)
					dataFollows = 1;
			} else {
				markLast = 1;
			}
		}
		room = macTxNotifyParseTable(connection->groupRing);
		ring = WLAN_TX_RING_GROUP;
	} else {
		ring = wlanAccessCategoryToQueue(0);
	}
	while (queue->count != 0 && (!dtim || room != 0)) {
		data = wlanApPsData(queue->head->buffer);
		TX_DATA_RING(data) = ring;
		if (dtim) {
			if ((queue->count == 1 || room == 1) && markLast)
				TX_DATA_TAG(data) = WLAN_TX_TAG_LAST_GROUP;
			more = 0;
			if ((queue->count > 1 || dataFollows) && room > 1)
				more = 1;
		} else {
			more = queue->count > 1;
		}
		if (wlanApPsSendNext(connection, queue, more) == 1 && dtim)
			room--;
	}
	if (dataFollows && room != 0)
		macTxApplyConfig(connection);
}

/* 0x12d9a: the peer entered (1) or left (0) power save. */
void wlanApPsSetState(struct WlanConnection *connection, int powerSave)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	UINT posture;

	posture = sysIrqDisable();
	if (powerSave == 0) {
		if (info->dozingStations != 0)
			info->dozingStations--;
		role->peerPowerSave = 0;
		if (info->dozingStations == 0)
			wlanApPsSendGroup(connection, 0);
		wlanApPsSendAll(connection, &role->uapsdQueue);
		wlanApPsSendAll(connection, &role->psQueue);
	} else if (powerSave == 1) {
		if (role->psCounted == 0) {
			++wlanApPsLimits.stations;
			role->psCounted = 1;
			wlanApPsUpdateQuota(wlanApPsLimits.stations);
		}
		role->peerPowerSave = 1;
		info->dozingStations++;
	}
	role->peerPowerSave = powerSave;
	sysIrqRestore(posture);
}

/* 0x12e38: a copy of a management frame to keep; the original goes back to its pool. */
OsaBuffer *wlanApPsCopyMgmtFrame(struct WlanConnection *connection, OsaBuffer *buffer)
{
	uint8_t *data = wlanApPsData(buffer);
	WlanWirelessFrame *frame = wlanApPsFrame(data);
	OsaBuffer *copy;

	if (WLAN_FC_TYPE(frame->frameControl[0]) != 0)
		return NULL;
	copy = wlanTxBufferAlloc(&wlanBlockPool28, wlanMgmtFrameSize, 0, 1);
	if (copy == NULL)
		return NULL;
	copy->packetClass = 2;
	copy->context = connection;
	copy->dataOffset = 28;
	__aeabi_memcpy4(copy->data, data, 116);
	copy->dataOffset = 176;
	__aeabi_uwrite4((uint32_t)(uintptr_t)((uint8_t *)copy + 176), copy->data + 96);	/* TX_DATA_FRAME */
	__aeabi_memcpy((uint8_t *)copy + 176, frame, LE16(frame->bodyLength) + 32);
	copy->dataOffset = 28;
	return copy;
}

/* 0x12e92: put the entry after another (NULL: at the tail). */
void wlanApPsInsert(struct WlanApPsEntry *entry, struct WlanApPsEntry *after)
{
	if (after == NULL) {
		if (entry->queue->head == NULL) {
			entry->queue->head = entry;
			entry->queue->tail = entry;
			return;
		}
		after = entry->queue->tail;
	}
	if (after->next != NULL)
		after->next->prev = entry;
	else
		after->queue->tail = entry;
	entry->prev = after;
	entry->next = after->next;
	after->next = entry;
}

/* 0x12ec0: frames of the first access category stay ahead of the second's. */
int wlanApPsAcPrecedes(unsigned int category, unsigned int other)
{
	if (category < 2) {
		if (category == other || other == 1)
			return 1;
	} else if ((category == 2 || category == 3) && category >= other) {
		return 1;
	}
	return 0;
}

/* 0x12ee6: the last frame of the queue the new one goes after. */
struct WlanApPsEntry *wlanApPsFindSlot(struct WlanApPsEntry *entry, unsigned int category)
{
	struct WlanApPsEntry *last = NULL;
	struct WlanApPsEntry *it;

	for (it = entry->queue->head; it != NULL; it = it->next) {
		if (wlanApPsAcPrecedes(wlanTidToAccessCategory(wlanApPsData(it->buffer)[0] & 7),
				       category) != 1)
			break;
		last = it;
	}
	return last;
}

/* 0x12f22 */
int wlanApPsAcDeliveryEnabled(uint8_t flags, unsigned int category)
{
	return ((8u >> category) & flags) != 0;
}

/* 0x12f34 */
struct WlanApPsEntry *wlanApPsAllocEntry(void)
{
	struct WlanApPsEntry *entry = NULL;
	struct RobinListNode *node;
	UINT posture;

	posture = sysIrqDisable();
	node = robinListPopTail(&wlanApPsFree);
	if (node != NULL) {
		entry = (struct WlanApPsEntry *)node;
		robinListPushHead(&wlanApPsUsed, node);
	}
	sysIrqRestore(posture);
	return entry;
}

/* 0x12f5e: keep a frame for a dozing peer, or for the DTIM beacon. Returns 1 when no entry is free. */
int wlanApPsQueueFrame(struct WlanConnection *connection, OsaBuffer *buffer)
{
	struct WlanApPsEntry *entry = wlanApPsAllocEntry();
	struct WlanApPsEntry *after;
	struct WlanConnectionRole *role;
	unsigned int aid;
	unsigned int category;
	uint8_t *data;
	int setTim;
	UINT posture;

	if (entry == NULL)
		return 1;
	posture = sysIrqDisable();
	after = NULL;
	entry->next = NULL;
	entry->buffer = buffer;
	entry->prev = NULL;
	data = wlanApPsData(buffer);
	if (wlanApPsFrame(data)->address1[0] & 1) {
		entry->queue = &wlanConnectionInfo(connection)->groupQueue;
		setTim = 1;
		aid = 0;
	} else {
		role = wlanConnectionInfoSlot(connection);
		aid = connection->associationId;
		category = wlanTidToAccessCategory(data[0] & 7);
		if (connection->qosPeer == 1 &&
		    wlanApPsAcDeliveryEnabled(role->uapsd.deliveryEnabled, category)) {
			entry->queue = &role->uapsdQueue;
			after = wlanApPsFindSlot(entry, category);
			setTim = (~role->uapsd.deliveryEnabled & 0x0f) == 0;
		} else {
			setTim = 1;
			entry->queue = &role->psQueue;
		}
	}
	wlanApPsInsert(entry, after);
	entry->queue->count++;
	if (setTim == 1)
		macMgmtInspectPacket(connection, aid, 1);
	sysIrqRestore(posture);
	return 0;
}

/* 0x1302a */
int wlanApPsUnderQuota(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	return (uint16_t)(role->psQueue.count + role->uapsdQueue.count) <
	       (unsigned int)wlanApPsLimits.quota;
}

/* 0x1304c: may the frame be kept for the peer. */
int wlanApPsMayBuffer(struct WlanConnection *connection, OsaBuffer *buffer)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	wlanConnectionInfo(connection);
	if (wlanApPsUnderQuota(connection))
		return 1;
	if (wlanApPsLimits.reportFull == 1 &&
	    wpaSuppPrepareDescriptor(buffer) != WLAN_FRAME_CLASS_EAPOL) {
		if (role->fullReported == 0) {
			role->fullReported = 1;
			wlanApPsReportFull(connection);
		}
		return 1;
	}
	return 0;
}

/*
 * 0x13096: keep a frame for a dozing peer. Returns 1 when the frame was
 * taken (kept, or dropped because nothing could keep it), 0 to send it now.
 */
int wlanTxHoldForPeer(OsaBuffer *buffer, struct WlanConnection *connection)
{
	struct WlanConnectionRole *role;
	struct WlanConnectionInfo *info;
	WlanWirelessFrame *frame;
	OsaBuffer *kept = buffer;
	uint8_t *data = wlanApPsData(buffer);
	int taken = 1;
	int failed = 0;

	role = wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	frame = wlanApPsFrame(data);
	if (WLAN_FC_TYPE(frame->frameControl[0]) == 0 &&
	    WLAN_FC_SUBTYPE(frame->frameControl[0]) == WLAN_SUBTYPE_PROBE_RESPONSE)
		goto send;
	if (frame->address1[0] & 1) {
		if (info->dozingStations == 0 || (frame->frameControl[1] & WLAN_FC1_ORDER))
			goto send;
		if (WLAN_FC_TYPE(frame->frameControl[0]) == 0) {
			kept = wlanApPsCopyMgmtFrame(connection, kept);
			if (kept == NULL)
				goto fail;
		}
		if (wlanTxHoldFrame(kept, 1) != 0 || wlanApPsQueueFrame(connection, kept) != 0)
			goto fail;
		goto queued;
	}
	if (role->peerPowerSave != 1 || memcmp(connection->peerAddress, frame->address1, 6) != 0)
		goto send;
	if (!wlanApPsMayBuffer(connection, kept))
		goto fail;
	if (WLAN_FC_TYPE(frame->frameControl[0]) == 0) {
		kept = wlanApPsCopyMgmtFrame(connection, kept);
		if (kept == NULL)
			goto fail;
	}
	if (wlanTxHoldFrame(kept, 1) != 0)
		goto fail;
	if (wlanApPsQueueFrame(connection, kept) == 0)
		goto queued;
	failed = 1;
	if (wlanApPsEnabled())
		macTxQueryConfig(wlanConnectionByRole(2));
	goto release;

send:
	taken = 0;
queued:
	if (kept == buffer)
		return taken;
	goto release;
fail:
	failed = 1;
release:
	if (buffer != NULL) {
		if (WLAN_FC_TYPE(frame->frameControl[0]) != 0)
			wlanTxWindowComplete(TX_DATA_RING(data));
		wlanCompletePacket(buffer, 0);
	}
	if (failed && kept != NULL && kept != buffer)
		wlanCompletePacket(kept, 0);
	return taken;
}

/* 0x131ba */
int wlanTxPeerHolding(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role;

	if (connection == NULL)
		return 0;
	role = wlanConnectionInfoSlot(connection);
	if (role == NULL)
		return 0;
	if (role->peerPowerSave != 1)
		return 0;
	return role->peerPowerSave;
}

/* 0x131d4: a PS-Poll from the peer; send one kept frame, or a null frame when none is kept. */
int wlanApPsPsPoll(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	UINT posture;

	posture = sysIrqDisable();
	if (role->psQueue.count != 0)
		wlanApPsSendNext(connection, &role->psQueue, 0);
	else
		cbProcPrepareFrame(connection, 0, 0);
	sysIrqRestore(posture);
	return 0;
}

/*
 * 0x1321c: send the frames of a service period (at most twice the peer's Max
 * SP Length), the last one with EOSP set; when the last is a management
 * frame, a QoS null frame of the trigger's TID ends the period.
 */
void wlanApPsStartServicePeriod(struct WlanConnection *connection, unsigned int tid)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanApPsQueue *queue = &role->uapsdQueue;
	unsigned int maxSpLength;
	unsigned int frames;
	uint8_t sent = 0;
	int sameRing = 1;
	int endWithNull = 0;
	unsigned int ring;
	uint8_t *data;
	UINT posture;

	posture = sysIrqDisable();
	maxSpLength = (role->uapsd.qosInfo >> 5) & 3;
	if (maxSpLength != 0) {
		frames = 2 * maxSpLength;
		if (frames >= queue->count)
			frames = queue->count;
		frames = (uint8_t)(frames - role->uapsd.spSent);
	} else {
		frames = (uint8_t)queue->count;
	}
	if (frames == 0)
		goto null;
	ring = TX_DATA_RING(wlanApPsData(queue->head->buffer));
	for (; frames > 1; frames = (uint8_t)(frames - 1)) {
		if (queue->head == NULL)
			continue;
		data = wlanApPsData(queue->head->buffer);
		TX_DATA_TAG(data) = WLAN_TX_TAG_SP;
		sent++;
		if (TX_DATA_RING(data) != ring)
			sameRing = 0;
		ring = TX_DATA_RING(data);
		wlanApPsSendNext(connection, queue, 0);
	}
	data = wlanApPsData(queue->head->buffer);
	if (sameRing == 1 && TX_DATA_RING(data) == ring) {
		sent++;
		if (WLAN_FC_TYPE(wlanApPsFrame(data)->frameControl[0]) != 0) {
			TX_DATA_TAG(data) = WLAN_TX_TAG_SP_LAST;
			data[0] |= WLAN_QOS_EOSP;
			TX_DATA_REQUEST(data)->type = data[0];
			TX_DATA_REQUEST(data)->info = data[1];
		} else {
			endWithNull = 1;
			tid = data[0] & 7;
		}
		wlanApPsSendNext(connection, queue, 0);
	}
	role->uapsd.spRemaining = sent;
	if ((role->uapsd.qosInfo >> 5) & 3)
		role->uapsd.spSent += sent;
	if (endWithNull != 1)
		goto out;
null:
	cbProcPrepareFrame(connection, queue->count, tid);
out:
	sysIrqRestore(posture);
}

/* 0x13334: a QoS data frame with Power Management set on a trigger-enabled AC starts a service period. */
void wlanApPsTrigger(struct WlanConnection *connection, const struct WlanRxStatus *status)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	unsigned int tid = status->qosControl[0] & 7;
	unsigned int category = wlanTidToAccessCategory(tid);
	const WlanWirelessFrame *frame;

	if (connection == NULL || connection->qosPeer == 0 || role->uapsd.inServicePeriod == 1)
		return;
	if (!wlanApPsAcDeliveryEnabled(role->uapsd.triggerEnabled, category) || role->peerPowerSave != 1)
		return;
	frame = (const WlanWirelessFrame *)__aeabi_uread4(status->frame);
	if ((frame->frameControl[1] & WLAN_FC1_POWER_MGMT) &&
	    WLAN_FC_TYPE(frame->frameControl[0]) == WLAN_TYPE_DATA &&
	    (frame->frameControl[0] & WLAN_FC_SUBTYPE_QOS)) {
		role->uapsd.inServicePeriod = 1;
		wlanApPsStartServicePeriod(connection, tid);
	}
}

/* 0x133b0: a frame of a service period was sent; send the next ones, or end the period. */
void wlanApPsTxDone(struct WlanConnection *connection, const uint8_t *data)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	UINT posture;

	if (connection == NULL || role == NULL)
		return;
	posture = sysIrqDisable();
	if (TX_DATA_TAG(data) == WLAN_TX_TAG_SP_LAST) {
		role->uapsd.inServicePeriod = 0;
		role->uapsd.spRemaining = 0;
		role->uapsd.spSent = 0;
	} else if (TX_DATA_TAG(data) == WLAN_TX_TAG_SP && role->uapsd.inServicePeriod == 1) {
		if (role->uapsd.spRemaining == 0 || --role->uapsd.spRemaining == 0)
			wlanApPsStartServicePeriod(connection, data[0] & 7);
	}
	sysIrqRestore(posture);
}
