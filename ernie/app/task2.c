/*
 * Task 2: the events of event flag 3, each group of bits cleared and handled.
 */
#pragma rtos_task task2

#pragma sfr

#include "ernie.h"
#include "board.h"
#include "command.h"
#include "config.h"
#include "debugpin.h"
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
#if SOURCE_0103
#include "sysinfo.h"
#endif

void task2MultiCnEvent(FLGPTN ptn);
unsigned char task2StateEvent(void);
unsigned char task2On(void);
unsigned char task2Off(void);
unsigned char task2Resume(void);
unsigned char task2Suspend(void);
unsigned char task2Shutdown(void);
unsigned char task2Cut(void);
unsigned char task2Reset(void);
#if SOURCE_0103
unsigned char task2UpdateAnswer(void);
unsigned char task2Update(void);
#endif
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

#if SOURCE_0103
/* the slim's multi-connector follows its own state; INTP11 is read and not used */
void task2MultiCnEvent(FLGPTN ptn)
{
	if (ptn & 0x0001)
		boardMultiCnUpdate();
	if (ptn & 0x0100) {
		if (intp11Low())
			;
	}
	return;
}
#else
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
#if FW_SINCE(0x00090804)
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
#else
			}
		} else {
			/* 0x00090206: the multi-connector's line back to serial 3 */
			serialClose1();
			serialOpen3(3);
			boardSetMultiCnOutput(0);
		}
#endif
	}
	if (ptn & 0x0100) {
		if (intp11Low())
			serial4Open2(2);
		else
			serial4Close2();
	}
	return;
}
#endif

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
#if SOURCE_0103
		case 0x8000 | STATE_UPDATE_ANSWER:
			ret = task2UpdateAnswer();
			break;
		case 0x8000 | STATE_UPDATE:
			ret = task2Update();
			break;
#endif
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
#if SOURCE_0103
		case STATE_UPDATE_ANSWER:
		case STATE_UPDATE:
#endif
			break;
		}
	}
	return ret;
}

/* the end of the power-on sequence: the clock started after a cold start (cause 0x60), the
   links opened, the NVS loaded, and the host's watchdog set as configured */
#if SOURCE_0103
/* the slim's: no serial 4 or 2 here, and the log started again after a cause 0x60 */
unsigned char task2On(void)
{
	unsigned char ret;
	unsigned char cause;

	ret = 0;
	cause = stateGetCause();
	if (cause == 0x60)
		rtcStart();
	serialReset(3);
	if (nvsInit() != 0)
		nvsJobService();
	if (cause == 0x60)
		logInit();
	boardMultiCnOutputOff();
	watchdogSet(&configGet()->watchdog);
	watchdogStop();
	return ret;
}
#else
unsigned char task2On(void)
{
	unsigned char ret;
#if !FW_SINCE(0x00090206)
	unsigned char config;
#endif

	ret = 0;
#if !FW_SINCE(0x00090206)
	/* 0x00090009 opens the settings here; its result is tested and nothing done (an empty
	   statement, as a print left out of the build) */
	config = configInit();
	if (config != 0)
		;
#endif
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
#if FW_SINCE(0x00090804)
	boardMultiCnOutputOff();
#endif
#if HOST_WATCHDOG
	watchdogSet(&configGet()->watchdog);
	watchdogStop();
#endif
	return ret;
}
#endif

unsigned char task2Off(void)
{
	unsigned char ret;
#if FW_SINCE(0x00090903)
	unsigned char value;
#endif
#if SOURCE_0103
	unsigned char length;
	unsigned char size;
#endif

	ret = 0;
	commandRequestInit();
	nvsRequestInit();
	serialAuthInit();
	serialRequestInit();
	serial4StartTimers();
#if SOURCE_0103
	boardMultiCnUpdate();
#elif FW_SINCE(0x00090804)
	if (boardMultiCnIsDongle() || boardMultiCnIsDebug())
		boardSetMultiCnOutput(0);
	else
		boardMultiCnStop();
#else
	boardSetMultiCnOutput(0);
#endif
	updaterRequestInit();
	configRequestInit();
#if HOST_WATCHDOG
	watchdogStop();
#endif
#if FW_SINCE(0x00090903)
	if (nvsRead(0x2d, 1, &value, 1) == 0)
		setDlFlags(value);
#endif
#if FW_SINCE(0x01000104)
	if (getAlarmMode() == 1)
		setAlarm(RTC_ALARM_NONE);
#endif
#if SOURCE_0103	/* the fat ones read them in stateOffEnter */
	logReadData(1, (unsigned char *)&idRecord1, &length);
	logReadData(0x0f, &size, &length);
	idRecordsLength = size;
	logReadData(0x10, &size, &length);
	idRecordsLength += size;
	logReadData(0x12, &size, &length);
	idRecordsLength += size;
	logReadData(0x16, &size, &length);
	idRecordsLength += size;
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
#if HOST_WATCHDOG
	watchdogSetup();
#endif
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
#if SOURCE_0103
	boardMultiCnUpdate();
#elif FW_SINCE(0x00090804)
	if (boardMultiCnIsDongle() || boardMultiCnIsDebug())
		boardSetMultiCnOutput(0);
	else
		boardMultiCnStop();
#else
	boardSetMultiCnOutput(0);
#endif
#if HOST_WATCHDOG
	watchdogStop();
#endif
	return ret;
}

unsigned char task2Shutdown(void)
{
	unsigned char ret;

	ret = 0;
#if SOURCE_0103
	boardMultiCnOutputOff();
#else
	serial4Close2();
#if FW_SINCE(0x00090804)
	boardMultiCnStop();
#else
	boardSetMultiCnOutput(0);
#endif
	serialClose1();
	serial1Reset();
#endif
#if HOST_WATCHDOG
	watchdogStop();
#endif
	return ret;
}

unsigned char task2Cut(void)
{
	unsigned char ret;

	ret = 0;
#if SOURCE_0103
	boardMultiCnOutputOff();
#else
	serial4Close2();
#if FW_SINCE(0x00090804)
	boardMultiCnStop();
#else
	boardSetMultiCnOutput(0);
#endif
	serialClose1();
	serial1Reset();
#endif
#if HOST_WATCHDOG
	watchdogStop();
#endif
	return ret;
}

unsigned char task2Reset(void)
{
	unsigned char ret;

	ret = 0;
	serialAuthInit();
#if HOST_WATCHDOG
	watchdogStop();
#endif
	return ret;
}

#if SOURCE_0103
unsigned char task2UpdateAnswer(void)
{
	return 0;
}

/* the settings readied for the update; the result is not used */
unsigned char task2Update(void)
{
	unsigned char ret;

	ret = 0;
	if (configPrepareUpdate() != 0)
		;
	return ret;
}
#endif

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
#if FW_SINCE(0x00090804)
	logUpdate();
#endif
	if (state != 0x0004 && state != 0x0008)
		taskHookStandby();
	taskHookTick();
#if BOARD_DEBUG_PINS_POWER
	debugPinSet((unsigned char)(P4.0 ^ 1));	/* toggled at each tick */
#endif
	return ret;
}
