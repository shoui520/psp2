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

/* the buffer an operation's dataset is built in */
#define MTP_CMD_WORK_SIZE		0x2000
/* an object property dataset: the representative sample data's is larger */
#define MTP_OBJECT_PROP_DESC_SIZE	0x1000
#define MTP_OBJECT_PROP_VALUE_SIZE_LARGE	0x10000
/* the ObjectPropList group of the sample data (GetObjectPropList's group code) */
#define MTP_OBJECT_PROP_GROUP_SAMPLE_DATA	4

/* the largest file a host may send whose size it gives only in the data */
#define MTP_CMD_FILE_SIZE_MAX		0x1000000000LL

extern "C" {

/*
 * receives the data phase of an operation into path (created when create is set), size bytes
 * or, at 0xFFFFFFFF, as many as the data says; received counts the bytes, responseCode is set
 * when the operation must fail with a response of its own
 */
int mtpCmdUtilReceiveFileData(const MtpOperation *operation, MtpOperation *response,
	const char *path, SceSize size, int create, SceUInt64 *received, int *responseCode,
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
	if (storage->dataset.freeSpaceInBytes < size || storage->dataset.freeSpaceInObjects == 0) {
		MTP_ERR(MTP_TAG_CMD, 1505, "store full: %llu, %u\n", storage->dataset.freeSpaceInBytes,
			storage->dataset.freeSpaceInObjects);
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

int mtpCmdSendObject(const MtpOperation *operation, MtpOperation *response)
{
	int code = MTP_RESPONSE_GENERAL_ERROR;
	MtpLogicalStorage *storage = NULL;
	MtpContext *context = NULL;
	SceUInt64 received = 0;
	SceDateTime created;
	SceDateTime modified;
	SceUInt32 uniqueId;
	SceUInt32 handle;
	SceSize nameLength;
	char *path;
	char *pfsPath;
	int ret;
	int n;

	ret = mtpContextGetContextByEvent(MTP_OPERATION_REQUEST_SEND_OBJECT, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1659, "GetContextByEvent failed: 0x%x\n", ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (!mtpCoreGetObjectTransferIsFile() || mtpCoreGetObjectTransferHandle2() == 0) {
		MTP_ERR(MTP_TAG_CMD, 1667, "not valid object info\n");
		code = MTP_RESPONSE_NO_VALID_OBJECT_INFO;
		ret = -2;
		goto error;
	}
	n = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(mtpCoreGetObjectTransferHandle2()),
		&storage);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 1676, "unable to get logical storage=0x%x\n", n);
		ret = -2;
		goto error;
	}
	nameLength = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(nameLength * 2 + 2046);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_CMD, 1685, "unable to alloc memory for full path name\n");
		code = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto error;
	}
	pfsPath = &path[nameLength + 1023];
	mtpUtilStrcpy(path, storage->info->name);
	handle = mtpCoreGetObjectTransferHandle();
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 1698, "unable to convert MTP handle to uniqueID=0x%x\n", n);
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	n = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_PATH,
		&path[nameLength], 1023);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 1705, "unable to get parent handle from cache=0x%x\n", n);
		ret = -2;
		goto free;
	}
	n = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_DATE_CREATED,
		&created, sizeof(created));
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 1710, "unable to get created time from cache: 0x%x\n", n);
		ret = -2;
		goto free;
	}
	n = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_DATE_MODIFIED,
		&modified, sizeof(modified));
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 1715, "unable to get modified time from cache: 0x%x\n", n);
		ret = -2;
		goto free;
	}
	ret = mtpStrgCreatePfsPath(storage, &path[nameLength], pfsPath, nameLength + 1023, 1);
	if (ret < 0)
		goto free;
	ret = mtpCmdUtilReceiveFileData(operation, response, pfsPath,
		mtpCoreGetObjectTransferSize(), 0, &received, &code, context);
	if (ret < 0) {
		if (received > 0 && mtpCoreGetObjectTransferSize() == 0xFFFFFFFF) {
			n = mtpCacheSetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_SIZE,
				&received, sizeof(received));
			if (n < 0) {
				MTP_ERR(MTP_TAG_CMD, 1769,
					"unable to set object size from cache=0x%x\n", n);
			} else {
				n = mtpCacheWriteBackDirtyCache(storage->cache);
				if (n < 0)
					MTP_ERR(MTP_TAG_CMD, 1771,
						"unable to write back cache=0x%x\n", n);
			}
		}
		goto free;
	}
	mtpFileSetDates(pfsPath, NULL, &modified);
	if (mtpCoreGetObjectTransferSize() == 0xFFFFFFFF) {
		n = mtpCacheSetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_SIZE,
			&received, sizeof(received));
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMD, 1740, "unable to set parent handle from cache=0x%x\n",
				n);
			ret = -2;
			goto free;
		}
		ret = mtpCacheWriteBackDirtyCache(storage->cache);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 1745, "unable to write back cache=0x%x\n", ret);
			ret = -2;
			goto free;
		}
	}
	mtpUtilFree(path);
	mtpCoreClearObjectTransfer();
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return ret;
free:
	mtpUtilFree(path);
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetDevicePropValue(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 code = operation->param[0];
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	SceSize size = MTP_DEVICE_PROP_DESC_SIZE;
	int send = 1;
	void *buf;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 1861, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if ((SceUInt16)code == MTP_DEVICE_PROP_DEVICE_ICON)
		size = MTP_DEVICE_PROP_DESC_SIZE_LARGE;
	buf = mtpUtilMalloc(size);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 1875, "unable to alloc work buffer\n");
		ret = -2;
		goto error;
	}
	n = mtpPropPackDevicePropValue((SceUInt16)code, buf, size);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 1883, "unable to get device prop value=0x%x\n", n);
		responseCode = MTP_RESPONSE_PARAMETER_NOT_SUPPORTED;
		ret = -2;
	} else {
		ret = mtpCoreSendData(buf, n, n);
		if (ret >= 0) {
			mtpUtilFree(buf);
			return ret;
		}
		MTP_ERR(MTP_TAG_CMD, 1891, "unable to send data=0x%x\n", ret);
		send = 0;
	}
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, responseCode, send);
}

