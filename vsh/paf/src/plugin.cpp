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
#include <versatileparam/resourceparam.h>
#include <timer/timer.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/surface/surface_display.h>
#include <widget/w_camera.h>
#include <file/localfile.h>
#include <compress/compress.h>
#include <memory/heap_allocator.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <kernel/iofilemgr.h>
#include <module/module.h>
#include <thread/thread.h>
#include <system/system.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/gutil.h>
#include <widget/core/environment.h>
#include <widget/w_scene.h>
#include <common/transition.h>

extern "C" int sceAppMgrConvertVs0UserDrivePath(const char *path, char *out, int out_size);

namespace paf {

const char s_empty_name[] = "";
/*
 * Two 32s nothing reads: Sony's files that draw (plugin, the graphics, camera and screen
 * files) each define this object, so it comes from a header of theirs we have not
 * identified; its name and header are ours until then.
 */
static struct DrawDefaultPair {
	int32_t first;
	int32_t second;
	__attribute__((always_inline)) DrawDefaultPair() : first(32), second(32) {}
} s_draw_default_pair;
/* the widget factories and the draw object factories, by their type's name */
static map<string, ui::FactoryFuncTable> s_factory_table;
static map<string, ui::FactoryFuncDrawObj> s_factory_drawobj_table;
/* the params of the widget the page debug mode focuses: its layout, style and widget params (names ours) */
static ui::WidgetLayoutParam *s_debug_layout_param;
static VersatileParam *s_debug_style_param;
static VersatileParam *s_debug_widget_param;

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

void Plugin::LoadResourceTable_Template()
{
	cxml::Element templates = m_rscobj->GetTableElement(resource::TableType_Template);

	if (templates.doc == NULL || templates.offset < 0)
		return;

	cxml::Element node;

	for (node = templates.GetFirstChild(); node.doc != NULL && node.offset >= 0;
	     node = node.GetNextSibling()) {
		cxml::Attribute attr;

		if (scePafResourceGetAttribute(&node, "fast_open", &attr) != 0)
			continue;

		int32_t value = 0;
		uint32_t idhash;

		if (scePafResourceGetInteger(&attr, &value) != 0 || value == 0 ||
		    scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash) != 0)
			continue;
		/* the const find, which Sony's build inlines here */
		const map<uint32_t, TemplateCache *> &table = m_template_table;

		if (table.find(idhash) != table.end())
			continue;

		TemplateCache *cache = new TemplateCache();

		if (cache == NULL)
			continue;
		if (cache->Create(this, IDParam(s_empty_id, idhash)) == 0) {
			typedef map<uint32_t, TemplateCache *> Table;

			m_template_table._Insert(Table::value_type(idhash, NULL), NULL)->value.second =
				cache;
		} else {
			delete cache;
		}
	}
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
	cxml::Element table = plugin->m_rscobj->GetTableElement(resource::TableType_SoundIndex);

	if (table.doc == NULL || table.offset < 0)
		return;

	cxml::Element child;
	cxml::Element node;

	for (child = table.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = node.GetNextSibling()) {
		node = child;

		const char *name = node.GetName();
		string type(name);
		/* past the element's "param_" */
		VersatileParam *param = ScePafToplevel_417B0721(type.c_str() + 6);
		uint32_t idhash = 0;

		scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);

		IDParam id(s_empty_id, idhash);

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

/* called whole by LoadResourceTable_Style */
__attribute__((noinline)) VersatileParam *Plugin::CallFactoryStyle(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_style();
	return NULL;
}

void Plugin::LoadResourceTable_Style()
{
	cxml::Element table = m_rscobj->GetTableElement(resource::TableType_Style);

	if (table.doc == NULL || table.offset < 0)
		return;

	cxml::Element child;
	cxml::Element node;

	for (child = table.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = node.GetNextSibling()) {
		node = child;

		const char *name = node.GetName();
		string type(name);
		/* past the element's "style_" */
		VersatileParam *param = CallFactoryStyle(type.c_str() + 6);
		uint32_t idhash = 0;

		scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);
		if (param != NULL) {
			IDParam id(s_empty_id, idhash);

			param->m_id = id;
			param->AttachPlugin(this);
			param->Create(1);
			param->Read(&node);
			param->Destroy(1);
			m_style_param_manager->AddParam(name, id, param);
		}
	}
}

void Plugin::UnloadResourceTable_Texture()
{
	if (m_resource_data != NULL)
		return;
	ScePafResource_D60FF214(m_rscobj, m_decode_heap);
	m_texture_table_loaded = false;
}

void Plugin::SetTextureDivide(const cxml::Element &node, graph::Surface *surface)
{
	const char *mode;
	uint32_t len;
	uint32_t divide_mode = 0;

	if (scePafResourceGetAttributeString(&node, "div_mode", &mode, &len) == 0) {
		if (sce_paf_strncmp(mode, "3x3", 3) == 0 && len == 3)
			divide_mode = 3;
		else if (sce_paf_strncmp(mode, "3x1", 3) == 0 && len == 3)
			divide_mode = 2;
		else if (sce_paf_strncmp(mode, "1x3", 3) == 0 && len == 3)
			divide_mode = 1;
		else if (sce_paf_strncmp(mode, "5x3", 3) == 0 && len == 3)
			divide_mode = 5;
		else if (sce_paf_strncmp(mode, "3x5", 3) == 0 && len == 3)
			divide_mode = 4;
		else if (sce_paf_strncmp(mode, "3x3_border", 10) == 0 && len == 10)
			divide_mode = 6;
		else if (sce_paf_strncmp(mode, "3x3_opaque", 10) == 0 && len == 10)
			divide_mode = 7;
		else if (sce_paf_strncmp(mode, "1x3_opaque", 10) == 0 && len == 10)
			divide_mode = 8;
		else if (sce_paf_strncmp(mode, "3x1_opaque", 10) == 0 && len == 10)
			divide_mode = 9;
	}
	surface->SetDivideMode(divide_mode);

	const float *ratio;
	uint32_t num;
	math::v2 ratio_h(1.0f / 3, 1.0f / 3);
	math::v2 ratio_v(1.0f / 3, 1.0f / 3);

	if (scePafResourceGetAttributeFloatArray(&node, "div_ratio_h", &ratio, &num) == 0) {
		if (num == 1)
			ratio_h = math::v2(ratio[0], ratio[0]);
		else if (num >= 2)
			ratio_h = math::v2(ratio[0], ratio[1]);
	}
	if (scePafResourceGetAttributeFloatArray(&node, "div_ratio_v", &ratio, &num) == 0) {
		if (num == 1)
			ratio_v = math::v2(ratio[0], ratio[0]);
		else if (num >= 2)
			ratio_v = math::v2(ratio[0], ratio[1]);
	}
	ScePafGraphics_36960B24(surface, ratio_h, ratio_v);

	math::v2 stretch(0.0f, 0.0f);

	if (scePafResourceGetAttributeFloatArray(&node, "div_ratio_stretch", &ratio, &num) == 0) {
		if (num == 1)
			stretch = math::v2(ratio[0], ratio[0]);
		else if (num >= 2)
			stretch = math::v2(ratio[0], ratio[1]);
	}
	surface->SetDivideStretchRatio(stretch);

	const int32_t *margin;
	int16_t left = 0, right = 0, top = 0, bottom = 0;

	if (scePafResourceGetAttributeIntegerArray(&node, "margin_h", &margin, &num) == 0) {
		if (num == 1) {
			left = margin[0];
			right = margin[0];
		} else if (num >= 2) {
			left = margin[0];
			right = margin[1];
		}
	}
	if (scePafResourceGetAttributeIntegerArray(&node, "margin_v", &margin, &num) == 0) {
		if (num == 1) {
			top = margin[0];
			bottom = margin[0];
		} else if (num >= 2) {
			top = margin[0];
			bottom = margin[1];
		}
	}
	surface->m_margin_left = left;
	surface->m_margin_bottom = bottom;
	surface->m_margin_right = right;
	surface->m_margin_top = top;
}

void Plugin::SetNodeEntity_Surface(const IDParam &id, graph::Surface *surface)
{
	typedef map<uint32_t, graph::Surface *> Table;

	/* map::operator[], inlined */
	graph::Surface *&entry = m_surface_table._Insert(Table::value_type(id.m_idhash, NULL),
							 NULL)->value.second;

	entry = surface;
	if (surface != NULL)
		surface->AddRef();
}

/* called whole by GetTexture */
__attribute__((noinline)) void Plugin::SetNodeEntity_Surface(const cxml::Element &node,
							      graph::Surface *surface)
{
	uint32_t idhash = 0;

	scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);
	SetNodeEntity_Surface(IDParam(s_empty_id, idhash), surface);
}

/* the texture data, inflated first when compressed, decoded with the decode heap */
intrusive_ptr<graph::Surface> Plugin::LoadTexture(graph::SurfacePool *pool, void *buf,
						  uint32_t size, uint32_t type, bool compressed,
						  uint32_t origsize)
{
	graph::Surface::LoadOption option;

	option.decode_heap = m_decode_heap;
	if (!compressed)
		return graph::Surface::Load(pool, buf, size, &option);

	void *data;

	if (m_decode_heap != NULL)
		data = m_decode_heap->sce_paf_malloc(origsize);
	else
		data = sce_paf_malloc(origsize);
	if (data == NULL)
		return intrusive_ptr<graph::Surface>();

	int32_t inflated = compress::Decompress(data, origsize, buf, size,
						compress::DecompressType_Zlib);

	if (inflated < 0) {
		if (m_decode_heap != NULL)
			m_decode_heap->sce_paf_free(data);
		else
			sce_paf_free(data);
		return intrusive_ptr<graph::Surface>();
	}

	intrusive_ptr<graph::Surface> surface = graph::Surface::Load(pool, data, inflated, &option);

	if (m_decode_heap != NULL)
		m_decode_heap->sce_paf_free(data);
	else
		sce_paf_free(data);
	return surface;
}

graph::Surface *Plugin::GetNodeEntity_Surface(const IDParam &id) const
{
	map<uint32_t, graph::Surface *>::iterator it = m_surface_table.find(id.GetIDHash());

	if (it == m_surface_table.end())
		return NULL;
	return it->second;
}

graph::Surface *Plugin::GetNodeEntity_Surface(const cxml::Element &node) const
{
	uint32_t idhash = 0;

	scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);
	return GetNodeEntity_Surface(IDParam(s_empty_id, idhash));
}

intrusive_ptr<graph::Surface> Plugin::GetTexture(const cxml::Element &node,
						 graph::SurfacePool *pool)
{
	int32_t error = 0;

	if (!cxml::util::CheckElementName(node, "texture"))
		return intrusive_ptr<graph::Surface>();
	if (m_rscobj == NULL)
		return intrusive_ptr<graph::Surface>();

	uint32_t idhash = 0;

	scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);

	intrusive_ptr<graph::Surface> surface(GetNodeEntity_Surface(IDParam(s_empty_id, idhash)));

	if (surface.get() != NULL)
		return surface;

	cxml::Element elm = m_rscobj->GetOverrideElement(node);
	cxml::File src;
	void *buf;
	uint32_t size, type;
	const char *value;
	uint32_t len;

	if (m_resource_data != NULL || m_texture_table_loaded) {
		if (m_rscobj->GetTextureFromBuffer(node, &buf, &size, &type) != 0 || buf == NULL)
			return intrusive_ptr<graph::Surface>();
		error = scePafResourceGetAttributeString(&elm, "compress", &value, &len);
		if (error == 0) {
			if (sce_paf_strncmp(value, "on", 2) == 0 && len == 2) {
				int32_t origsize;

				scePafResourceGetAttributeInteger(&elm, "origsize", &origsize);
				surface = LoadTexture(pool, buf, size, type, true, origsize);
			} else {
				surface = LoadTexture(pool, buf, size, type, false, 0);
			}
		} else {
			surface = LoadTexture(pool, buf, size, type, false, 0);
		}
		if (surface.get() == NULL)
			return intrusive_ptr<graph::Surface>();
	} else {
		error = m_rscobj->GetTextureFromFile(elm, &src, &size, &type);
		if (error != 0 || size == 0)
			return intrusive_ptr<graph::Surface>();

		if (m_decode_heap != NULL)
			buf = m_decode_heap->sce_paf_malloc(size);
		else if (m_private_heap != NULL)
			buf = m_private_heap->sce_paf_malloc(size);
		else
			buf = sce_paf_malloc(size);
		if (buf != NULL) {
			bool reopened = false;

			if (!m_rscobj->m_rscfp->IsOpened()) {
				reopened = true;
				m_rscobj->m_rscfp = LocalFile::Open(m_rscobj->GetFileName(),
								    SCE_O_RDONLY, 0, &error);
			}
			error = src.Load(m_rscobj->m_rscfp, buf, size);
			if (error == 0) {
				error = scePafResourceGetAttributeString(&elm, "compress", &value,
									 &len);
				if (error == 0) {
					if (sce_paf_strncmp(value, "on", 2) == 0 && len == 2) {
						int32_t origsize;

						scePafResourceGetAttributeInteger(&elm, "origsize",
										  &origsize);
						surface = LoadTexture(pool, buf, size, type, true,
								      origsize);
					} else {
						surface = LoadTexture(pool, buf, size, type, false, 0);
					}
				} else {
					surface = LoadTexture(pool, buf, size, type, false, 0);
				}
			}
			if (reopened)
				m_rscobj->m_rscfp->Close();
			if (m_decode_heap != NULL)
				m_decode_heap->sce_paf_free(buf);
			else if (m_private_heap != NULL)
				m_private_heap->sce_paf_free(buf);
			else
				sce_paf_free(buf);
			if (surface.get() == NULL)
				return intrusive_ptr<graph::Surface>();
		}
	}

	error = scePafResourceGetAttributeString(&elm, "ignore_make_surface_first", &value, &len);
	if ((error != 0 || sce_paf_strncmp(value, "on", 2) != 0 || len != 2) &&
	    (m_option & Option_RememberSurface))
		SetNodeEntity_Surface(node, surface.get());
	SetTextureDivide(elm, surface.get());
	return surface;
}

