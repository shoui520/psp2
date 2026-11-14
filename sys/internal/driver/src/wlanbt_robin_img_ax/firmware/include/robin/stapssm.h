/*
 * The station's power-save state machine (0x143dc..): one machine for the
 * station connection, driven through mdevRomQueryState() like the
 * connection machines of robin/pssm.h. Active: power save is off. PowerSave
 * and the states below it: Enter sets the power-management bit, Confirm
 * waits for the access point's acknowledgement and the host's confirmation,
 * Sleep is power save proper. Ieee drives the 802.11 power-management null
 * frames, IeeeNull (PM=1) and IeeeExit (PM=0) below it. The names are
 * inferred from use; see wlan/stapssm.c.
 */
#ifndef ROBIN_STAPSSM_H
#define ROBIN_STAPSSM_H

#include <stddef.h>
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/sys.h"
#include "robin/threadx.h"

/* WlanStaPsMachine.confirmBits */
#define WLAN_STA_PS_NULL_DONE		0x0001	/* the PM=1 null frame went out */
#define WLAN_STA_PS_HOST_DONE		0x0002	/* the host confirmed the sleep */

/* The machine (0x04004d20). */
struct WlanStaPsMachine {
	struct RomHsmState *state;	/* current state */
	struct RomHsmState root;
	struct RomHsmState ieee;
	struct RomHsmState ieeeNull;
	struct RomHsmState ieeeExit;
	struct RomHsmState active;
	struct RomHsmState powerSave;
	struct RomHsmState enter;
	struct RomHsmState sleep;
	struct RomHsmState confirm;
	struct RomHsmState state112;
	struct RomHsmState state124;
	struct RomHsmState state136;
	struct RomHsmState state148;
	struct RomHsmState state160;
	uint8_t reserved172[44];
	uint8_t slotHeld;		/* wlanSlotResume() once the machine wakes */
	uint8_t exitEventDue;		/* event 9 to the control task after IeeeExit */
	uint16_t wakeTimerArmed;	/* wlanPsBuildEntry() when the machine wakes */
	uint16_t confirmBits;		/* WLAN_STA_PS_* */
	uint16_t reserved222;
	uint32_t dataPendingSeen;	/* wlanPsDataPending when the sleep was checked */
	uint32_t ieeeFlagSeen;		/* wlanPsIeeeFlag when the sleep was checked */
	uint8_t hostState;		/* 2: the host asked to be woken; 4: it is waking */
	uint8_t reserved233[3];
	uint32_t beaconWait;		/* waiting for a beacon: wlanPsBuildEntry() */
	uint16_t nullRetry;		/* the PM=1 null frame is to be sent again */
	uint16_t reserved242;
	uint32_t dataPendingAtConfirm;	/* wlanPsDataPending when Confirm was entered */
	TX_TIMER confirmTimer;		/* "SleepConfirmTmr", the host's confirmation */
	TX_TIMER nullTimer;		/* "NullPktDoneTmr", the null frame's completion */
	uint64_t powerSaveChanged;	/* when the PM bit last changed (us) */
	uint32_t triggerSent;		/* a U-APSD trigger frame went out */
	struct RomHsmState *resume;	/* the state Ieee returns to */
	uint8_t ieeeState;		/* 1: PM=1 null frames, 2: PM=0, 3: gave up */
	uint8_t ieeeTries;
	uint8_t eventPending;		/* event 17 to the control task on waking */
	uint8_t powerStateDue;		/* wlanPsSetPowerState(2) on waking */
	uint8_t restartDue;		/* wlanStaPsRestart() on waking */
	uint8_t reserved357[3];
	uint32_t wakeBits;		/* macTxAcquireWakeBit() while in IeeeNull */
	uint32_t servicePeriod;		/* a U-APSD service period is under way */
	uint32_t uapsd;			/* U-APSD is in use */
	uint8_t btShare;		/* 1: sharing the antenna with Bluetooth, 2: starting to */
	uint8_t reserved373[3];
	uint32_t directLink;		/* a direct-link peer is active */
	struct WlanConnection *connection;
};

