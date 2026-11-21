/*
 * FileHandleTable.cc: the downloads under way, only one (0x81136EEA-0x81137208): the daemon
 * registers itself under the one handle, callers take it for a call and give it back, and
 * the daemon waits for them before it goes.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <std/new>
#include <std/stdlib.h>

#include "mtpr2_internal.h"

using namespace mtpr2;

FileHandleTable::FileHandleTable(int *error)
{
	*error = 0;
	canceled = false;
	entries = NULL;
	mutex = sceKernelCreateMutex("SceMtpr2FileHandleTable", SCE_KERNEL_MUTEX_ATTR_TH_PRIO, 0,
				     NULL);
	if (mutex < 0) {
		MTPR2_PRINT_ERROR();
		*error = mutex;
		return;
	}
	cond = sceKernelCreateCond("SceMtpr2FileHandleTable", SCE_KERNEL_COND_ATTR_TH_PRIO, mutex,
				   NULL);
	if (cond < 0) {
		MTPR2_PRINT_ERROR();
		*error = cond;
		sceKernelDeleteMutex(mutex);
	}
}

FileHandleTable::~FileHandleTable()
{
	FileHandleTableEntry *entry = entries;

	if (entry != NULL) {
		if (entry->refCount != 0) {
			MTPR2_PRINT_ERROR();
			entry->refCount = 0;
		}
		sce_paf_free(entry);
	}
	if (cond >= 0) {
		sceKernelDeleteCond(cond);
		cond = -1;
	}
	if (mutex >= 0) {
		sceKernelDeleteMutex(mutex);
		mutex = -1;
	}
}

int FileHandleTable::getDaemon(unsigned int fileHandle, Daemon **daemon)
{
	unsigned int timeout;
	int ret = 0;

	*daemon = NULL;
	timeout = MTPR2_LOCK_TIMEOUT_US;
	sceKernelLockMutex(mutex, 1, &timeout);
	if (entries == NULL) {
		MTPR2_PRINT_ERROR();
		ret = MTPR2_ERROR_NOT_STARTED;
	} else if (fileHandle != entries->fileHandle) {
		MTPR2_PRINT_ERROR();
		ret = MTPR2_ERROR_INVALID_ARGUMENT;
	} else {
		*daemon = entries->daemon;
		entries->refCount++;
	}
	sceKernelUnlockMutex(mutex, 1);
	return ret;
}

void FileHandleTable::releaseDaemon(Daemon *daemon)
{
	unsigned int timeout;

	timeout = MTPR2_LOCK_TIMEOUT_US;
	sceKernelLockMutex(mutex, 1, &timeout);
	if (entries->refCount-- == 1)
		sceKernelSignalCondAll(entries->cond);
	sceKernelUnlockMutex(mutex, 1);
}

int FileHandleTable::registerDaemon(Daemon *daemon)
{
	unsigned int timeout;
	FileHandleTableEntry *entry;

	timeout = MTPR2_LOCK_TIMEOUT_US;
	sceKernelLockMutex(mutex, 1, &timeout);
	entry = (FileHandleTableEntry *)sce_paf_malloc(sizeof(FileHandleTableEntry));
	if (entry == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NO_MEMORY;
	}
	new (entry) FileHandleTableEntry(daemon, mutex, cond);
	entries = entry;
	sceKernelUnlockMutex(mutex, 1);
	return 0;
}

inline int FileHandleTableEntry::waitRefCountZero(unsigned int *timeout)
{
	unsigned int left = *timeout;
	unsigned int wait;
	int ret = 0;

	if (refCount != 0) {
		do {
			wait = MTPR2_WAIT_US;
			ret = sceKernelWaitCond(cond, &wait);
			if (timeout != NULL) {
				if (left < MTPR2_WAIT_US)
					break;
				left -= MTPR2_WAIT_US;
				MTPR2_PRINT_ERROR();
			}
		} while (refCount != 0);
		if (refCount != 0) {
			MTPR2_PRINT_ERROR();
			refCount = 0;
		}
	}
	return ret;
}

int FileHandleTable::unregisterDaemon(Daemon *daemon)
{
	unsigned int timeout;
	unsigned int waitTimeout;
	FileHandleTableEntry *entry;
	int ret;

	timeout = MTPR2_LOCK_TIMEOUT_US;
	sceKernelLockMutex(mutex, 1, &timeout);
	entry = entries;
	waitTimeout = MTPR2_LOCK_TIMEOUT_US;
	ret = entry->waitRefCountZero(&waitTimeout);
	sce_paf_free(entry);
	entries = NULL;
	sceKernelUnlockMutex(mutex, 1);
	return ret;
}

bool FileHandleTable::findDaemon(SceUID thread, unsigned int *fileHandle)
{
	if (entries != NULL && entries->daemon->thread == thread) {
		*fileHandle = entries->fileHandle;
		return true;
	}
	return false;
}

void FileHandleTable::cancel(void)
{
	FileBuf *fileBuf;

	canceled = true;
	if (entries != NULL && entries->daemon != NULL) {
		entries->daemon->cancel = 1;
		entries->daemon->getFileBuf(&fileBuf);
		if (fileBuf != NULL)
			fileBuf->status = FILE_BUF_STATUS_CANCELED;
	}
}

bool FileHandleTable::isCanceled(void)
{
	return canceled;
}
