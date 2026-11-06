/*
 * SceLiveAreaUtil (vs0:sys/external/livearea_util.suprx): an application's LiveArea(TM) content,
 * replaced or updated by the shell (SceShellSvc's IPMI service). One replacement or update runs
 * at a time: its request is kept until sceLiveAreaGetStatus collects its result. The calls that
 * read the content wait for the shell's answer. Sony's file: vsh/app/shell/src/livearea_util/
 * client/livearea_util.cpp (0.945's build path).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <livearea.h>
#include "../../../../../lowlayer/src/ipmi/ipmi_error.h"
#include "../../../../../lowlayer/src/shell_svc/shell_svc_client.h"
#include "livearea_util_ipc_client.h"

/* the ShellSvc event the shell sets when a request is done */
#define LIVEAREA_UTIL_SHELL_EVENT	3

/* how long a synchronous call waits between looks at a running request (one frame) */
#define LIVEAREA_UTIL_BUSY_WAIT_USEC	16666

/*
 * No request: Sony's name (0.945's assert "IPMI_INVALID_TICKET!=gLiveAreaUtilContext.ticket";
 * that build kept the variables in one context, this one keeps them apart).
 */
#define IPMI_INVALID_TICKET			(-1)

/* the method sceLiveAreaGetStatus collects the running request's result of */
#define LIVEAREA_UTIL_KIND_REPLACE_ALL		1
#define LIVEAREA_UTIL_KIND_UPDATE_FRAME		2
#define LIVEAREA_UTIL_KIND_UPDATE_FRAME_LIST	3

static int s_ticket = IPMI_INVALID_TICKET;
static int s_requestKind = 0;
static bool s_isBgApp = false;
static SceKernelLwMutexWork s_lock;

extern "C" int module_start(SceSize args, const void *argp)
{
	s_isBgApp = false;
	return sceKernelCreateLwMutex(&s_lock, "SceLiveAreaUtilClient",
				      SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
				      SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
}

extern "C" SceInt32 sceLiveAreaGetStatus()
{
	int ret;

	sceKernelLockLwMutex(&s_lock, 1, NULL);
	if (s_ticket == IPMI_INVALID_TICKET) {
		sceKernelUnlockLwMutex(&s_lock, 1);
		return SCE_LIVEAREA_ERROR_STATE;
	}
	switch (s_requestKind) {
	case LIVEAREA_UTIL_KIND_REPLACE_ALL:
		ret = liveAreaUtilGetReplaceAllResult(SceShellSvc_B31E7F1C(), s_ticket);
		break;
	case LIVEAREA_UTIL_KIND_UPDATE_FRAME:
		ret = liveAreaUtilGetUpdateFrameResult(SceShellSvc_B31E7F1C(), s_ticket);
		break;
	case LIVEAREA_UTIL_KIND_UPDATE_FRAME_LIST:
		ret = liveAreaUtilGetUpdateFrameListResult(SceShellSvc_B31E7F1C(), s_ticket);
		break;
	default:
		ret = SCE_LIVEAREA_ERROR_STATE;
		break;
	}
	if (ret == IPMI_ERROR_WOULD_BLOCK) {
		sceKernelUnlockLwMutex(&s_lock, 1);
		return SCE_LIVEAREA_BUSY;
	}
	if (ret == IPMI_ERROR_INVALID_REQUEST_ID) {
		sceKernelUnlockLwMutex(&s_lock, 1);
		return SCE_LIVEAREA_ERROR_STATE;
	}
	s_ticket = IPMI_INVALID_TICKET;
	sceKernelUnlockLwMutex(&s_lock, 1);
	return ret;
}

/* waits for the running request */
extern "C" int module_stop(SceSize args, const void *argp)
{
	while (sceLiveAreaGetStatus() == SCE_LIVEAREA_BUSY)
		;
	sceKernelDelayThread(1000);
	sceKernelDeleteLwMutex(&s_lock);
	return 0;
}

/*
 * While another request runs, a synchronous call waits and an asynchronous one gives up; then the
 * request is sent, and a synchronous call waits for its result.
 */
static int replaceAll(const char *dirpath, const char *pOptionStr, const SceUInt64 *pOption,
		      bool fromSystem, bool sync)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	IPMI::EventFlagInfo *pEventFlag;
	SceUInt64 option;
	int ret;

	if (dirpath == NULL ||
	    sceClibStrnlen(dirpath, SCE_LIVEAREA_MAX_FS_PATH_LEN + 1) > SCE_LIVEAREA_MAX_FS_PATH_LEN)
		return SCE_LIVEAREA_ERROR_PARAM;
	sceKernelLockLwMutex(&s_lock, 1, NULL);
	while (s_ticket != IPMI_INVALID_TICKET) {
		if (sceLiveAreaGetStatus() == SCE_LIVEAREA_BUSY) {
			if (sync) {
				sceKernelUnlockLwMutex(&s_lock, 1);
				sceKernelDelayThread(LIVEAREA_UTIL_BUSY_WAIT_USEC);
				sceKernelLockLwMutex(&s_lock, 1, NULL);
			} else {
				sceKernelUnlockLwMutex(&s_lock, 1);
				return SCE_LIVEAREA_ERROR_STATE;
			}
		}
	}
	option = 0;
	client = SceShellSvc_B31E7F1C();
	pEventFlag = NULL;
	if (sync) {
		pEventFlag = &eventFlag;
		SceShellSvc_A0B067AC(&eventFlag, LIVEAREA_UTIL_SHELL_EVENT);
	}
	if (pOptionStr == NULL)
		pOptionStr = "";
	if (pOption == NULL)
		pOption = &option;
	ret = liveAreaUtilRequestReplaceAll(client, &s_ticket, pEventFlag, dirpath, pOptionStr,
					    pOption, fromSystem, s_isBgApp);
	if (ret == IPMI_ERROR_TOO_MANY_ASYNC_CALLS) {
		sceKernelUnlockLwMutex(&s_lock, 1);
		return SCE_LIVEAREA_ERROR_STATE;
	}
	s_requestKind = LIVEAREA_UTIL_KIND_REPLACE_ALL;
	sceKernelUnlockLwMutex(&s_lock, 1);
	if (sync) {
		ret = SceShellSvc_8D4F9F2E(LIVEAREA_UTIL_SHELL_EVENT, NULL);
		if (ret == 0)
			ret = sceLiveAreaGetStatus();
	}
	return ret;
}

