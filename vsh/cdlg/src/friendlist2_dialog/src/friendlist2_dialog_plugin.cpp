/*
 * SceFriendList2DialogPlugin (vs0:sys/external/libcdlg_friendlist2.suprx):
 * Launcher plugin bridging SceCommonDialogMain (COMMON_DIALOG_TYPE_FRIENDLIST2)
 * to the embedded "friend_select_plugin" ("vs0:data/external/cdlg/friend_select_plugin.rco").
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <common_dialog.h>
#include <np_friendlist2_dialog.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include "../../cdlg_mgr/common_dialog_mgr.h"
#include "../../cdlg_main/common_dialog_main.h"
#include "friend_select_plugin.h"

extern "C" {
#include "../../../../paf/src/std/stdc++/atexit.h"

struct SceHttpMemoryPoolStats {
	uint32_t poolSize;
	uint32_t maxInuseSize;
	uint32_t currentInuseSize;
	int32_t reserved;
};

struct SceSslMemoryPoolStats {
	uint32_t poolSize;
	uint32_t maxInuseSize;
	uint32_t currentInuseSize;
	int32_t reserved;
};

SceInt32 sceHttpGetMemoryPoolStats(SceHttpMemoryPoolStats *currentStat);
SceInt32 sceSslGetMemoryPoolStats(SceSslMemoryPoolStats *currentStat);
SceInt32 SceNpManager_A6964218(void);
}

using namespace paf;

#define FRIENDLIST2_ERROR_NO_WORK_MEMORY	-2146400174	/* 0x80108852 */
#define FRIENDLIST2_ERROR_API_MISMATCH		-2146400175	/* 0x80108851 */
#define FRIENDLIST2_ERROR_HTTP_POOL_SHORT	-2146400252	/* 0x80108804 */
#define FRIENDLIST2_ERROR_NP_NOT_SIGNED_IN	-2141913086	/* 0x80550002 */
#define FRIENDLIST2_ERROR_NET_DISCONNECTED	-2146400253	/* 0x80108803 */
#define FRIENDLIST2_ERROR_INTERNAL		-2146400255	/* 0x80108801 */
#define FRIENDLIST2_ERROR_CANCELED_SYSTEM	-2146400250	/* 0x80108806 */

typedef struct CommonDialogFriendList2Work {
	SceNpFriendList2DialogParam param;
	SceNpFriendList2DialogResult result;
	SceCommonDialogColor bgColor;
	char apiName[64];
} CommonDialogFriendList2Work;

static Plugin *s_launcherPlugin = NULL;
static bool s_launcherReady = false;
static SceNpId s_selectedNpId;

static int LockFriendList2Work(CommonDialogFriendList2Work **outWork, const char *apiName,
			       int lineNumber)
{
	(void)lineNumber;
	int ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_FRIENDLIST2);

	if (ret >= 0) {
		CommonDialogFriendList2Work *work =
			(CommonDialogFriendList2Work *)SceCommonDialogMain_DD2CAC30(
				COMMON_DIALOG_TYPE_FRIENDLIST2);
		if (work == NULL) {
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_FRIENDLIST2);
			return FRIENDLIST2_ERROR_NO_WORK_MEMORY;
		}
		sce_paf_strncpy(work->apiName, apiName, sizeof(work->apiName) - 1);
		work->apiName[sizeof(work->apiName) - 1] = '\0';
		*outWork = work;
	}
	return ret;
}

static int UnlockFriendList2Work(CommonDialogFriendList2Work *work, const char *apiName,
				 int lineNumber)
{
	(void)lineNumber;
	if (sce_paf_strncmp(work->apiName, apiName, sizeof(work->apiName)) != 0)
		return FRIENDLIST2_ERROR_API_MISMATCH;
	sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
	return SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_FRIENDLIST2);
}

static void OnFriendSelectUnloaded(const char *pluginName)
{
	(void)pluginName;
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_FRIENDLIST2);
}

static void LauncherMainThreadTick(void *userData);

static void OnFinishMainPlugin(int32_t selectResult, void *userData)
{
	(void)userData;
	CommonDialogFriendList2Work *work = NULL;

	if (LockFriendList2Work(&work, "OnFinishMainPlugin", 122) < 0)
		return;

	switch (selectResult) {
	case -5:
		work->result.result = FRIENDLIST2_ERROR_NET_DISCONNECTED;
		break;
	case -4:
		work->result.result = FRIENDLIST2_ERROR_INTERNAL;
		break;
	case -3:
		work->result.result = FRIENDLIST2_ERROR_CANCELED_SYSTEM;
		break;
	case -2:
		work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		break;
	case -1:
		work->result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
		break;
	case 0:
		sce_paf_memset(s_selectedNpId.reserved, 0, sizeof(s_selectedNpId.reserved));
		sce_paf_memcpy(&work->result.npId, &s_selectedNpId, sizeof(SceNpId));
		work->result.result = SCE_COMMON_DIALOG_RESULT_OK;
		break;
	default:
		break;
	}

	common::MainThreadCallList::Unregister(LauncherMainThreadTick, s_launcherPlugin);
	if (Plugin::Find("friend_select_plugin") != NULL)
		Plugin::UnloadAsync("friend_select_plugin", OnFriendSelectUnloaded, 0);

	UnlockFriendList2Work(work, "OnFinishMainPlugin", 168);
}