intrusive_ptr<graph::Surface> Plugin::GetTexture(const cxml::Element &node)
{
	return GetTexture(node, m_surface_pool);
}

void Plugin::SetResourceTable_Texture()
{
	if (m_resource_data != NULL)
		m_rscobj->SetTextureTable(64);
}

void Plugin::LoadResourceTable_Texture()
{
	if (m_resource_data != NULL)
		return;
	if (ScePafResource_B72D030D(m_rscobj, 64, m_decode_heap) == 0)
		m_texture_table_loaded = true;
}

}

using namespace paf;

void ScePafToplevel_952CD3CF(Plugin *plugin)
{
	if (plugin->m_resource_data != NULL)
		plugin->m_rscobj->SetSoundTable(64);
}

void ScePafToplevel_1230A05F(Plugin *plugin)
{
	if (plugin->m_resource_data != NULL)
		return;

	bool pool = false;
	int32_t mode = Framework::framework->GetMode();

	if (plugin->m_surface_pool != NULL)
		pool = plugin->m_surface_pool->m_type != graph::SurfacePool::MemoryType_Normal;
	ScePafResource_EA11BD0B(plugin->m_rscobj, 64, 0, mode == 1, pool);
}

namespace paf {

Plugin::Plugin(const InitParam *param)
	: m_initialized(false), m_name(param->name), m_caller_name(param->caller_name),
	  m_set_param_func(param->set_param_func), m_init_func(param->init_func),
	  m_start_func(param->start_func), m_stop_func(param->stop_func),
	  m_exit_func(param->exit_func), m_module(NULL), m_event_handler_table(NULL),
	  m_layout_handler_table(NULL), m_rscobj(NULL), m_private_heap(NULL),
	  m_private_heap_created(false), m_decode_heap(NULL), m_decode_heap_created(false),
	  m_surface_pool_addr(NULL), m_surface_pool(NULL), m_arguments(param->arguments),
	  m_option(param->option), m_param(param->param), m_last_active_page_node(NULL),
	  m_draw_priority(param->draw_priority), m_locale(param->locale),
	  m_texture_table_loaded(false), m_tex_load_filter(0),
	  m_monitoring_callback_pageopen(NULL), m_monitoring_callback_pageactivate(NULL),
	  m_monitoring_callback_pageclose(NULL), m_resource_data(param->resource_data),
	  m_resource_data_size(param->resource_data_size), m_resolution_debug_mode(0),
	  m_page_debug_mode(0), m_env_param(NULL), m_env_param_created(false),
	  m_graphics_flag(system::SystemParam::GetTextStyle()), m_unloading(false)
{
	m_style_param_manager = new VersatileParamManager();
	m_obj_param_manager = new VersatileParamManager();
	m_sound_param_manager = new VersatileParamManager();

	if (param->module_file.size() != 0) {
		char path[256];

		sceAppMgrConvertVs0UserDrivePath(param->module_file.c_str(), path, sizeof(path));

		int32_t option = param->module_option;

		if (Framework::framework->GetMode() == Framework::Mode_CommonDialog)
			option |= Module::Option_LoadTypeCdlg;
		for (uint32_t i = 0; i < 100; i++) {
			m_module = new Module(path, NULL, option, NULL);
			if (m_module == NULL || m_module->GetStatus() == 0)
				break;
			delete m_module;
			m_module = NULL;
			thread::Sleep(100);
		}
	}
	if (m_module != NULL && (param->module_option & Module::Option_UseInterface)) {
		const ModuleInterface *iface = static_cast<const ModuleInterface *>(
			m_module->GetInterface(param->module_interface_version));

		m_set_param_func = iface->set_param_func;
		m_init_func = iface->init_func;
		m_start_func = iface->start_func;
		m_stop_func = iface->stop_func;
		m_exit_func = iface->exit_func;
	}

	InitParam2 param2;

	if (m_set_param_func != NULL) {
		m_set_param_func(&param2);
		m_tex_load_filter = param2.texture_load_filter;
	}
	if (m_draw_priority < 0)
		m_draw_priority = param2.draw_priority;
	if (param->notify_callback != NULL)
		m_notify_callback_list.push_back(param->notify_callback);
	if (param->unload_callback != NULL)
		m_unload_callback_list.push_back(param->unload_callback);

	if (param->private_heap_size != 0) {
		char name[32];
		memory::HeapAllocator::Option heap_option;

		sce_paf_snprintf(name, sizeof(name), "ScePafPrivateHeap(%s)", m_name.c_str());
		heap_option.entry_max = 0;
		heap_option.warning_size = 0;
		heap_option.disable_dbgprint = false;
		heap_option.disable_assert_on_alloc_failure = true;
		heap_option.memblk_type = memory::AllocationBlockType_User;
		heap_option.alignment = 0;
		if (Framework::framework->GetMode() == Framework::Mode_CommonDialog)
			heap_option.memblk_type = memory::AllocationBlockType_User_Cdlg;
		m_private_heap = new memory::HeapAllocator(NULL, param->private_heap_size, name,
							   &heap_option);
		m_private_heap_created = true;
	} else if (param->private_heap != NULL) {
		m_private_heap = param->private_heap;
	} else {
		m_private_heap = NULL;
	}

	if (param->decode_heap_size != 0) {
		char name[32];
		memory::HeapAllocator::Option heap_option;

		sce_paf_snprintf(name, sizeof(name), "ScePafDecodeHeap(%s)", m_name.c_str());
		heap_option.entry_max = 0;
		heap_option.warning_size = 0;
		heap_option.disable_dbgprint = false;
		heap_option.disable_assert_on_alloc_failure = true;
		heap_option.memblk_type = memory::AllocationBlockType_User;
		heap_option.alignment = 0;
		if (Framework::framework->GetMode() == Framework::Mode_CommonDialog)
			heap_option.memblk_type = memory::AllocationBlockType_User_Cdlg;
		m_decode_heap = new memory::HeapAllocator(NULL, param->decode_heap_size, name,
							  &heap_option);
		m_decode_heap_created = true;
	} else if (param->decode_heap != NULL) {
		m_decode_heap = param->decode_heap;
	} else {
		m_decode_heap = Framework::framework->m_decode_heap;
		if (m_decode_heap == NULL) {
			if (m_private_heap != NULL)
				m_decode_heap = m_private_heap;
			else
				m_decode_heap = NULL;
		}
	}

	if (param->surface_pool != NULL) {
		m_surface_pool = param->surface_pool;
	} else if (param2.surface_pool_size != 0) {
		char name[64];

		sce_paf_snprintf(name, sizeof(name), "ScePafSurfacePool(%s)", m_name.c_str());
		m_surface_pool_addr = GraphMem::AllocOSMemory(GraphMem::DeviceType_SystemMemory,
							      param2.surface_pool_size, name, NULL);
		m_surface_pool = new graph::SurfacePool(m_surface_pool_addr,
							param2.surface_pool_size, name, NULL,
							graph::SurfacePool::MemoryType_Normal);
	} else {
		m_surface_pool = gutil::GetDefaultSurfacePool();
	}

	if (m_locale < 0)
		m_locale = Framework::framework->m_locale;

	if (param->resource_file.size() != 0 || m_resource_data != NULL) {
		resource::ResourceObj::Option option;

		if (m_private_heap != NULL)
			option.allocator = m_private_heap;
		else
			option.allocator = Framework::framework->m_plugin_heap;
		option.cxml_allocator_func = ResourceObjCxmlAllocator;
		if (m_option & Option_ResourceLoadWithDebugSymbol)
			option.load_type = 4;
		if (m_resource_data != NULL) {
			m_rscobj = new resource::ResourceObj(m_resource_data, m_resource_data_size,
							     &option);
		} else {
			char path[256];

			sceAppMgrConvertVs0UserDrivePath(param->resource_file.c_str(), path,
							 sizeof(path));
			m_rscobj = new resource::ResourceObj(path, &option);
		}
		if (m_rscobj != NULL && m_rscobj->GetLoadStatus() != 0) {
			delete m_rscobj;
			m_rscobj = NULL;
		}
	}

	ui::Environment *env = Framework::framework->m_env;

	if (env != NULL) {
		m_env_param = env->m_env_param;
		if (param->env_param != NULL && param->env_param != m_env_param) {
			m_env_param = param->env_param->Clone();
			m_env_param_created = true;
		}
		if (m_rscobj != NULL) {
			graph::Surface *surface = env->m_screen->m_surface.get();
			uint32_t id = m_env_param->GetResolutionIdHD(surface->m_width,
								     surface->m_height,
								     env->m_screen_orientation_mode);

			if (id == 255) {
				surface = env->m_screen->m_surface.get();
				id = m_env_param->GetResolutionIdHD(surface->m_width,
								    surface->m_height, 0);
			}

			ui::SceneResolution *resolution = ScePafWidget_CDC90BA2(m_env_param, id);

			if (resolution != NULL) {
				ui::ResolutionResource resource =
					ScePafWidget_E963EAEE(m_env_param, resolution->m_base_id);

				m_rscobj->SetResolutionResource(resource);
			}
		}
	}

	if (param->graphics_flag != 128)
		m_graphics_flag = param->graphics_flag;
	else if (param2.graphics_flag != 128)
		m_graphics_flag = param2.graphics_flag;

	if (m_rscobj != NULL) {
		ScePafResource_42E4E566(m_rscobj, m_locale, m_decode_heap);
		if (m_rscobj != NULL) {
			if (m_resource_data != NULL) {
				m_rscobj->SetSoundTable(64);
			} else {
				bool pool = false;
				int32_t mode = Framework::framework->GetMode();

				if (m_surface_pool != NULL)
					pool = m_surface_pool->m_type !=
					       graph::SurfacePool::MemoryType_Normal;
				ScePafResource_EA11BD0B(m_rscobj, 64, 0, mode == 1, pool);
			}
			if (m_rscobj != NULL) {
				if ((m_option & Option_MakeSurfaceFirst) &&
				    (m_option & Option_RememberSurface)) {
					if (m_resource_data != NULL)
						m_rscobj->SetTextureTable(64);
					else if (ScePafResource_B72D030D(m_rscobj, 64, m_decode_heap) == 0)
						m_texture_table_loaded = true;

					cxml::Element table =
						m_rscobj->GetTableElement(resource::TableType_Texture);

					if (table.doc != NULL && table.offset >= 0) {
						cxml::Element node;

						for (node = table.GetFirstChild();
						     node.doc != NULL && node.offset >= 0;
						     node = node.GetNextSibling()) {
							cxml::Element elm = m_rscobj->GetOverrideElement(node);
							const char *value;
							uint32_t len;

							if (scePafResourceGetAttributeString(
								    &elm, "ignore_make_surface_first",
								    &value, &len) != 0 ||
							    sce_paf_strncmp(value, "on", 2) != 0 || len != 2)
								GetTexture(node, m_surface_pool);
						}
					}
					UnloadResourceTable_Texture();
				}
				if (m_rscobj != NULL) {
					LoadResourceTable_Style();
					if (m_rscobj != NULL) {
						ScePafToplevel_C1BE28B4(this);
						if (m_rscobj != NULL) {
							LoadResourceTable_Template();
							if (m_rscobj != NULL && m_resource_data == NULL)
								ScePafResource_CEED7338(m_rscobj);
						}
					}
				}
			}
		}
	}

	if (m_init_func != NULL)
		m_init_func(this);
	m_initialized = true;
}

}

