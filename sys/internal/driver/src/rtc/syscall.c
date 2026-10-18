/*
 * SceRtc system calls: user buffers are copied in and out around the
 * kernel functions.
 */

#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include "rtc_internal.h"
#include <alloca.h>

#define RTC_FORMAT_SIZE		128

/*
 * Names unknown: the kernel functions as the system calls reach them (out of
 * line in the original).
 */
#define RTC_VENEER	static __attribute__((noinline))
RTC_VENEER int rtcSysGetCurrentTick(SceRtcTick *tick)
{
	return SceRtcForDriver_DEC408D4(tick);
}

RTC_VENEER int rtcSysGetCurrentNetworkTick(SceRtcTick *tick)
{
	return SceRtcForDriver_F1517B38(tick);
}

RTC_VENEER int rtcSysGetCurrentDebugNetworkTick(SceRtcTick *tick)
{
	return SceRtcForDriver_C141632A(tick);
}

RTC_VENEER int rtcSysGetCurrentRetainedNetworkTick(SceRtcTick *tick)
{
	return SceRtcForDriver_40B4BCFF(tick);
}

RTC_VENEER int rtcSysGetCurrentAdNetworkTick(SceRtcTick *tick)
{
	return SceRtcForDriver_3E3C09A0(tick);
}

RTC_VENEER int rtcSysGetCurrentGpsTick(SceRtcTick *tick)
{
	return SceRtcForDriver_F69B610F(tick);
}

SceUInt64 sceRtcGetAccumulativeTime(void)
{
	sceKernelApiDeclareTypeUInt64();
	return SceRtcForDriver_0A86FB04();
}

RTC_VENEER int rtcSysGetLastReincarnatedTick(SceRtcTick *tick)
{
	return SceRtcForDriver_3D614A9A(tick);
}

RTC_VENEER int rtcSysGetLastAdjustedTick(SceRtcTick *tick)
{
	return SceRtcForDriver_8A987573(tick);
}

RTC_VENEER int rtcSysGetCurrentClock(SceDateTime *time, int timezone)
{
	return SceRtcForDriver_A930CD1A(time, timezone);
}

RTC_VENEER int rtcSysGetCurrentClockLocalTime(SceDateTime *time)
{
	return sceRtcGetCurrentClockLocalTime(time);
}

RTC_VENEER int rtcSysConvertUtcToLocalTime(const SceRtcTick *utc, SceRtcTick *local)
{
	return SceRtcForDriver_C6E03DD4(utc, local);
}

RTC_VENEER int rtcSysConvertLocalTimeToUtc(const SceRtcTick *local, SceRtcTick *utc)
{
	return SceRtcForDriver_AD2F3544(local, utc);
}

RTC_VENEER int rtcSysFormatRFC2822LocalTime(char *buf, const SceRtcTick *tick)
{
	return SceRtcForDriver_1F7FC209(buf, tick);
}

RTC_VENEER int rtcSysFormatRFC2822(char *buf, const SceRtcTick *tick, int offset)
{
	return SceRtcForDriver_BA3415F8(buf, tick, offset);
}

RTC_VENEER int rtcSysFormatRFC3339LocalTime(char *buf, const SceRtcTick *tick)
{
	return SceRtcForDriver_35925318(buf, tick);
}

RTC_VENEER int rtcSysFormatRFC3339(char *buf, const SceRtcTick *tick, int offset)
{
	return SceRtcForDriver_D32AC698(buf, tick, offset);
}

/* name unknown: a kernel result unless copying out failed */
static inline int rtcCopyOut(void *dst, const void *src, SceSize size, int ret)
{
	int copy = SceSysmemForDriver_6D88EF8A(dst, src, size);

	return copy < 0 ? copy : ret;
}

#define RTC_SYSCALL_TICK(name, function)				\
int name(SceRtcTick *tick)						\
{									\
	SceRtcTick *ktick;						\
	int ret;							\
									\
	if (tick == NULL)						\
		return function(NULL);					\
	ktick = alloca(sizeof(*ktick));			\
	ktick->tick = 0;						\
	ret = function(ktick);						\
	return rtcCopyOut(tick, ktick, sizeof(*ktick), ret);		\
}

RTC_SYSCALL_TICK(_sceRtcGetCurrentTick, rtcSysGetCurrentTick)
RTC_SYSCALL_TICK(_sceRtcGetCurrentNetworkTick, rtcSysGetCurrentNetworkTick)
RTC_SYSCALL_TICK(_sceRtcGetCurrentAdNetworkTick, rtcSysGetCurrentAdNetworkTick)
RTC_SYSCALL_TICK(_sceRtcGetCurrentDebugNetworkTick, rtcSysGetCurrentDebugNetworkTick)
RTC_SYSCALL_TICK(_sceRtcGetCurrentRetainedNetworkTick, rtcSysGetCurrentRetainedNetworkTick)
RTC_SYSCALL_TICK(_sceRtcGetCurrentGpsTick, rtcSysGetCurrentGpsTick)
RTC_SYSCALL_TICK(_sceRtcGetLastReincarnatedTick, rtcSysGetLastReincarnatedTick)
RTC_SYSCALL_TICK(_sceRtcGetLastAdjustedTick, rtcSysGetLastAdjustedTick)