int mtpCmdSetDevicePropValue(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 code = operation->param[0];
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	MtpString string;
	SceUInt8 *buf;
	char *name;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 1926, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_DEVICE_PROP_DESC_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 1937, "unable to alloc work buffer\n");
		responseCode = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(buf, MTP_DEVICE_PROP_DESC_SIZE, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 1945, "unable to receive data=0x%x", ret);
		goto free;
	}
	if ((SceUInt16)code != MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME) {
		responseCode = MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED;
		ret = -2;
		goto free;
	}
	string.numChars = buf[0];
	string.chars = &buf[1];
	name = (char *)&buf[1] + string.numChars * 2;
	if (mtpUtilUnpackMtpString(name, 256, &string) < 0) {
		MTP_ERR(MTP_TAG_CMD, 1959, "unable to unpack synchronization partner\n");
		ret = -2;
		goto free;
	}
	mtpSpecSetFriendlyName(name);
	mtpUtilFree(buf);
	return ret;
free:
	mtpUtilFree(buf);
error:
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetPartialObject(const MtpOperation *operation, MtpOperation *response)
{
	return _getObject(operation, response, 1);
}

int mtpCmdGetObjectPropsSupported(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 format = operation->param[0];
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	MtpCodeArray array;
	SceUInt8 *buf;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 2031, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_OBJECT_PROP_DESC_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2040, "unable to alloc work buffer\n");
		ret = -2;
		goto error;
	}
	array.count = (MTP_OBJECT_PROP_DESC_SIZE - 4) / sizeof(SceUInt16);
	array.codes = (SceUInt16 *)(buf + 4);
	n = mtpPropPackObjectPropsSupported((SceUInt16)format, &array);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2050, "unable to pack object prop supported=0x%x\n", n);
		responseCode = MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT;
		ret = -2;
	} else {
		mtpUtilMemcpy(buf, &array.count, sizeof(array.count));
		ret = mtpCoreSendData(buf, n + 4, n + 4);
		if (ret >= 0) {
			mtpUtilFree(buf);
			return ret;
		}
		MTP_ERR(MTP_TAG_CMD, 2064, "unable to send data=0x%x\n", ret);
		send = 0;
	}
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, responseCode, send);
}