using namespace paf;

void ScePafToplevel_E87AD3F3(Plugin *plugin)
{
	if (plugin->m_resource_data != NULL)
		return;

	bool pool = false;
	int32_t mode = Framework::framework->GetMode();

	if (plugin->m_surface_pool != NULL)
		pool = plugin->m_surface_pool->m_type != graph::SurfacePool::MemoryType_Normal;
	ScePafResource_556665FC(plugin->m_rscobj, 0, mode == 1, pool);
}

namespace paf {

void Plugin::UnloadResourceTable_Template()
{
	for (map<uint32_t, TemplateCache *>::iterator it = m_template_table.begin();
	     it != m_template_table.end(); ++it) {
		if (it->second != NULL)
			delete it->second;
	}
	m_template_table._Tidy();
}

Plugin::PageCloseParam::PageCloseParam()
{
	fade = false;
	timer = NULL;
	fade_time_ms = 200.0f;
	transition_type = TransitionType_None;
}

__attribute__((noinline)) void Plugin::SetNodeEntity_Widget(const IDParam &id, ui::Widget *widget)
{
	typedef map<uint32_t, ui::Widget *> Table;

	uint32_t idhash = id.m_idhash;

	if (widget != NULL) {
		/* map::operator[], inlined */
		m_widget_table._Insert(Table::value_type(idhash, NULL), NULL)->value.second = widget;
		return;
	}
	m_widget_table.erase(idhash);
}

void Plugin::SetNodeEntity_Widget(const cxml::Element &node, ui::Widget *widget)
{
	IDParam id(s_empty_name, 0);
	uint32_t idhash;

	if (scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash) == 0)
		id.m_idhash = idhash;
	SetNodeEntity_Widget(id, widget);
}

void Plugin::ClearNodeEntity_ForEachWidget(const cxml::Element &node)
{
	IDParam id(s_empty_name, 0);
	uint32_t idhash;

	if (scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash) == 0)
		id.m_idhash = idhash;
	SetNodeEntity_Widget(id, NULL);
}

}

using namespace paf;

void ScePafToplevel_340EE53C(Plugin *plugin, ui::Widget *page,
			     const Plugin::PageCloseParam &param)
{
	if (!param.fade && param.transition_type == Plugin::TransitionType_None) {
		if (!page->m_destroy_protected)
			page->DestroyWidget();
		return;
	}

	float time = 0.0f;

	if (param.fade) {
		float fade_time_ms = param.fade_time_ms;

		if (fade_time_ms > 0.0f) {
			Timer *timer = new Timer(fade_time_ms);
			float total = timer != NULL ? timer->GetTotalTime() : 0.0f;

			page->m_transition_complete = false;
			page->m_transition = 0xff;
			page->DoTransitionReverse_core(total, true);
			ScePafWidget_4588699E(page, timer, 2, 0.0f, NULL);
		} else {
			page->m_transition_complete = false;
			page->m_transition = 0xff;
			page->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(page, NULL, 2, 0.0f, NULL);
		}
		time = param.fade_time_ms;
	}
	switch (param.transition_type) {
	case Plugin::TransitionType_SlideFromTop:
		ScePafCommon_B48C9BDC(0.0f, page, common::transition::Type_SlideFromTop3, true, false);
		break;
	case Plugin::TransitionType_SlideFromBottom:
		ScePafCommon_B48C9BDC(0.0f, page, common::transition::Type_SlideFromBottom4, true,
				      false);
		break;
	case Plugin::TransitionType_SlideFromLeft:
		ScePafCommon_B48C9BDC(0.0f, page, common::transition::Type_SlideFromLeft1, true,
				      false);
		break;
	case Plugin::TransitionType_SlideFromRight:
		ScePafCommon_B48C9BDC(0.0f, page, common::transition::Type_SlideFromRight2, true,
				      false);
		break;
	default:
		break;
	}
	if (time < 0.0f)
		time = 0.0f;
	ScePafWidget_AC0C81D5(page, time);
	if (time > 0.0f && !page->m_destroy_protected)
		ScePafWidget_5A9793B0(page, time);
}

namespace paf {

__attribute__((noinline)) void Plugin::SetDrawOrder_Remove(ui::Scene *page)
{
	for (vector<DrawOrderElement>::iterator it = m_draw_order.begin();
	     it != m_draw_order.end(); ++it) {
		if (it->page == page) {
			m_draw_order.erase(it);
			return;
		}
	}
}

ui::Widget *Plugin::GetNodeEntity_Widget(const IDParam &id) const
{
	map<uint32_t, ui::Widget *>::iterator it = m_widget_table.find(id.m_idhash);

	if (it == m_widget_table.end())
		return NULL;
	return it->second;
}

ui::Widget *Plugin::GetNodeEntity_Widget(const cxml::Element &node) const
{
	IDParam id(s_empty_name, 0);
	uint32_t idhash;

	if (scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash) == 0)
		id.m_idhash = idhash;
	return GetNodeEntity_Widget(id);
}

__attribute__((noinline)) void Plugin::PageClose(const cxml::Element &node,
						 const PageCloseParam &param)
{
	if (!m_initialized || !cxml::util::CheckElementName(node, "page"))
		return;

	ui::Widget *page = GetNodeEntity_Widget(node);

	if (page == NULL)
		return;
	SetDrawOrder_Remove(static_cast<ui::Scene *>(page));
	ScePafToplevel_340EE53C(this, page, param);
	SetNodeEntity_Widget(node, NULL);
	if (m_last_active_page_node == &node)
		m_last_active_page_node = NULL;
	if (m_monitoring_callback_pageclose != NULL) {
		cxml::Element closed = node;

		m_monitoring_callback_pageclose(this, &closed);
	}
}

__attribute__((noinline)) void Plugin::SetNodeEntity_CreatePage(const IDParam &id,
								ui::Widget *page)
{
	typedef map<string, ui::Widget *> Table;

	const char *name = id.m_id.c_str();

	if (page != NULL) {
		string key(name);

		/* map::operator[], inlined */
		m_create_page_table._Insert(Table::value_type(key, NULL), NULL)->value.second =
			page;
	} else {
		m_create_page_table.erase(string(name));
	}
}

__attribute__((noinline)) ui::Widget *Plugin::GetNodeEntity_CreatePage(const IDParam &id) const
{
	map<string, ui::Widget *>::iterator it = m_create_page_table.find(string(id.m_id.c_str()));

	if (it == m_create_page_table.end())
		return NULL;
	return it->second;
}

void Plugin::PageClose(const IDParam &id, const PageCloseParam &param)
{
	ui::Widget *page = GetNodeEntity_CreatePage(id);

	if (page != NULL) {
		SetDrawOrder_Remove(static_cast<ui::Scene *>(page));
		ScePafToplevel_340EE53C(this, page, param);
		SetNodeEntity_Widget(id, NULL);
		SetNodeEntity_CreatePage(id, NULL);
	} else if (m_rscobj != NULL) {
		cxml::Element node;

		if (m_rscobj->GetPage(id.m_idhash, node) == 0)
			PageClose(node, param);
	}
}

void Plugin::PageCloseAll()
{
	if (m_rscobj != NULL) {
		cxml::Element pages = m_rscobj->GetTableElement(resource::TableType_Page);

		if (pages.doc != NULL && pages.offset >= 0) {
			cxml::Element node;

			for (node = pages.GetFirstChild(); node.doc != NULL && node.offset >= 0;
			     node = node.GetNextSibling())
				PageClose(node, PageCloseParam());
		}
	}
	while (m_create_page_table.size() != 0) {
		IDParam id(m_create_page_table.begin()->first);

		PageClose(id, PageCloseParam());
	}
}

Plugin::~Plugin()
{
	if (m_rscobj != NULL) {
		PageCloseAll();
		UnloadResourceTable_Template();

		cxml::Element textures = m_rscobj->GetTableElement(resource::TableType_Texture);

		if (textures.doc != NULL && textures.offset >= 0) {
			cxml::Element node;

			for (node = textures.GetFirstChild(); node.doc != NULL && node.offset >= 0;
			     node = node.GetNextSibling()) {
				uint32_t idhash = 0;

				scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);

				graph::Surface *surface =
					GetNodeEntity_Surface(IDParam(s_empty_id, idhash));

				if (surface != NULL)
					surface->Release();
			}
		}
		if (m_resource_data == NULL)
			ScePafResource_556665FC(m_rscobj, 0, Framework::framework->GetMode() == 1,
						m_surface_pool != NULL && m_surface_pool->m_type !=
							graph::SurfacePool::MemoryType_Normal);
		if (m_rscobj != NULL)
			delete m_rscobj;
	}
	if (m_surface_pool_addr != NULL) {
		if (m_surface_pool != NULL)
			delete m_surface_pool;
		if (Framework::framework->GetMode() == 5)
			ScePafGraphics_E4B3B1D2(graph::arl::Gl());
		GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, m_surface_pool_addr);
	}
	if (m_sound_param_manager != NULL)
		delete m_sound_param_manager;
	if (m_obj_param_manager != NULL)
		delete m_obj_param_manager;
	if (m_style_param_manager != NULL)
		delete m_style_param_manager;
	if (m_module != NULL)
		delete m_module;
	if (m_decode_heap_created && m_decode_heap != NULL)
		delete m_decode_heap;
	if (m_private_heap_created && m_private_heap != NULL)
		delete m_private_heap;
	for (list<UnloadCallback>::iterator it = m_unload_callback_list.begin();
	     it != m_unload_callback_list.end(); ++it) {
		if (*it != NULL)
			(*it)(m_param);
	}
	if (m_env_param_created && m_env_param != NULL)
		delete m_env_param;
}

void Plugin::PageActivateActually()
{
	GetNodeEntity_Widget(*m_last_active_page_node);
}

void Plugin::PageActivate(const cxml::Element &node)
{
	if (!m_initialized)
		return;

	ui::Widget *page = GetNodeEntity_Widget(node);

	if (page == NULL)
		return;
	m_last_active_page_node = &node;
	GetNodeEntity_Widget(node);
	if (m_monitoring_callback_pageactivate != NULL) {
		cxml::Element activated = node;

		m_monitoring_callback_pageactivate(this, &activated);
	}
}

}

using namespace paf;

int32_t ScePafToplevel_68607CDE(Plugin *plugin, ui::Scene *page, int32_t mode, bool enable)
{
	if (page == NULL || page->GetTypeName() != ui::Scene::TypeName())
		return -1;

	switch (mode) {
	case 0:
	case 1:
		if (enable) {
			if (mode == 0)
				page->SetDebugMode(ui::Scene::DEBUG_AUTH_PARAMVIEW);
			else if (mode == 1)
				page->SetDebugMode(ui::Scene::DEBUG_AUTH_LAYOUT);
			page->m_debug_focus_func = Plugin::PageDebugFocusCB;
			gutil::debug::AddDebugGuiFunc(Plugin::DrawVisualParams, plugin);
		} else {
			gutil::debug::RemoveDebugGuiFunc(Plugin::DrawVisualParams, plugin);
			if (s_debug_layout_param != NULL) {
				s_debug_layout_param->Destroy(2);
				delete s_debug_layout_param;
				s_debug_layout_param = NULL;
			}
			if (s_debug_style_param != NULL) {
				s_debug_style_param->Destroy(2);
				s_debug_style_param = NULL;
			}
			if (s_debug_widget_param != NULL) {
				s_debug_widget_param->Destroy(2);
				s_debug_widget_param = NULL;
			}
			page->m_debug_focus_func = NULL;
			page->SetDebugMode(ui::Scene::DEBUG_OFF);
		}
		break;
	case 2:
		page->SetDebugMode(enable ? ui::Scene::DEBUG_AUTH_HIDEALL : ui::Scene::DEBUG_OFF);
		break;
	}
	return 0;
}

