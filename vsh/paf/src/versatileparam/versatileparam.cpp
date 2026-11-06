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
#include <widget/core/environment.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <plugin.h>
#include <std/algorithm>
#include <std/functional>
#include <std/string.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <versatileparam/resourceparam.h>
#include <versatileparam/versatileparam.h>

namespace paf {

/*
 * The ranges a debug view gives a widget's position, size and rotation, which the release
 * build never reads (names ours).
 */
static inline __attribute__((always_inline)) math::v3 Range3(float x, float y, float z)
{
	return math::v3(x, y, z);
}

static inline __attribute__((always_inline)) math::v4 Range4(float x, float y, float z, float w)
{
	return math::v4(x, y, z, w);
}

static const math::v3 s_pos_min = Range3(-500.0f, -500.0f, -500.0f);
static const math::v3 s_pos_max = Range3(500.0f, 500.0f, 500.0f);
static const math::v3 s_size_min = Range3(20.0f, 20.0f, 0.0f);
static const math::v3 s_size_max = Range3(1000.0f, 1000.0f, 1000.0f);
static const math::v4 s_size_max4 = Range4(20.0f, 20.0f, 1000.0f, 1000.0f);
static const math::v3 s_rot_min = Range3(-3.14159265f, -3.14159265f, -3.14159265f);
static const math::v3 s_rot_max = Range3(3.14159265f, 3.14159265f, 3.14159265f);
static const math::v4 s_rect_min = Range4(-500.0f, -500.0f, 0.0f, 0.0f);
static const math::v4 s_rect_max = Range4(500.0f, 500.0f, 1000.0f, 1000.0f);

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

namespace paf {
namespace ui {

const char WidgetLayoutParam::m_param_name[] = "layout_hint";

const char *WidgetLayoutParam::s_names[] = {
	"pos",
	"size",
	"rot",
	"alpha",
	"align",
	"adjust",
	"origin",
	"anchor",
};

const char *WidgetLayoutParam::GetParamName() const
{
	return m_param_name;
}

const char *WidgetLayoutParam::ParamName()
{
	return m_param_name;
}

int32_t WidgetLayoutParam::Init()
{
	m_pos = math::v3(0.0f, 0.0f, 0.0f);
	m_size = math::v3(0.0f, 0.0f, 0.0f);
	m_rot = math::v3(0.0f, 0.0f, 0.0f);
	m_alpha = 1.0f;
	m_align[0] = 0;
	m_align[1] = 0;
	m_align[2] = 0;
	m_adjust[0] = 0;
	m_adjust[1] = 0;
	m_adjust[2] = 0;
	m_origin[0] = 0;
	m_origin[1] = 0;
	m_origin[2] = 0;
	m_anchor[0] = 4;
	m_anchor[1] = 4;
	m_anchor[2] = 4;
	delete m_timer;
	m_timer = NULL;
	if (m_pos_array != NULL)
		delete[] m_pos_array;
	if (m_size_array != NULL)
		delete[] m_size_array;
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	m_pos_array = NULL;
	m_size_array = NULL;
	m_rot_array = NULL;
	m_resolution_num = 0;
	m_resolution_id = 255;
	m_base_resolution_id = -1;
	return 0;
}

WidgetLayoutParam::~WidgetLayoutParam()
{
	if (m_pos_array != NULL)
		delete[] m_pos_array;
	if (m_size_array != NULL)
		delete[] m_size_array;
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	delete m_timer;
	if (m_widget != NULL)
		m_widget->RemoveDeleteCB(delete_cb, this);
}

int32_t WidgetLayoutParam::Set()
{
	if (m_widget == NULL)
		return -1;

	if (m_resolution_num == 0) {
		ResolutionData data;

		data.m_id = -1;
		data.m_value[0] = m_pos.extract_x();
		data.m_value[1] = m_pos.extract_y();
		data.m_value[2] = m_pos.extract_z();
		m_widget->SetPos(&data, 1, m_timer != NULL ? new Timer(*m_timer) : NULL,
				 Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		data.m_value[0] = m_size.extract_x();
		data.m_value[1] = m_size.extract_y();
		data.m_value[2] = m_size.extract_z();
		m_widget->SetSize(&data, 1, m_timer != NULL ? new Timer(*m_timer) : NULL,
				  Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		data.m_value[0] = m_rot.extract_x();
		data.m_value[1] = m_rot.extract_y();
		data.m_value[2] = m_rot.extract_z();
		m_widget->SetRot(&data, 1, 0, m_timer != NULL ? new Timer(*m_timer) : NULL,
				 Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
	} else {
		uint32_t num = m_resolution_num + 1;
		ResolutionData *data = new ResolutionData[num];

		data[0].m_id = -1;
		math::v3 pos = m_pos;

		if (m_pos_array != NULL) {
			data[0].m_value[0] = pos.extract_x();
			data[0].m_value[1] = pos.extract_y();
			data[0].m_value[2] = pos.extract_z();
			sce_paf_memcpy(data + 1, m_pos_array, m_resolution_num * sizeof(ResolutionData));
			m_widget->SetPos(data, num, m_timer != NULL ? new Timer(*m_timer) : NULL,
					 Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		} else {
			m_widget->SetPos(math::v4(sce::Vectormath::Simd::Aos::Vector4(pos.getImpl(), 0.0f)),
					 m_timer != NULL ? new Timer(*m_timer) : NULL, Widget::ANIM_MOVE, 0,
					 0.0f, NULL, NULL);
		}
		math::v3 size = m_size;

		if (m_size_array != NULL) {
			data[0].m_value[0] = size.extract_x();
			data[0].m_value[1] = size.extract_y();
			data[0].m_value[2] = size.extract_z();
			sce_paf_memcpy(data + 1, m_size_array, m_resolution_num * sizeof(ResolutionData));
			m_widget->SetSize(data, num, m_timer != NULL ? new Timer(*m_timer) : NULL,
					  Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		} else {
			m_widget->SetSize(math::v4(sce::Vectormath::Simd::Aos::Vector4(size.getImpl(), 0.0f)),
					  m_timer != NULL ? new Timer(*m_timer) : NULL, Widget::ANIM_SIZE, 0,
					  0.0f, NULL, NULL);
		}
		math::v3 rot = m_rot;

		if (m_rot_array != NULL) {
			data[0].m_value[0] = rot.extract_x();
			data[0].m_value[1] = rot.extract_y();
			data[0].m_value[2] = rot.extract_z();
			sce_paf_memcpy(data + 1, m_rot_array, m_resolution_num * sizeof(ResolutionData));
			m_widget->SetRot(data, num, 0, m_timer != NULL ? new Timer(*m_timer) : NULL,
					 Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
		} else {
			m_widget->SetRot(math::v4(sce::Vectormath::Simd::Aos::Vector4(rot.getImpl(), 0.0f)), 0,
					 m_timer != NULL ? new Timer(*m_timer) : NULL, Widget::ANIM_ROT, 0,
					 0.0f, NULL, NULL);
		}
		delete[] data;
	}
	m_widget->SetMetaAlpha(m_alpha, m_timer != NULL ? new Timer(*m_timer) : NULL,
			       Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	m_widget->SetAlign(m_align[0], m_align[1], m_align[2], NULL);
	m_widget->SetAdjust(m_adjust[0], m_adjust[1], m_adjust[2]);
	m_widget->SetOrigin(m_origin[0], m_origin[1], m_origin[2], NULL);
	m_widget->SetAnchor(m_anchor[0], m_anchor[1], m_anchor[2], NULL);
	return 0;
}

int32_t WidgetLayoutParam::GetEach(Widget *widget)
{
	if (widget == NULL || m_resolution_num <= 0)
		return 0;
	if (m_resolution_num >= widget->m_pos_num)
		sce_paf_memcpy(m_pos_array, widget->m_pos_array,
			       widget->m_pos_num * sizeof(ResolutionData));
	if (m_resolution_num >= widget->m_size_num)
		sce_paf_memcpy(m_size_array, widget->m_size_array,
			       widget->m_size_num * sizeof(ResolutionData));
	if (m_resolution_num >= widget->m_rot_num)
		sce_paf_memcpy(m_rot_array, widget->m_rot_array,
			       widget->m_rot_num * sizeof(ResolutionData));
	uint32_t resolution_id = widget->m_resolution_id;
	Scene *scene = m_widget->m_scene;

	m_resolution_id = resolution_id;
	if (scene != NULL) {
		SceneResolution *resolution =
			ScePafWidget_CDC90BA2(scene->m_env_param, resolution_id);

		if (resolution != NULL)
			m_base_resolution_id = resolution->m_base_id;
		else
			m_base_resolution_id = -1;
	}
	return 0;
}

int32_t WidgetLayoutParam::Get()
{
	if (m_widget == NULL)
		return -1;

	m_pos = math::v3(m_widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				 .getImpl().getXYZ());
	m_size = math::v3(m_widget->GetSize(0)->getImpl().getXYZ());
	m_rot = math::v3(m_widget->GetRot(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0)
				 .getImpl().getXYZ());
	m_alpha = m_widget->m_meta_alpha;

	uint32_t x, y, z;

	x = m_widget->m_align_x;
	y = m_widget->m_align_y;
	z = m_widget->m_align_z;
	m_align[0] = x;
	m_align[1] = y;
	m_align[2] = z;
	x = m_widget->m_adjust_x;
	y = m_widget->m_adjust_y;
	z = m_widget->m_adjust_z;
	m_adjust[0] = x;
	m_adjust[1] = y;
	m_adjust[2] = z;
	m_widget->GetOrigin(x, y, z);
	m_origin[0] = x;
	m_origin[1] = y;
	m_origin[2] = z;
	m_widget->GetAnchor(x, y, z);
	m_anchor[0] = x;
	m_anchor[1] = y;
	m_anchor[2] = z;
	GetEach(m_widget);
	return 0;
}

int32_t WidgetLayoutParam::Create(int32_t mode)
{
	VersatileParam::Create(mode);
	if ((mode & 1) || m_resolution_num == 0) {
		if (m_serial != NULL)
			m_serial->AddParam(s_names[0], &m_pos, ResourceParam::SEMANTIC_VEC3);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[1], &m_size, ResourceParam::SEMANTIC_VEC3);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[2], &m_rot, ResourceParam::SEMANTIC_VEC3);
	}
	if (m_resolution_num > 0) {
		if (m_serial != NULL)
			m_serial->AddParam(s_names[0], m_pos_array, ResourceParam::SEMANTIC_RESOARRAY);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[1], m_size_array, ResourceParam::SEMANTIC_RESOARRAY);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[2], m_rot_array, ResourceParam::SEMANTIC_RESOARRAY);
	}
	if (m_serial != NULL)
		m_serial->AddParam(s_names[3], &m_alpha, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[4], m_align, ResourceParam::SEMANTIC_INT3);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[5], m_adjust, ResourceParam::SEMANTIC_INT3);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[6], m_origin, ResourceParam::SEMANTIC_INT3);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[7], m_anchor, ResourceParam::SEMANTIC_INT3);
	return 0;
}

/*
 * After the attributes, turns the resolution hashes of the "resolution"
 * children into resolution IDs: an entry without one takes the ID its
 * position's, then its rotation's hash found; an array with no entry is freed.
 */
int32_t WidgetLayoutParam::Read(const cxml::Element *elem)
{
	if (m_serial != NULL)
		m_serial->Read(*elem, m_plugin != NULL ? m_plugin->m_rscobj : NULL,
			       ResourceParam::READTYPE_FROM_PARAMLIST);
	if (m_resolution_num <= 0 || m_widget == NULL)
		return 0;

	Scene *scene = m_widget->m_scene;

	if (scene == NULL)
		return 0;

	int32_t rot_empty = 0;
	int32_t pos_empty = 0;
	int32_t size_empty = 0;

	for (int32_t i = 0; i < m_resolution_num; i++) {
		IDParam id(s_empty_id, m_pos_array[i].m_id);
		int32_t resolution_id;

		if (m_pos_array[i].m_id != 0) {
			resolution_id = ScePafWidget_36505C86(scene->m_env_param, id);
			m_pos_array[i].m_id = resolution_id < 0 ? -1 : resolution_id;
		} else {
			resolution_id = -1;
			m_pos_array[i].m_id = -1;
			m_pos_array[i].m_value[0] = 0.0f;
			m_pos_array[i].m_value[1] = 0.0f;
			m_pos_array[i].m_value[2] = 0.0f;
			pos_empty++;
		}
		if (m_rot_array[i].m_id != 0) {
			if (m_pos_array[i].m_id == (uint32_t)-1) {
				id.m_idhash = m_rot_array[i].m_id;
				resolution_id = ScePafWidget_36505C86(scene->m_env_param, id);
			}
			m_rot_array[i].m_id = resolution_id < 0 ? -1 : resolution_id;
		} else {
			m_rot_array[i].m_id = -1;
			m_rot_array[i].m_value[0] = 0.0f;
			m_rot_array[i].m_value[1] = 0.0f;
			m_rot_array[i].m_value[2] = 0.0f;
			rot_empty++;
		}
		if (m_size_array[i].m_id != 0) {
			if (m_pos_array[i].m_id == (uint32_t)-1 && m_rot_array[i].m_id == (uint32_t)-1) {
				id.m_idhash = m_size_array[i].m_id;
				resolution_id = ScePafWidget_36505C86(scene->m_env_param, id);
			}
			m_size_array[i].m_id = resolution_id < 0 ? -1 : resolution_id;
		} else {
			m_size_array[i].m_id = -1;
			m_size_array[i].m_value[0] = 0.0f;
			m_size_array[i].m_value[1] = 0.0f;
			m_size_array[i].m_value[2] = 0.0f;
			size_empty++;
		}
	}
	if (pos_empty == m_resolution_num) {
		delete[] m_pos_array;
		m_pos_array = NULL;
	}
	if (rot_empty == m_resolution_num) {
		delete[] m_rot_array;
		m_rot_array = NULL;
	}
	if (size_empty == m_resolution_num) {
		delete[] m_size_array;
		m_size_array = NULL;
	}
	return 0;
}

/*
 * With resolutions: the element's opening tag with its alpha, align, adjust,
 * origin and anchor, a "resolution" child for each resolution with its
 * position, size and rotation, then the closing tag.
 */
int32_t WidgetLayoutParam::Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent)
{
	if (m_resolution_num == 0) {
		if (fp.get() != NULL)
			return ScePafResource_441E989F(m_serial, fp, 0, indent, false);
		return ScePafResource_441E989F(m_serial, fp, 0, indent, true);
	}

	int32_t ret = 0;
	ResourceParam *param = new ResourceParam();

	if (param != NULL) {
		param->SetTagName(GetParamName());
		param->SetIDName(m_id.m_id.c_str());
		param->AddParam(s_names[3], &m_alpha, ResourceParam::SEMANTIC_FLOAT);
		param->AddParam(s_names[4], m_align, ResourceParam::SEMANTIC_INT3);
		param->AddParam(s_names[5], m_adjust, ResourceParam::SEMANTIC_INT3);
		param->AddParam(s_names[6], m_origin, ResourceParam::SEMANTIC_INT3);
		param->AddParam(s_names[7], m_anchor, ResourceParam::SEMANTIC_INT3);
		if (fp.get() != NULL)
			ScePafResource_441E989F(param, fp, 1, indent, false);
		else
			ScePafResource_441E989F(param, fp, 1, indent, true);
		delete param;
	}
	for (int32_t i = 0; i < m_resolution_num; i++) {
		if (m_pos_array[i].m_id == (uint32_t)-1)
			continue;

		Scene *scene = m_widget->m_scene;

		if (scene == NULL)
			continue;

		ResolutionResource resource =
			ScePafWidget_E963EAEE(scene->m_env_param, m_pos_array[i].m_id);
		ResourceParam *child = new ResourceParam();

		if (child != NULL) {
			child->SetTagName("resolution");
			child->SetIDName(resource.m_name.c_str());
			child->AddParam(s_names[0], &m_pos_array[i], ResourceParam::SEMANTIC_RESOARRAY);
			child->AddParam(s_names[1], &m_size_array[i], ResourceParam::SEMANTIC_RESOARRAY);
			child->AddParam(s_names[2], &m_rot_array[i], ResourceParam::SEMANTIC_RESOARRAY);
			if (fp.get() != NULL)
				ScePafResource_441E989F(child, fp, 0, indent + 1, false);
			else
				ScePafResource_441E989F(child, fp, 0, indent + 1, true);
			delete child;
		}
	}

	int32_t res;

	if (fp.get() != NULL)
		res = ScePafResource_441E989F(m_serial, fp, 2, indent, false);
	else
		res = ScePafResource_441E989F(m_serial, fp, 2, indent, true);
	if (res != 0)
		ret = res;
	return ret;
}

int32_t WidgetLayoutParam::InitResolution(int32_t num)
{
	if (m_pos_array != NULL)
		delete[] m_pos_array;
	if (m_size_array != NULL)
		delete[] m_size_array;
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	m_pos_array = NULL;
	m_size_array = NULL;
	m_rot_array = NULL;
	m_resolution_num = 0;
	if (num > 0) {
		m_pos_array = new ResolutionData[num];
		m_size_array = new ResolutionData[num];
		m_rot_array = new ResolutionData[num];
		sce_paf_memset(m_pos_array, 0, num * sizeof(ResolutionData));
		sce_paf_memset(m_size_array, 0, num * sizeof(ResolutionData));
		sce_paf_memset(m_rot_array, 0, num * sizeof(ResolutionData));
		m_resolution_num = num;
		m_resolution_id = 255;
		m_base_resolution_id = -1;
	}
	return 0;
}

int32_t WidgetLayoutParam::SetTimer(Timer *timer)
{
	delete m_timer;
	m_timer = timer != NULL ? new Timer(*timer) : NULL;
	return 0;
}

int32_t WidgetLayoutParam::Attach(Widget *widget)
{
	if (m_widget == widget)
		return 0;
	if (m_widget != NULL)
		m_widget->RemoveDeleteCB(delete_cb, this);
	Init();
	m_widget = widget;
	if (widget != NULL)
		widget->AddDeleteCB(delete_cb, this);
	return Get();
}

void WidgetLayoutParam::delete_cb(Widget *widget, void *param)
{
	static_cast<WidgetLayoutParam *>(param)->Attach(NULL);
}


const char *WidgetFocusParam::s_names[] = {
	"enable",
	"init",
	"direct_key",
	"up_id",
	"down_id",
	"left_id",
	"right_id",
	"focus_shape",
	"focus_offset",
	"focus_size",
	"focus_rot",
	"focus_texture",
	"search_rect",
};

const char *WidgetFocusParam::GetParamName() const
{
	return "focus_hint";
}

const char *WidgetFocusParam::ParamName()
{
	return "focus_hint";
}

int32_t WidgetFocusParam::Init()
{
	m_enable = true;
	m_init = false;
	m_direct_key = 0;
	m_up_id.m_idhash = 0;
	m_down_id.m_idhash = 0;
	m_left_id.m_idhash = 0;
	m_right_id.m_idhash = 0;
	m_focus_shape = 0;
	m_focus_offset = math::v3(0.0f, 0.0f, 0.0f);
	m_focus_size = math::v2(0.0f, 0.0f);
	m_focus_rot = math::v3(0.0f, 0.0f, 0.0f);
	m_focus_texture.m_idhash = 0;
	m_search_rect = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	if (m_offset_array != NULL)
		delete[] m_offset_array;
	if (m_size_array != NULL)
		delete[] m_size_array;
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	m_offset_array = NULL;
	m_size_array = NULL;
	m_rot_array = NULL;
	m_resolution_num = 0;
	m_resolution_id = 255;
	m_base_resolution_id = -1;
	return 0;
}

WidgetFocusParam::WidgetFocusParam()
	: m_widget(NULL), m_up_id(s_empty_name, 0), m_down_id(s_empty_name, 0),
	  m_left_id(s_empty_name, 0), m_right_id(s_empty_name, 0),
	  m_focus_texture(s_empty_name, 0), m_offset_array(NULL), m_size_array(NULL),
	  m_rot_array(NULL)
{
	Init();
}

WidgetFocusParam::~WidgetFocusParam()
{
	if (m_offset_array != NULL)
		delete[] m_offset_array;
	if (m_size_array != NULL)
		delete[] m_size_array;
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	if (m_widget != NULL)
		m_widget->RemoveDeleteCB(delete_cb, this);
}

/* the x, y and z, and the x and y, of a focus value as a resolution entry holds them */
struct FocusValue3 {
	float value[3];
};

struct FocusValue2 {
	float value[2];
};

/* the widget's array of a focus value: the entries of data replace its old ones */
static inline void SetFocusArray(vector<ResolutionData> &array, const ResolutionData *data,
				 uint32_t num)
{
	if (num == 0)
		return;
	array.clear();
	for (uint32_t i = 0; i < num; i++)
		array.push_back(data[i]);
}

/*
 * The widget's focus cursor takes the shape, and the offset, size and rotation
 * that are not all zero; with resolutions, each array that was read is the
 * value followed by its resolutions' entries.
 */
int32_t WidgetFocusParam::Set()
{
	if (m_enable)
		m_widget->EnableFocusEvent(true);
	else
		m_widget->EnableFocusEvent(false);
	if (m_direct_key != 0)
		ScePafWidget_21125870(m_widget, m_direct_key);
	if (m_up_id.m_idhash != 0)
		m_widget->SetFocusDirection(0, m_up_id);
	if (m_down_id.m_idhash != 0)
		m_widget->SetFocusDirection(1, m_down_id);
	if (m_left_id.m_idhash != 0)
		m_widget->SetFocusDirection(2, m_left_id);
	if (m_right_id.m_idhash != 0)
		m_widget->SetFocusDirection(3, m_right_id);

	FocusShape local;
	FocusShape *shape = m_widget->m_focus_shape;

	if (shape == NULL)
		shape = &local;
	if (m_focus_shape != 0)
		shape->m_shape = m_focus_shape;
	if (m_focus_offset == math::v3(0.0f, 0.0f, 0.0f))
		;
	else
		shape->m_offset = m_focus_offset;
	if (m_focus_size == math::v2(0.0f, 0.0f))
		;
	else
		shape->m_size = m_focus_size;
	if (m_focus_rot == math::v3(0.0f, 0.0f, 0.0f))
		;
	else
		shape->m_rot = m_focus_rot;
	if (m_plugin != NULL && m_focus_texture.m_idhash != 0)
		shape->m_texture = m_plugin->GetTexture(IDParam(s_empty_id, m_focus_texture.m_idhash));
	if (!(m_search_rect == math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
		math::v2 center(sce::Vectormath::Simd::Aos::Vector2(
			vget_low_f32(m_search_rect.getImpl().get128())));
		math::v2 size(sce::Vectormath::Simd::Aos::Vector2(
			vget_high_f32(m_search_rect.getImpl().get128())));

		m_widget->SetFocusSearchRect(math::rect(center, size));
	}
	if (m_resolution_num == 0) {
		ResolutionData data;

		data.m_id = -1;
		*reinterpret_cast<FocusValue3 *>(data.m_value) =
			*reinterpret_cast<const FocusValue3 *>(&m_focus_offset);
		SetFocusArray(shape->m_offset_array, &data, 1);
		*reinterpret_cast<FocusValue2 *>(data.m_value) =
			*reinterpret_cast<const FocusValue2 *>(&m_focus_size);
		data.m_value[2] = 0.0f;
		SetFocusArray(shape->m_size_array, &data, 1);
		*reinterpret_cast<FocusValue3 *>(data.m_value) =
			*reinterpret_cast<const FocusValue3 *>(&m_focus_rot);
		SetFocusArray(shape->m_rot_array, &data, 1);
	} else {
		uint32_t num = m_resolution_num + 1;
		ResolutionData *data = new ResolutionData[num];

		data[0].m_id = -1;
		if (m_offset_array != NULL) {
			*reinterpret_cast<FocusValue3 *>(data[0].m_value) =
				*reinterpret_cast<const FocusValue3 *>(&m_focus_offset);
			sce_paf_memcpy(data + 1, m_offset_array, m_resolution_num * sizeof(ResolutionData));
			SetFocusArray(shape->m_offset_array, data, num);
		}
		if (m_size_array != NULL) {
			*reinterpret_cast<FocusValue2 *>(data[0].m_value) =
				*reinterpret_cast<const FocusValue2 *>(&m_focus_size);
			data[0].m_value[2] = 0.0f;
			sce_paf_memcpy(data + 1, m_size_array, m_resolution_num * sizeof(ResolutionData));
			SetFocusArray(shape->m_size_array, data, num);
		}
		if (m_rot_array != NULL) {
			*reinterpret_cast<FocusValue3 *>(data[0].m_value) =
				*reinterpret_cast<const FocusValue3 *>(&m_focus_rot);
			sce_paf_memcpy(data + 1, m_rot_array, m_resolution_num * sizeof(ResolutionData));
			SetFocusArray(shape->m_rot_array, data, num);
		}
		delete[] data;
	}
	ScePafWidget_13B246E0(m_widget, shape);
	return 0;
}

/*
 * Each of the widget's focus arrays that has room in ours replaces it. Sony's
 * test for the rotation array looks at m_rot_array and frees m_size_array.
 */
int32_t WidgetFocusParam::GetEach(Widget *widget)
{
	if (widget == NULL || widget->m_focus_shape == NULL || m_resolution_num <= 0)
		return 0;

	const ResolutionData *src = widget->m_focus_shape->m_offset_array.ptr;
	int32_t num = widget->m_focus_shape->m_offset_array.count;

	if (src != NULL && m_resolution_num >= num) {
		size_t size = num * sizeof(ResolutionData);

		if (m_offset_array != NULL)
			delete[] m_offset_array;
		m_offset_array = new ResolutionData[num];
		sce_paf_memcpy(m_offset_array, src, size);
	}
	src = widget->m_focus_shape->m_size_array.ptr;
	num = widget->m_focus_shape->m_size_array.count;
	if (src != NULL && m_resolution_num >= num) {
		size_t size = num * sizeof(ResolutionData);

		if (m_size_array != NULL)
			delete[] m_size_array;
		m_size_array = new ResolutionData[num];
		sce_paf_memcpy(m_size_array, src, size);
	}
	src = widget->m_focus_shape->m_rot_array.ptr;
	num = widget->m_focus_shape->m_rot_array.count;
	if (src != NULL && m_resolution_num >= num) {
		size_t size = num * sizeof(ResolutionData);

		if (m_rot_array != NULL)
			delete[] m_size_array;
		m_rot_array = new ResolutionData[num];
		sce_paf_memcpy(m_rot_array, src, size);
	}
	return 0;
}

int32_t WidgetFocusParam::Get()
{
	if (m_widget == NULL)
		return -1;

	m_enable = (m_widget->m_event_enable & 0x400) != 0;
	m_init = m_widget->IsInitFocus();

	Widget *widget = m_widget;
	Focuser *focuser = widget->m_focuser;
	FocusShape *shape = widget->m_focus_shape;

	for (int32_t key = 1; key <= 0x8000; key <<= 1) {
		if (widget->m_bound_keycode && (widget->m_keycode & key) != 0)
			m_direct_key |= key;
	}
	if (shape != NULL) {
		m_focus_shape = shape->m_shape;
		m_focus_offset = shape->m_offset;
		m_focus_size = shape->m_size;
		m_focus_rot = shape->m_rot;
	}

	math::rect rect = focuser == NULL ? math::rect() :
		focuser->m_hint == NULL ? math::rect() : math::rect(focuser->m_hint->m_search_rect);

	m_search_rect = rect.get_xywh();
	GetEach(m_widget);
	return 0;
}

int32_t WidgetFocusParam::Create(int32_t mode)
{
	VersatileParam::Create(mode);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[0], &m_enable, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[1], &m_init, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[2], &m_direct_key, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[3], &m_up_id, ResourceParam::SEMANTIC_WIDGETREF);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[4], &m_down_id, ResourceParam::SEMANTIC_WIDGETREF);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[5], &m_left_id, ResourceParam::SEMANTIC_WIDGETREF);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[6], &m_right_id, ResourceParam::SEMANTIC_WIDGETREF);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[7], &m_focus_shape, ResourceParam::SEMANTIC_INT);
	if (m_resolution_num == 0) {
		if (m_serial != NULL)
			m_serial->AddParam(s_names[8], &m_focus_offset, ResourceParam::SEMANTIC_VEC3);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[9], &m_focus_size, ResourceParam::SEMANTIC_VEC2);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[10], &m_focus_rot, ResourceParam::SEMANTIC_VEC3);
	}
	if (m_resolution_num > 0) {
		if (m_serial != NULL)
			m_serial->AddParam(s_names[8], m_offset_array, ResourceParam::SEMANTIC_RESOARRAY);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[9], m_size_array, ResourceParam::SEMANTIC_RESOARRAY);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[10], m_rot_array, ResourceParam::SEMANTIC_RESOARRAY);
	}
	if (m_serial != NULL)
		m_serial->AddParam(s_names[11], &m_focus_texture, ResourceParam::SEMANTIC_TEXTUREREF);
	if (m_serial != NULL)
		m_serial->AddParam(s_names[12], &m_search_rect, ResourceParam::SEMANTIC_VEC4);
	return 0;
}