extern "C" SceInt32 sceLiveAreaReplaceAllSync(const char *dirpath)
{
	return replaceAll(dirpath, NULL, NULL, false, true);
}

extern "C" SceInt32 sceLiveAreaReplaceAllAsync(const char *dirpath)
{
	return replaceAll(dirpath, NULL, NULL, false, false);
}

static int updateFrame(const char *formatVer, const char *frameXmlStr, SceInt32 frameXmlLen,
		       const char *dirpathTop, SceUInt32 flag, const char *pOptionStr,
		       bool fromSystem, bool sync)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	IPMI::EventFlagInfo *pEventFlag;
	int ret;

	if (formatVer == NULL ||
	    sceClibStrncmp(formatVer, SCE_LIVEAREA_FORMAT_VER_CURRENT,
			   sizeof(SCE_LIVEAREA_FORMAT_VER_CURRENT)) != 0 ||
	    dirpathTop == NULL ||
	    sceClibStrnlen(dirpathTop, SCE_LIVEAREA_MAX_FS_PATH_LEN + 1) >
	    SCE_LIVEAREA_MAX_FS_PATH_LEN ||
	    frameXmlStr == NULL || (flag & ~SCE_LIVEAREA_FLAG_TEMP) != 0)
		return SCE_LIVEAREA_ERROR_PARAM;
	if (frameXmlLen < 0)
		frameXmlLen = sceClibStrnlen(frameXmlStr, SCE_LIVEAREA_MAX_FRAME_STR_LEN + 1);
	if (frameXmlLen > SCE_LIVEAREA_MAX_FRAME_STR_LEN)
		return SCE_LIVEAREA_ERROR_XML_SIZE;
	sceKernelLockLwMutex(&s_lock, 1, NULL);
	while (s_ticket != IPMI_INVALID_TICKET) {
		if (sceLiveAreaGetStatus() == SCE_LIVEAREA_BUSY) {
			if (sync) {
				sceKernelUnlockLwMutex(&s_lock, 1);
				sceKernelDelayThread(LIVEAREA_UTIL_BUSY_WAIT_USEC);
				sceKernelLockLwMutex(&s_lock, 1, NULL);
			} else {
				sceKernelUnlockLwMutex(&s_lock, 1);
				return SCE_LIVEAREA_ERROR_STATE;
			}
		}
	}
	client = SceShellSvc_B31E7F1C();
	pEventFlag = NULL;
	if (sync) {
		pEventFlag = &eventFlag;
		SceShellSvc_A0B067AC(&eventFlag, LIVEAREA_UTIL_SHELL_EVENT);
	}
	if (pOptionStr == NULL)
		pOptionStr = "";
	ret = liveAreaUtilRequestUpdateFrame(client, &s_ticket, pEventFlag, formatVer,
					     frameXmlStr, frameXmlLen, dirpathTop, flag, pOptionStr,
					     fromSystem, s_isBgApp);
	if (ret == IPMI_ERROR_TOO_MANY_ASYNC_CALLS) {
		sceKernelUnlockLwMutex(&s_lock, 1);
		return SCE_LIVEAREA_ERROR_STATE;
	}
	s_requestKind = LIVEAREA_UTIL_KIND_UPDATE_FRAME;
	sceKernelUnlockLwMutex(&s_lock, 1);
	if (sync) {
		ret = SceShellSvc_8D4F9F2E(LIVEAREA_UTIL_SHELL_EVENT, NULL);
		if (ret == 0)
			ret = sceLiveAreaGetStatus();
	}
	return ret;
}

