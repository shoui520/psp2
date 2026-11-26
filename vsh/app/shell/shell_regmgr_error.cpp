/*
 * shell_regmgr_error.cpp: registry error reporter that checks whether a
 * registry error requires a blue-screen or error notification and dispatches
 * SceShellUtil_BB54D049 on the PAF main thread (0x81345038..0x813450BB).
 */
#include <stddef.h>
#include <scetypes.h>
#include <thread/thread.h>

extern "C" int sceRegMgrIsBlueScreenForUser(int error);
extern "C" int SceShellUtil_BB54D049(int mode, int error, int option);

#define SHELL_UTIL_NOTIFY_BLUE_SCREEN	4
#define SHELL_UTIL_NOTIFY_ERROR_DIALOG	5

struct ShellRegMgrErrorRequest {
	int errorCode;
	int showErrorDialog;
};

/* 0x8134508E */
static int shellRegMgrLogErrorSyncCallback(void *userContext)
{
	ShellRegMgrErrorRequest *req = static_cast<ShellRegMgrErrorRequest *>(userContext);
	int notifyMode;

	if (sceRegMgrIsBlueScreenForUser(req->errorCode) == 1) {
		notifyMode = SHELL_UTIL_NOTIFY_BLUE_SCREEN;
	} else {
		if (req->showErrorDialog != 1)
			return 0;
		notifyMode = SHELL_UTIL_NOTIFY_ERROR_DIALOG;
	}

	SceShellUtil_BB54D049(notifyMode, req->errorCode, 0);
	return 0;
}

/* 0x81345038 */
void shellRegMgrLogError(int errorCode, int showErrorDialog)
{
	ShellRegMgrErrorRequest *req = new ShellRegMgrErrorRequest();
	if (req != NULL) {
		req->errorCode = errorCode;
		req->showErrorDialog = showErrorDialog;
	}

	if (!paf::thread::ThreadIDCache::Check(paf::thread::ThreadIDCache::Type_Main)) {
		paf::thread::SyncCall::main_thread_synccall.Call(
			shellRegMgrLogErrorSyncCallback, req);
		return;
	}

	int notifyMode;
	if (sceRegMgrIsBlueScreenForUser(req->errorCode) == 1) {
		notifyMode = SHELL_UTIL_NOTIFY_BLUE_SCREEN;
	} else {
		if (req->showErrorDialog != 1)
			return;
		notifyMode = SHELL_UTIL_NOTIFY_ERROR_DIALOG;
	}

	SceShellUtil_BB54D049(notifyMode, req->errorCode, 0);
}