int mtpCmdGetObjectPropDesc(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 format = operation->param[1];
	SceUInt32 code = operation->param[0];
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	void *buf;
	int ret;
	int n;

	if (operation->numParams < 2) {
		MTP_ERR(MTP_TAG_CMD, 2100, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	buf = mtpUtilMalloc(MTP_OBJECT_PROP_DESC_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2109, "unable to alloc memory for device property\n");
		ret = -2;
		goto error;
	}
	n = mtpPropPackObjectPropDesc((SceUInt16)code, (SceUInt16)format, buf);
	if (n < 0) {
		if (n == SCE_MTP_ERROR_UNSUPPORTED_FORMAT) {
			MTP_ERR(MTP_TAG_CMD, 2118, "not supported format\n");
			responseCode = MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT;
		} else {
			MTP_ERR(MTP_TAG_CMD, 2121, "invalid object prop code\n");
			responseCode = MTP_RESPONSE_INVALID_OBJECT_PROP_CODE;
		}
		ret = -2;
	} else {
		ret = mtpCoreSendData(buf, n, n);
		if (ret >= 0) {
			mtpUtilFree(buf);
			return ret;
		}
		MTP_ERR(MTP_TAG_CMD, 2130, "unable to send data=0x%x\n", ret);
		send = 0;
	}
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, responseCode, send);
}

int mtpCmdGetObjectPropValue(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 code = operation->param[1];
	SceUInt32 handle = operation->param[0];
	MtpLogicalStorage *storage = NULL;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	SceSize size;
	SceUInt32 uniqueId;
	void *buf;
	int ret;
	int n;

	if (operation->numParams < 2) {
		MTP_ERR(MTP_TAG_CMD, 2172, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	size = MTP_OBJECT_PROP_DESC_SIZE;
	if ((SceUInt16)code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA)
		size = MTP_OBJECT_PROP_VALUE_SIZE_LARGE;
	if (!mtpStrgIsObjectHandleValid(handle)) {
		MTP_ERR(MTP_TAG_CMD, 2183, "invalid object handle\n");
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto error;
	}
	n = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2192, "unable to get logical storage=0x%x", n);
		responseCode = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto error;
	}
	buf = mtpUtilMalloc(size);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2201, "unable to alloc memory for device property\n");
		ret = -2;
		goto error;
	}
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2208, "unable to convert MTP handle to uniqueId=0x%x\n", n);
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	n = mtpObjPackObjectPropValue(storage, uniqueId, (SceUInt16)code, buf, size);
	if (n < 0) {
		if (n == SCE_MTP_ERROR_UNSUPPORTED_PROPERTY) {
			MTP_ERR(MTP_TAG_CMD, 2218, "not supported property\n");
			responseCode = MTP_RESPONSE_INVALID_OBJECT_PROP_CODE;
		} else if (n == SCE_MTP_ERROR_UNSUPPORTED_FORMAT) {
			MTP_ERR(MTP_TAG_CMD, 2221, "not supported format\n");
			responseCode = MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE;
		} else {
			MTP_ERR(MTP_TAG_CMD, 2224, "general error\n");
		}
		ret = -2;
		goto free;
	}
	ret = mtpCoreSendData(buf, n, n);
	if (ret >= 0) {
		mtpUtilFree(buf);
		return ret;
	}
	MTP_ERR(MTP_TAG_CMD, 2232, "unable to send data=0x%x\n", ret);
	send = 0;
free:
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, responseCode, send);
}

