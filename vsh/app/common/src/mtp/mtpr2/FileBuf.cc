/*
 * FileBuf.cc: the downloaded file between the daemon and the reader (0x81136B70-0x81136EEA):
 * the daemon writes what it receives into the ring buffer, the reader takes it out in order
 * and wakes the daemon when there is room again.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <std/new>
#include <std/stdlib.h>

#include "mtpr2_internal.h"

using namespace mtpr2;

/* what a read or write in the wrong status returns */
#define FILE_BUF_STATUS_ERROR()								\
	do {										\
		MTPR2_PRINT_ERROR();							\
		return status == FILE_BUF_STATUS_CANCELED ? MTPR2_ERROR_CANCELED :	\
							    MTPR2_ERROR_NOT_STARTED;	\
	} while (0)

FileBuf::FileBuf(unsigned int ringBufSize, CCondvar *cCondvar, unsigned int *timeout, int *error)
{
	offset = 0;
	this->cCondvar = cCondvar;
	MTPR2_ASSERT(ringBufSize && cCondvar && error);
	*error = 0;
	mutex = sceKernelCreateMutex("SceMtpr2FileBuf", SCE_KERNEL_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (mutex >= 0) {
		ringBuf = (RingBuf *)sce_paf_malloc(sizeof(RingBuf));
		if (ringBuf == NULL) {
			MTPR2_PRINT_ERROR();
			*error = MTPR2_ERROR_NO_MEMORY;
		}
		new (ringBuf) RingBuf(ringBufSize, error);
		if (*error == 0) {
			this->timeout = timeout;
			status = FILE_BUF_STATUS_INITIALIZED;
			size = 0;
			return;
		}
		sce_paf_free(ringBuf);
		ringBuf = NULL;
	} else {
		MTPR2_PRINT_ERROR();
		*error = mutex;
		if (ringBuf != NULL) {
			ringBuf->~RingBuf();
			sce_paf_free(ringBuf);
			ringBuf = NULL;
		}
	}
	if (mutex >= 0) {
		sceKernelDeleteMutex(mutex);
		mutex = -1;
	}
}

void FileBuf::start(unsigned int size)
{
	this->size = size;
	status = FILE_BUF_STATUS_STARTED;
}

FileBuf::~FileBuf()
{
	if (cCondvar != NULL) {
		cCondvar->signal();
		cCondvar = NULL;
	}
	status = FILE_BUF_STATUS_ENDED;
	if (ringBuf != NULL) {
		ringBuf->~RingBuf();
		sce_paf_free(ringBuf);
		ringBuf = NULL;
	}
	if (mutex != 0) {
		sceKernelDeleteMutex(mutex);
		mutex = 0;
	}
}

int FileBuf::read(void *buf, unsigned int size, long long offset, unsigned int *read)
{
	unsigned int left = 0;
	unsigned int wait;
	int ret = 0;
	bool wake = false;
	int err;

	if (timeout != NULL)
		left = *timeout;
	if (status != FILE_BUF_STATUS_STARTED)
		FILE_BUF_STATUS_ERROR();
	if (buf == NULL || read == NULL) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_INVALID_ARGUMENT;
	}
	*read = 0;
	if ((long long)this->size <= offset) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_INVALID_ARGUMENT;
	}
	if (this->offset != (SceUInt64)offset) {
		MTPR2_PRINT_ERROR();
		return MTPR2_ERROR_INVALID_ARGUMENT;
	}
	while (ringBuf->dataSize() == 0) {
		if (this->offset == this->size) {
			*read = wake;		/* nothing read: the file has ended */
			return wake;
		}
		wait = MTPR2_WAIT_US;
		err = cCondvar->signalAndWait(&wait);
		if (err != 0) {
			if (err != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				return err;
			if (timeout != NULL) {
				if (left < MTPR2_WAIT_US) {
					MTPR2_PRINT_ERROR();
					return MTPR2_ERROR_TIMEOUT;
				}
				left -= MTPR2_WAIT_US;
			}
		}
		if (status != FILE_BUF_STATUS_STARTED)
			FILE_BUF_STATUS_ERROR();
	}
	if (ringBuf->freeSize() < MTPR2_FILE_BUF_LOW)
		wake = true;
	wait = MTPR2_LOCK_TIMEOUT_US;
	err = sceKernelLockMutex(mutex, 1, &wait);
	MTPR2_ASSERT(err == 0);
	ringBuf->read(buf, size, read);
	this->offset += *read;
	err = sceKernelUnlockMutex(mutex, 1);
	MTPR2_ASSERT(err == 0);
	if (wake != false)
		cCondvar->signal();
	return ret;
}

int FileBuf::write(void *buf, unsigned int size, long long offset, unsigned int *written)
{
	int ret = 0;
	bool wake = false;

	if (status != FILE_BUF_STATUS_STARTED)
		FILE_BUF_STATUS_ERROR();
	if (ringBuf->dataSize() == 0)
		wake = true;
	ringBuf->write(buf, size, written);
	MTPR2_ASSERT(size == *written);
	if (wake != false)
		cCondvar->signal();
	return ret;
}

unsigned int FileBuf::freeSize(void)
{
	MTPR2_ASSERT(ringBuf != NULL);
	return ringBuf->freeSize();
}
