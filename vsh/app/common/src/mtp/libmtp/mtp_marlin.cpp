/*
 * SceLibMtp: Marlin (MRLN), the vendor operations that carry Marlin DRM messages between the
 * host and the Marlin background application, and the licence requests the shell starts.
 */

#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <sceerror.h>
#include "mtp_internal.h"

#define MTP_TAG_MRLN			"MRLN:"

/* the data the background application and the host exchange (mtpMarlinGetInfo's types) */
#define MTP_MARLIN_INFO_STATUS		0
#define MTP_MARLIN_INFO_REPLY_SIZE	1
#define MTP_MARLIN_INFO_MESSAGE_SIZE	2
#define MTP_MARLIN_INFO_REPLY		3
#define MTP_MARLIN_INFO_MESSAGE		4

/* the messages a host may have open at once */
#define MTP_MARLIN_NUM_MESSAGES		5
/* a message's ObjectInfo: the format and the name the host gives it */
#define MTP_MARLIN_MESSAGE_FORMAT	0xB401
#define MTP_MARLIN_OBJECT_INFO_SIZE	108
/* the most a host sends in one go */
#define MTP_MARLIN_RECV_SIZE		0x200

/* the background application's event flag and its bits */
#define MTP_MARLIN_BG_EVF_NAME		"SceMarlinBgAppMsgProcEvent"
#define MTP_MARLIN_BG_EVF_REQUEST	0x1
#define MTP_MARLIN_BG_EVF_REPLY		0x2
#define MTP_MARLIN_BG_EVF_DONE		0x4
#define MTP_MARLIN_BG_EVF_ERROR		0x8
#define MTP_MARLIN_BG_OPEN_RETRIES	50
#define MTP_MARLIN_BG_OPEN_DELAY	200000
#define MTP_MARLIN_BG_READY_TIMEOUT	30000000
#define MTP_MARLIN_BG_REPLY_TIMEOUT	10000000
/* the background application's own errors, which mean it has no message */
#define MTP_MARLIN_BG_ERROR_MIN		0x100
#define MTP_MARLIN_BG_ERROR_MAX		0x1FF

/* the context data a licence request carries */
#define MTP_MARLIN_NUM_CONTEXT_DATA	10
#define MTP_MARLIN_CONTEXT_ARG		1
#define MTP_MARLIN_CONTEXT_LICENSE_ID	2
#define MTP_MARLIN_CONTEXT_LICENSE_ARG	3
#define MTP_MARLIN_CONTEXT_LICENSE_SIZE	5
#define MTP_MARLIN_LICENSE_ID_SIZE	0x81
#define MTP_MARLIN_LICENSE_PATH_SIZE	256
#define MTP_MARLIN_LICENSE_PATH		"%sMLN_ROOT/LICENSE/%s.MNL"
#define MTP_MARLIN_LICENSE_READ_SIZE	0x400
/* SetMarlinLicenseIdToContext's context, which no request owns */
#define MTP_MARLIN_CONTEXT_ID_NONE	0x1FFFF
#define MTP_MARLIN_EVENT_NONE		0xFFFF

/* the requests the shell starts: the event the host gets, and its context */
#define MTP_EVENT_MARLIN_ACTIVATION	0xC603
#define MTP_EVENT_MARLIN_DEACTIVATION	0xC604
#define MTP_EVENT_MARLIN_LICENSE_MOVE	0xC605
#define MTP_EVENT_MARLIN_LICENSE_COPY	0xC606
#define MTP_EVENT_MARLIN_LICENSE_CHECK	0xC607
#define MTP_EVENT_MARLIN_SHARED_INFO_ACQ	0xC608
#define MTP_MARLIN_CONTEXT_ID(event)	(0x10000 | (event))
#define MTP_MARLIN_EVF_NAME		"SceMtpMarlin"
#define MTP_MARLIN_EVF_RESULT		0x1
#define MTP_MARLIN_RESULT_TIMEOUT	120000000

/* the results the host sends back (SendMarlinResults) */
#define MTP_MARLIN_NUM_RESULTS		5
#define MTP_MARLIN_RESULT_ERROR		1
#define MTP_MARLIN_RESULT_ACTIVATION_2	2
#define MTP_MARLIN_RESULT_ACTIVATION_3	3
#define MTP_MARLIN_RESULT_LICENSE_STATE	5
#define MTP_MARLIN_RESULT_SHARED_INFO	6

/* the host's errors, as SceLibMtp reports them */
#define SCE_MTP_ERROR_MARLIN_BASE		((int)0x80546000)
#define SCE_MTP_ERROR_MARLIN_SECURE		((int)0x80546100)
#define SCE_MTP_ERROR_MARLIN_DRM		((int)0x80546200)
#define SCE_MTP_ERROR_MARLIN_DRM_OTHER		((int)0x805462FF)
#define SCE_MTP_ERROR_MARLIN_NET		((int)0x80546300)
#define SCE_MTP_ERROR_MARLIN_NET_OTHER		((int)0x805463FF)
#define SCE_MTP_ERROR_MARLIN_KERNEL		((int)0x80546400)
#define SCE_MTP_ERROR_MARLIN_OTHER		((int)0x805464FF)
#define SCE_MTP_ERROR_MARLIN_NOT_ACTIVATED	((int)0x8054602D)
#define SCE_MTP_ERROR_MARLIN_LICENSE_STATE	((int)0x8054604B)
#define SCE_MTP_ERROR_MARLIN_LICENSE_EXPIRED	((int)0x80546060)
#define SCE_MTP_ERROR_MARLIN_LICENSE_NOT_YET	((int)0x80546061)
#define SCE_MTP_ERROR_MARLIN_LICENSE_COUNT	((int)0x80546062)
#define SCE_MTP_ERROR_MARLIN_ALREADY_ACTIVATED	((int)0x80546063)

/* a message: its bytes and their count */
typedef struct MtpMarlinBuffer {
	SceSize size;
	void *data;
} MtpMarlinBuffer;

/* a message the host has open: its object handle and its bytes */
typedef struct MtpMarlinMessage {
	SceUInt32 handle;
	MtpMarlinBuffer *buffer;
} MtpMarlinMessage;

/* one result the host sends back */
typedef struct MtpMarlinResultEntry {
	SceUInt32 type;
	SceUInt32 error;
} MtpMarlinResultEntry;

/* the host's results (44 bytes), and the event flag a request waits for them on */
typedef struct MtpMarlinResult {
	SceUID eventFlag;
	MtpMarlinResultEntry entries[MTP_MARLIN_NUM_RESULTS];
} MtpMarlinResult;

/* one datum of a request's context */
typedef struct MtpMarlinContextEntry {
	SceUInt32 type;
	SceSize size;
	SceUInt8 data[];
} MtpMarlinContextEntry;

/* a request's context (56 bytes): its header goes to the host as it is */
typedef struct MtpMarlinContext {
	SceUInt32 reserved;
	SceUInt32 contextId;
	SceUInt32 eventCode;
	SceUInt32 numData;
	MtpMarlinContextEntry *data[MTP_MARLIN_NUM_CONTEXT_DATA];
} MtpMarlinContext;

/*
 * the ObjectInfo dataset of a message (108 bytes), little-endian fields at any
 * alignment, so read and written byte by byte
 */
typedef struct MtpMarlinObjectInfo {
	SceUInt8 storageId[4];
	SceUInt8 objectFormat[2];
	SceUInt8 protectionStatus[2];
	SceUInt8 objectCompressedSize[4];
	SceUInt8 thumbFormat[2];
	SceUInt8 thumbCompressedSize[4];
	SceUInt8 thumbPixWidth[4];
	SceUInt8 thumbPixHeight[4];
	SceUInt8 imagePixWidth[4];
	SceUInt8 imagePixHeight[4];
	SceUInt8 imageBitDepth[4];
	SceUInt8 parentObject[4];
	SceUInt8 associationType[2];
	SceUInt8 associationDesc[4];
	SceUInt8 sequenceNumber[4];
	SceUInt8 filename[53];
	SceUInt8 dateCreated;
	SceUInt8 dateModified;
	SceUInt8 keywords;
} MtpMarlinObjectInfo;

