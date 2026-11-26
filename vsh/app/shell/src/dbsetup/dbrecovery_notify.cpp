/*
 * dbrecovery_notify.cpp: database corruption notification, fatal/non-fatal
 * recovery error dialogs, and SceShellSvc IPMI 0x00150000 dispatch
 * (vsh/app/common/src/dbrecovery, 0x81371828..0x81371C6F).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <framework.h>
#include <thread/thread.h>
#include <ipmi/session.h>
#include <common_gui_dialog/common_gui_dialog.h>

#include "../../shell.h"

/* (0x81360836) in dbrecovery_lib.cpp */
int DbCheckClearCorruptByPath(const char *path);

#define DBRECOVERY_SQLITE_CORRUPT		11
#define DBRECOVERY_SQLITE_CANTOPEN		14
#define DBRECOVERY_SQLITE_FORMAT		24
#define DBRECOVERY_SQLITE_NOTADB		26

#define DBRECOVERY_FLAG_CHECK_APP_STATUS	0x00000001u
#define DBRECOVERY_FLAG_FATAL_REBUILD		0x00000002u
#define DBRECOVERY_FLAG_FORCE_REPORT		0x00000004u

#define DBRECOVERY_APP_ID_CURRENT_GAME		(-3)
#define DBRECOVERY_APP_ID_CALLER		(-2)

#define DBRECOVERY_ERROR_INVALID_PATH		(-2147418090)	/* 0x80010016 */
#define DBRECOVERY_IPMI_ERROR_UNSUPPORTED	(-2147351103)	/* 0x800205C1 */
#define DBRECOVERY_IPMI_METHOD_REPORT		0x00150000u

#define DBRECOVERY_MSG_SOFT_ERROR		0x61118CEBu	/* 1628539115 */
#define DBRECOVERY_MSG_FATAL_REBUILD		0x7F3859E2u	/* 2134370146 */

struct DbRecoveryIpmiReportHeader {
	SceUInt32 flags;
	SceInt32 error;
};

extern "C" {
int sceAppMgrGetStatusByAppId(SceUID appId, void *statusBuf);
}

extern int shellSvcLiveAreaSyncDefault(IPMI::Session *session, SceUInt32 methodId,
				       const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				       IPMI::BufferInfo *outBufs, SceUInt32 outCount);

/* (0x815C6FE0 / 0x81371BEC) Static message buffer used by recovery dialogs */
static paf::wstring s_dbRecoveryMessage;

/* (0x8137196C) Main-thread callback for non-fatal DB error dialog */
static void openSoftDbErrorDialog(void *arg)
{
	CommonGuiMessageDialogParam *param = static_cast<CommonGuiMessageDialogParam *>(arg);
	SceCommonGuiDialog_20B141F1(param);
	delete param;
}

/* (0x81371B30) Main-thread callback for fatal DB rebuild dialog */
static void openFatalDbErrorDialog(void *arg)
{
	CommonGuiMessageDialogParam *param = static_cast<CommonGuiMessageDialogParam *>(arg);
	shellErrorDialogOpen(2, param, false);
	delete param;
}

/* (0x81371A24) Sony name: DbRecoveryReportCorruption */
int DbRecoveryReportCorruption(const char *path, SceUInt32 flags, SceInt32 error, SceInt32 appId)
{
	int ret = DbCheckClearCorruptByPath(path);
	if (ret != 0)
		return ret;

	if ((flags & DBRECOVERY_FLAG_FATAL_REBUILD) != 0 &&
	    paf::Framework::Instance() != NULL) {
		CommonGuiMessageDialogParam *param = new CommonGuiMessageDialogParam();
		param->buttonType = appId;
		param->type = error;
		param->defaultButton = 1;
		param->messageId = DBRECOVERY_MSG_FATAL_REBUILD;
		paf::thread::CallQueue::main_thread_callqueue.Enqueue(
			NULL, openFatalDbErrorDialog, param);
	}

	return 0;
}

/* (0x81371828) Reports a database error and schedules the appropriate recovery dialog */
int shellDbRecoveryNotify(int error, const char *path, unsigned int flags)
{
	if (path == NULL)
		return DBRECOVERY_ERROR_INVALID_PATH;

	if (error != DBRECOVERY_SQLITE_CORRUPT &&
	    error != DBRECOVERY_SQLITE_CANTOPEN &&
	    error != DBRECOVERY_SQLITE_FORMAT &&
	    error != DBRECOVERY_SQLITE_NOTADB &&
	    (flags & DBRECOVERY_FLAG_FORCE_REPORT) == 0) {
		return 0;
	}

	unsigned int effectiveFlags = flags;
	if ((effectiveFlags & DBRECOVERY_FLAG_CHECK_APP_STATUS) != 0) {
		char appStatus[128];
		int statusRet = sceAppMgrGetStatusByAppId(DBRECOVERY_APP_ID_CURRENT_GAME, appStatus);
		if (statusRet == 0)
			effectiveFlags &= ~DBRECOVERY_FLAG_FATAL_REBUILD;
		else
			effectiveFlags |= DBRECOVERY_FLAG_FATAL_REBUILD;
	}

	int reportRet = DbRecoveryReportCorruption(
		path, effectiveFlags, error, DBRECOVERY_APP_ID_CALLER);

	if (paf::Framework::Instance() != NULL &&
	    (effectiveFlags & DBRECOVERY_FLAG_FATAL_REBUILD) == 0 &&
	    paf::Framework::Instance() != NULL) {
		CommonGuiMessageDialogParam *param = new CommonGuiMessageDialogParam();
		param->type = error;
		param->defaultButton = 1;
		param->messageId = DBRECOVERY_MSG_SOFT_ERROR;
		paf::thread::CallQueue::main_thread_callqueue.Enqueue(
			NULL, openSoftDbErrorDialog, param);
	}

	return reportRet;
}

/* (0x81371AF0) IPMI session helper for reporting DB corruption */
static int dbRecoveryReportFromSession(IPMI::Session *session, const char *path,
				       SceUInt32 flags, SceInt32 error)
{
	SceInt32 appId = session->getAppId();
	return DbRecoveryReportCorruption(path, flags, error, appId);
}

/* (0x81371C34) Sync dispatcher for SceShellSvc 0x00150000 */
static int dbRecoveryDispatchSync(IPMI::Session *session, SceUInt32 methodId,
				  const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				  IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	if (methodId != DBRECOVERY_IPMI_METHOD_REPORT)
		return DBRECOVERY_IPMI_ERROR_UNSUPPORTED;

	const DbRecoveryIpmiReportHeader *hdr =
		static_cast<const DbRecoveryIpmiReportHeader *>(inBufs[0].pData);
	const char *dbPath = static_cast<const char *>(inBufs[1].pData);
	int result = dbRecoveryReportFromSession(session, dbPath, hdr->flags, hdr->error);
	return session->respondToSyncMethodRequest(result, outBufs, outCount);
}

/* (0x81371B0C) Registered in SceShellSvc dispatch table for 0x00150000..0x0015FFFF */
int shellSvcDisp_15_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inBufs, SceUInt32 inCount,
			 IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	return dbRecoveryDispatchSync(session, methodId, inBufs, inCount, outBufs, outCount);
}

/* (0x81371B20) Registered in SceShellSvc dispatch table for 0x00150000..0x0015FFFF */
int shellSvcDisp_15_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inBufs, SceUInt32 inCount)
{
	(void)reqId;
	return shellSvcLiveAreaSyncDefault(session, methodId, inBufs, inCount, NULL, 0);
}
