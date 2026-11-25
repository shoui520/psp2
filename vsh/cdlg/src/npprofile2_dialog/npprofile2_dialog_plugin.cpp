/*
 * SceNpProfile2DialogPlugin (vs0:sys/external/libcdlg_npprofile2.suprx):
 * plugin entry points, HTTP/SSL pool checks, friend_profile_plugin loader,
 * and CommonDialogMain work-memory bridge.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <libssl.h>
#include <common_dialog.h>
#include <std/string.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include "npprofile2_dialog_plugin.h"

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
int SceNpManager_A6964218(void);
}

static Plugin *s_launcherPlugin = NULL;
static bool s_poolReady = false;
static bool s_profilePluginLoaded = false;

static void npProfile2PluginSetParam(Plugin::InitParam2 *param);
static void npProfile2PluginInit(Plugin *plugin);
static void npProfile2PluginStart(Plugin *plugin);
static void npProfile2PluginStop(Plugin *plugin);
static void npProfile2PluginExit(Plugin *plugin);

static Plugin::ModuleInterface s_moduleInterface = {
	npProfile2PluginSetParam,
	npProfile2PluginInit,
	npProfile2PluginStart,
	npProfile2PluginStop,
	npProfile2PluginExit
};

class NpProfileApiScope {
public:
	NpProfileApiScope(const char *apiName, SceInt32 &err)
		: m_work(NULL),
		  m_apiName(apiName)
	{
		err = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_NPPROFILE2);
		if (err != SCE_OK)
			return;
		m_work = (CommonDialogNpProfileParam *)
			SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_NPPROFILE2);
		if (m_work == NULL) {
			err = -1;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NPPROFILE2);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		err = SCE_OK;
	}

	~NpProfileApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NPPROFILE2);
		}
	}

	CommonDialogNpProfileParam *work() const
	{
		return m_work;
	}

private:
	CommonDialogNpProfileParam *m_work;
	const char *m_apiName;
};

static void OnProfileSubSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

static void OnProfileSubInit(Plugin *plugin)
{
	(void)plugin;
}

static void OnProfileSubStart(Plugin *plugin)
{
	(void)plugin;
}

static void OnProfileSubStop(Plugin *plugin)
{
	(void)plugin;
}

static void OnProfileSubExit(Plugin *plugin)
{
	(void)plugin;
}

static void OnProfilePluginLoaded(Plugin *plugin)
{
	(void)plugin;
	s_profilePluginLoaded = true;
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	NpProfileApiScope scope("OnUpdate", ret);
	CommonDialogNpProfileParam *work = scope.work();
	if (ret != SCE_OK || work == NULL)
		return;

	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NPPROFILE2)) {
		work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NPPROFILE2);
	}
}

static void LoadMainPlugin(void)
{
	SceInt32 ret = SCE_OK;
	NpProfileApiScope scope("LoadMainPlugin", ret);
	if (ret != SCE_OK)
		return;

	Plugin::InitParam initParam;
	initParam.name = "friend_profile_plugin";
	initParam.caller_name = "npprofile2_dialog_plugin";
	initParam.resource_file = "vs0:data/external/cdlg/friend_profile_plugin.rco";
	initParam.set_param_func = OnProfileSubSetParam;
	initParam.init_func = OnProfileSubInit;
	initParam.start_func = OnProfileSubStart;
	initParam.stop_func = OnProfileSubStop;
	initParam.exit_func = OnProfileSubExit;
	Plugin::LoadAsync(initParam, OnProfilePluginLoaded, Plugin::LoadUnloadOption_None);
}

static void npProfile2PluginSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

static void npProfile2PluginInit(Plugin *plugin)
{
	s_launcherPlugin = plugin;
	s_poolReady = false;

	SceHttpMemoryPoolStats httpStat;
	SceSslMemoryPoolStats sslStat;
	SceInt32 err = sceHttpGetMemoryPoolStats(&httpStat);
	if (err >= 0) {
		if (httpStat.poolSize - httpStat.currentInuseSize < 0x18000u) {
			err = NPPROFILE2_ERROR_HTTP_POOL;
		} else {
			err = sceSslGetMemoryPoolStats(&sslStat);
			if (err >= 0) {
				if (sslStat.poolSize - sslStat.currentInuseSize < 0x20000u) {
					err = NPPROFILE2_ERROR_SSL_POOL;
				} else if (SceNpManager_A6964218() != 0) {
					s_poolReady = true;
					return;
				} else {
					err = NPPROFILE2_ERROR_NOT_SIGNED_IN;
				}
			}
		}
	}

	SceInt32 lockRet = SCE_OK;
	NpProfileApiScope scope("InitLauncherPlugin", lockRet);
	CommonDialogNpProfileParam *work = scope.work();
	if (lockRet == SCE_OK && work != NULL)
		work->result.result = err;
}

static void npProfile2PluginStart(Plugin *plugin)
{
	(void)plugin;
	if (s_poolReady) {
		LoadMainPlugin();
		common::MainThreadCallList::Register(OnUpdate, NULL);
	} else {
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NPPROFILE2);
	}
}

static void npProfile2PluginStop(Plugin *plugin)
{
	(void)plugin;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
}

static void npProfile2PluginExit(Plugin *plugin)
{
	(void)plugin;
	if (s_profilePluginLoaded) {
		Plugin::UnloadAsync("friend_profile_plugin", NULL, Plugin::LoadUnloadOption_None);
		s_profilePluginLoaded = false;
	}
	s_launcherPlugin = NULL;
}

static SceInt32 npProfile2ModuleStart(SceSize arglen, const void *argp)
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

static SceInt32 npProfile2ModuleStop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(&__dso_handle);
	return SCE_KERNEL_STOP_SUCCESS;
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	npProfile2ModuleStart(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	npProfile2ModuleStop(arglen, argp);
	return SCE_KERNEL_STOP_SUCCESS;
}
