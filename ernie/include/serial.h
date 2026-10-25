/*
 * The command link on UART3 (app/serial.c): frames of a command, a status, a length, data and
 * a checksum, sent as they are (SERIAL_BINARY) or as hex text ending in "\r\n", each answered
 * by a frame with the command's bit 15 set. Mode 1 runs the commands 0x01xx (serialCommand1),
 * mode 2 those of serialCommands2.
 */
#ifndef ERNIE_SERIAL_H
#define ERNIE_SERIAL_H

#include "kernel.h"

#define SERIAL_PORT		3	/* the UART */
#define SERIAL_DATA_SIZE	0x39	/* the most data a frame carries */

/* a frame: the command, the status, the data's length (in text, twice it), the data, then
   0xffff less the sum of the bytes before */
#define SERIAL_FRAME_COMMAND	0
#define SERIAL_FRAME_STATUS	2
#define SERIAL_FRAME_LENGTH	3
#define SERIAL_FRAME_DATA	5
#define SERIAL_FRAME_SIZE	(SERIAL_FRAME_DATA + SERIAL_DATA_SIZE + 2)

/* serial.mode */
#define SERIAL_MODE		0x7f	/* 1, 2 or 3 */
#define SERIAL_BINARY		0x80	/* frames as they are, not as hex text */

/* serial.state: what serialEvent does at the next event */
#define SERIAL_RECEIVE		1	/* a frame comes in; event 0x0004 ends it */
#define SERIAL_RECEIVED		2	/* serialReceive takes it, serialDispatch runs it */
#define SERIAL_ANSWER_LATER	3	/* a command answers later (serialAnswer0105) */
#define SERIAL_NOTIFY		5	/* event 0x0002: runs a notification's handler */
#define SERIAL_SEND		6	/* event 0x0010: sends the received frame back */
#define SERIAL_ANSWER		7	/* event 0x0008: sends the answer */
#define SERIAL_RESTART		0xff

/* serialReceive's results, a frame's status */
#define SERIAL_OK		0
#define SERIAL_UNKNOWN		1	/* no handler has the command */
#define SERIAL_BAD_LENGTH	2
#define SERIAL_BAD_CHECKSUM	3
#define SERIAL_BAD_FRAME	4
#define SERIAL_BAD_HEX		5

/* serialUnlock's and serialLock's results */
#define SERIAL_NOT_OPEN_UNLOCK	0x11
#define SERIAL_NOT_OPEN_LOCK	0x14

struct SerialBuffer {
	unsigned char	*data;
	unsigned int	length;
};

struct Serial {
	unsigned char	link;		/* 0 closed, else the mode it was opened in */
	unsigned char	locked;		/* the commands marked so are refused */
	unsigned char	mode;		/* SERIAL_MODE, SERIAL_BINARY */
	unsigned char	option;		/* kept for a getter and a setter nobody calls */
	unsigned char	baud;		/* a uart3Open baud (serialSelectBaud) */
	unsigned char	state;		/* SERIAL_RECEIVE ... */
	unsigned char	*buffer;	/* frame (binary) or text */
	unsigned int	command;	/* the frame's */
	unsigned int	length;		/* the frame's data's */
	unsigned int	checksum;	/* the frame's */
	unsigned char	status;		/* the frame's; the answer's */
	unsigned char	*data;		/* the answer's data */
	unsigned int	dataLength;
};

extern struct Serial serial;
extern unsigned char serialText[0x80];			/* a frame as hex text */
extern unsigned char serialFrame[SERIAL_FRAME_SIZE];

/* a command of mode 2's (its handler answers from response), a notification's handler */
struct SerialCommand {
	unsigned int	command;
	unsigned char	(*handler)(struct SerialBuffer *request, struct SerialBuffer *response);
};

struct SerialNotify {
	unsigned int	command;
	unsigned char	(*handler)(void);
};

/* the modes' commands and notifications: their modules are not written yet */
extern const struct SerialCommand serialCommands2[];
extern const struct SerialNotify serialNotifies1[];
extern const struct SerialNotify serialNotifies2[];
extern unsigned int serialCommands2Count(void);
extern unsigned char serialNotifies1Count(void);
extern unsigned char serialNotifies2Count(void);
extern unsigned char serialCommand1(unsigned int command, struct SerialBuffer *request,
				    struct SerialBuffer *response);

extern unsigned char serialEvent(FLGPTN ptn);
extern unsigned char serialGetLink(void);
extern unsigned char serialGetMode(void);
extern unsigned char serialGetOption(void);
extern unsigned char serialSetOption(unsigned char option);
extern unsigned char serialGetBaud(void);
extern unsigned char serialSetBaud(unsigned char baud);
extern unsigned char serialSelectBaud(unsigned char code);
extern unsigned char serialGetState(void);
extern unsigned char serialSetState(unsigned char state);
extern unsigned char serialIsLocked(void);
extern void serialReset(unsigned char mode);
extern void serialStop(int port);
extern void serialStart(unsigned char mode);
extern unsigned char serialReceive(void);
extern unsigned int serialChecksum(unsigned char *data, unsigned int length);
extern unsigned char serialDispatch(void);
extern unsigned char serialSendAnswer(void);
extern unsigned char serialRead(int port, unsigned char *buf, int size);
extern unsigned char serialWrite(int port, unsigned char *buf, int size);
extern void serialFlush(int port);
extern unsigned char serialReopen(int baud);
extern unsigned char serialOpen1(int baud);
extern unsigned char serialClose1(void);
extern unsigned char serialOpen2(int baud);
extern unsigned char serialClose2(void);
extern unsigned char serialOpen3(int baud);
extern unsigned char serialClose3(void);
extern unsigned char serialClose(void);
extern unsigned char serialUnlock(unsigned char link);
extern unsigned char serialLock(unsigned char link);
extern unsigned char serialSetP41(unsigned char level);
extern unsigned char serialSend(struct Serial *s);
extern unsigned char serialNotify(void);
extern void serialSetAnswer(unsigned char state);
extern void serialPostAnswer(void);
extern void serialPostTimeout(void);
extern void serialStartTimer(void);
extern unsigned char serialAnswer0105(void);

#endif
