/*
 * The LEDs (app/led.c): the charge LED, driven by the Cookie's register 3, on, off, left to
 * the Cookie or blinking on timer 0x18; and the power LED, driven by the Elmo, on, off or
 * fading in and out by a pattern of steps (registers 0x60 and 0xa6, timers 0x14 and 0x1c).
 * Each LED takes requests as bits, and task 5 (flag 0x0800) works out the mode they ask for.
 */
#ifndef ERNIE_LED_H
#define ERNIE_LED_H

#define CHARGE_LED_TIMER	0x18	/* the charge LED's blink */
#define POWER_LED_STEP_TIMER	0x14	/* a fade's step */
#define POWER_LED_HOLD_TIMER	0x1c	/* the hold between two fades */

/* the charge LED's modes */
#define CHARGE_LED_AUTO		0	/* the Cookie lights it while charging */
#define CHARGE_LED_OFF		1
#define CHARGE_LED_HOST		2	/* the Cookie's register 3 bit 0: the host's setting */
#define CHARGE_LED_BLINK	3

/* the charge LED's requests that put it off */
#define CHARGE_LED_OFFS		(CHARGE_LED_REQUEST_ALL_OFF | CHARGE_LED_REQUEST_HOST_OFF | \
				 CHARGE_LED_REQUEST_RELEASED)

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
	unsigned char	keep;			/* the host's: the mode kept at a resume */
	unsigned char	step;			/* the step in the fade */
};

/* a fade's phases */
#define LED_FADE_START		0
#define LED_FADE_OUT		1
#define LED_FADE_OUT_DONE	2	/* then the hold off */
#define LED_FADE_IN_START	3
#define LED_FADE_IN		4
#define LED_FADE_IN_DONE	5	/* then the hold on */

/* the charge LED's requests */
union ChargeLedRequest {
	unsigned char	value;
	struct {
		unsigned char	off:1;		/* config's chargeLedOff at a reset */
		unsigned char	charging:1;	/* lit as charging while the Cookie powers the
						   battery */
		unsigned char	allOff:1;	/* ledSetOff's */
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
#define CHARGE_LED_REQUEST_CHARGING	0x02
#define CHARGE_LED_REQUEST_ALL_OFF	0x04
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

extern void chargeLedBlinkUpdate(void);
extern void chargeLedInit(void);
extern void chargeLedSetOff(unsigned char on);
extern void chargeLedSetStateOff(unsigned char on);
extern void chargeLedSetBlink(unsigned char on);
extern void chargeLedSetHost(unsigned char on);
extern void chargeLedSetHostOff(unsigned char on);
extern void chargeLedSetReleased(unsigned char on);
extern void chargeLedSetCharging(unsigned char on);
extern void powerLedSetAlarm(unsigned char on);
extern void powerLedFadeOff(void);
extern void powerLedFadeOn(void);
extern void powerLedSetKeep(unsigned char keep);
extern unsigned char powerLedGetKeep(void);
extern void powerLedSetMode(unsigned char mode);
extern void ledSetOff(unsigned char on);
extern void chargeLedUpdate(void);
extern void powerLedUpdate(void);
extern void ledUpdate(void);
extern void powerLedInit(void);
extern void powerLedFadeStep(void);
extern void powerLedFadeStart(unsigned char pattern);
extern int powerLedSetBrightness(unsigned char level);
extern int powerLedSetPwm(unsigned char on);
extern unsigned char powerLedGetStatus(void);
extern unsigned char powerLedGetState(void);
extern unsigned char chargeLedGetState(void);

#endif
