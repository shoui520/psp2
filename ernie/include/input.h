/*
 * The inputs (app/input.c): the keys the scan latches for task 3, the power key's hold
 * that the RTC's interrupt counts.
 */
#ifndef ERNIE_INPUT_H
#define ERNIE_INPUT_H

struct InputKeys {
	unsigned int	state;		/* the keys held */
	unsigned int	events;		/* the keys the last scan tick latched for task 3 */
};

extern struct InputKeys inputKeys;

/* ticks of the scan left before each request */
struct InputDelays {
	unsigned char	flag40;		/* sets bit 0x0040 of event flag 2 */
	unsigned char	flag80;		/* sets bit 0x0080 of event flag 2 */
	unsigned char	motionInfo;	/* starts reading the motion sensor's information */
	unsigned char	reserved;	/* cleared by inputInit, otherwise unused */
	unsigned char	latchOn;	/* calls analogLatchOn */
};

extern struct InputDelays inputDelays;

/* a handler of an input bit: called when the bit changes to the levels in edges */
struct InputHandler {
	unsigned char	type;		/* 1: called on the edges below */
	unsigned char	edges;		/* INPUT_EDGE_* */
	void		(*handler)(void);
};

#define INPUT_EDGE_LOW		0x01	/* the bit became 0 */
#define INPUT_EDGE_HIGH		0x02	/* the bit became 1 */

/* the handlers of one bit of a group: count() of them, in a constant list (app/inputbits.c) */
struct InputBit {
	struct InputHandler * const	*handlers;
	unsigned char			(*count)(void);
};

extern struct InputHandler inputHandlerWake2;
extern struct InputHandler inputHandler1245B;
extern struct InputHandler inputHandler1245C;
extern struct InputHandler inputHandler25263;
extern struct InputHandler inputHandler2526B;
extern struct InputHandler inputHandlerSerial1;
extern struct InputHandler inputHandler224FD;
extern struct InputHandler inputHandler1245D;
extern struct InputHandler inputHandlerWake1;
extern struct InputHandler inputHandler29B68;
extern struct InputHandler inputHandler1275F;
extern struct InputHandler inputHandler12698;
extern struct InputHandler inputHandler12489;
extern __sreg unsigned char inputState1;
extern __near unsigned char holdTicks;
extern __near unsigned char holdReached;

extern void inputProcess(void);
extern unsigned char sub_12081(void);
extern void sub_125F2(void);
extern void sub_1267E(void);
extern unsigned char sub_12775(unsigned char ticks);
extern void sub_1278E(void);
extern void inputScanHold(unsigned int ticks);

#endif
