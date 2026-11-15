/*
 * The scan engine of the management layer (0x36ad8..0x37a34): the host's scan command (Marvell's
 * HostCmd_CMD_802_11_SCAN, 0x0006), the periodic background scan of the host's command 0x006b, and
 * the channel loop they share: one request to the channel scheduler (slot.h) per scan, a
 * probe request per SSID on each channel, the dwell time as the request's duration. Marvell
 * published no source for it; the names are the firmware's own placeholder names (macMgmt*Entry
 * for the loop, macMgmt*Table for the background scan), kept, and the file name and the names of
 * the structures are inferred.
 */
#include <stddef.h>
#include "robin/p2pgo.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/hostps.h"
#include "robin/ie.h"
#include "robin/apsta.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/scan.h"
#include "robin/slot.h"
#include "robin/soc.h"
#include "robin/stamachine.h"
#include "robin/sync.h"
#include "robin/uapbss.h"
#include "robin/wlantask.h"

#define SCAN_LE16(p)	((p)[0] | ((p)[1] << 8))

/* The ROM's. */
void romLibSyncContext(unsigned int mask);
struct WlanConnection *p2pSqramCheckContext(OsaBuffer *buffer, uint32_t *channel);	/* SQRAM, veneer 0x52dc4 */

struct WlanScanState wlanScanState __attribute__((section(".itcm.bss")));			/* 0x578c0 */
struct WlanScan wlanCommandScan __attribute__((section(".itcm.bss")));				/* 0x578d4 */
struct WlanScanResults wlanCommandScanResults __attribute__((section(".itcm.bss")));		/* 0x57adc */
uint8_t wlanCommandScanWindow[2000] __attribute__((section(".itcm.bss")));			/* 0x57bde */
uint8_t wlanBackgroundScanWindow[1816] __attribute__((section(".itcm.bss")));			/* 0x583b0 */
struct WlanScanConfig wlanScanSlots[2] __attribute__((section(".itcm.bss")));			/* 0x58ac8 */
struct WlanBackgroundScanState wlanBackgroundScanState __attribute__((section(".itcm.bss")));	/* 0x58f88 */
struct WlanScan wlanBackgroundScan __attribute__((section(".itcm.bss")));			/* 0x590a4 */
struct WlanScanConfig *wlanScanConfig __attribute__((section(".itcm.rw"))) = &wlanScanSlots[0];	/* 0x55178 */
static struct WlanInitStage wlanScanInitStage __attribute__((section(".itcm.rw")));		/* 0x5517c */

/* 0x36ad8: sets the radio to the scan's next channel and queues the probe requests of it. */
__attribute__((noinline)) int macMgmtFormatEntry(void)
{
	struct WlanScan *scan = wlanScanState.scan;
	struct WlanConnection *connection = wlanScanState.connection;
	const struct WlanScanChannel *channel;
	unsigned int index;
	int result;

	if (connection->interfaceType == WLAN_IF_AP && connection->mode == 1) {
		if (p2pBssActiveGet(connection) != 0)
			return wlanSlotAbort(scan->slot, 6);
		if (p2pDirectInspectState() != 0 && macMgmtFormatTable(scan) &&
		    !macMgmtInspectTable(scan->channelIndex))
			return p2pSqramScheduleState(connection, scan->slot);
	}
	index = scan->channelIndex;
	if (index >= scan->channelCount)
		return wlanSlotAbort(scan->slot, 6);
	wlanScanState.completed = 0;
	wlanScanState.queued = 0;
	wlanScanState.dwellDone = 0;
	wlanScanState.allQueued = 0;
	wlanScanState.broadcastSent = 0;
	wlanScanState.repeat = 0;
	wlanScanState.ssidIndex = 0;
	channel = &scan->channels[index];
	if (!wlanSlotSetChannel(channel->channelFlags, channel->channel) ||
	    wlanSlotSetDuration(scan->slot, 1000 * SCAN_LE16(channel->maxDwell)) != WLAN_SLOT_OK)
		return wlanSlotAbort(scan->slot, 6);
	if (channel->flags & 1)
		result = 1;
	else
		result = macMgmtManageEntry();
	wlanScanState.allQueued = result;
	return result;
}

