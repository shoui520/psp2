/*
 * CCondvar.cc: a condition variable with its mutex (0x81136464-0x81136608), the one the
 * daemon and its callers wake each other with.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <std/stdlib.h>

#include "mtpr2_internal.h"

using namespace mtpr2;

CCondvar::CCondvar(int *error)
{
	waiters = 0;
	mutex = 0;
	cond = 0;
	MTPR2_ASSERT(error != NULL);
	*error = 0;
	mutex = sceKernelCreateMutex("SceMtpr2", SCE_KERNEL_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (mutex >= 0) {
		cond = sceKernelCreateCond("SceMtpr2", SCE_KERNEL_COND_ATTR_TH_PRIO, mutex, NULL);
		if (cond >= 0)
			return;
		MTPR2_PRINT_ERROR();
		*error = cond;
		sceKernelDeleteMutex(mutex);
	} else {
		MTPR2_PRINT_ERROR();
		*error = mutex;
	}
	mutex = 0;
	cond = 0;
}

CCondvar::~CCondvar()
{
	if (waiters != 0)
		MTPR2_PRINT_ERROR();
	if (cond != 0) {
		sceKernelDeleteCond(cond);
		cond = 0;
	}
	if (mutex != 0) {
		sceKernelDeleteMutex(mutex);
		mutex = 0;
	}
}

int CCondvar::signal(void)
{
	int ret;

	ret = sceKernelLockMutex(mutex, 1, NULL);
	MTPR2_ASSERT(ret == 0);
	ret = sceKernelSignalCondAll(cond);
	MTPR2_ASSERT(ret == 0);
	ret = sceKernelUnlockMutex(mutex, 1);
	MTPR2_ASSERT(ret == 0);
	return ret;
}

int CCondvar::wait(unsigned int *timeout)
{
	int ret;
	int err;

	err = sceKernelLockMutex(mutex, 1, NULL);
	MTPR2_ASSERT(err == 0);
	waiters++;
	ret = sceKernelWaitCond(cond, timeout);
	if (ret != 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		MTPR2_PRINT_ERROR();
	waiters--;
	err = sceKernelUnlockMutex(mutex, 1);
	MTPR2_ASSERT(err == 0);
	return ret;
}

int CCondvar::signalAndWait(unsigned int *timeout)
{
	int ret;
	int err;

	err = sceKernelLockMutex(mutex, 1, NULL);
	MTPR2_ASSERT(err == 0);
	err = sceKernelSignalCondAll(cond);
	MTPR2_ASSERT(err == 0);
	ret = sceKernelWaitCond(cond, timeout);
	if (ret != 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		MTPR2_PRINT_ERROR();
	err = sceKernelUnlockMutex(mutex, 1);
	MTPR2_ASSERT(err == 0);
	return ret;
}