extern "C" SceInt32 sceLiveAreaUpdateFrameSync(const char *formatVer, const char *frameXmlStr,
					      SceInt32 frameXmlLen, const char *dirpathTop,
					      SceUInt32 flag)
{
	return updateFrame(formatVer, frameXmlStr, frameXmlLen, dirpathTop, flag, NULL, false,
			   true);
}

extern "C" SceInt32 sceLiveAreaUpdateFrameAsync(const char *formatVer, const char *frameXmlStr,
					       SceInt32 frameXmlLen, const char *dirpathTop,
					       SceUInt32 flag)
{
	return updateFrame(formatVer, frameXmlStr, frameXmlLen, dirpathTop, flag, NULL, false,
			   false);
}

/* the content's revision is the revision of the frame with no ID */
extern "C" SceInt32 sceLiveAreaGetRevision(SceInt64 *revision)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int ret;

	if (revision == NULL)
		return SCE_LIVEAREA_ERROR_PARAM;
	requestId = IPMI_INVALID_TICKET;
	client = SceShellSvc_B31E7F1C();
	SceShellSvc_A0B067AC(&eventFlag, LIVEAREA_UTIL_SHELL_EVENT);
	ret = liveAreaUtilRequestGetFrameRevision(client, &requestId, &eventFlag, "", s_isBgApp);
	if (ret != 0)
		return ret;
	ret = SceShellSvc_8D4F9F2E(LIVEAREA_UTIL_SHELL_EVENT, NULL);
	if (ret != 0)
		return ret;
	ret = liveAreaUtilGetFrameRevisionResult(SceShellSvc_B31E7F1C(), requestId, revision);
	if (ret != 0)
		return ret;
	return SCE_LIVEAREA_OK;
}

extern "C" SceInt32 sceLiveAreaGetFrameRevision(const char *frameId, SceInt64 *revision)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int ret;

	if (frameId == NULL || frameId[0] == '\0' || revision == NULL)
		return SCE_LIVEAREA_ERROR_PARAM;
	requestId = IPMI_INVALID_TICKET;
	client = SceShellSvc_B31E7F1C();
	SceShellSvc_A0B067AC(&eventFlag, LIVEAREA_UTIL_SHELL_EVENT);
	ret = liveAreaUtilRequestGetFrameRevision(client, &requestId, &eventFlag, frameId,
						  s_isBgApp);
	if (ret != 0)
		return ret;
	ret = SceShellSvc_8D4F9F2E(LIVEAREA_UTIL_SHELL_EVENT, NULL);
	if (ret != 0)
		return ret;
	ret = liveAreaUtilGetFrameRevisionResult(SceShellSvc_B31E7F1C(), requestId, revision);
	if (ret != 0)
		return ret;
	return SCE_LIVEAREA_OK;
}

extern "C" SceInt32 sceLiveAreaGetFrameUserData(const char *frameId, char *userData,
					       SceUInt32 userDataSize)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	SceUInt32 size;
	int requestId;
	int ret;

	if (frameId == NULL || userData == NULL || userDataSize == 0)
		return SCE_LIVEAREA_ERROR_PARAM;
	size = userDataSize;
	if (size > SCE_LIVEAREA_MAX_USER_DATA_LEN)
		size = SCE_LIVEAREA_MAX_USER_DATA_LEN + 1;
	requestId = IPMI_INVALID_TICKET;
	client = SceShellSvc_B31E7F1C();
	SceShellSvc_A0B067AC(&eventFlag, LIVEAREA_UTIL_SHELL_EVENT);
	ret = liveAreaUtilRequestGetFrameUserData(client, &requestId, &eventFlag, frameId, size,
						  s_isBgApp);
	if (ret == 0) {
		ret = SceShellSvc_8D4F9F2E(LIVEAREA_UTIL_SHELL_EVENT, NULL);
		if (ret == 0)
			ret = liveAreaUtilGetFrameUserDataResult(SceShellSvc_B31E7F1C(), requestId,
								 userData, &size);
	}
	return ret;
}

/* a background application's requests say so */
extern "C" int SceLiveAreaUtilBgApp_381D11E6(void)
{
	s_isBgApp = true;
	return 0;
}

