/*
 * paf::thread: thin wrappers over the kernel's lightweight mutexes, conditions,
 * semaphores and event flags, paf's thread class, and the queues that hand work
 * to the main thread.
 */
#ifndef PAF_THREAD_THREAD_H
#define PAF_THREAD_THREAD_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <std/string>
#include <std/list>

namespace paf {
namespace thread {

class Mutex {
public:
	enum {
		OPTION_FIFO = 1		/* waiters in arrival order, not by priority */
	};

	Mutex(const char *name, int option);
	~Mutex();
	void Lock();
	void Unlock();
	bool TryLock();

	SceKernelLwMutexWork m_mutex;
};

class RMutex {
public:
	RMutex(const char *name, int option);
	~RMutex();
	void Lock();
	void Unlock();
	bool TryLock();

	static RMutex main_thread_mutex;

	SceKernelLwMutexWork m_mutex;
};

/* Holds an RMutex for its scope. */
class RMutexLock {
public:
	RMutexLock(RMutex *rmutex) : m_rmutex(rmutex)
	{
		m_rmutex->Lock();
	}

	~RMutexLock()
	{
		m_rmutex->Unlock();
	}

private:
	RMutex *m_rmutex;
};

class Cond {
public:
	Cond(Mutex &mutex, const char *name, int option);
	~Cond();
	bool Wait(int milliseconds);
	void Notify();
	void NotifyAll();

	Mutex *m_mutex;
	SceKernelLwCondWork m_cond;
};

/*
 * Readers and writers share one event flag: bit 1 is set while no reader holds
 * the lock, bit 2 guards the reader count and the writer.
 */
class RWLock {
public:
	RWLock(const char *name, int option);
	~RWLock();
	void ReaderLock();
	void ReaderUnlock();
	void WriterLock();
	void WriterUnlock();

	SceUID m_evf;
	int m_readers;
};

class Semaphore {
public:
	Semaphore(const char *name, int initCount, int maxCount, int option);
	~Semaphore();
	void Acquire();
	void Release();

	SceUID m_sema;
};

/* The thread priorities paf's own threads are created with. */
class Priority {
public:
	Priority();

	int high;
	int normal;
	int low;
};

Priority *GetPriority();
void SetPriority(const Priority &priority);
void Sleep(int milliseconds);

/* Runs a function on the thread that polls Check(), and waits for its result. */
class SyncCall {
public:
	SyncCall(const char *name);
	~SyncCall();
	void Check();
	int Call(int (*func)(void *), void *arg);

	static SyncCall main_thread_synccall;

	SceUID m_evf;
	int (*m_func)(void *);
	void *m_arg;
	int m_result;
};

/* Functions queued for the thread that polls Check(). */
class CallQueue {
public:
	struct Item {
		string name;
		void (*func)(void *);
		void *arg;
	};

	CallQueue(const char *name);
	~CallQueue();
	void Check();
	void Enqueue(const char *name, void (*func)(void *), void *arg);
	int NumItems();
	void WaitEmpty();

	static CallQueue main_thread_callqueue;

	Mutex m_mutex;
	list<Item *> m_queue;
};

class Thread {
public:
	class Option {
	public:
		Option();

		SceUInt32 attr;
		SceInt32 cpuAffinityMask;
		SceUInt32 stackMemoryType;
	};

	enum {
		ERROR_ALREADY_STARTED = -2136013054	/* 0x80AF0702 */
	};

	Thread(int priority, size_t stackSize, const char *name, const Option *option);
	virtual ~Thread();
	virtual void EntryFunction() = 0;

	int Start();
	void Join();
	void Cancel();
	bool IsCanceled();
	bool IsAlive();
	SceUID GetID();
	void ChangePriority(int priority);
	static void SetDefaultOption(const Option &option);
	static SceInt32 ThreadFunc(SceSize argSize, void *argBlock);

	SceUID m_thid;
	bool m_started;
	bool m_alive;
	bool m_canceled;
};

/* The thread IDs of paf's own threads, recorded by each thread for itself. */
class ThreadIDCache {
public:
	enum Type {
		Type_Main = 0
	};

	static void SetCurrentType(Type type);
	static SceUID Get(Type type);
	static bool Check(Type type);
};

}
}

#endif
