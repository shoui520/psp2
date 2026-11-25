/*
 * SceCompatDialogPlugin (vs0:sys/external/libcdlg_compat.suprx):
 * plugin entry points, CommonDialogMain work-memory bridge, and SceCompatMenuCtrl.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <std/string.h>
#include <std/stdio.h>
#include <plugin.h>
#include <module/module.h>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include <widget/w_scene.h>
#include <widget/w_button.h>
#include <widget/w_text.h>
#include "compat_dialog_plugin.h"

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

static Plugin *s_plugin = NULL;
static SceCompatMenuCtrl *s_menuCtrl = NULL;

static void compatPluginSetParam(Plugin::InitParam2 *param);
static void compatPluginInit(Plugin *plugin);
static void compatPluginStart(Plugin *plugin);
static void compatPluginStop(Plugin *plugin);
static void compatPluginExit(Plugin *plugin);

static Plugin::ModuleInterface s_moduleInterface = {
	compatPluginSetParam,
	compatPluginInit,
	compatPluginStart,
	compatPluginStop,
	compatPluginExit
};

class CompatApiScope {
public:
	CompatApiScope(const char *apiName, SceInt32 &err)
		: m_work(NULL),
		  m_apiName(apiName)
	{
		err = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_COMPAT);
		if (err != SCE_OK)
			return;
		m_work = (CommonDialogCompatParam *)
			SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_COMPAT);
		if (m_work == NULL) {
			err = COMPAT_DIALOG_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_COMPAT);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		err = SCE_OK;
	}

	~CompatApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_COMPAT);
		}
	}

	CommonDialogCompatParam *work() const
	{
		return m_work;
	}

private:
	CommonDialogCompatParam *m_work;
	const char *m_apiName;
};

static void OnUpdate(void *userData);

static void CompatDialogCallback(SceInt32 status, const CompatDialogResult *outResult)
{
	SceInt32 ret = SCE_OK;
	CompatApiScope scope("CompatDialogCallback", ret);
	CommonDialogCompatParam *work = scope.work();
	if (ret != SCE_OK || work == NULL)
		return;

	sce_paf_memset(&work->result, 0, sizeof(work->result));
	if (status < 0) {
		work->result.result = status;
	} else {
		if (outResult != NULL)
			work->result = *outResult;
		work->result.result =
			SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_COMPAT)
			? SCE_COMMON_DIALOG_RESULT_ABORTED
			: status;
	}
	work->abortResult = 0;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_COMPAT);
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	CompatApiScope scope("OnUpdate", ret);
	CommonDialogCompatParam *work = scope.work();
	if (ret != SCE_OK || work == NULL)
		return;

	SceInt32 status = SCE_OK;
	if (work->state == COMPAT_DIALOG_STATE_RUNNING) {
		if (s_menuCtrl != NULL)
			status = s_menuCtrl->Start(s_plugin, &work->param, CompatDialogCallback);
		work->state = 0;
	} else if (work->state == COMPAT_DIALOG_STATE_RESUME) {
		if (s_menuCtrl != NULL)
			s_menuCtrl->Resume();
		work->state = 0;
	} else if (work->state == 0 && work->abortResult == 1) {
		work->abortResult = 2;
		bool aborted = SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_COMPAT);
		if (s_menuCtrl != NULL)
			status = s_menuCtrl->HandleAbort(aborted);
	}

	if (status < 0) {
		sce_paf_memset(&work->result, 0, sizeof(work->result));
		work->result.result = status;
		work->abortResult = 0;
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_COMPAT);
	}
}

SceCompatMenuCtrl::SceCompatMenuCtrl()
	: m_plugin(NULL),
	  m_bgScene(NULL),
	  m_topList(NULL),
	  m_callback(NULL),
	  m_section(COMPAT_MENU_TOP),
	  m_activeDisc(1),
	  m_customAssign(false),
	  m_bilinearFilter(false),
	  m_open(false)
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));
	sce_paf_memset(&m_result, 0, sizeof(m_result));
}

SceCompatMenuCtrl::~SceCompatMenuCtrl()
{
	Close();
}

SceInt32 SceCompatMenuCtrl::Start(Plugin *plugin, const CompatDialogParam *param,
				  CompatDialogDoneCallback callback)
{
	if (plugin == NULL)
		return -1;
	m_plugin = plugin;
	m_callback = callback;
	if (param != NULL)
		m_param = *param;

	Plugin::PageOpenParam openParam;
	m_bgScene = m_plugin->PageOpen(IDParam("back_ground_page_id"), openParam);
	if (m_bgScene != NULL) {
		Plugin::TemplateOpenParam tplParam;
		m_plugin->TemplateOpen(m_bgScene, IDParam("plane_back_ground_template"), tplParam);
		m_plugin->TemplateOpen(m_bgScene, IDParam("plane_page_header_template"), tplParam);
		m_plugin->TemplateOpen(m_bgScene, IDParam("list_view_top_menu_template"), tplParam);
		m_topList = m_bgScene->FindChild(IDParam("list_view_top_menu"), 0);
	}

	m_section = COMPAT_MENU_TOP;
	m_open = true;
	return SCE_OK;
}

SceInt32 SceCompatMenuCtrl::HandleAbort(bool aborted)
{
	Close();
	if (m_callback != NULL) {
		m_callback(aborted ? SCE_COMMON_DIALOG_RESULT_ABORTED
				   : SCE_COMMON_DIALOG_RESULT_USER_CANCELED,
			   &m_result);
	}
	return SCE_OK;
}

void SceCompatMenuCtrl::Resume()
{
	if (!m_open)
		return;
	OpenSection(m_section);
}

void SceCompatMenuCtrl::Close()
{
	if (!m_open)
		return;
	if (m_plugin != NULL && m_bgScene != NULL) {
		Plugin::PageCloseParam closeParam;
		m_plugin->PageClose(IDParam("back_ground_page_id"), closeParam);
		m_bgScene = NULL;
	}
	m_topList = NULL;
	m_open = false;
}

void SceCompatMenuCtrl::OpenSection(CompatMenuSection section)
{
	m_section = section;
	if (m_plugin == NULL || m_bgScene == NULL)
		return;

	const char *tplName = NULL;
	switch (section) {
	case COMPAT_MENU_DISC_SWITCH:
		tplName = "ps1_menu_disc_switch_template";
		break;
	case COMPAT_MENU_CONTROLLER:
		tplName = "ps1_menu_controller_template";
		break;
	case COMPAT_MENU_BUTTON_ASSIGN:
		tplName = "ps1_menu_button_assign_template";
		break;
	case COMPAT_MENU_TOUCH_SCREEN:
		tplName = "plane_touch_screen_menu_template";
		break;
	case COMPAT_MENU_BACK_TOUCH_PAD:
		tplName = "plane_ps1_back_touch_screen_menu_template";
		break;
	case COMPAT_MENU_MEMORY_CARD:
		tplName = "ps1_menu_memorycard_template";
		break;
	case COMPAT_MENU_SCREEN_MODE:
		tplName = "screen_mode_custom_template";
		break;
	case COMPAT_MENU_OTHER:
		tplName = "ps1_menu_other_template";
		break;
	default:
		break;
	}
	if (tplName != NULL) {
		Plugin::TemplateOpenParam tplParam;
		m_plugin->TemplateOpen(m_bgScene, IDParam(tplName), tplParam);
	}
}

void SceCompatMenuCtrl::ApplyButtonAssign(bool customMode)
{
	m_customAssign = customMode;
}

void SceCompatMenuCtrl::SwitchDisc(SceInt32 discIndex)
{
	if (discIndex >= 1 && discIndex <= 5)
		m_activeDisc = discIndex;
}

static void compatPluginSetParam(Plugin::InitParam2 *param)
{
	if (param != NULL)
		param->draw_priority = 10;
}

static void compatPluginInit(Plugin *plugin)
{
	s_plugin = plugin;
	SceInt32 ret = SCE_OK;
	CompatApiScope scope("Init", ret);
	(void)scope;
	if (s_menuCtrl == NULL)
		s_menuCtrl = new SceCompatMenuCtrl();
}

static void compatPluginStart(Plugin *plugin)
{
	(void)plugin;
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void compatPluginStop(Plugin *plugin)
{
	if (s_menuCtrl != NULL) {
		s_menuCtrl->Close();
		delete s_menuCtrl;
		s_menuCtrl = NULL;
	}
	if (plugin != NULL)
		plugin->PageCloseAll();
}

static void compatPluginExit(Plugin *plugin)
{
	(void)plugin;
}

static SceInt32 compatModuleStart(SceSize arglen, const void *argp)
{
	(void)arglen;
	__dso_handle = &__dso_handle;
	for (void (**fn)(void) = __start__Zpreinit_array; fn < __stop__Zpreinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
	Module *module = *(Module *const *)argp;
	module->SetInterface(0, &s_moduleInterface);
	return SCE_KERNEL_START_SUCCESS;
}

static SceInt32 compatModuleStop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(&__dso_handle);
	return SCE_KERNEL_STOP_SUCCESS;
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	compatModuleStart(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	compatModuleStop(arglen, argp);
	return SCE_KERNEL_STOP_SUCCESS;
}
