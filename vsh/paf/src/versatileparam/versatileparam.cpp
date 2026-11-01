/*
 * paf::VersatileParam: a parameter object of a resource element, whose
 * ResourceParam reads its fields from the element and writes them back.
 */
#include <stddef.h>
#include <stdint.h>
#include <cxml/cxml.h>
#include <file/file.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <versatileparam/resourceparam.h>
#include <versatileparam/versatileparam.h>

namespace paf {

const char VersatileParam::m_param_name[] = "resource_base";

const char *VersatileParam::GetParamName() const
{
	return m_param_name;
}

const char *VersatileParam::ParamName()
{
	return m_param_name;
}

VersatileParam::VersatileParam() : m_serial(NULL), m_plugin(NULL), m_id(s_empty_name, 0)
{
	m_node = new cxml::Element;
}

VersatileParam::~VersatileParam()
{
	Destroy(-1);
	delete m_node;
}

/* the subclasses' Create call it whole */
__attribute__((noinline)) int32_t VersatileParam::Create(int32_t mode)
{
	ResourceParam *serial = m_serial;

	if (mode & 1) {
		delete serial;
		serial = new ResourceParam;
		m_serial = serial;
	}
	if (serial != NULL) {
		serial->SetTagName(GetParamName());
		if (m_serial != NULL)
			m_serial->SetIDName(m_id.m_id.c_str());
	}
	return 0;
}

int32_t VersatileParam::Destroy(int32_t mode)
{
	if (mode & 1) {
		delete m_serial;
		m_serial = NULL;
	}
	return 0;
}

int32_t VersatileParam::OnUpdate()
{
	if (m_serial != NULL) {
		if (m_serial->GetDirty())
			Set();
		m_serial->SetDirty(false);
	}
	return 0;
}

void VersatileParam::OnDraw()
{
}

int32_t VersatileParam::Read(const cxml::Element *elem)
{
	if (m_serial == NULL)
		return -1;

	resource::ResourceObj *rscobj = NULL;

	if (m_plugin != NULL)
		rscobj = m_plugin->m_rscobj;
	if (m_serial->Read(*elem, rscobj, ResourceParam::READTYPE_FROM_PARAMLIST))
		return 0;
	return -1;
}

int32_t VersatileParam::Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent)
{
	if (m_serial == NULL)
		return -1;

	int32_t part;

	switch (type) {
	case 1:
		part = 1;
		break;
	case 2:
		part = 2;
		break;
	default:
		part = 0;
		break;
	}
	if (fp.get() != NULL)
		return ScePafResource_441E989F(m_serial, fp, part, indent, false);
	return ScePafResource_441E989F(m_serial, fp, part, indent, true);
}


namespace graph {

const char DrawObjParam::m_param_name[] = "drawobj";

const char *DrawObjParam::s_names[] = {
	"color",
	"blend",
	"deepen_color",
	"reduce_transparency",
};

const char *DrawObjParam::GetParamName() const
{
	return m_param_name;
}

const char *DrawObjParam::ParamName()
{
	return m_param_name;
}

int32_t DrawObjParam::Init()
{
	m_color = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	m_blend = 1;
	m_deepen_color = -1;
	m_reduce_transparency = -1;
	return 0;
}

int32_t DrawObjParam::Set()
{
	if (m_drawobj_list.size() == 0)
		return -1;
	for (list<DrawObj *>::iterator it = m_drawobj_list.begin();
	     it != m_drawobj_list.end(); ++it)
		SetEach(*it);
	return 0;
}

int32_t DrawObjParam::Get()
{
	if (m_drawobj_list.size() == 0)
		return -1;

	DrawObj *obj = *m_drawobj_list.begin();

	m_color = obj->GetColor();
	GetEach(obj);
	return 0;
}

int32_t DrawObjParam::Create(int32_t mode)
{
	VersatileParam::Create(mode);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[0], &m_color, ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[1], &m_blend, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[2], &m_deepen_color, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[3], &m_reduce_transparency, ResourceParam::SEMANTIC_BOOL);
	return 0;
}

