/*
 * np_imps_core_notif.cpp: SceNpImpsNotification listener service
 * (0x8131F38A..0x8131F65B).
 *
 * Bridges SceNpImpsCtl session online/offline transitions and incoming
 * http://playstation.net/np/rt/notification text messages to shell UI
 * notification listeners.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <np/np_common.h>
#include "np_imps_core.h"

static SceKernelLwMutexWork s_notifMutex;
static int s_notifMutexInit = 0;
static int s_notifInitialized = 0;
static int s_notifCoreCallbackId = 0;
static SceUInt8 s_notifOnlineFlag = 0;
static NpImpsNotifMessageCallback s_notifMessageCallback = NULL;
static NpImpsNotifOnlineCallback s_notifOnlineCallback = NULL;

/* 0x8131F5B8 */
static int npImpsNotifMutexCreate(void)
{
	if (s_notifMutexInit > 0)
		return NP_IMPS_NOTIF_ERR_ALREADY_INIT;
	int ret = sceKernelCreateLwMutex(&s_notifMutex, "SceNpImpsNotification", 0x2002u, 0, NULL);
	if (ret >= 0)
		s_notifMutexInit = 1;
	return ret;
}

/* 0x8131F608 */
static int npImpsNotifMutexDelete(void)
{
	if (s_notifMutexInit >= 0)
		return sceKernelDeleteLwMutex(&s_notifMutex);
	return s_notifMutexInit;
}

/* 0x8131F626 */
static int npImpsNotifMutexLock(void)
{
	int ret = sceKernelLockLwMutex(&s_notifMutex, 1, NULL);
	if (ret >= 0)
		return 1;
	return ret;
}

/* 0x8131F642 */
static int npImpsNotifMutexUnlock(int lockState)
{
	if (lockState > 0)
		sceKernelUnlockLwMutex(&s_notifMutex, 1);
	return 0;
}

/* 0x8131F584 */
static int npImpsNotifCheckInit(void)
{
	return (s_notifInitialized != 0) ? 0 : NP_IMPS_NOTIF_ERR_NOT_INITIALIZED;
}

/* 0x8131F38A */
static void npImpsNotifDispatchMessage(const NpImpsSessionIdentity *session,
				       const SceNpId *senderNpId, const NpImpsCoreMessage *msg)
{
	(void)session;
	char textBuf[128];
	sceClibMemset(textBuf, 0, sizeof(textBuf));
	const char *text = msg->notificationExt->text;
	if (text != NULL)
		sceClibStrncpy(textBuf, text, sizeof(textBuf) - 1);

	int lockState = npImpsNotifMutexLock();
	if (s_notifMessageCallback != NULL)
		s_notifMessageCallback(senderNpId, textBuf);
	npImpsNotifMutexUnlock(lockState);
}

/* 0x8131F59E */
static void npImpsNotifOnMessageRecv(const NpImpsSessionIdentity *session,
				     const SceNpId *senderNpId, const NpImpsCoreMessage *msg)
{
	if (msg->notificationExt != NULL && msg->notificationExt->kind == 0)
		npImpsNotifDispatchMessage(session, senderNpId, msg);
}

/* 0x8131F45A / 0x8131F5AE */
static void npImpsNotifOnSessionState(const NpImpsSessionIdentity *session, int sessionState,
				      int errorCode)
{
	(void)session;
	(void)errorCode;

	switch (sessionState) {
	case NP_IMPS_EVT_ONLINE:
		if (s_notifOnlineFlag == 0) {
			int lockState = npImpsNotifMutexLock();
			if (s_notifOnlineCallback != NULL)
				s_notifOnlineCallback(0);
			npImpsNotifMutexUnlock(lockState);
			s_notifOnlineFlag = 1;
		}
		break;

	case NP_IMPS_EVT_POLL_INTERVAL:
	case NP_IMPS_EVT_POLL_CRON:
	case NP_IMPS_EVT_SUSPENDED:
	case NP_IMPS_EVT_STOPPING:
	case NP_IMPS_EVT_STOPPED:
		if (s_notifOnlineFlag != 0) {
			int lockState = npImpsNotifMutexLock();
			if (s_notifOnlineCallback != NULL)
				s_notifOnlineCallback(1);
			npImpsNotifMutexUnlock(lockState);
			s_notifOnlineFlag = 0;
		}
		break;

	default:
		break;
	}
}

/* 0x8131F3E8 */
int npImpsNotifRegisterOnlineCallback(NpImpsNotifOnlineCallback callback)
{
	int ret = npImpsNotifCheckInit();
	if (ret < 0)
		return ret;

	int lockState = npImpsNotifMutexLock();
	int status = NP_IMPS_NOTIF_ERR_CALLBACK_SET;
	if (s_notifOnlineCallback == NULL) {
		s_notifOnlineCallback = callback;
		status = 0;
	}
	npImpsNotifMutexUnlock(lockState);
	return status;
}

/* 0x8131F422 */
int npImpsNotifUnregisterOnlineCallback(void)
{
	int ret = npImpsNotifCheckInit();
	if (ret < 0)
		return ret;

	int lockState = npImpsNotifMutexLock();
	int status = NP_IMPS_NOTIF_ERR_NO_CALLBACK;
	if (s_notifOnlineCallback != NULL) {
		s_notifOnlineCallback = NULL;
		status = 0;
	}
	npImpsNotifMutexUnlock(lockState);
	return status;
}

/* 0x8131F4D4 */
int npImpsNotifInit(void)
{
	if (s_notifInitialized != 0)
		return NP_IMPS_NOTIF_ERR_ALREADY_INIT;

	int ret = npImpsNotifMutexCreate();
	if (ret < 0)
		return ret;

	NpImpsCoreCallbacks callbacks;
	sceClibMemset(&callbacks, 0, sizeof(callbacks));
	callbacks.onSessionState = npImpsNotifOnSessionState;
	callbacks.onMessageRecv = npImpsNotifOnMessageRecv;

	ret = npImpsCoreRegisterCallbacks(&callbacks, &s_notifCoreCallbackId);
	if (ret < 0) {
		npImpsNotifMutexDelete();
		return ret;
	}

	s_notifInitialized = 1;
	return 0;
}

/* 0x8131F560 */
int npImpsNotifTerm(void)
{
	if (s_notifInitialized == 0)
		return 0;
	npImpsCoreUnregisterCallbacks(s_notifCoreCallbackId);
	return npImpsNotifMutexDelete();
}