static inline __attribute__((always_inline)) void _mtpMarlinSetU16(SceUInt8 *field,
	SceUInt16 value)
{
	field[0] = (SceUInt8)value;
	field[1] = (SceUInt8)(value >> 8);
}

static inline __attribute__((always_inline)) void _mtpMarlinSetU32(SceUInt8 *field,
	SceUInt32 value)
{
	field[0] = (SceUInt8)value;
	field[1] = (SceUInt8)(value >> 8);
	field[2] = (SceUInt8)(value >> 16);
	field[3] = (SceUInt8)(value >> 24);
}

static inline __attribute__((always_inline)) SceUInt16 _mtpMarlinGetU16(const SceUInt8 *field)
{
	return (SceUInt16)(field[0] | (field[1] << 8));
}

static inline __attribute__((always_inline)) SceUInt32 _mtpMarlinGetU32(const SceUInt8 *field)
{
	return field[0] | (field[1] << 8) | (field[2] << 16) | ((SceUInt32)field[3] << 24);
}

/* the name of every message, as an MTP string ("Marlin Message For MTP") */
typedef struct __attribute__((packed)) MtpMarlinMessageName {
	SceUInt8 length;
	SceUInt16 chars[23];
} MtpMarlinMessageName;

static const MtpMarlinMessageName s_marlinMessageName = {
	23,
	{ 'M', 'a', 'r', 'l', 'i', 'n', ' ', 'M', 'e', 's', 's', 'a', 'g', 'e', ' ', 'F', 'o',
	  'r', ' ', 'M', 'T', 'P', 0 },
};

/* the message and the reply the background application exchanges with SceLibMtp */
static SceUInt32 s_marlinStatus;
static void *s_marlinMessage;
static SceSize s_marlinMessageSize;
static void *s_marlinReply;
static SceSize s_marlinReplySize;
static MtpMarlinResult s_marlinResult;
static MtpMarlinContext s_marlinContext;
static MtpMarlinMessage s_marlinMessages[MTP_MARLIN_NUM_MESSAGES];

