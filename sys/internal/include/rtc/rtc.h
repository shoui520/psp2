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
int SceRtcForDriver_DEC408D4(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentTick */
int SceRtcForDriver_7D431C87(const SceRtcTick *tick);	/* vitasdk: ksceRtcSetCurrentSecureTick */
int SceRtcForDriver_401C0954(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentSecureTick */
int SceRtcForDriver_162E486E(const SceRtcTick *tick);	/* vitasdk: ksceRtcSetCurrentNetworkTick */
int SceRtcForDriver_F1517B38(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentNetworkTick */
int SceRtcForDriver_40B4BCFF(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentRetainedNetworkTick */
int SceRtcForDriver_3E3C09A0(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentAdNetworkTick */
/* vitasdk: ksceRtcSetCurrentDebugNetworkTick */
int SceRtcForDriver_AAB6053F(const SceRtcTick *tick);
int SceRtcForDriver_C141632A(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentDebugNetworkTick */
int SceRtcForDriver_A7236656(const SceRtcTick *tick);	/* name unknown: sets the GPS tick */
int SceRtcForDriver_F69B610F(SceRtcTick *tick);	/* vitasdk: ksceRtcGetCurrentGpsTick */
int SceRtcForDriver_A0D7899A(SceRtcTick *tick);	/* name unknown: aux clock 2 */
int sceRtcSetAlarmTick(const SceRtcTick *tick, int arg);
int SceRtcForDriver_C838275A(SceRtcTick *tick, int *arg);	/* vitasdk: ksceRtcGetAlarmTick */
int SceRtcForDriver_CD295F0C(void);	/* vitasdk: ksceRtcIsAlarmed */
int SceRtcForDriver_5220870D(SceUID cb);	/* vitasdk: ksceRtcRegisterCallback */
int SceRtcForDriver_9546C68B(SceUID cb);	/* vitasdk: ksceRtcUnregisterCallback */
int SceRtcForDriver_8333790D(const SceRtcTick *tick);	/* vitasdk: ksceRtcSetSecureAlarmTick */
int SceRtcForDriver_BD53731C(SceRtcTick *tick);	/* vitasdk: ksceRtcGetSecureAlarmTick */
int SceRtcForDriver_E39F2ABE(void);	/* vitasdk: ksceRtcIsSecureAlarmed */
int SceRtcForDriver_CF84DFC0(SceUID cb);	/* vitasdk: ksceRtcRegisterSecureAlarmCallback */
int SceRtcForDriver_EDDC2325(SceUID cb);	/* vitasdk: ksceRtcUnregisterSecureAlarmCallback */
/* vitasdk: ksceRtcGetCurrentClock */
int SceRtcForDriver_A930CD1A(SceDateTime *time, int timezone);
int sceRtcGetCurrentClockLocalTime(SceDateTime *time);
SceUInt64 SceRtcForDriver_0A86FB04(void);	/* vitasdk: ksceRtcGetAccumulativeTime */
int SceRtcForDriver_3D614A9A(SceRtcTick *tick);	/* vitasdk: ksceRtcGetLastReincarnatedTick */
int SceRtcForDriver_8A987573(SceRtcTick *tick);	/* vitasdk: ksceRtcGetLastAdjustedTick */
/* vitasdk: ksceRtcConvertUtcToLocal */
int SceRtcForDriver_C6E03DD4(const SceRtcTick *utc, SceRtcTick *local);
/* vitasdk: ksceRtcConvertLocalTimeToUtc */
int SceRtcForDriver_AD2F3544(const SceRtcTick *local, SceRtcTick *utc);

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
/* vitasdk: ksceRtcConvertDateTimeToUnixTime */
int SceRtcForDriver_497B5DC9(const SceDateTime *time, SceUInt64 *unixTime);
int SceRtcForDriver_FFD7A04F(const SceDateTime *time, unsigned int *time32);
/* vitasdk: ksceRtcConvertTickToDateTime */
int SceRtcForDriver_42A0DFCB(SceDateTime *time, const SceRtcTick *tick);
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
/* vitasdk: ksceRtcFormatRFC2822 */
int SceRtcForDriver_BA3415F8(char *buf, const SceRtcTick *tick, int offset);
/* vitasdk: ksceRtcFormatRFC2822LocalTime */
int SceRtcForDriver_1F7FC209(char *buf, const SceRtcTick *tick);
/* vitasdk: ksceRtcFormatRFC3339 */
int SceRtcForDriver_D32AC698(char *buf, const SceRtcTick *tick, int offset);
/* vitasdk: ksceRtcFormatRFC3339LocalTime */
int SceRtcForDriver_35925318(char *buf, const SceRtcTick *tick);

#ifdef __cplusplus
}
#endif
