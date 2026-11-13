/*
 * The LEDs. The charge LED is the Cookie's: chargeLedUpdate picks its mode from its requests,
 * the charge and the system state, and blinks it on timer 0x18. The power LED is the
 * Elmo's: powerLedUpdate picks its mode, and powerLedFadeStep, in task 5, steps a pattern's
 * fade out and in, holding after each, until the mode changes.
 */
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "elmo.h"
#include "kernel.h"
#include "led.h"
#include "param.h"
#include "powerreq.h"
#include "powerseq.h"
#include "state.h"
#include "task.h"
#include "timer.h"

/* a step's ticks of timer 0x14: a quarter of the pattern's time, rounded up */
#define LED_STEP_TICKS(time)	(((time) >> 2) + (((time) & 3) ? 1 : 0))

static void chargeLedBlinkStop(void);
static void chargeLedBlinkStart(unsigned char onTime, unsigned char offTime,
				unsigned char count);

#if FW_SINCE(0x01000306)
static const struct LedPattern ledPatternDefault = {
	{ 0x00, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0x01 },
	{ 0x01, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0x00 },
	0x0c, 0x12,
};
#else
/* before 0x01000306 pattern 2 is this one, not the parameter block's */
static const struct LedPattern ledPattern2Default = {
	{ 0x00, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 0x01 },
	{ 0x01, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 0x00 },
	0x04, 0x0a,
};

static const struct LedPattern ledPatternDefault = {
	{ 0x00, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 0x01 },
	{ 0x01, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 0x00 },
	0x08, 0x0a,
};
#endif

static struct LedPattern powerLedPattern;	/* a pattern of the parameter block */
static struct LedBlink chargeLedBlink;
static struct LedFade powerLedFade;
static struct Leds leds;

static void chargeLedBlinkExpire(void)
{
	switch (leds.charge.mode) {
#if FW_SINCE(0x01000107)
	case CHARGE_LED_AUTO:
	case CHARGE_LED_OFF:
	case CHARGE_LED_HOST:
		break;
#else
	case CHARGE_LED_BLINK1:
	case CHARGE_LED_BLINK2:
#endif
	case CHARGE_LED_BLINK:
		if (chargeLedBlink.on == 1) {
			chargeLedBlink.on = 0;
			timerStart(CHARGE_LED_TIMER, chargeLedBlink.offTime - 1);
		} else {
			if (chargeLedBlink.count != 0) {
				chargeLedBlink.count--;
				if (chargeLedBlink.count == 0) {
#if !FW_SINCE(0x01000107)
					/* a counted blink's end ends its request */
					if (leds.charge.mode == CHARGE_LED_BLINK1)
						chargeLedSetBlink1(0);
					else if (leds.charge.mode == CHARGE_LED_BLINK2)
						chargeLedSetBlink2(0);
#endif
					break;
				}
			}
			chargeLedBlink.on = 1;
			timerStart(CHARGE_LED_TIMER, chargeLedBlink.onTime - 1);
		}
		task5SetFlag(4, 0x2000);
		break;
	}
}

/* task 5's (flag 0x2000): the blink's state to the Cookie */
void chargeLedBlinkUpdate(void)
{
	switch (leds.charge.mode) {
	case CHARGE_LED_AUTO:
	case CHARGE_LED_OFF:
	case CHARGE_LED_HOST:
		break;
#if !FW_SINCE(0x01000107)
	case CHARGE_LED_BLINK1:
	case CHARGE_LED_BLINK2:
#endif
	case CHARGE_LED_BLINK:
		cookieCtrlReg3Bit2(chargeLedBlink.on);
		break;
	}
}

void chargeLedInit(void)
{
	struct TimerHandler handler;

	chargeLedBlink.on = 0;
	chargeLedBlink.count = 0;
	handler.handler = chargeLedBlinkExpire;
	timerSetHandler(CHARGE_LED_TIMER, &handler);
	leds.charge.request.value = 0;
	leds.charge.shown.value = 0;
#if FW_SINCE(0x01000107)
	leds.charge.mode = CHARGE_LED_AUTO;
#else
	leds.charge.mode = CHARGE_LED_NONE;
#endif
	if (configGet()->state.bits.chargeLedOff)
		chargeLedSetOff(1);
#if FW_SINCE(0x01000306)
	if (configGet()->state.bits.chargeLedStateOff)
		chargeLedSetStateOff(1);
#endif
	chargeLedUpdate();
}

