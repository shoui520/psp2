/*
 * paf::Plugin: a plugin of the application, with its resource, the widgets and
 * pages made from it, its heaps and its callbacks: 0x13c bytes. Names after
 * ScePaf-RE.
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
#include <widget/factory.h>
#include <timer/timer.h>
#include <common/shared_ptr.h>
#include <src/paf_types.h>

namespace paf {

class File;
class Module;
class Timer;
class VersatileParam;
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
}

class WidgetResourceInfo;

/* a plugin's init event handler, by the ID of the widget it is for */
class InitEventFunctionEntry {
public:
	IDParam id;
	ui::HandlerCB func;
};

/* a page's own init event handler, with its data; the table ends with a zero ID */
class InitEventFunctionEntry2 {
public:
	IDParam id;
	ui::HandlerCB func;
	void *userdata;
};

/* a plugin's event handler, by the name an element's event attribute gives */
class EventFunctionEntry {
public:
	const char *name;
	ui::HandlerCB func;
	/* nothing in libpaf reads it */
	void *reserved;
};

/* a plugin's layout handler, by the name an element's layout attribute gives */
class LayoutFunctionEntry {
public:
	const char *name;
	ui::LayoutCB func;
};

class Plugin;

class TemplateCache;

class Plugin {
public:
	/* InitParam::option */
	enum Option {
		Option_None = 0,
		Option_AutoOpen = 1,
		Option_AutoActivate = 2,
		Option_RememberSurface = 4,
		Option_MakeSurfaceFirst = 8,
		Option_WaitWidgetActuallyDestroyed = 16,
		Option_ResourceLoadWithDebugSymbol = 32,
		Option_Default = 28
	};

	enum TransitionType {
		TransitionType_None = 0,
		TransitionType_SlideFromTop = 1,
		TransitionType_SlideFromBottom = 2,
		TransitionType_SlideFromLeft = 3,
		TransitionType_SlideFromRight = 4
	};

	/* PageOpenParam::option */
	enum PageOption {
		PageOption_None = 0,
		PageOption_Create = 1
	};

	/* the debug views of a page (ScePafToplevel_68607CDE's modes; names after ScePaf-RE) */
	/* names after ScePaf-RE */
	enum ResolutionDebugMode {
		ResolutionDebugMode_Off = 0,
		ResolutionDebugMode_Add2 = 1
	};

	enum PageDebugMode {
		PageDebugMode_PageVisual = 0,
		PageDebugMode_PageVisualEdit = 1,
		PageDebugMode_PageHide = 2
	};

	/* the option bits of loads and unloads */
	enum LoadUnloadOption {
		LoadUnloadOption_None = 0,
		LoadUnloadOption_DontLockInputDevice = 1,
		LoadUnloadOption_Default = 0
	};

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
	typedef void (*UnloadFinishCallback)(const char *name);

	class Argument {
	public:
		Argument(int32_t value) : type(ArgumentType_Int), i(value)
		{
		}

		/* copies the value both as an int and as a float */
		Argument(const Argument &src)
		{
			type = src.type;
			i = src.i;
			f = src.f;
			str = src.str;
		}

		operator int32_t() const;
		operator float() const;
		operator string() const;

		ArgumentType type;
		union {
			int32_t i;
			float f;
		};
		string str;
	};

	/* what a plugin's module gives through Module::GetInterface (name ours) */
	class ModuleInterface {
	public:
		SetParamFunction set_param_func;
		InitFunction init_func;
		StartFunction start_func;
		StopFunction stop_func;
		ExitFunction exit_func;
	};

	/* a texture AddResourceTexture added from a file (field names ours) */
	class AddTextureInfo {
	public:
		AddTextureInfo() : id(s_empty_name, 0)
		{
		}

		IDParam id;
		string path;
		/* the image type the file's extension gave */
		int32_t type;
	};

	class DrawOrderElement {
	public:
		ui::Scene *page;
		int32_t view_priority;
		int32_t page_priority;
	};

	/* what a plugin's set_param_func may change before the plugin is made */
	class InitParam2 {
	public:
		InitParam2();

		int32_t draw_priority;
		uint32_t surface_pool_size;
		int32_t texture_load_filter;
		int32_t graphics_flag;
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

	class PageOpenParam {
	public:
		PageOpenParam();

		int32_t draw_layer;
		int32_t draw_mask;
		int32_t overwrite_draw_priority;
		int32_t option;
		InitEventFunctionEntry2 *event_handler_table;
		bool fade;
		float fade_time_ms;
		TransitionType transition_type;
		Timer *timer;
		ui::EnvironmentParam *env_param;
		int32_t graphics_flag;
	};

