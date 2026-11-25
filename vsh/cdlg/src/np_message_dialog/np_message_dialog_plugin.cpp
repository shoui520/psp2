/*
 * SceNpMessageDialogPlugin (vs0:sys/external/libcdlg_np_message.suprx):
 * launcher and sub-plugin orchestrator for NP Message Dialog.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <np/np_message.h>
#include <np_message_dialog.h>
#include <std/string.h>
#include <math/ngp/v4.h>
#include <framework.h>
#include <plugin.h>
#include <module/module.h>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include <widget/w_scene.h>
#include "../friendlist2_dialog/src/friend_select_plugin.h"
#include "np_message_dialog_plugin.h"

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

static Plugin::ModuleInterface s_moduleInterface;
static job::JobQueue *s_pluginLoadQueue = NULL;
static bool s_abortHandled = false;

static const SceUInt32 kBgPageHash = 0x9572A90Eu;
static const SceUInt32 kBgPlaneHash = 0x80F88408u;

class NpMessageDialogApiScope {
public:
	NpMessageDialogApiScope(const char *apiName, SceInt32 &ret)
		: m_apiName(apiName), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_NP_MESSAGE);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogNpMessageParam *)SceCommonDialogMain_DD2CAC30(
			COMMON_DIALOG_TYPE_NP_MESSAGE);
		if (m_work == NULL) {
			ret = NP_MESSAGE_DIALOG_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_MESSAGE);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~NpMessageDialogApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_MESSAGE);
		}
	}

	CommonDialogNpMessageParam *work(void) const
	{
		return m_work;
	}

private:
	const char *m_apiName;
	CommonDialogNpMessageParam *m_work;
};

static void npMessageDialogInit(SceSize argSize, const void *argp);
static void npMessageDialogTerm(void);
static void npMessagePluginInit(Plugin *plugin);
static void npMessagePluginStart(Plugin *plugin);
static void npMessagePluginStop(Plugin *plugin);
static void npMessagePluginExit(Plugin *plugin);
static void npMessagePluginSetParam(Plugin::InitParam2 *param);
static void OnUpdate(void *userData);
static void OnUnloadImplPlugin(const char *name);
static void CbLoadImplPlugin(SceInt32 kind, SceInt32 result, void *userData);
static void CbLoadFriendSelect(SceInt32 kind, SceInt32 result, void *userData);
static void CbPartyMemberListPluginLoaded(SceInt32 kind, SceInt32 result, void *userData);
static void CbContactsListPluginLoaded(SceInt32 kind, SceInt32 result, void *userData);

extern "C" int module_start(SceSize arglen, const void *argp)
{
	npMessageDialogInit(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	npMessageDialogTerm();
	return SCE_KERNEL_STOP_SUCCESS;
}

static void npMessageDialogInit(SceSize argSize, const void *argp)
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
	s_moduleInterface.set_param_func = npMessagePluginSetParam;
	s_moduleInterface.init_func = npMessagePluginInit;
	s_moduleInterface.start_func = npMessagePluginStart;
	s_moduleInterface.stop_func = npMessagePluginStop;
	s_moduleInterface.exit_func = npMessagePluginExit;
	if (module != NULL)
		module->SetInterface(0, &s_moduleInterface);
}

static void npMessageDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

ScePsmJobPluginLoad::ScePsmJobPluginLoad(SceInt32 kind)
	: job::JobItem("ScePsmJobPluginLoad", NULL)
	, m_kind(kind)
	, m_callback(NULL)
	, m_userData(NULL)
{
}

ScePsmJobPluginLoad::~ScePsmJobPluginLoad()
{
}

void ScePsmJobPluginLoad::SetCallback(NpMessagePluginLoadCallback callback, void *userData)
{
	m_callback = callback;
	m_userData = userData;
}

SceInt32 ScePsmJobPluginLoad::Run(void)
{
	if (IsCanceled())
		return -1;

	Plugin::InitParam initParam;
	char rcoPath[128];
	sce_paf_memset(rcoPath, 0, sizeof(rcoPath));

	if (m_kind == NP_MSG_SUBPLUGIN_IMPL) {
		if (SceCommonDialogMain_1DE9E1A7(rcoPath, sizeof(rcoPath),
				"np_message_dialog_impl_plugin.rco", 1) < 0)
			return -1;
		initParam.name = "np_message_dialog_impl_plugin";
		initParam.resource_file = rcoPath;
	} else if (m_kind == NP_MSG_SUBPLUGIN_FRIEND_SELECT) {
		initParam.name = "friend_select_plugin";
		initParam.resource_file = "vs0:data/external/cdlg/friend_select_plugin.rco";
		initParam.set_param_func = FriendSelectSetParam;
		initParam.init_func = FriendSelectInit;
		initParam.start_func = FriendSelectStart;
		initParam.stop_func = FriendSelectStop;
		initParam.exit_func = FriendSelectExit;
	} else if (m_kind == NP_MSG_SUBPLUGIN_CONTACTS) {
		initParam.name = "contacts_list_plugin";
		initParam.resource_file = "vs0:data/external/cdlg/contacts_list_plugin.rco";
	} else if (m_kind == NP_MSG_SUBPLUGIN_PARTY_MEMBERS) {
		initParam.name = "party_memberlist_plugin";
		initParam.resource_file = "vs0:data/external/cdlg/party_member_list_plugin.rco";
	} else {
		return -1;
	}

	Plugin::LoadAsync(initParam, NULL, Plugin::LoadUnloadOption_None);
	return SCE_OK;
}

void ScePsmJobPluginLoad::Finish(SceInt32 result)
{
	if (!IsCanceled() && m_callback != NULL)
		m_callback(m_kind, result, m_userData);
}

static SceUInt32 GetDialogBgColor(void)
{
	SceUInt32 packed = 0;
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("GetDialogBgColor", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL) {
		packed = ((SceUInt32)work->bgColor.r) |
			 ((SceUInt32)work->bgColor.g << 8) |
			 ((SceUInt32)work->bgColor.b << 16) |
			 ((SceUInt32)work->bgColor.a << 24);
	}
	return packed;
}

static SceInt32 GetDrawPriority(void)
{
	SceInt32 priority = 120;
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("GetDrawPriority", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL && work->drawPriority != -1)
		priority = work->drawPriority;
	return priority;
}

static SceInt32 GetDialogMode(void)
{
	SceInt32 mode = 0;
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("GetDialogMode", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL)
		mode = (SceInt32)work->param.mode;
	return mode;
}

static SceInt32 GetDialogStatus(void)
{
	SceInt32 status = 0;
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("GetDialogStatus", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL)
		status = work->result.result;
	return status;
}

static void SetDialogResult(const SceNpMessageDialogResult *newResult)
{
	if (newResult == NULL)
		return;
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("SetResult", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL) {
		sce_paf_memset(&work->result, 0, sizeof(work->result));
		work->result = *newResult;
		work->result.mode = work->param.mode;
		work->result.type = work->param.type;
		work->result.userdata = work->param.userData;
	}
}

static void EnqueueSubPluginLoad(SceInt32 kind, NpMessagePluginLoadCallback cb)
{
	if (s_pluginLoadQueue == NULL)
		return;
	ScePsmJobPluginLoad *job = new ScePsmJobPluginLoad(kind);
	if (job == NULL)
		return;
	job->SetCallback(cb, NULL);
	common::SharedPtr<job::JobItem> item(job);
	s_pluginLoadQueue->Enqueue(item);
}

static void OnUnloadImplPlugin(const char *name)
{
	(void)name;
}

static void UnloadImplPluginIfLoaded(void)
{
	if (Plugin::Find("np_message_dialog_impl_plugin") != NULL)
		Plugin::UnloadAsync("np_message_dialog_impl_plugin", OnUnloadImplPlugin,
				    Plugin::LoadUnloadOption_None);
}

static void OnPartyMemberListSelected(SceInt32 result)
{
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("OnPartyMemberListSelected", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL)
		work->result.result = result;
	UnloadImplPluginIfLoaded();
}

static void OnContactsListSelected(SceInt32 result)
{
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("OnContactsListSelected", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL)
		work->result.result = result;
	UnloadImplPluginIfLoaded();
}

static void OnFriendSelectSelected(SceInt32 result)
{
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("OnFriendSelectSelected", ret);
	CommonDialogNpMessageParam *work = scope.work();
	if (ret == SCE_OK && work != NULL)
		work->result.result = result;
	UnloadImplPluginIfLoaded();
}

static void CbLoadImplPlugin(SceInt32 kind, SceInt32 result, void *userData)
{
	(void)kind;
	(void)userData;
	if (result != SCE_OK) {
		SceNpMessageDialogResult errRes;
		sce_paf_memset(&errRes, 0, sizeof(errRes));
		errRes.result = result;
		SetDialogResult(&errRes);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NP_MESSAGE);
		return;
	}
	SceInt32 mode = GetDialogMode();
	if (mode == 1)
		EnqueueSubPluginLoad(NP_MSG_SUBPLUGIN_FRIEND_SELECT, CbLoadFriendSelect);
	else if (mode == 2)
		EnqueueSubPluginLoad(NP_MSG_SUBPLUGIN_CONTACTS, CbContactsListPluginLoaded);
	else if (mode == 3)
		EnqueueSubPluginLoad(NP_MSG_SUBPLUGIN_PARTY_MEMBERS, CbPartyMemberListPluginLoaded);
}

static void CbLoadFriendSelect(SceInt32 kind, SceInt32 result, void *userData)
{
	(void)kind;
	(void)userData;
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("CbLoadFriendSelect", ret);
	if (result != SCE_OK)
		OnFriendSelectSelected(result);
}

static void CbContactsListPluginLoaded(SceInt32 kind, SceInt32 result, void *userData)
{
	(void)kind;
	(void)userData;
	if (result != SCE_OK)
		OnContactsListSelected(result);
}

static void CbPartyMemberListPluginLoaded(SceInt32 kind, SceInt32 result, void *userData)
{
	(void)kind;
	(void)userData;
	SceInt32 ret = SCE_OK;
	NpMessageDialogApiScope scope("CbPartyMemberListPluginLoaded", ret);
	if (result != SCE_OK)
		OnPartyMemberListSelected(result);
}

static void OnUpdate(void *userData)
{
	(void)userData;
	if (s_abortHandled)
		return;
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NP_MESSAGE)) {
		s_abortHandled = true;
		SceNpMessageDialogResult abortRes;
		sce_paf_memset(&abortRes, 0, sizeof(abortRes));
		abortRes.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		SetDialogResult(&abortRes);
		UnloadImplPluginIfLoaded();
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NP_MESSAGE);
	}
}

static void npMessagePluginInit(Plugin *plugin)
{
	(void)plugin;
	job::JobQueue::Option opt;
	opt.priority = 0x100000FB;
	opt.stackSize = 0x4000;
	opt.numThreads = 1;
	s_pluginLoadQueue = new job::JobQueue("ScePsmDialogPluginLoadQueue", &opt);
	s_abortHandled = false;
}

static void npMessagePluginStart(Plugin *plugin)
{
	(void)plugin;
	Plugin *selfPlugin = Plugin::Find("np_message_dialog_plugin");
	if (selfPlugin != NULL) {
		SceInt32 drawPriority = GetDrawPriority();
		SceUInt32 bgPacked = GetDialogBgColor();
		Plugin::PageOpenParam openParam;
		openParam.overwrite_draw_priority = drawPriority;
		ui::Scene *bgPage = selfPlugin->PageOpen(IDParam(kBgPageHash), openParam);
		if (bgPage != NULL) {
			ui::Widget *bgPlane = bgPage->FindChild(IDParam(kBgPlaneHash), 0);
			if (bgPlane != NULL) {
				math::v4 color(
					(float)(bgPacked & 0xFFu) / 255.0f,
					(float)((bgPacked >> 8) & 0xFFu) / 255.0f,
					(float)((bgPacked >> 16) & 0xFFu) / 255.0f,
					(float)((bgPacked >> 24) & 0xFFu) / 255.0f);
				bgPlane->SetColor(color);
			}
		}
	}
	(void)GetDialogStatus();
	EnqueueSubPluginLoad(NP_MSG_SUBPLUGIN_IMPL, CbLoadImplPlugin);
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void npMessagePluginStop(Plugin *plugin)
{
	UnloadImplPluginIfLoaded();
	if (plugin != NULL)
		plugin->PageCloseAll();
}

static void npMessagePluginExit(Plugin *plugin)
{
	(void)plugin;
	if (s_pluginLoadQueue != NULL) {
		s_pluginLoadQueue->CancelAllItems();
		s_pluginLoadQueue->WaitEmpty();
		delete s_pluginLoadQueue;
		s_pluginLoadQueue = NULL;
	}
}

static void npMessagePluginSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}