/*
 * As WidgetLayoutParam::Read, for the focus offset, rotation and size.
 */
int32_t WidgetFocusParam::Read(const cxml::Element *elem)
{
	if (m_serial != NULL)
		m_serial->Read(*elem, m_plugin != NULL ? m_plugin->m_rscobj : NULL,
			       ResourceParam::READTYPE_FROM_PARAMLIST);
	if (m_resolution_num <= 0 || m_widget == NULL)
		return 0;

	Scene *scene = m_widget->m_scene;

	if (scene == NULL)
		return 0;

	int32_t rot_empty = 0;
	int32_t offset_empty = 0;
	int32_t size_empty = 0;

	for (int32_t i = 0; i < m_resolution_num; i++) {
		IDParam id(s_empty_id, m_offset_array[i].m_id);
		int32_t resolution_id;

		if (m_offset_array[i].m_id != 0) {
			resolution_id = ScePafWidget_36505C86(scene->m_env_param, id);
			m_offset_array[i].m_id = resolution_id < 0 ? -1 : resolution_id;
		} else {
			resolution_id = -1;
			m_offset_array[i].m_id = -1;
			m_offset_array[i].m_value[0] = 0.0f;
			m_offset_array[i].m_value[1] = 0.0f;
			m_offset_array[i].m_value[2] = 0.0f;
			offset_empty++;
		}
		if (m_rot_array[i].m_id != 0) {
			if (m_offset_array[i].m_id == (uint32_t)-1) {
				id.m_idhash = m_rot_array[i].m_id;
				resolution_id = ScePafWidget_36505C86(scene->m_env_param, id);
			}
			m_rot_array[i].m_id = resolution_id < 0 ? -1 : resolution_id;
		} else {
			m_rot_array[i].m_id = -1;
			m_rot_array[i].m_value[0] = 0.0f;
			m_rot_array[i].m_value[1] = 0.0f;
			m_rot_array[i].m_value[2] = 0.0f;
			rot_empty++;
		}
		if (m_size_array[i].m_id != 0) {
			if (m_offset_array[i].m_id == (uint32_t)-1 && m_rot_array[i].m_id == (uint32_t)-1) {
				id.m_idhash = m_size_array[i].m_id;
				resolution_id = ScePafWidget_36505C86(scene->m_env_param, id);
			}
			m_size_array[i].m_id = resolution_id < 0 ? -1 : resolution_id;
		} else {
			m_size_array[i].m_id = -1;
			m_size_array[i].m_value[0] = 0.0f;
			m_size_array[i].m_value[1] = 0.0f;
			m_size_array[i].m_value[2] = 0.0f;
			size_empty++;
		}
	}
	if (offset_empty == m_resolution_num) {
		delete[] m_offset_array;
		m_offset_array = NULL;
	}
	if (rot_empty == m_resolution_num) {
		delete[] m_rot_array;
		m_rot_array = NULL;
	}
	if (size_empty == m_resolution_num) {
		delete[] m_size_array;
		m_size_array = NULL;
	}
	return 0;
}

