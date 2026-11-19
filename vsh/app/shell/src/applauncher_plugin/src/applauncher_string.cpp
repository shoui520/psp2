/*
 * applauncher_string.cpp: the plugin's strings (0x810C9D9C-0x810C9E20). The file's name is ours
 * (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher.h"

/* (0x810C9D9C) */
const wchar_t *applauncherGetString(const paf::IDParam &id)
{
	return g_applauncherPlugin->GetString(id);
}
