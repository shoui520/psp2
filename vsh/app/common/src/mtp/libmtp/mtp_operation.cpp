/*
 * The standard MTP operations ("CMD :"): the storages, the objects, their properties and
 * references, the device property values, and the file data a SendObject receives.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <sceerror.h>
#include "mtp_internal.h"

#define MTP_TAG_CMD			"CMD :"

/* the transfer buffers: the heap's, or the large buffer's while it is free */
#define MTP_CMD_XFER_SIZE		0x10000
#define MTP_CMD_XFER_SIZE_LARGE		0x100000
#define MTP_CMD_XFER_ALIGN		0x40

/* the buffer an operation's dataset is built in */
#define MTP_CMD_WORK_SIZE		0x2000

/* the largest file a host may send whose size it gives only in the data */
#define MTP_CMD_FILE_SIZE_MAX		0x1000000000LL

/* what a SendObject's progress reports, and how often (in percent) */
#define MTP_CORE_PUBLIC_STATUS_RECEIVING	6
#define MTP_CMD_PROGRESS_STEP		10

/* the transfer thread's receive request, and the wait for its end */
#define MTP_XFER_REQUEST_SEND		1
#define MTP_XFER_REQUEST_RECV		2
#define MTP_XFER_DONE_SEND		0x10
#define MTP_XFER_DONE			0x20

/* the buffer states mtpUtilGetBufferState reports between a transfer's buffers */
#define MTP_UTIL_BUFFER_STATE_TO_HEAP	1
#define MTP_UTIL_BUFFER_STATE_TO_LARGE	3