int32_t WidgetFocusParam::InitResolution(int32_t num)
{
	if (m_offset_array != NULL)
		delete[] m_offset_array;
	if (m_size_array != NULL)
		delete[] m_size_array;
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	m_offset_array = NULL;
	m_size_array = NULL;
	m_rot_array = NULL;
	m_resolution_num = 0;
	if (num > 0) {
		m_offset_array = new ResolutionData[num];
		m_size_array = new ResolutionData[num];
		m_rot_array = new ResolutionData[num];
		sce_paf_memset(m_offset_array, 0, num * sizeof(ResolutionData));
		sce_paf_memset(m_size_array, 0, num * sizeof(ResolutionData));
		sce_paf_memset(m_rot_array, 0, num * sizeof(ResolutionData));
		m_resolution_num = num;
		m_resolution_id = 255;
		m_base_resolution_id = -1;
	}
	return 0;
}

int32_t WidgetFocusParam::Attach(Widget *widget)
{
	if (m_widget == widget)
		return 0;
	if (m_widget != NULL)
		m_widget->RemoveDeleteCB(delete_cb, this);
	Init();
	m_widget = widget;
	if (widget != NULL)
		widget->AddDeleteCB(delete_cb, this);
	return Get();
}

void WidgetFocusParam::delete_cb(Widget *widget, void *param)
{
	static_cast<WidgetFocusParam *>(param)->Attach(NULL);
}


