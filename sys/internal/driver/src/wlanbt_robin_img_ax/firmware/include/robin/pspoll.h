/*
 * Power-save polling of a station (0x8e90..0x994a): the PS-Poll frame that
 * asks the access point for one buffered frame, the timers around it and the
 * estimate of how long the answer takes. See wlan/pspoll.c.
 */
#ifndef ROBIN_PSPOLL_H
#define ROBIN_PSPOLL_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/txring.h"
#include "robin/protection.h"

struct WlanConnection;

/* Control-task event bits (wlanRaiseEvent) raised and handled here. */
#define WLAN_EVENT_PS_POLL_RETRY	32	/* the retry timer ran out */
#define WLAN_EVENT_PS_POLL		33	/* poll again, or go back to sleep */

/* Poll state (0x44 bytes at 0x54900, initialised data). */
typedef struct WlanPsPoll {
	uint8_t pending;		/* a poll is out, its answer not yet seen */
	uint8_t sent;			/* the PS-Poll frame is in a transmit ring */
	uint8_t sleepAfterTx;		/* go to sleep once that frame is sent */
	uint8_t retryLimit;
	uint8_t retries;
	uint8_t reserved5[3];
	uint32_t retryTimer;
	uint32_t retryDelay;
	uint32_t retryWanted;
	uint32_t eventTimer;
	uint32_t eventDelay;
	uint32_t triggerTimer;
	uint32_t triggerDelay;
	uint32_t triggered;		/* the trigger timer has polled once */
	uint32_t timeoutTimer;
	uint32_t timeoutDelay;
	uint32_t busyPolls;		/* polls asked for while the MAC was busy */
	uint32_t deferredSleeps;
	uint32_t windowClosed;		/* polls refused for lack of a sequence window */
	uint32_t ringWaits;		/* loops spent waiting for the rings to drain */
	uint32_t airtimeCorrections;
} WlanPsPoll;

_Static_assert(sizeof(WlanPsPoll) == 0x44, "poll state");
_Static_assert(offsetof(WlanPsPoll, retryTimer) == 0x08, "retry timer");
_Static_assert(offsetof(WlanPsPoll, triggered) == 0x24, "trigger flag");
_Static_assert(offsetof(WlanPsPoll, airtimeCorrections) == 0x40, "airtime corrections");

/*
 * Answer timing (0x2c bytes at 0x560b8, zeroed; the 64-bit start pads it
 * to 0x30 here). Times are microseconds; the interval and offset come from
 * the Bluetooth schedule when there is one.
 */
typedef struct WlanPsPollTiming {
	uint32_t span;			/* interval - offset: the longest answer counted */
	uint32_t minTimeout;
	uint32_t maxTimeout;
	uint32_t interval;
	uint32_t offset;
	uint32_t reserved14;
	uint64_t start;			/* when the poll went out; 0 when none is timed */
	int32_t mean;
	uint32_t countdown;		/* estimates left before the timeout is reprogrammed */
	int32_t deviation;
} WlanPsPollTiming;

_Static_assert(sizeof(WlanPsPollTiming) == 0x30, "poll timing");
_Static_assert(offsetof(WlanPsPollTiming, start) == 0x18, "poll start");
_Static_assert(offsetof(WlanPsPollTiming, deviation) == 0x28, "poll deviation");

/* The PS-Poll frame as the MAC reads it (36 bytes cleared). */
typedef struct WlanPsPollFrame {
	uint8_t frameLength[2];		/* length of the body: none */
	uint8_t frameControl[2];
	uint8_t associationId[2];	/* AID with both top bits set */
	uint8_t bssid[6];
	uint8_t transmitter[6];
	uint8_t reserved18[18];
} WlanPsPollFrame;

_Static_assert(sizeof(WlanPsPollFrame) == 0x24, "PS-Poll frame");

extern WlanPsPoll wlanPsPoll;
extern WlanPsPollTiming wlanPsPollTiming;

int wlanPsPollTriggerOnce(void);
void wlanPsPollProgramTimeout(void);
void wlanPsPollTimeoutExpired(uint32_t timer, uint32_t argument);
void wlanPsPollStartTimeout(void);
int wlanPsPollReserve(void);
int wlanPsPollBusy(void);
void wlanPsPollEventTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsPollRetryTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsPollStartRetryTimer(void);
void wlanPsPollKick(int urgent);
void wlanPsPollTriggerTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsPollStartTriggerTimer(void);
void wlanPsPollCancelTriggerTimer(void);
void wlanPsPollCancelRetryTimer(void);
void wlanPsPollStartEventTimer(void);
void wlanPsPollCancelEventTimer(void);
void wlanPsPollCancelTimeout(void);
void wlanPsPollWaitRingsDrained(void);
int wlanPsPollSend(struct WlanConnection *connection);
int wlanPsPollResume(void);
int wlanPsPollHostQueued(struct WlanConnection *connection, int moreData, int result, int active);
void wlanPsPollEnableMac(void);
void wlanPsPollUpdateTiming(void);
void wlanPsPollReset(void);
void wlanPsPollStop(void);
void wlanPsPollSetTimeout(int32_t mean, int32_t deviation);
void wlanPsPollAnswered(struct WlanConnection *connection);
void wlanPsPollDeductAirtime(uint32_t airtime);
void wlanPsPollTxDone(struct WlanConnection *connection, struct WlanTxStatus *status);
int wlanPsPollTxStatus(struct WlanConnection *connection, uint8_t *data, struct WlanTxStatus *status);
void wlanPsPollHandleEvent(struct WlanConnection *connection, int event);
void wlanPsPollFrameDone(void);
int wlanTxDiscardFrame(OsaBuffer *buffer);
int wlanTxSendFrame(OsaBuffer *buffer);

/*
 * Not converted yet. The names say what this file uses them for and are
 * provisional until their own files are written.
 */
int wlanPsPollUsesMacTimer(void);			/* 0x2e9d0, always 1 */
int wlanPsTriggerEnabled(void);				/* 0x2e9d4 */
int wlanCoexPsPollForced(void);				/* 0x2ea3a */
uint32_t wlanPsTriggerMode(void);			/* 0x2ea9e */
uint8_t wlanPsPollEnabled(void);			/* 0x2eaaa */
int btCoexProfilePollable(void);			/* 0x24516 */
void btCoexPollTiming(uint32_t *interval, uint32_t *offset);	/* 0x244ec */
int wlanTxRingsDrained(void);				/* 0x31ffe */
int wlanTxFramesOutsideRings(void);			/* 0x3200e */
void wlanTxRingClearCounters(void);			/* 0x31f30 */
uint8_t wlanTxControlRate(struct WlanConnection *connection);	/* 0x31e1c */
int wlanTxQueueFrame(OsaBuffer *buffer);		/* 0x32094 */
int wlanTxReleaseFrame(OsaBuffer *buffer);		/* 0x319d0 */
int wlanIbssTxFrame(OsaBuffer *buffer, int transmit);	/* 0x3baec */
uint32_t wlanTxAirtime(struct WlanConnection *connection, struct WlanTxStatus *status); /* 0x41db2 */
extern uint8_t wlanTxDataRing;				/* 0xc0011bd4 */

#endif /* ROBIN_PSPOLL_H */
