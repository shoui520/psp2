/*
 * The API's requests ("MINI:"): each Begin makes a context for its operation, the End waits for
 * its result; a request is cancelled with its context, and the responder is told to end.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include "libmtp.h"
#include "mtp_internal.h"

/* the vendor operations a request runs, and the events the host is told of */
#define MTP_OPERATION_EXPORT_OBJECT	0xC107

/* the context's kind: whether it holds a data phase */
#define MTP_CONTEXT_KIND_DATA		1

/* the application in front, to sceAppMgrGetStatusByAppId */
#define MTP_APP_ID_ACTIVE		(-4)

/* the content manager; requests from Settings do not check the host's protocol version */
#define MTP_TITLE_ID_SETTINGS		"NPXS10015"
#define MTP_TITLE_ID_CONTENT_MANAGER	"NPXS10026"

int mtpIfEndCommon(SceUInt32 contextId, void *result, const char *name)
{
	MtpContext *context = NULL;
	int ret;

	ret = mtpContextGet(contextId, 0, &context);
	if (ret < 0) {
		MTP_ERR("MINI:", 92, "GetContext %u failed: 0x%x\n", contextId, ret);
		return ret;
	}
	ret = mtpContextWaitResult(context, result);
	if (ret < 0 && ret != SCE_MTP_ERROR_CANCELED)
		mtpTaskQueueCancel(context);
	mtpContextDelete(context);
	return ret;
}

int sceMtpBeginHttpGetDataWithUrl(const char *url, int arg, SceUInt64 offset,
	SceUInt32 *contextId)
{
	MtpContext *context;
	int checkVersion;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	char titleId[32] = { 0 };
	context = NULL;
	ret = mtpTaskQueueGetTitleId(titleId, sizeof(titleId));
	if (ret < 0)
		return ret;
	checkVersion = mtpUtilStrcmp(titleId, MTP_TITLE_ID_SETTINGS) != 0;
	ret = mtpStatusCheck(checkVersion);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(MTP_CONTEXT_KIND_DATA, &context, MTP_OPERATION_HTTP_GET_DATA);
	if (ret < 0)
		return ret;
	context->checkVersion = checkVersion;
	ret = mtpContextHttpGetData(context, url, arg, offset);
	if (ret < 0) {
		mtpContextDelete(context);
		return ret;
	}
	*contextId = context->id;
	return ret;
}

int SceLibMtp_520D2696(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginHttpGetPropertyWithUrl(const char *url, int arg, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(0);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_HTTP_GET_PROPERTY);
	if (ret < 0)
		return ret;
	ret = mtpContextHttpGetProperty(context, url, arg);
	if (ret < 0) {
		mtpContextDelete(context);
		return ret;
	}
	*contextId = context->id;
	return ret;
}

int SceLibMtp_54CDF039(SceUInt32 contextId, int arg1, int arg2)
{
	int result[2];

	result[0] = arg1;
	result[1] = arg2;
	return mtpIfEndCommon(contextId, result, NULL);
}

int SceLibMtp_CA704934(SceUInt32 contextId, void *data, SceSize size, SceSize *received,
	SceUInt64 *totalSize)
{
	MtpContext *context;
	int ret;

	if (received == NULL || totalSize == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0)
		return ret;
	ret = mtpContextRecvData(context, data, size, received, totalSize);
	mtpContextPutContext(context, 0);
	return ret;
}

int sceMtpSendData(SceUInt32 contextId, const void *data, SceSize size, SceSize *sent,
	SceUInt64 *totalSize)
{
	MtpContext *context;
	int ret;

	if (sent == NULL || totalSize == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0)
		return ret;
	ret = mtpContextSendData(context, data, size, sent, totalSize);
	mtpContextPutContext(context, 0);
	return ret;
}

int SceLibMtp_77793BF2(SceUInt32 contextId, int result)
{
	MtpContext *context = NULL;
	int ret;

	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0)
		return ret;
	ret = mtpContextNotifyResultResponse(context, result);
	mtpContextPutContext(context, 0);
	return ret;
}

int sceMtpGetHostInfo(MtpStatus *status)
{
	mtpStatusGet(status);
	return 0;
}

static inline int isCurrentActiveApp(void)
{
	SceAppMgrAppStatus appStatus;
	int ret;

	ret = sceAppMgrGetStatusByAppId(MTP_APP_ID_ACTIVE, &appStatus);
	if (ret < 0) {
		MTP_ERR("MINI:", 338, "sceAppMgrGetStatusByAppId failed: 0x%x\n", ret);
		return 0;
	}
	if (sce_paf_strcmp(appStatus.appName, MTP_TITLE_ID_CONTENT_MANAGER) != 0)
		return 0;
	return 1;
}

int SceLibMtp_F82B75F5(MtpStatus *status)
{
	mtpStatusGet(status);
	if (isCurrentActiveApp()) {
		if (status->state != 0)
			return 0;
		if (status->reason != 9)
			mtpCoreWake(0);
	}
	if (status->state == 0 && !netMtpIsWlanAvailable())
		status->reason = 3;
	return 0;
}

static inline void _cancelTransaction(void)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.code = MTP_EVENT_CANCEL_TRANSACTION;
	event.numParams = 0;
	ret = mtpCoreSendEvent(&event);
	if (ret < 0)
		MTP_ERR("MINI:", 66, "mtpCoreSendEvent failed: 0x%x\n", ret);
}

int SceLibMtp_1021F75F(SceUInt32 contextId)
{
	MtpContext *cancel = NULL;
	MtpContext *context = NULL;
	int ret;

	if (contextId == 0) {
		mtpCoreSetCancelRequested(1);
		return mtpCoreWakeCancel();
	}
	if (mtpContextGet(contextId, 0, &context) == 0 &&
	    context->opcode == MTP_OPERATION_EXPORT_OBJECT) {
		mtpContextPauseSource(context->pauseData[0]);
		mtpContextPauseTarget(context->pauseData[1]);
		mtpContextSetPauseContextData(contextId);
	}
	ret = mtpContextCancelById(contextId);
	if (ret < 0)
		return ret;
	_cancelTransaction();
	ret = mtpContextCreateCancel(&cancel, MTP_OPERATION_CANCEL);
	if (ret < 0)
		return ret;
	paf::common::SharedPtr<MtpContext> held(cancel);
	ret = mtpContextCancel(cancel, contextId);
	mtpContextRegister(held);
	mtpContextDelete(cancel);
	return ret;
}

int sceMtpTerminateExecution(void)
{
	MtpContext *context = NULL;
	MtpEvent event;
	int ret;

	ret = mtpContextCreateTerminate(&context, MTP_OPERATION_TERMINATE);
	if (ret < 0) {
		mtpCoreSleep();
		return ret;
	}
	paf::common::SharedPtr<MtpContext> held(context);
	mtpUtilMemset(&event, 0, sizeof(event));
	event.numParams = 1;
	event.code = MTP_OPERATION_TERMINATE;
	event.param[0] = context->id;
	ret = mtpCoreSendEvent(&event);
	if (ret < 0 && ret != MTP_ERROR_DISCONNECTED)
		MTP_ERR("MINI:", 455, "MtpIfSendEvent failed: 0x%x\n", ret);
	mtpContextRegister(held);
	mtpCoreSetPublicStatus(2, -1);
	if (mtpCoreHasPendingOperation() == 1) {
		mtpCoreClearPendingOperation();
		mtpContextCancelAll();
	}
	mtpDevIfCancelTransfer();
	netMtpResponderDisconnect();
	mtpCoreSleep();
	mtpContextDelete(context);
	return ret;
}