void ScePafToplevel_995AE4B6(Plugin *plugin, ui::Widget *page, const Plugin::PageOpenParam &param)
{
	if (!param.fade && param.transition_type == Plugin::TransitionType_None)
		return;

	float time = 0.0f;

	if (param.fade) {
		page->m_transition_complete = false;
		page->m_transition = 0xff;
		page->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(page, NULL, 2, 0.0f, NULL);

		float fade_time_ms = param.fade_time_ms;

		if (fade_time_ms > 0.0f) {
			Timer *timer = new Timer(fade_time_ms);

			if (!page->m_transition_complete) {
				page->m_transition = 0xff;
				page->m_transition_complete = true;
				page->DoTransition_core(0.0f, true);
			}
			ScePafWidget_FF5BC957(page, timer, 2, 0.0f, NULL);
		} else {
			if (!page->m_transition_complete) {
				page->m_transition = 0xff;
				page->m_transition_complete = true;
				page->DoTransition_core(0.0f, true);
			}
			ScePafWidget_FF5BC957(page, NULL, 2, 0.0f, NULL);
		}
		time = param.fade_time_ms;
	}
	switch (param.transition_type) {
	case Plugin::TransitionType_SlideFromTop:
		ScePafCommon_6CF3A3C8(0.0f, page, common::transition::Type_SlideFromTop3, false, false);
		break;
	case Plugin::TransitionType_SlideFromBottom:
		ScePafCommon_6CF3A3C8(0.0f, page, common::transition::Type_SlideFromBottom4, false,
				      false);
		break;
	case Plugin::TransitionType_SlideFromLeft:
		ScePafCommon_6CF3A3C8(0.0f, page, common::transition::Type_SlideFromLeft1, false,
				      false);
		break;
	case Plugin::TransitionType_SlideFromRight:
		ScePafCommon_6CF3A3C8(0.0f, page, common::transition::Type_SlideFromRight2, false,
				      false);
		break;
	default:
		break;
	}
	if (time < 0.0f)
		time = 0.0f;
	ScePafWidget_AC0C81D5(page, time);
}

namespace paf {

void Plugin::SetDrawOrder_Insert(ui::Scene *page, int32_t view_priority, int32_t page_priority)
{
	for (vector<DrawOrderElement>::iterator it = m_draw_order.begin();
	     it != m_draw_order.end(); ++it) {
		if (it->view_priority < view_priority ||
		    (it->view_priority == view_priority && it->page_priority < page_priority)) {
			page->SetBehind(it->page);

			DrawOrderElement elm = { page, view_priority, page_priority };

			m_draw_order.insert(it, elm);
			return;
		}
	}

	DrawOrderElement elm = { page, view_priority, page_priority };

	m_draw_order.push_back(elm);
}

void Plugin::AdjustPageDrawOrder(ui::Scene *page, const IDParam &id, int32_t view_priority)
{
	if (m_rscobj == NULL)
		return;

	int32_t page_priority = 0;
	cxml::Element pages = m_rscobj->GetTableElement(resource::TableType_Page);

	if (pages.doc != NULL && pages.offset >= 0) {
		cxml::Element node;

		for (node = pages.GetFirstChild(); node.doc != NULL && node.offset >= 0;
		     node = node.GetNextSibling(), page_priority--) {
			uint32_t idhash;

			if (scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash) != 0)
				continue;
			if (id.m_idhash == IDParam(s_empty_id, idhash).m_idhash)
				break;
		}
	}
	if (view_priority < 0)
		view_priority = m_draw_priority;
	SetDrawOrder_Insert(page, view_priority, page_priority);
}

}

using namespace paf;

void ScePafToplevel_2E1F0393(Plugin *plugin, const cxml::Element &elm, ui::Widget *widget,
			     const IDParam &id)
{
	if (widget == NULL || id.m_idhash == 0)
		return;

	const char *type = widget->GetTypeName();
	char name[32] = "sound_";
	size_t len = sce_paf_strlen(type) + 1;

	if (len > sizeof(name) - sizeof("sound_") + 1)
		len = sizeof(name) - sizeof("sound_");
	sce_paf_strncat(name, type, len);

	ui::WidgetSoundParam *param =
		static_cast<ui::WidgetSoundParam *>(plugin->m_sound_param_manager->FindParam(name, id));

	if (param == NULL) {
		Plugin *common = Framework::framework->m_common_resource_plugin;

		if (common != NULL)
			param = static_cast<ui::WidgetSoundParam *>(
				common->m_sound_param_manager->FindParam(name, id));
	}
	if (param != NULL) {
		param->AddWidget(widget);
		widget->m_sound_num = param->m_sound_list.size();
		widget->m_sounds = param->m_sound_list.ptr;
	}
}

namespace paf {

ui::HandlerCB Plugin::GetInitEventHandler(const IDParam &id) const
{
	map<uint32_t, int32_t>::iterator it = m_event_handler_name_table.find(id.m_idhash);

	if (it == m_event_handler_name_table.end())
		return NULL;
	return m_init_event_handler_table[it->second].func;
}

void Plugin::InstantiateWidget_InitEventHandler(const IDParam &id, ui::Widget *widget,
						InitEventFunctionEntry2 *event_handler_table)
{
	if (event_handler_table != NULL) {
		for (int32_t i = 0; event_handler_table[i].id.m_idhash != 0; i++) {
			if (id.m_idhash == event_handler_table[i].id.m_idhash) {
				widget->AddEventCallback(ui::Handler::CB_STATE_INIT,
							 event_handler_table[i].func,
							 event_handler_table[i].userdata);
				return;
			}
		}
	}
	widget->AddEventCallback(ui::Handler::CB_STATE_INIT, GetInitEventHandler(id));
}

/* called whole by InstantiateWidget_Focus */
__attribute__((noinline)) VersatileParam *Plugin::CallFactoryFocus(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_resource_param();
	return NULL;
}

void Plugin::InstantiateWidget_Focus(const cxml::Element &elm, ui::Widget *widget)
{
	cxml::Element focus;

	for (cxml::Element child = elm.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = child.GetNextSibling()) {
		const char *name = child.GetName();

		if (name[0] == 'f' && sce_paf_strncmp("focus_", name, 6) == 0 &&
		    (focus.doc == NULL || focus.offset < 0))
			focus = child;
	}
	if (focus.doc == NULL || focus.offset < 0)
		return;

	ui::WidgetFocusParam *param = static_cast<ui::WidgetFocusParam *>(CallFactoryFocus(elm.GetName()));

	if (param == NULL)
		return;
	param->InitResolution(focus.GetNumChild());
	param->Create(1);
	param->m_widget = widget;
	param->m_plugin = this;
	param->Read(&focus);
	param->Set();
	param->m_widget = NULL;
	param->Destroy(1);
	delete param;
}

/* Sony's build calls this vector's push_back rather than inlining it. */
template<>
inline __attribute__((noinline)) int
vector<WidgetResourceInfo::LayoutData>::push_back(const WidgetResourceInfo::LayoutData &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) WidgetResourceInfo::LayoutData(val);
		result = ++count;
	}
	return result;
}

/* called whole by InstantiateWidget_Layout */
__attribute__((noinline)) VersatileParam *Plugin::CallFactoryLayout(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_layout();
	return NULL;
}

void Plugin::InstantiateWidget_Layout(const cxml::Element &elm, ui::Widget *widget,
				      WidgetResourceInfo *info)
{
	cxml::Element layout;

	for (cxml::Element child = elm.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = child.GetNextSibling()) {
		const char *name = child.GetName();

		if (name[0] != 'l' || sce_paf_strncmp("layout_", name, 7) != 0)
			continue;
		if (layout.doc == NULL || layout.offset < 0)
			layout = child;

		uint32_t idhash;

		if (scePafResourceGetAttributeStyleId(&child, "id", &idhash) == 0) {
			WidgetResourceInfo::LayoutData data = { IDParam(s_empty_id, idhash), child,
								NULL };

			info->m_list_layout.push_back(data);
		}
	}
	if (layout.doc == NULL || layout.offset < 0)
		return;

	ui::WidgetLayoutParam *param =
		static_cast<ui::WidgetLayoutParam *>(CallFactoryLayout(elm.GetName()));

	if (param == NULL)
		return;
	param->InitResolution(layout.GetNumChild());
	param->Create(1);
	param->m_widget = widget;
	param->Read(&layout);
	param->GetEach(widget);
	param->Set();
	param->m_widget = NULL;
	param->Destroy(1);
	delete param;
}

/* called whole by CreateWidgetWithStyleType */
__attribute__((noinline)) ui::WidgetParam *Plugin::CallFactoryWidgetParam(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_param();
	return NULL;
}

/* called whole by CreateWidgetWithStyleType */
__attribute__((noinline)) ui::Widget *Plugin::CallFactoryWidget(const char *name,
								ui::Widget *parent)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_widget(parent);
	return NULL;
}

ui::Widget *Plugin::CreateWidgetWithStyleType(ui::Widget *parent, const char *type,
					      const IDParam &id, const char *style_type,
					      const IDParam &style_id)
{
	if (parent == NULL)
		return NULL;

	IDParam style(style_id);

	if (style.m_idhash == 0) {
		char name[48] = "_common_default_";

		sce_paf_strncat(name, style_type, 31);
		style.SetID(string(name));
	}

	ui::WidgetStyleParam *style_param = static_cast<ui::WidgetStyleParam *>(
		m_style_param_manager->FindParam(style_type, style));

	if (style_param == NULL) {
		Plugin *common = Framework::framework->m_common_resource_plugin;

		if (common != NULL)
			style_param = static_cast<ui::WidgetStyleParam *>(
				common->m_style_param_manager->FindParam(style_type, style));
		if (style_param == NULL && style_id.m_idhash != 0)
			return NULL;
	}

	ui::Widget *widget = CallFactoryWidget(type, parent);

	if (widget == NULL)
		return NULL;
	widget->m_name = id;
	if (style_param != NULL) {
		style_param->AddWidget(widget);
		style_param->SetEach(widget);
	}

	WidgetResourceInfo *info = new WidgetResourceInfo(this);

	if (info == NULL) {
		widget->DestroyWidget();
		return NULL;
	}
	info->m_style_id = style;
	if (widget->m_user_data2[0] != NULL)
		delete widget->m_user_data2[0];
	widget->m_user_data2[0] = info;

	ui::WidgetParam *wparam = CallFactoryWidgetParam(type);

	if (wparam != NULL) {
		wparam->m_id = id;
		wparam->AttachPlugin(this);
		ScePafResource_190E99A6(wparam, widget);
		wparam->m_style_id = style;
		info->m_wparam = wparam;
		if (style_param != NULL)
			info->m_sound_id = style_param->m_sound_id;
	}
	return widget;
}

ui::Widget *Plugin::CreateWidget(ui::Widget *parent, const char *type, const IDParam &id,
				 const IDParam &style)
{
	char style_type[32] = "style_";
	size_t len = sce_paf_strlen(type) + 1;

	if (len > sizeof(style_type) - sizeof("style_") + 1)
		len = sizeof(style_type) - sizeof("style_");
	sce_paf_strncat(style_type, type, len);
	return CreateWidgetWithStyleType(parent, type, id, style_type, style);
}

int32_t Plugin::TemplateOpen(ui::Widget *parent, const IDParam &id,
			     const TemplateOpenParam &param)
{
	map<uint32_t, TemplateCache *>::iterator it = m_template_table.find(id.m_idhash);

	if (it != m_template_table.end() && it->second != NULL)
		return it->second->Open(this, parent, param);

	if (m_rscobj != NULL) {
		cxml::Element table = m_rscobj->GetTableElement(resource::TableType_Template);

		if (table.doc != NULL && table.offset >= 0) {
			cxml::Element node =
				cxml::util::FindChildElement(table, "template", "id", id.m_idhash);

			if (node.doc != NULL && node.offset >= 0) {
				cxml::Element child;

				for (child = node.GetFirstChild(); child.doc != NULL && child.offset >= 0;
				     child = child.GetNextSibling())
					InstantiateWidget(child, parent, true, param.event_handler_table,
							  param.sibling);
				return 0;
			}
		}
	}
	if (sce_paf_strncmp(m_name.c_str(), "__system__common_resource", 26) != 0) {
		Plugin *common = Framework::framework->m_common_resource_plugin;

		if (common != NULL)
			return common->TemplateOpen(parent, id, param);
	}
	return -1;
}

Plugin::TemplateOpenParam::TemplateOpenParam()
{
	sibling = NULL;
	event_handler_table = NULL;
}

