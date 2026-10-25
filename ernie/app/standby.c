/*
 * Standby: HALT, or in the power states that allow it STOP, with the tick and the
 * serial unit 1 stopped around it.
 */
#pragma sfr
#pragma DI
#pragma EI
#pragma HALT
#pragma STOP

#include "ernie.h"

extern unsigned int sub_2AB46(void);
extern int sub_2C564(void);
extern void sub_2C57D(void);
extern void sub_1BBCB(void);
extern void tickStart(void);
extern void tickStop(void);

void standbyPrepare(void);
void standbyResume(void);

void standby(void)
{
	switch (sub_2AB46()) {
	case 0x0004:
	case 0x0008:
	case 0x0010:
	case 0x0800:
		if (sub_2C564()) {
			sub_2C57D();
			HALT();
			return;
		}
		DI();
		standbyPrepare();
		sub_2C57D();
		STOP();
		standbyResume();
		EI();
		return;
	}
	HALT();
}

void standbyPrepare(void)
{
	sub_1BBCB();
	SAU1EN = 0;
	tickStop();
	return;
}

void standbyResume(void)
{
	tickStart();
	return;
}
