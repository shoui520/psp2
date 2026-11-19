/*
 * The LEDs (app/led.c): the charge LED, driven by the Cookie's register 3, on, off, left to
 * the Cookie or blinking on timer 0x18; and the power LED, driven by the Elmo, on, off or
 * fading in and out by a pattern of steps (registers 0x60 and 0xa6, timers 0x14 and 0x1c).
 * Each LED takes requests as bits, and task 5 (flag 0x0800) works out the mode they ask for.
 */
#ifndef ERNIE_LED_H
#define ERNIE_LED_H

#include "ernie.h"
#include "board.h"

/* the LEDs' requests and modes, the charge LED's blink: since 0x01000104; before, the power
   LED's fade only */
#define LED_REQUESTS		FW_SINCE(0x01000104)

#define CHARGE_LED_TIMER	TIMER_RTC(0x18)	/* the charge LED's blink */
#if FW_SINCE(0x01000306)
#define POWER_LED_STEP_TIMER	TIMER(0x14)	/* a fade's step */
#endif
#define POWER_LED_HOLD_TIMER	TIMER_RTC(0x1c)	/* the hold between two fades */

/* the charge LED's modes */
#define CHARGE_LED_AUTO		0	/* the Cookie lights it while charging */
#define CHARGE_LED_OFF		1
#define CHARGE_LED_HOST		2	/* the Cookie's register 3 bit 0: the host's setting */
#if FW_SINCE(0x01000107)
#define CHARGE_LED_BLINK	3
#else
/* before 0x01000107 two counted blinks, which put the power LED's alarm on (their requests are
   only ever cleared), and the battery alarm's */
#define CHARGE_LED_BLINK1	3	/* the config's chargeLedBlink1* */
#define CHARGE_LED_BLINK2	4	/* paramSetting1 and 2, chargeLedBlink2Count */
#define CHARGE_LED_BLINK	5
#define CHARGE_LED_NONE		0xff	/* not set yet */
#endif

/* the charge LED's requests that put it off (before 0x01000306 there is no ledSetOff) */
#if FW_SINCE(0x01000306)
#define CHARGE_LED_OFFS		(CHARGE_LED_REQUEST_ALL_OFF | CHARGE_LED_REQUEST_HOST_OFF | \
				 CHARGE_LED_REQUEST_RELEASED)
#else
#define CHARGE_LED_OFFS		(CHARGE_LED_REQUEST_HOST_OFF | CHARGE_LED_REQUEST_RELEASED)
#endif

/* the power LED's modes */
#define POWER_LED_ON		0
#define POWER_LED_OFF		1
#define POWER_LED_PATTERN1	2	/* the parameter block's patterns */
#define POWER_LED_PATTERN2	3
#define POWER_LED_PATTERN3	4	/* ledPatternDefault */
#define POWER_LED_NONE		0xff	/* not set yet */

/* powerLedSetMode's */
#define POWER_LED_SET_NONE	0x00
#define POWER_LED_SET_OFF	0x01
#define POWER_LED_SET_ON	0x02
#define POWER_LED_SET_PATTERN1	0x04
#define POWER_LED_SET_PATTERN2	0x08
#define POWER_LED_SET_PATTERN3	0x10

/* a fade: the steps' times from full brightness down and from off up, each in timer ticks
   rounded up to 4 (0: no step), and the holds after each fade */
struct LedPattern {
	unsigned char	fadeOut[16];
	unsigned char	fadeIn[16];
	unsigned char	holdOff;
	unsigned char	holdOn;
};

/* the charge LED's blink */
struct LedBlink {
	unsigned char	on;			/* lit */
	unsigned char	onTime;			/* in ticks of timer 0x18 */
	unsigned char	offTime;
	unsigned char	count;			/* the blinks left, plus one; 0: no end */
};

/* the power LED's fade */
struct LedFade {
	unsigned char	active;
	unsigned char	phase;			/* LED_FADE_* */
	unsigned char	pattern;		/* powerLedFadeStart's */
	unsigned char	status;			/* read by powerLedGetStatus only */
	struct LedPattern *steps;
#if LED_REQUESTS
	unsigned char	keep;			/* the host's: the mode kept at a resume */
#endif
#if FW_SINCE(0x01000306)
	unsigned char	step;			/* the step in the fade */
#endif
};

