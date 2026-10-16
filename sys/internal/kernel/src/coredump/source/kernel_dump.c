/*
 * Kernel dumps: a kernel component writes its own data into a core file as the
 * system program NPXS19998. One dump at a time, under dumpBufferMutexId:
 * SceCoredumpForDriver_13EF8516 begins it (and takes the mutex),
 * SceCoredumpForDriver_12392973 writes, SceCoredumpForDriver_10863B61 ends it.
 * Not Sony's names (unknown).
 */

#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sce/coredump.h>

extern int oalMutexInit(SceUID *mutexId);
extern int oalMutexFinal(SceUID mutexId);
extern int generatorBeginKernelDump(SceCoredumpTriggerParam param, SceSize payloadSize);
extern int generatorWriteKernelDump(const void *data, SceSize size);
extern int generatorEndKernelDump(void);

/* the request id of the dump in progress; -1 none, -2 the end failed to unlock */
int dumpBufferRequestId = -1;
SceUID dumpBufferMutexId = -1;
SceCoredumpTriggerParam dumpBufferTriggerParam;

int dumpBufferInit(void)
{
	int result;

	dumpBufferMutexId = -1;
	result = oalMutexInit(&dumpBufferMutexId);
	if (result != 0)
		oalMutexFinal(dumpBufferMutexId);
	return result;
}

/* returns the dump's request id, or -1 */
int SceCoredumpForDriver_13EF8516(SceSize payloadSize)
{
	int result;

	if (ksceKernelLockMutex(dumpBufferMutexId, 1, NULL) < 0)
		goto error;
	dumpBufferTriggerParam.titleid_len = 10;
	dumpBufferTriggerParam.titleid = "NPXS19998";
	dumpBufferTriggerParam.triggerData = NULL;
	dumpBufferTriggerParam.app_name_len = 1;
	dumpBufferTriggerParam.app_name = "";
	dumpBufferTriggerParam.appVersion = 1;
	dumpBufferTriggerParam.triggerReason = -1;
	dumpBufferTriggerParam.crash_thid = -1;
	dumpBufferTriggerParam.triggerFlags = 1;
	dumpBufferTriggerParam.size = sizeof(dumpBufferTriggerParam);
	dumpBufferTriggerParam.triggerDataSize = 0;
	dumpBufferTriggerParam.dumpLevel = 0;
	dumpBufferTriggerParam.triggerDataType = 0;
	result = generatorBeginKernelDump(dumpBufferTriggerParam, payloadSize);
	if (result < 0)
		goto error;
	dumpBufferRequestId = result;
	return result;
error:
	dumpBufferRequestId = -1;
	return -1;
}

int SceCoredumpForDriver_12392973(int requestId, const void *data, SceSize size)
{
	if (requestId != dumpBufferRequestId)
		return 0;
	return generatorWriteKernelDump(data, size);
}

int SceCoredumpForDriver_10863B61(int requestId)
{
	int result;

	if (requestId == dumpBufferRequestId)
		generatorEndKernelDump();
	while ((result = ksceKernelUnlockMutex(dumpBufferMutexId, 1)) < 0)
		dumpBufferRequestId = -2;
	return result;
}
