/*
 * SceDriverUser: SceRtcUser clocks and text forms, all system calls of
 * SceRtc. The format calls hand the kernel an unused argument block.
 */

#include <rtc.h>
#include "driver_user.h"

int _sceRtcGetCurrentTick(SceRtcTick *tick);
int _sceRtcGetCurrentNetworkTick(SceRtcTick *tick);
int _sceRtcGetCurrentAdNetworkTick(SceRtcTick *tick);
int _sceRtcGetCurrentDebugNetworkTick(SceRtcTick *tick);
int _sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick);
int _sceRtcGetCurrentGpsTick(SceRtcTick *tick);
int _sceRtcGetLastReincarnatedTick(SceRtcTick *tick);
int _sceRtcGetLastAdjustedTick(SceRtcTick *tick);
int _sceRtcGetCurrentClock(SceDateTime *time, int timezone);
int _sceRtcGetCurrentClockLocalTime(SceDateTime *time);
int _sceRtcConvertUtcToLocalTime(const SceRtcTick *utc, SceRtcTick *local);
int _sceRtcConvertLocalTimeToUtc(const SceRtcTick *local, SceRtcTick *utc);
int _sceRtcFormatRFC2822LocalTime(char *buf, const SceRtcTick *tick, SceSyscallArgs2 *args);
int _sceRtcFormatRFC2822(char *buf, const SceRtcTick *tick, int offset, SceSyscallArgs2 *args);
int _sceRtcFormatRFC3339LocalTime(char *buf, const SceRtcTick *tick, SceSyscallArgs2 *args);
int _sceRtcFormatRFC3339(char *buf, const SceRtcTick *tick, int offset, SceSyscallArgs2 *args);

int sceRtcGetCurrentTick(SceRtcTick *tick)
{
	return _sceRtcGetCurrentTick(tick);
}

int sceRtcGetCurrentNetworkTick(SceRtcTick *tick)
{
	return _sceRtcGetCurrentNetworkTick(tick);
}

int sceRtcGetCurrentAdNetworkTick(SceRtcTick *tick)
{
	return _sceRtcGetCurrentAdNetworkTick(tick);
}

int sceRtcGetCurrentDebugNetworkTick(SceRtcTick *tick)
{
	return _sceRtcGetCurrentDebugNetworkTick(tick);
}

int sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick)
{
	return _sceRtcGetCurrentRetainedNetworkTick(tick);
}

int sceRtcGetCurrentGpsTick(SceRtcTick *tick)
{
	return _sceRtcGetCurrentGpsTick(tick);
}

int sceRtcGetLastReincarnatedTick(SceRtcTick *tick)
{
	return _sceRtcGetLastReincarnatedTick(tick);
}

int sceRtcGetLastAdjustedTick(SceRtcTick *tick)
{
	return _sceRtcGetLastAdjustedTick(tick);
}

int sceRtcGetCurrentClock(SceDateTime *time, int timezone)
{
	return _sceRtcGetCurrentClock(time, timezone);
}

int sceRtcGetCurrentClockLocalTime(SceDateTime *time)
{
	return _sceRtcGetCurrentClockLocalTime(time);
}

int sceRtcConvertUtcToLocalTime(const SceRtcTick *utc, SceRtcTick *local)
{
	return _sceRtcConvertUtcToLocalTime(utc, local);
}

int sceRtcConvertLocalTimeToUtc(const SceRtcTick *local, SceRtcTick *utc)
{
	return _sceRtcConvertLocalTimeToUtc(local, utc);
}

int sceRtcFormatRFC2822LocalTime(char *buf, const SceRtcTick *tick)
{
	SceSyscallArgs2 args;

	return _sceRtcFormatRFC2822LocalTime(buf, tick, &args);
}

int sceRtcFormatRFC2822(char *buf, const SceRtcTick *tick, int offset)
{
	SceSyscallArgs2 args;

	return _sceRtcFormatRFC2822(buf, tick, offset, &args);
}

int sceRtcFormatRFC3339LocalTime(char *buf, const SceRtcTick *tick)
{
	SceSyscallArgs2 args;

	return _sceRtcFormatRFC3339LocalTime(buf, tick, &args);
}

int sceRtcFormatRFC3339(char *buf, const SceRtcTick *tick, int offset)
{
	SceSyscallArgs2 args;

	return _sceRtcFormatRFC3339(buf, tick, offset, &args);
}
