/*
 * The timer array units (app/tau.c): TAU0 and TAU1 powered on and off, the kernel's tick on
 * TM10, and the interval timers on channels 0 of TAU0 and 0 and 1 of TAU1.
 */
#ifndef ERNIE_TAU_H
#define ERNIE_TAU_H

extern void tickStart(void);
extern void tickStop(void);
extern void tau0PowerOn(void);
extern void tau0PowerOff(void);
extern void tau1PowerOn(void);
extern void tau1PowerOff(void);
extern void tm00Start(void);
extern void tm00Stop(void);
extern void tm10Start(void);
extern void tm10Stop(void);
extern void tm11Start(void);
extern void tm11Stop(void);

#endif
