/*
 * mtpr2.cc: MTPR2's calls (0x8113566C-0x81135CA0): a download is started on its daemon
 * thread, its file's attributes waited for, read in order and ended. Only one download
 * runs at a time, under the one file handle.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>

#include "mtpr2_internal.h"

using namespace mtpr2;

#define MTPR2_DAEMON_STACK_SIZE		SCE_KERNEL_8KiB		/* PLACEHOLDER name */

FileHandleTable *g_fileHandleTable;

static SceInt32 daemonEntry(SceSize argSize, void *argBlock)
{
	(*(Daemon **)argBlock)->main();
	return 0;
}

int MTPR2StartDownloadStep1(MTPR2Args *args, unsigned int *fileHandle)
{
	CCondvar *cCondvar = NULL;
	Daemon *daemon = NULL;
	SceInt32 cpuAffinityMask = 0;
	const SceKernelThreadOptParam *option = NULL;
	unsigned int timeout;
	SceUID thread;
	int error;
	int ret;
	int stat;

	if (args == NULL || fileHandle == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_INVALID_ARGUMENT;
	}
	*fileHandle = 0;
	if (sce_paf_strlen(args->url) > MTPR2_URL_MAX) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_INVALID_ARGUMENT;
	}
	if (args->offset != 0) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_INVALID_ARGUMENT;
	}
	if (g_fileHandleTable != NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_BUSY;
	}
	g_fileHandleTable = (FileHandleTable *)sce_paf_malloc(sizeof(FileHandleTable));
	if (g_fileHandleTable == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NO_MEMORY;
	}
	new (g_fileHandleTable) FileHandleTable(&error);
	if (error != 0)
		goto cleanup;
	if (g_fileHandleTable->isCanceled() != false) {
		error = MTPR2_ERROR_CANCELED;
		goto cleanup;
	}
	cCondvar = (CCondvar *)sce_paf_malloc(sizeof(CCondvar));
	if (cCondvar == NULL) {
		MTPR2_PRINT_ERROR();
		error = MTPR2_ERROR_NO_MEMORY;
		goto cleanup;
	}
	new (cCondvar) CCondvar(&error);
	if (error != 0) {
		sce_paf_free(cCondvar);
		goto cleanup;
	}
	daemon = (Daemon *)sce_paf_malloc(sizeof(Daemon));
	if (daemon == NULL) {
		MTPR2_PRINT_ERROR();
		error = MTPR2_ERROR_NO_MEMORY;
		goto cleanup;
	}
	new (daemon) Daemon(cCondvar, args->url, args->offset, args->ringBufSize, args->timeout);
	thread = sceKernelCreateThread("SceMtpr2Daemon", daemonEntry, SCE_KERNEL_DEFAULT_PRIORITY_USER,
				       MTPR2_DAEMON_STACK_SIZE, 0, cpuAffinityMask, option);
	if (thread < 0) {
		MTPR2_PRINT_ERROR();
		error = thread;
		goto deletethread;
	}
	daemon->thread = thread;
	if (args->optionType == MTPR2_OPTION_PROTOCOL) {
		if (sce_paf_strcmp(*(const char **)args->optionValue, MTPR2_PROTOCOL_HOSTFS) == 0) {
			MTPR2_PRINT_ERROR();
			daemon->protocol = DAEMON_PROTOCOL_HOSTFS;
		} else if (sce_paf_strcmp(*(const char **)args->optionValue, MTPR2_PROTOCOL_PROTO2) == 0) {
			MTPR2_PRINT_ERROR();
			daemon->protocol = DAEMON_PROTOCOL_PROTO2;
		}
	}
	if (g_fileHandleTable->isCanceled() != false) {
		error = MTPR2_ERROR_CANCELED;
		goto deletethread;
	}
	error = sceKernelStartThread(thread, sizeof(daemon), &daemon);
	if (error < 0) {
		MTPR2_PRINT_ERROR();
		goto deletethread;
	}
	for (;;) {
		timeout = MTPR2_WAIT_US;
		error = cCondvar->wait(&timeout);
		if (error != 0 && error != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			MTPR2_PRINT_ERROR();
			break;
		}
		if (g_fileHandleTable->findDaemon(thread, fileHandle) != false)
			return 0;
		if (g_fileHandleTable->isCanceled() != false) {
			error = MTPR2_ERROR_CANCELED;
			break;
		}
		error = daemon->error;
		if (error != 0) {
			MTPR2_PRINT_ERROR();
			break;
		}
	}
	daemon->cancel = 1;
	cCondvar->signal();
	ret = sceKernelWaitThreadEnd(thread, &stat, NULL);
	if (ret < 0)
		MTPR2_PRINT_ERROR();
	return error;

