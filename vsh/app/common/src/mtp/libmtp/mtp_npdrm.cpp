/*
 * SceLibMtp: NpDrm requests (NDRM).
 *
 * The shell's NpDrm requests (the RTC, activation and licences of a PlayStation account)
 * go to the host as events; the host fetches each request's data and sends back its
 * results with GetChallenge, SendRtc, SendActivation and SendLicense.
 */

#include "mtp_internal.h"

#define MTP_TAG_NDRM	"NDRM:"

/* the sizes of the requests */
#define MTP_NPDRM_RTC_REQUEST_SIZE		48
#define MTP_NPDRM_RTC_REPLY_SIZE		0x130
#define MTP_NPDRM_AUTH_REQUEST_SIZE		128
#define MTP_NPDRM_ACTIVATION_REPLY_SIZE		0x1090
#define MTP_NPDRM_LICENSE_ID_SIZE		128
#define MTP_NPDRM_LICENSE_MAX			0x200
#define MTP_NPDRM_RTC_RECV_SIZE			0x1000

/* the request kind a challenge is asked for */
#define MTP_NPDRM_CHALLENGE_PCACT		2

/* one block of a request: its size, then its bytes */
typedef struct MtpNpDrmBlock {
	SceUInt32 size;
	SceUInt32 reserved;
	SceUInt8 data[];
} MtpNpDrmBlock;

/* an NpDrm request, the data of its context (28 bytes) */
typedef struct MtpNpDrmBase {
	MtpNpDrmBlock *request;		/* what the host fetches with GetChallenge */
	SceSize requestSize;
	void *reply;			/* what the host sends back */
	SceSize replySize;
	SceUInt32 result[3];		/* the host's results */
} MtpNpDrmBase;

/* a licence request: two blocks, the licence id, then the RTC */
typedef struct MtpNpDrmLicenseRequest {
	SceUInt32 idSize;
	SceUInt32 reserved;
	SceUInt8 id[MTP_NPDRM_LICENSE_ID_SIZE];
	SceUInt32 rtcSize;
	SceUInt32 reserved2;
	SceUInt8 rtc[MTP_NPDRM_RTC_REQUEST_SIZE];
} MtpNpDrmLicenseRequest;

/* a licence the host sends back */
typedef struct MtpNpDrmLicense {
	SceUInt8 data[MTP_NPDRM_LICENSE_MAX];
	SceUInt32 size;
} MtpNpDrmLicense;

/* the RTC the host sends back, ahead of its bytes */
typedef struct __attribute__((packed)) MtpNpDrmRtcHeader {
	SceUInt32 result;
	SceUInt64 size;
} MtpNpDrmRtcHeader;

/* the activation results the host sends back, ahead of the activation data */
typedef struct __attribute__((packed)) MtpNpDrmActivationHeader {
	SceUInt8 result[3][4];
	SceUInt64 size;
} MtpNpDrmActivationHeader;