void chargeLedSetOff(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.off = 1;
	else
		leds.charge.request.bits.off = 0;
	task5SetFlag(4, 0x0800);
}

#if !FW_SINCE(0x01000107)
void chargeLedSetBlink1(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.blink1 = 1;
	else
		leds.charge.request.bits.blink1 = 0;
	task5SetFlag(4, 0x0800);
}

void chargeLedSetBlink2(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.blink2 = 1;
	else
		leds.charge.request.bits.blink2 = 0;
	task5SetFlag(4, 0x0800);
}
#endif

#if FW_SINCE(0x01000306)
void chargeLedSetStateOff(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.stateOff = 1;
	else
		leds.charge.request.bits.stateOff = 0;
	task5SetFlag(4, 0x0800);
}
#endif

void chargeLedSetBlink(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.blink = 1;
	else
		leds.charge.request.bits.blink = 0;
	task5SetFlag(4, 0x0800);
}

void chargeLedSetHost(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.host = 1;
	else
		leds.charge.request.bits.host = 0;
	task5SetFlag(4, 0x0800);
}

void chargeLedSetHostOff(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.hostOff = 1;
	else
		leds.charge.request.bits.hostOff = 0;
	task5SetFlag(4, 0x0800);
}

void chargeLedSetReleased(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.released = 1;
	else
		leds.charge.request.bits.released = 0;
	task5SetFlag(4, 0x0800);
}

#if FW_SINCE(0x01000107)
void chargeLedSetCharging(unsigned char on)
{
	if (on == 1)
		leds.charge.request.bits.charging = 1;
	else
		leds.charge.request.bits.charging = 0;
	task5SetFlag(4, 0x0800);
}
#endif

void powerLedSetAlarm(unsigned char on)
{
	if (on == 1)
		leds.power.request.bits.alarm = 1;
	else
		leds.power.request.bits.alarm = 0;
	task5SetFlag(4, 0x0800);
}

/* one fade, then off */
void powerLedFadeOff(void)
{
#if FW_SINCE(0x01000306)
	leds.power.request.value = (leds.power.request.value & 0xfd) | 0x41;
#else
	leds.power.request.value = (leds.power.request.value & 0xa0) | 0x41;
#endif
}

/* one fade, then on */
void powerLedFadeOn(void)
{
#if FW_SINCE(0x01000306)
	leds.power.request.value = (leds.power.request.value & 0xfe) | 0x42;
#else
	leds.power.request.value = (leds.power.request.value & 0xa0) | 0x42;
#endif
}

void powerLedSetKeep(unsigned char keep)
{
	powerLedFade.keep = keep;
}

unsigned char powerLedGetKeep(void)
{
	return powerLedFade.keep;
}

/* POWER_LED_SET_*: one of the power LED's modes, or none */
void powerLedSetMode(unsigned char mode)
{
	switch (mode) {
	case POWER_LED_SET_NONE:
		leds.power.request.value = 0;
		break;
	case POWER_LED_SET_OFF:
		leds.power.request.bits.off = 1;
		leds.power.request.value &= 0xe1;
		break;
	case POWER_LED_SET_ON:
		leds.power.request.bits.on = 1;
		leds.power.request.value &= 0xe2;
		break;
	case POWER_LED_SET_PATTERN1:
		leds.power.request.bits.pattern1 = 1;
		leds.power.request.value &= 0xe7;
		break;
	case POWER_LED_SET_PATTERN2:
		leds.power.request.bits.pattern2 = 1;
		leds.power.request.value &= 0xeb;
		break;
	case POWER_LED_SET_PATTERN3:
		leds.power.request.bits.pattern3 = 1;
		leds.power.request.value &= 0xf3;
		break;
	}
	task5SetFlag(4, 0x0800);
}

#if FW_SINCE(0x01000306)
/* both LEDs off */
void ledSetOff(unsigned char on)
{
	if (on == 1) {
		leds.power.request.bits.allOff = 1;
		leds.charge.request.bits.allOff = 1;
	} else {
		leds.power.request.bits.allOff = 0;
		leds.charge.request.bits.allOff = 0;
	}
	task5SetFlag(4, 0x0800);
}
#endif

static void chargeLedBlinkStop(void)
{
	timerStop(CHARGE_LED_TIMER);
	chargeLedBlink.on = 0;
}

