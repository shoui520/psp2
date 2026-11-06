/*
 * The shell's trigger methods over SceShellSvc's IPMI connection. Sony's build keeps them after
 * trigger_util.cpp's functions, not inlined; the file's name and the method names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "trigger_util_ipc_client.h"

#define TRIGGER_UTIL_METHOD_REGISTER_PERIODIC_TIMER			0x100000
#define TRIGGER_UTIL_METHOD_UNREGISTER_PERIODIC_TIMER			0x100001
#define TRIGGER_UTIL_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME		0x100002
#define TRIGGER_UTIL_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME_RESULT	0x100003
#define TRIGGER_UTIL_METHOD_GET_PERIODIC_TIMER_LAST_ERROR		0x100004
#define TRIGGER_UTIL_METHOD_REGISTER_TIME_OF_DAY_TIMER			0x100005
#define TRIGGER_UTIL_METHOD_UNREGISTER_TIME_OF_DAY_TIMER		0x100006
#define TRIGGER_UTIL_METHOD_GET_TIME_OF_DAY_TIMER_TIME			0x100007
#define TRIGGER_UTIL_METHOD_GET_TIME_OF_DAY_TIMER_TIME_RESULT		0x100008
#define TRIGGER_UTIL_METHOD_GET_TIME_OF_DAY_TIMER_LAST_ERROR		0x100009
#define TRIGGER_UTIL_METHOD_REGISTER_DAILY_TIMER			0x10000A
#define TRIGGER_UTIL_METHOD_UNREGISTER_DAILY_TIMER			0x10000B
#define TRIGGER_UTIL_METHOD_GET_DAILY_TIMER_INFO			0x10000C
#define TRIGGER_UTIL_METHOD_GET_DAILY_TIMER_INFO_RESULT			0x10000D
#define TRIGGER_UTIL_METHOD_GET_DAILY_TIMER_LAST_ERROR			0x10000E
#define TRIGGER_UTIL_METHOD_GET_POLICY					0x10000F
#define TRIGGER_UTIL_METHOD_GET_POLICY_RESULT				0x100010
#define TRIGGER_UTIL_METHOD_GET_RESULT					0x100011
#define TRIGGER_UTIL_METHOD_SET_TITLE_FLAG				0x100012
#define TRIGGER_UTIL_METHOD_GET_APP_TITLE_ID_LIST			0x100013
#define TRIGGER_UTIL_METHOD_GET_APP_INFO				0x100014
#define TRIGGER_UTIL_METHOD_GET_APP_INFO_RESULT				0x100015
#define TRIGGER_UTIL_METHOD_GET_SYSTEM_TITLE_ID_LIST			0x100016
#define TRIGGER_UTIL_METHOD_GET_SYSTEM_SERVICE_INFO			0x100017
#define TRIGGER_UTIL_METHOD_GET_SYSTEM_SERVICE_INFO_RESULT		0x100018

/* what a timer request sends first */
struct TimerRequestHeader {
	SceUInt32 timerId;
	bool useCallback;
};

/* what an information request sends first */
struct InfoRequestHeader {
	SceUInt32 option;
	bool useCallback;
};

/* the bytes before the terminator, counted inline */
static inline SceSize stringLength(const char *str)
{
	SceSize length = 0;

	while (*str != '\0') {
		str++;
		length++;
	}
	return length;
}