extern "C" {

/*
 * receives the data phase of an operation into path (created when create is set), size bytes
 * or, at 0xFFFFFFFF, as many as the data says; received counts the bytes, responseCode is set
 * when the operation must fail with a response of its own
 */
int mtpCmdUtilReceiveFileData(const MtpOperation *operation, MtpOperation *response,
	const char *path, SceSize size, int create, SceUInt64 *received, SceUInt32 *responseCode,
	MtpContext *context)
{
	void *(*allocFunc)(SceSize align, SceSize size);
	void (*freeFunc)(void *ptr);
	SceSize bufSize;
	SceInt64 remaining;
	void *buffers[2];
	int bufferState;
	SceSize xferSize;
	int started;
	int current;
	SceInt64 percentTotal;
	int lastPercent;
	int percent;
	SceInt64 progressTotal;
	int flags;
	SceMode mode;
	SceUID fd;
	int count;
	int ret = 0;

	buffers[0] = NULL;
	buffers[1] = NULL;
	mode = 0;
	started = 0;
	*received = 0;
	current = 0;
	allocFunc = mtpUtilMallocAlign;
	lastPercent = 0;
	freeFunc = mtpUtilFree;
	bufSize = MTP_CMD_XFER_SIZE;
	flags = SCE_O_RDWR;
	if (mtpUtilIsLargeBufferEnabled()) {
		bufferState = mtpUtilGetBufferState();
		if (bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE) {
			mtpUtilSetBufferState(MTP_UTIL_BUFFER_STATE_HEAP);
		} else if (bufferState != MTP_UTIL_BUFFER_STATE_HEAP &&
			   mtpUtilLargeBufferStart() == 0) {
			freeFunc = mtpUtilLargeBufferFree;
			bufSize = MTP_CMD_XFER_SIZE_LARGE;
			allocFunc = mtpUtilLargeBufferMemalign;
		}
	}
	buffers[0] = allocFunc(MTP_CMD_XFER_ALIGN, bufSize);
	buffers[1] = allocFunc(MTP_CMD_XFER_ALIGN, bufSize);
	if (buffers[0] == NULL || buffers[1] == NULL) {
		MTP_ERR(MTP_TAG_CMD, 663, "unable to alloc memory for xfer buffers\n");
		*responseCode = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto free;
	}
	if (create) {
		flags = SCE_O_RDWR | SCE_O_CREAT;
		mode = 0007;
	}
	fd = mtpFileOpen(path, flags, mode);
	if (fd < 0) {
		MTP_ERR(MTP_TAG_CMD, 677, "unable to open file=%s, 0x%x\n", path, fd);
		ret = -2;
		goto free;
	}
	mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, 0);
	if (context != NULL)
		mtpContextSetProgress(context, 0, size, 1);
	if (size == 0xFFFFFFFF) {
		count = mtpCoreDriverRecvData(buffers[0], bufSize, (SceUInt64 *)&remaining);
		if (count < 0) {
			MTP_ERR(MTP_TAG_CMD, 694, "unable to receive data=0x%x\n", count);
			ret = count;
			goto close;
		}
		if (count < (int)bufSize) {
			remaining = count;
		} else if (remaining != -1 && (SceUInt64)remaining > MTP_CMD_FILE_SIZE_MAX) {
			MTP_ERR(MTP_TAG_CMD, 705, "File size is too large. size=%lld\n", remaining);
			ret = -2;
			goto close;
		}
		xferSize = bufSize;
		progressTotal = remaining;
		percentTotal = size;
	} else {
		remaining = size;
		xferSize = bufSize;
		if (size < bufSize)
			xferSize = size;
		percentTotal = size;
		progressTotal = size;
		count = mtpCoreDriverRecvData(buffers[0], xferSize, NULL);
		if (count < 0) {
			MTP_ERR(MTP_TAG_CMD, 720, "unable to receive data=0x%x\n", count);
			ret = count;
			goto close;
		}
	}
	while (remaining > 0 || remaining == -1) {
		if (remaining > 0)
			remaining -= count;
		*received += count;
		if (context != NULL)
			mtpContextSetProgress(context, *received, progressTotal, 0);
		bufferState = mtpUtilGetBufferState();
		started = 0;
		if (bufferState != MTP_UTIL_BUFFER_STATE_TO_LARGE &&
		    bufferState != MTP_UTIL_BUFFER_STATE_TO_HEAP &&
		    (remaining > 0 || remaining == -1)) {
			xferSize = bufSize;
			if (remaining != -1 && remaining < bufSize)
				xferSize = remaining;
			ret = mtpXferStart(MTP_XFER_REQUEST_RECV, buffers[!current], xferSize, 0);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_CMD, 751, "unable to start xfer=0x%x\n", ret);
				ret = -2;
				break;
			}
			started = 1;
		}
		ret = mtpFileWrite(fd, buffers[current], count);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 762, "unable to write 0x%x\n", ret);
			mtpXferWait(MTP_XFER_DONE);
			mtpDevIfCancelTransfer();
			*responseCode = MTP_RESPONSE_TRANSACTION_CANCELLED;
			ret = -2;
			break;
		}
		if (started) {
			count = mtpXferWait(MTP_XFER_DONE);
			if (count < 0) {
				MTP_ERR(MTP_TAG_CMD, 776, "wait xfer failed=0x%x\n", count);
				ret = count;
				break;
			}
			if (remaining == -1 && count < (int)xferSize)
				remaining = count;
			else if (count == 0)
				remaining = 0;
		}
		current ^= 1;
		percent = 100 - (int)(remaining * 100 / percentTotal);
		if (percent - lastPercent >= MTP_CMD_PROGRESS_STEP) {
			mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, percent);
			lastPercent = percent;
		}
		if ((bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE && !started) ||
		    (bufferState == MTP_UTIL_BUFFER_STATE_TO_HEAP && !started)) {
			ret = mtpUtilChangeBufferAllocator(&buffers[0], &buffers[1], &bufSize,
				&freeFunc, bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_CMD, 812,
					"unable to alloc memory for xfer buffers\n");
				*responseCode = MTP_RESPONSE_VENDOR_NO_MEMORY;
				ret = -2;
			}
			count = 0;
		}
		if (ret == -2)
			break;
	}
	if (progressTotal == -1 && context != NULL)
		mtpContextSetProgress(context, *received, *received, 0);
close:
	mtpFileClose(fd);
free:
	freeFunc(buffers[0]);
	freeFunc(buffers[1]);
	if (mtpContextIsDeleted(context))
		mtpUtilLargeBufferEnd();
	return ret;
}

int mtpCmdGetStorageIds(const MtpOperation *operation, MtpOperation *response)
{
	MtpStorageIds ids;
	SceUInt8 *buf;
	int send = 1;
	int ret;
	int n;

	buf = (SceUInt8 *)mtpUtilMalloc(MTP_CMD_WORK_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 868, "unable to alloc work buffer\n");
		ret = -2;
		goto error;
	}
	ids.num = MTP_CMD_WORK_SIZE / sizeof(SceUInt32) - 1;
	ids.ids = (SceUInt32 *)(buf + 4);
	n = mtpStrgGetStorageIds(&ids);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 878, "unable to get storage ids=0x%x", n);
		ret = -2;
		goto free;
	}
	mtpUtilMemcpy(buf, &ids.num, 4);
	ret = mtpCoreSendData(buf, n + 4, n + 4);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 892, "unable to send data=0x%x\n", ret);
		send = 0;
		goto free;
	}
	mtpUtilFree(buf);
	return ret;
