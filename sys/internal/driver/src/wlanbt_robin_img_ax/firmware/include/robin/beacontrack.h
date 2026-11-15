/*
 * The station's tracking of when the access point's beacons arrive (wlan/beacontrack.c, 0x3d8dc to
 * 0x3e088): the time of the last beacon, the DTIM period, the length of the beacon interval and
 * where in it the beacons fall (a histogram of 128 steps, learned from the first beacons after
 * a join), which the sleep machine uses to wake just before the next one. The firmware's
 * placeholder names (wlanPs*Frame: the beacons, wlanPs*Header: the times) are kept; the names of
 * the fields are inferred from use.
 */
#ifndef ROBIN_BEACONTRACK_H
#define ROBIN_BEACONTRACK_H

#include <stddef.h>
#include <stdint.h>

struct WlanConnection;

#define WLAN_BEACON_STEPS	128

/* WlanBeaconTracker.state */
#define WLAN_BEACON_GAVE_UP	0
#define WLAN_BEACON_LEARNING	1
#define WLAN_BEACON_LOCKED	2

/* The state, cleared by wlanPsResetHeader() (DTCM 0x040031c0, 248 bytes). */
struct WlanBeaconTracker {
	uint8_t state;			/* WLAN_BEACON_*: learning the phase of the beacons, locked on it, or not */
	uint8_t samples;		/* the beacons counted in the histogram */
	uint8_t sampleTarget;		/* the count that ends a round of learning: 32, then 48 or 16 more */
	uint8_t attempts;		/* the rounds that found no narrow phase, up to 5 */
	uint32_t pending;		/* a beacon was received and wlanPsAdjustFrame() has not counted it */
	uint8_t low;			/* the lowest and highest step of the histogram that was hit */
	uint8_t high;
	uint8_t start;			/* the steps the beacons fall in, start to end (start > end: it wraps) */
	uint8_t end;
	uint32_t span;			/* the microseconds from start to end, plus one step */
	uint32_t offset;		/* the microseconds from the start of the interval to start */
	uint16_t missed;		/* the beacons whose wake-up timer ran out since the last check */
	uint16_t received;		/* the beacons received since */
	uint8_t histogram[WLAN_BEACON_STEPS];	/* the beacons by step of the interval */
	uint32_t timer;			/* osaCreateTimer() handle: the beacon did not come in time */
	uint32_t spare0;
	uint64_t beaconTime;		/* when the last beacon came (the ROM's timer), microseconds */
	uint32_t interval;		/* the beacon interval, microseconds */
	uint16_t intervalTu;		/* the same, in time units of 1024 microseconds */
	uint8_t dtimPeriod;		/* the beacons between two DTIM beacons */
	uint8_t dtimCount;		/* the beacons before the next DTIM beacon, at the last one */
	uint16_t airtime;		/* what a beacon of lastLength bytes takes on the air, microseconds */
	uint16_t lastLength;
	uint32_t timerExpiries;		/* the wake-up timers that ran out */
	uint32_t beaconsReceived;
	uint32_t missedStreak;		/* the beacons missed in a row */
	uint32_t missedRounds;		/* the groups of 5 of them */
	uint32_t compensations;		/* the wake-up times moved earlier for a missed beacon */
	uint64_t sleepTime;		/* the ROM's timer when the machine went to sleep */
	uint64_t sleepHardwareTime;	/* osaGetHardwareTime() then */
	uint8_t waiting;		/* asleep, waiting for a beacon */
	uint8_t lateBeacons;		/* the beacons that came too late to be recorded */
	uint8_t measuring;		/* the first beacon after wlanPsResetHeader() compares the clocks */
	uint8_t spare1;
	uint32_t spare2;
	uint64_t resetTime;		/* the ROM's timer at wlanPsResetHeader() */
	uint64_t resetHardwareTime;	/* osaGetHardwareTime() then */
	uint64_t clockOffset;		/* what the two clocks differ by after the first beacon, microseconds */
};

_Static_assert(sizeof(struct WlanBeaconTracker) == 248, "beacon tracker");
_Static_assert(offsetof(struct WlanBeaconTracker, span) == 12, "beacon span");
_Static_assert(offsetof(struct WlanBeaconTracker, missed) == 20, "beacons missed");
_Static_assert(offsetof(struct WlanBeaconTracker, histogram) == 24, "beacon histogram");
_Static_assert(offsetof(struct WlanBeaconTracker, timer) == 152, "beacon timer");
_Static_assert(offsetof(struct WlanBeaconTracker, beaconTime) == 160, "beacon time");
_Static_assert(offsetof(struct WlanBeaconTracker, interval) == 168, "beacon interval");
_Static_assert(offsetof(struct WlanBeaconTracker, airtime) == 176, "beacon airtime");
_Static_assert(offsetof(struct WlanBeaconTracker, timerExpiries) == 180, "timer expiries");
_Static_assert(offsetof(struct WlanBeaconTracker, sleepTime) == 200, "beacon sleep time");
_Static_assert(offsetof(struct WlanBeaconTracker, waiting) == 216, "beacon waiting");
_Static_assert(offsetof(struct WlanBeaconTracker, resetTime) == 224, "beacon reset time");
_Static_assert(offsetof(struct WlanBeaconTracker, clockOffset) == 240, "beacon clock offset");

extern struct WlanBeaconTracker wlanBeaconTracker;	/* 0x40031c0 */
extern uint16_t wlanPsHeaderSpacing;			/* 0x562b0: the host's spacing of the headers */

int wlanPsReleaseFrame(void);								/* 0x3d8dc */
void wlanPsSetupFrame(uint32_t timer, uint32_t argument);				/* 0x3d96e */
uint32_t wlanPsClearFrame(void);							/* 0x3da0c */
void wlanPsConvertFrame(void);								/* 0x3da1e */
int wlanPsVerifyFrame(int reason);							/* 0x3dac6 */
void wlanPsSelectFrame(const uint8_t *tim);						/* 0x3db0c */
int wlanPsAdjustFrame(struct WlanConnection *connection, const uint8_t *beacon);	/* 0x3db1a */
void wlanPsRecordFrame(void);								/* 0x3dc46 */
void wlanPsFilterFrame(uint16_t beaconPeriod);						/* 0x3dc4e */
int wlanPsHandleHeader(struct WlanConnection *connection, const uint8_t *beacon, unsigned int length,
		       const uint8_t *tim, int ofdm);					/* 0x3dc5c */
int wlanPsProcessHeader(void);								/* 0x3dd26 */
uint32_t wlanPsUpdateHeader(void);							/* 0x3dd34 */
uint32_t wlanLinkCheckPeriod(void);							/* 0x3dd46 */
unsigned int wlanPsCheckHeader(void);							/* 0x3dd4c */
void wlanPsValidateHeader(uint64_t *time);						/* 0x3dd54 */
void wlanPsPrepareHeader(uint64_t *next);						/* 0x3dd98 */
void wlanPsDispatchHeader(uint64_t *time, uint32_t *period);				/* 0x3de44 */
uint32_t wlanPsParseHeader(uint64_t *wake, uint16_t *lead);				/* 0x3defe */
int wlanPsLifetimeWanted(void);								/* 0x3dfa0 */
int wlanPsComputeHeader(void);								/* 0x3dfbe */
unsigned int wlanPsDiscountMissed(uint8_t *count);					/* 0x3dfc4 */
void wlanPsResetHeader(void);								/* 0x3e028 */

#endif /* ROBIN_BEACONTRACK_H */
