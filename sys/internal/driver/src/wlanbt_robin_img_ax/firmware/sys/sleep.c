/*
 * Deep sleep (0xcb40..0xd4a6): the sleep clock is calibrated against the
 * bus clock, the time to the next wake-up deadline is turned into sleep
 * clock ticks for the event engine's countdown, and the MAC is halted
 * before the chip sleeps and resumed after it wakes. The filename is
 * inferred; no Marvell source names this code.
 */
#include <stddef.h>
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/mac.h"
#include "robin/osa.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/rfbus.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/threadx.h"
#include "robin/timer.h"
#include "robin/wlantask.h"

/* Microseconds every sleep costs on top of the wake-up latency. */
#define SLEEP_OVERHEAD_US	795
#define SLEEP_COUNTDOWN_US	345	/* the shortest countdown, in bus clock time */
#define SLEEP_HALT_STEPS	5000
#define SLEEP_SHORT_US		3000	/* a shorter sleep skips the radio's recalibration */
#define SLEEP_REPROGRAM_US	2600	/* sysPrepareEventDeadline(): too close to reprogram */
#define SLEEP_REPROGRAM_TO_US	6100

/* MAC_650_OPERATION */
#define MAC_OPERATION_HALT	0x03
#define MAC_OPERATION_KEEP	0x04
#define MAC_OPERATION_HALT_ALL	0x0b
#define MAC_OPERATION_ASLEEP	0x10
/* MAC_658_OPERATION_STATUS, bits 4-5 */
#define MAC_OPERATION_STATE(v)	(((v) << 26) >> 30)
#define MAC_STATE_HALTING	1
#define MAC_STATE_HALTED	3

#define SOC_SLEEP_MEASURING	0x8000
#define SOC_SLEEP_CALIBRATION	0x2000
#define SOC_POWER_AWAKE		0x8000000

/* Wake sources sleepEnter() arms (sysEnableEventSources()). */
#define SLEEP_WAKE_SOURCES	0x100010

/* sysSleepAllow()/sysSleepForbid() votes. */
#define SLEEP_VOTE_WLAN		0x10
#define SLEEP_VOTE_BT		0x20

/* Pending events that refuse a sleep (wlanPendingEvents[0], [1]). */
#define SLEEP_BLOCKING_EVENTS0	0x20018
#define SLEEP_BLOCKING_EVENTS1	0x7e00

/* mdevIoctl() of the SDIO device: non-zero while the host keeps it awake. */
#define SDIO_IOCTL_HOST_AWAKE	0x4a

/* Not converted yet; provisional names. */
extern uint8_t wlanPrimaryEvents;				/* 0x04008828, bit 0: radio relock */
extern const uint16_t rfCalibrationTable[];			/* 0x53d50 */
extern uint32_t wlanPendingEvents[2];				/* wlan/connctl.c */
extern struct OsaEventFlags *wlanHostSleepFlags;		/* 0x54bdc */
extern uint8_t sysSleepShortWake;				/* 0x54cf1 */
int rfCalibConfigureContext(void);				/* 0x26842 */
void rfCalibNotifyContext(void);				/* 0x26b48 */
uint32_t sysGetBusClock(void);					/* 0x23b0a, in Hz */
void sysSleepAllow(uint32_t votes);				/* 0x23d64 */
void sysSleepForbid(uint32_t votes);				/* 0x23dea */
int sysEnableEventSources(uint32_t sources);			/* 0x351c */
int wlanPsAcquireEntry(void);					/* 0x17024 */
void cbProcFlush(void);						/* 0x1c840 */
void uapSqramFilterEntry(int enable);				/* SQRAM 0xc0009ba0 */
int uapSqramHandleTable(void);					/* SQRAM 0xc00099e5 */

uint32_t sleepWakeLatency __attribute__((section(".itcm.bss")));
uint8_t sleepKeepWakeTiming __attribute__((section(".itcm.bss")));
uint8_t sleepCalibrationHeld __attribute__((section(".itcm.bss")));
uint8_t sleepCalibrating __attribute__((section(".itcm.bss")));
uint16_t sleepCalibrationDivisor __attribute__((section(".itcm.bss")));
uint32_t sleepCalibrationInterval __attribute__((section(".itcm.bss")));
/* Bus clock cycles per measurement unit: 100 times the table's clock. */
uint32_t sleepCalibrationClockRate __attribute__((section(".itcm.bss")));
uint32_t sleepCalibrationPeriod __attribute__((section(".itcm.bss")));
uint32_t sleepCalibrationCycles __attribute__((section(".itcm.bss")));
/* Bus clock time measured over sleepCalibrationSamples sleep clock samples. */
uint64_t sleepCalibrationTime __attribute__((section(".itcm.bss")));
uint64_t sleepCalibrationSamples __attribute__((section(".itcm.bss")));
static uint32_t sleepSavedMode __attribute__((section(".itcm.bss")));
static uint32_t sleepSavedEventSources __attribute__((section(".itcm.bss")));
uint64_t sleepWakeDeadline __attribute__((section(".itcm.bss")));

