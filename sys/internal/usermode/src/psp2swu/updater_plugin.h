/*
 * updater_plugin callbacks and progress/error UI
 * (sys/internal/usermode/src/psp2swu/updater_plugin.h).
 */
#ifndef PSP2SWU_UPDATER_PLUGIN_H
#define PSP2SWU_UPDATER_PLUGIN_H

#include <scetypes.h>
#include <plugin.h>

extern SceInt32 g_updaterComplete;

void OnVerifyFinishedMainThread(void *userContext);
void OnUpdaterPluginSetParam(paf::Plugin::InitParam2 *param);
void OnUpdaterPluginInit(paf::Plugin *plugin);
void OnUpdaterPluginStart(paf::Plugin *plugin);
void OnUpdaterPluginStop(paf::Plugin *plugin);
void OnUpdaterPluginExit(paf::Plugin *plugin);

#endif
