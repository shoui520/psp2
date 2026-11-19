/*
 * The input devices' requests (app/inputreq.c): the controls' (request 1), the touch panels'
 * (request 3) and the motion sensor's (request 4), the streams of their frames (0x0300-0x0302,
 * 0x0400), and the controls' reads the command table answers at once.
 */
#ifndef ERNIE_INPUTREQ_H
#define ERNIE_INPUTREQ_H

#include "host.h"

extern struct HostRequest controlsRequest;
extern struct HostRequest touchRequest;
extern struct HostRequest motionRequest;
extern struct HostStream hostStream0300;
extern struct HostStream hostStream0301;
extern struct HostStream hostStream0302;
extern struct HostStream hostStream0400;

extern void inputRequestInit(void);
#if BOARD_USS1001
extern void touchRequestRun(void);
#else
extern void devicesRequestRun(void);
#endif
extern void controlsRequestRun(void);
extern void controlsReadButtons(unsigned char *out);
extern void controlsReadAnalog0(unsigned char *out);
extern void controlsReadAnalog1(unsigned char *out);
extern unsigned char hostCommandControlsInfo(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandAnalog(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandAnalogBoth(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandControlsAnalog(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandControlsAnalogBoth(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandMultiCnInfo(unsigned char *out, unsigned char *length);
extern unsigned char hostCommand0112(unsigned char *out, unsigned char *length);
extern unsigned char hostCommand0102(unsigned char *out, unsigned char *length);
#if !BOARD_USS1001
extern void touchRequestCancel(void);
#endif
#if FW_SINCE(0x01000104)
extern void controlsGetButtons(unsigned char *out);
#if FW_SINCE(0x0100060B)
extern unsigned char hostCommand0120(unsigned char *out, unsigned char *length);
#if BOARD_USS1001
extern unsigned char hostCommand0130(unsigned char *out, unsigned char *length);
extern void controlsRequest0180Done(void);
#endif
#endif
#endif

#endif