free:
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret,
		MTP_RESPONSE_GENERAL_ERROR, send);
}

int mtpCmdGetStorageInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 storageId = operation->param[0];
	const MtpStorageDataset *dataset = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	MtpString string;
	SceUInt8 *buf;
	int send = 1;
	int ret;
	int n1;
	int n2;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 928, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if (!mtpStrgIsStorageValid(storageId)) {
		MTP_ERR(MTP_TAG_CMD, 936, "invalid storage id\n");
		code = MTP_RESPONSE_INVALID_STORAGE_ID;
		ret = -2;
		goto error;
	}
	if (!mtpStrgIsStorageAvailable(storageId)) {
		MTP_ERR(MTP_TAG_CMD, 944, "storage not available\n");
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto error;
	}
	ret = mtpStrgGetStorageInfo(storageId, &dataset);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 952, "unable to get storage info=0x%x\n", ret);
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto error;
	}
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_CMD_WORK_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 961, "unable to alloc memory for send data\n");
		ret = -2;
		goto error;
	}
	mtpUtilMemcpy(buf, &dataset->storageType, 2);
	mtpUtilMemcpy(buf + 2, &dataset->filesystemType, 2);
	mtpUtilMemcpy(buf + 4, &dataset->accessCapability, 2);
	mtpUtilMemcpy(buf + 6, &dataset->maxCapacity, 8);
	mtpUtilMemcpy(buf + 14, &dataset->freeSpaceInBytes, 8);
	mtpUtilMemcpy(buf + 22, &dataset->freeSpaceInObjects, 4);
	string.numChars = 0xFF;
	string.chars = buf + 27;
	n1 = mtpUtilPackMtpString(&string, dataset->storageDescription);
	if (n1 < 0) {
		MTP_ERR(MTP_TAG_CMD, 989, "unable to pack MTP Extensions=0x%x", n1);
		ret = -2;
		goto free;
	}
	buf[26] = string.numChars;
	string.numChars = 0xFF;
	string.chars = buf + n1 + 28;
	n2 = mtpUtilPackMtpString(&string, dataset->volumeIdentifier);
	if (n2 < 0) {
		MTP_ERR(MTP_TAG_CMD, 1001, "unable to pack MTP Extensions=0x%x", n2);
		ret = -2;
		goto free;
	}
	buf[n1 + 27] = string.numChars;
	ret = mtpCoreSendData(buf, n1 + n2 + 28, (SceSize)(n1 + n2 + 28));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1011, "unable to send data=0x%x\n", ret);
		send = 0;
		goto free;
	}
	mtpUtilFree(buf);
	return ret;
free:
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

