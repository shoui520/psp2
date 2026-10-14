#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include "update_objects.h"

int UpdateUtilLockPupHeader(void)
{
	return ksceKernelLockFastMutex((SceKernelFastMutex *)&updateMutex);
}

int UpdateUtilUnlockPupHeader(void)
{
	return ksceKernelUnlockFastMutex((SceKernelFastMutex *)&updateMutex);
}

int UpdateUtilLockMbr(void)
{
	return ksceKernelLockFastMutex((SceKernelFastMutex *)&updateMbrMutex);
}

int UpdateUtilUnlockMbr(void)
{
	return ksceKernelUnlockFastMutex((SceKernelFastMutex *)&updateMbrMutex);
}

int UpdateUtilLockCom(void)
{
	return ksceKernelLockFastMutex((SceKernelFastMutex *)&updateComMutex);
}

int UpdateUtilUnlockCom(void)
{
	return ksceKernelUnlockFastMutex((SceKernelFastMutex *)&updateComMutex);
}
