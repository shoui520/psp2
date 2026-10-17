/*
 * SceKernelThreadMgr: kernel TLS slots. A driver allocates a slot (2-7)
 * with a size and a destructor; each thread then gets its own zeroed
 * memory for the slot on first use, freed when the thread ends.
 */

#include <string.h>
#include "private.h"
#include <intrmgr/intrmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include "syscall_context.h"

#define TLS_SLOT_FIRST			2
#define TLS_SLOT_COUNT			8
#define SCE_KERNEL_ERROR_TLS_FULL	0x80028081	/* PLACEHOLDER: names */
#define SCE_KERNEL_ERROR_TLS_NOT_ACTIVE	0x80028082
#define SCE_KERNEL_ERROR_TLS_BUSY	0x80028083

extern SceUID ksceKernelGetThreadId(void);
extern void *ksceKernelAllocHeapMemory2(SceUID pid, SceSize size, void *opt);
extern int SceSysrootForKernel_7968F091(SceUID pid, void *memory);

_Static_assert(sizeof(struct ThreadmgrTlsSlot) == 0x14, "TLS descriptor stride");

static inline int driverSlot(uint32_t slot)
{
	return slot - TLS_SLOT_FIRST < TLS_SLOT_COUNT - TLS_SLOT_FIRST;
}

/* SceThreadmgrForDriver_B645C7EF: a free slot for memory of size per thread */
int SceThreadmgrForDriver_B645C7EF(uint32_t size, void (*destructor)(void *, uintptr_t),
				   uintptr_t argument)
{
	SceKernelSpinlock *lock = &s_threadmgrRuntime.manager->threadMgrCBLock;
	struct ThreadmgrTlsSlot *descriptor;
	SceKernelIntrStatus intr = ksceKernelSpinlockLowLockCpuSuspendIntr(lock);
	uint32_t slot;

	for (slot = TLS_SLOT_FIRST; slot < TLS_SLOT_COUNT; slot++) {
		descriptor = &s_threadmgrRuntime.manager->tlsSlots[slot];
		if (descriptor->active)
			continue;
		descriptor->destructor = destructor;
		descriptor->destructorArgument = argument;
		descriptor->active = 1;
		descriptor->liveCount = 0;
		descriptor->size = size;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(lock, intr);
		return slot;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(lock, intr);
	return SCE_KERNEL_ERROR_TLS_FULL;
}

/* SceThreadmgrForDriver_8C3C1F78: a slot no thread uses any more goes back */
int SceThreadmgrForDriver_8C3C1F78(uint32_t slot)
{
	SceKernelSpinlock *lock = &s_threadmgrRuntime.manager->threadMgrCBLock;
	struct ThreadmgrTlsSlot *descriptor;
	SceKernelIntrStatus intr;
	int result;

	if (!driverSlot(slot))
		return SCE_KERNEL_ERROR_TLS_NOT_ACTIVE;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(lock);
	descriptor = &s_threadmgrRuntime.manager->tlsSlots[slot];
	if (!descriptor->active) {
		result = SCE_KERNEL_ERROR_TLS_NOT_ACTIVE;
	} else if (descriptor->liveCount != 0) {
		result = SCE_KERNEL_ERROR_TLS_BUSY;
	} else {
		descriptor->active = 0;
		result = 0;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(lock, intr);
	return result;
}

/*
 * SceThreadmgrForDriver_89CA5698: threadId's (0: the caller's) memory of
 * slot, allocated with create if it has none.
 */
void *SceThreadmgrForDriver_89CA5698(uint32_t slot, SceUID threadId, int create)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrTlsSlot *descriptor;
	SceKernelIntrStatus intr;
	void *memory = NULL;
	void **field;

	if (ksceKernelIsIntrAllowedInCurrentContext() || !driverSlot(slot))
		return NULL;
	if (threadId == 0) {
		memory = currentThreadContext()->tlsSlotMemory.slot[slot];
		if (memory != NULL)
			return memory;
		threadId = ksceKernelGetThreadId();
	}
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(threadId,
	    s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) >= 0) {
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (!(object->context.status & THREAD_STATE_GONE)) {
			field = &object->context.tlsSlotMemory.slot[slot];
			memory = *field;
			if (memory == NULL) {
				ksceKernelSpinlockLowLock(
					&s_threadmgrRuntime.manager->threadMgrCBLock);
				descriptor = &s_threadmgrRuntime.manager->tlsSlots[slot];
				if (descriptor->active && create) {
					memory = ksceKernelAllocHeapMemory2(THREADMGR_KERNEL_PID,
									    descriptor->size, NULL);
					if (memory != NULL) {
						memset(memory, 0, descriptor->size);
						descriptor->liveCount++;
						*field = memory;
					}
				}
				ksceKernelSpinlockLowUnlock(
					&s_threadmgrRuntime.manager->threadMgrCBLock);
			}
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceGUIDReleaseObject(threadId);
	}
	ksceKernelCpuResumeIntr(intr);
	return memory;
}

/* SceThreadmgrForDriver_3A72C6D8: the caller's memory of slot */
void *SceThreadmgrForDriver_3A72C6D8(uint32_t slot)
{
	return SceThreadmgrForDriver_89CA5698(slot, 0, 1);
}

/* A thread's slot memory goes when it does (its destructors first) */
int releaseThreadTlsSlots(struct ThreadCB *thread)
{
	SceKernelSpinlock *lock = &s_threadmgrRuntime.manager->threadMgrCBLock;
	struct ThreadmgrTlsSlot *descriptor;
	void *memory;
	uint32_t slot;

	ksceKernelSpinlockLowLock(lock);
	for (slot = TLS_SLOT_FIRST; slot < TLS_SLOT_COUNT; slot++) {
		memory = thread->tlsSlotMemory.slot[slot];
		if (memory == NULL)
			continue;
		descriptor = &s_threadmgrRuntime.manager->tlsSlots[slot];
		if (descriptor->destructor != NULL)
			descriptor->destructor(memory, descriptor->destructorArgument);
		SceSysrootForKernel_7968F091(THREADMGR_KERNEL_PID, memory);
		thread->tlsSlotMemory.slot[slot] = NULL;
		descriptor->liveCount--;
	}
	ksceKernelSpinlockLowUnlock(lock);
	return 0;
}
