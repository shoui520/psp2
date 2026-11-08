/*
 * paf::ui::PlaneButton (TOOL 0x81104724..0x811056d4): a ButtonBase with an image plane (0) and a
 * glow (1). Where it adjusts it is sized to its visible planes and the margins of its adjust
 * parameter, its style animator laying the planes out; it plays the image's texture animation
 * as it is pressed and released. With its resource, style and handler parameters and its
 * factories. No record or string names the file; the names of the static functions are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/factory.h>
#include <widget/w_plane_button.h>

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::maxPerElem;
using sce::Vectormath::Simd::Aos::minPerElem;

/* the draw object, where it is a plane object (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) graph::PlaneObj *GetPlane(Widget *widget,
								      int32_t obj)
{
	if (widget->GetDrawObj(obj) != NULL &&
	    widget->GetDrawObj(obj)->IsInherit(graph::PlaneObj::DrawObjType()))
		return (graph::PlaneObj *)widget->GetDrawObj(obj);
	return NULL;
}

/* defined in reverse: SNC emits a file's static functions last defined first */
static graph::PlaneObj *CreateImage(PlaneButton *button)
{
	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 1;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;

	graph::PlaneObj *obj = new graph::PlaneObj(style);
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(PlaneButton::OBJ_IMAGE);

	attr.m_flag1 = false;
	button->InsertDrawObj(button->LowerBoundDrawObj(PlaneButton::OBJ_IMAGE),
			      Widget::DrawObjEntry(ptr, attr), false);
	return (graph::PlaneObj *)ptr.get();
}

static graph::PlaneObj *CreateGlow(PlaneButton *button)
{
	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 1;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;

	graph::PlaneObj *obj = new graph::PlaneObj(style);
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(PlaneButton::OBJ_GLOW);

	button->InsertDrawObj(button->LowerBoundDrawObj(PlaneButton::OBJ_GLOW),
			      Widget::DrawObjEntry(ptr, attr), false);
	button->SetGlowObj(PlaneButton::OBJ_GLOW);
	return (graph::PlaneObj *)ptr.get();
}

/* the style type and its animator (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) void SetStyleType(PlaneButton *button,
							       PlaneButton::StyleType type)
{
	button->m_style_type = type;
	button->SetStyleAnimator(type);
}

PlaneButton::PlaneButton(Widget *parent, CreateParam *param)
	: ButtonBase(parent, param), m_unused0(0), m_unused1(0), m_unused2(0),
	  m_style_type(STYLE_AUTO), m_frame_num(0), m_frame_time(0.0f)
{
	/* through the vtable, as Sony's */
	PlaneButton *self = this;

	ReserveDrawObj(2);
	if (param == NULL || param->m_create_drawobj) {
		CreateImage(this);
		CreateGlow(this)->SetVisible(false);
		SetGlowObj(-1);
	}
	self->SetStyleAnimator(self->m_style_type);
}

PlaneButtonParam::PlaneButtonParam()
{
}

/* the parameter named and indexed, among the style's draw object parameters (inlined in Sony's
   code, name ours) */
static inline __attribute__((always_inline)) void AddObjParam(WidgetStyleParam *style,
							      graph::DrawObjParam *param,
							      const char *name, int32_t index)
{
	param->m_name = name;
	param->m_index = index;
	style->m_drawobj_param_list.push_back(param);
}

/* the style's draw object parameters by name */
static const char *const s_plane_button_objs[] = {
	"image_obj",
	"glow_obj",
};

void PlaneButtonStyleParam::InitParam()
{
	m_style_type = PlaneButton::STYLE_AUTO;
	m_frame_num = 0;
	m_frame_time = 0.0f;
	AddObjParam(this, &m_image_obj, s_plane_button_objs[0], PlaneButton::OBJ_IMAGE);
	AddObjParam(this, &m_glow_obj, s_plane_button_objs[1], PlaneButton::OBJ_GLOW);
}

const char PlaneButton::m_widget_type[] = "plane_button";

