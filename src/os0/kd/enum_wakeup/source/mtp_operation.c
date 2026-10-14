/* SceEnumWakeUp: The MTP operations the responder answers. */
#include "ew_internal.h"

int mtpGetDeviceInfo(MtpContext *ctx)
{
	const MtpDeviceInfo *info;
	MtpDeviceInfoHead *head;
	MtpString str;
	SceUInt32 zero = 0;
	char *buf;
	const void *port = ctx->port;
	int off, n, ret;

	buf = mtpAlloc(MTP_DEVICE_INFO_MAX);
	if (buf == NULL)
		goto no_data;
	info = mtpDeviceInfo();
	head = (MtpDeviceInfoHead *)buf;
	head->standardVersion = info->standardVersion;
	head->vendorExtensionId = info->vendorExtensionId;
	head->vendorExtensionVersion = info->vendorExtensionVersion;

	str.buf = buf + sizeof(*head) + 1;
	str.length = 0xff;
	n = mtpPackString(&str, info->vendorExtensionDesc);
	if (n < 0)
		goto fail;
	buf[sizeof(*head)] = str.length;
	*(MtpUInt16 *)(buf + n + 9) = info->functionalMode;
	off = n + 0xb;
	if ((n = mtpPackArray16(buf + off, &info->operations)) < 0)
		goto fail;
	off += n;
	if ((n = mtpPackArray16(buf + off, &info->events)) < 0)
		goto fail;
	off += n;
	if ((n = mtpPackArray16(buf + off, &info->deviceProperties)) < 0)
		goto fail;
	off += n;
	if ((n = mtpPackArray16(buf + off, &info->captureFormats)) < 0)
		goto fail;
	off += n;
	if ((n = mtpPackArray16(buf + off, &info->playbackFormats)) < 0)
		goto fail;
	off += n;

	str.buf = buf + off + 1;
	str.length = 0xff;
	if ((n = mtpPackString(&str, info->manufacturer)) < 0)
		goto fail;
	buf[off] = str.length;
	off += n + 1;
	str.buf = buf + off + 1;
	str.length = 0xff;
	if ((n = mtpPackString(&str, info->model)) < 0)
		goto fail;
	buf[off] = str.length;
	off += n + 1;
	str.buf = buf + off + 1;
	str.length = 0xff;
	if ((n = mtpPackString(&str, info->deviceVersion)) < 0)
		goto fail;
	buf[off] = str.length;
	off += n + 1;
	str.buf = buf + off + 1;
	str.length = 0xff;
	if ((n = mtpPackString(&str, info->serialNumber)) < 0)
		goto fail;
	buf[off] = str.length;
	off += n + 1;

	ret = SceMtpIfForDriver_EDC3AB26(port, buf, off, off);
	if (ret >= 0) {
		mtpFree(buf);
		return ret;
	}
	ret = -1;
	mtpFree(buf);
	goto respond;

fail:
	mtpFree(buf);
no_data:
	ret = SceMtpIfForDriver_EDC3AB26(port, &zero, 4, 4);
	if (ret < 0)
		ret = -1;
respond:
	mtpSetResponse(PTP_RC_GENERAL_ERROR, NULL, 0, &ctx->request, &ctx->response);
	return ret;
}

int mtpOpenSession(MtpContext *ctx)
{
	int open = mtpIsSessionOpen(ctx);

	if (open == 1)
		mtpSetResponse(PTP_RC_SESSION_ALREADY_OPEN, NULL, 0, &ctx->request, &ctx->response);
	else
		ctx->sessionId = ctx->request.params[0];
	return open == 1;
}

int mtpCloseSession(MtpContext *ctx)
{
	if (mtpIsSessionOpen(ctx)) {
		ctx->sessionId = 0;
		return 0;
	}
	mtpSetResponse(PTP_RC_SESSION_NOT_OPEN, NULL, 0, &ctx->request, &ctx->response);
	return 1;
}

int mtpGetDevicePropDesc(MtpContext *ctx)
{
	SceUInt16 code = ctx->request.params[0];
	SceSize size = code == MTP_DPC_DEVICE_ICON ? 0x10000 : 0x4000;
	SceUInt32 zero = 0;
	SceUInt16 rc;
	char *buf;
	const void *port = ctx->port;
	int n, ret;

	buf = mtpAlloc(size);
	if (buf == NULL) {
		rc = PTP_RC_GENERAL_ERROR;
	} else {
		n = mtpBuildPropDesc(code, buf, size);
		if (n < 0) {
			rc = 0x2006;		/* ParameterNotSupported */
		} else {
			ret = SceMtpIfForDriver_EDC3AB26(port, buf, n, n);
			if (ret >= 0) {
				mtpFree(buf);
				return ret;
			}
			rc = PTP_RC_GENERAL_ERROR;
		}
		mtpFree(buf);
	}
	ret = SceMtpIfForDriver_EDC3AB26(port, &zero, 4, 4);
	if (ret < 0)
		ret = -1;
	mtpSetResponse(rc, NULL, 0, &ctx->request, &ctx->response);
	return ret;
}