/* 0x36ba2: a probe request has been sent; more are queued, and the channel ends when the last has. */
int macMgmtInspectEntry(void)
{
	macMgmtFilterEntry();
	if (wlanScanState.queued != 0 && wlanScanState.completed < wlanScanState.queued) {
		wlanScanState.completed++;
		while (wlanScanState.allQueued == 0 && wlanScanState.queued - wlanScanState.completed < 3)
			wlanScanState.allQueued = macMgmtManageEntry();
		if (macMgmtProcessTable()) {
			wlanScanState.scan->channelIndex++;
			macMgmtFormatEntry();
		}
	}
	return macMgmtHandleTable();
}

/* 0x36bf0: what a probe request's release hook returns, the function that goes on with the scan. */
int macMgmtTrackEntry(OsaBuffer *buffer)
{
	return (int)(uintptr_t)macMgmtInspectEntry;
}

/*
 * 0x36bf4: queues the next probe request of the channel: the SSIDs one after the other, each
 * probes times, and the wildcard one first. 1: there is none left.
 */
__attribute__((noinline)) int macMgmtManageEntry(void)
{
	struct WlanScan *scan = wlanScanState.scan;
	struct WlanScanSsid *ssid = NULL;
	uint8_t *elements = NULL;
	unsigned int length;
	unsigned int broadcast = scan->broadcast;
	unsigned int count;
	int result;

	if (wlanScanState.repeat == scan->probes) {
		if (broadcast == 0 || wlanScanState.broadcastSent != 0)
			wlanScanState.ssidIndex++;
		else
			wlanScanState.broadcastSent = 1;
		wlanScanState.repeat = 0;
	}
	count = scan->ssidCount;
	if (wlanScanState.ssidIndex < count) {
		if (broadcast == 0 || wlanScanState.broadcastSent != 0) {
			while (wlanScanState.ssidIndex < count) {
				if (scan->ssids[wlanScanState.ssidIndex].maxLength == 0) {
					ssid = &scan->ssids[wlanScanState.ssidIndex];
					break;
				}
				wlanScanState.ssidIndex++;
			}
			if (ssid == NULL)
				return 1;
		}
	} else {
		if (broadcast == 0 || wlanScanState.broadcastSent != 0)
			return 1;
	}
	length = wlanIeBufferGet(&scan->ies, &elements);
	result = macMgmtBuildState(wlanScanState.connection, NULL, ssid != NULL ? ssid->ssid : NULL, elements,
				   length, 19, (int (*)(OsaBuffer *))macMgmtTrackEntry);
	if (result == 0) {
		wlanScanState.repeat++;
		wlanScanState.queued++;
	}
	return 0;
}

/* 0x36cba: a frame received during the scan: a beacon or probe response goes to the results. */
int macMgmtAcquireEntry(uint32_t handle, uint32_t event, OsaBuffer *buffer, uint32_t unused,
			uint32_t argument)
{
	uint32_t channel = unused;
	const uint8_t *frame;
	unsigned int subtype;

	if (buffer == NULL)
		return 0;
	frame = (const uint8_t *)buffer + buffer->dataOffset;
	subtype = frame[2] >> 4;
	if (subtype == 2) {
		channel = 0;
		if (p2pSqramCheckContext(buffer, &channel) != NULL)
			macMgmtDispatchState(buffer, argument, channel);
	} else if (subtype == 5 || subtype == 8) {
		fmCoexFormatBuffer((uintptr_t)buffer->context, (WlanWirelessFrame *)frame, argument, wlanScanState.scan,
				   wlanScanState.scan->channelIndex);
		return 1;
	}
	return 0;
}