/* a fade's phases */
#define LED_FADE_START		0
#define LED_FADE_OUT		1
#define LED_FADE_OUT_DONE	2	/* then the hold off */
#define LED_FADE_IN_START	3
#define LED_FADE_IN		4
#define LED_FADE_IN_DONE	5	/* then the hold on */
/* before 0x01000306 task 5 runs a fade whole, waiting between its steps: out at
   LED_FADE_START, then in at LED_FADE_IN_NEXT */
#define LED_FADE_IN_NEXT	1

/* the charge LED's requests */
union ChargeLedRequest {
	unsigned char	value;
	struct {
		unsigned char	off:1;		/* config's chargeLedOff at a reset */
#if FW_SINCE(0x01000107)
		unsigned char	charging:1;	/* lit as charging while the Cookie powers the
						   battery */
		unsigned char	allOff:1;	/* ledSetOff's */
#else
		unsigned char	blink1:1;	/* CHARGE_LED_BLINK1 */
		unsigned char	blink2:1;	/* CHARGE_LED_BLINK2 */
#endif
		unsigned char	blink:1;	/* a battery alarm */
		unsigned char	hostOff:1;	/* the host's (0x089e mode 0) */
		unsigned char	host:1;		/* the host's (0x089e mode 2) */
		unsigned char	released:1;	/* set when the Cookie is released, cleared at a
						   low battery */
		unsigned char	stateOff:1;	/* off in the system states with bit 5 (config's
						   chargeLedStateOff) */
	} bits;
};

/* the power LED's requests */
union PowerLedRequest {
	unsigned char	value;
	struct {
		unsigned char	off:1;
		unsigned char	on:1;
		unsigned char	pattern1:1;
		unsigned char	pattern2:1;
		unsigned char	pattern3:1;
		unsigned char	allOff:1;	/* ledSetOff's */
		unsigned char	once:1;		/* one fade, then off or on and 0x089d answered */
		unsigned char	alarm:1;	/* on: a battery alarm */
	} bits;
};

/* the requests' bits, as masks */
#define CHARGE_LED_REQUEST_OFF		0x01
#if FW_SINCE(0x01000107)
#define CHARGE_LED_REQUEST_CHARGING	0x02
#define CHARGE_LED_REQUEST_ALL_OFF	0x04
#else
#define CHARGE_LED_REQUEST_BLINK1	0x02
#define CHARGE_LED_REQUEST_BLINK2	0x04
#endif
#define CHARGE_LED_REQUEST_BLINK	0x08
#define CHARGE_LED_REQUEST_HOST_OFF	0x10
#define CHARGE_LED_REQUEST_HOST		0x20
#define CHARGE_LED_REQUEST_RELEASED	0x40
#define CHARGE_LED_REQUEST_STATE_OFF	0x80

#define POWER_LED_REQUEST_OFF		0x01
#define POWER_LED_REQUEST_ON		0x02
#define POWER_LED_REQUEST_PATTERN1	0x04
#define POWER_LED_REQUEST_PATTERN2	0x08
#define POWER_LED_REQUEST_PATTERN3	0x10
#define POWER_LED_REQUEST_ALL_OFF	0x20
#define POWER_LED_REQUEST_PATTERNS	(POWER_LED_REQUEST_PATTERN1 | POWER_LED_REQUEST_PATTERN2 | \
					 POWER_LED_REQUEST_PATTERN3)
#define POWER_LED_REQUEST_ONCE		0x40
#define POWER_LED_REQUEST_ALARM		0x80

struct ChargeLed {
	union ChargeLedRequest shown;		/* the requests the mode was worked out from */
	union ChargeLedRequest request;
	unsigned char	mode;			/* CHARGE_LED_* */
};

struct PowerLed {
	union PowerLedRequest shown;
	union PowerLedRequest request;
	unsigned char	mode;			/* POWER_LED_* */
};

/* the two LEDs' requests and modes */
struct Leds {
	struct ChargeLed charge;
	struct PowerLed	power;
};

