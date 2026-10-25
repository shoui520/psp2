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
#include "power.h"
#include "scratchpad.h"
#include "timer.h"
#include "version.h"

/* the reset control flags (RESF) the chip had at the start: TRAP, WDRF, LVIRF */
#define RESF_TRAP	0x80
#define RESF_WDRF	0x10

extern __sreg unsigned char saddr_FFE68;
extern __sreg unsigned int powerState;
extern __sreg unsigned int saddr_FFE6C;
extern __sreg unsigned int saddr_FFE6E;
extern __sreg unsigned char saddr_FFE70;
extern __near unsigned char resetFlags;

extern void sub_2C57F(void);
extern void sub_2407A(void);
extern void sub_2C051(void);
extern void sub_2C54C(int);
extern void sub_2C539(int);
extern void sub_2AC15(void);
extern unsigned char getResetCause(void);
extern void sub_2D6EF(void);
extern void sub_2D712(void);
extern void sub_2DAE1(int);
extern void sub_2ABAE(void);
extern void sub_2A92E(unsigned char *data);
extern void sub_2C3BD(void);

struct Task1Msg {
	T_MSG		header;
	unsigned char	data[8];
};

void systemInit(void);

void task1(VP_INT exinf)
{
	ER ercd;
	struct Task1Msg *msg;
	unsigned char data[8];

	systemInit();
	for (;;) {
		ercd = rcv_mbx(1, (T_MSG **)&msg);
		memcpy(data, msg->data, sizeof(data));
		rel_mpf(1, msg);
		if (ercd == E_OK) {
			sub_2A92E(data);
			powerSequence();
		}
		sub_2C3BD();
	}
}

void systemInit(void)
{
	struct TimerHandler timer;
	unsigned char cause;

	powerState = 1;
	saddr_FFE68 = 2;
	saddr_FFE6C = 0;
	saddr_FFE6E = 0;
	saddr_FFE70 = 0x7f;
	resetFlags = RESF;
	sub_2C57F();
	sub_2407A();
	sub_2C051();
	sub_2C54C(-1);
	sub_2C539(1);
	timer.handler = sub_2AC15;
	sub_240CF(0x1a, &timer);
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
	sub_2ABAE();
	return;
}