	/* how PageClose closes the page */
	class PageCloseParam {
	public:
		PageCloseParam();

		~PageCloseParam()
		{
			if (timer != NULL)
				delete timer;
		}

		bool fade;
		float fade_time_ms;
		TransitionType transition_type;
		Timer *timer;
	};

	/* what TemplateOpen adds the template's widgets with */
	class TemplateOpenParam {
	public:
		TemplateOpenParam();

		InitEventFunctionEntry2 *event_handler_table;
		ui::Widget *sibling;
	};

	Plugin(const InitParam *param);
	~Plugin();

	static void Init();
	static void End();
	/* the debug mode's focus callback and the debug GUI's draw of its params */
	static int32_t PageDebugFocusCB(ui::Widget *focus, ui::Widget *prev, void *userdata);
	static void DrawVisualParams(void *userdata);
	static void OnAnimPageHideFin(Timer *t, ui::AnimListener *anim, void *data1, void *data2,
				      void *data3);
	static void WidgetDebugDeleteCb(ui::Widget *widget, void *userdata);
	static void DebugAddResolution(ui::Widget *widget, ResolutionDebugMode mode);
	static void RegistFactoryTable(const char *name, ui::FactoryFuncTable table);
	static void RemoveFactoryTable(const char *name);
	static void RegistFactoryDrawObj(const char *name, ui::FactoryFuncDrawObj table);
	static void RemoveFactoryDrawObj(const char *name);
	static map<string, ui::FactoryFuncTable> *GetWFactoryTable();
	static void LoadSync(const InitParam &param, LoadFinishCallback finish, int32_t option);
	static void LoadAsync(const InitParam &param, LoadFinishCallback finish, int32_t option);
	static void UnloadAsync(Plugin *plugin, UnloadFinishCallback finish, int32_t option);
	static void UnloadAsync(const char *name, UnloadFinishCallback finish, int32_t option);
	static Plugin *Find(const char *name);

	static VersatileParam *CallFactoryStyle(const char *name);
	static VersatileParam *CallFactoryFocus(const char *name);
	static VersatileParam *CallFactoryLayout(const char *name);
	static ui::WidgetParam *CallFactoryWidgetParam(const char *name);
	static ui::Widget *CallFactoryWidget(const char *name, ui::Widget *parent);
	/* a widget of the type with the style's parameter (its type's default style without one) */
	ui::Widget *CreateWidgetWithStyleType(ui::Widget *parent, const char *type, const IDParam &id,
					      const char *style_type, const IDParam &style_id);
	ui::Scene *PageOpen(const cxml::Element &node, const PageOpenParam &param);
	ui::Scene *PageOpen(const IDParam &id, const PageOpenParam &param);
	void PageActivate(const cxml::Element &node);
	void PageActivate(const IDParam &id);
	void PageActivateActually();
	void PageInactivateActually();
	void PageInactivate(const cxml::Element &node);
	void PageInactivate(const IDParam &id);
	ui::Scene *PageRoot(const IDParam &id) const;
	ui::Scene *PageRoot(const cxml::Element &node) const;

	void Start();
	void Stop();
	void Exit();
	void PageCloseAll();
	int32_t TemplateOpen(ui::Widget *parent, const IDParam &id, const TemplateOpenParam &param);
	void InstantiateWidget(const cxml::Element &elm, ui::Widget *parent, bool is_template,
			       InitEventFunctionEntry2 *event_handler_table, ui::Widget *sibling);
	ui::Widget *CreateWidget(ui::Widget *parent, const char *type, const IDParam &id,
				 const IDParam &style);

	const char *GetName() const
	{
		return m_name.c_str();
	}

	Argument GetArg(const char *key, bool *exist) const;
	void Notify(int32_t status) const;

