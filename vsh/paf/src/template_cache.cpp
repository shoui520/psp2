/*
 * paf::TemplateCache: a template's widgets read ahead from the resource, so that
 * Plugin::TemplateOpen makes them without reading the template's elements again
 * (the templates the resource marks fast_open="1"). Each widget of the template is
 * an Item: its type's factories, its style and sound parameters, and its layout,
 * focus and widget parameters, read once and set on each widget it makes.
 * (Sony's file name is not known; class and function names ours.)
 */

#include <stddef.h>
#include <stdint.h>
#include <plugin.h>
#include <framework.h>
#include <cxml/cxml.h>
#include <resource/resource_obj.h>
#include <versatileparam/versatileparam.h>
#include <std/string.h>
#include "paf_types.h"

namespace paf {

/* Sony's build calls these lists' push_back rather than inlining them. */
template<>
inline __attribute__((noinline)) void
list<WidgetResourceInfo::LayoutData>::push_back(const WidgetResourceInfo::LayoutData &val)
{
	if (head == NULL)
		return;

	_Node *node = (_Node *)operator new(sizeof(_Node));

	if (node == NULL)
		ScePafStdc_D608246A();
	if (node != NULL)
		new (&node->value) WidgetResourceInfo::LayoutData(val);
	else
		node = head;
	if (node != head) {
		node->prev = head->prev;
		node->next = head;
		head->prev->next = node;
		size_type n = count + 1;

		head->prev = node;
		count = n;
	}
}

template<>
inline __attribute__((noinline)) void
list<TemplateCache::Item *>::push_back(TemplateCache::Item *const &val)
{
	if (head == NULL)
		return;

	_Node *node = (_Node *)operator new(sizeof(_Node));

	if (node == NULL)
		ScePafStdc_D608246A();
	if (node != NULL)
		new (&node->value) TemplateCache::Item *(val);
	else
		node = head;
	if (node != head) {
		node->prev = head->prev;
		node->next = head;
		head->prev->next = node;
		head->prev = node;
		++count;
	}
}

/* and inlines this list's erase */
template<>
inline list<TemplateCache::Item *>::iterator
list<TemplateCache::Item *>::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;

	node->next->prev = node->prev;
	node->prev->next = node->next;
	operator delete(node);
	--count;
	return iterator(next);
}

int32_t TemplateCache::Clear()
{
	m_created = false;
	for (list<Item *>::iterator it = m_items.begin(); it != m_items.end();) {
		Item *item = *it;

		if (item != NULL) {
			item->Clear();
			delete item;
		}
		it = m_items.erase(it);
	}
	return 0;
}

int32_t TemplateCache::Item::Clear()
{
	m_created = false;
	m_node = cxml::Element();
	m_layout = cxml::Element();
	m_focus = cxml::Element();
	m_id.m_idhash = 0;
	m_style_id.m_idhash = 0;
	sce_paf_memset(&m_factory, 0, sizeof(m_factory));
	m_style_param = NULL;
	m_sound_param = NULL;
	if (m_wparam != NULL) {
		delete m_wparam;
		m_wparam = NULL;
	}
	if (m_layout_param != NULL) {
		delete m_layout_param;
		m_layout_param = NULL;
	}
	if (m_focus_param != NULL) {
		delete m_focus_param;
		m_focus_param = NULL;
	}
	m_list_layout.clear();
	for (list<Item *>::iterator it = m_children.begin(); it != m_children.end();) {
		Item *child = *it;

		if (child != NULL) {
			child->Clear();
			delete child;
		}
		it = m_children.erase(it);
	}
	if (m_template != NULL) {
		m_template->Clear();
		delete m_template;
		m_template = NULL;
	}
	return 0;
}

TemplateCache::Item::Item()
	: m_id(s_empty_name, 0), m_style_id(s_empty_name, 0), m_style_param(NULL),
	  m_sound_param(NULL), m_wparam(NULL), m_layout_param(NULL), m_focus_param(NULL),
	  m_template(NULL)
{
	Clear();
}

int32_t TemplateCache::Item::SetSoundParam(Plugin *plugin, const IDParam &id)
{
	string name("sound_");

	name += m_node.GetName();
	m_sound_param = static_cast<ui::WidgetSoundParam *>(
		plugin->m_sound_param_manager->FindParam(name.c_str(), id));
	if (m_sound_param == NULL) {
		Plugin *common = Framework::Instance()->m_common_resource_plugin;

		if (common == NULL)
			return -1;
		m_sound_param = static_cast<ui::WidgetSoundParam *>(
			common->m_sound_param_manager->FindParam(name.c_str(), id));
	}
	if (m_sound_param != NULL)
		return 0;
	return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
}