const char *PlaneButton::TypeName()
{
	return m_widget_type;
}

int32_t PlaneButtonStyleAnimatorBase::OnUpdate_core()
{
	m_adjust_x = m_widget->m_adjust_x;
	m_adjust_y = m_widget->m_adjust_y;
	m_adjust_z = m_widget->m_adjust_z;
	if (m_last_adjust_x != m_adjust_x || m_last_adjust_y != m_adjust_y ||
	    m_last_adjust_z != m_adjust_z) {
		m_last_adjust_x = m_adjust_x;
		m_last_adjust_y = m_adjust_y;
		m_last_adjust_z = m_adjust_z;
		SetDirty(1);
	}
	if (m_dirty != 0 && Layout() != 0)
		m_dirty = 0;
	m_first = 0;
	return 0;
}

bool PlaneButton::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ButtonBase::IsInherit(type);
}

char *PlaneButton::GetTypeName() const
{
	return (char *)m_widget_type;
}

void PlaneButton::UpdateAdjustSize()
{
	UpdateButtonSize();
}

bool PlaneButton::UpdateObjsSize()
{
	graph::PlaneObj *image = GetPlane(this, OBJ_IMAGE);

	if (image == NULL)
		return false;

	/* the planes take the button's size and its centre where it does not adjust */
	math::v3 size = image->m_size;
	math::v3 pos = image->m_pos;

	switch (m_adjust_x) {
	case 0:
	case 1:
		pos.set_x(0.0f);
		size.set_x(m_size.extract_x());
		break;
	}
	switch (m_adjust_y) {
	case 0:
	case 1:
		pos.set_y(0.0f);
		size.set_y(m_size.extract_y());
		break;
	}
	switch (m_adjust_z) {
	case 0:
	case 1:
		size.set_z(m_size.extract_z());
		pos.set_z(0.0f);
		break;
	}
	image->SetSize(size);
	image->SetPos(pos);

	graph::PlaneObj *glow = GetPlane(this, OBJ_GLOW);

	if (glow != NULL) {
		glow->SetSize(size);
		glow->SetPos(pos);
	}
	return true;
}

math::v4 PlaneButton::GetLayoutSize(const math::v4 &fit_size, const math::v4 &offs) const
{
	math::v4 size = m_size;

	if (m_adjust_x > 1 && m_adjust_x <= 3)
		size.set_x(fit_size.extract_x() + __builtin_fabsf(offs.extract_x() * 2.0f));
	if (m_adjust_y > 1 && m_adjust_y <= 3)
		size.set_y(fit_size.extract_y() + __builtin_fabsf(offs.extract_y() * 2.0f));
	if (m_adjust_z > 1 && m_adjust_z <= 3)
		size.set_z(fit_size.extract_z());
	return size;
}

void PlaneButton::GetFitGeometry(math::v4 *size, math::v4 *offs) const
{
	math::v3 min(0.0f, 0.0f, 0.0f);
	math::v3 max(0.0f, 0.0f, 0.0f);
	bool first = true;
	/* paf's vector has no const_iterator */
	vector<DrawObjEntry> &objs = const_cast<vector<DrawObjEntry> &>(m_drawobjs);

	/* the bounds of its visible draw objects that do not follow its size */
	for (vector<DrawObjEntry>::iterator it = objs.begin(); it != objs.end(); ++it) {
		if (it->m_attr.m_flag1)
			continue;

		graph::DrawObj *obj = it->m_obj.get();

		if (obj == NULL || !obj->m_flags.m_visible)
			continue;
		if (first) {
			graph::DrawObjBounds bounds = obj->GetBounds();

			min = bounds.m_min;
			max = bounds.m_max;
			first = false;
		} else {
			graph::DrawObjBounds bounds = obj->GetBounds();

			min = math::v3(minPerElem(bounds.m_max.getImpl(),
						  minPerElem(bounds.m_min.getImpl(), min.getImpl())));
			max = math::v3(maxPerElem(bounds.m_max.getImpl(),
						  maxPerElem(bounds.m_min.getImpl(), max.getImpl())));
		}
	}
	if (m_adjust_param != NULL) {
		min = min - math::v3(m_adjust_param->m_left_margin, m_adjust_param->m_bottom_margin,
				     0.0f);
		max = max + math::v3(m_adjust_param->m_right_margin, m_adjust_param->m_top_margin,
				     0.0f);
	}
	*offs = math::v4((max + min) * 0.5f, 0.0f);
	*size = math::v4(max - min, 0.0f);
}