int mtpCmdSetObjectPropValue(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 code = operation->param[1];
	SceUInt32 handle = operation->param[0];
	MtpLogicalStorage *storage = NULL;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	const MtpPropDesc *desc;
	SceUInt32 length;
	SceUInt32 uniqueId;
	SceSize size;
	void *buf;
	int ret;
	int n;

	if (operation->numParams < 2) {
		MTP_ERR(MTP_TAG_CMD, 2273, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	size = MTP_OBJECT_PROP_DESC_SIZE;
	if ((SceUInt16)code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA)
		size = MTP_OBJECT_PROP_VALUE_SIZE_LARGE;
	buf = mtpUtilMalloc(size);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2285, "unable to alloc work buffer\n");
		responseCode = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(buf, size, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 2293, "unable to receive data=0x%x", ret);
		goto free;
	}
	if ((SceUInt16)code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA) {
		desc = mtpSpecGetObjectPropDesc(MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA);
		if (desc == NULL) {
			MTP_ERR(MTP_TAG_CMD, 2302, "unable to get object prop desc\n");
			responseCode = MTP_RESPONSE_INVALID_OBJECT_PROP_CODE;
			ret = -2;
			goto free;
		}
		mtpUtilMemcpy(&length, buf, sizeof(length));
		if (length > desc->form.maxLength) {
			MTP_ERR(MTP_TAG_CMD, 2309, "Representative Sample Data too large=%d\n",
				length);
			length -= desc->form.maxLength;
			do {
				ret = mtpCoreRecvData(buf, size, NULL);
				if (ret < 0) {
					MTP_ERR(MTP_TAG_CMD, 2315, "unable to receive data=0x%x",
						ret);
					goto free;
				}
				length -= ret;
			} while (length != 0);
			responseCode = MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE;
			ret = -2;
			goto free;
		}
	}
	if (!mtpStrgIsObjectHandleValid(handle)) {
		MTP_ERR(MTP_TAG_CMD, 2329, "invalid object handle\n");
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	n = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2338, "unable to get logical storage=0x%x", n);
		responseCode = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto free;
	}
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2346, "unable to convert MTP handle to uniqueId=0x%x\n", n);
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	n = mtpObjSetObjectPropValue(storage, uniqueId, (SceUInt16)code, buf, 1);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2354, "unable to set object property value=0x%x\n", n);
		if (n == SCE_MTP_ERROR_READ_ONLY_PROPERTY)
			responseCode = MTP_RESPONSE_ACCESS_DENIED;
		ret = -2;
		goto free;
	}
	mtpUtilFree(buf);
	return n;
free:
	mtpUtilFree(buf);
error:
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetObjectReferences(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 numParams = operation->numParams;
	SceUInt32 handle = operation->param[0];
	MtpLogicalStorage *storage = NULL;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	SceUInt32 uniqueId;
	void *buf;
	int ret;
	int n;

	if (numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 2399, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if (!mtpStrgIsObjectHandleValid(handle)) {
		MTP_ERR(MTP_TAG_CMD, 2407, "invalid object handle\n");
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto error;
	}
	n = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2415, "unable to get logical storage=0x%x", n);
		responseCode = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto error;
	}
	buf = mtpUtilMalloc(MTP_OBJECT_PROP_DESC_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2424, "unable to alloc memory for device property\n");
		ret = -2;
		goto error;
	}
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2431, "unable to convert MTP handle to uniqueId=0x%x\n", n);
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	n = mtpObjPackObjectReferences(storage, uniqueId, buf, MTP_OBJECT_PROP_DESC_SIZE);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2440, "unable to pack object references=0x%x", n);
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	ret = mtpCoreSendData(buf, n, n);
	if (ret >= 0) {
		mtpUtilFree(buf);
		return ret;
	}
	MTP_ERR(MTP_TAG_CMD, 2448, "unable to send data=0x%x\n", ret);
	send = 0;
free:
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, responseCode, send);
}

int mtpCmdSetObjectReferences(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 numParams = operation->numParams;
	SceUInt32 handle = operation->param[0];
	MtpLogicalStorage *storage = NULL;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	SceUInt32 count;
	SceUInt32 uniqueId;
	SceUInt8 *buf;
	int ret;
	int n;

	if (numParams == 0) {
		MTP_ERR(MTP_TAG_CMD, 2486, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if (!mtpStrgIsObjectHandleValid(handle)) {
		MTP_ERR(MTP_TAG_CMD, 2494, "invalid object handle\n");
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto error;
	}
	n = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2502, "unable to get logical storage=0x%x", n);
		responseCode = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto error;
	}
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_OBJECT_PROP_DESC_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2511, "unable to alloc work buffer\n");
		responseCode = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(&count, sizeof(count), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 2519, "unable to receive data=0x%x\n", ret);
		goto free;
	}
	if (count > (MTP_OBJECT_PROP_DESC_SIZE - 4) / sizeof(SceUInt32)) {
		MTP_ERR(MTP_TAG_CMD, 2524, "buffer size is not enough\n");
		ret = -2;
		goto free;
	}
	mtpUtilMemcpy(buf, &count, sizeof(count));
	ret = mtpCoreRecvData(buf + 4, count * 4, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 2536, "unable to receive data=0x%x", ret);
		goto free;
	}
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2542, "unable to convert MTP handle to uniqueId=0x%x\n", n);
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	n = mtpObjSetObjectReferences(storage, uniqueId, buf, count * 4 + 4);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2550, "unable to set object references=0x%x\n", n);
		ret = -2;
		goto free;
	}
	mtpUtilFree(buf);
	return n;
