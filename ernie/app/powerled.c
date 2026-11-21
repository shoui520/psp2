/*
 * USS-1001's power LED module: the causes the LEDs show (powerLedFlags.flags), set and cleared by
 * the charge, the power jobs and the host, and the mode they choose, shown on the power LED
 * (powerLedSet) or the charge LED (the Cookie's register 3 bit 2): off, on, or blinking on
 * timer 0x14 (powerLedFlagsStep, from task 5's flag 0x2000).
 */
#include "board.h"

#if SOURCE_0103
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "kernel.h"
#include "led.h"
#include "param.h"
#include "powerseq.h"
#include "state.h"
#include "task.h"
#include "timer.h"
#ifdef ERNIE_UPDATER
#include "../updater/updater.h"
#endif

#if BOARD_DOL1001
/*
 * DOL-1001's power LED: the mode asked for (powerLedStartMode), kept or refused by the system
 * state and the mode shown, ended by powerLedEndMode; the blinks run on timer 0x16
 * (DOL-1002: 0x17; powerLedFlagsStep, from task 5's flag 0x2000).
 */
#if BOARD_DOL1002
#define POWER_LED_TIMER		0x17
#else
#define POWER_LED_TIMER		0x16
#endif
#define POWER_LED_FOREVER	0xff	/* powerLedFlags.count: no end */

#define POWER_LED_TICKS(time)	((time) / 4 + (((time) % 4) ? 1 : 0))

struct PowerLedFlags powerLedFlags;

static void powerLedShowMode(void);
static void powerLedFlagsTimer(void);

void powerLedFlagsInit(unsigned char resumed)
{
	struct TimerHandler handler;

#if UPD_0103
	/* the PS TV's updater: on in the resume */
	switch (resumed) {
	case 0:
		powerLedSet(0);
		powerLedFlags.next = POWER_LED_MODE_OFF;
		powerLedFlags.mode = POWER_LED_MODE_OFF;
		break;
	case 1:
		powerLedStartMode(POWER_LED_MODE_ON);
		break;
	}
#else
	if (resumed == 0) {
		powerLedSet(0);
		powerLedFlags.next = POWER_LED_MODE_OFF;
		powerLedFlags.mode = POWER_LED_MODE_OFF;
	}
#endif
	powerLedFlags.onTime = 0;
	powerLedFlags.offTime = 0;
	powerLedFlags.count = 0;
	powerLedFlags.phase = 0;
	powerLedFlags.blinkOn = 0;
	powerLedFlags.blinkOff = 0;
	powerLedFlags.blinkCount = 0;
	powerLedFlags.hostPattern = 0xff;
	powerLedFlags.hostOn = 0;
	powerLedFlags.hostOff = 0;
	handler.handler = powerLedFlagsTimer;
	timerSetHandler(POWER_LED_TIMER, &handler);
}

/* task 5's (flag 0x2000, timer 0x16): the blink's next phase */
void powerLedFlagsStep(void)
{
	unsigned int time;

	if (powerLedFlags.count != POWER_LED_FOREVER && powerLedFlags.phase == 0 && powerLedFlags.count != 0)
		powerLedFlags.count--;
	switch (powerLedFlags.mode) {
	case POWER_LED_MODE_BLINK:
	case POWER_LED_MODE_HOST_BLINK:
		if (powerLedFlags.phase == 1) {
			powerLedSet(0);
			powerLedFlags.phase = 0;
			time = powerLedFlags.offTime;
		} else {
			powerLedSet(1);
			powerLedFlags.phase = 1;
			time = powerLedFlags.onTime;
		}
		break;
	case POWER_LED_MODE_HOST:
#if UPD_0103
		/* the PS TV's updater has no host pattern */
		break;
#else
		if (powerLedFlags.phase == 1) {
			if (powerLedFlags.offTime != 0) {
				powerLedSet(0);
				powerLedFlags.phase = 0;
				time = powerLedFlags.offTime;
			} else {
				time = powerLedFlags.onTime;
			}
		} else {
			if (powerLedFlags.onTime != 0) {
				powerLedSet(1);
				powerLedFlags.phase = 1;
				time = powerLedFlags.onTime;
			} else {
				time = powerLedFlags.offTime;
			}
		}
		break;
#endif
	case POWER_LED_MODE_OFF:
	case POWER_LED_MODE_ON:
	default:
		powerLedFlags.count = 0;
		break;
	}
	if (powerLedFlags.count != 0)
		timerStart(POWER_LED_TIMER, POWER_LED_TICKS(time));
	else if (stateGet() == STATE_RESUME && powerLedFlags.mode == POWER_LED_MODE_BLINK)
#if BOARD_DOL1002
		powerLedStartMode(POWER_LED_EVENT_BLINK_END);
#else
		powerLedFlags.mode = POWER_LED_MODE_ON;
#endif
}

