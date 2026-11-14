/*
 * WMM: the parameters of the access categories and the queues they use,
 * and the host commands for them. See robin/wmm.h. The filename is
 * inferred; no source filename survives.
 */
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/wmm.h"

struct WlanWmm wlanWmm __attribute__((section(".dtcm.bss")));					/* 0x04008c4c */
struct WlanWmmSlot wlanWmmSlots[WLAN_WMM_SLOTS] __attribute__((section(".dtcm.bss")));	/* 0x04008ca0 */

extern int macMgmtComputeState(struct WlanConnection *connection);				/* 0x34b66 */
extern int wlanHostEventSend(struct WlanConnection *connection, OsaBuffer *buffer);

/* WMM is on for the BSS. */
int wlanWmmEnable(void)
{
	wlanWmm.enabled = 1;
	return 1;
}

/*
 * The slots of a queue follow the queue's admission state: a queue that
 * is ruled out (no admitted time, or streams admitted elsewhere) uses its
 * fallback queue, and the slots with a matching queue are marked active.
 */
int wlanWmmApplyQueue(unsigned int queue)
{
	struct WlanWmmQueue *record = &wlanWmm.queues[queue];
	unsigned int category = wlanAccessCategoryToQueue((record->info >> 1) & 3);
	int fallback = 0;
	unsigned int chosen = category;
	unsigned int target = category;
	unsigned int i;
	int slot;

	if (record->info & 8) {
		if (record->admittedTime == 0) {
			fallback = 1;
			chosen = record->fallback;
			target = record->fallback;
			wlanWmmSetEdca(queue, record);
		} else if (record->streams != 0) {
			chosen = record->fallback;
		}
	}
	for (i = 0; i < 2; i = (uint8_t)(i + 1)) {
		struct WlanWmmSlot *entry = record->slots[i];

		if (target == 255 || chosen == 255) {
			entry->activeQueue = 255;
			entry->activeRing = 255;
		} else {
			if (entry->activeQueue != target) {
				unsigned int expected = fallback != 0 ? entry->ownQueue : entry->fallbackQueue;

				if (entry->activeQueue == expected) {
					entry->activeQueue = target;
					entry->activeRing = wlanWmmRings[2 * target + i];
				}
			}
			wlanWmmSetEdca(entry->activeQueue, &wlanWmm.queues[chosen]);
		}
	}
	for (slot = WLAN_WMM_SLOTS - 1; wlanWmmSlots[slot].activeQueue != wlanWmmSlots[slot].ownQueue;) {
		if (--slot < 0)
			return slot;
	}
	wlanWmmCurrentSlot = slot;
	return slot;
}

/* A WMM parameter element is taken into the queues' records. 1: it was valid. */
int wlanWmmSetParameters(const uint8_t *element, uint8_t *count)
{
	unsigned int i;
	int result = wlanWmmParseParameter(element, count);

	if (result == 0)
		return result;
	for (i = 0; i < WLAN_WMM_QUEUES; i = (uint8_t)(i + 1)) {
		unsigned int queue = wlanAccessCategoryToQueue((element[4 * i + 10] >> 5) & 3);

		wlanWmmStoreQueue(&wlanWmm.queues[queue], &element[4 * i + 10]);
	}
	for (i = 0; i < WLAN_WMM_QUEUES; i = (uint8_t)(i + 1)) {
		struct WlanWmmQueue *record = &wlanWmm.queues[i];
		unsigned int fallback;

		if (record->info & 8) {
			unsigned int k;

			fallback = 255;
			for (k = 0; k <= i; k = (uint8_t)(k + 1)) {
				if (!(wlanWmm.queues[k].info & 8))
					fallback = k;
			}
		} else {
			fallback = i;
		}
		record->fallback = fallback;
		record->slots[0]->fallbackQueue = fallback;
		record->slots[1]->fallbackQueue = fallback;
		wlanWmmApplyQueue(i);
	}
	return 1;
}

/* The four queue status TLVs of Marvell's WMM status response. Returns their length. */
int wlanWmmQueueStatus(const struct WlanConnection *connection, uint8_t *tlvs)
{
	unsigned int queue;
	int length = 0;

	__aeabi_memclr(tlvs, 56);
	for (queue = wlanAccessCategoryToQueue(1); queue <= 3; queue = (uint8_t)(queue + 1)) {
		const struct WlanWmmQueue *record = &wlanWmm.queues[queue];
		int bit;

		tlvs[0] = 16;		/* type 0x0110: queue status */
		tlvs[1] = 1;
		tlvs[2] = 10;		/* length */
		tlvs[3] = 0;
		tlvs[4] = queue;
		bit = wlanWmmQueueBit(queue);
		if (!(connection->flags2 & 0x10)) {
			tlvs[5] = 1;	/* disabled */
		} else if (wlanWmmEnabled() == 0) {
			if (wlanAccessCategoryToQueue(0) != queue)
				tlvs[5] = 1;
		} else {
			uint16_t admitted = record->admittedTime;

			tlvs[8] = (record->info & 8) != 0;	/* flow required */
			tlvs[9] = admitted != 0;		/* flow created */
			tlvs[6] = admitted;			/* medium time */
			tlvs[7] = admitted >> 8;
			if ((record->info & 8) && admitted == 0)
				tlvs[5] = 1;
			if (wlanWmmFlowBits & (1 << bit))
				tlvs[10] = 1;
			if (wlanWmmSessionBits & (1 << bit))
				tlvs[11] = 1;
		}
		tlvs += 14;
		length += 14;
	}
	return length;
}

