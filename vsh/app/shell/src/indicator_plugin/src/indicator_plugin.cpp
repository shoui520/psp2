/*
 * indicator_plugin.cpp: indicator_plugin's set-param, init, start, stop and exit functions
 * (0x8115B560-0x8115B59C), as boot.cpp loads the plugin. The folder's and the file's names are
 * ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "../../../plugins.h"
#include "indicator.h"

using namespace paf;

/* the plugin's draw priority and surface pool, as boot.cpp makes it (0x8115B560) */
void indicatorPluginSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = 20;
	param->surface_pool_size = 0x4b000;
}

/* the interface given (0x8115B56C) */
void indicatorPluginInit(Plugin *plugin)
{
	indicatorInterfaceRegister(plugin);
	indicatorInterfaceInit();
}

/* the cache manager and the indicator made (0x8115B578) */
void indicatorPluginStart(Plugin *plugin)
{
	indicatorCacheCreate(plugin);
	indicatorCreate();
}

/* (0x8115B584) */
void indicatorPluginStop(Plugin *plugin)
{
	indicatorDestroy();
	indicatorCacheDestroy();
}

/* (0x8115B590) */
void indicatorPluginExit(Plugin *plugin)
{
	indicatorInterfaceExit();
	indicatorExitReserved();
}
