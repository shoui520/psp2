/*
 * SceTrophySetupDialogPlugin (vs0:sys/external/libcdlg_np_trophy_setup.suprx): the trophy setup
 * dialog's plugin, which SceCommonDialogMain loads for libcdlg's sceNpTrophySetupDialog*. It
 * opens the application's trophy title for the caller's context on a job thread, asks before it
 * removes an installed title the user has completed, and sets the title up, with a processing
 * dialog while it works when the caller asks for one. The names the plugin writes into the work
 * memory (CommonDialogTrophySetupParam's apiName) are Sony's function names.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <np_trophy_setup_dialog.h>
#include <plugin.h>
#include <module/module.h>
#include <job/job.h>
#include <common/main_thread_call_list.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "np_trophy_setup_dialog_plugin.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/* the trophy error the title's state was read with an expired session: the dialog opens it
   again (name ours) */
#define TROPHY_SETUP_ERROR_SESSION_EXPIRED	-2141907265	/* 0x805516BF */

/*
 * The work memory, locked for one of the plugin's functions under the function's name, as
 * libcdlg's own calls lock it (CommonDialogApiScope): ret is the lock's error, then the first
 * error of the call.
 */
class TrophySetupPluginApiScope {
public:
	TrophySetupPluginApiScope(const char *name, int &ret) : m_ret(ret), m_name(name), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_TROPHY_SETUP);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogTrophySetupParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_TROPHY_SETUP);
		if (m_work == NULL) {
			ret = TROPHY_SETUP_PLUGIN_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TROPHY_SETUP);
			return;
		}
		sceClibStrncpy(m_work->apiName, name, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~TrophySetupPluginApiScope()
	{
		int ret;

		if (m_work == NULL)
			return;
		if (sceClibStrncmp(m_work->apiName, m_name, sizeof(m_work->apiName)) != 0) {
			ret = TROPHY_SETUP_PLUGIN_ERROR_API_NAME;
		} else {
			sceClibMemset(m_work->apiName, 0, sizeof(m_work->apiName));
			ret = SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TROPHY_SETUP);
			if (ret == SCE_OK)
				return;
		}
		if (m_ret == SCE_OK)
			m_ret = ret;
	}

	CommonDialogTrophySetupParam *work(void) const
	{
		return m_work;
	}

private:
	int &m_ret;
	const char *m_name;
	CommonDialogTrophySetupParam *m_work;
};

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

/* the caller asked for the processing dialog */
/* the empty name of the plugin's resource IDs: paf's headers declare it weak, so each module that
   builds an IDParam from it keeps its own copy (Sony's code tests it for NULL at every use) */
const char paf::s_empty_name[] = "";

static bool s_showProcessingDialog;
static job::JobQueue *s_jobQueue;
static SetupController *s_setupController;
static Plugin::ModuleInterface s_moduleInterface;

