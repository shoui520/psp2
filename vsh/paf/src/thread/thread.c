#include "thread.h"
#include "../common/halt_assert.h"
#include "../file/localfile.h"
#include "../std/new.h"
#include "../std/string.h"
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>

/* Extended kernel option: attr selects which trailing fields the kernel reads. */
typedef struct ThreadKernelOpt {
	SceSize size;
	SceUInt32 attr;
	SceUInt32 kStackMemType;
	SceUInt32 uStackMemType;
	SceUInt32 uTLSMemType;
	SceUInt32 uStackMemid;
	SceUInt32 handedExceptions;
} ThreadKernelOpt;

typedef char ThreadKernelOptSizeCheck[sizeof(ThreadKernelOpt) == 28 ? 1 : -1];

static ThreadOpt threadDefaultOpt;

const ThreadClass threadClass = {
	0, NULL,
	{
		ThreadDestroy,
		ThreadDelete,
		(void (*)(Thread *))__cxa_pure_virtual
	}
};

static inline __attribute__((always_inline))
Thread *ThreadConstructImpl(Thread *self, SceInt32 priority, SceSize stackSize,
			    const char *name, const ThreadOpt *opt)
{
	ThreadKernelOpt option;
	const SceKernelThreadOptParam *param = NULL;
	SceInt32 affinity;
	SceUInt32 attr;
	SceUInt32 stackMemoryType;

	self->vtable = &threadClass.methods;
	self->m_canceled = 0;
	self->m_alive = 0;
	self->m_started = 0;
	if (!opt)
		opt = &threadDefaultOpt;
	affinity = opt->cpuAffinityMask;
	attr = opt->attr;
	if (priority >= 192 && priority <= 251 && !affinity)
		affinity = ThreadAffinity_AllCores;
	stackMemoryType = opt->stackMemoryType;
	if (stackMemoryType != SCE_KERNEL_MEMBLOCK_TYPE_USER_RW) {
		option.uStackMemType = stackMemoryType;
		option.uTLSMemType = stackMemoryType;
		option.size = sizeof(option);
		option.attr = 6;
		param = (const SceKernelThreadOptParam *)&option;
	}
	self->m_thid = sceKernelCreateThread(name, ThreadInternalThreadEntry,
					   priority, stackSize, attr, affinity, param);
	if (self->m_thid < 0)
		on_error();
	return self;
}

Thread *ThreadConstructBase(Thread *self, SceInt32 priority, SceSize stackSize,
			    const char *name, const ThreadOpt *opt)
{
	return ThreadConstructImpl(self, priority, stackSize, name, opt);
}

Thread *ThreadConstruct(Thread *self, SceInt32 priority, SceSize stackSize,
			const char *name, const ThreadOpt *opt)
{
	return ThreadConstructImpl(self, priority, stackSize, name, opt);
}

Thread *ThreadDestroy(Thread *self)
{
	SceUID thid = self->m_thid;

	self->vtable = &threadClass.methods;
	if (thid >= 0)
		sceKernelDeleteThread(thid);
	return self;
}

void ThreadDelete(Thread *self)
{
	SceUID thid = self->m_thid;

	self->vtable = &threadClass.methods;
	if (thid >= 0)
		sceKernelDeleteThread(thid);
	ScePafStdc_1B77082E(self);
}

Thread *ThreadDestroyBase(Thread *self)
{
	SceUID thid = self->m_thid;

	self->vtable = &threadClass.methods;
	if (thid >= 0)
		sceKernelDeleteThread(thid);
	return self;
}

SceInt32 ThreadStart(Thread *self)
{
	SceInt32 result;
	SceUID thid;
	Thread *arg;

	if (self->m_started)
		return SCE_PAF_ERROR_THREAD_THREAD_ALREADY_STARTED;
	thid = self->m_thid;
	self->m_alive = 1;
	if (thid >= 0) {
		arg = self;
		result = sceKernelStartThread(thid, sizeof(arg), &arg);
		if (!result) {
			self->m_started = 1;
			return 0;
		}
		on_error();
	} else {
		result = thid;
	}
	self->m_alive = 0;
	return result;
}

void ThreadJoin(Thread *self)
{
	SceUID thid = self->m_thid;

	if (thid >= 0)
		sceKernelWaitThreadEnd(thid, NULL, NULL);
}

void ThreadCancel(Thread *self)
{
	self->m_canceled = 1;
}

unsigned char ThreadIsCanceled(Thread *self)
{
	return self->m_canceled;
}

unsigned char ThreadIsAlive(Thread *self)
{
	return self->m_alive;
}

SceUID ThreadGetID(Thread *self)
{
	return self->m_thid;
}

void ThreadChangePriority(Thread *self, SceInt32 priority)
{
	SceUID thid = self->m_thid;

	if (thid >= 0)
		sceKernelChangeThreadPriority(thid, priority);
}

SceUInt32 ThreadSetDefaultOpt(const ThreadOpt *opt)
{
	threadDefaultOpt.attr = opt->attr;
	threadDefaultOpt.cpuAffinityMask = opt->cpuAffinityMask;
	threadDefaultOpt.stackMemoryType = opt->stackMemoryType;
	return threadDefaultOpt.stackMemoryType;
}

SceInt32 ThreadInternalThreadEntry(SceSize argSize, void *argBlock)
{
	Thread *self;

	LocalFileSetDefaultPriority(14);
	self = *(Thread **)argBlock;
	self->vtable->EntryFunction(self);
	self->m_alive = 0;
	return 0;
}

ThreadOpt *ThreadOptConstruct(ThreadOpt *self)
{
	self->attr = 0;
	self->cpuAffinityMask = 0;
	self->stackMemoryType = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
	return self;
}

ThreadOpt *ThreadOptConstructBase(ThreadOpt *self)
{
	self->attr = 0;
	self->cpuAffinityMask = 0;
	self->stackMemoryType = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
	return self;
}

ThreadOpt *ThreadDefaultOptInitialize(void)
{
	return ThreadOptConstruct(&threadDefaultOpt);
}