	const wchar_t *GetString(const IDParam &id) const;
	intrusive_ptr<graph::Surface> GetTexture(const IDParam &id);
	void SetLocale(Locale locale);
	static void SetLocaleAll(Locale locale);
	void SetInitEventHandler(const InitEventFunctionEntry *table);
	void SetEventHandler(const EventFunctionEntry *table);
	void SetLayoutHandler(const LayoutFunctionEntry *table);
	void SetInterface(int32_t id, void *iface);
	void *GetInterface(int32_t id) const;
	void AppendNotifyCallback(NotifyCallback cb);
	void RemoveNotifyCallback(NotifyCallback cb);
	void AppendUnloadCallback(UnloadCallback cb);
	void RemoveUnloadCallback(UnloadCallback cb);
	/* a style parameter of the type, added to the plugin's styles as the ID */
	int32_t CreateStyle(const char *type, const IDParam &id);
	/* sets a widget's layout from an element, one of its layouts by index, or by ID */
	int32_t MorphWidgetLayout(ui::Widget *widget, const cxml::Element &elm, Timer *timer);
	int32_t MorphWidgetLayout(ui::Widget *widget, uint32_t index, Timer *timer);
	int32_t MorphWidgetLayout(ui::Widget *widget, const IDParam &id, Timer *timer);
	/* turns a debug view of an opened page on or off */
	int32_t SetPageDebugMode(const IDParam &id, PageDebugMode mode, bool enable);
	AddTextureInfo *GetNodeEntity_AddTexture(const IDParam &id) const;
	/* the debug writers: the plugin's tables as resource XML, within the table tags with table */
	int32_t WriteTextureTable(common::SharedPtr<File> fp, bool table);
	int32_t WriteStyleTable(common::SharedPtr<File> fp, bool table);
	/* the widget, its layout and its children, at the indent; with print, also those made by templates */
	int32_t WriteWidget(ui::Widget *widget, common::SharedPtr<File> fp, uint32_t indent, bool print);
	/* its opened pages and their widgets; to the TTY without a file */
	int32_t WritePageTable(common::SharedPtr<File> fp, bool table, bool print);
	int32_t WritePage(common::SharedPtr<File> fp, const IDParam &id);
	/* a whole resource document: its pages, styles and textures, or one of the tables */
	int32_t WriteResourceAll(common::SharedPtr<File> fp);
	int32_t WriteResourcePageTable(common::SharedPtr<File> fp, bool table);
	int32_t WriteResourceStyleTable(common::SharedPtr<File> fp, bool table);
	int32_t WriteResourceTextureTable(common::SharedPtr<File> fp, bool table);
	/* the debug IDs of the resource's pages, or of the opened ones, then the created pages */
	int32_t GetResourcePages(vector<string> *pages);
	int32_t GetOpenedPages(vector<string> *pages);
	/* the debug IDs of the styles (of a widget type's, with type) and of the textures */
	int32_t GetResourceStyles(const char *type, vector<string> *styles);
	int32_t GetResourceTextures(vector<string> *textures);
	/* whether the ID is the resource's, an opened page's or its widget's, a style's or an added texture's */
	bool IsRegisteredID(const IDParam &id);
	void SetNodeEntity_AddTexture(const IDParam &id, AddTextureInfo &info);
	/* loads the file's texture under the ID; src is its path in written resources */
	int32_t AddResourceTexture(const IDParam &id, const char *filename, const char *src);
	static VersatileParam *CallFactoryHandler(const char *name);
	static graph::DrawObj *CallFactoryDrawObj(const char *name);
	static VersatileParam *CallFactoryDrawObjParam(const char *name);
	/* the event handler of the plugin's table with the name; NULL without one */
	ui::HandlerCB GetEventHandler(const char *name) const;
	/* the element's attribute names an event handler: its listener for the event type */
	void InstantiateWidget_EventHandler(const cxml::Element &node, ui::Widget *widget,
					    const char *attr, int32_t type);
	ui::LayoutCB GetLayoutHandler(const char *name) const;
	/* the element's attribute names a layout handler: the widget's Layouter calls it */
	void InstantiateWidget_LayoutHandler(const cxml::Element &node, ui::Widget *widget,
					     const char *attr);
	/* the texture the element's attribute refers to, else the common one, else def */
	void InstantiateWidget_Texture(const cxml::Element &node, ui::Widget *widget, int32_t obj,
				       const char *attr, const intrusive_ptr<graph::Surface> &def);
	/* the handler parameter of the element's type, read from it and set on the widget */
	void InstantiateWidget_HandlerAll(const cxml::Element &node, ui::Widget *widget);
	/* the string the element's attribute refers to, else the common one */
	void InstantiateWidget_String(const cxml::Element &node, const char *attr,
				      ui::Widget *widget);

	/* The resource's texture id: the plugin's surface, its texture table, the common ones. */
	intrusive_ptr<graph::Surface> GetTexture(const IDParam &id, graph::SurfacePool *pool);
	intrusive_ptr<graph::Surface> GetTexture(const cxml::Element &node);

