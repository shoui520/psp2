/*
 * Task 5's part of the power sequences: a sequence posts its job (powerJobPostOn, ...,
 * task5.c), task 5 runs it here, and the job reports part 5 done (powerReady).
 */
#pragma sfr

#include "ernie.h"
#include "battery.h"
#include "bbmc.h"
#include "board.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "cookie.h"
#include "elmo.h"
#include "eventlog.h"
#include "iica.h"
#include "kernel.h"
#include "led.h"
#include "log.h"
#include "power.h"
#include "powerjob.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "sleepfactor.h"
#include "state.h"
#include "task.h"
#if BOARD_DOL1001
#include "thermal.h"
#endif
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
unsigned int powerCutRegs[POWER_CUT_REGS];	/* registers 0xb0-0xb2 (the slim's: 0xb3) of
						   elmoRead's device, read at a power cut and
						   cleared at a power on */

/* the charge update's period: chargeSetAcDelay's since 0x01000306, 10 before */
#if FW_SINCE(0x01000306)
#define POWER_JOB_AC_DELAY()	chargeSetAcDelay()
#else
#define POWER_JOB_AC_DELAY()	(charge.acDelay = 10)
#endif
/* after an off or a suspend: 60 in PCH-1000's and 0x00411000's 0x010001xx */
#if (FW_RETAIL_10X && BOARD_IS(0x00406000)) || \
    (BOARD_IS(0x00411000) && FW_SINCE(0x01000104) && !FW_SINCE(0x01000306))
#define POWER_JOB_AC_DELAY_DOWN()	(charge.acDelay = 0x3c)
#else
#define POWER_JOB_AC_DELAY_DOWN()	POWER_JOB_AC_DELAY()
#endif

/* before 0x01000104, the off and suspend jobs: the Cookie's hold released (task 5's event
   0x400) and the VBUS charge's first bit set; 0x00090804 has no hold and one VBUS flag */
#if FW_SINCE(0x00090903)
#define POWER_JOB_VBUS_ON()	(task5SetFlag(4, 0x400), chargeVbus.0 = 1)
#else
#define POWER_JOB_VBUS_ON()	(chargeVbus = 1)
#endif

#if FW_SINCE(0x0100060B)
extern unsigned char input1Bit7Active(void);
#endif

static void powerJobOn(void);
static void powerJobResume(void);
static void powerJobOff(void);
static void powerJobShutdown(void);
static void powerJobSuspend(void);
static void powerJobCut(void);
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
static void powerJobOffEnd(void);
#endif
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
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
	case POWER_JOB_OFF_END:
		powerJobOffEnd();
		return;
#endif
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

#if BOARD_DOL1001
/*
 * DOL-1001's jobs: the fat board's lines, INTP9's edge (intp9Enable) and the temperature
 * sensor's mode (thermalSetMode) in place of the slim's LED and charge settings.
 */
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
	EGP1.0 = 1;
	EGN1.0 = 1;
	PIF8 = 0;
	PMK8 = 0;
	task5SetFlag(4, 0x200);
	intp9Enable(0);
	powerSeqOn();
#if BOARD_DOL1002
	chargeInit();
#endif
	if (stateGetCause() != 0x42)
		powerCutRegs[0] = powerCutRegs[1] = powerCutRegs[2] = powerCutRegs[3] = 0;
	powerTimersInit();
	powerSeqInit();
	powerLedFlagsInit(0);
	timerStart(TIMER_RTC(0x17), 10);
	powerReady(POWER_JOB_PART);
}

static void powerJobResume(void)
{
	intp9Enable(0);
	powerSeqResume();
	powerRequestInit();
	thermalSetMode(THERMAL_MODE_STANDBY);
	powerReady(POWER_JOB_PART);
}

static void powerJobOff(void)
{
	intp9Enable(0);
	powerSeqOff();
	chargeSetAcDelay();
	thermalSetMode(THERMAL_MODE_ON);
	powerReady(POWER_JOB_PART);
}

static void powerJobShutdown(void)
{
	powerSeqOff();
	chargeUsbModeUpdate();
	chargeTimerStop();
	powerReady(POWER_JOB_PART);
}

static void powerJobSuspend(void)
{
	powerSeqSuspend();
	thermalSetMode(THERMAL_MODE_ON);
	intp9Enable(1);
	powerReady(POWER_JOB_PART);
}

