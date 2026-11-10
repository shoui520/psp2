/*
 * paf::ui::BusyIndicator (DEBUGGER 0x810ddf4c..0x810df010): ten plane objects, the head (1) that
 * circles the centre of the widget and the trail (2 to 9) it leaves behind as it moves on, each
 * trail ball fading and shrinking to the ball size; object 0 is the widget's plane. Start grows
 * the circle in, Stop shrinks it away and hides the widget when the last trail ball has faded.
 * No record or string names the file; the names of the static function and objects are ours.
 */
#include <graphics/drawobj/plane_obj.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/event.h>
#include <common/resource_hooks.h>
#include <widget/factory.h>
#include <widget/w_busyindicator.h>
#include "../std/math.h"

namespace paf {
namespace ui {

/* the balls and the plane: 1 the head, 2 to 9 the trail */
#define BALL_OBJS	10

void BusyIndicator::SetBallSize(float size)
{
	if (size <= 0.0f)
		return;
	m_ball_size = size;
	m_trail_spacing = 0.62831853f;
}

int32_t BusyIndicator::SetTexture(const intrusive_ptr<graph::Surface> &surf)
{
	for (int32_t i = 1; i < BALL_OBJS; i++) {
		graph::DrawObj *obj = GetDrawObj(i);

		if (obj != NULL)
			obj->SetTexture(surf, 0);
	}
	return 0;
}

BusyIndicator::BusyIndicator(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_anim_state(ANIM_STOPPED), m_unused(0.0f, 0.0f, 0.0f, 0.0f),
	  m_trail_spacing(0.0f), m_rotation(0.0f), m_anim_time(0.0f), m_last_trail_angle(0.0f)
{
	m_event_enable |= 0x4000;
	ReserveDrawObj(BALL_OBJS);

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

	graph::PlaneObj *plane = new graph::PlaneObj(style);

	if (plane != NULL) {
		plane->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		AddDrawObj(DrawObjEntry(plane, 0), false);
	}
	for (int32_t i = 1; i < BALL_OBJS; i++) {
		graph::PlaneObj *ball = new graph::PlaneObj(style);

		if (ball == NULL)
			continue;
		ball->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		ball->SetSize(math::v3(0.0f, 0.0f, 0.0f));
		AddDrawObj(DrawObjEntry(ball, i), false);
	}

	{
		IDParam id("_common_texture_busy");
		intrusive_ptr<graph::Surface> tex = ScePafCommon_F820A54B(id);
		/* through the vtable, as Sony's */
		Widget *self = this;

		for (int32_t i = 1; i < BALL_OBJS; i++) {
			graph::DrawObj *obj = self->GetDrawObj(i);

			if (obj != NULL)
				obj->SetTexture(tex, 0);
		}
	}
	SetBallSize(10.0f);
}

BusyIndicatorParam::BusyIndicatorParam()
{
}

BusyIndicatorStyleParam::BusyIndicatorStyleParam()
	: m_ball_size(0.0f), m_ball_color(1.0f, 1.0f, 1.0f, 1.0f)
{
}

const char BusyIndicator::m_widget_type[] = "busyindicator";

bool BusyIndicator::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *BusyIndicator::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *BusyIndicator::TypeName()
{
	return m_widget_type;
}

/* the color of the ball obj, its alpha kept */
static int32_t SetBallColor(BusyIndicator *self, const math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw_obj = self->GetDrawObj(obj);

	if (draw_obj != NULL) {
		math::v4 c = color;

		c.set_w(draw_obj->GetColor().extract_w());
		draw_obj->SetColor(c);
	}
	return 0;
}

int32_t BusyIndicator::SetObjColor(const math::v4 &color, int32_t obj)
{
	if (obj == 0) {
		for (int32_t i = 1; i < BALL_OBJS; i++)
			SetBallColor(this, color, i);
	} else {
		SetBallColor(this, color, obj);
	}
	return 0;
}

int32_t BusyIndicator::GetObjColor(math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj != NULL)
		color = draw_obj->GetColor();
	return 0;
}

void BusyIndicator::DestroyWidget()
{
	Widget::DestroyWidget();
}

/* the widget's state, but 3 (ready) or 0 for states 0, 1 and 3 as its draw objects are ready */
uint32_t BusyIndicator::CheckState()
{
	uint32_t state = m_state;

	if (state != 1 && state != 0 && state != 3)
		return state;
	return IsPrepareObjs() ? 3 : 0;
}

/* the plane at the widget's size */
void BusyIndicator::UpdateSize()
{
	math::v4 size = m_size;

	if (m_ball_size > 0.0f)
		SetBallSize(m_ball_size);
	for (int32_t i = 0; i < BALL_OBJS; i++) {
		graph::DrawObj *obj = GetDrawObj(i);

		if (obj != NULL && i == 0)
			obj->SetSize(math::v3(size.getImpl().getXYZ()));
	}
	Widget::UpdateSize();
}

int32_t BusyIndicator::OnDraw(graph::DrawInfo &info)
{
	if (m_anim_state != ANIM_STOPPED)
		Widget::OnDraw(info);
	return 0;
}

/* the event of the type, its data d0 to d3, dispatched forward from the indicator */
void BusyIndicator::SendEvent(int32_t type, int32_t d0, int32_t d1, int32_t d2, int32_t d3)
{
	Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, d0, d1, d2, d3);

