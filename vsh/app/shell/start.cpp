/*
 * The shell's start: the time it started, the framework and the start job that loads the system
 * modules and starts the shell's services, then the cold-boot plugin and the framework's run.
 * When the job is done the start's error, if any, is shown, else the system plugins are loaded.
 * The allocator given to bXCe takes paf's heap. The file's and every name here are ours (no
 * record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string>
#include <framework.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <common/main_thread_call_list.h>
#include <dir/dir.h>
#include <system/system.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <shell_svc/shellsvc.h>
#include <shell_svc/shell_util.h>
#include <bXCe/bxce_internal.h>

#include "boot.h"
#include "shell.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* the application data mounted at start */
#define SHELL_APPDATA_MOUNT_ID		105

/* ux0:data's mode */
#define SHELL_DATA_DIR_MODE		0x186

/* the start ends here when the id storage does not say test mode and the kit is not a test
   kit (value only: no name known) */
#define SHELL_START_ERROR_NOT_TEST	0x80AF0002

/* the error dialog's types: the error is a blue screen, or another start error */
#define SHELL_ERROR_DIALOG_BLUE_SCREEN	4
#define SHELL_ERROR_DIALOG_START	5

/* the common GUI dialog value set after the start (value only) */
#define SHELL_COMMON_GUI_DIALOG_VALUE	38

/* paf's model type of the PlayStation TV (value only) */
#define SHELL_MODEL_TYPE_1		1

static SceUInt64 s_startTime = 0;
static int s_startupError = 0;

/* bXCe's memory: paf's heap */
class ShellXmlAllocator : public bXCe::MemAllocator {
public:
	ShellXmlAllocator()
	{
	}

	virtual ~ShellXmlAllocator()
	{
	}

	virtual void *allocate(size_t size, void *userData);

	virtual void deallocate(void *ptr, void *userData)
	{
		sce_paf_free(ptr);
	}
};

/* the message the error dialog is given (empty) */
static wstring s_errorMessage;
static ShellXmlAllocator s_xmlAllocator;
static ScePfsRndDriveId s_appDataMountPoint;

/* the services' start, run by the job queue */
class ShellStartJob : public job::JobItem {
public:
	ShellStartJob() : job::JobItem(NULL, NULL)
	{
	}

	virtual ~ShellStartJob()
	{
	}

	virtual int Run();
	virtual void Finish(int result);
};

/* the shell service's server and the shell utility's events */
__attribute__((noinline)) static int shellSvcStart(void)
{
	shellSvcServerCreate();
	shellSvcServerStart();
	return SceShellSvc_E91AEAD4();
}

int main(int argc, char *argv[])
{
	s_startTime = SceThreadmgr_47F6DE49();
	sceClibPrintf("[Shell]: vsh/release_branches/release_03.650 r142704 (build#757166)\n");
	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	s_startupError = shellStartupInit();
	shellFrameworkCreate();
	shellLsdbRegCallbackCreate();
	shellMtpRegCallbackCreate();

	{
		ShellStartJob *item = new ShellStartJob();
		common::SharedPtr<job::JobItem> job(item);

		job::JobQueue::default_queue->Enqueue(job);
		job::JobQueue::CheckAllQueues();
	}

	g_shellFramework->LoadCommonResourceSync(NULL);
	g_shellFramework->InitCommonSE();
	shellBootLoadColdbootPlugin();
	shellFrameworkRun();
	return 0;
}

int shellStartAppDataMount(void)
{
	return sceAppMgrAppDataMount(SHELL_APPDATA_MOUNT_ID, &s_appDataMountPoint);
}

void *ShellXmlAllocator::allocate(size_t size, void *userData)
{
	return sce_paf_malloc(size);
}

int shellStartAppDataUmount(void)
{
	return sceAppMgrUmount(s_appDataMountPoint.drive_id);
}

SceUInt64 shellStartGetTime(void)
{
	return s_startTime;
}

/* run on the main thread after the start: callbacks, and the shell utility's events */
static void shellStartPoll(void *userdata)
{
	sceKernelCheckCallback();
	SceShellUtilUketorne_FDC48BBE();
}

/* what the start does only on a model other than the PlayStation TV (nothing in this build) */
__attribute__((noinline)) static void shellStartHandheldInit(void)
{
}

int ShellStartJob::Run()
{
	int ret = shellModulesLoad();

	if (ret != 0)
		return ret;

	if (shellStartupNeedsInitialSetup()) {
		Dir::Create("ur0:app");
		Dir::Create("ur0:shell");
		Dir::Create("ur0:shell/tabpaper");
		shellConsoleInfoRecord(0, -1, 0);
	}
	Dir::Create("ux0:data", SHELL_DATA_DIR_MODE);
	shellStartAppDataMount();
	shellClosedCaptionMigrate();
	shellSvcStart();
	sceShellUtilInitEvents(1);
	ScebXCe_EB4E6C8D(&s_xmlAllocator, NULL);
	shellDbCheckAll();
	shellNetStart();
	shellLsdbStart();
	shellTriggerMgrStart();
	shellNpStart();
	shellActivityDbOpen();
	shellShutterSoundInit();
	shellNpServicesStart();
	shellImeServiceStart();
	if (system::Model::GetType() != SHELL_MODEL_TYPE_1)
		shellStartHandheldInit();
	shellNpNotifyStart();
	shellMtpStart();
	shellMtpMarlinStart();
	shellNoticeServiceStart();
	shellIncomingDialogServerCreate();
	ScePafToplevel_397DD062(g_shellFramework, "vs0:data/internal/effects", 7);
	if (shellIsComTestMode(NULL) == 0 && vshSblUtMgrHasComTestFlag() == 0)
		return SHELL_START_ERROR_NOT_TEST;
	shellDatabaseCheckRecovery();
	shellThemeLoad();
	return ret;
}

void ShellStartJob::Finish(int result)
{
	if (result != 0)
		s_startupError = result;
	SceCommonGuiDialog_5057376C(SHELL_COMMON_GUI_DIALOG_VALUE);
	shellStateWatcherCreate();
	shellAccessibilityMgrCreate();
	shellHandlerTableCreate();
	shellUtilServerMgrCreate();
	common::MainThreadCallList::Register(shellStartPoll, NULL);
	if (s_startupError == 0) {
		shellMainThreadPollerCreate();
		shellBootStartSystemEntry();
	} else {
		int type = SHELL_ERROR_DIALOG_START;

		if (sceRegMgrIsBlueScreen(s_startupError) != 0)
			type = SHELL_ERROR_DIALOG_BLUE_SCREEN;
		shellErrorDialogShow(type, s_startupError, &s_errorMessage, 0);
	}
	sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
}