int32_t TemplateCache::Item::SetStyleParam(Plugin *plugin, const IDParam &id)
{
	string name("style_");

	name += m_node.GetName();
	m_style_param = static_cast<ui::WidgetStyleParam *>(
		plugin->m_style_param_manager->FindParam(name.c_str(), id));
	if (m_style_param == NULL) {
		Plugin *common = Framework::Instance()->m_common_resource_plugin;

		if (common == NULL)
			return -1;
		m_style_param = static_cast<ui::WidgetStyleParam *>(
			common->m_style_param_manager->FindParam(name.c_str(), id));
	}
	if (m_style_param != NULL)
		return 0;
	return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
}

/* the element's style, else its type's default style ("_common_default_style_<type>") */
static uint32_t GetStyleIDHash(const cxml::Element &elm)
{
	uint32_t idhash = 0;
	cxml::Element entity;

	if (scePafResourceGetAttributeIdInt(&elm, "style", &idhash, &entity) != 0) {
		string name("_common_default_style_");

		name += elm.GetName();
		idhash = IDParam(name).m_idhash;
	}
	return idhash;
}

/* the template of that ID in a resource's template table */
static inline cxml::Element FindTemplate(resource::ResourceObj *rscobj, const IDParam &id)
{
	if (rscobj != NULL) {
		cxml::Element table = rscobj->GetTableElement(resource::TableType_Template);

		if (table.doc != NULL && table.offset >= 0)
			return cxml::util::FindChildElement(table, "template", "id", id.m_idhash);
	}
	return cxml::Element();
}

