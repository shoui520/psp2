/*
 * Deep sleep (0xcb40..0xd4a6): the calibration of the sleep clock against
 * the bus clock, the wake-up countdown and the MAC's entry into and exit
 * from sleep. No Marvell source names it; see sys/sleep.c.
 */
#ifndef ROBIN_SLEEP_H
#define ROBIN_SLEEP_H

#include <stdint.h>

/* Why sleeps were refused or cut short, and how they went. */
struct SleepStats {
	uint8_t mode;			/* 2: wake on the secondary source only */
	uint8_t reserved[3];
	uint32_t entered;		/* set while asleep */
	uint32_t savedHaltBit;		/* MAC_650_OPERATION bit 2 before sleepMacHalt() */
	uint32_t savedEnterBit;		/* MAC_650_OPERATION bit 2 before sleepEnter() */
	uint32_t haltRetriesUnderway;	/* MAC state 1 when sleepEnter() gave up waiting */
	uint32_t haltRetriesBusy;	/* the same in sleepMacHalt() */
	uint32_t refused;		/* the deadline was past, or no wake-up could be programmed */
	uint32_t outOfRange;		/* sleep length outside the window; << 16: after wake-up */
	uint32_t haltTimeouts;		/* 5000 steps without the MAC halting */
	uint32_t haltSteps;
	uint32_t tooShort;		/* no time left after the wake-up latency */
	uint32_t countdownBehind;	/* the countdown ran past its reload */
};

_Static_assert(sizeof(struct SleepStats) == 0x30, "sleep statistics");

extern struct SleepStats sleepStats;
extern uint32_t sleepWakeLatency;
extern uint8_t sleepKeepWakeTiming;
extern uint8_t sleepCalibrationHeld;
extern uint8_t sleepCalibrating;
extern uint16_t sleepCalibrationDivisor;
extern uint32_t sleepCalibrationInterval;
extern uint32_t sleepCalibrationClockRate;
extern uint32_t sleepCalibrationPeriod;
extern uint32_t sleepCalibrationCycles;
extern uint64_t sleepCalibrationTime;
extern uint64_t sleepCalibrationSamples;
extern uint64_t sleepWakeDeadline;

void sleepClockInit(void);
uint32_t sleepTicksToUs(uint32_t ticks);
uint32_t sleepUsToTicks(uint32_t us);
uint32_t sleepClockTicksToUs(uint32_t ticks);
uint32_t sleepUsToClockTicks(uint32_t us);
void sleepCalibrationStart(uint32_t cycles);
int sleepCalibrationWait(void);
void sleepCalibrationRead(void);
void sleepCalibrationPlan(void);
int sleepCalibrate(void);
void sleepCalibrationRestart(uint32_t cycles);
void sleepCalibrationKick(uint32_t cycles);
void sleepCalibrationAverage(uint32_t time, uint32_t samples);
int sleepCalibrationUpdate(void);
int sleepProgramWakeup(uint32_t remaining, uint32_t minimum, uint32_t maximum);
int sleepHoldRequested(void);
void sleepRfRelock(void);
void sleepExit(void);
void sleepMacResume(void);
int sleepEnter(const uint64_t *deadline, uint32_t margin, uint32_t minimum, uint32_t maximum,
	       int force);
void sleepWake(void);
void sleepMacHalt(void);
int sysPrepareEventDeadline(void);
uint8_t sleepWakeTimingKept(void);
uint8_t sleepCalibrationActive(void);
uint32_t sleepElapsedUs(void);

#endif /* ROBIN_SLEEP_H */
