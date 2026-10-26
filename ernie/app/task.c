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
#include "iica.h"
#include "kernel.h"
#include "state.h"
#include "sysinfo.h"
#include "task.h"
#include "timer.h"

__sreg unsigned int taskPending;

ER task1SendMessage(ID mbxid, T_MSG *msg)
{
	ER ercd;

	dis_dsp();
	ercd = snd_mbx((unsigned char)mbxid, msg);
	taskSetPending(TASK_PENDING_1);
	ena_dsp();
	return ercd;
}

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

unsigned int taskGetPending(void)
{
	return taskPending;
}

#if BOARD_STANDBY_STOP
/* standby halts while a timer, a task, a dongle or debug link or a hang needs the clock */
int taskStandbyHalt(void)
{
	return timerArmed() || taskGetPending() || boardMultiCnIsDongle() ||
	       boardMultiCnIsDebug() || hangStarted();
}
#else
/* standby halts, never stops, on the other boards */
int taskStandbyHalt(void)
{
	return 1;
}
#endif

unsigned int taskPending1(void)
{
	return taskGetPending() & TASK_PENDING_1;
}

/* hooks, empty in this build: a state's change, a transition handled, a standby, the tick */
void taskHookState(void)
{
}

void taskHookEvent(void *event)
{
}

void taskHookStandby(void)
{
}

void taskHookTick(void)
{
}
