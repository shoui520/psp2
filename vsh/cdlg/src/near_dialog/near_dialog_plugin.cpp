/*
 * SceNearDialogPlugin (vs0:sys/external/libcdlg_near.suprx): the near dialog's plugin, which
 * SceCommonDialogMain loads for libcdlg's sceNearDialog*. On the main thread it steps the dialog
 * through its mode's states (near_dialog_state.cpp): each state opens a message dialog
 * (near_dialog.cpp), starts a check-in or a gift download with the near background service, or
 * waits for one to report, and the dialog ends when a state gives its result. The names the
 * plugin writes into the work memory (OnUpdate, NearDialogCallback, StartCheckinCallback,
 * StartItemDownloadCallback, TermCallback) are Sony's function names, as is the file's
 * (vsh/cdlg/src/near_dialog/near_dialog_plugin.cpp, 3.20's path string).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <near_dialog_util.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <module/module.h>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "near_dialog_plugin.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/*
 * The work memory, locked for one of the plugin's functions under the function's name, as
 * libcdlg's own calls lock it (CommonDialogApiScope): ret is the lock's error. Unlike libcdlg's
 * scope it lets the lock go without checking the name it finds.
 */
class NearDialogPluginApiScope {
public:
	NearDialogPluginApiScope(const char *name, int &ret) : m_name(name), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_NEAR);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogNearParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_NEAR);
		if (m_work == NULL) {
			ret = NEAR_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NEAR);
			return;
		}
		sce_paf_strncpy(m_work->apiName, name, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~NearDialogPluginApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_name, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NEAR);
		}
	}

	CommonDialogNearParam *work(void) const
	{
		return m_work;
	}

private:
	const char *m_name;
	CommonDialogNearParam *m_work;
};

/* what the dialog passes its callback as its user data */
#define NEAR_DIALOG_USERDATA	((void *)0xDEADBEEF)

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

Plugin *g_nearDialogPlugin = NULL;
static NearCheckinLib *s_checkinLib = NULL;

/* the page's init event handlers: none */
static InitEventFunctionEntry s_initEventTable[] = {
	{IDParam((uint32_t)0), NULL}
};

static Plugin::ModuleInterface s_moduleInterface;
/* the dialog's result is set: the plugin sets it once */
static bool s_resultSet;

static void nearDialogInit(SceSize argSize, const void *argp);
static void nearDialogTerm(void);
static void nearDialogPluginInit(Plugin *plugin);
static void nearDialogPluginStart(Plugin *plugin);
static void nearDialogPluginStop(Plugin *plugin);
static void nearDialogPluginExit(Plugin *plugin);
static void nearDialogPluginSetParam(Plugin::InitParam2 *param);
static void NearDialogCallback(SceInt32 result, SceInt32 mode, SceInt32 button, void *userdata);
static void StartCheckinCallback(SceInt32 result, void *userdata);
static void StartItemDownloadCallback(SceInt32 result, void *userdata);
static void TermCallback(SceInt32 result, void *userdata);
static void OnUpdate(void *userdata);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	nearDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	nearDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void nearDialogInit(SceSize argSize, const void *argp)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	s_moduleInterface.init_func = nearDialogPluginInit;
	s_moduleInterface.start_func = nearDialogPluginStart;
	s_moduleInterface.stop_func = nearDialogPluginStop;
	s_moduleInterface.exit_func = nearDialogPluginExit;
	s_moduleInterface.set_param_func = nearDialogPluginSetParam;
	(*(Module *const *)argp)->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void nearDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

/*
 * The dialog's result for GetResult, set once: aborted when the caller aborted, an unexpected
 * error for an error of the plugin's own, a gift with the user's content that could not be posted
 * when the check-in posted one; then the check-in library terminates, and the dialog ends when it
 * has (TermCallback).
 */
