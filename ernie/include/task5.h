/*
 * Task 5 (app/task5.c): the events of event flag 4, and what other modules call to post them:
 * powerJobPostOn-powerJobPostUpdate each post the job powerJob runs (bit 1), sub_21F33 the bits of
 * wordFF24E that sub_2A593 handles (bit 7).
 */
#ifndef ERNIE_TASK5_H
#define ERNIE_TASK5_H

#include "kernel.h"

extern unsigned int wordFF24E;

extern void task5(VP_INT exinf);
extern void powerJobPostOn(void);
extern void powerJobPostResume(void);
extern void powerJobPostOff(void);
extern void powerJobPostSuspend(void);
extern void powerJobPostShutdown(void);
extern void powerJobPostCut(void);
extern void powerJobPostOffEnd(void);
extern void powerJobPostReset(void);
extern void powerJobPostUpdate(void);
extern unsigned char sub_21F2A(void);
extern void sub_21F33(unsigned int bits);

#endif