int32_t DrawObjParam::SetEach(DrawObj *obj)
{
	if (obj == NULL)
		return -1;

	obj->SetColor(m_color);

	DrawStyle style = obj->m_draw_style;

	style.blend = m_blend;
	if (m_deepen_color != -1)
		style.deepen_color = m_deepen_color;
	if (m_reduce_transparency != -1)
		style.reduce_transparency = m_reduce_transparency;
	obj->m_draw_style = style;
	return 0;
}

int32_t DrawObjParam::GetEach(DrawObj *obj)
{
	if (obj == NULL)
		return -1;

	m_color = obj->GetColor();

	DrawStyle style = obj->m_draw_style;

	m_blend = style.blend;
	m_deepen_color = style.deepen_color;
	m_reduce_transparency = style.reduce_transparency;
	return 0;
}

int32_t DrawObjParam::AddDrawObj(DrawObj *obj)
{
	if (obj == NULL)
		return -1;
	for (list<DrawObj *>::iterator it = m_drawobj_list.begin();
	     it != m_drawobj_list.end(); ++it) {
		if (*it == obj)
			return 0;
	}
	m_drawobj_list.push_back(obj);

	DrawObj *added = obj;

	if (added != NULL) {
		if (added->m_delete_cb[0].func == NULL) {
			added->m_delete_cb[0].func = delete_cb;
			added->m_delete_cb[0].data = this;
		} else if (added->m_delete_cb[1].func == NULL) {
			added->m_delete_cb[1].func = delete_cb;
			added->m_delete_cb[1].data = this;
		}
	}
	return 0;
}

int32_t DrawObjParam::DelDrawObj(DrawObj *obj)
{
	if (obj == NULL)
		return -1;
	for (list<DrawObj *>::iterator it = m_drawobj_list.begin();
	     it != m_drawobj_list.end(); ++it) {
		if (*it != obj)
			continue;
		m_drawobj_list.erase(it);
		if (obj != NULL) {
			if (obj->m_delete_cb[0].func == delete_cb && obj->m_delete_cb[0].data == this) {
				obj->m_delete_cb[0].func = NULL;
				obj->m_delete_cb[0].data = NULL;
			} else if (obj->m_delete_cb[1].func == delete_cb &&
				   obj->m_delete_cb[1].data == this) {
				obj->m_delete_cb[1].func = NULL;
				obj->m_delete_cb[1].data = NULL;
			}
		}
		return 0;
	}
	return -1;
}

void DrawObjParam::delete_cb(DrawObj *obj, void *param)
{
	static_cast<DrawObjParam *>(param)->DelDrawObj(obj);
}

}


namespace ui {

const char WidgetParam::m_param_name[] = "widget_base";

const char *WidgetParam::s_names[] = {
	"style",
	"texture",
	"sound",
};

const char *WidgetParam::GetParamName() const
{
	return m_param_name;
}

const char *WidgetParam::ParamName()
{
	return m_param_name;
}

int32_t WidgetParam::Init()
{
	m_style_id.m_idhash = 0;
	m_texture_id.m_idhash = 0;
	m_sound_id.m_idhash = 0;
	for (list<IndexedRef>::iterator it = m_texture_list.begin(); it != m_texture_list.end(); ++it)
		it->id.m_idhash = 0;
	for (list<IndexedRef>::iterator it = m_string_list.begin(); it != m_string_list.end(); ++it)
		it->id.m_idhash = 0;
	for (list<IndexedRef>::iterator it = m_alias_string_list.begin();
	     it != m_alias_string_list.end(); ++it)
		it->id.m_idhash = 0;
	return 0;
}

const wchar_t *WidgetParam::GetString(const IDParam &id)
{
	if (m_plugin == NULL)
		return NULL;
	return m_plugin->GetString(IDParam(s_empty_id, id.m_idhash));
}

}
}

