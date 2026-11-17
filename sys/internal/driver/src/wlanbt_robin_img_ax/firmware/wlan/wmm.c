/*
 * WMM: the parameters of the access categories and the queues they use,
 * and the host commands for them. See robin/wmm.h. The filename is
 * inferred; no source filename survives.
 */
#include "robin/tspec.h"
#include "robin/ratesel.h"
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/htba.h"
#include "robin/ie.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/stapssm.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/threadx.h"
#include "robin/txring.h"
#include "robin/txstats.h"
#include "robin/wlanrx.h"
#include "robin/wmm.h"
#include "robin/macentry.h"

uint32_t wlanWmmDefaultTime __attribute__((section(".itcm.bss")));				/* 0x00054ec0 */
const uint8_t wlanWmmRings[8] __attribute__((section(".sqram1.ro"))) = { 1, 2, 0, 3, 4, 5, 6, 7 };	/* SQRAM 0xc000fa9b */

/* Zero-initialised at load; the addresses are the original's. */
struct WlanWmm wlanWmm __attribute__((section(".dtcm.bss")));					/* 0x04008c4c */

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
	unsigned int category = wlanAccessCategoryToQueue(record->edca.category);
	int fallback = 0;
	unsigned int chosen = category;
	unsigned int target = category;
	unsigned int i;
	int slot;

	if (record->edca.acm) {
		if (record->admittedTime == 0) {
			fallback = 1;
			chosen = record->fallback;
			target = record->fallback;
			wlanWmmSetEdca(queue, &record->edca);
		} else if (record->exhausted != 0) {
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
			wlanWmmSetEdca(entry->activeQueue, &wlanWmm.queues[chosen].edca);
		}
	}
	for (slot = WLAN_WMM_SLOTS - 1; wlanWmm.slots[slot].activeQueue != wlanWmm.slots[slot].ownQueue;) {
		if (--slot < 0)
			return slot;
	}
	wlanWmm.currentSlot = slot;
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

		wlanWmmStoreQueue(&wlanWmm.queues[queue].edca, &element[4 * i + 10]);
	}
	for (i = 0; i < WLAN_WMM_QUEUES; i = (uint8_t)(i + 1)) {
		struct WlanWmmQueue *record = &wlanWmm.queues[i];
		unsigned int fallback;

		if (record->edca.acm) {
			unsigned int k;

			fallback = 255;
			for (k = 0; k <= i; k = (uint8_t)(k + 1)) {
				if (!(wlanWmm.queues[k].edca.acm))
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

			tlvs[8] = (record->edca.acm) != 0;	/* flow required */
			tlvs[9] = admitted != 0;		/* flow created */
			tlvs[6] = admitted;			/* medium time */
			tlvs[7] = admitted >> 8;
			if ((record->edca.acm) && admitted == 0)
				tlvs[5] = 1;
			if (wlanWmm.infoIe[0][WLAN_WMM_IE_QOS_INFO] & (1 << bit))
				tlvs[10] = 1;
			if (wlanWmm.infoIe[2][WLAN_WMM_IE_QOS_INFO] & (1 << bit))
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

	return wlanWmmAppendIe((uint8_t *)body, wlanWmm.element) + length;
}

/* The host is told the WMM status changed (event 0x17). */
int wlanWmmReportStatus(struct WlanConnection *connection)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *frame;

	if (!wlanTxAllocFrame(&buffer, (struct WlanHostEventFrame **)&frame, 400, WLAN_HOST_EVENT_WMM_STATUS))
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
					__aeabi_memcpy(wlanWmm.element, parameter, 26);
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

/* Whether the BSS uses WMM (0x41cb8, apart from this file's other code: called, never inlined). */
__attribute__((noinline)) int wlanWmmEnabled(void)
{
	return wlanWmm.enabled;
}

/*
 * A TSPEC's TS Info (802.11 8.4.2.32: three bytes) as these routines read it:
 * the traffic direction is bits 5-6 of the first byte, the U-APSD flag bit 10 and
 * the user priority bits 11-13 (bits 2 and 3-5 of the second byte).
 */
#define TSINFO_DIRECTION(tsInfo)	(((tsInfo)[0] >> 5) & 3)
#define TSINFO_APSD(tsInfo)		(((tsInfo)[1] >> 2) & 1)
#define TSINFO_USER_PRIORITY(tsInfo)	(((tsInfo)[1] >> 3) & 7)

/* The flow direction codes of Marvell's HostCmd_DS_WMM_TS_STATUS: uplink 1, downlink 2, both 3. */
enum {
	WLAN_TSPEC_UPLINK = 1,
	WLAN_TSPEC_DOWNLINK = 2,
	WLAN_TSPEC_BIDIRECTIONAL = 3,
};

/* 0x2cfbc: TS Info direction 0 is uplink, 1 downlink, 3 both; 2 is reserved and gives 0. */
unsigned int wlanTspecDirection(const uint8_t *tsInfo, uint8_t *directions)
{
	unsigned int direction = TSINFO_DIRECTION(tsInfo);

	*directions = 0;
	if (direction == 0)
		*directions = WLAN_TSPEC_UPLINK;
	else if (direction == 1)
		*directions = WLAN_TSPEC_DOWNLINK;
	else if (direction == 3)
		*directions = WLAN_TSPEC_BIDIRECTIONAL;
	return direction;
}

/* 0x2cfde: the bit of a user priority's access category in the U-APSD flags (AC_VO 0, AC_VI 1, AC_BK 2, AC_BE 3). */
int wlanTspecUapsdBit(int userPriority)
{
	switch (userPriority) {
	case 1:
	case 2:
		return 2;
	case 4:
	case 5:
		return 1;
	case 6:
	case 7:
		return 0;
	default:
		return 3;
	}
}

/*
 * 0x2d000: the TSPEC's access category takes (action 1, with U-APSD set; without it, the bit
 * is cleared) or gives back (action 0, to the value in *reference) its U-APSD bit in the
 * bitmap of each direction the TSPEC covers.
 */
int wlanTspecApplyBits(int action, uint8_t *uplink, uint8_t *downlink, const uint8_t *reference,
		       const uint8_t *tspec, const uint8_t *directions)
{
	unsigned int bit = wlanTspecUapsdBit(TSINFO_USER_PRIORITY(tspec));
	unsigned int mask = 1 << bit;

	if (action == 1) {
		if (TSINFO_APSD(tspec)) {
			if (*directions & WLAN_TSPEC_DOWNLINK)
				*downlink |= mask;
			if (*directions & WLAN_TSPEC_UPLINK)
				*uplink |= mask;
		} else {
			if (*directions & WLAN_TSPEC_DOWNLINK)
				*downlink &= ~mask;
			if (*directions & WLAN_TSPEC_UPLINK)
				*uplink &= ~mask;
		}
	} else if (action == 0) {
		if (*directions & WLAN_TSPEC_DOWNLINK)
			*downlink = (*downlink & ~mask) | (*reference & mask);
		if (*directions & WLAN_TSPEC_UPLINK)
			*uplink = (*uplink & ~mask) | (*reference & mask);
	}
	return action;
}

/* The WMM TSPEC element's header: vendor element (61 bytes), Microsoft OUI, type 2, subtype 2, version 1. */
static const uint8_t wlanWmmTspecHeader[8] = { 0xdd, 0x3d, 0x00, 0x50, 0xf2, 0x02, 0x02, 0x01 };

/*
 * 0x2d092: the body of a WMM ADDTS Request action frame (the category byte is the caller's):
 * action 0, the dialog token, status 0, the TSPEC element's header and the length bytes of the
 * TSPEC itself. Returns the body.
 */
uint8_t *wlanTspecBuildAddts(OsaBuffer *buffer, const uint8_t *tspec, unsigned int length,
			     unsigned int dialogToken)
{
	WlanWirelessFrame *frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	uint8_t *body = frame->body;

	body[1] = 0;
	body[2] = dialogToken;
	body[3] = 0;
	__aeabi_memcpy(body + 4, wlanWmmTspecHeader, 8);
	__aeabi_memcpy(body + 12, tspec, length);
	length += 12;
	frame->bodyLength[0] = length;
	frame->bodyLength[1] = length >> 8;
	return body;
}

/*
 * 0x2d0ca: the response to HostCmd_CMD_WMM_ADDTS_REQ (Marvell's HostCmd_DS_WMM_ADDTS_REQ): a timeout
 * without the access point's ADDTS Response frame, otherwise the response's dialog token, status code
 * and TSPEC (the elements after its four-byte header, length bytes in all).
 */
void wlanWmmAddtsResult(uint8_t *command, const uint8_t *response, unsigned int length)
{
	command[0] = 0x6e;
	command[1] = 0x80;
	command[6] = 0;
	command[7] = 0;
	command[4] = 0;		/* the sequence number's low byte; its high byte keeps the BSS type and number */
	command[2] = 9;
	command[3] = 0;
	if (response == NULL) {
		command[8] = 2;	/* MLAN_CMD_RESULT_TIMEOUT */
		return;
	}
	command[8] = 0;
	length -= 4;
	command[2] = length + 15;
	command[3] = (length + 15) >> 8;
	command[13] = response[2];
	command[14] = response[3];
	__aeabi_memcpy(command + 15, response + 4, length);
}

/*
 * 0x2d10c: the body of a WMM DELTS action frame: action 2, token 0, status 0 and the 63 bytes of
 * the TSPEC element. Its length is 4 + 63; the length the host gave is not read.
 */
void wlanTspecBuildDelts(OsaBuffer *buffer, const uint8_t *tspecElement, unsigned int length)
{
	WlanWirelessFrame *frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	uint8_t *body = frame->body;

	body[1] = 2;
	body[2] = 0;
	body[3] = 0;
	__aeabi_memcpy(body + 4, tspecElement, 63);
	frame->bodyLength[0] = 67;
	frame->bodyLength[1] = 0;
}

/* HostCmd_DS_WMM_TS_STATUS (Marvell's name) */
struct __attribute__((packed)) WlanWmmTsStatus {
	uint8_t tid;
	uint8_t valid;
	uint8_t accessCategory;		/* the queue of the stream's access category */
	uint8_t userPriority;
	uint8_t psb;			/* 0 legacy, 1 U-APSD */
	uint8_t flowDirection;
	uint16_t mediumTime;
};

/*
 * 0x2d164: one traffic stream's status, from its TS Info (tsInfo), the flow direction (*direction)
 * and the medium time at offset 53 of its TSPEC. The stream's index is the record's first byte and
 * has to be under 8. Returns the record's size, 0 for a stream that does not exist.
 */
unsigned int wlanWmmTsStatus(uint8_t *body, const uint8_t *tsInfo, const uint8_t *direction, int valid)
{
	struct WlanWmmTsStatus *status = (struct WlanWmmTsStatus *)body;
	unsigned int tid = status->tid;
	unsigned int priority;

	__aeabi_memclr(status, 8);
	status->tid = tid;
	if (tid >= 8)
		return 0;
	if (tsInfo == NULL || direction == NULL)
		return 8;
	if (valid)
		status->valid = 1;
	priority = TSINFO_USER_PRIORITY(tsInfo);
	status->userPriority = priority;
	status->psb = TSINFO_APSD(tsInfo);
	status->flowDirection = *direction;
	status->mediumTime = tsInfo[53] | (tsInfo[54] << 8);
	status->accessCategory = wlanAccessCategoryToQueue(wlanTidToAccessCategory(priority));
	return 8;
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
int wlanWmmSetEdca(unsigned int queue, const struct WlanWmmEdca *edca)
{
	romEdcaSetCwMin(queue, (1 << edca->ecwMin) - 1);
	romEdcaSetCwMax(queue, (1 << edca->ecwMax) - 1);
	romEdcaSetAifsn(queue, edca->aifsn);
	return romEdcaSetTxop(queue, edca->txop);
}

/* 1: the WMM parameter element holds all four access categories; its parameter set count goes to *count. */
int wlanWmmParseParameter(const uint8_t *element, uint8_t *count)
{
	/* Sony's code reads the records before the test of the element (also when it is NULL). */
	const struct WlanWmmAcRecord *ac = (const struct WlanWmmAcRecord *)&element[10];
	uint8_t categories = (1 << ac[0].aci) | (1 << ac[1].aci) | (1 << ac[2].aci) | (1 << ac[3].aci);

	if (element == NULL || element[1] != 24 || categories != 0xf)
		return 0;
	if (count != NULL)
		*count = element[8] & 0xf;
	return 1;
}

/* An AC parameter record of the element (ACI/AIFSN, ECW, TXOP limit) becomes a queue's record. */
struct WlanWmmEdca *wlanWmmStoreQueue(struct WlanWmmEdca *edca, const uint8_t *record)
{
	const struct WlanWmmAcRecord *ac = (const struct WlanWmmAcRecord *)record;

	edca->category = ac->aci;
	edca->acm = ac->acm;
	edca->aifsn = ac->aifsn;
	edca->ecwMin = ac->ecwMin;
	edca->ecwMax = ac->ecwMax;
	edca->txop = ac->txopLimit;
	return edca;
}

/* HostCmd_CMD_WMM_QUEUE_CONFIG's action on one queue's medium time: 0 get, 1 set, 2 default. */
int wlanWmmQueueConfig(uint8_t *body, uint16_t *table, uint32_t *length)
{
	unsigned int queue = body[1];

	if (queue < 4) {
		unsigned int action = body[0];

		if (action == 0) {
			unsigned int time = table[queue];

			body[2] = time;
			body[3] = time >> 8;
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

/* Each queue's frames in flight and what they cost. */
struct WlanWmmTiming wlanWmmTiming[WLAN_WMM_QUEUES] __attribute__((section(".dtcm.bss")));	/* 0x04009018 */
struct WlanWmmStats wlanWmmStats[WLAN_WMM_QUEUES] __attribute__((section(".dtcm.bss")));	/* 0x04009038 */
uint16_t wlanQosNullSequence __attribute__((section(".sqram1.bss")));			/* SQRAM 0xc0011b08 */
int (*wlanEdcaApplyHook)(struct WlanConnection *connection) __attribute__((section(".sqram1.bss")));	/* SQRAM 0xc0011bb4 */
uint8_t wlanWmmParameterElement[26] __attribute__((section(".sqram1.bss")));		/* SQRAM 0xc0031ee4 */

/* The pointer to a transmit data's frame (at 96), read as an aligned word here. */
#define WMM_TX_FRAME(data)	(*(const uint8_t *const *)((data) + 96))


/* HostCmd_CMD_WMM_QUEUE_STATS. */
int hostCmd0081(uint8_t *body)
{
	return wlanWmmStatsCommand(body, wlanWmmStats, &wlanWmm.policed);
}

/* The queues but the first start with no outstanding admission counters. */
void wlanWmmResetCounters(void)
{
	int queue;

	for (queue = wlanAccessCategoryToQueue(1); queue <= 3; queue++) {
		wlanWmmTiming[queue].lastTime = 0;
		wlanWmmTiming[queue].pending = 0;
	}
}

/*
 * WMM is switched on or off for a connection. Off, every queue gets the
 * default contention parameters (AIFSN 2, CWmin 15, CWmax 1023, no TXOP) and
 * the default medium times, and the information elements are forgotten.
 */
__attribute__((noinline)) void wlanWmmSet(struct WlanConnection *connection, int enable, int report)
{
	struct WlanWmmEdca edca;

	if (enable != 0) {
		wlanWmm.enabled = 1;
	} else {
		wlanWmm.enabled = 0;
		wlanWmmResetDefaults(wlanWmm.infoIe[0], wlanWmm.infoIe[1], wlanWmm.infoIe[2], wlanWmm.element, &edca,
				     wlanRadioConfig->wmmQueueTime, 4, 10);
	}
	if (report != 0)
		wlanWmmReportStatus(connection);
	p2pDirectTrackEntry();
	wlanWmmResetCounters();
}

/*
 * The WMM elements of a beacon or probe response, or their absence, decide
 * whether the BSS is a WMM one. Without elements the BSS's own parameters
 * are the defaults of the WMM parameter element.
 */
void wlanWmmJoin(struct WlanConnection *connection, struct WlanIePointers *elements, int sameBss,
		 int report)
{
	unsigned int queue;

	if (elements->pWmmParam == NULL) {
		if (elements->pEdcaParam == NULL && wlanConnectionIsHt(connection) == 0) {
			wlanWmmSet(connection, 0, report);
			wlanPsSetRoleMasks(0, 0);
			return;
		}
	}
	if (elements->pEdcaParam != NULL) {
		wlanWmmSet(connection, 1, 0);
		return;
	}
	if (elements->pWmmParam == NULL) {
		wlanWmmBuildParameterElement(wlanWmm.bssIe);
		wlanWmmFillParameterElement(wlanWmmParameterElement);
		elements->pWmmParam = wlanWmmParameterElement;
	}
	for (queue = wlanAccessCategoryToQueue(1); queue <= 3; queue = (uint8_t)(queue + 1))
		wlanWmmSetupQueue(queue);
	if (sameBss != 0)
		p2pDirectManageEntry();
	__aeabi_memcpy(wlanWmm.infoIe[1], wlanWmm.bssIe, 9);
	__aeabi_memcpy(wlanWmm.infoIe[0], wlanWmm.bssIe, 9);
	__aeabi_memcpy(wlanWmm.infoIe[2], wlanWmm.bssIe, 9);
	wlanPsSetRoleMasks(wlanWmm.infoIe[0][WLAN_WMM_IE_QOS_INFO], wlanWmm.infoIe[2][WLAN_WMM_IE_QOS_INFO]);
	if (wlanWmmSetParameters(elements->pWmmParam, &wlanWmm.parameterSetCount) != 0)
		__aeabi_memcpy(wlanWmm.element, elements->pWmmParam, 26);
	wlanWmmSet(connection, 1, report);
}

/* Whether a WMM parameter element header is on hand. */
int wlanWmmHasElement(void)
{
	if (wlanWmm.bssIe[0] != 0)
		return 1;
	return wlanWmm.bssIe[0];
}

int wlanWmmCurrentSlotNumber(void)
{
	return wlanWmm.currentSlot;
}

/* A transmit Block Ack window changed the queue and ring a TID's frames use. */
int wlanWmmSetBaWindow(uint8_t tid, unsigned int queue, unsigned int ring)
{
	struct WlanWmmSlot *slot = &wlanWmm.slots[tid];

	slot->activeQueue = queue;
	slot->statsRing = ring;
	wlanWmmApplyQueue(wlanAccessCategoryToQueue(wlanTidToAccessCategory(tid)));
	return macTxStatsAddValidated(ring, &wlanWmm.validatedSeen[ring]);
}

/* Everything WMM knows is forgotten. */
void wlanWmmClear(void)
{
	__aeabi_memclr4(&wlanWmm, sizeof(wlanWmm));
	__aeabi_memclr4(wlanWmmStats, sizeof(wlanWmmStats));
}

/* The EDCA parameters the AP announced for the four access categories reach the MAC. */
int wlanWmmApplyApEdca(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanWmmEdca edca = { 0 };
	unsigned int category = 0;
	unsigned int queue;
	int result;

	do {
		queue = wlanAccessCategoryToQueue(category);
		romLibDispatchEntry(queue);
		while (((1 << queue) & MAC_258_PENDING_STATUS) == 0)
			;
		wlanWmmStoreQueue(&edca, info->wmmRecords[category]);
		wlanWmmSetEdca(queue, &edca);
		result = romLibParseEntry(queue);
		category = (uint8_t)(category + 1);
	} while (category < 4);
	return result;
}

/* Bit 7 of the QoS info of a WMM element: the AP supports U-APSD. */
void wlanWmmSetApsdSupport(uint8_t *ie, int enable)
{
	if (ie != NULL) {
		if (enable == 1)
			ie[8] |= 0x80;
		else
			ie[8] &= 0x7f;
	}
}

/* A QoS Null frame from the access point to a peer's TID ends a U-APSD service period. */
void wlanWmmSendQosNull(struct WlanConnection *connection, int moreData, unsigned int tid)
{
	OsaBuffer *buffer;
	struct WlanReplyFrame *request;
	WlanWirelessFrame *frame;
	unsigned int sequence;

	if (connection == NULL)
		return;
	while ((buffer = wlanMgmtAllocFrame(connection, 300)) == NULL)
		osaTaskSleep(1);
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	wlanInitMgmtHeader(frame, 2, 12, connection->peerAddress, connection->interfaceAddress,
			   connection->localAddress);
	frame->duration[0] = 2;
	frame->duration[1] = 1;
	sequence = wlanQosNullSequence++ << 4;
	sequence |= ((frame->sequenceControl[1] << 8) | frame->sequenceControl[0]) & 0xf;
	frame->sequenceControl[0] = (uint8_t)sequence;
	frame->sequenceControl[1] = (uint8_t)(sequence >> 8);
	frame->frameControl[1] |= 2;
	if (moreData != 0)
		frame->frameControl[1] |= 0x20;
	else
		frame->frameControl[1] &= 0xdf;
	buffer->dataOffset = 28;
	request = (struct WlanReplyFrame *)buffer->data;
	__aeabi_memclr4(request, offsetof(struct WlanReplyFrame, frameLength));
	request->kind = 49;
	request->frame = frame->bodyLength;
	request->descriptor.header.bssType = (request->descriptor.header.bssType >> 3 << 3) | (tid & 7) | 0x10;
	request->descriptor.header.wire.packetLength[0] = wlanTxDataRate(connection, 0);
	request->queue = wlanAccessCategoryToQueue(wlanTidToAccessCategory(tid));
	if (wlanTxSubmit(buffer) != 1)
		osa_BufferFree(buffer);
}

/* The defaults of the parameter element the access point announces without WMM. */
void wlanWmmInitDefaults(void)
{
	wlanWmmDefaultTime = 500;
	wlanEdcaApplyHook = wlanWmmApplyApEdca;
}

/* The two TID slots of a queue are pointed at it and cleared. */
void wlanWmmSetupQueue(unsigned int queue)
{
	struct WlanWmmQueue *record = &wlanWmm.queues[queue];
	unsigned int i;

	record->exhausted = 0;
	record->fallback = 0;
	record->usedTime = 0;
	record->admittedTime = 0;
	*(uint32_t *)&record->edca = 0;
	for (i = 0; i < 2; i = (uint8_t)(i + 1)) {
		unsigned int tid = wlanWmmRings[2 * queue + i];
		struct WlanWmmSlot *slot = &wlanWmm.slots[tid];

		record->slots[i] = slot;
		slot->ownQueue = queue;
		slot->activeQueue = queue;
		slot->nextSequence = 0;
		slot->lastSequence = 0;
		slot->activeRing = tid;
		slot->fallbackQueue = queue;
		slot->statsRing = 0;
	}
}

/* Bits of the TS Info field's first byte and the response of HostCmd_CMD_WMM_QUEUE_STATS. */
struct __attribute__((packed)) WlanWmmStatsResponse {
	uint8_t action;			/* 0 start, 1 stop, 2 get */
	uint8_t bin;
	uint16_t packets;
	uint16_t lost;
	uint8_t averageQueueDelay[4];		/* unaligned words, set with __aeabi_uwrite4 */
	uint8_t averageTransmitDelay[4];
	uint16_t usedTime;
	uint16_t policedTime;
	uint8_t histogram[14];
	uint16_t policed;
};

/*
 * HostCmd_CMD_WMM_QUEUE_STATS: a queue's bin is cleared (actions 0 and 1) or read
 * and cleared (action 2). Returns the length of the response, 0 on error.
 */
int wlanWmmStatsCommand(uint8_t *body, struct WlanWmmStats *bins, uint16_t *policed)
{
	struct WlanWmmStatsResponse *response = (struct WlanWmmStatsResponse *)body;
	struct WlanWmmStats *bin;
	unsigned int queue = body[1];
	unsigned int level;
	int length;

	if (queue >= 4)
		return 0;
	level = tx_interrupt_control(TX_INT_DISABLE);
	bin = &bins[queue];
	if (response->action == 0 || response->action == 1) {
		__aeabi_memclr4(bin, sizeof(*bin));
		length = 2;
	} else if (response->action == 2) {
		unsigned int packets;
		unsigned int delivered;
		unsigned int transmitDelay;

		response->lost = bin->lost;
		packets = bin->packets;
		response->packets = packets;
		delivered = packets - response->lost;
		if (delivered == 0) {
			__aeabi_uwrite4(0, body + 6);
			transmitDelay = 0;
		} else {
			__aeabi_uwrite4(bin->queueDelay / delivered, body + 6);
			transmitDelay = bin->transmitDelay / delivered;
		}
		__aeabi_uwrite4(transmitDelay, body + 10);
		response->usedTime = bin->usedTime;
		response->policedTime = bin->policedTime;
		__aeabi_memcpy(response->histogram, bin->histogram, 14);
		response->policed = *policed;
		__aeabi_memclr4(bin, sizeof(*bin));
		length = 34;
		*policed = 0;
	} else {
		length = 0;
	}
	tx_interrupt_control(level);
	return length;
}

/* Without WMM the four queues, the three information elements and the parameter element are reset. */
void wlanWmmResetDefaults(void *info1, void *info2, void *info3, void *element, struct WlanWmmEdca *edca,
			 uint16_t *times, unsigned int cwMin, unsigned int cwMax)
{
	unsigned int queue;

	__aeabi_memclr(info1, 9);
	__aeabi_memclr(info2, 9);
	__aeabi_memclr(info3, 9);
	__aeabi_memclr(element, 26);
	edca->aifsn = 2;
	edca->ecwMin = cwMin;
	edca->ecwMax = cwMax;
	edca->txop = 0;
	for (queue = 0; queue <= 7; queue = (uint8_t)(queue + 1)) {
		wlanWmmSetEdca(queue, edca);
		times[queue] = wlanWmmDefaultTime;
	}
}

/* A WMM parameter element's header: vendor element, Microsoft OUI, type 2, subtype 1, version 1. */
void wlanWmmBuildParameterElement(uint8_t *ie)
{
	uint8_t oui[4] = { 0x00, 0x50, 0xf2, 0x02 };	/* Microsoft OUI, type 2 */

	ie[0] = 221;
	ie[1] = 24;
	__aeabi_memcpy(ie + 2, oui, 4);
	ie[6] = 1;
	ie[7] = 1;
	ie[8] = 0;
}

/* The parameter element an AP announces without any of its own (the WMM defaults). */
void wlanWmmFillParameterElement(uint8_t *ie)
{
	struct WlanWmmAcRecord *ac = (struct WlanWmmAcRecord *)&ie[10];

	wlanWmmBuildParameterElement(ie);
	ie[9] = 0;
	ac[0].aifsn = 3;		/* AC_BE: AIFSN 3, CWmin 15, CWmax 1023 */
	ac[0].acm = 0;
	ac[0].aci = 0;
	ac[0].reserved = 0;
	ac[0].ecwMin = 4;
	ac[0].ecwMax = 10;
	ac[0].txopLimit = 0;
	ac[1].aifsn = 7;		/* AC_BK: AIFSN 7, CWmin 15, CWmax 1023 */
	ac[1].acm = 0;
	ac[1].aci = 1;
	ac[1].reserved = 0;
	ac[1].ecwMin = 4;
	ac[1].ecwMax = 10;
	ac[1].txopLimit = 0;
	ac[2].aifsn = 2;		/* AC_VI: AIFSN 2, CWmin 7, CWmax 15, TXOP 6016 us */
	ac[2].acm = 0;
	ac[2].aci = 2;
	ac[2].reserved = 0;
	ac[2].ecwMin = 3;
	ac[2].ecwMax = 4;
	ac[2].txopLimit = 188;
	ac[3].aifsn = 2;		/* AC_VO: AIFSN 2, CWmin 3, CWmax 7, TXOP 3264 us */
	ac[3].acm = 0;
	ac[3].aci = 3;
	ac[3].reserved = 0;
	ac[3].ecwMin = 2;
	ac[3].ecwMax = 3;
	ac[3].txopLimit = 102;
}

/* The transmit TID of a queue's slot. */
unsigned int wlanWmmTid(unsigned int priority)
{
	return wlanWmm.slots[priority].activeRing;
}

/* The next sequence number of a TID, as it stands. */
unsigned int wlanGroupNextSequence(unsigned int tid)
{
	return wlanWmm.slots[tid].nextSequence;
}

/* The sequence number of the last frame of a TID that went out. */
unsigned int wlanWmmLastSequence(unsigned int tid)
{
	return wlanWmm.slots[tid].lastSequence;
}

/* The AIFSN of a queue (2 without WMM). */
unsigned int wlanWmmAifsn(unsigned int queue)
{
	if (wlanWmm.enabled != 0 && queue < 4)
		return wlanWmm.queues[queue].edca.aifsn;
	return 2;
}

/*
 * The sequence number of the next frame of a TID. A peer that is not
 * a QoS peer shares one sequence space among a TID's access category.
 */
unsigned int wlanNextSequence(struct WlanConnection *connection, unsigned int tid)
{
	struct WlanWmmSlot *slot = &wlanWmm.slots[tid];
	unsigned int sequence = slot->nextSequence;

	if (wlanConnectionIsHt(connection) != 0) {
		slot->nextSequence++;
	} else {
		switch (tid) {
		case 0:
		case 3:
			wlanWmm.slots[0].nextSequence++;
			wlanWmm.slots[3].nextSequence++;
			break;
		case 1:
		case 2:
			wlanWmm.slots[1].nextSequence++;
			wlanWmm.slots[2].nextSequence++;
			break;
		case 4:
		case 5:
			wlanWmm.slots[4].nextSequence++;
			wlanWmm.slots[5].nextSequence++;
			break;
		case 6:
		case 7:
			wlanWmm.slots[6].nextSequence++;
			wlanWmm.slots[7].nextSequence++;
			break;
		default:
			break;
		}
	}
	return sequence;
}

/* A queue's frame that never got a delivery: counted as sent and lost. */
__attribute__((noinline)) void wlanWmmCountLost(unsigned int queue)
{
	wlanWmmStats[queue].packets++;
	wlanWmmStats[queue].lost++;
}

/* The delays of a delivered frame go into its queue's bin: the sums and the histogram. */
void wlanWmmRecordDelay(struct WlanWmmStats *bin, unsigned int queueDelay, unsigned int transmitDelay)
{
	bin->packets++;
	bin->transmitDelay += transmitDelay;
	bin->queueDelay += queueDelay;
	if (queueDelay > 5000) {
		if (queueDelay > 10000) {
			if (queueDelay > 20000) {
				if (queueDelay > 30000) {
					if (queueDelay > 40000) {
						if (queueDelay > 50000)
							bin->histogram[6]++;
						else
							bin->histogram[5]++;
					} else {
						bin->histogram[4]++;
					}
				} else {
					bin->histogram[3]++;
				}
			} else {
				bin->histogram[2]++;
			}
		} else {
			bin->histogram[1]++;
		}
	} else {
		bin->histogram[0]++;
	}
}

/*
 * A frame left the queue. Its delays (from when it was queued, and from the previous
 * frame's end) go into the queue's statistics, the medium time it took and the part
 * over the admitted time into the used and policed times. Without WMM every frame counts for queue 1.
 */
void wlanWmmFrameDone(uint8_t *data, unsigned int queue, int delivered, unsigned int usedTime,
		      unsigned int policedTime)
{
	struct WlanWmmTiming *timing = &wlanWmmTiming[queue];
	unsigned int statsQueue;
	unsigned int level;
	uint32_t high, now, last, transmitDelay, queueDelay, limit;
	uint32_t queuedAt;

	if (timing->pending == 0)
		return;
	statsQueue = wlanWmm.enabled != 0 ? queue : 1;
	level = sysIrqDisable();
	timing->pending--;
	queuedAt = *(uint32_t *)(data + 100);
	if (queuedAt == 0) {
		wlanWmmCountLost(statsQueue);
		sysIrqRestore(level);
		return;
	}
	sysIrqRestore(level);
	msi_wl_GetMCUCoreTimerTxTSF(&high, &now);
	last = data[106] != 0 ? *(uint32_t *)(data + 100) : timing->lastTime;
	timing->lastTime = now;
	if (delivered != 0) {
		transmitDelay = now >= last ? now - last : 5000;
		queuedAt = *(uint32_t *)(data + 100);
		queueDelay = queuedAt <= now ? now - queuedAt : transmitDelay;
		queueDelay += 1000 * *(uint16_t *)(data + 104);
		limit = wlanRadioConfig->wmmQueueTime[queue] << 10;
		if (limit <= queueDelay)
			queueDelay = limit;
		if (limit <= transmitDelay)
			transmitDelay = limit;
		wlanWmmRecordDelay(&wlanWmmStats[statsQueue], queueDelay, transmitDelay);
	} else {
		wlanWmmCountLost(statsQueue);
	}
	wlanWmmStats[statsQueue].usedTime += usedTime;
	wlanWmmStats[statsQueue].policedTime += policedTime;
}

/* The medium time a transmitted frame took: the airtime of each rate it was sent at. */
uint32_t wlanTxAirtime(struct WlanConnection *connection, struct WlanTxStatus *status)
{
	uint8_t attempts[32];
	const uint8_t *frame = WMM_TX_FRAME(status->data);
	uint32_t airtime = 0;
	unsigned int rate;

	__aeabi_memclr4(attempts, sizeof(attempts));
	wlanTxRateAttempts(status, attempts);
	for (rate = 0; rate < 0x1f; rate = (uint8_t)(rate + 1)) {
		while (attempts[rate] != 0) {
			int time = romLibConfigureState(rate, (uint16_t)*(uint32_t *)(status->data + 8),
							(frame[1] << 8) | frame[0], wlanWmm.enabled);

			airtime += time + wlanConnectionAckTime(connection, rate,
								 (uint16_t)*(uint32_t *)(status->data + 8));
			attempts[rate]--;
		}
	}
	return airtime;
}

/* The medium time of a frame that was sent at the rate in use, as many times as its statistics ring saw it validated. */
int wlanTxAirtimeValidated(struct WlanConnection *connection, struct WlanTxStatus *status)
{
	const uint8_t *frame = WMM_TX_FRAME(status->data);
	unsigned int ring = wlanWmm.slots[status->data[0] & 7].statsRing;
	int count = macTxStatsAddValidated(ring, &wlanWmm.validatedSeen[ring]);
	uint8_t rate = connection->peerLink.rateControl.rates[connection->peerLink.rateControl.current];

	return count * romLibConfigureState(rate, (uint16_t)*(uint32_t *)(status->data + 8),
					    (frame[1] << 8) | frame[0], 1);
}

/*
 * A frame was queued: which slot and queue it uses, whether the queue's medium time is used up,
 * and how many frames the queue now has outstanding. 0: the frame is dropped.
 */
int wlanWmmSetQueue(struct WlanConnection *connection, uint8_t *data, unsigned int priority,
		    unsigned int delay)
{
	struct WlanWmmSlot *slot = &wlanWmm.slots[priority];
	unsigned int queue;
	unsigned int ring;
	int accepted = 1;
	unsigned int subtype;

	*(uint16_t *)(data + 4) = 0;
	queue = slot->ownQueue;
	ring = slot->activeQueue;
	if (connection->interfaceType == 1) {
		ring = 0;
	} else if (wlanWmm.enabled != 0) {
		unsigned int limit = wlanRadioConfig->wmmQueueTime[queue];

		accepted = priority != 255;
		if (limit != 0xffff) {
			if (limit <= delay) {
				wlanWmmCountLost(queue);
				wlanTxStatsCountDropped(data[2], 1);
				return 0;
			}
			*(uint16_t *)(data + 4) = 0;
		}
		if (!accepted)
			return accepted;
	}
	data[108] = ring;
	subtype = WMM_TX_FRAME(data)[2] >> 4;
	if ((subtype == 0 || subtype == 8 || subtype == 12) && accepted) {
		struct WlanWmmTiming *timing = &wlanWmmTiming[queue];
		unsigned int level;

		*(uint16_t *)(data + 104) = delay;
		level = sysIrqDisable();
		if ((int8_t)timing->pending > 0) {
			data[106] = 0;
		} else {
			timing->pending = 0;
			data[106] = 1;
		}
		timing->pending++;
		sysIrqRestore(level);
	}
	return accepted;
}

/* A frame that was dropped before it went out counts as lost in its queue's statistics. */
void wlanTxCountDropped(OsaBuffer *buffer)
{
	uint8_t *data;
	unsigned int queue;
	unsigned int subtype;

	if (((struct WlanConnection *)buffer->context)->mode == 2)
		return;
	data = (uint8_t *)buffer + buffer->dataOffset;
	queue = wlanWmm.slots[data[0] & 7].ownQueue;
	subtype = WMM_TX_FRAME(data)[2] >> 4;
	if (subtype == 0 || subtype == 8 || subtype == 12)
		wlanWmmFrameDone(data, queue, 0, 0, 0);
}

/*
 * A transmitted frame's medium time is charged to its queue when admission control is on, and
 * the queue falls back once it exceeds the medium time that was admitted.
 */
void wlanWmmTxDone(struct WlanConnection *connection, struct WlanTxStatus *status)
{
	uint8_t *data = status->data;
	const uint8_t *frame = WMM_TX_FRAME(data);
	unsigned int used = 0;
	unsigned int policed = 0;
	int reapply = 0;
	unsigned int slotIndex;
	unsigned int queue;
	unsigned int subtype;

	romGetTimerTimestamp();
	slotIndex = status->data[0] & 7;
	queue = wlanWmm.slots[slotIndex].ownQueue;
	wlanWmm.slots[slotIndex].lastSequence = ((frame[25] << 8) | frame[24]) >> 4;
	subtype = frame[2] >> 4;
	/* plain data frames are done without airtime; QoS data and QoS null are charged */
	if (subtype != 0) {
		struct WlanWmmQueue *record = &wlanWmm.queues[queue];

		if (subtype != 8 && subtype != 12)
			return;
		if (record->edca.acm) {
			int airtime;
			unsigned int medium;
			unsigned int admitted;

			if (*(uint32_t *)(data + 8) & 0x20000000)
				airtime = wlanTxAirtimeValidated(connection, status);
			else
				airtime = wlanTxAirtime(connection, status);
			medium = (unsigned int)(airtime << 11) >> 16;
			admitted = record->admittedTime;
			if (admitted == 0 || record->exhausted != 0) {
				policed = medium;
			} else {
				unsigned int total;

				used = medium;
				total = (uint16_t)(record->usedTime + medium);
				record->usedTime = total;
				if (total > admitted) {
					policed = (uint16_t)(total - admitted);
					record->exhausted = 1;
					reapply = 1;
				}
			}
		}
	}
	wlanWmmFrameDone(status->data, queue, status->result == 0, used, policed);
	if (reapply != 0)
		wlanWmmApplyQueue(queue);
}