const char WidgetStyleParam::m_param_name[] = "style_widget";

const char *const WidgetStyleParam::s_names[] = {
	"color",
	"sound",
};

const char *WidgetStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *WidgetStyleParam::ParamName()
{
	return m_param_name;
}

/* inline in the constructor */
__attribute__((always_inline)) int32_t WidgetStyleParam::Init()
{
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it)
		(*it)->Init();
	m_color = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	m_sound_id.m_idhash = 0;
	return 0;
}

WidgetStyleParam::WidgetStyleParam() : m_sound_id(s_empty_name, 0)
{
	m_widget_list.clear();
	m_drawobj_param_list.clear();
	Init();
}

int32_t WidgetStyleParam::Set()
{
	if (m_widget_list.size() == 0)
		return -1;
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it)
		SetEach(*it);
	return 0;
}

/* from the first widget that has the style */
int32_t WidgetStyleParam::Get()
{
	if (m_widget_list.size() == 0)
		return -1;
	GetEach(*m_widget_list.begin());
	return 0;
}

int32_t WidgetStyleParam::Create(int32_t mode)
{
	VersatileParam::Create(mode);
	if (m_serial != NULL) {
		m_serial->AddParam(s_names[0], &m_color, ResourceParam::SEMANTIC_VEC4);
		if (m_serial != NULL)
			m_serial->AddParam(s_names[1], &m_sound_id, ResourceParam::SEMANTIC_SOUNDREF);
	}
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it) {
		(*it)->Create(mode);
		if (m_serial != NULL)
			m_serial->AddParam((*it)->m_name, *it, ResourceParam::SEMANTIC_HASHREF);
	}
	return 0;
}

