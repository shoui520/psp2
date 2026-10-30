/*
 * Solaris reader-writer locks (Sony's dtrace/sce/solaris_rwlock.c): a kernel
 * mutex guards the counts, and readers and the writer wait on a semaphore
 * each, signalled once per holder let in.
 */
#include <dtrace/sce/solaris_rwlock.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>

#define RW_NAME_LEN	32

void rw_init(krwlock_t *rwlp, const char *name, krw_type_t type, void *arg)
{
	char mutexName[RW_NAME_LEN];
	char readName[RW_NAME_LEN];
	char writeName[RW_NAME_LEN];

	SCE_DBG_ASSERT(rwlp != NULL, 25);

	if (name != NULL && name[0] != '\0') {
		snprintf(mutexName, sizeof(mutexName), "SceDeci4pDtracepRw%s", name);
		snprintf(readName, sizeof(readName), "SceDeci4pDtracepRd%s", name);
		snprintf(writeName, sizeof(writeName), "SceDeci4pDtracepWr%s", name);
	} else {
		__builtin_strcpy(mutexName, "SceDeci4pDtracepRw");
		__builtin_strcpy(readName, "SceDeci4pDtracepRd");
		__builtin_strcpy(writeName, "SceDeci4pDtracepWr");
	}

	rwlp->mutex = sceKernelCreateMutex(mutexName, 0, 0, NULL);
	SCE_DBG_ASSERT(rwlp->mutex >= 0, 44);

	rwlp->readSema = sceKernelCreateSema(readName, 0, 0, 255, NULL);
	SCE_DBG_ASSERT(rwlp->readSema >= 0, 51);

	rwlp->writeSema = sceKernelCreateSema(writeName, 0, 0, 1, NULL);
	SCE_DBG_ASSERT(rwlp->writeSema >= 0, 58);

	rwlp->readers = 0;
	rwlp->readersWaiting = 0;
	rwlp->writer = 0;
	rwlp->writersWaiting = 0;
}

void rw_destroy(krwlock_t *rwlp)
{
	SCE_DBG_ASSERT(rwlp != NULL, 66);

	SCE_DBG_ASSERT(rwlp->mutex >= 0, 68);
	sceKernelDeleteMutex(rwlp->mutex);
	rwlp->mutex = -1;

	SCE_DBG_ASSERT(rwlp->readSema >= 0, 72);
	sceKernelDeleteSema(rwlp->readSema);
	rwlp->readSema = -1;

	SCE_DBG_ASSERT(rwlp->writeSema >= 0, 76);
	sceKernelDeleteSema(rwlp->writeSema);
	rwlp->writeSema = -1;
}

void rw_enter(krwlock_t *rwlp, krw_t enter_type)
{
	SCE_DBG_ASSERT(rwlp != NULL, 229);

	if (enter_type == RW_WRITER) {
		sceKernelLockMutex(rwlp->mutex, 1, NULL);
		if (rwlp->readers != 0 || rwlp->writer != 0) {
			rwlp->writersWaiting++;
		} else {
			rwlp->writer = 1;
			sceKernelSignalSema(rwlp->writeSema, 1);
		}
		sceKernelUnlockMutex(rwlp->mutex, 1);
		sceKernelWaitSema(rwlp->writeSema, 1, NULL);
	} else if (enter_type == RW_READER) {
		sceKernelLockMutex(rwlp->mutex, 1, NULL);
		if (rwlp->writer != 0 || rwlp->writersWaiting != 0) {
			rwlp->readersWaiting++;
		} else {
			rwlp->readers++;
			sceKernelSignalSema(rwlp->readSema, 1);
		}
		sceKernelUnlockMutex(rwlp->mutex, 1);
		sceKernelWaitSema(rwlp->readSema, 1, NULL);
	} else {
		SCE_DBG_ASSERT(0, 236);
	}
}

void rw_exit(krwlock_t *rwlp)
{
	SCE_DBG_ASSERT(rwlp != NULL, 242);

	if (rwlp->writer == 1) {
		sceKernelLockMutex(rwlp->mutex, 1, NULL);
		rwlp->writer = 0;
		if (rwlp->readersWaiting > 0) {
			/* let every waiting reader in */
			do {
				rwlp->readersWaiting--;
				rwlp->readers++;
				sceKernelSignalSema(rwlp->readSema, 1);
			} while (rwlp->readersWaiting != 0);
		} else if (rwlp->writersWaiting > 0) {
			rwlp->writer = 1;
			rwlp->writersWaiting--;
			sceKernelSignalSema(rwlp->writeSema, 1);
		}
		sceKernelUnlockMutex(rwlp->mutex, 1);
	} else if (rwlp->readers > 0) {
		sceKernelLockMutex(rwlp->mutex, 1, NULL);
		if (--rwlp->readers == 0 && rwlp->writersWaiting > 0) {
			rwlp->writersWaiting--;
			rwlp->writer = 1;
			sceKernelSignalSema(rwlp->writeSema, 1);
		}
		sceKernelUnlockMutex(rwlp->mutex, 1);
	} else {
		SCE_DBG_ASSERT(0, 250);
	}
}