_Static_assert(offsetof(struct WlanStaPsMachine, sleep) == 88, "station PS sleep state");
_Static_assert(offsetof(struct WlanStaPsMachine, slotHeld) == 216, "station PS slot");
_Static_assert(offsetof(struct WlanStaPsMachine, confirmTimer) == 248, "station PS confirm timer");
_Static_assert(offsetof(struct WlanStaPsMachine, nullTimer) == 292, "station PS null timer");
_Static_assert(offsetof(struct WlanStaPsMachine, powerSaveChanged) == 336, "station PS PM time");
_Static_assert(offsetof(struct WlanStaPsMachine, ieeeState) == 352, "station PS IEEE state");
_Static_assert(offsetof(struct WlanStaPsMachine, connection) == 380, "station PS connection");
_Static_assert(sizeof(struct WlanStaPsMachine) == 384, "station PS machine");

extern struct WlanStaPsMachine wlanStaPs;

/* The station's power-save state (0x54aa8..0x54b40). */
extern uint8_t wlanPsBtShareMode;
extern uint8_t wlanPsLastTxComplete;
extern uint8_t wlanPsNullSuppressed;
extern uint8_t wlanPsSleepAllowance;
extern uint8_t wlanPsConfirmArmed;
extern uint8_t wlanPsRoleMask;
extern uint16_t wlanPsHoldSeconds;
extern uint32_t wlanPsLastHostEvent;
extern uint32_t wlanPsDataPending;
extern uint32_t wlanPsIeeeFlag;
extern uint32_t wlanPsNullRetries;
extern uint32_t wlanPsNullTimeouts;
extern uint32_t wlanPsPowerStateChanges;
extern uint32_t wlanPsRetries;
extern uint32_t wlanPsHostBusy;
extern uint32_t wlanPsRetryTimer;
extern uint32_t wlanPsListenTimer;
extern uint32_t wlanPsLastEvent;
extern uint32_t wlanPsHostWakeSkips;
extern uint32_t wlanPsIeeeEntries;
extern uint64_t wlanPsPollReplyTime;
extern uint64_t wlanPsPollSentTime;

void wlanPsArmRetryTimer(uint32_t delay, OsaTimerCallback callback);
void wlanPsCancelRetryTimer(void);
void wlanPsClearHostEvent(void);
void wlanStaPsSendNull(struct WlanConnection *connection, int powerManagement);
void wlanStaPsRestart(struct WlanStaPsMachine *machine);
void wlanPsListenTimerExpired(uint32_t timer, uint32_t argument);
int wlanStaPsIdle(void);
int wlanStaPsBusy(void);
int wlanStaPsHoldExpired(void);
int wlanStaPsDeepSleep(void);
const int32_t *wlanStaPsRoot(struct WlanStaPsMachine *machine, const int32_t *event);
const int32_t *wlanStaPsIeee(struct WlanStaPsMachine *machine, const int32_t *event);
void wlanStaPsWakeAp(struct WlanStaPsMachine *machine);
const int32_t *wlanStaPsIeeeExit(struct WlanStaPsMachine *machine, const int32_t *event);
const int32_t *wlanStaPsIeeeNull(struct WlanStaPsMachine *machine, const int32_t *event);
int wlanCoexUseStationMode(void);
int wlanCoexUseP2pMode(void);
const int32_t *wlanStaPsActive(struct WlanStaPsMachine *machine, const int32_t *event);
const int32_t *wlanStaPsPowerSave(struct WlanStaPsMachine *machine, const int32_t *event);
const int32_t *wlanStaPsEnter(struct WlanStaPsMachine *machine, const int32_t *event);
int wlanPsLegacyPollMode(void);
void wlanPsEnterSleep(struct WlanConnection *connection, int urgent);
void wlanPsSendPowerSaveNull(struct WlanConnection *connection);
int wlanCoexUseP2pSleepMode(void);
int wlanStaPsNotifyPending(void);
const int32_t *wlanStaPsSleep(struct WlanStaPsMachine *machine, const int32_t *event);
const int32_t *wlanStaPsConfirm(struct WlanStaPsMachine *machine, const int32_t *event);

#endif /* ROBIN_STAPSSM_H */
