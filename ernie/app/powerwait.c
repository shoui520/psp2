/*
 * The power sequences' waits: the device jobs posted to task 5, the waits on TM03 (31.25 kHz)
 * and on the kernel's ticks (4 ms), the wait for an edge of the power input, and the timers
 * 4-0xe (4-0x10 before 0x01000104), each of which clears its bit of powerTimers when it runs out.
 */
#pragma sfr
#pragma NOP
#pragma DI
#pragma EI

#include "board.h"
#include "charge.h"
#include "ernie.h"
#include "input.h"
#include "kernel.h"
#include "powerjob.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "sleepfactor.h"
#include "state.h"
#include "task5.h"
#include "tau.h"
#include "timer.h"

#if FW_SINCE(0x0100060B) && !BOARD_102603 && !SOURCE_0103
#define POWER_DEVICE_JOBS	9
#else
#define POWER_DEVICE_JOBS	11
#endif

static const struct PowerDeviceJob powerDeviceJobs[POWER_DEVICE_JOBS] = {
	{ 0x0001, powerAccOn },
	{ 0x0002, powerAccOff },
	{ 0x0004, powerHpremoteOn },
	{ 0x0008, powerHpremoteOff },
#if FW_SINCE(0x00090006)
	{ 0x0010, powerMultiCnOn },
	{ 0x0020, powerMultiCnOff },
#else	/* 0x00080504: no multi-connector power */
	{ 0x0010, powerHdmiCecOn },
	{ 0x0020, powerHdmiCecOff },
#endif
#if SOURCE_0103
	{ 0x0040, (unsigned char (*)(void))chargeUsbRequestUpdate },
#elif BOARD_102603
	{ 0x0040, powerSdOn },
	{ 0x0080, powerSdOff },
#elif !FW_SINCE(0x0100060B)
	{ 0x0040, powerNoneOn },
	{ 0x0080, powerNoneOff },
#endif
	{ 0x0100, powerBbmcStart },
	{ 0x0200, powerBbmcCycle },
	{ 0x1000, powerCookieSet },
#if SOURCE_0103
	{ 0x2000, powerBbmcStop },
#endif
};

static unsigned int powerWaitReserved[2];
unsigned int powerTimers;

/* task 5's, for each job whose bit powerDeviceSetPending posted */
void powerDeviceRun(void)
{
	unsigned char i;

	for (i = 0; i < POWER_DEVICE_JOBS; i++) {
		if (powerDeviceJobs[i].bits & powerDevicePending) {
			powerDeviceJobs[i].job();
#if SOURCE_0103
			dis_dsp();
			powerDevicePending &= ~powerDeviceJobs[i].bits;
			ena_dsp();
#else
			powerDevicePending &= ~powerDeviceJobs[i].bits;
#endif
		}
	}
}

void powerDeviceInit(void)
{
	powerJobState.id = 0;
	powerJobState.flag = 0;
	powerDevicePending = 0;
#if !FW_SINCE(0x00080502)
	/* 0x00080307 sets TM03 up here, TAU0 being on since hdwinit */
	TMR03 = 0x8008;
	TDR03 = 0;
	TMMK03 = 1;
#endif
}

/* TM03, one count at a time; its interrupt sets bit 2 of event flag 4 */
void powerWaitInit(void)
{
#if FW_SINCE(0x00080502)
	tau0PowerOn();
	TMR03 = 0x8008;
	TDR03 = 0;
	TMMK03 = 1;
#endif
	TS0L = 0x08;
	TMIF03 = 0;
	TMMK03 = 0;
}

void powerWaitMs(unsigned int ms)
{
	ER ercd;
	FLGPTN ptn;

#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x04);
#endif
	TDR03 = (unsigned long)ms * 1000 / 32 + (ms * 1000 % 32 ? 1 : 0);
	TS0L = 0x08;
	ercd = twai_flg(4, 0x04, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x04)
			clr_flg(4, ~0x04);
	}
}

#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
/* DOL-1002's (and USS-1002's): powerWaitMs polling TM03's interrupt request, its interrupt masked (dispatching is
   disabled where elmoUpdateMasks calls it) */