void Plugin::InstantiateWidget(const cxml::Element &elm, ui::Widget *parent, bool is_template,
			       InitEventFunctionEntry2 *event_handler_table, ui::Widget *sibling)
{
	const char *name = elm.GetName();

	if (name == NULL || parent == NULL || sce_paf_strncmp(name, "layout_", 7) == 0 ||
	    sce_paf_strncmp(name, "focus_", 6) == 0)
		return;

	if (sce_paf_strncmp(name, "template_ref", 12) == 0) {
		cxml::Element node = m_rscobj->GetOverrideElement(elm);
		cxml::Element entity;
		uint32_t idhash;

		if (scePafResourceGetAttributeIdInt(&node, "template", &idhash, &entity) == 0) {
			TemplateOpenParam param;

			param.event_handler_table = event_handler_table;
			TemplateOpen(parent, IDParam(s_empty_id, idhash), param);
		}
		return;
	}

	cxml::Element style_entity;
	uint32_t style_idhash;

	if (scePafResourceGetAttributeIdInt(&elm, "style", &style_idhash, &style_entity) != 0)
		style_idhash = 0;

	IDParam style(s_empty_id, style_idhash);

	ScePafToplevel_3B003F16(this, &style.m_id, style_idhash, true);

	uint32_t idhash = 0;

	scePafResourceGetAttributeStyleId(&elm, "id", &idhash);

	IDParam id(s_empty_id, idhash);

	ScePafToplevel_3B003F16(this, &id.m_id, idhash, true);

	char style_type[32] = "style_";
	size_t len = sce_paf_strlen(name) + 1;

	if (len > sizeof(style_type) - sizeof("style_") + 1)
		len = sizeof(style_type) - sizeof("style_");
	sce_paf_strncat(style_type, name, len);

	ui::Widget *widget = CreateWidgetWithStyleType(parent, name, id, style_type, style);

	if (widget == NULL)
		return;
	if (is_template)
		widget->m_under_template = true;

	WidgetResourceInfo *info = static_cast<WidgetResourceInfo *>(widget->m_user_data2[0]);

	if (info != NULL) {
		info->m_node = elm;
		InstantiateWidget_Layout(elm, widget, info);
	}
	InstantiateWidget_Focus(elm, widget);
	InstantiateWidget_InitEventHandler(id, widget, event_handler_table);

	cxml::Element child;

	for (child = elm.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = child.GetNextSibling())
		InstantiateWidget(child, widget->GetInternalWidget(), is_template, event_handler_table,
				  NULL);

	if (info != NULL && info->m_wparam != NULL) {
		ui::WidgetParam *wparam = info->m_wparam;

		wparam->Create(1);
		wparam->Read(&elm);
		wparam->Set();
		wparam->Destroy(1);
		if (wparam->m_sound_id.m_idhash != 0) {
			ScePafToplevel_3B003F16(this, &wparam->m_sound_id.m_id,
						wparam->m_sound_id.m_idhash, true);
			info->m_sound_id = wparam->m_sound_id;
		}
		ScePafToplevel_2E1F0393(this, elm, widget, info->m_sound_id);
	}
	if (sibling != NULL)
		widget->SetBehind(sibling);
}

ui::Scene *Plugin::PageOpen(const cxml::Element &node, const PageOpenParam &param)
{
	if (!m_initialized || !cxml::util::CheckElementName(node, "page"))
		return NULL;

	ui::Widget *opened = GetNodeEntity_Widget(node);

	if (opened != NULL)
		return static_cast<ui::Scene *>(opened);

	uint32_t idhash = 0;

	scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);

	IDParam id(s_empty_id, idhash);

	ScePafToplevel_3B003F16(this, &id.m_id, idhash, false);

	ui::EnvironmentParam *env_param = m_env_param;
	int32_t graphics_flag = m_graphics_flag;

	if (param.env_param != NULL)
		env_param = param.env_param;
	if (param.graphics_flag != 128)
		graphics_flag = param.graphics_flag;
	ui::Scene *page = new ui::Scene(Framework::framework->m_env,
					param.overwrite_draw_priority >= 0 ?
						param.overwrite_draw_priority : m_draw_priority,
					NULL, env_param, graphics_flag);

	InstantiateWidget_InitEventHandler(id, page, param.event_handler_table);
	SetNodeEntity_Widget(node, page);
	if (id.m_idhash != 0)
		page->m_name = id;

	IDParam plugin_id(m_name);

	page->m_plugin_id = plugin_id.m_idhash;

	ui::Widget::CreateParam camera_param;

	camera_param.m_parent_protected = true;
	new ui::Camera(page, &camera_param);

	const int32_t *origin;
	uint32_t num;

	if (scePafResourceGetAttributeIntegerArray(&node, "origin", &origin, &num) == 0) {
		uint32_t ax = num != 0 ? origin[0] : 0;
		uint32_t ay = --num != 0 ? origin[1] : 0;
		uint32_t az = --num != 0 ? origin[2] : 0;

		if ((ax | ay | az) != 0)
			page->SetOrigin(ax, ay, az, NULL);
	}

	cxml::Element entity;
	uint32_t template_id;

	if (scePafResourceGetAttributeIdInt(&node, "template", &template_id, &entity) == 0) {
		TemplateOpenParam template_param;

		template_param.event_handler_table = param.event_handler_table;
		TemplateOpen(page, IDParam(s_empty_id, template_id), template_param);
	}

	cxml::Element child;

	for (child = node.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = child.GetNextSibling())
		InstantiateWidget(child, page->GetInternalWidget(), false, param.event_handler_table,
				  NULL);
	AdjustPageDrawOrder(page, id, param.overwrite_draw_priority);
	ScePafToplevel_995AE4B6(this, page, param);
	if (m_monitoring_callback_pageopen != NULL) {
		cxml::Element opened_node = node;

		m_monitoring_callback_pageopen(this, &opened_node);
	}
	switch (m_page_debug_mode) {
	case 1:
		ScePafToplevel_68607CDE(this, page, 0, true);
		break;
	case 2:
		ScePafToplevel_68607CDE(this, page, 1, true);
		break;
	case 3:
		ScePafToplevel_68607CDE(this, page, 2, true);
		break;
	}
	graph::s_page_opened = true;
	return page;
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

