/*
 * The tracking of the access point's beacons (0x3d8dc - 0x3e088): where in the beacon interval
 * they arrive, and when the next one is due, for the station's power-save machine. See
 * robin/beacontrack.h; the firmware's placeholder names are kept.
 */
#include <stddef.h>
#include "robin/beacontrack.h"
#include "robin/conn.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/signal.h"
#include "robin/sleep.h"
#include "robin/sys.h"


struct WlanBeaconTracker wlanBeaconTracker __attribute__((section(".dtcm.bss")));	/* 0x40031c0 */

/* The times the dispatch of the headers found no wake-up time near enough (it moved away). */
static uint8_t wlanPsDispatchDiverged __attribute__((section(".dtcm.bss")));		/* 0x40009a6 */

/* A beacon was received. 1 when the machine was waiting for it. */
int wlanPsReleaseFrame(void)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	int waiting;
	uint32_t elapsed;
	int64_t delta;

	t->beaconTime = romGetTimerTimestamp();
	++t->beaconsReceived;
	++t->received;
	t->pending = 1;
	waiting = t->waiting;
	if (waiting != 0) {
		if (t->timer != 0) {
			osaCancelTimer(t->timer);
			t->timer = 0;
		}
		t->waiting = 0;
	}
	if (t->measuring != 0) {
		elapsed = (uint32_t)osaGetHardwareTime() - (uint32_t)t->resetHardwareTime;
		delta = (int64_t)(t->beaconTime - t->resetTime);
		sysLogStatistic(12, (uint32_t)(delta < 0 ? -delta : delta), elapsed);
		t->clockOffset = delta - elapsed;
		t->measuring = 0;
	}
	return waiting;
}

/* The wake-up timer ran out: the beacon did not come. */
void wlanPsSetupFrame(uint32_t timer, uint32_t argument)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	unsigned int missed;
	unsigned int attempts;
	int limit;

	if (t->timer != 0) {
		t->timer = 0;
		++t->timerExpiries;
		++t->missedStreak;
		if (t->missedStreak % 5 == 0)
			++t->missedRounds;
		missed = (uint16_t)++t->missed;
		if (t->state == WLAN_BEACON_LOCKED && missed + t->received > 300) {
			attempts = t->attempts;
			if (attempts > 1) {
				if (attempts > 3)
					limit = t->received >> 1;
				else
					limit = t->received >> 2;
			} else {
				limit = t->received >> 3;
			}
			if ((int)missed > limit) {
				if (attempts >= 5) {
					if (missed > t->received)
						t->state = WLAN_BEACON_GAVE_UP;
				} else {
					t->state = WLAN_BEACON_LEARNING;
					t->samples = 0;
					__aeabi_memclr4(t->histogram, sizeof(t->histogram));
					t->attempts = attempts + 1;
					t->sampleTarget += 16;
				}
			}
			t->missed = 0;
			t->received = 0;
		}
		wlanPsBeaconIdle(wlanConnectionByRole(0));
	}
}

/* The span of the beacons' steps, once locked on them. */
__attribute__((noinline)) uint32_t wlanPsClearFrame(void)
{
	if (wlanBeaconTracker.state == WLAN_BEACON_LOCKED)
		return wlanBeaconTracker.span;
	return 0;
}

/* The machine goes to sleep: arms the timer that tells when the beacon did not come. */
void wlanPsConvertFrame(void)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	unsigned int missed;
	unsigned int window;
	uint32_t delay;
	uint64_t now;
	uint32_t timer;

	if (wlanPsGetPowerState() == 5) {
		t->sleepHardwareTime = osaGetHardwareTime();
		t->sleepTime = romGetTimerTimestamp();
		timer = t->timer;
		if (timer != 0) {
			t->timer = 0;
			osaCancelTimer(timer);
		}
		if (wlanPsNullWindow != 0xffff) {
			missed = t->missedStreak % 5;
			now = romGetTimerTimestamp();
			if (now < t->beaconTime || now - t->beaconTime > 0x100590 || missed == 4) {
				delay = 2 * t->interval;
			} else {
				window = 1000 * wlanPsNullWindow;
				delay = wlanPsClearFrame();
				if (delay < window)
					delay = window;
				else
					delay = wlanPsClearFrame();
			}
			osaCreateTimer(wlanPsSetupFrame, 0, (missed << 11) + delay, &t->timer, 0);
		}
	}
}