void powerWaitMsPolled(unsigned int ms)
{
	TMMK03 = 1;
	TDR03 = (unsigned long)ms * 1000 / 32 + (ms * 1000 % 32 ? 1 : 0);
	TS0L = 0x08;
	while (!TMIF03)
		NOP();
	TMIF03 = 0;
	TMMK03 = 0;
}
#endif

void powerSleep(unsigned int ms)
{
	dly_tsk(ms / 4 + (ms % 4 ? 1 : 0));
}

void powerSleep2(unsigned int ms)
{
	dly_tsk(ms / 4 + (ms % 4 ? 1 : 0));
}

#if !(BOARD_USS1001 && SOURCE_0104)	/* not in USS-1002 */
/* until the power input's edges, or the timeout */
void powerWaitInput(unsigned char edges, unsigned int timeout)
{
	ER ercd;
	FLGPTN ptn;

#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x40);
#endif
	powerInputHandler.edges = edges;
	powerInputHandler.type = 1;
	ercd = twai_flg(4, 0x40, TWF_ORW, &ptn, timeout);
#if FW_SINCE(0x0100060B)
	if (ercd == E_OK) {
		if (ptn & 0x40)
			clr_flg(4, ~0x40);
	}
	powerInputHandler.type = 0;
#elif !FW_SINCE(0x00080302)
	/* 0x00080003: only an edge seen ends the wait's handler */
	if (ercd == E_OK && (0x40 & ptn)) {
		clr_flg(4, ~0x40);
		powerInputHandler.type = 0;
	}
#else
	powerInputHandler.type = 0;
	clr_flg(4, ~0x40);
#endif
}
#endif

/* until the timers of the bits ran out */
#if SOURCE_0103
/* the slim gives up after 120 waits, and clears the bits itself */
void powerWaitTimers(unsigned int bits)
{
	unsigned char tries;

	tries = 120;
	while (bits & powerTimers) {
		dly_tsk(3);
		if (--tries == 0) {
			DI();
			powerTimers &= ~bits;
			EI();
			break;
		}
	}
}
#else
void powerWaitTimers(unsigned int bits)
{
	while (bits & powerTimers)
		dly_tsk(3);
}
#endif

void powerStartTimer(unsigned int bits, unsigned char id, unsigned int ticks)
{
	powerTimers |= bits;
	timerStart(id, ticks);
}

#if !FW_SINCE(0x0100060B)
/* bits 0 and 3 have no timer since 0x0100060B */
static void powerTimerEnd0(void)
{
	powerTimers &= ~0x0001;
}
#endif

static void powerTimerEnd1(void)
{
	powerTimers &= ~0x0002;
}

static void powerTimerEnd2(void)
{
	powerTimers &= ~0x0004;
}

#if !FW_SINCE(0x0100060B)
static void powerTimerEnd3(void)
{
	powerTimers &= ~0x0008;
}
#endif

static void powerTimerEnd4(void)
{
	powerTimers &= ~0x0010;
}

static void powerTimerEnd5(void)
{
	powerTimers &= ~0x0020;
}

static void powerTimerEnd6(void)
{
	powerTimers &= ~0x0040;
}

static void powerTimerEnd7(void)
{
	powerTimers &= ~0x0080;
}

/* bits 9 and 10's have no timer since 0x01000104 (USS-1002 drops the functions too) */
#if !(BOARD_USS1001 && SOURCE_0104)
static void powerTimerEnd9(void)
{
	powerTimers &= ~0x0200;
}

static void powerTimerEnd10(void)
{
	powerTimers &= ~0x0400;
}
#endif

static void powerTimerEnd11(void)
{
	powerTimers &= ~0x0800;
}

#if FW_SINCE(0x00090006)	/* 0x00080504 has no bits 12 and 13 */
static void powerTimerEnd12(void)
{
	powerTimers &= ~0x1000;
}

static void powerTimerEnd13(void)
{
	powerTimers &= ~0x2000;
}
#endif

