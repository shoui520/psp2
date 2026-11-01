/*
 * paf::VersatileParam: a parameter object of a resource element, whose
 * ResourceParam reads its fields from the element and writes them back.
 */
#include <stddef.h>
#include <stdint.h>
#include <cxml/cxml.h>
#include <file/file.h>
#include <graphics/drawobj/draw_obj.h>
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

}
