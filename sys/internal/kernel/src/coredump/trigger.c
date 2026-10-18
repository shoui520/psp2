/* SceCoredump asynchronous trigger requests. */

#include <string.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/processmgr.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include "oal_internal.h"

typedef struct CoredumpTriggerRequest {
	int available;
	int requestId;
	char titleid[12];
	char app_name[128];
	unsigned char triggerData[1024];
} CoredumpTriggerRequest;

_Static_assert(sizeof(CoredumpTriggerRequest) == 1172,
	"coredump trigger-request size");

SceUID syncDumpEvent = -1;
char *syncDumpPath;
SceSize syncDumpPathSize;
int nextRequestId;
int isCex;
SceUID triggerMutexId;
SceUID triggerCondId;
CoredumpTriggerRequest triggerRequests[2];

extern uint32_t oalGetDumpLevelFromSettings(void);
extern int oalMutexLock(SceUID mutexId);
extern int oalMutexUnlock(SceUID mutexId);
extern int oalCondSignalTo(SceUID condId, SceUID threadId);
extern int oalIsCEX(void);
extern int oalMutexInit(SceUID *mutexId);
extern int oalMutexFinal(SceUID mutexId);
extern int oalCondInit(SceUID *condId, SceUID mutexId);
extern int oalCondFinal(SceUID condId);
extern SceUID oalGetThreadId(void);
extern int oalReferObject(SceUID guid);
extern int oalReleaseObject(SceUID guid);
extern int generatorSendEvent(const CoredumpEvent *event);
extern int generatorCancel(int requestId);
extern int generatorBeginShutdown(void);
extern int oalCondWait(SceUID condId);

SCE_DBGINFO_DEFINE(static, triggerErrorInfo, 400, "sceCoredumpTrigger");
SCE_DBGINFO_DEFINE(static, triggerParamErrorInfo, 406, "sceCoredumpTrigger");

int _notifyAsyncTriggerResultCallback(int requestId, SceUID pid, int result,
	const char *path, SceSize pathLength, int encrypted,
	CoredumpCompletionContext *context)
{
	int index;

	SCE_DBG_ASSERT_FUNC(context != NULL, 653,
		"_notifyAsyncTriggerResultCallback");
	if (triggerRequests[0].requestId == requestId)
		index = 0;
	else if (triggerRequests[1].requestId == requestId)
		index = 1;
	else
		goto notify;
	triggerRequests[index].available = 1;
notify:
	return context->callback(requestId, pid, result, path, pathLength,
		encrypted);
}

int _prepareTriggerParam(int requestId, const SceCoredumpTriggerParam *param,
	SceCoredumpTriggerParam *out)
{
	CoredumpTriggerRequest *request;
	int index;

	memset(out, 0, sizeof(*out));
	out->dumpLevel = 15;
	if (triggerRequests[0].available)
		index = 0;
	else if (triggerRequests[1].available)
		index = 1;
	else
		return SCE_ERROR_ERRNO_ENOMEM;
	request = &triggerRequests[index];
	request->requestId = requestId;
	request->available = 0;
	if (param->size > 51) {
		if (param->titleid != NULL) {
			out->titleid = request->titleid;
			if (param->titleid_len != 0)
				memcpy(request->titleid, param->titleid,
					param->titleid_len);
		}
		out->titleid_len = param->titleid_len;
		if (param->app_name != NULL) {
			out->app_name = request->app_name;
			if (param->app_name_len != 0)
				memcpy(request->app_name, param->app_name,
					param->app_name_len);
		}
		out->app_name_len = param->app_name_len;
		out->appVersion = param->appVersion;
		out->triggerReason = param->triggerReason;
		out->crash_thid = param->crash_thid;
		out->triggerFlags = param->triggerFlags;
	}
	if (param->size > 19) {
		out->dumpLevel = param->dumpLevel;
		out->triggerDataType = param->triggerDataType;
		if (param->triggerDataType == 10) {
			out->triggerDataSize = param->triggerDataSize;
			if (param->triggerData != NULL) {
				out->triggerData = request->triggerData;
				if (param->triggerDataSize != 0)
					memcpy(request->triggerData,
						param->triggerData,
						param->triggerDataSize);
			}
		}
	}
	if (param->size > 7) {
		unsigned int dumpLevel = param->dumpLevel;

		if (dumpLevel != 0)
			out->dumpLevel = dumpLevel;
		else
			out->dumpLevel = oalGetDumpLevelFromSettings();
	}
	return 0;
}

int _notifySyncTriggerResultCallback(int requestId, SceUID pid, int result,
	const char *path, SceSize pathLength, int encrypted)
{
	char *destination = syncDumpPath;

	(void)requestId;
	(void)pid;
	(void)result;
	(void)pathLength;
	(void)encrypted;
	if (destination != NULL) {
		SceSize copySize = syncDumpPathSize - 1;

		strncpy(destination, path, copySize);
		destination[copySize] = '\0';
	}
	syncDumpPath = NULL;
	return sceKernelSetEventFlag(syncDumpEvent, 1);
}

