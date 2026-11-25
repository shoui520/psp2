/*
 * ScePocsDialogPlugin (vs0:sys/external/libcdlg_pocketstation.suprx):
 * plugin lifecycle, work-memory polling, and dialog close dispatch.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <std/string.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include <thread/thread.h>
#include "pocketstation_dialog_plugin.h"
#include "pocketstation_menu.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

extern "C" void *__dso_handle;
void *__dso_handle;

extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

const char paf::s_empty_name[] = "";

Plugin *g_pocketStationPlugin = NULL;
static Plugin::ModuleInterface s_moduleInterface;

class PocketStationPluginApiScope {
public:
	PocketStationPluginApiScope(const char *apiName, SceInt32 &ret)
		: m_apiName(apiName), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_POCKETSTATION);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogPocketStationParam *)SceCommonDialogMain_DD2CAC30(
			COMMON_DIALOG_TYPE_POCKETSTATION);
		if (m_work == NULL) {
			ret = POCKETSTATION_DIALOG_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_POCKETSTATION);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~PocketStationPluginApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_POCKETSTATION);
		}
	}

	CommonDialogPocketStationParam *work(void) const
	{
		return m_work;
	}

private:
	const char *m_apiName;
	CommonDialogPocketStationParam *m_work;
};

static void pocketStationDialogInit(SceSize argSize, const void *argp);
static void pocketStationDialogTerm(void);
static void pocketStationPluginInit(Plugin *plugin);
static void pocketStationPluginStart(Plugin *plugin);
static void pocketStationPluginStop(Plugin *plugin);
static void pocketStationPluginExit(Plugin *plugin);
static void pocketStationPluginSetParam(Plugin::InitParam2 *param);
static void OnUpdate(void *userData);
static void CbClosePocketstationDialog(SceInt32 result);

extern "C" int module_start(SceSize arglen, const void *argp)
{
	pocketStationDialogInit(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	pocketStationDialogTerm();
	return SCE_KERNEL_STOP_SUCCESS;
}

static void pocketStationDialogInit(SceSize argSize, const void *argp)
{
	(void)argSize;
	__dso_handle = &__dso_handle;
	for (void (**fn)(void) = __start__Zpreinit_array; fn < __stop__Zpreinit_array; ++fn) {
		if (*fn != NULL)
			(*fn)();
	}
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; ++fn) {
		if (*fn != NULL)
			(*fn)();
	}

	Module *module = (argp != NULL) ? *(Module *const *)argp : NULL;
	s_moduleInterface.set_param_func = pocketStationPluginSetParam;
	s_moduleInterface.init_func = pocketStationPluginInit;
	s_moduleInterface.start_func = pocketStationPluginStart;
	s_moduleInterface.stop_func = pocketStationPluginStop;
	s_moduleInterface.exit_func = pocketStationPluginExit;
	if (module != NULL)
		module->SetInterface(0, &s_moduleInterface);
}

static void pocketStationDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

static void WritePocketStationResult(CommonDialogPocketStationParam *work, SceInt32 status)
{
	if (work == NULL)
		return;
	sce_paf_memset(&work->result, 0, sizeof(work->result));
	work->result.result = status;
	if (status >= 0) {
		sce_paf_memset(&work->result, 0, sizeof(work->result));
		if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_POCKETSTATION))
			work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		else
			work->result.result = status;
		PocketStationMenu::CopyResultData(&work->result.selectedAction);
	}
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	PocketStationPluginApiScope scope("OnUpdate", ret);
	CommonDialogPocketStationParam *work = scope.work();

	if (ret == SCE_OK && work != NULL) {
		if (work->state == POCKETSTATION_DIALOG_STATE_RESUME) {
			PocketStationMenu::Resume();
			work->state = 0;
		} else if (work->state == POCKETSTATION_DIALOG_STATE_RUNNING) {
			ret = PocketStationMenu::Open(&work->param, CbClosePocketstationDialog);
			work->state = 0;
		} else if (work->abortResult == 1) {
			work->abortResult = 2;
			bool isAborted = SceCommonDialogMain_75F10F83(
				COMMON_DIALOG_TYPE_POCKETSTATION);
			ret = PocketStationMenu::Close(isAborted);
		}
	}

	if (ret < 0) {
		WritePocketStationResult(work, ret);
		if (work != NULL)
			work->abortResult = 0;
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceInt32 closeRet = SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_POCKETSTATION);
		if (closeRet != SCE_OK)
			__builtin_trap();
	}
}

static void pocketStationPluginInit(Plugin *plugin)
{
	g_pocketStationPlugin = plugin;
	SceInt32 ret = SCE_OK;
	PocketStationPluginApiScope scope("Init", ret);
	(void)ret;
}

static void pocketStationPluginStart(Plugin *plugin)
{
	(void)plugin;
	PocketStationMenu::Initialize();
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void pocketStationPluginStop(Plugin *plugin)
{
	PocketStationMenu::Stop();
	if (plugin != NULL)
		plugin->PageCloseAll();
}

static void pocketStationPluginExit(Plugin *plugin)
{
	(void)plugin;
	PocketStationMenu::Finalize();
	g_pocketStationPlugin = NULL;
}

static void pocketStationPluginSetParam(Plugin::InitParam2 *param)
{
	if (param != NULL)
		param->draw_priority = 10;
}

static void CbClosePocketstationDialog(SceInt32 result)
{
	SceInt32 ret = SCE_OK;
	PocketStationPluginApiScope scope("CbClosePocketstationDialog", ret);
	CommonDialogPocketStationParam *work = scope.work();
	if (ret != SCE_OK || work == NULL)
		return;

	if (work->result.result == 0)
		WritePocketStationResult(work, result);
	work->abortResult = 0;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	SceInt32 closeRet = SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_POCKETSTATION);
	if (work->result.result == 0 && closeRet != SCE_OK)
		__builtin_trap();
}
