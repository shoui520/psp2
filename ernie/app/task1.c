/*
 * Task 1: the system's start (the power state, the reset's flags and cause, the software
 * timers), then the messages of mailbox 1, each copied out of its block of fixed-size memory
 * pool 1 and handled.
 */
#pragma rtos_task task1
#pragma sfr

#include <string.h>
#include "ernie.h"
#include "board.h"
#if SOURCE_0103
#include "bstore.h"
#include "chipid.h"
#endif
#include "config.h"
#include "debugpin.h"
#if SOURCE_0103
#include "eventlog.h"
#endif
#include "kernel.h"
#include "log.h"
#include "param.h"
#include "power.h"
#include "scratchpad.h"
#include "state.h"
#include "sysinfo.h"
#include "task.h"
#include "timer.h"
#include "version.h"

/* the reset control flags (RESF) the chip had at the start: TRAP, WDRF, LVIRF */
#define RESF_TRAP	0x80
#define RESF_WDRF	0x10
#if SOURCE_0103
#define RESF_RPERF	0x04	/* the RAM's parity */
#define RESF_IAWRF	0x02	/* an illegal memory access */
#define RESF_LVIRF	0x01	/* the voltage detector */
#endif

extern unsigned char getResetCause(void);

void systemInit(void);

void task1(VP_INT exinf)
{
	ER ercd;
	struct StateMsg *msg;
	struct StateEvent event;

	systemInit();
	for (;;) {
		ercd = rcv_mbx(1, (T_MSG **)&msg);
		memcpy(&event, &msg->event, sizeof(event));
#if SOURCE_0511
		/* the slim's enter message is not the pool's (from 0x01030511, state.c) */
		if (event.type != STATE_EVENT_ENTER)
			ercd = rel_mpf(1, msg);
#else
		rel_mpf(1, msg);
#endif
		if (ercd == E_OK) {
			stateEvent(&event);
			powerSequence();
		}
		task1Idle();
	}
}

void systemInit(void)
{
	struct TimerHandler timer;
#if FW_SINCE(0x00090804)
	unsigned char cause;
#endif
#if BOARD_DL_SCRATCHPAD
	union {
		unsigned long	value;
		unsigned char	byte[4];
	} dlVersion;
#endif

#if SOURCE_0103
	WDTE = 0xac;
	memset(stateHistory, 0, sizeof(stateHistory));
	powerState = 1;
	stateRecord.count = 0;
	statePhase = 2;
	stateNext = 0;
	stateEvents = 0;
	stateCause = 0x7f;
#if BOARD_USS1001
	chipIdInit();
	eventLogInit();
	eventLogStart();
#endif
	debugPinInit();
	resetFlags = RESF;
	configInit();
	timerInit();
	paramInit();
	taskClearPending(-1);
	taskSetPending(1);
	timer.handler = stateCheck;
	timerSetHandler(TIMER_RTC(0x1a), &timer);
	versionInit();
	cause = getResetCause() & 0xf0;
#if BOARD_USS1001
	/* the reset's cause and flags, in the event log */
	eventLogTask1.byte[0] = cause;
	eventLogTask1.byte[1] = resetFlags;
	eventLogTask1.byte[2] = 0;
	eventLogTask1.byte[3] = 0;
	eventLogRecord(2, 0xff, eventLogTask1.byte, 4);
#endif
	logInit();
	logQueueInit();
#if BOARD_BSTORE
	bstoreInit();
#endif
	if (cause == 0) {
		if (resetFlags & RESF_TRAP) {
			scratchPadClear();
			logEvent(0x19);
		}
		if (resetFlags & RESF_WDRF) {
			scratchPadClear();
			logEvent(7);
		}
#if BOARD_USS1001	/* the RL78's flags */
		if (resetFlags & RESF_RPERF)
			scratchPadClear();
		if (resetFlags & RESF_IAWRF) {
			scratchPadClear();
			logEvent(0x1a);
		}
		if (resetFlags & RESF_LVIRF)
			scratchPadClear();
#endif
	} else if (cause == 0x30) {
		if (resetFlags & RESF_TRAP) {
			scratchPadClear();
			logEvent(6);
		}
	} else if (cause == 0x50) {
		scratchPadClear();
	} else if (cause == 0x60) {
		logEvent(5);
		logEvent(0x1d);
	}
	stateEnter();
	taskHookState();
#else
#if BOARD_WDT
	WDTE = 0xac;
#endif
	powerState = 1;
	statePhase = 2;
	stateNext = 0;
	stateEvents = 0;
#if FW_SINCE(0x00090009)
	stateCause = 0x7f;
#endif
#if FW_SINCE(0x0100060B)
	debugPinInit();
#endif
#if BOARD_DEBUG_PINS_POWER
	debugPinReserved(0);
	debugPinCode(0x7f);
#endif
#if FW_SINCE(0x00090206)
	resetFlags = RESF;
	configInit();
#endif
	/* 0x00090009 keeps no reset flags and opens the settings in task2On */
	timerInit();
	paramInit();
	taskClearPending(-1);
	taskSetPending(1);
	timer.handler = stateCheck;
	timerSetHandler(TIMER_RTC(0x1a), &timer);
	versionInit();
#if FW_SINCE(0x00090804)
	cause = getResetCause() & 0xf0;
#if BOARD_DL_SCRATCHPAD
	if (cause == 0) {
		getErnieDlVersion(dlVersion.byte);
		if (dlVersion.byte[1] == 0 && dlVersion.byte[2] < 0x16)
			scratchPadClear();
	}
#endif
	logQueueInit();
	logInit();
#if !FW_SINCE(0x01000104)
	if (cause == 0x10)
		logEvent(4);
	if (cause == 0) {
		if (resetFlags & RESF_TRAP)
			logEvent(6);
		if (resetFlags & RESF_WDRF)
			logEvent(7);
	}
#else
	if (cause == 0) {
		if (resetFlags & RESF_TRAP)
			scratchPadClear();
		if (resetFlags & RESF_WDRF) {
			scratchPadClear();
			logEvent(7);
		}
	} else if (cause == 0x30) {
		if (resetFlags & RESF_TRAP) {
			scratchPadClear();
			logEvent(6);
		}
	}
#endif
#endif
	stateEnter();
#if FW_SINCE(0x0100060B)
	taskHookState();
#endif
#endif	/* SOURCE_0103 */
	return;
}
