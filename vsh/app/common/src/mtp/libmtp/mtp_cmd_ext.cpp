/*
 * The vendor operations ("CMDE:"): the requests the API's contexts queue for the host, and
 * the commands the host runs to answer them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <sceerror.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "mtp_internal.h"

#define MTP_TAG_CMDE			"CMDE:"

/* the FS operation event of the hosts that speak application type 1 */
#define MTP_OPERATION_REQUEST_FS_OPERATION_TYPE1	0xC10A

/* a request's data as some requests hand it over: a buffer and its size */
typedef struct MtpContextBuffer {
	void *data;
	SceSize size;
} MtpContextBuffer;

/* SearchObject's data: the query, then the host's answer */
typedef struct MtpSearchData {
	void *data;
	SceSize size;
	SceUInt32 numFound;
	SceUInt32 searchId;
} MtpSearchData;

/* the search ID SendNumOfObject gives a search's count */
#define MTP_SEARCH_ID_NUM_OF_OBJECT	19

/* what GetHostStorageSize and Handover hand over */
#define MTP_HOST_STORAGE_SIZE_DATA_SIZE	28
#define MTP_HANDOVER_INFO_SIZE		16

/* what GetObjectMetadata asks for (the browsing request) */
typedef struct MtpBrowsingRequest {
	SceUInt32 type;
	SceUInt32 reserved;
	SceUInt32 param[3];
} MtpBrowsingRequest;

/* what GetObjectStatus asks for: a path and what to check */
typedef struct MtpObjectStatusRequest {
	char path[1024];
	SceUInt32 length;
	SceUInt32 type;
} MtpObjectStatusRequest;

/* a part of an object a request reads or writes */
typedef struct __attribute__((packed)) MtpPartOfObjectRequest {
	SceUInt32 handle;
	SceUInt64 offset;
	SceUInt64 size;
} MtpPartOfObjectRequest;

/* GetPartOfObjectSpecfiedByHost's first packet: the data size, then the file's first bytes */
typedef struct MtpPartOfObjectHeader {
	SceUInt8 total[8];
	SceUInt8 data[12];
} MtpPartOfObjectHeader;

/* CheckObjectExistence's data phase, received and dropped */
#define MTP_CHECK_OBJECT_EXISTENCE_DATA_SIZE	0x2000

/* SendCopyConfirmationInfo's data phase: the object, then the items that follow */
typedef struct __attribute__((packed)) MtpCopyConfirmationAnswer {
	SceUInt64 object;
	SceUInt32 numItems;
} MtpCopyConfirmationAnswer;

/* a copy confirmation's context data (20 bytes) */
typedef struct __attribute__((packed)) MtpCopyConfirmationInfo {
	SceUInt32 size;			/* the bytes GetCopyConfirmationInfoRequest sends */
	SceUInt32 header;		/* its first word */
	SceUInt64 object;		/* the object the host names */
	SceUInt32 numItems;
} MtpCopyConfirmationInfo;

/* GetTotalObjectSize's request and the host's answer (20 bytes) */
#define MTP_TOTAL_OBJECT_SIZE_DATA_SIZE	20
typedef struct MtpTotalObjectSize {
	SceUInt32 size;			/* the bytes the handles take */
	SceUInt32 numHandles;
	SceUInt64 totalSize;		/* the host's */
	SceUInt32 numObjects;		/* the host's */
} MtpTotalObjectSize;

/* the results the callers of GetTotalObjectSize and SearchObject wait for */
typedef struct MtpTotalObjectSizeResult {
	SceUInt32 *numObjects;
	SceUInt64 *totalSize;
} MtpTotalObjectSizeResult;

typedef struct MtpSearchResult {
	SceUInt32 *numFound;
	SceUInt32 *searchId;
} MtpSearchResult;

static void getResultParam(MtpContext *context, void *result);
static void getNumOfObjectResult(MtpContext *context, void *result);
static void getHostStorageSizeResult(MtpContext *context, void *result);
static void getTotalObjectSizeResult(MtpContext *context, void *result);
static void deleteQueryObjectMetadataInfo(void *data, SceSize size);
static void getSearchObjectResult(MtpContext *context, void *result);
static void deleteSearchNum(void *data, SceSize size);

/* the export the host is told to resume, or 0 */
static SceUInt32 s_resumeContextId;

