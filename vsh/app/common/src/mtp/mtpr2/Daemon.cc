/*
 * Daemon.cc: the thread that downloads the file (0x81136608-0x81136B70): it starts the
 * responder the download asks for, receives the file into its FileBuf while the reader
 * makes room, and on an error waits for the reader to give up.
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

Daemon::Daemon(CCondvar *cCondvar, const char *url, long long offset, unsigned int ringBufSize,
	       unsigned int *timeout)
{
	this->cCondvar = cCondvar;
	registered = false;
	state = DAEMON_STATE_NONE;
	error = 0;
	cancel = 0;
	fileBuf = NULL;
	buf = NULL;
	bufSize = ringBufSize >> 1;
	protocol = DAEMON_PROTOCOL_PROTO2;
	mtpResponder = NULL;
	this->offset = offset;
	this->ringBufSize = ringBufSize;
	this->timeout = timeout;
	size = 0;
	time = 0;
	kind = 0;
	thread = 0;
	if (offset != 0)
		MTPR2_PRINT_ERROR();
	sce_paf_strncpy(this->url, url, sizeof(this->url));
}

Daemon::~Daemon()
{
}

int Daemon::initialize(void)
{
	int err;

	registered = false;
	fileBuf = NULL;
	buf = NULL;
	fileBuf = (FileBuf *)sce_paf_malloc(sizeof(FileBuf));
	if (fileBuf == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NO_MEMORY;
	}
	new (fileBuf) FileBuf(ringBufSize, cCondvar, timeout, &err);
	if (err != 0) {
		sce_paf_free(fileBuf);
		fileBuf = NULL;
		if (buf != NULL) {
			sce_paf_free(buf);
			buf = NULL;
			if (fileBuf != NULL) {
				fileBuf->~FileBuf();
				sce_paf_free(fileBuf);
				fileBuf = NULL;
			}
		}
		return err;
	}
	bufSize = ringBufSize >> 1;
	buf = sce_paf_memalign(MTPR2_FILE_BUF_LOW, bufSize);
	if (buf == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_NO_MEMORY;
	}
	g_fileHandleTable->registerDaemon(this);
	registered = true;
	return 0;
}

int Daemon::startResponder(void)
{
	int ret;

	switch (protocol) {
	case DAEMON_PROTOCOL_HOSTFS:
		mtpResponder = (MtpResponder *)sce_paf_malloc(sizeof(MtpResponderHostFs));
		if (mtpResponder == NULL)
			goto nomem;
		new (mtpResponder) MtpResponderHostFs();
		break;
	case DAEMON_PROTOCOL_PROTO2:
		mtpResponder = (MtpResponder *)sce_paf_malloc(sizeof(MtpResponderProto2));
		if (mtpResponder == NULL)
			goto nomem;
		new (mtpResponder) MtpResponderProto2();
		break;
	}
	ret = mtpResponder->getExclusiveUse();
	if (ret == 0) {
		ret = mtpResponder->startDownload(url, offset, &kind, &size, &time);
		if (ret == 0) {
			fileBuf->start(size);
			return 0;
		}
		MTPR2_PRINT_ERROR();
	}
	if (mtpResponder != NULL) {
		mtpResponder->~MtpResponder();
		sce_paf_free(mtpResponder);
		mtpResponder = NULL;
	}
	return ret;

nomem:
	MTPR2_PRINT_ERROR();
	return MTPR2_ERROR_NO_MEMORY;
}

int Daemon::terminate(void)
{
	int ret;

	if (mtpResponder != NULL) {
		ret = mtpResponder->endDownload();
		if (ret != 0)
			MTPR2_PRINT_ERROR();
		ret = mtpResponder->releaseExclusiveUse();
		if (ret != 0)
			MTPR2_PRINT_ERROR();
		mtpResponder->~MtpResponder();
		sce_paf_free(mtpResponder);
		mtpResponder = NULL;
	}
	if (registered != false) {
		g_fileHandleTable->unregisterDaemon(this);
		registered = false;
	}
	if (buf != NULL) {
		sce_paf_free(buf);
		buf = NULL;
	}
	if (fileBuf != NULL) {
		fileBuf->~FileBuf();
		sce_paf_free(fileBuf);
		fileBuf = NULL;
	}
	if (cCondvar != NULL) {
		cCondvar->~CCondvar();
		sce_paf_free(cCondvar);
		cCondvar = NULL;
	}
	sce_paf_free(this);
	if (g_fileHandleTable != NULL) {
		g_fileHandleTable->~FileHandleTable();
		sce_paf_free(g_fileHandleTable);
		g_fileHandleTable = NULL;
	}
	return 0;
}

void Daemon::main(void)
{
	unsigned int timeout;
	unsigned int received;
	unsigned int size;		/* what is received, then what is written */
	int ret;

	ret = initialize();
	if (ret != 0) {
		error = ret;
		goto wait;
	}
	if (cancel == 1)
		goto canceled;
	state = DAEMON_STATE_STARTING;
	if (g_fileHandleTable->isCanceled() != false)
		goto canceled;
	cCondvar->signal();
	ret = startResponder();
	if (ret != 0) {
		error = ret;
		MTPR2_PRINT_ERROR();
		goto wait;
	}
	state = DAEMON_STATE_RUNNING;
	cCondvar->signal();
	while (cancel != 1) {
		for (;;) {
			size = fileBuf->freeSize();
			if (size <= MTPR2_FILE_BUF_LOW || offset >= this->size)
				break;
			if (size > bufSize)
				size = bufSize;
			ret = mtpResponder->receiveData(buf, size, &received);
			if (ret != 0) {
				MTPR2_PRINT_ERROR();
				error = ret;
				fileBuf->status = FILE_BUF_STATUS_ENDED;
				cCondvar->signal();
				goto wait;
			}
			ret = fileBuf->write(buf, received, offset, &size);
			if (ret != 0) {
				MTPR2_PRINT_ERROR();
				mtpResponder->abortDownload();
				if (ret == MTPR2_ERROR_CANCELED)
					goto end;
				goto wait;
			}
			offset += received;
		}
		if (this->size <= offset) {
			MTPR2_PRINT_ERROR();
			goto wait;
		}
		timeout = MTPR2_WAIT_US;
		ret = cCondvar->wait(&timeout);
		if (ret != 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			MTPR2_PRINT_ERROR();
			error = ret;
			fileBuf->status = FILE_BUF_STATUS_ENDED;
			cCondvar->signal();
			mtpResponder->abortDownload();
			goto wait;
		}
	}
	error = MTPR2_ERROR_CANCELED;
	MTPR2_PRINT_ERROR();
	mtpResponder->abortDownload();
	goto end;

canceled:
	error = MTPR2_ERROR_CANCELED;
	goto end;

wait:
	state = DAEMON_STATE_ERROR;
	do {
		if (cancel == 1) {
			MTPR2_PRINT_ERROR();
			goto end;
		}
		timeout = MTPR2_DAEMON_ERROR_WAIT_US;
		ret = cCondvar->wait(&timeout);
	} while (ret == 0 || ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT);
	MTPR2_PRINT_ERROR();
	error = ret;

end:
	state = DAEMON_STATE_DONE;
	fileBuf->status = FILE_BUF_STATUS_ENDED;
	cCondvar->signal();
	terminate();
	sceKernelExitDeleteThread(error);
}

int Daemon::getFileBuf(FileBuf **fileBuf)
{
	*fileBuf = NULL;
	if (error != 0)
		return error;
	*fileBuf = this->fileBuf;
	return 0;
}

int Daemon::getFileAttributes(MTPR2FileAttributes *attr)
{
	if (error != 0)
		return error;
	attr->size = size;
	attr->time = time;
	attr->kind = kind;
	return 0;
}

int Daemon::abortResponder(void)
{
	return mtpResponder->abortDownload();
}
