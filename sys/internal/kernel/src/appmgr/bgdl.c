/* Background-download queue storage and publication. */

#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <string.h>

extern SceUID g_appmgrCoreHeap;
extern SceUID g_appmgrAppMutex;

enum {
	BGDL_QUEUE_DATA_SIZE = 0x700,
	BGDL_QUEUE_STORAGE_SIZE = 0x720,
};

/* These operations allocate and copy the entire externally supplied buffer. */
void *g_appmgrBgdlQueueStorage;

int appmgrInitializeBgdlQueueStorage(void)
{
	SceAllocOpt opt;
	int result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);

	if (result >= 0) {
		if (g_appmgrBgdlQueueStorage != NULL)
			sceKernelFreeHeapMemory(g_appmgrCoreHeap, g_appmgrBgdlQueueStorage);
		result = 0;
		opt.mmapKind = 0;
		opt.mmapBase = NULL;
		opt.mmapSize = 0;
		opt.size = sizeof(opt);
		opt.align = 32;
		g_appmgrBgdlQueueStorage = SceSysmemForDriver_49D4DD9B(
			g_appmgrCoreHeap, BGDL_QUEUE_STORAGE_SIZE, &opt);
		if (g_appmgrBgdlQueueStorage != NULL)
			memset(g_appmgrBgdlQueueStorage, 0, BGDL_QUEUE_STORAGE_SIZE);
		else
			result = (int)0x80801006;
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	return result;
}

int SceAppMgrForDriver_5B1BE482(const void *status)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);

	if (result >= 0) {
		if (status != NULL)
			memcpy(g_appmgrBgdlQueueStorage, status, BGDL_QUEUE_DATA_SIZE);
		else
			result = (int)0x80801001;
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	sceSyscallLeave(syscallState);
	return result;
}