/* 0x36d12: what the channel scheduler tells the scan about its request (WlanSlotRequest.callback). */
int macMgmtReleaseEntry(uint32_t handle, int event, uint32_t argument)
{
	struct WlanScan *scan = (struct WlanScan *)argument;
	struct WlanScanResults *results;
	void (*callback)(struct WlanScanResults *);

	switch (event) {
	case 1:
	case 2:
		if (wlanScanState.connection->interfaceType == WLAN_IF_AP)
			p2pSqramProcessState(wlanScanState.connection, 3, argument, event);
		macMgmtFilterPacket(40, 2);
		macMgmtFilterPacket(40, 3);
		if (wlanScanState.connection->interfaceType == WLAN_IF_AP)
			MAC_REG(0x300) |= 0x2000;
		break;
	case 3:
		macMgmtFilterEntry();
		wlanScanState.scan = scan;
		macMgmtConfigureTable();
		macMgmtFormatEntry();
		macMgmtHandleTable();
		break;
	case 4:
		macMgmtFilterEntry();
		wlanScanState.dwellDone = 1;
		if (macMgmtProcessTable()) {
			scan->channelIndex++;
			macMgmtFormatEntry();
		} else {
			wlanScanState.completed = wlanScanState.queued;
			macTxPostEvent(0x10);
		}
		macMgmtHandleTable();
		break;
	case 6:
	case 11:
		scan->slot = 0;
		results = scan->results;
		callback = (void (*)(struct WlanScanResults *))__aeabi_uread4(results->callback);
		callback(results);
		macMgmtUpdateTable();
		wlanIeBufferFree(&scan->ies);
		break;
	case 9:
		macMgmtFilterEntry();
		macMgmtCheckTable();
		macMgmtHandleTable();
		break;
	default:
		break;
	}
	return WLAN_SLOT_OK;
}

/* 0x36dd4: the end of a host scan: the event message (0x8006) names the results. */
OsaBuffer *macMgmtSetupEntry(struct WlanScanResults *results)
{
	OsaBuffer *message = wlanHostMessageAlloc(wlanScanState.connection);
	uint8_t *data;

	if (message != NULL) {
		data = (uint8_t *)message + message->dataOffset;
		data[0] = 6;
		data[1] = 0x80;
		__aeabi_uwrite4((int)(uintptr_t)results, data + 9);
		data[6] = 0;
		data[7] = 0;
		return (OsaBuffer *)(uintptr_t)wlanHostMessagePost(message);
	}
	return message;
}

/*
 * 0x36e08: the host's answer to a scan: the length of the descriptors, their number and them, then
 * the TSF timestamps (TLV 0x0113) and the channels and bands (TLV 0x012a) of each. The length of the
 * answer; 3 and no descriptor when there are none.
 */
int macMgmtClearEntry(uint8_t *out, const struct WlanScanResults *results)
{
	unsigned int length;
	unsigned int count;
	uint8_t *tlv;
	uint8_t *channels;
	unsigned int offset;
	unsigned int i;

	if (results != NULL && __aeabi_uread4(&results->data) != 0 && (length = __aeabi_uread4(&results->length)) != 0 &&
	    results->count != 0) {
		count = results->count;
		out[0] = length;
		out[1] = length >> 8;
		out[2] = count;
		__aeabi_memcpy(out + 3, (const void *)__aeabi_uread4(&results->data), __aeabi_uread4(&results->length));
		length = __aeabi_uread4(&results->length);
		tlv = out + length + 3;
		tlv[0] = 19;
		tlv[1] = 1;
		tlv[2] = 8 * count;
		tlv[3] = (8 * count) >> 8;
		offset = length + 7 + 8 * count;
		for (i = 0; i < out[2]; i++)
			__aeabi_memcpy(tlv + 4 + 8 * i, results->records[i].timestamp, 8);
		channels = out + offset;
		channels[0] = 42;
		channels[1] = 1;
		channels[2] = 2 * count;
		channels[3] = (2 * count) >> 8;
		for (i = 0; i < out[2]; i++) {
			channels[4 + 2 * i] = results->records[i].channelBand[0];
			channels[5 + 2 * i] = results->records[i].channelBand[1];
		}
		return offset + 2 * count + 4;
	}
	out[0] = 0;
	out[1] = 0;
	out[2] = 0;
	return 3;
}

