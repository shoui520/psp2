/*
 * The game plugin's entry points, which SceShell's boot loads it with: its interface is set at
 * init, its main object made at start and deleted at stop. The file's and the functions' names
 * are ours (no record names them).
 */
#include <stddef.h>
#include <plugin.h>

#include "game_plugin.h"
#include "../../shell/plugins.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"

using namespace paf;

static Plugin *s_plugin;

void gamePluginInit(Plugin *plugin)
{
	s_plugin = plugin;
	if (plugin != NULL)
		plugin->SetInterface(1, &gamePluginInterface);
}

void gamePluginStart(Plugin *plugin)
{
	gamePluginCreate(plugin);
}

void gamePluginStop(Plugin *plugin)
{
	gamePluginDestroy();
}

void gamePluginExit(Plugin *plugin)
{
	s_plugin = NULL;
}
