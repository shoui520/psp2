/*
 * The network side's threads and mutexes: a thread runs its entry with its argument, and
 * the mutexes report every failure but a recursive lock or an unlock by another thread.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include "mtp_internal.h"

#define NET_MTP_THREAD_PRIORITY		0x10000100
#define NET_MTP_THREAD_STACK		0x40000

static int netMtpThreadEntry(SceSize argSize, void *argBlock);

NetMtpThread *netMtpThreadCreate(int (*entry)(void *arg), void *arg, int reserved)
{
	SceUID thread;
	NetMtpThread *t;

	thread = sceKernelCreateThread("SceMtpNet", netMtpThreadEntry, NET_MTP_THREAD_PRIORITY,
				       NET_MTP_THREAD_STACK, 0, 0, NULL);
	if (thread < 0)
		return NULL;
	t = (NetMtpThread *)mtpUtilMalloc(sizeof(*t));
	if (t == NULL)
		return NULL;
	t->thread = thread;
	t->entry = entry;
	t->arg = arg;
	t->reserved = reserved;
	return t;
}

int netMtpThreadDelete(NetMtpThread *thread)
{
	SceUID uid = thread->thread;

	mtpUtilFree(thread);
	return sceKernelDeleteThread(uid);
}

int netMtpThreadStart(NetMtpThread *thread)
{
	return sceKernelStartThread(thread->thread, sizeof(*thread), thread);
}

int netMtpThreadJoin(NetMtpThread *thread, int *exitStatus)
{
	return sceKernelWaitThreadEnd(thread->thread, exitStatus, NULL);
}

int createMutex(SceUID *mutex)
{
	SceUID uid;

	uid = sceKernelCreateMutex("SceNetmtpResponder", 0, 0, NULL);
	if (uid < 0) {
		MTP_ERR("", 74, "unable to create device mutex=0x%x\n", uid);
		return uid;
	}
	*mutex = uid;
	return 0;
}

int deleteMutex(SceUID mutex)
{
	return sceKernelDeleteMutex(mutex);
}

int mutexLock(SceUID mutex)
{
	int ret;

	ret = sceKernelLockMutex(mutex, 1, NULL);
	if (ret != 0 && ret != SCE_KERNEL_ERROR_MUTEX_RECURSIVE &&
	    ret != SCE_KERNEL_ERROR_MUTEX_LOCK_OVF)
		MTP_ERR("", 96, "unable to lock mutex for MTP dev %x\n", ret);
	return ret;
}

int mutexUnlock(SceUID mutex)
{
	int ret;

	ret = sceKernelUnlockMutex(mutex, 1);
	if (ret != 0 && ret != SCE_KERNEL_ERROR_MUTEX_NOT_OWNED)
		MTP_ERR("", 112, "unable to unlock mutex for MTP dev %x\n", ret);
	return ret;
}

static int netMtpThreadEntry(SceSize argSize, void *argBlock)
{
	NetMtpThread *thread = (NetMtpThread *)argBlock;

	return thread->entry(thread->arg);
}