static void chargeLedBlinkStart(unsigned char onTime, unsigned char offTime,
				unsigned char count)
{
	chargeLedBlink.onTime = onTime;
	chargeLedBlink.offTime = offTime;
	if (count != 0)
		chargeLedBlink.count = count + 1;
	chargeLedBlink.on = 1;
	cookieCtrlReg3Bit2(1);
	timerStart(CHARGE_LED_TIMER, chargeLedBlink.onTime);
}

#if !FW_SINCE(0x01000107)
/* before 0x01000107 the requests are taken in turn, the counted blinks and the host's mode put
   the power LED's alarm on, and the charge does not decide */
void chargeLedUpdate(void)
{
	unsigned char mode;
	unsigned char onTime;
	unsigned char offTime;

	mode = leds.charge.mode;
	if (leds.charge.request.value & CHARGE_LED_REQUEST_OFF)
		mode = CHARGE_LED_OFF;
	else if (leds.charge.request.value & CHARGE_LED_REQUEST_BLINK1)
		mode = CHARGE_LED_BLINK1;
	else if (leds.charge.request.value & CHARGE_LED_REQUEST_BLINK)
		mode = CHARGE_LED_BLINK;
	else if (leds.charge.request.value & CHARGE_LED_REQUEST_HOST_OFF)
		mode = CHARGE_LED_OFF;
	else if (leds.charge.request.value & CHARGE_LED_REQUEST_HOST)
		mode = CHARGE_LED_HOST;
	else if (leds.charge.request.value & CHARGE_LED_REQUEST_RELEASED)
		mode = CHARGE_LED_OFF;
	else
		mode = CHARGE_LED_AUTO;
	/* CHARGE_LED_HOST and the blinks */
	if (mode & 0x0e)
		leds.power.request.bits.alarm = 1;
	else
		leds.power.request.bits.alarm = 0;
	if (mode != leds.charge.mode) {
		switch (mode) {
		case CHARGE_LED_AUTO:
			chargeLedBlinkStop();
			cookieCtrlReg3Mode(0);
			break;
		case CHARGE_LED_HOST:
			chargeLedBlinkStop();
			cookieCtrlReg3Bit0();
			break;
		case CHARGE_LED_OFF:
			chargeLedBlinkStop();
			cookieCtrlReg3Bit2(0);
			break;
		case CHARGE_LED_BLINK1:
			chargeLedBlinkStart(configGet()->chargeLedBlink1On,
					    configGet()->chargeLedBlink1Off,
					    configGet()->chargeLedBlink1Count);
			break;
		case CHARGE_LED_BLINK2:
			chargeLedBlinkStart(configGet()->paramSetting1, configGet()->paramSetting2,
					    configGet()->chargeLedBlink2Count);
			break;
		case CHARGE_LED_BLINK:
			paramRead(0x54, 1, &onTime);
			paramRead(0x55, 1, &offTime);
			chargeLedBlinkStart(onTime, offTime, 0);
			break;
		}
		leds.charge.mode = mode;
	}
	leds.charge.shown = leds.charge.request;
}
#else
void chargeLedUpdate(void)
{
	unsigned char mode;
	unsigned char onTime;
	unsigned char offTime;
	unsigned char charging;
#if FW_SINCE(0x01000306)
	unsigned int state;

	state = stateGet();
	if ((cookieStatus2.byte[1] & 0x0c) == 0x04 &&
	    ((leds.charge.request.value & CHARGE_LED_REQUEST_CHARGING) || (state & 0x0004)))
		charging = 1;
	else
		charging = 0;
	if ((leds.charge.request.value & CHARGE_LED_REQUEST_OFF) ||
	    ((leds.charge.request.value & CHARGE_LED_REQUEST_STATE_OFF) && (state & 0x0020)))
		mode = CHARGE_LED_OFF;
#else
	if ((cookieStatus2.byte[1] & 0x0c) == 0x04 &&
	    ((leds.charge.request.value & CHARGE_LED_REQUEST_CHARGING) || (stateGet() & 0x0004)))
		charging = 1;
	else
		charging = 0;
	if (leds.charge.request.value & CHARGE_LED_REQUEST_OFF)
		mode = CHARGE_LED_OFF;
#endif
	else {
		switch (leds.charge.mode) {
		case CHARGE_LED_AUTO:
			if (leds.charge.request.value & CHARGE_LED_REQUEST_BLINK)
				mode = CHARGE_LED_BLINK;
			else if ((leds.charge.request.value & CHARGE_LED_OFFS) ||
				 (!charging && !chargeFactors.0))
				mode = CHARGE_LED_OFF;
			else if (leds.charge.request.value & CHARGE_LED_REQUEST_HOST)
				mode = CHARGE_LED_HOST;
			else
				mode = CHARGE_LED_AUTO;
			break;
		case CHARGE_LED_OFF:
			if (leds.charge.request.value & CHARGE_LED_REQUEST_BLINK)
				mode = CHARGE_LED_BLINK;
			else if (!(leds.charge.request.value & CHARGE_LED_OFFS) &&
				 (charging || chargeFactors.0))
				mode = CHARGE_LED_AUTO;
			else if (leds.charge.request.value & CHARGE_LED_REQUEST_HOST)
				mode = CHARGE_LED_HOST;
			else
				mode = CHARGE_LED_OFF;
			break;
		case CHARGE_LED_HOST:
			if (leds.charge.request.value & CHARGE_LED_REQUEST_BLINK)
				mode = CHARGE_LED_BLINK;
			else if (!(leds.charge.request.value & CHARGE_LED_REQUEST_HOST)) {
				if ((leds.charge.request.value & CHARGE_LED_OFFS) ||
				    (!charging && !chargeFactors.0))
					mode = CHARGE_LED_OFF;
				else
					mode = CHARGE_LED_AUTO;
			} else
				mode = CHARGE_LED_HOST;
			break;
		case CHARGE_LED_BLINK:
			if (!(leds.charge.request.value & CHARGE_LED_REQUEST_BLINK)) {
				if ((leds.charge.request.value & CHARGE_LED_OFFS) ||
				    (!charging && !chargeFactors.0))
					mode = CHARGE_LED_OFF;
				else if (leds.charge.request.value & CHARGE_LED_REQUEST_HOST)
					mode = CHARGE_LED_HOST;
				else
					mode = CHARGE_LED_AUTO;
			} else
				mode = CHARGE_LED_BLINK;
			break;
		}
	}
	if (mode != leds.charge.mode) {
		switch (mode) {
		case CHARGE_LED_AUTO:
			chargeLedBlinkStop();
			cookieCtrlReg3Mode(0);
			break;
		case CHARGE_LED_HOST:
			chargeLedBlinkStop();
			cookieCtrlReg3Bit0();
			break;
		case CHARGE_LED_OFF:
			chargeLedBlinkStop();
			cookieCtrlReg3Bit2(0);
			break;
		case CHARGE_LED_BLINK:
			paramRead(0x54, 1, &onTime);
			paramRead(0x55, 1, &offTime);
			chargeLedBlinkStart(onTime, offTime, 0);
			break;
		}
		leds.charge.mode = mode;
	}
	leds.charge.shown = leds.charge.request;
}
#endif

