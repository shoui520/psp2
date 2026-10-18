/* Public thread-object APIs marshal user arguments to their syscall ABI. */

#include "libkernel_backends.h"
#include <kernel/threadmgr.h>

struct SemaCreateOptions {
	int maximumCount;
	const SceKernelSemaOptParam *option;
};

struct ObjectInfoRequest {
	unsigned int size;
};


struct EventFlagWaitOptions {
	unsigned int *resultBits;
	SceUInt *timeout;
};



SceUID sceKernelCreateSema(const char *name, SceUInt attributes,
			 int initialCount, int maximumCount,
			 const SceKernelSemaOptParam *option)
{
	struct SemaCreateOptions options = { maximumCount, option };

	return _sceKernelCreateSema(name, attributes, initialCount, &options);
}

SceUID sceKernelCreateSema_16XX(const char *name, SceUInt attributes,
			      int initialCount, int maximumCount,
			      SceKernelSemaOptParam *option)
{
	struct SemaCreateOptions options = { maximumCount, option };

	return _sceKernelCreateSema_16XX(name, attributes, initialCount, &options);
}

int sceKernelWaitSema(SceUID id, int count, SceUInt *timeout)
{
	return _sceKernelWaitSema(id, count, timeout);
}

int sceKernelWaitSemaCB(SceUID id, int count, SceUInt *timeout)
{
	return _sceKernelWaitSemaCB(id, count, timeout);
}

int sceKernelCancelSema(SceUID id, int count, SceUInt32 *waiting)
{
	return _sceKernelCancelSema(id, count, waiting);
}

int sceKernelGetSemaInfo(SceUID id, SceKernelSemaInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetSemaInfo(id, info, &request);
}

SceUID sceKernelCreateMutex(const char *name, SceUInt attributes,
			  int initialCount, const SceKernelMutexOptParam *option)
{
	return _sceKernelCreateMutex(name, attributes, initialCount, option);
}

int sceKernelLockMutex(SceUID id, int count, unsigned int *timeout)
{
	return _sceKernelLockMutex(id, count, timeout);
}

int sceKernelLockMutexCB(SceUID id, int count, unsigned int *timeout)
{
	return _sceKernelLockMutexCB(id, count, timeout);
}

int sceKernelCancelMutex(SceUID id, int count, SceUInt32 *waiting)
{
	return _sceKernelCancelMutex(id, count, waiting);
}

int sceKernelGetMutexInfo(SceUID id, SceKernelMutexInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetMutexInfo(id, info, &request);
}

SceUID sceKernelCreateEventFlag(const char *name, SceUInt32 attributes, SceUInt32 pattern,
			      const SceKernelEventFlagOptParam *option)
{
	return _sceKernelCreateEventFlag(name, attributes, pattern, option);
}

int sceKernelWaitEventFlag(SceUID id, unsigned int bits, unsigned int mode,
			   unsigned int *resultBits, SceUInt *timeout)
{
	struct EventFlagWaitOptions options = { resultBits, timeout };

	return _sceKernelWaitEventFlag(id, bits, mode, &options);
}

int sceKernelWaitEventFlagCB(SceUID id, unsigned int bits, unsigned int mode,
			     unsigned int *resultBits, SceUInt *timeout)
{
	struct EventFlagWaitOptions options = { resultBits, timeout };

	return _sceKernelWaitEventFlagCB(id, bits, mode, &options);
}

int sceKernelPollEventFlag(SceUID id, unsigned int bits, unsigned int mode,
			   unsigned int *resultBits)
{
	return _sceKernelPollEventFlag(id, bits, mode, resultBits);
}

int sceKernelCancelEventFlag(SceUID id, unsigned int bits, unsigned int *waiting)
{
	return _sceKernelCancelEventFlag(id, bits, waiting);
}

int sceKernelGetEventFlagInfo(SceUID id, SceKernelEventFlagInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetEventFlagInfo(id, info, &request);
}

SceUID sceKernelCreateCond(const char *name, SceUInt attributes, SceUID mutexId,
			 const SceKernelCondOptParam *option)
{
	return _sceKernelCreateCond(name, attributes, mutexId, option);
}

int sceKernelWaitCond(SceUID id, unsigned int *timeout)
{
	return _sceKernelWaitCond(id, timeout);
}

int sceKernelWaitCondCB(SceUID id, unsigned int *timeout)
{
	return _sceKernelWaitCondCB(id, timeout);
}

int sceKernelGetCondInfo(SceUID id, SceKernelCondInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetCondInfo(id, info, &request);
}

SceUID sceKernelCreateRWLock(const char *name, SceUInt32 attributes,
			   const SceKernelRWLockOptParam *option)
{
	return _sceKernelCreateRWLock(name, attributes, option);
}

int sceKernelLockReadRWLock(SceUID id, unsigned int *timeout)
{
	return _sceKernelLockReadRWLock(id, timeout);
}

int sceKernelLockReadRWLockCB(SceUID id, unsigned int *timeout)
{
	return _sceKernelLockReadRWLockCB(id, timeout);
}

int sceKernelLockWriteRWLock(SceUID id, unsigned int *timeout)
{
	return _sceKernelLockWriteRWLock(id, timeout);
}

int sceKernelLockWriteRWLockCB(SceUID id, unsigned int *timeout)
{
	return _sceKernelLockWriteRWLockCB(id, timeout);
}

int sceKernelCancelRWLock(SceUID id, SceUInt32 *readWaiting,
			SceUInt32 *writeWaiting, SceInt32 flags)
{
	return _sceKernelCancelRWLock(id, readWaiting, writeWaiting, flags);
}

int sceKernelGetRWLockInfo(SceUID id, SceKernelRWLockInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetRWLockInfo(id, info, &request);
}

int sceKernelGetThreadInfo(SceUID id, SceKernelThreadInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetThreadInfo(id, info, &request);
}

int sceKernelGetCallbackInfo(SceUID id, SceKernelCallbackInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetCallbackInfo(id, info, &request);
}

int sceKernelGetSystemInfo(SceKernelSystemInfo *info)
{
	struct ObjectInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetSystemInfo(info, &request);
}

int sceKernelGetThreadExitStatus(SceUID id, int *status)
{
	return _sceKernelGetThreadExitStatus(id, status);
}
