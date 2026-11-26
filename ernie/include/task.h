/*
 * The tasks' pending work (app/task.c): bit n-1 of taskPending is set when an event is sent
 * to task n and cleared when the task finds its flag or mailbox empty, so the system may
 * stand by only when nothing is pending.
 */
#ifndef ERNIE_TASK_H
#define ERNIE_TASK_H

#include "board.h"
#include "kernel.h"

#define TASK_PENDING_1		0x0001	/* mailbox 1 */
#define TASK_PENDING_2		0x0002	/* flag 3 */
#define TASK_PENDING_3		0x0004	/* flag 2 */
#define TASK_PENDING_4		0x0008	/* flag 1 */
#define TASK_PENDING_5		0x0010	/* flag 4, with the I2C bus on */
#define TASK_PENDING_6		0x0020	/* flag 6 */

extern __sreg unsigned int taskPending;

/* the slim's 0x0105 (USS-1002): each task's setter knows its flag and takes only the pattern
   (task<n>SetPattern), marks the task pending itself, and has a twin by iset_flg for the
   interrupts and the timers' handlers (task<n>ISetFlag) */
#define TASK_FLAG_SETTERS	(BOARD_USS1001 && SOURCE_0104)

/* 0x00080307 has none of this (no task.c): the events go to the kernel as they are, and
   nothing is marked pending */
#define TASK_PENDING_MARKS	FW_SINCE(0x00080502)

#if !TASK_PENDING_MARKS
#define task1SendMessage(mbxid, msg)	snd_mbx(mbxid, msg)
#define task2SetFlag(flgid, setptn)	set_flg(flgid, setptn)
#define task5SetFlag(flgid, setptn)	set_flg(flgid, setptn)
#define task3SetFlag(flgid, setptn)	set_flg(flgid, setptn)
#define task4SetFlag(flgid, setptn)	set_flg(flgid, setptn)
#define task6SetFlag(flgid, setptn)	set_flg(flgid, setptn)
#define task2ISetFlag(setptn)	iset_flg(3, setptn)
#define task5ISetFlag(setptn)	iset_flg(4, setptn)
#define task3ISetFlag(setptn)	iset_flg(2, setptn)
#define task4ISetFlag(setptn)	iset_flg(1, setptn)
#define task6ISetFlag(setptn)	iset_flg(6, setptn)
#else
extern ER task1SendMessage(ID mbxid, T_MSG *msg);
#endif
#if !TASK_PENDING_MARKS
#elif TASK_FLAG_SETTERS
extern ER task2SetPattern(FLGPTN setptn);
extern ER task2ISetFlag(FLGPTN setptn);
extern ER task5SetPattern(FLGPTN setptn);
extern ER task5ISetFlag(FLGPTN setptn);
extern ER task3SetPattern(FLGPTN setptn);
extern ER task3ISetFlag(FLGPTN setptn);
extern ER task4SetPattern(FLGPTN setptn);
extern ER task4ISetFlag(FLGPTN setptn);
extern ER task6SetPattern(FLGPTN setptn);
extern ER task6ISetFlag(FLGPTN setptn);
/* the callers name the flag as the other builds' do */
#define task2SetFlag(flgid, setptn)	task2SetPattern(setptn)
#define task5SetFlag(flgid, setptn)	task5SetPattern(setptn)
#define task3SetFlag(flgid, setptn)	task3SetPattern(setptn)
#define task4SetFlag(flgid, setptn)	task4SetPattern(setptn)
#define task6SetFlag(flgid, setptn)	task6SetPattern(setptn)
#else
extern ER task2SetFlag(ID flgid, FLGPTN setptn);
extern ER task5SetFlag(ID flgid, FLGPTN setptn);
extern ER task3SetFlag(ID flgid, FLGPTN setptn);
extern ER task4SetFlag(ID flgid, FLGPTN setptn);
extern ER task6SetFlag(ID flgid, FLGPTN setptn);
/* the other builds' interrupts set the flags as the tasks do */
#define task2ISetFlag(setptn)	task2SetFlag(3, setptn)
#define task5ISetFlag(setptn)	task5SetFlag(4, setptn)
#define task3ISetFlag(setptn)	task3SetFlag(2, setptn)
#define task4ISetFlag(setptn)	task4SetFlag(1, setptn)
#define task6ISetFlag(setptn)	task6SetFlag(6, setptn)
#endif
#if TASK_PENDING_MARKS
extern void task1Idle(void);
extern void task2Idle(void);
extern void task5Idle(void);
extern void task3Idle(void);
extern void task4Idle(void);
extern void task6Idle(void);
#if TASK_FLAG_SETTERS
extern void taskPendingReset(void);
#else
extern void taskSetPending(unsigned int pending);
extern void taskClearPending(unsigned int pending);
#endif
extern unsigned int taskGetPending(void);
#endif
extern int taskStandbyHalt(void);
extern unsigned int taskPending1(void);
extern void taskHookState(void);
extern void taskHookEvent(void *event);
extern void taskHookStandby(void);
extern void taskHookTick(void);

#endif