/* each draw object's parameter goes to the widget's draw object, then the color */
int32_t WidgetStyleParam::SetEach(Widget *widget)
{
	if (widget == NULL)
		return -1;
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it) {
		if (m_plugin != NULL)
			(*it)->AttachPlugin(m_plugin);

		graph::DrawObjParam *param = *it;

		param->SetEach(widget->GetDrawObj(param->m_index));
	}
	widget->SetColor(m_color);
	return 0;
}

int32_t WidgetStyleParam::GetEach(Widget *widget)
{
	if (widget == NULL)
		return -1;
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it) {
		graph::DrawObjParam *param = *it;

		param->GetEach(widget->GetDrawObj(param->m_index));
	}
	m_color = widget->GetColor();
	return 0;
}

int32_t WidgetStyleParam::Read(const cxml::Element *elem)
{
	if (m_serial != NULL) {
		resource::ResourceObj *rscobj = NULL;

		if (m_plugin != NULL)
			rscobj = m_plugin->m_rscobj;
		m_serial->Read(*elem, rscobj, ResourceParam::READTYPE_FROM_PARAMLIST);
	}
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it)
		(*it)->Read((*it)->m_node);
	return 0;
}

/*
 * With draw object parameters: the element's opening tag, each parameter's
 * element one level in, then the closing tag.
 */
