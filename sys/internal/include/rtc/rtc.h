/*
 * SceRtc (os0:kd/rtc.skprx) driver interface.
 *
 * Ticks count microseconds since 0001-01-01 00:00:00.
 */

#pragma once

#include <scetypes.h>
#include <rtc.h>
#include <rtc/rtc_tick.h>
#include <syscon/syscon.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>
#include <processmgr/processmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SceRtcForDriver: clocks and alarms */
int sceRtcSynchronize(void);
int SceRtcForDriver_ABA035B7(void);	/* name unknown: invalidates every aux clock */
int SceRtcForDriver_ACFE5A9F(unsigned int dateFormat, unsigned int timeFormat, int timezone,
		   unsigned int daylightSaving);	/* vitasdk: ksceRtcSetConf */
int sceRtcSetCurrentTick(const SceRtcTick *tick);
int sceRtcGetCurrentTick(SceRtcTick *tick);
int SceRtcForDriver_7D431C87(const SceRtcTick *tick);	/* vitasdk: ksceRtcSetCurrentSecureTick */
int SceRtcForDriver_401C0954(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentSecureTick */
int sceRtcSetCurrentNetworkTick(const SceRtcTick *tick);
int sceRtcGetCurrentNetworkTick(SceRtcTick *tick);
int SceRtcForDriver_40B4BCFF(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentRetainedNetworkTick */
int SceRtcForDriver_3E3C09A0(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentAdNetworkTick */
/* vitasdk: ksceRtcSetCurrentDebugNetworkTick */
int SceRtcForDriver_AAB6053F(const SceRtcTick *tick);
int SceRtcForDriver_C141632A(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentDebugNetworkTick */
int SceRtcForDriver_A7236656(const SceRtcTick *tick);	/* name unknown: sets the GPS tick */
int SceRtcForDriver_F69B610F(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentGpsTick */
int SceRtcForDriver_A0D7899A(SceRtcTick *tick);	/* name unknown: aux clock 2 */
int sceRtcSetAlarmTick(const SceRtcTick *tick, int arg);
int sceRtcGetAlarmTick(SceRtcTick *tick, int *arg);
int SceRtcForDriver_CD295F0C(void);	/* vitasdk: ksceRtcIsAlarmed */
int SceRtcForDriver_5220870D(SceUID cb);	/* vitasdk: ksceRtcRegisterCallback */
int SceRtcForDriver_9546C68B(SceUID cb);	/* vitasdk: ksceRtcUnregisterCallback */
int SceRtcForDriver_8333790D(const SceRtcTick *tick);	/* vitasdk: ksceRtcSetSecureAlarmTick */
int SceRtcForDriver_BD53731C(SceRtcTick *tick);	/* vitasdk: ksceRtcGetSecureAlarmTick */
int SceRtcForDriver_E39F2ABE(void);	/* vitasdk: ksceRtcIsSecureAlarmed */
int SceRtcForDriver_CF84DFC0(SceUID cb);	/* vitasdk: ksceRtcRegisterSecureAlarmCallback */
int SceRtcForDriver_EDDC2325(SceUID cb);	/* vitasdk: ksceRtcUnregisterSecureAlarmCallback */
int sceRtcGetCurrentClock(SceDateTime *time, int timezone);
int sceRtcGetCurrentClockLocalTime(SceDateTime *time);
SceUInt64 sceRtcGetAccumulativeTime(void);
int sceRtcGetLastReincarnatedTick(SceRtcTick *tick);
int sceRtcGetLastAdjustedTick(SceRtcTick *tick);
int sceRtcConvertUtcToLocalTime(const SceRtcTick *utc, SceRtcTick *local);
int sceRtcConvertLocalTimeToUtc(const SceRtcTick *local, SceRtcTick *utc);

/* SceRtcForDriver: calendar arithmetic */
unsigned int sceRtcGetTickResolution(void);	/* sceRtcGetTickResolution */
int sceRtcIsLeapYear(int year);	/* sceRtcIsLeapYear */
int sceRtcGetDaysInMonth(int year, int month);	/* sceRtcGetDaysInMonth */
int sceRtcGetDayOfWeek(int year, int month, int day);	/* sceRtcGetDayOfWeek */
int sceRtcGetDayOfYear(int year, int month, int day);	/* day of year */
int sceRtcCheckValid(const SceDateTime *time);	/* sceRtcCheckValid */
int sceRtcSetDosTime(SceDateTime *time, unsigned int dosTime);
int sceRtcGetDosTime(const SceDateTime *time, unsigned int *dosTime);
int sceRtcSetWin32FileTime(SceDateTime *time, SceUInt64 fileTime);
int sceRtcGetWin32FileTime(const SceDateTime *time, SceUInt64 *fileTime);
int sceRtcSetTime64_t(SceDateTime *time, SceUInt64 unixTime);
int sceRtcSetTime_t(SceDateTime *time, unsigned int time32);
int sceRtcGetTime64_t(const SceDateTime *time, SceUInt64 *unixTime);
int sceRtcGetTime_t(const SceDateTime *time, unsigned int *time32);
int sceRtcSetTick(SceDateTime *time, const SceRtcTick *tick);
int sceRtcGetTick(const SceDateTime *time, SceRtcTick *tick);	/* sceRtcGetTick */
int sceRtcTickAddMicroseconds(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int sceRtcTickAddTicks(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int sceRtcTickAddSeconds(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int sceRtcTickAddMinutes(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);
int sceRtcTickAddHours(SceRtcTick *res, const SceRtcTick *tick, int n);
int sceRtcTickAddDays(SceRtcTick *res, const SceRtcTick *tick, int n);
int sceRtcTickAddWeeks(SceRtcTick *res, const SceRtcTick *tick, int n);
int sceRtcTickAddMonths(SceRtcTick *res, const SceRtcTick *tick, int n);
int sceRtcTickAddYears(SceRtcTick *res, const SceRtcTick *tick, int n);
int sceRtcCompareTick(const SceRtcTick *a, const SceRtcTick *b);	/* sceRtcCompareTick */
int sceRtcParseRFC3339(SceRtcTick *tick, const char *str);	/* sceRtcParseRFC3339 */
int sceRtcParseDateTime(SceRtcTick *tick, const char *str);	/* sceRtcParseDateTime */
int sceRtcFormatRFC2822(char *buf, const SceRtcTick *tick, int offset);
int sceRtcFormatRFC2822LocalTime(char *buf, const SceRtcTick *tick);
int sceRtcFormatRFC3339(char *buf, const SceRtcTick *tick, int offset);
int sceRtcFormatRFC3339LocalTime(char *buf, const SceRtcTick *tick);

#ifdef __cplusplus
}
#endif
