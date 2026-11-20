/*
 * coldboot_interface.cpp: the cold boot plugin's function table, given as its GetInterface(1)
 * (0x810E4BF0-0x810E4C1A); coldboot.cpp fills it. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "../../../plugins.h"
#include "coldboot.h"

using namespace paf;

/* (0x81555F28, 0x81555F2C, 0x81555F30) */
ColdbootPluginInterface *g_coldbootInterface;
Plugin *g_coldbootPlugin;
static ColdbootPluginInterface s_interface;

/* (0x810E4BF0) */
void coldbootInterfaceRegister(Plugin *plugin)
{
	g_coldbootPlugin = plugin;
	g_coldbootInterface = &s_interface;
	plugin->SetInterface(1, &s_interface);
}

/* (0x814098DC, folded with the other empty functions) */
void coldbootInterfaceUnregister(void)
{
}