/* The reasons the machine tells of the beacons: 0 one came, 1 it went to sleep, 2 asleep, 3 start over. */
int wlanPsVerifyFrame(int reason)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	int result = 0;

	switch (reason) {
	case 0:
		return wlanPsReleaseFrame();
	case 1:
		if (wlanPsGetPowerState() == 5) {
			t->waiting = 1;
			t->sleepHardwareTime = osaGetHardwareTime();
		}
		break;
	case 2:
		wlanPsConvertFrame();
		break;
	case 3:
		wlanPsResetHeader();
		break;
	default:
		break;
	}
	return result;
}

/* The DTIM period and count of a beacon's TIM element. */
void wlanPsSelectFrame(const uint8_t *tim)
{
	wlanBeaconTracker.dtimPeriod = tim[3];
	wlanBeaconTracker.dtimCount = tim[2];
}

/*
 * Counts the beacon in the histogram of the steps of the interval, and, once enough are counted,
 * finds the span of steps they fall in. 1 when the beacon was counted.
 */
int wlanPsAdjustFrame(struct WlanConnection *connection, const uint8_t *beacon)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	uint32_t interval;
	unsigned int step;
	unsigned int target;
	unsigned int first;
	unsigned int last;
	uint32_t length;
	uint32_t from;

	if (t->pending == 0)
		return 0;
	t->pending = 0;
	if (t->state != WLAN_BEACON_LEARNING || t->samples >= t->sampleTarget)
		return 0;
	interval = (((unsigned int)beacon[9] << 8) | beacon[8]) << 10;
	step = (uint32_t)(((*(const uint64_t *)beacon % interval) << 7) / interval);
	++t->histogram[step];
	if (++t->samples == 1) {
		t->high = step;
		t->low = step;
		return 1;
	}
	if (t->high >= step) {
		if (t->low > step)
			t->low = step;
	} else {
		t->high = step;
	}
	target = t->sampleTarget;
	if (t->samples != target)
		return 1;
	if (t->high - t->low <= 64) {
		t->start = t->low;
		t->end = t->high;
	} else {
		if (t->sampleTarget == 32) {
			t->sampleTarget = 48;
			return 1;
		}
		for (first = 65; t->high >= first && t->histogram[first] == 0; ++first)
			;
		t->start = first;
		for (last = 64; t->low <= last && t->histogram[last] == 0; --last)
			;
		t->end = last;
	}
	if (t->start <= t->end) {
		if ((unsigned int)t->start - 1 < 2)
			t->start = 0;
	} else {
		--t->start;
	}
	from = t->start;
	length = t->end - t->start + 1;
	if (t->start > t->end) {
		length = t->end - t->start + 129;
		from = t->start - 128;
	}
	t->span = (t->interval >> 7) * length;
	t->offset = (t->interval >> 7) * from;
	if ((t->interval >> 7) * length > interval >> 1) {
		unsigned int attempts = t->attempts;

		if (attempts >= 5) {
			t->state = WLAN_BEACON_GAVE_UP;
		} else {
			t->samples = 0;
			__aeabi_memclr4(t->histogram, sizeof(t->histogram));
			t->attempts = attempts + 1;
			t->sampleTarget = target + 16;
		}
	} else {
		t->state = WLAN_BEACON_LOCKED;
		t->received = 0;
		t->missed = 0;
	}
	return 1;
}

/* The beacon came on time: the run of missed ones is over. */
void wlanPsRecordFrame(void)
{
	wlanBeaconTracker.missedStreak = 0;
}

