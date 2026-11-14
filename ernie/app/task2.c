/*
 * Task 2: the events of event flag 3, each group of bits cleared and handled.
 */
#pragma rtos_task task2

#pragma sfr

#include "ernie.h"
#include "board.h"
#include "command.h"
#include "config.h"
#include "configreq.h"
#include "handshake.h"
#include "kernel.h"
#include "log.h"
#include "nvs.h"
#include "nvsreq.h"
#include "power.h"
#include "rtc.h"
#include "serial.h"
#include "serial1.h"
#include "serial2.h"
#include "serial4.h"
#include "serialauth.h"
#include "serialreq.h"
#include "sleepfactor.h"
#include "snvs.h"
#include "state.h"
#include "task.h"
#include "updatereq.h"
#include "version.h"
#include "watchdog.h"

extern unsigned char input1Bit5Active(void);
extern unsigned char intp11Low(void);

void task2MultiCnEvent(FLGPTN ptn);
unsigned char task2StateEvent(void);
unsigned char task2On(void);
unsigned char task2Off(void);
unsigned char task2Resume(void);
unsigned char task2Suspend(void);
unsigned char task2Shutdown(void);
unsigned char task2Cut(void);
unsigned char task2Reset(void);
unsigned char task2Tick(void);

/* the NVS request's events (0x00090804: 0x0040 too, the job service's, which task2On and the
   RTC's interrupt set) */
#if FW_SINCE(0x00090903)
#define TASK2_NVS_EVENTS	0x00a0
#else
#define TASK2_NVS_EVENTS	0x00e0
#endif

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
				task2StateEvent();
			}
			if (ptn & 0x0101) {
				clr_flg(3, ~(ptn & 0x0101));
				task2MultiCnEvent(ptn);
			}
			if (ptn & 0x2000) {
				clr_flg(3, ~(ptn & 0x2000));
				commandRequestRun();
			}
			if (ptn & 0x0e1e)
				serial4Event(ptn);
			if (ptn & TASK2_NVS_EVENTS) {
				clr_flg(3, ~(ptn & TASK2_NVS_EVENTS));
				nvsRequestEvent(ptn);
			}
			if (ptn & 0x8000) {
				clr_flg(3, ~0x8000);
				task2Tick();
			}
			if (ptn & 0x4000) {
				clr_flg(3, ~0x4000);
				configRequestEvent(ptn);
			}
#if FW_SINCE(0x00090903)
			if (ptn & 0x0040) {
				clr_flg(3, ~(ptn & 0x0040));
				updaterRequestEvent(ptn);
			}
#endif
		}
		task2Idle();
	}
}