void powerLedUpdate(void)
{
	unsigned char mode;

	mode = leds.power.mode;
	if (leds.power.request.value & POWER_LED_REQUEST_ALARM)
		mode = POWER_LED_ON;
#if FW_SINCE(0x01000306)
	else if (leds.power.request.value & POWER_LED_REQUEST_ALL_OFF)
		mode = POWER_LED_OFF;
#endif
	else if (leds.power.request.value & POWER_LED_REQUEST_PATTERN1)
		mode = POWER_LED_PATTERN1;
	else if (leds.power.request.value & POWER_LED_REQUEST_PATTERN2)
		mode = POWER_LED_PATTERN2;
	else if (leds.power.request.value & POWER_LED_REQUEST_PATTERN3)
		mode = POWER_LED_PATTERN3;
	else if (leds.power.request.value & POWER_LED_REQUEST_OFF)
		mode = POWER_LED_OFF;
	else if (leds.power.request.value & POWER_LED_REQUEST_ON)
		mode = POWER_LED_ON;
	else
		mode = POWER_LED_ON;
	if (mode != leds.power.mode) {
		switch (mode) {
		case POWER_LED_ON:
			powerLedFadeStart(0);
			powerLedSetBrightness(15);
			powerLedSetPwm(0);
			break;
		case POWER_LED_OFF:
			powerLedFadeStart(0);
			powerLedSetBrightness(0);
			powerLedSetPwm(1);
			break;
		case POWER_LED_PATTERN1:
			powerLedFadeStart(1);
			break;
		case POWER_LED_PATTERN2:
			powerLedFadeStart(2);
			break;
		case POWER_LED_PATTERN3:
			powerLedFadeStart(3);
			break;
		}
		leds.power.mode = mode;
	}
	leds.power.shown = leds.power.request;
}

