/*
 * PluginApp: the application a plugin's object is (photoreview_dialog_plugin.h), and the photo
 * apps' main-thread unregistration the dialog's build leaves empty. Names ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <plugin.h>
#include "event_dispatcher.h"
#include "photoreview_dialog_plugin.h"

using namespace paf;

PluginApp::PluginApp(Plugin *plugin) : m_plugin(plugin)
{
}

PluginApp::~PluginApp()
{
}

void PluginApp::Notify(int32_t status)
{
	if (m_plugin != NULL)
		m_plugin->Notify(status);
}

/* the dialog never registers the timers, so there is nothing to unregister */
int EventDispatcherUnregister(void (*callback)(void *userdata), void *userdata)
{
	return -1;
}