extern "C" {

int mtpContextGetNumOfObject(MtpContext *context, SceUInt32 type, SceUInt32 param)
{
	MtpEvent event;
	SceUInt32 *num;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	num = (SceUInt32 *)mtpUtilMalloc(sizeof(*num));
	if (num == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 425, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	event.code = MTP_OPERATION_REQUEST_SEND_NUM_OF_OBJECT;
	event.param[2] = param;
	event.numParams = 3;
	event.param[0] = context->id;
	event.param[1] = type;
	context->data = num;
	context->dataSize = sizeof(*num);
	context->onResult = getNumOfObjectResult;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 441, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(num);
		return ret;
	}
	return 0;
}

int mtpContextImportBrowsingData(MtpContext *context, SceUInt32 type, SceUInt32 param1,
	SceUInt32 param2, SceUInt32 param3)
{
	MtpEvent event;
	MtpBrowsingRequest *request;
	MtpContextBuffer *buffer;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	request = (MtpBrowsingRequest *)mtpUtilMalloc(sizeof(*request));
	buffer = (MtpContextBuffer *)mtpUtilMalloc(sizeof(*buffer));
	if (request == NULL || buffer == NULL) {
		if (request != NULL)
			mtpUtilFree(request);
		if (buffer != NULL)
			mtpUtilFree(buffer);
		MTP_ERR(MTP_TAG_CMDE, 467, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	event.code = MTP_OPERATION_REQUEST_SEND_OBJECT_METADATA;
	event.numParams = 1;
	event.param[0] = context->id;
	request->type = type;
	request->reserved = 0;
	request->param[0] = param1;
	request->param[1] = param2;
	request->param[2] = param3;
	buffer->data = request;
	buffer->size = sizeof(*request);
	context->data = buffer;
	context->dataSize = sizeof(*buffer);
	context->freeData = mtpContextDeleteBrowsingContext;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 489, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(request);
		mtpUtilFree(buffer);
		return ret;
	}
	return 0;
}

void mtpContextDeleteBrowsingContext(void *data, SceSize size)
{
	if (data == NULL || size != sizeof(MtpContextBuffer)) {
		MTP_ERR(MTP_TAG_CMDE, 503, "Wrong data\n");
		return;
	}
	mtpUtilFree(((MtpContextBuffer *)data)->data);
	mtpUtilFree(data);
}

int mtpContextImportObjectData(MtpContext *context, SceUInt32 source, const char *path,
	const char *name, int import)
{
	MtpEvent event;
	SceUInt32 *handle;
	SceUInt32 target;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	handle = (SceUInt32 *)mtpUtilMalloc(sizeof(*handle));
	if (handle == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 522, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(handle, 0, sizeof(*handle));
	ret = mtpStrgGetMtpHandleByPath(path, &target);
	if (ret == SCE_MTP_ERROR_NOT_FOUND)
		ret = mtpStrgRegisterDirectory(path, &target);
	if (ret < 0) {
		mtpUtilFree(handle);
		return ret;
	}
	event.numParams = 3;
	event.param[0] = context->id;
	event.param[1] = source;
	event.param[2] = target;
	event.code = MTP_OPERATION_REQUEST_SEND_OBJECT;
	*handle = target;
	context->data = handle;
	context->dataSize = sizeof(*handle);
	context->onResult = getResultParam;
	context->opcode = MTP_OPERATION_REQUEST_SEND_OBJECT;
	context->pauseData[0] = (void *)source;
	context->pauseData[1] = (void *)target;
	if (name != NULL)
		mtpUtilStrncpy((char *)context->work, name, sizeof(context->work));
	if (import != 0) {
		mtpCoreSetObjectTransferImporting(1);
		mtpCoreSetObjectTransferPath(path);
		mtpCoreSetImportRunning(1);
		mtpCoreSetImportContextId(context->id);
	}
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 562, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpCoreSetObjectTransferImporting(0);
		mtpCoreSetObjectTransferPath("");
		mtpCoreSetImportRunning(0);
		mtpCoreSetImportContextId(0);
		mtpUtilFree(handle);
		return ret;
	}
	return 0;
}

int mtpContextSetResumeContextId(SceUInt32 contextId)
{
	s_resumeContextId = contextId;
	return 0;
}

int mtpContextExportObjectWithCheck(MtpContext *context, SceUInt64 object, SceUInt64 target)
{
	SceUInt32 uniqueId = (SceUInt32)object;
	SceUInt32 storageId = MTP_OBJECT_STORAGE_ID((SceUInt32)(object >> 32));
	MtpEvent event;
	SceUInt32 *request;
	SceUInt32 handle;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, &handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 595, "convert mtp handle failed: 0x%x, uid=%x\n", ret,
			uniqueId);
		return ret;
	}
	/* the target, then the handle */
	request = (SceUInt32 *)mtpUtilMalloc(12);
	if (request == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 601, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(request, 0, 12);
	event.param[0] = context->id;
	event.numParams = 1;
	event.code = MTP_OPERATION_REQUEST_EXPORT_OBJECT;
	if (s_resumeContextId != 0) {
		event.numParams = 2;
		MTP_WARN(MTP_TAG_CMDE, 613, "++++++++++++++++++ resume_flag+++++++++++ %d\n",
			s_resumeContextId);
		event.param[1] = s_resumeContextId;
	}
	request[0] = (SceUInt32)target;
	request[1] = (SceUInt32)(target >> 32);
	request[2] = handle;
	context->data = request;
	context->dataSize = 12;
	context->opcode = MTP_OPERATION_REQUEST_OBJECT_INFO;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 628, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(request);
		return ret;
	}
	return 0;
}

int mtpContextRequestFsOperation(MtpContext *context, int operation, SceUInt32 param1,
	SceUInt32 param2, const char *path, SceSize size)
{
	MtpEvent event;
	SceUInt32 numFields;
	SceUInt32 offset;
	SceUInt32 total;
	SceUInt8 *buf;
	int code;
	int ret;
	int n;

	mtpUtilMemset(&event, 0, sizeof(event));
	ret = mtpStatusIsApplicationType1();
	total = size + 16;
	n = 3;
	code = MTP_OPERATION_REQUEST_FS_OPERATION;
	numFields = 4;
	if (ret) {
		code = MTP_OPERATION_REQUEST_FS_OPERATION_TYPE1;
		total = size + 12;
		n = 2;
		numFields = 3;
	}
	event.code = code;
	event.numParams = 1;
	event.param[0] = context->id;
	buf = (SceUInt8 *)mtpUtilMalloc(total);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 664, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemcpy(buf, &operation, 4);
	mtpUtilMemcpy(buf + 4, &param1, 4);
	offset = n * 4;
	if (numFields == 4)
		mtpUtilMemcpy(buf + 8, &param2, 4);
	mtpUtilMemcpy(&buf[offset], &size, 4);
	if (path != NULL)
		mtpUtilMemcpy(&buf[offset + 4], path, size);
	context->data = buf;
	context->dataSize = total;
	context->onResult = getResultParam;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 683, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(buf);
		return ret;
	}
	return 0;
}

