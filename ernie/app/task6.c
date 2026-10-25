/*
 * Task 6: the events of event flag 6, the 3G modem's (bbmc.c) and the UART3 link's (bits
 * 0x3000 and 0x4000, cleared here).
 */
#pragma rtos_task task6

#include "ernie.h"
#include "task.h"
#include "trace.h"
#include "bbmc.h"
#include "kernel.h"


void task6(VP_INT exinf)
{
	FLGPTN ptn;
	ER ercd;

	traceInit();
	bbmcInit();
	for (;;) {
		ercd = wai_flg(6, 0x73ff, TWF_ORW, &ptn);
		if (ercd != E_OK)
			continue;
		if (ptn & BBMC_EVENTS) {
			clr_flg(6, ~(ptn & BBMC_EVENTS));
			bbmcEvent(ptn);
		}
		if (ptn & 0x3000)
			clr_flg(6, ~(ptn & 0x3000));
		if (ptn & 0x4000)
			clr_flg(6, ~(ptn & 0x4000));
		task6Idle();
	}
}