int triggerUtilRegisterPeriodicTimer(IPMI::Client *client, int *pRequestId,
				     const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				     const SceTriggerUtilPeriodicTimerParam *param,
				     SceUInt32 timerId, bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[3];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = titleId;
	in[1].dataSize = stringLength(titleId) + 1;
	in[2].pData = param;
	in[2].dataSize = sizeof(*param);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_REGISTER_PERIODIC_TIMER, in, 3,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilUnregisterPeriodicTimer(IPMI::Client *client, int *pRequestId,
				       const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				       bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_UNREGISTER_PERIODIC_TIMER, in, 1,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetPeriodicTimerRemainingTime(IPMI::Client *client, int *pRequestId,
					     const IPMI::EventFlagInfo *pEventFlag,
					     SceUInt32 timerId, bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME, in,
					1, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetPeriodicTimerRemainingTimeResult(IPMI::Client *client, int requestId,
						   SceUInt32 *pRemainingTime)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pRemainingTime;
	out[0].bufferSize = sizeof(*pRemainingTime);
	ret = client->tryGetResult(TRIGGER_UTIL_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME_RESULT,
				   requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetPeriodicTimerLastError(IPMI::Client *client, int *pRequestId,
					 const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
					 bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_PERIODIC_TIMER_LAST_ERROR, in, 1,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilRegisterTimeOfDayTimer(IPMI::Client *client, int *pRequestId,
				      const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				      const SceTriggerUtilTimeOfDayTimerParam *param,
				      SceUInt32 timerId, bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[3];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = titleId;
	in[1].dataSize = stringLength(titleId) + 1;
	in[2].pData = param;
	in[2].dataSize = sizeof(*param);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_REGISTER_TIME_OF_DAY_TIMER, in, 3,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilUnregisterTimeOfDayTimer(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
					bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_UNREGISTER_TIME_OF_DAY_TIMER, in, 1,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetTimeOfDayTimerTime(IPMI::Client *client, int *pRequestId,
				     const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				     bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_TIME_OF_DAY_TIMER_TIME, in, 1,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetTimeOfDayTimerTimeResult(IPMI::Client *client, int requestId, SceRtcTick *pTime,
					   bool *pFlag)
{
	int result = 0;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = pTime;
	out[0].bufferSize = sizeof(*pTime);
	out[1].pBuffer = pFlag;
	out[1].bufferSize = sizeof(*pFlag);
	ret = client->tryGetResult(TRIGGER_UTIL_METHOD_GET_TIME_OF_DAY_TIMER_TIME_RESULT, requestId,
				   &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetTimeOfDayTimerLastError(IPMI::Client *client, int *pRequestId,
					  const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
					  bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_TIME_OF_DAY_TIMER_LAST_ERROR, in, 1,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilRegisterDailyTimer(IPMI::Client *client, int *pRequestId,
				  const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				  const SceTriggerUtilDailyTimerParam *param, SceUInt32 timerId,
				  bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[3];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = titleId;
	in[1].dataSize = stringLength(titleId) + 1;
	in[2].pData = param;
	in[2].dataSize = sizeof(*param);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_REGISTER_DAILY_TIMER, in, 3, pRequestId,
					pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilUnregisterDailyTimer(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				    bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_UNREGISTER_DAILY_TIMER, in, 1,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetDailyTimerInfo(IPMI::Client *client, int *pRequestId,
				 const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				 bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_DAILY_TIMER_INFO, in, 1, pRequestId,
					pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetDailyTimerInfoResult(IPMI::Client *client, int requestId,
				       SceTriggerUtilDailyTimerState *pState,
				       SceTriggerUtilDailyTimerParam *param, SceUInt32 *pStatus)
{
	int result = 0;
	IPMI::BufferInfo out[3];
	int ret;

	out[0].pBuffer = pState;
	out[0].bufferSize = sizeof(*pState);
	out[1].pBuffer = param;
	out[1].bufferSize = sizeof(*param);
	out[2].pBuffer = pStatus;
	out[2].bufferSize = sizeof(*pStatus);
	ret = client->tryGetResult(TRIGGER_UTIL_METHOD_GET_DAILY_TIMER_INFO_RESULT, requestId,
				   &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetDailyTimerLastError(IPMI::Client *client, int *pRequestId,
				      const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				      bool useCallback)
{
	TimerRequestHeader header;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	header.timerId = timerId;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_DAILY_TIMER_LAST_ERROR, in, 1,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetPolicy(IPMI::Client *client, int *pRequestId,
			 const IPMI::EventFlagInfo *pEventFlag, bool useCallback)
{
	bool header;
	IPMI::DataInfo in[1];
	int ret;

	header = useCallback;
	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_POLICY, in, 1, pRequestId,
					pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetPolicyResult(IPMI::Client *client, int requestId, SceUInt8 *pPolicy)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pPolicy;
	out[0].bufferSize = sizeof(*pPolicy);
	ret = client->tryGetResult(TRIGGER_UTIL_METHOD_GET_POLICY_RESULT, requestId, &result, out,
				   1);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(TRIGGER_UTIL_METHOD_GET_RESULT, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilSetTitleFlag(IPMI::Client *client, const char *titleId, bool flag)
{
	int result = 0;
	bool header;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = titleId;
	in[1].dataSize = stringLength(titleId) + 1;
	header = flag;
	ret = client->invokeSyncMethod(TRIGGER_UTIL_METHOD_SET_TITLE_FLAG, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetAppTitleIdList(IPMI::Client *client, char *titleIdList, SceUInt32 *pCount)
{
	int result = 0;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = titleIdList;
	out[0].bufferSize = SCE_TRIGGER_UTIL_APP_TITLE_ID_LIST_SIZE;
	out[1].pBuffer = pCount;
	out[1].bufferSize = sizeof(*pCount);
	ret = client->invokeSyncMethod(TRIGGER_UTIL_METHOD_GET_APP_TITLE_ID_LIST, NULL, 0, &result,
				       out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetAppInfo(IPMI::Client *client, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
			  SceUInt32 option, bool useCallback)
{
	InfoRequestHeader header;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = titleId;
	in[1].dataSize = stringLength(titleId) + 1;
	header.option = option;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_APP_INFO, in, 2, pRequestId,
					pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetAppInfoResult(IPMI::Client *client, int requestId, SceTriggerUtilAppInfo *pInfo)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pInfo;
	out[0].bufferSize = sizeof(*pInfo);
	ret = client->tryGetResult(TRIGGER_UTIL_METHOD_GET_APP_INFO_RESULT, requestId, &result, out,
				   1);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetSystemTitleIdList(IPMI::Client *client, char *titleIdList, SceUInt32 *pCount)
{
	int result = 0;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = titleIdList;
	out[0].bufferSize = SCE_TRIGGER_UTIL_SYSTEM_TITLE_ID_LIST_SIZE;
	out[1].pBuffer = pCount;
	out[1].bufferSize = sizeof(*pCount);
	ret = client->invokeSyncMethod(TRIGGER_UTIL_METHOD_GET_SYSTEM_TITLE_ID_LIST, NULL, 0,
				       &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int triggerUtilGetSystemServiceInfo(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				    SceUInt32 option, bool useCallback)
{
	InfoRequestHeader header;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = titleId;
	in[1].dataSize = stringLength(titleId) + 1;
	header.option = option;
	header.useCallback = useCallback;
	ret = client->invokeAsyncMethod(TRIGGER_UTIL_METHOD_GET_SYSTEM_SERVICE_INFO, in, 2,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int triggerUtilGetSystemServiceInfoResult(IPMI::Client *client, int requestId,
					  SceTriggerUtilSystemServiceInfo *pInfo)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pInfo;
	out[0].bufferSize = sizeof(*pInfo);
	ret = client->tryGetResult(TRIGGER_UTIL_METHOD_GET_SYSTEM_SERVICE_INFO_RESULT, requestId,
				   &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}
