/*
 * ThreadFactory.h: the thread factories: what a caller creates and joins its threads with,
 * without knowing whether they are ulthreads of a runtime or kernel threads. A factory is
 * used through its interface: its functions, given the factory.
 *
 * Sony's SDK sample tutorial_multi_threads carries the same factories as C++ classes
 * (thread::ThreadFactoryInterface, UlthreadFactory, KernelThreadFactory); the names here are
 * its names.
 */
#ifndef THREAD_FACTORY_H
#define THREAD_FACTORY_H

#include "NBQueue.h"

#define THREAD_FACTORY_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("ThreadFactory.h", line, text, actual, expected)

namespace Impl {

typedef intptr_t id_t;
typedef int32_t (*ThreadEntry)(uint32_t arg);

/* what a factory does, each function given the factory */
struct ThreadFactoryFunctions {
	int (*createThread)(void *factory, id_t *thread, ThreadEntry entry, uint32_t arg);
	uint32_t (*getNumMaxThreads)(void *factory);
	int (*joinThread)(void *factory, id_t id, int32_t *status);
	uint32_t reserved[253];
};

/* a factory as its users see it: the start of every factory */
struct ThreadFactoryInterface {
	const ThreadFactoryFunctions *functions;
	void *factory;
};

/* threads that are ulthreads of a runtime, at most m_maxThreads of them at a time */
class UlthreadFactory {
public:
	struct ThreadInfo {
		SceUltUlthread ulthread;
		void *context;
		id_t id;
		NBQueueEntry2 *entry;		/* its entry, out of the free queue while it runs */
	};

	ThreadFactoryInterface m_interface;
	SceUltUlthreadRuntime *m_runtime;
	uint32_t m_maxThreads;
	uint32_t m_sizeContext;
	NBQueueEntry2 *m_entries;
	NBQueue3 m_freeQueue;			/* the entries of the threads not running */
	NBQueueEntry2 m_dummy;
	ThreadInfo *m_threads;

	static const ThreadFactoryFunctions s_functions;

	/* the work area: the threads, their entries, then their contexts */
	ULT_INLINE static uint32_t getWorkAreaSize(uint32_t maxThreads, uint32_t sizeContext)
	{
		return (sizeof(ThreadInfo) + sizeof(NBQueueEntry2) + sizeContext) * maxThreads;
	}

	ULT_INLINE int initialize(SceUltUlthreadRuntime *runtime, uint32_t maxThreads,
				  uint32_t sizeContext, void *workArea)
	{
		m_runtime = runtime;
		m_maxThreads = maxThreads;
		m_sizeContext = sizeContext;
		m_threads = (ThreadInfo *)workArea;
		m_entries = (NBQueueEntry2 *)&m_threads[maxThreads];
		char *context = (char *)&m_entries[maxThreads];
		m_freeQueue.initialize(&m_dummy);
		for (uint32_t i = 0; i < m_maxThreads; i++) {
			ThreadInfo *info = &m_threads[i];
			info->context = context;
			info->id = i;
			NBQueueEntry2 *entry = &m_entries[i];
			context += m_sizeContext;
			entry->data = (uint32_t)info;
			int ret = m_freeQueue.push(entry);
			THREAD_FACTORY_ASSERT_EQUAL(139, "ret==SCE_OK", ret, SCE_OK);
		}
		m_interface.factory = this;
		m_interface.functions = &s_functions;
		return SCE_OK;
	}

	ULT_INLINE int finalize()
	{
		return SCE_OK;
	}

	/* a free entry's thread, created on the runtime with its context */
	ULT_INLINE int createThread(id_t *thread, ThreadEntry entry, uint32_t arg)
	{
		NBQueueEntry2 *e = m_freeQueue.pop();
		if (e == NULL)
			return SCE_ULT_ERROR_STATE;
		ThreadInfo *info = (ThreadInfo *)e->data;
		info->entry = e;
		*thread = info->id;
		return sceUltUlthreadCreate(&info->ulthread, "ulthread", entry, arg, info->context,
					    m_sizeContext, m_runtime, NULL);
	}

	ULT_INLINE uint32_t getNumMaxThreads()
	{
		return m_maxThreads;
	}

	/* the thread's entry goes back to the free queue once it has ended */
	ULT_INLINE int joinThread(id_t id, int32_t *status)
	{
		int ret = sceUltUlthreadJoin(&m_threads[id].ulthread, status);
		if (ret != SCE_OK)
			return ret;
		m_freeQueue.pushInline(m_threads[id].entry);
		return SCE_OK;
	}

	/* the interface's functions (ThreadFactory.cpp), each given the factory */
	static int createThread(void *factory, id_t *thread, ThreadEntry entry, uint32_t arg);
	static uint32_t getNumMaxThreads(void *factory);
	static int joinThread(void *factory, id_t id, int32_t *status);
};

/* threads that are kernel threads */
class KernelThreadFactory {
public:
	/* what a kernel thread is started with */
	struct StrArg {
		ThreadEntry entry;
		uint32_t arg;
	};

	ThreadFactoryInterface m_interface;
	uint32_t m_maxThreads;
	int32_t m_initPriority;
	uint32_t m_stackSize;

	static const ThreadFactoryFunctions s_functions;

	ULT_INLINE int initialize(int32_t initPriority, uint32_t stackSize)
	{
		m_initPriority = initPriority;
		m_stackSize = stackSize;
		m_interface.factory = this;
		m_interface.functions = &s_functions;
		return SCE_OK;
	}

	ULT_INLINE int finalize()
	{
		return SCE_OK;
	}

	/* the kernel thread runs the entry and exits with what it returns */
	static int32_t kernelThreadEntry(SceSize argSize, void *pArgBlock)
	{
		StrArg *strArg = (StrArg *)pArgBlock;

		SceThreadmgrCoredumpTime_sceKernelExitThread(strArg->entry(strArg->arg));
		return SCE_OK;
	}

	ULT_INLINE int createThread(id_t *thread, ThreadEntry entry, uint32_t arg)
	{
		StrArg strArg;

		strArg.entry = entry;
		strArg.arg = arg;
		SceUID thid = sceKernelCreateThread("factory thread", kernelThreadEntry,
						    m_initPriority, m_stackSize, 0,
						    SCE_KERNEL_CPU_MASK_USER_ALL, NULL);
		if (thid < 0)
			return thid;
		*thread = thid;
		int ret = sceKernelStartThread(thid, sizeof(strArg), &strArg);
		THREAD_FACTORY_ASSERT_EQUAL(281, "ret==SCE_OK", ret, SCE_OK);
		return SCE_OK;
	}

	ULT_INLINE uint32_t getNumMaxThreads()
	{
		return 1024;
	}

	ULT_INLINE int joinThread(id_t id, int32_t *status)
	{
		int ret = sceKernelWaitThreadEnd(id, status, NULL);
		if (ret < 0)
			return ret;
		ret = sceKernelDeleteThread(id);
		THREAD_FACTORY_ASSERT_EQUAL(314, "ret==SCE_OK", ret, SCE_OK);
		return SCE_OK;
	}

	/* the interface's functions (ThreadFactory.cpp), each given the factory */
	static int createThread(void *factory, id_t *thread, ThreadEntry entry, uint32_t arg);
	static uint32_t getNumMaxThreads(void *factory);
	static int joinThread(void *factory, id_t id, int32_t *status);
};

}

#endif
