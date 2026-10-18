#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include "update_objects.h"

int UpdateUtilLockPupHeader(void)
{
	return lockFastMutex((SceKernelFastMutex *)&updateMutex);
}

int UpdateUtilUnlockPupHeader(void)
{
	return sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateMutex);
}

int UpdateUtilLockMbr(void)
{
	return lockFastMutex((SceKernelFastMutex *)&updateMbrMutex);
}

int UpdateUtilUnlockMbr(void)
{
	return sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateMbrMutex);
}

int UpdateUtilLockCom(void)
{
	return lockFastMutex((SceKernelFastMutex *)&updateComMutex);
}

int UpdateUtilUnlockCom(void)
{
	return sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateComMutex);
}
