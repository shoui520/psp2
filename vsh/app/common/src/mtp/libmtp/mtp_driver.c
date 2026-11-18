/*
 * The MTP driver ("DRIV:"): data phases over SceMtpIf, in the pieces the interface takes,
 * error responses with their data, and the error history for USB errors.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <error/error.h>
#include "mtp_internal.h"

/* the most a USB data phase moves at once */
#define MTP_DRIVER_USB_CHUNK		0x10000

#define MTP_DRIVER_SEND_EVENT_RETRY	99
#define MTP_DRIVER_SEND_EVENT_DELAY	100000		/* us */

/* the last error posted to the error history, so that it is posted once */
static int s_lastError = 0;

int MtpIfSendData(const void *port, SceUInt32 transactionId, const void *data, SceSize size,
	SceUInt64 totalSize)
{
	MtpIfTransfer transfer;
	int ret;

	transfer.transactionId = transactionId;
	transfer.buffer = (void *)data;
	transfer.size = size;
	transfer.totalSize = totalSize;
	transfer.transferred = 0;
	ret = MtpIfSendDataWithParam(port, &transfer);
	if (ret >= 0)
		ret = (int)transfer.transferred;
	return ret;
}

int MtpIfRecvData(const void *port, SceUInt32 transactionId, void *data, SceSize size,
	SceUInt64 *totalSize)
{
	MtpIfTransfer transfer;
	int ret;

	transfer.transactionId = transactionId;
	transfer.buffer = data;
	transfer.size = size;
	transfer.totalSize = 0;
	transfer.transferred = 0;
	ret = MtpIfRecvDataWithParam(port, &transfer);
	if (ret >= 0) {
		if (totalSize != NULL)
			*totalSize = transfer.totalSize;
		ret = (int)transfer.transferred;
	}
	return ret;
}

int mtpDriverSendData(const void *port, int ifType, SceUInt32 transactionId, const void *data,
	SceSize size, SceUInt64 totalSize)
{
	const char *p = data;
	SceSize left = size;
	SceSize chunk;
	int ret;

	while (left != 0) {
		chunk = left;
		if (ifType == MTP_IF_TYPE_USB && chunk > MTP_DRIVER_USB_CHUNK)
			chunk = MTP_DRIVER_USB_CHUNK;
		ret = MtpIfSendData(port, transactionId, p, chunk, totalSize);
		if (ret < 0)
			return ret;
		if (chunk < ret) {
			MTP_ERR("DRIV:", 88, "MtpIfSendData returned bigger size %d than %d\n", ret,
				chunk);
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		if (ret == 0 && chunk != 0) {
			MTP_ERR("DRIV:", 93, "MtpIfSendData couldn't send any data %u\n", chunk);
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		left -= ret;
		p += ret;
	}
	return 0;
}

int mtpDriverRecvData(const void *port, int ifType, SceUInt32 transactionId, void *data,
	SceSize size, SceUInt64 *totalSize)
{
	char *p = data;
	SceSize left = size;
	SceSize chunk;
	int received = 0;
	int ret;

	if (size == 0)
		return received;
	for (;;) {
		chunk = left;
		if (ifType == MTP_IF_TYPE_USB && chunk > MTP_DRIVER_USB_CHUNK)
			chunk = MTP_DRIVER_USB_CHUNK;
		ret = MtpIfRecvData(port, transactionId, p, chunk, totalSize);
		if (ret < 0)
			break;
		if (ret == 0 && chunk != 0)
			return received;
		/* a short piece ends the phase */
		if (ret < chunk)
			left = ret;
		if (chunk < ret) {
			MTP_ERR("DRIV:", 130, "MtpIfRecvData returned bigger size %d than %d\n", ret,
				chunk);
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		left -= ret;
		p += ret;
		received += ret;
		if (left == 0)
			return received;
	}
	if (ifType == MTP_IF_TYPE_NET && ret == MTP_ERROR_DISCONNECTED)
		return received;
	return ret;
}

int mtpDriverSendEvent(const void *port, const MtpOperation *command, const void *event)
{
	int retry = MTP_DRIVER_SEND_EVENT_RETRY;
	int ret;

	do {
		ret = MtpIfSendEvent(port, command, event);
		if (ret != MTP_ERROR_CANCELLED)
			break;
		sceKernelDelayThread(MTP_DRIVER_SEND_EVENT_DELAY);
	} while (retry-- != 0);
	return ret;
}

int mtpDriverHandleErrorResponse(const void *port, const MtpOperation *operation,
	MtpOperation *response, SceUInt32 value, int code, int send)
{
	int ret = 0;

	if (send) {
		ret = MtpIfSendData(port, operation->transactionId, &value, sizeof(value),
			sizeof(value));
		if (ret < 0)
			MTP_ERR("DRIV:", 162, "MtpIfSendData failed: 0x%x\n", ret);
	}
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	if (ret >= 0)
		ret = value;
	return ret;
}

void mtpDriverClearLastError(void)
{
	s_lastError = 0;
}

/* post a USB error to the error history, once; the errors of a cancelled wait are not */
int mtpDriverPostError(int error)
{
	ErrorRecord record;
	int posted = 0;

	switch (error) {
	case 0:
	case (int)0x8024300A:
	case (int)0x8024300C:
	case (int)0x8024300D:
		break;
	default:
		if (error == s_lastError)
			break;
		sceClibMemset(&record, 0, sizeof(record));
		record.code = error;
		record.id = posted;
		sceErrorHistoryPostError(&record);
		s_lastError = error;
		posted = 1;
		break;
	}
	return posted;
}
