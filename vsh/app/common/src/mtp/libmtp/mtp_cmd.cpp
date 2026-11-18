/*
 * The device's own operations ("CMD :"): DeviceInfo, the session, the device properties,
 * and the vendor operations a content manager host starts a connection with (the
 * console's information and capabilities, its own information and status, the results of
 * the requests it ran, and keep-alive).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <regmgr/regmgr.h>
#include "mtp_internal.h"

#define MTP_DEVICE_INFO_SIZE		0x2000
#define MTP_VITA_INFORMATION_SIZE	0x1000
#define MTP_CMD_RECV_SIZE		0x10000

/* the host status SendHostStatus reports (the names are ours) */
#define MTP_HOST_STATUS_CONNECTED	0
#define MTP_HOST_STATUS_LOCKED		2
#define MTP_HOST_STATUS_BUSY		3
#define MTP_HOST_STATUS_ERROR		4
#define MTP_HOST_STATUS_UNSUPPORTED	6
#define MTP_HOST_STATUS_CONNECTED_EX	0x10005


extern "C" {

int mtpCmdGetDeviceInfo(const MtpOperation *operation, MtpOperation *response)
{
	const MtpDeviceInfo *info;
	SceUInt8 *buf;
	MtpString string;
	int send = 1;
	int ret;
	int n;
	int off;
	int n1;
	int n2;
	int n3;

	buf = (SceUInt8 *)mtpUtilMalloc(MTP_DEVICE_INFO_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 58, "unable to alloc memory for send data\n");
		ret = -2;
		goto error;
	}
	info = mtpSpecGetDeviceInfo();
	mtpUtilMemcpy(buf, &info->standardVersion, 2);
	mtpUtilMemcpy(buf + 2, &info->vendorExtensionId, 4);
	mtpUtilMemcpy(buf + 6, &info->vendorExtensionVersion, 2);
	string.numChars = 0xFF;
	string.chars = buf + 9;
	n = mtpUtilPackMtpString(&string, info->vendorExtensionDesc);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 80, "unable to pack MTP Extensions=0x%x", n);
		ret = -2;
		goto free;
	}
	buf[8] = string.numChars;
	mtpUtilMemcpy(buf + n + 9, &info->functionalMode, 2);
	off = n + 11;
	n = mtpUtilPackCodeList(buf + off, &info->operations);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 93, "unable to pack Operations Supported=0x%x", n);
		ret = -2;
		goto free;
	}
	off += n;
	n = mtpUtilPackCodeList(buf + off, &info->events);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 101, "unable to pack Events Supported=0x%x", n);
		ret = -2;
		goto free;
	}
	off += n;
	n = mtpUtilPackCodeList(buf + off, &info->deviceProperties);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 109, "unable to pack Device Properties Supported=0x%x", n);
		ret = -2;
		goto free;
	}
	off += n;
	n = mtpUtilPackCodeList(buf + off, &info->captureFormats);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 117, "unable to pack Capture Formats=0x%x", n);
		ret = -2;
		goto free;
	}
	off += n;
	n = mtpUtilPackCodeList(buf + off, &info->playbackFormats);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 125, "unable to pack Playback Formats=0x%x", n);
		ret = -2;
		goto free;
	}
	off += n;
	string.numChars = 0xFF;
	string.chars = buf + off + 1;
	n1 = mtpUtilPackMtpString(&string, info->manufacturer);
	if (n1 < 0) {
		MTP_ERR(MTP_TAG_CMD, 135, "unable to pack Manufacturer=0x%x", n1);
		ret = -2;
		goto free;
	}
	buf[off] = string.numChars;
	off += n1 + 1;
	string.numChars = 0xFF;
	string.chars = buf + off + 1;
	n2 = mtpUtilPackMtpString(&string, info->model);
	if (n2 < 0) {
		MTP_ERR(MTP_TAG_CMD, 147, "unable to pack Model=0x%x", n2);
		ret = -2;
		goto free;
	}
	buf[off] = string.numChars;
	off += n2 + 1;
	string.numChars = 0xFF;
	string.chars = buf + off + 1;
	n3 = mtpUtilPackMtpString(&string, info->deviceVersion);
	if (n3 < 0) {
		MTP_ERR(MTP_TAG_CMD, 159, "unable to pack Device Version=0x%x", n3);
		ret = -2;
		goto free;
	}
	buf[off] = string.numChars;
	off += n3 + 1;
	string.numChars = 0xFF;
	string.chars = buf + off + 1;
	n = mtpUtilPackMtpString(&string, info->serialNumber);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 171, "unable to pack Serial Number=0x%x", n);
		ret = -2;
		goto free;
	}
	buf[off] = string.numChars;
	ret = mtpCoreSendData(buf, off + n + 1, off + n + 1);
	if (ret >= 0) {
		mtpUtilFree(buf);
		return ret;
	}
	MTP_ERR(MTP_TAG_CMD, 182, "unable to send data=0x%x\n", ret);
	send = 0;