extern "C" {

int mtpMarlinGetInfo(int type, SceSize size, SceUInt32 offset, void *data, SceSize *outSize)
{
	int ret = 0;

	if (data == NULL || outSize == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	switch (type) {
	case MTP_MARLIN_INFO_STATUS:
		mtpUtilMemcpy(data, &s_marlinStatus, sizeof(s_marlinStatus));
		*outSize = sizeof(s_marlinStatus);
		break;
	case MTP_MARLIN_INFO_REPLY_SIZE:
		mtpUtilMemcpy(data, &s_marlinReplySize, sizeof(s_marlinReplySize));
		*outSize = sizeof(s_marlinReplySize);
		break;
	case MTP_MARLIN_INFO_MESSAGE_SIZE:
		mtpUtilMemcpy(data, &s_marlinMessageSize, sizeof(s_marlinMessageSize));
		*outSize = sizeof(s_marlinMessageSize);
		break;
	case MTP_MARLIN_INFO_REPLY:
		mtpUtilMemcpy(data, (SceUInt8 *)s_marlinReply + offset, size);
		*outSize = size;
		break;
	case MTP_MARLIN_INFO_MESSAGE:
		mtpUtilMemcpy(data, (SceUInt8 *)s_marlinMessage + offset, size);
		*outSize = size;
		break;
	}
	return ret;
}

int mtpMarlinSetInfo(int type, SceUInt32 offset, const void *data, SceSize size)
{
	int ret = 0;

	if (data == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	switch (type) {
	case MTP_MARLIN_INFO_STATUS:
		mtpUtilMemcpy(&s_marlinStatus, data, sizeof(s_marlinStatus));
		break;
	case MTP_MARLIN_INFO_REPLY_SIZE:
		mtpUtilMemcpy(&s_marlinReplySize, data, sizeof(s_marlinReplySize));
		if (s_marlinReply != NULL)
			mtpUtilFree(s_marlinReply);
		s_marlinReply = mtpUtilMalloc(s_marlinReplySize);
		if (s_marlinReply == NULL)
			ret = SCE_MTP_ERROR_NO_MEMORY;
		break;
	case MTP_MARLIN_INFO_MESSAGE_SIZE:
		mtpUtilMemcpy(&s_marlinMessageSize, data, sizeof(s_marlinMessageSize));
		if (s_marlinMessage != NULL)
			mtpUtilFree(s_marlinMessage);
		s_marlinMessage = mtpUtilMalloc(s_marlinMessageSize);
		if (s_marlinMessage == NULL)
			ret = SCE_MTP_ERROR_NO_MEMORY;
		break;
	case MTP_MARLIN_INFO_REPLY:
		mtpUtilMemcpy((SceUInt8 *)s_marlinReply + offset, data, size);
		break;
	case MTP_MARLIN_INFO_MESSAGE:
		mtpUtilMemcpy((SceUInt8 *)s_marlinMessage + offset, data, size);
		break;
	}
	return ret;
}

} /* extern "C" */

static __attribute__((noinline)) int _mtpMarlinSendData(const void *port, SceUInt32 transactionId,
	SceSize size, const void *data, int *send)
{
	MtpIfTransfer transfer;
	SceSize remaining;
	SceSize offset;
	int chunk;
	int ret = 0;

	transfer.transactionId = transactionId;
	transfer.buffer = NULL;
	transfer.size = 0;
	transfer.transferred = 0;
	transfer.totalSize = size;
	if (data == NULL || send == NULL)
		return -2;
	remaining = size;
	offset = 0;
	while (remaining != 0) {
		chunk = MTP_CMD_XFER_SIZE;
		if (remaining <= MTP_CMD_XFER_SIZE)
			chunk = remaining;
		transfer.size = chunk;
		transfer.buffer = (SceUInt8 *)data + offset;
		transfer.transferred = 0;
		ret = MtpIfSendDataWithParam(port, &transfer);
		if (ret < 0) {
			*send = 0;
			MTP_ERR(MTP_TAG_MRLN, 637, "unable to send data=0x%x\n", ret);
			return ret;
		}
		ret = 0;
		remaining -= (SceSize)transfer.transferred;
		offset += (SceSize)transfer.transferred;
	}
	*send = 0;
	return ret;
}

static inline __attribute__((always_inline)) int _mtpMarlinGetMessageHandles(SceUInt32 **handles,
	SceUInt32 *num)
{
	SceUInt32 *list;
	SceUInt32 count;
	int i;

	list = (SceUInt32 *)mtpUtilMalloc(MTP_MARLIN_NUM_MESSAGES * sizeof(SceUInt32));
	if (list == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 374, "unable to alloc handle buffer\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	count = 0;
	for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
		if (s_marlinMessages[i].handle != 0)
			list[count++] = s_marlinMessages[i].handle;
	}
	*handles = list;
	*num = count;
	return 0;
}

/* an MTP array of object handles, as GetMarlinMsgHdls sends it */
typedef struct MtpMarlinHandleArray {
	SceUInt8 num[4];
	SceUInt8 handles[][4];
} MtpMarlinHandleArray;

extern "C" {

int mtpCmdGetMarlinMsgHdls(const MtpOperation *operation, MtpOperation *response)
{
	MtpMarlinHandleArray *array = NULL;
	SceUInt32 *handles = NULL;
	SceUInt32 *params = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	SceUInt32 format = (SceUInt16)operation->param[1];
	SceUInt32 numParams = 0;
	SceUInt32 num;
	SceUInt32 param;
	SceUInt32 i;
	SceSize size;
	int send = 1;
	int ret = 0;
	int n;

	if (format != MTP_MARLIN_MESSAGE_FORMAT) {
		MTP_ERR(MTP_TAG_MRLN, 1093, "format code is not supported\n");
		ret = -2;
		code = MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
		goto end;
	}
	n = _mtpMarlinGetMessageHandles(&handles, &num);
	if (n < 0) {
		MTP_ERR(MTP_TAG_MRLN, 1099, "unable to get message num=%x\n", n);
		ret = -2;
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		goto end;
	}
	size = num * sizeof(SceUInt32) + sizeof(SceUInt32);
	array = (MtpMarlinHandleArray *)mtpUtilMalloc(size);
	if (array == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 1107, "unable to alloc work buffer\n");
		ret = -2;
		goto free;
	}
	mtpUtilMemset(array, 0, size);
	_mtpMarlinSetU32(array->num, num);
	for (i = 0; i < num; i++)
		_mtpMarlinSetU32(array->handles[i], handles[i]);
	ret = _mtpMarlinSendData(port, transactionId, size, array, &send);
	if (ret == 0) {
		code = MTP_RESPONSE_OK;
		param = num;
		params = &param;
		numParams = 1;
	}
free:
	if (handles != NULL)
		mtpUtilFree(handles);
	if (array != NULL)
		mtpUtilFree(array);
end:
	if (send == 1)
		_mtpMarlinSendData(port, transactionId, sizeof(ret), &ret, &send);
	mtpUtilSetMtpResponse(code, params, numParams, operation, response);
	return ret;
}

} /* extern "C" */

static __attribute__((noinline)) int _mtpMarlinCreateBuffer(SceSize size, const void *data,
	MtpMarlinBuffer **buffer)
{
	MtpMarlinBuffer *created;
	void *bytes;
	int ret = 0;

	if (size == 0 || buffer == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	created = (MtpMarlinBuffer *)mtpUtilMalloc(sizeof(*created));
	if (created == NULL) {
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto error;
	}
	mtpUtilMemset(created, 0, sizeof(*created));
	bytes = mtpUtilMalloc(size);
	if (bytes == NULL) {
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto error;
	}
	mtpUtilMemset(bytes, 0, size);
	if (data != NULL)
		mtpUtilMemcpy(bytes, data, size);
	created->data = bytes;
	created->size = size;
	*buffer = created;
	return ret;
error:
	if (created != NULL) {
		mtpUtilFree(created->data);
		mtpUtilFree(created);
	}
	return ret;
}

static inline __attribute__((always_inline)) void _mtpMarlinFreeBuffer(MtpMarlinBuffer *buffer)
{
	mtpUtilFree(buffer->data);
	mtpUtilFree(buffer);
}

static inline __attribute__((always_inline)) int _mtpMarlinCreateObjectInfo(SceUInt32 storageId,
	const MtpMarlinBuffer *message, MtpMarlinBuffer **objectInfo)
{
	MtpMarlinBuffer *buffer = NULL;
	MtpMarlinObjectInfo *info;
	SceSize size;
	int ret;

	if (message == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	size = message->size;
	ret = _mtpMarlinCreateBuffer(MTP_MARLIN_OBJECT_INFO_SIZE, NULL, &buffer);
	if (ret != 0) {
		if (buffer != NULL)
			_mtpMarlinFreeBuffer(buffer);
		return ret;
	}
	info = (MtpMarlinObjectInfo *)buffer->data;
	_mtpMarlinSetU32(info->storageId, storageId);
	_mtpMarlinSetU16(info->objectFormat, MTP_MARLIN_MESSAGE_FORMAT);
	_mtpMarlinSetU32(info->objectCompressedSize, size);
	_mtpMarlinSetU32(info->parentObject, 0);
	_mtpMarlinSetU16(info->associationType, 0);
	_mtpMarlinSetU32(info->associationDesc, 0);
	_mtpMarlinSetU32(info->sequenceNumber, 0);
	mtpUtilMemcpy(info->filename, &s_marlinMessageName, sizeof(s_marlinMessageName));
	info->dateCreated = 0;
	info->dateModified = 0;
	info->keywords = 0;
	*objectInfo = buffer;
	return ret;
}

extern "C" {

int mtpCmdGetMarlinMsgInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 handle = operation->param[0];
	MtpMarlinBuffer *objectInfo = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	/* the message's storage: the logical storage, then the physical one in bits 16-18 */
	SceUInt32 storageId = (MTP_OBJECT_STORAGE_ID(handle) & 0xFFF8FFFF) |
		((((handle >> 29) & 3) + 1) & 7) << 16;
	int send = 1;
	int ret = 0;
	int n;
	int i;

	for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
		if (s_marlinMessages[i].handle == handle && s_marlinMessages[i].buffer != NULL)
			break;
	}
	if (i == MTP_MARLIN_NUM_MESSAGES) {
		MTP_ERR(MTP_TAG_MRLN, 1168, "unable to get message buffer =0x%x\n",
			SCE_MTP_ERROR_NOT_INITIALIZED);
		ret = -2;
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
		goto end;
	}
	n = _mtpMarlinCreateObjectInfo(storageId, s_marlinMessages[i].buffer, &objectInfo);
	if (n < 0) {
		MTP_ERR(MTP_TAG_MRLN, 1174, "unable to create msginfo data=0x%x\n", n);
		ret = -2;
		goto free;
	}
	ret = _mtpMarlinSendData(port, transactionId, objectInfo->size, objectInfo->data, &send);
	if (ret == 0)
		code = MTP_RESPONSE_OK;
free:
	if (objectInfo != NULL)
		_mtpMarlinFreeBuffer(objectInfo);
end:
	if (send == 1)
		_mtpMarlinSendData(port, transactionId, sizeof(ret), &ret, &send);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetMarlinMsg(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 *params = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 handle = operation->param[0];
	SceUInt32 transactionId;
	SceUInt32 numParams = 0;
	MtpMarlinBuffer *buffer;
	int ret = 0;
	int send = 1;
	SceUInt32 param;
	int i;

	for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
		if (s_marlinMessages[i].handle == handle && s_marlinMessages[i].buffer != NULL)
			break;
	}
	transactionId = operation->transactionId;
	if (i == MTP_MARLIN_NUM_MESSAGES) {
		MTP_ERR(MTP_TAG_MRLN, 1222, "unable to get message buffer =0x%x\n",
			SCE_MTP_ERROR_NOT_INITIALIZED);
		ret = -2;
		code = MTP_RESPONSE_INVALID_OBJECT_HANDLE;
	} else {
		buffer = s_marlinMessages[i].buffer;
		ret = _mtpMarlinSendData(port, transactionId, buffer->size, buffer->data, &send);
		if (ret == 0) {
			param = 0;
			params = &param;
			code = MTP_RESPONSE_OK;
			numParams = 1;
		}
	}
	/* the message is the host's once it has read it */
	for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
		if (s_marlinMessages[i].handle == handle)
			break;
	}
	if (i != MTP_MARLIN_NUM_MESSAGES) {
		buffer = s_marlinMessages[i].buffer;
		if (buffer != NULL) {
			_mtpMarlinFreeBuffer(buffer);
			s_marlinMessages[i].buffer = NULL;
		}
		s_marlinMessages[i].handle = 0;
	}
	if (send == 1)
		_mtpMarlinSendData(port, transactionId, sizeof(ret), &ret, &send);
	mtpUtilSetMtpResponse(code, params, numParams, operation, response);
	return ret;
}

} /* extern "C" */

