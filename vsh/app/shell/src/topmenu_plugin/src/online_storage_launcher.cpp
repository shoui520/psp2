/*
 * online_storage_launcher.cpp: loads online_storage_plugin.suprx for a title
 * from the LiveArea PlayStation Plus saved-data icon (0x8129984C-0x81299ABF).
 */
#include "topmenu_internal.h"

namespace {

static bool s_onlineStorageActive;
static char s_onlineStorageTitleId[14];

/* 0x81299A38 */
static void onOnlineStorageLoaded(paf::Plugin *plugin)
{
	(void)plugin;
}

/* 0x81299A3A */
static void onOnlineStorageUnloaded(void *userdata)
{
	(void)userdata;
	s_onlineStorageActive = false;
	topmenuEndGameHold();
}

/* 0x8129984C */
static int loadOnlineStoragePlugin(const LsdbAppInfoPtr *appInfo)
{
	const char *titleId = sceLsdbGetName(appInfo);
	sce_paf_strncpy(s_onlineStorageTitleId, titleId, 13);
	s_onlineStorageTitleId[12] = '\0';

	paf::Plugin::InitParam param;
	param.name = "online_storage_plugin";
	param.resource_file = "vs0:vsh/online_storage/online_storage_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/online_storage/online_storage_plugin.suprx";
	param.module_interface_version = 1;
	param.module_option = 1;
	param.unload_callback = onOnlineStorageUnloaded;
	param.arguments[paf::string("title_id")] =
		paf::Plugin::Argument((int32_t)(intptr_t)s_onlineStorageTitleId);

	paf::Plugin::LoadAsync(param, onOnlineStorageLoaded, NULL);
	return 0;
}

} /* namespace */

/* 0x81299A0A */
int topmenuLaunchOnlineStorage(const LsdbAppInfoPtr *appInfo)
{
	if (s_onlineStorageActive)
		return 0;

	if (loadOnlineStoragePlugin(appInfo) == 0) {
		s_onlineStorageActive = true;
		topmenuBeginGameHold();
	}
	return 1;
}
