/*
 * paf::Plugin: a plugin of the application, with its resource, the widgets and
 * pages made from it, its heaps and its callbacks. The functions are still C
 * (src/plugin.c, src/plugin.h's struct); this is the layout of 0x13c bytes and
 * what converted C++ calls. Names after ScePaf-RE.
 */
#ifndef PAF_PLUGIN_H_CXX
#define PAF_PLUGIN_H_CXX

#include <stdint.h>
#include <std/string>
#include <std/memory>
#include <std/map>
#include <std/list>
#include <std/vector>
#include <cxml/cxml.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>

namespace paf {

class Module;
class VersatileParamManager;

namespace memory {
class HeapAllocator;
}

namespace graph {
class Surface;
class SurfacePool;
}

namespace resource {
class ResourceObj;
}

namespace ui {
class Scene;
class EnvironmentParam;
class WidgetLayoutParam;
}

class InitEventFunctionEntry;
class EventFunctionEntry;
class LayoutFunctionEntry;

class Plugin {
public:
	enum ArgumentType {
		ArgumentType_Int = 0,
		ArgumentType_Float = 1,
		ArgumentType_String = 2
	};

	class InitParam2;

	typedef void (*SetParamFunction)(InitParam2 *param);
	typedef void (*InitFunction)(Plugin *plugin);
	typedef void (*StartFunction)(Plugin *plugin);
	typedef void (*StopFunction)(Plugin *plugin);
	typedef void (*ExitFunction)(Plugin *plugin);
	typedef void (*PageOpenCallback)(Plugin *plugin, cxml::Element *node);
	typedef void (*PageActivateCallback)(Plugin *plugin, cxml::Element *node);
	typedef void (*PageCloseCallback)(Plugin *plugin, cxml::Element *node);
	typedef void (*NotifyCallback)(int32_t event, void *userdata);
	typedef void (*UnloadCallback)(void *userdata);
	typedef void (*LoadFinishCallback)(Plugin *plugin);

	class Argument {
	public:
		ArgumentType type;
		union {
			int32_t i;
			float f;
		};
		string str;
	};

	class AddTextureInfo {
	public:
		char reserved[32];
	};

	class DrawOrderElement {
	public:
		ui::Scene *page;
		int32_t view_priority;
		int32_t page_priority;
	};

	class InitParam {
	public:
		InitParam();

		string name;
		string caller_name;
		SetParamFunction set_param_func;
		InitFunction init_func;
		StartFunction start_func;
		StopFunction stop_func;
		ExitFunction exit_func;
		string resource_file;
		uint8_t *resource_data;
		uint32_t resource_data_size;
		/* the Locale */
		int32_t locale;
		uint32_t private_heap_size;
		memory::HeapAllocator *private_heap;
		uint32_t decode_heap_size;
		memory::HeapAllocator *decode_heap;
		graph::SurfacePool *surface_pool;
		int32_t draw_priority;
		map<string, Argument> arguments;
		string module_file;
		int32_t module_interface_version;
		int32_t module_option;
		int32_t option;
		NotifyCallback notify_callback;
		UnloadCallback unload_callback;
		void *param;
		ui::EnvironmentParam *env_param;
		int32_t graphics_flag;
	};

	static void Init();
	static void End();
	static Plugin *Find(const char *name);
	static void LoadAsync(const InitParam &param, LoadFinishCallback finish, int32_t option);

	void Stop();
	void PageCloseAll();

	const char *GetName() const
	{
		return m_name.c_str();
	}

	/* The resource's texture id: the plugin's surface, its texture table, the common ones. */
	intrusive_ptr<graph::Surface> GetTexture(const IDParam &id, graph::SurfacePool *pool);

	bool m_initialized;
	string m_name;
	string m_caller_name;
	SetParamFunction m_set_param_func;
	InitFunction m_init_func;
	StartFunction m_start_func;
	StopFunction m_stop_func;
	ExitFunction m_exit_func;
	Module *m_module;
	InitEventFunctionEntry *m_init_event_handler_table;
	EventFunctionEntry *m_event_handler_table;
	map<uint32_t, int32_t> m_event_handler_name_table;
	LayoutFunctionEntry *m_layout_handler_table;
	map<string, int32_t> m_layout_handler_name_table;
	map<int32_t, void *> m_interface_table;
	map<uint32_t, ui::Widget *> m_widget_table;
	map<string, ui::Widget *> m_create_page_table;
	map<uint32_t, graph::Surface *> m_surface_table;
	map<uint32_t, AddTextureInfo> m_add_texture_table;
	map<uint32_t, void *> m_template_table;
	vector<DrawOrderElement> m_draw_order;
	resource::ResourceObj *m_rscobj;
	VersatileParamManager *m_style_param_manager;
	VersatileParamManager *m_obj_param_manager;
	VersatileParamManager *m_sound_param_manager;
	memory::HeapAllocator *m_private_heap;
	bool m_private_heap_created;
	memory::HeapAllocator *m_decode_heap;
	bool m_decode_heap_created;
	void *m_surface_pool_addr;
	graph::SurfacePool *m_surface_pool;
	map<string, Argument> m_arguments;
	int32_t m_option;
	list<NotifyCallback> m_notify_callback_list;
	list<UnloadCallback> m_unload_callback_list;
	void *m_param;
	cxml::Element *m_last_active_page_node;
	int32_t m_draw_priority;
	/* the Locale */
	int32_t m_locale;
	bool m_texture_table_loaded;
	int32_t m_tex_load_filter;
	PageOpenCallback m_monitoring_callback_pageopen;
	PageActivateCallback m_monitoring_callback_pageactivate;
	PageCloseCallback m_monitoring_callback_pageclose;
	uint8_t *m_resource_data;
	uint32_t m_resource_data_size;
	uint32_t m_page_debug_focus_enabled;
	int32_t m_page_debug_mode;
	ui::EnvironmentParam *m_env_param;
	bool m_env_param_created;
	bool m_graphics_flag;
	bool m_unloading;
};

/*
 * What a widget made from a resource keeps as its first user data: its
 * plugin, its parameter and its resource node, and the layouts read for it.
 */
class WidgetResourceInfo : public ui::Widget::UserData {
public:
	class LayoutData {
	public:
		IDParam m_id;
		cxml::Element m_node;
		ui::WidgetLayoutParam *m_param;
	};

	Plugin *m_plugin;
	ui::WidgetParam *m_wparam;
	cxml::Element m_node;
	vector<LayoutData> m_list_layout;
	IDParam m_style_id;
};

}

#endif