static __attribute__((noinline)) int _mtpMarlinRecvData(const void *port,
	SceUInt32 transactionId, SceSize size, void **data)
{
	MtpIfTransfer transfer;
	SceSize remaining;
	SceSize offset;
	void *buffer;
	int chunk;
	int ret = 0;

	if (size == 0 || data == NULL)
		return -2;
	transfer.transferred = 0;
	transfer.totalSize = 0;
	transfer.size = 0;
	transfer.transactionId = transactionId;
	transfer.buffer = NULL;
	buffer = mtpUtilMalloc(size);
	if (buffer == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 675, "unable to alloc work buffer\n");
		ret = -2;
		goto error;
	}
	remaining = size;
	offset = 0;
	while (remaining != 0) {
		chunk = MTP_MARLIN_RECV_SIZE;
		if (remaining <= MTP_MARLIN_RECV_SIZE)
			chunk = remaining;
		transfer.size = chunk;
		transfer.buffer = (SceUInt8 *)buffer + offset;
		transfer.transferred = 0;
		ret = MtpIfRecvDataWithParam(port, &transfer);
		if (ret != 0)
			goto error;
		remaining -= (SceSize)transfer.transferred;
		offset += (SceSize)transfer.transferred;
	}
	*data = buffer;
	return ret;
error:
	if (buffer != NULL)
		mtpUtilFree(buffer);
	return ret;
}

extern "C" {

int mtpCmdSendMarlinMsgInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 storageId = operation->param[0];
	MtpMarlinObjectInfo *info = NULL;
	SceUInt32 *params = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	SceUInt32 numParams = 0;
	SceUInt32 handle;
	SceUInt32 param[2];
	int ret;
	int i;

	/* a new message replaces whatever the host had open */
	for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
		s_marlinMessages[i].handle = 0;
		s_marlinMessages[i].buffer = NULL;
	}
	for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
		if (s_marlinMessages[i].handle == 0)
			break;
	}
	if (i == MTP_MARLIN_NUM_MESSAGES) {
		MTP_ERR(MTP_TAG_MRLN, 1278, "unable to create message handle =0x%x",
			SCE_MTP_ERROR_NOT_INITIALIZED);
		ret = -2;
		goto end;
	}
	handle = (((storageId - 1) & 3) << 27) | ((((storageId >> 16) - 1) & 3) << 29);
	if (i == 0)
		handle |= 0x7FFFFFF;
	else
		handle |= i & 0x3FFFFFF;
	s_marlinMessages[i].handle = handle;
	ret = _mtpMarlinRecvData(port, transactionId, MTP_MARLIN_OBJECT_INFO_SIZE,
		(void **)&info);
	if (ret != 0)
		goto end;
	if (_mtpMarlinGetU16(info->objectFormat) != MTP_MARLIN_MESSAGE_FORMAT) {
		MTP_ERR(MTP_TAG_MRLN, 1293, "format code is not supported\n");
		code = MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
		ret = -2;
		goto end;
	}
	if (mtpUtilMemcmp(info->filename, &s_marlinMessageName,
	    sizeof(s_marlinMessageName)) != 0) {
		MTP_ERR(MTP_TAG_MRLN, 1299, "file name is not supported\n");
		code = MTP_RESPONSE_INVALID_DATASET;
		ret = -2;
		goto end;
	}
	mtpCoreClearObjectTransfer();
	mtpCoreSetObjectTransferSize(_mtpMarlinGetU32(info->objectCompressedSize));
	mtpCoreSetObjectTransferHandle(handle);
	mtpCoreClearObjectImport();
	mtpCoreSetObjectImportHandle(handle);
	param[0] = storageId;
	code = MTP_RESPONSE_OK;
	param[1] = mtpCoreGetObjectTransferHandle2() & 0x7FFFFFFF;
	params = param;
	numParams = 2;
end:
	if (info != NULL)
		mtpUtilFree(info);
	mtpUtilSetMtpResponse(code, params, numParams, operation, response);
	return ret;
}

} /* extern "C" */

/* hands a message to the background application and keeps its reply for the host */
static __attribute__((noinline)) int _mtpMarlinProcessMessage(SceUInt32 handle, const void *message,
	SceSize size)
{
	MtpMarlinBuffer *reply = NULL;
	SceSize replySize = 0;
	SceSize outSize = 0;
	SceUInt32 error = 0;
	SceUInt32 outBits = 0;
	SceUInt32 replyTimeout = MTP_MARLIN_BG_REPLY_TIMEOUT;
	SceUInt32 timeout = MTP_MARLIN_BG_READY_TIMEOUT;
	SceUID evf = -1;
	int retries = MTP_MARLIN_BG_OPEN_RETRIES;
	int ret;
	int i;

	if (message == NULL) {
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	ret = mtpMarlinSetInfo(MTP_MARLIN_INFO_MESSAGE_SIZE, 0, &size, sizeof(size));
	if (ret != 0)
		goto end;
	ret = mtpMarlinSetInfo(MTP_MARLIN_INFO_MESSAGE, 0, message, size);
	if (ret != 0)
		goto end;
	for (;;) {
		evf = sceKernelOpenEventFlag(MTP_MARLIN_BG_EVF_NAME);
		if (evf > 0)
			break;
		ret = evf;
		sceKernelDelayThread(MTP_MARLIN_BG_OPEN_DELAY);
		if (--retries == 0) {
			if (evf < 0)
				goto end;
			break;
		}
	}
	ret = sceKernelWaitEventFlag(evf, MTP_MARLIN_BG_EVF_DONE | MTP_MARLIN_BG_EVF_ERROR,
		SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &outBits, &timeout);
	if (ret < 0 || outBits == MTP_MARLIN_BG_EVF_ERROR)
		goto end;
	ret = sceKernelSetEventFlag(evf, MTP_MARLIN_BG_EVF_REQUEST);
	if (ret < 0)
		goto end;
	ret = sceKernelWaitEventFlag(evf, MTP_MARLIN_BG_EVF_REPLY | MTP_MARLIN_BG_EVF_ERROR,
		SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &outBits,
		&replyTimeout);
	if (ret < 0 || outBits == MTP_MARLIN_BG_EVF_ERROR)
		goto end;
	ret = mtpMarlinGetInfo(MTP_MARLIN_INFO_REPLY_SIZE, sizeof(replySize), 0, &replySize,
		&outSize);
	if (ret != 0)
		goto end;
	ret = _mtpMarlinCreateBuffer(replySize, NULL, &reply);
	if (ret != 0)
		goto end;
	ret = mtpMarlinGetInfo(MTP_MARLIN_INFO_REPLY, replySize, 0, reply->data, &reply->size);
	if (ret != 0)
		goto end;
	ret = sceKernelSetEventFlag(evf, MTP_MARLIN_BG_EVF_DONE);
	if (ret < 0)
		goto end;
	if (reply == NULL) {
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
	} else {
		for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
			if (s_marlinMessages[i].handle == handle)
				break;
		}
		if (i == MTP_MARLIN_NUM_MESSAGES) {
			ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		} else {
			if (s_marlinMessages[i].buffer != NULL)
				_mtpMarlinFreeBuffer(s_marlinMessages[i].buffer);
			s_marlinMessages[i].buffer = reply;
			ret = 0;
		}
	}
	if (ret == 0)
		reply = NULL;
end:
	if (outBits == MTP_MARLIN_BG_EVF_ERROR) {
		mtpMarlinGetInfo(MTP_MARLIN_INFO_REPLY, sizeof(error), 0, &error, &outSize);
		if (error != 0) {
			if (error < MTP_MARLIN_BG_ERROR_MIN || error > MTP_MARLIN_BG_ERROR_MAX)
				ret = error;
			else
				ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		}
	}
	sceKernelCloseEventFlag(evf);
	if (ret != 0) {
		for (i = 0; i < MTP_MARLIN_NUM_MESSAGES; i++) {
			if (s_marlinMessages[i].handle == handle)
				break;
		}
		if (i != MTP_MARLIN_NUM_MESSAGES) {
			if (s_marlinMessages[i].buffer != NULL) {
				_mtpMarlinFreeBuffer(s_marlinMessages[i].buffer);
				s_marlinMessages[i].buffer = NULL;
			}
			s_marlinMessages[i].handle = 0;
		}
	}
	return ret;
}

