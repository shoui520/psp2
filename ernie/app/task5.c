/*
 * Task 5: the events of event flag 4, each bit cleared and handled: among them the power and
 * battery requests (powerreq.c).
 */
#pragma rtos_task task5

#include "ernie.h"
#include "battery.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "kernel.h"
#include "led.h"
#include "powerjob.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "serial1.h"
#include "state.h"
#include "task.h"
#include "task5.h"
#if BOARD_DOL1001
#include "thermal.h"
#endif

unsigned int powerDevicePending;
#if !FW_SINCE(0x00090206)
unsigned char chargeLedBlinkOn;
#endif

extern unsigned char input1Bit7Active(void);

#if !BOARD_DOL1001
static void task5ChargeLed(void);
#if !LED_REQUESTS
static void chargeLedBlinkUpdate(void);
#endif
#if FW_SINCE(0x01000306)
static void task5LedOff(void);
#endif
#endif

void task5(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;

	powerDeviceInit();
#if BOARD_DOL1001
	/* DOL-1001's: the temperature (0x0020), the power LED's blink (0x2000), INTP9's wake
	   (0x1000) and P7.4's sleep factor (0x0200) */
	for (;;) {
		ercd = wai_flg(4, 0xf3a3, TWF_ORW, &ptn);
		if (ercd == E_OK) {
			if (ptn & 0x0002) {
				clr_flg(4, ~0x0002);
				powerJobState.flag = 0;
				powerJob();
			}
			if (ptn & 0x0080) {
				clr_flg(4, ~0x0080);
				powerDeviceRun();
			}
			if (ptn & 0x0001) {
				clr_flg(4, ~0x0001);
				powerRequestEvent();
			}
			if (ptn & 0x0100) {
				clr_flg(4, ~0x0100);
				batteryRequestEvent();
			}
#if BATTERY_GAUGE
			if (ptn & 0x0040) {
				clr_flg(4, ~0x0040);
				batteryUpdate();
			}
#endif
			if (ptn & 0x0020) {
				clr_flg(4, ~0x0020);
				thermalPoll();
			}
			if (ptn & 0x2000) {
				clr_flg(4, ~0x2000);
				powerLedFlagsStep();
			}
			if (ptn & 0x4000) {
				clr_flg(4, ~0x4000);
				serial1Operation();
			}
			if (ptn & 0x8000)
				clr_flg(4, ~0x8000);
			if (ptn & 0x1000) {
				clr_flg(4, ~0x1000);
				intp9Event();
			}
			if (ptn & 0x0200) {
				clr_flg(4, ~0x0200);
				powerSleepFactor21();
			}
		}
		task5Idle();
	}
#else
	for (;;) {
#if BATTERY_GAUGE
		ercd = wai_flg(4, 0xffeb, TWF_ORW, &ptn);	/* the gauge's update (0x0040) */
#elif FW_SINCE(0x01000306)
		ercd = wai_flg(4, 0xffab, TWF_ORW, &ptn);
#elif LED_REQUESTS
		ercd = wai_flg(4, 0xffa3, TWF_ORW, &ptn);
#elif FW_SINCE(0x00090903)
		ercd = wai_flg(4, 0xf7a3, TWF_ORW, &ptn);
#elif FW_SINCE(0x00090804)
		ercd = wai_flg(4, 0xf3a3, TWF_ORW, &ptn);	/* no Cookie cycle */
#else
		ercd = wai_flg(4, 0xf1a3, TWF_ORW, &ptn);	/* no power LED fade */
#endif
		if (ercd == E_OK) {
			if (ptn & 0x0002) {
				clr_flg(4, ~0x0002);
				powerJobState.flag = 0;
				powerJob();
			}
			if (ptn & 0x0080) {
				clr_flg(4, ~0x0080);
				powerDeviceRun();
			}
			if (ptn & 0x0001) {
				clr_flg(4, ~0x0001);
				powerRequestEvent();
			}
			if (ptn & 0x0100) {
				clr_flg(4, ~0x0100);
				batteryRequestEvent();
			}
#if BATTERY_GAUGE
			if (ptn & 0x0040) {
				clr_flg(4, ~0x0040);
				batteryUpdate();
			}
#endif
			if (ptn & 0x0020) {
				clr_flg(4, ~0x0020);
				chargeUpdate();
#if FW_SINCE(0x01000107)
				ledUpdate();
#endif
			}
#if SOURCE_0103
			if (ptn & 0x0008) {
				clr_flg(4, ~0x0008);
				task5LedOff();
			}
			if (ptn & 0x0400) {
				clr_flg(4, ~0x0400);
				chargeUsbChanged();
			}
#else
#if FW_SINCE(0x00090903)
			if (ptn & 0x0400) {
				clr_flg(4, ~0x0400);
#if FW_SINCE(0x01000104)
				powerCookieRelease();
#else
				powerCookieCycle();
#endif
			}
#endif
#if FW_SINCE(0x01000306)
			if (ptn & 0x0008) {
				clr_flg(4, ~0x0008);
				task5LedOff();
			}
#endif
#endif
			if (ptn & 0x1000) {
				clr_flg(4, ~0x1000);
				task5ChargeLed();
			}
#if SOURCE_0103
			if (ptn & 0x0800) {
				clr_flg(4, ~0x0800);
				powerLedFlagsUpdate();
				ledUpdate();
			}
			if (ptn & 0x2000) {
				clr_flg(4, ~0x2000);
				powerLedFlagsStep();
			}
#else
#if LED_REQUESTS
			if (ptn & 0x0800) {
				clr_flg(4, ~0x0800);
				ledUpdate();
			}
#endif
			if (ptn & 0x2000) {
				clr_flg(4, ~0x2000);
				chargeLedBlinkUpdate();
			}
#endif
#if FW_SINCE(0x00090804)
			if (ptn & 0x0200) {
				clr_flg(4, ~0x0200);
				powerLedFadeStep();
			}
#endif
			if (ptn & 0x4000) {
				clr_flg(4, ~0x4000);
				serial1Operation();
			}
			if (ptn & 0x8000)
				clr_flg(4, ~0x8000);
		}
		task5Idle();
	}
#endif
}

