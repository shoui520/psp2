/*
 * Task 5: the events of event flag 4, each bit cleared and handled: among them the power and
 * battery requests (powerreq.c).
 */
#pragma rtos_task task5

#include "ernie.h"
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
#include "task.h"
#include "task5.h"


unsigned int powerDevicePending;

extern unsigned char input1Bit7Active(void);

static void task5ChargeLed(void);
#if !LED_REQUESTS
static void chargeLedBlinkUpdate(void);
#endif
#if FW_SINCE(0x01000306)
static void task5LedOff(void);
#endif

void task5(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;

	powerDeviceInit();
	for (;;) {
#if FW_SINCE(0x01000306)
		ercd = wai_flg(4, 0xffab, TWF_ORW, &ptn);
#elif LED_REQUESTS
		ercd = wai_flg(4, 0xffa3, TWF_ORW, &ptn);
#else
		ercd = wai_flg(4, 0xf7a3, TWF_ORW, &ptn);
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
			if (ptn & 0x0020) {
				clr_flg(4, ~0x0020);
				chargeUpdate();
#if FW_SINCE(0x01000107)
				ledUpdate();
#endif
			}
			if (ptn & 0x0400) {
				clr_flg(4, ~0x0400);
#if FW_SINCE(0x01000104)
				powerCookieRelease();
#else
				powerCookieCycle();
#endif
			}
#if FW_SINCE(0x01000306)
			if (ptn & 0x0008) {
				clr_flg(4, ~0x0008);
				task5LedOff();
			}
#endif
			if (ptn & 0x1000) {
				clr_flg(4, ~0x1000);
				task5ChargeLed();
			}
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
			if (ptn & 0x0200) {
				clr_flg(4, ~0x0200);
				powerLedFadeStep();
			}
			if (ptn & 0x4000) {
				clr_flg(4, ~0x4000);
				serial1Operation();
			}
			if (ptn & 0x8000)
				clr_flg(4, ~0x8000);
		}
		task5Idle();
	}
}

void powerJobPostOn(void)
{
	powerJobState.id = POWER_JOB_ON;
	task5SetFlag(4, 0x0002);
	return;
}

void powerJobPostResume(void)
{
	powerJobState.id = POWER_JOB_RESUME;
	task5SetFlag(4, 0x0002);
	return;
}

void powerJobPostOff(void)
{
	powerJobState.id = POWER_JOB_OFF;
	task5SetFlag(4, 0x0002);
	return;
}

void powerJobPostSuspend(void)
{
	powerJobState.id = POWER_JOB_SUSPEND;
	task5SetFlag(4, 0x0002);
	return;
}

void powerJobPostShutdown(void)
{
	powerJobState.id = POWER_JOB_SHUTDOWN;
	task5SetFlag(4, 0x0002);
	return;
}

void powerJobPostCut(void)
{
	powerJobState.id = POWER_JOB_CUT;
	task5SetFlag(4, 0x0002);
	return;
}

void powerJobPostOffEnd(void)
{
	powerJobState.id = POWER_JOB_OFF_END;
	task5SetFlag(4, 0x0002);
	return;
}

/* before 0x01000306 the reset job is not posted: powerReset puts the charge LED off itself
   (before 0x01000107 there is no charging request to clear) */
#if FW_SINCE(0x01000107)
void powerJobPostReset(void)
{
#if FW_SINCE(0x01000306)
	powerJobState.id = POWER_JOB_RESET;
	task5SetFlag(4, 0x0002);
	return;
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
	return;
}
#endif

unsigned char elmoVersion0(void)
{
	return deviceVersions.elmo[0];
}

void powerDeviceSetPending(unsigned int bits)
{
	powerDevicePending |= bits;
	task5SetFlag(4, 0x0080);
	return;
}

#if LED_REQUESTS
static void task5ChargeLed(void)
{
	if (input1Bit7Active()) {
		if (configGet()->state.bits.chargeLedOff) {
			chargeLedSetOff(1);
			return;
		}
		chargeLedSetOff(0);
		if (chargeBatteryLevel9())
			chargeLedSetBlink(1);
	} else {
		chargeLedSetBlink(0);
		chargeLedSetReleased(0);
	}
	return;
}
#else
/* before 0x01000104 the charge LED is the Cookie's register 3 here: off (bits 2 and 0), blinking
   (chargeLedSetBlink) or the Cookie's */
static void task5ChargeLed(void)
{
	if (input1Bit7Active()) {
		if (configGet()->start.bits.chargeLedOff) {
			cookieCtrlReg3Bit2(0);
			cookieCtrlReg3Mode(1);
			return;
		}
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
}

/* task 5's (flag 0x2000): the blink's state to the Cookie */
static void chargeLedBlinkUpdate(void)
{
	cookieCtrlReg3Bit2(chargeLedBlinkOn);
}
#endif

#if FW_SINCE(0x01000306)
static void task5LedOff(void)
{
	ledSetOff(1);
}
#endif
