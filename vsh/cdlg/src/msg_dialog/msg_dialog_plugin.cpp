/*
 * SceMsgDialogPlugin (vs0:sys/external/libcdlg_msg.suprx): the message dialog's plugin, which
 * SceCommonDialogMain loads for libcdlg's sceMsgDialog*. On the main thread it opens the dialog
 * once the caller's parameter is in the work memory (CommonDialogMsgParam), passes the progress
 * bar's value and message on, closes the dialog on the caller's Close or Abort, and puts the
 * dialog's result there when it closes. The names the plugin writes into the work memory
 * (OnUpdate, MsgDialogCallback) are Sony's function names.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <common_dialog/error.h>
#include <message_dialog.h>
#include <std/string.h>
#include <plugin.h>
#include <module/module.h>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "msg_dialog_plugin.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/* the work memory is missing for a call (name ours) */
#define MSG_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY	-2146432478	/* 0x80100A22 */

/*
 * The work memory, locked for one of the plugin's functions under the function's name, as
 * libcdlg's own calls lock it (CommonDialogApiScope): ret is the lock's error. Unlike libcdlg's
 * scope it lets the lock go without checking the name it finds.
 */
class MsgDialogPluginApiScope {
public:
	MsgDialogPluginApiScope(const char *name, int &ret) : m_name(name), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_MSG);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogMsgParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_MSG);
		if (m_work == NULL) {
			ret = MSG_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_MSG);
			return;
		}
		sce_paf_strncpy(m_work->apiName, name, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~MsgDialogPluginApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_name, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_MSG);
		}
	}

	CommonDialogMsgParam *work(void) const
	{
		return m_work;
	}

private:
	const char *m_name;
	CommonDialogMsgParam *m_work;
};

/* what the dialog passes its callback as its user data */
#define MSG_DIALOG_USERDATA	((void *)0xDEADBEEF)

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

/* the empty name of the plugin's resource IDs: paf's headers declare it weak, so each module that
   builds an IDParam from it keeps its own copy (Sony's code tests it for NULL at every use) */
const char paf::s_empty_name[] = "";

Plugin *g_msgDialogPlugin = NULL;

/* the page's init event handlers: none */
static InitEventFunctionEntry s_initEventTable[] = {
	{IDParam((uint32_t)0), NULL}
};

static Plugin::ModuleInterface s_moduleInterface;

static void msgDialogInit(SceSize argSize, const void *argp);
static void msgDialogTerm(void);
static void msgDialogPluginInit(Plugin *plugin);
static void msgDialogPluginStart(Plugin *plugin);
static void msgDialogPluginStop(Plugin *plugin);
static void msgDialogPluginExit(Plugin *plugin);
static void msgDialogPluginSetParam(Plugin::InitParam2 *param);
static void MsgDialogCallback(SceInt32 result, SceMsgDialogMode mode, SceInt32 button,
			      void *userdata);
static void OnUpdate(void *userdata);

/* the dialog has ended: the plugin stops updating, and SceCommonDialogMain finishes the call */
static inline void EndDialog(CommonDialogMsgParam *work)
{
	int ret;

	work->closeRequested = 0;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	ret = SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_MSG);
	/* Sony's assertion: its message is left out of the release build */
	if (ret != SCE_OK)
		__breakpoint(0);
}

extern "C" int module_start(SceSize argSize, const void *argp)
{
	msgDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	msgDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void msgDialogInit(SceSize argSize, const void *argp)
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
	s_moduleInterface.init_func = msgDialogPluginInit;
	s_moduleInterface.start_func = msgDialogPluginStart;
	s_moduleInterface.stop_func = msgDialogPluginStop;
	s_moduleInterface.exit_func = msgDialogPluginExit;
	s_moduleInterface.set_param_func = msgDialogPluginSetParam;
	module->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void msgDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

/*
 * The dialog's result for GetResult: the mode, and the result (aborted when the caller aborted,
 * an unexpected error for an error of the plugin's own) with the button that closed the dialog,
 * as SceMsgDialogButtonId (a 3-button dialog's buttons are BUTTON1 to BUTTON3; the other
 * dialogs' cancel button closes them with SCE_COMMON_DIALOG_RESULT_USER_CANCELED).
 */
static void SetResult(CommonDialogMsgParam *work, SceInt32 ret, SceMsgDialogMode mode,
		      SceInt32 button)
{
	sce_paf_memset(&work->result, 0, sizeof(work->result));
	work->result.mode = mode;
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_MSG)) {
		work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		return;
	}
	if (ret < 0 && ret != SCE_MSG_DIALOG_ERROR_PARAM && ret != SCE_MSG_DIALOG_ERROR_MODULE) {
		work->result.result = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		return;
	}
	work->result.result = ret;
	if (ret < 0 || ret == SCE_COMMON_DIALOG_RESULT_ABORTED)
		return;
	switch (mode) {
	case SCE_MSG_DIALOG_MODE_INVALID:
		break;
	case SCE_MSG_DIALOG_MODE_USER_MSG:
		if (work->userMsgParam.buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS) {
			switch (button) {
			case 2:
				work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_BUTTON1;
				break;
			case 3:
				work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_BUTTON2;
				break;
			case 6:
				work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_BUTTON3;
				break;
			default:
				work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
				break;
			}
			break;
		}
		switch (button) {
		case 2:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_YES;
			break;
		case 3:
			work->result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
			break;
		case 4:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_OK;
			break;
		case 5:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_NO;
			break;
		case 6:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_RETRY;
			break;
		default:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
			break;
		}
		break;
	case SCE_MSG_DIALOG_MODE_SYSTEM_MSG:
	case SCE_MSG_DIALOG_MODE_ERROR_CODE:
	case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
		switch (button) {
		case 2:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_YES;
			break;
		case 3:
			work->result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
			break;
		case 4:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_OK;
			break;
		case 5:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_NO;
			break;
		case 6:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_RETRY;
			break;
		default:
			work->result.buttonId = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
			break;
		}
		break;
	}
}

