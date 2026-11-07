/*
 * paf::ui::CornerButton (DEBUGGER 0x810e8288..0x810e92a8): a button in a corner of the screen, its
 * corner object sized to its texture, with the key focus frame of the corner. No record or string
 * names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/resource_hooks.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/factory.h>
#include <widget/w_corner_button.h>

namespace paf {
namespace ui {

/* the corner object, kept to its texture's aspect (inlined in Sony's code, name ours) */
static graph::PlaneObj *CreateCornerObj(CornerButton *button)
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

	if (obj != NULL) {
		obj->SetScaleMode(graph::PlaneObj::SCALE_ASPECT_SIZE, graph::PlaneObj::SCALE_ASPECT_SIZE);

		common::SharedPtr<graph::DrawObj> ptr(obj);
		Widget::DrawObjAttr attr(CornerButton::OBJ_CORNER);

		attr.m_flag0 = false;
		attr.m_flag1 = false;
		button->InsertDrawObj(button->LowerBoundDrawObj(CornerButton::OBJ_CORNER),
				      Widget::DrawObjEntry(ptr, attr), false);
	}
	return obj;
}

CornerButton::CornerButton(Widget *parent, CreateParam *param)
	: Button(parent, param), m_layout_type(0), m_corner_texture(), reserved(0),
	  m_focus_priority(true)
{
	/* through the vtable, as Sony's */
	Button *self = this;

	if (param == NULL || param->m_create_drawobj)
		CreateCornerObj(this);
	self->SetStyleAnimator(self->m_style_type);

	FocusShape shape;

	shape.m_texture = ScePafCommon_F820A54B(IDParam("_common_texture_keyfocus_corner_frame"));
	shape.m_size = math::v2((float)shape.m_texture->GetWidth(),
				(float)shape.m_texture->GetHeight());
	ScePafWidget_13B246E0(this, &shape);
}

static const struct {
	const char *name;
	int32_t index;
} s_corner_button_textures[] = {
	{ "texture_corner", CornerButton::OBJ_CORNER },
};

CornerButtonParam::CornerButtonParam()
{
	for (uint32_t i = 0; i < sizeof(s_corner_button_textures) / sizeof(s_corner_button_textures[0]);
	     i++)
		m_texture_list.push_back(IndexedRef(s_corner_button_textures[i].name,
						    s_corner_button_textures[i].index));
}

CornerButtonLayoutParam::CornerButtonLayoutParam()
{
	m_align[0] = Widget::ALIGN_LEFT;
	m_align[1] = Widget::ALIGN_TOP;
	m_size = math::v3(1.0f, 1.0f, 0.0f) * 90.0f;
	m_pos = math::v3((float)g_screen_width_qhd * -0.5f, (float)g_screen_height_qhd * 0.5f, 0.0f);
}

/* the style's draw object parameters by name (name ours) */
static const char *const s_corner_button_objs[] = {
	"corner_obj",
};

void CornerButtonStyleParam::InitParam()
{
	m_layout_type = 0;
	m_corner_obj.m_name = s_corner_button_objs[0];
	m_corner_obj.m_index = CornerButton::OBJ_CORNER;
	m_drawobj_param_list.push_back(&m_corner_obj);
}

const char CornerButton::m_widget_type[] = "corner_button";

bool CornerButton::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Button::IsInherit(type);
}

char *CornerButton::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *CornerButton::TypeName()
{
	return m_widget_type;
}

bool CornerButton::UpdateObjsSize()
{
	return true;
}

graph::DrawObj *CornerButton::CreateDrawObj(int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw != NULL)
		return draw;
	if (obj == OBJ_CORNER)
		return CreateCornerObj(this);
	return Button::CreateDrawObj(obj);
}

void CornerButton::UpdatePost()
{
}

/* the draw object with the ID when it is a plane, else NULL (inlined in Sony's code, name ours) */
static inline graph::PlaneObj *GetPlaneObj(Widget *widget, int32_t id)
{
	if (widget->GetDrawObj(id) != NULL &&
	    widget->GetDrawObj(id)->IsInherit(graph::PlaneObj::DrawObjType()))
		return (graph::PlaneObj *)widget->GetDrawObj(id);
	return NULL;
}

