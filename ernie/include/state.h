/*
 * The system state (app/state.c): powerState, which task 1 changes as the events of mailbox 1
 * ask (stateEvent), each state's table entry, and the wake factors.
 */
#ifndef ERNIE_STATE_H
#define ERNIE_STATE_H

#include "kernel.h"

/* the request powerResume runs (stateRequest), and the wakeup factor it leaves: the mode picks
   its last step */
union PowerRequest {
	unsigned int	word;
	unsigned char	byte[2];
	struct {
		unsigned char	mode:7;
		unsigned char	notify:1;	/* set: event 0x08 of flag 4; and a button's
						   presses go on being counted across a state
						   change */
		unsigned char	data;
	} bits;
};

/* an event of mailbox 1 */
struct StateEvent {
	unsigned char	type;			/* STATE_EVENT_* */
	unsigned char	transition;		/* STATE_EVENT_TRANSITION's */
	union {
		unsigned int	state;		/* STATE_EVENT_ENTER's: powerState when sent */
		unsigned char	data;		/* transition 0x0b's, cleared by the off state */
	} arg;
	unsigned long	wakeMask;		/* the updater's transitions: stateGetWakeMask's */
};

#define STATE_EVENT_ENTER	1	/* the state's enter, and its events */
#define STATE_EVENT_TRANSITION	2	/* the state's handler of a transition */
#define STATE_EVENT_CHECK	3

struct StateMsg {
	T_MSG		header;
	struct StateEvent event;
};

/* the states: powerState, bit 15 set while the power sequence of the state runs */
#define STATE_START		0x0001
#define STATE_ON		0x0002
#define STATE_OFF		0x0004
#define STATE_SUSPEND		0x0008
#define STATE_SHUTDOWN		0x0010
#define STATE_RESUME		0x0020
#define STATE_RESET		0x0040
#define STATE_UPDATE_ANSWER	0x0200
#define STATE_UPDATE		0x0400
#define STATE_CUT		0x0800

/* a state: its handlers, and the events that end it */
struct StateEntry {
	unsigned int	state;
	unsigned int	events;
	int		(*enter)(void);
	int		(*handle)(struct StateEvent *event);
};

extern __sreg unsigned char statePhase;	/* the phase: 1 to run, 2 done, 3 failed */
extern __sreg unsigned int powerState;
extern __sreg unsigned int stateNext;
extern __sreg unsigned int stateEvents;
extern __sreg unsigned char stateCause;	/* the transition that started the state */
extern union PowerRequest stateWakeupFactor;
extern union PowerRequest stateRequest;

extern void stateHostStop(void);
extern void stateHostStart(void);
extern void stateEvent(struct StateEvent *event);
extern void stateSetWakeMask(unsigned long mask);
extern unsigned long stateGetWakeMask(void);
extern void stateInit(void);
extern union PowerRequest *stateGetWakeupFactor(void);
extern unsigned char stateGetCause(void);
extern unsigned int stateGet(void);
extern int stateTransition(unsigned char transition);
extern int stateEnter(void);
extern int stateCheck(void);
extern void stateInputWake2(void);
extern void stateInputWake1(void);
extern unsigned char stateSerialUnlock(void);
extern int stateSerialLock(void);

#endif
