/*
 * The power sequences (app/power.c): task 1 runs the one powerState asks for, and the parts
 * report themselves to it.
 */
#ifndef ERNIE_POWER_H
#define ERNIE_POWER_H

extern void powerSequence(void);
extern unsigned char powerReady(unsigned char id);

#endif