/* the lines low, registers 0xb0-0xb2 read */
static void powerJobCut(void)
{
	unsigned char reg;
	unsigned char result;
#if BOARD_DOL1002
	union {
		long		value;
		unsigned int	word[2];
	} event;		/* the registers logged */
#endif

	P13.1 = 0;
	P37 = 0;
	P36 = 0;
	P8.0 = 0;
	reg = 0xb0;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[0]);
	reg = 0xb1;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[1]);
	reg = 0xb2;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[2]);
#if BOARD_DOL1002
	event.word[0] = powerCutRegs[0];
	event.word[1] = powerCutRegs[1];
	logEventValue(0x1e, event.value);
	event.word[0] = powerCutRegs[2];
	logEventValue(0x1f, event.value);
	reg = 0xb3;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[3]);
	event.word[0] = powerCutRegs[3];
	logEventValue(0x20, event.value);
#endif
	powerLedFlagsInit(0);
	thermalSetMode(THERMAL_MODE_OFF);
	chargeTimerStop();
	powerReady(POWER_JOB_PART);
	return;
}

static void powerJobOffEnd(void)
{
	batteryHibernate();
	chargeTimerStop();
}

static void powerJobUpdate(void)
{
	POWER_LED_END(POWER_JOB_PART, POWER_LED_EVENT_JOB_END);
	powerReady(POWER_JOB_PART);
}

static void powerJobReset(void)
{
	POWER_LED_END(POWER_JOB_PART, POWER_LED_EVENT_JOB_END);
	powerReady(POWER_JOB_PART);
}
#elif SOURCE_0103
/*
 * The slim's jobs: the power LED module's flags (powerLedSetFlags) in place of the charge
 * LED's settings, and chargeUsbModeUpdate in place of the mode applied again.
 */
static void powerJobOn(void)
{
	STATE_TRANSITION_EVENT
	if (!BOARD_INTP0_PIN) {
		BOARD_POWER_CUT_PIN = 0;
		STATE_TRANSITION(0x41);
	}
	EGP0.4 = 0;
	EGN0.4 = 1;
	PIF4 = 0;
	PMK4 = 0;
	EGP1.1 = 1;
	EGN1.1 = 1;
	PIF9 = 0;
	PMK9 = 0;
	task5SetFlag(4, 0x400);
	powerSeqOn();
#if BATTERY_GAUGE
	chargeInit();	/* USS-1002: here, not in powerSeqOn */
#endif
	if (stateGetCause() != 0x42)
		powerCutRegs[0] = powerCutRegs[1] = powerCutRegs[2] = powerCutRegs[3] = 0;
	powerTimersInit();
	powerSeqInit();
	powerLedInit();
	powerLedFlagsInit(0);
	timerStart(TIMER_RTC(0x17), 1);
	powerResumed = 0;
	powerReady(POWER_JOB_PART);
}

static void powerJobResume(void)
{
	union PowerRequest *mode;

	batteryBLLineOff();
	powerResumed = 1;
	mode = stateGetWakeupFactor();
	if (mode->bits.mode == 0x0f && !mode->bits.notify)
		powerLedSetFlags(0x40);
#if BATTERY_GAUGE
	powerRegAA15On();
#endif
	powerSeqResume();
	powerRequestInit();
	chargeSetAcDelay();
	task5SetFlag(4, 0x20);
	powerReady(POWER_JOB_PART);
}

