/*
 * applauncher_getinterface.cpp: another plugin's function table (0x810C9D14-0x810C9D9A). The
 * file's name is ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher.h"

/* (0x810C9D14) */
void *applauncherPluginGetInterface(const char *name, int id)
{
	paf::Plugin *plugin = paf::Plugin::Find(name);

	if (plugin == NULL)
		return NULL;
	return plugin->GetInterface(id);
}
