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
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>

#define TLS_SLOT_FIRST			2
#define TLS_SLOT_COUNT			8

_Static_assert(sizeof(struct ThreadmgrTlsSlot) == 0x14, "TLS descriptor stride");

static inline int driverSlot(uint32_t slot)
{
	return slot - TLS_SLOT_FIRST < TLS_SLOT_COUNT - TLS_SLOT_FIRST;
}

/* SceThreadmgrForDriver_B645C7EF: a free slot for memory of size per thread */
int SceThreadmgrForDriver_B645C7EF(uint32_t size, void (*destructor)(void *, uintptr_t),
				   uintptr_t argument)
{
	struct ThreadmgrTlsSlot *descriptor;
	SceKernelIntrStatus intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_threadmgrRuntime.manager->threadMgrCBLock);
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
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
		return slot;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
	return SCE_KERNEL_ERROR_KERNEL_TLS_FULL;
}

/* SceThreadmgrForDriver_8C3C1F78: a slot no thread uses any more goes back */
int SceThreadmgrForDriver_8C3C1F78(uint32_t slot)
{
	struct ThreadmgrTlsSlot *descriptor;
	SceKernelIntrStatus intr;
	int result;

	if (!driverSlot(slot))
		return SCE_KERNEL_ERROR_ILLEGAL_KERNEL_TLS_INDEX;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_threadmgrRuntime.manager->threadMgrCBLock);
	descriptor = &s_threadmgrRuntime.manager->tlsSlots[slot];
	if (!descriptor->active) {
		result = SCE_KERNEL_ERROR_ILLEGAL_KERNEL_TLS_INDEX;
	} else if (descriptor->liveCount != 0) {
		result = SCE_KERNEL_ERROR_KERNEL_TLS_BUSY;
	} else {
		descriptor->active = 0;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
	return result;
}

/*
 * SceThreadmgrForDriver_89CA5698: threadId's (0: the caller's) memory of
 * slot, allocated with create if it has none.
 */
void *SceThreadmgrForDriver_89CA5698(uint32_t slot, SceUID threadId, int create)
{
	struct SceUIDThreadObject *object;
	SceObjectBase *pObject;
	struct ThreadmgrTlsSlot *descriptor;
	SceKernelIntrStatus intr;
	void *memory = NULL;
	void **field;

	if (SceIntrmgrForDriver_182EE3E3() || !driverSlot(slot))
		return NULL;
	if (threadId == 0) {
		memory = currentThreadContext()->tlsSlotMemory.slot[slot];
		if (memory != NULL)
			return memory;
		threadId = sceKernelGetThreadId();
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(threadId,
	    s_threadmgrRuntime.manager->threadClass, &pObject) >= 0) {
		object = (struct SceUIDThreadObject *)pObject;
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (!(object->context.status & THREAD_STATE_GONE)) {
			field = &object->context.tlsSlotMemory.slot[slot];
			memory = *field;
			if (memory == NULL) {
				sceKernelSpinlockLowLock(
					&s_threadmgrRuntime.manager->threadMgrCBLock);
				descriptor = &s_threadmgrRuntime.manager->tlsSlots[slot];
				if (descriptor->active && create) {
					memory = sceKernelSysrootAllocRemoteProcessHeap(
						SCE_KERNEL_PROCESS_ID, descriptor->size, NULL);
					if (memory != NULL) {
						memset(memory, 0, descriptor->size);
						descriptor->liveCount++;
						*field = memory;
					}
				}
				sceKernelSpinlockLowUnlock(
					&s_threadmgrRuntime.manager->threadMgrCBLock);
			}
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
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
	struct ThreadmgrTlsSlot *descriptor;
	void *memory;
	uint32_t slot;

	sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	for (slot = TLS_SLOT_FIRST; slot < TLS_SLOT_COUNT; slot++) {
		memory = thread->tlsSlotMemory.slot[slot];
		if (memory == NULL)
			continue;
		descriptor = &s_threadmgrRuntime.manager->tlsSlots[slot];
		if (descriptor->destructor != NULL)
			descriptor->destructor(memory, descriptor->destructorArgument);
		SceSysrootForKernel_7968F091(SCE_KERNEL_PROCESS_ID, memory);
		thread->tlsSlotMemory.slot[slot] = NULL;
		descriptor->liveCount--;
	}
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	return 0;
}
