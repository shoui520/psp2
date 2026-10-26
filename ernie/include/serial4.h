/*
 * The command link on UART4 (app/serial4.c): app/serial.c's frames and states, in mode 2 only,
 * stepped by the events 0x0200-0x0800 of event flag 3.
 */
#ifndef ERNIE_SERIAL4_H
#define ERNIE_SERIAL4_H

#include "board.h"
#include "kernel.h"
#include "serial.h"

#if BOARD_SERIAL4
#define SERIAL4_PORT		4	/* the UART */

extern struct Serial serial4;
extern unsigned char serial4Text[0x80];
extern unsigned char serial4Frame[SERIAL_FRAME_SIZE];

extern unsigned char serial4GetMode(void);
extern unsigned char serial4GetNextMode(void);
extern unsigned char serial4SetNextMode(unsigned char mode);
extern unsigned char serial4GetBaud(void);
extern unsigned char serial4SetBaud(unsigned char baud);
extern unsigned char serial4SelectBaud(unsigned char code);
extern unsigned char serial4GetState(void);
extern unsigned char serial4SetState(unsigned char state);
extern unsigned char serial4IsLocked(void);
extern void serial4Reset(unsigned char mode);
extern void serial4Stop(void);
extern void serial4Start(unsigned char mode);
extern unsigned char serial4Receive(void);
extern unsigned int serial4Checksum(unsigned char *data, unsigned int length);
extern unsigned char serial4Dispatch(void);
extern unsigned char serial4SendAnswer(void);
extern unsigned char serial4Read(int port, unsigned char *buf, int size);
extern unsigned char serial4Write(int port, unsigned char *buf, int size);
extern void serial4Flush(int port);
extern unsigned char serial4Reopen(int baud);
extern unsigned char serial4Open2(int baud);
extern unsigned char serial4Close2(void);
extern unsigned char serial4Close(void);
extern unsigned char serial4Unlock(unsigned char link);
extern unsigned char serial4Lock(unsigned char link);
extern unsigned char serial4Notify(void);
extern void serial4SetAnswer(unsigned char state);
extern void serial4PostAnswer(void);
extern unsigned char serial4Answer0205(void);
extern unsigned int serial4GetCommand(void);
extern unsigned char serial4GetStatus(void);
extern unsigned char serial4GetP71(void);
extern void serial4Trace(unsigned char *buffer);
#else
/* no UART4 link: nothing to open, close or answer, its state read as 0 */
#define serial4Reopen(baud)
#define serial4Open2(baud)
#define serial4Close2()
#define serial4Answer0205()
#define serial4GetCommand()	0
#define serial4GetStatus()	0
#define serial4GetP71()		0
#endif
extern unsigned char serial4Event(FLGPTN ptn);
extern void serial4PostTimeout(void);
extern void serial4PostTimeout3(void);
extern void serial4StartTimers(void);

#endif