/* 0x36ef4: the host's scan command into the scan (the BSS type and BSSID, then TLVs). */
int macMgmtConvertEntry(int unused, uint8_t *command, int16_t length, struct WlanScan *scan)
{
	static const uint8_t anyBssid[8] = { 0 };
	struct WlanIeBuffer *ies = &scan->ies;
	unsigned int remaining;
	uint8_t *tlv;
	unsigned int type;
	unsigned int size;
	uint32_t channel = 0;
	uint32_t extra;

	__aeabi_memclr4(scan, sizeof(*scan));
	wlanIeBufferAlloc(ies);
	scan->probes = 1;
	__aeabi_memcpy(scan, command, 7);
	remaining = (uint16_t)(length - 7);
	tlv = command + 7;
	while (remaining != 0) {
		type = SCAN_LE16(tlv);
		size = (uint16_t)(SCAN_LE16(tlv + 2) + 4);
		switch (type) {
		case 0:
		case 274:
			fmCoexConvertEntry(type, tlv, (uint8_t *)&scan->ssids[scan->ssidCount], &scan->ssidCount,
					   WLAN_SCAN_SSIDS);
			break;
		case 1:
			wlanTlvCopyOfType(1, tlv, scan->rates, WLAN_SCAN_RATES);
			break;
		case 45:
		case 127:
		case 221:
			wlanIeBufferAppendTlv(ies, tlv);
			break;
		case 257:
			wlanTlvChannelListParse(tlv, (uint8_t *)scan->channels, &channel);
			macMgmtPrepareContext(NULL, &extra);
			break;
		case 258:
			wlanTlvNumProbesParse(tlv, (uint8_t *)&scan->probes);
			break;
		case 266:
			wlanIeBufferAppend(ies, tlv + 4, SCAN_LE16(tlv + 2));
			break;
		case 291:
			__aeabi_memcpy(scan->bssid, tlv + 4, 6);
			break;
		case 330:
			tlv[2] = 26;
			tlv[3] = 0;
			tlv[0] = 45;
			tlv[1] = 0;
			wlanIeBufferAppendTlv(ies, tlv);
			break;
		case 335:
			tlv[0] = 127;
			tlv[1] = 0;
			wlanIeBufferAppendTlv(ies, tlv);
			break;
		default:
			break;
		}
		remaining = (uint16_t)(remaining - size);
		tlv += size;
	}
	if (memcmp(scan->bssid, anyBssid, 6) == 0)
		__aeabi_memset4(scan->bssid, 6, 0xff);
	return 1;
}

/* 0x37042: starts a scan: asks the channel scheduler for the radio, delay microseconds from now. */
uint32_t macMgmtVerifyEntry(struct WlanConnection *connection, struct WlanScan *scan, uint32_t delay)
{
	struct WlanSlotParams params;
	unsigned int i;

	scan->channelCount = 0;
	do {
		if (scan->channels[scan->channelCount].channel == 0)
			break;
		scan->channelCount++;
	} while (scan->channelCount < WLAN_SCAN_CHANNELS);
	if (scan->channelCount == 0)
		return 0;
	for (i = 0; i < scan->ssidCount; i++) {
		if (scan->ssids[i].maxLength != 0) {
			scan->broadcast = 1;
			break;
		}
	}
	if (i == scan->ssidCount && scan->ssidCount == 0)
		scan->broadcast = 1;
	macMgmtParseState(scan->rates, WLAN_SCAN_RATES);
	wlanScanState.connection = connection;
	__aeabi_memclr4(&params, sizeof(params));
	params.callback = (int (*)(uint32_t, int, uint32_t))macMgmtReleaseEntry;
	params.context = (uint32_t)(uintptr_t)macMgmtAcquireEntry;
	params.connection = connection;
	params.delay = delay;
	params.argument = (uint32_t)(uintptr_t)scan;
	params.tag = 100000;
	params.elapsed = &scan->waited;
	params.flags |= 9;
	scan->slot = wlanSlotSubmit(&params);
	if (scan->slot == 0)
		macMgmtUpdateTable();
	return scan->slot;
}

/* 0x370f2: HostCmd_CMD_802_11_SCAN. */
int hostCmd0006(struct WlanConnection *connection, uint8_t parameter, uint8_t *command, int16_t length)
{
	if (wlanCommandScan.slot == 0 && macMgmtConvertEntry(parameter, command, length, &wlanCommandScan) != 0) {
		__aeabi_memclr4(&wlanCommandScanResults, sizeof(wlanCommandScanResults));
		wlanCommandScan.parameter = parameter;
		wlanCommandScan.results = &wlanCommandScanResults;
		__aeabi_memclr(wlanCommandScanWindow, sizeof(wlanCommandScanWindow));
		__aeabi_uwrite4((int)(uintptr_t)wlanCommandScanWindow, &wlanCommandScanResults.data);
		__aeabi_uwrite4(sizeof(wlanBackgroundScanWindow), &wlanCommandScanResults.size);
		wlanCommandScanResults.capacity = WLAN_SCAN_RECORDS;
		__aeabi_uwrite4((int)(uintptr_t)macMgmtSetupEntry, wlanCommandScanResults.callback);
		return macMgmtVerifyEntry(connection, &wlanCommandScan, 0) != 0;
	}
	return 0;
}

