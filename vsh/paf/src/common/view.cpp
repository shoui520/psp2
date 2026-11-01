/*
 * paf::View and paf::ViewController.
 */
#include <stddef.h>
#include <std/string>
#include <plugin.h>
#include <resource/resource_obj.h>
#include <widget/core/widget.h>
#include <widget/wutil.h>
#include <common/view.h>

using namespace paf;

View::View(const string &plugin_name, const string &resource_path, ViewController *controller)
	: m_controller(controller)
{
	Plugin::InitParam param;

	param.name = plugin_name;
	param.resource_file = resource_path;
	param.caller_name = "__main__";
	param.init_func = ScePafCommon_1F998F92;
	param.start_func = ScePafCommon_0DC3E4F3;
	param.param = controller;
	param.stop_func = ScePafCommon_A7E01D66;
	param.exit_func = ScePafCommon_7C7D3188;
	Plugin::LoadAsync(param, NULL, 0);
}

View::~View()
{
}

void ScePafCommon_E234DA6E(View *self, Plugin *plugin, ui::Widget *widget)
{
	WidgetResourceInfo *info = static_cast<WidgetResourceInfo *>(widget->m_user_data2[0]);
	cxml::Element page = plugin->m_rscobj->GetTableElement(resource::TableType_Page);

	if (info != NULL) {
		info->m_node = page;

		ui::WidgetParam *wparam = info->m_wparam;

		if (wparam != NULL) {
			wparam->Create(1);
			wparam->Read(&page);
			wparam->Set();
			wparam->Destroy(1);
		}
	}
}

void ViewController::PluginOnInit(Plugin *plugin)
{
}

void ViewController::PluginOnStart(Plugin *plugin)
{
}

void ViewController::PluginOnStop(Plugin *plugin)
{
}

void ViewController::PluginOnExit(Plugin *plugin)
{
}

void ScePafCommon_1F998F92(Plugin *plugin)
{
	if (plugin != NULL) {
		ViewController *controller = static_cast<ViewController *>(plugin->m_param);

		if (controller != NULL)
			controller->PluginOnInit(plugin);
	}
}

void ScePafCommon_0DC3E4F3(Plugin *plugin)
{
	ui::MessageDialogListener *listener = ui::wutil::GetMessageDialogListener();

	if (listener != NULL)
		listener->PluginOnStart(plugin);
	if (plugin != NULL) {
		ViewController *controller = static_cast<ViewController *>(plugin->m_param);

		if (controller != NULL)
			controller->PluginOnStart(plugin);
	}
}

void ScePafCommon_A7E01D66(Plugin *plugin)
{
	if (plugin != NULL) {
		ViewController *controller = static_cast<ViewController *>(plugin->m_param);

		if (controller != NULL)
			controller->PluginOnStop(plugin);
	}
}

void ScePafCommon_7C7D3188(Plugin *plugin)
{
	if (plugin != NULL) {
		ViewController *controller = static_cast<ViewController *>(plugin->m_param);

		if (controller != NULL)
			controller->PluginOnExit(plugin);
	}
}
