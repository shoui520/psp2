/*
 * paf::ui::SpeechBalloon (DEBUGGER 0x81129d40..0x8112a3f0): a plane drawn as a balloon with a
 * tail and a shadow. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/list>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <widget/factory.h>
#include <widget/w_speech_balloon.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

/* the style its shadow plane is made with, as its word: blend, depth and lighting 1 (name
   ours) */
static const uint32_t s_shadow_style = 0x111;

SpeechBalloon::SpeechBalloon(Widget *parent, CreateParam *param)
	: Plane(parent, param), m_tail_dir(TAIL_NONE)
{
}

SpeechBalloonStyleParam::SpeechBalloonStyleParam()
{
	m_shadow_obj.m_name = "shadow_obj";
	m_shadow_obj.m_index = SpeechBalloon::OBJ_SHADOW;
	m_drawobj_param_list.push_back(&m_shadow_obj);
}

const char SpeechBalloon::m_widget_type[] = "speech_balloon";

bool SpeechBalloon::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *SpeechBalloon::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *SpeechBalloon::TypeName()
{
	return m_widget_type;
}

float SpeechBalloon::GetTailLength()
{
	intrusive_ptr<graph::Surface> surf;

	static_cast<Widget *>(this)->GetTexture(surf);
	if (surf.get() == NULL)
		return 0.0f;

	uint32_t length;

	switch (m_tail_dir) {
	case TAIL_LEFT:
		length = surf->GetMarginLeft();
		break;
	case TAIL_RIGHT:
		length = surf->GetMarginRight();
		break;
	case TAIL_BOTTOM:
		length = surf->GetMarginBottom();
		break;
	case TAIL_TOP:
		length = surf->GetMarginTop();
		break;
	default:
		length = 0;
		break;
	}
	return (float)length;
}

void SpeechBalloon::SetTailPos(float pos)
{
	((graph::PlaneObj *)GetDrawObj(0))->SetDivCenterPos(pos);
}

graph::PlaneObj *SpeechBalloon::CreateShadowObj()
{
	graph::DrawObj *found = GetDrawObj(OBJ_SHADOW);

	if (found != NULL)
		return (graph::PlaneObj *)found;

	graph::PlaneObj *obj = new graph::PlaneObj(reinterpret_cast<const graph::DrawStyle &>(s_shadow_style));
	common::SharedPtr<graph::DrawObj> ptr(obj);
	DrawObjAttr attr(OBJ_SHADOW);

	InsertDrawObj(LowerBoundDrawObj(OBJ_SHADOW), DrawObjEntry(ptr, attr), false);
	return obj;
}

uint32_t SpeechBalloon::CheckState()
{
	uint32_t state = Plane::CheckState();

	if (state == 0 && m_prepare_count != 0)
		return 3;
	return state;
}

void SpeechBalloon::UpdateSize()
{
	Widget::UpdateSize();
}

void SpeechBalloon::UpdateAdjustSize()
{
	math::v4 size = m_size;

	if (m_adjust_x == 2) {
		float width = 0.0f;

		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end();
		     ++it) {
			float w = (*it)->GetSize(0)->extract_x();

			if (w > width)
				width = w;
		}
		size.set_x(width);
	}
	if (m_adjust_y == 2) {
		float height = 0.0f;

		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end();
		     ++it) {
			float h = (*it)->GetSize(0)->extract_y();

			if (h > height)
				height = h;
		}
		size.set_y(height);
	}
	if (!(m_size == size))
		SetSize_core(size, 0xFF02, false);
}

const char SpeechBalloonStyleParam::m_param_name[] = "style_speech_balloon";

const char *SpeechBalloonStyleParam::GetParamName() const
{
	return m_param_name;
}

int32_t SpeechBalloonStyleParam::SetEach(Widget *widget)
{
	if (m_shadow_obj.m_id.GetIDHash() != 0)
		((SpeechBalloon *)widget)->CreateShadowObj();
	return WidgetStyleParam::SetEach(widget);
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewSpeechBalloon(Widget *parent)
{
	return new SpeechBalloon(parent, NULL);
}

static WidgetParam *NewSpeechBalloonParam()
{
	return new PlaneParam;
}

static VersatileParam *NewSpeechBalloonLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewSpeechBalloonFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewSpeechBalloonStyleParam()
{
	return new SpeechBalloonStyleParam;
}

static VersatileParam *NewSpeechBalloonHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewSpeechBalloonSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factorySpeechBalloon = {
	SpeechBalloon::TypeName(),
	NewSpeechBalloon,
	NewSpeechBalloonParam,
	NewSpeechBalloonLayoutParam,
	NewSpeechBalloonFocusParam,
	NewSpeechBalloonStyleParam,
	NewSpeechBalloonHandlerParam,
	NewSpeechBalloonSoundParam,
};