/* 0x37176: the access point's machine is in its fifth state: the channel's time is up. */
void macMgmtAdjustEntry(struct WlanConnection *connection)
{
	if (connection->interfaceType == WLAN_IF_AP && connection->smState == connection->smStates + 72) {
		if (wlanScanState.completed < wlanScanState.queued)
			wlanScanState.completed = wlanScanState.queued;
		wlanScanState.allQueued = 1;
		wlanScanState.dwellDone = 1;
	}
}

/* 0x3719a: the loop's semaphore. */
int macMgmtRecordEntry(void)
{
	__aeabi_memclr4(&wlanScanState, 20);
	return osaSemaphoreInit(&wlanScanState.semaphore, 1, 12);
}

__attribute__((noinline)) int macMgmtFilterEntry(void)
{
	return osaSemaphoreWait(wlanScanState.semaphore, 0xffffffff);
}

__attribute__((noinline)) int macMgmtHandleTable(void)
{
	return osaSemaphoreRelease(wlanScanState.semaphore);
}

/* 0x371d4: the channel is done: every probe request is queued and has been sent, and the time is up. */
__attribute__((noinline)) int macMgmtProcessTable(void)
{
	if (wlanScanState.allQueued != 0 && wlanScanState.queued == wlanScanState.completed)
		return wlanScanState.dwellDone != 0;
	return 0;
}

__attribute__((noinline)) int macMgmtUpdateTable(void)
{
	macMgmtFilterPacket(41, 2);
	macMgmtFilterPacket(41, 3);
	return 1;
}

__attribute__((noinline)) void macMgmtConfigureTable(void)
{
	romLibSyncContext(300);
	wlanIbssTimingEnd();
}

/* 0x37216: ends the channel at once. */
__attribute__((noinline)) int macMgmtCheckTable(void)
{
	wlanScanState.completed = wlanScanState.queued;
	wlanScanState.allQueued = 1;
	if (wlanScanState.scan->slot != 0) {
		wlanSlotClearTimer(wlanScanState.scan->slot);
		wlanScanState.dwellDone = 1;
	}
	if (macMgmtProcessTable()) {
		wlanScanState.scan->channelIndex++;
		macMgmtConfigureTable();
		return macMgmtFormatEntry();
	}
	return 0;
}

/*
 * 0x3726c: what the background scan does when something happens (the event: 0 a connection ended,
 * 1 and 2 one is up or idle, 3..5 the host's configuration, 6 and 7 a scan has been asked for,
 * 8 the radio is free, 9 nothing): its mode changes, or not. The numbers of the events and of the
 * modes are inferred from the conditions.
 */
void macMgmtValidateTable(int event)
{
	struct WlanBackgroundScanState *state = &wlanBackgroundScanState;
	struct WlanConnection *connection = state->connection;
	int mode;

	switch (event) {
	case 0:
		if (connection != NULL && connection->interfaceType == WLAN_IF_AP && connection->p2pState == 3) {
			if (state->mode != 0)
				return;
			mode = 1;
		} else if (wlanConnectionGetCount() == 0 && wlanAnyConnectionFlags2Bit4() == 0 && state->mode != 2) {
			mode = 0;
		} else {
			return;
		}
		break;
	case 1:
		if (connection != NULL && connection->interfaceType == WLAN_IF_AP && connection->mode != 0 &&
		    connection->p2pState != 3)
			return;
		if ((wlanConnectionGetCount() != 1 || wlanAnyConnectionFlags2Bit4() != 0) &&
		    wlanConnectionGetCount() != 0)
			return;
		if (state->mode != 0)
			return;
		mode = 1;
		break;
	case 2:
		if (state->mode != 0)
			return;
		if (connection != NULL && connection->interfaceType == WLAN_IF_AP && connection->mode != 0)
			return;
		mode = 1;
		break;
	case 3:
		mode = 3;
		wlanScanConfig = &wlanScanSlots[0];
		break;
	case 4:
		wlanScanConfig = &wlanScanSlots[1];
		mode = 4;
		break;
	case 5:
		mode = 5;
		wlanScanConfig = &wlanScanSlots[0];
		break;
	case 6:
		wlanScanConfig = &wlanScanSlots[0];
		if (state->mode != 4 && state->mode != 3 && state->mode != 5)
			return;
		mode = 1;
		break;
	case 7:
		mode = 2;
		break;
	case 8:
		mode = 0;
		break;
	default:
		return;
	}
	macMgmtParseTable(mode);
}