/* task 5's (flag 0x0800) */
void ledUpdate(void)
{
	chargeLedUpdate();
	powerLedUpdate();
}

static void powerLedFadeHoldExpire(void)
{
	task5SetFlag(4, 0x0200);
}

#if FW_SINCE(0x01000306)
static void powerLedFadeStepExpire(void)
{
	task5SetFlag(4, 0x0200);
}
#endif

void powerLedInit(void)
{
	struct TimerHandler handler;

	powerLedFade.active = 0;
	powerLedFade.phase = LED_FADE_START;
	powerLedFade.pattern = 0;
	powerLedFade.steps = &powerLedPattern;
	powerLedFade.status = 0;
	powerLedFade.keep = 0;
#if FW_SINCE(0x01000306)
	powerLedFade.step = 0;
#endif
	handler.handler = powerLedFadeHoldExpire;
	timerSetHandler(POWER_LED_HOLD_TIMER, &handler);
#if FW_SINCE(0x01000306)
	handler.handler = powerLedFadeStepExpire;
	timerSetHandler(POWER_LED_STEP_TIMER, &handler);
#endif
	leds.power.request.value = 0;
	leds.power.shown.value = 0;
	leds.power.mode = POWER_LED_NONE;
	powerLedUpdate();
}

#if FW_SINCE(0x01000306)
/* task 5's (flag 0x0200): the fade's next step, or a hold */
void powerLedFadeStep(void)
{
	if (powerLedFade.active != 0) {
		switch (powerLedFade.phase) {
		case LED_FADE_START:
			powerLedFade.step = 0;
			powerLedFade.phase = LED_FADE_OUT;
		case LED_FADE_OUT:
			while (powerLedFade.step < 16) {
				if (powerLedFade.steps->fadeOut[powerLedFade.step] != 0) {
					powerLedSetBrightness(15 - powerLedFade.step);
					timerStart(POWER_LED_STEP_TIMER, LED_STEP_TICKS(
						powerLedFade.steps->fadeOut[powerLedFade.step]));
					if (powerLedFade.step == 15)
						powerLedFade.phase = LED_FADE_OUT_DONE;
					powerLedFade.step++;
					return;
				}
				powerLedFade.step++;
			}
			powerLedFade.phase = LED_FADE_OUT_DONE;
		case LED_FADE_OUT_DONE:
			powerLedFade.phase = LED_FADE_IN_START;
			if ((leds.power.request.value & POWER_LED_REQUEST_ONCE) &&
			    (leds.power.request.value & POWER_LED_REQUEST_OFF)) {
				leds.power.request.value &= ~(POWER_LED_REQUEST_PATTERNS |
							      POWER_LED_REQUEST_ONCE);
				task5SetFlag(4, 0x0800);
				powerRequestLedDone();
				return;
			}
			timerStart(POWER_LED_HOLD_TIMER, powerLedFade.steps->holdOff);
			return;
		case LED_FADE_IN_START:
			powerLedFade.step = 15;
			powerLedFade.phase = LED_FADE_IN;
		case LED_FADE_IN:
			while (powerLedFade.step < 16) {
				if (powerLedFade.steps->fadeIn[powerLedFade.step] != 0) {
					powerLedSetBrightness(15 - powerLedFade.step);
					timerStart(POWER_LED_STEP_TIMER, LED_STEP_TICKS(
						powerLedFade.steps->fadeIn[powerLedFade.step]));
					if (powerLedFade.step == 0)
						powerLedFade.phase = LED_FADE_IN_DONE;
					powerLedFade.step--;
					return;
				}
				powerLedFade.step--;
			}
			powerLedFade.phase = LED_FADE_IN_DONE;
		case LED_FADE_IN_DONE:
			powerLedFade.phase = LED_FADE_START;
			if ((leds.power.request.value & POWER_LED_REQUEST_ONCE) &&
			    (leds.power.request.value & POWER_LED_REQUEST_ON)) {
				leds.power.request.value &= ~(POWER_LED_REQUEST_PATTERNS |
							      POWER_LED_REQUEST_ONCE);
				task5SetFlag(4, 0x0800);
				powerRequestLedDone();
				return;
			}
			timerStart(POWER_LED_HOLD_TIMER, powerLedFade.steps->holdOn);
			return;
		}
	}
}
#else
/* task 5's (flag 0x0200): a whole fade, out or in, then the hold */
void powerLedFadeStep(void)
{
	unsigned char step;

	if (powerLedFade.active != 0) {
		if (powerLedFade.phase == LED_FADE_IN_NEXT) {
			step = 16;
			do {
				step--;
				if (powerLedFade.steps->fadeIn[step] != 0) {
					powerLedSetBrightness(15 - step);
					dly_tsk(LED_STEP_TICKS(powerLedFade.steps->fadeIn[step]));
				}
			} while (step != 0);
			powerLedFade.phase = LED_FADE_START;
			if ((leds.power.request.value & POWER_LED_REQUEST_ONCE) &&
			    (leds.power.request.value & POWER_LED_REQUEST_ON)) {
				leds.power.request.value &= ~POWER_LED_REQUEST_ONCE;
				task5SetFlag(4, 0x0800);
				powerRequestLedDone();
				return;
			}
			timerStart(POWER_LED_HOLD_TIMER, powerLedFade.steps->holdOn);
		} else {
			for (step = 0; step < 16; step++) {
				if (powerLedFade.steps->fadeOut[step] != 0) {
					powerLedSetBrightness(15 - step);
					dly_tsk(LED_STEP_TICKS(powerLedFade.steps->fadeOut[step]));
				}
			}
			powerLedFade.phase = LED_FADE_IN_NEXT;
			if ((leds.power.request.value & POWER_LED_REQUEST_ONCE) &&
			    (leds.power.request.value & POWER_LED_REQUEST_OFF)) {
				leds.power.request.value &= ~POWER_LED_REQUEST_ONCE;
				task5SetFlag(4, 0x0800);
				powerRequestLedDone();
				return;
			}
			timerStart(POWER_LED_HOLD_TIMER, powerLedFade.steps->holdOff);
		}
	}
}
#endif