void PlaneButton::UpdateButtonSize()
{
	if ((m_adjust_x | m_adjust_y | m_adjust_z) == 0)
		return;

	math::v4 fit_size;
	math::v4 fit_offs;

	GetFitGeometry(&fit_size, &fit_offs);

	math::v4 layout = GetLayoutSize(fit_size, fit_offs);
	math::v4 size = layout;
	math::v4 lo(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 hi(3.4028235e+18f, 3.4028235e+18f, 3.4028235e+18f, 3.4028235e+18f);
	math::v4 min_size;
	math::v4 max_size;

	if (m_adjust_param != NULL) {
		min_size = m_adjust_param->m_min_size;
		max_size = m_adjust_param->m_max_size;
	} else {
		min_size = ScePafWidget_BE1CEA2E.m_min_size;
		max_size = ScePafWidget_BE1CEA2E.m_max_size;
	}

	if (m_adjust_x != 0) {
		lo.set_x(min_size.extract_x());
		hi.set_x(max_size.extract_x());
	}
	if (m_adjust_y != 0) {
		lo.set_y(min_size.extract_y());
		hi.set_y(max_size.extract_y());
	}
	if (m_adjust_z != 0) {
		lo.set_z(min_size.extract_z());
		hi.set_z(max_size.extract_z());
	}
	size = math::v4(minPerElem(hi.getImpl(), maxPerElem(lo.getImpl(), layout.getImpl())));
	if (!(m_size == size))
		SetSize_core(size, 0xFF02, false);
}

void PlaneButton::ScePafWidget_8AA684F6(bool forward)
{
	int32_t num = m_frame_num;

	if (num <= 0)
		return;

	int32_t start = 0;
	int32_t end = num - 1;

	if (!forward) {
		start = num - 1;
		end = 0;
	}

	Timer *t = NULL;
	float time = m_frame_time * (float)num;

	if (time > 0.0f)
		t = new Timer(time, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
	SetTexAnim(OBJ_IMAGE, 1, m_frame_num, start, end, t, 1, 0.0f, NULL, NULL);
}

uint32_t PlaneButton::ChangeButtonState(ButtonState state)
{
	uint32_t old = m_btn_state;
	uint32_t ret = ButtonBase::ChangeButtonState(state);

	if (ret != old) {
		if (ret == ST_BTN_PRESS)
			ScePafWidget_8AA684F6(true);
		else if (old == ST_BTN_PRESS)
			ScePafWidget_8AA684F6(false);
	}
	return ret;
}

graph::DrawObj *PlaneButton::CreateDrawObj(int32_t obj)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj != NULL)
		return draw_obj;
	switch (obj) {
	case OBJ_IMAGE:
		return CreateImage(this);
	case OBJ_GLOW:
		return CreateGlow(this);
	default:
		return ButtonBase::CreateDrawObj(obj);
	}
}

void PlaneButton::SetStyleAnimator(StyleType style_type)
{
	Animator *animator = NULL;

	if (style_type != STYLE_MANUAL)
		animator = new PlaneButtonStyleAnimator;
	SetAnimator(animator);
}

const char PlaneButtonParam::m_param_name[] = "button_base";

const char *PlaneButtonParam::GetParamName() const
{
	return m_param_name;
}

const char *PlaneButtonParam::ParamName()
{
	return m_param_name;
}