/* HostCmd_CMD_WMM_GET_STATUS: the queue status and the parameter element in use. */
unsigned int hostCmd0071(struct WlanConnection *connection, char *body)
{
	int length = wlanWmmQueueStatus(connection, (uint8_t *)body);

	return wlanWmmAppendIe((uint8_t *)body, wlanWmmElement) + length;
}

/* The host is told the WMM status changed (event 0x17). */
int wlanWmmReportStatus(struct WlanConnection *connection)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *frame;

	if (!wlanTxAllocFrame(&buffer, (WlanTxDescriptor **)&frame, 400, WLAN_HOST_EVENT_WMM_STATUS))
		return 0;
	frame->length = hostCmd0071(connection, (char *)&frame->code);
	return wlanHostEventSend(connection, buffer);
}

/* A beacon or probe response carried a WMM parameter element (or information element) of the BSS. */
int wlanWmmUpdate(struct WlanConnection *connection, const uint8_t *info, const uint8_t *parameter)
{
	int result = wlanWmmEnabled();

	if (result != 0) {
		if (parameter != NULL) {
			result = parameter[8] & 0xf;
			if (result != wlanWmm.parameterSetCount) {
				result = wlanWmmSetParameters(parameter, &wlanWmm.parameterSetCount);
				if (result != 0) {
					__aeabi_memcpy(wlanWmmElement, parameter, 26);
					return wlanWmmReportStatus(connection);
				}
			}
		} else if (info != NULL) {
			result = info[8] & 0xf;
			if (result != wlanWmm.parameterSetCount)
				return macMgmtComputeState(connection);
		}
	}
	return result;
}

/* HostCmd_CMD_WMM_QUEUE_CONFIG: its response length. */
unsigned int hostCmd0070(uint8_t *body)
{
	uint32_t length = 0;

	wlanWmmQueueConfig(body, wlanRadioConfig->wmmQueueTime, &length);
	return length;
}

int wlanWmmEnabled(void)
{
	return wlanWmm.enabled;
}

/* The bit (in the flow and session bitmaps) of a queue. */
int wlanWmmQueueBit(int queue)
{
	switch (queue) {
	case 0:
		return 2;
	case 2:
		return 1;
	case 3:
		return 0;
	default:
		return 3;
	}
}

/* A queue's contention parameters go to the MAC (the ROM's EDCA setters). */
int wlanWmmSetEdca(unsigned int queue, const struct WlanWmmQueue *record)
{
	romEdcaSetCwMin(queue, (1 << (record->contention >> 4)) - 1);
	romEdcaSetCwMax(queue, (1 << (record->contention & 0xf)) - 1);
	romEdcaSetAifsn(queue, record->info >> 4);
	return romEdcaSetTxop(queue, record->txop);
}

/* 1: the WMM parameter element holds all four access categories; its parameter set count goes to *count. */
int wlanWmmParseParameter(const uint8_t *element, uint8_t *count)
{
	if (element == NULL || element[1] != 24 ||
	    (uint8_t)((1 << ((element[22] >> 5) & 3)) | (1 << ((element[18] >> 5) & 3)) |
		      (1 << ((element[14] >> 5) & 3)) | (1 << ((element[10] >> 5) & 3))) != 0xf)
		return 0;
	if (count != NULL)
		*count = element[8] & 0xf;
	return 1;
}

/* An AC parameter record of the element (ACI/AIFSN, ECW, TXOP limit) becomes a queue's record. */
struct WlanWmmQueue *wlanWmmStoreQueue(struct WlanWmmQueue *queue, const uint8_t *record)
{
	queue->info = (queue->info & 0xf9) | (2 * ((record[0] >> 5) & 3));
	queue->info = (queue->info & 0xf7) | (8 * ((record[0] >> 4) & 1));
	queue->info = (queue->info & 0x0f) | (16 * record[0]);
	queue->contention = ((record[1] & 0xf) << 4) | (record[1] >> 4);
	queue->txop = (record[3] << 8) | record[2];
	return queue;
}

/* HostCmd_CMD_WMM_QUEUE_CONFIG's action on one queue's medium time: 0 get, 1 set, 2 default. */
int wlanWmmQueueConfig(uint8_t *body, uint16_t *table, uint32_t *length)
{
	unsigned int queue = body[1];

	if (queue < 4) {
		unsigned int action = body[0];

		if (action == 0) {
			*(uint16_t *)(body + 2) = table[queue];
		} else if (action == 1) {
			unsigned int time = (body[3] << 8) | body[2];

			if (time != 0)
				table[queue] = time;
		} else if (action == 2) {
			table[queue] = wlanWmmDefaultTime;
		} else {
			return 0;
		}
		*length = 68;
		return 1;
	}
	return 0;
}

/* A TLV with the element's ID as its type and the element's body is appended to the response (at 56). */
int wlanWmmAppendIe(uint8_t *body, const uint8_t *element)
{
	uint8_t *tlv;

	if (element == NULL || element[1] == 0)
		return 0;
	tlv = body + 56;
	tlv[0] = element[0];
	tlv[1] = 0;
	tlv[2] = element[1];
	tlv[3] = 0;
	__aeabi_memcpy(tlv + 4, element + 2, element[1]);
	return element[1] + 4;
}
