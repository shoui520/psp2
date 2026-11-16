/*
 * The SCE WWS platform layer FIOS2 runs on: mutexes, condition variables, readers-writer
 * locks, threads and time over SceThreadmgr's lightweight objects and SceRtc. Sony's names are
 * known only from the kernel objects' default names ("WwsPlatformMutex", "WwsPlatformCond",
 * "WwsPlatformRwlock", "WwsPlatformThread"); the class and method names are ours. Each call
 * whose failure is a bug stops at a breakpoint (bkpt 1).
 */
#ifndef WWS_PLATFORM_H
#define WWS_PLATFORM_H

#include <scetypes.h>
#include <time.h>
#include <kernel.h>

/* not in the SDK's headers */
extern "C" SceInt64 sceKernelGetSystemTimeWide(void);

namespace Wws {
namespace Platform {

/* an RTC tick count: SceRtc's current tick, or a time relative to one */
typedef SceInt64 Time;

extern const SceUID THREAD_INVALID;	/* a Thread not created */
extern const SceUID THREAD_ID_INVALID;	/* a Mutex no thread owns */
extern const int THREAD_AFFINITY_ANY;

/* the platform layer's checks: a failed one stops at a breakpoint */
#define WWS_BREAK()		__breakpoint(1)
#define WWS_ASSERT(test)						\
	do {								\
		if (__builtin_expect(!(test), 0))			\
			WWS_BREAK();					\
	} while (0)

/* a lightweight mutex that knows its owner; recursive ones count their locks */
class Mutex {
public:
	SceKernelLwMutexWork work;
	SceUID owner;			/* the locking thread, or -1 */
	int count;			/* recursive locks, or -1 for a plain mutex */

	int Init(int recursive, const char *name);
	int Init(int recursive);
	SceUID Term();
	bool TryTerm();
	void Lock();
	bool TryLock();
	int Unlock();
	SceUID GetOwner();
};

/* a lightweight condition variable on one mutex */
class Cond {
public:
	SceKernelLwCondWork work;
	Mutex *mutex;

	int Init(Mutex *m, const char *name);
	int Init(Mutex *m);
	int Term();
	int Wait();
	bool WaitUntil(Time deadline);
	int Signal();
	int Broadcast();
};

/* a readers-writer lock on a mutex and two condition variables */
class Rwlock {
public:
	Mutex mutex;
	Cond readCond;
	Cond writeCond;
	int waitingReaders;
	int readers;
	int waitingWriters;
	int writers;

	int Init(const char *name);
	int Init();
	SceUID Term();
	int ReadLock();
	bool TryReadLock();
	int ReadUnlock();
	int WriteLock();
	bool TryWriteLock();
	int WriteUnlock();
};

/* what a thread is created with: zero fields take the defaults */
struct ThreadParams {
	SceSize stackSize;
	int cpuAffinityMask;
	int priority;
	SceUInt attr;
	char name[32];
};

typedef void (*ThreadEntry)(uintptr_t arg);

/* a kernel thread that runs entry(arg) */
class Thread {
public:
	SceUID uid;
	ThreadEntry entry;
	uintptr_t arg;

	void Init();
	int Create(const ThreadParams *params, ThreadEntry entry, uintptr_t arg);
	int CreateChecked(const ThreadParams *params, ThreadEntry entry, uintptr_t arg);
	int Join();
	SceUID GetId();
};

/* a thread object: its parameters, then Start runs pEntry(arg) on it until Join */
class ThreadObject {
public:
	typedef void (*Entry)(void *arg);

	Thread m_thread;
	ThreadParams m_params;
	Entry m_pEntry;
	void *m_pArg;
	struct State {
		uint8_t started : 1;
		uint8_t finished : 1;	/* m_pEntry returned */
		uint8_t reserved : 6;
	} m_state;

	ThreadObject();
	ThreadObject(const ThreadParams *params);
	ThreadObject(const char *name, int priority, SceSize stackSize);
	ThreadObject(const char *name, int priority, SceSize stackSize, int cpuAffinityMask,
		     SceUInt attr);
	virtual ~ThreadObject();
	/* a ThreadObject is never allocated with new */
	static void operator delete(void *) { WWS_BREAK(); }
	void Check();
	void SetParams(const ThreadParams *params);
	void SetParams(SceSize stackSize, int cpuAffinityMask, int priority, SceUInt attr,
		       const char *name);
	void SetParams(SceSize stackSize, int priority, const char *name);
	void Join();
	int Start(Entry pEntry, void *arg);
	bool IsCurrent();
};

/* time: RTC ticks, nanoseconds and calendar dates */
Time GetTime();
SceInt64 TimeToNanoseconds(Time time);
Time NanosecondsToTime(SceInt64 ns);
Time GetEpochTime();
SceInt64 GetCurrentDate();
SceInt64 DateFromTm(const struct tm *pTm);
struct tm *DateToTm(SceInt64 date, struct tm *pTm);
void SleepUntil(Time deadline);
void SleepMilliseconds(SceInt64 ms);
void Sleep(Time time);

/* the calling thread */
int InitThreadVfpExceptions();
SceUID GetCurrentThreadId();
bool ThreadIdEqual(SceUID a, SceUID b);
int SetThreadVfpExceptions(SceUInt32 set);
int SetThreadPriority(SceUID thread, int priority);
int GetThreadPriority(SceUID thread);
int GetCurrentThreadPriority();
int SetThreadCpuAffinityMask(SceUID thread, int mask);
int GetThreadCpuAffinityMask(SceUID thread);
int GetCurrentThreadCpuAffinityMask();

/* the system time as "seconds.microseconds" */
int FormatSystemTime(char *pBuf, size_t size);

} /* namespace Platform */
} /* namespace Wws */

#endif
