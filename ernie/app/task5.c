/*
 * Task 5: the events of event flag 4, each bit cleared and handled: among them the power and
 * battery requests (powerreq.c).
 */
#pragma rtos_task task5

#include "ernie.h"
#include "charge.h"
#include "config.h"
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
static void task5LedOff(void);

void task5(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;

	powerDeviceInit();
	for (;;) {
		ercd = wai_flg(4, 0xffab, TWF_ORW, &ptn);
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
				ledUpdate();
			}
			if (ptn & 0x0400) {
				clr_flg(4, ~0x0400);
				powerCookieRelease();
			}
			if (ptn & 0x0008) {
				clr_flg(4, ~0x0008);
				task5LedOff();
			}
			if (ptn & 0x1000) {
				clr_flg(4, ~0x1000);
				task5ChargeLed();
			}
			if (ptn & 0x0800) {
				clr_flg(4, ~0x0800);
				ledUpdate();
			}
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

void powerJobPostReset(void)
{
	powerJobState.id = POWER_JOB_RESET;
	task5SetFlag(4, 0x0002);
	return;
}

void powerJobPostUpdate(void)
{
	powerJobState.id = POWER_JOB_UPDATE;
	task5SetFlag(4, 0x0002);
	return;
}

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

static void task5LedOff(void)
{
	ledSetOff(1);
}
