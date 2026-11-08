/*
 * paf::ui::Plane (TOOL 0x81103d98..0x81104724): a widget drawn as one plane object, sized to
 * the widget, its scene or its texture on each axis as it adjusts; its resource and style
 * parameters and its factories. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/factory.h>
#include <widget/w_plane.h>
#include <widget/w_scene.h>

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace ui {

Plane::Plane(Widget *parent, CreateParam *param) : Widget(parent, param)
{
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_plane = NULL;
	ReserveDrawObj(1);
	m_plane = new graph::PlaneObj(graph::PlaneObjStyle());
	AddDrawObj(DrawObjEntry(m_plane, 0), false);
}

/* the style's draw object parameter by name */
static const char *const s_plane_objs[] = {
	"planeobj",
};

PlaneStyleParam::PlaneStyleParam()
{
	m_plane_param.m_name = s_plane_objs[0];
	m_plane_param.m_index = 0;
	m_drawobj_param_list.push_back(&m_plane_param);
}

const char Plane::m_widget_type[] = "plane";

bool Plane::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *Plane::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Plane::TypeName()
{
	return m_widget_type;
}

void Plane::Update()
{
	if (m_plane->IsDirty())
		UpdateAdjustSize();
	Widget::Update();
}

void Plane::UpdateSize()
{
	math::v3 size = m_plane->m_size;
	math::v3 pos = m_plane->m_pos;
	math::v4 scene_size;

	if (m_scene != NULL) {
		scene_size = *m_scene->GetSize(0);
	} else {
		math::rect screen(Screen::s_root_screen->sp);

		scene_size = math::v4(screen.width(), screen.height(), 0.0f, 0.0f);
	}

	/* the plane takes the widget's size where it does not adjust, the scene's where it
	   adjusts to it */
	switch (m_adjust_x) {
	case 0:
	case 1:
		pos.set_x(0.0f);
		size.set_x(m_size.extract_x());
		break;
	case 4:
		size.set_x(scene_size.extract_x());
		break;
	}
	switch (m_adjust_y) {
	case 0:
	case 1:
		pos.set_y(0.0f);
		size.set_y(m_size.extract_y());
		break;
	case 4:
		size.set_y(scene_size.extract_y());
		break;
	}
	switch (m_adjust_z) {
	case 0:
	case 1:
		size.set_z(m_size.extract_z());
		pos.set_z(0.0f);
		break;
	}
	m_plane->SetSize(size);
	m_plane->SetPos(pos);

	/* and so do the other draw objects that follow it */
	if (m_drawobjs.size() >= 2) {
		for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end();
		     ++it) {
			if (it->m_attr.m_flag1 && it->m_attr.m_id != 0) {
				it->m_obj->SetSize(size);
				it->m_obj->SetPos(pos);
			}
		}
	}
}

void Plane::UpdateAdjustSize()
{
	if ((m_adjust_x | m_adjust_y | m_adjust_z) == 0)
		return;

	/* sized to the plane's bounds, as far from the centre as they reach */
	math::v4 size = m_size;
	graph::DrawObjBounds bounds = m_plane->GetBounds();
	Vector3 min = bounds.m_min.getImpl();
	Vector3 max = bounds.m_max.getImpl();
	Vector3 center = (max + min) * 0.5f;
	math::v3 fit(absPerElem(center) * 2.0f + (max - min));

	if (m_adjust_x > 1 && m_adjust_x <= 3)
		size.set_x(fit.extract_x());
	if (m_adjust_y > 1 && m_adjust_y <= 3)
		size.set_y(fit.extract_y());
	if (m_adjust_z > 1 && m_adjust_z <= 3)
		size.set_z(fit.extract_z());
	if (!(m_size == size))
		SetSize_core(size, 0xFF02, false);
}

uint32_t Plane::CheckState()
{
	bool prepare = IsPrepareObjs();
	uint32_t state = m_state;

	if ((state == 3 || state == 1) && !prepare)
		return 0;
	return state;
}

int32_t Plane::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	ChangeState(3, -1);
	draw->SetTexture(surf, index);
	return 0;
}

int32_t Plane::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->GetTexture(surf, index);
	return 0;
}

common::SharedPtr<graph::DrawObj> Plane::SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
						     int32_t id)
{
	m_plane = (graph::PlaneObj *)obj.get();
	return Widget::SwapDrawObj(obj, id);
}

/* the texture released; the plane marked gone */
Plane::~Plane()
{
	Widget::SetTexture(intrusive_ptr<graph::Surface>());
	m_plane = (graph::PlaneObj *)-1;
}

const char *PlaneParam::GetParamName() const
{
	return "plane";
}

int32_t PlaneParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("hit_ignore", &m_hit_ignore, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t PlaneParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	Widget *widget = m_widget;

	if (m_hit_ignore == 1) {
		widget->m_hit_detect_b_mode = 2;
		widget->m_hit_detect_mode = 2;
	} else if (m_hit_ignore == 0) {
		widget->m_hit_detect_b_mode = 0;
		widget->m_hit_detect_mode = 0;
	}
	return 0;
}

int32_t PlaneParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;

	uint8_t mode = m_widget->m_hit_detect_mode;

	if (mode == 2)
		m_hit_ignore = 1;
	else if (mode == 0)
		m_hit_ignore = 0;
	else
		m_hit_ignore = -1;
	return 0;
}

const char PlaneStyleParam::m_param_name[] = "style_plane";

const char *PlaneStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *PlaneStyleParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewPlane(Widget *parent)
{
	return new Plane(parent, NULL);
}

static WidgetParam *NewPlaneParam()
{
	return new PlaneParam;
}

static VersatileParam *NewPlaneLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewPlaneFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewPlaneStyleParam()
{
	return new PlaneStyleParam;
}

static VersatileParam *NewPlaneHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewPlaneSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryPlane = {
	Plane::TypeName(),
	NewPlane,
	NewPlaneParam,
	NewPlaneLayoutParam,
	NewPlaneFocusParam,
	NewPlaneStyleParam,
	NewPlaneHandlerParam,
	NewPlaneSoundParam,
};