extern "C" {

int mtpCmdSendMarlinMsg(const MtpOperation *operation, MtpOperation *response)
{
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	void *message = NULL;
	SceSize size;
	SceUInt32 handle;
	int ret;
	int n;

	size = mtpCoreGetObjectTransferSize();
	handle = mtpCoreGetObjectTransferHandle2();
	if (size == 0 || handle == 0) {
		MTP_ERR(MTP_TAG_MRLN, 1352, "not found valid message info\n");
		code = MTP_RESPONSE_NO_VALID_OBJECT_INFO;
		ret = -2;
		goto end;
	}
	ret = _mtpMarlinRecvData(port, transactionId, size, &message);
	if (ret != 0)
		goto end;
	n = _mtpMarlinProcessMessage(handle, message, size);
	if (n < 0) {
		MTP_ERR(MTP_TAG_MRLN, 1365, "unable to process marlin data=0x%x\n", n);
		ret = -2;
	} else {
		code = MTP_RESPONSE_OK;
	}
end:
	if (message != NULL)
		mtpUtilFree(message);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdSendMarlinResults(const MtpOperation *operation, MtpOperation *response)
{
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	MtpMarlinResult *results = NULL;
	SceUID evf = -1;
	int ret;

	ret = _mtpMarlinRecvData(port, transactionId, sizeof(MtpMarlinResult),
		(void **)&results);
	if (ret == 0) {
		if (results == NULL) {
			MTP_ERR(MTP_TAG_MRLN, 1409, "unable to process marlin data=0x%x\n", 0);
			ret = -2;
		} else {
			/* the host's results, under the request's own event flag */
			evf = s_marlinResult.eventFlag;
			s_marlinResult = *results;
			code = MTP_RESPONSE_OK;
			s_marlinResult.eventFlag = evf;
		}
	}
	if (results != NULL)
		mtpUtilFree(results);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	sceKernelSetEventFlag(evf, MTP_MARLIN_EVF_RESULT);
	return ret;
}

} /* extern "C" */

/* copies out the request context's datum of a type */
static __attribute__((noinline)) int _mtpGetContextData(SceUInt32 contextId, SceUInt32 type,
	SceSize *size, void **data)
{
	MtpMarlinContextEntry *entry;
	SceSize found = 0;
	void *copy = NULL;
	int ret = 0;
	int i;

	(void)contextId;
	if (size == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < MTP_MARLIN_NUM_CONTEXT_DATA; i++) {
		entry = s_marlinContext.data[i];
		if (entry != NULL && entry->type == type) {
			found = entry->size;
			copy = mtpUtilMalloc(found);
			if (copy == NULL) {
				ret = SCE_MTP_ERROR_NO_MEMORY;
				goto error;
			}
			mtpUtilMemset(copy, 0, found);
			mtpUtilMemcpy(copy, s_marlinContext.data[i]->data, found);
			break;
		}
	}
	if (found != 0 || copy != NULL) {
		*size = found;
		*data = copy;
		return ret;
	}
	ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
error:
	if (copy != NULL)
		mtpUtilFree(copy);
	return ret;
}

static __attribute__((noinline)) int _mtpSetContextData(SceUInt32 contextId, SceUInt32 type,
	SceSize size, const void *data)
{
	MtpMarlinContextEntry *entry;
	int ret = 0;
	int i;

	if (s_marlinContext.contextId != contextId)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (s_marlinContext.numData >= MTP_MARLIN_NUM_CONTEXT_DATA)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	entry = (MtpMarlinContextEntry *)mtpUtilMalloc(size + sizeof(MtpMarlinContextEntry));
	if (entry == NULL) {
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto end;
	}
	mtpUtilMemset(entry, 0, size + sizeof(MtpMarlinContextEntry));
	entry->type = type;
	entry->size = size;
	mtpUtilMemcpy(entry->data, data, size);
	/* a datum of a type the context has replaces it */
	for (i = 0; i < MTP_MARLIN_NUM_CONTEXT_DATA; i++) {
		if (s_marlinContext.data[i] != NULL && s_marlinContext.data[i]->type == type) {
			mtpUtilFree(s_marlinContext.data[i]);
			s_marlinContext.data[i] = entry;
			entry = NULL;
			break;
		}
	}
	if (entry != NULL) {
		for (i = 0; i < MTP_MARLIN_NUM_CONTEXT_DATA; i++) {
			if (s_marlinContext.data[i] == NULL) {
				s_marlinContext.data[i] = entry;
				entry = NULL;
				break;
			}
		}
	}
	if (i == MTP_MARLIN_NUM_CONTEXT_DATA && entry != NULL)
		ret = SCE_MTP_ERROR_NO_MEMORY;
	else
		s_marlinContext.numData++;
end:
	if (entry != NULL)
		mtpUtilFree(entry);
	return ret;
}

extern "C" {

int mtpCmdGetMarlinLicenseInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 *params = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 contextId = operation->param[0];
	SceUInt32 transactionId = operation->transactionId;
	char path[MTP_MARLIN_LICENSE_PATH_SIZE];
	SceUInt32 *info = NULL;
	char *licenseId = NULL;
	SceUInt32 numParams = 0;
	SceSize idSize = 0;
	SceUInt32 param;
	SceIoStat stat;
	int send = 1;
	int ret = 0;
	int n;

	if (_mtpGetContextData(contextId, MTP_MARLIN_CONTEXT_LICENSE_ID, &idSize,
	    (void **)&licenseId) == 0) {
		sceClibSnprintf(path, sizeof(path), MTP_MARLIN_LICENSE_PATH, "ux0:", licenseId);
		info = (SceUInt32 *)mtpUtilMalloc(3 * sizeof(SceUInt32));
		if (info == NULL) {
			MTP_ERR(MTP_TAG_MRLN, 1469, "unable to alloc work buffer\n");
			ret = -2;
		} else {
			info[0] = 0;
			info[1] = 0;
			n = sceIoGetstat(path, &stat);
			if (n >= 0) {
				info[2] = (SceUInt32)stat.st_size;
			} else {
				MTP_ERR(MTP_TAG_MRLN, 1477, "license file not exist %s : ret=%x\n",
					path, n);
				info[2] = 0;
			}
			if (_mtpSetContextData(contextId, MTP_MARLIN_CONTEXT_LICENSE_SIZE,
			    sizeof(info[2]), &info[2]) == 0) {
				ret = _mtpMarlinSendData(port, transactionId, 3 * sizeof(SceUInt32),
					info, &send);
				if (ret == 0) {
					param = 0;
					params = &param;
					code = MTP_RESPONSE_OK;
					numParams = 1;
				}
			}
		}
	}
	mtpUtilFree(info);
	mtpUtilFree(licenseId);
	if (send == 1)
		_mtpMarlinSendData(port, transactionId, sizeof(ret), &ret, &send);
	mtpUtilSetMtpResponse(code, params, numParams, operation, response);
	return ret;
}

int mtpCmdSendMarlinLicenseInfo(const MtpOperation *operation, MtpOperation *response)
{
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	SceUInt32 contextId = operation->param[0];
	SceUInt32 *info = NULL;
	SceSize idSize = 0;
	char *licenseId = NULL;
	char path[MTP_MARLIN_LICENSE_PATH_SIZE];
	int ret;

	ret = _mtpMarlinRecvData(port, transactionId, 3 * sizeof(SceUInt32), (void **)&info);
	if (ret != 0)
		goto end;
	if (info == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 1537, "unable to process marlin data=0x%x\n", 0);
		ret = -2;
		goto end;
	}
	if (info[2] != 0) {
		if (_mtpSetContextData(contextId, MTP_MARLIN_CONTEXT_LICENSE_SIZE, sizeof(info[2]),
		    &info[2]) != 0)
			goto end;
	} else {
		/* the host has no licence for it: the one on the memory card goes */
		if (_mtpGetContextData(contextId, MTP_MARLIN_CONTEXT_LICENSE_ID, &idSize,
		    (void **)&licenseId) != 0)
			goto end;
		sceClibSnprintf(path, sizeof(path), MTP_MARLIN_LICENSE_PATH, "ux0:", licenseId);
		sceIoRemove(path);
	}
	code = MTP_RESPONSE_OK;
end:
	if (info != NULL)
		mtpUtilFree(info);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetMarlinLicense(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 *params = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 contextId = operation->param[0];
	SceUInt32 transactionId = operation->transactionId;
	char path[MTP_MARLIN_LICENSE_PATH_SIZE];
	char *licenseId = NULL;
	SceUInt32 numParams = 0;
	SceUInt8 *license = NULL;
	SceSize idSize = 0;
	SceSize fileSize;
	SceSize done;
	SceUInt32 param;
	SceIoStat stat;
	SceUID fd = -1;
	int send = 1;
	int ret = 0;
	int n;

	if (_mtpGetContextData(contextId, MTP_MARLIN_CONTEXT_LICENSE_ID, &idSize,
	    (void **)&licenseId) == 0) {
		sceClibSnprintf(path, sizeof(path), MTP_MARLIN_LICENSE_PATH, "ux0:", licenseId);
		n = sceIoGetstat(path, &stat);
		if (n < 0) {
			MTP_ERR(MTP_TAG_MRLN, 1610, "license file not exist %s : ret=%x\n", path,
				n);
			ret = -2;
		} else {
			fileSize = (SceSize)stat.st_size;
			license = (SceUInt8 *)mtpUtilMalloc(fileSize);
			if (license == NULL) {
				MTP_ERR(MTP_TAG_MRLN, 1620, "unable to alloc work buffer\n");
				ret = -2;
			} else {
				fd = sceIoOpen(path, SCE_O_RDONLY, 0);
				if (fd < 0) {
					MTP_ERR(MTP_TAG_MRLN, 1627, "unable to open file : %s\n",
						path);
					ret = fd;
				} else {
					for (done = 0; done < fileSize; )
						done += sceIoRead(fd, &license[done],
							MTP_MARLIN_LICENSE_READ_SIZE);
					sceIoClose(fd);
					fd = -1;
					ret = _mtpMarlinSendData(port, transactionId, fileSize,
						license, &send);
					if (ret == 0) {
						param = 0;
						params = &param;
						code = MTP_RESPONSE_OK;
						numParams = 1;
					}
				}
			}
		}
	}
	mtpUtilFree(license);
	mtpUtilFree(licenseId);
	if (fd != -1)
		sceIoClose(fd);
	if (send == 1)
		_mtpMarlinSendData(port, transactionId, sizeof(ret), &ret, &send);
	mtpUtilSetMtpResponse(code, params, numParams, operation, response);
	return ret;
}

int mtpCmdSendMarlinLicense(const MtpOperation *operation, MtpOperation *response)
{
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 contextId = operation->param[0];
	SceUInt32 transactionId = operation->transactionId;
	char path[MTP_MARLIN_LICENSE_PATH_SIZE];
	SceUInt32 *licenseSize = NULL;
	char *licenseId = NULL;
	void *license = NULL;
	SceSize idSize = 0;
	SceSize sizeSize = 0;
	SceSize fileSize;
	SceSize length;
	const char *p;
	char *slash;
	char *dir;
	SceUID fd = -1;
	int ret = 0;
	int n;

	if (_mtpGetContextData(contextId, MTP_MARLIN_CONTEXT_LICENSE_ID, &idSize,
	    (void **)&licenseId) == 0 &&
	    _mtpGetContextData(contextId, MTP_MARLIN_CONTEXT_LICENSE_SIZE, &sizeSize,
	    (void **)&licenseSize) == 0) {
		fileSize = *licenseSize;
		sceClibSnprintf(path, sizeof(path), MTP_MARLIN_LICENSE_PATH, "ux0:", licenseId);
		/* the licence's folders, one by one */
		length = mtpUtilStrlen(path) + 1;
		dir = (char *)mtpUtilMalloc(length);
		for (p = path; (slash = mtpUtilStrchr(p, '/')) != NULL; p = slash + 1) {
			mtpUtilMemset(dir, 0, length);
			mtpUtilMemcpy(dir, path, slash - path);
			n = sceIoMkdir(dir, 0007);
			if (n != SCE_ERROR_ERRNO_EEXIST && n < 0)
				break;
		}
		if (dir != NULL)
			mtpUtilFree(dir);
		ret = _mtpMarlinRecvData(port, transactionId, fileSize, &license);
		if (ret == 0) {
			fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0006);
			if (fd < 0) {
				ret = fd;
			} else {
				n = sceIoWrite(fd, license, fileSize);
				if (n < 0)
					ret = n;
				else
					code = MTP_RESPONSE_OK;
			}
		}
	}
	mtpUtilFree(licenseSize);
	mtpUtilFree(licenseId);
	mtpUtilFree(license);
	sceIoClose(fd);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetMarlinEventInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 *params = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	SceUInt32 numParams = 0;
	MtpMarlinContextEntry *entry;
	SceUInt8 *info;
	SceSize offset;
	SceSize size;
	int ret = 0;
	int send = 1;
	SceUInt32 param;
	int i;

	/* the context's header, then each datum: its type, its size and its bytes */
	size = offsetof(MtpMarlinContext, data);
	for (i = 0; i < MTP_MARLIN_NUM_CONTEXT_DATA; i++) {
		if (s_marlinContext.data[i] != NULL)
			size += s_marlinContext.data[i]->size + sizeof(MtpMarlinContextEntry);
	}
	info = (SceUInt8 *)mtpUtilMalloc(size);
	if (info == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 1765, "_mtpGetAllContextData failed: 0x%x\n",
			SCE_MTP_ERROR_NO_MEMORY);
		goto end;
	}
	mtpUtilMemset(info, 0, size);
	mtpUtilMemcpy(info, &s_marlinContext, offsetof(MtpMarlinContext, data));
	offset = offsetof(MtpMarlinContext, data);
	for (i = 0; i < MTP_MARLIN_NUM_CONTEXT_DATA; i++) {
		entry = s_marlinContext.data[i];
		if (entry != NULL) {
			mtpUtilMemcpy(&info[offset], &entry->type, sizeof(entry->type));
			mtpUtilMemcpy(&info[offset + 4], &s_marlinContext.data[i]->size,
				sizeof(entry->size));
			offset += sizeof(MtpMarlinContextEntry);
			mtpUtilMemcpy(&info[offset], s_marlinContext.data[i]->data,
				s_marlinContext.data[i]->size);
			offset += s_marlinContext.data[i]->size;
		}
	}
	ret = _mtpMarlinSendData(port, transactionId, size, info, &send);
	if (ret == 0) {
		param = 0;
		params = &param;
		code = MTP_RESPONSE_OK;
		numParams = 1;
	}
	if (info != NULL)
		mtpUtilFree(info);
