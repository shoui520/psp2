/*
 * The power sequences' waits: the device jobs posted to task 5, the waits on TM03 (31.25 kHz)
 * and on the kernel's ticks (4 ms), the wait for an edge of the power input, and the timers
 * 4-0xe, each of which clears its bit of powerTimers when it runs out.
 */
#pragma sfr

#include "ernie.h"
#include "input.h"
#include "kernel.h"
#include "powerjob.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "task5.h"
#include "tau.h"
#include "timer.h"

static const struct PowerDeviceJob powerDeviceJobs[] = {
	{ 0x0001, powerAccOn },
	{ 0x0002, powerAccOff },
	{ 0x0004, powerHpremoteOn },
	{ 0x0008, powerHpremoteOff },
	{ 0x0010, powerMultiCnOn },
	{ 0x0020, powerMultiCnOff },
	{ 0x0040, powerNoneOn },
	{ 0x0080, powerNoneOff },
	{ 0x0100, powerBbmcStart },
	{ 0x0200, powerBbmcCycle },
	{ 0x1000, powerCookieSet },
};

static unsigned int powerWaitReserved[2];
unsigned int powerTimers;

/* task 5's, for each job whose bit sub_21F33 posted */
void powerDeviceRun(void)
{
	unsigned char i;

	for (i = 0; i < 11; i++) {
		if (powerDeviceJobs[i].bits & powerDevicePending) {
			powerDeviceJobs[i].job();
			powerDevicePending &= ~powerDeviceJobs[i].bits;
		}
	}
}

void powerDeviceInit(void)
{
	powerJobState.id = 0;
	powerJobState.flag = 0;
	powerDevicePending = 0;
}

/* TM03, one count at a time; its interrupt sets bit 2 of event flag 4 */
void powerWaitInit(void)
{
	tau0PowerOn();
	TMR03 = 0x8008;
	TDR03 = 0;
	TMMK03 = 1;
	TS0L = 0x08;
	TMIF03 = 0;
	TMMK03 = 0;
}

void powerWaitMs(unsigned int ms)
{
	ER ercd;
	FLGPTN ptn;

	TDR03 = (unsigned long)ms * 1000 / 32 + (ms * 1000 % 32 ? 1 : 0);
	TS0L = 0x08;
	ercd = twai_flg(4, 0x04, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x04)
			clr_flg(4, ~0x04);
	}
}

void powerSleep(unsigned int ms)
{
	dly_tsk(ms / 4 + (ms % 4 ? 1 : 0));
}

void powerSleep2(unsigned int ms)
{
	dly_tsk(ms / 4 + (ms % 4 ? 1 : 0));
}

/* until the power input's edges, or the timeout */
void powerWaitInput(unsigned char edges, unsigned int timeout)
{
	ER ercd;
	FLGPTN ptn;

	powerInputHandler.edges = edges;
	powerInputHandler.type = 1;
	ercd = twai_flg(4, 0x40, TWF_ORW, &ptn, timeout);
	powerInputHandler.type = 0;
	clr_flg(4, ~0x40);
}

/* until the timers of the bits ran out */
void powerWaitTimers(unsigned int bits)
{
	while (bits & powerTimers)
		dly_tsk(3);
}

void powerStartTimer(unsigned int bits, unsigned char id, unsigned int ticks)
{
	powerTimers |= bits;
	timerStart(id, ticks);
}

static void powerTimerEnd0(void)
{
	powerTimers &= ~0x0001;
}

static void powerTimerEnd1(void)
{
	powerTimers &= ~0x0002;
}

static void powerTimerEnd2(void)
{
	powerTimers &= ~0x0004;
}

static void powerTimerEnd3(void)
{
	powerTimers &= ~0x0008;
}

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

/* bits 9 and 10's have no timer */
static void powerTimerEnd9(void)
{
	powerTimers &= ~0x0200;
}

static void powerTimerEnd10(void)
{
	powerTimers &= ~0x0400;
}

static void powerTimerEnd11(void)
{
	powerTimers &= ~0x0800;
}

static void powerTimerEnd12(void)
{
	powerTimers &= ~0x1000;
}

static void powerTimerEnd13(void)
{
	powerTimers &= ~0x2000;
}

void powerTimersInit(void)
{
	struct TimerHandler handler;

	powerTimers = 0;
	handler.handler = powerTimerEnd0;
	timerSetHandler(0x04, &handler);
	handler.handler = powerTimerEnd1;
	timerSetHandler(0x05, &handler);
	handler.handler = powerTimerEnd2;
	timerSetHandler(0x06, &handler);
	handler.handler = powerTimerEnd3;
	timerSetHandler(0x07, &handler);
	handler.handler = powerTimerEnd4;
	timerSetHandler(0x08, &handler);
	handler.handler = powerTimerEnd5;
	timerSetHandler(0x09, &handler);
	handler.handler = powerTimerEnd6;
	timerSetHandler(0x0a, &handler);
	handler.handler = powerTimerEnd7;
	timerSetHandler(0x0b, &handler);
	handler.handler = powerTimerEnd11;
	timerSetHandler(0x0c, &handler);
	handler.handler = powerTimerEnd12;
	timerSetHandler(0x0d, &handler);
	handler.handler = powerTimerEnd13;
	timerSetHandler(0x0e, &handler);
}