/* the system applications' calls: the request carries their option string and value */
extern "C" int SceLiveAreaUtil_B0571F88(const char *dirpath, const char *pOptionStr,
					const SceUInt64 *pOption)
{
	return replaceAll(dirpath, pOptionStr, pOption, true, true);
}

extern "C" int SceLiveAreaUtil_EF2A3C3D(const char *dirpath, const char *pOptionStr,
					const SceUInt64 *pOption)
{
	return replaceAll(dirpath, pOptionStr, pOption, true, false);
}

extern "C" int SceLiveAreaUtil_009CD8E2(const char *formatVer, const char *frameXmlStr,
					SceInt32 frameXmlLen, const char *dirpathTop,
					SceUInt32 flag, const char *pOptionStr)
{
	return updateFrame(formatVer, frameXmlStr, frameXmlLen, dirpathTop, flag, pOptionStr, true,
			   true);
}

extern "C" int SceLiveAreaUtil_C1F7A931(const char *formatVer, const char *frameXmlStr,
					SceInt32 frameXmlLen, const char *dirpathTop,
					SceUInt32 flag, const char *pOptionStr)
{
	return updateFrame(formatVer, frameXmlStr, frameXmlLen, dirpathTop, flag, pOptionStr, true,
			   false);
}

/* several frames in one request, for the system applications only */
static int updateFrameList(const char *formatVer, const char **frameXmlStrList,
			   SceUInt32 frameNum, const char *dirpathTop, SceUInt32 flag,
			   const char *pOptionStr, bool sync)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	IPMI::EventFlagInfo *pEventFlag;
	int ret;

	if (formatVer == NULL ||
	    sceClibStrncmp(formatVer, SCE_LIVEAREA_FORMAT_VER_CURRENT,
			   sizeof(SCE_LIVEAREA_FORMAT_VER_CURRENT)) != 0 ||
	    dirpathTop == NULL ||
	    sceClibStrnlen(dirpathTop, SCE_LIVEAREA_MAX_FS_PATH_LEN + 1) >
	    SCE_LIVEAREA_MAX_FS_PATH_LEN ||
	    frameXmlStrList == NULL || frameNum == 0 || (flag & ~SCE_LIVEAREA_FLAG_TEMP) != 0)
		return SCE_LIVEAREA_ERROR_PARAM;
	sceKernelLockLwMutex(&s_lock, 1, NULL);
	while (s_ticket != IPMI_INVALID_TICKET) {
		if (sceLiveAreaGetStatus() == SCE_LIVEAREA_BUSY) {
			if (sync) {
				sceKernelUnlockLwMutex(&s_lock, 1);
				sceKernelDelayThread(LIVEAREA_UTIL_BUSY_WAIT_USEC);
				sceKernelLockLwMutex(&s_lock, 1, NULL);
			} else {
				sceKernelUnlockLwMutex(&s_lock, 1);
				return SCE_LIVEAREA_ERROR_STATE;
			}
		}
	}
	client = SceShellSvc_B31E7F1C();
	pEventFlag = NULL;
	if (sync) {
		pEventFlag = &eventFlag;
		SceShellSvc_A0B067AC(&eventFlag, LIVEAREA_UTIL_SHELL_EVENT);
	}
	if (pOptionStr == NULL)
		pOptionStr = "";
	ret = liveAreaUtilRequestUpdateFrameList(client, &s_ticket, pEventFlag, formatVer,
						 frameXmlStrList, frameNum, dirpathTop, flag,
						 pOptionStr, true);
	if (ret == IPMI_ERROR_TOO_MANY_ASYNC_CALLS) {
		sceKernelUnlockLwMutex(&s_lock, 1);
		return SCE_LIVEAREA_ERROR_STATE;
	}
	s_requestKind = LIVEAREA_UTIL_KIND_UPDATE_FRAME_LIST;
	sceKernelUnlockLwMutex(&s_lock, 1);
	if (sync) {
		ret = SceShellSvc_8D4F9F2E(LIVEAREA_UTIL_SHELL_EVENT, NULL);
		if (ret == 0)
			ret = sceLiveAreaGetStatus();
	}
	return ret;
}

extern "C" int SceLiveAreaUtil_31A5E6AB(const char *formatVer, const char **frameXmlStrList,
					SceUInt32 frameNum, const char *dirpathTop,
					SceUInt32 flag, const char *pOptionStr)
{
	return updateFrameList(formatVer, frameXmlStrList, frameNum, dirpathTop, flag, pOptionStr,
			       true);
}

extern "C" int SceLiveAreaUtil_876A649B(const char *formatVer, const char **frameXmlStrList,
					SceUInt32 frameNum, const char *dirpathTop,
					SceUInt32 flag, const char *pOptionStr)
{
	return updateFrameList(formatVer, frameXmlStrList, frameNum, dirpathTop, flag, pOptionStr,
			       false);
}
