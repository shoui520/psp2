/*
 * The access point's BSS machine: it starts and stops the BSS and, when the host gave several
 * channels (WlanConnectionInfo.channelCount), picks the least loaded one first: every channel of
 * the list is surveyed by the SQRAM code (uapSqramSetupEntry()) and its load (the share of time the
 * channel was busy, and the access points heard on it) decides. Marvell published no source for
 * it; the names are the firmware's own placeholder names, kept, and the file name is inferred.
 *
 * The machine is the ROM's hierarchical state machine (romHsm*) with four states below the root,
 * named after their handlers: the parse state is the stopped access point, build the BSS being
 * brought up, compute the BSS running.
 */
#include <stddef.h>
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/htba.h"
#include "robin/hostps.h"
#include "robin/keyMgmtAp.h"
#include "robin/macentry.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/wlan.h"

/* The events the machine is sent (the SME's, numbers only). */
#define WLAN_AP_EVENT_START		29	/* the host started the BSS */
#define WLAN_AP_EVENT_STARTED		30	/* the SQRAM code has the BSS ready */
#define WLAN_AP_EVENT_STOP		33	/* the host stopped the BSS */
#define WLAN_AP_EVENT_STOPPED		34
#define WLAN_AP_EVENT_CONFIGURE		36
#define WLAN_AP_EVENT_CHECK		37
#define WLAN_AP_EVENT_VALIDATE		40
#define WLAN_AP_EVENT_PREPARE		41
#define WLAN_AP_EVENT_RESTART		46
#define WLAN_AP_EVENT_RESUME		47
#define WLAN_AP_EVENT_FAILURE		49	/* never expected */

#define WLAN_AP_REPORT_SELECTED		37	/* wlanReportEvent(): the channel is chosen */
#define WLAN_AP_REPORT_STARTED		29
#define WLAN_AP_MIC_ERROR_TIMEOUT	60000000	/* us: the TKIP countermeasures' time */
#define WLAN_AP_SCAN_TIMEOUT		500000		/* us */
#define WLAN_AP_COST_NONE		0xffffffffu

/* What the machine's events carry: the id and a parameter block. */
struct WlanApEventData {
	uint32_t reserved0[3];
	uint32_t argument;
};

struct WlanApEvent {
	int32_t id;
	const struct WlanApEventData *data;
};

/* The request of one channel's survey: the SQRAM code answers with a WlanSurveyResult. */
struct WlanSurveyRequest {
	uint8_t channelFlags;
	uint8_t channel;
	uint8_t reserved2[2];
	uint32_t reserved4[3];
	uint32_t duration;		/* us */
	uint8_t reserved20[40];
	uint8_t enable;
	uint8_t reserved61[11];
};

_Static_assert(sizeof(struct WlanSurveyRequest) == 0x48, "survey request");

struct WlanSurveyResult {
	uint8_t reserved0[16];
	uint8_t channelFlags;
	uint8_t channel;
	uint8_t reserved18;
	uint8_t busy;			/* the share of the time the channel was busy, 0..255 */
	uint8_t reserved20[16];
	uint8_t accessPoints;		/* access points heard on it */
};

/* Not converted yet; provisional names. */
int uapSqramSetupEntry(struct WlanConnection *connection, struct WlanSurveyRequest *request,
		       int (*callback)(struct WlanConnection *connection, OsaBuffer *buffer),
		       struct WlanSurveyRequest *argument);			/* SQRAM, via the veneer at 0x532ec */
void macTxNotifySetupBuffer(void);							/* 0x323ce */
void macTxNotifyReleaseBuffer(void);							/* 0x3239c */

/* The least loaded channel of the survey so far: its cost, flags and number, and the channels done. */
uint32_t wlanApBestCost __attribute__((section(".itcm.rw")));				/* 0x551c8 */
uint8_t wlanApBestFlags __attribute__((section(".itcm.rw")));				/* 0x551cc */
uint8_t wlanApBestChannel __attribute__((section(".itcm.rw")));			/* 0x551cd */
uint8_t wlanApSurveyed __attribute__((section(".itcm.rw")));				/* 0x551ce */

struct WlanApStates wlanApStates __attribute__((section(".itcm.bss")));		/* 0x59330 */

/* 0x3a5d6: the survey is over; the best channel becomes the BSS's. */
int macMgmtHandleFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	uint8_t *htOperation = (uint8_t *)wlanConnectionHtOperation(connection);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	role->channel = wlanApBestChannel;
	connection->channel = wlanApBestChannel;
	role->channelFlags = (role->channelFlags & 0xc) | (wlanApBestFlags & 3);
	if (info->htEnabled == 1 && (info->htCapability[2] & 2)) {
		htOperation[3] = (htOperation[3] & ~3) | 1;
		macMgmtProcessContext(connection, &role->channelFlags, role->channel);
	}
	return 1;
}