	DoEvent(type, &e);
}

/*
 * The head moves on six times its rotation; the circle's radius, the head's size and how much of
 * the spacing it must move before it leaves a trail ball grow in over 0.3 while it starts, and
 * shrink away over 0.5 while it stops. A trail ball left behind fades by the share of the
 * spacing the head moved, and stops fading at 0.18, unless the indicator stops. Stopped, with the
 * last trail ball faded, the widget is hidden and sends event 0x10000001.
 */
void BusyIndicator::Update()
{
	if (m_anim_state != ANIM_STOPPED) {
		float dt = Timer::s_tick_unit * Timer::s_tick_interval;

		m_rotation += dt;
		if (m_rotation >= 3.1415927f) {
			m_rotation -= 3.1415927f;
			m_last_trail_angle -= 18.849556f;
		}

		float angle = m_rotation * 6.0f;
		bool leave_trail = false;
		float scale = 1.0f;
		graph::DrawObj *head = GetDrawObj(1);

		if (head != NULL) {
			math::v4 color = head->GetColor();
			math::v4 ball(m_ball_size, m_ball_size, 0.0f, 0.0f);

			color.set_w(1.0f);
			head->SetColor(color);

			math::v3 pos;
			math::v4 size = ball;
			float radius;
			float s;

			switch (m_anim_state) {
			case ANIM_STARTING:
				m_anim_time += dt;
				s = m_anim_time / 0.3f;
				if (s > 1.0f) {
					s = 1.0f;
					m_anim_state = ANIM_RUNNING;
				}
				s = sce_paf_sinf(s * 1.5707964f);
				radius = (m_size.extract_x() - m_ball_size) * 0.5f * (2.0f - s);
				pos = math::v3(radius * sce_paf_cosf(angle),
					       -(radius * sce_paf_sinf(angle)), 0.0f);
				scale = s * s;
				size = ball * s;
				break;
			case ANIM_RUNNING:
				radius = (m_size.extract_x() - m_ball_size) * 0.5f;
				pos = math::v3(radius * sce_paf_cosf(angle),
					       -(radius * sce_paf_sinf(angle)), 0.0f);
				break;
			case ANIM_STOPPING:
				m_anim_time += dt;
				s = 0.0f;
				if (!(m_anim_time * 2.0f >= 1.0f))
					s = sce_paf_cosf(m_anim_time * 2.0f * 1.5707964f);
				scale = s;
				radius = (m_size.extract_x() - m_ball_size) * 0.5f * (2.0f - s);
				pos = math::v3(radius * sce_paf_cosf(angle),
					       -(radius * sce_paf_sinf(angle)), 0.0f);
				size = ball * s;
				break;
			}

			float fade = 0.18f;

			if (scale > 0.0f) {
				scale = m_trail_spacing * scale;
				fade = 0.0f;
				if (scale > 0.001f)
					fade = dt * 6.0f * 0.82f / (scale * 8.0f);
				if (m_last_trail_angle + scale < angle)
					leave_trail = true;
			}

			math::v4 head_size(sce::Vectormath::Simd::Aos::maxPerElem(
				sce::Vectormath::Simd::Aos::Vector4(0.0f), size.getImpl()));

			head->SetPos(pos);
			head->SetSize(math::v3(head_size.getImpl().getXYZ()));

			math::v4 trail_size = head_size * 1.25f;
			bool fading = false;

			for (int32_t i = 2; i < BALL_OBJS; i++) {
				graph::DrawObj *obj = GetDrawObj(i);

				if (obj == NULL)
					continue;

				math::v4 c = obj->GetColor();
				float alpha = c.extract_w();

				if (alpha <= 0.18f) {
					/* faded: left where the head is, once it has moved on */
					if (head_size.extract_x() > 0.0f && leave_trail) {
						obj->SetPos(pos);
						obj->SetSize(
							math::v3(trail_size.getImpl().getXYZ()));
						c.set_w(1.0f);
						obj->SetColor(c);
						m_last_trail_angle = angle;
						leave_trail = false;
					} else {
						c.set_w(0.0f);
						obj->SetColor(c);
					}
					continue;
				}
				if (alpha > 0.18f) {
					float obj_width = obj->m_size.extract_x();

					if (obj_width > m_ball_size) {
						float w = m_ball_size;

						if (obj_width - m_ball_size > 0.5f)
							w = (obj_width + m_ball_size) * 0.5f;
						obj->SetSize(math::v3(w, w, 0.0f));
					}
					c.set_w(alpha - fade);
					fading = true;
					if (m_anim_state == ANIM_STOPPING) {
						sce::Vectormath::Simd::Aos::Vector3 p =
							obj->m_pos.getImpl();
						float f = fade * (m_ball_size /
								  obj->m_size.extract_x());

						obj->SetPos(math::v3(p + p * f * 0.05f));
					}
				} else {
					c.set_w(m_anim_state == ANIM_STOPPING ? 0.0f : 0.18f);
				}
				obj->SetColor(c);
			}

			if (!fading && m_anim_state == ANIM_STOPPING) {
				m_transition_complete = false;
				m_transition = 0xff;
				DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
				m_anim_state = ANIM_STOPPED;

				Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0,
					0, 0, 0, 0);

				DoEvent(0x10000001, &e);
			}
		}
	}
	Widget::Update();
}

}
}

