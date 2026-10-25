/*
 * The serial link's mode 1 (app/serial1.c): the commands of serialCommands1, found by
 * serialCommand1, and the notifications of serialNotifies1.
 */
#ifndef ERNIE_SERIAL1_H
#define ERNIE_SERIAL1_H

#include "serial.h"

/* a command's flags: what it needs */
#define SERIAL1_UNLOCKED	0x01	/* the link unlocked (0x0103), else status 0x20 */
#define SERIAL1_NVS_KEY2	0x02	/* sub_224DF, else status 0x10 */
#define SERIAL1_KEY		0x04	/* serial1Key at SERIAL1_KEY_OPEN, else status 1 */
#define SERIAL1_NVS_KEY3	0x08	/* sub_224EE, else status 0x10 */

#define SERIAL1_KEY_OPEN	0xa5a5
#define SERIAL1_KEY_CLOSED	0x5a5a

struct SerialCommand1 {
	unsigned int	command;
	unsigned char	(*handler)(struct SerialBuffer *request, struct SerialBuffer *response);
	unsigned char	flags;		/* SERIAL1_* */
};

extern void serial1Reset(void);
extern unsigned char serial1BufferRead(unsigned char offset, unsigned char size,
				       unsigned char *data);
extern unsigned char serial1BufferWrite(unsigned char offset, unsigned char size,
					unsigned char *data);
extern unsigned char serial1BufferClear(void);
extern void serial1InputHandler(void);
extern unsigned int serial1SleepFactor(void);
extern unsigned char serialCommands1Count(void);
extern int serial1KeyOpen(void);

#endif