/* 0x3a636: the survey of the next channel of the list. */
int macMgmtProcessFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	const struct WlanApChannel *entry = &info->channels[wlanApSurveyed];
	struct WlanSurveyRequest request;

	__aeabi_memclr4(&request, sizeof(request));
	request.channelFlags = entry->channelFlags & 3;
	request.channel = entry->channel;
	request.duration = (uint32_t)(entry->measureTime[0] | entry->measureTime[1] << 8) << 10;
	request.enable = 1;
	return uapSqramSetupEntry(connection, &request, macMgmtUpdateFrame, &request);
}

/* 0x3a6a4: a channel's survey is in: its cost is the share of busy time (%) plus 10 per access point. */
int macMgmtUpdateFrame(struct WlanConnection *unused, OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	const struct WlanSurveyResult *result = (const struct WlanSurveyResult *)((uint8_t *)buffer + buffer->dataOffset);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	unsigned int cost = 100u * result->busy / 255 + 10 * result->accessPoints;

	if (wlanApBestCost > cost) {
		wlanApBestCost = cost;
		wlanApBestChannel = result->channel;
		wlanApBestFlags = result->channelFlags;
	}
	osa_BufferFree(buffer);
	if (++wlanApSurveyed >= info->channelCount) {
		macMgmtHandleFrame(connection);
		wlanReportEvent(connection, NULL, WLAN_AP_REPORT_SELECTED);
	} else {
		macMgmtProcessFrame(connection);
	}
	return 1;
}

/* 0x3a712: the BSS is to start: with one channel it is that, with more the survey begins. */
void macMgmtConfigureFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	connection->channel = 0;
	info->bssStarted = 2;
	if (info->channelCount == 0) {
		wlanReportEvent(connection, NULL, WLAN_AP_REPORT_SELECTED);
		return;
	}
	if (info->channelCount == 1) {
		wlanApBestChannel = info->channels[0].channel;
		wlanApBestFlags = (wlanApBestFlags & ~3) | (info->channels[0].channelFlags & 3);
		macMgmtHandleFrame(connection);
		wlanReportEvent(connection, NULL, WLAN_AP_REPORT_SELECTED);
		return;
	}
	wlanApBestCost = WLAN_AP_COST_NONE;
	wlanApBestChannel = 0;
	wlanApSurveyed = 0;
	macMgmtProcessFrame(connection);
}

/* 0x3a778 */
void macMgmtCheckFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection)->bssStarted = 0;
	wlanReportEvent(connection, NULL, WLAN_AP_REPORT_STARTED);
}

/* 0x3a796: the BSS stops: its timers end and, with the last BSS, the radio is put back. */
int macMgmtValidateFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanConnection *peer = connection;
	uint32_t count;
	uint32_t held;

	wlanConnectionDtimHold(connection);
	if ((info->securityMode.wpa || info->securityMode.wpa2) && info->apRsn.GrpReKeyTime != 0)
		osaCancelTimer(info->grpRekeyTimer);
	if (info->apMicError.status != 0)
		osaCancelTimer(info->apMicTimer);
	do {
		struct WlanConnectionRole *role = wlanConnectionInfoSlot(peer);

		unsigned int hskState = role->keyMgmt.keyMgmtState;

		if (hskState == 1 || hskState == 2 || hskState == 3 || hskState == 4)
			osaCancelTimer(role->keyMgmt.HskTimer);
		peer = wlanConnectionNextByBss(peer, peer->bssType, peer->bssNumber);
	} while (peer != NULL);
	if (info->challengePending != 0)
		osaCancelTimer(info->challengeTimer);
	count = wlanConnectionGetCount();
	held = wlanConnectionDtimHoldCount();
	if (count == held) {
		if (wlanSessionId != 0)
			wlanPsCancelHostModeTimer();
		romLibResolveState();
		uapSqramUpdateState(0);
		macTxNotifySetupBuffer();
		wlanMacOperationModeSet(0);
		wlanConnectionCommandCheck(3, 1);
		MAC_REG(0x0e0) = 31;
		wlanConnectionCommandPrepare(3, 0);
		return romLibApplyState();
	}
	return held;
}

