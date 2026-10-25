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
#include "charge.h"
#include "kernel.h"
#include "keepram.h"
#include "serial.h"
#include "serial2.h"
#include "serial4.h"
#include "serialauth.h"
#include "sleepfactor.h"
#include "state.h"
#include "tau.h"
#include "timer.h"
#include "updatereq.h"

extern __near unsigned int idRecordsLength;	/* the lengths of id records 0x0f, 0x10, 0x12
						   and 0x16 */
extern __near unsigned char idRecord1[];
extern void sub_2DAE1(int);
extern unsigned char sub_2DC1F(unsigned int id, unsigned char *data, unsigned char *length);
extern void sub_1164E(void);
extern void sub_116A1(void);
extern void sub_116CB(void);
extern void sub_1177B(void);
extern unsigned char sub_11D3B(void);
extern unsigned char sub_1267F(void);
extern unsigned char sub_1268B(void);
extern unsigned char sub_126C9(void);
extern int sub_23A40(void);
extern unsigned char sub_11CFB(void);
extern unsigned char sub_11D03(void);
extern void sub_11DE0(unsigned char enable);
extern void sub_11FEF(unsigned char enable);
extern int sub_12019(void);
extern int sub_1203A(void);
extern int sub_1204D(void);
extern int sub_1206E(void);
extern void sub_2C2C1(ID mbxid, T_MSG *msg);
extern void sub_2C315(ID flgid, FLGPTN setptn);
extern void sub_2C394(int, int);
extern unsigned int sub_2C561(void);
extern void sub_2C572(void);
extern void sub_2C573(struct StateEvent *event);

static int stateStartEnter(void);
static int stateOnEnter(void);
static int stateOffEnter(void);
static int stateResumeEnter(void);
static int stateSuspendEnter(void);
static int stateShutdownEnter(void);
static int stateCutEnter(void);
static int stateResetEnter(void);
static int stateUpdateAnswerEnter(void);
static int stateUpdateEnter(void);
static int stateStartHandle(struct StateEvent *event);
static int stateOnHandle(struct StateEvent *event);
static int stateOffHandle(struct StateEvent *event);
static int stateResumeHandle(struct StateEvent *event);
static int stateSuspendHandle(struct StateEvent *event);
static int stateShutdownHandle(struct StateEvent *event);
static int stateCutHandle(struct StateEvent *event);
static int stateResetHandle(struct StateEvent *event);
static int stateUpdateAnswerHandle(struct StateEvent *event);
static int stateUpdateHandle(struct StateEvent *event);
static unsigned int stateEventsOf(unsigned int state);
static unsigned int stateNextOf(void);

static const struct StateEntry stateTable[] = {
	{ STATE_START, 0x0002, stateStartEnter, stateStartHandle },
	{ STATE_ON, 0x0804, stateOnEnter, stateOnHandle },
	{ STATE_OFF, 0x0830, stateOffEnter, stateOffHandle },
	{ STATE_RESUME, 0x0e5c, stateResumeEnter, stateResumeHandle },
	{ STATE_SUSPEND, 0x0834, stateSuspendEnter, stateSuspendHandle },
	{ STATE_SHUTDOWN, 0x0802, stateShutdownEnter, stateShutdownHandle },
	{ STATE_CUT, 0x0002, stateCutEnter, stateCutHandle },
	{ STATE_RESET, 0x0830, stateResetEnter, stateResetHandle },
	{ STATE_UPDATE_ANSWER, 0x0834, stateUpdateAnswerEnter, stateUpdateAnswerHandle },
	{ STATE_UPDATE, 0x0000, stateUpdateEnter, stateUpdateHandle },
};

__sreg unsigned char statePhase;
__sreg unsigned int powerState;
__sreg unsigned int stateNext;
__sreg unsigned int stateEvents;
__sreg unsigned char stateCause;
static unsigned char stateReserved[6];
union PowerRequest stateWakeupFactor;
union PowerRequest stateRequest;
static unsigned long stateWakeMask;		/* a mode whose bit is set does not resume */

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
				stateEvents = stateEventsOf(powerState);
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
				stateNext = stateNextOf();
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
	if (sub_2C658()->wakeUnmask22)
		mask &= ~0x00400000UL;
	if (sub_2C658()->wakeUnmask15)
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
		event.arg.state = stateGet();
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

static int stateStartEnter(void)
{
	act_tsk(2);
	act_tsk(4);
	act_tsk(3);
	act_tsk(5);
	act_tsk(6);
	chg_pri(TSK_SELF, 3);
	stateTransition(0x60);
	return 0;
}

static int stateOnEnter(void)
{
	stateInit();
	stateTransition(0x32);
	return 0;
}