void ScePafWidget_C371ECA3(void)
{
}

namespace paf {
namespace ui {

void BusyIndicator::Start()
{
	if (m_anim_state == ANIM_STOPPED) {
		if (!m_transition_complete) {
			m_transition = 0xff;
			m_transition_complete = true;
			DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(this, NULL, 2, 0.0f, NULL);
		m_anim_state = ANIM_STARTING;
		m_rotation = 0.0f;
		m_anim_time = 0.0f;
		m_last_trail_angle = 0.0f;
	} else if (m_anim_state == ANIM_STOPPING) {
		m_anim_state = ANIM_STARTING;
		m_anim_time = (1.0f - m_anim_time * 2.0f) * 0.3f;
	}
}

void BusyIndicator::Stop()
{
	if (m_anim_state == ANIM_STARTING) {
		m_anim_state = ANIM_STOPPING;
		m_anim_time = (1.0f - m_anim_time / 0.3f) * 0.5f;
	} else if (m_anim_state == ANIM_RUNNING) {
		m_anim_state = ANIM_STOPPING;
		m_anim_time = 0.0f;
	}
}

BusyIndicator::~BusyIndicator()
{
}

const char BusyIndicatorParam::m_param_name[] = "busyindicator_base";

const char *BusyIndicatorParam::GetParamName() const
{
	return m_param_name;
}

const char *BusyIndicatorParam::ParamName()
{
	return m_param_name;
}

int32_t BusyIndicatorParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t BusyIndicatorParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t BusyIndicatorParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;
	return 0;
}

const char BusyIndicatorStyleParam::m_param_name[] = "style_busyindicator";

const char *BusyIndicatorStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *BusyIndicatorStyleParam::ParamName()
{
	return m_param_name;
}

int32_t BusyIndicatorStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (m_serial != NULL) {
		m_serial->AddParam("ball_size", &m_ball_size, ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL)
			m_serial->AddParam("ball_color", &m_ball_color,
					   ResourceParam::SEMANTIC_VEC4);
	}
	return ret;
}

int32_t BusyIndicatorStyleParam::SetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		BusyIndicator *indicator = (BusyIndicator *)widget;

		indicator->SetBallSize(m_ball_size);
		indicator->SetObjColor(m_ball_color, 0);
	}
	return 0;
}

int32_t BusyIndicatorStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		BusyIndicator *indicator = (BusyIndicator *)widget;

		m_ball_size = indicator->m_ball_size;
		indicator->GetObjColor(m_ball_color, 1);
	}
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewBusyIndicator(Widget *parent)
{
	return new BusyIndicator(parent, NULL);
}

static WidgetParam *NewBusyIndicatorParam()
{
	return new BusyIndicatorParam;
}

static VersatileParam *NewBusyIndicatorLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewBusyIndicatorFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewBusyIndicatorStyleParam()
{
	return new BusyIndicatorStyleParam;
}

static VersatileParam *NewBusyIndicatorHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewBusyIndicatorSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryBusyIndicator = {
	BusyIndicator::TypeName(),
	NewBusyIndicator,
	NewBusyIndicatorParam,
	NewBusyIndicatorLayoutParam,
	NewBusyIndicatorFocusParam,
	NewBusyIndicatorStyleParam,
	NewBusyIndicatorHandlerParam,
	NewBusyIndicatorSoundParam,
};
