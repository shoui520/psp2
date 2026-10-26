/*
 * The interrupts of the timers and pins whose drivers poll: each wakes what serves it. The
 * pins' edges start the input scan again (inputScanHold), or run a pin monitor's handler at
 * once when it has no reload, or start the monitor counting.
 */
#pragma interrupt INTTM01 tm01Interrupt
#pragma rtos_interrupt INTTM03 tm03Interrupt
#pragma rtos_interrupt INTP1 intp1Interrupt
#pragma rtos_interrupt INTP2 intp2Interrupt
#pragma rtos_interrupt INTP7 intp7Interrupt
#pragma rtos_interrupt INTP4 intp4Interrupt
#pragma rtos_interrupt INTP3 intp3Interrupt
#pragma rtos_interrupt INTP10 intp10Interrupt
#pragma rtos_interrupt INTP8 intp8Interrupt
#if ERNIE_HW != 0x00406000	/* BOARD_INTP11 (#pragma comes first) */
#pragma rtos_interrupt INTP11 intp11Interrupt
#endif
#pragma interrupt INTTM04 tm04Interrupt
#pragma rtos_interrupt INTSRE0 sre0Interrupt
#pragma rtos_interrupt INTP0 intp0Interrupt
#pragma interrupt INTTM11 tm11Interrupt
#pragma sfr
#pragma DI

#include "board.h"
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

/* the motion sensor's frame timer: a frame read while key 0x0010 is held */
void tm01Interrupt(void)
{
	if (inputKeys.state & 0x0010)
		motionReadFrame();
}

/* the power sequence's wait: event 0x0004 of flag 4 */
void tm03Interrupt(void)
{
	task5SetFlag(4, 0x0004);
}

/* P4.6 low: the scan held long */
void intp1Interrupt(void)
{
	if (!P4.6)
		inputScanHold(0x754);
}

void intp2Interrupt(void)
{
}

void intp7Interrupt(void)
{
}

void intp4Interrupt(void)
{
}

/* P3.0 low: pin monitor 4 */
void intp3Interrupt(void)
{
	if (!P3.0) {
		if (pinMonitors.pin[4].config.reload != 0) {
			pinMonitor4Enable(1);
			pinMonitors.pin[4].last = 0;
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[4].config.handler();
	}
}

/* P7.6 low: pin monitor 5 */
void intp10Interrupt(void)
{
	if (!P7.6) {
		if (pinMonitors.pin[5].config.reload != 0) {
			pinMonitor5Enable(1);
			pinMonitors.pin[5].last = 0;
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[5].config.handler();
	}
}

/* P7.4 high: pin monitor 6 */
void intp8Interrupt(void)
{
	if (P7.4) {
		if (pinMonitors.pin[6].config.reload != 0) {
			pinMonitor6Enable(1);
			pinMonitors.pin[6].last = 1;
			inputScanHold(0x1a);
		} else
			pinMonitors.pin[6].config.handler();
	}
}

/* P7.7's edges: the level kept, and the serial link's input told */
#if BOARD_INTP11
void intp11Interrupt(void)
{
	intp11Level = P7.7;
	serial2InputLink();
}
#endif

/* the touch panels' frame timer: one shot (TT0 stops channel 4), a frame started while keys
   0x000c are held */
void tm04Interrupt(void)
{
	TT0L = 0x10;
	if (inputKeys.state & 0x000c)
		touchStartFrameNoLock();
}

/* P11.5 low: bit 0 of input group 1 taken as changed, and the scan held long */
void sre0Interrupt(void)
{
	if (!P11.5) {
		inputGroup1.prev &= ~0x01;
		inputScanHold(0x754);
	}
}

/* P12.0 low: transition 0x41 */
void intp0Interrupt(void)
{
	if (!P12.0)
		stateTransition(0x41);
}

/* the hang timer: as the RTC's interrupt counts, the system reset (cause 0x30) at the
   eighth tick that hangLimitReached allows; with no hang, the timer stopped */
void tm11Interrupt(void)
{
	if (hangDetected() != 0) {
		hangCount++;
		if (hangCount >= 8 && hangLimitReached() != 0) {
			DI();
			hangState = HANG_RESET;
			setResetCause(0x30);
			resetSystem();
		}
	} else {
		tm11Stop();
		hangCount = 0;
		hangState = HANG_NONE;
	}
}