static void SetResult(CommonDialogNearParam *work, SceInt32 ret)
{
	int err;

	if (s_resultSet)
		return;
	work->flags = 0;
	sce_paf_memset(&work->result, 0, sizeof(work->result));
	work->result.mode = work->param.mode;
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NEAR))
		work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
	else if (ret < 0 && (SceUInt32)ret >= (SceUInt32)NEAR_DIALOG_PLUGIN_ERROR_FIRST &&
		 (SceUInt32)ret <= (SceUInt32)NEAR_DIALOG_PLUGIN_ERROR_LAST)
		work->result.result = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
	else if (work->userContentPosted && (ret == SCE_OK || ret == SCE_NEAR_DIALOG_RESULT_SEND_OK))
		work->result.result = SCE_NEAR_DIALOG_RESULT_SEND_FAILED_UGC;
	else
		work->result.result = ret;
	if (work->jobState == NEAR_DIALOG_JOB_STARTED) {
		work->jobState = NEAR_DIALOG_JOB_NONE;
		s_checkinLib->Cancel();
	}
	err = s_checkinLib->Term(TermCallback, NULL);
	if (err != 0) {
		if (err != 8)
			work->result.result = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		work->flags |= NEAR_DIALOG_FLAG_TERM;
	}
	s_resultSet = true;
}

/*
 * On the main thread: does what the flags ask for (check the SDK version and enter the first
 * state, open the state's dialog, start or cancel a check-in or a download and wait for its
 * status, move the progress bar), closes the dialog when asked, and ends it once the check-in
 * library has terminated; on an error the dialog ends with it.
 */
