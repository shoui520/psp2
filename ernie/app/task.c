/*
 * The tasks' pending work: each event goes to its task with dispatching off and marks the
 * task pending; after its work each task checks, with interrupts off too, that nothing new
 * has come, and clears its mark. The ids reach the kernel as bytes.
 */
#pragma sfr
#pragma DI
#pragma EI

#include <stddef.h>

#include "board.h"
#include "ernie.h"
#if SOURCE_0103
#include "eventlog.h"
#endif
#include "iica.h"
#include "kernel.h"
#include "state.h"
#include "sysinfo.h"
#include "task.h"
#include "timer.h"

#if BOARD_STANDBY_STOP && !FW_SINCE(0x00090203)
extern unsigned char input1Bit5Active(void);
#endif

__sreg unsigned int taskPending;

ER task1SendMessage(ID mbxid, T_MSG *msg)
{
	ER ercd;

	dis_dsp();
	ercd = snd_mbx((unsigned char)mbxid, msg);
#if TASK_FLAG_SETTERS
	taskPending |= TASK_PENDING_1;
#else
	taskSetPending(TASK_PENDING_1);
#endif
	ena_dsp();
	return ercd;
}

#if TASK_FLAG_SETTERS
ER task2SetPattern(FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg(3, setptn);
	taskPending |= TASK_PENDING_2;
	ena_dsp();
	return ercd;
}

ER task2ISetFlag(FLGPTN setptn)
{
	taskPending |= TASK_PENDING_2;
	return iset_flg(3, setptn);
}

ER task5SetPattern(FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg(4, setptn);
	iicaInit();
	taskPending |= TASK_PENDING_5;
	ena_dsp();
	return ercd;
}

ER task5ISetFlag(FLGPTN setptn)
{
	iicaInit();
	taskPending |= TASK_PENDING_5;
	return iset_flg(4, setptn);
}

ER task3SetPattern(FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg(2, setptn);
	taskPending |= TASK_PENDING_3;
	ena_dsp();
	return ercd;
}

/* by set_flg, not iset_flg */
ER task3ISetFlag(FLGPTN setptn)
{
	taskPending |= TASK_PENDING_3;
	return set_flg(2, setptn);
}

ER task4SetPattern(FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg(1, setptn);
	taskPending |= TASK_PENDING_4;
	ena_dsp();
	return ercd;
}

ER task4ISetFlag(FLGPTN setptn)
{
	taskPending |= TASK_PENDING_4;
	return iset_flg(1, setptn);
}

ER task6SetPattern(FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg(6, setptn);
	taskPending |= TASK_PENDING_6;
	ena_dsp();
	return ercd;
}

ER task6ISetFlag(FLGPTN setptn)
{
	taskPending |= TASK_PENDING_6;
	return iset_flg(6, setptn);
}

/* task 1 stays pending outside the off, suspend, shutdown and cut states */
void task1Idle(void)
{
	T_RMBX rmbx;
	unsigned int state;

	dis_dsp();
	DI();
	ref_mbx(1, &rmbx);
	state = stateGet();
	if (rmbx.pk_msg == NULL &&
	    (state == STATE_OFF || state == STATE_SUSPEND || state == STATE_SHUTDOWN ||
	     state == STATE_CUT))
		taskPending &= ~TASK_PENDING_1;
	EI();
	ena_dsp();
}

/* the others only once task 1 is idle */
void task2Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(3, &rflg);
		if (rflg.flgptn == 0)
			taskPending &= ~TASK_PENDING_2;
		EI();
		ena_dsp();
	}
}

void task5Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(4, &rflg);
		if (rflg.flgptn == 0) {
			iicaClose();
			taskPending &= ~TASK_PENDING_5;
		}
		EI();
		ena_dsp();
	}
}

void task3Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(2, &rflg);
		if (rflg.flgptn == 0)
			taskPending &= ~TASK_PENDING_3;
		EI();
		ena_dsp();
	}
}

void task4Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(1, &rflg);
		if (rflg.flgptn == 0)
			taskPending &= ~TASK_PENDING_4;
		EI();
		ena_dsp();
	}
}

void task6Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(6, &rflg);
		if (rflg.flgptn == 0)
			taskPending &= ~TASK_PENDING_6;
		EI();
		ena_dsp();
	}
}

void taskPendingReset(void)
{
	taskPending = TASK_PENDING_1;
}
#else
ER task2SetFlag(ID flgid, FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg((unsigned char)flgid, setptn);
	taskSetPending(TASK_PENDING_2);
	ena_dsp();
	return ercd;
}

ER task5SetFlag(ID flgid, FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg((unsigned char)flgid, setptn);
	iicaInit();
	taskSetPending(TASK_PENDING_5);
	ena_dsp();
	return ercd;
}

ER task3SetFlag(ID flgid, FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg((unsigned char)flgid, setptn);
	taskSetPending(TASK_PENDING_3);
	ena_dsp();
	return ercd;
}

