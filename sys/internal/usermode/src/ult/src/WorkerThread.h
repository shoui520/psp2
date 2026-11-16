/*
 * WorkerThread.h: an ulthread runtime's worker: a kernel thread whose fiber takes the runtime's
 * ready ulthreads one after the other and runs them; an ulthread that stops running (it
 * exits, yields or waits) switches back to its worker's fiber with its request.
 */
#ifndef WORKER_THREAD_H
#define WORKER_THREAD_H

#include "ult_internal.h"

class Ulthread;
class UlthreadRuntime;

/* what an ulthread asks of its worker when it switches back to it */
enum {
	WORKER_REQUEST_EXIT = 1,
	WORKER_REQUEST_YIELD = 2,
	WORKER_REQUEST_WAIT = 3		/* from the ulthread's waitSignal: any thread may ask */
};

/* how a runtime creates its workers (its option parameter's) */
struct WorkerThreadParam {
	int32_t priority;
	uint32_t stackSize;			/* the worker's own */
	uint32_t oneShotThreadStackSize;	/* for its ulthreads without a context */
	uint32_t attr;
	uint32_t cpuAffinityMask;
	const SceKernelThreadOptParam *optParam;
};

/* the worker's own stack size, beyond the one-shot ulthreads' */
#define WORKER_THREAD_STACK_SIZE	4096

class WorkerThread {
public:
	UlthreadRuntime *m_runtime;
	SceUID m_threadId;
	SceFiber m_fiber;
	void *m_stack;				/* the kernel thread's */
	Ulthread *m_caller;			/* the ulthread that switched back, with its request */
	int32_t m_request;			/* WORKER_REQUEST_* */
	int32_t m_requestArg;
	uint32_t reserved;
	uint32_t m_oneShotThreadStackSize;	/* the stack's part a one-shot ulthread runs on */
	uint32_t reserved2[24];

	int initialize(UlthreadRuntime *runtime, const char *name, const WorkerThreadParam *param);
	int start();
	int join();
	int finalize();
	/* the ulthread caller (current) switches back to the worker: what it is switched back with */
	int switchFromUlthread(Ulthread *caller, int32_t request, int32_t arg);
	/* the worker's fiber: what the last ulthread asked, then the next ready one */
	int run();
};

#endif