/*
 * 0x3733a: a round of the background scan has ended (the callback of its results): the next one is
 * prepared, or the mode that follows.
 */
void macMgmtCompleteTable(struct WlanScanResults *results)
{
	struct WlanScanConfig *config = wlanScanConfig;
	struct WlanBackgroundScanState *state = &wlanBackgroundScanState;
	struct WlanConnection *connection;
	unsigned int index;
	unsigned int record;

	state->slot = 0;
	if (config->enabled == 0)
		return;
	state->position += config->channelsPerScan;
	if (state->position >= config->channelCount) {
		state->position = 0;
		state->progress |= 0x80000000;
	}
	state->progress = (state->progress >> 3) << 3;
	if (results->count != 0) {
		state->progress |= 1;
		index = fmCoexVerifyEntry(results, 0);
		record = fmCoexTrackEntry(&results->records[index], 0);
		if (fmCoexManageEntry(record, config->thresholds[0], 0) != 0)
			state->progress |= 4;
		index = fmCoexVerifyEntry(results, 1);
		record = fmCoexTrackEntry(&results->records[index], 1);
		if (fmCoexManageEntry(record, config->thresholds[1], 1) != 0)
			state->progress |= 2;
	}
	if (config->progress != 0 && (config->progress & ~state->progress) == 0)
		state->completed = state->progress;
	if (state->completed != 0) {
		connection = state->connection;
		if (connection->interfaceType != WLAN_IF_AP || connection->mode != 1) {
			wlanScanSlots[0].enabled = 0;
			wlanScanSlots[1].enabled = 0;
			wlanPortOpenNotify(connection, 0x2000000);
		}
	}
	switch (state->mode) {
	case 1:
	case 3:
	case 4:
	case 5:
		macMgmtPrepareTable();
		break;
	case 2:
		if (wlanScanConfig->enabled != 0) {
			wlanMacFlush();
			macMgmtPrepareTable();
			wlanPsIdleSleep();
			return;
		}
		if (wlanConnectionPhase != 0) {
			wlanPsRestoreDtimPolicy();
			wlanPsPhaseResume(wlanConnectionByRole(wlanHasApRole() != 0 ? 2 : 0));
			return;
		}
		macMgmtValidateTable(8);
		wlanPsRestoreDtimPolicy();
		break;
	default:
		break;
	}
}

/* 0x3747c: the scan of the next round of the background scan, from the configuration: -1 if it does not start. */
int macMgmtPrepareTable(void)
{
	struct WlanScanConfig *config = wlanScanConfig;
	struct WlanBackgroundScanState *state = &wlanBackgroundScanState;
	struct WlanScan *scan = &wlanBackgroundScan;
	struct WlanConnection *connection;
	struct WlanPsPeerState *ps;
	int elapsed;
	unsigned int count;
	unsigned int interval;

	if (config->enabled == 0 || state->mode == 0 || state->slot != 0)
		return -1;
	elapsed = (int)scan->waited;
	__aeabi_memclr4(scan, sizeof(*scan));
	scan->bssType = config->bssType;
	__aeabi_memset4(scan->bssid, 6, 0xff);
	count = WLAN_SCAN_CHANNELS;
	if (config->channelsPerScan < count)
		count = config->channelsPerScan;
	if (state->mode != 2) {
		if ((int)(config->channelCount - state->position) <= (int)count)
			count = config->channelCount - state->position;
		count = (uint8_t)count;
	}
	__aeabi_memcpy(scan->rates, wlanLocalState->rates, WLAN_SCAN_RATES);
	connection = state->connection;
	if (connection->interfaceType == WLAN_IF_AP) {
		if (p2pSqramNotifyState() != 0)
			config->channelsPerScan = 1;
		else
			config->channelsPerScan = config->channelCount - state->position;
	}
	__aeabi_memcpy(scan->channels, &config->channels[state->position], 7 * count);
	__aeabi_memcpy(scan->ssids, config->ssids, sizeof(config->ssids));
	scan->ssidCount = config->ssidCount;
	__aeabi_uwrite4((int)(uintptr_t)macMgmtCompleteTable, state->results.callback);
	scan->results = &state->results;
	if (config->progress & 2)
		scan->policy = 1;
	scan->probes = config->probes;
	if (connection->interfaceType == WLAN_IF_AP && connection->mode == 1) {
		if (p2pDirectInspectState() != 0) {
			interval = config->interval;
		} else {
			ps = wlanConnectionPsState(connection);
			p2pSqramProcessState(connection, 2, 0, 0);
			if (ps->p2pWindowValid != 0)
				interval = (uint16_t)(ps->p2pWindowEnd - ps->p2pWindowStart);
			else
				interval = p2pDirectFormatContext(connection);
		}
	} else {
		interval = config->interval;
	}
	interval *= 1000;
	if (elapsed < 0 && (unsigned int)-elapsed < interval)
		interval -= elapsed;
	state->slot = macMgmtVerifyEntry(connection, scan, interval);
	if (state->slot == 0) {
		macMgmtCompleteTable(&state->results);
		return -1;
	}
	return 0;
}

