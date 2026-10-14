/*
 * The crypt engine: one thread that takes block requests off a "todo" list,
 * runs them and puts them on a "done" list, where the submitting thread
 * waits for them.
 */

#include <psp2kern/kernel/threadmgr.h>
#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_NO_JOB	0x80140f09	/* name unknown */

#define PFS_CRYPT_ENGINE_STOP	0x1	/* name unknown */

static SceUInt32 g_pfsCryptEngineFlags;		/* names unknown */
static SceUID g_pfsCryptEngineThread;
static SceUID g_pfsCryptEngineTodoMtx;
static SceUID g_pfsCryptEngineTodoCnd;
static SceUID g_pfsCryptEngineDoneMtx;
static SceUID g_pfsCryptEngineDoneCnd;
static ScePfsListNode g_pfsCryptEngineTodo;
static ScePfsListNode g_pfsCryptEngineDone;

static inline void pfsListUnlink(ScePfsListNode *node)	/* name unknown */
{
	node->next->prev = node->prev;
	node->prev->next = node->next;
}

static inline void pfsListAppend(ScePfsListNode *list, ScePfsListNode *node)	/* name unknown */
{
	node->next = list;
	node->prev = list->prev;
	list->prev = node;
	node->prev->next = node;
}

static int pfsCryptEngineThread(SceSize args, void *argp)
{
	ScePfsCryptJob *job;
	SceUInt32 flags;
	int ret;

	for (;;) {
		ret = ksceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
		if (ret != 0)
			return ret;
		for (;;) {
			job = (ScePfsCryptJob *)g_pfsCryptEngineTodo.next;
			flags = g_pfsCryptEngineFlags;
			if (&job->link != &g_pfsCryptEngineTodo)
				break;
			if (flags & PFS_CRYPT_ENGINE_STOP) {
				ret = 0;
				goto out;
			}
			ret = ksceKernelWaitCond(g_pfsCryptEngineTodoCnd, NULL);
			if (ret != 0)
				goto out;
		}
		pfsListUnlink(&job->link);
		if (flags & PFS_CRYPT_ENGINE_STOP) {
			ret = 0;
			goto out;
		}
		ksceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);

		pfsCryptEngineProcess(job);

		ret = ksceKernelLockMutex(g_pfsCryptEngineDoneMtx, 1, NULL);
		if (ret != 0)
			return ret;
		pfsListAppend(&g_pfsCryptEngineDone, &job->link);
		if (job->waiting) {
			job->waiting = 0;
			ret = ksceKernelSignalCondTo(g_pfsCryptEngineDoneCnd, job->thread);
			if (ret != 0) {
				ksceKernelUnlockMutex(g_pfsCryptEngineDoneMtx, 1);
				return ret;
			}
		}
		ksceKernelUnlockMutex(g_pfsCryptEngineDoneMtx, 1);
	}
out:
	ksceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
	return ret;
}

int pfsCryptEngineInit(void)
{
	SceUID ret;

	ret = ksceKernelCreateMutex("ScePfsCryptEngineTodoMtx", PFS_MUTEX_ATTR | 2, 0, NULL);
	g_pfsCryptEngineTodoMtx = ret;
	if (ret < 0)
		return ret;
	ret = ksceKernelCreateCond("ScePfsCryptEngineTodoCnd", PFS_MUTEX_ATTR, ret, NULL);
	g_pfsCryptEngineTodoCnd = ret;
	if (ret < 0)
		return ret;
	ret = ksceKernelCreateMutex("ScePfsCryptEngineDoneMtx", PFS_MUTEX_ATTR | 2, 0, NULL);
	g_pfsCryptEngineDoneMtx = ret;
	if (ret < 0)
		return ret;
	ret = ksceKernelCreateCond("ScePfsCryptEngineDoneCnd", PFS_MUTEX_ATTR, ret, NULL);
	g_pfsCryptEngineDoneCnd = ret;
	if (ret < 0)
		return ret;
	g_pfsCryptEngineFlags = 0;
	g_pfsCryptEngineTodo.next = &g_pfsCryptEngineTodo;
	g_pfsCryptEngineTodo.prev = &g_pfsCryptEngineTodo;
	g_pfsCryptEngineDone.next = &g_pfsCryptEngineDone;
	g_pfsCryptEngineDone.prev = &g_pfsCryptEngineDone;
	ret = ksceKernelCreateThread("ScePfsCryptEngineThread", pfsCryptEngineThread, 0x20, 0x2000,
				     0, 0x80000, NULL);
	g_pfsCryptEngineThread = ret;
	if (ret < 0)
		return ret;
	return ksceKernelStartThread(ret, 0, NULL);
}

