/*
 * Task 5's part of the power sequences: a sequence posts its job (powerJobPostOn, ...,
 * task5.c), task 5 runs it here, and the job reports part 5 done (powerReady).
 */
#pragma sfr

#include "ernie.h"
#include "battery.h"
#include "bbmc.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "elmo.h"
#include "iica.h"
#include "kernel.h"
#include "led.h"
#include "power.h"
#include "powerjob.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "state.h"
#include "task.h"
#include "timer.h"

#define POWER_JOB_PART	5	/* the power part the jobs report */


extern void inputFlag40Later(void);
extern void inputFlag80Later(void);
extern unsigned char input2Bit0Get(void);

#if FW_SINCE(0x01000104)
unsigned char powerResumed;		/* set by a resume, cleared by a power on */
static unsigned char padding;		/* 0xff2ed: nothing uses it */
#endif
struct PowerJobState powerJobState;
unsigned int powerCutReg0;		/* registers 0xb0-0xb2 of elmoRead's device, read at a */
unsigned int powerCutReg1;		/* power cut and cleared at a power on */
unsigned int powerCutReg2;

/* the charge update's period: chargeSetAcDelay's since 0x01000306, 10 before */
#if FW_SINCE(0x01000306)
#define POWER_JOB_AC_DELAY()	chargeSetAcDelay()
#else
#define POWER_JOB_AC_DELAY()	(charge.acDelay = 10)
#endif

static void powerJobOn(void);
static void powerJobResume(void);
static void powerJobOff(void);
static void powerJobShutdown(void);
static void powerJobSuspend(void);
static void powerJobCut(void);
static void powerJobOffEnd(void);
#if FW_SINCE(0x01000104)
static void powerJobUpdate(void);
#else
static void chargeLedBlinkExpire(void);
#endif
#if FW_SINCE(0x01000306)
static void powerJobReset(void);
#endif

/* task 5, event 0x0002 of flag 4: runs the job posted */
void powerJob(void)
{
	switch (powerJobState.id) {
	case POWER_JOB_ON:
		powerJobOn();
		return;
	case POWER_JOB_RESUME:
		powerJobResume();
		return;
	case POWER_JOB_OFF:
		powerJobOff();
		return;
	case POWER_JOB_SUSPEND:
		powerJobSuspend();
		return;
	case POWER_JOB_SHUTDOWN:
		powerJobShutdown();
		return;
	case POWER_JOB_CUT:
		powerJobCut();
		return;
	case POWER_JOB_OFF_END:
		powerJobOffEnd();
		return;
#if FW_SINCE(0x01000104)
	case POWER_JOB_UPDATE:
		powerJobUpdate();
		return;
#endif
#if FW_SINCE(0x01000306)
	case POWER_JOB_RESET:
		powerJobReset();
		return;
#endif
	}
}

static void powerJobOn(void)
{
	STATE_TRANSITION_EVENT
	if (!P12.0) {
		P6.2 = 0;
		STATE_TRANSITION(0x41);
	}
	EGP0.0 = 0;
	EGN0.0 = 1;
	PIF0 = 0;
	PMK0 = 0;
	powerSeqOn();
	powerCutReg0 = powerCutReg1 = powerCutReg2 = 0;
	powerTimersInit();
	powerSeqInit();
#if FW_SINCE(0x01000104)
	powerLedInit();
	chargeLedInit();
	timerStart(TIMER_RTC(0x17), 1);
	powerResumed = 0;
#else
	if (configGet()->start.bits.chargeLedOff) {
		cookieCtrlReg3Bit2(0);
		cookieCtrlReg3Mode(1);
	}
	chargeLedInit();
	powerLedInit();
	timerStart(TIMER_RTC(0x17), 1);
#endif
	powerReady(POWER_JOB_PART);
}

static void powerJobResume(void)
{
#if FW_SINCE(0x01000107)
	union PowerRequest *mode;
#endif

	batteryBLLineOff();
#if FW_SINCE(0x01000104)
	powerResumed = 1;
	if (elmoRegA6 & 0x8000) {
		powerLedSetPwm(0);
		powerSleep(4);
		powerLedSetPwm(1);
	}
#else
	powerLedFadeStart(0);
	powerSleep(4);
#endif
	powerSeqResume();
#if BOARD_TOUCH_RESET
	P0.6 = 1;
	P0.5 = 1;
	inputFlag40Later();
	inputFlag80Later();
#endif
	powerRequestInit();
	POWER_JOB_AC_DELAY();
	task5SetFlag(4, 0x20);
#if FW_SINCE(0x01000107)
	mode = stateGetWakeupFactor();
	if (mode->bits.mode == 0x0f && !mode->bits.notify)
		chargeLedSetCharging(1);
#endif
	powerReady(POWER_JOB_PART);
}

/* the mode chargeUsbMode asks for, applied again */
#define POWER_JOB_MODE()						\
	switch (chargeUsbMode) {						\
	case 1:								\
	case 2:								\
	case 3:								\
		if (chargeBatteryLevel9())					\
			chargeUsbMode = 1;				\
		else							\
			chargeUsbMode = 4;				\
		chargeSetCurrent();						\
		break;							\
	case 0:								\
	case 4:								\
	case 5:								\
		break;							\
	}

