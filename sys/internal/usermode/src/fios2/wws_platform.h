/*
 * The SCE WWS platform layer FIOS2 runs on: mutexes, condition variables and threads over
 * SceThreadmgr's lightweight objects. Sony's names are known only from the kernel objects'
 * default names ("WwsPlatformMutex", "WwsPlatformCond", "WwsPlatformThread"); the class
 * and method names are ours.
 */
#ifndef WWS_PLATFORM_H
#define WWS_PLATFORM_H

#include <scetypes.h>
#include <kernel.h>

namespace Wws {
namespace Platform {

/* a lightweight mutex that knows its owner; recursive ones count their locks */
class Mutex {
public:
	SceKernelLwMutexWork work;
	SceUID owner;			/* the locking thread, or -1 */
	int count;			/* recursive locks, or -1 for a plain mutex */

	void Init(int recursive, const char *name);
	void Init(int recursive);
	void Term();
	void Lock();
	bool TryLock();
	void Unlock();
};

/* a lightweight condition variable on one mutex */
class Cond {
public:
	SceKernelLwCondWork work;
	Mutex *mutex;

	void Init(Mutex *m, const char *name);
	void Init(Mutex *m);
	void Term();
	void Wait();
	void Signal();
	void Broadcast();
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

class Thread {
public:
	SceUID uid;
	ThreadEntry entry;
	uintptr_t arg;

	int Create(const ThreadParams *params, ThreadEntry entry, uintptr_t arg);
	void Join();
};

SceUID GetCurrentThreadId();
bool ThreadIdEqual(SceUID a, SceUID b);

} /* namespace Platform */
} /* namespace Wws */

#endif