int _notifyShutdownResultCallback(int result, CoredumpShutdownContext *context)
{
	SceUID threadId = context->threadId;

	*context->result = result;
	result = oalMutexLock(triggerMutexId);
	if (result != 0)
		return result;
	oalCondSignalTo(triggerCondId, threadId);
	return oalMutexUnlock(triggerMutexId);
}

int triggerInit(void)
{
	int result;

	isCex = oalIsCEX();
	result = oalMutexInit(&triggerMutexId);
	if (result != 0)
		return result;
	result = oalCondInit(&triggerCondId, triggerMutexId);
	if (result != 0) {
		oalMutexFinal(triggerMutexId);
		return result;
	}
	nextRequestId = 1;
	memset(triggerRequests, 0, sizeof(triggerRequests));
	triggerRequests[0].available = 1;
	triggerRequests[1].available = 1;
	return result;
}

int triggerFinal(void)
{
	int condResult = oalCondFinal(triggerCondId);
	int result = oalMutexFinal(triggerMutexId);

	if (result == 0)
		result = condResult;
	return result;
}

int sceCoredumpTrigger(SceUID pid, SceKernelCoredumpStateUpdateCallback update,
	SceKernelCoredumpStateFinishCallback finish, SceCoredumpTriggerParam *param)
{
	SceCoredumpTriggerParam prepared;
	CoredumpEvent event;
	int requestId;
	int followingRequestId;
	int result;
	int unlockResult;

	if (param == NULL || param->size < 4)
		return SCE_ERROR_ERRNO_EINVAL;
	if (param->size > 51) {
		if (param->titleid_len > 10 ||
		    (param->titleid_len != 0 && param->titleid == NULL))
			return SCE_ERROR_ERRNO_EINVAL;
		if (param->app_name_len > 128 ||
		    (param->app_name_len != 0 && param->app_name == NULL))
			return SCE_ERROR_ERRNO_EINVAL;
	}
	if (param->size > 19) {
		if (param->triggerDataType != 10 && param->triggerDataType > 2)
			return SCE_ERROR_ERRNO_EINVAL;
		if (param->triggerDataType == 10 &&
		    (param->triggerDataSize > 1024 ||
		     (param->triggerDataSize != 0 && param->triggerData == NULL)))
			return SCE_ERROR_ERRNO_EINVAL;
	}
	if (finish == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	result = oalMutexLock(triggerMutexId);
	if (result != 0)
		goto error;
	requestId = nextRequestId;
	followingRequestId = requestId + 1;
	if (followingRequestId == 0x7fffffff)
		followingRequestId = 1;
	nextRequestId = followingRequestId;
	result = _prepareTriggerParam(requestId, param, &prepared);
	if (result != 0)
		goto unlockError;
	memset(&event, 0, sizeof(event));
	event.kind = 1;
	event.pid = pid;
	event.requestId = requestId;
	event.trigger = prepared;
	event.progressCallback = update;
	event.completionCallback = _notifyAsyncTriggerResultCallback;
	event.completionContext.threadId = oalGetThreadId();
	event.completionContext.callback = finish;
	event.completionContext.triggerData = prepared.triggerData;
	result = oalReferObject(event.pid);
	if (result < 0)
		goto unlockError;
	result = generatorSendEvent(&event);
	if (result != 0) {
		oalReleaseObject(event.pid);
		goto unlockError;
	}
	result = oalMutexUnlock(triggerMutexId);
	if (result == 0)
		return event.requestId;
	goto error;
unlockError:
	unlockResult = oalMutexUnlock(triggerMutexId);
	if (unlockResult != 0)
		result = unlockResult;
error:
	SceDebugForDriver_02B04343(0, &triggerErrorInfo,
		"0x%08x:%d,%d:0x%08x:0x%08x\n",
		0xcf168d67U, 2, 400, 0x101, result);
	SceDebugForDriver_02B04343(0, &triggerParamErrorInfo,
		"0x%08x:%d,%d:0x%08x:0x%08x,%p,%p,0x%08x,0x%08x,0x%08x,0x%08x,%p\n",
		0xcf168d67U, 2, 406, 0x102, pid, update, finish,
		param->size, param->dumpLevel, param->triggerDataType,
		param->triggerDataSize, param->triggerData);
	return result;
}

int SceCoredumpForDriver_D064F6DC(SceUID pid, SceKernelCoredumpStateUpdateCallback update,
	SceKernelCoredumpStateFinishCallback finish, SceCoredumpTriggerParam *param)
{
	SceCoredumpTriggerParam prepared;
	CoredumpEvent event;
	int requestId;
	int followingRequestId;
	int result;
	int unlockResult;

	if (param == NULL || param->size < 4)
		return SCE_ERROR_ERRNO_EINVAL;
	if (param->size > 51) {
		if (param->titleid_len > 10 ||
		    (param->titleid_len != 0 && param->titleid == NULL))
			return SCE_ERROR_ERRNO_EINVAL;
		if (param->app_name_len > 128 ||
		    (param->app_name_len != 0 && param->app_name == NULL))
			return SCE_ERROR_ERRNO_EINVAL;
	}
	if (param->size > 19) {
		if (param->triggerDataType != 10 && param->triggerDataType > 2)
			return SCE_ERROR_ERRNO_EINVAL;
		if (param->triggerDataType == 10 &&
		    (param->triggerDataSize > 1024 ||
		     (param->triggerDataSize != 0 && param->triggerData == NULL)))
			return SCE_ERROR_ERRNO_EINVAL;
	}
	if (finish == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	result = oalMutexLock(triggerMutexId);
	if (result != 0)
		goto error;
	requestId = nextRequestId;
	followingRequestId = requestId + 1;
	if (followingRequestId == 0x7fffffff)
		followingRequestId = 1;
	nextRequestId = followingRequestId;
	result = _prepareTriggerParam(requestId, param, &prepared);
	if (result != 0)
		goto unlockError;
	memset(&event, 0, sizeof(event));
	event.kind = 1;
	event.pid = pid;
	event.requestId = requestId;
	event.trigger = prepared;
	event.progressCallback = update;
	event.completionCallback = _notifyAsyncTriggerResultCallback;
	event.completionContext.threadId = oalGetThreadId();
	event.completionContext.callback = finish;
	event.completionContext.triggerData = prepared.triggerData;
	result = oalReferObject(event.pid);
	if (result < 0)
		goto unlockError;
	result = generatorSendEvent(&event);
	if (result != 0) {
		oalReleaseObject(event.pid);
		goto unlockError;
	}
	result = oalMutexUnlock(triggerMutexId);
	if (result == 0)
		return event.requestId;
	goto error;
unlockError:
	unlockResult = oalMutexUnlock(triggerMutexId);
	if (unlockResult != 0)
		result = unlockResult;
error:
	SceDebugForDriver_02B04343(0, &triggerErrorInfo,
		"0x%08x:%d,%d:0x%08x:0x%08x\n",
		0xcf168d67U, 2, 400, 0x101, result);
	SceDebugForDriver_02B04343(0, &triggerParamErrorInfo,
		"0x%08x:%d,%d:0x%08x:0x%08x,%p,%p,0x%08x,0x%08x,0x%08x,0x%08x,%p\n",
		0xcf168d67U, 2, 406, 0x102, pid, update, finish,
		param->size, param->dumpLevel, param->triggerDataType,
		param->triggerDataSize, param->triggerData);
	return result;
}

int sceKernelCoredumpCancelForDriver(int requestId)
{
	return generatorCancel(requestId);
}

int SceCoredumpForDriver_097AA37D(void)
{
	return 1;
}

int generatorFinal(void)
{
	CoredumpEvent event;
	int shutdownResult;
	int beginResult = generatorBeginShutdown();
	int result;
	int waitResult;

	memset(&event, 0, sizeof(event));
	event.kind = 2;
	event.shutdownCallback = _notifyShutdownResultCallback;
	event.shutdownContext.threadId = oalGetThreadId();
	event.shutdownContext.result = &shutdownResult;
	result = generatorSendEvent(&event);
	if (result != 0)
		return result;
	if (oalMutexLock(triggerMutexId) != 0)
		return beginResult;
	waitResult = oalCondWait(triggerCondId);
	if (waitResult == 0)
		waitResult = shutdownResult;
	result = oalMutexUnlock(triggerMutexId);
	if (result == 0)
		result = waitResult;
	return result;
}

int triggerGetNextRequestId(void)
{
	int requestId = nextRequestId++;

	if (nextRequestId == 0x7fffffff)
		nextRequestId = 1;
	return requestId;
}

int SceCoredumpForDriver_0C10313F(SceUID pid, const char *titleid, int titleidLength,
	const char *appName, int appNameLength, int appVersion,
	char *path, SceSize pathSize)
{
	SceCoredumpTriggerParam param;
	SceUID eventFlag;
	int result;
	int dumpResult;

	if ((titleidLength | appNameLength) < 0 || appName == NULL ||
	    titleid == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	syncDumpPathSize = pathSize;
	syncDumpPath = path;
	result = sceKernelProcessDebugSuspend(pid, 17);
	if (result != 0)
		return result;
	__builtin_memset(&param.dumpLevel, 0, sizeof(param.dumpLevel) +
		sizeof(param.triggerDataType) + sizeof(param.triggerDataSize) +
		sizeof(param.triggerData));
	param.titleid_len = titleidLength;
	param.app_name = appName;
	param.appVersion = appVersion;
	param.titleid = titleid;
	param.app_name_len = appNameLength;
	param.triggerReason = -2;
	param.crash_thid = -1;
	param.size = sizeof(param);
	param.triggerFlags = 1;
	eventFlag = sceKernelCreateEventFlag("SceCreatedCoreDumpEvent", 0, 0, NULL);
	if (eventFlag < 0)
		return eventFlag;
	syncDumpEvent = eventFlag;
	dumpResult = sceCoredumpTrigger(pid, NULL,
		_notifySyncTriggerResultCallback, &param);
	if (dumpResult >= 0)
		dumpResult = sceKernelWaitEventFlag(eventFlag, 1, 1, NULL, NULL);
	result = sceKernelDeleteEventFlag(eventFlag);
	if (result == 0)
		result = dumpResult;
	return result;
}