/* the posts end in an explicit return, which keeps the call a call; USS-1002's fall off it, so
   it becomes a branch */
#define POST_RETURN		(!(BOARD_USS1001 && SOURCE_0104))

void powerJobPostOn(void)
{
	powerJobState.id = POWER_JOB_ON;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}

void powerJobPostResume(void)
{
	powerJobState.id = POWER_JOB_RESUME;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}

void powerJobPostOff(void)
{
	powerJobState.id = POWER_JOB_OFF;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}

void powerJobPostSuspend(void)
{
	powerJobState.id = POWER_JOB_SUSPEND;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}

void powerJobPostShutdown(void)
{
	powerJobState.id = POWER_JOB_SHUTDOWN;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}

void powerJobPostCut(void)
{
	powerJobState.id = POWER_JOB_CUT;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}

#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
void powerJobPostOffEnd(void)
{
	powerJobState.id = POWER_JOB_OFF_END;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}
#endif

/* before 0x01000306 the reset job is not posted: powerReset puts the charge LED off itself
   (before 0x01000107 there is no charging request to clear) */
#if FW_SINCE(0x01000107)
void powerJobPostReset(void)
{
#if FW_SINCE(0x01000306)
	powerJobState.id = POWER_JOB_RESET;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
#else
	chargeLedSetCharging(0);
#endif
}
#endif

#if FW_SINCE(0x01000104)
void powerJobPostUpdate(void)
{
	powerJobState.id = POWER_JOB_UPDATE;
	task5SetFlag(4, 0x0002);
#if POST_RETURN
	return;
#endif
}
#endif

#if !(BOARD_USS1001 && SOURCE_0104)	/* USS-1002 has no caller */
unsigned char elmoVersion0(void)
{
	return deviceVersions.elmo[0];
}
#endif

void powerDeviceSetPending(unsigned int bits)
{
#if SOURCE_0103
	dis_dsp();
	powerDevicePending |= bits;
	ena_dsp();
#else
	powerDevicePending |= bits;
#endif
	task5SetFlag(4, 0x0080);
	return;
}

#if BOARD_DOL1001
/* DOL-1001's task 5 handles no charge LED */
#elif SOURCE_0103
/* the slim's: the power LED module's causes (the charge LED off, its full charge) */
static void task5ChargeLed(void)
{
	if (input1Bit7Active()) {
		if (configGet()->state.bits.chargeLedOff) {
			powerLedSetFlags(POWER_LED_FLAG_OFF);
			return;
		}
		powerLedClearFlags(POWER_LED_FLAG_OFF);
		if (chargeBatteryLevel9() && stateGet() != STATE_RESUME)
			;
	} else {
		powerLedClearFlags(POWER_LED_FLAG_FULL);
	}
	return;
}
#elif LED_REQUESTS
static void task5ChargeLed(void)
{
	if (input1Bit7Active()) {
		if (configGet()->state.bits.chargeLedOff) {
			chargeLedSetOff(1);
			return;
		}
		chargeLedSetOff(0);
#if FW_SINCE(0x0100060B)
		if (chargeBatteryLevel9() && stateGet() != STATE_RESUME)
#else
		if (chargeBatteryLevel9())
#endif
			chargeLedSetBlink(1);
	} else {
		chargeLedSetBlink(0);
		chargeLedSetReleased(0);
	}
	return;
}
#else
/* before 0x01000104 the charge LED is the Cookie's register 3 here: off (bits 2 and 0), blinking
   (chargeLedSetBlink) or the Cookie's; 0x00090009 leaves it to the Cookie */
static void task5ChargeLed(void)
{
#if FW_SINCE(0x00090206)
	if (input1Bit7Active()) {
#if FW_SINCE(0x00090804)
		if (configGet()->start.bits.chargeLedOff) {
			cookieCtrlReg3Bit2(0);
			cookieCtrlReg3Mode(1);
			return;
		}
#endif
		if (chargeBatteryLevel9())
			chargeLedSetBlink(1);
		else
			cookieCtrlReg3Mode(0);
	} else {
		if (configGet()->start.bits.chargeLedOffReleased)
			cookieCtrlReg3Mode(1);
		else
			chargeLedSetBlink(0);
	}
#endif
}

/* task 5's (flag 0x2000): the blink's state to the Cookie */
static void chargeLedBlinkUpdate(void)
{
#if FW_SINCE(0x00090206)
	cookieCtrlReg3Bit2(chargeLedBlinkOn);
#endif
}
#endif

#if BOARD_DOL1001
#elif SOURCE_0103
/* the slim's: nothing */
static void task5LedOff(void)
{
}
#elif FW_SINCE(0x01000306)
static void task5LedOff(void)
{
	ledSetOff(1);
}
#endif
