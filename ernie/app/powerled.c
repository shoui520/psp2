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

#if BOARD_DOL1001
/*
 * DOL-1001's power LED: the mode asked for (powerLedStartMode), kept or refused by the system
 * state and the mode shown, ended by powerLedEndMode; the blinks run on timer 0x16
 * (powerLedFlagsStep, from task 5's flag 0x2000).
 */
#define POWER_LED_TIMER		0x16
#define POWER_LED_FOREVER	0xff	/* powerLedFlags.count: no end */

#define POWER_LED_TICKS(time)	((time) / 4 + (((time) % 4) ? 1 : 0))

struct PowerLedFlags powerLedFlags;

static void powerLedShowMode(void);
static void powerLedFlagsTimer(void);

void powerLedFlagsInit(unsigned char resumed)
{
	struct TimerHandler handler;

	if (resumed == 0) {
		powerLedSet(0);
		powerLedFlags.next = POWER_LED_MODE_OFF;
		powerLedFlags.mode = POWER_LED_MODE_OFF;
	}
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
	case POWER_LED_MODE_OFF:
	case POWER_LED_MODE_ON:
	default:
		powerLedFlags.count = 0;
		break;
	}
	if (powerLedFlags.count != 0)
		timerStart(POWER_LED_TIMER, POWER_LED_TICKS(time));
	else if (stateGet() == STATE_RESUME && powerLedFlags.mode == POWER_LED_MODE_BLINK)
		powerLedFlags.mode = POWER_LED_MODE_ON;
}

/* the mode asked for, shown when the system state and the mode shown allow it; a host
   pattern's request while the host pattern is shown is its mode to return to */
void powerLedStartMode(unsigned char mode)
{
	unsigned int state;

	state = stateGet();
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
	case POWER_LED_MODE_OFF:
	case POWER_LED_MODE_ON:
	case POWER_LED_MODE_BLINK:
	default:
		return;
	}
	powerLedShowMode();
}

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
	}
}

static void powerLedFlagsTimer(void)
{
	task5SetFlag(4, 0x2000);
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

/* host request 0x089f's: the pattern (0 the power LED; others none) and its times, in ms */
void powerLedSetPattern(unsigned char pattern, unsigned int on, unsigned int off)
{
	powerLedFlags.hostPattern = pattern;
	powerLedFlags.hostOn = on;
	powerLedFlags.hostOff = off;
}
#else
#define POWER_LED_TIMER		0x14

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
static void powerLedFlagsStart(unsigned char pattern, unsigned int on, unsigned int off);

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
	if (resumed == 0) {
		powerLedFlags.mode = POWER_LED_MODE_OFF;
		powerLedFlags.causes.value = 0;
		powerLedFlagsShow(POWER_LED_SHOW_NONE);
		if (chargeBatteryLevel3())
			powerLedFlags.causes.bits.low = 1;
		if (configGet()->state.bits.chargeLedOff)
			powerLedFlags.causes.bits.off = 1;
		if (configGet()->state.bits.chargeLedStateOff)
			powerLedFlags.causes.bits.stateOff = 1;
	}
}

/* the mode the causes choose, started when it changes */
void powerLedFlagsUpdate(void)
{
	unsigned char mode;

	if (powerLedFlags.causes.bits.alarm && !powerLedFlags.causes.bits.off &&
	    !(powerLedFlags.causes.bits.stateOff && powerLedFlags.causes.bits.on))
		mode = POWER_LED_MODE_ALARM;
	else if (powerLedFlags.causes.bits.host)
		mode = POWER_LED_MODE_HOST;
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
			powerLedFlags.onTime = powerLedFlags.blinkOn;
			powerLedFlags.offTime = powerLedFlags.blinkOff;
			powerLedFlags.count = POWER_LED_FOREVER;
			powerLedFlagsShow(POWER_LED_SHOW_POWER);
			powerLedFlags.phase = 1;
			timerStart(POWER_LED_TIMER, POWER_LED_TICKS(powerLedFlags.onTime));
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
			if (mode != powerLedFlags.mode || powerLedFlags.pattern != powerLedFlags.hostPattern ||
			    powerLedFlags.onTime != powerLedFlags.hostOn || powerLedFlags.offTime != powerLedFlags.hostOff) {
				timerStop(POWER_LED_TIMER);
				powerLedFlags.onTime = powerLedFlags.hostOn;
				powerLedFlags.offTime = powerLedFlags.hostOff;
				powerLedFlags.count = POWER_LED_FOREVER;
				powerLedFlagsStart(powerLedFlags.hostPattern, powerLedFlags.onTime, powerLedFlags.offTime);
			}
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

/* the causes set or cleared for task 5 (event flag 4's 0x0800) */
void powerLedSetFlags(unsigned int flags)
{
	dis_dsp();
	powerLedFlags.causes.value |= flags;
	ena_dsp();
	task5SetFlag(4, 0x0800);
}

void powerLedClearFlags(unsigned int flags)
{
	dis_dsp();
	powerLedFlags.causes.value &= ~flags;
	ena_dsp();
	task5SetFlag(4, 0x0800);
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
	task5SetFlag(4, 0x2000);
}

/* the charge LED on: charging, not full, and the USB powered, a resume, or the dongle or
   debug adapter (or USB status 1) in system state 4 or 8 */
static unsigned int powerLedFlagsCharging(void)
{
	unsigned int state;
	unsigned char usbPower;
	unsigned char usbIs1;
	unsigned char usbAdapter;

	state = stateGet();
	usbPower = chargeUsbPower();
	usbIs1 = chargeUsbIs1();
#if FW_SINCE(0x01030511)
	usbAdapter = chargeUsbDongle() || chargeUsbDebug();
#else
	usbAdapter = chargeUsbDongle();
#endif
	return !powerLedFlags.causes.bits.off &&
	       !(powerLedFlags.causes.bits.stateOff && powerLedFlags.causes.bits.on) &&
	       powerLedFlags.causes.bits.charging && !powerLedFlags.causes.bits.full &&
	       (usbPower != 0 || powerLedFlags.causes.bits.resumed ||
		((usbIs1 != 0 || usbAdapter != 0) && (state == 8 || state == 4)));
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