/* 0x37614: the results of the background scan are empty again. */
__attribute__((noinline)) void *macMgmtDispatchTable(void)
{
	struct WlanBackgroundScanState *state = &wlanBackgroundScanState;

	state->results.length = 0;
	state->results.data = wlanBackgroundScanWindow;
	state->results.count = 0;
	state->results.size = sizeof(wlanBackgroundScanWindow);
	state->results.capacity = WLAN_SCAN_RECORDS;
	state->completed = 0;
	state->progress = 0;
	return state;
}

/* 0x37634: the background scan's mode changes. */
void macMgmtParseTable(int mode)
{
	struct WlanBackgroundScanState *state = &wlanBackgroundScanState;
	int previous = state->mode;

	state->mode = mode;
	if (mode == 0) {
		wlanSlotCancelById(state->slot);
	} else if (previous == 4 || mode == 4 || previous == 0) {
		if (state->completed == 0)
			macMgmtDispatchTable();
		if (state->slot != 0) {
			state->mode = 0;
			wlanSlotCancelById(state->slot);
			state->mode = mode;
		}
		wlanBackgroundScan.waited = 0;
		macMgmtPrepareTable();
	}
}

/*
 * 0x376a0: the host's configuration of the background scan (command 0x006b) into slot 0 or 1:
 * 0 or -1.
 */
int macMgmtBuildTable(unsigned int slot, const uint8_t *command, int length)
{
	struct WlanScanConfig *config = slot == 1 ? &wlanScanSlots[1] : &wlanScanSlots[0];
	const uint8_t *tlv;
	const uint8_t *end;
	struct WlanPsPeerState *ps;
	struct WlanConnection *connection;
	unsigned int type;
	unsigned int size = 0;
	unsigned int window;
	unsigned int i;
	uint8_t pair[2];
	uint16_t value;
	uint32_t extra;

	__aeabi_memclr4(config, sizeof(*config));
	config->probes = 1;
	config->flag = 1;
	if (command[0] == 0)
		return 0;
	config->enabled = command[0];
	config->bssType = command[1];
	config->channelsPerScan = command[2];
	config->interval = __aeabi_uread4(command + 6);
	config->progress = __aeabi_uread4(command + 14);
	tlv = command + 20;
	end = command + length;
	while (tlv < end) {
		type = SCAN_LE16(tlv);
		switch (type) {
		case 260:
		case 261:
			pair[0] = 0;
			pair[1] = 0;
			size = wlanTlvParseThreshold(tlv, pair, NULL);
			config->thresholds[type - 260] = pair[0];
			break;
		case 257:
			if (SCAN_LE16(tlv + 2) > 0xe0)
				return -1;
			size = wlanTlvChannelListParse(tlv, (uint8_t *)config->channels, NULL);
			if (size == 0)
				goto fail;
			if ((uint16_t)size != 4)
				config->channelCount = (uint16_t)(size - 4) / 7;
			connection = wlanBackgroundScanState.connection;
			if (connection->interfaceType == WLAN_IF_AP) {
				ps = wlanConnectionPsState(connection);
				if (ps->p2pWindowValid != 0) {
					window = (uint16_t)(ps->p2pWindowEnd - ps->p2pWindowStart);
					config->probes = 1;
					if (window / config->channelCount <= 4) {
						extra = 0;
						macMgmtPrepareContext(NULL, &extra);
						config->channelCount = 0;
						config->channels[0].channel = extra;
					} else {
						for (i = 0; config->channelCount > i; i++) {
							value = window / config->channelCount - 4;
							config->channels[i].minDwell[0] = value;
							config->channels[i].minDwell[1] = value >> 8;
							value = window / config->channelCount - 4;
							config->channels[i].maxDwell[0] = value;
							config->channels[i].maxDwell[1] = value >> 8;
						}
					}
				}
			}
			break;
		case 258:
			size = wlanTlvNumProbesParse(tlv, (uint8_t *)&config->probes);
			if (size == 0)
				goto fail;
			break;
		case 0:
		case 274:
			size = fmCoexConvertEntry(type, tlv, (uint8_t *)&config->ssids[config->ssidCount], &config->ssidCount,
						  WLAN_SCAN_SSIDS);
			if (size == 0)
				goto fail;
			break;
		case 286:
			size = wlanTlvCopyOfType(286, tlv, &value, 2);
			config->flag = value != 0;
			if (size == 0)
				goto fail;
			break;
		default:
			size = (uint16_t)(SCAN_LE16(tlv + 2) + 4);
			break;
		}
		if (size == 0)
			goto fail;
		tlv += size;
	}
	return 0;
fail:
	__aeabi_memclr4(config, sizeof(*config));
	return -1;
}

