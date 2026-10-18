/*
 * The crypt engine: one thread that takes block requests off a "todo" list,
 * runs them and puts them on a "done" list, where the submitting thread
 * waits for them.
 */

#include <threadmgr/threadmgr.h>
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
		ret = sceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
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
			ret = sceKernelWaitCond(g_pfsCryptEngineTodoCnd, NULL);
			if (ret != 0)
				goto out;
		}
		pfsListUnlink(&job->link);
		if (flags & PFS_CRYPT_ENGINE_STOP) {
			ret = 0;
			goto out;
		}
		sceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);

		pfsCryptEngineProcess(job);

		ret = sceKernelLockMutex(g_pfsCryptEngineDoneMtx, 1, NULL);
		if (ret != 0)
			return ret;
		pfsListAppend(&g_pfsCryptEngineDone, &job->link);
		if (job->waiting) {
			job->waiting = 0;
			ret = sceKernelSignalCondTo(g_pfsCryptEngineDoneCnd, job->thread);
			if (ret != 0) {
				sceKernelUnlockMutex(g_pfsCryptEngineDoneMtx, 1);
				return ret;
			}
		}
		sceKernelUnlockMutex(g_pfsCryptEngineDoneMtx, 1);
	}
out:
	sceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
	return ret;
}

int pfsCryptEngineInit(void)
{
	SceUID ret;

	ret = sceKernelCreateMutex("ScePfsCryptEngineTodoMtx", PFS_MUTEX_ATTR | 2, 0, NULL);
	g_pfsCryptEngineTodoMtx = ret;
	if (ret < 0)
		return ret;
	ret = sceKernelCreateCond("ScePfsCryptEngineTodoCnd", PFS_MUTEX_ATTR, ret, NULL);
	g_pfsCryptEngineTodoCnd = ret;
	if (ret < 0)
		return ret;
	ret = sceKernelCreateMutex("ScePfsCryptEngineDoneMtx", PFS_MUTEX_ATTR | 2, 0, NULL);
	g_pfsCryptEngineDoneMtx = ret;
	if (ret < 0)
		return ret;
	ret = sceKernelCreateCond("ScePfsCryptEngineDoneCnd", PFS_MUTEX_ATTR, ret, NULL);
	g_pfsCryptEngineDoneCnd = ret;
	if (ret < 0)
		return ret;
	g_pfsCryptEngineFlags = 0;
	g_pfsCryptEngineTodo.next = &g_pfsCryptEngineTodo;
	g_pfsCryptEngineTodo.prev = &g_pfsCryptEngineTodo;
	g_pfsCryptEngineDone.next = &g_pfsCryptEngineDone;
	g_pfsCryptEngineDone.prev = &g_pfsCryptEngineDone;
	ret = sceKernelCreateThread("ScePfsCryptEngineThread", pfsCryptEngineThread, 0x20, 0x2000,
				     0, 0x80000, NULL);
	g_pfsCryptEngineThread = ret;
	if (ret < 0)
		return ret;
	return sceKernelStartThread(ret, 0, NULL);
}

void pfsCryptEngineTerm(void)
{
	sceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
	g_pfsCryptEngineFlags |= PFS_CRYPT_ENGINE_STOP;
	sceKernelSignalCond(g_pfsCryptEngineTodoCnd);
	sceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
	sceKernelWaitThreadEnd(g_pfsCryptEngineThread, NULL, NULL);
	sceKernelDeleteCond(g_pfsCryptEngineTodoCnd);
	sceKernelDeleteCond(g_pfsCryptEngineDoneCnd);
	sceKernelDeleteMutex(g_pfsCryptEngineTodoMtx);
	sceKernelDeleteMutex(g_pfsCryptEngineDoneMtx);
}

int pfsCryptEngineSubmit(ScePfsCryptJob *job)
{
	int ret;

	if (job == NULL)
		return SCE_PFS_ERROR_NO_JOB;
	ret = sceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
	if (ret != 0)
		return ret;
	pfsListAppend(&g_pfsCryptEngineTodo, &job->link);
	ret = sceKernelSignalCond(g_pfsCryptEngineTodoCnd);
	sceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
	return ret;
}

/* take a job back off the done list, waiting until the engine has run it */
static inline int pfsCryptEngineTakeDone(ScePfsCryptJob *job)	/* name unknown */
{
	ScePfsListNode *node;
	int ret;

	ret = sceKernelLockMutex(g_pfsCryptEngineDoneMtx, 1, NULL);
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
		ret = sceKernelWaitCond(g_pfsCryptEngineDoneCnd, NULL);
	} while (ret == 0);
out:
	sceKernelUnlockMutex(g_pfsCryptEngineDoneMtx, 1);
	return ret;
}

int pfsCryptEngineCancel(ScePfsCryptJob *job)
{
	ScePfsListNode *node;
	int ret;

	if (job == NULL)
		return SCE_PFS_ERROR_NO_JOB;
	ret = sceKernelLockMutex(g_pfsCryptEngineTodoMtx, 1, NULL);
	if (ret != 0)
		return ret;
	for (node = g_pfsCryptEngineTodo.next; node != &g_pfsCryptEngineTodo && node != NULL;
	     node = node->next) {
		if (node == &job->link) {
			pfsListUnlink(&job->link);
			sceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
			return 0;
		}
	}
	sceKernelUnlockMutex(g_pfsCryptEngineTodoMtx, 1);
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