/* GetNumObjects (countOnly) and GetObjectHandles */
static int _getObjectHandles(const MtpOperation *operation, MtpOperation *response,
	int countOnly)
{
	SceUInt32 storageId = operation->param[0];
	SceUInt32 numParams = operation->numParams;
	SceUInt32 format = 0;
	SceUInt32 num = 0;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = countOnly == 0;
	/* Sony's default parent is the send flag: 1 for GetObjectHandles, 0 for GetNumObjects */
	SceUInt32 parent = send;
	int ret;

	if (numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 68, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if (numParams >= 2)
		format = (SceUInt16)operation->param[1];
	if (numParams >= 3)
		parent = operation->param[2];
	if (storageId != 0xFFFFFFFF && !mtpStrgIsStorageValid(storageId)) {
		MTP_ERR(MTP_TAG_CMD, 83, "invalid storage id=0x%08x\n", storageId);
		code = MTP_RESPONSE_INVALID_STORAGE_ID;
		ret = -2;
		goto error;
	}
	if (format != 0) {
		MTP_ERR(MTP_TAG_CMD, 92, "format code is not supported\n");
		code = MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
		ret = -2;
		goto error;
	}
	if (storageId != 0xFFFFFFFF && parent != 0xFFFFFFFF &&
	    storageId != MTP_OBJECT_STORAGE_ID(parent)) {
		MTP_ERR(MTP_TAG_CMD, 103, "sotarge Id and Mtp Handle mismatch\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if (parent != 0 && (parent & MTP_OBJECT_HANDLE_DIR) == 0) {
		MTP_ERR(MTP_TAG_CMD, 112, "invalid parent handle\n");
		code = MTP_RESPONSE_INVALID_PARENT_OBJECT;
		ret = -2;
		goto error;
	}
	ret = mtpStrgGetNumObjects(storageId, parent, &num);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 120, "unable to get num objects=0x%x\n", ret);
		switch (ret) {
		case SCE_MTP_ERROR_DEVICE:
			code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
			break;
		case SCE_MTP_ERROR_INVALID_PARENT:
			code = MTP_RESPONSE_INVALID_PARENT_OBJECT;
			break;
		}
		ret = -2;
		goto error;
	}
	if (!countOnly) {
		ret = mtpCoreSendData(&num, sizeof(num), sizeof(num));
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 143, "unable to send data=0x%x\n", ret);
			send = 0;
			goto error;
		}
		ret = mtpStrgGetMtpHandles(storageId, parent, num, &send);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 148, "unable to get object handles=0x%x\n", ret);
			switch (ret) {
			case SCE_MTP_ERROR_DEVICE:
				code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
				break;
			case SCE_MTP_ERROR_INVALID_PARENT:
				code = MTP_RESPONSE_INVALID_PARENT_OBJECT;
				break;
			}
			goto error;
		}
	}
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &num, 1, operation, response);
	return ret;
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdGetNumObjects(const MtpOperation *operation, MtpOperation *response)
{
	return _getObjectHandles(operation, response, 1);
}

int mtpCmdGetObjectHandles(const MtpOperation *operation, MtpOperation *response)
{
	return _getObjectHandles(operation, response, 0);
}

int mtpCmdGetObjectInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 handle = operation->param[0];
	MtpLogicalStorage *storage = NULL;
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	SceUInt32 uniqueId;
	SceUInt8 *buf;
	int send = 1;
	int ret;
	int n;

	mtpContextGetContextByEvent(MTP_OPERATION_REQUEST_OBJECT_INFO, &context);
	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 1078, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if (!mtpStrgIsObjectHandleValid(handle)) {
		MTP_ERR(MTP_TAG_CMD, 1086, "invalid object handle\n");
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto error;
	}
	ret = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1094, "unable to get logical storage=0x%x", ret);
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto error;
	}
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_CMD_WORK_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 1103, "unable to alloc memory for xfer\n");
		ret = -2;
		goto error;
	}
	ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1110, "unable to convert MTP handle(%x) to uniqueId %x\n",
			handle, ret);
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	n = mtpObjPackObjectInfo(storage, uniqueId, buf, MTP_CMD_WORK_SIZE);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 1118, "unable to pack object inf=0x%x\n", n);
		ret = -2;
		goto free;
	}
	ret = mtpCoreSendData(buf, n, n);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1127, "unable to send data=0x%x\n", ret);
		send = 0;
		goto free;
	}
	mtpUtilFree(buf);
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return ret;
free:
	mtpUtilFree(buf);
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