static void powerJobOff(void)
{
	powerLedClearFlags(0x04);
	powerBbmcOff();
	powerSeqOff();
	powerBbmcOn();
	cookieUpdateBatteryCharge(1);
	if (powerResumed) {
#if BATTERY_GAUGE
		/* USS-1002: the gauge off for the battery's own off causes, else register 0xaa's bit */
		switch (stateGetCause()) {
		case 0x5d:
		case 0x68:
			batteryPowerOff();
			break;
		default:
			powerRegAA15Off();
			break;
		}
#endif
		powerCookieHold();
		chargeUsbModeUpdate();
	}
	chargeSetAcDelay();
	task5SetFlag(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerLedSetMode(POWER_LED_SET_ON);
	/* the charge LED's blink of the fat's sleep factor 19 is gone; its test stays */
	if ((sleepFactorGet() & 0x00080000) && input1Bit7Active()) {
	}
	if (configGet()->state.bits.chargeLedOff)
		powerLedSetFlags(0x100);
	else
		powerLedClearFlags(0x100);
	if (configGet()->state.bits.chargeLedStateOff)
		powerLedSetFlags(0x200);
	else
		powerLedClearFlags(0x200);
	powerLedClearFlags(0x40);
	chargeTimer1fStop();
#if BATTERY_GAUGE
	batterySaveRecord();
#endif
	powerReady(POWER_JOB_PART);
}

static void powerJobShutdown(void)
{
	powerLedClearFlags(0x04);
	powerSeqOff();
	chargeUsbModeUpdate();
	chargeTimerStop();
	powerLedSetMode(POWER_LED_SET_ON);
	powerLedClearFlags(0x40);
#if BATTERY_GAUGE
	batterySaveRecord();
#endif
	powerReady(POWER_JOB_PART);
}

static void powerJobSuspend(void)
{
	powerLedClearFlags(0x04);
	powerSeqSuspend();
	if (chargeBatteryLevel9()) {
		powerOffWlan();
		if (!bbmcModeIsNone() || configGet()->suspendCheck)
			powerBbmcCycle();
	}
#if BATTERY_GAUGE
	powerRegAA15Off();
#endif
	chargeUsbModeUpdate();
	if (!powerLedGetKeep())
		powerLedSetMode(POWER_LED_SET_ON);
	if (sleepFactorGet() & 0x00080000) {
		if (input1Bit7Active()) {
		}
		powerLedSetAlarm(1);
	}
	chargeSetAcDelay();
	task5SetFlag(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerCookieHold();
	powerLedClearFlags(0x40);
	powerReady(POWER_JOB_PART);
}

/* the lines low, registers 0xb0-0xb3 read and logged (event log kind 5, codes 0x20 and 0x21) */
static void powerJobCut(void)
{
	unsigned char reg;
	unsigned char result;
#if BATTERY_GAUGE
	union {
		long		value;
		unsigned int	word[2];
	} event;		/* the registers logged, as DOL-1002 does */
#endif

	P4.1 = 0;
	P4.2 = 0;
#if !BATTERY_GAUGE	/* USS-1002 has no P6.5 line */
	P6.5 = 0;
#endif
	P0.5 = 0;
	reg = 0xb0;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[0]);
	reg = 0xb1;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[1]);
	reg = 0xb2;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[2]);
#if BATTERY_GAUGE
	event.word[0] = powerCutRegs[0];
	event.word[1] = powerCutRegs[1];
	logEventValue(0x1e, event.value);
	event.word[0] = powerCutRegs[2];
	logEventValue(0x1f, event.value);
#endif
	reg = 0xb3;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[3]);
#if BATTERY_GAUGE
	event.word[0] = powerCutRegs[3];
	logEventValue(0x20, event.value);
#endif
#if BOARD_EVENT_LOG
	eventLogRecord(5, 0x20, (unsigned char *)&powerCutRegs[0], 4);
	eventLogRecord(5, 0x21, (unsigned char *)&powerCutRegs[2], 4);
#endif
	powerLedFlagsInit(0);
#if !BATTERY_GAUGE
	powerLedClearFlags(0x04);
#endif
	chargeUsbModeUpdate();
	chargeTimerStop();
	powerReady(POWER_JOB_PART);
}

static void powerJobOffEnd(void)
{
	powerLedFlagsInit(0);
#if BATTERY_GAUGE
	batterySaveRecord();
#endif
	batteryHibernate();
	chargeTimerStop();
}

static void powerJobUpdate(void)
{
	powerLedClearFlagsNow(0x04);
	if (powerLedGetState() & POWER_LED_REQUEST_PATTERNS)
		powerLedSetMode(POWER_LED_SET_OFF);
	ledUpdate();
#if BATTERY_GAUGE
	batterySaveRecord();
#endif
	powerReady(POWER_JOB_PART);
}

static void powerJobReset(void)
{
	powerLedClearFlags(0x40);
	powerLedClearFlags(0x04);
	powerReady(POWER_JOB_PART);
}
#else
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
#if FW_SINCE(0x0100060B)
	/* the power cut's registers kept through a start from cause 0x42 */
	if (stateGetCause() != 0x42)
