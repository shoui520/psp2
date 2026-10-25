/*
 * Task 5 (app/task5.c): the events of event flag 4, and what other modules call to post them:
 * sub_21E89-sub_21F18 each set the job sub_244DD runs (bit 1), sub_21F33 the bits of
 * wordFF24E that sub_2A593 handles (bit 7).
 */
#ifndef ERNIE_TASK5_H
#define ERNIE_TASK5_H

#include "kernel.h"

extern unsigned int wordFF24E;

extern void task5(VP_INT exinf);
extern void sub_21E89(void);
extern void sub_21E9A(void);
extern void sub_21EAC(void);
extern void sub_21EBE(void);
extern void sub_21ED0(void);
extern void sub_21EE2(void);
extern void sub_21EF4(void);
extern void sub_21F06(void);
extern void sub_21F18(void);
extern unsigned char sub_21F2A(void);
extern void sub_21F33(unsigned int bits);

#endif
