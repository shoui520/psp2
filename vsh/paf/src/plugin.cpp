/*
 * paf::Plugin: the widget factories by type name, loading and unloading
 * through the Framework, and a plugin's resource, pages and arguments.
 */
#include <stddef.h>
#include <stdint.h>
#include <plugin.h>
#include <framework.h>
#include <cxml/cxml.h>
#include <resource/resource_obj.h>
#include <versatileparam/versatileparam.h>
#include <timer/timer.h>

namespace paf {

/* the widget factories and the draw object factories, by their type's name */
static map<string, ui::FactoryFuncTable> s_factory_table;
static map<string, ui::FactoryFuncDrawObj> s_factory_drawobj_table;

/*
 * Sony's map::operator[] and erase(iterator) are inlined in this file and nowhere
 * else, so the four functions below spell out their bodies.
 */
void Plugin::RegistFactoryTable(const char *name, ui::FactoryFuncTable table)
{
	typedef map<string, ui::FactoryFuncTable> Table;

	const string &key = string(name);

	ui::FactoryFuncTable &entry = s_factory_table._Insert(Table::value_type(key, ui::FactoryFuncTable()), NULL)->value.second;

	entry = table;
}

void Plugin::RemoveFactoryTable(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end() && it.node != NULL)
		s_factory_table.erase(it->first);
}

void Plugin::RegistFactoryDrawObj(const char *name, ui::FactoryFuncDrawObj table)
{
	typedef map<string, ui::FactoryFuncDrawObj> Table;

	const string &key = string(name);

	ui::FactoryFuncDrawObj &entry = s_factory_drawobj_table._Insert(Table::value_type(key, ui::FactoryFuncDrawObj()), NULL)->value.second;

	entry = table;
}

void Plugin::RemoveFactoryDrawObj(const char *name)
{
	map<string, ui::FactoryFuncDrawObj>::iterator it =
		s_factory_drawobj_table.find(string(name));

	if (it != s_factory_drawobj_table.end() && it.node != NULL)
		s_factory_drawobj_table.erase(it->first);
}

map<string, ui::FactoryFuncTable> *Plugin::GetWFactoryTable()
{
	return &s_factory_table;
}

void Plugin::LoadSync(const InitParam &param, LoadFinishCallback finish, int32_t option)
{
	Framework::framework->LoadPluginSync(param, finish, option);
}

void Plugin::LoadAsync(const InitParam &param, LoadFinishCallback finish, int32_t option)
{
	Framework::framework->RequestLoadPlugin(param, finish, option);
}

void Plugin::UnloadAsync(Plugin *plugin, UnloadFinishCallback finish, int32_t option)
{
	Framework::framework->RequestUnloadPlugin(plugin, NULL, finish, option);
}

void Plugin::UnloadAsync(const char *name, UnloadFinishCallback finish, int32_t option)
{
	Framework::framework->RequestUnloadPlugin(NULL, name, finish, option);
}

Plugin *Plugin::Find(const char *name)
{
	if (Framework::framework == NULL)
		return NULL;
	return Framework::framework->FindPlugin(name, false);
}

}

using namespace paf;

void ScePafToplevel_3B003F16(Plugin *plugin, string *id, uint32_t idhash, bool with_common)
{
	if (!(plugin->m_option & Plugin::Option_ResourceLoadWithDebugSymbol) ||
	    plugin->m_rscobj == NULL)
		return;

	string symbol;

	if (plugin->m_rscobj->GetDebugSymbol(idhash, 0, &symbol) == 0)
		*id = symbol;
	if (with_common) {
		Plugin *common = Framework::framework->m_common_resource_plugin;

		if (common != NULL && plugin != common)
			ScePafToplevel_3B003F16(common, id, idhash, false);
	}
}

/* called whole by C1BE28B4 */
__attribute__((noinline)) VersatileParam *ScePafToplevel_417B0721(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_sound();
	return NULL;
}

