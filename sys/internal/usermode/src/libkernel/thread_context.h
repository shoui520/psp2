#ifndef LIBKERNEL_THREAD_CONTEXT_H
#define LIBKERNEL_THREAD_CONTEXT_H

#include <sysmem/cp15.h>
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

union KernelThreadSlots {
	struct {
		uint32_t reserved;
		SceUID threadId;
		void *stackTop;
		void *stackBottom;
		uint32_t vfpException;
		uint32_t cannotWait;	/* waits fail with SCE_KERNEL_ERROR_CAN_NOT_WAIT */
		uint32_t stackGuard[2];
		int currentPriority;
		uint32_t cpuAffinityMask;
	} thread;
	uint32_t slot[256];
};

struct ThreadLocalStorage {
	union KernelThreadSlots kernel;
	unsigned char reserved[0x400];
	unsigned char user[];
};

_Static_assert(offsetof(struct ThreadLocalStorage, user) == 0x800,
	       "stack-switch TLS displacement");
_Static_assert(offsetof(union KernelThreadSlots, thread.stackTop) == 8,
	       "stack-switch bounds displacement");

static inline __attribute__((always_inline))
struct ThreadLocalStorage *currentThreadStorage(void)
{
	uintptr_t address = readTpidruro();

	return (struct ThreadLocalStorage *)(address -
					   offsetof(struct ThreadLocalStorage, user));
}

#endif
