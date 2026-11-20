/*
 * coldboot_plugin.cpp: the cold boot plugin's set-param, init, start, stop and exit functions
 * (0x810E4BBC-0x810E4BF0), as boot.cpp loads the plugin. The plugin's page, opened while the
 * shell boots, is coldboot.cpp's. The folder's, the file's and every name here are ours
 * (PLACEHOLDER: no record names them), after the plugin's own name.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "../../../plugins.h"
#include "coldboot.h"

using namespace paf;

/* the plugin's draw priority */
#define COLDBOOT_DRAW_PRIORITY		24

/* (0x810E4BBC) */
void coldbootPluginSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = COLDBOOT_DRAW_PRIORITY;
}

/* (0x810E4BC2) */
void coldbootPluginInit(Plugin *plugin)
{
	coldbootInterfaceRegister(plugin);
	coldbootCreate(plugin);
}

/* (0x810E4BD2) */
void coldbootPluginStart(Plugin *plugin)
{
	coldbootOpen();
}

/* (0x810E4BDA) */
void coldbootPluginStop(Plugin *plugin)
{
	coldbootClose();
}

/* (0x810E4BE2) */
void coldbootPluginExit(Plugin *plugin)
{
	coldbootDestroy();
	coldbootInterfaceUnregister();
}