int mtpContextGetObjectStatus(MtpContext *context, SceUInt32 type, const char *path,
	SceSize length)
{
	MtpEvent event;
	MtpObjectStatusRequest *request;
	MtpContextBuffer *buffer;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	request = (MtpObjectStatusRequest *)mtpUtilMalloc(sizeof(*request));
	buffer = (MtpContextBuffer *)mtpUtilMalloc(sizeof(*buffer));
	if (request == NULL || buffer == NULL) {
		if (request != NULL)
			mtpUtilFree(request);
		if (buffer != NULL)
			mtpUtilFree(buffer);
		MTP_ERR(MTP_TAG_CMDE, 710, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	event.code = MTP_OPERATION_REQUEST_OBJECT_STATUS;
	event.numParams = 1;
	event.param[0] = context->id;
	if (length > sizeof(request->path) - 1)
		length = sizeof(request->path) - 1;
	mtpUtilStrncpy(request->path, path, length);
	request->length = length;
	request->type = type;
	buffer->data = request;
	buffer->size = sizeof(*request);
	context->data = buffer;
	context->dataSize = sizeof(*buffer);
	context->freeData = mtpContextDeleteBrowsingContext;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 734, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(buffer);
		mtpUtilFree(request);
		return ret;
	}
	return 0;
}

int mtpContextImportObjectThumbnail(MtpContext *context, SceUInt32 handle, SceUInt32 type)
{
	MtpEvent event;
	SceUInt32 *thumbnail;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	/* the handle, then what the host sends back: its size and data */
	thumbnail = (SceUInt32 *)mtpUtilMalloc(12);
	if (thumbnail == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 753, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(thumbnail, 0, 12);
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_SEND_OBJECT_THUMBNAIL;
	event.param[2] = type;
	event.numParams = 3;
	event.param[1] = handle;
	thumbnail[0] = handle;
	thumbnail[1] = 0;
	thumbnail[2] = 0;
	context->data = thumbnail;
	context->dataSize = 12;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 773, "mtpTaskQueuePush failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

int mtpContextDeleteObject(MtpContext *context, SceUInt32 handle)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_DELETE_OBJECT;
	event.numParams = 2;
	event.param[1] = handle;
	context->data = NULL;
	context->dataSize = 0;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 797, "mtpTaskQueuePush failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

int mtpContextGetPartOfObject(MtpContext *context, SceUInt32 handle, SceUInt64 offset,
	SceUInt64 size, int import)
{
	MtpEvent event;
	MtpPartOfObjectRequest *request;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	request = (MtpPartOfObjectRequest *)mtpUtilMalloc(sizeof(*request));
	if (request == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 814, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(request, 0, sizeof(*request));
	event.numParams = 1;
	event.code = MTP_OPERATION_REQUEST_PART_OF_OBJECT;
	event.param[0] = context->id;
	request->handle = handle;
	request->offset = offset;
	request->size = size;
	context->data = request;
	context->dataSize = sizeof(*request);
	context->opcode = MTP_OPERATION_REQUEST_PART_OF_OBJECT;
	if (import != 0) {
		mtpCoreSetImportRunning(1);
		mtpCoreSetImportContextId(context->id);
		context->objectHandle = mtpCoreGetObjectImportHandle();
	}
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 840, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(request);
		mtpCoreSetImportRunning(0);
		mtpCoreSetImportContextId(0);
		return ret;
	}
	return 0;
}

int mtpContextSendPartOfObject(MtpContext *context, SceUInt32 handle, SceUInt64 offset,
	SceUInt64 size)
{
	MtpEvent event;
	MtpPartOfObjectRequest *request;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	request = (MtpPartOfObjectRequest *)mtpUtilMalloc(sizeof(*request));
	if (request == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 860, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(request, 0, sizeof(*request));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_SEND_PART_OF_OBJECT;
	event.numParams = 1;
	request->handle = handle;
	request->offset = offset;
	request->size = size;
	context->data = request;
	context->dataSize = sizeof(*request);
	context->opcode = MTP_OPERATION_REQUEST_SEND_PART_OF_OBJECT;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 879, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(request);
		return ret;
	}
	return 0;
}

int mtpContextGetHostStorageSize(MtpContext *context, SceUInt32 storageId)
{
	MtpEvent event;
	void *size;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	size = mtpUtilMalloc(MTP_HOST_STORAGE_SIZE_DATA_SIZE);
	if (size == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 897, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(size, 0, MTP_HOST_STORAGE_SIZE_DATA_SIZE);
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_STORAGE_SIZE;
	event.numParams = 2;
	event.param[1] = storageId;
	context->data = size;
	context->dataSize = MTP_HOST_STORAGE_SIZE_DATA_SIZE;
	context->onResult = getHostStorageSizeResult;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 913, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(size);
		return ret;
	}
	return 0;
}

int mtpContextCompareObjectData(MtpContext *context, SceUInt64 object)
{
	SceUInt32 uniqueId = (SceUInt32)object;
	SceUInt32 storageId = MTP_OBJECT_STORAGE_ID((SceUInt32)(object >> 32));
	MtpEvent event;
	SceUInt32 handle;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, &handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 935, "convert mtp handle failed: 0x%x, uid=%x\n", ret,
			uniqueId);
		return ret;
	}
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_COPY_CONFIRMATION_INFO;
	event.numParams = 2;
	event.param[1] = handle;
	context->data = NULL;
	context->dataSize = 0;
	context->onResult = getResultParam;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 951, "mtpTaskQueuePush failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

int mtpContextGetTotalObjectSize(MtpContext *context, SceUInt32 numHandles)
{
	MtpEvent event;
	MtpTotalObjectSize *total;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	total = (MtpTotalObjectSize *)mtpUtilMalloc(MTP_TOTAL_OBJECT_SIZE_DATA_SIZE);
	if (total == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 968, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(total, 0, MTP_TOTAL_OBJECT_SIZE_DATA_SIZE);
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_OBJECT_METADATA_ITEMS;
	event.numParams = 1;
	total->numHandles = numHandles;
	total->size = numHandles * 4 + 4;
	context->data = total;
	context->dataSize = MTP_TOTAL_OBJECT_SIZE_DATA_SIZE;
	context->onResult = getTotalObjectSizeResult;
	context->opcode = MTP_OPERATION_REQUEST_OBJECT_METADATA_ITEMS;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 987, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(total);
		return ret;
	}
	return 0;
}

int mtpContextGetSpecifiedObjectMetadata(MtpContext *context, SceUInt32 type,
	const SceUInt32 *items, SceUInt32 numItems)
{
	MtpEvent event;
	MtpContextBuffer *buffer;
	SceUInt8 *query;
	SceSize size;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	buffer = (MtpContextBuffer *)mtpUtilMalloc(sizeof(*buffer));
	if (buffer == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1006, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(buffer, 0, sizeof(*buffer));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_SPECIFIED_OBJECT_METADATA;
	event.numParams = 1;
	size = numItems * 4 + 8;
	buffer->size = size;
	query = (SceUInt8 *)mtpUtilMalloc(size);
	if (query == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1018, "no memory\n");
		mtpUtilFree(buffer);
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	buffer->data = query;
	mtpUtilMemcpy(query, &type, 4);
	mtpUtilMemcpy(query + 4, &numItems, 4);
	mtpUtilMemcpy(query + 8, items, numItems * 4);
	context->data = buffer;
	context->dataSize = sizeof(*buffer);
	context->freeData = deleteQueryObjectMetadataInfo;
	context->opcode = MTP_OPERATION_REQUEST_SPECIFIED_OBJECT_METADATA;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1034, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(buffer->data);
		mtpUtilFree(buffer);
		return ret;
	}
	return 0;
}

/* 0 when the context imports the object of this handle */
int mtpContextCheckImportHandle(SceUInt32 handle, const MtpContext *context)
{
	if (context == NULL || context->dataSize != sizeof(SceUInt32))
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (handle == *(const SceUInt32 *)context->data)
		return 0;
	return SCE_MTP_ERROR_INVALID_ARGUMENT;
}

int mtpContextSearchObject(MtpContext *context, const char *keyword, SceUInt32 type,
	SceUInt32 flags)
{
	MtpEvent event;
	MtpSearchData *search;
	SceSize length = 0;
	SceSize size;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	search = (MtpSearchData *)mtpUtilMalloc(sizeof(*search));
	if (search == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1068, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(search, 0, sizeof(*search));
	length = mtpUtilStrlen(keyword) + 1;
	size = length + 12;
	search->size = size;
	search->data = mtpUtilMalloc(size);
	if (search->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1077, "no memory\n");
		sce_paf_free(search);
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_SEARCH_OBJECT;
	event.numParams = 1;
	mtpUtilMemcpy(search->data, &type, 4);
	mtpUtilMemcpy((SceUInt8 *)search->data + 4, &flags, 4);
	mtpUtilMemcpy((SceUInt8 *)search->data + 8, &length, 4);
	mtpUtilMemcpy((SceUInt8 *)search->data + 12, keyword, length);
	context->data = search;
	context->dataSize = sizeof(*search);
	context->opcode = MTP_OPERATION_REQUEST_SEARCH_OBJECT;
	context->onResult = getSearchObjectResult;
	context->freeData = deleteSearchNum;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1099, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(search->data);
		mtpUtilFree(search);
		return ret;
	}
	return 0;
}

int mtpContextHandover(MtpContext *context, const void *info)
{
	MtpEvent event;
	void *data;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	data = mtpUtilMalloc(MTP_HANDOVER_INFO_SIZE);
	if (data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1118, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemcpy(data, info, MTP_HANDOVER_INFO_SIZE);
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_HANDOVER;
	event.numParams = 1;
	context->data = data;
	context->dataSize = MTP_HANDOVER_INFO_SIZE;
	context->onResult = NULL;
	context->opcode = MTP_OPERATION_REQUEST_HANDOVER;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1134, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(data);
		return ret;
	}
	return 0;
}

int mtpContextPauseExportObject(MtpContext *context, SceUInt32 contextId)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_PAUSE_EXPORT_OBJECT;
	event.numParams = 2;
	event.param[1] = contextId;
	context->data = NULL;
	context->dataSize = 0;
	context->opcode = MTP_OPERATION_PAUSE_EXPORT_OBJECT;
	mtpContextSetPauseContextData(contextId);
	ret = mtpCoreSendResponse(&event, contextId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1162, "mtpCoreSendEvent failed: 0x%x\n", ret);
		return ret;
	}
	ret = mtpTaskQueueCancelAllForDoCancelAndPause();
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1168,
			"mtpTaskQueueCancelAllForDoCancelAndPause failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

int mtpContextResumeExportObject(MtpContext *context)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_RESUME_EXPORT_OBJECT;
	event.numParams = 2;
	event.param[1] = mtpContextGetPauseContextId();
	context->data = NULL;
	context->dataSize = 0;
	context->opcode = MTP_OPERATION_REQUEST_RESUME_EXPORT_OBJECT;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1193, "mtpTaskQueuePush failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

int mtpContextResumeImportObject(MtpContext *context)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_RESUME_IMPORT_OBJECT;
	event.numParams = 3;
	event.param[1] = (SceUInt32)mtpExportGetPauseSource();
	event.param[2] = (SceUInt32)mtpExportGetPauseTarget();
	context->data = NULL;
	context->dataSize = 0;
	context->onResult = NULL;
	context->opcode = MTP_OPERATION_REQUEST_RESUME_IMPORT_OBJECT;
	context->objectHandle = mtpCoreGetObjectImportHandle();
	mtpCoreSetImportRunning(1);
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1229, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpCoreSetImportRunning(0);
		return ret;
	}
	return 0;
}