void pfsCryptEngineTerm(void)
{
	ksceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
	g_pfsCryptEngineFlags |= PFS_CRYPT_ENGINE_STOP;
	ksceKernelSignalCond(g_pfsCryptEngineTodoCnd);
	ksceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
	ksceKernelWaitThreadEnd(g_pfsCryptEngineThread, NULL, NULL);
	ksceKernelDeleteCond(g_pfsCryptEngineTodoCnd);
	ksceKernelDeleteCond(g_pfsCryptEngineDoneCnd);
	ksceKernelDeleteMutex(g_pfsCryptEngineTodoMtx);
	ksceKernelDeleteMutex(g_pfsCryptEngineDoneMtx);
}

int pfsCryptEngineSubmit(ScePfsCryptJob *job)
{
	int ret;

	if (job == NULL)
		return SCE_PFS_ERROR_NO_JOB;
	ret = ksceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
	if (ret != 0)
		return ret;
	pfsListAppend(&g_pfsCryptEngineTodo, &job->link);
	ret = ksceKernelSignalCond(g_pfsCryptEngineTodoCnd);
	ksceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
	return ret;
}

/* take a job back off the done list, waiting until the engine has run it */
static inline int pfsCryptEngineTakeDone(ScePfsCryptJob *job)	/* name unknown */
{
	ScePfsListNode *node;
	int ret;

	ret = ksceKernelLockMutex(g_pfsCryptEngineDoneMtx, 1, NULL);
	if (ret != 0)
		return ret;
	do {
		for (node = g_pfsCryptEngineDone.next; node != &g_pfsCryptEngineDone;
		     node = node->next) {
			if (node == NULL) {
				ret = SCE_PFS_ERROR_NO_JOB;
				goto out;
			}
			if (node == &job->link) {
				pfsListUnlink(&job->link);
				ret = 0;
				goto out;
			}
		}
		job->waiting = 1;
		ret = ksceKernelWaitCond(g_pfsCryptEngineDoneCnd, NULL);
	} while (ret == 0);
out:
	ksceKernelUnlockMutex(g_pfsCryptEngineDoneMtx, 1);
	return ret;
}

int pfsCryptEngineCancel(ScePfsCryptJob *job)
{
	ScePfsListNode *node;
	int ret;

	if (job == NULL)
		return SCE_PFS_ERROR_NO_JOB;
	ret = ksceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
	if (ret != 0)
		return ret;
	for (node = g_pfsCryptEngineTodo.next; node != &g_pfsCryptEngineTodo && node != NULL;
	     node = node->next) {
		if (node == &job->link) {
			pfsListUnlink(&job->link);
			ksceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
			return 0;
		}
	}
	ksceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
	return pfsCryptEngineTakeDone(job);
}

int pfsCryptEngineWait(ScePfsCryptJob *job)
{
	if (job == NULL)
		return SCE_PFS_ERROR_NO_JOB;
	return pfsCryptEngineTakeDone(job);
}

/* PLACEHOLDER: cryptographic routine, deferred: encrypts or decrypts
 * the job's blocks between its plain and disk buffers and hashes their icvs */
void pfsCryptEngineProcess(ScePfsCryptJob *job)
{
	job->result = 0;
}