static int stateOffEnter(void)
{
	unsigned char cause;
	unsigned char length;
	unsigned char size;

	sub_1164E();
	serial4Answer0205();
	serialAnswer0105();
	tau0PowerOff();
	stateCheck();
	switch (stateCause) {
	case 0x31:
		sub_2DAE1(5);
		break;
	case 0x39:
		sub_2DAE1(0x17);
		break;
	case 0x35:
		sub_2DAE1(0x18);
		break;
	}
	cause = getResetCause();
	switch (cause) {
	case 0xf1:
	case 0x11:
		stateTransition(0x0a);
		break;
	}
	setResetCause(cause & 0xfe);
	if (stateCause == 0x58)
		stateTransition(0x0a);
	else if (stateCause == 0x31) {
		if (sub_2C658()->offTransition18 && sub_2C658()->offTransitionBlock == 0)
			stateTransition(0x18);
	} else if (stateCause == 0x5b)
		stateTransition(0x19);
	sub_2DC1F(1, (unsigned char *)idRecord1, &length);
	sub_2DC1F(0x0f, &size, &length);
	idRecordsLength = size;
	sub_2DC1F(0x10, &size, &length);
	idRecordsLength += size;
	sub_2DC1F(0x12, &size, &length);
	idRecordsLength += size;
	sub_2DC1F(0x16, &size, &length);
	idRecordsLength += size;
	return 0;
}

static int stateResumeEnter(void)
{
	union PowerRequest *factor;

	serial4Answer0205();
	serialAnswer0105();
	factor = stateGetWakeupFactor();
	if (factor->bits.notify)
		sub_2DAE1(2);
	else
		sub_2DAE1(1);
	return 0;
}

static int stateSuspendEnter(void)
{
	sub_1177B();
	tau0PowerOff();
	stateCheck();
	if (sleepFactorGet() & 0x00800000)
		sub_2DAE1(0x0b);
	if (stateCause == 0x59)
		stateTransition(0x0a);
	else if (stateCause == 0x5a)
		stateTransition(0x17);
	return 0;
}

static int stateShutdownEnter(void)
{
	sub_116A1();
	tau0PowerOff();
	stateCheck();
	return 0;
}

static int stateCutEnter(void)
{
	sub_116CB();
	tau0PowerOff();
	stateCheck();
	if (stateCause == 0x41)
		sub_2DAE1(8);
	return 0;
}

static int stateResetEnter(void)
{
	stateTransition(4);
	return 0;
}

static int stateUpdateAnswerEnter(void)
{
	return 0;
}

static int stateUpdateEnter(void)
{
	updaterStartDownloader();
	return 0;
}

static int stateStartHandle(struct StateEvent *event)
{
	if (event->transition == 0x60)
		stateNext = STATE_ON;
	return 0;
}

static int stateOnHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x32:
		stateNext = STATE_OFF;
		break;
	}
	return 0;
}

/* a transition below 0x20 resumes in its mode, unless the mode's wake factor is masked (its
   bit set in stateWakeMask) or the charge or the test area holds the system off */
static int stateOffHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x40:
		stateNext = STATE_SHUTDOWN;
		break;
	default:
		if ((event->transition & 0x7f) >= 0x20)
			break;
		if (sub_23A40() != 0) {
			stateNext = STATE_RESUME;
			stateRequest.bits.mode = 0x0d;
		} else if (serialAuthUnlocked0() != 0 || sub_11D3B() != 0) {
			stateNext = STATE_RESUME;
			stateRequest.bits.mode = 0x0c;
		} else if (event->transition == 0x11) {
			stateNext = STATE_RESUME;
			stateRequest.bits.mode = 0x11;
		} else if (event->transition == 0x0b) {
			stateNext = STATE_RESUME;
			stateRequest.bits.mode = 0x0b;
			stateRequest.bits.data = event->arg.data = 0;
		} else {
			stateNext = STATE_RESUME;
			stateRequest.bits.mode = event->transition & 0x7f;
		}
		stateRequest.bits.notify = 0;
		stateWakeMask |= sub_2C658()->wakeMaskOn;
		stateWakeMask &= ~sub_2C658()->wakeMaskOff;
		if (sub_2C658()->offWakeUnmask22)
			stateWakeMask &= ~0x00400000UL;
		if (stateRequest.bits.mode != 0 &&
		    ((1UL << stateRequest.bits.mode) & stateWakeMask))
			stateNext = 0;
		if (!(unsigned char)chargeMayStart() || (unsigned char)chargeAlarm0On() ||
		    (serial2TestAreaUsed() == 0 && !sub_2C658()->startCheckOff &&
		     serial2InTime() != 0))
			stateNext = 0;
		break;
	}
	return 0;
}

static int stateResumeHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x40:
		stateNext = STATE_SHUTDOWN;
		break;
	case 0x31:
	case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: case 0x38: case 0x39: case 0x3a:
	case 0x50:
	case 0x52: case 0x53:
	case 0x56: case 0x57: case 0x58:
	case 0x5b:
		stateNext = STATE_OFF;
		break;
	case 0x51:
	case 0x59: case 0x5a:
		stateNext = STATE_SUSPEND;
		break;
	case 0x54:
		stateNext = STATE_RESET;
		break;
	case 0x71:
		stateNext = STATE_UPDATE_ANSWER;
		break;
	case 0x70:
		stateNext = STATE_UPDATE;
		break;
	}
	return 0;
}