struct SleepStats sleepStats __attribute__((section(".itcm.rw")));

void sleepClockInit(void)
{
	sleepSavedMode = MAC_514_SLEEP_MODE;
	sleepSavedEventSources = SOC_EVENT_SOURCE_ENABLE;
	MAC_658_OPERATION_STATUS = 0;
	sysEnableEventEngineMode();
	sysSetEventFlags(4);
	sysSetEventFlags(0xc);
	MAC_500_SLEEP_CONTROL = 0;
}

uint32_t sleepTicksToUs(uint32_t ticks)
{
	return sleepCalibrationSamples * ticks * 1000 / sleepCalibrationTime;
}

uint32_t sleepUsToTicks(uint32_t us)
{
	return sleepCalibrationTime * us / (sleepCalibrationSamples * 1000);
}

uint32_t sleepClockTicksToUs(uint32_t ticks)
{
	uint32_t clock = sysGetBusClock();

	return (uint64_t)ticks * 1000000 / clock;
}

uint32_t sleepUsToClockTicks(uint32_t us)
{
	return (uint64_t)sysGetBusClock() * us / 1000000;
}

/* Counts the bus clock over cycles units of the sleep clock. */
void sleepCalibrationStart(uint32_t cycles)
{
	sleepCalibrating = 1;
	SOC_SLEEP_MEASURE_CONTROL |= SOC_SLEEP_MEASURING;
	SOC_SLEEP_CLOCK_CONTROL |= SOC_SLEEP_CALIBRATION;
	sleepCalibrationClockRate = rfCalibrationTable[3 + rfCalibConfigureContext()] * 100;
	sysStartEventMeasurement(sleepCalibrationClockRate * cycles);
}

int sleepCalibrationWait(void)
{
	sysWaitForEventMeasurement();
	return 1;
}

void sleepCalibrationRead(void)
{
	uint32_t sample = sysReadEventMeasurementSample();
	uint32_t units = sysReadEventMeasurementUnits();

	sleepCalibrationSamples = sample;
	sleepCalibrationTime = sleepCalibrationClockRate * units;
	SOC_SLEEP_MEASURE_CONTROL &= ~SOC_SLEEP_MEASURING;
	sleepCalibrating = 0;
}

/* Sets how long the next measurement runs: at least 20 ms of the bus clock. */
void sleepCalibrationPlan(void)
{
	uint32_t minimum;

	sleepCalibrationPeriod = sleepTicksToUs(sleepCalibrationInterval) / sleepCalibrationDivisor;
	minimum = 20000000 / sleepCalibrationClockRate;
	if (minimum > sleepCalibrationPeriod)
		sleepCalibrationPeriod = minimum;
	sleepCalibrationCycles = sleepCalibrationPeriod / 1000;
}

/* A short measurement, then a longer one sized by it. */
int sleepCalibrate(void)
{
	int done;

	sleepCalibrationStart(10);
	done = sleepCalibrationWait();
	if (done) {
		sleepCalibrationRead();
		sleepCalibrationPlan();
		if (sleepCalibrationCycles > 10) {
			sleepCalibrationStart(sleepCalibrationCycles);
			sysWaitForEventMeasurement();
			sleepCalibrationRead();
			sleepCalibrationPlan();
		}
	}
	return done;
}

/* Starts a measurement whose result sleepCalibrationUpdate() averages in. */
void sleepCalibrationRestart(uint32_t cycles)
{
	uint32_t extent;

	sleepCalibrating = 1;
	SOC_SLEEP_MEASURE_CONTROL |= SOC_SLEEP_MEASURING;
	sleepCalibrationClockRate = rfCalibrationTable[3 + rfCalibConfigureContext()] * 100;
	if (cycles != 0)
		extent = sleepCalibrationClockRate * cycles;
	else
		extent = sleepCalibrationPeriod * sleepCalibrationClockRate / 1000;
	sysStartEventMeasurement(extent);
}