free:
	mtpUtilFree(buf);
error:
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetObjectPropList(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 group = 0;
	SceUInt32 handle = operation->param[0];
	SceUInt32 format = operation->param[1];
	SceUInt32 depth = 0;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	SceUInt32 code = operation->param[2];
	SceUInt32 numParams = operation->numParams;
	const MtpStorageInfo *physical = NULL;
	MtpLogicalStorage *storage = NULL;
	SceUInt32 count = 0;
	SceUInt32 size = 0;
	int send = 1;
	SceUInt32 uniqueId;
	SceUInt32 i;
	SceUInt32 j;
	void *buf;
	int ret;
	int n;

	if (numParams < 3) {
		MTP_ERR(MTP_TAG_CMD, 2600, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if (numParams >= 4)
		group = operation->param[3];
	if (numParams >= 5)
		depth = operation->param[4];
	if ((SceUInt16)format != 0 && !mtpSpecIsSupportedFormat(format)) {
		MTP_ERR(MTP_TAG_CMD, 2614, "not supported object format");
		responseCode = MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
		ret = -2;
		goto error;
	}
	if (handle != 0 && handle != 0xFFFFFFFF && !mtpStrgIsObjectHandleValid(handle)) {
		MTP_ERR(MTP_TAG_CMD, 2623, "invalid object handle\n");
		responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto error;
	}
	if (((SceUInt16)code | group) == 0) {
		MTP_ERR(MTP_TAG_CMD, 2632, "invalid parameter(prop & gp)\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if ((handle | depth) == 0) {
		MTP_ERR(MTP_TAG_CMD, 2640, "depth is not positive\n");
		responseCode = MTP_RESPONSE_PARAMETER_NOT_SUPPORTED;
		ret = -2;
		goto error;
	}
	if (handle == 0xFFFFFFFF)
		depth = 0xFFFFFFFF;
	if (handle == 0 || handle == 0xFFFFFFFF) {
		/* every root of every started storage */
		for (i = 0; i < mtpStrgGetNumPhysicalStorages(); i++) {
			mtpStrgGetPhysicalStorageByIndex(i, &physical);
			if (physical == NULL || physical->started == 0)
				continue;
			for (j = 0; j < physical->numLogicalStorages; j++) {
				storage = &physical->logicalStorages[j];
				n = mtpStrgVTConvertMtpHandleToUniqueId(
					MTP_OBJECT_ROOT_HANDLE(physical->index << 16 | storage->index),
					&uniqueId);
				if (n < 0) {
					MTP_ERR(MTP_TAG_CMD, 2666,
						"unable to convert MTP handle to uniqueId=0x%x\n", n);
					responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
					ret = -2;
					goto error;
				}
				n = mtpObjPackObjectPropList(storage, uniqueId, (SceUInt16)format,
					(SceUInt16)code, group, depth, NULL, &count, &size, NULL);
				if (n < 0) {
					MTP_ERR(MTP_TAG_CMD, 2674,
						"unable to get object prop list for (all)root=0x%x\n", n);
					ret = -2;
					goto error;
				}
			}
		}
	} else {
		if (mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage) < 0) {
			MTP_ERR(MTP_TAG_CMD, 2682, "unable to get logical storage\n");
			responseCode = MTP_RESPONSE_STORE_NOT_AVAILABLE;
			ret = -2;
			goto error;
		}
		n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMD, 2690, "unable to convert MTP handle to uniqueId=0x%x\n",
				n);
			responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
			ret = -2;
			goto error;
		}
		n = mtpObjPackObjectPropList(storage, uniqueId, (SceUInt16)format, (SceUInt16)code,
			group, depth, NULL, &count, &size, NULL);
		if (n < 0) {
			if (n != SCE_MTP_ERROR_NO_SUCH_OBJECT)
				MTP_ERR(MTP_TAG_CMD, 2700,
					"unable to get object prop list for mtpHdl %08x=0x%x\n",
					handle, n);
			ret = -2;
			goto error;
		}
	}
	size += 4;
	if ((SceUInt16)code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA ||
	    group == MTP_OBJECT_PROP_GROUP_SAMPLE_DATA)
		buf = mtpUtilMalloc(MTP_OBJECT_PROP_VALUE_SIZE_LARGE);
	else
		buf = mtpUtilMalloc(MTP_OBJECT_PROP_DESC_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2718, "unable to alloc memory for xfer\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(&count, sizeof(count), size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 2724, "unable to send data=0x%x\n", ret);
		send = 0;
		goto free;
	}
	if (count != 0) {
		if (handle != 0 && handle != 0xFFFFFFFF) {
			ret = mtpObjPackObjectPropList(storage, uniqueId, (SceUInt16)format,
				(SceUInt16)code, group, depth, buf, &count, &size, &send);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_CMD, 2770, "unable to get object prop list=0x%x\n",
					ret);
				if (send != 0)
					ret = -2;
				goto free;
			}
		} else {
			for (i = 0; i < mtpStrgGetNumPhysicalStorages(); i++) {
				mtpStrgGetPhysicalStorageByIndex(i, &physical);
				if (physical == NULL || physical->started == 0)
					continue;
				for (j = 0; j < physical->numLogicalStorages; j++) {
					storage = &physical->logicalStorages[j];
					n = mtpStrgVTConvertMtpHandleToUniqueId(
						MTP_OBJECT_ROOT_HANDLE(physical->index << 16 |
							storage->index), &uniqueId);
					if (n < 0) {
						MTP_ERR(MTP_TAG_CMD, 2751,
							"unable to convert MTP handle to uniqueId=0x%x\n",
							n);
						responseCode = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
						ret = -2;
						goto free;
					}
					ret = mtpObjPackObjectPropList(storage, uniqueId,
						(SceUInt16)format, (SceUInt16)code, group, depth, buf,
						&count, &size, &send);
					if (ret < 0) {
						MTP_ERR(MTP_TAG_CMD, 2759,
							"unable to get object prop list for (all)root=0x%x",
							ret);
						if (send != 0)
							ret = -2;
						goto free;
					}
				}
			}
		}
	}
	mtpUtilFree(buf);
	return ret;
