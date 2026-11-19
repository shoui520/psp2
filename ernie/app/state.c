/*
 * The system state: task 1 hands each event of mailbox 1 to the entry of powerState in
 * stateTable. An entry's events, once one of them is set, start the next state (its bit 15
 * set until the power sequence ran it, power.c).
 */
#pragma sfr
#pragma DI

#include <string.h>

#include "ernie.h"
#include "board.h"
#include "config.h"
#include "host.h"
#include "input.h"
#include "charge.h"
#include "kernel.h"
#include "keepram.h"
#include "led.h"
#include "log.h"
#include "serial.h"
#include "serial2.h"
#include "serial4.h"
#include "serialauth.h"
#include "sleepfactor.h"
#include "state.h"
#include "sysinfo.h"
#include "task.h"
#include "tau.h"
#include "timer.h"
#include "updatereq.h"

/* the reset's transition: a resume in this mode (0x00090009's 0x55) */
#if FW_SINCE(0x00090206)
#define STATE_RESET_TRANSITION	0x04
#else
#define STATE_RESET_TRANSITION	0x55
#endif

extern void inputStateOff(void);
extern void inputStateShutdown(void);
extern void inputStateCut(void);
extern void inputStateSuspend(void);
extern unsigned char input3Bit2Active(void);
extern unsigned char levelMonitor2Low(void);
extern unsigned char levelMonitor1High(void);
extern unsigned char input1Bit7Active(void);
extern int powerFaulty(void);
extern unsigned char input1Bit4Active(void);
extern unsigned char input1Bit6Active(void);
extern void pinMonitor1Enable(unsigned char enable);
extern void pinMonitor0Enable(unsigned char enable);
extern int button1Reset(void);
extern int button1Result(void);
extern int button2Reset(void);
extern int button2Result(void);
#if !FW_SINCE(0x00090206)
extern int button1State(void);
extern int button2State(void);
#endif

static int stateStartEnter(void);
static int stateOnEnter(void);
static int stateOffEnter(void);
static int stateResumeEnter(void);
static int stateSuspendEnter(void);
static int stateShutdownEnter(void);
static int stateCutEnter(void);
static int stateResetEnter(void);
#if FW_SINCE(0x00090903)
static int stateUpdateAnswerEnter(void);
static int stateUpdateEnter(void);
#endif
static int stateStartHandle(struct StateEvent *event);
static int stateOnHandle(struct StateEvent *event);
static int stateOffHandle(struct StateEvent *event);
static int stateResumeHandle(struct StateEvent *event);
static int stateSuspendHandle(struct StateEvent *event);
static int stateShutdownHandle(struct StateEvent *event);
static int stateCutHandle(struct StateEvent *event);
static int stateResetHandle(struct StateEvent *event);
#if FW_SINCE(0x00090903)
static int stateUpdateAnswerHandle(struct StateEvent *event);
static int stateUpdateHandle(struct StateEvent *event);
#endif
static unsigned int stateEventsOf(unsigned int state);
static unsigned int stateNextOf(void);
#if BOARD_USS1001
static int stateModeAtLevel9(union PowerRequest *request);
#endif

/* the input's part of a state: on the slim task 3 runs it (inputStateRun) */
#if BOARD_USS1001
#define stateInputOff()		inputStateRequest(2)
#define stateInputSuspend()	inputStateRequest(4)
#define stateInputCut()		inputStateRequest(8)
#define stateInputShutdown()	inputStateRequest(10)
#else
#define stateInputOff()		inputStateOff()
#define stateInputSuspend()	inputStateSuspend()
#define stateInputCut()		inputStateCut()
#define stateInputShutdown()	inputStateShutdown()
#endif

/* the transitions below it are wakes, a resume's mode (the slim's has two more) */
#if BOARD_USS1001
#define STATE_WAKE_TRANSITIONS	0x22
#else
#define STATE_WAKE_TRANSITIONS	0x20
#endif

/* a message sent to task 1: on the slim 3 when the mailbox refuses it */
#if BOARD_USS1001
#define STATE_SEND(send)	do { ercd = send; if (ercd != E_OK) return 3; } while (0)
#else
#define STATE_SEND(send)	send
#endif

