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
	unsigned char	call1AFD8;
	unsigned char	reserved;	/* cleared by inputInit, otherwise unused */
	unsigned char	call1D0CB;
};

extern struct InputDelays inputDelays;
extern __sreg unsigned char inputState1;
extern __near unsigned char holdTicks;
extern __near unsigned char holdReached;

extern void inputProcess(void);
extern unsigned char sub_12081(void);
extern void sub_125F2(void);
extern void sub_1267E(void);
extern unsigned char sub_12775(unsigned char ticks);
extern void sub_1278E(void);

#endif
