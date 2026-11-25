/*
 * patch_chk_launcher.cpp: loads and drives patch_chk_plugin from the LiveArea
 * update-check icon (0x81299C50-0x8129A0DF).
 */
#include "topmenu_internal.h"
#include <modulemgr/module_info.h>
#include "../../patch_chk_plugin/patch_chk_plugin.h"

extern "C" int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *param);

namespace {

static PatchChkStartParam s_patchChkParam;
static bool s_patchChkActive;

/* 0x81299ECA */
static void onPatchChkFinished(int result, const PatchChkResultData *resultData, void *userData)
{
	(void)result;
	(void)resultData;
	(void)userData;

	paf::thread::RMutex::main_thread_mutex.Lock();
	PatchChkInterface *iface =
		(PatchChkInterface *)topmenuFindPluginInterface("patch_chk_plugin", 1);
	if (iface != NULL)
		iface->setStateCallback(NULL, NULL);

	paf::Plugin::UnloadAsync("patch_chk_plugin", NULL, NULL);
	s_patchChkActive = false;
	topmenuEndGameHold();
	topmenuEndBusy();
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x81299F2E */
static void onPatchChkStateChanged(int state, void *userData)
{
	(void)userData;
	if (state == 1)
		netCtlApiIncActiveRef();
	else if (state == 0)
		netCtlApiDecActiveRef();
}

/* 0x81299F42 */
static void onPatchChkPluginLoaded(paf::Plugin *plugin)
{
	if (plugin == NULL)
		return;

	PatchChkInterface *iface = (PatchChkInterface *)plugin->GetInterface(1);
	if (iface == NULL) {
		paf::Plugin::UnloadAsync("patch_chk_plugin", NULL, NULL);
		return;
	}

	if (iface->startCheck(s_patchChkParam, onPatchChkFinished, NULL) != 0)
		paf::Plugin::UnloadAsync("patch_chk_plugin", NULL, NULL);
	else
		iface->setStateCallback(onPatchChkStateChanged, NULL);
}

} /* namespace */

/* 0x81299C50 */
int topmenuLaunchPatchChk(const LsdbAppInfoPtr *appInfo)
{
	if (s_patchChkActive)
		return 0;

	SceKernelSystemSwVersion swVersion;
	sce_paf_memset(&swVersion, 0, sizeof(swVersion));
	swVersion.size = sizeof(swVersion);
	if (sceKernelGetSystemSwVersion(&swVersion) != 0)
		return 0;

	const char *titleId = sceLsdbGetName(appInfo);
	const char *appVer = sceLsdbGetAppVer(appInfo);
	const char *contentId = SceLsdb_8FDA59EA(appInfo);
	const char *gameDataId = sceLsdbGetGameDataId(appInfo);

	typedef int (*GetCategoryKindFn)(const LsdbAppInfoPtr *info, int *outKind);
	typedef int (*GetStorageFn)(const LsdbAppInfoPtr *info, int *outStorage);

	int categoryKind = 0;
	if (((GetCategoryKindFn)g_topWindow->m_gameIf->func[GAME_PLUGIN_FUNC_GET_CATEGORY_KIND])(
		    appInfo, &categoryKind) != 0) {
		return 0;
	}

	if (titleId == NULL || appVer == NULL)
		return 0;
	if (categoryKind == 1 && contentId == NULL)
		return 0;

	int storageKind = 0;
	if (((GetStorageFn)g_topWindow->m_gameIf->func[GAME_PLUGIN_FUNC_GET_STORAGE])(
		    appInfo, &storageKind) != 0) {
		return 0;
	}

	sce_paf_memset(&s_patchChkParam, 0, sizeof(s_patchChkParam));
	s_patchChkParam.checkMode = 0;
	s_patchChkActive = true;
	sce_paf_strncpy(s_patchChkParam.titleId, titleId, sizeof(s_patchChkParam.titleId));
	sce_paf_strncpy(s_patchChkParam.currentAppVer, appVer, 6);
	if (contentId != NULL)
		sce_paf_strncpy(s_patchChkParam.contentId, contentId,
				sizeof(s_patchChkParam.contentId));
	if (gameDataId != NULL)
		sce_paf_strncpy(s_patchChkParam.patchTitleId, gameDataId,
				sizeof(s_patchChkParam.patchTitleId));

	s_patchChkParam.currentPsp2SysVer = swVersion.version;
	s_patchChkParam.targetPlatform = categoryKind;
	if (categoryKind == 2)
		s_patchChkParam.currentPspSysVer =
			(SceUInt32)sceCompatGetPspSystemSoftwareVersion();

	paf::Plugin::InitParam param;
	param.name = "patch_chk_plugin";
	param.resource_file = "vs0:data/external/cdlg/patch_chk_plugin.rco";
	param.caller_name = "__main__";
	param.set_param_func = patchChkPluginSetParam;
	param.init_func = patchChkPluginInit;
	param.start_func = patchChkPluginStart;
	param.stop_func = patchChkPluginStop;
	param.exit_func = patchChkPluginExit;
	param.private_heap_size = 0;

	paf::Plugin::LoadAsync(param, onPatchChkPluginLoaded, NULL);
	topmenuBeginGameHold();
	topmenuBeginBusy();
	return 1;
}