/* Restarts the measurement when there is time for one and none runs. */
void sleepCalibrationKick(uint32_t cycles)
{
	if (sleepCalibrationCycles <= cycles && !sleepCalibrating && !sleepCalibrationHeld)
		sleepCalibrationRestart(cycles);
}

/*
 * Folds a new measurement into the average (7/8 old, 1/8 new), unless it is
 * more than a fifth away from what the average predicts.
 */
void sleepCalibrationAverage(uint32_t time, uint32_t samples)
{
	uint64_t measured = (uint64_t)time * sleepCalibrationSamples;
	uint64_t expected = sleepCalibrationTime * samples;
	uint64_t upper = expected * 6 / 5;

	if (measured < (expected << 2) / 5 || upper < measured)
		return;
	sleepCalibrationTime = ((expected * 7 + measured) >> 3) / sleepCalibrationSamples;
	sleepCalibrationSamples = samples;
}

int sleepCalibrationUpdate(void)
{
	if (!sleepCalibrationHeld) {
		uint32_t sample;
		uint32_t units;

		sysWaitForEventMeasurement();
		sample = sysReadEventMeasurementSample();
		units = sysReadEventMeasurementUnits();
		SOC_SLEEP_MEASURE_CONTROL &= ~SOC_SLEEP_MEASURING;
		sleepCalibrationAverage(sleepCalibrationClockRate * units, sample);
	}
	sleepCalibrating = 0;
	return 0;
}

/* Programs the countdown to wake before remaining microseconds have passed. */
int sleepProgramWakeup(uint32_t remaining, uint32_t minimum, uint32_t maximum)
{
	int length = remaining - sleepWakeLatency - SLEEP_OVERHEAD_US;
	uint32_t count;
	uint32_t limit;

	if (length <= 0) {
		sleepStats.tooShort++;
		return 0;
	}
	if (minimum > remaining || maximum < remaining)
		return 0;
	count = sleepUsToTicks(length);
	limit = sleepUsToClockTicks(SLEEP_COUNTDOWN_US);
	if (count > limit) {
		limit = count;
		count = sleepUsToTicks(length - sleepClockTicksToUs(count) + SLEEP_COUNTDOWN_US);
	}
	sysProgramEventCountdown(count, 0, 0, limit);
	if (!sleepKeepWakeTiming)
		sysSetEventPrimaryTimingLower((uint16_t)sleepUsToTicks(sleepWakeLatency));
	return 1;
}

int sleepHoldRequested(void)
{
	int coex = wlanCoexHoldRequested();

	return wlanPsAcquireEntry() | coex;
}

void sleepRfRelock(void)
{
	if (wlanPrimaryEvents & 1) {
		rfBusAcquire();
		rfCheckLock();
		rfBusRelease();
	}
}

/* Leaves sleep, or gives up entering it: the tasks, the MAC and the timers run again. */
void sleepExit(void)
{
	sleepStats.entered = 0;
	wlanResumeTasks();
	SOC_POWER_CONTROL |= SOC_POWER_AWAKE;
	if (wlanPrimaryEvents & 1) {
		UINT posture = sysIrqDisable();

		MAC_650_OPERATION &= ~MAC_OPERATION_HALT_ALL;
		MAC_650_OPERATION |= sleepStats.savedEnterBit;
		MAC_658_OPERATION_STATUS = 0;
		sysEnableEventEngineMode();
		sysClearEventCountdownMode();
		SOC_EVENT_STATUS = ~0x10u;
		SOC_MAC_EVENT_CONTROL |= 8;
		MAC_300_MODE |= 8;
		sysIrqRestore(posture);
	} else {
		MAC_658_OPERATION_STATUS = 0;
		sysEnableEventEngineMode();
		sysClearEventCountdownMode();
		SOC_EVENT_STATUS = ~0x10u;
	}
	romMacSleepNotify();
	MAC_514_SLEEP_MODE = sleepSavedMode;
	MAC_650_OPERATION &= ~MAC_OPERATION_ASLEEP;
	uapSqramFilterEntry(wlanAnyConnectionFlags2Bit4() ? 2 : uapSqramHandleTable());
	osaResumeTimers();
}

void sleepMacResume(void)
{
	MAC_650_OPERATION &= ~MAC_OPERATION_HALT_ALL;
	MAC_650_OPERATION |= sleepStats.savedHaltBit;
	MAC_658_OPERATION_STATUS = 0;
	rfBusAcquire();
	rfCheckLock();
	rfBusRelease();
	SOC_MAC_EVENT_CONTROL |= 8;
	MAC_300_MODE |= 8;
}