static int stateSuspendHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x40:
		stateNext = STATE_SHUTDOWN;
		break;
	case 0x31:
	case 0x34:
	case 0x37: case 0x38: case 0x39: case 0x3a:
	case 0x52:
	case 0x57:
		stateNext = STATE_OFF;
		break;
	}
	if ((event->transition & 0x7f) < 0x20) {
		stateNext = STATE_RESUME;
		stateRequest.bits.mode = event->transition & 0x7f;
		stateRequest.bits.notify = 1;
		stateWakeMask |= sub_2C658()->suspendWakeMaskOn;
		stateWakeMask &= ~sub_2C658()->suspendWakeMaskOff;
		if (stateRequest.bits.mode != 0 &&
		    ((1UL << stateRequest.bits.mode) & stateWakeMask))
			stateNext = 0;
		if (!(unsigned char)chargeMayStart() || (unsigned char)chargeAlarm0On())
			stateNext = 0;
	}
	return 0;
}

static int stateShutdownHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x61:
		if (sub_1268B())
			stateNext = STATE_ON;
		break;
	case 0x62:
		if (sub_126C9())
			stateNext = STATE_ON;
		break;
	}
	return 0;
}

static int stateCutHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x42:
	case 0x61:
		if (!sub_1267F() && sub_1268B())
			stateNext = STATE_ON;
		break;
	}
	return 0;
}

static int stateResetHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x40:
		stateNext = STATE_SHUTDOWN;
		break;
	case 0x04:
		stateNext = STATE_RESUME;
		stateRequest.bits.mode = event->transition & 0x7f;
		stateRequest.bits.notify = 0;
		break;
	}
	return 0;
}

static int stateUpdateAnswerHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x40:
		stateNext = STATE_SHUTDOWN;
		break;
	case 0x31:
	case 0x33: case 0x34: case 0x35:
	case 0x37: case 0x38: case 0x39: case 0x3a:
	case 0x50:
	case 0x52: case 0x53:
	case 0x56: case 0x57: case 0x58:
		stateNext = STATE_OFF;
		break;
	case 0x72:
		stateNext = STATE_RESUME;
		break;
	}
	return 0;
}

static int stateUpdateHandle(struct StateEvent *event)
{
	event = event;
	return 0;
}

/* the events that end the state: the off, shutdown and cut states' own, and every state's
   own bit, are not */
static unsigned int stateEventsOf(unsigned int state)
{
	unsigned int events;

	events = stateEvents;
	switch (state) {
	case STATE_CUT:
		events &= ~(state | 0x0e7f);
		break;
	case STATE_SHUTDOWN:
		events &= ~(state | 0x067f);
		break;
	case STATE_OFF:
		events &= ~(state | 0x066f);
		break;
	default:
		events &= ~state;
		break;
	}
	return events;
}

/* the cut, shutdown and off states first, then the lowest bit's (Sony's leaves next unset
   when no event is set) */
static unsigned int stateNextOf(void)
{
	unsigned int events;
	unsigned int next;
	unsigned int i;

	events = stateEvents & 0x0e7f;
	if (events & STATE_CUT)
		next = STATE_CUT;
	else if (events & STATE_SHUTDOWN)
		next = STATE_SHUTDOWN;
	else if (events & STATE_OFF)
		next = STATE_OFF;
	else {
		for (i = 0; i < 15; i++) {
			if ((1 << i) & events)
				next = 1 << i;
		}
	}
	return next;
}

/* input handler: input 1.4 (sub_11CFB) wakes a suspended system; its release with button 2
   not pressed and input 1.6 up starts an off one */
void stateInputWake2(void)
{
	unsigned int state;

	state = stateGet();
	if (sub_11CFB()) {
		if (state == STATE_SUSPEND)
			stateTransition(0);
		sub_1204D();
		sub_11DE0(1);
	} else {
		if (!(unsigned char)sub_1206E() && !sub_11D03() && state == STATE_OFF) {
			stateTransition(0);
			serial2Arm();
		}
		sub_1204D();
	}
}

/* input handler: input 1.6 (sub_11D03) wakes a suspended system by transition 0x12 */
void stateInputWake1(void)
{
	unsigned int state;

	state = stateGet();
	if (sub_11D03()) {
		if (state == STATE_SUSPEND)
			stateTransition(0x12);
		sub_12019();
		sub_11FEF(1);
	} else {
		if ((unsigned char)sub_1203A()) {
		}
		sub_12019();
	}
}

/* serial mode 0's unlock (serialAuthUnlock0): an off system starts by transition 0x0c */
unsigned char stateSerialUnlock(void)
{
	if (stateGet() != STATE_OFF)
		return 1;
	return (unsigned char)stateTransition(0x0c);
}

/* and its lock: transition 0x34 */
int stateSerialLock(void)
{
	return (unsigned char)stateTransition(0x34);
}
