/*
 * The serial link's mode 2 (app/serial2.c): its commands and notifications (serialCommands2,
 * serialNotifies2, serial.h), the scratch pad's test area and P7.1.
 */
#ifndef ERNIE_SERIAL2_H
#define ERNIE_SERIAL2_H

#include "board.h"

extern unsigned char levelTrim(unsigned char level, unsigned char trim);
#if BOARD_SERIAL2
extern void serial2Init(void);
extern void serial2InputRequest(void);
extern void serial2InputLink(void);
extern unsigned char serial2SetP71(unsigned char level);
extern int serial2TestAreaUsed(void);
extern int serial2GetArmed(void);
extern unsigned char serial2Disarm(void);
extern unsigned char serial2Arm(void);
extern int serial2InTime(void);
#else
/* no mode 2: the test area taken as used, never armed or in time */
#define serial2Init()
#define serial2SetP71(level)
#define serial2TestAreaUsed()	1
#define serial2GetArmed()	0
#define serial2InTime()		0
#endif

#endif
