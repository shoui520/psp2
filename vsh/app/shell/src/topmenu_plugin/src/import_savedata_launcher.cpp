/*
 * import_savedata_launcher.cpp: loads and drives import_savedata_plugin from
 * the LiveArea action bar (0x812962C4-0x8129666B).
 */
#include "topmenu_internal.h"

namespace {

struct ImportSavedataTitleParam {
	char titleId[16];
};

typedef void (*ImportSavedataFinishCallback)(void);
typedef void (*ImportSavedataStateCallback)(int busy);

struct ImportSavedataInterface {
	int (*startImport)(ImportSavedataTitleParam titleParam,
			   ImportSavedataFinishCallback finishCb, void *userData);
	void (*cancelImport)(void);
	int (*setStateCallback)(ImportSavedataStateCallback stateCb, void *userData);
};

static bool s_importSavedataActive;
static ImportSavedataTitleParam s_importSavedataTitle;

/* 0x8129642C */
static void onImportSavedataFinished(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	paf::Plugin::UnloadAsync("import_savedata_plugin", NULL, NULL);
	s_importSavedataActive = false;
	topmenuEndGameHold();
	topmenuEndBusy();
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x81296468 */
static void onImportSavedataStateChanged(int busy)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	if (busy == 1) {
		topmenuBeginGameHold();
		topmenuBeginBusy();
	} else {
		topmenuEndGameHold();
		topmenuEndBusy();
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x81296498 */
static void onImportSavedataPluginLoaded(paf::Plugin *plugin)
{
	if (plugin == NULL)
		return;

	ImportSavedataInterface *iface =
		(ImportSavedataInterface *)plugin->GetInterface(1);
	if (iface == NULL) {
		paf::Plugin::UnloadAsync("import_savedata_plugin", NULL, NULL);
		return;
	}

	if (iface->setStateCallback(onImportSavedataStateChanged, NULL) != 0 ||
	    iface->startImport(s_importSavedataTitle, onImportSavedataFinished, NULL) != 0) {
		paf::Plugin::UnloadAsync("import_savedata_plugin", NULL, NULL);
	}
}

} /* namespace */

/* 0x812962C4 */
int topmenuLaunchImportSavedata(const LsdbAppInfoPtr *appInfo)
{
	if (s_importSavedataActive)
		return 0;

	const char *titleId = sceLsdbGetName(appInfo);
	sce_paf_strncpy(s_importSavedataTitle.titleId, titleId, 13);
	s_importSavedataActive = true;

	paf::Plugin::InitParam param;
	param.name = "import_savedata_plugin";
	param.resource_file = "vs0:vsh/import_savedata/import_savedata_plugin.rco";
	param.caller_name = "__main__";
	param.set_param_func = importSavedataPluginSetParam;
	param.init_func = importSavedataPluginInit;
	param.start_func = importSavedataPluginStart;
	param.stop_func = importSavedataPluginStop;
	param.exit_func = importSavedataPluginExit;
	param.private_heap_size = 0;

	paf::Plugin::LoadAsync(param, onImportSavedataPluginLoaded, NULL);
	topmenuBeginGameHold();
	topmenuBeginBusy();
	return 1;
}
