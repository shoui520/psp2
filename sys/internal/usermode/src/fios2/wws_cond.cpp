/*
 * wws_cond.cpp: the WWS platform's condition variables, and the system time for FIOS2's
 * profiling messages.
 */
#include <stdarg.h>
#include <libdbg.h>
#include "fios2_internal.h"

namespace Wws {
namespace Platform {

int FormatSystemTime(char *pBuf, size_t size)
{
	SceInt64 time = sceKernelGetSystemTimeWide();
	SceInt64 seconds = time / 1000000;

	fiosSnprintf(pBuf, size, "%lld.%06lld", seconds, time - seconds * 1000000);
	return 0;
}

int Cond::Init(Mutex *m, const char *name)
{
	int ret;

	if (name == NULL || *name == 0)
		name = "WwsPlatformCond";
	mutex = m;
	ret = sceKernelCreateLwCond(&work, name, SCE_KERNEL_LW_COND_ATTR_TH_PRIO, &m->work, NULL);
	WWS_ASSERT(ret >= 0);
	return ret;
}

int Cond::Init(Mutex *m)
{
	int ret;

	mutex = m;
	ret = sceKernelCreateLwCond(&work, "WwsPlatformCond", SCE_KERNEL_LW_COND_ATTR_TH_PRIO,
				    &m->work, NULL);
	WWS_ASSERT(ret >= 0);
	return ret;
}

int Cond::Term()
{
	WWS_ASSERT(sceKernelDeleteLwCond(&work) == SCE_OK);
	mutex = NULL;
	return 0;
}

/* wait for a signal; the caller holds the mutex */
int Cond::Wait()
{
	SceUID self = GetCurrentThreadId();
	Mutex *m = mutex;
	int ret;

	WWS_ASSERT(ThreadIdEqual(m->owner, self));
	m->owner = THREAD_ID_INVALID;
	ret = sceKernelWaitLwCond(&work, NULL);
	m->owner = self;
	WWS_ASSERT(ret == SCE_OK);
	return ret;
}

/* wait for a signal until the deadline: false when it passed */
bool Cond::WaitUntil(Time deadline)
{
	SceUID self = GetCurrentThreadId();
	Mutex *m = mutex;
	Time remaining = deadline - GetTime();
	SceInt64 us = (TimeToNanoseconds(remaining) + 999) / 1000;
	SceUInt32 timeout = (SceUInt64)us < 0xffffffff ? (SceUInt32)us : 0xffffffff;
	bool signalled;

	WWS_ASSERT(ThreadIdEqual(m->owner, self));
	if (remaining <= 0 || us <= 0)
		return false;
	m->owner = THREAD_ID_INVALID;
	signalled = sceKernelWaitLwCond(&work, &timeout) == SCE_OK;
	m->owner = self;
	return signalled;
}

int Cond::Signal()
{
	SceUID self = GetCurrentThreadId();
	int ret;

	WWS_ASSERT(ThreadIdEqual(mutex->owner, self));
	ret = sceKernelSignalLwCond(&work);
	WWS_ASSERT(ret == SCE_OK);
	return ret;
}

int Cond::Broadcast()
{
	SceUID self = GetCurrentThreadId();
	int ret;

	WWS_ASSERT(ThreadIdEqual(mutex->owner, self));
	ret = sceKernelSignalLwCondAll(&work);
	WWS_ASSERT(ret == SCE_OK);
	return ret;
}

/* an assertion's report, with its arguments */
static __attribute__((used)) bool reportAssertion(const char *file, int line, bool stop, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	sceDbgAssertionHandler(file, line, stop, "", format, args);
	va_end(args);
	return stop;
}

} /* namespace Platform */
} /* namespace Wws */