/* 0x378b0: the host's command 0x006b: sets the configuration of one slot, or of both (255). */
int hostCmd006bSet(struct WlanConnection *connection, unsigned int slot, const uint8_t *command, int length)
{
	int result;

	wlanBackgroundScanState.connection = connection;
	if (slot == 255) {
		result = macMgmtBuildTable(0, command, length);
		result |= macMgmtBuildTable(1, command, length);
	} else {
		result = macMgmtBuildTable(slot, command, length);
	}
	if (result == 0)
		macMgmtPrepareTable();
	return result;
}

/* 0x378f0: what the background scan found since the last time: its progress and the results. */
int hostCmd006bGet(void *out, int reset)
{
	int length;

	__aeabi_uwrite4(wlanBackgroundScanState.completed, out);
	length = macMgmtClearEntry((uint8_t *)out + 4, &wlanBackgroundScanState.results) + 4;
	wlanBackgroundScanState.completed = 0;
	if (reset != 0)
		macMgmtDispatchTable();
	return length;
}

/* 0x3791c: the configurations and the background scan's state are empty. */
int macMgmtResetTable(void)
{
	wlanScanConfig = &wlanScanSlots[0];
	__aeabi_memclr4(&wlanScanSlots[0], sizeof(wlanScanSlots[0]));
	__aeabi_memclr4(&wlanScanSlots[1], sizeof(wlanScanSlots[1]));
	__aeabi_memclr4(&wlanBackgroundScanState, 0x11c);
	return 0;
}

int macMgmtSyncTable(void)
{
	macMgmtResetTable();
	wlanScanInitStage.run = macMgmtResetTable;
	return wlanInitStageRegister(&wlanScanInitStage);
}

void macMgmtApplyTable(void)
{
	wlanSlotCancelById(wlanBackgroundScanState.slot);
}

int hostCmd001cResolve(void)
{
	return wlanScanConfig->enabled;
}

unsigned int macMgmtScheduleTable(unsigned int enabled)
{
	if (wlanScanConfig != NULL)
		wlanScanConfig->enabled = enabled;
	return enabled;
}

int macMgmtNotifyTable(void)
{
	wlanSlotAbort(wlanBackgroundScanState.slot, 5);
	wlanBackgroundScanState.slot = macMgmtVerifyEntry(wlanBackgroundScanState.connection, &wlanBackgroundScan, 0);
	return wlanBackgroundScanState.slot;
}

__attribute__((noinline)) int macMgmtFormatTable(const struct WlanScan *scan)
{
	return scan == &wlanBackgroundScan;
}

/* 0x379a8: the scan's next channels do not end the configuration's list (the first channel of a round, or one that does not end it). */
__attribute__((noinline)) int macMgmtInspectTable(unsigned int channelIndex)
{
	unsigned int count = wlanScanConfig->channelCount;
	unsigned int next = wlanBackgroundScanState.position + wlanScanConfig->channelsPerScan;

	return (channelIndex == 0 || next != count) && next <= count;
}
