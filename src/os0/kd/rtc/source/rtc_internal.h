/*
 * SceRtc internals shared by the module's sources.
 */

#ifndef RTC_INTERNAL_H
#define RTC_INTERNAL_H

#include <psp2kern/types.h>
#include "sce/excpmgr_cpu.h"
#include "sce/rtc.h"
#include "sce/syscon_scratchpad.h"

#define RTC_AUX_CLOCKS		8

/* the auxiliary clocks (index into RtcState.aux) */
#define RTC_AUX_SECURE		0
#define RTC_AUX_NETWORK		1
#define RTC_AUX_2		2	/* name unknown; set from the dipsw at cold boot */
#define RTC_AUX_GPS		3
#define RTC_AUX_DEBUG_NETWORK	4

/* RtcAuxClock.flags */
#define RTC_AUX_VALID		0x1
#define RTC_AUX_SET		0x2	/* set since boot */

#define RTC_TICK_1980		0x00DDDEF8B3A14000ULL	/* 1980-01-01, before synchronisation */
#define RTC_TICK_2014_END	0x00E1CB8AE6EB3FFFULL	/* aux clocks must be later */
#define RTC_COUNT_UNIT		500000			/* syscon RTC counter unit, us */

typedef struct RtcAuxClock {
	SceRtcTick rtcOffset;		/* tick minus the RTC base */
	SceRtcTick sysOffset;		/* tick minus the system time */
	SceRtcTick alarmTick;
	SceUInt32 flags;
	SceUID callback;
	SceUID alarm;
	SceUInt32 reserved;
} RtcAuxClock;

typedef struct RtcState {
	SceUInt32 dateFormat;		/* ksceRtcSetConf: 0..2 */
	SceUInt32 timeFormat;		/* 0: 12 hours, 1: 24 hours */
	SceInt32 daylightSaving;	/* minutes */
	SceInt32 timezone;		/* minutes */
	SceUInt32 initialized;
	SceUInt32 rtcCarry;		/* syscon RTC counter carried over boots */
	SceUInt64 rtcBase;		/* RTC counter at synchronisation, us */
	SceUInt64 systemTimeBase;	/* system time at synchronisation */
	SceRtcTick lastSetTick;
	SceRtcTick tickOffset;		/* tick minus the system time */
	RtcAuxClock aux[RTC_AUX_CLOCKS];
	SceUInt8 auxPacked[RTC_AUX_CLOCKS][5];	/* rtcOffset >> 19, as syscon keeps it */
	SceRtcTick alarmTick;
	SceUInt32 reserved0[2];
	int alarmArg;
	SceUInt32 alarmState;		/* 1: setting, 2: fired while setting */
	SceUID alarmCallback;
	SceUInt32 reserved1;
} RtcState;

extern RtcState g_rtc;

/*
 * Names unknown: every SceRtc entry point keeps TPIDRURO (the caller's
 * context word) shifted while it runs and restores it on the way out.
 */
static inline unsigned int rtcEnter(void)
{
	unsigned int context = readTpidruro();

	writeTpidruro(context << 16);
	return context;
}

static inline void rtcLeave(unsigned int context)
{
	writeTpidruro(context);
}

static inline unsigned int rtcContext(void)
{
	return readTpidruro();
}

/*
 * The run-time division helpers, called by name where a newer compiler
 * would otherwise fold the division into different operands.
 */
SceUInt64 __aeabi_uldivmod(SceUInt64 n, SceUInt64 d);
SceInt64 __aeabi_ldivmod(SceInt64 n, SceInt64 d);

int sceRtcSynchronizeCold(void);
void rtcRegisterLibcTime(void);

#endif