/*
 * Puts the chip to sleep until *deadline less margin (deadline NULL: until a
 * wake source), when the sleep length falls within [minimum, maximum] µs.
 * force sleeps although the SDIO host keeps the device awake. 1 when the
 * sleep was entered; 0 when it was refused or undone.
 */
int sleepEnter(const uint64_t *deadline, uint32_t margin, uint32_t minimum, uint32_t maximum,
	       int force)
{
	uint32_t elapsed = 0;
	int steps = SLEEP_HALT_STEPS;
	struct WlanConnection *connection;
	uint32_t wake;
	uint32_t actual;
	UINT posture;
	int raise;

	connection = wlanConnectionByRole(wlanHasApRole() ? 2 : 0);
	raise = connection != NULL ? (connection->flags2 >> 4) & 1 : 0;
	wlanSuspendTasks(raise);
	posture = sysIrqDisable();
	osaEventFlagsWait(wlanHostSleepFlags, 1, 8, &actual, 0);
	if (actual != 0 ||
	    (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_AWAKE, 0) != 0 && !force) ||
	    ((wlanPendingEvents[0] & SLEEP_BLOCKING_EVENTS0) |
	     (wlanPendingEvents[1] & SLEEP_BLOCKING_EVENTS1)) != 0) {
		wlanPendingEvents[1] &= SLEEP_BLOCKING_EVENTS1;
		wlanPendingEvents[0] &= SLEEP_BLOCKING_EVENTS0;
		sleepExit();
		goto out;
	}
	if (deadline != NULL) {
		uint64_t now = romGetTimerTimestamp();

		if (now >= *deadline - margin ||
		    !sleepProgramWakeup(*deadline - now, minimum, maximum)) {
			sleepStats.refused++;
			sleepExit();
			goto out;
		}
		sleepWakeDeadline = *deadline;
		elapsed = sleepElapsedUs();
	}

	MAC_300_MODE &= ~8u;
	SOC_MAC_EVENT_CONTROL &= ~8u;
	sleepStats.savedEnterBit = MAC_650_OPERATION & MAC_OPERATION_KEEP;
	MAC_650_OPERATION &= ~MAC_OPERATION_KEEP;
	MAC_650_OPERATION |= MAC_OPERATION_HALT;
	while (MAC_OPERATION_STATE(MAC_658_OPERATION_STATUS) != MAC_STATE_HALTED) {
		sleepStats.haltSteps++;
		if (--steps == 0) {
			uint32_t status;

			sleepStats.haltTimeouts++;
			if (wlanRoleIbssOnly()) {
				(void)MAC_658_OPERATION_STATUS;
				(void)MAC_658_OPERATION_STATUS;
				MAC_650_OPERATION &= ~MAC_OPERATION_HALT_ALL;
				MAC_300_MODE |= 8;
				MAC_650_OPERATION |= sleepStats.savedEnterBit;
				osaResumeTimers();
				goto out;
			}
			status = MAC_658_OPERATION_STATUS;
			if (MAC_OPERATION_STATE(status) == MAC_STATE_HALTING)
				sleepStats.haltRetriesUnderway++;
			sleepMacResume();
			cbProcFlush();
			MAC_300_MODE &= ~8u;
			SOC_MAC_EVENT_CONTROL &= ~8u;
			MAC_650_OPERATION &= ~MAC_OPERATION_KEEP;
			MAC_650_OPERATION |= MAC_OPERATION_HALT;
			steps = SLEEP_HALT_STEPS;
		}
		rfCalibSyncState(1);
	}
	if (deadline != NULL) {
		elapsed = sleepElapsedUs();
		if (elapsed > maximum || elapsed < minimum) {
			sleepStats.outOfRange++;
			goto undo;
		}
	}

	sleepSavedMode = MAC_514_SLEEP_MODE;
	MAC_514_SLEEP_MODE = 2;
	sysEnableEventSources(SLEEP_WAKE_SOURCES);
	romMacWakeRequest();
	SOC_POWER_CONTROL &= ~SOC_POWER_AWAKE;
	MAC_650_OPERATION |= MAC_OPERATION_ASLEEP;
	wake = sleepStats.mode == 2 ? 0x02 : 0x40;
	if (deadline != NULL)
		elapsed = sleepElapsedUs();
	if (elapsed != 0) {
		uapSqramFilterEntry(3);
		wake |= 0x01;
	} else {
		uapSqramFilterEntry(4);
	}
	rfCalibNotifyContext();
	if (wake & 0x01)
		sysDisableEventEngineMode();
	wake &= 0x42;
	if (wake != 0) {
		if (wake & 0x40) {
			sysClearEventFlags(0x10);
			sysSetSecondaryEventFlags(0x10);
			sysSelectEventEngineSource(4);
			sysTriggerEventEngineSource();
		}
		if (wake & 0x02) {
			sysClearEventFlags(0x1000);
			sysClearSecondaryEventFlags(0x1000);
			sysSelectEventEngineSource(0xc);
			sysTriggerEventEngineSource();
		}
	}
	sleepStats.entered = 1;
	if (deadline != NULL) {
		elapsed = sleepElapsedUs();
		if (elapsed > maximum || elapsed < minimum) {
			sleepStats.outOfRange += 0x10000;
			goto undo;
		}
	}
	if (elapsed < SLEEP_SHORT_US && elapsed != 0)
		sysSleepShortWake = 1;
	sysSleepAllow(sleepHoldRequested() ? SLEEP_VOTE_WLAN : SLEEP_VOTE_WLAN | SLEEP_VOTE_BT);
	sysIrqRestore(posture);
	if (tx_thread_identify() != &osaIdleThread)
		osaEventFlagsWait(wlanHostSleepFlags, 1, 8, &actual, 0xffffffff);
	return 1;

