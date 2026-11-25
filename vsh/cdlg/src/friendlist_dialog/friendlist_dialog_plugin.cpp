/*
 * SceFriendListDialogPlugin (vs0:sys/external/libcdlg_friendlist.suprx):
 * plugin entry points, CommonDialogMain work-memory bridge, and SceFriendListDialogCtrl.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <std/string.h>
#include <framework.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include "friendlist_dialog_plugin.h"

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

extern "C" {
int sceSysmoduleLoadModuleInternal(SceUInt32 id);
int sceSysmoduleUnloadModuleInternal(SceUInt32 id);
SceUInt32 SceLibKernel_9F793F84(void);
}

static SceFriendListDialogCtrl *s_dialogCtrl = NULL;
static SceUInt32 s_cachedSdkVersion = 0;

static void friendListPluginSetParam(Plugin::InitParam2 *param);
static void friendListPluginInit(Plugin *plugin);
static void friendListPluginStart(Plugin *plugin);
static void friendListPluginStop(Plugin *plugin);
static void friendListPluginExit(Plugin *plugin);

static Plugin::ModuleInterface s_moduleInterface = {
	friendListPluginSetParam,
	friendListPluginInit,
	friendListPluginStart,
	friendListPluginStop,
	friendListPluginExit
};

class FriendListApiScope {
public:
	FriendListApiScope(const char *apiName, SceInt32 &err)
		: m_work(NULL),
		  m_apiName(apiName)
	{
		err = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_FRIENDLIST);
		if (err != SCE_OK)
			return;
		m_work = (CommonDialogFriendListParam *)
			SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_FRIENDLIST);
		if (m_work == NULL) {
			err = -1;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_FRIENDLIST);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		err = SCE_OK;
	}

	~FriendListApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_FRIENDLIST);
		}
	}

	CommonDialogFriendListParam *work() const
	{
		return m_work;
	}

private:
	CommonDialogFriendListParam *m_work;
	const char *m_apiName;
};

SceFriendListDialogCtrl::SceFriendListDialogCtrl(Plugin *plugin)
	: m_plugin(plugin),
	  m_scene(NULL),
	  m_sessionHandle(-1),
	  m_active(false)
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));
}

SceFriendListDialogCtrl::~SceFriendListDialogCtrl()
{
	Stop();
}

SceInt32 SceFriendListDialogCtrl::Start(const CommonDialogFriendListParam *work)
{
	if (work != NULL)
		m_param = work->param;
	m_sessionHandle = 0;
	m_active = true;
	return SCE_OK;
}

void SceFriendListDialogCtrl::Stop()
{
	if (!m_active)
		return;
	m_sessionHandle = -1;
	m_active = false;
}

void SceFriendListDialogCtrl::Cancel()
{
	Stop();
}

bool SceFriendListDialogCtrl::IsActive() const
{
	return m_active;
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	FriendListApiScope scope("OnUpdate", ret);
	CommonDialogFriendListParam *work = scope.work();
	if (ret != SCE_OK || work == NULL)
		return;

	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_FRIENDLIST)) {
		if (s_dialogCtrl != NULL)
			s_dialogCtrl->Cancel();
		if (s_cachedSdkVersion == 0)
			s_cachedSdkVersion = SceLibKernel_9F793F84();
		if (s_cachedSdkVersion != 0) {
			work->result.result = (s_cachedSdkVersion >= 0x01650000u)
				? SCE_COMMON_DIALOG_RESULT_ABORTED
				: SCE_COMMON_DIALOG_RESULT_OK;
		}
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_FRIENDLIST);
	}
}

static void friendListPluginSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

static void friendListPluginInit(Plugin *plugin)
{
	Framework *fw = Framework::Instance();
	if (fw == NULL || fw->GetMode() != 5)
		sceSysmoduleLoadModuleInternal(0x80000018u);
	s_dialogCtrl = new SceFriendListDialogCtrl(plugin);
}

static void friendListPluginStart(Plugin *plugin)
{
	(void)plugin;
	SceInt32 ret = SCE_OK;
	FriendListApiScope scope("Start", ret);
	CommonDialogFriendListParam *work = scope.work();
	if (ret == SCE_OK && work != NULL && s_dialogCtrl != NULL)
		s_dialogCtrl->Start(work);
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void friendListPluginStop(Plugin *plugin)
{
	(void)plugin;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	if (s_dialogCtrl != NULL)
		s_dialogCtrl->Stop();
}

static void friendListPluginExit(Plugin *plugin)
{
	(void)plugin;
	if (s_dialogCtrl != NULL) {
		delete s_dialogCtrl;
		s_dialogCtrl = NULL;
	}
	Framework *fw = Framework::Instance();
	if (fw == NULL || fw->GetMode() != 5)
		sceSysmoduleUnloadModuleInternal(0x80000018u);
}

static SceInt32 friendListModuleStart(SceSize arglen, const void *argp)
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

static SceInt32 friendListModuleStop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(&__dso_handle);
	return SCE_KERNEL_STOP_SUCCESS;
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	friendListModuleStart(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	friendListModuleStop(arglen, argp);
	return SCE_KERNEL_STOP_SUCCESS;
}