deletethread:
	if (thread > 0) {
		ret = sceKernelDeleteThread(thread);
		if (ret < 0)
			MTPR2_PRINT_ERROR();
	}
	goto cleanup;

cleanup:
	if (daemon != NULL) {
		daemon->~Daemon();
		sce_paf_free(daemon);
		daemon = NULL;
	}
	if (cCondvar != NULL) {
		cCondvar->~CCondvar();
		sce_paf_free(cCondvar);
	}
	if (g_fileHandleTable != NULL) {
		g_fileHandleTable->~FileHandleTable();
		sce_paf_free(g_fileHandleTable);
		g_fileHandleTable = NULL;
	}
	return error;
}

int MTPR2StartDownloadStep2(unsigned int fileHandle, MTPR2FileAttributes *attr)
{
	Daemon *daemon = NULL;
	unsigned int timeout;
	int ret;

	if (g_fileHandleTable == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NOT_STARTED;
	}
	ret = g_fileHandleTable->getDaemon(fileHandle, &daemon);
	if (ret != 0)
		return ret;
	ret = daemon->error;
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
	} else {
		bool ready = false;
		CCondvar *cCondvar = daemon->cCondvar;

		do {
			if (g_fileHandleTable->isCanceled() != false) {
				ret = MTPR2_ERROR_CANCELED;
				goto release;
			}
			ret = daemon->error;
			if (ret != 0) {
				MTPR2_PRINT_ERROR();
				goto release;
			}
			switch (daemon->state) {
			case DAEMON_STATE_RUNNING:
			case DAEMON_STATE_ERROR:
				ready = true;
				break;
			case DAEMON_STATE_DONE:
				ret = MTPR2_ERROR_CANCELED;
				goto release;
			}
			timeout = MTPR2_WAIT_US;
			ret = cCondvar->wait(&timeout);
			if (ret != 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				goto release;
			if (daemon->cancel == 1) {
				MTPR2_PRINT_ERROR();
				ret = MTPR2_ERROR_CANCELED;
				goto release;
			}
		} while (ready == false);
		ret = daemon->getFileAttributes(attr);
	}
release:
	g_fileHandleTable->releaseDaemon(daemon);
	return ret;
}

int MTPR2EndDownload(unsigned int fileHandle, unsigned int *timeout)
{
	int ret;
	Daemon *daemon;
	FileBuf *fileBuf;
	int err;

	if (g_fileHandleTable == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NOT_STARTED;
	}
	ret = g_fileHandleTable->getDaemon(fileHandle, &daemon);
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
		return ret;
	}
	if (daemon->state == DAEMON_STATE_STARTING)
		daemon->abortResponder();
	daemon->cancel = 1;
	daemon->getFileBuf(&fileBuf);
	if (fileBuf != NULL)
		fileBuf->status = FILE_BUF_STATUS_CANCELED;
	daemon->cCondvar->signal();
	g_fileHandleTable->releaseDaemon(daemon);
	err = sceKernelWaitThreadEnd(daemon->thread, &ret, timeout);
	if (err != 0) {
		ret = err;
		MTPR2_PRINT_ERROR();
		if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return MTPR2_ERROR_TIMEOUT;
		return ret;
	}
	return err;
}

int MTPR2Read(unsigned int fileHandle, void *buf, unsigned int nbyte, long long offset,
	      unsigned int *read)
{
	Daemon *daemon;
	FileBuf *fileBuf;
	int ret;

	if (g_fileHandleTable == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NOT_STARTED;
	}
	ret = g_fileHandleTable->getDaemon(fileHandle, &daemon);
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
		return ret;
	}
	ret = daemon->error;
	if (ret == 0) {
		int state = daemon->state;

		if (state != DAEMON_STATE_RUNNING && state != DAEMON_STATE_ERROR) {
			MTPR2_PRINT_ERROR();
			ret = state == DAEMON_STATE_DONE ? MTPR2_ERROR_CANCELED : MTPR2_ERROR_NOT_STARTED;
			goto release;
		}
		ret = daemon->getFileBuf(&fileBuf);
		if (ret != 0)
			goto release;
		ret = fileBuf->read(buf, nbyte, offset, read);
		if (ret == 0)
			goto release;
		if (daemon->error != 0)
			ret = daemon->error;
		if (daemon->state == DAEMON_STATE_DONE)
			ret = MTPR2_ERROR_CANCELED;
	}
	MTPR2_PRINT_ERROR();
release:
	g_fileHandleTable->releaseDaemon(daemon);
	return ret;
}

int MTPR2AbortDownload(void)
{
	int ret = 0;

	if (g_fileHandleTable == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NOT_STARTED;
	}
	g_fileHandleTable->cancel();
	return ret;
}
