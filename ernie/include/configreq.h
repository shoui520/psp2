/*
 * The settings' requests (app/configreq.c): request 0x13 takes the host's commands
 * 0x1380-0x1386 (the settings' read, write, open, close, commit and records), run in task 2
 * at event flag 3's bit 0x4000.
 */
#ifndef ERNIE_CONFIGREQ_H
#define ERNIE_CONFIGREQ_H

#include "host.h"
#include "kernel.h"

extern struct HostRequest configRequest;

extern void configRequestInit(void);
extern unsigned char hostCommandConfigVersions(unsigned char *out, unsigned char *length);
extern void configRequestEvent(FLGPTN ptn);
extern void configRequestNotify(void);

#endif