void Plugin::Exit()
{
	if (m_option & Option_WaitWidgetActuallyDestroyed) {
		while (ui::Widget::s_destroying_count > 0)
			thread::Sleep(17);
	}
	if (m_exit_func != NULL)
		m_exit_func(this);
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

ui::Scene *Plugin::PageOpen(const IDParam &id, const PageOpenParam &param)
{
	if (param.option & PageOption_Create) {
		if (m_rscobj != NULL && m_rscobj->IsRegisteredID(id.m_idhash) == true)
			return NULL;
		if (GetNodeEntity_Widget(id) != NULL)
			return NULL;

		ui::EnvironmentParam *env_param = m_env_param;
		int32_t graphics_flag = m_graphics_flag;

		if (param.env_param != NULL)
			env_param = param.env_param;
		if (param.graphics_flag != 128)
			graphics_flag = param.graphics_flag;
		ui::Scene *page = new ui::Scene(Framework::framework->m_env,
						param.overwrite_draw_priority >= 0 ?
							param.overwrite_draw_priority : m_draw_priority,
						NULL, env_param, graphics_flag);

		SetNodeEntity_Widget(id, page);
		SetNodeEntity_CreatePage(id, page);
		page->m_name = id;

		IDParam plugin_id(m_name);

		page->m_plugin_id = plugin_id.m_idhash;

		ui::Widget::CreateParam camera_param;

		camera_param.m_parent_protected = true;
		new ui::Camera(page, &camera_param);
		AdjustPageDrawOrder(page, id, param.overwrite_draw_priority);
		ScePafToplevel_995AE4B6(this, page, param);
		graph::s_page_opened = true;
		return page;
	}
	if (m_rscobj == NULL)
		return NULL;

	cxml::Element node;

	if (m_rscobj->GetPage(id.m_idhash, node) != 0)
		return NULL;
	return PageOpen(node, param);
}

void Plugin::PageActivate(const IDParam &id)
{
	if (m_rscobj == NULL)
		return;

	cxml::Element node;

	if (m_rscobj->GetPage(id.m_idhash, node) == 0)
		PageActivate(node);
}

void Plugin::PageInactivateActually()
{
	GetNodeEntity_Widget(*m_last_active_page_node);
}

/* called whole by PageInactivate(IDParam) */
__attribute__((noinline)) void Plugin::PageInactivate(const cxml::Element &node)
{
	if (!m_initialized || m_last_active_page_node != &node)
		return;
	GetNodeEntity_Widget(node);
	m_last_active_page_node = NULL;
}

void Plugin::PageInactivate(const IDParam &id)
{
	if (m_rscobj == NULL)
		return;

	cxml::Element node;

	if (m_rscobj->GetPage(id.m_idhash, node) == 0)
		PageInactivate(node);
}

ui::Scene *Plugin::PageRoot(const IDParam &id) const
{
	return static_cast<ui::Scene *>(GetNodeEntity_Widget(id));
}

ui::Scene *Plugin::PageRoot(const cxml::Element &node) const
{
	return static_cast<ui::Scene *>(GetNodeEntity_Widget(node));
}

void Plugin::SetLocale(Locale locale)
{
	if (m_rscobj == NULL)
		return;
	if (locale < 0)
		locale = static_cast<Locale>(Framework::framework->m_locale);
	m_locale = locale;
	ScePafResource_42E4E566(m_rscobj, locale, m_decode_heap);
}

void Plugin::SetLocaleAll(Locale locale)
{
	Framework::framework->SetLocaleAllPlugin(locale);
}

const wchar_t *Plugin::GetString(const IDParam &id) const
{
	const wchar_t *str;
	uint32_t length;

	if (m_rscobj != NULL && m_rscobj->GetString(id.m_idhash, &str, &length) == 0)
		return str;
	if (sce_paf_strncmp(m_name.c_str(), "__system__common_resource", 26) != 0)
		return Framework::framework->GetCommonString(id);
	return L"";
}

intrusive_ptr<graph::Surface> Plugin::GetTexture(const IDParam &id, graph::SurfacePool *pool)
{
	graph::Surface *surface = GetNodeEntity_Surface(id);

	if (surface != NULL) {
		intrusive_ptr<graph::Surface> texture(surface);

		return texture;
	}
	if (m_rscobj != NULL) {
		cxml::Element table = m_rscobj->GetTableElement(resource::TableType_Texture);

		if (table.doc != NULL && table.offset >= 0) {
			cxml::Element node;

			node = cxml::util::FindChildElement(table, "texture", "id", id.m_idhash);

			if (node.doc != NULL && node.offset >= 0)
				return GetTexture(node, pool);
		}
	}
	if (sce_paf_strncmp(m_name.c_str(), "__system__common_resource", 26) != 0)
		return Framework::framework->GetCommonTexture(id);
	return intrusive_ptr<graph::Surface>();
}

intrusive_ptr<graph::Surface> Plugin::GetTexture(const IDParam &id)
{
	return GetTexture(id, m_surface_pool);
}

void Plugin::Notify(int32_t status) const
{
	for (list<NotifyCallback>::iterator it = m_notify_callback_list.begin();
	     it != m_notify_callback_list.end(); ++it) {
		if (*it != NULL)
			(*it)(status, m_param);
	}
}

void Plugin::SetInitEventHandler(const InitEventFunctionEntry *table)
{
	typedef map<uint32_t, int32_t> Table;

	m_init_event_handler_table = table;
	for (int32_t i = 0; m_init_event_handler_table[i].id.m_idhash != 0; i++)
		m_event_handler_name_table._Insert(
			Table::value_type(m_init_event_handler_table[i].id.m_idhash, 0), NULL)
			->value.second = i;
}

void Plugin::SetEventHandler(const EventFunctionEntry *table)
{
	typedef map<uint32_t, int32_t> Table;

	m_event_handler_table = table;
	for (int32_t i = 0; m_event_handler_table[i].name != NULL; i++) {
		IDParam id(m_event_handler_table[i].name);

		m_event_handler_name_table._Insert(Table::value_type(id.m_idhash, 0), NULL)
			->value.second = i;
	}
}

void Plugin::SetLayoutHandler(const LayoutFunctionEntry *table)
{
	typedef map<string, int32_t> Table;

	m_layout_handler_table = table;
	for (int32_t i = 0; m_layout_handler_table[i].name != NULL; i++) {
		string name(m_layout_handler_table[i].name);
		int32_t &index =
			m_layout_handler_name_table._Insert(Table::value_type(name, 0), NULL)->value.second;

		index = i;
	}
}

void Plugin::SetInterface(int32_t id, void *iface)
{
	typedef map<int32_t, void *> Table;

	m_interface_table._Insert(Table::value_type(id, NULL), NULL)->value.second = iface;
}

void *Plugin::GetInterface(int32_t id) const
{
	map<int32_t, void *>::iterator it = m_interface_table.find(id);

	if (it == m_interface_table.end())
		return NULL;
	return it->second;
}

void Plugin::AppendNotifyCallback(NotifyCallback cb)
{
	if (cb != NULL)
		m_notify_callback_list.push_back(cb);
}

void Plugin::RemoveNotifyCallback(NotifyCallback cb)
{
	for (list<NotifyCallback>::iterator it = m_notify_callback_list.begin();
	     it != m_notify_callback_list.end(); ++it) {
		if (*it == cb)
			m_notify_callback_list.erase(it);
	}
}

void Plugin::AppendUnloadCallback(UnloadCallback cb)
{
	if (cb != NULL)
		m_unload_callback_list.push_back(cb);
}

void Plugin::RemoveUnloadCallback(UnloadCallback cb)
{
	for (list<UnloadCallback>::iterator it = m_unload_callback_list.begin();
	     it != m_unload_callback_list.end(); ++it) {
		if (*it == cb)
			m_unload_callback_list.erase(it);
	}
}

int32_t Plugin::CreateStyle(const char *type, const IDParam &id)
{
	if (type == NULL)
		return -1;

	int32_t ret = 0;
	VersatileParam *param = CallFactoryStyle(type);

	if (param != NULL) {
		char name[32] = "style_";
		size_t len = sce_paf_strlen(type) + 1;

		if (len > sizeof(name) - sizeof("style_") + 1)
			len = sizeof(name) - sizeof("style_");
		sce_paf_strncat(name, type, len);
		param->m_id = id;
		param->AttachPlugin(this);
		m_style_param_manager->AddParam(name, id, param);
	} else {
		ret = -1;
	}
	return ret;
}

int32_t Plugin::MorphWidgetLayout(ui::Widget *widget, const cxml::Element &elm, Timer *timer)
{
	if (widget == NULL)
		return -1;

	WidgetResourceInfo *info = static_cast<WidgetResourceInfo *>(widget->m_user_data2[0]);

	if (info == NULL)
		return -1;
	if (info->m_node.doc != NULL && info->m_node.offset >= 0 && elm.doc != NULL &&
	    elm.offset >= 0) {
		cxml::Element node = info->m_node;
		ui::WidgetLayoutParam *param =
			static_cast<ui::WidgetLayoutParam *>(CallFactoryLayout(node.GetName()));

		if (param != NULL) {
			param->Attach(widget);
			param->Create(1);
			param->Read(&elm);
			param->SetTimer(timer);
			param->Set();
			param->Destroy(1);
			delete param;
		}
	}
	return 0;
}

int32_t Plugin::MorphWidgetLayout(ui::Widget *widget, uint32_t index, Timer *timer)
{
	if (widget == NULL)
		return -1;

	WidgetResourceInfo *info = static_cast<WidgetResourceInfo *>(widget->m_user_data2[0]);

	if (info == NULL || info->m_list_layout.size() < index)
		return -1;

	WidgetResourceInfo::LayoutData &data = info->m_list_layout[index];

	if (data.m_param != NULL) {
		data.m_param->SetTimer(timer);
		data.m_param->Set();
	} else if (data.m_node.doc != NULL && data.m_node.offset >= 0) {
		MorphWidgetLayout(widget, data.m_node, timer);
	}
	return 0;
}

int32_t Plugin::MorphWidgetLayout(ui::Widget *widget, const IDParam &id, Timer *timer)
{
	if (widget == NULL)
		return -1;

	WidgetResourceInfo *info = static_cast<WidgetResourceInfo *>(widget->m_user_data2[0]);

	if (info == NULL)
		return -1;

	vector<WidgetResourceInfo::LayoutData>::iterator it;

	for (it = info->m_list_layout.begin(); it != info->m_list_layout.end(); ++it) {
		if (it->m_id.m_idhash != id.m_idhash)
			continue;
		if (it->m_param != NULL) {
			it->m_param->SetTimer(timer);
			it->m_param->Set();
		} else if (it->m_node.doc != NULL && it->m_node.offset >= 0) {
			MorphWidgetLayout(widget, it->m_node, timer);
		}
		return 0;
	}
	return -1;
}

int32_t Plugin::SetPageDebugMode(const IDParam &id, PageDebugMode mode, bool enable)
{
	ui::Widget *page = GetNodeEntity_Widget(id);

	if (page == NULL)
		return -1;
	return ScePafToplevel_68607CDE(this, static_cast<ui::Scene *>(page), mode, enable);
}

Plugin::AddTextureInfo *Plugin::GetNodeEntity_AddTexture(const IDParam &id) const
{
	map<uint32_t, AddTextureInfo>::iterator it = m_add_texture_table.find(id.m_idhash);

	if (it == m_add_texture_table.end())
		return NULL;
	return &it->second;
}

int32_t Plugin::WriteTextureTable(common::SharedPtr<File> fp, bool table)
{
	int32_t indent = 0;
	int32_t ret = 0;

	if (m_surface_table.begin() == m_surface_table.end())
		return 0;
	if (table) {
		if (ScePafResource_D74427E6(fp, 6, 1, false) != 0)
			return -1;
		indent = 1;
	}
	for (map<uint32_t, graph::Surface *>::iterator it = m_surface_table.begin();
	     it != m_surface_table.end(); ++it) {
		if (it->second == NULL)
			continue;

		string id;
		string src;
		string type;
		AddTextureInfo *info = GetNodeEntity_AddTexture(IDParam(s_empty_id, it->first));

		if (info != NULL) {
			id = info->id.m_id;
			src = info->path;
			switch (info->type) {
			case 0:
				type = "texture/png";
				break;
			case 1:
				type = "texture/jpg";
				break;
			case 2:
				type = "texture/tif";
				break;
			case 3:
				type = "texture/gif";
				break;
			case 4:
				type = "texture/bmp";
				break;
			case 5:
				type = "texture/gim";
				break;
			case 6:
				type = "texture/dds";
				break;
			case 7:
				type = "texture/gxt";
				break;
			default:
				type = "";
				break;
			}
		} else if (m_rscobj != NULL) {
			if (m_rscobj->GetDebugSymbol(it->first, 0, &id) != 0) {
				ScePafResource_D74427E6(fp, 7, indent + 1, false);
				return -1;
			}
			if (m_rscobj->GetDebugSymbol(it->first, 1, &src) != 0) {
				ScePafResource_D74427E6(fp, 7, indent + 1, false);
				return -1;
			}
			ret = m_rscobj->GetDebugSymbol(it->first, 2, &type);
			if (ret != 0) {
				ScePafResource_D74427E6(fp, 7, indent + 1, false);
				return -1;
			}
		}

		const char *src_name = src.c_str();
		const char *type_name = type.c_str();
		ResourceParam param;

		param.SetTagName("texture");
		param.SetIDName(id.c_str());
		param.AddParam("src", &src_name, ResourceParam::SEMANTIC_STRING);
		param.AddParam("type", &type_name, ResourceParam::SEMANTIC_STRING);
		ScePafResource_441E989F(&param, fp, 0, indent + 1, false);
	}
	if (table && ScePafResource_D74427E6(fp, 7, 1, false) != 0)
		return -1;
	return ret;
}

int32_t Plugin::WriteStyleTable(common::SharedPtr<File> fp, bool table)
{
	int32_t indent = 0;
	int32_t ret = 0;

	if (m_style_param_manager->m_param_table.begin() ==
	    m_style_param_manager->m_param_table.end())
		return 0;
	if (table) {
		if (ScePafResource_D74427E6(fp, 4, 1, false) != 0)
			return -1;
		indent = 1;
	}
	for (VersatileParamManager::iterator it = m_style_param_manager->begin();
	     it != m_style_param_manager->end(); ++it) {
		VersatileParam *param = m_style_param_manager->FindParam(it.GetTag().c_str(),
									 it.GetID());
		string id;

		param->Create(1);
		ret = m_rscobj->GetDebugSymbol(param->m_id.m_idhash, 0, &id);
		if (ret == 0) {
			param->m_id.m_id = string(id.c_str());
			param->m_serial->SetIDName(id.c_str());
		}
		param->Write(fp, 0, indent + 1);
		param->Destroy(1);
	}
	if (table && ScePafResource_D74427E6(fp, 5, 1, false) != 0)
		return -1;
	return ret;
}

int32_t Plugin::WriteWidget(ui::Widget *widget, common::SharedPtr<File> fp, uint32_t indent,
			    bool print)
{
	if (widget == NULL)
		return -1;
	if (!print && widget->m_under_template)
		return 1;

	const char *type = widget->GetTypeName();

	if (widget->m_parent_protected) {
		if (widget->m_children.size() != 0) {
			for (list<ui::Widget *>::iterator it = widget->m_children.begin();
			     it != widget->m_children.end(); ++it)
				WriteWidget(*it, fp, indent, print);
		}
		return 0;
	}

	WidgetResourceInfo *info = (WidgetResourceInfo *)widget->m_user_data2[0];

	if (info == NULL || info->m_wparam == NULL)
		return -1;
	info->m_wparam->Create(1);
	{
		IDParam id = widget->m_name;

		info->m_wparam->m_id = id;
		if (info->m_wparam->m_serial != NULL)
			info->m_wparam->m_serial->SetIDName(info->m_wparam->m_id.m_id.c_str());
		info->m_wparam->Write(fp, 1, indent);

		uint8_t pos_num = widget->m_pos_num;
		ui::WidgetLayoutParam *layout = (ui::WidgetLayoutParam *)CallFactoryLayout(type);

		if (layout != NULL) {
			layout->Attach(widget);
			layout->m_id.SetID(string("type1"));
			if (pos_num != 0)
				layout->InitResolution(pos_num);
			layout->Create(1);
			layout->Get();
			layout->Write(fp, 0, indent + 1);
			layout->Destroy(1);
			delete layout;
		}
		for (list<ui::Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); ++it)
			WriteWidget(*it, fp, indent + 1, print);
		if (info->m_wparam->Write(fp, 2, indent) != 0)
			return -1;
	}
	info->m_wparam->Destroy(1);
	return 0;
}

int32_t Plugin::WritePageTable(common::SharedPtr<File> fp, bool table, bool print)
{
	int32_t indent = 0;
	int32_t ret = 0;
	int32_t err;

	if (m_widget_table.begin() == m_widget_table.end())
		return 0;
	/* without a file, to the TTY */
	if (table) {
		if (fp.get() != NULL)
			err = ScePafResource_D74427E6(fp, 2, 1, false);
		else
			err = ScePafResource_D74427E6(fp, 2, 1, true);
		if (err != 0)
			return -1;
		indent = 1;
	}

	list<ui::Widget *> &scenes = Framework::framework->m_env->m_scenes_onupdate;

	for (list<ui::Widget *>::iterator it = scenes.begin(), end = scenes.end(); it != end;
	     ++it) {
		IDParam plugin_id(m_name);

		if (((ui::Scene *)*it)->m_plugin_id != plugin_id.m_idhash)
			continue;

		const char *name = (*it)->m_name.m_id.c_str();
		ResourceParam param;

		param.SetTagName("page");
		param.SetIDName(name);

		cxml::Element elm;
		string template_id(s_empty_name);
		uint32_t template_idhash = 0;

		if (m_rscobj != NULL && m_rscobj->GetPage(name, elm) == 0) {
			cxml::Element entity;

			if (scePafResourceGetAttributeIdInt(&elm, "template", &template_idhash,
							    &entity) == 0) {
				string symbol;

				ret = m_rscobj->GetDebugSymbol(template_idhash, 0, &symbol);
				if (ret == 0)
					template_id = string(symbol.c_str());
				param.AddParam("template", &template_id,
					       ResourceParam::SEMANTIC_IDHASHREF);
			}
		}
		if ((int32_t)(*it)->m_children.size() - 1 <= 0) {
			if (fp.get() != NULL)
				err = ScePafResource_441E989F(&param, fp, 0, indent + 1, false);
			else
				err = ScePafResource_441E989F(&param, fp, 0, indent + 1, true);
			if (err != 0)
				return -1;
		} else {
			if (fp.get() != NULL)
				err = ScePafResource_441E989F(&param, fp, 1, indent + 1, false);
			else
				err = ScePafResource_441E989F(&param, fp, 1, indent + 1, true);
			if (err != 0)
				return -1;
			for (list<ui::Widget *>::iterator child = (*it)->m_children.begin();
			     child != (*it)->m_children.end(); ++child)
				WriteWidget(*child, fp, indent + 2, print);
			if (fp.get() != NULL)
				err = ScePafResource_441E989F(&param, fp, 2, indent + 1, false);
			else
				err = ScePafResource_441E989F(&param, fp, 2, indent + 1, true);
			if (err != 0)
				return -1;
		}
	}
	if (table) {
		if (fp.get() != NULL)
			err = ScePafResource_D74427E6(fp, 3, 1, false);
		else
			err = ScePafResource_D74427E6(fp, 3, 1, true);
		if (err != 0)
			return -1;
	}
	return ret;
}

