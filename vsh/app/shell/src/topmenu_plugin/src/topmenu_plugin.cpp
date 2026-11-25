/*
 * topmenu_plugin.cpp: plugin lifecycle entry points for topmenu_plugin
 * (0x8126F718-0x8126F817).
 */
#include "topmenu_internal.h"

/* 0x8126F718 */
void topmenuPluginSetParam(paf::Plugin::InitParam2 *param)
{
	param->draw_priority = 50;
	param->surface_pool_size = 0x40000;
}

/* 0x8126F724 */
void topmenuPluginInit(paf::Plugin *plugin)
{
	topmenuInterfaceInit(plugin);
	topmenuHeapInit();
	topmenuLsdbSyncInit();
	topmenuTopWindowInitConfig();
	topmenuHelpInit();
	topmenuWhatsNewInit();
	topmenuDialogMgrInit();
	g_topmenuInterface->checkCrashReportFile = topmenuCheckCrashReportFile;
}

/* 0x8126F75A */
void topmenuPluginStart(paf::Plugin *plugin)
{
	topmenuLsdbSyncStart();
	topmenuMotionInit();
	topmenuTopWindowStart(plugin);
	topmenuBootScreenStart();
}

/* 0x8126F772 */
void topmenuPluginStop(paf::Plugin *plugin)
{
	(void)plugin;
	topmenuBootScreenStop();
	topmenuTopWindowStop();
	topmenuLsdbSyncStop();
}

/* 0x8126F786 */
void topmenuPluginExit(paf::Plugin *plugin)
{
	(void)plugin;
	topmenuDialogMgrTerm();
	topmenuWhatsNewTerm();
	topmenuHelpTerm();
	topmenuTopWindowExit();
	topmenuLsdbSyncTerm();
	topmenuHeapTerm();
	topmenuInterfaceTerm();
}