int mtpCmdSendNumOfObject(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 num = operation->param[1];
	int code = MTP_RESPONSE_GENERAL_ERROR;
	MtpContext *context = NULL;
	MtpSearchData *search;
	SceUInt32 param;
	int ret;

	if (operation->numParams < 2) {
		MTP_ERR(MTP_TAG_CMDE, 1254, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1262, "GetContext %u failed: 0x%x\n", contextId, ret);
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1268, "pContext->pData is NULL\n");
		goto error;
	}
	if (context->dataSize == sizeof(SceUInt32)) {
		*(SceUInt32 *)context->data = num;
	} else if (context->dataSize == sizeof(MtpSearchData)) {
		search = (MtpSearchData *)context->data;
		search->numFound = num;
		((MtpSearchData *)context->data)->searchId = MTP_SEARCH_ID_NUM_OF_OBJECT;
	} else {
		MTP_ERR(MTP_TAG_CMDE, 1281, "Invalid data size\n");
		goto error;
	}
	mtpContextPutContext(context, 1);
	param = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &param, 1, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return -2;
}

int mtpCmdSendNumOfFoundObject(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 num = operation->param[1];
	SceUInt32 searchId = operation->param[2];
	int code = MTP_RESPONSE_GENERAL_ERROR;
	MtpContext *context = NULL;
	SceUInt32 param;
	int ret;

	if (operation->numParams < 3) {
		MTP_ERR(MTP_TAG_CMDE, 1320, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1328, "GetContext %u failed: 0x%x\n", contextId, ret);
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1334, "pContext->pData is NULL\n");
		goto error;
	}
	if (context->dataSize != sizeof(MtpSearchData)) {
		MTP_ERR(MTP_TAG_CMDE, 1344, "Invalid data size\n");
		goto error;
	}
	((MtpSearchData *)context->data)->numFound = num;
	((MtpSearchData *)context->data)->searchId = searchId;
	mtpContextPutContext(context, 1);
	param = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &param, 1, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return -2;
}

int mtpCmdGetBrowseInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	MtpContextBuffer *buffer;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 1383, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1391, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpContextBuffer)) {
		MTP_ERR(MTP_TAG_CMDE, 1398, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	buffer = (MtpContextBuffer *)context->data;
	if (buffer == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1405, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	if (buffer->size != sizeof(MtpBrowsingRequest)) {
		MTP_ERR(MTP_TAG_CMDE, 1411, "Invalid data size\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(buffer->data, sizeof(MtpBrowsingRequest),
		sizeof(MtpBrowsingRequest));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1420, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdSendObjectMetadata(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	SceUInt32 code;
	SceUInt32 param;
	int ret;
	int n;

	if (operation->numParams == 0) {
		code = MTP_RESPONSE_INVALID_PARAMETER;
		MTP_ERR(MTP_TAG_CMDE, 1456, "wrong param\n");
		ret = -2;
		goto error;
	}
	code = MTP_RESPONSE_GENERAL_ERROR;
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1463, "GetContext %u failed: 0x%x\n", contextId, n);
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpContextBuffer)) {
		MTP_ERR(MTP_TAG_CMDE, 1482, "Invalid data size\n");
		ret = -2;
		goto error;
	}
	if (context->sendRecv != NULL) {
		ret = mtpContextPassRecvData(context, 0, &code, 1);
	} else {
		MTP_ERR(MTP_TAG_CMDE, 1490, "RecvData is not set\n");
		ret = -2;
	}
	if (ret != 0) {
		MTP_ERR(MTP_TAG_CMDE, 1494, "Failed for context %u\n", context->id);
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	param = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &param, 1, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetObjectStatusRequest(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	MtpContextBuffer *buffer;
	MtpObjectStatusRequest *request;
	SceUInt32 total;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 1532, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1540, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpContextBuffer)) {
		MTP_ERR(MTP_TAG_CMDE, 1547, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	buffer = (MtpContextBuffer *)context->data;
	if (buffer == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1554, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	if (buffer->size != sizeof(MtpObjectStatusRequest)) {
		MTP_ERR(MTP_TAG_CMDE, 1560, "Invalid data size\n");
		ret = -2;
		goto error;
	}
	request = (MtpObjectStatusRequest *)buffer->data;
	total = request->length + sizeof(request->type) + sizeof(request->length);
	ret = mtpCoreSendData(&request->type, sizeof(request->type), total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1570, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	ret = mtpCoreSendData(&request->length, sizeof(request->length), total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1577, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	ret = mtpCoreSendData(request->path, request->length, total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1584, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdSendObjectThumbnail(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	SceUInt32 length = 0;
	MtpContext *context = NULL;
	SceUInt64 size = 0;
	SceUInt32 param;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 1621, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1629, "GetContext %u failed: 0x%x\n", contextId, n);
		ret = -2;
		goto error;
	}
	if (context->dataSize != 12 && context->dataSize != sizeof(MtpContextBuffer)) {
		MTP_ERR(MTP_TAG_CMDE, 1636, "Invalid data size\n");
		ret = -2;
		goto error;
	}
	if (context->sendRecv == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1644, "RecvData is not set\n");
		ret = -2;
		goto error;
	}
	/* the thumbnail's size, then its data with the size of the data phase first */
	ret = mtpCoreRecvData(&length, sizeof(length), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1651, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	ret = mtpContextPassRecvData(context, length, &code, 0);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_CMDE, 1658, "mtpContextPassRecvData failed: 0x%x\n", ret);
		goto error;
	}
	ret = mtpCoreRecvData(&size, sizeof(size), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1664, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	ret = mtpContextPassRecvData(context, size, &code, 1);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_CMDE, 1672, "Failed for context %u: 0x%x\n", context->id, ret);
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	param = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &param, 1, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetFsOperation(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 1708, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1716, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1723, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(context->data, context->dataSize, context->dataSize);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1730, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdGetPartOfObjectRequest(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 1765, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1773, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpPartOfObjectRequest) ||
	    context->opcode != MTP_OPERATION_REQUEST_PART_OF_OBJECT) {
		MTP_ERR(MTP_TAG_CMDE, 1780, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1787, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(context->data, sizeof(MtpPartOfObjectRequest),
		sizeof(MtpPartOfObjectRequest));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1794, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

/* receives size bytes of the import's object into its file; dates from the cache with setDates */
static int receiveFileData(MtpContext *context, int isFile, SceInt64 size, SceUInt32 *code,
	int setDates)
{
	MtpLogicalStorage *storage = NULL;
	SceSize bufSize = MTP_CMD_XFER_SIZE;
	SceDateTime created;
	SceDateTime modified;
	void *buffers[2];
	SceUInt32 uniqueId;
	SceUInt32 handle;
	SceSize nameLength;
	SceSize xferSize;
	SceInt64 remaining;
	SceInt64 total;
	char *pfsPath;
	char *path;
	int lastPercent;
	int current;
	int percent;
	int count;
	SceUID fd;
	int ret;
	int n;

	handle = mtpCoreGetObjectImportHandle();
	buffers[0] = mtpUtilMallocAlign(MTP_CMD_XFER_ALIGN, MTP_CMD_XFER_SIZE);
	buffers[1] = mtpUtilMallocAlign(MTP_CMD_XFER_ALIGN, MTP_CMD_XFER_SIZE);
	if (buffers[0] == NULL || buffers[1] == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 248, "unable to alloc memory for xfer buffers\n");
		pfsPath = NULL;
		*code = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto free;
	}
	n = mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 258, "unable to get logical storage=0x%x\n", n);
		pfsPath = NULL;
		ret = -2;
		goto free;
	}
	nameLength = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(nameLength * 2 + 2046);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 265, "unable to alloc memory for full path name\n");
		pfsPath = NULL;
		*code = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto free;
	}
	mtpUtilStrcpy(path, storage->info->name);
	pfsPath = &path[nameLength + 1023];
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 273, "unable to convert MTP handle to uniqueID=0x%x\n", n);
		*code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free_path;
	}
	n = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_PATH,
		&path[nameLength], 1023);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 280, "unable to get parent handle from cache=0x%x\n", n);
		ret = -2;
		goto free_path;
	}
	if (setDates != 0) {
		n = mtpCacheGetObjectProperty(storage->cache, uniqueId,
			MTP_CACHE_PROP_DATE_CREATED, &created, sizeof(created));
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMDE, 288,
				"unable to get created time from cache: 0x%x\n", n);
			ret = -2;
			goto free_path;
		}
		n = mtpCacheGetObjectProperty(storage->cache, uniqueId,
			MTP_CACHE_PROP_DATE_MODIFIED, &modified, sizeof(modified));
		if (n < 0) {
			MTP_ERR(MTP_TAG_CMDE, 294,
				"unable to get modified time from cache: 0x%x\n", n);
			ret = -2;
			goto free_path;
		}
	}
	ret = mtpStrgCreatePfsPath(storage, &path[nameLength], pfsPath, nameLength + 1023, 1);
	if (ret < 0)
		goto free_path;
	fd = mtpFileOpen(path, SCE_O_RDWR | SCE_O_APPEND, 0007);
	if (fd < 0) {
		MTP_ERR(MTP_TAG_CMDE, 307, "unable to open file=%s, 0x%x\n", path, fd);
		ret = -2;
		goto free;
	}
	mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, 0);
	remaining = size;
	xferSize = size;
	if (size >= MTP_CMD_XFER_SIZE)
		xferSize = MTP_CMD_XFER_SIZE;
	total = size;
	if (context != NULL)
		mtpContextSetProgress(context, 0, size, 1);
	count = mtpCoreDriverRecvData(buffers[0], xferSize, NULL);
	if (count < 0) {
		MTP_ERR(MTP_TAG_CMDE, 325, "unable to receive data=0x%x\n", count);
		ret = -2;
		goto close;
	}
	if (remaining > 0) {
		current = 0;
		lastPercent = 0;
		for (;;) {
			remaining -= count;
			if (context != NULL)
				mtpContextSetProgress(context, total - remaining, total, 0);
			if (remaining > 0) {
				xferSize = remaining;
				if (remaining >= bufSize)
					xferSize = bufSize;
				ret = mtpXferStart(MTP_XFER_REQUEST_RECV, buffers[current == 0],
					xferSize, 0);
				if (ret < 0) {
					MTP_ERR(MTP_TAG_CMDE, 346, "unable to start xfer=0x%x\n",
						ret);
					ret = -2;
					break;
				}
			}
			if (mtpFileWrite(fd, buffers[current], count) < 0) {
				MTP_ERR(MTP_TAG_CMDE, 356, "unable to write\n");
				mtpXferWait(MTP_XFER_DONE);
				mtpDevIfCancelTransfer();
				*code = MTP_RESPONSE_TRANSACTION_CANCELLED;
				ret = -2;
				break;
			}
			count = mtpXferWait(MTP_XFER_DONE);
			if (count < 0) {
				MTP_ERR(MTP_TAG_CMDE, 370, "wait xfer failed=0x%x\n", count);
				ret = -2;
				break;
			}
			current ^= 1;
			percent = 100 - (int)(remaining * 100 / total);
			if (percent - lastPercent >= MTP_CMD_PROGRESS_STEP) {
				mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, percent);
				lastPercent = percent;
			}
			if (remaining <= 0)
				break;
		}
	}
	if (context != NULL)
		mtpContextSetProgress(context, total - remaining, total, 0);