free:
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, responseCode, send);
}

/* receives one element's value into buf: a string as an MtpString's count and characters */
static int _receiveObjectPropListElementValue(SceUInt16 dataType, SceUInt8 *buf, SceSize size)
{
	SceUInt32 length;
	SceUInt8 numChars;
	void *chars;
	int ret;

	switch (dataType) {
	case MTP_TYPE_INT8:
	case MTP_TYPE_UINT8:
		ret = mtpCoreRecvData(buf, 1, NULL);
		if (ret < 0)
			ret = SCE_MTP_ERROR_RECEIVE;
		break;
	case MTP_TYPE_INT16:
	case MTP_TYPE_UINT16:
		ret = mtpCoreRecvData(buf, 2, NULL);
		if (ret < 0)
			ret = SCE_MTP_ERROR_RECEIVE;
		break;
	case MTP_TYPE_INT32:
	case MTP_TYPE_UINT32:
		ret = mtpCoreRecvData(buf, 4, NULL);
		if (ret < 0)
			ret = SCE_MTP_ERROR_RECEIVE;
		break;
	case MTP_TYPE_INT64:
	case MTP_TYPE_UINT64:
		ret = mtpCoreRecvData(buf, 8, NULL);
		if (ret < 0)
			ret = SCE_MTP_ERROR_RECEIVE;
		break;
	case MTP_TYPE_INT128:
	case MTP_TYPE_UINT128:
		ret = mtpCoreRecvData(buf, 16, NULL);
		if (ret < 0)
			ret = SCE_MTP_ERROR_RECEIVE;
		break;
	case MTP_TYPE_STR:
		numChars = 0;
		ret = mtpCoreRecvData(&numChars, 1, NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 543, "unable to receive data=0x%x\n", ret);
			mtpUtilFree(NULL);
			break;
		}
		buf[0] = numChars;
		chars = mtpUtilMalloc(numChars * 2);
		if (chars == NULL) {
			MTP_ERR(MTP_TAG_CMD, 550, "unable to alloc memory for string\n");
			ret = SCE_MTP_ERROR_NO_MEMORY;
			break;
		}
		ret = mtpCoreRecvData(chars, numChars * 2, NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 555, "unable to receive data=0x%x\n", ret);
			mtpUtilFree(chars);
			break;
		}
		mtpUtilMemcpy(&buf[1], chars, numChars * 2);
		mtpUtilFree(chars);
		break;
	case MTP_TYPE_AUINT8:
		length = 0;
		ret = mtpCoreRecvData(&length, sizeof(length), NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 568, "unable to receive data=0x%x\n", ret);
			break;
		}
		if (size >= length) {
			ret = mtpCoreRecvData(buf, length, NULL);
			if (ret < 0)
				MTP_ERR(MTP_TAG_CMD, 573, "unable to receive AUINT data=0x%x\n", ret);
			break;
		}
		/* too long: the bytes are read and dropped */
		MTP_ERR(MTP_TAG_CMD, 577, "buffer too small (%d)\n", length);
		do {
			ret = mtpCoreRecvData(buf, size, NULL);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_CMD, 580, "unable to receive AUINT data=0x%x\n",
					ret);
				break;
			}
			length -= ret;
		} while (length != 0);
		break;
	default:
		MTP_ERR(MTP_TAG_CMD, 589, "unknown data type=%x\n", dataType);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		break;
	}
	return ret;
}

