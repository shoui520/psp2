/*
 * Task 1: the system's start (the power state, the reset's flags and cause, the software
 * timers), then the messages of mailbox 1, each copied out of its block of fixed-size memory
 * pool 1 and handled.
 */
#pragma rtos_task task1
#pragma sfr

#include <string.h>
#include "ernie.h"
#include "kernel.h"
#include "param.h"
#include "power.h"
#include "scratchpad.h"
#include "state.h"
#include "task.h"
#include "timer.h"
#include "version.h"

/* the reset control flags (RESF) the chip had at the start: TRAP, WDRF, LVIRF */
#define RESF_TRAP	0x80
#define RESF_WDRF	0x10

extern __near unsigned char resetFlags;

extern void sub_2C57F(void);
extern unsigned char getResetCause(void);
extern void sub_2D6EF(void);
extern void sub_2D712(void);
extern void sub_2DAE1(int);

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
		rel_mpf(1, msg);
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
	unsigned char cause;

	powerState = 1;
	statePhase = 2;
	stateNext = 0;
	stateEvents = 0;
	stateCause = 0x7f;
	resetFlags = RESF;
	sub_2C57F();
	timerInit();
	paramInit();
	taskClearPending(-1);
	taskSetPending(1);
	timer.handler = stateCheck;
	timerSetHandler(0x1a, &timer);
	versionInit();
	cause = getResetCause() & 0xf0;
	sub_2D6EF();
	sub_2D712();
	if (cause == 0) {
		if (resetFlags & RESF_TRAP)
			scratchPadClear();
		if (resetFlags & RESF_WDRF) {
			scratchPadClear();
			sub_2DAE1(7);
		}
	} else if (cause == 0x30) {
		if (resetFlags & RESF_TRAP) {
			scratchPadClear();
			sub_2DAE1(6);
		}
	}
	stateEnter();
	return;
}
