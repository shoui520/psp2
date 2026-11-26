/*
 * Task 3: the events of event flag 2, each bit cleared and handled: the keys, the scan of the
 * inputs and the requests that come with them.
 */
#pragma rtos_task task3
#pragma sfr

#include "ernie.h"
#include "analog.h"
#include "board.h"
#include "debugpin.h"
#include "input.h"
#include "inputreq.h"
#include "kernel.h"
#include "state.h"
#include "task.h"
#include "touch.h"


#if SOURCE_0103
void task3(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;

	for (;;) {
#if BOARD_DOL1001
		ercd = wai_flg(2, 0x861c, TWF_ORW, &ptn);
#else
		ercd = wai_flg(2, 0x851c, TWF_ORW, &ptn);
#endif
		if (ercd == E_OK) {
			if (ptn & 0x0400) {
				clr_flg(2, ~0x0400);
				inputStateRun();
			}
#if BOARD_DOL1001
			if (ptn & 0x0200) {	/* sre0Interrupt: P11.5 high */
				clr_flg(2, ~0x0200);
				stateTransition(0x1e);
				inputLogTransition(0x1e);
			}
#endif
			if (ptn & 0x0008) {
				clr_flg(2, ~0x0008);
				inputProcess();
				if (stateGet() == 0x0020)
					touchRequestRun();
				debugPinSet((unsigned char)(P4.0 ^ 1));
			}
			if (ptn & 0x0004) {
				clr_flg(2, ~0x0004);
#if BOARD_DOL1002
				analogEvent(inputKeys.events & 0x72);
#elif INPUT_MEASURE
				analogEvent(inputKeys.events & 0xf2);
#else
				analogEvent(inputKeys.events & 0x32);
#endif
			}
			if (ptn & 0x0010) {
				clr_flg(2, ~0x0010);
				controlsRequestRun();
			}
			if (ptn & 0x0100) {
				clr_flg(2, ~0x0100);
				touchUpdateState();
			}
#if CONTROLS_MODE_DELAYED
			if (ptn & 0x0800) {
				clr_flg(2, ~0x0800);
				controlsRequest0180Done();
			}
#endif
			if (ptn & 0x8000)
				clr_flg(2, ~0x8000);
#if INPUT_0104
			if (ptn & 0x1000) {	/* not waited for: inputTouchRequest */
				clr_flg(2, ~0x1000);
				inputTouchRun();
			}
#endif
#if INPUT_MEASURE
			if (ptn & 0x2000) {	/* not waited for: inputCalibrationRequest */
				clr_flg(2, ~0x2000);
				inputCalibrationRun();
			}
#endif
		}
#if TASK_PENDING_MARKS
		task3Idle();
#endif
	}
	ext_tsk();
}
#else
void task3(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;
#if BOARD_TOUCH_RESET && FW_SINCE(0x01000109)
	unsigned char data[2];
#endif

	for (;;) {
#if BOARD_TOUCH_RESET && FW_SINCE(0x00090006)	/* 0x00080504 identifies the panels in inputProcess */
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
#if BOARD_DEBUG_PINS_POWER
				/* toggled by task 2's ticks instead */
#elif FW_SINCE(0x0100060B)
				debugPinSet((unsigned char)(P4.0 ^ 1));
#else
				if (P4.0)
					P4.0 = 0;
				else
					P4.0 = 1;
#endif
			}
			if (ptn & 0x0004) {
				clr_flg(2, ~0x0004);
				analogEvent(inputKeys.events & 0xe3);
			}
			if (ptn & 0x0010) {
				clr_flg(2, ~0x0010);
				controlsRequestRun();
			}
#if BOARD_TOUCH_RESET && FW_SINCE(0x00090006)
			if (ptn & 0x0040) {
				clr_flg(2, ~0x0040);
				touchIdentify0();
#if FW_SINCE(0x01000109)
				touchReadConfigVersion0(data);
#elif FW_SINCE(0x00090502)
				touchReadModes0();
#endif
			} else if (ptn & 0x0080) {
				clr_flg(2, ~0x0080);
				touchIdentify1();
#if FW_SINCE(0x01000109)
				touchReadConfigVersion1(data);
#elif FW_SINCE(0x00090502)
				touchReadModes1();
#endif
			}
#endif
#if FW_SINCE(0x00090502)
			if (ptn & 0x0100) {
				clr_flg(2, ~0x0100);
				touchUpdateState();
			}
#endif
		}
#if TASK_PENDING_MARKS
		task3Idle();
#endif
	}
	ext_tsk();
}
#endif