/* an ObjectPropList element's header: the value follows */
typedef struct MtpObjectPropListElement {
	SceUInt32 handle;
	SceUInt16 code;
	SceUInt16 dataType;
} MtpObjectPropListElement;

int mtpCmdSetObjectPropList(const MtpOperation *operation, MtpOperation *response)
{
	MtpLogicalStorage *storage = NULL;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	MtpObjectPropListElement element;
	SceUInt32 count;
	SceUInt32 uniqueId;
	SceUInt32 i;
	SceUInt8 *buf;
	int ret;
	int n;

	ret = mtpCoreRecvData(&count, sizeof(count), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 2810, "unable to receive data=0x%x", ret);
		goto error;
	}
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_OBJECT_PROP_VALUE_SIZE_LARGE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 2819, "unable to alloc work buffer\n");
		responseCode = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto error;
	}
	for (i = 0; i < count; i++) {
		ret = mtpCoreRecvData(&element, sizeof(element), NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 2828, "unable to receive data=0x%x", ret);
			goto free;
		}
		n = _receiveObjectPropListElementValue(element.dataType, buf,
			MTP_OBJECT_PROP_VALUE_SIZE_LARGE);
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMD, 2835,
				"unable to receive object prop list value=0x%x\n", n);
			ret = -2;
			goto free;
		}
		n = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(element.handle), &storage);
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMD, 2842, "unable to get logical storage=0x%x", n);
			continue;
		}
		n = mtpStrgVTConvertMtpHandleToUniqueId(element.handle, &uniqueId);
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMD, 2848, "unable to convert MTP handle to uniqueId=0x%x\n",
				n);
			continue;
		}
		n = mtpObjSetObjectPropValue(storage, uniqueId, element.code, buf, 0);
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMD, 2854, "unable to set object property value=0x%x\n", n);
			if (n == SCE_MTP_ERROR_READ_ONLY_PROPERTY)
				responseCode = MTP_RESPONSE_ACCESS_DENIED;
		}
	}
	n = mtpCacheWriteBackDirtyCache(storage->cache);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 2862, "unable to write back cache=0x%x\n", n);
		ret = -2;
		goto free;
	}
	mtpUtilFree(buf);
	return n;
free:
	mtpUtilFree(buf);
error:
	mtpCacheWriteBackDirtyCache(storage->cache);
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return ret;
}

}