int32_t WidgetStyleParam::Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent)
{
	if (m_drawobj_param_list.size() == 0) {
		if (fp.get() != NULL)
			return ScePafResource_441E989F(m_serial, fp, 0, indent, false);
		return ScePafResource_441E989F(m_serial, fp, 0, indent, true);
	}

	int32_t ret;

	if (fp.get() != NULL)
		ret = ScePafResource_441E989F(m_serial, fp, 1, indent, false);
	else
		ret = ScePafResource_441E989F(m_serial, fp, 1, indent, true);
	if (ret != 0)
		return ret;
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it) {
		ret = (*it)->Write(fp, type, indent + 1);
		if (ret != 0)
			return ret;
	}
	if (fp.get() != NULL)
		ret = ScePafResource_441E989F(m_serial, fp, 2, indent, false);
	else
		ret = ScePafResource_441E989F(m_serial, fp, 2, indent, true);
	if (ret != 0)
		return ret;
	return 0;
}

int32_t WidgetStyleParam::Destroy(int32_t mode)
{
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it)
		(*it)->Destroy(mode);
	if (mode & 1) {
		delete m_serial;
		m_serial = NULL;
	}
	return 0;
}

int32_t WidgetStyleParam::OnUpdate()
{
	if (m_serial != NULL) {
		if (m_serial->GetDirty())
			Set();
		m_serial->SetDirty(false);
	}
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it) {
		int32_t ret = (*it)->OnUpdate();

		if (ret != 0)
			return ret;
	}
	return 0;
}

