#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <sce/debug.h>
#include "exfatfs_internal.h"

int UVFAT_InitLockMountPoint(SceExfatfsFileSystem *fileSystem)
{
	char name[32] = "SceExfatfsRoot";
	char *end = &name[14];
	const unsigned char *assignName;
	unsigned char character;
	int result;

	assignName = (const unsigned char *)fileSystem->drive->assignName;
	name[14] = assignName[0] - 32;
	character = *++assignName;
	if (character != '0') {
		do {
			*++end = character;
			character = *++assignName;
		} while (character != '0' && end < &name[31]);
	}
	end[1] = 0;
	result = ksceKernelInitializeFastMutex((SceKernelFastMutex *)&fileSystem->rootFastMutex,
		name, 0x2002, NULL);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 246,
			"%s: sceKernelInitializeFastMutex failed[%#x]\n",
			"UVFAT_InitLockMountPoint", result);
	} else
		result = 0;
	return result;
}

int UVFAT_FinalizeLockMountPoint(SceExfatfsFileSystem *fileSystem)
{
	SceKernelFastMutex *mutex = (SceKernelFastMutex *)&fileSystem->rootFastMutex;
	int result;

	if (mutex) {
		result = ksceKernelFinalizeFastMutex(mutex);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 0, 262,
				"%s: sceKernelFinalizeFastMutex failed[%#x]\n",
				"UVFAT_FinalizeLockMountPoint", result);
	}
	return 0;
}

int UVFAT_LockMountPoint(SceExfatfsFileSystem *fileSystem)
{
	SceKernelFastMutex *mutex = (SceKernelFastMutex *)&fileSystem->rootFastMutex;
	int result;

	if (!mutex)
		return 0;
	result = ksceKernelLockFastMutex(mutex);
	if (result >= 0)
		return 0;
	SCE_DBG_PRINTF_LEVEL(2, 0, 282,
		"%s: sceKernelLockFastMutex failed[%#x]\n",
		"UVFAT_LockMountPoint", result);
	return -15;
}

int UVFAT_UnlockMountPoint(SceExfatfsFileSystem *fileSystem)
{
	SceKernelFastMutex *mutex = (SceKernelFastMutex *)&fileSystem->rootFastMutex;
	int result;

	if (!mutex)
		return 0;
	result = ksceKernelUnlockFastMutex(mutex);
	if (result >= 0)
		return 0;
	SCE_DBG_PRINTF_LEVEL(2, 0, 308,
		"%s: sceKernelUnlockFastMutex failed[%#x]\n",
		"UVFAT_UnlockMountPoint", result);
	return -15;
}
