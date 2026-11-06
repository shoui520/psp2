/*
 * The shell's notification methods over SceShellSvc's IPMI connection. Sony's build keeps them
 * out of the module's file (after its exports, not inlined); the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "notification_util_ipc_client.h"

#define NOTIFICATION_UTIL_METHOD_SEND			0x70007
#define NOTIFICATION_UTIL_METHOD_SEND_RESULT		0x70008
#define NOTIFICATION_UTIL_METHOD_CLEAN_HISTORY		0x70009
#define NOTIFICATION_UTIL_METHOD_CLEAN_HISTORY_RESULT	0x7000a
#define NOTIFICATION_UTIL_METHOD_PROGRESS_BEGIN		0x7000b
#define NOTIFICATION_UTIL_METHOD_PROGRESS_BEGIN_RESULT	0x7000c
#define NOTIFICATION_UTIL_METHOD_PROGRESS_UPDATE	0x7000d
#define NOTIFICATION_UTIL_METHOD_PROGRESS_FINISH	0x7000e
#define NOTIFICATION_UTIL_METHOD_PROGRESS_FINISH_RESULT	0x7000f

int notificationUtilRequestSend(IPMI::Client *client, int *pRequestId,
				const IPMI::EventFlagInfo *pEventFlag,
				const SceNotificationUtilSendParam *pParam, bool isBgApp)
{
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &isBgApp;
	in[0].dataSize = sizeof(isBgApp);
	in[1].pData = pParam;
	in[1].dataSize = sizeof(*pParam);
	ret = client->invokeAsyncMethod(NOTIFICATION_UTIL_METHOD_SEND, in, 2, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int notificationUtilGetSendResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(NOTIFICATION_UTIL_METHOD_SEND_RESULT, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int notificationUtilRequestCleanHistory(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, bool isBgApp)
{
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &isBgApp;
	in[0].dataSize = sizeof(isBgApp);
	ret = client->invokeAsyncMethod(NOTIFICATION_UTIL_METHOD_CLEAN_HISTORY, in, 1, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int notificationUtilGetCleanHistoryResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(NOTIFICATION_UTIL_METHOD_CLEAN_HISTORY_RESULT, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int notificationUtilRequestProgressBegin(IPMI::Client *client, int *pRequestId,
					 const IPMI::EventFlagInfo *pEventFlag,
					 const SceNotificationUtilProgressBeginParam *pParam,
					 bool isBgApp)
{
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &isBgApp;
	in[0].dataSize = sizeof(isBgApp);
	in[1].pData = pParam;
	in[1].dataSize = sizeof(*pParam);
	ret = client->invokeAsyncMethod(NOTIFICATION_UTIL_METHOD_PROGRESS_BEGIN, in, 2, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int notificationUtilGetProgressBeginResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(NOTIFICATION_UTIL_METHOD_PROGRESS_BEGIN_RESULT, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int notificationUtilProgressUpdate(IPMI::Client *client,
				   const SceNotificationUtilProgressUpdateParam *pParam)
{
	IPMI::DataInfo in[1];
	int result = 0;
	int ret;

	in[0].pData = pParam;
	in[0].dataSize = sizeof(*pParam);
	ret = client->invokeSyncMethod(NOTIFICATION_UTIL_METHOD_PROGRESS_UPDATE, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int notificationUtilRequestProgressFinish(IPMI::Client *client, int *pRequestId,
					  const IPMI::EventFlagInfo *pEventFlag,
					  const SceNotificationUtilProgressFinishParam *pParam)
{
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = pParam;
	in[0].dataSize = sizeof(*pParam);
	ret = client->invokeAsyncMethod(NOTIFICATION_UTIL_METHOD_PROGRESS_FINISH, in, 1, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int notificationUtilGetProgressFinishResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(NOTIFICATION_UTIL_METHOD_PROGRESS_FINISH_RESULT, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
