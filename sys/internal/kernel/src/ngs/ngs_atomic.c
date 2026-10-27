/* SceNgs atomic service wrapper; original call boundary is kept. */

#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <stdint.h>

#include "ngs_platform.h"
#include "ngs_cache.h"

int ngsAtomicSet(int *value, int replacement, int expected)
{
	return sceKernelAtomicCompareAndSet32(value, expected, replacement);
}

int ngsLockSystemMutex(NgsPlatformContext *context)
{
	if (lockFastMutex(
	    (SceKernelFastMutex *)&context->systemMutex) != 0)
		return (int)0x804a0001;
	return 0;
}

int ngsUnlockSystemMutex(NgsPlatformContext *context)
{
	int result;

	result = sceKernelUnlockFastMutex(
		(SceKernelFastMutex *)&context->systemMutex);
	if (result != 0)
		return -1;
	return result;
}

int ngsLockVoice(NgsPlatformContext *context)
{
	int retries = 100001;
	int result;

	for (;;) {
		result = sceKernelAtomicCompareAndSet32(&context->voiceLock, 0, 1);
		if (result == 0)
			return result;
		if (--retries == 0)
			break;
		sceKernelDelayThread(100);
	}
	return (int)0x804a0008;
}

int ngsUnlockVoice(NgsPlatformContext *context)
{
	if (sceKernelAtomicCompareAndSet32(&context->voiceLock, 1, 0) == 1)
		return 0;
	return (int)0x804a0001;
}

int ngsLockUpdate(NgsPlatformContext *context)
{
	int retries = 100001;
	int result;

	for (;;) {
		result = sceKernelAtomicCompareAndSet32(&context->updateLock, 0, 1);
		if (result == 0)
			return result;
		if (--retries == 0)
			break;
		sceKernelDelayThread(100);
	}
	return (int)0x804a0008;
}

int ngsUnlockUpdate(NgsPlatformContext *context)
{
	if (sceKernelAtomicCompareAndSet32(&context->updateLock, 1, 0) == 1)
		return 0;
	return (int)0x804a0001;
}

int ngsLockSystem(NgsPlatformContext *context)
{
	int retries = 100001;
	int result;

	for (;;) {
		result = sceKernelAtomicCompareAndSet32(&context->systemLock, 0, 1);
		if (result == 0)
			return result;
		if (--retries == 0)
			break;
		sceKernelDelayThread(100);
	}
	return (int)0x804a0008;
}

int ngsUnlockSystem(NgsPlatformContext *context)
{
	if (sceKernelAtomicCompareAndSet32(&context->systemLock, 1, 0) == 1)
		return 0;
	return (int)0x804a0001;
}

int ngsTryLockSystem(NgsPlatformContext *context)
{
	if (sceKernelAtomicCompareAndSet32(&context->systemLock, 0, 1) != 0)
		return (int)0x804a0001;
	return 0;
}

int ngsLockParams(NgsPlatformContext *context)
{
	int retries = 100001;
	int result;

	for (;;) {
		result = sceKernelAtomicCompareAndSet32(&context->paramsLock, 0, 1);
		if (result == 0)
			return result;
		if (--retries == 0)
			break;
		sceKernelDelayThread(100);
	}
	return (int)0x804a0008;
}

int ngsUnlockParams(NgsPlatformContext *context)
{
	if (sceKernelAtomicCompareAndSet32(&context->paramsLock, 1, 0) == 1)
		return 0;
	return (int)0x804a0001;
}

int ngsLockCodec(NgsPlatformContext *context)
{
	int result;

	result = sceKernelLockMutex(context->codecMutex, 1, NULL);
	if (result != 0)
		return -1;
	return result;
}

int ngsUnlockCodec(NgsPlatformContext *context)
{
	int result;

	result = sceKernelUnlockMutex(context->codecMutex, 1);
	if (result != 0)
		return -1;
	return result;
}

int ngsLockProcess(NgsProcessStorage *storage)
{
	int result;

	for (;;) {
		result = sceKernelAtomicCompareAndSet32(&storage->processLock, 0, 1);
		if (result == 0)
			return result;
		sceKernelDelayThread(10);
	}
}

int ngsUnlockProcess(NgsProcessStorage *storage)
{
	if (sceKernelAtomicCompareAndSet32(&storage->processLock, 1, 0) == 1)
		return 0;
	return (int)0x804a0001;
}

int ngsLockTransfer(NgsPlatformContext *context)
{
	int result;

	for (;;) {
		result = sceKernelAtomicCompareAndSet32(&context->transferLock, 0, 1);
		if (result == 0)
			return result;
		sceKernelDelayThread(10);
	}
}

int ngsUnlockTransfer(NgsPlatformContext *context)
{
	if (sceKernelAtomicCompareAndSet32(&context->transferLock, 1, 0) == 1)
		return 0;
	return (int)0x804a0001;
}

extern void ngsCleanDcacheRange(void *address, int size);
