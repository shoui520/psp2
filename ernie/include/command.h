/*
 * The base commands (app/command.c): the reads the command table answers at once, and
 * request 0, which takes the writes.
 */
#ifndef ERNIE_COMMAND_H
#define ERNIE_COMMAND_H

#include "host.h"

extern struct HostRequest commandRequest;

extern unsigned char hostCommandErnieVersion(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandTimeStamp(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandHardwareInfo(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandWakeupFactor(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandClock(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandAlarm(unsigned char *out, unsigned char *length);
extern unsigned char hostCommand0013(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandPins(unsigned char *out, unsigned char *length);
extern void commandRequestInit(void);
extern void commandRequestReceive(void);
extern void commandRequestNotify(void);

#endif