/* 0: no fade, 1 and 2: the parameter block's patterns, 3: ledPatternDefault */
void powerLedFadeStart(unsigned char pattern)
{
	powerLedFade.pattern = pattern;
	if (pattern != 0) {
		switch (pattern) {
		case 1:
			paramGetPattern1((unsigned char *)&powerLedPattern);
			powerLedFade.steps = &powerLedPattern;
			break;
		case 2:
#if FW_SINCE(0x01000306)
			paramGetPattern2((unsigned char *)&powerLedPattern);
			powerLedFade.steps = &powerLedPattern;
#else
			powerLedFade.steps = (struct LedPattern *)&ledPattern2Default;
#endif
			break;
		case 3:
			powerLedFade.steps = (struct LedPattern *)&ledPatternDefault;
			break;
		}
		powerLedFade.active = 1;
		powerLedFade.phase = LED_FADE_START;
		powerLedSetPwm(1);
		task5SetFlag(4, 0x0200);
	} else {
		timerStop(POWER_LED_HOLD_TIMER);
#if FW_SINCE(0x01000306)
		timerStop(POWER_LED_STEP_TIMER);
#endif
		powerLedSetPwm(0);
		powerLedSetBrightness(0);
		powerLedFade.active = 0;
		powerLedFade.phase = LED_FADE_START;
	}
}

/* 0-15, into the Elmo's register 0x60 bits 12-15 */
int powerLedSetBrightness(unsigned char level)
{
	unsigned char result;
	unsigned int value;

	if (level >= 16)
		level = 15;
	value = (elmoReg60 & 0x0fff) | ((unsigned int)level << 12);
	elmoCommand[0] = 0x60;
	elmoCommand[1] = ((unsigned char *)&value)[1];
	elmoCommand[2] = ((unsigned char *)&value)[0];
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoReg60 = value;
	return result;
}

/* the Elmo's register 0xa6 bit 15: the brightness used */
int powerLedSetPwm(unsigned char on)
{
	unsigned char result;
	unsigned int value;

	if (on == 1)
		value = elmoRegA6 | 0x8000;
	else
		value = elmoRegA6 & 0x7fff;
	elmoCommand[0] = 0xa6;
	elmoCommand[1] = ((unsigned char *)&value)[1];
	elmoCommand[2] = ((unsigned char *)&value)[0];
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoRegA6 = value;
	return result;
}

unsigned char powerLedGetStatus(void)
{
	return powerLedFade.status;
}

unsigned char powerLedGetState(void)
{
	return leds.power.shown.value;
}

unsigned char chargeLedGetState(void)
{
	return leds.charge.shown.value;
}