/* 0x3a876: the BSS starts: the radio is set for it (with the first BSS) and its timers begin. */
void macMgmtPrepareFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanConnection *peer;
	uint8_t channel[2];

	if ((uint32_t)wlanConnectionGetCount() == wlanConnectionDtimHoldCount()) {
		romLibResolveState();
		uapSqramUpdateState(wlanConnectionCommandPrepare(2, 0));
		macTxNotifyReleaseBuffer();
		wlanMacOperationModeSet(1);
		romLibNotifyContext();
		uapSqramConfigureState(wlanConnectionCommandCheck(2, 0));
		wlanConnectionCommandPrepare(2, 1);
		romLibApplyState();
		if (wlanSessionId != 0)
			wlanPsStartHostModeTimer(connection);
	}
	wlanConnectionDtimRelease(connection);
	macMgmtNotifyPacket(connection, channel);
	macMgmtUpdateContext(channel[0], channel[1]);
	macMgmtCheckContext();
	if ((info->securityMode.wpa || info->securityMode.wpa2) && wlanInterfaceNeedsSecurity(connection) != 0)
		KeyMgmtStartGrpRekeyTimer(connection);
	if (info->apMicError.status != 0)
		osaCreateTimer((OsaTimerCallback)ApMicErrTimerExpCb, (uint32_t)connection,
			       WLAN_AP_MIC_ERROR_TIMEOUT, &info->apMicTimer, 0);
	peer = connection;
	do {
		struct WlanConnectionRole *role = wlanConnectionInfoSlot(peer);
		unsigned int hskState = role->keyMgmt.keyMgmtState;
		uint32_t timeout = 0;

		if (peer->authSequence == 3)
			osaCreateTimer((OsaTimerCallback)macMgmtFormatBuffer, (uint32_t)peer,
				       WLAN_AP_SCAN_TIMEOUT, &info->challengeTimer, 0);
		if (hskState == 1 || hskState == 2)
			timeout = info->apRsn.PwsHskTimeOut;
		else if (hskState == 3 || hskState == 4)
			timeout = info->apRsn.GrpHskTimeOut;
		if (timeout != 0)
			KeyMgmtStartHskTimer(peer, timeout);
		peer = wlanConnectionNextByBss(peer, peer->bssType, peer->bssNumber);
	} while (peer != NULL);
}

/* The states of the machine whose root a handler was given. */
static inline struct WlanApStates *apStates(const struct WlanApMachine *machine)
{
	return (struct WlanApStates *)machine->root;
}

/* 0x3a9a8: the machine's root: it begins in the parse state. */
const int32_t *macMgmtDispatchFrame(struct WlanApMachine *machine, const int32_t *event)
{
	if (*event == ROM_HSM_ENTRY)
		romHsmTransition(machine, &apStates(machine)->parse);
	return NULL;
}

/* 0x3a9bc: the stopped access point. */
const int32_t *macMgmtParseFrame(struct WlanApMachine *machine, const struct WlanApEvent *event)
{
	struct WlanConnection *connection = machine->connection;
	int result;

	if (machine == NULL || event == NULL || connection == NULL)
		return NULL;
	switch (event->id) {
	case ROM_HSM_ENTRY:
		macMgmtValidateTable(0);
		return NULL;
	case WLAN_AP_EVENT_START:
		result = macMgmtFormatPacket(connection) == 1 ? 0 : 1;
		macMgmtParsePacket(connection, result, WLAN_AP_EVENT_STARTED);
		return NULL;
	case WLAN_AP_EVENT_STARTED:
		if (macMgmtSyncBuffer(connection, (const uint8_t *)event->data->argument, 1) == 0)
			romHsmTransition(machine, &apStates(machine)->build);
		return NULL;
	case WLAN_AP_EVENT_STOP:
		result = macMgmtSyncPacket(connection);
		if (result == 2)
			return NULL;
		macMgmtParsePacket(connection, result, WLAN_AP_EVENT_STOPPED);
		return NULL;
	case WLAN_AP_EVENT_STOPPED:
		macMgmtApplyPacket();
		macMgmtResetBuffer(connection, event->data->argument, WLAN_AP_EVENT_STOPPED, 1);
		return NULL;
	case WLAN_AP_EVENT_CONFIGURE:
		macMgmtConfigureFrame(connection);
		return NULL;
	case WLAN_AP_EVENT_CHECK:
		macMgmtCheckFrame(connection);
		return NULL;
	case 38: case 39: case 42: case 43: case 44: case 45: case 48:
		return (const int32_t *)event;
	case 40: case 41: case 46: case 47:
		return NULL;
	case 22:
		romHsmTransition(machine, &apStates(machine)->query);
		return NULL;
	case 23:
		sysIrqDisable();
		for (;;)
			;
	case WLAN_AP_EVENT_FAILURE:
		for (;;)
			;
	}
	return (const int32_t *)event;
}

