/*
 * The inputs (app/input.c): the keys the scan latches for task 3.
 */
#ifndef ERNIE_INPUT_H
#define ERNIE_INPUT_H

struct InputKeys {
	unsigned int	state;		/* the keys held */
	unsigned int	events;		/* the keys the last scan tick latched for task 3 */
};

extern struct InputKeys inputKeys;

extern void inputProcess(void);

#endif