end:
	if (send == 1)
		_mtpMarlinSendData(port, transactionId, sizeof(ret), &ret, &send);
	mtpUtilSetMtpResponse(code, params, numParams, operation, response);
	return ret;
}

int mtpCmdSetMarlinLicenseIdToContext(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 *params = NULL;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	const void *port = mtpDevGetPort();
	SceUInt32 transactionId = operation->transactionId;
	SceUInt32 numParams = 0;
	char *licenseId = NULL;
	SceUInt32 param;
	SceSize length;
	int ret;
	int n;

	ret = _mtpMarlinRecvData(port, transactionId, MTP_MARLIN_LICENSE_ID_SIZE,
		(void **)&licenseId);
	if (ret == 0) {
		length = mtpUtilStrlen(licenseId);
		mtpUtilMemset(&s_marlinContext, 0, sizeof(s_marlinContext));
		s_marlinContext.contextId = MTP_MARLIN_CONTEXT_ID_NONE;
		s_marlinContext.reserved = 0;
		s_marlinContext.eventCode = MTP_MARLIN_EVENT_NONE;
		s_marlinContext.numData = 0;
		n = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID_NONE, MTP_MARLIN_CONTEXT_LICENSE_ID,
			length, licenseId);
		if (n != 0) {
			MTP_ERR(MTP_TAG_MRLN, 1824, "_mtpSetContextData failed: 0x%x\n", n);
		} else {
			param = MTP_MARLIN_CONTEXT_ID_NONE;
			params = &param;
			code = MTP_RESPONSE_OK;
			numParams = 1;
		}
	}
	mtpUtilFree(licenseId);
	mtpUtilSetMtpResponse(code, params, numParams, operation, response);
	return ret;
}