/* the updater's states' events (0x00090804 has no updater states) */
#if FW_SINCE(0x00090903)
#define STATE_UPDATES	(STATE_UPDATE_ANSWER | STATE_UPDATE)
#else
#define STATE_UPDATES	0
#endif

static const struct StateEntry stateTable[] = {
	{ STATE_START, 0x0002, stateStartEnter, stateStartHandle },
	{ STATE_ON, 0x0804, stateOnEnter, stateOnHandle },
	{ STATE_OFF, 0x0830, stateOffEnter, stateOffHandle },
	{ STATE_RESUME, 0x085c | STATE_UPDATES, stateResumeEnter, stateResumeHandle },
	{ STATE_SUSPEND, 0x0834, stateSuspendEnter, stateSuspendHandle },
	{ STATE_SHUTDOWN, 0x0802, stateShutdownEnter, stateShutdownHandle },
	{ STATE_CUT, 0x0002, stateCutEnter, stateCutHandle },
	{ STATE_RESET, 0x0830, stateResetEnter, stateResetHandle },
#if FW_SINCE(0x00090903)
	{ STATE_UPDATE_ANSWER, 0x0834, stateUpdateAnswerEnter, stateUpdateAnswerHandle },
#if BOARD_USS1001
	{ STATE_UPDATE, 0x0814, stateUpdateEnter, stateUpdateHandle },
#else
	{ STATE_UPDATE, 0x0000, stateUpdateEnter, stateUpdateHandle },
#endif
#endif
};

#if BOARD_USS1001
__sreg struct StateRecord stateRecord;
struct StateRecord stateHistory[2];	/* the last two states entered, the last at
					   stateRecord.count & 1 */
#define stateRecordSave()	memcpy(&stateHistory[stateRecord.count & 1], &stateRecord, \
				       sizeof(stateRecord))
#else
__sreg unsigned char statePhase;
__sreg unsigned int powerState;
__sreg unsigned int stateNext;
__sreg unsigned int stateEvents;
#if FW_SINCE(0x00090903)
__sreg unsigned char stateCause;
#else
__sreg unsigned int stateCause;
#endif
static unsigned char stateReserved[6];
#endif
union PowerRequest stateWakeupFactor;
union PowerRequest stateRequest;
#if BOARD_USS1001
static struct StateMsg stateEnterMsg;		/* stateEnter's */
#endif
static unsigned long stateWakeMask;		/* a mode whose bit is set does not resume */

#if FW_SINCE(0x01000104)
#define STATE_TRANSITION_WAKE(t)	stateTransition(t)
#else
/* the input wakes put the mask in their event themselves */
#define STATE_TRANSITION_WAKE(t)	(transitionEvent.transition = (t), \
					 transitionEvent.wakeMask = stateWakeMask, \
					 stateTransition(&transitionEvent))
#endif

#if !BOARD_USS1001	/* not in the slim's */
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
#endif

