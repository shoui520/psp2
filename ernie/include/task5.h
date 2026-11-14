/*
 * Task 5 (app/task5.c): the events of event flag 4, and what other modules call to post them:
 * powerJobPostOn-powerJobPostUpdate each post the job powerJob runs (bit 1), powerDeviceSetPending the bits of
 * powerDevicePending that powerDeviceRun handles (bit 7).
 */
#ifndef ERNIE_TASK5_H
#define ERNIE_TASK5_H

#include "ernie.h"
#include "kernel.h"

extern unsigned int powerDevicePending;

extern void task5(VP_INT exinf);
extern void powerJobPostOn(void);
extern void powerJobPostResume(void);
extern void powerJobPostOff(void);
extern void powerJobPostSuspend(void);
extern void powerJobPostShutdown(void);
extern void powerJobPostCut(void);
extern void powerJobPostOffEnd(void);
#if FW_SINCE(0x01000107)
extern void powerJobPostReset(void);
#endif
#if FW_SINCE(0x01000104)
extern void powerJobPostUpdate(void);
#endif
extern unsigned char elmoVersion0(void);
extern void powerDeviceSetPending(unsigned int bits);

#endif
