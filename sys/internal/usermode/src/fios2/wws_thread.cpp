/*
 * wws_thread.cpp: the WWS platform's readers-writer locks, sleeps and threads.
 */
#include <string.h>
#include "fios2_internal.h"

namespace Wws {
namespace Platform {

const SceUID THREAD_INVALID = -1;
const SceUID THREAD_ID_INVALID = -1;
const int THREAD_AFFINITY_ANY = 0;

int Rwlock::Init(const char *name)
{
	if (name == NULL || *name == 0)
		name = "WwsPlatformRwlock";
	mutex.Init(0, name);
	readCond.Init(&mutex, name);
	writeCond.Init(&mutex, name);
	waitingReaders = 0;
	readers = 0;
	waitingWriters = 0;
	writers = 0;
	return 0;
}

int Rwlock::Init()
{
	mutex.Init(0, "WwsPlatformRwlock");
	readCond.Init(&mutex, "WwsPlatformRwlock");
	writeCond.Init(&mutex, "WwsPlatformRwlock");
	waitingReaders = 0;
	readers = 0;
	waitingWriters = 0;
	writers = 0;
	return 0;
}

SceUID Rwlock::Term()
{
	writeCond.Term();
	readCond.Term();
	return mutex.Term();
}

int Rwlock::ReadLock()
{
	mutex.Lock();
	if (writers != 0) {
		waitingReaders++;
		do
			readCond.Wait();
		while (writers != 0);
		waitingReaders--;
	}
	readers++;
	return mutex.Unlock();
}

bool Rwlock::TryReadLock()
{
	bool locked = false;

	if (mutex.TryLock()) {
		if (writers == 0) {
			locked = true;
			readers++;
		}
		mutex.Unlock();
	}
	return locked;
}

int Rwlock::ReadUnlock()
{
	mutex.Lock();
	if (--readers == 0 && waitingWriters > 0)
		writeCond.Signal();
	return mutex.Unlock();
}

int Rwlock::WriteLock()
{
	mutex.Lock();
	if ((writers | readers) != 0) {
		waitingWriters++;
		do
			writeCond.Wait();
		while ((writers | readers) != 0);
		waitingWriters--;
	}
	writers++;
	return mutex.Unlock();
}

bool Rwlock::TryWriteLock()
{
	bool locked = false;

	if (mutex.TryLock()) {
		if ((writers | readers) == 0) {
			writers++;
			locked = true;
		}
		mutex.Unlock();
	}
	return locked;
}

int Rwlock::WriteUnlock()
{
	mutex.Lock();
	WWS_ASSERT(writers == 1);
	writers = 0;
	if (waitingReaders != 0)
		readCond.Broadcast();
	else if (waitingWriters != 0)
		writeCond.Signal();
	return mutex.Unlock();
}

/* the delay that sleeps for a time: its microseconds, rounded up */
static inline SceUInt32 delayOf(Time time)
{
	return (SceUInt32)((TimeToNanoseconds(time) + 999) / 1000);
}

void SleepUntil(Time deadline)
{
	Time remaining = deadline - GetTime();

	if (remaining <= 0)
		remaining = 1;
	while (remaining > 0) {
		WWS_ASSERT(sceKernelDelayThread(delayOf(remaining)) == SCE_OK);
		remaining = deadline - GetTime();
	}
}

void SleepMilliseconds(SceInt64 ms)
{
	Time deadline, remaining;

	WWS_ASSERT(ms >= 0);
	deadline = GetTime() + NanosecondsToTime(1000000 * ms);
	remaining = deadline - GetTime();
	if (remaining <= 0)
		remaining = 1;
	while (remaining > 0) {
		WWS_ASSERT(sceKernelDelayThread(delayOf(remaining)) == SCE_OK);
		remaining = deadline - GetTime();
	}
}

void Sleep(Time time)
{
	Time deadline, remaining;

	WWS_ASSERT(time >= 0);
	deadline = GetTime() + time;
	remaining = deadline - GetTime();
	if (remaining <= 0)
		remaining = 1;
	while (remaining > 0) {
		WWS_ASSERT(sceKernelDelayThread(delayOf(remaining)) == SCE_OK);
		remaining = deadline - GetTime();
	}
}

__attribute__((noinline)) void Thread::Init()
{
	WWS_ASSERT(this != NULL);
	uid = THREAD_INVALID;
}

/* the VFP exceptions a WWS thread runs with */
#define WWS_THREAD_VFP_EXCEPTIONS	0x0800009f

/* a thread's start: its Thread is the argument block */
static int threadEntry(SceSize argSize, void *pArgBlock)
{
	Thread *pThread = (Thread *)pArgBlock;

	WWS_ASSERT(pThread != NULL);
	WWS_ASSERT(argSize == sizeof(Thread));
	WWS_ASSERT(sceKernelChangeThreadVfpException(WWS_THREAD_VFP_EXCEPTIONS, 0) >= 0);
	pThread->entry(pThread->arg);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

int Thread::Create(const ThreadParams *params, ThreadEntry entry, uintptr_t arg)
{
	int priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
	int cpuAffinityMask = 0;
	SceSize stackSize = 0x8000;
	const char *name = "WwsPlatformThread";
	SceUInt attr = 0;
	SceUID thread;

	if (this == NULL || entry == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (params != NULL) {
		if (params->priority != 0)
			priority = params->priority;
		if (params->stackSize != 0)
			stackSize = params->stackSize;
		if (params->name[0] != 0)
			name = params->name;
		if (params->cpuAffinityMask != 0)
			cpuAffinityMask = params->cpuAffinityMask;
		if (params->attr != 0)
			attr = params->attr;
	}
	this->entry = entry;
	this->arg = arg;
	thread = sceKernelCreateThread(name, threadEntry, priority, stackSize, attr,
				       cpuAffinityMask, NULL);
	if (thread > 0) {
		uid = thread;
		return sceKernelStartThread(thread, sizeof(Thread), this);
	}
	if (thread == 0)
		return SCE_ERROR_ERRNO_EIO;
	return thread;
}

int InitThreadVfpExceptions()
{
	int ret = sceKernelChangeThreadVfpException(WWS_THREAD_VFP_EXCEPTIONS, 0);

	WWS_ASSERT(ret >= 0);
	return ret;
}

__attribute__((noinline)) int Thread::Join()
{
	int ret;

	WWS_ASSERT(this != NULL);
	WWS_ASSERT(sceKernelWaitThreadEnd(uid, NULL, NULL) == SCE_OK);
	ret = sceKernelDeleteThread(uid);
	WWS_ASSERT(ret == SCE_OK);
	return ret;
}

__attribute__((noinline)) SceUID Thread::GetId()
{
	WWS_ASSERT(this != NULL);
	return uid;
}

__attribute__((noinline)) SceUID GetCurrentThreadId()
{
	return sceKernelGetThreadId();
}

__attribute__((noinline)) bool ThreadIdEqual(SceUID a, SceUID b)
{
	return a == b;
}

int SetThreadVfpExceptions(SceUInt32 set)
{
	int ret = sceKernelChangeThreadVfpException(0, set);

	WWS_ASSERT(ret >= 0);
	return ret;
}

int SetThreadPriority(SceUID thread, int priority)
{
	int ret = sceKernelChangeThreadPriority2(thread, priority);

	WWS_ASSERT(ret >= 0);
	return ret;
}

int GetThreadPriority(SceUID thread)
{
	SceKernelThreadInfo info;

	info.size = sizeof(info);
	WWS_ASSERT(sceKernelGetThreadInfo(thread, &info) == SCE_OK);
	return info.currentPriority;
}

int GetCurrentThreadPriority()
{
	return sceKernelGetThreadCurrentPriority();
}

int SetThreadCpuAffinityMask(SceUID thread, int mask)
{
	int ret = sceKernelChangeThreadCpuAffinityMask(thread, mask);

	WWS_ASSERT(ret >= 0);
	return ret;
}

int GetThreadCpuAffinityMask(SceUID thread)
{
	int ret = sceKernelGetThreadCpuAffinityMask(thread);

	WWS_ASSERT(ret >= 0);
	return ret;
}

int GetCurrentThreadCpuAffinityMask()
{
	int ret = sceKernelGetThreadCpuAffinityMask(sceKernelGetThreadId());

	WWS_ASSERT(ret >= 0);
	return ret;
}

int Thread::CreateChecked(const ThreadParams *params, ThreadEntry entry, uintptr_t arg)
{
	int ret = Create(params, entry, arg);

	WWS_ASSERT(ret >= 0);
	return ret;
}

/* the members a constructor starts from */
#define THREAD_OBJECT_CLEAR()						\
	do {								\
		m_thread.uid = 0;					\
		m_thread.entry = NULL;					\
		m_thread.arg = 0;					\
		memset(&m_params, 0, sizeof(m_params));			\
		m_pArg = NULL;						\
		m_state.started = 0;					\
		m_state.finished = 0;					\
		m_state.reserved = 0;					\
		m_thread.Init();					\
	} while (0)

ThreadObject::ThreadObject()
{
	THREAD_OBJECT_CLEAR();
}

/* a finished thread is a started one; nothing else is set */
void ThreadObject::Check()
{
	State state = m_state;

	WWS_ASSERT(state.started >= state.finished);
	WWS_ASSERT(state.reserved == 0);
}

void ThreadObject::SetParams(const ThreadParams *params)
{
	Check();
	WWS_ASSERT(!(m_state.started));
	WWS_ASSERT(params != NULL);
	m_params = *params;
	m_params.name[sizeof(m_params.name) - 1] = 0;
}

ThreadObject::ThreadObject(const ThreadParams *params)
{
	THREAD_OBJECT_CLEAR();
	SetParams(params);
}

void ThreadObject::SetParams(SceSize stackSize, int cpuAffinityMask, int priority, SceUInt attr,
			     const char *name)
{
	ThreadParams params = { 0, 0, 0, 0, "" };
	uint32_t i;

	params.stackSize = stackSize;
	params.cpuAffinityMask = cpuAffinityMask;
	params.priority = priority;
	params.attr = attr;
	for (i = 0; i < sizeof(params.name) - 1; i++) {
		params.name[i] = name[i];
		if (name[i] == 0)
			break;
	}
	params.name[sizeof(params.name) - 1] = 0;
	SetParams(&params);
}

void ThreadObject::SetParams(SceSize stackSize, int priority, const char *name)
{
	SetParams(stackSize, 0, priority, 0, name);
}

ThreadObject::ThreadObject(const char *name, int priority, SceSize stackSize)
{
	THREAD_OBJECT_CLEAR();
	SetParams(stackSize, 0, priority, 0, name);
}

ThreadObject::ThreadObject(const char *name, int priority, SceSize stackSize,
			   int cpuAffinityMask, SceUInt attr)
{
	THREAD_OBJECT_CLEAR();
	SetParams(stackSize, cpuAffinityMask, priority, attr, name);
}

void ThreadObject::Join()
{
	Check();
	WWS_ASSERT(m_state.started);
	m_thread.Join();
	m_thread.Init();
	m_state.started = 0;
	m_state.finished = 0;
}

ThreadObject::~ThreadObject()
{
	Check();
	if (m_state.started) {
		WWS_ASSERT(m_state.finished);
		Join();
	}
	m_thread.Init();
}

/* a ThreadObject's thread */
static void threadObjectEntry(uintptr_t arg)
{
	ThreadObject *pObject = (ThreadObject *)arg;

	pObject->m_pEntry(pObject->m_pArg);
	pObject->m_state.finished = 1;
}

int ThreadObject::Start(Entry pEntry, void *arg)
{
	int ret;

	Check();
	WWS_ASSERT(!(m_state.started));
	m_pEntry = pEntry;
	m_pArg = arg;
	m_state.started = 1;
	m_state.finished = 0;
	ret = m_thread.Create(&m_params, threadObjectEntry, (uintptr_t)this);
	WWS_ASSERT(ret >= 0);
	return ret;
}

bool ThreadObject::IsCurrent()
{
	SceUID thread;

	Check();
	thread = m_thread.GetId();
	return ThreadIdEqual(thread, GetCurrentThreadId());
}

} /* namespace Platform */
} /* namespace Wws */
