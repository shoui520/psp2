/*
 * livearea_interface.cpp: livearea_plugin interface registration (0x81199D10-0x81199DA8,
 * sti 0x81199D38). The file and symbol names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "livearea_internal.h"

LiveAreaPluginInterface *g_liveareaInterface;		/* 0x81566F90 */
paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
static LiveAreaPluginInterface s_liveareaInterfaceTable; /* 0x81566F98 */

/* 0x81199D10 */
void liveareaRegisterInterface(paf::Plugin *plugin)
{
	g_liveareaPlugin = plugin;
	g_liveareaInterface = &s_liveareaInterfaceTable;
	plugin->SetInterface(1, &s_liveareaInterfaceTable);
}

/* 0x81199D36 */
void liveareaInterfaceUnused(void)
{
}
