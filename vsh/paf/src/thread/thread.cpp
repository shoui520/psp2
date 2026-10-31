/*
 * paf::thread::Thread: a kernel thread that runs the object's EntryFunction().
 * Threads in the priority band shared by all cores run on any core unless
 * their option pins them, and a stack in other than user RW memory takes the
 * kernel's extended option.
 */
#include <thread/thread.h>
#include <common/common_internal.h>
#include <kernel/error.h>
#include <kernel/sysmem/memblock.h>
#include <file/localfile.h>

namespace paf {
namespace thread {

/* sceKernelCreateThread's option; attr selects which fields after it the kernel reads. */
struct ThreadKernelOpt {
	SceSize size;
	SceUInt32 attr;
	SceUInt32 kStackMemType;
	SceUInt32 uStackMemType;
	SceUInt32 uTLSMemType;
	SceUInt32 uStackMemid;
	SceUInt32 handedExceptions;
};

enum {
	THREAD_KERNEL_OPT_ATTR_STACK_MEM_TYPE = 6,	/* uStackMemType and uTLSMemType */
	THREAD_ALL_CORES = 0x000f0000,
	THREAD_ALL_CORES_PRIORITY_MIN = 192,
	THREAD_ALL_CORES_PRIORITY_MAX = 251,
	THREAD_FILE_PRIORITY = 14
};

static Thread::Option s_default_option;

Thread::Thread(int priority, size_t stackSize, const char *name, const Option *option)
{
	ThreadKernelOpt kernelOpt;
	ThreadKernelOpt *pKernelOpt = NULL;

	m_canceled = false;
	m_alive = false;
	m_started = false;
	if (option == NULL)
		option = &s_default_option;

	SceInt32 cpuAffinityMask = option->cpuAffinityMask;
	SceUInt32 attr = option->attr;

	if (priority >= THREAD_ALL_CORES_PRIORITY_MIN && priority <= THREAD_ALL_CORES_PRIORITY_MAX &&
	    cpuAffinityMask == 0)
		cpuAffinityMask = THREAD_ALL_CORES;

	SceUInt32 stackMemoryType = option->stackMemoryType;

	if (stackMemoryType != SCE_KERNEL_MEMBLOCK_TYPE_USER_RW) {
		kernelOpt.uStackMemType = stackMemoryType;
		kernelOpt.uTLSMemType = stackMemoryType;
		kernelOpt.size = sizeof(kernelOpt);
		kernelOpt.attr = THREAD_KERNEL_OPT_ATTR_STACK_MEM_TYPE;
		pKernelOpt = &kernelOpt;
	}
	m_thid = sceKernelCreateThread(name, ThreadFunc, priority, stackSize, attr, cpuAffinityMask,
				       (const SceKernelThreadOptParam *)pKernelOpt);
	if (m_thid < 0)
		common_internal::on_error();
}

Thread::~Thread()
{
	if (m_thid >= 0)
		sceKernelDeleteThread(m_thid);
}

int Thread::Start()
{
	if (m_started)
		return ERROR_ALREADY_STARTED;

	int result;
	SceUID thid = m_thid;

	m_alive = true;
	if (thid >= 0) {
		Thread *self = this;

		result = sceKernelStartThread(thid, sizeof(self), &self);
		if (result == SCE_OK) {
			m_started = true;
			return SCE_OK;
		}
		common_internal::on_error();
	} else {
		result = thid;
	}
	m_alive = false;
	return result;
}

void Thread::Join()
{
	if (m_thid >= 0)
		sceKernelWaitThreadEnd(m_thid, NULL, NULL);
}

void Thread::Cancel()
{
	m_canceled = true;
}

bool Thread::IsCanceled()
{
	return m_canceled;
}

bool Thread::IsAlive()
{
	return m_alive;
}

SceUID Thread::GetID()
{
	return m_thid;
}

void Thread::ChangePriority(int priority)
{
	if (m_thid >= 0)
		sceKernelChangeThreadPriority(m_thid, priority);
}

void Thread::SetDefaultOption(const Option &option)
{
	s_default_option = option;
}

SceInt32 Thread::ThreadFunc(SceSize argSize, void *argBlock)
{
	LocalFile::SetThreadDefaultPriority(THREAD_FILE_PRIORITY);

	Thread *self = *(Thread **)argBlock;

	self->EntryFunction();
	self->m_alive = false;
	return 0;
}

Thread::Option::Option()
{
	attr = 0;
	cpuAffinityMask = 0;
	stackMemoryType = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
}

}
}