void task2MultiCnEvent(FLGPTN ptn)
{
	unsigned char state;

	state = serialGetMode();
	if (ptn & 0x0001) {
		if (input1Bit5Active()) {
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
		if (intp11Low())
			serial4Open2(2);
		else
			serial4Close2();
	}
	return;
}

unsigned char task2StateEvent(void)
{
	unsigned char ret;
	unsigned int state;

	ret = 0;
	state = stateGet();
	if (state & 0x8000) {		/* the state's power sequence ran: what follows it */
		switch (state) {
		case 0x8000 | STATE_RESUME:
			ret = task2Resume();
			break;
		case 0x8000 | STATE_SUSPEND:
			ret = task2Suspend();
			break;
		case 0x8000 | STATE_OFF:
			ret = task2Off();
			break;
		case 0x8000 | STATE_ON:
			ret = task2On();
			break;
		case 0x8000 | STATE_SHUTDOWN:
			ret = task2Shutdown();
			break;
		case 0x8000 | STATE_CUT:
			ret = task2Cut();
			break;
		case 0x8000 | STATE_RESET:
			ret = task2Reset();
			break;
		}
		powerReady(2);
	} else {
		switch (state) {
		case STATE_ON:
		case STATE_OFF:
		case STATE_SUSPEND:
		case STATE_SHUTDOWN:
		case STATE_RESUME:
		case STATE_RESET:
		case STATE_CUT:
			break;
		}
	}
	return ret;
}

/* the end of the power-on sequence: the clock started after a cold start (cause 0x60), the
   links opened, the NVS loaded, and the host's watchdog set as configured */
unsigned char task2On(void)
{
	unsigned char ret;

	ret = 0;
#if FW_SINCE(0x01000104)
	if (stateGetCause() == 0x60)
#endif
		rtcStart();
	serialReset(3);
	serial4Reopen(2);
	serial2Init();
	if (nvsInit() != 0)
#if FW_SINCE(0x00090903)
		nvsJobService();
#else
		task2SetFlag(3, 0x0040);
#endif
	serial1Reset();
	boardMultiCnOutputOff();
	watchdogSet(&configGet()->watchdog);
	watchdogStop();
	return ret;
}

unsigned char task2Off(void)
{
	unsigned char ret;
#if FW_SINCE(0x00090903)
	unsigned char value;
#endif

	ret = 0;
	commandRequestInit();
	nvsRequestInit();
	serialAuthInit();
	serialRequestInit();
	serial4StartTimers();
	if (boardMultiCnIsDongle() || boardMultiCnIsDebug())
		boardSetMultiCnOutput(0);
	else
		boardMultiCnStop();
	updaterRequestInit();
	configRequestInit();
	watchdogStop();
#if FW_SINCE(0x00090903)
	if (nvsRead(0x2d, 1, &value, 1) == 0)
		setDlFlags(value);
#endif
#if FW_SINCE(0x01000104)
	if (getAlarmMode() == 1)
		setAlarm(RTC_ALARM_NONE);
#endif
	return ret;
}

/* the end of the resume sequence: the handshake's session key kept only for a wakeup that
   notifies (else the secure NVS starts over), and the host's requests opened again */
unsigned char task2Resume(void)
{
	unsigned char ret;
#if FW_SINCE(0x01000104)
	unsigned char keepKey;
	union PowerRequest *factor;
#endif

	ret = 0;
#if FW_SINCE(0x01000104)
	factor = stateGetWakeupFactor();
	if (factor->bits.notify)
		keepKey = 1;
	else {
		keepKey = 0;
		snvsReset();
	}
	handshakeInit(keepKey);
#else
	handshakeInit();
#endif
	commandRequestReceive();
	serialRequestReset();
	watchdogSetup();
#if !FW_SINCE(0x01000104) && FW_SINCE(0x00090903)
	snvsReset();	/* (0x00090804 has no SNVS) */
#endif
	return ret;
}

unsigned char task2Suspend(void)
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

unsigned char task2Shutdown(void)
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

unsigned char task2Cut(void)
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

unsigned char task2Reset(void)
{
	unsigned char ret;

	ret = 0;
	serialAuthInit();
	watchdogStop();
	return ret;
}

/* every second (the RTC's interrupt sets flag 0x8000): the alarm checked against the clock */
unsigned char task2Tick(void)
{
#if FW_SINCE(0x01000104)
	unsigned char ret;
	unsigned char clock;
	unsigned int state;
#else
	unsigned char ret;
	STATE_TRANSITION_EVENT
	unsigned int state;
#if FW_SINCE(0x00090903)
	unsigned char clock;
#endif
#endif

	ret = 0;
	state = stateGet();
#if FW_SINCE(0x00090903)
	clock = getClock().byte[0];
#endif
#if BOARD_WDT
	WDTE = 0xac;
#endif
	if (isAlarmReached()) {
		sleepFactorSet3(1);
		if ((stateGetWakeMask() & 2) == 0) {
#if FW_SINCE(0x01000104)
			if ((state == 0x0004 && getAlarmMode() != 1) || state == 0x0008)
#else
			if (state == 0x0004 || state == 0x0008)
#endif
				ret = STATE_TRANSITION(1);
		}
	} else {
		sleepFactorSet3(0);
	}
#if FW_SINCE(0x00090903)
	if (state != 0x0010 && (clock & 3) == 3)
		nvsJobService();
#endif
	logUpdate();
	if (state != 0x0004 && state != 0x0008)
		taskHookStandby();
	taskHookTick();
	return ret;
}
