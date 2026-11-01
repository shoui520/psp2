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
#include <std/stdlib.h>
#include <kernel/iofilemgr.h>

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
