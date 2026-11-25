/*
 * topmenu_interface.cpp: topmenu_plugin instance and GetInterface(1) table
 * registration (0x81284398-0x8128442F).
 */
#include "topmenu_internal.h"

static TopmenuPluginInterface s_topmenuInterface;

TopmenuPluginInterface *g_topmenuInterface;
paf::Plugin *g_topmenuPlugin;

/* 0x81284398 */
void topmenuInterfaceInit(paf::Plugin *plugin)
{
	g_topmenuPlugin = plugin;
	g_topmenuInterface = &s_topmenuInterface;
	plugin->SetInterface(1, &s_topmenuInterface);
}

/* 0x812843BE */
void topmenuInterfaceTerm(void)
{
}