close:
	mtpFileClose(fd);
free_path:
	mtpUtilFree(path);
free:
	mtpUtilFree(buffers[0]);
	mtpUtilFree(buffers[1]);
	if (setDates != 0 && ret == 0)
		mtpFileSetDates(pfsPath, &created, &modified);
	return ret;
}

int mtpCmdSendPartOfObject(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	SceUInt64 size = 0;
	MtpContext *context = NULL;
	SceUInt32 param;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 1830, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1838, "GetContext %u failed: 0x%x\n", contextId, n);
		ret = -2;
		goto error;
	}
	if (!((context->dataSize == sizeof(MtpPartOfObjectRequest) &&
	       context->opcode == MTP_OPERATION_REQUEST_PART_OF_OBJECT) ||
	      context->opcode == MTP_OPERATION_REQUEST_RESUME_IMPORT_OBJECT ||
	      (context->dataSize == sizeof(SceUInt32) &&
	       context->opcode == MTP_OPERATION_REQUEST_SEND_OBJECT))) {
		MTP_ERR(MTP_TAG_CMDE, 1846, "Invalid data size\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(&size, sizeof(size), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1854, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	if (mtpCoreGetImportRunning()) {
		if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
			NetMtpExpandSocketReceiveTimeout();
		if (context->opcode == MTP_OPERATION_REQUEST_PART_OF_OBJECT ||
		    context->opcode == MTP_OPERATION_REQUEST_RESUME_IMPORT_OBJECT)
			ret = receiveFileData(context, 0, size, &code, 0);
		else if (context->opcode == MTP_OPERATION_REQUEST_SEND_OBJECT)
			ret = receiveFileData(context, 1, size, &code, 1);
		if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
			NetMtpResetSocketReceiveTimeout();
		if (ret != 0) {
			MTP_ERR(MTP_TAG_CMDE, 1909, "Failed for context %u: 0x%x\n", context->id,
				ret);
			goto error;
		}
	} else {
		if (context->sendRecv == NULL) {
			MTP_ERR(MTP_TAG_CMDE, 1881, "RecvData is not set\n");
			ret = -2;
			goto error;
		}
		if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
			NetMtpExpandSocketReceiveTimeout();
		ret = mtpContextPassRecvData(context, size, &code, 1);
		if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
			NetMtpResetSocketReceiveTimeout();
		if (ret != 0) {
			MTP_ERR(MTP_TAG_CMDE, 1877, "Failed for context %u: 0x%x\n", context->id,
				ret);
			goto error;
		}
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	param = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &param, 1, operation, response);
	mtpCoreSetImportRunning(0);
	mtpCoreSetImportContextId(0);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	mtpCoreSetImportRunning(0);
	mtpCoreSetImportContextId(0);
	return ret;
}

int mtpCmdCheckObjectExistence(const MtpOperation *operation, MtpOperation *response)
{
	MtpLogicalStorage *storage = NULL;
	MtpContext *context = NULL;
	SceUInt32 storageId = 0;
	SceUInt32 param[3];
	SceIoStat stat;
	SceUInt32 uniqueId;
	SceUInt32 handle;
	SceSize nameLength;
	SceUInt32 code;
	char *pfsPath;
	char *path;
	void *buf;
	int ret = 0;
	int n;

	mtpContextGetContextByEvent(MTP_OPERATION_REQUEST_RESUME_IMPORT_OBJECT, &context);
	if (operation->numParams != 0)
		storageId = operation->param[0];
	handle = mtpCoreGetObjectImportHandle();
	if (handle == 0) {
		code = MTP_RESPONSE_VENDOR_NO_IMPORT_HANDLE;
		goto error;
	}
	if (!mtpStrgIsObjectHandleValid(handle)) {
		code = MTP_RESPONSE_VENDOR_INVALID_IMPORT_HANDLE;
		goto error;
	}
	code = MTP_RESPONSE_VENDOR_OBJECT_EXISTENCE;
	buf = mtpUtilMalloc(MTP_CHECK_OBJECT_EXISTENCE_DATA_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 1987, "unable to alloc for receive buffer\n");
		code = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(buf, MTP_CHECK_OBJECT_EXISTENCE_DATA_SIZE, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 1995, "unable to receive data=0x%x\n", ret);
		goto free_buf;
	}
	n = mtpStrgGetLogicalStorage(storageId, &storage);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2001, "unable to get logical storage=0x%x\n", n);
		ret = -2;
		goto free_buf;
	}
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2008, "unable to convert MTP handle to uniqueID=0x%x\n", n);
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free_buf;
	}
	nameLength = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(nameLength * 2 + 2046);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2018, "unable to alloc memory for file name\n");
		ret = -2;
		goto free_buf;
	}
	pfsPath = &path[nameLength + 1023];
	mtpUtilStrcpy(path, storage->info->name);
	n = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_PATH,
		&path[nameLength], 1023);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2030, "unable to get full path name from cache=0x%x\n", n);
		ret = -2;
		goto free_path;
	}
	ret = mtpStrgCreatePfsPath(storage, &path[nameLength], &path[nameLength + 1023],
		nameLength + 1023, 1);
	if (ret < 0)
		goto free_path;
	ret = sceIoGetstat(pfsPath, &stat);
	if (ret < 0)
		goto free_path;
	mtpUtilFree(path);
	if (context != NULL)
		mtpContextPutContext(context, 1);
	param[0] = handle;
	param[1] = (SceUInt32)(stat.st_size >> 32);
	param[2] = (SceUInt32)stat.st_size;
	mtpUtilSetMtpResponse(MTP_RESPONSE_VENDOR_OBJECT_EXISTENCE, param, 3, operation,
		response);
	mtpUtilFree(buf);
	return ret;
free_path:
	mtpUtilFree(path);