void mtpMarlinInitialize(void)
{
	mtpUtilMemset(s_marlinMessages, 0, sizeof(s_marlinMessages));
	s_marlinStatus = 0;
	s_marlinMessage = NULL;
	s_marlinMessageSize = 0;
	s_marlinReply = NULL;
	s_marlinReplySize = 0;
	mtpUtilMemset(&s_marlinResult, 0, sizeof(s_marlinResult));
	mtpUtilMemset(&s_marlinContext, 0, sizeof(s_marlinContext));
}

int mtpMarlinCancelWaitResult(void)
{
	int ret;

	ret = sceKernelCancelEventFlag(s_marlinResult.eventFlag, 0, NULL);
	if (ret < 0)
		MTP_ERR(MTP_TAG_MRLN, 1859, "sceKernelCancelEventFlag failed: 0x%x\n", ret);
	return ret;
}

int mtpMarlinIsWaiting(void)
{
	return s_marlinResult.eventFlag != 0;
}

void mtpCmdMarlinFreeBuffers(void)
{
	if (s_marlinMessage != NULL) {
		mtpUtilFree(s_marlinMessage);
		s_marlinMessage = NULL;
	}
	if (s_marlinReply != NULL) {
		mtpUtilFree(s_marlinReply);
		s_marlinReply = NULL;
	}
}

} /* extern "C" */

/* the host's result of a type, its errors turned into SceLibMtp's */
static __attribute__((noinline)) void _mtpMarlinGetResult(SceUInt32 type, SceUInt32 *result)
{
	SceUInt32 error;
	int i;

	if (result == NULL)
		return;
	for (i = 0; i < MTP_MARLIN_NUM_RESULTS; i++) {
		if (s_marlinResult.entries[i].type != type)
			continue;
		error = s_marlinResult.entries[i].error;
		if (type == MTP_MARLIN_RESULT_ERROR) {
			if (error == 0)
				error = 0;
			else if (error >= 0x80022F00 && error <= 0x80022F7F)
				error = SCE_MTP_ERROR_MARLIN_BASE | (error & 0x7F);
			else if ((error >= 0x80022F80 && error <= 0x80022FFF) ||
				 (error >= 0x80023500 && error <= 0x800235FF) ||
				 (error >= 0x80023600 && error <= 0x800236FF) ||
				 (error >= 0x80023700 && error <= 0x800237FF) ||
				 (error >= 0x80023800 && error <= 0x800238FF))
				error = SCE_MTP_ERROR_MARLIN_SECURE | (error & 0x7F);
			else if (error >= 0x80710001 && error <= 0x807100FF)
				error = SCE_MTP_ERROR_MARLIN_DRM | (SceUInt8)error;
			else if (error >= 0x80710100 && error <= 0x80710FFF)
				error = SCE_MTP_ERROR_MARLIN_DRM_OTHER;
			else if (error >= 0x80025000 && error <= 0x800250FF)
				error = SCE_MTP_ERROR_MARLIN_NET | (SceUInt8)error;
			else if (error >= 0x80025100 && error <= 0x800252FF)
				error = SCE_MTP_ERROR_MARLIN_NET_OTHER;
			else if (error >= 0x80010600 && error <= 0x800106FF)
				error = SCE_MTP_ERROR_MARLIN_KERNEL | (SceUInt8)error;
			else
				error = SCE_MTP_ERROR_MARLIN_OTHER;
		}
		*result = error;
	}
}

/* starts a request's context with its event and context id */
#define MTP_MARLIN_CONTEXT_START(event) do {					\
	mtpUtilMemset(&s_marlinContext, 0, sizeof(s_marlinContext));		\
	s_marlinContext.reserved = 0;						\
	s_marlinContext.numData = 0;						\
	s_marlinContext.contextId = MTP_MARLIN_CONTEXT_ID(event);		\
	s_marlinContext.eventCode = (event);					\
} while (0)

/* tells the host about a request and waits for its results: 0, or the error (also in ret) */
static inline __attribute__((always_inline)) int _mtpMarlinSendRequest(SceUInt16 code,
	int line, const char *func, int ifEvent, int *ret)
{
	SceUInt32 outBits;
	SceUInt32 timeout;
	MtpEvent event;
	int err;
	int i;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = MTP_MARLIN_CONTEXT_ID(code);
	event.code = code;
	event.numParams = 1;
	for (i = 0; i < MTP_MARLIN_NUM_RESULTS; i++) {
		s_marlinResult.entries[i].type = 0;
		s_marlinResult.entries[i].error = 0xFFFFFFFF;
	}
	err = sceKernelCreateEventFlag(MTP_MARLIN_EVF_NAME, SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (err > 0) {
		s_marlinResult.eventFlag = err;
		err = 0;
	}
	*ret = err;
	if (err != 0)
		return err;
	err = mtpCoreSendResponse(&event, MTP_MARLIN_CONTEXT_ID(code));
	*ret = err;
	if (err < 0) {
		if (ifEvent)
			sceClibPrintf("[MTP %s%s(%d)err] MtpIfSendEvent failed: 0x%x\n",
				MTP_TAG_MRLN, func, line, err);
		else
			sceClibPrintf("[MTP %s%s(%d)err] mtpCoreSendEvent failed: 0x%x\n",
				MTP_TAG_MRLN, func, line, err);
		return err;
	}
	outBits = 0;
	timeout = MTP_MARLIN_RESULT_TIMEOUT;
	do {
		err = sceKernelWaitEventFlag(s_marlinResult.eventFlag, MTP_MARLIN_EVF_RESULT,
			SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &outBits,
			&timeout);
	} while (err >= 0 && (outBits & MTP_MARLIN_EVF_RESULT) == 0);
	*ret = err;
	return err;
}

/* ends a request: its event flag and its context go */
static inline __attribute__((always_inline)) void _mtpMarlinEndRequest(void)
{
	int i;

	sceKernelDeleteEventFlag(s_marlinResult.eventFlag);
	s_marlinResult.eventFlag = 0;
	for (i = 0; i < MTP_MARLIN_NUM_CONTEXT_DATA; i++) {
		if (s_marlinContext.data[i] != NULL) {
			mtpUtilFree(s_marlinContext.data[i]);
			s_marlinContext.data[i] = NULL;
		}
	}
	s_marlinContext.numData = 0;
}

extern "C" {

int mtpExecMarlinActivation(int reserved, SceUInt32 arg, SceUInt32 *result2, SceUInt32 *result3)
{
	int ret = 0;

	(void)reserved;
	if (result2 == NULL || result3 == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 1890, "invalid parameter\n");
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	MTP_MARLIN_CONTEXT_START(MTP_EVENT_MARLIN_ACTIVATION);
	ret = 0;
	ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_ACTIVATION),
		MTP_MARLIN_CONTEXT_ARG, sizeof(arg), &arg);
	if (ret != 0)
		goto end;
	if (_mtpMarlinSendRequest(MTP_EVENT_MARLIN_ACTIVATION, 1910, "mtpExecMarlinActivation",
		0, &ret) != 0)
		goto end;
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ACTIVATION_2, result2);
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ACTIVATION_3, result3);
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ERROR, (SceUInt32 *)&ret);
	if (arg != 1 && ret == SCE_MTP_ERROR_MARLIN_NOT_ACTIVATED)
		ret = SCE_MTP_ERROR_MARLIN_ALREADY_ACTIVATED;