/* 0x3aa8c: the BSS being brought up. */
const int32_t *macMgmtBuildFrame(struct WlanApMachine *machine, const struct WlanApEvent *event)
{
	struct WlanConnection *connection = machine->connection;
	int result;

	if (machine == NULL || event == NULL)
		return NULL;
	switch (event->id) {
	case ROM_HSM_ENTRY:
		macMgmtValidateTable(1);
		return NULL;
	case 32:
		macMgmtQueryPacket(connection);
		if (macMgmtResetBuffer(connection, event->data->argument, 32, 1) != 0)
			return NULL;
		romHsmTransition(machine, &apStates(machine)->parse);
		return NULL;
	case WLAN_AP_EVENT_STOP:
		result = macMgmtSyncPacket(connection);
		if (result == 2)
			return NULL;
		macMgmtParsePacket(connection, result, WLAN_AP_EVENT_STOPPED);
		return NULL;
	case WLAN_AP_EVENT_STOPPED:
		macMgmtApplyPacket();
		if (macMgmtResetBuffer(connection, event->data->argument, 34, 1) != 0)
			return NULL;
		romHsmTransition(machine, &apStates(machine)->parse);
		return NULL;
	case 3:
		return NULL;
	case 31:
		result = macMgmtResetPacket(connection, 1);
		if (result == 3)
			return NULL;
		macMgmtParsePacket(connection, result, 32);
		return NULL;
	case 38: case 39: case 41: case 47:
		return NULL;
	case WLAN_AP_EVENT_VALIDATE:
		macMgmtValidateFrame(connection);
		romHsmTransition(machine, &apStates(machine)->compute);
		return NULL;
	case 42: case 43: case 44: case 45: case 48:
		return (const int32_t *)event;
	case 46:
		if (macMgmtResetPacket(connection, 1) != 0)
			return NULL;
		macMgmtQueryPacket(connection);
		macMgmtResetBuffer(connection, event->data->argument, 32, 0);
		romHsmTransition(machine, &apStates(machine)->compute);
		return NULL;
	case WLAN_AP_EVENT_FAILURE:
		for (;;)
			;
	}
	return (const int32_t *)event;
}

/* 0x3ab68: the BSS running. */
const int32_t *macMgmtComputeFrame(struct WlanApMachine *machine, const struct WlanApEvent *event)
{
	struct WlanConnection *connection = machine->connection;

	if (machine == NULL || event == NULL)
		return NULL;
	switch (event->id) {
	case ROM_HSM_EXIT:
	case ROM_HSM_ENTRY:
	case 20:
	case WLAN_AP_EVENT_VALIDATE:
	case WLAN_AP_EVENT_RESTART:
		return NULL;
	case 24:
		romHsmTransition(machine, &apStates(machine)->parse);
		return NULL;
	case WLAN_AP_EVENT_PREPARE:
		macMgmtPrepareFrame(connection);
		romHsmTransition(machine, &apStates(machine)->build);
		return NULL;
	case WLAN_AP_EVENT_RESUME:
		if (macMgmtFormatPacket(connection) != 1) {
			romHsmTransition(machine, &apStates(machine)->parse);
			return NULL;
		}
		macMgmtSyncBuffer(connection, NULL, 0);
		romHsmTransition(machine, &apStates(machine)->build);
		return NULL;
	}
	return (const int32_t *)event;
}

/* 0x3abce */
const int32_t *macMgmtQueryFrame(struct WlanApMachine *machine, const int32_t *event)
{
	switch (*event) {
	case ROM_HSM_EXIT:
	case ROM_HSM_ENTRY:
	case 22:
		return NULL;
	case 21:
		romHsmTransition(machine, &apStates(machine)->compute);
		return NULL;
	}
	return event;
}

/* 0x3abfc: the states of the machine below the root they are given. */
void macMgmtSyncFrame(struct RomHsmState *root, struct RomHsmState *parent, RomHsmHandler handler)
{
	romHsmStateInit(root, parent, handler);
	romHsmStateInit(&wlanApStates.parse, &wlanApStates.root, (RomHsmHandler)macMgmtParseFrame);
	romHsmStateInit(&wlanApStates.build, &wlanApStates.root, (RomHsmHandler)macMgmtBuildFrame);
	romHsmStateInit(&wlanApStates.compute, &wlanApStates.root, (RomHsmHandler)macMgmtComputeFrame);
	romHsmStateInit(&wlanApStates.query, &wlanApStates.root, (RomHsmHandler)macMgmtQueryFrame);
}

/* 0x3abf6 */
void macMgmtResetFrame(void)
{
	macMgmtSyncFrame(&wlanApStates.root, NULL, (RomHsmHandler)macMgmtDispatchFrame);
}

/* 0x3ac34: the access point's machine of this BSS starts in the stopped state. */
void macMgmtApplyFrame(struct WlanConnectionInfo *info)
{
	info->machine.root = &wlanApStates.root;
	romHsmStart(&info->machine, &wlanApStates.parse);
}

/* 0x3ac50 */
void macMgmtResolveFrame(struct WlanConnectionInfo *info)
{
	macMgmtApplyFrame(info);
}