	void LoadResourceTable_Style();
	/* the templates with fast_open, made ahead, and their release (names ours) */
	void LoadResourceTable_Template();
	void UnloadResourceTable_Template();
	void LoadResourceTable_Texture();
	void UnloadResourceTable_Texture();
	void SetResourceTable_Texture();
	void SetNodeEntity_Surface(const IDParam &id, graph::Surface *surface);
	void SetNodeEntity_Surface(const cxml::Element &node, graph::Surface *surface);
	graph::Surface *GetNodeEntity_Surface(const IDParam &id) const;
	graph::Surface *GetNodeEntity_Surface(const cxml::Element &node) const;
	void SetNodeEntity_Widget(const IDParam &id, ui::Widget *widget);
	void SetNodeEntity_Widget(const cxml::Element &node, ui::Widget *widget);
	void ClearNodeEntity_ForEachWidget(const cxml::Element &node);
	void SetDrawOrder_Remove(ui::Scene *page);
	void SetDrawOrder_Insert(ui::Scene *page, int32_t view_priority, int32_t page_priority);
	/* inserts the page by its draw priority and its place in the resource's page table */
	void AdjustPageDrawOrder(ui::Scene *page, const IDParam &id, int32_t view_priority);
	ui::Widget *GetNodeEntity_Widget(const IDParam &id) const;
	ui::Widget *GetNodeEntity_Widget(const cxml::Element &node) const;
	void PageClose(const cxml::Element &node, const PageCloseParam &param);
	void SetNodeEntity_CreatePage(const IDParam &id, ui::Widget *page);
	ui::Widget *GetNodeEntity_CreatePage(const IDParam &id) const;
	void PageClose(const IDParam &id, const PageCloseParam &param);
	ui::HandlerCB GetInitEventHandler(const IDParam &id) const;
	void InstantiateWidget_InitEventHandler(const IDParam &id, ui::Widget *widget,
						InitEventFunctionEntry2 *event_handler_table);
	/* reads the element's "layout_" children into the widget's layouts and its type's layout */
	void InstantiateWidget_Layout(const cxml::Element &elm, ui::Widget *widget,
				      WidgetResourceInfo *info);
	/* reads the element's first "focus_" child into its type's FocusParam for the widget */
	void InstantiateWidget_Focus(const cxml::Element &elm, ui::Widget *widget);

	/*
	 * A texture element's surface: the one made before, else loaded from the resource's
	 * buffer or file (zlib when compress="on"), cut by its div_ and margin_ attributes
	 * (names ours).
	 */
	intrusive_ptr<graph::Surface> GetTexture(const cxml::Element &node, graph::SurfacePool *pool);
	intrusive_ptr<graph::Surface> LoadTexture(graph::SurfacePool *pool, void *buf, uint32_t size,
						  uint32_t type, bool compressed, uint32_t origsize);
	void SetTextureDivide(const cxml::Element &node, graph::Surface *surface);

	bool m_initialized;
	string m_name;
	string m_caller_name;
	SetParamFunction m_set_param_func;
	InitFunction m_init_func;
	StartFunction m_start_func;
	StopFunction m_stop_func;
	ExitFunction m_exit_func;
	Module *m_module;
	const InitEventFunctionEntry *m_init_event_handler_table;
	const EventFunctionEntry *m_event_handler_table;
	map<uint32_t, int32_t> m_event_handler_name_table;
	const LayoutFunctionEntry *m_layout_handler_table;
	map<string, int32_t> m_layout_handler_name_table;
	map<int32_t, void *> m_interface_table;
	map<uint32_t, ui::Widget *> m_widget_table;
	map<string, ui::Widget *> m_create_page_table;
	map<uint32_t, graph::Surface *> m_surface_table;
	map<uint32_t, AddTextureInfo> m_add_texture_table;
	map<uint32_t, TemplateCache *> m_template_table;
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
	const cxml::Element *m_last_active_page_node;
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
	/* a ResolutionDebugMode: the debug focus adds resolutions to widgets without them */
	uint32_t m_resolution_debug_mode;
	int32_t m_page_debug_mode;
	ui::EnvironmentParam *m_env_param;
	bool m_env_param_created;
	/* the text style (SystemParam's, the InitParam's or the InitParam2's) */
	char m_graphics_flag;
	bool m_unloading;
};

/*
 * A template's widgets made ahead for TemplateOpen (fast_open="1"): the plugin
 * keeps one per template ID (class and method names ours; 0x81192a24, 0x81192312).
 */
class TemplateCache {
public:
	TemplateCache() : m_created(false)
	{
	}

	~TemplateCache()
	{
		Clear();
	}

	/* makes the template's widgets; 0 when done */
	int32_t Create(Plugin *plugin, const IDParam &id);
	void Clear();
	/* opens the template from the widgets made ahead (0x8119311a, name ours) */
	int32_t Open(Plugin *plugin, ui::Widget *parent, const Plugin::TemplateOpenParam &param);