#if BOARD_DOL1002
/* the event's next mode, by the mode shown, the system state and the wakeup factor (a resume
   woken by the power button blinks); a host pattern returns to the mode it replaced */
void powerLedStartMode(unsigned char event)
{
	unsigned int state;
	union PowerRequest factor;
	unsigned char next;
	unsigned char change;

	state = stateGet();
	factor = *stateGetWakeupFactor();
	next = powerLedFlags.mode;
	change = 0;
	switch (powerLedFlags.mode) {
	case POWER_LED_MODE_OFF:
		switch (event) {
		case POWER_LED_EVENT_RESUME:
			if ((state & 0x20) == 0x20 && factor.bits.mode != 0x20) {
				next = POWER_LED_MODE_ON;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_RESUME_BLINK:
			if ((state & 0x20) == 0x20 && factor.bits.mode == 0x20) {
				next = POWER_LED_MODE_BLINK;
				change = 1;
			}
			break;
		}
		break;
	case POWER_LED_MODE_ON:
		switch (event) {
		case POWER_LED_EVENT_OFF_DONE:
			if ((state & 4) == 4) {
				next = POWER_LED_MODE_OFF;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_SUSPEND_DONE:
			if ((state & 8) == 8) {
				next = POWER_LED_MODE_OFF;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_HOST_BLINK:
			if (state == STATE_RESUME) {
				next = POWER_LED_MODE_HOST_BLINK;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_HOST:
			next = POWER_LED_MODE_HOST;
			powerLedFlags.next = POWER_LED_MODE_ON;
			change = 1;
			break;
		}
		break;
	case POWER_LED_MODE_BLINK:
		switch (event) {
		case POWER_LED_EVENT_BLINK_END:
			if ((state & 0x20) == 0x20)
				next = POWER_LED_MODE_ON;	/* not shown */
			break;
		case POWER_LED_EVENT_OFF:
			if ((state & 4) == 4) {
				next = POWER_LED_MODE_ON;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_SUSPEND:
			if ((state & 8) == 8) {
				next = POWER_LED_MODE_ON;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_HOST_BLINK:
			if (state == STATE_RESUME) {
				next = POWER_LED_MODE_HOST_BLINK;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_HOST:
			next = POWER_LED_MODE_HOST;
			powerLedFlags.next = POWER_LED_MODE_BLINK;
			change = 1;
			break;
		}
		break;
	case POWER_LED_MODE_HOST_BLINK:
		switch (event) {
		case POWER_LED_EVENT_HOST_BLINK_END:
			if (state == STATE_RESUME) {
				next = POWER_LED_MODE_ON;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_OFF:
			if ((state & 4) == 4) {
				next = POWER_LED_MODE_ON;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_SUSPEND:
			if ((state & 8) == 8) {
				next = POWER_LED_MODE_ON;
				change = 1;
			}
			break;
		case POWER_LED_EVENT_HOST_BLINK:
			if (state == STATE_RESUME)
				change = 1;
			break;
		case POWER_LED_EVENT_HOST:
			next = POWER_LED_MODE_HOST;
			powerLedFlags.next = POWER_LED_MODE_HOST_BLINK;
			change = 1;
			break;
		}
		break;
	case POWER_LED_MODE_HOST:
		switch (event) {
		case POWER_LED_EVENT_HOST_BLINK:
			powerLedFlags.next = POWER_LED_MODE_HOST_BLINK;
			break;
		case POWER_LED_EVENT_HOST_BLINK_END:
			powerLedFlags.next = POWER_LED_MODE_ON;
			break;
		case POWER_LED_EVENT_OFF:
		case POWER_LED_EVENT_SUSPEND:
			next = POWER_LED_MODE_ON;
			change = 1;
			break;
		case POWER_LED_EVENT_JOB_END:
			next = powerLedFlags.next;
			change = 1;
			break;
		case POWER_LED_EVENT_HOST:
			change = 1;
			break;
		}
		break;
	}
	if (change == 1) {
		powerLedFlags.mode = next;
		powerLedShowMode();
	}
}
#else
/* the mode asked for, shown when the system state and the mode shown allow it; a host
   pattern's request while the host pattern is shown is its mode to return to */
void powerLedStartMode(unsigned char mode)
{
	unsigned int state;

	state = stateGet();
#if UPD_0103
	/* the PS TV's updater: on whenever asked, the blink from off in the off state, the host
	   blink in the resume */
	switch (mode) {
	case POWER_LED_MODE_OFF:
		break;
	case POWER_LED_MODE_ON:
		break;
	case POWER_LED_MODE_BLINK:
		if (state != STATE_OFF || powerLedFlags.mode != POWER_LED_MODE_OFF)
			return;
		break;
	case POWER_LED_MODE_HOST_BLINK:
		if (!(state == STATE_RESUME &&
		      (powerLedFlags.mode == POWER_LED_MODE_ON ||
		       powerLedFlags.mode == POWER_LED_MODE_BLINK ||
		       powerLedFlags.mode == POWER_LED_MODE_HOST_BLINK))) {
			if (powerLedFlags.mode == POWER_LED_MODE_HOST)
				powerLedFlags.next = mode;
			return;
		}
		break;
	case POWER_LED_MODE_HOST:
	default:
		return;
	}
#else
	switch (mode) {
	case POWER_LED_MODE_OFF:
		break;
	case POWER_LED_MODE_ON:
		if (!(((state == 0x8004 || state == 0x8008) &&
		       (powerLedFlags.mode == POWER_LED_MODE_BLINK ||
			powerLedFlags.mode == POWER_LED_MODE_HOST_BLINK ||
			powerLedFlags.mode == POWER_LED_MODE_HOST)) ||
		      ((state & 0x20) == 0x20 &&
		       (powerLedFlags.mode == POWER_LED_MODE_OFF ||
			powerLedFlags.mode == POWER_LED_MODE_BLINK ||
			powerLedFlags.mode == POWER_LED_MODE_HOST_BLINK)))) {
			return;
		}
		break;
	case POWER_LED_MODE_BLINK:
		if (!(state == 0x8020 && powerLedFlags.mode == POWER_LED_MODE_OFF)) {
			if (powerLedFlags.mode == POWER_LED_MODE_HOST)
				powerLedFlags.next = mode;
			return;
		}
		break;
	case POWER_LED_MODE_HOST_BLINK:
		if (!(state == STATE_RESUME &&
		      (powerLedFlags.mode == POWER_LED_MODE_ON ||
		       powerLedFlags.mode == POWER_LED_MODE_BLINK ||
		       powerLedFlags.mode == POWER_LED_MODE_HOST_BLINK))) {
			if (powerLedFlags.mode == POWER_LED_MODE_HOST)
				powerLedFlags.next = mode;
			return;
		}
		break;
	case POWER_LED_MODE_HOST:
		if ((state & 0x20) == 0x20) {
			if ((powerLedFlags.mode == POWER_LED_MODE_ON ||
			     powerLedFlags.mode == POWER_LED_MODE_BLINK ||
			     powerLedFlags.mode == POWER_LED_MODE_HOST_BLINK ||
			     powerLedFlags.mode == POWER_LED_MODE_HOST) &&
			    powerLedFlags.hostPattern == 0 &&
			    (powerLedFlags.hostOn != 0 || powerLedFlags.hostOff != 0)) {
				/* the host's pattern shown */
			} else {
				return;
			}
		} else {
			return;
		}
		if (powerLedFlags.mode != POWER_LED_MODE_HOST)
			powerLedFlags.next = powerLedFlags.mode;
		break;
	default:
		return;
	}
#endif
	powerLedFlags.mode = mode;
	powerLedShowMode();
}

/* the mode's end: the host blink back to on (in state 0x20) or off, the host pattern back to
   the mode it replaced */
void powerLedEndMode(unsigned char mode)
{
	unsigned int state;

	state = stateGet();
	switch (mode) {
#if !UPD_0103
	case POWER_LED_MODE_HOST:
		if (powerLedFlags.mode == POWER_LED_MODE_HOST) {
			if ((state & 0x20) != 0x20) {
				powerLedFlags.mode = POWER_LED_MODE_OFF;
				powerLedFlags.next = POWER_LED_MODE_OFF;
			} else
				powerLedFlags.mode = powerLedFlags.next;
		} else
			return;
		break;
#endif
	case POWER_LED_MODE_HOST_BLINK:
		if (powerLedFlags.mode == POWER_LED_MODE_HOST_BLINK) {
			if ((state & 0x20) != 0x20) {
				powerLedFlags.mode = POWER_LED_MODE_OFF;
				powerLedFlags.next = POWER_LED_MODE_OFF;
			} else {
				powerLedFlags.mode = POWER_LED_MODE_ON;
				powerLedFlags.next = POWER_LED_MODE_ON;
			}
		} else {
			if (powerLedFlags.mode == POWER_LED_MODE_HOST)
				powerLedFlags.next = POWER_LED_MODE_ON;
			return;
		}
		break;
#if UPD_0103
	case POWER_LED_MODE_HOST:
#endif
	case POWER_LED_MODE_OFF:
	case POWER_LED_MODE_ON:
	case POWER_LED_MODE_BLINK:
	default:
		return;
	}
	powerLedShowMode();
}

#endif

static void powerLedShowMode(void)
{
	unsigned int time;

	time = 0;
	switch (powerLedFlags.mode) {
	case POWER_LED_MODE_OFF:
		timerStop(POWER_LED_TIMER);
		powerLedSet(0);
		break;
	case POWER_LED_MODE_ON:
		timerStop(POWER_LED_TIMER);
		powerLedSet(1);
		break;
	case POWER_LED_MODE_BLINK:
		powerLedFlags.onTime = powerLedFlags.blinkOn;
		powerLedFlags.offTime = powerLedFlags.blinkOff;
		powerLedFlags.count = powerLedFlags.blinkCount;
		powerLedSet(1);
		powerLedFlags.phase = 1;
		timerStart(POWER_LED_TIMER, POWER_LED_TICKS(powerLedFlags.onTime));
		break;
	case POWER_LED_MODE_HOST_BLINK:
		powerLedFlags.onTime = powerLedFlags.blinkOn;
		powerLedFlags.offTime = powerLedFlags.blinkOff;
		powerLedFlags.count = POWER_LED_FOREVER;
		powerLedSet(1);
		powerLedFlags.phase = 1;
		timerStart(POWER_LED_TIMER, POWER_LED_TICKS(powerLedFlags.onTime));
		break;
#if !UPD_0103
	case POWER_LED_MODE_HOST:
		timerStop(POWER_LED_TIMER);
		powerLedFlags.onTime = powerLedFlags.hostOn;
		powerLedFlags.offTime = powerLedFlags.hostOff;
		powerLedFlags.count = POWER_LED_FOREVER;
		if (powerLedFlags.onTime != 0) {
			powerLedSet(1);
			powerLedFlags.phase = 1;
			time = powerLedFlags.onTime;
		} else if (powerLedFlags.offTime != 0) {
			powerLedSet(0);
			powerLedFlags.phase = 0;
			time = powerLedFlags.offTime;
		} else {
			powerLedSet(1);
			powerLedFlags.phase = 1;
			time = 0xffff;
		}
		if (time != 0xffff)
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(time));
		break;
#else
	case POWER_LED_MODE_HOST:	/* the PS TV's updater: no host pattern */
		break;
#endif
	}
}

static void powerLedFlagsTimer(void)
{
#if UPD_0103
	updEventSet(0x97);
#else
	task5SetFlag(4, 0x2000);
#endif
}

/* powerSeqResume's blink: count blinks of on and off ms */
void powerLedSetBlink(unsigned char count, unsigned int on, unsigned int off)
{
	powerLedFlags.blinkCount = count;
	powerLedFlags.blinkOn = on;
	powerLedFlags.blinkOff = off;
}

/* host command 0x0821's (index 0) or 0x0822's (1) blink times: the parameter block's on and off
   times (0x7a-0x7d), in 16 ms, at least one */
void powerLedLoadBlink(unsigned char index)
{
	unsigned char on;
	unsigned char off;

	on = 0;
	off = 0;
	if (index < 2) {
		if (index == 0) {
			paramRead(0x7a, 1, &on);
			paramRead(0x7b, 1, &off);
		} else if (index == 1) {
			paramRead(0x7c, 1, &on);
			paramRead(0x7d, 1, &off);
		}
		if (on == 0)
			on = 1;
		if (off == 0)
			off = 1;
		powerLedFlags.blinkOn = (unsigned int)on << 4;
		powerLedFlags.blinkOff = (unsigned int)off << 4;
	}
}

#if !UPD_0103
/* host request 0x089f's: the pattern (0 the power LED; others none) and its times, in ms */
void powerLedSetPattern(unsigned char pattern, unsigned int on, unsigned int off)
{
	powerLedFlags.hostPattern = pattern;
	powerLedFlags.hostOn = on;
	powerLedFlags.hostOff = off;
}
#endif
#else
/* the slim's 0x0105 (USS-1002) */
#define POWER_LED_0105		(BOARD_USS1001 && SOURCE_0104)

#if POWER_LED_0105
#define POWER_LED_TIMER		0x11
#else
#define POWER_LED_TIMER		0x14
#endif

/* the modes (powerLedFlags.mode) */
#define POWER_LED_MODE_OFF	1
#define POWER_LED_MODE_ON	2
#define POWER_LED_MODE_LOW	3	/* the power LED blinks, 0.5 s on, 2 s off */
#define POWER_LED_MODE_BLINK	4	/* and with the parameter block's times */
#define POWER_LED_MODE_ALARM	5	/* the charge LED blinks five times */
#define POWER_LED_MODE_CHARGING	6	/* the charge LED on */
#define POWER_LED_MODE_HOST	7	/* the host's pattern */

/* the LED shown (powerLedFlagsShow) */
#define POWER_LED_SHOW_NONE	0
#define POWER_LED_SHOW_POWER	1
#define POWER_LED_SHOW_CHARGE	2

#define POWER_LED_FOREVER	0xff	/* powerLedFlags.count: no end */

#define POWER_LED_TICKS(time)	((time) / 4 + (((time) % 4) ? 1 : 0))

struct PowerLedFlags powerLedFlags;

static void powerLedFlagsTimer(void);
static unsigned int powerLedFlagsCharging(void);
static void powerLedFlagsShow(unsigned char led);
#ifndef ERNIE_UPDATER
static void powerLedFlagsStart(unsigned char pattern, unsigned int on, unsigned int off);
#endif

void powerLedFlagsInit(unsigned char resumed)
{
	struct TimerHandler handler;

	powerLedFlags.onTime = 0;
	powerLedFlags.offTime = 0;
	powerLedFlags.count = 0;
	powerLedFlags.phase = 0;
	powerLedFlags.blinkOn = 0;
	powerLedFlags.blinkOff = 0;
	powerLedFlags.hostPattern = 0xff;
	powerLedFlags.pattern = 0xff;
	powerLedFlags.hostOn = 0;
	powerLedFlags.hostOff = 0;
	handler.handler = powerLedFlagsTimer;
	timerSetHandler(POWER_LED_TIMER, &handler);
#ifdef ERNIE_UPDATER
	switch (resumed) {
	case 0:
#else
	if (resumed == 0) {
#endif
		powerLedFlags.mode = POWER_LED_MODE_OFF;
		powerLedFlags.causes.value = 0;
		powerLedFlagsShow(POWER_LED_SHOW_NONE);
		if (chargeBatteryLevel3())
			powerLedFlags.causes.bits.low = 1;
		if (configGet()->state.bits.chargeLedOff)
			powerLedFlags.causes.bits.off = 1;
		if (configGet()->state.bits.chargeLedStateOff)
			powerLedFlags.causes.bits.stateOff = 1;
#ifdef ERNIE_UPDATER
		break;
	case 1:	/* the slim updater's: resumed, the mode the causes choose at once */
		powerLedFlags.mode = 0;
		powerLedFlagsUpdate();
		break;
#endif
	}
}

/* the mode the causes choose, started when it changes */
void powerLedFlagsUpdate(void)
{
	unsigned char mode;

	if (powerLedFlags.causes.bits.alarm && !powerLedFlags.causes.bits.off &&
	    !(powerLedFlags.causes.bits.stateOff && powerLedFlags.causes.bits.on))
		mode = POWER_LED_MODE_ALARM;
#if POWER_LED_0105
	else if (powerLedFlags.mode == POWER_LED_MODE_ALARM && powerLedFlags.causes.bits.alarm)
		mode = POWER_LED_MODE_ALARM;	/* the alarm's blinks run to their end */
#endif
#ifndef ERNIE_UPDATER	/* the slim updater has no host pattern */
	else if (powerLedFlags.causes.bits.host)
		mode = POWER_LED_MODE_HOST;
#endif
	else if (powerLedFlagsCharging() != 0)
		mode = POWER_LED_MODE_CHARGING;
	else if (powerLedFlags.causes.bits.hostBlink &&
		 !powerLedFlags.causes.bits.resumed)
		mode = POWER_LED_MODE_BLINK;
	else if (powerLedFlags.causes.bits.low && powerLedFlags.causes.bits.on &&
		 !powerLedFlags.causes.bits.resumed)
		mode = POWER_LED_MODE_LOW;
	else if (powerLedFlags.causes.bits.on && !powerLedFlags.causes.bits.resumed)
		mode = POWER_LED_MODE_ON;
	else
		mode = POWER_LED_MODE_OFF;
	if (mode != powerLedFlags.mode || mode == POWER_LED_MODE_BLINK || mode == POWER_LED_MODE_HOST) {
		switch (mode) {
		case POWER_LED_MODE_OFF:
			timerStop(POWER_LED_TIMER);
			powerLedFlagsShow(POWER_LED_SHOW_NONE);
			break;
		case POWER_LED_MODE_ON:
			timerStop(POWER_LED_TIMER);
			powerLedFlagsShow(POWER_LED_SHOW_POWER);
			break;
		case POWER_LED_MODE_LOW:
			powerLedFlags.onTime = 500;
			powerLedFlags.offTime = 2000;
			powerLedFlags.count = POWER_LED_FOREVER;
			powerLedFlagsShow(POWER_LED_SHOW_POWER);
			powerLedFlags.phase = 1;
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(powerLedFlags.onTime));
			break;
		case POWER_LED_MODE_BLINK:
#if POWER_LED_0105
			/* restarted only when its times changed */
			if (mode != powerLedFlags.mode || powerLedFlags.onTime != powerLedFlags.blinkOn ||
			    powerLedFlags.offTime != powerLedFlags.blinkOff) {
				powerLedFlags.onTime = powerLedFlags.blinkOn;
				powerLedFlags.offTime = powerLedFlags.blinkOff;
				powerLedFlags.count = POWER_LED_FOREVER;
				powerLedFlagsShow(POWER_LED_SHOW_POWER);
				powerLedFlags.phase = 1;
				timerStart(POWER_LED_TIMER, POWER_LED_TICKS(powerLedFlags.onTime));
			}
#else
			powerLedFlags.onTime = powerLedFlags.blinkOn;
			powerLedFlags.offTime = powerLedFlags.blinkOff;
			powerLedFlags.count = POWER_LED_FOREVER;
			powerLedFlagsShow(POWER_LED_SHOW_POWER);
			powerLedFlags.phase = 1;
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(powerLedFlags.onTime));
#endif
			break;
		case POWER_LED_MODE_ALARM:
			powerLedFlags.onTime = powerLedFlags.offTime = 200;
			powerLedFlags.count = 5;
			powerLedFlagsShow(POWER_LED_SHOW_CHARGE);
			powerLedFlags.phase = 1;
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(powerLedFlags.onTime));
			break;
		case POWER_LED_MODE_CHARGING:
			timerStop(POWER_LED_TIMER);
			powerLedFlagsShow(POWER_LED_SHOW_CHARGE);
			break;
		case POWER_LED_MODE_HOST:
#ifndef ERNIE_UPDATER
			if (mode != powerLedFlags.mode || powerLedFlags.pattern != powerLedFlags.hostPattern ||
			    powerLedFlags.onTime != powerLedFlags.hostOn || powerLedFlags.offTime != powerLedFlags.hostOff) {
				timerStop(POWER_LED_TIMER);
				powerLedFlags.onTime = powerLedFlags.hostOn;
				powerLedFlags.offTime = powerLedFlags.hostOff;
				powerLedFlags.count = POWER_LED_FOREVER;
				powerLedFlagsStart(powerLedFlags.hostPattern, powerLedFlags.onTime, powerLedFlags.offTime);
			}
#endif
			break;
		}
	}
	powerLedFlags.mode = mode;
}

/* task 5's (flag 0x2000, timer 0x14): the blink's next phase */
void powerLedFlagsStep(void)
{
	unsigned int time;

	if (powerLedFlags.count != POWER_LED_FOREVER && powerLedFlags.phase == 0 && powerLedFlags.count != 0)
		powerLedFlags.count--;
	switch (powerLedFlags.mode) {
	case POWER_LED_MODE_LOW:
	case POWER_LED_MODE_BLINK:
		if (powerLedFlags.phase == 1) {
			powerLedFlagsShow(POWER_LED_SHOW_NONE);
			powerLedFlags.phase = 0;
			time = powerLedFlags.offTime;
		} else {
			powerLedFlagsShow(POWER_LED_SHOW_POWER);
			powerLedFlags.phase = 1;
			time = powerLedFlags.onTime;
		}
		break;
	case POWER_LED_MODE_ALARM:
		if (powerLedFlags.count == 0) {
			powerLedClearFlags(POWER_LED_FLAG_ALARM);
		} else if (powerLedFlags.phase == 1) {
			powerLedFlagsShow(POWER_LED_SHOW_NONE);
			powerLedFlags.phase = 0;
			time = powerLedFlags.offTime;
		} else {
			powerLedFlagsShow(POWER_LED_SHOW_CHARGE);
			powerLedFlags.phase = 1;
			time = powerLedFlags.onTime;
		}
		break;
	case POWER_LED_MODE_HOST:
#ifndef ERNIE_UPDATER
		if (powerLedFlags.phase == 1) {
			if (powerLedFlags.offTime != 0) {
				switch (powerLedFlags.pattern) {
				case 0:
				case 1:
					powerLedFlagsShow(POWER_LED_SHOW_NONE);
					break;
				case 2:
					powerLedFlagsShow(POWER_LED_SHOW_CHARGE);
					break;
				default:
					powerLedFlags.count = 0;
					powerLedClearFlags(POWER_LED_FLAG_HOST);
					break;
				}
				powerLedFlags.phase = 0;
				time = powerLedFlags.offTime;
			} else {
				time = powerLedFlags.onTime;
			}
		} else {
			if (powerLedFlags.onTime != 0) {
				switch (powerLedFlags.pattern) {
				case 0:
					powerLedFlagsShow(POWER_LED_SHOW_POWER);
					break;
				case 1:
					powerLedFlagsShow(POWER_LED_SHOW_CHARGE);
					break;
				case 2:
					powerLedFlagsShow(POWER_LED_SHOW_POWER);
					break;
				default:
					powerLedFlags.count = 0;
					powerLedClearFlags(POWER_LED_FLAG_HOST);
					break;
				}
				powerLedFlags.phase = 1;
				time = powerLedFlags.onTime;
			} else {
				time = powerLedFlags.offTime;
			}
		}
#endif
		break;
	case POWER_LED_MODE_OFF:
	case POWER_LED_MODE_ON:
	case POWER_LED_MODE_CHARGING:
	default:
		powerLedFlags.count = 0;
		break;
	}
	if (powerLedFlags.count != 0)
		timerStart(POWER_LED_TIMER, POWER_LED_TICKS(time));
}

#ifdef ERNIE_UPDATER
/* the slim updater's: its main loop's events 0x96 (powerLedFlagsUpdate) and 0x97
   (powerLedFlagsStep) */
#define POWER_LED_UPDATE()	updEventSet(0x96)
#define POWER_LED_STEP()	updEventSet(0x97)
#else
#define POWER_LED_UPDATE()	task5SetFlag(4, 0x0800)
#if POWER_LED_0105
#define POWER_LED_STEP()	task5ISetFlag(0x2000)
#else
#define POWER_LED_STEP()	task5SetFlag(4, 0x2000)
#endif
#endif

/* the causes set or cleared for task 5 (event flag 4's 0x0800) */
void powerLedSetFlags(unsigned int flags)
{
	dis_dsp();
	powerLedFlags.causes.value |= flags;
	ena_dsp();
	POWER_LED_UPDATE();
}

void powerLedClearFlags(unsigned int flags)
{
	dis_dsp();
	powerLedFlags.causes.value &= ~flags;
	ena_dsp();
	POWER_LED_UPDATE();
}

/* and at once */
void powerLedSetFlagsNow(unsigned int flags)
{
	dis_dsp();
	powerLedFlags.causes.value |= flags;
	ena_dsp();
	powerLedFlagsUpdate();
}

void powerLedClearFlagsNow(unsigned int flags)
{
	dis_dsp();
	powerLedFlags.causes.value &= ~flags;
	ena_dsp();
	powerLedFlagsUpdate();
}

/* host command 0x0821's (index 0) or 0x0822's (1) blink times: the parameter block's on and off
   times (0x7a-0x7d), in 16 ms, at least one */
void powerLedLoadBlink(unsigned char index)
{
	unsigned char on;
	unsigned char off;

	on = 0;
	off = 0;
	if (index < 2) {
		if (index == 0) {
			paramRead(0x7a, 1, &on);
			paramRead(0x7b, 1, &off);
		} else if (index == 1) {
			paramRead(0x7c, 1, &on);
			paramRead(0x7d, 1, &off);
		}
		if (on == 0)
			on = 1;
		if (off == 0)
			off = 1;
		powerLedFlags.blinkOn = (unsigned int)on << 4;
		powerLedFlags.blinkOff = (unsigned int)off << 4;
	}
}

static void powerLedFlagsTimer(void)
{
	POWER_LED_STEP();
}

/* the charge LED on: charging, not full, and the USB powered, a resume, or the dongle or
   debug adapter (or USB status 1) in system state 4 or 8 */
static unsigned int powerLedFlagsCharging(void)
{
	unsigned int state;
	unsigned char usbPower;
	unsigned char usbIs1;
#if POWER_LED_0105
	unsigned char usbDongle;
	unsigned char usbDebug;
#else
	unsigned char usbAdapter;
#endif

	state = stateGet();
	usbPower = chargeUsbPower();
	usbIs1 = chargeUsbIs1();
#if POWER_LED_0105
	usbDongle = chargeUsbDongle();
	usbDebug = chargeUsbDebug();
	return !powerLedFlags.causes.bits.off &&
	       !(powerLedFlags.causes.bits.stateOff && powerLedFlags.causes.bits.on) &&
	       powerLedFlags.causes.bits.charging && !powerLedFlags.causes.bits.full &&
	       (usbPower != 0 || powerLedFlags.causes.bits.resumed ||
		((usbIs1 != 0 || usbDongle != 0 || usbDebug != 0) && (state == 8 || state == 4)));
#elif APP_SINCE(0x01030511)
	usbAdapter = chargeUsbDongle() || chargeUsbDebug();
#else
	usbAdapter = chargeUsbDongle();
#endif
#if !POWER_LED_0105
	return !powerLedFlags.causes.bits.off &&
	       !(powerLedFlags.causes.bits.stateOff && powerLedFlags.causes.bits.on) &&
	       powerLedFlags.causes.bits.charging && !powerLedFlags.causes.bits.full &&
	       (usbPower != 0 || powerLedFlags.causes.bits.resumed ||
		((usbIs1 != 0 || usbAdapter != 0) && (state == 8 || state == 4)));
#endif
}

static void powerLedFlagsShow(unsigned char led)
{
	switch (led) {
	case POWER_LED_SHOW_NONE:
		powerLedSet(0);
		cookieCtrlReg3Bit2(0);
		break;
	case POWER_LED_SHOW_POWER:
		cookieCtrlReg3Bit2(0);
		powerLedSet(1);
		break;
	case POWER_LED_SHOW_CHARGE:
		powerLedSet(0);
		cookieCtrlReg3Bit2(1);
		break;
	}
}

#ifndef ERNIE_UPDATER
/* host request 0x089f's: the pattern (0 the power LED and off, 1 the charge LED and off, 2 the
   power LED and the charge LED; 0xff none) and its times, in ms */
void powerLedSetPattern(unsigned char pattern, unsigned int on, unsigned int off)
{
	powerLedFlags.hostPattern = pattern;
	powerLedFlags.hostOn = on;
	powerLedFlags.hostOff = off;
}

static void powerLedFlagsStart(unsigned char pattern, unsigned int on, unsigned int off)
{
	unsigned int time;

	time = 0;
	if (on == 0 && off == 0) {
		powerLedClearFlags(POWER_LED_FLAG_HOST);
		return;
	}
	switch (pattern) {
	case 0:
		if (on != 0) {
			powerLedFlagsShow(POWER_LED_SHOW_POWER);
			powerLedFlags.phase = 1;
			time = on;
		} else if (off != 0) {
			powerLedFlagsShow(POWER_LED_SHOW_NONE);
			powerLedFlags.phase = 0;
			time = off;
		}
		if (time != 0xffff)
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(time));
		powerLedFlags.pattern = pattern;
		break;
	case 1:
		if (on != 0) {
			powerLedFlagsShow(POWER_LED_SHOW_CHARGE);
			powerLedFlags.phase = 1;
			time = on;
		} else if (off != 0) {
			powerLedFlagsShow(POWER_LED_SHOW_NONE);
			powerLedFlags.phase = 0;
			time = off;
		}
		if (time != 0xffff)
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(time));
		powerLedFlags.pattern = pattern;
		break;
	case 2:
		if (on != 0) {
			powerLedFlagsShow(POWER_LED_SHOW_POWER);
			powerLedFlags.phase = 1;
			time = on;
		} else if (off != 0) {
			powerLedFlagsShow(POWER_LED_SHOW_CHARGE);
			powerLedFlags.phase = 0;
			time = off;
		}
		if (time != 0xffff)
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(time));
		powerLedFlags.pattern = pattern;
		break;
	default:
		powerLedClearFlags(POWER_LED_FLAG_HOST);
		break;
	}
}
#endif
#endif
#endif