void WidgetStyleParam::OnDraw()
{
	for_each(m_drawobj_param_list.begin(), m_drawobj_param_list.end(),
		 mem_fun(&graph::DrawObjParam::OnDraw));
}

int32_t WidgetStyleParam::AttachPlugin(Plugin *plugin)
{
	int32_t ret = 0;

	m_plugin = plugin;
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it) {
		ret = (*it)->AttachPlugin(plugin);
		if (ret != 0)
			break;
	}
	return ret;
}

vector<graph::DrawObjParam *>::iterator WidgetStyleParam::FindDrawObjParam(int32_t index)
{
	vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();

	for (; it != m_drawobj_param_list.end(); ++it) {
		if ((*it)->m_index == index)
			break;
	}
	return it;
}

graph::DrawObjParam *WidgetStyleParam::GetDrawObjParam(int32_t index)
{
	vector<graph::DrawObjParam *>::iterator it = FindDrawObjParam(index);

	if (it != m_drawobj_param_list.end())
		return *it;
	return NULL;
}

void WidgetStyleParam::SetDrawObjParam(const char *name, graph::DrawObjParam *param,
				       int32_t index)
{
	vector<graph::DrawObjParam *>::iterator it = FindDrawObjParam(index);

	if (it != m_drawobj_param_list.end()) {
		param->m_name = name;
		param->Init();
		param->m_index = index;
		*it = param;
	}
}

/* each draw object's parameter takes the widget's draw object */
int32_t WidgetStyleParam::AddWidget(Widget *widget)
{
	if (widget == NULL)
		return -1;
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it) {
		if (*it == widget)
			return 0;
	}
	m_widget_list.push_back(widget);
	if (widget != NULL)
		widget->AddDeleteCB(delete_cb, this);
	for (uint32_t i = 0; i < m_drawobj_param_list.size(); i++) {
		int32_t ret = m_drawobj_param_list[i]->AddDrawObj(widget->GetDrawObj(i));

		if (ret != 0)
			return ret;
	}
	return 0;
}

int32_t WidgetStyleParam::DelWidget(Widget *widget)
{
	if (widget == NULL)
		return -1;
	for (uint32_t i = 0; i < m_drawobj_param_list.size(); i++)
		m_drawobj_param_list[i]->DelDrawObj(widget->GetDrawObj(i));
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it) {
		if (*it == widget) {
			m_widget_list.erase(it);
			return 0;
		}
	}
	return -1;
}

void WidgetStyleParam::delete_cb(Widget *widget, void *param)
{
	static_cast<WidgetStyleParam *>(param)->DelWidget(widget);
}


const char WidgetHandlerParam::m_param_name[] = "handler";

const WidgetHandlerParam::EventList WidgetHandlerParam::c_eventList[] = {
	{ "on_init", 0x100001 },
	{ "on_prepare", 0x100003 },
	{ "on_ready", 0x100004 },
};

const char *WidgetHandlerParam::GetParamName() const
{
	return m_param_name;
}

const char *WidgetHandlerParam::ParamName()
{
	return m_param_name;
}

WidgetHandlerParam::WidgetHandlerParam() : m_widget(NULL), m_layouter(NULL)
{
	m_event_list.clear();
	for (uint32_t i = 0; i < sizeof(c_eventList) / sizeof(c_eventList[0]); i++) {
		EventEntry entry = { c_eventList[i].name, c_eventList[i].type, NULL };

		m_event_list.push_back(entry);
	}
}

