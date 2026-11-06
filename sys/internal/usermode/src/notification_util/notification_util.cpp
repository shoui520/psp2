/*
 * SceNotificationUtil (vs0:sys/external/notification_util.suprx): notifications, and progress
 * notifications, sent to the shell (SceShellSvc's IPMI service). Every call holds the module's
 * lock and waits for the shell's answer. No Sony record names its files or internal functions;
 * the file's name is the module's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "notification_util.h"
#include "notification_util_ipc_client.h"

/* the ShellSvc event the shell sets when a request is done */
#define NOTIFICATION_UTIL_SHELL_EVENT		1
/* the ShellSvc event of the progress notification, for its event handler */
#define NOTIFICATION_UTIL_PROGRESS_EVENT	52

static bool s_isBgApp;
static SceNotificationUtilProgressEventHandler s_progressEventHandler;
static SceKernelLwMutexWork s_lock;

static int progressEventCallback(int event, void *pArg);

/* holds the module's lock while in scope */
class ScopedLock {
public:
	explicit ScopedLock(SceKernelLwMutexWork *pWork) : m_pWork(pWork)
	{
		sceKernelLockLwMutex(m_pWork, 1, NULL);
	}

	~ScopedLock()
	{
		sceKernelUnlockLwMutex(m_pWork, 1);
	}

private:
	SceKernelLwMutexWork *m_pWork;
};

extern "C" int module_start(SceSize args, const void *argp)
{
	s_isBgApp = false;
	return sceKernelCreateLwMutex(&s_lock, "SceNotificationUtilClient",
				      SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
				      SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	sceKernelDeleteLwMutex(&s_lock);
	return 0;
}

extern "C" int sceNotificationUtilSendNotification(const SceNotificationUtilSendParam *pParam)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	const SceWChar16 *p;
	int requestId;
	SceUInt32 length;
	SceUInt32 i;
	int hasNewline;
	int ret;

	ScopedLock lock(&s_lock);

	if (pParam == NULL)
		goto invalid;
	length = 0;
	for (p = pParam->text; *p++ != 0;) {
		if (++length >= SCE_NOTIFICATION_UTIL_TEXT_LENGTH)
			break;
	}
	if (length == 0 || length >= SCE_NOTIFICATION_UTIL_TEXT_LENGTH)
		goto invalid;
	hasNewline = 0;
	for (i = 0; i < length; i++) {
		int c = pParam->text[i];

		if ((c >= 0x00 && c <= 0x1f) || c == 0x7f) {
			if (hasNewline || c != '\n')
				goto invalid;
			hasNewline = 1;
		}
	}
	if (sceClibStrnlen(pParam->data, SCE_NOTIFICATION_UTIL_DATA_SIZE) >=
	    SCE_NOTIFICATION_UTIL_DATA_SIZE ||
	    pParam->reserved[0] != 0 || pParam->reserved[1] != 0)
		goto invalid;

	requestId = -1;
	client = SceShellSvc_B31E7F1C();
	SceShellSvc_A0B067AC(&eventFlag, NOTIFICATION_UTIL_SHELL_EVENT);
	ret = notificationUtilRequestSend(client, &requestId, &eventFlag, pParam, s_isBgApp);
	if (ret == 0) {
		ret = SceShellSvc_8D4F9F2E(NOTIFICATION_UTIL_SHELL_EVENT, NULL);
		if (ret == 0)
			ret = notificationUtilGetSendResult(SceShellSvc_B31E7F1C(), requestId);
	}
	return ret;

invalid:
	return SCE_NOTIFICATION_UTIL_ERROR_INVALID_ARGUMENT;
}

/* the shell's answer is not returned */
extern "C" void sceNotificationUtilCleanHistory(void)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	int requestId;

	ScopedLock lock(&s_lock);

	requestId = -1;
	client = SceShellSvc_B31E7F1C();
	SceShellSvc_A0B067AC(&eventFlag, NOTIFICATION_UTIL_SHELL_EVENT);
	if (notificationUtilRequestCleanHistory(client, &requestId, &eventFlag, s_isBgApp) == 0 &&
	    SceShellSvc_8D4F9F2E(NOTIFICATION_UTIL_SHELL_EVENT, NULL) == 0)
		notificationUtilGetCleanHistoryResult(SceShellSvc_B31E7F1C(), requestId);
}

extern "C" int sceNotificationUtilProgressBegin(const SceNotificationUtilProgressBeginParam *pParam)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int ret;

	ScopedLock lock(&s_lock);

	requestId = -1;
	client = SceShellSvc_B31E7F1C();
	SceShellSvc_A0B067AC(&eventFlag, NOTIFICATION_UTIL_SHELL_EVENT);
	ret = notificationUtilRequestProgressBegin(client, &requestId, &eventFlag, pParam,
						   s_isBgApp);
	if (ret == 0) {
		ret = SceShellSvc_8D4F9F2E(NOTIFICATION_UTIL_SHELL_EVENT, NULL);
		if (ret == 0) {
			ret = notificationUtilGetProgressBeginResult(SceShellSvc_B31E7F1C(),
								     requestId);
			if (ret == 0) {
				s_progressEventHandler = pParam->eventHandler;
				SceShellSvc_D943CE15(NOTIFICATION_UTIL_PROGRESS_EVENT,
						     progressEventCallback,
						     pParam->pEventHandlerArg);
			}
		}
	}
	return ret;
}

extern "C" int sceNotificationUtilProgressUpdate(const SceNotificationUtilProgressUpdateParam *pParam)
{
	int ret;

	ScopedLock lock(&s_lock);

	ret = notificationUtilProgressUpdate(SceShellSvc_B31E7F1C(), pParam);
	SceShellSvc_FB03FBC9(NOTIFICATION_UTIL_PROGRESS_EVENT);
	return ret;
}

extern "C" int sceNotificationUtilProgressFinish(const SceNotificationUtilProgressFinishParam *pParam)
{
	IPMI::Client *client;
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int ret;

	ScopedLock lock(&s_lock);

	requestId = -1;
	client = SceShellSvc_B31E7F1C();
	SceShellSvc_A0B067AC(&eventFlag, NOTIFICATION_UTIL_SHELL_EVENT);
	ret = notificationUtilRequestProgressFinish(client, &requestId, &eventFlag, pParam);
	if (ret == 0) {
		ret = SceShellSvc_8D4F9F2E(NOTIFICATION_UTIL_SHELL_EVENT, NULL);
		if (ret == 0) {
			ret = notificationUtilGetProgressFinishResult(SceShellSvc_B31E7F1C(),
								      requestId);
			if (ret == 0) {
				SceShellSvc_7CBF442B(NOTIFICATION_UTIL_PROGRESS_EVENT);
				s_progressEventHandler = NULL;
			}
		}
	}
	return ret;
}

/* a background application's notifications say so */
extern "C" int sceNotificationUtilBgAppInitialize(void)
{
	s_isBgApp = true;
	return 0;
}

/* the progress event's ShellSvc callback: the handler gets its argument */
static int progressEventCallback(int event, void *pArg)
{
	return s_progressEventHandler(pArg);
}