#endif
		powerCutRegs[0] = powerCutRegs[1] = powerCutRegs[2] = 0;
	powerTimersInit();
#if FW_SINCE(0x00090804)
	powerSeqInit();
#endif
#if FW_SINCE(0x01000104)
	powerLedInit();
	chargeLedInit();
	timerStart(TIMER_RTC(0x17), 1);
	powerResumed = 0;
#elif !FW_SINCE(0x00090804)
	chargeLedInit();
	timerStart(TIMER_RTC(0x17), 1);
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

#if FW_SINCE(0x00090009)	/* 0x00080504 has no bootloader line */
	batteryBLLineOff();
#endif
#if FW_SINCE(0x01000104)
	powerResumed = 1;
#if !BOARD_IRS1001
	/* not on IRS-1001: the power LED's PWM restarted */
	if (elmoRegA6 & 0x8000) {
		powerLedSetPwm(0);
		powerSleep(4);
		powerLedSetPwm(1);
	}
#endif
#elif FW_SINCE(0x00090804)
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
	POWER_JOB_AC_DELAY_DOWN();
	task5SetFlag(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerLedSetMode(POWER_LED_SET_ON);
#if FW_SINCE(0x01000306)
	ledSetOff(0);
#endif
#if FW_SINCE(0x0100060B)
	/* sleep factor 19 (the charge's, sleepFactorSet19) with input 1's bit 7: the charge LED
	   blinks */
	if ((sleepFactorGet() & 0x00080000) && input1Bit7Active())
		chargeLedSetBlink(1);
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
#if FW_SINCE(0x00090804)
	cookieCtrlBatteryCharge(1);
#endif
	charge.acDelay = 0x3c;
	task5SetFlag(4, 0x20);
	POWER_JOB_VBUS_ON();
#if FW_SINCE(0x00090804)
	chargeControl.1 = 1;
	powerLedFadeStart(0);
#endif
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
#if FW_SINCE(0x00090804)
	chargeTimerStop();
	powerLedFadeStart(0);
#else
	timerStop(TIMER_RTC(0x17));
#endif
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
#if BOARD_POWER_LED_ELMO
		powerLedSet(0);
#else
		powerLedSetMode(POWER_LED_SET_ON);
#endif
#if FW_SINCE(0x0100060B)
	if (sleepFactorGet() & 0x00080000) {
		if (input1Bit7Active())
			chargeLedSetBlink(1);
		powerLedSetAlarm(1);
	}
#endif
	POWER_JOB_AC_DELAY_DOWN();
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
	POWER_JOB_VBUS_ON();
#if FW_SINCE(0x00090804)
	chargeControl.1 = 1;
#endif
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
#if BOARD_102603 && !HW_IS(0x00315200)
	P6.7 = 0;
#endif
	reg = 0xb0;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[0]);
	reg = 0xb1;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[1]);
	reg = 0xb2;
	result = elmoRead(&reg, (unsigned char *)&powerCutRegs[2]);
#if FW_SINCE(0x01000104)
	chargeLedInit();
#endif
	iicaClose();
#if FW_SINCE(0x00090804)
	chargeTimerStop();
#else
	timerStop(TIMER_RTC(0x17));
#endif
	powerReady(POWER_JOB_PART);
}

#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
static void powerJobOffEnd(void)
{
	batteryHibernate();
#if FW_SINCE(0x00090804)
	chargeTimerStop();
#else
	timerStop(TIMER_RTC(0x17));
	clr_flg(4, ~0x0020);
#endif
}
#endif

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
#if FW_SINCE(0x00090206)	/* 0x00090009 only posts the update */
	if (chargeLedBlinkOn == 1) {
		chargeLedBlinkOn = 0;
		timerStart(CHARGE_LED_TIMER, configGet()->paramSetting2 - 1);
	} else {
		chargeLedBlinkOn = 1;
		timerStart(CHARGE_LED_TIMER, configGet()->paramSetting1 - 1);
	}
#endif
	task5SetFlag(4, 0x2000);
}

void chargeLedInit(void)
{
	struct TimerHandler handler;

#if FW_SINCE(0x00090206)
	chargeLedBlinking = 0;
#endif
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
#endif /* SOURCE_0103 */