static void LauncherMainThreadTick(void *userData)
{
	if (!s_launcherReady) {
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_FRIENDLIST2);
		common::MainThreadCallList::Unregister(LauncherMainThreadTick, userData);
		return;
	}
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_FRIENDLIST2) != 0) {
		Plugin *selectPlugin = Plugin::Find("friend_select_plugin");

		if (selectPlugin != NULL) {
			FriendSelectInterface *iface =
				(FriendSelectInterface *)selectPlugin->GetInterface(1);
			if (iface != NULL && iface->abort != NULL) {
				iface->abort();
				common::MainThreadCallList::Unregister(LauncherMainThreadTick,
								       userData);
			}
		}
	}
}

static void LoadMainPlugin(void)
{
	CommonDialogFriendList2Work *work = NULL;

	if (LockFriendList2Work(&work, "LoadMainPlugin", 64) < 0)
		return;

	Plugin::InitParam initParam;

	initParam.name = "friend_select_plugin";
	initParam.caller_name = "friendlist2_dialog_plugin";
	initParam.resource_file = "vs0:data/external/cdlg/friend_select_plugin.rco";
	initParam.set_param_func = FriendSelectSetParam;
	initParam.init_func = FriendSelectInit;
	initParam.start_func = FriendSelectStart;
	initParam.stop_func = FriendSelectStop;
	initParam.exit_func = FriendSelectExit;
	initParam.notify_callback = OnFinishMainPlugin;
	initParam.draw_priority = 70;

	initParam.arguments["mode"] = Plugin::Argument(0);
	initParam.arguments["npid"] = Plugin::Argument((int32_t)(uintptr_t)&s_selectedNpId);
	initParam.arguments["sort"] =
		Plugin::Argument(work->param.defaultSort != SCE_NP_FRIENDLIST2_DIALOG_SORT_BY_ID
					 ? 1
					 : 0);

	if (work->param.commonParam.bgColor != NULL) {
		uint32_t rgba = ((uint32_t)work->bgColor.a << 24) |
				((uint32_t)work->bgColor.b << 16) |
				((uint32_t)work->bgColor.g << 8) | (uint32_t)work->bgColor.r;
		initParam.arguments["bg_color"] = Plugin::Argument((int32_t)rgba);
	}

	Plugin::LoadAsync(initParam, NULL, NULL);
	UnlockFriendList2Work(work, "LoadMainPlugin", 112);
}

static void InitLauncherPlugin(Plugin *plugin)
{
	s_launcherPlugin = plugin;
	s_launcherReady = false;
	sce_paf_memset(&s_selectedNpId, 0, sizeof(s_selectedNpId));

	SceHttpMemoryPoolStats httpStat;
	SceSslMemoryPoolStats sslStat;
	int ret = sceHttpGetMemoryPoolStats(&httpStat);

	if (ret >= 0) {
		if (httpStat.poolSize - httpStat.currentInuseSize >= 0x18000U) {
			ret = sceSslGetMemoryPoolStats(&sslStat);
			if (ret >= 0) {
				if (sslStat.poolSize - sslStat.currentInuseSize < 0x20000U)
					return;
				if (SceNpManager_A6964218() != 0) {
					s_launcherReady = true;
					return;
				}
				ret = FRIENDLIST2_ERROR_NP_NOT_SIGNED_IN;
			}
		} else {
			ret = FRIENDLIST2_ERROR_HTTP_POOL_SHORT;
		}
	}

	CommonDialogFriendList2Work *work = NULL;

	if (LockFriendList2Work(&work, "InitLauncherPlugin", 280) >= 0) {
		work->result.result = ret;
		UnlockFriendList2Work(work, "InitLauncherPlugin", 280);
	}
}

static void StartLauncherPlugin(Plugin *plugin)
{
	if (s_launcherReady)
		LoadMainPlugin();
	common::MainThreadCallList::Register(LauncherMainThreadTick, plugin);
}

static void StopLauncherPlugin(Plugin *plugin)
{
	(void)plugin;
}

static void ExitLauncherPlugin(Plugin *plugin)
{
	(void)plugin;
	s_launcherPlugin = NULL;
}

static void SetLauncherPluginParam(Plugin::InitParam2 *param)
{
	param->draw_priority = 80;
}

extern "C" {
void *__dso_handle;
extern void (*__start__Zpreinit_array[])(void);
extern void (*__stop__Zpreinit_array[])(void);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);
}

static void FriendList2DialogInit(uint32_t arglen, const void *argp)
{
	(void)arglen;
	__dso_handle = &__dso_handle;
	for (void (**ctor)(void) = __start__Zpreinit_array; ctor < __stop__Zpreinit_array; ++ctor) {
		if (*ctor != NULL)
			(*ctor)();
	}
	for (void (**ctor)(void) = __start__Zinit_array; ctor < __stop__Zinit_array; ++ctor) {
		if (*ctor != NULL)
			(*ctor)();
	}

	Module *module = *(Module *const *)argp;
	static Plugin::ModuleInterface s_pluginParam;

	s_pluginParam.set_param_func = SetLauncherPluginParam;
	s_pluginParam.init_func = InitLauncherPlugin;
	s_pluginParam.start_func = StartLauncherPlugin;
	s_pluginParam.stop_func = StopLauncherPlugin;
	s_pluginParam.exit_func = ExitLauncherPlugin;
	module->SetInterface(0, &s_pluginParam);
}

static void FriendList2DialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	FriendList2DialogInit(arglen, argp);
	return 0;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	FriendList2DialogTerm();
	return 0;
}
