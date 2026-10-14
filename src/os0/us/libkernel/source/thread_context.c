/* User thread metadata preceding the TPIDRURO thread-local storage address. */

#include <sce/libkernel_backends.h>
#include "thread_context.h"

extern void *sceKernelGetThreadTLSAddr(SceUID threadId, unsigned int index);
#include "cpu_context.h"


void *sceKernelGetTLSAddr(unsigned int index)
{
	if (index > 255)
		return NULL;
	return &currentThreadStorage()->kernel.slot[index];
}

/* Name unknown: thread-specific access to the kernel-reserved TLS slots. */
void *SceLibKernel_35D20E49(SceUID threadId, unsigned int index)
{
	struct ThreadLocalStorage *storage;

	if (index > 255)
		return NULL;
	storage = currentThreadStorage();
	if (threadId && threadId != storage->kernel.thread.threadId)
		return sceKernelGetThreadTLSAddr(threadId, index);
	return &storage->kernel.slot[index];
}

SceUID sceKernelGetThreadId(void)
{
	return currentThreadStorage()->kernel.thread.threadId;
}

int sceKernelCheckThreadStack(void)
{
	register intptr_t stack __asm__("sp");
	struct ThreadLocalStorage *storage = currentThreadStorage();

	if ((intptr_t)storage->kernel.thread.stackTop < stack ||
	    stack < (intptr_t)storage->kernel.thread.stackBottom)
		return 0;
	return stack - (intptr_t)storage->kernel.thread.stackBottom;
}

uint32_t sceKernelGetCurrentThreadVfpException(void)
{
	return currentThreadStorage()->kernel.thread.vfpException;
}

int sceKernelGetThreadCurrentPriority(void)
{
	return currentThreadStorage()->kernel.thread.currentPriority;
}

int sceKernelGetThreadCpuAffinityMask(SceUID threadId)
{
	struct ThreadLocalStorage *storage = currentThreadStorage();

	if (threadId && threadId != storage->kernel.thread.threadId)
		return _sceKernelGetThreadCpuAffinityMask(threadId);
	return storage->kernel.thread.cpuAffinityMask;
}