extern "C" {

int mtpContextCreateNpDrmBase(const void *request, SceSize size, SceSize replySize,
	MtpNpDrmBase **base)
{
	MtpNpDrmBlock *block;
	void *reply;
	SceSize requestSize = size + sizeof(MtpNpDrmBlock);

	block = (MtpNpDrmBlock *)mtpUtilMalloc(requestSize);
	if (block == NULL) {
		MTP_ERR(MTP_TAG_NDRM, 58, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	if (replySize != 0) {
		reply = mtpUtilMalloc(replySize);
		if (reply == NULL) {
			MTP_ERR(MTP_TAG_NDRM, 64, "no memory\n");
			mtpUtilFree(block);
			return SCE_MTP_ERROR_NO_MEMORY;
		}
		mtpUtilMemset(reply, 0, replySize);
	} else {
		reply = NULL;
	}
	*base = (MtpNpDrmBase *)mtpUtilMalloc(sizeof(MtpNpDrmBase));
	if (*base == NULL) {
		MTP_ERR(MTP_TAG_NDRM, 74, "no memory\n");
		mtpUtilFree(reply);
		mtpUtilFree(block);
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(*base, 0, sizeof(MtpNpDrmBase));
	if (request != NULL) {
		block->size = size;
		block->reserved = 0;
		mtpUtilMemcpy(block->data, request, size);
	}
	(*base)->request = block;
	(*base)->requestSize = requestSize;
	(*base)->reply = reply;
	(*base)->replySize = replySize;
	return 0;
}

} /* extern "C" */

static __attribute__((noinline)) void _mtpNpDrmFreeBase(void *data, SceSize size)
{
	MtpNpDrmBase *base = (MtpNpDrmBase *)data;

	if (base == NULL || size != sizeof(MtpNpDrmBase))
		return;
	mtpUtilFree(base->request);
	mtpUtilFree(base->reply);
	mtpUtilFree(base);
}

static void _mtpNpDrmGetRtcResult(MtpContext *context, void *result);
static void _mtpNpDrmAuthenticationResult(MtpContext *context, void *result);
static void _mtpNpDrmGetLicenseResult(MtpContext *context, void *result);

extern "C" {

int mtpContextNpDrmGetRtc(MtpContext *context, const void *request, SceSize size)
{
	MtpNpDrmBase *base = NULL;
	MtpNpDrmBase *allocated;
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	if (size != MTP_NPDRM_RTC_REQUEST_SIZE)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = mtpContextCreateNpDrmBase(request, MTP_NPDRM_RTC_REQUEST_SIZE,
		MTP_NPDRM_RTC_REPLY_SIZE, &base);
	if (ret < 0)
		return ret;
	event.code = MTP_OPERATION_REQUEST_NPDRM_RTC;
	event.numParams = 1;
	event.param[0] = context->id;
	context->data = base;
	context->dataSize = sizeof(MtpNpDrmBase);
	context->freeData = _mtpNpDrmFreeBase;
	context->onResult = _mtpNpDrmGetRtcResult;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 176, "mtpTaskQueuePush failed: 0x%x\n", ret);
		allocated = base;
		if (allocated != NULL) {
			mtpUtilFree(allocated->request);
			mtpUtilFree(allocated->reply);
			mtpUtilFree(allocated);
		}
		return ret;
	}
	return 0;
}

int mtpContextNpDrmExecAuthentication(MtpContext *context, SceUInt32 arg, int activate,
	const void *request, SceSize size)
{
	MtpNpDrmBase *base = NULL;
	MtpNpDrmBase *allocated;
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	if (size != MTP_NPDRM_AUTH_REQUEST_SIZE)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = mtpContextCreateNpDrmBase(request, size,
		activate == 1 ? MTP_NPDRM_ACTIVATION_REPLY_SIZE : 0, &base);
	if (ret < 0)
		return ret;
	event.code = MTP_OPERATION_REQUEST_NPDRM;
	event.param[2] = activate;
	event.numParams = 3;
	event.param[0] = context->id;
	event.param[1] = arg;
	context->data = base;
	context->dataSize = sizeof(MtpNpDrmBase);
	context->freeData = _mtpNpDrmFreeBase;
	context->onResult = _mtpNpDrmAuthenticationResult;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 219, "mtpTaskQueuePush failed: 0x%x\n", ret);
		allocated = base;
		if (allocated != NULL) {
			mtpUtilFree(allocated->request);
			mtpUtilFree(allocated->reply);
			mtpUtilFree(allocated);
		}
		return ret;
	}
	return 0;
}

int mtpContextNpDrmGetLicense(MtpContext *context, const void *rtc, const void *licenseId,
	SceSize size)
{
	MtpNpDrmBase *base = NULL;
	MtpNpDrmBase *allocated;
	MtpNpDrmLicenseRequest *request;
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	if (size != MTP_NPDRM_LICENSE_ID_SIZE)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = mtpContextCreateNpDrmBase(NULL,
		sizeof(MtpNpDrmLicenseRequest) - sizeof(MtpNpDrmBlock), sizeof(MtpNpDrmLicense),
		&base);
	if (ret < 0)
		return ret;
	request = (MtpNpDrmLicenseRequest *)base->request;
	request->idSize = MTP_NPDRM_LICENSE_ID_SIZE;
	request->reserved = 0;
	mtpUtilMemcpy(request->id, licenseId, MTP_NPDRM_LICENSE_ID_SIZE);
	request->rtcSize = MTP_NPDRM_RTC_REQUEST_SIZE;
	request->reserved2 = 0;
	mtpUtilMemcpy(request->rtc, rtc, MTP_NPDRM_RTC_REQUEST_SIZE);
	event.code = MTP_OPERATION_REQUEST_NPDRM_LICENSE;
	event.numParams = 1;
	event.param[0] = context->id;
	context->data = base;
	context->dataSize = sizeof(MtpNpDrmBase);
	context->freeData = _mtpNpDrmFreeBase;
	context->onResult = _mtpNpDrmGetLicenseResult;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 264, "mtpTaskQueuePush failed: 0x%x\n", ret);
		allocated = base;
		if (allocated != NULL) {
			mtpUtilFree(allocated->request);
			mtpUtilFree(allocated->reply);
			mtpUtilFree(allocated);
		}
		return ret;
	}
	return 0;
}

