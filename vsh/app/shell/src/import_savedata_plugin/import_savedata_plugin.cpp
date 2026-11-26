/*
 * import_savedata_plugin.cpp: plugin lifecycle hooks and interface 1 entry points
 * for import_savedata_plugin (0x81393628..0x813937F3).
 */
#include "import_savedata_internal.h"

static ImportSaveDataController *s_controller = NULL;	/* 0x81548D50 */

/* 0x813936F2 */
static SceInt32 importSavedataInterfaceStartImport(ImportSavedataTitleParam titleParam,
						   ImportSavedataFinishCallback finishCb,
						   void *userData)
{
	if (s_controller != NULL)
		s_controller->StartImport(titleParam, finishCb, userData);
	return 0;
}

/* 0x81393758 */
static SceInt32 importSavedataInterfaceCancelImport(void)
{
	if (s_controller != NULL)
		return s_controller->CancelImport();
	return 0;
}

/* 0x8139376A */
static SceInt32 importSavedataInterfaceSetStateCallback(ImportSavedataStateCallback stateCb,
							void *userData)
{
	if (s_controller != NULL)
		s_controller->SetStateCallback(stateCb, userData);
	return 0;
}

/* 0x81548D54 */
static ImportSavedataPluginInterface s_pluginInterface = {
	importSavedataInterfaceStartImport,
	importSavedataInterfaceCancelImport,
	importSavedataInterfaceSetStateCallback
};

/* 0x81393628 */
void importSavedataPluginSetParam(paf::Plugin::InitParam2 *param)
{
	param->draw_priority = 45;
}

/* 0x8139362E */
void importSavedataPluginInit(paf::Plugin *plugin)
{
	importSavedataSetPlugin(plugin);

	if (s_controller == NULL)
		s_controller = new ImportSaveDataController();

	ImportSaveDataJobManager::Create();
	ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
	if (jobMgr != NULL)
		jobMgr->CreateQueue();

	plugin->SetInterface(1, &s_pluginInterface);
}

/* 0x81393688 */
void importSavedataPluginStart(paf::Plugin *plugin)
{
	(void)plugin;
}

/* 0x8139369E */
void importSavedataPluginStop(paf::Plugin *plugin)
{
	(void)plugin;
}

/* 0x813936B4 */
void importSavedataPluginExit(paf::Plugin *plugin)
{
	(void)plugin;

	if (s_controller != NULL) {
		s_controller->Cleanup();
		delete s_controller;
		s_controller = NULL;
	}

	ImportSaveDataJobManager::Destroy();
	importSavedataClearPlugin();
}