free_buf:
	mtpUtilFree(buf);
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdSendStorageSize(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpContext *context = NULL;
	SceUInt32 param;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2090, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2098, "GetContext %u failed: 0x%x\n", contextId, n);
		ret = -2;
		goto error;
	}
	if (context->dataSize != MTP_HOST_STORAGE_SIZE_DATA_SIZE) {
		MTP_ERR(MTP_TAG_CMDE, 2104, "Invalid data size\n");
		ret = -2;
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2110, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(context->data, MTP_HOST_STORAGE_SIZE_DATA_SIZE, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2117, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	mtpContextPutContext(context, 1);
	param = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &param, 1, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetPartOfObject(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpPartOfObjectRequest *request;
	MtpContext *context = NULL;
	SceUInt64 dataSize;
	SceUInt32 total;
	int send = 1;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2155, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2163, "GetContext %u failed: 0x%x\n", contextId, n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpPartOfObjectRequest) ||
	    context->opcode != MTP_OPERATION_REQUEST_SEND_PART_OF_OBJECT) {
		MTP_ERR(MTP_TAG_CMDE, 2170, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	request = (MtpPartOfObjectRequest *)context->data;
	if (request == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2177, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	dataSize = request->size + sizeof(MtpPartOfObjectRequest);
	total = 0xFFFFFFFF;
	if (dataSize <= 0xFFFFFFFF)
		total = (SceUInt32)dataSize;
	ret = mtpCoreSendData(request, sizeof(MtpPartOfObjectRequest), total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2191, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	if (context->sendRecv == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2218, "SendRecvData is not set\n");
		ret = -2;
		goto error;
	}
	if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
		NetMtpExpandSocketSendTimeout();
	ret = mtpContextPassSendData(context->sendRecv, request->size, total, &code, 1);
	if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
		NetMtpResetSocketSendTimeout();
	if (ret != 0) {
		MTP_ERR(MTP_TAG_CMDE, 2211, "Failed for context %u: 0x%x\n", context->id, ret);
		if (ret != -2)
			send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdGetPartOfObjectSpecfiedByHost(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 handle = operation->param[0];
	SceInt64 offset = (SceInt64)((SceUInt64)operation->param[1] << 32 | operation->param[2]);
	SceInt64 size = (SceInt64)((SceUInt64)operation->param[3] << 32 | operation->param[4]);
	void *(*allocFunc)(SceSize align, SceSize size);
	void (*freeFunc)(void *ptr) = NULL;
	MtpLogicalStorage *storage = NULL;
	MtpContext *context = NULL;
	MtpPartOfObjectHeader header;
	void *buffers[2];
	SceSize bufSize = 0;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int current;
	int lastPercent;
	int percent;
	int bufferState;
	int started;
	SceInt64 objectSize = 0;
	SceInt64 remaining;
	SceInt64 total;
	SceSize xferSize;
	SceUInt32 uniqueId;
	SceSize nameLength;
	char *pfsPath;
	char *path;
	SceUID fd;
	int ret;
	int n;

	buffers[0] = NULL;
	buffers[1] = NULL;
	if (operation->numParams < 3) {
		MTP_ERR(MTP_TAG_CMDE, 2279, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGetContextByEvent(MTP_OPERATION_REQUEST_RESUME_EXPORT_OBJECT, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2287, "mtpContextGetContextByEvent failed: 0x%x\n", n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if ((handle & MTP_OBJECT_HANDLE_DIR) != 0) {
		MTP_ERR(MTP_TAG_CMDE, 2295, "handle is association\n");
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto error;
	}
	if (mtpStrgGetLogicalStorage(MTP_OBJECT_STORAGE_ID(handle), &storage) < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2303, "storage is not available\n");
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto error;
	}
	nameLength = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(nameLength * 2 + 2046);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2313, "unable to alloc memory for file name\n");
		ret = -2;
		goto error;
	}
	pfsPath = &path[nameLength + 1023];
	n = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2321, "unable to convert MTP handle to UniquId=0x%x\n", n);
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		ret = -2;
		goto free;
	}
	mtpUtilStrcpy(path, storage->info->name);
	n = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_PATH,
		&path[nameLength], 1023);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2333, "unable to get full path name from cache=0x%x\n", n);
		ret = -2;
		goto free;
	}
	ret = mtpStrgCreatePfsPath(storage, &path[nameLength], &path[nameLength + 1023],
		nameLength + 1023, 1);
	if (ret < 0)
		goto free;
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_CACHE_PROP_SIZE,
		&objectSize, sizeof(objectSize));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2345, "unable to get object size from cache=0x%x\n", ret);
		ret = -2;
		goto free;
	}
	if (offset + size > objectSize) {
		MTP_ERR(MTP_TAG_CMDE, 2352, "invalid offset or size specified, exceeding the "
			"file size %lld bytes.\n", offset + size - objectSize);
		code = MTP_RESPONSE_VENDOR_UNKNOWN_STATUS;
		goto free;
	}
	total = size + sizeof(header.total);
	mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, 0);
	if (context != NULL)
		mtpContextSetProgress(context, 0, total, 1);
	if (size <= 0) {
		ret = mtpCoreSendData(&total, 0, total);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMDE, 2520, "unable to send data=0x%x", ret);
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
		MTP_ERR(MTP_TAG_CMDE, 2397, "unable to alloc xfer bufs\n");
		ret = -2;
		goto free_buffers;
	}
	fd = mtpFileOpen(pfsPath, SCE_O_RDONLY, 0);
	if (fd < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2406, "unable to open=%s, 0x%x\n", pfsPath, fd);
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto free_buffers;
	}
	if (offset > 0 && mtpFileSeek(fd, offset, SCE_SEEK_SET) < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2415, "unable to seek to offset\n");
		code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
		ret = -2;
		goto close;
	}
	sce_paf_memcpy(header.total, &total, sizeof(header.total));
	if (mtpFileRead(fd, header.data, sizeof(header.data)) < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2425, "unable to read data\n");
		mtpDevIfCancelTransfer();
		code = MTP_RESPONSE_TRANSACTION_CANCELLED;
		ret = -2;
		goto close;
	}
	ret = mtpCoreSendData(&header, sizeof(header), total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2432, "unable to send data=0x%x", ret);
		send = 0;
		goto close;
	}
	remaining = size - sizeof(header.data);
	if (remaining > 0) {
		current = 0;
		lastPercent = 0;
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
					MTP_ERR(MTP_TAG_CMDE, 2447, "unable to read data\n");
					mtpXferWait(MTP_XFER_DONE_SEND);
					mtpDevIfCancelTransfer();
					code = MTP_RESPONSE_TRANSACTION_CANCELLED;
					goto close;
				}
				started = 1;
			}
			ret = mtpXferWait(MTP_XFER_DONE_SEND);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_CMDE, 2460, "wait xfer error=0x%x\n", ret);
				send = 0;
				goto close;
			}
			if (started) {
				ret = mtpXferStart(MTP_XFER_REQUEST_SEND, buffers[current],
					xferSize, total);
				if (ret < 0) {
					MTP_ERR(MTP_TAG_CMDE, 2468, "start xfer error=0x%x\n", ret);
					ret = -2;
					goto close;
				}
			}
			remaining -= xferSize;
			current ^= 1;
			percent = 100 - (int)(remaining * 100 / total);
			if (percent - lastPercent >= MTP_CMD_PROGRESS_STEP) {
				mtpCoreSetPublicStatus(MTP_CORE_PUBLIC_STATUS_RECEIVING, percent);
				lastPercent = percent;
			}
			if (context != NULL)
				mtpContextSetProgress(context, total - remaining, total, 0);
			if ((bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE && !started) ||
			    (bufferState == MTP_UTIL_BUFFER_STATE_TO_HEAP && !started)) {
				ret = mtpUtilChangeBufferAllocator(&buffers[0], &buffers[1],
					&bufSize, &freeFunc,
					bufferState == MTP_UTIL_BUFFER_STATE_TO_LARGE);
				if (ret < 0) {
					ret = -2;
					MTP_ERR(MTP_TAG_CMDE, 2500, "unable to alloc xfer bufs\n");
				}
			}
			if (ret == -2)
				break;
		} while (remaining > 0);
	}
	ret = mtpXferWait(MTP_XFER_DONE_SEND);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2511, "wait xfer error=0x%x\n", ret);
		send = 0;
		goto close;
	}
	mtpFileClose(fd);