	bool m_created;
	list<void *> m_items;
};

/*
 * What a widget made from a resource keeps as its first user data: its
 * plugin, its parameter and its resource node, and the layouts read for it.
 */
class WidgetResourceInfo : public ui::Widget::UserData {
public:
	WidgetResourceInfo(Plugin *plugin)
		: m_plugin(plugin), m_wparam(NULL), m_style_id(s_empty_name, 0),
		  m_sound_id(s_empty_name, 0)
	{
		m_node = cxml::Element();
	}

	virtual ~WidgetResourceInfo()
	{
		if (m_wparam != NULL)
			delete m_wparam;
	}

	class LayoutData {
	public:
		IDParam m_id;
		cxml::Element m_node;
		ui::WidgetLayoutParam *m_param;
	};

	Plugin *m_plugin;
	ui::WidgetParam *m_wparam;
	cxml::Element m_node;
	IDParam m_style_id;
	/* the ID of its sound parameter, its style's or its parameter's (name ours) */
	IDParam m_sound_id;
	vector<LayoutData> m_list_layout;
};

}

/* Exports with no name found for their NIDs. */
extern "C" {
/* starts an opened page's fade-in and slide (the open counterpart of 340EE53C) */
void ScePafToplevel_995AE4B6(paf::Plugin *plugin, paf::ui::Widget *page,
			     const paf::Plugin::PageOpenParam &param);
/* gives a new widget the sounds of its type's sound parameter ("sound_<type>"), the plugin's
   or the common resource's */
void ScePafToplevel_2E1F0393(paf::Plugin *plugin, const paf::cxml::Element &elm,
			     paf::ui::Widget *widget, const paf::IDParam &id);
/* puts the page into a debug mode (0 param view, 1 layout, 2 hide all) or out of it */
int32_t ScePafToplevel_68607CDE(paf::Plugin *plugin, paf::ui::Scene *page, int32_t mode,
				bool enable);
/* the ID's name from the resource's debug symbols, and from the common resource's */
void ScePafToplevel_3B003F16(paf::Plugin *plugin, paf::string *id, uint32_t idhash,
			     bool with_common);
/* the sound parameter factory of a widget type */
paf::VersatileParam *ScePafToplevel_417B0721(const char *name);
/* reads the sound index table's parameters into the sound parameter manager */
void ScePafToplevel_C1BE28B4(paf::Plugin *plugin);
/* the plugin's sound table, from its resource's file and as loaded (Sony's names not found) */
void ScePafToplevel_952CD3CF(paf::Plugin *plugin);
void ScePafToplevel_1230A05F(paf::Plugin *plugin);
/* lets the resource's sound table go (Sony's name not found) */
void ScePafToplevel_E87AD3F3(paf::Plugin *plugin);
/* closes a page: fades it out and slides it away as the parameter says, else destroys it */
void ScePafToplevel_340EE53C(paf::Plugin *plugin, paf::ui::Widget *page,
			     const paf::Plugin::PageCloseParam &param);
/*
 * Sets the debug view of every opened page of the resource (m_page_debug_mode: 0 none,
 * else the PageDebugMode + 1); -1 without a page table.
 */
int32_t ScePafToplevel_DD14F248(paf::Plugin *plugin, int32_t mode);
/* a sound of the plugin's resource, else of the common resource's */
int32_t ScePafToplevel_CA3E606F(paf::Plugin *plugin, const paf::IDParam &id,
				const char **buffer1, uint32_t *size1, const char **buffer2,
				uint32_t *size2, int32_t *samplerate, uint32_t *type);
/* the sounds of a sound parameter of the plugin's, else of the common resource's; -1 without one */
int32_t ScePafToplevel_DA3D4AF0(paf::Plugin *plugin, const paf::IDParam &id,
				paf::ui::WidgetSoundParam::Sound **sounds, uint32_t *num);
/*
 * The ID the plugin's resource aliases the ID to (for this model), else the common
 * resource's; the ID itself and 0x80AF0601 without one.
 */
int32_t ScePafToplevel_1AB2DB65(paf::Plugin *plugin, const paf::IDParam &id, paf::IDParam *alias);
/* the string of the ID's alias, as Plugin::GetString */
const wchar_t *ScePafToplevel_D8801CA4(paf::Plugin *plugin, const paf::IDParam &id);
void ScePafToplevel_4D79D4E2(paf::Plugin *plugin);
paf::intrusive_ptr<paf::graph::Surface> ScePafToplevel_F13D2937(paf::Plugin *plugin, void *buf,
								uint32_t size, uint32_t type,
								bool compressed, uint32_t origsize);
}

#endif