int32_t Plugin::WriteResourceAll(common::SharedPtr<File> fp)
{
	if (ScePafResource_D74427E6(fp, 0, 0, false) != 0)
		return -1;
	WritePageTable(fp, true, false);
	WriteStyleTable(fp, true);
	WriteTextureTable(fp, true);
	if (ScePafResource_D74427E6(fp, 1, 0, false) != 0)
		return -1;
	return 0;
}

int32_t Plugin::WriteResourcePageTable(common::SharedPtr<File> fp, bool table)
{
	if (ScePafResource_D74427E6(fp, 0, 0, false) != 0)
		return -1;
	WritePageTable(fp, table, false);
	if (ScePafResource_D74427E6(fp, 1, 0, false) != 0)
		return -1;
	return 0;
}

int32_t Plugin::WriteResourceStyleTable(common::SharedPtr<File> fp, bool table)
{
	if (ScePafResource_D74427E6(fp, 0, 0, false) != 0)
		return -1;
	WriteStyleTable(fp, table);
	if (ScePafResource_D74427E6(fp, 1, 0, false) != 0)
		return -1;
	return 0;
}

int32_t Plugin::WriteResourceTextureTable(common::SharedPtr<File> fp, bool table)
{
	if (ScePafResource_D74427E6(fp, 0, 0, false) != 0)
		return -1;
	WriteTextureTable(fp, table);
	if (ScePafResource_D74427E6(fp, 1, 0, false) != 0)
		return -1;
	return 0;
}

int32_t Plugin::GetResourcePages(vector<string> *pages)
{
	int32_t ret = 0;

	if (pages == NULL)
		return -1;
	if (m_rscobj != NULL) {
		cxml::Element table = m_rscobj->GetTableElement(resource::TableType_Page);

		if (table.doc != NULL && table.offset >= 0) {
			cxml::Element node;

			for (node = table.GetFirstChild(); node.doc != NULL && node.offset >= 0;
			     node = node.GetNextSibling()) {
				uint32_t idhash;

				ret = scePafResourceGetAttributeIdIntLpb(&node, "id", &idhash);
				if (ret == 0 && m_rscobj != NULL) {
					string id;

					ret = m_rscobj->GetDebugSymbol(idhash, 0, &id);
					if (ret == 0)
						pages->push_back(id);
				}
			}
		}
	}
	for (map<string, ui::Widget *>::iterator it = m_create_page_table.begin();
	     it != m_create_page_table.end(); ++it) {
		if (it->second != NULL)
			pages->push_back(it->first);
	}
	return ret;
}

int32_t Plugin::GetOpenedPages(vector<string> *pages)
{
	int32_t ret = 0;

	if (pages == NULL)
		return -1;

	list<ui::Widget *> &scenes = Framework::framework->m_env->m_scenes_onupdate;

	for (list<ui::Widget *>::iterator it = scenes.begin(), end = scenes.end(); it != end;
	     ++it) {
		IDParam plugin_id(m_name);
		ui::Scene *scene = (ui::Scene *)*it;

		if (scene->m_plugin_id != plugin_id.m_idhash)
			continue;
		if (m_rscobj != NULL) {
			string id;

			ret = m_rscobj->GetDebugSymbol(scene->m_name.m_idhash, 0, &id);
			if (ret == 0)
				pages->push_back(id);
		}
	}
	for (map<string, ui::Widget *>::iterator it = m_create_page_table.begin();
	     it != m_create_page_table.end(); ++it) {
		if (it->second != NULL)
			pages->push_back(it->first);
	}
	return ret;
}

int32_t Plugin::GetResourceStyles(const char *type, vector<string> *styles)
{
	int32_t ret = 0;

	if (styles == NULL)
		return -1;
	for (VersatileParamManager::iterator it = m_style_param_manager->begin();
	     it != m_style_param_manager->end(); ++it) {
		if (type != NULL) {
			char tag[32] = "style_";
			size_t len = sce_paf_strlen(type) + 1;

			if (len > 26)
				len = 25;
			sce_paf_strncat(tag, type, len);
			if (m_style_param_manager->FindParam(tag, it.GetID()) == NULL)
				continue;
		}
		if (m_rscobj != NULL) {
			string id;

			ret = m_rscobj->GetDebugSymbol(it.GetID().m_idhash, 0, &id);
			if (ret == 0)
				styles->push_back(id);
		}
	}
	return ret;
}

int32_t Plugin::GetResourceTextures(vector<string> *textures)
{
	int32_t ret = 0;

	if (textures == NULL)
		return -1;
	for (map<uint32_t, graph::Surface *>::iterator it = m_surface_table.begin();
	     it != m_surface_table.end(); ++it) {
		if (it->second == NULL)
			continue;

		AddTextureInfo *info = GetNodeEntity_AddTexture(IDParam(s_empty_id, it->first));

		if (info != NULL) {
			textures->push_back(info->id.m_id);
		} else if (m_rscobj != NULL) {
			string id;

			ret = m_rscobj->GetDebugSymbol(it->first, 0, &id);
			if (ret == 0)
				textures->push_back(id);
		}
	}
	return ret;
}

bool Plugin::IsRegisteredID(const IDParam &id)
{
	if (m_rscobj != NULL && m_rscobj->IsRegisteredID(id.m_id.c_str(), 0) == true)
		return true;
	if (m_rscobj != NULL && m_rscobj->IsRegisteredID(id.m_idhash) == true)
		return true;
	for (map<uint32_t, ui::Widget *>::iterator it = m_widget_table.begin();
	     it != m_widget_table.end(); ++it) {
		ui::Widget *page = it->second;

		if (page != NULL) {
			if (id.m_idhash == page->m_name.m_idhash)
				return true;
			if (page->FindChild(id, 0) != NULL)
				return true;
		}
	}
	return m_style_param_manager->FindParam(id) != NULL || GetNodeEntity_AddTexture(id) != NULL;
}

void Plugin::SetNodeEntity_AddTexture(const IDParam &id, AddTextureInfo &info)
{
	uint32_t idhash = id.m_idhash;

	m_add_texture_table[idhash] = info;
}

}

/* a texture loaded from the buffer into the plugin's surface pool */
intrusive_ptr<graph::Surface> ScePafToplevel_F13D2937(Plugin *plugin, void *buf, uint32_t size,
						      uint32_t type, bool compressed,
						      uint32_t origsize)
{
	return plugin->LoadTexture(plugin->m_surface_pool, buf, size, type, compressed, origsize);
}

namespace paf {

int32_t Plugin::AddResourceTexture(const IDParam &id, const char *filename, const char *src)
{
	int32_t ret;
	void *buf = NULL;
	uint32_t read = 0;
	int32_t type;

	if (filename == NULL || src == NULL)
		return -1;

	const char *ext = sce_paf_strrchr(filename, '.');

	if (ext == NULL)
		return -1;
	if (sce_paf_strncasecmp(ext, ".gim", 4) == 0)
		type = 5;
	else if (sce_paf_strncasecmp(ext, ".png", 4) == 0)
		type = 0;
	else if (sce_paf_strncasecmp(ext, ".jpg", 4) == 0)
		type = 1;
	else if (sce_paf_strncasecmp(ext, ".tif", 4) == 0)
		type = 2;
	else if (sce_paf_strncasecmp(ext, ".gif", 4) == 0)
		type = 3;
	else if (sce_paf_strncasecmp(ext, ".bmp", 4) == 0)
		type = 4;
	else if (sce_paf_strncasecmp(ext, ".dds", 4) == 0)
		type = 6;
	else if (sce_paf_strncasecmp(ext, ".gxt", 4) == 0)
		type = 7;
	else
		return -1;

	int32_t error;

	ret = 0;

	common::SharedPtr<File> fp = LocalFile::Open(filename, SCE_O_RDONLY, 0, &error);

	if (fp.get() == NULL)
		return -1;

	uint32_t size = fp->GetFileSize();

	if (size != 0) {
		if (m_private_heap != NULL)
			buf = m_private_heap->sce_paf_malloc(size);
		else
			buf = sce_paf_malloc(size);
		if (buf != NULL)
			read = fp->Read(buf, size);
	} else {
		ret = -1;
	}
	fp->Close();
	if (read != 0 && read == size) {
		intrusive_ptr<graph::Surface> surface = LoadTexture(m_surface_pool, buf, size, type,
								    false, 0);

		if (surface.get() != NULL) {
			typedef map<uint32_t, graph::Surface *> Table;

			m_surface_table._Insert(Table::value_type(id.m_idhash, NULL), NULL)->value.second =
				surface.get();
			surface->AddRef();

			AddTextureInfo info;

			info.id = id;
			info.path = src;
			info.type = type;
			m_add_texture_table[id.m_idhash] = info;
		} else {
			ret = -1;
		}
	}
	if (buf != NULL) {
		if (m_private_heap != NULL)
			m_private_heap->sce_paf_free(buf);
		else
			sce_paf_free(buf);
	}
	return ret;
}

}

int32_t ScePafToplevel_1AB2DB65(Plugin *plugin, const IDParam &id, IDParam *alias)
{
	if (plugin->m_rscobj != NULL) {
		*alias = plugin->m_rscobj->GetAliasID(id);
		if (alias->m_idhash != 0)
			return 0;
	}
	if (sce_paf_strncmp(plugin->m_name.c_str(), "__system__common_resource", 26) != 0)
		return ScePafToplevel_1AB2DB65(Framework::Instance()->m_common_resource_plugin, id,
					       alias);
	*alias = id;
	return 0x80AF0601;	/* no name found */
}

const wchar_t *ScePafToplevel_D8801CA4(Plugin *plugin, const IDParam &id)
{
	IDParam alias(s_empty_name, 0);
	const wchar_t *str;
	uint32_t length;

	ScePafToplevel_1AB2DB65(plugin, id, &alias);
	if (plugin->m_rscobj != NULL &&
	    plugin->m_rscobj->GetString(alias.m_idhash, &str, &length) == 0)
		return str;
	if (sce_paf_strncmp(plugin->m_name.c_str(), "__system__common_resource", 26) != 0)
		return Framework::framework->GetCommonString(alias);
	return L"";
}

namespace paf {

__attribute__((noinline)) VersatileParam *Plugin::CallFactoryHandler(const char *name)
{
	map<string, ui::FactoryFuncTable>::iterator it = s_factory_table.find(string(name));

	if (it != s_factory_table.end())
		return it->second.m_factory_handler();
	return NULL;
}

graph::DrawObj *Plugin::CallFactoryDrawObj(const char *name)
{
	map<string, ui::FactoryFuncDrawObj>::iterator it =
		s_factory_drawobj_table.find(string(name));

	if (it != s_factory_drawobj_table.end())
		return it->second.m_factory_drawobj();
	return NULL;
}

VersatileParam *Plugin::CallFactoryDrawObjParam(const char *name)
{
	map<string, ui::FactoryFuncDrawObj>::iterator it =
		s_factory_drawobj_table.find(string(name));

	if (it != s_factory_drawobj_table.end())
		return it->second.m_factory_param();
	return NULL;
}

ui::HandlerCB Plugin::GetEventHandler(const char *name) const
{
	IDParam id(name);
	map<uint32_t, int32_t>::iterator it = m_event_handler_name_table.find(id.m_idhash);

	if (it == m_event_handler_name_table.end())
		return NULL;
	return m_event_handler_table[it->second].func;
}

void Plugin::InstantiateWidget_EventHandler(const cxml::Element &node, ui::Widget *widget,
					    const char *attr, int32_t type)
{
	const char *name;
	uint32_t len;

	if (scePafResourceGetAttributeString(&node, attr, &name, &len) == 0)
		widget->AddEventCallback(type, GetEventHandler(name));
}

__attribute__((noinline)) ui::LayoutCB Plugin::GetLayoutHandler(const char *name) const
{
	map<string, int32_t>::iterator it = m_layout_handler_name_table.find(string(name));

	if (it == m_layout_handler_name_table.end())
		return NULL;
	return m_layout_handler_table[it->second].func;
}

void Plugin::InstantiateWidget_LayoutHandler(const cxml::Element &node, ui::Widget *widget,
					     const char *attr)
{
	const char *name;
	uint32_t len;

	if (scePafResourceGetAttributeString(&node, attr, &name, &len) == 0) {
		ui::LayoutCB cb = GetLayoutHandler(name);

		widget->SetLayouter(new ui::Layouter(cb, NULL));
	}
}

void Plugin::InstantiateWidget_Texture(const cxml::Element &node, ui::Widget *widget, int32_t obj,
				       const char *attr,
				       const intrusive_ptr<graph::Surface> &def)
{
	uint32_t idhash;
	cxml::Element entity;

	scePafResourceGetAttributeIdInt(&node, attr, &idhash, &entity);
	if (entity.doc != NULL && entity.offset >= 0) {
		intrusive_ptr<graph::Surface> texture = GetTexture(entity, m_surface_pool);

		if (texture.get() != NULL)
			widget->SetTexture(texture, obj, 0);
		return;
	}

	intrusive_ptr<graph::Surface> texture =
		Framework::framework->GetCommonTexture(IDParam(s_empty_id, idhash));

	if (texture.get() != NULL)
		widget->SetTexture(texture, obj, 0);
	else
		widget->SetTexture(def, obj, 0);
}

void Plugin::InstantiateWidget_String(const cxml::Element &node, const char *attr,
				      ui::Widget *widget)
{
	uint32_t idhash;
	cxml::Element entity;

	if (m_rscobj->GetStringIDRef(node, attr, &idhash, &entity) == 0) {
		const wchar_t *str;
		uint32_t len;

		if (scePafResourceGetAttributeChar(&entity, "src", &str, &len) == 0)
			widget->SetString(wstring(str));
	} else if (scePafResourceGetAttributeIdInt(&node, attr, &idhash, &entity) == 0) {
		widget->SetString(wstring(Framework::framework->GetCommonString(
			IDParam(s_empty_id, idhash))));
	}
}

void Plugin::InstantiateWidget_HandlerAll(const cxml::Element &node, ui::Widget *widget)
{
	if (node.doc == NULL || node.offset < 0)
		return;

	ui::WidgetHandlerParam *param = (ui::WidgetHandlerParam *)CallFactoryHandler(node.GetName());

	if (param == NULL)
		return;
	param->AttachPlugin(this);
	param->AttachWidget(widget);
	param->Create(1);
	param->Read(&node);
	param->Set();
	param->Destroy(1);
	delete param;
}

}