const char PlaneButtonStyleParam::m_param_name[] = "style_plane_button";

const char *PlaneButtonStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *PlaneButtonStyleParam::ParamName()
{
	return m_param_name;
}

int32_t PlaneButtonStyleParam::Init()
{
	int32_t ret = WidgetStyleParam::Init();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t PlaneButtonStyleParam::Create(int32_t mode)
{
	int32_t ret = ButtonBaseStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_adjust == NULL)
		return -1;
	if (m_serial != NULL)
		m_serial->AddParam("style_type", &m_style_type, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("adjust_min_size", &m_adjust->m_min_size,
				   ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("adjust_max_size", &m_adjust->m_max_size,
				   ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("top_margin", &m_adjust->m_top_margin,
				   ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("bottom_margin", &m_adjust->m_bottom_margin,
				   ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL) {
		m_serial->AddParam("left_margin", &m_adjust->m_left_margin,
				   ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL) {
			m_serial->AddParam("right_margin", &m_adjust->m_right_margin,
					   ResourceParam::SEMANTIC_FLOAT);
			if (m_serial != NULL) {
				m_serial->AddParam("frame_num", &m_frame_num,
						   ResourceParam::SEMANTIC_INT);
				if (m_serial != NULL)
					m_serial->AddParam("frame_time", &m_frame_time,
							   ResourceParam::SEMANTIC_FLOAT);
			}
		}
	}
	return 0;
}

int32_t PlaneButtonStyleParam::SetEach(Widget *widget)
{
	/* an image without an ID draws nothing */
	if (m_image_obj.m_id.GetIDHash() == 0)
		m_image_obj.m_color.set_w(0.0f);

	int32_t ret = ButtonBaseStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		PlaneButton *button = (PlaneButton *)widget;

		SetStyleType(button, m_style_type);
		ScePafWidget_55D83675(button, m_adjust);
		button->m_frame_num = m_frame_num > 0 ? m_frame_num : 0;
		button->ScePafWidget_8AA684F6(button->m_btn_state == ButtonBase::ST_BTN_PRESS);
		button->m_frame_time = m_frame_time > 0.0f ? m_frame_time : 0.0f;
		if (m_glow_obj.m_id.GetIDHash() == 0) {
			button->SetGlowObj(-1);
			m_glow_obj.m_color.set_w(0.0f);
		} else {
			button->SetGlowObj(PlaneButton::OBJ_GLOW);
		}
	}
	return 0;
}

int32_t PlaneButtonStyleParam::GetEach(Widget *widget)
{
	int32_t ret = ButtonBaseStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		PlaneButton *button = (PlaneButton *)widget;

		m_style_type = button->m_style_type;
		m_frame_time = button->m_frame_time;
		m_frame_num = button->m_frame_num;
	}
	return 0;
}

const char PlaneButtonHandlerParam::m_param_name[] = "handler_plane_button";

const char *PlaneButtonHandlerParam::GetParamName() const
{
	return m_param_name;
}

const char *PlaneButtonHandlerParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewPlaneButton(Widget *parent)
{
	return new PlaneButton(parent, NULL);
}

static WidgetParam *NewPlaneButtonParam()
{
	return new PlaneButtonParam;
}

static VersatileParam *NewPlaneButtonLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewPlaneButtonFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewPlaneButtonStyleParam()
{
	return new PlaneButtonStyleParam;
}

static VersatileParam *NewPlaneButtonHandlerParam()
{
	return new PlaneButtonHandlerParam;
}

static VersatileParam *NewPlaneButtonSoundParam()
{
	return new ButtonBaseSoundParam;
}

FactoryFuncTable g_factoryPlaneButton = {
	PlaneButton::TypeName(),
	NewPlaneButton,
	NewPlaneButtonParam,
	NewPlaneButtonLayoutParam,
	NewPlaneButtonFocusParam,
	NewPlaneButtonStyleParam,
	NewPlaneButtonHandlerParam,
	NewPlaneButtonSoundParam,
};