#if LED_REQUESTS
extern void chargeLedBlinkUpdate(void);
extern void chargeLedInit(void);
extern void chargeLedSetOff(unsigned char on);
#if !FW_SINCE(0x01000107)
extern void chargeLedSetBlink1(unsigned char on);
extern void chargeLedSetBlink2(unsigned char on);
#endif
#if FW_SINCE(0x01000306)
extern void chargeLedSetStateOff(unsigned char on);
#endif
extern void chargeLedSetBlink(unsigned char on);
extern void chargeLedSetHost(unsigned char on);
extern void chargeLedSetHostOff(unsigned char on);
extern void chargeLedSetReleased(unsigned char on);
#if FW_SINCE(0x01000107)
extern void chargeLedSetCharging(unsigned char on);
#endif
extern void powerLedSetAlarm(unsigned char on);
#if SOURCE_0103
/* the slim's power LED module: the causes it shows, set and cleared */
/* the causes (powerLedSetFlags' masks) */
#define POWER_LED_FLAG_ON	0x0001
#define POWER_LED_FLAG_LOW	0x0002
#define POWER_LED_FLAG_HOST	0x0004
#define POWER_LED_FLAG_HOST_BLINK 0x0008
#define POWER_LED_FLAG_CHARGING	0x0010
#define POWER_LED_FLAG_FULL	0x0020
#define POWER_LED_FLAG_RESUMED	0x0040
#define POWER_LED_FLAG_ALARM	0x0080
#define POWER_LED_FLAG_OFF	0x0100
#define POWER_LED_FLAG_STATE_OFF 0x0200
union PowerLedCauses {
	unsigned int	value;
	struct {
		unsigned int	on:1;		/* the power LED on */
		unsigned int	low:1;		/* the battery low (chargeBatteryLevel3): it blinks */
		unsigned int	host:1;		/* host request 0x089f's pattern (powerLedSetPattern) */
		unsigned int	hostBlink:1;	/* host command 0x0821/0x0822's blink */
		unsigned int	charging:1;	/* the Cookie's status 1: charging */
		unsigned int	full:1;		/* cleared below the battery's low voltage */
		unsigned int	resumed:1;	/* a power-on by wake factor 0x0f without notify */
		unsigned int	alarm:1;	/* the charge LED blinks five times */
		unsigned int	off:1;		/* the config's chargeLedOff */
		unsigned int	stateOff:1;	/* the config's chargeLedStateOff, with the power
						   LED on */
		unsigned int	reserved:6;
	} bits;
};
/* the module's state */
#if BOARD_DOL1001
/* DOL-1001's power LED: one mode at a time (POWER_LED_MODE_*), the host pattern's mode
   returning to the one it replaced */
#define POWER_LED_MODE_OFF	1
#define POWER_LED_MODE_ON	2
#define POWER_LED_MODE_BLINK	3	/* powerLedSetBlink's count of blinks */
#define POWER_LED_MODE_HOST_BLINK 4	/* host commands 0x0821 and 0x0822: no end */
#define POWER_LED_MODE_HOST	5	/* the host's pattern (powerLedSetPattern) */
#if BOARD_DOL1002
/* DOL-1002's events (powerLedStartMode), each taking the mode shown to the next one by the
   system state */
#define POWER_LED_EVENT_RESUME		0	/* powerSeqResume */
#define POWER_LED_EVENT_OFF		1	/* powerSeqOff's start */
#define POWER_LED_EVENT_OFF_DONE	2	/* and its end */
#define POWER_LED_EVENT_SUSPEND		3	/* powerSeqSuspend's start */
#define POWER_LED_EVENT_SUSPEND_DONE	4	/* and its end */
#define POWER_LED_EVENT_HOST_BLINK	5	/* host commands 0x0821 and 0x0822 */
#define POWER_LED_EVENT_HOST_BLINK_END	6
#define POWER_LED_EVENT_HOST		7	/* the host's pattern */
#define POWER_LED_EVENT_JOB_END		8	/* a power job's update or reset done */
#define POWER_LED_EVENT_RESUME_BLINK	9	/* powerSeqResume woken by the power button */
#define POWER_LED_EVENT_BLINK_END	10	/* powerLedFlagsStep: the blinks done */
#endif