int32_t CornerButton::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				 uint32_t index)
{
	int32_t ret = 0;
	graph::PlaneObj *plane;

	if (obj == OBJ_CORNER && (plane = GetPlaneObj(this, obj)) != NULL) {
		plane->SetTexture(surf, 0);

		math::v3 size(0.0f, 0.0f, 0.0f);

		if (surf.get() != NULL)
			size = math::v3((float)surf->GetWidth(), (float)surf->GetHeight(), 0.0f);
		plane->SetSize(size);
	} else {
		ret = Button::SetTexture(surf, obj, index);
	}
	return ret;
}

}
}

using namespace paf;
using namespace paf::ui;

/* the plane's rotation, when the draw object is a plane */
static inline void SetPlaneRot(CornerButton *button, int32_t id, const math::v3 &rot)
{
	graph::PlaneObj *plane = GetPlaneObj(button, id);

	if (plane != NULL)
		plane->SetRot(rot);
}

/* the focus frame's offset and rotation, when it has one */
static inline void SetFocusShape(CornerButton *button, const math::v3 &offset, const math::v3 &rot)
{
	FocusShape *shape = button->m_focus_shape;

	if (shape != NULL) {
		shape->m_rot = rot;
		shape->m_offset = offset;
	}
}

void ScePafWidget_DFEF98E2(CornerButton *button, int32_t type)
{
	const float pi = 3.14159265f;

	button->m_layout_type = type;
	switch (type & 7) {
	case 0:
		SetPlaneRot(button, Button::OBJ_BG, math::v3(pi, 0.0f, 0.0f));
		SetPlaneRot(button, Button::OBJ_HIGHLIGHT, math::v3(pi, 0.0f, 0.0f));
		SetPlaneRot(button, Button::OBJ_GLOW, math::v3(pi, 0.0f, 0.0f));
		/* fall through */
	case 4:
		button->SetAlign(Widget::ALIGN_LEFT, Widget::ALIGN_TOP, Widget::ALIGN_CENTER, NULL);
		button->SetAnchor(110, 111, 0, NULL);
		if (type & 8)
			button->SetPos(math::v4(0.0f, -32.0f, 0.0f, 0.0f), NULL, Widget::ANIM_MOVE);
		SetFocusShape(button, math::v3(3.0f, -3.0f, 0.0f), math::v3(0.0f, pi, pi));
		button->SetFocusDirection(0, button->m_name);
		button->SetFocusDirection(2, button->m_name);
		break;
	case 1:
		SetPlaneRot(button, Button::OBJ_BG, math::v3(pi, pi, 0.0f));
		SetPlaneRot(button, Button::OBJ_HIGHLIGHT, math::v3(pi, pi, 0.0f));
		SetPlaneRot(button, Button::OBJ_GLOW, math::v3(pi, pi, 0.0f));
		/* fall through */
	case 5:
		button->SetAlign(Widget::ALIGN_RIGHT, Widget::ALIGN_TOP, Widget::ALIGN_CENTER, NULL);
		button->SetAnchor(111, 111, 0, NULL);
		if (type & 8)
			button->SetPos(math::v4(0.0f, -32.0f, 0.0f, 0.0f), NULL, Widget::ANIM_MOVE);
		SetFocusShape(button, math::v3(-3.0f, -3.0f, 0.0f), math::v3(0.0f, 0.0f, pi));
		button->SetFocusDirection(0, button->m_name);
		button->SetFocusDirection(3, button->m_name);
		break;
	case 2:
		button->SetAlign(Widget::ALIGN_LEFT, Widget::ALIGN_BOTTOM, Widget::ALIGN_CENTER, NULL);
		button->SetAnchor(110, 110, 0, NULL);
		SetFocusShape(button, math::v3(3.0f, 3.0f, 0.0f), math::v3(0.0f, 0.0f, 0.0f));
		button->SetFocusDirection(1, button->m_name);
		button->SetFocusDirection(2, button->m_name);
		break;
	case 3:
		SetPlaneRot(button, Button::OBJ_BG, math::v3(0.0f, pi, 0.0f));
		SetPlaneRot(button, Button::OBJ_HIGHLIGHT, math::v3(0.0f, pi, 0.0f));
		SetPlaneRot(button, Button::OBJ_GLOW, math::v3(0.0f, pi, 0.0f));
		/* fall through */
	case 7:
		button->SetAlign(Widget::ALIGN_RIGHT, Widget::ALIGN_BOTTOM, Widget::ALIGN_CENTER, NULL);
		button->SetAnchor(111, 110, 0, NULL);
		SetFocusShape(button, math::v3(-3.0f, 3.0f, 0.0f), math::v3(0.0f, pi, 0.0f));
		button->SetFocusDirection(1, button->m_name);
		button->SetFocusDirection(3, button->m_name);
		break;
	case 6:
		break;
	}
	button->SetSize(math::v4(90.0f, 90.0f, 0.0f, 0.0f), NULL, Widget::ANIM_SIZE);
}