/* the texture of the ID from the parameter's plugin, NULL without one; Set calls it whole */
__attribute__((noinline)) paf::intrusive_ptr<paf::graph::Surface> ScePafResource_31C3B828(paf::ui::WidgetParam *param,
								 const paf::IDParam &id)
{
	if (param->m_plugin == NULL)
		return paf::intrusive_ptr<paf::graph::Surface>();
	return param->m_plugin->GetTexture(paf::IDParam(paf::s_empty_id, id.m_idhash));
}

namespace paf {
namespace ui {

int32_t WidgetParam::Set()
{
	Widget *widget = m_widget;

	if (widget == NULL)
		return -1;
	if (m_texture_id.m_idhash != 0)
		widget->SetTexture(ScePafResource_31C3B828(this, m_texture_id));
	for (list<IndexedRef>::iterator it = m_texture_list.begin(); it != m_texture_list.end(); ++it) {
		if (it->id.m_idhash != 0)
			m_widget->SetTexture(ScePafResource_31C3B828(this, it->id), it->index, 0);
	}
	for (list<IndexedRef>::iterator it = m_string_list.begin(); it != m_string_list.end(); ++it) {
		if (it->id.m_idhash != 0)
			m_widget->SetString(wstring(GetString(it->id)), it->index);
	}
	for (list<IndexedRef>::iterator it = m_alias_string_list.begin();
	     it != m_alias_string_list.end(); ++it) {
		if (it->id.m_idhash != 0)
			m_widget->SetString(wstring(m_plugin != NULL ?
						    ScePafToplevel_D8801CA4(m_plugin, it->id) : NULL),
					    it->index);
	}
	return 0;
}

int32_t WidgetParam::Get()
{
	return m_widget != NULL ? 0 : -1;
}

int32_t WidgetParam::Create(int32_t mode)
{
	VersatileParam::Create(mode);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[0], &m_style_id, ResourceParam::SEMANTIC_STYLEREF);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[1], &m_texture_id, ResourceParam::SEMANTIC_TEXTUREREF);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[2], &m_sound_id, ResourceParam::SEMANTIC_SOUNDREF);
	for (list<IndexedRef>::iterator it = m_texture_list.begin(); it != m_texture_list.end(); ++it) {
		if (m_serial != NULL)
			m_serial->AddParam(it->name, &it->id, ResourceParam::SEMANTIC_TEXTUREREF);
	}
	for (list<IndexedRef>::iterator it = m_string_list.begin(); it != m_string_list.end(); ++it) {
		if (m_serial != NULL)
			m_serial->AddParam(it->name, &it->id, ResourceParam::SEMANTIC_LABELREF);
	}
	for (list<IndexedRef>::iterator it = m_alias_string_list.begin();
	     it != m_alias_string_list.end(); ++it) {
		if (m_serial != NULL)
			m_serial->AddParam(it->name, &it->id, ResourceParam::SEMANTIC_LABELREF);
	}
	return 0;
}

int32_t WidgetParam::Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent)
{
	if (m_widget == NULL)
		return -1;
	if (m_serial != NULL)
		m_serial->SetTagName(m_widget->GetTypeName());
	return VersatileParam::Write(fp, type, indent);
}

}

}

/* attaches the parameter to the widget, dropping its delete callback from the old one */
int32_t ScePafResource_190E99A6(paf::ui::WidgetParam *param, paf::ui::Widget *widget)
{
	if (param->m_widget == widget)
		return 0;
	if (param->m_widget != NULL)
		param->m_widget->RemoveDeleteCB(ScePafResource_2B873BCD, param);
	param->m_widget = widget;
	return 0;
}

/* the old widget's delete callback: the parameter forgets it */
void ScePafResource_2B873BCD(paf::ui::Widget *widget, void *param)
{
	ScePafResource_190E99A6(static_cast<paf::ui::WidgetParam *>(param), NULL);
}
