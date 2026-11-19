/*
 * The connection's state machine of an IBSS, a station and a P2P link (wlan/stamachine.c, the
 * firmware's placeholder names "macMgmt*State"), and the management-layer messages its states send
 * (the probe request, the host's event messages). The names of the states and of the events are
 * inferred from what the handlers do.
 */
#ifndef ROBIN_STAMACHINE_H
#define ROBIN_STAMACHINE_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/softtimer.h"
#include "robin/wlan.h"

/* What the host is told when the machine leaves a state: wlanMgmtReport (0x57832). */
struct WlanMgmtReport {
	uint8_t code;		/* 3: the IBSS ended, 8: a disassociation, 9: a deauthentication; 0: none */
	uint8_t reserved1;
	uint16_t reason;
	uint8_t peer[6];
};

_Static_assert(sizeof(struct WlanMgmtReport) == 10, "management report");

extern struct WlanMgmtReport wlanMgmtReport;

/* The probe requests' supported rates (without the basic-rate bit), set by macMgmtParseState(). */
extern uint8_t wlanProbeRates[14];

/* The events the machine is sent (an event is the ROM's pointer to an id and to the data below it). */
enum {
	WLAN_STA_EVENT_FRAME = 2,		/* a frame for the P2P context */
	WLAN_STA_EVENT_RELEASE = 3,		/* macMgmtReleaseContext(): a frame and its argument */
	WLAN_STA_EVENT_MANAGE = 6,		/* macMgmtManageContext(): a frame (and its argument, unread) */
	WLAN_STA_EVENT_LOST = 7,		/* the IBSS ended: reported with code 3 */
	WLAN_STA_EVENT_DISASSOCIATION = 8,	/* a disassociation frame: reported with code 8 */
	WLAN_STA_EVENT_DEAUTHENTICATION = 9,	/* a deauthentication frame: reported with code 9 */
	WLAN_STA_EVENT_RESET_ENTRY = 10,
	WLAN_STA_EVENT_JOIN = 11,		/* the host's request to join an IBSS */
	WLAN_STA_EVENT_QUERY_ENTRY = 12,
	WLAN_STA_EVENT_START = 13,		/* the host's request to start an IBSS */
	WLAN_STA_EVENT_STOP = 14,		/* the IBSS is left */
	WLAN_STA_EVENT_HOLD = 21,		/* held in the Record state: the hold state is entered */
	WLAN_STA_EVENT_JOINED = 22,		/* the machine is for an IBSS being joined */
	WLAN_STA_EVENT_STARTED = 23,		/* the machine is for an IBSS being started */
	WLAN_STA_EVENT_RESUME = 24,		/* the hold state returns to the state it came from */
	WLAN_STA_EVENT_RESELECT = 25,		/* the hold state goes to the Select state */
	WLAN_STA_EVENT_REVERIFY = 26,		/* the hold state goes to the Verify state */
	WLAN_STA_EVENT_DIRECT_SSID = 42,	/* a probe request for the P2P group's SSID */
};

/* The data an event points to. */
struct WlanStaEventData {
	uintptr_t argument;		/* passed on to the handler of the frame (the third argument of the context functions) */
	const uint8_t *frame;
	const uint8_t *request;		/* the host's join or start request */
	const uint8_t *message;		/* the event message: its address and reason are the ones reported */
};

struct WlanStaEvent {
	int32_t id;
	const struct WlanStaEventData *data;
};

/* The address and the reason a disassociation or a deauthentication event carries, at message + 12. */
struct WlanStaEventMessage {
	uint8_t reserved0[12];
	uint8_t address[6];
	uint16_t reason;
};

/* The states of the machine (0x5783c), below one root: macMgmtFilterState() initialises them. */
struct WlanIbssStates {
	struct RomHsmState clear;	/* root */
	struct RomHsmState convert;	/* below clear; the others are below it */
	struct RomHsmState verify;	/* the machine starts here (the events of the host's requests) */
	struct RomHsmState adjust;	/* the IBSS ended: its beacons and templates are dropped */
	struct RomHsmState select;	/* the IBSS is up */
	struct RomHsmState record;	/* the machine is held: no frames are handled */
	struct RomHsmState hold;	/* reached from record: only the report is kept */
};

_Static_assert(sizeof(struct WlanIbssStates) == 84, "IBSS states");

extern struct WlanIbssStates wlanIbssStates;			/* 0x5783c */

void macMgmtFilterState(struct WlanIbssMachine *machine);		/* 0x354cc */

int macMgmtConfigureState(struct WlanConnection *connection);		/* 0x347e8 */
int macMgmtCheckState(struct WlanSoftTimer *timer);			/* 0x34814 */
int macMgmtDispatchState(OsaBuffer *buffer, uintptr_t argument, int matched);	/* 0x348c4 */
void macMgmtParseState(const uint8_t *rates, unsigned int count);	/* 0x34960 */
int macMgmtBuildState(struct WlanConnection *connection, const uint8_t *peer, const uint8_t *ssid,
		      const uint8_t *elements, unsigned int length, uint8_t tag,
		      int (*release)(OsaBuffer *buffer));		/* 0x34990 */
int macMgmtComputeState(struct WlanConnection *connection);		/* 0x34b66 */
int macMgmtQueryState(uint32_t flags, uint32_t extra);			/* 0x34b82 */
int macMgmtResetState(uintptr_t argument, unsigned int command, uintptr_t second,
		      int third);					/* 0x34ba6 */
OsaBuffer *wlanHostMessageAlloc(struct WlanConnection *connection);	/* 0x34c94 */
int macMgmtResolveState(struct WlanConnection *connection, unsigned int security);	/* 0x34cc4 */
void macMgmtScheduleState(struct WlanConnection *connection);		/* 0x34d18 */
int wlanHostMessagePost(OsaBuffer *message);				/* 0x34d80 */
int macMgmtFormatState(struct WlanConnection *connection, const uint8_t *event);	/* 0x34dae */
int macMgmtInspectState(struct WlanConnection *connection, uint8_t code, uint16_t reason,
			const uint8_t *peer);				/* 0x34e06 */
void macMgmtTrackState(struct WlanConnection *connection);		/* 0x34e3a */
unsigned int hostCmd00c7(struct WlanConnection *connection, uint8_t *body);	/* 0x34e58 */
int macMgmtAcquireState(struct WlanConnection *connection, const uint8_t *peer, unsigned int reason,
			uint8_t notify);					/* 0x34f7c */
int wlanHostDelbaReply(struct WlanConnection *connection, const uint8_t *body, int success,
		       const uint8_t *peer);			/* 0x34fba */
int wlanHostCoexistenceReply(struct WlanConnection *connection, const uint8_t *body);	/* 0x34fe4 */
int macMgmtPostReceived(void);						/* 0x3483e */
int macMgmtPostRelease(void);						/* 0x34852 */

#endif /* ROBIN_STAMACHINE_H */