end:
	_mtpMarlinEndRequest();
	return ret;
}

int mtpExecMarlinDeactivation(int reserved, SceUInt32 arg, SceUInt32 *result2,
	SceUInt32 *result3)
{
	int ret = 0;

	(void)reserved;
	if (result2 == NULL || result3 == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 1940, "invalid parameter\n");
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	MTP_MARLIN_CONTEXT_START(MTP_EVENT_MARLIN_DEACTIVATION);
	ret = 0;
	ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_DEACTIVATION),
		MTP_MARLIN_CONTEXT_ARG, sizeof(arg), &arg);
	if (ret != 0)
		goto end;
	if (_mtpMarlinSendRequest(MTP_EVENT_MARLIN_DEACTIVATION, 1960, "mtpExecMarlinDeactivation",
		0, &ret) != 0)
		goto end;
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ACTIVATION_2, result2);
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ACTIVATION_3, result3);
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ERROR, (SceUInt32 *)&ret);
end:
	_mtpMarlinEndRequest();
	return ret;
}

int mtpExecMarlinLicenseMove(int reserved, const char *licenseId, SceSize length,
	SceUInt32 arg, int unused)
{
	int ret = 0;

	(void)reserved;
	(void)unused;
	if (licenseId == NULL || length == 0 || length > MTP_MARLIN_LICENSE_ID_SIZE) {
		MTP_ERR(MTP_TAG_MRLN, 1986, "invalid parameter\n");
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	MTP_MARLIN_CONTEXT_START(MTP_EVENT_MARLIN_LICENSE_MOVE);
	ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_LICENSE_MOVE),
		MTP_MARLIN_CONTEXT_LICENSE_ID, length, licenseId);
	if (ret == 0)
		ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_LICENSE_MOVE),
			MTP_MARLIN_CONTEXT_LICENSE_ARG, sizeof(arg), &arg);
	if (ret != 0)
		goto end;
	if (_mtpMarlinSendRequest(MTP_EVENT_MARLIN_LICENSE_MOVE, 2007, "mtpExecMarlinLicenseMove",
		1, &ret) != 0)
		goto end;
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ERROR, (SceUInt32 *)&ret);
end:
	_mtpMarlinEndRequest();
	return ret;
}

int mtpExecMarlinLicenseCopy(int reserved, const char *licenseId, SceSize length,
	SceUInt32 arg, int unused)
{
	int ret = 0;

	(void)reserved;
	(void)unused;
	if (licenseId == NULL || length == 0 || length > MTP_MARLIN_LICENSE_ID_SIZE) {
		MTP_ERR(MTP_TAG_MRLN, 2030, "invalid parameter\n");
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	MTP_MARLIN_CONTEXT_START(MTP_EVENT_MARLIN_LICENSE_COPY);
	ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_LICENSE_COPY),
		MTP_MARLIN_CONTEXT_LICENSE_ID, length, licenseId);
	if (ret == 0)
		ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_LICENSE_COPY),
			MTP_MARLIN_CONTEXT_LICENSE_ARG, sizeof(arg), &arg);
	if (ret != 0)
		goto end;
	if (_mtpMarlinSendRequest(MTP_EVENT_MARLIN_LICENSE_COPY, 2051, "mtpExecMarlinLicenseCopy",
		1, &ret) != 0)
		goto end;
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ERROR, (SceUInt32 *)&ret);
end:
	_mtpMarlinEndRequest();
	return ret;
}

int mtpExecMarlinLicenseCheck(int reserved, const char *licenseId, SceSize length,
	SceUInt32 arg, SceUInt32 *state)
{
	int ret = 0;

	(void)reserved;
	if (licenseId == NULL || length == 0 || length > MTP_MARLIN_LICENSE_ID_SIZE ||
	    state == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 2075, "invalid parameter\n");
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	MTP_MARLIN_CONTEXT_START(MTP_EVENT_MARLIN_LICENSE_CHECK);
	ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_LICENSE_CHECK),
		MTP_MARLIN_CONTEXT_LICENSE_ID, length, licenseId);
	if (ret == 0)
		ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_LICENSE_CHECK),
			MTP_MARLIN_CONTEXT_LICENSE_ARG, sizeof(arg), &arg);
	if (ret != 0)
		goto end;
	if (_mtpMarlinSendRequest(MTP_EVENT_MARLIN_LICENSE_CHECK, 2096, "mtpExecMarlinLicenseCheck",
		1, &ret) != 0)
		goto end;
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ERROR, (SceUInt32 *)&ret);
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_LICENSE_STATE, state);
	if (ret == SCE_MTP_ERROR_MARLIN_LICENSE_STATE) {
		switch (*state) {
		case 1:
			ret = SCE_MTP_ERROR_MARLIN_LICENSE_EXPIRED;
			break;
		case 2:
			ret = SCE_MTP_ERROR_MARLIN_LICENSE_NOT_YET;
			break;
		case 3:
			ret = SCE_MTP_ERROR_MARLIN_LICENSE_COUNT;
			break;
		}
	}
end:
	_mtpMarlinEndRequest();
	return ret;
}

int mtpExecMarlinSharedInfoAcq(int reserved, const char *licenseId, SceSize length,
	SceUInt32 *info)
{
	int ret = 0;

	(void)reserved;
	if (licenseId == NULL || length == 0 || length > MTP_MARLIN_LICENSE_ID_SIZE ||
	    info == NULL) {
		MTP_ERR(MTP_TAG_MRLN, 2138, "invalid parameter\n");
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	MTP_MARLIN_CONTEXT_START(MTP_EVENT_MARLIN_SHARED_INFO_ACQ);
	ret = _mtpSetContextData(MTP_MARLIN_CONTEXT_ID(MTP_EVENT_MARLIN_SHARED_INFO_ACQ),
		MTP_MARLIN_CONTEXT_LICENSE_ID, length, licenseId);
	if (ret != 0)
		goto end;
	if (_mtpMarlinSendRequest(MTP_EVENT_MARLIN_SHARED_INFO_ACQ, 2158,
		"mtpExecMarlinSharedInfoAcq", 1, &ret) != 0)
		goto end;
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_ERROR, (SceUInt32 *)&ret);
	_mtpMarlinGetResult(MTP_MARLIN_RESULT_SHARED_INFO, info);
end:
	_mtpMarlinEndRequest();
	return ret;
}

} /* extern "C" */