int mtpCmdGetChallenge(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	SceUInt8 challenge[32];
	int code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	MtpNpDrmBase *base;
	SceSize size;
	int ret;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_NDRM, 295, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 306, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpNpDrmBase)) {
		MTP_ERR(MTP_TAG_NDRM, 315, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	base = (MtpNpDrmBase *)context->data;
	if (base == NULL) {
		MTP_ERR(MTP_TAG_NDRM, 322, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	size = base->requestSize;
	if (base->request->data[2] == MTP_NPDRM_CHALLENGE_PCACT) {
		ret = sceSblGcAuthMgrPcactGetChallenge(MTP_NPDRM_CHALLENGE_PCACT, challenge,
			base->request->data);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NDRM, 336, "sceSblGcAuthMgrPcactGetChallenge failed: 0x%x\n",
				ret);
			ret = -2;
			goto error;
		}
	}
	ret = mtpCoreSendData(base->request, size, size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 346, "sceMtpIfSendData failed: 0x%x\n", ret);
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

int mtpCmdSendRtc(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	MtpNpDrmRtcHeader *rtc;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	MtpNpDrmBase *base;
	int ret;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_NDRM, 383, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto end;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 391, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto end;
	}
	if (context->dataSize != sizeof(MtpNpDrmBase)) {
		MTP_ERR(MTP_TAG_NDRM, 398, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto end;
	}
	rtc = (MtpNpDrmRtcHeader *)mtpUtilMalloc(MTP_NPDRM_RTC_RECV_SIZE);
	if (rtc == NULL) {
		MTP_ERR(MTP_TAG_NDRM, 406, "no memory\n");
		code = MTP_RESPONSE_VENDOR_NO_MEMORY;
		ret = -2;
		goto free;
	}
	base = (MtpNpDrmBase *)context->data;
	if (base == NULL) {
		MTP_ERR(MTP_TAG_NDRM, 413, "pContext->pData is NULL\n");
		ret = -2;
		goto free;
	}
	ret = mtpCoreRecvData(rtc, base->replySize + sizeof(MtpNpDrmRtcHeader), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 423, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto free;
	}
	base->result[0] = rtc->result;
	if (rtc->size != base->replySize) {
		MTP_ERR(MTP_TAG_NDRM, 429, "wrong data format\n");
		code = MTP_RESPONSE_VENDOR_UNKNOWN_STATUS;
		ret = -2;
		goto free;
	}
	mtpUtilMemcpy(base->reply, rtc + 1, base->replySize);
	mtpUtilFree(rtc);
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
free:
	if (rtc != NULL)
		mtpUtilFree(rtc);
end:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdSendActivation(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	MtpNpDrmActivationHeader header;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	MtpNpDrmBase *base;
	int ret;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_NDRM, 473, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto end;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 481, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto end;
	}
	if (context->dataSize != sizeof(MtpNpDrmBase)) {
		MTP_ERR(MTP_TAG_NDRM, 488, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto end;
	}
	base = (MtpNpDrmBase *)context->data;
	if (base == NULL) {
		MTP_ERR(MTP_TAG_NDRM, 495, "pContext->pData is NULL\n");
		ret = -2;
		goto end;
	}
	ret = mtpCoreRecvData(&header, sizeof(header), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 504, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto end;
	}
	mtpUtilMemcpy(&base->result[0], header.result[0], sizeof(base->result[0]));
	mtpUtilMemcpy(&base->result[1], header.result[1], sizeof(base->result[1]));
	mtpUtilMemcpy(&base->result[2], header.result[2], sizeof(base->result[2]));
	if (header.size != base->replySize) {
		MTP_ERR(MTP_TAG_NDRM, 515, "wrong data format: size %llu\n", header.size);
		code = MTP_RESPONSE_VENDOR_UNKNOWN_STATUS;
		ret = -2;
		goto end;
	}
	if (base->replySize != 0) {
		ret = mtpCoreRecvData(base->reply, base->replySize, NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NDRM, 524, "sceMtpIfRecvData failed: 0x%x\n", ret);
			goto end;
		}
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
end:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

int mtpCmdSendLicense(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	SceUInt64 size = 0;
	int code = MTP_RESPONSE_GENERAL_ERROR;
	MtpNpDrmLicense *license;
	MtpNpDrmBase *base;
	int ret;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_NDRM, 562, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto end;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 570, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto end;
	}
	if (context->dataSize != sizeof(MtpNpDrmBase)) {
		MTP_ERR(MTP_TAG_NDRM, 577, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto end;
	}
	base = (MtpNpDrmBase *)context->data;
	if (base == NULL) {
		MTP_ERR(MTP_TAG_NDRM, 584, "pContext->pData is NULL\n");
		ret = -2;
		goto end;
	}
	license = (MtpNpDrmLicense *)base->reply;
	ret = mtpCoreRecvData(&base->result[0], sizeof(base->result[0]), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 594, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto end;
	}
	ret = mtpCoreRecvData(&size, sizeof(size), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NDRM, 600, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto end;
	}
	if (size > MTP_NPDRM_LICENSE_MAX) {
		MTP_ERR(MTP_TAG_NDRM, 605, "License Size is too big: %llu\n", size);
		code = MTP_RESPONSE_VENDOR_DATA_TOO_LARGE;
		ret = -2;
		goto end;
	}
	if ((SceSize)size != 0) {
		ret = mtpCoreRecvData(license->data, (SceSize)size, NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NDRM, 614, "sceMtpIfRecvData failed: 0x%x\n", ret);
			goto end;
		}
	}
	license->size = (SceSize)size;
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
end:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

} /* extern "C" */

/* sceMtpEndNpDrmGetRtc: { SceUInt32 *result; void *rtc } */
static void _mtpNpDrmGetRtcResult(MtpContext *context, void *result)
{
	void **out = (void **)result;
	MtpNpDrmBase *base;

	if (context->dataSize != sizeof(MtpNpDrmBase))
		return;
	base = (MtpNpDrmBase *)context->data;
	if (base == NULL || out == NULL)
		return;
	*(SceUInt32 *)out[0] = base->result[0];
	mtpUtilMemcpy(out[1], base->reply, base->replySize);
}

/* sceMtpEndNpDrmActivate/Deactivate: { the three results; the activation data } */
static void _mtpNpDrmAuthenticationResult(MtpContext *context, void *result)
{
	void **out = (void **)result;
	MtpNpDrmBase *base;

	if (context->dataSize != sizeof(MtpNpDrmBase))
		return;
	base = (MtpNpDrmBase *)context->data;
	if (base == NULL || out == NULL)
		return;
	*(SceUInt32 *)out[0] = base->result[0];
	*(SceUInt32 *)out[1] = base->result[1];
	*(SceUInt32 *)out[2] = base->result[2];
	if (base->replySize != 0 && out[3] != NULL)
		mtpUtilMemcpy(out[3], base->reply, base->replySize);
}

/* sceMtpEndNpDrmGetLicense: { SceUInt32 *result; MtpNpDrmLicense *license } */
static void _mtpNpDrmGetLicenseResult(MtpContext *context, void *result)
{
	void **out = (void **)result;
	MtpNpDrmLicense *license;
	MtpNpDrmBase *base;

	if (context->dataSize != sizeof(MtpNpDrmBase))
		return;
	base = (MtpNpDrmBase *)context->data;
	if (base == NULL)
		return;
	license = (MtpNpDrmLicense *)base->reply;
	if (out == NULL)
		return;
	*(SceUInt32 *)out[0] = base->result[0];
	mtpUtilMemcpy(out[1], license->data, license->size);
	((MtpNpDrmLicense *)out[1])->size = license->size;
}