int32_t WidgetHandlerParam::Init()
{
	m_layouter = NULL;
	for (list<EventEntry>::iterator it = m_event_list.begin(); it != m_event_list.end(); ++it)
		it->m_handler = NULL;
	return 0;
}

HandlerCB WidgetHandlerParam::GetHandler(const char *name)
{
	if (m_plugin != NULL)
		return m_plugin->GetEventHandler(name);
	return NULL;
}

/* the widget listens to the event with the plugin's handler of the name */
int32_t WidgetHandlerParam::SetHandler(int32_t type, const char *name)
{
	if (name == NULL)
		return -1;

	Widget *widget = m_widget;

	if (widget == NULL)
		return -1;
	if (m_plugin == NULL)
		return -1;

	HandlerCB func = m_plugin->GetEventHandler(name);

	widget->AddEventListener(type, new EventCBListener(func, NULL), false);
	return 0;
}

/* the widget takes the layout handler and the event handlers that are named */
int32_t WidgetHandlerParam::Set()
{
	if (m_widget == NULL)
		return -1;
	if (m_plugin == NULL)
		return -1;
	if (m_layouter != NULL) {
		LayoutCB cb = m_plugin->GetLayoutHandler(m_layouter);

		m_widget->SetLayouter(new Layouter(cb, NULL));
	}
	for (list<EventEntry>::iterator it = m_event_list.begin(); it != m_event_list.end(); ++it)
		SetHandler(it->m_type, it->m_handler);
	return 0;
}

int32_t WidgetHandlerParam::Get()
{
	if (m_widget == NULL || m_plugin == NULL)
		return -1;
	return 0;
}

int32_t WidgetHandlerParam::Create(int32_t mode)
{
	VersatileParam::Create(mode);
	if (m_serial != NULL)
		m_serial->AddParam("layouter", &m_layouter, ResourceParam::SEMANTIC_STRING);
	for (list<EventEntry>::iterator it = m_event_list.begin(); it != m_event_list.end(); ++it) {
		if (m_serial != NULL)
			m_serial->AddParam(it->m_name, &it->m_handler, ResourceParam::SEMANTIC_STRING);
	}
	return 0;
}

int32_t WidgetHandlerParam::AttachWidget(Widget *widget)
{
	if (m_widget != widget) {
		if (m_widget != NULL)
			m_widget->RemoveDeleteCB(delete_cb, this);
		m_widget = widget;
		if (widget != NULL)
			widget->AddDeleteCB(delete_cb, this);
	}
	return 0;
}

void WidgetHandlerParam::delete_cb(Widget *widget, void *param)
{
	static_cast<WidgetHandlerParam *>(param)->AttachWidget(NULL);
}


/* Sony's copy of the handler parameter's name */
const char WidgetSoundParam::m_param_name[] = "handler";

const char *WidgetSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *WidgetSoundParam::ParamName()
{
	return m_param_name;
}

WidgetSoundParam::WidgetSoundParam()
{
	m_widget_list.clear();
	m_sound_list.clear();
}

int32_t WidgetSoundParam::Init()
{
	for (vector<Sound>::iterator it = m_sound_list.begin(); it != m_sound_list.end(); ++it)
		it->m_handle = -1;
	return 0;
}

/* each widget takes the sounds */
int32_t WidgetSoundParam::Set()
{
	if (m_widget_list.size() == 0)
		return -1;
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it) {
		Widget *widget = *it;

		if (widget != NULL) {
			widget->m_sound_num = m_sound_list.size();
			widget->m_sounds = m_sound_list.ptr;
		}
	}
	return 0;
}

int32_t WidgetSoundParam::Create(int32_t mode)
{
	VersatileParam::Create(mode);
	for (vector<Sound>::iterator it = m_sound_list.begin(); it != m_sound_list.end(); ++it) {
		if (m_serial != NULL)
			m_serial->AddParam(it->m_name, &it->m_id, ResourceParam::SEMANTIC_SOUNDENTRYREF);
	}
	return 0;
}

/* the sounds with an ID take their waveforms from the plugin's sound table */
int32_t WidgetSoundParam::Read(const cxml::Element *elem)
{
	int32_t ret = -1;

	if (m_serial != NULL) {
		resource::ResourceObj *rscobj = NULL;

		if (m_plugin != NULL)
			rscobj = m_plugin->m_rscobj;
		if (m_serial->Read(*elem, rscobj, ResourceParam::READTYPE_FROM_PARAMLIST))
			ret = 0;
	}
	if (m_plugin == NULL)
		return -1;
	for (vector<Sound>::iterator it = m_sound_list.begin(); it != m_sound_list.end(); ++it) {
		if (it->m_id.m_idhash == 0)
			continue;

		const char *buffer1;
		uint32_t size1;
		const char *buffer2;
		uint32_t size2;
		int32_t samplerate;
		uint32_t type;

		if (ScePafToplevel_CA3E606F(m_plugin, it->m_id, &buffer1, &size1, &buffer2, &size2,
					    &samplerate, &type) == 0) {
			it->m_handle = -1;
			it->m_buffer1 = buffer1;
			it->m_size1 = size1;
			it->m_buffer2 = buffer2;
			it->m_size2 = size2;
			it->m_samplerate = samplerate;
			it->m_type = type;
		}
	}
	return ret;
}

int32_t WidgetSoundParam::AddWidget(Widget *widget)
{
	if (widget == NULL)
		return -1;
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it) {
		if (*it == widget)
			return 0;
	}
	m_widget_list.push_back(widget);
	widget->AddDeleteCB(delete_cb, this);
	return 0;
}

/* delete_cb calls it whole */
__attribute__((noinline)) int32_t WidgetSoundParam::DelWidget(Widget *widget)
{
	if (widget == NULL)
		return -1;
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it) {
		if (*it == widget) {
			m_widget_list.erase(it);
			return 0;
		}
	}
	return -1;
}

void WidgetSoundParam::delete_cb(Widget *widget, void *param)
{
	static_cast<WidgetSoundParam *>(param)->DelWidget(widget);
}

}
}