free:
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret,
					       MTP_RESPONSE_GENERAL_ERROR, send);
}

int mtpCmdOpenSession(const MtpOperation *operation, MtpOperation *response)
{
	int ret = 0;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_CMD, 212, "wrong param\n");
		mtpUtilSetMtpResponse(MTP_RESPONSE_INVALID_PARAMETER, NULL, 0, operation,
				      response);
		ret = -2;
	} else if (mtpCoreHasPendingOperation() == 1) {
		mtpUtilSetMtpResponse(MTP_RESPONSE_SESSION_ALREADY_OPEN, NULL, 0, operation,
				      response);
		ret = -2;
	} else {
		mtpCoreSetPendingOperation(operation->param[0]);
		mtpStatusSetState(2, -1, 0);
	}
	return ret;
}

int mtpCmdCloseSession(const MtpOperation *operation, MtpOperation *response)
{
	int session = mtpCoreHasPendingOperation();

	if (!session) {
		mtpUtilSetMtpResponse(MTP_RESPONSE_SESSION_NOT_OPEN, NULL, 0, operation, response);
		return -2;
	}
	mtpContextCancelAll();
	if (mtpDevGetIfType() == MTP_IF_TYPE_NET &&
	    mtpStatusGetPlatformClass() == MTP_PLATFORM_CLASS_PS && mtpStatusGetState() == 4)
		mtpStatusSetState(2, 14, 0);
	else
		mtpStatusSetState(0, -1, 0);
	mtpCoreSetPendingOperation(0);
	return 0;
}

int mtpCmdGetDevicePropDesc(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 code = operation->param[0];
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	SceSize size = MTP_DEVICE_PROP_DESC_SIZE;
	void *buf;
	int ret;
	int n;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_CMD, 299, "wrong param\n");
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	if ((SceUInt16)code == MTP_DEVICE_PROP_DEVICE_ICON)
		size = MTP_DEVICE_PROP_DESC_SIZE_LARGE;
	buf = mtpUtilMalloc(size);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 313, "unable to alloc memory for device property\n");
		ret = -2;
		goto error;
	}
	n = mtpPropPackDevicePropDesc((SceUInt16)code, buf, size);
	if (n < 0) {
		MTP_ERR(MTP_TAG_CMD, 321, "device property is not supported\n");
		responseCode = MTP_RESPONSE_PARAMETER_NOT_SUPPORTED;
		ret = -2;
	} else {
		ret = mtpCoreSendData(buf, n, n);
		if (ret >= 0) {
			mtpUtilFree(buf);
			return ret;
		}
		MTP_ERR(MTP_TAG_CMD, 329, "unable to send data=0x%x\n", ret);
	}
	mtpUtilFree(buf);
error:
	return mtpDevDriverHandleErrorResponse(operation, response, ret, responseCode, 0);
}