/*
 * On the main thread: opens the dialog once the caller's parameter is set, passes the progress
 * bar's message and value on, and closes the dialog on the caller's Close or Abort; on an error
 * the dialog ends with it.
 */
static void OnUpdate(void *userdata)
{
	int ret;
	MsgDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogMsgParam *work = scope.work();

	if (ret == SCE_OK) {
		if (work->requestFlags & MSG_DIALOG_FLAG_PARAM) {
			ret = MsgDialogParamCheck(&work->param);
			if (ret != SCE_OK) {
				if (ret == SCE_MSG_DIALOG_ERROR_PARAM)
					MsgDialogPrintParamError();
			} else {
				ret = MsgDialog::Create(&work->param, MsgDialogCallback,
							MSG_DIALOG_USERDATA);
			}
			if (ret == SCE_OK && MsgDialog::Get() != NULL) {
				MsgDialog::Get()->ProgInc(work->progressBarTarget,
							  work->progressBarRate);
				MsgDialog::Get()->SetButtonMsg(&work->param, &work->buttonParam);
			}
			work->request = 0;
			work->requestFlags &= ~MSG_DIALOG_FLAG_PARAM;
		}
		if (ret == SCE_OK) {
			if (work->requestFlags & MSG_DIALOG_FLAG_MSG) {
				if (MsgDialogMsgCheck(work->progressBarTarget, work->msg) != SCE_OK) {
					MsgDialogPrintParamError();
				} else if (MsgDialog::Get() != NULL) {
					MsgDialog::Get()->SetMsg(work->progressBarTarget, work->msg);
				}
				ret = SCE_OK;
				work->request = 0;
				work->requestFlags &= ~MSG_DIALOG_FLAG_MSG;
			}
			if (work->requestFlags & MSG_DIALOG_FLAG_PROGRESS_INC) {
				if (MsgDialog::Get() != NULL)
					MsgDialog::Get()->ProgInc(work->progressBarTarget,
								  work->progressBarRate);
				ret = SCE_OK;
				work->request = 0;
				work->requestFlags &= ~MSG_DIALOG_FLAG_PROGRESS_INC;
			}
			if (work->requestFlags & MSG_DIALOG_FLAG_PROGRESS_SET) {
				if (MsgDialog::Get() != NULL)
					MsgDialog::Get()->SetValue(work->progressBarTarget,
								   work->progressBarRate);
				ret = SCE_OK;
				work->request = 0;
				work->requestFlags &= ~MSG_DIALOG_FLAG_PROGRESS_SET;
			}
			if (work->closeRequested == 1) {
				ret = MsgDialog::Abort(SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_MSG));
				work->closeRequested = 2;
			}
		}
	}
	if (ret < 0) {
		SetResult(work, ret, work->param.mode, -1);
		EndDialog(work);
	}
}

/* the page has no init event handlers of its own */
static void msgDialogPluginInit(Plugin *plugin)
{
	g_msgDialogPlugin = plugin;
	plugin->SetInitEventHandler(s_initEventTable);
}

static void msgDialogPluginStart(Plugin *plugin)
{
	common::MainThreadCallList::Register(OnUpdate, NULL);
	SceCommonDialogMain_877BB562();
}

static void msgDialogPluginStop(Plugin *plugin)
{
	plugin->PageCloseAll();
}

static void msgDialogPluginExit(Plugin *plugin)
{
	g_msgDialogPlugin = NULL;
}

static void msgDialogPluginSetParam(Plugin::InitParam2 *param)
{
}

/* the dialog has closed: its result for GetResult, and the dialog ends */
static void MsgDialogCallback(SceInt32 result, SceMsgDialogMode mode, SceInt32 button,
			      void *userdata)
{
	int ret;
	MsgDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogMsgParam *work = scope.work();

	if (ret == SCE_OK) {
		SetResult(work, result, mode, button);
		if (MsgDialog::Destroy() == SCE_OK)
			EndDialog(work);
	}
}
