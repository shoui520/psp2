/*
 * The tasks' pending work (app/task.c): bit n-1 of taskPending is set when an event is sent
 * to task n and cleared when the task finds its flag or mailbox empty, so the system may
 * stand by only when nothing is pending.
 */
#ifndef ERNIE_TASK_H
#define ERNIE_TASK_H

#include "kernel.h"

#define TASK_PENDING_1		0x0001	/* mailbox 1 */
#define TASK_PENDING_2		0x0002	/* flag 3 */
#define TASK_PENDING_3		0x0004	/* flag 2 */
#define TASK_PENDING_4		0x0008	/* flag 1 */
#define TASK_PENDING_5		0x0010	/* flag 4, with the I2C bus on */
#define TASK_PENDING_6		0x0020	/* flag 6 */

extern __sreg unsigned int taskPending;

extern ER task1SendMessage(ID mbxid, T_MSG *msg);
extern ER task2SetFlag(ID flgid, FLGPTN setptn);
extern ER task5SetFlag(ID flgid, FLGPTN setptn);
extern ER task3SetFlag(ID flgid, FLGPTN setptn);
extern ER task4SetFlag(ID flgid, FLGPTN setptn);
extern ER task6SetFlag(ID flgid, FLGPTN setptn);
extern void task1Idle(void);
extern void task2Idle(void);
extern void task5Idle(void);
extern void task3Idle(void);
extern void task4Idle(void);
extern void task6Idle(void);
extern void taskSetPending(unsigned int pending);
extern void taskClearPending(unsigned int pending);
extern unsigned int taskGetPending(void);
extern int taskStandbyHalt(void);
extern unsigned int taskPending1(void);
extern void taskHookState(void);
extern void taskHookEvent(void *event);
extern void taskHookStandby(void);
extern void taskHookTick(void);

#endif
