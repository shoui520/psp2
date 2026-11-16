/*
 * WorkerThread.cpp: an ulthread runtime's workers.
 */
#include "Ulthread.h"

#define WORKER_THREAD_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("WorkerThread.cpp", line, text, cond)
#define WORKER_THREAD_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("WorkerThread.cpp", line, text, actual, expected)

/* the kernel thread runs the worker's fiber, and exits with what it returns */
static int32_t threadEntry(SceSize argSize, void *argp)
{
	WorkerThread *worker = *(WorkerThread **)argp;
	SceUInt32 argOnRun = 0;

	int ret = sceFiberRun(&worker->m_fiber, 0, &argOnRun);
	WORKER_THREAD_ASSERT_EQUAL(88, "ret==SCE_OK", ret, SCE_OK);
	SceThreadmgrCoredumpTime_sceKernelExitThread(argOnRun);
	return SCE_OK;
}

int WorkerThread::run()
{
	if (m_caller != NULL) {
		int ret;

		switch (m_request) {
		case WORKER_REQUEST_EXIT:
			ret = m_caller->setExit();
			WORKER_THREAD_ASSERT_EQUAL(219, "ret==SCE_OK", ret, SCE_OK);
			break;
		case WORKER_REQUEST_YIELD:
			ret = m_caller->setReady();
			WORKER_THREAD_ASSERT_EQUAL(225, "ret==SCE_OK", ret, SCE_OK);
			ret = m_runtime->pushReady(m_caller);
			WORKER_THREAD_ASSERT_EQUAL(228, "ret==SCE_OK", ret, SCE_OK);
			break;
		case WORKER_REQUEST_WAIT:
			ret = m_caller->setWaiting();
			WORKER_THREAD_ASSERT(235, "ret >= 0", ret >= 0);
			if (ret == SCE_OK) {
				/* a signal came first: it runs on */
				ret = m_caller->setReady();
				WORKER_THREAD_ASSERT_EQUAL(238, "ret==SCE_OK", ret, SCE_OK);
				m_caller->run(this);
			} else {
				WORKER_THREAD_ASSERT_EQUAL(242, "ret==Ulthread::START_WAITING", ret,
							   Ulthread::START_WAITING);
			}
			break;
		default:
			__breakpoint(0);
			for (;;)
				;
		}
	}
	__builtin_dmb();
	Ulthread *next = m_runtime->popReady();
	if (next != NULL)
		next->run(this);
	return -1;
}

/* the worker's fiber: it ends (back to the kernel thread) when the runtime stops it */
static void fiberEntry(SceUInt32 argOnInitialize, SceUInt32 argOnRun)
	__attribute__((noreturn));

static void fiberEntry(SceUInt32 argOnInitialize, SceUInt32 argOnRun)
{
	WorkerThread *worker = (WorkerThread *)argOnInitialize;

	WORKER_THREAD_ASSERT(119, "worker != NULL", worker != NULL);
	int ret = worker->run();
	WORKER_THREAD_ASSERT(122, "ret == -1", ret == -1);
	ret = sceFiberReturnToThread(0, NULL);
	WORKER_THREAD_ASSERT_EQUAL(125, "ret==SCE_OK", ret, SCE_OK);
	__breakpoint(0);
	for (;;)
		;
}

int WorkerThread::initialize(UlthreadRuntime *runtime, const char *name,
			     const WorkerThreadParam *param)
{
	SceKernelThreadInfo info;

	m_runtime = runtime;
	m_caller = NULL;
	m_request = 0;
	m_requestArg = 0;
	int ret = _sceFiberInitializeWithInternalOptionImpl(&m_fiber, name, fiberEntry,
							    (SceUInt32)this, NULL, 0, NULL,
							    ULT_FIBER_FLAG_NO_RAZOR, ULT_VERSION);
	WORKER_THREAD_ASSERT_EQUAL(31, "ret==SCE_OK", ret, SCE_OK);
	m_threadId = sceKernelCreateThread(name, threadEntry, param->priority,
					   param->stackSize + param->oneShotThreadStackSize,
					   param->attr, param->cpuAffinityMask, param->optParam);
	if (m_threadId < 0)
		return m_threadId;
	info.size = sizeof(info);
	int ret2 = sceKernelGetThreadInfo(m_threadId, &info);
	WORKER_THREAD_ASSERT_EQUAL(47, "ret2==SCE_OK", ret2, SCE_OK);
	m_stack = info.pStack;
	m_oneShotThreadStackSize = param->oneShotThreadStackSize;
	return SCE_OK;
}

int WorkerThread::start()
{
	WorkerThread *worker = this;

	int ret = sceKernelStartThread(m_threadId, sizeof(worker), &worker);
	WORKER_THREAD_ASSERT_EQUAL(60, "ret==SCE_OK", ret, SCE_OK);
	return SCE_OK;
}

int WorkerThread::join()
{
	int ret = sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
	WORKER_THREAD_ASSERT_EQUAL(67, "ret==SCE_OK", ret, SCE_OK);
	return SCE_OK;
}

int WorkerThread::finalize()
{
	int ret = sceKernelDeleteThread(m_threadId);
	WORKER_THREAD_ASSERT_EQUAL(74, "ret==SCE_OK", ret, SCE_OK);
	sceFiberFinalize(&m_fiber);
	return SCE_OK;
}

int WorkerThread::switchFromUlthread(Ulthread *caller, int32_t request, int32_t arg)
{
	SceUInt32 argOnRun;

	/* a wait may be asked on behalf of the ulthread by another thread */
	if (request != WORKER_REQUEST_WAIT)
		WORKER_THREAD_ASSERT_EQUAL(98, "AbstractThread::getID2()==caller->getAbstractThread()",
					   (int)AbstractThread::getID2(),
					   (int)caller->getAbstractThread());
	AbstractThread::leave(caller->getAbstractThread());
	m_caller = caller;
	m_request = request;
	m_requestArg = arg;
	int ret = sceFiberSwitch(&m_fiber, 0, &argOnRun);
	WORKER_THREAD_ASSERT_EQUAL(109, "ret==SCE_OK", ret, SCE_OK);
	return argOnRun;
}
