/*
 * Task 6: the events of event flag 6, the 3G modem's (bbmc.c) and the UART3 link's (bits
 * 0x3000 and 0x4000, cleared here).
 */
#pragma rtos_task task6

#include "ernie.h"
#include "board.h"
#include "task.h"
#include "trace.h"
#include "bbmc.h"
#include "kernel.h"


void task6(VP_INT exinf)
{
	FLGPTN ptn;
	ER ercd;

#if !FW_SINCE(0x0100060B)
	/* since 0x0100060B the trace is gone, and bbmcEvent starts the modem at a power on */
	traceInit();
	bbmcInit();
#endif
	for (;;) {
#if !FW_SINCE(0x00080302)
		/* 0x00080003: the modem's events in the low byte, left unhandled, and 0x0300 */
		ercd = wai_flg(6, 0x03ff, TWF_ORW, &ptn);
		if (ercd != E_OK)
			continue;
		if (0xff & ptn)
			clr_flg(6, ~(0xff & ptn));
		if (ptn & 0x0300)
			clr_flg(6, ~(ptn & 0x0300));
#else
#if FW_SINCE(0x00080502)
		ercd = wai_flg(6, 0x7000 | BBMC_EVENTS, TWF_ORW, &ptn);
#else
		/* no UART3 bit 0x4000 in 0x00080307 */
		ercd = wai_flg(6, 0x3000 | BBMC_EVENTS, TWF_ORW, &ptn);
#endif
		if (ercd != E_OK)
			continue;
		if (ptn & BBMC_EVENTS) {
			clr_flg(6, ~(ptn & BBMC_EVENTS));
#if BOARD_BBMC || SOURCE_0103
			bbmcEvent(ptn);
#endif
		}
		if (ptn & 0x3000)
			clr_flg(6, ~(ptn & 0x3000));
#if FW_SINCE(0x00080502)
		if (ptn & 0x4000)
			clr_flg(6, ~(ptn & 0x4000));
#endif
#if TASK_PENDING_MARKS
		task6Idle();
#endif
#endif /* !FW_SINCE(0x00080302) */
	}
}
