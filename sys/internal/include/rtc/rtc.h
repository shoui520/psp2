/*
 * SceRtc (os0:kd/rtc.skprx) driver interface.
 *
 * Ticks count microseconds since 0001-01-01 00:00:00.
 */

#pragma once

#include <psp2kern/types.h>
#include <psp2common/kernel/rtc.h>
#include <syscon/syscon.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>

/* SceRtcForDriver: clocks and alarms */
int sceRtcSynchronize(void);
int SceRtcForDriver_ABA035B7(void);	/* name unknown: invalidates every aux clock */
int ksceRtcSetConf(unsigned int dateFormat, unsigned int timeFormat, int timezone,
		   unsigned int daylightSaving);
int sceRtcSetCurrentTick(const SceRtcTick *tick);
int ksceRtcGetCurrentTick(SceRtcTick *tick);
int ksceRtcSetCurrentSecureTick(const SceRtcTick *tick);
int ksceRtcGetCurrentSecureTick(SceRtcTick *tick);
int ksceRtcSetCurrentNetworkTick(const SceRtcTick *tick);
int ksceRtcGetCurrentNetworkTick(SceRtcTick *tick);
int ksceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick);
int ksceRtcGetCurrentAdNetworkTick(SceRtcTick *tick);
int ksceRtcSetCurrentDebugNetworkTick(const SceRtcTick *tick);
int ksceRtcGetCurrentDebugNetworkTick(SceRtcTick *tick);
int SceRtcForDriver_A7236656(const SceRtcTick *tick);	/* name unknown: sets the GPS tick */
int ksceRtcGetCurrentGpsTick(SceRtcTick *tick);
int SceRtcForDriver_A0D7899A(SceRtcTick *tick);	/* name unknown: aux clock 2 */
int sceRtcSetAlarmTick(const SceRtcTick *tick, int arg);
int ksceRtcGetAlarmTick(SceRtcTick *tick, int *arg);
int ksceRtcIsAlarmed(void);
int ksceRtcRegisterCallback(SceUID cb);
int ksceRtcUnregisterCallback(SceUID cb);
int ksceRtcSetSecureAlarmTick(const SceRtcTick *tick);
int ksceRtcGetSecureAlarmTick(SceRtcTick *tick);
int ksceRtcIsSecureAlarmed(void);
int ksceRtcRegisterSecureAlarmCallback(SceUID cb);
int ksceRtcUnregisterSecureAlarmCallback(SceUID cb);
int ksceRtcGetCurrentClock(SceDateTime *time, int timezone);
int ksceRtcGetCurrentClockLocalTime(SceDateTime *time);
SceUInt64 ksceRtcGetAccumulativeTime(void);
int ksceRtcGetLastReincarnatedTick(SceRtcTick *tick);
int ksceRtcGetLastAdjustedTick(SceRtcTick *tick);
int ksceRtcConvertUtcToLocal(const SceRtcTick *utc, SceRtcTick *local);
int ksceRtcConvertLocalTimeToUtc(const SceRtcTick *local, SceRtcTick *utc);

/* SceRtcForDriver: calendar arithmetic */
unsigned int SceRtcForDriver_6965E5DF(void);	/* sceRtcGetTickResolution */
int SceRtcForDriver_6F96FAA5(int year);	/* sceRtcIsLeapYear */
int SceRtcForDriver_C2DC38D9(int year, int month);	/* sceRtcGetDaysInMonth */
int SceRtcForDriver_0F800878(int year, int month, int day);	/* sceRtcGetDayOfWeek */
int SceRtcForDriver_9097EF0D(int year, int month, int day);	/* day of year */
int SceRtcForDriver_ADF63D7C(const SceDateTime *time);	/* sceRtcCheckValid */
int SceRtcForDriver_EB27F94E(SceDateTime *time, unsigned int dosTime);
int SceRtcForDriver_09AF419B(const SceDateTime *time, unsigned int *dosTime);
int SceRtcForDriver_BA14600F(SceDateTime *time, SceUInt64 fileTime);
int SceRtcForDriver_1DD4C5B6(const SceDateTime *time, SceUInt64 *fileTime);
int SceRtcForDriver_F4E23829(SceDateTime *time, SceUInt64 unixTime);
int SceRtcForDriver_DE9F4515(SceDateTime *time, unsigned int time32);
int ksceRtcConvertDateTimeToUnixTime(const SceDateTime *time, SceUInt64 *unixTime);
int SceRtcForDriver_FFD7A04F(const SceDateTime *time, unsigned int *time32);
int ksceRtcConvertTickToDateTime(SceDateTime *time, const SceRtcTick *tick);
int SceRtcForDriver_F139E359(const SceDateTime *time, SceRtcTick *tick);	/* sceRtcGetTick */
int SceRtcForDriver_A82AC5B3(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int SceRtcForDriver_CE51C2A1(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int SceRtcForDriver_9C8AF264(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int SceRtcForDriver_0F9E02EF(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int SceRtcForDriver_32E50C36(SceRtcTick *res, const SceRtcTick *tick, int n);
int SceRtcForDriver_0CB72FAF(SceRtcTick *res, const SceRtcTick *tick, int n);
int SceRtcForDriver_24209FBF(SceRtcTick *res, const SceRtcTick *tick, int n);
int SceRtcForDriver_A8CDB6BD(SceRtcTick *res, const SceRtcTick *tick, int n);
int SceRtcForDriver_85F3026C(SceRtcTick *res, const SceRtcTick *tick, int n);
int SceRtcForDriver_0EBB3F12(const SceRtcTick *a, const SceRtcTick *b);	/* sceRtcCompareTick */
int SceRtcForDriver_A1F41AE2(SceRtcTick *tick, const char *str);	/* sceRtcParseRFC3339 */
int SceRtcForDriver_0E7091D6(SceRtcTick *tick, const char *str);	/* sceRtcParseDateTime */
int ksceRtcFormatRFC2822(char *buf, const SceRtcTick *tick, int offset);
int ksceRtcFormatRFC2822LocalTime(char *buf, const SceRtcTick *tick);
int ksceRtcFormatRFC3339(char *buf, const SceRtcTick *tick, int offset);
int ksceRtcFormatRFC3339LocalTime(char *buf, const SceRtcTick *tick);

/*
 * Imports of SceRtc that no other header declares yet (their exporters'
 * headers: syscon/syscon.h, threadmgr/threadmgr.h, processmgr/processmgr.h, sysmem/qafmgr.h).
 */

/* syscon RTC counter, in units of 0.5 s since boot */
/* threadmgr: a one-shot alarm calling handler(argp) after *delay microseconds */
SceUID SceThreadmgrForDriver_10517330(const char *name, const SceUInt64 *delay,
				      int (*handler)(void *argp), void *argp);
/* processmgr: the C library time functions for processes */
int SceProcessmgrForDriver_4140D633(const void *functions);
int ksceSblUtMgrHasStoreFlag(void);