int mtpCmdGetVitaInformation(const MtpOperation *operation, MtpOperation *response)
{
	int length = 0;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	char *buf;
	int ret;
	int n;

	buf = (char *)mtpUtilMalloc(MTP_VITA_INFORMATION_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 363, "no memory\n");
		ret = -2;
		responseCode = MTP_RESPONSE_VENDOR_NO_MEMORY;
		goto error;
	}
	n = mtpSpecGetVitaInformation(buf, MTP_VITA_INFORMATION_SIZE,
				      mtpDevGetIfType() != MTP_IF_TYPE_NET);
	if (n < 0) {
		ret = -2;
		goto error;
	}
	length = n + 1;
	ret = mtpCoreSendData(&length, sizeof(length), sizeof(length) + length);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 384, "sceMtpIfSendData failed: 0x%x\n", ret);
		goto error;
	}
	ret = mtpCoreSendData(buf, length, sizeof(length) + length);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 389, "sceMtpIfSendData failed: 0x%x\n", ret);
		goto error;
	}
	mtpUtilFree(buf);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (buf != NULL)
		mtpUtilFree(buf);
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return ret;
}

int mtpCmdGetCapabilityInfo(const MtpOperation *operation, MtpOperation *response)
{
	int length = 0;
	const char *xml = NULL;
	int ret;

	mtpSpecGetCapabilityInfo(&xml, &length);
	ret = mtpCoreSendData(&length, sizeof(length), sizeof(length) + length);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 427, "sceMtpIfSendData failed: 0x%x\n", ret);
	} else {
		ret = mtpCoreSendData(xml, length, sizeof(length) + length);
		if (ret >= 0) {
			mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
			return 0;
		}
		MTP_ERR(MTP_TAG_CMD, 432, "sceMtpIfSendData failed: 0x%x\n", ret);
	}
	mtpUtilSetMtpResponse(MTP_RESPONSE_GENERAL_ERROR, NULL, 0, operation, response);
	return ret;
}

int mtpCmdSendCapabilityInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 remaining = 0;
	CapabilityInfoParser parser;
	SceUInt32 result;
	char *buf = NULL;
	SceUInt32 size;
	int ret;
	int res;

	ret = mtpCoreRecvData(&remaining, sizeof(remaining), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 464, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	if (parser.Initialize() < 0) {
		ret = -2;
		goto error;
	}
	buf = (char *)mtpUtilMalloc(MTP_CMD_RECV_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 478, "no memory\n");
		ret = -2;
		goto free;
	}
	while (remaining != 0) {
		size = MTP_CMD_RECV_SIZE;
		if (remaining <= MTP_CMD_RECV_SIZE)
			size = remaining;
		ret = mtpCoreRecvData(buf, size, NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 492, "sceMtpIfRecvData failed: 0x%x\n", ret);
			goto free;
		}
		res = parser.Parse(buf, ret, (SceUInt32)ret == remaining);
		if (res < 0) {
			MTP_ERR(MTP_TAG_CMD, 501, "Parser analyze failed: 0x%x\n", res);
			ret = -2;
			goto free;
		}
		remaining -= ret;
	}
	parser.Terminate();
	mtpUtilFree(buf);
	result = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &result, 1, operation, response);
	return 0;
free:
	if (buf != NULL)
		mtpUtilFree(buf);
error:
	mtpUtilSetMtpResponse(MTP_RESPONSE_GENERAL_ERROR, NULL, 0, operation, response);
	return ret;
}

int mtpCmdReportResult(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 resultParam = 0;
	SceUInt32 result = operation->param[1];
	MtpContext *context = NULL;
	int responseCode = MTP_RESPONSE_GENERAL_ERROR;
	int ret;

	if (operation->numParams < 2) {
		MTP_ERR(MTP_TAG_CMD, 545, "wrong param %u\n", operation->numParams);
		responseCode = MTP_RESPONSE_INVALID_PARAMETER;
		goto error;
	}
	if (operation->numParams >= 3)
		resultParam = operation->param[2];
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 556,
			"GetContext %u failed. ret: 0x%x, reporting result: 0x%x \n", contextId, ret,
			result);
		responseCode = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		goto error;
	}
	context->result = result;
	context->resultParam = resultParam;
	if (mtpContextSetDone(context) >= 0) {
		mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
		return 0;
	}
error:
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return -2;
}

int mtpCmdSendInitiatorInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 remaining = 0;
	InitiatorInfoParser parser;
	SceUInt32 result;
	char *buf;
	SceUInt32 size;
	int ret;
	int res;

	ret = mtpCoreRecvData(&remaining, sizeof(remaining), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 601, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	if (parser.Initialize() < 0) {
		ret = -2;
		goto error;
	}
	buf = (char *)mtpUtilMalloc(MTP_CMD_RECV_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_CMD, 615, "no memory\n");
		ret = -2;
		goto free;
	}
	while (remaining != 0) {
		size = MTP_CMD_RECV_SIZE;
		if (remaining <= MTP_CMD_RECV_SIZE)
			size = remaining;
		ret = mtpCoreRecvData(buf, size, NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CMD, 629, "sceMtpIfRecvData failed: 0x%x\n", ret);
			goto free;
		}
		res = parser.Parse(buf, ret, (SceUInt32)ret == remaining);
		if (res < 0) {
			MTP_ERR(MTP_TAG_CMD, 638, "Parser analyze failed: 0x%x\n", res);
			ret = -2;
			goto free;
		}
		remaining -= ret;
	}
	mtpUtilFree(buf);
	parser.Terminate();
	if (mtpStatusGetPlatformClass() == MTP_PLATFORM_CLASS_PS)
		mtpStatusSetState(5, -1, 0);
	result = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &result, 1, operation, response);
	return 0;
free:
	if (buf != NULL)
		mtpUtilFree(buf);
error:
	parser.Terminate();
	mtpUtilSetMtpResponse(MTP_RESPONSE_GENERAL_ERROR, NULL, 0, operation, response);
	return ret;
}

int mtpCmdSendHostStatus(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 status = operation->param[0];
	int responseCode = MTP_RESPONSE_OK;
	int state;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_CMD, 684, "wrong param\n");
		mtpUtilSetMtpResponse(MTP_RESPONSE_INVALID_PARAMETER, NULL, 0, operation,
				      response);
		return -2;
	}
	switch (status) {
	case MTP_HOST_STATUS_CONNECTED:
	case MTP_HOST_STATUS_CONNECTED_EX:
		if (mtpStatusGetPlatformClass() == MTP_PLATFORM_CLASS_PS &&
		    sceRegMgrSrvGetRegion() == MTP_REGION_CHINA) {
			mtpStatusSetState(0, -1, 1);
			break;
		}
		mtpStatusSetFlags(status == MTP_HOST_STATUS_CONNECTED_EX, 1);
		state = 3;
		if (mtpStatusIsSupportedHost())
			state = 4;
		mtpStatusSetState(state, -1, 0);
		break;
	case MTP_HOST_STATUS_LOCKED:
		mtpStatusSetFlags(0, 1);
		mtpStatusSetState(5, -1, 1);
		break;
	case MTP_HOST_STATUS_BUSY:
		mtpDevGetIfType();
		mtpStatusSetState(2, 8, 1);
		break;
	case MTP_HOST_STATUS_ERROR:
		mtpStatusSetState(6, -1, 1);
		break;
	case MTP_HOST_STATUS_UNSUPPORTED:
		if (mtpDevGetIfType() != MTP_IF_TYPE_NET)
			mtpStatusSetState(2, -1, 1);
		else
			mtpStatusSetState(2, 9, 1);
		break;
	default:
		MTP_ERR(MTP_TAG_CMD, 749, "Unknown status: %u\n", operation->param[0]);
		responseCode = MTP_RESPONSE_VENDOR_UNKNOWN_STATUS;
		break;
	}
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return 0;
}

int mtpCmdKeepAlive(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	int responseCode = MTP_RESPONSE_OK;
	int ret;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_CMD, 777, "wrong param\n");
		mtpUtilSetMtpResponse(MTP_RESPONSE_INVALID_PARAMETER, NULL, 0, operation,
				      response);
		return -2;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CMD, 784, "GetContext %u failed: 0x%x\n", contextId, ret);
		responseCode = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
	} else {
		mtpContextPutContext(context, 1);
		ret = 0;
	}
	mtpUtilSetMtpResponse(responseCode, NULL, 0, operation, response);
	return ret;
}

}