undo:
	sleepExit();
	sleepRfRelock();
out:
	sysIrqRestore(posture);
	return 0;
}

/* Wakes from sleep: always outside IBSS, inside it only when asleep. */
void sleepWake(void)
{
	if (!wlanRoleIbssOnly() || sleepStats.entered)
		sleepExit();
}

/* Halts the MAC, resuming and retrying every 5000 steps until it has. */
void sleepMacHalt(void)
{
	int steps = SLEEP_HALT_STEPS;

	MAC_300_MODE &= ~8u;
	SOC_MAC_EVENT_CONTROL &= ~8u;
	sleepStats.savedHaltBit = MAC_650_OPERATION & MAC_OPERATION_KEEP;
	MAC_650_OPERATION &= ~MAC_OPERATION_KEEP;
	do {
		MAC_650_OPERATION |= MAC_OPERATION_HALT_ALL;
		if (--steps == 0) {
			sleepMacResume();
			cbProcFlush();
			sleepStats.savedHaltBit = MAC_650_OPERATION & MAC_OPERATION_KEEP;
			MAC_650_OPERATION &= ~MAC_OPERATION_KEEP;
			MAC_300_MODE &= ~8u;
			SOC_MAC_EVENT_CONTROL &= ~8u;
			if (MAC_OPERATION_STATE(MAC_658_OPERATION_STATUS) == MAC_STATE_HALTING)
				sleepStats.haltRetriesBusy++;
			steps = SLEEP_HALT_STEPS;
		}
	} while (MAC_OPERATION_STATE(MAC_658_OPERATION_STATUS) != MAC_STATE_HALTED);
}

/*
 * Moves the wake-up closer when the deadline is more than 2.6 ms away:
 * 0 when it was reprogrammed, 1 when it was left.
 */
int sysPrepareEventDeadline(void)
{
	uint64_t remaining = 0;
	uint64_t now;
	int length;
	uint32_t count;
	uint32_t limit;

	sysSleepForbid(SLEEP_VOTE_WLAN | SLEEP_VOTE_BT);
	now = romGetTimerTimestamp();
	if (now < sleepWakeDeadline)
		remaining = sleepWakeDeadline - now;
	if (remaining <= SLEEP_REPROGRAM_US)
		return 1;
	length = SLEEP_REPROGRAM_TO_US - sleepWakeLatency - SLEEP_OVERHEAD_US;
	count = sleepUsToTicks(length);
	limit = sleepUsToClockTicks(SLEEP_COUNTDOWN_US);
	if (count > limit) {
		limit = count;
		count = sleepUsToTicks(length - sleepClockTicksToUs(count) + SLEEP_COUNTDOWN_US);
	}
	sysProgramEventCountdown(count, 0, 0, limit);
	sysSleepAllow(SLEEP_VOTE_WLAN);
	return 0;
}

uint8_t sleepWakeTimingKept(void)
{
	return sleepKeepWakeTiming;
}

uint8_t sleepCalibrationActive(void)
{
	return sleepCalibrating;
}

/* Time slept so far, from the countdown; 0 (and counted) when it ran past. */
uint32_t sleepElapsedUs(void)
{
	uint32_t reload = sysReadEventCountdownReload();
	int ticks = reload - sysReadEventEngineCountdown();

	if (ticks < 0) {
		sleepStats.countdownBehind++;
		return 0;
	}
	return sleepTicksToUs(ticks);
}