/* the draw order's debug print, compiled out */
void ScePafToplevel_4D79D4E2(Plugin *plugin)
{
	for (vector<Plugin::DrawOrderElement>::iterator it = plugin->m_draw_order.begin();
	     it != plugin->m_draw_order.end(); ++it)
		sce_paf_printf_dummy();
}

namespace paf {

/* a hidden page's widget is destroyed when its hide animation ends */
void Plugin::OnAnimPageHideFin(Timer *t, ui::AnimListener *anim, void *data1, void *data2,
			       void *data3)
{
	ui::Widget *widget = (ui::Widget *)data1;

	if (widget != NULL && !widget->m_destroy_protected)
		widget->DestroyWidget();
}

/* the debugged widget is deleted: the debug params go with it */
void Plugin::WidgetDebugDeleteCb(ui::Widget *widget, void *userdata)
{
	if (s_debug_layout_param != NULL) {
		s_debug_layout_param->Destroy(2);
		delete s_debug_layout_param;
		s_debug_layout_param = NULL;
	}
	if (s_debug_style_param != NULL) {
		s_debug_style_param->Destroy(2);
		s_debug_style_param = NULL;
	}
	if (s_debug_widget_param != NULL) {
		s_debug_widget_param->Destroy(2);
		s_debug_widget_param = NULL;
	}
}

/* gives the widget its current position, size and rotation for resolutions 15 and 16 */
void Plugin::DebugAddResolution(ui::Widget *widget, ResolutionDebugMode mode)
{
	math::v4 pos = widget->GetPos(ui::Widget::GET_CURRENT, NULL,
				      math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 size = *widget->GetSize(ui::Widget::GET_CURRENT);
	math::v4 rot = widget->GetRot(ui::Widget::GET_CURRENT, NULL,
				      math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
	uint32_t reso[2] = { 15, 16 };
	ui::ResolutionData pos_data[2];
	ui::ResolutionData size_data[2];
	ui::ResolutionData rot_data[2];

	for (int32_t i = 0; i < 2; i++) {
		pos_data[i].m_id = reso[i];
		pos_data[i].m_value[0] = pos.extract_x();
		pos_data[i].m_value[1] = pos.extract_y();
		pos_data[i].m_value[2] = pos.extract_z();
		size_data[i].m_id = reso[i];
		size_data[i].m_value[0] = size.extract_x();
		size_data[i].m_value[1] = size.extract_y();
		size_data[i].m_value[2] = size.extract_z();
		rot_data[i].m_id = reso[i];
		rot_data[i].m_value[0] = rot.extract_x();
		rot_data[i].m_value[1] = rot.extract_y();
		rot_data[i].m_value[2] = rot.extract_z();
	}
	widget->SetPos(pos_data, 2, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
	widget->SetSize(size_data, 2, NULL, ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
	widget->SetRot(rot_data, 2, 0, NULL, ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
}

/*
 * The debug mode's focus moved: the newly focused widget gets the debug params of its
 * layout, its style and its own parameter (the old ones are destroyed), and the delete
 * callback that drops them moves from the previous widget to it.
 */
int32_t Plugin::PageDebugFocusCB(ui::Widget *focus, ui::Widget *prev, void *userdata)
{
	if (focus == NULL)
		return 0;

	if (prev != NULL)
		prev->RemoveDeleteCB(WidgetDebugDeleteCb, NULL);
	if (focus != NULL)
		focus->AddDeleteCB(WidgetDebugDeleteCb, NULL);

	const char *type = focus->GetTypeName();

	if (s_debug_layout_param != NULL) {
		s_debug_layout_param->Destroy(2);
		delete s_debug_layout_param;
		s_debug_layout_param = NULL;
	}

	uint32_t reso_num = focus->m_pos_num;
	WidgetResourceInfo *info = (WidgetResourceInfo *)focus->m_user_data2[0];

	if (info != NULL && info->m_plugin->m_resolution_debug_mode != ResolutionDebugMode_Off &&
	    reso_num == 0) {
		DebugAddResolution(focus,
				   (ResolutionDebugMode)info->m_plugin->m_resolution_debug_mode);
		reso_num = focus->m_pos_num;
	}
	s_debug_layout_param = (ui::WidgetLayoutParam *)CallFactoryLayout(type);
	if (s_debug_layout_param != NULL) {
		s_debug_layout_param->Attach(focus);
		if (reso_num != 0)
			s_debug_layout_param->InitResolution(reso_num);
		s_debug_layout_param->Create(2);
		s_debug_layout_param->Get();
	}

	if (s_debug_style_param != NULL) {
		s_debug_style_param->Destroy(2);
		s_debug_style_param = NULL;
	}
	if (info != NULL) {
		char tag[32] = "style_";
		uint32_t len = sce_paf_strlen(type) + 1;

		if (len > 26)
			len = 25;
		sce_paf_strncat(tag, type, len);
		s_debug_style_param = info->m_plugin->m_style_param_manager->FindParam(tag,
										     info->m_style_id);
		if (s_debug_style_param != NULL)
			s_debug_style_param->Create(2);
	}

	if (s_debug_widget_param != NULL) {
		s_debug_widget_param->Destroy(2);
		s_debug_widget_param = NULL;
	}
	if (info != NULL) {
		s_debug_widget_param = info->m_wparam;
		s_debug_widget_param->Create(2);
	}
	return 0;
}

/* the debug GUI's frame: the debug params update and draw */
void Plugin::DrawVisualParams(void *userdata)
{
	if (s_debug_layout_param != NULL) {
		s_debug_layout_param->OnUpdate();
		s_debug_layout_param->OnDraw();
	}
	if (s_debug_style_param != NULL) {
		s_debug_style_param->OnUpdate();
		s_debug_style_param->OnDraw();
	}
	if (s_debug_widget_param != NULL) {
		s_debug_widget_param->OnUpdate();
		s_debug_widget_param->OnDraw();
	}
}

/* writes the opened page with the ID and its widgets (to the TTY without a file) */
int32_t Plugin::WritePage(common::SharedPtr<File> fp, const IDParam &id)
{
	int32_t err;

	if (m_widget_table.begin() == m_widget_table.end())
		return 0;

	ui::Widget *page = GetNodeEntity_Widget(id);

	if (page == NULL)
		return 0;

	const char *name = page->m_name.m_id.c_str();
	ResourceParam param;

	param.SetTagName("page");
	param.SetIDName(name);
	if ((int32_t)page->m_children.size() - 1 > 0) {
		if (fp.get() != NULL)
			err = ScePafResource_441E989F(&param, fp, 1, 1, false);
		else
			err = ScePafResource_441E989F(&param, fp, 1, 1, true);
		if (err != 0)
			return -1;
		for (list<ui::Widget *>::iterator child = page->m_children.begin();
		     child != page->m_children.end(); ++child)
			WriteWidget(*child, fp, 2, true);
		if (fp.get() != NULL)
			err = ScePafResource_441E989F(&param, fp, 2, 1, false);
		else
			err = ScePafResource_441E989F(&param, fp, 2, 1, true);
		if (err != 0)
			return -1;
	} else {
		if (fp.get() != NULL)
			err = ScePafResource_441E989F(&param, fp, 0, 1, false);
		else
			err = ScePafResource_441E989F(&param, fp, 0, 1, true);
		if (err != 0)
			return -1;
	}
	return 0;
}

void Plugin::Init()
{
}

void Plugin::End()
{
	gutil::debug::RemoveDebugGuiFuncs(DrawVisualParams);
	if (s_debug_layout_param != NULL) {
		s_debug_layout_param->Destroy(2);
		delete s_debug_layout_param;
		s_debug_layout_param = NULL;
	}
	if (s_debug_style_param != NULL) {
		s_debug_style_param->Destroy(2);
		s_debug_style_param = NULL;
	}
	if (s_debug_widget_param != NULL) {
		s_debug_widget_param->Destroy(2);
		s_debug_widget_param = NULL;
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

int32_t ScePafToplevel_CA3E606F(Plugin *plugin, const IDParam &id, const char **buffer1,
				uint32_t *size1, const char **buffer2, uint32_t *size2,
				int32_t *samplerate, uint32_t *type)
{
	int32_t ret = -1;

	if (plugin->m_rscobj != NULL) {
		ret = ScePafResource_ADDC0711(plugin->m_rscobj, id.m_idhash, buffer1, size1, buffer2,
					      size2, samplerate, type);
		if (ret == 0)
			return ret;
	}
	if (sce_paf_strncmp(plugin->m_name.c_str(), "__system__common_resource", 26) != 0)
		ret = ScePafToplevel_95CF05AC(Framework::framework, id, buffer1, size1, buffer2,
					      size2, samplerate, type);
	return ret;
}

int32_t ScePafToplevel_DA3D4AF0(Plugin *plugin, const IDParam &id,
				ui::WidgetSoundParam::Sound **sounds, uint32_t *num)
{
	ui::WidgetSoundParam *param =
		static_cast<ui::WidgetSoundParam *>(plugin->m_sound_param_manager->FindParam(id));

	if (param == NULL) {
		Plugin *common = Framework::framework->m_common_resource_plugin;

		if (common != NULL)
			param = static_cast<ui::WidgetSoundParam *>(
				common->m_sound_param_manager->FindParam(id));
	}
	if (param == NULL) {
		*sounds = NULL;
		*num = 0;
		return -1;
	}
	*sounds = param->m_sound_list.ptr;
	*num = param->m_sound_list.size();
	return 0;
}

int32_t ScePafToplevel_DD14F248(Plugin *plugin, int32_t mode)
{
	if (plugin->m_page_debug_mode == mode)
		return 0;
	if (plugin->m_rscobj == NULL)
		return -1;

	cxml::Element pages = plugin->m_rscobj->GetTableElement(resource::TableType_Page);

	if (pages.doc == NULL || pages.offset < 0)
		return -1;

	cxml::Element node;

	for (node = pages.GetFirstChild(); node.doc != NULL && node.offset >= 0;
	     node = node.GetNextSibling()) {
		ui::Scene *page = static_cast<ui::Scene *>(plugin->GetNodeEntity_Widget(node));

		if (page == NULL)
			continue;
		switch (plugin->m_page_debug_mode) {
		case 1:
			ScePafToplevel_68607CDE(plugin, page, 0, false);
			break;
		case 2:
			ScePafToplevel_68607CDE(plugin, page, 1, false);
			break;
		case 3:
			ScePafToplevel_68607CDE(plugin, page, 2, false);
			break;
		}
		switch (mode) {
		case 1:
			ScePafToplevel_68607CDE(plugin, page, 0, true);
			break;
		case 2:
			ScePafToplevel_68607CDE(plugin, page, 1, true);
			break;
		case 3:
			ScePafToplevel_68607CDE(plugin, page, 2, true);
			break;
		}
	}
	plugin->m_page_debug_mode = mode;
	return 0;
}
