/*
 * The serial link's mode 1 (app/serial1.c): the commands of serialCommands1, found by
 * serialCommand1, and the notifications of serialNotifies1.
 */
#ifndef ERNIE_SERIAL1_H
#define ERNIE_SERIAL1_H

#include "ernie.h"
#include "board.h"
#include "serial.h"
#include "task.h"
#if BOARD_DOL1001
#include "thermal.h"
#endif

/* a command's flags: what it needs */
#define SERIAL1_UNLOCKED	0x01	/* the link unlocked (0x0103), else status 0x20 */
#define SERIAL1_NVS_KEY2	0x02	/* serialAuthUnlocked1, else status 0x10 */
#define SERIAL1_KEY		0x04	/* serial1Key at SERIAL1_KEY_OPEN, else status 1 */
#define SERIAL1_NVS_KEY3	0x08	/* serialAuthUnlocked14, else status 0x10 */

#define SERIAL1_KEY_OPEN	0xa5a5
#define SERIAL1_KEY_CLOSED	0x5a5a

struct SerialCommand1 {
	unsigned int	command;
	unsigned char	(*handler)(struct SerialBuffer *request, struct SerialBuffer *response);
	unsigned char	flags;		/* SERIAL1_* */
};

/* the operation task 5 runs (serial1Operation, flag 4's 0x4000: 0x0180-0x0182 and
   0x0940-0x0945), 0 when none, and its arguments, which its answer's data replace */
#if SOURCE_0103
/* operation 0x1a's (0x0108): a device's versions */
struct Serial1Device {
	unsigned char	type;		/* 0x10 the Elmo, 0x20 the Cookie, 0x30 and 0x31 the battery,
					   0x40 none */
	unsigned char	result;		/* the read's, 0xff when none answered at the power-on */
	unsigned int	data[3];
};
#endif

union Serial1Args {
#if SOURCE_0103
	unsigned char	byte[0x28];	/* the slim's: operation 0x1a's five devices */
	unsigned int	word[0x14];
	struct Serial1Device device[5];
#if BOARD_DOL1001
	struct ThermalReport thermal;	/* operation 0x1b's */
#endif
#elif FW_SINCE(0x00090903)
	unsigned char	byte[12];
	unsigned int	word[6];
#else
	unsigned char	byte[4];	/* 0x00090804: no 0x0180-0x0182 */
	unsigned int	word[2];
#endif
};

extern unsigned char serial1State;
extern unsigned char serial1Buffer[0x28];
extern union Serial1Args serial1Args;

extern void serial1Reset(void);
extern unsigned char serial1BufferRead(unsigned char offset, unsigned char size,
				       unsigned char *data);
extern unsigned char serial1BufferWrite(unsigned char offset, unsigned char size,
					unsigned char *data);
#if FW_SINCE(0x00090903)
extern unsigned char serial1BufferClear(void);
#endif
#if TASK_FLAG_SETTERS
extern ER serial1InputHandler(void);	/* the setter's result, unused */
#define SERIAL1_INPUT_HANDLER	((void (*)(void))serial1InputHandler)
#else
extern void serial1InputHandler(void);
#define SERIAL1_INPUT_HANDLER	serial1InputHandler
#endif
extern unsigned int serial1SleepFactor(void);
extern unsigned char serialCommands1Count(void);
extern int serial1KeyOpen(void);
extern void serial1Operation(void);

#endif