#if FW_SINCE(0x00090502) && !FW_SINCE(0x00090804)
/* 0x00090502 has no powerSeqInit: powerTimersInit sets timers 0x11 and 0x12 itself */
/* timer 0x11 (powerAccOn) */
void powerAccTimer(void)
{
	sleepFactorSet2(1);
}

/* timer 0x12 (powerSdOn) */
void powerSdTimer(void)
{
	sleepFactorSet10(1);
}
#endif

#if (FW_SINCE(0x0100060B) && !SOURCE_0103) || BOARD_DOL1001	/* the slim has no bit 14 */
static void powerTimerEnd14(void)
{
	powerTimers &= ~0x4000;
}
#endif

#if BOARD_DOL1001
static void powerTimerEnd15(void)
{
	powerTimers &= ~0x8000;
}

/* DOL-1001's INTP9: its falling edge (enable 1) wakes the suspended system (task 5's flag
   0x1000, intp9Event) */
void intp9Enable(unsigned char enable)
{
	if (enable == 1) {
		EGP1.1 = 0;
		EGN1.1 = 1;
		PIF9 = 0;
		PMK9 = 0;
	} else {
		EGP1.1 = 0;
		EGN1.1 = 0;
		PIF9 = 0;
		PMK9 = 1;
	}
}

/* task 5's, for flag 0x1000 */
void intp9Event(void)
{
	STATE_TRANSITION_EVENT
	STATE_TRANSITION(0x1d);
}
#endif

void powerTimersInit(void)
{
	struct TimerHandler handler;

	powerTimers = 0;
#if !FW_SINCE(0x0100060B)
	handler.handler = powerTimerEnd0;
	timerSetHandler(POWER_TIMER(0x04), &handler);
#endif
#if !(BOARD_USS1001 && SOURCE_0104)	/* USS-1002 leaves bit 1's timer unset */
	handler.handler = powerTimerEnd1;
	timerSetHandler(POWER_TIMER(0x05), &handler);
#endif
	handler.handler = powerTimerEnd2;
	timerSetHandler(POWER_TIMER(0x06), &handler);
#if !FW_SINCE(0x0100060B)
	handler.handler = powerTimerEnd3;
	timerSetHandler(POWER_TIMER(0x07), &handler);
#endif
	handler.handler = powerTimerEnd4;
	timerSetHandler(POWER_TIMER(0x08), &handler);
	handler.handler = powerTimerEnd5;
	timerSetHandler(POWER_TIMER(0x09), &handler);
	handler.handler = powerTimerEnd6;
	timerSetHandler(POWER_TIMER(0x0a), &handler);
	handler.handler = powerTimerEnd7;
	timerSetHandler(POWER_TIMER(0x0b), &handler);
#if !FW_SINCE(0x01000104)
	handler.handler = powerTimerEnd9;
	timerSetHandler(POWER_TIMER(0x0c), &handler);
	handler.handler = powerTimerEnd10;
	timerSetHandler(POWER_TIMER(0x0d), &handler);
#endif
	handler.handler = powerTimerEnd11;
	timerSetHandler(POWER_TIMER11, &handler);
#if FW_SINCE(0x00090006)
	handler.handler = powerTimerEnd12;
	timerSetHandler(TIMER(0x0d), &handler);
	handler.handler = powerTimerEnd13;
	timerSetHandler(TIMER(0x0e), &handler);
#endif
#if FW_SINCE(0x00090502) && !FW_SINCE(0x00090804)
	handler.handler = powerAccTimer;
	timerSetHandler(TIMER(0x11), &handler);
	handler.handler = powerSdTimer;
	timerSetHandler(TIMER(0x12), &handler);
#endif
#if BOARD_DOL1001
	handler.handler = powerTimerEnd14;
	timerSetHandler(0x13, &handler);
	handler.handler = powerTimerEnd15;
#if BOARD_DOL1002
	timerSetHandler(0x16, &handler);
#else
	timerSetHandler(0x15, &handler);
#endif
#elif FW_SINCE(0x0100060B) && !SOURCE_0103
	handler.handler = powerTimerEnd14;
	timerSetHandler(0x15, &handler);
#endif
}
