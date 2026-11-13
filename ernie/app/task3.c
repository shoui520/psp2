/*
 * Task 3: the events of event flag 2, each bit cleared and handled: the keys, the scan of the
 * inputs and the requests that come with them.
 */
#pragma rtos_task task3
#pragma sfr

#include "ernie.h"
#include "analog.h"
#include "board.h"
#include "input.h"
#include "inputreq.h"
#include "kernel.h"
#include "state.h"
#include "task.h"
#include "touch.h"


void task3(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;
#if BOARD_TOUCH_RESET && FW_SINCE(0x01000109)
	unsigned char data[2];
#endif

	for (;;) {
#if BOARD_TOUCH_RESET
		ercd = wai_flg(2, 0x00dc, TWF_ORW, &ptn);
#else
		ercd = wai_flg(2, 0x001c, TWF_ORW, &ptn);
#endif
		if (ercd == E_OK) {
			if (ptn & 0x0008) {
				clr_flg(2, ~0x0008);
				inputProcess();
				if (stateGet() == 0x0020)
					devicesRequestRun();
				if (P4.0)
					P4.0 = 0;
				else
					P4.0 = 1;
			}
			if (ptn & 0x0004) {
				clr_flg(2, ~0x0004);
				analogEvent(inputKeys.events & 0xe3);
			}
			if (ptn & 0x0010) {
				clr_flg(2, ~0x0010);
				controlsRequestRun();
			}
#if BOARD_TOUCH_RESET
			if (ptn & 0x0040) {
				clr_flg(2, ~0x0040);
				touchIdentify0();
#if FW_SINCE(0x01000109)
				touchReadConfigVersion0(data);
#else
				touchReadModes0();
#endif
			} else if (ptn & 0x0080) {
				clr_flg(2, ~0x0080);
				touchIdentify1();
#if FW_SINCE(0x01000109)
				touchReadConfigVersion1(data);
#else
				touchReadModes1();
#endif
			}
#endif
			if (ptn & 0x0100) {
				clr_flg(2, ~0x0100);
				touchUpdateState();
			}
		}
		task3Idle();
	}
	ext_tsk();
}