struct PowerLedFlags {
	unsigned char	mode;		/* POWER_LED_MODE_* */
	unsigned char	next;		/* the mode the host's pattern returns to */
	unsigned char	phase;		/* 1 while the blink is on */
	unsigned char	count;		/* the blinks left, 0xff no end */
	unsigned int	onTime;		/* the blink's times, in ms */
	unsigned int	offTime;
	unsigned int	blinkOn;	/* powerLedSetBlink's and powerLedLoadBlink's */
	unsigned int	blinkOff;
	unsigned char	blinkCount;
	unsigned int	hostOn;		/* powerLedSetPattern's */
	unsigned int	hostOff;
	unsigned char	hostPattern;
};
extern struct PowerLedFlags powerLedFlags;
extern void powerLedFlagsInit(unsigned char resumed);
extern void powerLedFlagsStep(void);
#if BOARD_DOL1002
extern void powerLedStartMode(unsigned char event);
#else
extern void powerLedStartMode(unsigned char mode);
extern void powerLedEndMode(unsigned char mode);
#endif
/* a mode started or ended (else), the event DOL-1002 passes for it */
#if BOARD_DOL1002
#define POWER_LED_START(mode, event)	powerLedStartMode(event)
#define POWER_LED_END(mode, event)	powerLedStartMode(event)
#else
#define POWER_LED_START(mode, event)	powerLedStartMode(mode)
#define POWER_LED_END(mode, event)	powerLedEndMode(mode)
#endif
extern void powerLedSetBlink(unsigned char count, unsigned int on, unsigned int off);
extern void powerLedLoadBlink(unsigned char index);
extern void powerLedSetPattern(unsigned char pattern, unsigned int on, unsigned int off);
#else
struct PowerLedFlags {
	unsigned char	mode;		/* the mode shown (powerLedFlagsUpdate's) */
	union PowerLedCauses causes;	/* the causes the LEDs show */
	unsigned char	phase;		/* 1 while the blink is on */
	unsigned char	count;		/* the blinks left, 0xff no end */
	unsigned int	onTime;		/* the blink's times, in ms */
	unsigned int	offTime;
	unsigned int	blinkOn;	/* powerLedLoadBlink's, for the parameter block's blink */
	unsigned int	blinkOff;
	unsigned char	pattern;	/* the host pattern shown */
	unsigned char	hostPattern;	/* powerLedSetPattern's */
	unsigned int	hostOn;
	unsigned int	hostOff;
};
extern struct PowerLedFlags powerLedFlags;
extern void powerLedFlagsInit(unsigned char resumed);
extern void powerLedFlagsUpdate(void);
extern void powerLedFlagsStep(void);
extern void powerLedSetFlags(unsigned int flags);
extern void powerLedClearFlags(unsigned int flags);
extern void powerLedSetFlagsNow(unsigned int flags);
extern void powerLedClearFlagsNow(unsigned int flags);
extern void powerLedLoadBlink(unsigned char index);
extern void powerLedSetPattern(unsigned char pattern, unsigned int on, unsigned int off);
#endif
#endif
extern void powerLedFadeOff(void);
extern void powerLedFadeOn(void);
extern void powerLedSetKeep(unsigned char keep);
extern unsigned char powerLedGetKeep(void);
extern void powerLedSetMode(unsigned char mode);
#if FW_SINCE(0x01000306)
extern void ledSetOff(unsigned char on);
#endif
extern void chargeLedUpdate(void);
extern void powerLedUpdate(void);
extern void ledUpdate(void);
#else
extern void chargeLedInit(void);		/* powerjob.c's before 0x01000104 */
extern void chargeLedSetBlink(unsigned char on);	/* powerseq.c's */
#endif
extern void powerLedInit(void);
extern void powerLedFadeStep(void);
extern void powerLedFadeStart(unsigned char pattern);
extern int powerLedSetBrightness(unsigned char level);
extern int powerLedSetPwm(unsigned char on);
extern unsigned char powerLedGetStatus(void);
#if LED_REQUESTS
extern unsigned char powerLedGetState(void);
extern unsigned char chargeLedGetState(void);
#endif

#endif