namespace paf {
namespace ui {

void CornerButton::OnTransitionEnd(int32_t type, Handler *self, Event *e, void *data)
{
	e->m_data[0] = (int32_t)data;
	self->DoEvent(ButtonBase::CB_BTN_END, e);
}

CornerButton::~CornerButton()
{
}

const char CornerButtonParam::m_param_name[] = "corner_button_base";

const char *CornerButtonParam::GetParamName() const
{
	return m_param_name;
}

const char *CornerButtonParam::ParamName()
{
	return m_param_name;
}

const char CornerButtonLayoutParam::m_param_name[] = "layout_corner_button";

const char *CornerButtonLayoutParam::GetParamName() const
{
	return m_param_name;
}

const char CornerButtonStyleParam::m_param_name[] = "style_corner_button";

const char *CornerButtonStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *CornerButtonStyleParam::ParamName()
{
	return m_param_name;
}

int32_t CornerButtonStyleParam::Init()
{
	int32_t ret = ButtonStyleParam::Init();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t CornerButtonStyleParam::Create(int32_t mode)
{
	int32_t ret = ButtonStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("layout_type", &m_layout_type, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t CornerButtonStyleParam::SetEach(Widget *widget)
{
	/* without an ID of its own, the corner object is not drawn */
	if (m_corner_obj.m_id.m_idhash == 0)
		m_corner_obj.m_color.set_w(0.0f);

	int32_t ret = ButtonStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL)
		ScePafWidget_DFEF98E2((CornerButton *)widget, m_layout_type);
	return 0;
}

int32_t CornerButtonStyleParam::GetEach(Widget *widget)
{
	int32_t ret = ButtonStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL)
		m_layout_type = ((CornerButton *)widget)->m_layout_type;
	return 0;
}

const char CornerButtonHandlerParam::m_param_name[] = "handler_corner_button";

const char *CornerButtonHandlerParam::GetParamName() const
{
	return m_param_name;
}

const char *CornerButtonHandlerParam::ParamName()
{
	return m_param_name;
}

}
}

static Widget *NewCornerButton(Widget *parent)
{
	return new CornerButton(parent, NULL);
}

static WidgetParam *NewCornerButtonParam()
{
	return new CornerButtonParam;
}

static VersatileParam *NewCornerButtonLayoutParam()
{
	return new CornerButtonLayoutParam;
}

static VersatileParam *NewCornerButtonFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewCornerButtonStyleParam()
{
	return new CornerButtonStyleParam;
}

static VersatileParam *NewCornerButtonHandlerParam()
{
	return new CornerButtonHandlerParam;
}

static VersatileParam *NewCornerButtonSoundParam()
{
	return new ButtonBaseSoundParam;
}

FactoryFuncTable g_factoryCornerButton = {
	CornerButton::TypeName(),
	NewCornerButton,
	NewCornerButtonParam,
	NewCornerButtonLayoutParam,
	NewCornerButtonFocusParam,
	NewCornerButtonStyleParam,
	NewCornerButtonHandlerParam,
	NewCornerButtonSoundParam,
};