/* GetObject, and GetPartialObject (partial: param[1] the offset, param[2] the bytes) */
static int _getObject(const MtpOperation *operation, MtpOperation *response, int partial)
{
	SceUInt32 handle = operation->param[0];
	void *(*allocFunc)(SceSize align, SceSize size);
	void (*freeFunc)(void *ptr) = NULL;
	MtpLogicalStorage *storage = NULL;
	MtpContext *context = NULL;
	void *buffers[2];
	SceSize bufSize = 0;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int current = 0;
	int lastPercent = 0;
	int percent;
	int bufferState;
	int started;
	SceUInt64 objectSize;
	SceUInt64 xferSize;
	SceUInt64 offset;
	SceUInt64 total;
	SceInt64 remaining;
	SceUInt32 count;
	SceUInt32 uniqueId;
	SceSize nameLength;
	char *path;
	char *pfsPath;
	SceUID fd;
	int ret;

	buffers[0] = NULL;
	buffers[1] = NULL;
	mtpContextGetContextByEvent(MTP_OPERATION_REQUEST_OBJECT_INFO, &context);
	if (partial) {
		if (operation->numParams < 3) {
			MTP_ERR(MTP_TAG_CMD, 216, "wrong param\n");
			code = MTP_RESPONSE_INVALID_PARAMETER;
			ret = -2;
			goto error;
		}
	} else if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 221, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if ((handle & MTP_OBJECT_HANDLE_DIR) != 0) {
		ret = -2;
		MTP_ERR(MTP_TAG_CMD, 232, "handle is association\n");
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		goto error;
	}
	if (mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage) < 0) {
		ret = -2;
		MTP_ERR(MTP_TAG_CMD, 240, "storage is not available\n");
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		goto error;
	}
	nameLength = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(nameLength * 2 + 2046);
	if (path == NULL) {
		ret = -2;
		MTP_ERR(MTP_TAG_CMD, 250, "unable to alloc memory for file name\n");
		goto error;
	}
	pfsPath = &path[nameLength + 1023];
	ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (ret < 0) {
		ret = -2;
		MTP_ERR(MTP_TAG_CMD, 258, "unable to convert MTP handle to UniquId=0x%x\n", ret);
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		goto free;
	}
	mtpUtilStrcpy(path, storage->info->name);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_PATH,
		&path[nameLength], 1023);
	if (ret < 0) {
		ret = -2;
		MTP_ERR(MTP_TAG_CMD, 270, "unable to get full path name from cache=0x%x\n", ret);
		goto free;
	}
	ret = mtpStrgCreatePfsPath(storage, &path[nameLength], pfsPath, nameLength + 1023, 1);
	if (ret < 0)
		goto free;
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_SIZE,
		&xferSize, sizeof(xferSize));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 283, "unable to get object size from cache=0x%x\n", ret);
		ret = -2;
		goto free;
	}
	objectSize = xferSize;
	if (partial == 1) {
		count = operation->param[2];
		offset = operation->param[1];
		total = objectSize;
		if (count < objectSize)
			total = count;
		if (offset + total > objectSize) {
			MTP_ERR(MTP_TAG_CMD, 294, "size illegal over %dbytes\n",
				(int)(offset + total - objectSize));
			code = MTP_RESPONSE_INVALID_PARAMETER;
			goto free;
		}
	} else {
		total = objectSize;
		offset = 0;
	}
	remaining = total;
	mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, 0);
	if (context != NULL)
		mtpContextSetProgress(context, 0, total, 1);
	if (remaining <= 0) {
		ret = mtpCoreSendData(&xferSize, 0, 0);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 449, "unable to send data=0x%x", ret);
			send = 0;
			goto free;
		}
		goto done;
	}
	freeFunc = mtpUtilFree;
	bufSize = MTP_CMD_XFER_SIZE;
	allocFunc = mtpUtilMallocAlign;
	if (mtpUtilIsLargeBufferEnabled()) {
		bufferState = mtpUtilGetBufferState();
		if (bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE) {
			mtpUtilSetBufferState(MTP_UTIL_BUFFER_STATE_HEAP);
		} else if (bufferState != MTP_UTIL_BUFFER_STATE_HEAP &&
			   mtpUtilLargeBufferStart() == 0) {
			freeFunc = mtpUtilLargeBufferFree;
			bufSize = MTP_CMD_XFER_SIZE_LARGE;
			allocFunc = mtpUtilLargeBufferMemalign;
		}
	}
	buffers[0] = allocFunc(MTP_CMD_XFER_ALIGN, bufSize);
	buffers[1] = allocFunc(MTP_CMD_XFER_ALIGN, bufSize);
	if (buffers[0] == NULL || buffers[1] == NULL) {
		ret = -2;
		MTP_ERR(MTP_TAG_CMD, 342, "unable to alloc xfer bufs\n");
		goto free_buffers;
	}
	fd = mtpFileOpen(pfsPath, SCE_O_RDONLY, 0);
	if (fd < 0) {
		ret = -2;
		MTP_ERR(MTP_TAG_CMD, 351, "unable to open=%s, 0x%x\n", pfsPath, fd);
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		goto free_buffers;
	}
	if (offset > 0 && mtpFileSeek(fd, offset, SCE_SEEK_SET) < 0) {
		MTP_ERR(MTP_TAG_CMD, 360, "unable to seek to offset\n");
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto close;
	}
	do {
		bufferState = mtpUtilGetBufferState();
		xferSize = 0;
		started = 0;
		if (bufferState != MTP_UTIL_BUFFER_STATE_TO_LARGE &&
		    bufferState != MTP_UTIL_BUFFER_STATE_TO_HEAP) {
			xferSize = remaining;
			if (remaining >= bufSize)
				xferSize = bufSize;
			if (mtpFileRead(fd, buffers[current], xferSize) < 0) {
				ret = -2;
				MTP_ERR(MTP_TAG_CMD, 376, "unable to read data\n");
				mtpXferWait(MTP_XFER_DONE_SEND);
				mtpDevIfCancelTransfer();
				code = MTP_RESPONSE_TRANSACTION_CANCELLED;
				goto close;
			}
			started = 1;
		}
		ret = mtpXferWait(MTP_XFER_DONE_SEND);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 389, "wait xfer error=0x%x\n", ret);
			send = 0;
			goto close;
		}
		if (started) {
			ret = mtpXferStart(MTP_XFER_REQUEST_SEND, buffers[current], xferSize, total);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_CMD, 397, "start xfer error=0x%x\n", ret);
				ret = -2;
				goto close;
			}
		}
		remaining -= xferSize;
		percent = 100 - (int)(remaining * 100 / (SceInt64)total);
		if (percent - lastPercent >= MTP_CMD_PROGRESS_STEP) {
			mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, percent);
			lastPercent = percent;
		}
		if (context != NULL)
			mtpContextSetProgress(context, total - remaining, total, 0);
		if ((bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE && !started) ||
		    (bufferState == MTP_UTIL_BUFFER_STATE_TO_HEAP && !started)) {
			ret = mtpUtilChangeBufferAllocator(&buffers[0], &buffers[1], &bufSize,
				&freeFunc, bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE);
			if (ret < 0) {
				ret = -2;
				MTP_ERR(MTP_TAG_CMD, 429, "unable to alloc xfer bufs\n");
			}
		}
		if (ret == -2)
			break;
		current ^= 1;
	} while (remaining > 0);
	ret = mtpXferWait(MTP_XFER_DONE_SEND);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 440, "wait xfer error=0x%x\n", ret);
		send = 0;
		goto close;
	}
	mtpFileClose(fd);
