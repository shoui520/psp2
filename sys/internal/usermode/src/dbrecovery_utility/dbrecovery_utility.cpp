/*
 * SceDbrecoveryUtility (vs0:sys/external/dbrecovery_utility.suprx): reports a damaged database
 * to the shell, which recovers it, and tells the user with a message dialog. No Sony record
 * names its functions; the file's name is the module's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <std/new>
#include <std/stdlib.h>
#include <framework.h>
#include <thread/thread.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "dbrecovery_client.h"

/* the application whose status says whether a dialog can be shown */
#define DBRECOVERY_STATUS_APP_ID	-3

/* the flags */
#define DBRECOVERY_FLAG_CHECK_APP	0x1	/* no dialog while the application is not running */
#define DBRECOVERY_FLAG_NO_DIALOG	0x2
#define DBRECOVERY_FLAG_ANY_DATABASE	0x4	/* report a database of any kind */

/* the databases a report is always made for */
#define DBRECOVERY_DB_11	11
#define DBRECOVERY_DB_14	14
#define DBRECOVERY_DB_24	24
#define DBRECOVERY_DB_26	26

/* the dialog's text, a resource ID of the common dialog's */
#define DBRECOVERY_MESSAGE_ID	0x611188eb

static void showRecoveryDialog(void *arg);

extern "C" int SceDbrecoveryUtility_4AB1DAA9(SceInt32 kind, const char *path, SceUInt32 flags)
{
	SceAppMgrAppStatus status;
	CommonGuiMessageDialogParam *param;
	int result;

	if (path == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (kind != DBRECOVERY_DB_11 && kind != DBRECOVERY_DB_14 && kind != DBRECOVERY_DB_24 &&
	    kind != DBRECOVERY_DB_26 && (flags & DBRECOVERY_FLAG_ANY_DATABASE) == 0)
		return 0;
	if ((flags & DBRECOVERY_FLAG_CHECK_APP) != 0) {
		int ret = sceAppMgrGetStatusByAppId(DBRECOVERY_STATUS_APP_ID, &status);

		sce_paf_printf_dummy();
		if (ret != 0)
			flags |= DBRECOVERY_FLAG_NO_DIALOG;
		else
			flags &= ~DBRECOVERY_FLAG_NO_DIALOG;
	}
	result = DbRecoveryReportCorruption(SceShellSvc_B31E7F1C(), path, flags, kind);
	if (paf::Framework::Instance() != NULL && (flags & DBRECOVERY_FLAG_NO_DIALOG) == 0 &&
	    paf::Framework::Instance() != NULL) {
		param = new CommonGuiMessageDialogParam;
		param->type = kind;
		param->defaultButton = 1;
		param->messageId = DBRECOVERY_MESSAGE_ID;
		paf::thread::CallQueue::main_thread_callqueue.Enqueue(NULL, showRecoveryDialog, param);
	}
	return result;
}

static void showRecoveryDialog(void *arg)
{
	CommonGuiMessageDialogParam *param = (CommonGuiMessageDialogParam *)arg;

	SceCommonGuiDialog_20B141F1(param);
	delete param;
}

extern "C" int module_start(SceSize args, const void *argp)
{
	return 0;
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	return 0;
}