/* task 1's, for each event of mailbox 1 */
void stateEvent(struct StateEvent *event)
{
	unsigned char i;
	unsigned int status;

	for (i = 0; i < sizeof(stateTable) / sizeof(stateTable[0]); i++) {
		if (stateTable[i].state == powerState) {
			switch (event->type) {
			case STATE_EVENT_ENTER:
#if BOARD_USS1001
				stateRecord.count++;
				if (statePhase != 3)
					stateTable[i].enter();
				stateRecordSave();
				taskHookState();
				statePhase = 0;
				stateEvents = stateEventsOf(powerState);
#else
				stateTable[i].enter();
				statePhase = 0;
				stateEvents = stateEventsOf(powerState);
				taskHookState();
#endif
				break;
			case STATE_EVENT_TRANSITION:
				stateTable[i].handle(event);
				stateEvents |= stateNext;
				taskHookEvent(event);
				if (statePhase != 0)
					return;
				break;
			case STATE_EVENT_CHECK:
				status = taskGetPending();
				if (status & 0x20)
					task6SetFlag(6, 0x4000);
				if (status & 0x10)
					task5SetFlag(4, 0x8000);
#if BOARD_USS1001
				if (status & 0x04)
					task3SetFlag(2, 0x8000);
				if (status & 0x3c)
					timerStart(TIMER_RTC(0x1a), 0x14);
#else
				if (status & 0x38)
					timerStart(TIMER_RTC(0x1a), 0x14);
#endif
				return;
			}
			if (stateEvents & stateTable[i].events) {
				stateNext = stateNextOf();
				powerState = stateNext | 0x8000;
				statePhase = 1;
#if FW_SINCE(0x00090009)	/* 0x00080504 keeps no cause */
				stateCause = event->transition;
#endif
				taskHookState();
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
#if FW_SINCE(0x00090009)
	unsigned long mask;
#endif

	stateRequest.byte[0] = 0x7f;
	stateRequest.byte[1] = 0xff;
	stateWakeupFactor.word = stateRequest.word;
#if FW_SINCE(0x00090804)
	mask = 0xffffffff;
	mask |= configGet()->BOARD_WAKE_MASKS.on;
	mask &= ~configGet()->BOARD_WAKE_MASKS.off;
	if (configGet()->start.bits.wakeUnmask22)
		mask &= ~0x00400000UL;
#elif !FW_SINCE(0x00090009)
	/* 0x00080504 wakes on everything */
	stateSetWakeMask(0xffffffff);
#else
	/* 0x00090206's configuration has no wake masks */
	if (configGet()->start.bits.wakeUnmask22)
		mask = ~0x00400000UL;
	else
		mask = 0xffffffff;
#endif
#if FW_SINCE(0x01000104)
	if (configGet()->start.bits.wakeUnmask15)
		mask &= ~0x00008000UL;
#endif
#if FW_SINCE(0x00090009)
	stateSetWakeMask(mask);
#endif
}

#if BOARD_USS1001
/* the slim's: the state record (serial 0x0109, mode 0) */
void stateGetRecord(unsigned char *out)
{
	memcpy(out, &stateRecord, sizeof(stateRecord));
}

/* the slim's: the last two states' records (serial 0x0109, mode 1) */
void stateGetHistory(unsigned char *out)
{
	memcpy(out, stateHistory, sizeof(stateHistory));
}
#endif

/* host command 0x0010 (sceSysconGetWakeupFactor) */
union PowerRequest *stateGetWakeupFactor(void)
{
	return &stateWakeupFactor;
}

#if FW_SINCE(0x00090903)
unsigned char stateGetCause(void)
{
	return stateCause;
}
#endif

unsigned int stateGet(void)
{
	return powerState;
}

/* 0 sent; 1 no block free; 2 an error */
#if FW_SINCE(0x01000104)
#if BOARD_USS1001
int stateTransition(unsigned int transition)
#else
int stateTransition(unsigned char transition)
#endif
{
	struct StateMsg *msg;
	ER ercd;
	struct StateEvent event;

	ercd = pget_mpf(1, (VP *)&msg);
	if (ercd == E_OK) {
		event.type = STATE_EVENT_TRANSITION;
		event.transition = transition;
		memcpy(&msg->event, &event, sizeof(event));
		STATE_SEND(task1SendMessage(1, (T_MSG *)msg));
	} else if (ercd == E_TMOUT)
		return 1;
	else
		return 2;
	return 0;
}
#else
int stateTransition(struct StateEvent *event)
{
	struct StateMsg *msg;
	ER ercd;

	ercd = pget_mpf(1, (VP *)&msg);
	if (ercd == E_OK) {
		event->type = STATE_EVENT_TRANSITION;
		memcpy(&msg->event, event, sizeof(*event));
		task1SendMessage(1, (T_MSG *)msg);
	} else if (ercd == E_TMOUT)
		return 1;
	else
		return 2;
	return 0;
}
#endif

int stateEnter(void)
{
	struct StateMsg *msg;
	ER ercd;
	struct StateEvent event;

#if BOARD_USS1001
	msg = &stateEnterMsg;			/* the slim's: its own message, not the pool's */
	ercd = E_OK;
#else
	ercd = pget_mpf(1, (VP *)&msg);
#endif
	if (ercd == E_OK) {
		event.type = STATE_EVENT_ENTER;
		event.arg.state = stateGet();
		memcpy(&msg->event, &event, sizeof(event));
		STATE_SEND(task1SendMessage(1, (T_MSG *)msg));
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
		STATE_SEND(snd_mbx(1, (T_MSG *)msg));
	} else if (ercd == E_TMOUT)
		return 1;
	else
		return 2;
	return 0;
}

static int stateStartEnter(void)
{
	STATE_TRANSITION_EVENT
	act_tsk(2);
	act_tsk(4);
	act_tsk(3);
	act_tsk(5);
	act_tsk(6);
	chg_pri(TSK_SELF, 3);
	STATE_TRANSITION_NOMASK(0x60);
	return 0;
}

static int stateOnEnter(void)
{
	STATE_TRANSITION_EVENT
	stateInit();
#if !FW_SINCE(0x00090903)
	transitionEvent.type = STATE_EVENT_TRANSITION;
#endif
	STATE_TRANSITION_NOMASK(0x32);
	return 0;
}

static int stateOffEnter(void)
{
#if FW_SINCE(0x01000104)
	STATE_TRANSITION_EVENT
	unsigned char cause;
#endif
#if FW_SINCE(0x01000107) && !BOARD_USS1001
	unsigned char length;
	unsigned char size;
#endif

	stateInputOff();
#if !BOARD_USS1001
	serial4Answer0205();
#endif
	serialAnswer0105();
	tau0PowerOff();
	stateCheck();
#if FW_SINCE(0x00090903)
	switch (stateCause) {
	case 0x31:
		logEvent(5);
		break;
	case 0x39:
		logEvent(0x17);
		break;
	case 0x35:
		logEvent(0x18);
		break;
	}
#elif FW_SINCE(0x00090804)	/* 0x00090206 has no log */
	if (stateCause == 0x31)
		logEvent(5);
#endif
#if FW_SINCE(0x01000104)
	cause = getResetCause();
	switch (cause) {
	case 0xf1:
	case 0x11:
		STATE_TRANSITION(0x0a);
		break;
#if BOARD_USS1001
	case 0x51:
		STATE_TRANSITION(0x1b);
		/* falls through */
	case 0x61:
		STATE_TRANSITION(0x18);
		break;
#endif
	}
	setResetCause(cause & 0xfe);
	if (stateCause == 0x58)
		STATE_TRANSITION(0x0a);
#if FW_SINCE(0x01000306)
	else if (stateCause == 0x31) {
		if (configGet()->offTransition18 && configGet()->offTransitionBlock == 0)
			STATE_TRANSITION(0x18);
	}
#if BOARD_USS1001
	else if (stateCause == 0x68) {		/* the slim's: a reset with cause 0x61 */
		dis_dsp();
		DI();
		setResetCause(0x61);
		resetSystem();
	}
#endif
	else if (stateCause == 0x5b)
		STATE_TRANSITION(0x19);
#if BOARD_USS1001
	else if (stateCause == 0x5d) {		/* and with cause 0x51 */
		dis_dsp();
		DI();
		setResetCause(0x51);
		resetSystem();
	}
#endif
#endif
#endif
#if FW_SINCE(0x01000107) && !BOARD_USS1001
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
	return 0;
}

static int stateResumeEnter(void)
{
#if FW_SINCE(0x00090804)
	union PowerRequest *factor;

#endif
	serial4Answer0205();
	serialAnswer0105();
#if FW_SINCE(0x00090804)
	factor = stateGetWakeupFactor();
	if (factor->bits.notify)
		logEvent(2);
	else
		logEvent(1);
#endif
	return 0;
}

static int stateSuspendEnter(void)
{
#if FW_SINCE(0x01000104)
	STATE_TRANSITION_EVENT
#endif
	stateInputSuspend();
	tau0PowerOff();
	stateCheck();
#if FW_SINCE(0x00090903)
	if (sleepFactorGet() & 0x00800000)
		logEvent(0x0b);
#endif
#if FW_SINCE(0x01000104)
	if (stateCause == 0x59)
		STATE_TRANSITION(0x0a);
	else if (stateCause == 0x5a)
		STATE_TRANSITION(0x17);
#endif
	return 0;
}

static int stateShutdownEnter(void)
{
	stateInputShutdown();
	tau0PowerOff();
	stateCheck();
	return 0;
}

static int stateCutEnter(void)
{
	stateInputCut();
	tau0PowerOff();
	stateCheck();
#if BOARD_USS1001
	setResetCause(0x20);
#endif
#if FW_SINCE(0x00090804)
	if (stateCause == 0x41)
		logEvent(8);
#endif
	return 0;
}

static int stateResetEnter(void)
{
#if FW_SINCE(0x01000104)
	STATE_TRANSITION_EVENT
	STATE_TRANSITION(4);
#else
	/* before 0x01000104 the event goes with no wake mask */
	struct StateEvent event;

	event.type = STATE_EVENT_TRANSITION;
	event.transition = STATE_RESET_TRANSITION;
	stateTransition(&event);
#endif
	return 0;
}

#if FW_SINCE(0x00090903)
static int stateUpdateAnswerEnter(void)
{
	return 0;
}

static int stateUpdateEnter(void)
{
#if BOARD_USS1001
	stateRecordSave();			/* the slim's: recorded before the downloader */
	taskHookState();
#endif
	updaterStartDownloader();
	return 0;
}
#endif

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
#if FW_SINCE(0x0100060B)
	case 0x43:
#endif
		stateNext = STATE_CUT;
		break;
	case 0x40:
		stateNext = STATE_SHUTDOWN;
		break;
	default:
		if ((event->transition & 0x7f) >= STATE_WAKE_TRANSITIONS)
			break;
#if BOARD_USS1001
		if (serialAuthUnlocked0() != 0) {	/* the slim's: no fault, no input 3 */
#else
		if (powerFaulty() != 0) {
			stateNext = STATE_RESUME;
			stateRequest.bits.mode = 0x0d;
		} else if (serialAuthUnlocked0() != 0 || input3Bit2Active() != 0) {
#endif
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
#if FW_SINCE(0x00090804)
		stateWakeMask |= configGet()->BOARD_WAKE_MASKS.on;
		stateWakeMask &= ~configGet()->BOARD_WAKE_MASKS.off;
#else
		/* 0x00090206 has the masks in its code */
		/* bit 13 wakes the other boards (0x00315200 too) */
		stateWakeMask |= BOARD_IS(0x00411000) && !HW_IS(0x00315200) ? 0xffa0057cUL : 0xffa0257cUL;
		stateWakeMask &= BOARD_IS(0x00411000) && !HW_IS(0x00315200) ? ~0x001e7a01UL : ~0x001e5a01UL;
#endif
#if FW_SINCE(0x01000104)
		if (configGet()->state.bits.offWakeUnmask22)
			stateWakeMask &= ~0x00400000UL;
#endif
		if (stateRequest.bits.mode != 0 &&
		    ((1UL << stateRequest.bits.mode) & stateWakeMask))
			stateNext = 0;
#if BOARD_USS1001
		/* the slim's: at battery level 9 only some modes resume, and while no alarm the
		   power LED shows it */
		if (!(unsigned char)chargeMayStart() || (unsigned char)chargeAlarm0On())
			stateNext = 0;
		if (chargeBatteryLevel9() && stateModeAtLevel9(&stateRequest) == 0)
			stateNext = 0;
		if (chargeBatteryLevel9() && !(unsigned char)chargeAlarm0On() &&
		    stateModeAtLevel9(&stateRequest) != 0)
			powerLedSetFlags(0x80);
#elif FW_SINCE(0x01000104)
		if (!(unsigned char)chargeMayStart() || (unsigned char)chargeAlarm0On() ||
		    (serial2TestAreaUsed() == 0 && !configGet()->test.bits.startCheckOff &&
		     serial2InTime() != 0))
			stateNext = 0;
#else
		if (!(unsigned char)chargeMayStart())
			stateNext = 0;
#endif
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
#if FW_SINCE(0x00090903)
	case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
#else
	case 0x32:
#endif
#if FW_SINCE(0x01000104)
	case 0x3a:
#endif
	case 0x50:
	case 0x52: case 0x53:
#if FW_SINCE(0x00090009)
	case 0x56:
#endif
#if FW_SINCE(0x01000104)
	case 0x57: case 0x58:
#endif
#if FW_SINCE(0x01000306)
	case 0x5b:
#endif
#if FW_SINCE(0x0100060B)
	case 0x5c:
#endif
#if BOARD_USS1001
	case 0x5d: case 0x68:			/* the slim's resets (stateOffEnter) */
#endif
		stateNext = STATE_OFF;
		break;
	case 0x51:
#if FW_SINCE(0x01000104)
	case 0x59: case 0x5a:
#endif
		stateNext = STATE_SUSPEND;
		break;
	case 0x54:
		stateNext = STATE_RESET;
		break;
#if FW_SINCE(0x00090903)
	case 0x71:
		stateNext = STATE_UPDATE_ANSWER;
		break;
	case 0x70:
		stateNext = STATE_UPDATE;
		break;
#endif
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
#if FW_SINCE(0x00090903)
	case 0x34:
	case 0x37: case 0x38: case 0x39:
#else
	case 0x32:
#endif
#if FW_SINCE(0x01000104)
	case 0x3a:
	case 0x52:
	case 0x57:
#endif
#if FW_SINCE(0x0100060B)
	case 0x5c:
#endif
		stateNext = STATE_OFF;
		break;
	}
	if ((event->transition & 0x7f) < STATE_WAKE_TRANSITIONS) {
		stateNext = STATE_RESUME;
		stateRequest.bits.mode = event->transition & 0x7f;
		stateRequest.bits.notify = 1;
#if FW_SINCE(0x00090804)
		stateWakeMask |= configGet()->BOARD_WAKE_MASKS.suspendOn;
		stateWakeMask &= ~configGet()->BOARD_WAKE_MASKS.suspendOff;
		if (stateRequest.bits.mode != 0 &&
		    ((1UL << stateRequest.bits.mode) & stateWakeMask))
			stateNext = 0;
#if FW_SINCE(0x01000104)
		if (!(unsigned char)chargeMayStart() || (unsigned char)chargeAlarm0On())
#else
		if (!(unsigned char)chargeMayStart())
#endif
			stateNext = 0;
#if BOARD_USS1001
		if (chargeBatteryLevel9() && stateModeAtLevel9(&stateRequest) == 0)
			stateNext = 0;
		if (chargeBatteryLevel9() && !(unsigned char)chargeAlarm0On() &&
		    stateModeAtLevel9(&stateRequest) != 0)
			powerLedSetFlags(0x80);
#endif
	}
#else
	}
	/* 0x00090206 sets the masks and checks the request at every event */
	stateWakeMask |= BOARD_IS(0x00411000) && !HW_IS(0x00315200) ? 0xffa21c10UL : 0xffa23d10UL;
	stateWakeMask &= BOARD_IS(0x00411000) && !HW_IS(0x00315200) ? ~0x001c6001UL : ~0x001c4001UL;
	if (stateRequest.bits.mode != 0 &&
	    ((1UL << stateRequest.bits.mode) & stateWakeMask))
		stateNext = 0;
	if (!(unsigned char)chargeMayStart())
		stateNext = 0;
#endif
	return 0;
}

static int stateShutdownHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x41:
		stateNext = STATE_CUT;
		break;
	case 0x61:
		if (levelMonitor1High())
			stateNext = STATE_ON;
		break;
#if FW_SINCE(0x00090804)	/* 0x00090206 has no transition 0x62 */
	case 0x62:
		if (input1Bit7Active())
			stateNext = STATE_ON;
		break;
#endif
	}
	return 0;
}