/* The beacon interval, in time units. */
__attribute__((noinline)) void wlanPsFilterFrame(uint16_t beaconPeriod)
{
	wlanBeaconTracker.intervalTu = beaconPeriod;
	wlanBeaconTracker.interval = beaconPeriod << 10;
}

/* A beacon came in: 1 when it was late. */
int wlanPsHandleHeader(struct WlanConnection *connection, const uint8_t *beacon, unsigned int length,
		       const uint8_t *tim, int ofdm)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	uint64_t timestamp;
	uint16_t airtime;

	__aeabi_memcpy(&timestamp, beacon, 8);
	wlanPsFilterFrame((beacon[9] << 8) | beacon[8]);
	if (t->clockOffset != 0) {
		wlanLinkMonitorStart(connection);
		t->clockOffset = 0;
	}
	if (t->lastLength != length) {
		t->lastLength = length;
		airtime = 8 * (length + 32);
		t->airtime = airtime;
		if (ofdm != 0)
			airtime = airtime / 6 + 20;
		else
			airtime = airtime + 192;
		t->airtime = airtime;
	}
	if (tim != NULL) {
		t->dtimPeriod = tim[3];
		t->dtimCount = tim[2];
	}
	wlanPsAdjustFrame(connection, beacon);
	sysLogStatistic(9, t->compensations, t->missedRounds);
	if ((uint64_t)(t->interval >> 1) + timestamp >= romGetTimerTimestamp()) {
		wlanPsRecordFrame();
		return 0;
	}
	++t->lateBeacons;
	return 1;
}

/* 1 while the phase of the beacons is being learned. */
int wlanPsProcessHeader(void)
{
	int state = wlanBeaconTracker.state;

	if (wlanBeaconTracker.state != WLAN_BEACON_LEARNING)
		return 0;
	return state;
}

/* The microseconds from the start of the interval to the first beacon, once locked. */
__attribute__((noinline)) uint32_t wlanPsUpdateHeader(void)
{
	if (wlanBeaconTracker.state == WLAN_BEACON_LOCKED)
		return wlanBeaconTracker.offset;
	return 0;
}

__attribute__((noinline)) uint32_t wlanLinkCheckPeriod(void)
{
	return wlanBeaconTracker.interval;
}

__attribute__((noinline)) unsigned int wlanPsCheckHeader(void)
{
	return wlanBeaconTracker.intervalTu;
}

/* The time of the first beacon after the last one, from now on. */
void wlanPsValidateHeader(uint64_t *time)
{
	uint64_t beacon = wlanBeaconTracker.beaconTime;
	uint32_t interval = wlanBeaconTracker.interval;

	*time = beacon - beacon % interval;
	do {
		*time += wlanBeaconTracker.interval;
	} while (*time < romGetTimerTimestamp());
}

/* The time of the next beacon that has to be received (a DTIM one), in *next. */
void wlanPsPrepareHeader(uint64_t *next)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	uint64_t now = romGetTimerTimestamp();
	unsigned int beacons;

	if (now < t->beaconTime || now > t->beaconTime + ((uint64_t)t->interval << 6)) {
		t->beaconTime = romGetTimerTimestamp();
		t->dtimCount = 1;
	}
	*next = (int64_t)(int)wlanPsUpdateHeader() + (t->beaconTime % t->interval);
	beacons = t->dtimCount;
	if (beacons == 0) {
		beacons = t->dtimPeriod;
		if (beacons == 0)
			beacons = 1;
	}
	do {
		*next += t->interval * beacons;
		beacons = t->dtimPeriod;
		if (beacons == 0)
			beacons = 1;
	} while (*next < romGetTimerTimestamp());
}