static void OnUpdate(void *userdata)
{
	int ret;
	NearDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogNearParam *work = scope.work();
	SceUInt32 flags;
	SceInt32 step;

	if (ret != SCE_OK)
		goto out;
	s_checkinLib->Update();
	flags = work->flags;
	if (flags & NEAR_DIALOG_FLAG_INIT) {
		work->flags = flags & ~NEAR_DIALOG_FLAG_INIT;
		ret = NearDialogCheckSdkVersion(work->param.sdkVersion);
		if (ret != SCE_OK)
			goto out;
		ret = NearDialogNextState(work, 0);
		if (ret != SCE_OK)
			goto out;
		flags = work->flags;
	}
	if (flags & NEAR_DIALOG_FLAG_OPEN) {
		work->flags = flags & ~NEAR_DIALOG_FLAG_OPEN;
		ret = NearDialog::Create(&work->msgParam, NearDialogCallback, NEAR_DIALOG_USERDATA);
		if (ret != SCE_OK)
			goto out;
		flags = work->flags;
	}
	if (flags & NEAR_DIALOG_FLAG_CANCEL_CHECKIN) {
		if (work->cancelling) {
			ret = s_checkinLib->GetCheckinStatus(&work->checkinStatus);
			if (ret != SCE_OK) {
				ret = SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
				goto error;
			}
			if (work->checkinStatus.finished) {
				work->cancelling = false;
				work->jobState = NEAR_DIALOG_JOB_NONE;
				work->flags &= ~NEAR_DIALOG_FLAG_CANCEL_CHECKIN;
				work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
			}
		} else {
			work->cancelling = true;
			if (work->jobState == NEAR_DIALOG_JOB_STARTED) {
				work->flags = flags & ~NEAR_DIALOG_FLAG_CANCEL_CHECKIN;
			} else {
				ret = s_checkinLib->Cancel();
				if (ret != SCE_OK) {
					ret = SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
					goto error;
				}
			}
		}
	}
	if (work->flags & NEAR_DIALOG_FLAG_CHECKIN_STATUS) {
		if (work->jobState == NEAR_DIALOG_JOB_RUNNING) {
			ret = s_checkinLib->GetCheckinStatus(&work->checkinStatus);
			if (ret != SCE_OK) {
				ret = SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
				goto error;
			}
			if (work->checkinStatus.finished)
				work->jobState = NEAR_DIALOG_JOB_NONE;
		}
		step = NearDialogGetStepResult(work, work->state);
		if (step != NEAR_DIALOG_STEP_PENDING) {
			work->stepResult = step;
			ret = NearDialogNextState(work, 0);
			if (ret != SCE_OK)
				goto out;
		}
	}
	flags = work->flags;
	if (flags & NEAR_DIALOG_FLAG_START_CHECKIN) {
		work->flags = flags & ~NEAR_DIALOG_FLAG_START_CHECKIN;
		if (work->retry)
			step = s_checkinLib->StartCheckinRetry(StartCheckinCallback, NULL);
		else
			step = s_checkinLib->StartCheckin(StartCheckinCallback, NULL);
		work->retry = false;
		if (step != SCE_OK) {
			ret = SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
			goto error;
		}
		ret = NearDialogNextState(work, 0);
		if (ret != SCE_OK)
			goto out;
		flags = work->flags;
		work->jobState = NEAR_DIALOG_JOB_STARTED;
	}
	if (flags & NEAR_DIALOG_FLAG_CANCEL_DOWNLOAD) {
		if (work->cancelling) {
			ret = s_checkinLib->GetItemDownloadStatus(&work->downloadStatus);
			if (ret != SCE_OK || work->downloadStatus.finished) {
				work->cancelling = false;
				work->jobState = NEAR_DIALOG_JOB_NONE;
				work->flags &= ~NEAR_DIALOG_FLAG_CANCEL_DOWNLOAD;
				SetResult(work, SCE_NEAR_DIALOG_RESULT_DOWNLOAD_CANCELED);
			}
		} else {
			work->cancelling = true;
			if (work->jobState == NEAR_DIALOG_JOB_STARTED) {
				work->flags = flags & ~NEAR_DIALOG_FLAG_CANCEL_DOWNLOAD;
			} else {
				ret = s_checkinLib->Cancel();
				if (ret != SCE_OK) {
					ret = SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
					goto error;
				}
			}
		}
	}
	flags = work->flags;
	if (flags & NEAR_DIALOG_FLAG_DOWNLOAD_STATUS) {
		ret = s_checkinLib->GetItemDownloadStatus(&work->downloadStatus);
		if (ret != SCE_OK) {
			ret = SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
			goto error;
		}
		flags = work->flags;
		if (work->downloadStatus.finished) {
			flags &= ~NEAR_DIALOG_FLAG_DOWNLOAD_STATUS;
			work->flags = flags;
			if (work->downloadStatus.errorCode != SCE_OK)
				work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
			work->downloadResult = work->downloadStatus.errorCode;
			work->jobState = NEAR_DIALOG_JOB_NONE;
		}
		if (work->downloadStatus.rate > work->progressRate) {
			flags |= NEAR_DIALOG_FLAG_PROGRESS;
			work->progressRate = work->downloadStatus.rate;
			work->flags = flags;
		}
	}
	if (flags & NEAR_DIALOG_FLAG_START_DOWNLOAD) {
		work->flags = flags & ~NEAR_DIALOG_FLAG_START_DOWNLOAD;
		{
			string giftDiscoveringId(work->giftDiscoveringId);

			step = s_checkinLib->StartItemDownload(giftDiscoveringId, StartItemDownloadCallback,
								NULL);
		}
		if (step != SCE_OK) {
			ret = SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
			goto error;
		}
		ret = NearDialogNextState(work, 0);
		if (ret != SCE_OK)
			goto out;
		flags = work->flags;
		work->jobState = NEAR_DIALOG_JOB_STARTED;
	}
	if (flags & NEAR_DIALOG_FLAG_PROGRESS) {
		if (NearDialog::Get() != NULL && NearDialog::Get()->m_opened != 0) {
			work->flags &= ~NEAR_DIALOG_FLAG_PROGRESS;
			NearDialog::Get()->ProgInc(work->progressTarget, work->progressRate);
			if (work->progressRate >= 100)
				work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
		}
	}
	if (work->closeRequest == NEAR_DIALOG_CLOSE_REQUESTED) {
		if (NearDialog::Get() != NULL) {
			ret = NearDialog::Abort(SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NEAR));
		} else {
			if (!SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NEAR)) {
				ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
				s_checkinLib->Cancel();
				goto error;
			}
			if (work->jobState == NEAR_DIALOG_JOB_NONE) {
				SetResult(work, SCE_COMMON_DIALOG_RESULT_ABORTED);
			} else if (work->jobState != NEAR_DIALOG_JOB_STARTED) {
				s_checkinLib->Cancel();
				SetResult(work, SCE_COMMON_DIALOG_RESULT_ABORTED);
			}
		}
		work->closeRequest = NEAR_DIALOG_CLOSE_DONE;
	}
	if (work->flags & NEAR_DIALOG_FLAG_TERM) {
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		/* Sony's assertion: its message is left out of the release build */
		if (SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NEAR) != SCE_OK)
			__breakpoint(0);
		sce_paf_memset(work, 0, offsetof(CommonDialogNearParam, result));
		work->flags &= ~NEAR_DIALOG_FLAG_TERM;
		goto done;
	}