ER task4SetFlag(ID flgid, FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg((unsigned char)flgid, setptn);
	taskSetPending(TASK_PENDING_4);
	ena_dsp();
	return ercd;
}

ER task6SetFlag(ID flgid, FLGPTN setptn)
{
	ER ercd;

	dis_dsp();
	ercd = set_flg((unsigned char)flgid, setptn);
	taskSetPending(TASK_PENDING_6);
	ena_dsp();
	return ercd;
}

/* task 1 stays pending outside the off, suspend, shutdown and cut states */
void task1Idle(void)
{
	T_RMBX rmbx;
	unsigned int state;

	dis_dsp();
	DI();
	ref_mbx(1, &rmbx);
	state = stateGet();
	if (rmbx.pk_msg == NULL &&
	    (state == STATE_OFF || state == STATE_SUSPEND || state == STATE_SHUTDOWN ||
	     state == STATE_CUT))
		taskClearPending(TASK_PENDING_1);
	EI();
	ena_dsp();
}

/* the others only once task 1 is idle */
void task2Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(3, &rflg);
		if (rflg.flgptn == 0)
			taskClearPending(TASK_PENDING_2);
		EI();
		ena_dsp();
	}
}

void task5Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(4, &rflg);
		if (rflg.flgptn == 0) {
			iicaClose();
			taskClearPending(TASK_PENDING_5);
		}
		EI();
		ena_dsp();
	}
}

void task3Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(2, &rflg);
		if (rflg.flgptn == 0)
			taskClearPending(TASK_PENDING_3);
		EI();
		ena_dsp();
	}
}

void task4Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(1, &rflg);
		if (rflg.flgptn == 0)
			taskClearPending(TASK_PENDING_4);
		EI();
		ena_dsp();
	}
}

void task6Idle(void)
{
	T_RFLG rflg;

	if (taskPending1() == 0) {
		dis_dsp();
		DI();
		ref_flg(6, &rflg);
		if (rflg.flgptn == 0)
			taskClearPending(TASK_PENDING_6);
		EI();
		ena_dsp();
	}
}

void taskSetPending(unsigned int pending)
{
	DI();
	taskPending |= pending;
	EI();
}

void taskClearPending(unsigned int pending)
{
	DI();
	taskPending &= ~pending;
	EI();
}

#endif

unsigned int taskGetPending(void)
{
	return taskPending;
}

#if BOARD_STANDBY_STOP
/* standby halts while a timer, a task, a dongle or debug link or a hang needs the clock */
int taskStandbyHalt(void)
{
#if !FW_SINCE(0x00090203) || (FW_SINCE(0x00090502) && !FW_SINCE(0x00090804))
	/* PCH-1000's 0x00090007 and 0x00090502: whatever is on the multi-connector, no debug
	   link or hang */
	return timerArmed() || taskGetPending() || input1Bit5Active();
#elif FW_SINCE(0x0100060B)
	/* no hang check since 0x0100060B */
	return timerArmed() || taskGetPending() || boardMultiCnIsDongle() ||
	       boardMultiCnIsDebug();
#else
	return timerArmed() || taskGetPending() || boardMultiCnIsDongle() ||
	       boardMultiCnIsDebug() || hangStarted();
#endif
}
#else
/* standby halts, never stops, on the other boards */
int taskStandbyHalt(void)
{
	return 1;
}
#endif

#if SOURCE_0103
/* USS-1001: 1 while a state's sequence runs or the state is none of 0x0004, 0x0008, 0x0010 and
   0x0800 */
unsigned int taskPending1(void)
{
	unsigned int state;

	state = stateGet();
	return (state & 0x8000) || !(state & 0x081c);
}
#else
unsigned int taskPending1(void)
{
	return taskGetPending() & TASK_PENDING_1;
}
#endif

#if BOARD_EVENT_LOG
/* hooks: a state's change and (from 0x01030511) a transition handled go into the event log
   (kind 2, codes 1 and 2), a standby and the tick do nothing */
void taskHookState(void)
{
	eventLogTask1.word[0] = powerState;
	eventLogTask1.byte[2] = stateCause;
	eventLogTask1.byte[3] = stateWakeupFactor.byte[0];
	eventLogRecord(2, 1, eventLogTask1.byte, 4);
}

void taskHookEvent(void *event)
{
#if FW_SINCE(0x01030511)	/* 0x01030505's is empty */
	eventLogTask1.word[0] = powerState;
	eventLogTask1.byte[2] = ((struct StateEvent *)event)->type;
	eventLogTask1.byte[3] = ((struct StateEvent *)event)->transition;
	eventLogRecord(2, 2, eventLogTask1.byte, 4);
#endif
}
#else
/* hooks, empty in this build: a state's change, a transition handled, a standby, the tick */
void taskHookState(void)
{
}

void taskHookEvent(void *event)
{
}
#endif

void taskHookStandby(void)
{
}

void taskHookTick(void)
{
}
