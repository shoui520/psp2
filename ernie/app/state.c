/*
 * The system state: task 1 hands each event of mailbox 1 to the entry of powerState in
 * stateTable. An entry's events, once one of them is set, start the next state (its bit 15
 * set until the power sequence ran it, power.c).
 */
#pragma sfr

#include <string.h>

#include "ernie.h"
#include "config.h"
#include "host.h"
#include "kernel.h"
#include "state.h"
#include "timer.h"

extern void sub_2AC74(void);
extern void sub_2ACA9(void);
extern void sub_2ACB6(void);
extern void sub_2ADFC(void);
extern void sub_2AE31(void);
extern void sub_2AE7D(void);
extern void sub_2AE8B(void);
extern void sub_2AEA5(void);
extern void sub_2AEAE(void);
extern void sub_2AEB0(void);
extern void sub_2AEB6(struct StateEvent *event);
extern void sub_2AED2(struct StateEvent *event);
extern void sub_2AEF8(struct StateEvent *event);
extern void sub_2B0DE(struct StateEvent *event);
extern void sub_2B150(struct StateEvent *event);
extern void sub_2B275(struct StateEvent *event);
extern void sub_2B2B4(struct StateEvent *event);
extern void sub_2B2E2(struct StateEvent *event);
extern void sub_2B32F(struct StateEvent *event);
extern void sub_2B385(struct StateEvent *event);
extern unsigned int sub_2B391(unsigned int state);
extern unsigned int sub_2B3FB(void);
extern void sub_2C2C1(ID mbxid, T_MSG *msg);
extern void sub_2C315(ID flgid, FLGPTN setptn);
extern void sub_2C394(int, int);
extern unsigned int sub_2C561(void);
extern void sub_2C572(void);
extern void sub_2C573(struct StateEvent *event);

static const struct StateEntry stateTable[] = {
	{ 0x0001, 0x0002, sub_2AC74, sub_2AEB6 },
	{ 0x0002, 0x0804, sub_2ACA9, sub_2AED2 },
	{ 0x0004, 0x0830, sub_2ACB6, sub_2AEF8 },
	{ 0x0020, 0x0e5c, sub_2ADFC, sub_2B0DE },
	{ 0x0008, 0x0834, sub_2AE31, sub_2B150 },
	{ 0x0010, 0x0802, sub_2AE7D, sub_2B275 },
	{ 0x0800, 0x0002, sub_2AE8B, sub_2B2B4 },
	{ 0x0040, 0x0830, sub_2AEA5, sub_2B2E2 },
	{ 0x0200, 0x0834, sub_2AEAE, sub_2B32F },
	{ 0x0400, 0x0000, sub_2AEB0, sub_2B385 },
};

__sreg unsigned char statePhase;
__sreg unsigned int powerState;
__sreg unsigned int stateNext;
__sreg unsigned int stateEvents;
__sreg unsigned char stateCause;
static unsigned char stateReserved[6];
union PowerRequest stateWakeupFactor;
union PowerRequest stateRequest;
static unsigned long stateWakeMask;

void stateHostStop(void)
{
	hostIntDisable();
	set_flg(1, 0x0100);
	dly_tsk(250);
}

void stateHostStart(void)
{
	P5.7 = 0;
	hostInit();
	hostIntEnable();
}

/* task 1's, for each event of mailbox 1 */
void stateEvent(struct StateEvent *event)
{
	unsigned char i;
	unsigned int status;

	for (i = 0; i < 10; i++) {
		if (stateTable[i].state == powerState) {
			switch (event->type) {
			case STATE_EVENT_ENTER:
				stateTable[i].enter();
				statePhase = 0;
				stateEvents = sub_2B391(powerState);
				sub_2C572();
				break;
			case STATE_EVENT_TRANSITION:
				stateTable[i].handle(event);
				stateEvents |= stateNext;
				sub_2C573(event);
				if (statePhase != 0)
					return;
				break;
			case STATE_EVENT_CHECK:
				status = sub_2C561();
				if (status & 0x20)
					sub_2C394(6, 0x4000);
				if (status & 0x10)
					sub_2C315(4, 0x8000);
				if (status & 0x38)
					timerStart(0x1a, 0x14);
				return;
			}
			if (stateTable[i].events & stateEvents) {
				stateNext = sub_2B3FB();
				powerState = stateNext | 0x8000;
				statePhase = 1;
				stateCause = event->transition;
				sub_2C572();
				stateNext = 0;
			}
			return;
		}
	}
}

/* the wake factors the next sleep may wake on (host command 0x00c0) */
void stateSetWakeMask(unsigned long mask)
{
	stateWakeMask = mask;
}

unsigned long stateGetWakeMask(void)
{
	return stateWakeMask;
}

void stateInit(void)
{
	unsigned long mask;

	stateRequest.byte[0] = 0x7f;
	stateRequest.byte[1] = 0xff;
	stateWakeupFactor.word = stateRequest.word;
	mask = 0xffffffff;
	mask |= sub_2C658()->wakeMaskOn;
	mask &= ~sub_2C658()->wakeMaskOff;
	if (sub_2C658()->wakeOff22)
		mask &= ~0x00400000UL;
	if (sub_2C658()->wakeOff15)
		mask &= ~0x00008000UL;
	stateSetWakeMask(mask);
}

/* host command 0x0010 (sceSysconGetWakeupFactor) */
union PowerRequest *stateGetWakeupFactor(void)
{
	return &stateWakeupFactor;
}

unsigned char stateGetCause(void)
{
	return stateCause;
}

unsigned int stateGet(void)
{
	return powerState;
}

/* 0 sent; 1 no block free; 2 an error */
int stateTransition(unsigned char transition)
{
	struct StateMsg *msg;
	ER ercd;
	struct StateEvent event;

	ercd = pget_mpf(1, (VP *)&msg);
	if (ercd == E_OK) {
		event.type = STATE_EVENT_TRANSITION;
		event.transition = transition;
		memcpy(&msg->event, &event, sizeof(event));
		sub_2C2C1(1, (T_MSG *)msg);
	} else if (ercd == E_TMOUT)
		return 1;
	else
		return 2;
	return 0;
}

int stateEnter(void)
{
	struct StateMsg *msg;
	ER ercd;
	struct StateEvent event;

	ercd = pget_mpf(1, (VP *)&msg);
	if (ercd == E_OK) {
		event.type = STATE_EVENT_ENTER;
		event.state = stateGet();
		memcpy(&msg->event, &event, sizeof(event));
		sub_2C2C1(1, (T_MSG *)msg);
	} else if (ercd == E_TMOUT)
		return 1;
	else
		return 2;
	return 0;
}

int stateCheck(void)
{
	struct StateMsg *msg;
	ER ercd;
	struct StateEvent event;

	ercd = pget_mpf(1, (VP *)&msg);
	if (ercd == E_OK) {
		event.type = STATE_EVENT_CHECK;
		memcpy(&msg->event, &event, sizeof(event));
		snd_mbx(1, (T_MSG *)msg);
	} else if (ercd == E_TMOUT)
		return 1;
	else
		return 2;
	return 0;
}