out:
	if (ret < 0) {
error:
		work->closeRequest = 0;
		SetResult(work, ret);
	}
done:
	;
}

/* the page has no init event handlers of its own; the check-in library is the plugin's */
static void nearDialogPluginInit(Plugin *plugin)
{
	g_nearDialogPlugin = plugin;
	plugin->SetInitEventHandler(s_initEventTable);
	s_checkinLib = new NearCheckinLib();
}

static void nearDialogPluginStart(Plugin *plugin)
{
	common::MainThreadCallList::Register(OnUpdate, NULL);
	SceCommonDialogMain_877BB562();
}

static void nearDialogPluginStop(Plugin *plugin)
{
	plugin->PageCloseAll();
}

static void nearDialogPluginExit(Plugin *plugin)
{
	g_nearDialogPlugin = NULL;
	if (s_checkinLib != NULL)
		delete s_checkinLib;
	s_checkinLib = NULL;
}

static void nearDialogPluginSetParam(Plugin::InitParam2 *param)
{
}

/* the state's dialog has closed: the state's result ends the dialog, or the next state starts */
static void NearDialogCallback(SceInt32 result, SceInt32 mode, SceInt32 button, void *userdata)
{
	int ret;
	NearDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogNearParam *work = scope.work();
	SceInt32 res;

	if (ret == SCE_OK) {
		if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NEAR)) {
			if (work->jobState != NEAR_DIALOG_JOB_STARTED) {
				s_checkinLib->Cancel();
				SetResult(work, result);
			}
		} else if (NearDialogStateResult(work, result, button, &res)) {
			SetResult(work, res);
		} else {
			SceInt32 next = NearDialogNextState(work, button);

			if (next != SCE_OK)
				SetResult(work, next);
		}
	}
	work->closeRequest = 0;
}

/* the check-in has started: its result, and the next state waits for its status */
static void StartCheckinCallback(SceInt32 result, void *userdata)
{
	int ret;
	NearDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogNearParam *work = scope.work();

	if (ret == SCE_OK) {
		work->jobState = NEAR_DIALOG_JOB_CALLED_BACK;
		if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NEAR)) {
			s_checkinLib->Cancel();
			SetResult(work, SCE_OK);
		} else if (work->cancelling) {
			work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
			s_checkinLib->Cancel();
		} else {
			work->startResult = result;
			if (result != SCE_OK) {
				work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
			} else {
				work->jobState = NEAR_DIALOG_JOB_RUNNING;
				NearDialogNextState(work, 0);
			}
		}
	}
}

/* the download has started: its result, with the download's own error when it has one */
static void StartItemDownloadCallback(SceInt32 result, void *userdata)
{
	int ret;
	NearDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogNearParam *work = scope.work();

	if (ret == SCE_OK) {
		work->jobState = NEAR_DIALOG_JOB_CALLED_BACK;
		if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NEAR)) {
			s_checkinLib->Cancel();
			SetResult(work, SCE_OK);
		} else {
			work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
			if (work->cancelling) {
				s_checkinLib->Cancel();
			} else {
				work->startResult = result;
				if (result == SCE_OK) {
					if (s_checkinLib->GetItemDownloadStatus(&work->downloadStatus) != SCE_OK)
						work->startResult = 1;
					else if (work->downloadStatus.errorCode ==
						 NEAR_SERVICE_ERROR_GIFT_DOWNLOADING)
						work->startResult = 3;
					else
						work->jobState = NEAR_DIALOG_JOB_RUNNING;
				}
			}
		}
	}
}

/* the check-in library has terminated: the dialog ends */
static void TermCallback(SceInt32 result, void *userdata)
{
	int ret;
	NearDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogNearParam *work = scope.work();

	if (ret == SCE_OK)
		work->flags |= NEAR_DIALOG_FLAG_TERM;
}