/* Moves the wake-up time to the beacon the host's header spacing asks for. */
void wlanPsDispatchHeader(uint64_t *time, uint32_t *period)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	uint32_t spacing = wlanPsHeaderSpacing;
	uint64_t now = romGetTimerTimestamp();
	uint64_t target;
	uint32_t step;
	uint32_t limit;
	int distance = 0;
	int previous = 0;

	if (spacing >= 50)
		step = spacing << 10;
	else
		step = spacing * t->interval;
	target = now + step;
	if (wlanPsDtimPolicy == 65533) {
		limit = *period >> 1;
	} else {
		if ((int)wlanPsDtimPolicy <= 5 && *time < target)
			return;
		wlanPsValidateHeader(time);
		*time += (int64_t)(int)wlanPsUpdateHeader();
		limit = t->interval;
		*period = t->interval;
	}
	while (distance <= previous) {
		distance = (int32_t)((uint32_t)target - (uint32_t)*time);
		if (distance < 0)
			distance = -distance;
		if ((uint32_t)distance <= limit)
			return;
		*time += *period;
		previous = distance;
	}
	++wlanPsDispatchDiverged;
}

/* The wake-up time, in *wake, and how long before it the machine wakes (*lead). Returns the time left. */
uint32_t wlanPsParseHeader(uint64_t *wake, uint16_t *lead)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	uint32_t period = t->dtimPeriod * t->interval;
	int64_t left;

	wlanPsPrepareHeader(wake);
	if (wlanPsDtimPolicy - 2 <= 3)
		*wake += (uint32_t)((wlanPsDtimPolicy - 1) * period);
	if (wlanPsHeaderSpacing != 0)
		wlanPsDispatchHeader(wake, &period);
	if (t->missedStreak != 0) {
		++t->compensations;
		*wake -= (uint32_t)((t->missedStreak % 5) << 10);
	}
	*lead = sleepWakeLatency + 345;
	while ((left = *wake - romGetTimerTimestamp()) < 0)
		*wake += period;
	return left;
}

/* The machine must not sleep: a run of beacons is being measured or is missing. */
int wlanPsLifetimeWanted(void)
{
	if (wlanBeaconTracker.measuring != 0)
		return 1;
	if (wlanBeaconTracker.clockOffset != 0)
		return 1;
	return 0;
}

int wlanPsComputeHeader(void)
{
	return wlanBeaconTracker.timer;
}

/* Takes the beacons the machine skips off a count, by what the DTIM policy and the spacing allow. */
unsigned int wlanPsDiscountMissed(uint8_t *count)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	unsigned int spacing = wlanPsHeaderSpacing;
	unsigned int skipped = t->dtimPeriod;
	unsigned int cap = 0;
	unsigned int result;

	if (spacing != 0) {
		if (spacing >= 50)
			cap = (uint8_t)(spacing / t->intervalTu);
		else
			cap = (uint8_t)spacing;
	}
	if (wlanPsDtimPolicy - 2 > 3) {
		if (wlanPsDtimPolicy == 65533 || wlanPsDtimPolicy == 65534)
			skipped = cap;
	} else {
		skipped = (uint8_t)(skipped * wlanPsDtimPolicy);
	}
	if (spacing != 0 && cap < skipped)
		skipped = cap;
	if (*count < skipped)
		result = 1;
	else
		result = *count - skipped;
	*count = result;
	return result;
}

/* Starts the tracking over, as after a join. */
void wlanPsResetHeader(void)
{
	struct WlanBeaconTracker *t = &wlanBeaconTracker;
	UINT posture = sysIrqDisable();

	__aeabi_memclr4(t, sizeof(*t));
	t->measuring = 1;
	t->resetTime = romGetTimerTimestamp();
	t->resetHardwareTime = osaGetHardwareTime();
	t->intervalTu = 100;
	t->interval = 102400;
	t->dtimPeriod = 1;
	t->beaconTime = t->resetTime;
	t->state = WLAN_BEACON_LEARNING;
	t->sampleTarget = 32;
	sysIrqRestore(posture);
	sysLogStatistic(11, (uint32_t)(t->resetTime >> 32), (uint32_t)t->resetTime);
}
