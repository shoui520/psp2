/*
 * The clock output (app/clockout.c): PCLBUZ0 on P14.0, which the host turns on and off with
 * command 0x00b0 (sceSysconOutputClock).
 */
#ifndef ERNIE_CLOCKOUT_H
#define ERNIE_CLOCKOUT_H

extern void clockOutputStart(void);
extern void clockOutputStop(void);

#endif
