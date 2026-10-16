/*
 * SceKernelThreadMgr: thread queries by UID: a TLS slot's address, and
 * whether a thread is outside the process's object limits.
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#define TLS_SLOTS			0x100
#define TLS_SLOT_AREA			0x800	/* the slots: below the TLS pointer */
#define THREAD_ATTR_UNLIMITED_SHIFT	14
#define UNKNOWN_THREAD			0x40000000	/* PLACEHOLDER: what an unknown UID gets */

/* ksceKernelGetThreadTLSAddr: uid's TLS slot index (NULL if none) */
void *ksceKernelGetThreadTLSAddr(SceUID uid, unsigned int index)
{
	struct ThreadmgrThreadObject *object;
	SceKernelIntrStatus intr;
	uint32_t saved;
	void *result = NULL;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (index < TLS_SLOTS) {
		intr = ksceKernelCpuSuspendIntr();
		if (ksceGUIDReferObjectWithClass(uid,
		    s_threadmgrRuntime.manager->threadClass,
		    (SceObjectBase **)&object) >= 0) {
			if (object->context.tls != 0)
				result = (void *)(object->context.tls - TLS_SLOT_AREA +
						  index * sizeof(uint32_t));
			ksceGUIDReleaseObject(uid);
		}
		ksceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* SceThreadmgrForDriver_1096042E: whether uid (0: the caller) has attribute 0x4000 */
int SceThreadmgrForDriver_1096042E(SceUID uid)
{
	struct ThreadmgrThreadObject *object;
	int result;

	if (uid == 0) {
		if (currentThreadContext() == NULL)
			return 0;
		return (currentThreadContext()->currentAttr >> THREAD_ATTR_UNLIMITED_SHIFT) & 1;
	}
	if (ksceGUIDReferObjectWithClass(uid, s_threadmgrRuntime.manager->threadClass,
					 (SceObjectBase **)&object) < 0)
		return UNKNOWN_THREAD;
	result = (object->context.currentAttr >> THREAD_ATTR_UNLIMITED_SHIFT) & 1;
	ksceGUIDReleaseObject(uid);
	return result;
}
