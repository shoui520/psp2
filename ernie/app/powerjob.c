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


extern void sub_11D73(void);
extern void sub_11D7B(void);

unsigned char powerResumed;		/* set by a resume, cleared by a power on */
static unsigned char padding;		/* 0xff2ed: nothing uses it */
struct PowerJobState powerJobState;
unsigned int powerCutReg0;		/* registers 0xb0-0xb2 of elmoRead's device, read at a */
unsigned int powerCutReg1;		/* power cut and cleared at a power on */
unsigned int powerCutReg2;

static void powerJobOn(void);
static void powerJobResume(void);
static void powerJobOff(void);
static void powerJobShutdown(void);
static void powerJobSuspend(void);
static void powerJobCut(void);
static void powerJobOffEnd(void);
static void powerJobUpdate(void);
static void powerJobReset(void);

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
	case POWER_JOB_UPDATE:
		powerJobUpdate();
		return;
	case POWER_JOB_RESET:
		powerJobReset();
		return;
	}
}

static void powerJobOn(void)
{
	if (!P12.0) {
		P6.2 = 0;
		stateTransition(0x41);
	}
	EGP0.0 = 0;
	EGN0.0 = 1;
	PIF0 = 0;
	PMK0 = 0;
	powerSeqOn();
	powerCutReg0 = powerCutReg1 = powerCutReg2 = 0;
	powerTimersInit();
	powerSeqInit();
	powerLedInit();
	chargeLedInit();
	timerStart(0x17, 1);
	powerResumed = 0;
	powerReady(POWER_JOB_PART);
}

static void powerJobResume(void)
{
	union PowerRequest *mode;

	batteryBLLineOff();
	powerResumed = 1;
	if (elmoRegA6 & 0x8000) {
		powerLedSetPwm(0);
		powerSleep(4);
		powerLedSetPwm(1);
	}
	powerSeqResume();
	P0.6 = 1;
	P0.5 = 1;
	sub_11D73();
	sub_11D7B();
	powerRequestInit();
	chargeSetAcDelay();
	task5SetFlag(4, 0x20);
	mode = stateGetWakeupFactor();
	if (mode->bits.mode == 0x0f && !mode->bits.notify)
		chargeLedSetCharging(1);
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
	chargeSetAcDelay();
	task5SetFlag(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerLedSetMode(POWER_LED_SET_ON);
	ledSetOff(0);
	if (configGet()->state.bits.chargeLedOff)
		chargeLedSetOff(1);
	else
		chargeLedSetOff(0);
	if (configGet()->state.bits.chargeLedStateOff)
		chargeLedSetStateOff(1);
	else
		chargeLedSetStateOff(0);
	chargeLedSetHost(0);
	chargeLedSetHostOff(0);
	chargeLedSetCharging(0);
	powerReady(POWER_JOB_PART);
}

static void powerJobShutdown(void)
{
	powerSeqOff();
	iicaClose();
	chargeTimerStop();
	powerLedSetMode(POWER_LED_SET_ON);
	ledSetOff(0);
	chargeLedSetHost(0);
	chargeLedSetHostOff(0);
	chargeLedSetCharging(0);
	powerReady(POWER_JOB_PART);
}

static void powerJobSuspend(void)
{
	powerSeqSuspend();
	if (chargeBatteryLevel9()) {
		powerWirelessOff();
		if (!bbmcModeIsNone() || configGet()->suspendCheck)
			powerBbmcCycle();
	}
	POWER_JOB_MODE();
	if (!powerLedGetKeep())
		powerLedSetMode(POWER_LED_SET_ON);
	chargeSetAcDelay();
	task5SetFlag(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerCookieHold();
	chargeLedSetCharging(0);
	ledSetOff(0);
	powerReady(POWER_JOB_PART);
}

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
	chargeLedInit();
	iicaClose();
	chargeTimerStop();
	powerReady(POWER_JOB_PART);
}

static void powerJobOffEnd(void)
{
	batteryHibernate();
	chargeTimerStop();
}

static void powerJobUpdate(void)
{
	chargeLedInit();
	if (powerLedGetState() & POWER_LED_REQUEST_PATTERNS)
		powerLedSetMode(POWER_LED_SET_OFF);
	ledUpdate();
	powerReady(POWER_JOB_PART);
}

static void powerJobReset(void)
{
	chargeLedSetCharging(0);
	powerReady(POWER_JOB_PART);
}