static void trophySetupDialogInit(SceSize argSize, const void *argp);
static void trophySetupDialogTerm(void);
static void InitJobQueue(Plugin *plugin);
static int InitSetupController(void);
static void SetTrophySetupResult(SceInt32 result);
static void trophySetupDialogStart(Plugin *plugin);
static void trophySetupDialogStop(Plugin *plugin);
static void trophySetupDialogExit(Plugin *plugin);
static void trophySetupDialogSetParam(Plugin::InitParam2 *param);
static void trophySetupDialogUpdate(void *userdata);
static void TrophyOpenJobFinished(bool canceled, SceInt32 result, SetupController *controller);
static void RemovalQuestionClosed(SceInt32 value, SceInt32 button, void *arg);
static void ExitProcess(void);
static void TrophySetupJobFinished(bool canceled, SceInt32 result, SetupController *controller);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	trophySetupDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	trophySetupDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void trophySetupDialogInit(SceSize argSize, const void *argp)
{
	void (**init)(void);
	Module *module;

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	module = *(Module *const *)argp;
	s_moduleInterface.init_func = InitJobQueue;
	s_moduleInterface.start_func = trophySetupDialogStart;
	s_moduleInterface.stop_func = trophySetupDialogStop;
	s_moduleInterface.exit_func = trophySetupDialogExit;
	s_moduleInterface.set_param_func = trophySetupDialogSetParam;
	module->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void trophySetupDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

static void InitJobQueue(Plugin *plugin)
{
	s_jobQueue = new job::JobQueue("SceNpTrophySetupJobQueue", NULL);
}

/* the trophy work for the caller's context */
static int InitSetupController(void)
{
	int ret;

	if (s_setupController != NULL)
		return TROPHY_SETUP_PLUGIN_ERROR_INTERNAL;
	{
		TrophySetupPluginApiScope scope("InitSetupController", ret);

		s_setupController = new SetupController(scope.work()->param.context);
		if (ret >= 0 && (ret = s_setupController->Init()) >= 0) {
			ret = SCE_OK;
		} else if (s_setupController != NULL) {
			delete s_setupController;
			s_setupController = NULL;
		}
	}
	return ret;
}

static void SetTrophySetupResult(SceInt32 result)
{
	int ret;
	TrophySetupPluginApiScope scope("SetTrophySetupResult", ret);

	if (scope.work() != NULL)
		scope.work()->result.result = result;
}

/* whether the caller asked for the processing dialog */
static inline int GetProcessingDialogConfig(void)
{
	int ret;
	TrophySetupPluginApiScope scope("GetProcessingDialogConfig", ret);

	if (ret >= 0) {
		if (scope.work()->param.options & SCE_NP_TROPHY_SETUP_DIALOG_OPTION_SHOW_PROCESSING_DIALOG)
			s_showProcessingDialog = true;
		ret = SCE_OK;
	}
	return ret;
}

static inline void ShowError(SceInt32 error)
{
	SetTrophySetupResult(error);
	TrophySetupDialogController::GetInstance()->ShowError(error);
}

/* the session expired: the title is opened again with a new one */
static inline int RestartTrophyOpenJob(void)
{
	int ret;

	if (s_setupController != NULL) {
		delete s_setupController;
		s_setupController = NULL;
	}
	ret = InitSetupController();
	if (ret >= 0)
		ret = StartTrophyOpenJob(s_jobQueue, TrophyOpenJobFinished, s_setupController);
	return ret;
}

static inline void CancelJobs(void)
{
	if (s_jobQueue != NULL)
		s_jobQueue->CancelAllItems();
}

static void trophySetupDialogStart(Plugin *plugin)
{
	int ret;

	common::MainThreadCallList::Register(trophySetupDialogUpdate, plugin);
	s_showProcessingDialog = false;
	if ((ret = GetProcessingDialogConfig()) < 0 || (ret = InitSetupController()) < 0 ||
	    (ret = TrophySetupPage::GetInstance()->Open()) < 0)
		goto error;
	if (s_showProcessingDialog)
		TrophySetupDialogController::GetInstance()->ShowProcessing();
	ret = StartTrophyOpenJob(s_jobQueue, TrophyOpenJobFinished, s_setupController);
	if (ret >= 0)
		return;
error:
	ShowError(ret);
}

static void trophySetupDialogStop(Plugin *plugin)
{
	CancelJobs();
	common::MainThreadCallList::Unregister(trophySetupDialogUpdate, plugin);
}

static void trophySetupDialogExit(Plugin *plugin)
{
	if (s_jobQueue != NULL)
		delete s_jobQueue;
	if (s_setupController != NULL) {
		delete s_setupController;
		s_setupController = NULL;
	}
}

static void trophySetupDialogSetParam(Plugin::InitParam2 *param)
{
}

/* on the main thread: the caller's abort cancels the jobs, then ends the dialog */
static void trophySetupDialogUpdate(void *userdata)
{
	if (!SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_TROPHY_SETUP))
		return;
	if (s_jobQueue != NULL && s_jobQueue->NumItems() != 0) {
		CancelJobs();
		return;
	}
	SetTrophySetupResult(SCE_COMMON_DIALOG_RESULT_ABORTED);
	ExitProcess();
}

/* the title is open: asks whether to remove the installed one, else sets it up */
static void TrophyOpenJobFinished(bool canceled, SceInt32 result, SetupController *controller)
{
	wstring title;
	intrusive_ptr<graph::Surface> icon;
	int ret;

	if (canceled) {
		SetTrophySetupResult(SCE_COMMON_DIALOG_RESULT_ABORTED);
		ExitProcess();
		return;
	}
	if (result < 0) {
		if (result == TROPHY_SETUP_ERROR_SESSION_EXPIRED) {
			ret = RestartTrophyOpenJob();
			if (ret >= 0)
				return;
		} else {
			ret = result;
		}
		ShowError(ret);
		return;
	}
	if (controller->GetRemovalInfo(&title, &icon)) {
		TrophySetupDialogController::GetInstance()->ShowRemovalQuestion(title, icon,
										 RemovalQuestionClosed, NULL);
		return;
	}
	ret = StartTrophySetupJob(s_jobQueue, TrophySetupJobFinished, s_setupController);
	if (ret < 0)
		ShowError(ret);
}

/* the user's answer: yes (button 4) removes the title and sets it up again */
static void RemovalQuestionClosed(SceInt32 value, SceInt32 button, void *arg)
{
	int ret;

	if (button != 4) {
		TrophySetupDialogController::GetInstance()->ShowExitNotice();
		return;
	}
	if (s_showProcessingDialog)
		TrophySetupDialogController::GetInstance()->ShowProcessing();
	else
		TrophySetupDialogController::GetInstance()->Close(true);
	ret = StartTrophySetupJob(s_jobQueue, TrophySetupJobFinished, s_setupController);
	if (ret < 0)
		ShowError(ret);
}

static void ExitProcess(void)
{
	TrophySetupDialogController::GetInstance()->Hide();
	TrophySetupPage::GetInstance()->Close();
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TROPHY_SETUP);
}

/* the title is set up: the dialog ends, after the processing dialog has shown */
static void TrophySetupJobFinished(bool canceled, SceInt32 result, SetupController *controller)
{
	int ret;

	if (canceled) {
		SetTrophySetupResult(SCE_COMMON_DIALOG_RESULT_ABORTED);
		ExitProcess();
		return;
	}
	if (result >= 0) {
		SetTrophySetupResult(SCE_COMMON_DIALOG_RESULT_OK);
		if (TrophySetupDialogController::GetInstance()->IsProcessing())
			TrophySetupDialogController::GetInstance()->CloseAfterProcessing(true, ExitProcess);
		else
			ExitProcess();
		return;
	}
	if (result == TROPHY_SETUP_ERROR_SESSION_EXPIRED) {
		ret = RestartTrophyOpenJob();
		if (ret >= 0)
			return;
	} else {
		ret = result;
	}
	ShowError(ret);
}
