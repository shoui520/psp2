/*
 * What the code the updater shares with the application calls, as the updater has it: the
 * clock started and read without the kept clock counting, the state fixed by the update's
 * mode, a state transition turned into the updater's events, the inputs read from the pins,
 * and a wait on timer channel 3.
 */
#pragma sfr
#pragma NOP
#pragma DI
#pragma EI

#include "ernie.h"
#include "analog.h"
#include "keepram.h"
#include "rtc.h"
#include "state.h"
#include "updater.h"

#if SOURCE_0103
union PowerRequest stateWakeupFactor;	/* the PS TV's: before getClock's return buffer */
#endif

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

#if SOURCE_0103
void setClock(unsigned long clock)
{
	DI();
	keepRam.clock = clock;
	EI();
}
#endif

#if !SOURCE_0103
union PowerRequest stateWakeupFactor;
#endif

/* the state the update's mode stands for */
unsigned int stateGet(void)
{
	switch (updMode) {
	case 1:
		return STATE_OFF;
	case 3:
#if SOURCE_0103	/* the PS TV's: modes 8 and 9 resume too */
	case 8:
	case 9:
#endif
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

#if SOURCE_0103
unsigned char stateGetCause(void)
{
	return 0;
}
#endif

#if FW_SINCE(0x0100060D)
unsigned char stateSerialUnlock(void)
{
	return (unsigned char)updStateEvent(0x0c);
}

int stateSerialLock(void)
{
	return (unsigned char)updStateEvent(0x34);
}
#else
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
#endif

#if SOURCE_0103
/* the PS TV's updater: the inputs read 0 */
int input2Bit0Get(void)
{
	return 0;
}

int input2Bit1Get(void)
{
	return 0;
}

int input3Bit0High(void)
{
	return 0;
}
#else
int input2Bit0Get(void)
{
	if (P12.6)
		return 1;
	return 0;
}

int input2Bit1Get(void)
{
	if (P3.4)
		return 1;
	return 0;
}

/* PDEL 0x00416000's 0x0100060D keeps the earlier pins, P7.3 and P7.7, and intp11Low before
   chargeVbusCharging (UPD_060D_FORMS) */
#if UPD_060D_FORMS
/* nothing calls it in 0x0100060D */
int input3Bit0High(void)
{
	return 0;
}
#else
int input3Bit0High(void)
{
	return P7.3 != 0;
}
#endif
#endif /* SOURCE_0103 */

#if FW_SINCE(0x0100060D)
unsigned char input1Bit5Active(void)
#else
int input1Bit5Active(void)
#endif
{
	return updInput.bits.multiCnReported;
}

#if SOURCE_0103
int intp11Low(void)
{
	return 0;
}

int chargeVbusCharging(void)
{
	return 0;
}

/* what is on the multi connector, as the updater read it */
unsigned char analogGetMultiCn(void)
{
	return updMultiCn;
}
#elif UPD_060D_FORMS
/* nothing calls it in 0x0100060D, nor intp11Low */
int chargeVbusCharging(void)
{
	return 0;
}

int intp11Low(void)
{
	return P14.5 == 0;
}
#else
unsigned char intp11Low(void)
{
	return P7.7 == 0;
}
#endif

#if !UPD_060D_FORMS && !SOURCE_0103
int chargeVbusCharging(void)
{
	return 1;
}
#endif

/* a transition: the updater's event for it */
#if FW_SINCE(0x0100060D)
int updStateEvent(unsigned char transition)
{
	switch (transition) {
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
	case 0x34:
		updEventSet(0x16);
		break;
#if SOURCE_0103	/* the PS TV's: the serial unlock's event, and five more */
	case 0x0c:
		updEventSet(0x18);
		break;
	case 0x00:
		updEventSet(0x67);
		break;
	case 0x35:
		updEventSet(0x1f);
		break;
	case 0x5e:
		updEventSet(0x1f);
		break;
	case 0x5f:
		updEventSet(0x20);
		break;
	case 0x11:
		updEventSet(0x17);
		break;
#else
	case 0x00:
		updEventSet(0x67);
		break;
	case 0x0c:
		break;
#endif
	}
	return 0;
}
#else
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
#endif

/* waits ms milliseconds on timer channel 3, at most 500 at a time */
void powerWaitLong(unsigned int ms)
{
	unsigned int count;
	unsigned int step;

	powerWaitInit();
	while (ms != 0) {
		step = ms < 500 ? ms : 500;
		count = (unsigned int)((unsigned long)step * 1000 >> 5) << 2;
		TMIF03 = 0;
#if SOURCE_0103	/* the PS TV's: the count loaded before the start */
		TDR03 = count;
		TS0L = 0x08;
#else
		TS0L = 0x08;
		TDR03 = count;
#endif
		while (!TMIF03)
			NOP();
		powerWaitStop();
		ms -= step;
	}
}

int bbmcStart(void)
{
	return 0;
}

int bbmcSetOff(void)
{
	return 0;
}

void bbmcStop(void)
{
}

void bbmcWake(void)
{
}

int bbmcGetMode(void)
{
	return 0;
}

#if FW_SINCE(0x0100060D)
/* the application's hostInit, to its config: the frame's size and the config's */
int hostInit(void)
{
	hostLink.frameSize = 0x20;
	hostLink.config.size = 0;
	hostLink.config.level = 0;
	return 0;
}
#endif
