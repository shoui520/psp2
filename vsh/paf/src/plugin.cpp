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
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
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
/* the widget factories and the draw object factories, by their type's name */
static map<string, ui::FactoryFuncTable> s_factory_table;
static map<string, ui::FactoryFuncDrawObj> s_factory_drawobj_table;
/* the params of the widget the page debug mode focuses: its layout, style and widget params (names ours) */
static VersatileParam *s_debug_layout_param;
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
	  m_resource_data_size(param->resource_data_size), m_page_debug_focus_enabled(0),
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