int32_t TemplateCache::Create(Plugin *plugin, const IDParam &id)
{
	Clear();
	if (plugin == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;

	cxml::Element node = FindTemplate(plugin->m_rscobj, id);

	if (node.doc == NULL || node.offset < 0)
		node = FindTemplate(Framework::Instance()->m_common_resource_plugin->m_rscobj, id);
	if (node.doc == NULL || node.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;

	cxml::Element child;

	for (child = node.GetFirstChild(); child.doc != NULL && child.offset >= 0;
	     child = child.GetNextSibling()) {
		Item *item = new Item();

		if (item == NULL) {
			Clear();
			return SCE_PAF_ERROR_NO_OBJECT;
		}

		int32_t ret = item->Create(plugin, child);

		if (ret != 0) {
			Clear();
			return ret;
		}
		m_items.push_back(item);
	}
	m_created = true;
	return 0;
}

/* a "template_ref" element's template, read ahead like the template that has it */
static int32_t CreateTemplateRef(Plugin *plugin, const cxml::Element &elm, TemplateCache **out)
{
	uint32_t idhash;
	cxml::Element entity;
	int32_t ret = scePafResourceGetAttributeIdInt(&elm, "template", &idhash, &entity);

	if (ret != 0)
		return ret;

	TemplateCache *cache = new TemplateCache();

	if (cache == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	ret = cache->Create(plugin, IDParam(s_empty_id, idhash));
	if (ret != 0)
		delete cache;
	else
		*out = cache;
	return ret;
}

int32_t TemplateCache::Item::Create(Plugin *plugin, const cxml::Element &elm)
{
	int32_t ret;

	Clear();

	const char *name = elm.GetName();

	if (name == NULL) {
		ret = SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
		goto error;
	}
	if (sce_paf_strncmp(name, "layout_", 7) == 0) {
		ret = 0;
		goto error;
	}
	if (sce_paf_strncmp(name, "focus_", 6) == 0) {
		ret = 0;
		goto error;
	}
	if (sce_paf_strncmp(name, "template_ref", 12) == 0) {
		ret = CreateTemplateRef(plugin, elm, &m_template);
		if (ret == 0)
			m_created = true;
		return ret;
	}

	{
		m_node = elm;

		uint32_t idhash = 0;

		if (scePafResourceGetAttributeStyleId(&elm, "id", &idhash) == 0) {
			m_id.m_idhash = idhash;
			ScePafToplevel_3B003F16(plugin, &m_id.m_id, idhash, true);
		}

		map<string, ui::FactoryFuncTable> *table = Plugin::GetWFactoryTable();

		ret = SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
		if (table != NULL) {
			map<string, ui::FactoryFuncTable>::iterator it = table->find(string(name));

			if (it != table->end()) {
				m_factory = it->second;
				ret = 0;
			}
		}
		if (ret != 0)
			goto error;

		idhash = GetStyleIDHash(elm);
		m_style_id.m_idhash = idhash;
		ScePafToplevel_3B003F16(plugin, &m_style_id.m_id, idhash, true);
		ret = SetStyleParam(plugin, m_style_id);
		if (ret != 0)
			goto error;

		m_wparam = m_factory.m_factory_param();
		if (m_wparam == NULL) {
			ret = SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
			goto error;
		}
		m_wparam->m_id = m_id;
		m_wparam->AttachPlugin(plugin);
		m_wparam->m_style_id = m_style_id;
		m_wparam->Create(1);
		m_wparam->Read(&m_node);
		m_wparam->Destroy(1);

		/* the widget's sound parameter, else its style's */
		IDParam sound = m_wparam->m_sound_id;

		if (sound.m_idhash == 0)
			sound = m_style_param->m_sound_id;
		if (sound.m_idhash != 0) {
			ret = SetSoundParam(plugin, sound);
			if (ret != 0)
				goto error;
		} else {
			m_sound_param = NULL;
		}

		cxml::Element child;

		for (child = elm.GetFirstChild(); child.doc != NULL && child.offset >= 0;
		     child = child.GetNextSibling()) {
			const char *child_name = child.GetName();

			if (child_name[0] == 'l' && sce_paf_strncmp("layout_", child_name, 7) == 0) {
				if (m_layout.doc == NULL || m_layout.offset < 0)
					m_layout = child;

				uint32_t layout_idhash = 0;

				scePafResourceGetAttributeStyleId(&child, "id", &layout_idhash);

				WidgetResourceInfo::LayoutData data = {
					IDParam(s_empty_name, layout_idhash), child, NULL
				};

				m_list_layout.push_back(data);
			} else if (child_name[0] == 'f' &&
				   sce_paf_strncmp("focus_", child_name, 6) == 0) {
				if (m_focus.doc == NULL || m_focus.offset < 0)
					m_focus = child;
			} else {
				Item *item = new Item();

				if (item == NULL) {
					ret = SCE_PAF_ERROR_NO_OBJECT;
					break;
				}
				/* a child that cannot be read ends the children, the item stays */
				if (item->Create(plugin, child) != 0) {
					if (item != NULL) {
						item->Clear();
						delete item;
					}
					break;
				}
				m_children.push_back(item);
			}
		}
		if (ret != 0)
			goto error;
		m_created = true;
		return 0;
	}

error:
	Clear();
	return ret;
}

int32_t TemplateCache::Open(Plugin *plugin, ui::Widget *parent, const Plugin::TemplateOpenParam &param)
{
	if (!m_created)
		return SCE_PAF_ERROR_FAILED;
	if (plugin == NULL || parent == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;

	int32_t ret = 0;

	for (list<Item *>::iterator it = m_items.begin(); it != m_items.end(); it++) {
		ret = (*it)->Open(plugin, parent, param.event_handler_table, param.sibling);
		if (ret != 0)
			break;
	}
	return ret;
}

int32_t TemplateCache::Item::Open(Plugin *plugin, ui::Widget *parent,
				  InitEventFunctionEntry2 *event_handler_table, ui::Widget *sibling)
{
	if (!m_created)
		return SCE_PAF_ERROR_FAILED;
	if (m_template != NULL) {
		Plugin::TemplateOpenParam param;

		param.sibling = sibling;
		param.event_handler_table = event_handler_table;
		return m_template->Open(plugin, parent, param);
	}

	ui::Widget *widget = m_factory.m_factory_widget(parent);

	if (widget == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	widget->m_name = m_id;
	m_style_param->AddWidget(widget);
	m_style_param->SetEach(widget);

	if (m_layout.doc != NULL && m_layout.offset >= 0) {
		if (m_layout_param != NULL) {
			m_layout_param->m_widget = widget;
			m_layout_param->Set();
			m_layout_param->m_widget = NULL;
		} else {
			ui::WidgetLayoutParam *param =
				static_cast<ui::WidgetLayoutParam *>(m_factory.m_factory_layout());

			if (param != NULL) {
				param->m_widget = widget;
				param->Get();
				param->InitResolution(m_layout.GetNumChild());
				param->Create(1);
				param->Read(&m_layout);
				param->Set();
				param->Destroy(1);
				param->m_widget = NULL;
				m_layout_param = param;
			}
		}
	}
	if (m_focus.doc != NULL && m_focus.offset >= 0) {
		if (m_focus_param != NULL) {
			m_focus_param->m_widget = widget;
			m_focus_param->Set();
			m_focus_param->m_widget = NULL;
		} else {
			ui::WidgetFocusParam *param = static_cast<ui::WidgetFocusParam *>(
				m_factory.m_factory_resource_param());

			if (param != NULL) {
				param->m_widget = widget;
				param->m_plugin = plugin;
				param->Get();
				param->InitResolution(m_focus.GetNumChild());
				param->Create(1);
				param->Read(&m_focus);
				param->Set();
				param->Destroy(1);
				param->m_widget = NULL;
				m_focus_param = param;
			}
		}
	}
	plugin->InstantiateWidget_InitEventHandler(m_id, widget, event_handler_table);
	for (list<Item *>::iterator it = m_children.begin(); it != m_children.end(); it++) {
		if ((*it)->Open(plugin, widget->GetInternalWidget(), event_handler_table, sibling) != 0)
			break;
	}
	if (m_wparam != NULL) {
		m_wparam->m_widget = widget;
		m_wparam->Set();
		m_wparam->m_widget = NULL;
	}
	if (m_sound_param != NULL) {
		m_sound_param->AddWidget(widget);
		if (widget != NULL) {
			widget->m_sound_num = m_sound_param->m_sound_list.size();
			widget->m_sounds = m_sound_param->m_sound_list.ptr;
		}
	}
	return 0;
}

}
