/*
 * SceImeDialogPlugin (vs0:sys/external/libcdlg_ime.suprx): the IME dialog's plugin, which
 * SceCommonDialogMain loads for libcdlg's sceImeDialog*. On the main thread it opens the dialog
 * once the caller's parameter is in the work memory (CommonDialogImeParam), closes it on the
 * caller's abort, and puts the dialog's result there when it closes. The names the plugin
 * writes into the work memory (OnUpdate, Init, ImeDialogCallback) are Sony's function names.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <ime_dialog.h>
#include <std/string.h>
#include <plugin.h>
#include <module/module.h>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "ime_dialog_plugin.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/* the work memory is missing for a call (name ours) */
#define IME_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY	-2146431710	/* 0x80100D22 */

/*
 * The work memory, locked for one of the plugin's functions under the function's name, as
 * libcdlg's own calls lock it (CommonDialogApiScope): ret is the lock's error. Unlike libcdlg's
 * scope it lets the lock go without checking the name it finds.
 */
class ImeDialogPluginApiScope {
public:
	ImeDialogPluginApiScope(const char *name, int &ret) : m_name(name), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_IME);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogImeParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_IME);
		if (m_work == NULL) {
			ret = IME_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_IME);
			return;
		}
		sce_paf_strncpy(m_work->apiName, name, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~ImeDialogPluginApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_name, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_IME);
		}
	}

	CommonDialogImeParam *work(void) const
	{
		return m_work;
	}

private:
	const char *m_name;
	CommonDialogImeParam *m_work;
};

/* the plugin's draw priority and texture filter (Plugin::InitParam2) */
#define IME_DIALOG_DRAW_PRIORITY	10
#define IME_DIALOG_TEXTURE_LOAD_FILTER	1

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

Plugin *g_imeDialogPlugin;

static void imeDialogPageInit(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

/* the page's init event handler: nothing to do */
static InitEventFunctionEntry s_initEventTable[] = {
	{IDParam(IME_DIALOG_PAGE_ID), imeDialogPageInit},
	{IDParam((uint32_t)0), NULL}
};

static Plugin::ModuleInterface s_moduleInterface;

static void imeDialogInit(SceSize argSize, const void *argp);
static void imeDialogTerm(void);
static void OnUpdate(void *userdata);
static void Init(Plugin *plugin);
static void imeDialogStart(Plugin *plugin);
static void imeDialogStop(Plugin *plugin);
static void imeDialogExit(Plugin *plugin);
static void imeDialogSetParam(Plugin::InitParam2 *param);
static void ImeDialogCallback(SceInt32 button, SceInt32 result);

/* the dialog's result for GetResult: an error other than a bad parameter is reported as fatal */
static inline void SetDialogResult(CommonDialogImeParam *work, SceInt32 button, SceInt32 result)
{
	if (result >= 0) {
		sce_paf_memset(&work->result, 0, sizeof(work->result));
		if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_IME)) {
			work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		} else if (result < 0 && result != SCE_IME_DIALOG_ERROR_PARAM) {
			work->result.result = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		} else {
			if (work->hasButton)
				work->result.button = button;
			work->result.result = result;
		}
	}
}

/* the dialog has ended: the plugin stops updating, and SceCommonDialogMain finishes the call */
static inline void EndDialog(CommonDialogImeParam *work)
{
	int ret;

	work->abortResult = 0;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	ret = SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_IME);
	/* Sony's assertion: its message is left out of the release build */
	if (ret != SCE_OK)
		__breakpoint(0);
}

extern "C" int module_start(SceSize argSize, const void *argp)
{
	imeDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	imeDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void imeDialogInit(SceSize argSize, const void *argp)
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
	s_moduleInterface.init_func = Init;
	s_moduleInterface.start_func = imeDialogStart;
	s_moduleInterface.stop_func = imeDialogStop;
	s_moduleInterface.exit_func = imeDialogExit;
	s_moduleInterface.set_param_func = imeDialogSetParam;
	module->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void imeDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

/* on the main thread: opens the dialog once the caller's parameter is set, and closes it on the
   caller's abort; on an error the dialog ends */
static void OnUpdate(void *userdata)
{
	int ret;
	ImeDialogPluginApiScope scope(__FUNCTION__, ret);

	if (ret == SCE_OK) {
		CommonDialogImeParam *work = scope.work();

		if (work->paramSet != 0) {
			if (work->paramSet == 1) {
				ret = ImeDialog::Create(&work->param, ImeDialogCallback, work->infobarShown);
				work->paramSet = 0;
			}
		} else if (work->abortResult == 1) {
			work->abortResult = 2;
			ret = ImeDialog::Abort(SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_IME));
		}
	}
	if (ret < 0) {
		SetDialogResult(scope.work(), 0, ret);
		EndDialog(scope.work());
	}
}

static void Init(Plugin *plugin)
{
	int ret;

	g_imeDialogPlugin = plugin;
	plugin->SetInitEventHandler(s_initEventTable);
	ImeDialogPluginApiScope scope(__FUNCTION__, ret);
}

static void imeDialogStart(Plugin *plugin)
{
	ImeDialog::Reset();
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void imeDialogStop(Plugin *plugin)
{
	plugin->PageCloseAll();
}

static void imeDialogExit(Plugin *plugin)
{
}

static void imeDialogSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = IME_DIALOG_DRAW_PRIORITY;
	param->texture_load_filter = IME_DIALOG_TEXTURE_LOAD_FILTER;
}

static void imeDialogPageInit(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
}

/* the dialog has closed: its result for GetResult, and the dialog ends */
static void ImeDialogCallback(SceInt32 button, SceInt32 result)
{
	int ret;
	ImeDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogImeParam *work = scope.work();

	if (work == NULL)
		return;
	SetDialogResult(work, button, result);
	if (ImeDialog::Destroy() == SCE_OK)
		EndDialog(work);
}
