/*
 * The interrupts of the timers and pins whose drivers poll: each wakes what serves it. The
 * pins' edges start the input scan again (inputScanHold), or run a pin monitor's handler at
 * once when it has no reload, or start the monitor counting.
 */
#if ERNIE_FW < 0x0100060B	/* BOARD_MOTION (board.h) */
#pragma interrupt INTTM01 tm01Interrupt
#endif
#pragma rtos_interrupt INTTM03 tm03Interrupt
#pragma rtos_interrupt INTP1 intp1Interrupt
#if (ERNIE_HW >> 20) == 0x008	/* BOARD_USS1001 (board.h): pin monitor 2 on INTP3 */
#pragma rtos_interrupt INTP3 intp3Interrupt
#pragma rtos_interrupt INTP2 intp2Interrupt
#elif (ERNIE_HW >> 20) == 0x007	/* BOARD_DOL1001 (board.h): pin monitor 2 on INTP3 */
#pragma rtos_interrupt INTP9 intp9Interrupt
#pragma rtos_interrupt INTP3 intp3Interrupt
#else
#pragma rtos_interrupt INTP2 intp2Interrupt
#pragma rtos_interrupt INTP7 intp7Interrupt
#pragma rtos_interrupt INTP4 intp4Interrupt
#pragma rtos_interrupt INTP3 intp3Interrupt
#pragma rtos_interrupt INTP10 intp10Interrupt
#pragma rtos_interrupt INTP8 intp8Interrupt
#pragma rtos_interrupt INTP11 intp11Interrupt
#endif
#pragma interrupt INTTM04 tm04Interrupt
#if (ERNIE_HW >> 20) == 0x008
#pragma rtos_interrupt INTP0 intp0Interrupt
#pragma rtos_interrupt INTSRE0 sre0Interrupt
#else
#pragma rtos_interrupt INTSRE0 sre0Interrupt
#pragma rtos_interrupt INTP0 intp0Interrupt
#endif
#if ERNIE_FW < 0x0100060B && ERNIE_FW >= 0x00090206	/* HANG_TIMER */
#pragma interrupt INTTM11 tm11Interrupt
#endif
#pragma sfr
#pragma DI

#include "board.h"
#include "config.h"
#include "ernie.h"
#include "input.h"
#include "keepram.h"
#include "motion.h"
#include "serial2.h"
#include "state.h"
#include "sysinfo.h"
#include "task.h"
#include "tau.h"
#include "touch.h"

#if BOARD_MOTION
/* the motion sensor's frame timer: a frame read while key 0x0010 is held */
void tm01Interrupt(void)
{
	if (inputKeys.state & 0x0010)
		motionReadFrame();
}
#endif

/* the power sequence's wait: event 0x0004 of flag 4 */
void tm03Interrupt(void)
{
	task5SetFlag(4, 0x0004);
}

#if BOARD_DOL1001
/* DOL-1001's INTP9: event 0x1000 of flag 4 */
void intp9Interrupt(void)
{
	task5SetFlag(4, 0x1000);
}
#endif

/* BOARD_INTP1_PIN (P4.6) low: the scan held long */
void intp1Interrupt(void)
{
#if FW_SINCE(0x00090804)
	if (!BOARD_INTP1_PIN)
		inputScanHold(0x754);
#else
	/* in 0x00090206 the interrupt is turned off and the scan runs in mode 1 */
	if (!BOARD_INTP1_PIN) {
		intp1Enable(0);
		inputScanMode1(1);
	}
#endif
}