done:
	if (freeFunc != NULL) {
		freeFunc(buffers[0]);
		freeFunc(buffers[1]);
	}
	mtpUtilFree(path);
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
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

int mtpCmdGetTreateObjectRequest(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2574, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2582, "GetContext %u failed: 0x%x\n", contextId, n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2589, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(context->data, context->dataSize, context->dataSize);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2596, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdSendCopyConfirmationInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpCopyConfirmationAnswer answer;
	MtpCopyConfirmationInfo *info;
	SceUInt32 numItems;
	MtpContext *context = NULL;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2636, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2644, "GetContext %u failed: 0x%x\n", contextId, n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpCopyConfirmationInfo)) {
		MTP_ERR(MTP_TAG_CMDE, 2651, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(&answer, sizeof(answer), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2659, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	info = (MtpCopyConfirmationInfo *)context->data;
	if (info == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2664, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	numItems = answer.numItems;
	info->object = answer.object;
	info->numItems = numItems;
	if (context->sendRecv == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2682, "RecvData is not set\n");
		ret = -2;
		goto error;
	}
	ret = mtpContextPassRecvData(context, numItems * sizeof(SceUInt32), &code, 1);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_CMDE, 2678, "Failed for context %u: 0x%x\n", context->id, ret);
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetCopyConfirmationInfoRequest(const MtpOperation *operation,
	MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpCopyConfirmationInfo *info;
	MtpContext *context = NULL;
	SceUInt32 header;
	SceUInt32 size;
	int send = 1;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2720, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2728, "GetContext %u failed: 0x%x\n", contextId, n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpCopyConfirmationInfo)) {
		MTP_ERR(MTP_TAG_CMDE, 2735, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	info = (MtpCopyConfirmationInfo *)context->data;
	if (info == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2742, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	header = info->header;
	size = info->size;
	ret = mtpCoreSendData(&header, sizeof(header), size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2753, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	if (context->sendRecv == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2765, "SendData is not set\n");
		ret = -2;
		goto error;
	}
	ret = mtpContextPassSendData(context->sendRecv, size - sizeof(header), size, &code, 0);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_CMDE, 2761, "Failed for context %u: 0x%x\n", context->id, ret);
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdGetObjectMetadataItemsRequest(const MtpOperation *operation,
	MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContextBuffer *buffer;
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2801, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2809, "GetContext %u failed: 0x%x\n", contextId, n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpContextBuffer)) {
		MTP_ERR(MTP_TAG_CMDE, 2826, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	buffer = (MtpContextBuffer *)context->data;
	if (buffer == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2833, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(buffer->data, buffer->size, buffer->size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2842, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdGetSearchInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpSearchData *search;
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2878, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2886, "GetContext %u failed: 0x%x\n", contextId, n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpSearchData) ||
	    context->opcode != MTP_OPERATION_REQUEST_SEARCH_OBJECT) {
		MTP_ERR(MTP_TAG_CMDE, 2893, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	search = (MtpSearchData *)context->data;
	if (search == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2900, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(search->data, search->size, search->size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2909, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

int mtpCmdGetHandoverInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	int ret;
	int n;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_CMDE, 2937, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	n = mtpContextGet(contextId, 1, &context);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2945, "GetContext %u failed: 0x%x\n", contextId, n);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != MTP_HANDOVER_INFO_SIZE ||
	    context->opcode != MTP_OPERATION_REQUEST_HANDOVER) {
		MTP_ERR(MTP_TAG_CMDE, 2952, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_CMDE, 2959, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreSendData(context->data, MTP_HANDOVER_INFO_SIZE, MTP_HANDOVER_INFO_SIZE);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMDE, 2966, "mtpCoreSendData64 failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

}

/* the results the API's End calls copy out */
static void getResultParam(MtpContext *context, void *result)
{
	if (result != NULL)
		*(SceUInt32 *)result = context->resultParam;
}

static void getNumOfObjectResult(MtpContext *context, void *result)
{
	if (context->dataSize == sizeof(SceUInt32) && context->data != NULL && result != NULL)
		*(SceUInt32 *)result = *(SceUInt32 *)context->data;
}

static void getHostStorageSizeResult(MtpContext *context, void *result)
{
	if (context->dataSize == MTP_HOST_STORAGE_SIZE_DATA_SIZE && context->data != NULL &&
	    result != NULL)
		mtpUtilMemcpy(result, context->data, MTP_HOST_STORAGE_SIZE_DATA_SIZE);
}

static void getTotalObjectSizeResult(MtpContext *context, void *result)
{
	const MtpTotalObjectSize *total;
	MtpTotalObjectSizeResult *out = (MtpTotalObjectSizeResult *)result;

	if (context->dataSize != MTP_TOTAL_OBJECT_SIZE_DATA_SIZE)
		return;
	total = (const MtpTotalObjectSize *)context->data;
	if (total != NULL && out != NULL) {
		*out->numObjects = total->numObjects;
		*out->totalSize = total->totalSize;
	}
}

static void deleteQueryObjectMetadataInfo(void *data, SceSize size)
{
	if (data == NULL || size != sizeof(MtpContextBuffer)) {
		MTP_ERR(MTP_TAG_CMDE, 176, "Wrong Data: %u\n", size);
		return;
	}
	mtpUtilFree(((MtpContextBuffer *)data)->data);
	mtpUtilFree(data);
}

static void getSearchObjectResult(MtpContext *context, void *result)
{
	const MtpSearchData *search;
	MtpSearchResult *out = (MtpSearchResult *)result;
	SceUInt32 *numFound;
	SceUInt32 *searchId;

	if (context->dataSize != sizeof(MtpSearchData))
		return;
	search = (const MtpSearchData *)context->data;
	if (search != NULL && out != NULL) {
		numFound = out->numFound;
		searchId = out->searchId;
		if (numFound != NULL)
			*numFound = search->numFound;
		if (searchId != NULL)
			*searchId = search->searchId;
	}
}

static void deleteSearchNum(void *data, SceSize size)
{
	if (data == NULL || size != sizeof(MtpSearchData)) {
		MTP_ERR(MTP_TAG_CMDE, 205, "Wrong Data: %u\n", size);
		return;
	}
	mtpUtilFree(((MtpSearchData *)data)->data);
	mtpUtilFree(data);
}
