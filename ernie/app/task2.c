/*
 * Task 2: the events of event flag 3, each group of bits cleared and handled.
 */
#pragma rtos_task task2

#include "ernie.h"
#include "board.h"
#include "command.h"
#include "configreq.h"
#include "kernel.h"
#include "log.h"
#include "nvs.h"
#include "nvsreq.h"
#include "power.h"
#include "rtc.h"
#include "serial.h"
#include "serial1.h"
#include "serial4.h"
#include "serialauth.h"
#include "sleepfactor.h"
#include "state.h"
#include "task.h"
#include "updatereq.h"
#include "version.h"
#include "watchdog.h"

extern unsigned char sub_10D7A(void);
extern void sub_2E8C6(void);
extern unsigned char sub_10CB3(void);
extern unsigned char sub_11D1B(void);
extern unsigned char sub_11D4B(void);

void sub_10B67(FLGPTN ptn);
unsigned char sub_10C03(void);
unsigned char sub_10D08(void);
unsigned char sub_10DC1(void);
unsigned char sub_10DEA(void);
unsigned char sub_10E06(void);
unsigned char sub_10E22(void);
unsigned char sub_10E32(void);

void task2(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;

	ptn = 0;
	for (;;) {
		ercd = wai_flg(3, 0xffff, TWF_ORW, &ptn);
		if (ercd == E_OK) {
			if (ptn & 0x1000) {
				clr_flg(3, ~0x1000);
				sub_10C03();
			}
			if (ptn & 0x0101) {
				clr_flg(3, ~(ptn & 0x0101));
				sub_10B67(ptn);
			}
			if (ptn & 0x2000) {
				clr_flg(3, ~(ptn & 0x2000));
				commandRequestRun();
			}
			if (ptn & 0x0e1e)
				serial4Event(ptn);
			if (ptn & 0x00a0) {
				clr_flg(3, ~(ptn & 0x00a0));
				nvsRequestEvent(ptn);
			}
			if (ptn & 0x8000) {
				clr_flg(3, ~0x8000);
				sub_10E32();
			}
			if (ptn & 0x4000) {
				clr_flg(3, ~0x4000);
				configRequestEvent(ptn);
			}
			if (ptn & 0x0040) {
				clr_flg(3, ~(ptn & 0x0040));
				updaterRequestEvent(ptn);
			}
		}
		task2Idle();
	}
}

void sub_10B67(FLGPTN ptn)
{
	unsigned char state;

	state = serialGetMode();
	if (ptn & 0x0001) {
		if (sub_11D1B()) {
			if (boardMultiCnIsDongle()) {
				boardSetMultiCnOutput(0);
				switch (state) {
				case 0:
				case 3:
					serialClose3();
					serial1Reset();
					serialOpen1(2);
					break;
				case 1:
					break;
				}
			} else if (boardMultiCnIsDebug()) {
				serialClose1();
				serialOpen3(3);
				boardSetMultiCnOutput(0);
			} else {
				serialClose3();
				serialClose1();
				boardMultiCnStop();
			}
		} else {
			serialClose3();
			serialClose1();
			boardMultiCnStop();
		}
	}
	if (ptn & 0x0100) {
		if (sub_11D4B())
			serial4Open2(2);
		else
			serial4Close2();
	}
	return;
}

unsigned char sub_10C03(void)
{
	unsigned char ret;
	unsigned int state;

	ret = 0;
	state = stateGet();
	if (state & 0x8000) {
		switch (state) {
		case 0x8020:
			ret = sub_10D7A();
			break;
		case 0x8008:
			ret = sub_10DC1();
			break;
		case 0x8004:
			ret = sub_10D08();
			break;
		case 0x8002:
			ret = sub_10CB3();
			break;
		case 0x8010:
			ret = sub_10DEA();
			break;
		case 0x8800:
			ret = sub_10E06();
			break;
		case 0x8040:
			ret = sub_10E22();
			break;
		}
		powerReady(2);
	} else {
		switch (state) {
		case 0x0002:
		case 0x0004:
		case 0x0008:
		case 0x0010:
		case 0x0020:
		case 0x0040:
		case 0x0800:
			break;
		}
	}
	return ret;
}

unsigned char sub_10D08(void)
{
	unsigned char ret;
	unsigned char value;

	ret = 0;
	commandRequestInit();
	nvsRequestInit();
	serialAuthInit();
	sub_2E8C6();
	serial4StartTimers();
	if (boardMultiCnIsDongle() || boardMultiCnIsDebug())
		boardSetMultiCnOutput(0);
	else
		boardMultiCnStop();
	updaterRequestInit();
	configRequestInit();
	watchdogStop();
	if (nvsRead(0x2d, 1, &value, 1) == 0)
		setDlFlags(value);
	if (getAlarmMode() == 1)
		setAlarm(RTC_ALARM_NONE);
	return ret;
}

unsigned char sub_10DC1(void)
{
	unsigned char ret;

	ret = 0;
	serialAuthInit();
	if (boardMultiCnIsDongle() || boardMultiCnIsDebug())
		boardSetMultiCnOutput(0);
	else
		boardMultiCnStop();
	watchdogStop();
	return ret;
}

unsigned char sub_10DEA(void)
{
	unsigned char ret;

	ret = 0;
	serial4Close2();
	boardMultiCnStop();
	serialClose1();
	serial1Reset();
	watchdogStop();
	return ret;
}

unsigned char sub_10E06(void)
{
	unsigned char ret;

	ret = 0;
	serial4Close2();
	boardMultiCnStop();
	serialClose1();
	serial1Reset();
	watchdogStop();
	return ret;
}

unsigned char sub_10E22(void)
{
	unsigned char ret;

	ret = 0;
	serialAuthInit();
	watchdogStop();
	return ret;
}

/* every second (the RTC's interrupt sets flag 0x8000): the alarm checked against the clock */
unsigned char sub_10E32(void)
{
	unsigned char ret;
	unsigned char clock;
	unsigned int state;

	ret = 0;
	state = stateGet();
	clock = getClock().byte[0];
	if (isAlarmReached()) {
		sleepFactorSet3(1);
		if ((stateGetWakeMask() & 2) == 0) {
			if ((state == 0x0004 && getAlarmMode() != 1) || state == 0x0008)
				ret = stateTransition(1);
		}
	} else {
		sleepFactorSet3(0);
	}
	if (state != 0x0010 && (clock & 3) == 3)
		nvsJobService();
	logUpdate();
	if (state != 0x0004 && state != 0x0008)
		taskHookStandby();
	taskHookTick();
	return ret;
}
