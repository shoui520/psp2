/*
 * applauncher_interface.cpp: the plugin's function table, given as its GetInterface(1)
 * (0x810C9C7C-0x810C9D12). The file's and the names are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher.h"

/* (0x815442E0); the home fills it */
static ApplauncherInterface s_interface = { { NULL } };

/* (0x81555B40, 0x81555B44) */
ApplauncherInterface *g_applauncherInterface;
paf::Plugin *g_applauncherPlugin;

/* (0x810C9C7C) */
void applauncherInterfaceRegister(paf::Plugin *plugin)
{
	g_applauncherPlugin = plugin;
	g_applauncherInterface = &s_interface;
	plugin->SetInterface(1, &s_interface);
}

/* PLACEHOLDER: an empty function nothing calls (0x810C9CA2) */
void applauncherInterfaceReserved(void)
{
}
