/*
 * paf::View: loads a plugin and passes its init, start, stop and exit on to a
 * ViewController. Names after ScePaf-RE; they hash to none of the exports'
 * NIDs, so the exports that are not virtual or constructors keep their NID
 * link names.
 */
#ifndef PAF_COMMON_VIEW_H
#define PAF_COMMON_VIEW_H

#include <std/string>

namespace paf {

class Plugin;

namespace ui {
class Widget;
}

class ViewController {
public:
	ViewController()
	{
	}

	virtual ~ViewController()
	{
	}

	virtual void PluginOnInit(Plugin *plugin);
	virtual void PluginOnStart(Plugin *plugin);
	virtual void PluginOnStop(Plugin *plugin);
	virtual void PluginOnExit(Plugin *plugin);
};

class View {
public:
	View(const string &plugin_name, const string &resource_path, ViewController *controller);
	virtual ~View();

	ViewController *m_controller;
};

}

extern "C" {
/* View::BindWidget(Plugin *, ui::Widget *): reads the widget's parameter from the page table */
void ScePafCommon_E234DA6E(paf::View *self, paf::Plugin *plugin, paf::ui::Widget *widget);
/* the plugin's functions a View loads it with */
void ScePafCommon_1F998F92(paf::Plugin *plugin);
void ScePafCommon_0DC3E4F3(paf::Plugin *plugin);
void ScePafCommon_A7E01D66(paf::Plugin *plugin);
void ScePafCommon_7C7D3188(paf::Plugin *plugin);
}

#endif
