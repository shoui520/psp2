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
#include "serial.h"
#include "state.h"
#include "task.h"
#include "tau.h"


void standbyPrepare(void);
void standbyResume(void);

void standby(void)
{
	switch (stateGet()) {
	case 0x0004:
	case 0x0008:
	case 0x0010:
	case 0x0800:
		if (taskStandbyHalt()) {
			taskHookStandby();
			HALT();
			return;
		}
		DI();
		standbyPrepare();
		taskHookStandby();
		STOP();
		standbyResume();
		EI();
		return;
	}
	HALT();
}

void standbyPrepare(void)
{
	serialClose3();
	SAU1EN = 0;
	tickStop();
	return;
}

void standbyResume(void)
{
	tickStart();
	return;
}