static int stateCutHandle(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x42:
	case 0x61:
		if (!levelMonitor2Low() && levelMonitor1High())
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
	case STATE_RESET_TRANSITION:
		stateNext = STATE_RESUME;
		stateRequest.bits.mode = event->transition & 0x7f;
#if FW_SINCE(0x00090206)
		stateRequest.bits.notify = 0;
#endif
		break;
	}
	return 0;
}

#if FW_SINCE(0x00090903)
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
	case 0x37: case 0x38: case 0x39:
#if FW_SINCE(0x01000104)
	case 0x3a:
#endif
	case 0x50:
	case 0x52: case 0x53:
	case 0x56:
#if FW_SINCE(0x01000104)
	case 0x57: case 0x58:
#endif
#if BOARD_USS1001
	case 0x5b: case 0x5d: case 0x68:
#endif
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
#if BOARD_USS1001
	/* the slim's: the updater's state ends as the answer's does, without its resume */
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
	case 0x5b: case 0x5d: case 0x68:
		stateNext = STATE_OFF;
		break;
	}
#else
	event = event;
#endif
	return 0;
}
#endif

/* the events that end the state: the off, shutdown and cut states' own, and every state's
   own bit, are not */
static unsigned int stateEventsOf(unsigned int state)
{
	unsigned int events;

	events = stateEvents;
	switch (state) {
	case STATE_CUT:
		events &= ~(state | 0x0a7f | STATE_UPDATES);
		break;
	case STATE_SHUTDOWN:
		events &= ~(state | 0x027f | STATE_UPDATES);
		break;
	case STATE_OFF:
		events &= ~(state | 0x026f | STATE_UPDATES);
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

	events = stateEvents & (0x0a7f | STATE_UPDATES);
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

/* input handler: input 1.4 (input1Bit4Active) wakes a suspended system; its release with button 2
   not pressed and input 1.6 up starts an off one */
#if !FW_SINCE(0x00090206)
/* 0x00090009: released with no press counted, in the state the press began in (not resuming),
   the system wakes by transition 0 */
void stateInputWake2(void)
{
	STATE_TRANSITION_EVENT

	if (input1Bit4Active()) {
		button2Reset();
		pinMonitor1Enable(1);
	} else {
		if (!(unsigned char)button2Result() && !input1Bit6Active() &&
		    stateGet() != STATE_RESUME && stateGet() == button2State())
			STATE_TRANSITION_WAKE(0);
		button2Reset();
	}
}

/* and input 1.6 by transition 0x12 */
void stateInputWake1(void)
{
	STATE_TRANSITION_EVENT

	if (input1Bit6Active()) {
		button1Reset();
		pinMonitor0Enable(1);
	} else {
		if (!(unsigned char)button1Result() && stateGet() != STATE_RESUME &&
		    stateGet() == button1State())
			STATE_TRANSITION_WAKE(0x12);
		button1Reset();
	}
}
#else
void stateInputWake2(void)
{
	STATE_TRANSITION_EVENT
	unsigned int state;

	state = stateGet();
	if (input1Bit4Active()) {
		if (state == STATE_SUSPEND)
			STATE_TRANSITION_WAKE(0);
		button2Reset();
		pinMonitor1Enable(1);
	} else {
#if BOARD_USS1001
		/* the slim starts nothing here, and does not look at the state */
		if (!(unsigned char)button2Result() && !input1Bit6Active()) {
		}
#else
		if (!(unsigned char)button2Result() && !input1Bit6Active() && state == STATE_OFF) {
#if BOARD_SERIAL2
			STATE_TRANSITION_WAKE(0);
#if SERIAL2_ARM
			serial2Arm();
#endif
#endif
		}
#endif
		button2Reset();
	}
}

/* input handler: input 1.6 (input1Bit6Active) wakes a suspended system by transition 0x12 */
void stateInputWake1(void)
{
	STATE_TRANSITION_EVENT
	unsigned int state;

	state = stateGet();
	if (input1Bit6Active()) {
		if (state == STATE_SUSPEND)
			STATE_TRANSITION_WAKE(0x12);
		button1Reset();
		pinMonitor0Enable(1);
	} else {
		if ((unsigned char)button1Result()) {
		}
		button1Reset();
	}
}
#endif

/* serial mode 0's unlock (serialAuthUnlock0): an off system starts by transition 0x0c */
unsigned char stateSerialUnlock(void)
{
	STATE_TRANSITION_EVENT
#if FW_SINCE(0x00090804)	/* 0x00090206 starts from any state */
	if (stateGet() != STATE_OFF)
		return 1;
#endif
	return (unsigned char)STATE_TRANSITION_NOMASK(0x0c);
}

/* and its lock: transition 0x34 (0x00090804's 0x32) */
int stateSerialLock(void)
{
	STATE_TRANSITION_EVENT
#if FW_SINCE(0x00090903)
	return (unsigned char)STATE_TRANSITION_NOMASK(0x34);
#else
	return (unsigned char)STATE_TRANSITION_NOMASK(0x32);
#endif
}

#if BOARD_USS1001
/* the slim's: the modes a resume keeps at battery level 9 */
static int stateModeAtLevel9(union PowerRequest *request)
{
	return request->bits.mode == 0 || request->bits.mode == 0x14 ||
	       request->bits.mode == 0x12 || request->bits.mode == 0x18 ||
	       request->bits.mode == 0x0b || request->bits.mode == 0x0f;
}
#endif