int _sceRtcGetCurrentClock(SceDateTime *time, int timezone)
{
	SceDateTime *ktime;
	int ret;

	if (time == NULL)
		return rtcSysGetCurrentClock(NULL, timezone);
	ktime = alloca(sizeof(*ktime));
	*ktime = (SceDateTime){ 0 };
	ret = rtcSysGetCurrentClock(ktime, timezone);
	return rtcCopyOut(time, ktime, sizeof(*ktime), ret);
}

int _sceRtcGetCurrentClockLocalTime(SceDateTime *time)
{
	SceDateTime *ktime;
	int ret;

	if (time == NULL)
		return rtcSysGetCurrentClockLocalTime(NULL);
	ktime = alloca(sizeof(*ktime));
	*ktime = (SceDateTime){ 0 };
	ret = rtcSysGetCurrentClockLocalTime(ktime);
	return rtcCopyOut(time, ktime, sizeof(*ktime), ret);
}

#define RTC_SYSCALL_CONVERT(name, function)				\
int name(const SceRtcTick *src, SceRtcTick *dst)			\
{									\
	SceRtcTick *ksrc = NULL, *kdst;					\
	int ret;							\
									\
	if (src != NULL) {						\
		ksrc = alloca(sizeof(*ksrc));			\
		ret = SceSysmemForDriver_BC996A7A(ksrc, src, sizeof(*ksrc));	\
		if (ret < 0)						\
			return ret;					\
	}								\
	if (dst == NULL)						\
		return function(ksrc, NULL);				\
	kdst = alloca(sizeof(*kdst));				\
	kdst->tick = 0;							\
	ret = function(ksrc, kdst);					\
	return rtcCopyOut(dst, kdst, sizeof(*kdst), ret);		\
}

RTC_SYSCALL_CONVERT(_sceRtcConvertUtcToLocalTime, rtcSysConvertUtcToLocalTime)
RTC_SYSCALL_CONVERT(_sceRtcConvertLocalTimeToUtc, rtcSysConvertLocalTimeToUtc)

/* the result string, at most RTC_FORMAT_SIZE bytes with its NUL */
static inline int rtcCopyOutString(char *buf, const char *kbuf, int ret)
{
	SceSize len = strnlen(kbuf, RTC_FORMAT_SIZE) + 1;

	if (len > RTC_FORMAT_SIZE)
		len = RTC_FORMAT_SIZE;
	return rtcCopyOut(buf, kbuf, len, ret);
}

/*
 * The format calls copy in an 8-byte argument block that the kernel
 * side does not use.
 */
#define RTC_SYSCALL_FORMAT_LOCAL(name, function)			\
int name(char *buf, const SceRtcTick *tick, const void *reserved)		\
{									\
	char *kbuf = NULL;						\
	SceRtcTick *ktick = NULL;					\
	SceUInt32 block[2];						\
	int ret;							\
									\
	ret = SceSysmemForDriver_BC996A7A(block, reserved, sizeof(block));	\
	if (ret < 0)							\
		return ret;						\
	if (buf != NULL) {						\
		kbuf = alloca(RTC_FORMAT_SIZE);		\
		memset(kbuf, 0, RTC_FORMAT_SIZE);			\
	}								\
	if (tick != NULL) {						\
		ktick = alloca(sizeof(*ktick));		\
		ret = SceSysmemForDriver_BC996A7A(ktick, tick, sizeof(*ktick)); \
		if (ret < 0)						\
			return ret;					\
	}								\
	ret = function(kbuf, ktick);					\
	if (buf == NULL)						\
		return ret;						\
	return rtcCopyOutString(buf, kbuf, ret);			\
}

#define RTC_SYSCALL_FORMAT(name, function)				\
int name(char *buf, const SceRtcTick *tick, int offset, const void *reserved) \
{									\
	char *kbuf = NULL;						\
	SceRtcTick *ktick = NULL;					\
	SceUInt32 block[2];						\
	int ret;							\
									\
	ret = SceSysmemForDriver_BC996A7A(block, reserved, sizeof(block));	\
	if (ret < 0)							\
		return ret;						\
	if (buf != NULL) {						\
		kbuf = alloca(RTC_FORMAT_SIZE);		\
		memset(kbuf, 0, RTC_FORMAT_SIZE);			\
	}								\
	if (tick != NULL) {						\
		ktick = alloca(sizeof(*ktick));		\
		ret = SceSysmemForDriver_BC996A7A(ktick, tick, sizeof(*ktick)); \
		if (ret < 0)						\
			return ret;					\
	}								\
	ret = function(kbuf, ktick, offset);				\
	if (buf == NULL)						\
		return ret;						\
	return rtcCopyOutString(buf, kbuf, ret);			\
}

RTC_SYSCALL_FORMAT_LOCAL(_sceRtcFormatRFC2822LocalTime, rtcSysFormatRFC2822LocalTime)
RTC_SYSCALL_FORMAT(_sceRtcFormatRFC2822, rtcSysFormatRFC2822)
RTC_SYSCALL_FORMAT_LOCAL(_sceRtcFormatRFC3339LocalTime, rtcSysFormatRFC3339LocalTime)
RTC_SYSCALL_FORMAT(_sceRtcFormatRFC3339, rtcSysFormatRFC3339)