void ScePafToplevel_C1BE28B4(Plugin *plugin)
{
	cxml::Element table = plugin->m_rscobj->GetTableElement(resource::TableType_File);

	if (table.doc == NULL || table.offset < 0)
		return;

	cxml::Element node;

	for (cxml::Element child = table.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = node.GetNextSibling()) {
		node = child;

		const char *name = node.GetName();
		string type(name);
		/* past the element's "param_" */
		VersatileParam *param = ScePafToplevel_417B0721(type.c_str() + 6);
		uint32_t idhash = 0;

		scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);

		IDParam id("", idhash);

		ScePafToplevel_3B003F16(plugin, &id.m_id, idhash, true);
		if (param != NULL) {
			param->m_id = id;
			param->AttachPlugin(plugin);
			param->Create(1);
			param->Read(&node);
			param->Destroy(1);
			plugin->m_sound_param_manager->AddParam(name, id, param);
		}
	}
}

namespace paf {

VersatileParam *Plugin::CallFactoryStyle(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_style();
	return NULL;
}

Plugin::InitParam2::InitParam2()
{
	draw_priority = 150;
	surface_pool_size = 0;
	texture_load_filter = 0;
	graphics_flag = 128;
}

Plugin::PageOpenParam::PageOpenParam()
{
	fade = false;
	draw_layer = 0;
	draw_mask = 0;
	overwrite_draw_priority = -1;
	option = PageOption_None;
	event_handler_table = NULL;
	fade_time_ms = 200.0f;
	transition_type = TransitionType_None;
	timer = NULL;
	env_param = NULL;
	graphics_flag = 128;
}

void Plugin::Start()
{
	if (m_rscobj != NULL && (m_option & Option_AutoOpen)) {
		cxml::Element table = m_rscobj->GetTableElement(resource::TableType_Page);

		if (table.doc != NULL && table.offset >= 0) {
			cxml::Element page = table.GetFirstChild();

			if (page.doc != NULL && page.offset >= 0) {
				PageOpenParam param;

				PageOpen(page, param);
				if (m_option & Option_AutoActivate)
					PageActivate(page);
				if (param.timer != NULL)
					delete param.timer;
			}
		}
	}
	if (m_start_func != NULL)
		m_start_func(this);
}

void Plugin::Stop()
{
	if (m_stop_func != NULL)
		m_stop_func(this);
}

Plugin::Argument Plugin::GetArg(const char *key, bool *exist) const
{
	map<string, Argument>::iterator it = m_arguments.find(string(key));

	if (it == m_arguments.end()) {
		if (exist != NULL)
			*exist = false;
		return Argument(0);
	}
	if (exist != NULL)
		*exist = true;
	return it->second;
}

void Plugin::Notify(int32_t status) const
{
	for (list<NotifyCallback>::iterator it = m_notify_callback_list.begin();
	     it != m_notify_callback_list.end(); ++it) {
		if (*it != NULL)
			(*it)(status, m_param);
	}
}

Plugin::Argument::operator int32_t() const
{
	switch (type) {
	case ArgumentType_Int:
		return i;
	case ArgumentType_Float:
		return (int32_t)f;
	case ArgumentType_String:
		return 0;
	}
	return 0;
}

Plugin::Argument::operator float() const
{
	switch (type) {
	case ArgumentType_Int:
		return (float)i;
	case ArgumentType_Float:
		return f;
	case ArgumentType_String:
		return 0.0f;
	}
	return 0.0f;
}

Plugin::Argument::operator string() const
{
	switch (type) {
	case ArgumentType_Int:
		return string("");
	case ArgumentType_Float:
		return string("");
	case ArgumentType_String:
		return str;
	}
	return string("");
}

Plugin::InitParam::InitParam()
{
	set_param_func = NULL;
	init_func = NULL;
	start_func = NULL;
	stop_func = NULL;
	exit_func = NULL;
	resource_data = NULL;
	resource_data_size = 0;
	locale = Locale_Default;
	private_heap_size = 0;
	private_heap = NULL;
	decode_heap_size = 0;
	decode_heap = NULL;
	surface_pool = NULL;
	draw_priority = -1;
	module_interface_version = 0;
	module_option = 0;
	option = Option_Default;
	notify_callback = NULL;
	unload_callback = NULL;
	param = NULL;
	env_param = NULL;
	graphics_flag = 128;
}

}
