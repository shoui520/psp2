/*
 * impose_plugin.cpp: impose_plugin's set-param, init, start, stop and exit functions
 * (0x81140C48-0x81140CF0), as boot.cpp loads the plugin. The folder's and the file's names are
 * ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "../../../plugins.h"
#include "../../livespace_db/lsdb_keys.h"
#include "impose.h"

using namespace paf;

/* the plugin's draw priority and surface pool, as boot.cpp makes it (0x81140C48) */
void imposePluginSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = 20;
	param->surface_pool_size = 0x40000;
}

/* the interface given and the plugin's object made (0x81140C54) */
void imposePluginInit(Plugin *plugin)
{
	imposeInterfaceRegister(plugin);
	Impose::Create(plugin);
}

/* (0x81140C64) */
void imposePluginStart(Plugin *plugin)
{
	Impose::Start();
}

/* (0x81140C6C) */
void imposePluginStop(Plugin *plugin)
{
	Impose::Stop();
}

/* (0x81140C74) */
void imposePluginExit(Plugin *plugin)
{
	Impose::Destroy();
	imposeExitReserved();
}