#if FW_SINCE(0x01000104)
static void powerJobOff(void)
{
	powerBbmcOff();
	powerSeqOff();
	powerBbmcOn();
	cookieCtrlBatteryCharge(1);
	if (powerResumed) {
		powerCookieHold();
		POWER_JOB_MODE();
	}
	POWER_JOB_AC_DELAY();
	task5SetFlag(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerLedSetMode(POWER_LED_SET_ON);
#if FW_SINCE(0x01000306)
	ledSetOff(0);
#endif
	if (configGet()->state.bits.chargeLedOff)
		chargeLedSetOff(1);
	else
		chargeLedSetOff(0);
#if FW_SINCE(0x01000306)
	if (configGet()->state.bits.chargeLedStateOff)
		chargeLedSetStateOff(1);
	else
		chargeLedSetStateOff(0);
#endif
	chargeLedSetHost(0);
	chargeLedSetHostOff(0);
#if FW_SINCE(0x01000107)
	chargeLedSetCharging(0);
#endif
#if BOARD_CHARGE_TIMER1F
	chargeTimer1fStop();
#endif
	powerReady(POWER_JOB_PART);
}
#else
static void powerJobOff(void)
{
	if ((signed char)bbmc.state >= 2 || input2Bit0Get() == 1)
		powerBbmcRestart();
	powerSeqOff();
	cookieCtrlBatteryCharge(1);
	charge.acDelay = 0x3c;
	task5SetFlag(4, 0x20);
	task5SetFlag(4, 0x400);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerLedFadeStart(0);
	powerReady(POWER_JOB_PART);
}
#endif

#if FW_SINCE(0x01000104)
static void powerJobShutdown(void)
{
	powerSeqOff();
	iicaClose();
	chargeTimerStop();
	powerLedSetMode(POWER_LED_SET_ON);
#if FW_SINCE(0x01000306)
	ledSetOff(0);
#endif
	chargeLedSetHost(0);
	chargeLedSetHostOff(0);
#if FW_SINCE(0x01000107)
	chargeLedSetCharging(0);
#endif
	powerReady(POWER_JOB_PART);
}
#else
static void powerJobShutdown(void)
{
	powerSeqOff();
	iicaClose();
	chargeTimerStop();
	powerLedFadeStart(0);
	powerReady(POWER_JOB_PART);
}
#endif

#if FW_SINCE(0x01000104)
static void powerJobSuspend(void)
{
	powerSeqSuspend();
	if (chargeBatteryLevel9()) {
		powerOffWlan();
		if (!bbmcModeIsNone() || configGet()->suspendCheck)
			powerBbmcCycle();
	}
	POWER_JOB_MODE();
	if (!powerLedGetKeep())
		powerLedSetMode(POWER_LED_SET_ON);
	POWER_JOB_AC_DELAY();
	task5SetFlag(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerCookieHold();
#if FW_SINCE(0x01000107)
	chargeLedSetCharging(0);
#endif
#if FW_SINCE(0x01000306)
	ledSetOff(0);
#endif
	powerReady(POWER_JOB_PART);
}
#else
static void powerJobSuspend(void)
{
	powerSeqSuspend();
	charge.acDelay = 0x3c;
	task5SetFlag(4, 0x20);
	task5SetFlag(4, 0x400);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerReady(POWER_JOB_PART);
}
#endif

static void powerJobCut(void)
{
	unsigned char reg;
	unsigned char result;

	P0.6 = 0;
	P0.5 = 0;
	P6.3 = 0;
	P13.1 = 0;
	reg = 0xb0;
	result = elmoRead(&reg, (unsigned char *)&powerCutReg0);
	reg = 0xb1;
	result = elmoRead(&reg, (unsigned char *)&powerCutReg1);
	reg = 0xb2;
	result = elmoRead(&reg, (unsigned char *)&powerCutReg2);
#if FW_SINCE(0x01000104)
	chargeLedInit();
#endif
	iicaClose();
	chargeTimerStop();
	powerReady(POWER_JOB_PART);
}

static void powerJobOffEnd(void)
{
	batteryHibernate();
	chargeTimerStop();
}

#if FW_SINCE(0x01000104)
static void powerJobUpdate(void)
{
	chargeLedInit();
	if (powerLedGetState() & POWER_LED_REQUEST_PATTERNS)
		powerLedSetMode(POWER_LED_SET_OFF);
	ledUpdate();
	powerReady(POWER_JOB_PART);
}
#else
/* before 0x01000104 the charge LED's blink is here: timer 0x18's, its on and off times
   paramSetting1 and 2 */
static void chargeLedBlinkExpire(void)
{
	if (chargeLedBlinkOn == 1) {
		chargeLedBlinkOn = 0;
		timerStart(CHARGE_LED_TIMER, configGet()->paramSetting2 - 1);
	} else {
		chargeLedBlinkOn = 1;
		timerStart(CHARGE_LED_TIMER, configGet()->paramSetting1 - 1);
	}
	task5SetFlag(4, 0x2000);
}

void chargeLedInit(void)
{
	struct TimerHandler handler;

	chargeLedBlinking = 0;
	chargeLedBlinkOn = 0;
	handler.handler = chargeLedBlinkExpire;
	timerSetHandler(CHARGE_LED_TIMER, &handler);
}
#endif

#if FW_SINCE(0x01000306)
static void powerJobReset(void)
{
#if FW_SINCE(0x01000107)
	chargeLedSetCharging(0);
#endif
	powerReady(POWER_JOB_PART);
}
#endif
