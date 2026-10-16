/* SCE GPU modification-object wait registry. */
#include <psp2kern/kernel/cpu/spinlock.h>
#include "../../gpu_es4/srvkm/sync_sce.h"
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <sce/debug.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/misc/waitmodobj.c"

typedef struct SceGpuWaitModObj {
	unsigned pollPending;
	int waitEventFlag;
	MODIFY_SYNC_OP_INFO *psModSyncOpInfo;
	struct SceGpuWaitModObj *next;
} SceGpuWaitModObj;

_Static_assert(sizeof(SceGpuWaitModObj) == 16, "native wait registry entry");

static SceKernelIntrStatus waitModObjIntrStatus;
static SceGpuWaitModObj *waitModObjs;
static SceKernelSpinlock waitModObjSpinlock;

void LockWaitModObjList(void)
{
	waitModObjIntrStatus =
		ksceKernelSpinlockLowLockCpuSuspendIntr(&waitModObjSpinlock);
}

void UnlockWaitModObjList(void)
{
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&waitModObjSpinlock,
			waitModObjIntrStatus);
}

int sceGpuModObjPoll(MODIFY_SYNC_OP_INFO *psModSyncOpInfo);
int sceGpuWaitModObjSignal(int waitEventFlag,
		MODIFY_SYNC_OP_INFO *psModSyncOpInfo);
void *OSAllocMem(unsigned size);
void *memset(void *destination, int value, unsigned size);

void sceGpuWaitModObjPollAll(void)
{
	SceGpuWaitModObj *entry;

	LockWaitModObjList();
	for (entry = waitModObjs; entry; entry = entry->next) {
		if (entry->pollPending && !sceGpuModObjPoll(entry->psModSyncOpInfo)) {
			entry->pollPending = 0;
			sceGpuWaitModObjSignal(entry->waitEventFlag, entry->psModSyncOpInfo);
		}
	}
	UnlockWaitModObjList();
}

int sceGpuWaitModObjAdd(MODIFY_SYNC_OP_INFO *psModSyncOpInfo,
		int waitEventFlag)
{
	SceGpuWaitModObj *entry, *first;

	if (!psModSyncOpInfo)
		return (int)0x804c0000;
	if (waitEventFlag <= 0)
		return (int)0x804c0000;
	entry = OSAllocMem(sizeof(*entry));
	if (!entry)
		return (int)0x804c0001;
	memset(entry, 0, sizeof(*entry));
	entry->waitEventFlag = waitEventFlag;
	entry->psModSyncOpInfo = psModSyncOpInfo;
	LockWaitModObjList();
	first = waitModObjs;
	waitModObjs = entry;
	entry->next = first;
	/* SCE increments this same word the bridge exposes as ui32WaitState. */
	psModSyncOpInfo->ui32WaitState++;
	UnlockWaitModObjList();
	return 0;
}


int OSFreeMem(void *memory);

int sceGpuWaitModObjRemove(MODIFY_SYNC_OP_INFO *psModSyncOpInfo)
{
	SceGpuWaitModObj *entry, *previous = NULL;
	int waitEventFlag, ret;

	LockWaitModObjList();
	for (entry = waitModObjs; entry; entry = entry->next) {
		if (entry->psModSyncOpInfo == psModSyncOpInfo)
			break;
		previous = entry;
	}
	if (entry) {
		waitEventFlag = entry->waitEventFlag;
		if (previous)
			previous->next = entry->next;
		else
			waitModObjs = entry->next;
		entry->psModSyncOpInfo->ui32WaitState--;
		OSFreeMem(entry);
		/* Native removal signals the first word, unlike the poll callback. */
		ksceKernelSetEventFlag(waitEventFlag,
				(unsigned)psModSyncOpInfo->hResItem);
		ret = 0;
	} else {
		ret = (int)0x804c0005;
	}
	UnlockWaitModObjList();
	return ret;
}

int sceGpuWaitModObjRemoveAll(int waitEventFlag)
{
	SceGpuWaitModObj *entry, *previous = NULL;

	LockWaitModObjList();
	entry = waitModObjs;
	while (entry) {
		if (entry->waitEventFlag == waitEventFlag) {
			if (previous)
				previous->next = entry->next;
			else
				waitModObjs = entry->next;
			entry->psModSyncOpInfo->ui32WaitState--;
			OSFreeMem(entry);
			entry = previous ? previous->next : waitModObjs;
		} else {
			previous = entry;
			entry = entry->next;
		}
	}
	UnlockWaitModObjList();
	return 0;
}

int sceGpuWaitModObjCheck(MODIFY_SYNC_OP_INFO *psModSyncOpInfo)
{
	SceGpuWaitModObj *entry;
	int ret;

	if (!psModSyncOpInfo)
		return (int)0x804c0000;
	LockWaitModObjList();
	for (entry = waitModObjs; entry; entry = entry->next) {
		if (entry->psModSyncOpInfo == psModSyncOpInfo)
			break;
	}
	SCE_DBG_ASSERT(entry != NULL, 164);
	if (entry) {
		entry->pollPending = 1;
		ret = sceGpuModObjPoll(psModSyncOpInfo);
		if (!ret)
			entry->pollPending = 0;
	} else {
		ret = (int)0x804c0005;
	}
	UnlockWaitModObjList();
	return ret;
}

int InitWaitModObjList(void)
{
	waitModObjSpinlock = 0;
	return 0;
}