#if BOARD_USS1001 || BOARD_DOL1001
/* USS-1001 and DOL-1001: P3.0 low runs pin monitor 2 */
void intp3Interrupt(void)
{
	if (!P3.0) {
		if (pinMonitors.pin[2].config.reload != 0) {
			pinMonitor2Enable(1);
			pinMonitors.pin[2].last = 0;
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[2].config.handler();
	}
}

#if BOARD_USS1001
/* USS-1001's INTP2 does nothing */
void intp2Interrupt(void)
{
}
#else
/* DOL-1001: P11.5 high, event 0x0200 of flag 2 */
void sre0Interrupt(void)
{
	if (P11.5)
		task3SetFlag(2, 0x0200);
}
#endif
#else
#if BOARD_INPUTS_102603
/* P4.7 high: bit 4 of input group 1 taken as changed, and the scan held */
void intp2Interrupt(void)
{
	if (P4.7) {
#if !FW_SINCE(0x00090804)
		intp2Enable(0);
#endif
		inputGroup1.prev &= ~0x10;
#if !FW_SINCE(0x00090804)
		if (inputScan.mode != 1)
#endif
		inputScanHold(0x1a);
	}
}

/* P14.1 low: pin monitor 2 */
void intp7Interrupt(void)
{
	if (!P14.1) {
#if !FW_SINCE(0x00090804)
		intp7Enable(0);
#endif
		if (pinMonitors.pin[2].config.reload != 0) {
			pinMonitor2Enable(1);
			pinMonitors.pin[2].last = 0;
#if !FW_SINCE(0x00090804)
			if (inputScan.mode != 1)
#endif
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[2].config.handler();
	}
}

#if BOARD_PIN3_102603
/* P3.1 low: pin monitor 3 */
void intp4Interrupt(void)
{
	if (!P3.1) {
		if (pinMonitors.pin[3].config.reload != 0) {
			pinMonitor3Enable(1);
			pinMonitors.pin[3].last = 0;
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[3].config.handler();
	}
}
#endif
#else
void intp2Interrupt(void)
{
}

void intp7Interrupt(void)
{
}
#endif

#if BOARD_PIN3_102603
#elif FW_SINCE(0x01000104)
void intp4Interrupt(void)
{
}
#elif !FW_SINCE(0x00090804)
/* in 0x00090206 the pin monitors' interrupts are turned off at the edge, and the scan is
   held only while it does not run in mode 1 */
void intp4Interrupt(void)
{
	if (!P3.1) {
		intp4Enable(0);
		if (pinMonitors.pin[3].config.reload != 0) {
			pinMonitor3Enable(1);
			pinMonitors.pin[3].last = 0;
			if (inputScan.mode != 1)
				inputScanHold(0x1a);
		} else
			pinMonitors.pin[3].config.handler();
	}
}
#else
/* before 0x01000104 INTP4 (P3.1 falling, with no WLAN line) runs pin monitor 3, or its
   handler at once when it has no ticks */
void intp4Interrupt(void)
{
	if (configGet()->CONFIG_WLAN_LINE == 0 && !P3.1) {
		if (pinMonitors.pin[3].config.reload != 0) {
			pinMonitor3Enable(1);
			pinMonitors.pin[3].last = 0;
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[3].config.handler();
	}
}
#endif

/* P3.0 low: pin monitor 4 */
void intp3Interrupt(void)
{
	if (!P3.0) {
#if !FW_SINCE(0x00090804)
		intp3Enable(0);
#endif
		if (pinMonitors.pin[4].config.reload != 0) {
			pinMonitor4Enable(1);
			pinMonitors.pin[4].last = 0;
#if !FW_SINCE(0x00090804)
			if (inputScan.mode != 1)
#endif
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[4].config.handler();
	}
}

/* P7.6 low: pin monitor 5 */
void intp10Interrupt(void)
{
	if (!P7.6) {
#if !FW_SINCE(0x00090804)
		intp10Enable(0);
#endif
		if (pinMonitors.pin[5].config.reload != 0) {
			pinMonitor5Enable(1);
			pinMonitors.pin[5].last = 0;
#if !FW_SINCE(0x00090804)
			if (inputScan.mode != 1)
#endif
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[5].config.handler();
	}
}

/* P7.4 high: pin monitor 6 */
void intp8Interrupt(void)
{
	/* nothing in 0x00090206 */
#if !BOARD_INPUTS_102603 && FW_SINCE(0x00090804)
	if (P7.4) {
		if (pinMonitors.pin[6].config.reload != 0) {
			pinMonitor6Enable(1);
			pinMonitors.pin[6].last = 1;
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[6].config.handler();
	}
#endif
}

/* P7.7's edges: the level kept, and the serial link's input told */
void intp11Interrupt(void)
{
#if BOARD_INTP11_LEVEL
	intp11Level = P7.7;
	serial2InputLink();
#elif BOARD_INTP11
	/* 0x00090009: P7.7 high, INTP11 off and the scan held, unless it scans in mode 1 */
	if (P7.7) {
		intp11Enable(0);
		if (inputScan.mode != 1)
			inputScanHold(0x1a);
	}
#endif
}

#endif	/* BOARD_USS1001 || BOARD_DOL1001 */

/* the touch panels' frame timer: one shot (TT0 stops channel 4), a frame started while keys
   0x000c are held */
void tm04Interrupt(void)
{
	TT0L = 0x10;
	if (inputKeys.state & 0x000c)
#if FW_SINCE(0x00090804)
		touchStartFrameNoLock();
#else
		touchStartFrame();
#endif
}

#if BOARD_USS1001
/* BOARD_INTP0_PIN (P12.0) low: transition 0x41 */
void intp0Interrupt(void)
{
	STATE_TRANSITION_EVENT
	if (!BOARD_INTP0_PIN)
		STATE_TRANSITION(0x41);
}
#endif

#if !BOARD_DOL1001
/* BOARD_SRE0_PIN (P11.5) low: bit 0 of input group 1 taken as changed, and the scan held long */
void sre0Interrupt(void)
{
	if (!BOARD_SRE0_PIN) {
#if FW_SINCE(0x00090804)
		inputGroup1.prev &= ~0x01;
		inputScanHold(0x754);
#else
		intp13Enable(0);
		inputGroup1.prev &= ~0x01;
		inputScanMode1(1);
#endif
	}
}
#endif

#if !BOARD_USS1001
/* BOARD_INTP0_PIN (P12.0) low: transition 0x41 */
void intp0Interrupt(void)
{
	STATE_TRANSITION_EVENT
	if (!BOARD_INTP0_PIN)
		STATE_TRANSITION(0x41);
}
#endif

#if HANG_TIMER
/* the hang timer: as the RTC's interrupt counts, the system reset (cause 0x30) at the
   eighth tick that hangLimitReached allows; with no hang, the timer stopped */
void tm11Interrupt(void)
{
	if (HANG_DETECTED()) {
		hangCount++;
		if (hangCount >= 8 && hangLimitReached() != 0) {
#if FW_SINCE(0x01000104)
			DI();
			hangState = HANG_RESET;
			setResetCause(0x30);
			resetSystem();
#else
			/* before 0x01000104 the chip reset as it is, no cause kept */
			hangState = HANG_RESET;
			resetChip();
#endif
		}
	} else {
		tm11Stop();
		hangCount = 0;
		hangState = HANG_NONE;
	}
}
#endif
