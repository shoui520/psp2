/*
 * What the code the updater shares with the application calls, as the updater has it: the
 * clock started and read without the kept clock counting, the state fixed by the update's
 * mode, a state transition turned into the updater's events, the inputs read from the pins,
 * and a wait on timer channel 3.
 */
#pragma sfr
#pragma NOP

#include "ernie.h"
#include "keepram.h"
#include "rtc.h"
#include "state.h"
#include "updater.h"

/* the counters at 0 (1 January), the 24-hour clock, an interrupt every 0.5 s */
void rtcInit(void)
{
	RTCMK = 1;
	RTCEN = 1;
	RTCE = 0;
	RTCC0 = 0x09;
	RTCC1 = 0x00;
	SEC = 0;
	MIN = 0;
	HOUR = 0;
	DAY = 1;
	WEEK = 0;
	MONTH = 1;
	YEAR = 0;
	SUBCUD = 0;
	ALARMWM = 0;
	ALARMWH = 0;
	ALARMWW = 0;
	RTCIF = 0;
	RTCMK = 0;
	RTCE = 1;
}

union RtcTick getClock(void)
{
	return *(union RtcTick __far *)&keepRam.clock;
}

union PowerRequest stateWakeupFactor;

/* the state the update's mode stands for */
unsigned int stateGet(void)
{
	switch (updMode) {
	case 1:
		return STATE_OFF;
	case 3:
		return STATE_RESUME;
	default:
		return STATE_CUT;
	}
}

unsigned long stateGetWakeMask(void)
{
	return 0;
}

union PowerRequest *stateGetWakeupFactor(void)
{
	return &stateWakeupFactor;
}

unsigned char stateSerialUnlock(void)
{
	struct StateEvent event;

	event.transition = 0x0c;
	return (unsigned char)updStateEvent(&event);
}

int stateSerialLock(void)
{
	struct StateEvent event;

	event.transition = 0x32;
	return (unsigned char)updStateEvent(&event);
}

int sub_4D3F(void)
{
	if (P12.6)
		return 1;
	return 0;
}

int sub_4D47(void)
{
	if (P3.4)
		return 1;
	return 0;
}

int sub_4D4F(void)
{
	return P7.3 != 0;
}

int sub_4D57(void)
{
	return updInput.bits.multiCnReported;
}

unsigned char sub_4D63(void)
{
	return P7.7 == 0;
}

int sub_4D6B(void)
{
	return 1;
}

/* a transition: the updater's event for it */
int updStateEvent(struct StateEvent *event)
{
	switch (event->transition) {
	case 0x52:
		updEventSet(0x14);
		break;
	case 0x0e:
		updEventSet(0x15);
		break;
	case 0x53:
		updEventSet(0x14);
		break;
	case 0x09:
		updEventSet(0x15);
		break;
	case 0x32:
		updEventSet(0x16);
		break;
	case 0x0c:
		break;
	}
	return 0;
}

/* waits ms milliseconds on timer channel 3, at most 500 at a time */
void sub_4DC5(unsigned int ms)
{
	unsigned int count;
	unsigned int step;

	sub_6CFF();
	while (ms != 0) {
		step = ms < 500 ? ms : 500;
		count = (unsigned int)((unsigned long)step * 1000 >> 5) << 2;
		TMIF03 = 0;
		TS0L = 0x08;
		TDR03 = count;
		while (!TMIF03)
			NOP();
		sub_6BFE();
		ms -= step;
	}
}

int sub_4E1F(void)
{
	return 0;
}

int sub_4E21(void)
{
	return 0;
}

void sub_4E23(void)
{
}

void sub_4E24(void)
{
}

int sub_4E25(void)
{
	return 0;
}