done:
	if (partial) {
		count = (SceUInt32)(total - remaining);
		if ((total - remaining) >> 32 != 0)
			count = 0xFFFFFFFF;
		mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &count, 1, operation, response);
	}
	if (freeFunc != NULL) {
		freeFunc(buffers[0]);
		freeFunc(buffers[1]);
	}
	mtpUtilFree(path);
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return ret;
close:
	mtpFileClose(fd);
free_buffers:
	freeFunc(buffers[0]);
	freeFunc(buffers[1]);
	if (mtpContextIsDeleted(context))
		mtpUtilLargeBufferEnd();
free:
	mtpUtilFree(path);
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdGetObject(const MtpOperation *operation, MtpOperation *response)
{
	return _getObject(operation, response, 0);
}

/* reads a little-endian field of a received dataset */
static inline SceUInt32 mtpCmdGetLe16(const SceUInt8 *p)
{
	return p[0] | p[1] << 8;
}

static inline SceUInt32 mtpCmdGetLe32(const SceUInt8 *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
}

int mtpCmdSendObjectInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 storageId = 0;
	MtpLogicalStorage *storage = NULL;
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const char *dateModified = NULL;
	const char *dateCreated = NULL;
	SceUInt32 reserved = 0;
	int atRoot = 0;
	SceUInt32 params[5];
	SceUInt32 parent;
	SceUInt32 parentStorage;
	SceUInt32 uniqueId;
	SceUInt32 handle;
	SceUInt32 transfer;
	SceUInt32 format;
	SceUInt32 size;
	SceUInt32 imagePixWidth;
	SceUInt32 imagePixHeight;
	char created[18];
	char modified[18];
	MtpString string;
	SceUInt8 *buf;
	char *name;
	char *trimmed;
	int offset;
	int ret;
	int n;

	ret = mtpContextGetContextByEvent(MTP_OPERATION_REQUEST_SEND_OBJECT, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1303, "GetContextByEvent failed: 0x%x\n", ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (operation->numParams != 0)
		storageId = operation->param[0];
	if (operation->numParams >= 2)
		parent = operation->param[1];
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_CMD_WORK_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 1321, "unable to alloc for receive buffer\n");
		code = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(buf, MTP_CMD_WORK_SIZE, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1329, "unable to receive data=0x%x\n", ret);
		goto error;
	}
	format = mtpCmdGetLe16(buf + 4);
	size = mtpCmdGetLe32(buf + 8);
	imagePixWidth = mtpCmdGetLe32(buf + 26);
	imagePixHeight = mtpCmdGetLe32(buf + 30);
	string.numChars = 0xFF;
	string.chars = buf + 53;
	name = (char *)mtpUtilMalloc(1023);
	if (name == NULL) {
		MTP_ERR(MTP_TAG_CMD, 1353, "unable to alloc memory for object file name\n");
		goto free_buf;
	}
	if (context != NULL && mtpUtilStrlen((const char *)context->work) != 0) {
		mtpUtilStrncpy(name, (const char *)context->work, 1023);
	} else {
		n = mtpUtilUnpackMtpString(name, 1023, &string);
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMD, 1363, "unable to unpack object file name=0x%x\n", n);
			goto free_name;
		}
		if (n > 192) {
			trimmed = (char *)mtpUtilMalloc(192);
			if (trimmed == NULL) {
				MTP_ERR(MTP_TAG_CMD, 1369,
					"unable to alloc memory for object file name buffer\n");
				goto free_name;
			}
			n = mtpUtilTrimFileName(name, trimmed, 192, 192);
			if (n < 0) {
				mtpUtilFree(trimmed);
				MTP_ERR(MTP_TAG_CMD, 1376, "unable to trim file name=0x%x\n", n);
				goto free_name;
			}
			mtpUtilMemset(name, 0, 1023);
			mtpUtilStrncpy(name, trimmed, 192);
			mtpUtilFree(trimmed);
		}
	}
	mtpUtilReplaceInvalidChars(name, mtpUtilStrlen(name));
	mtpUtilGetUnpackMtpStringSize(&string);
	offset = string.numChars * 2 + 54;
	string.chars = &buf[offset];
	string.numChars = 0xFF;
	if (mtpUtilUnpackMtpString(created, sizeof(created), &string) >= 0)
		dateCreated = created;
	mtpUtilGetUnpackMtpStringSize(&string);
	string.chars = &buf[string.numChars * 2 + 1 + offset];
	string.numChars = 0xFF;
	if (mtpUtilUnpackMtpString(modified, sizeof(modified), &string) >= 0)
		dateModified = modified;
	if (storageId == 0)
		storageId = 0x10001;
	if (!mtpStrgIsStorageValid(storageId)) {
		MTP_ERR(MTP_TAG_CMD, 1421, "storage is not valid\n");
		code = MTP_RESPONSE_INVALID_STORAGE_ID;
		goto free_name;
	}
	ret = mtpStrgGetLogicalStorage(storageId, &storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1427, "unable to get logical storage=0x%x\n", ret);
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		goto free_name;
	}
	if (size == 0xFFFFFFFF && !mtpStrgIsLargeObjectSupported(storageId)) {
		MTP_ERR(MTP_TAG_CMD, 1437, "object is larger than 4GiB, currently not supported\n");
		code = MTP_RESPONSE_VENDOR_OBJECT_TOO_LARGE;
		goto free_name;
	}
	if (parent != 0) {
		if (parent == 0xFFFFFFFF) {
			atRoot = 1;
			parent = MTP_OBJECT_ROOT_HANDLE(storageId);
		}
		if ((parent & MTP_OBJECT_HANDLE_DIR) == 0) {
			MTP_ERR(MTP_TAG_CMD, 1458, "invalid object handle not association\n");
			code = MTP_RESPONSE_INVALID_PARENT_OBJECT;
			goto free_name;
		}
		if (mtpStrgCheckDestination(storageId, format, &parent, context) < 0) {
			MTP_ERR(MTP_TAG_CMD, 1467, "Destination is not supported: %u, 0x%x, %u, %s\n",
				storageId, format, parent, name);
			code = MTP_RESPONSE_DESTINATION_UNSUPPORTED;
			goto free_name;
		}
		parentStorage = MTP_OBJECT_STORAGE_ID(parent);
		if (parent != MTP_OBJECT_ROOT_HANDLE(storageId))
			atRoot = 0;
	} else {
		parent = MTP_OBJECT_ROOT_HANDLE(storageId);
		atRoot = 1;
		parentStorage = MTP_OBJECT_STORAGE_ID(parent);
	}
	if (storageId != parentStorage) {
		MTP_ERR(MTP_TAG_CMD, 1483, "storage id and handle mismatched\n");
		code = MTP_RESPONSE_INVALID_PARENT_OBJECT;
		goto free_name;
	}
	if (mtpStrgGetAccessCapability(storageId) == MTP_ACCESS_READ_ONLY) {
		MTP_ERR(MTP_TAG_CMD, 1491, "storage is read only\n");
		code = MTP_RESPONSE_STORE_READ_ONLY;
		goto free_name;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1499, "unable to update logical info=0x%x\n", ret);
		goto free_name;
	}
	if (storage->freeSpaceInBytes < size || storage->freeSpaceInObjects == 0) {
		MTP_ERR(MTP_TAG_CMD, 1505, "store full: %llu, %u\n", storage->freeSpaceInBytes,
			storage->freeSpaceInObjects);
		code = MTP_RESPONSE_STORE_FULL;
		goto free_name;
	}
	if (mtpCoreGetObjectTransferIsFile()) {
		transfer = mtpCoreGetObjectTransferHandle();
		ret = mtpStrgVTConvertMtpHandleToUniqueId(transfer, &uniqueId);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 1516,
				"unable to convert MTP handle to uniqueId=0x%x %u\n", ret, transfer);
		} else {
			ret = mtpObjRemoveObject(storage, uniqueId);
			if (ret < 0)
				MTP_ERR(MTP_TAG_CMD, 1520, "unable to remove object=0x%x\n", ret);
		}
		mtpCoreClearObjectTransfer();
	}
	if (format == MTP_FORMAT_ASSOCIATION) {
		if (atRoot == 1 && !mtpStrgIsRootDirAllowed(storage, name)) {
			MTP_ERR(MTP_TAG_CMD, 1530, "access denied %s\n", name);
			code = MTP_RESPONSE_ACCESS_DENIED;
			goto free_name;
		}
	} else if (atRoot == 1 && mtpUtilStrcmp(name, "WMPInfo.xml") != 0 &&
		   mtpUtilStrcmp(name, "DevIcon.fil") != 0) {
		MTP_ERR(MTP_TAG_CMD, 1543, "access denied(%s)\n", name);
		code = MTP_RESPONSE_ACCESS_DENIED;
		goto free_name;
	}
	ret = mtpStrgVTConvertMtpHandleToUniqueId(parent, &uniqueId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1553, "unable to convert MTP handle to uniqueId=0x%x\n", ret);
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		goto free_name;
	}
	ret = mtpObjAddRegularObject(storage, uniqueId, format, size, imagePixWidth,
		imagePixHeight, name, dateCreated, dateModified, &handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1561, "unable to add object=0x%x\n", ret);
		if (ret == SCE_MTP_ERROR_OBJECT_EXISTS) {
			n = mtpObjRemoveFile(storage, uniqueId, name);
			if (n != 0)
				MTP_ERR(MTP_TAG_CMD, 1569, "mtpObjRemoveFile failed: 0x%x\n", n);
			else
				ret = mtpObjAddRegularObject(storage, uniqueId, format, size,
					imagePixWidth, imagePixHeight, name, dateCreated, dateModified,
					&handle);
		}
		if (ret == SCE_ERROR_ERRNO_ENOSPC)
			code = MTP_RESPONSE_STORE_FULL;
		if (ret < 0)
			goto free_name;
	}
	mtpCoreSetObjectTransferHandle(handle);
	mtpCoreSetObjectTransferSize(size);
	mtpCoreSetObjectImportHandle(handle);
	if (format == MTP_FORMAT_ASSOCIATION)
		mtpCoreSetObjectTransferIsFile(0);
	else
		mtpCoreSetObjectTransferIsFile(1);
	if (parent != operation->param[1]) {
		MTP_ERR(MTP_TAG_CMD, 1596, "WARNING unexpected parent: %u, %u\n", parent,
			operation->param[1]);
	}
	parent = operation->param[1];
	if (context != NULL)
		mtpContextPutContext(context, 1);
	params[0] = storageId;
	params[1] = atRoot != 1 ? parent : 0xFFFFFFFF;
	params[2] = mtpCoreGetObjectTransferHandle2() & 0x7FFFFFFF;
	params[3] = reserved;
	params[4] = reserved;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, params, 5, operation, response);
	mtpUtilFree(name);
	mtpUtilFree(buf);
	return ret;
free_name:
	mtpUtilFree(name);
free_buf:
	ret = -2;
	mtpUtilFree(buf);
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

}
