/*
 * wws_mutex.cpp: the WWS platform's time and mutexes. A Time is an RTC tick count; dates are
 * nanoseconds since 1970.
 */
#include <stdarg.h>
#include <string.h>
#include <libdbg.h>
#include <rtc.h>
#include "fios2_internal.h"

namespace Wws {
namespace Platform {

Time GetTime()
{
	SceRtcTick tick;

	WWS_ASSERT(sceRtcGetCurrentTick(&tick) == SCE_OK);
	return tick.tick;
}

SceInt64 TimeToNanoseconds(Time time)
{
	return (SceInt64)((double)time * 1000000000.0 / (double)sceRtcGetTickResolution());
}

Time NanosecondsToTime(SceInt64 ns)
{
	return (Time)((double)ns * (double)sceRtcGetTickResolution() / 1000000000.0);
}

/* the tick count at the start of 1970 */
Time GetEpochTime()
{
	SceDateTime time;
	SceUInt64 seconds;
	Time now;

	sceRtcGetCurrentClock(&time, 0);
	now = GetTime();
	sceRtcGetTime64_t(&time, &seconds);
	return now - NanosecondsToTime(1000000000 * seconds);
}

SceInt64 GetCurrentDate()
{
	SceDateTime time;
	SceUInt64 seconds;

	WWS_ASSERT(sceRtcGetCurrentClock(&time, 0) == SCE_OK);
	WWS_ASSERT(sceRtcGetTime64_t(&time, &seconds) == SCE_OK);
	return 1000 * (1000000 * seconds + time.microsecond);
}

SceInt64 DateFromTm(const struct tm *pTm)
{
	SceInt64 date = 0;

	if (pTm != NULL) {
		SceUInt64 seconds = 0;
		SceDateTime time;

		memset(&time, 0, sizeof(time));
		time.year = pTm->tm_year + 1900;
		time.month = pTm->tm_mon + 1;
		time.day = pTm->tm_mday;
		time.hour = pTm->tm_hour;
		time.minute = pTm->tm_min;
		time.second = pTm->tm_sec;
		if (__builtin_expect(sceRtcGetTime64_t(&time, &seconds) != SCE_OK, 0))
			WWS_BREAK();
		else
			date = 1000000000 * seconds;
	}
	return date;
}

struct tm *DateToTm(SceInt64 date, struct tm *pTm)
{
	if (pTm != NULL) {
		SceDateTime time;

		memset(&time, 0, sizeof(time));
		WWS_ASSERT(sceRtcSetTime64_t(&time, (SceUInt64)date / 1000000000) == SCE_OK);
		pTm->tm_sec = time.second;
		pTm->tm_min = time.minute;
		pTm->tm_hour = time.hour;
		pTm->tm_mday = time.day;
		pTm->tm_mon = time.month - 1;
		pTm->tm_year = time.year - 1900;
		pTm->tm_wday = 0;
		pTm->tm_yday = 0;
		pTm->tm_isdst = 0;
	}
	return pTm;
}

/* recursive == 1: a recursive mutex, which counts its locks */
int Mutex::Init(int recursive, const char *name)
{
	SceUInt32 attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO;
	int locks = -1;
	int ret;

	owner = THREAD_ID_INVALID;
	if (recursive == 1) {
		locks = 0;
		attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE;
	}
	count = locks;
	if (name == NULL || *name == 0)
		name = "WwsPlatformMutex";
	ret = sceKernelCreateLwMutex(&work, name, attr, 0, NULL);
	WWS_ASSERT(ret >= 0);
	return ret;
}

int Mutex::Init(int recursive)
{
	SceUInt32 attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO;
	int locks = -1;
	int ret;

	owner = THREAD_ID_INVALID;
	if (recursive == 1) {
		locks = 0;
		attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE;
	}
	count = locks;
	ret = sceKernelCreateLwMutex(&work, "WwsPlatformMutex", attr, 0, NULL);
	WWS_ASSERT(ret >= 0);
	return ret;
}

SceUID Mutex::Term()
{
	WWS_ASSERT(ThreadIdEqual(owner, THREAD_ID_INVALID));
	WWS_ASSERT(sceKernelDeleteLwMutex(&work) == SCE_OK);
	owner = THREAD_ID_INVALID;
	return owner;
}

/* delete it if no thread holds it */
bool Mutex::TryTerm()
{
	if (owner != THREAD_ID_INVALID)
		return false;
	WWS_ASSERT(sceKernelDeleteLwMutex(&work) == SCE_OK);
	owner = THREAD_ID_INVALID;
	return true;
}

void Mutex::Lock()
{
	if (__builtin_expect(sceKernelLockLwMutex(&work, 1, NULL) != SCE_OK, 0)) {
		WWS_BREAK();
	} else {
		owner = GetCurrentThreadId();
		if (count >= 0)
			count++;
	}
}

bool Mutex::TryLock()
{
	int ret = sceKernelTryLockLwMutex(&work, 1);

	WWS_ASSERT(ret == SCE_OK || ret == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN ||
		   ret == SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE);
	if (ret == SCE_OK) {
		owner = GetCurrentThreadId();
		if (count >= 0)
			count++;
	}
	return ret == SCE_OK;
}

int Mutex::Unlock()
{
	int ret;

	WWS_ASSERT(ThreadIdEqual(owner, GetCurrentThreadId()));
	if (count <= 0) {
		owner = THREAD_ID_INVALID;
	} else {
		count--;
		if (count == 0)
			owner = THREAD_ID_INVALID;
	}
	ret = sceKernelUnlockLwMutex(&work, 1);
	WWS_ASSERT(ret == SCE_OK);
	return ret;
}

SceUID Mutex::GetOwner()
{
	return owner;
}

/* an assertion's report, with its arguments: nothing calls it (Sony's binary keeps it) */
bool reportMutexAssertion(const char *file, int line, bool stop, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	sceDbgAssertionHandler(file, line, stop, "", format, args);
	va_end(args);
	return stop;
}

} /* namespace Platform */
} /* namespace Wws */
