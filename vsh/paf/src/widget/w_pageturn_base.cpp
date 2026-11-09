/*
 * paf::ui::PageTurnBase (DEBUGGER 0x81102c1c..0x81103d98): the page whose top right corner
 * the touch or the keys drag, the events the drag sends and the state it is in; its style
 * parameter and its factories. No record or string names the file; the names of the static
 * functions are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <inputdevice/inputlistener.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/factory.h>
#include <widget/w_pageturn_base.h>

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace ui {

/* the event sent with e, or with an empty one where there is none (name ours) */
static inline void SendEvent(Widget *widget, int32_t type, Event *e)
{
	if (e != NULL) {
		widget->DoEvent(type, e);
	} else {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		widget->DoEvent(type, &ev);
	}
}

const char PageTurnBase::m_widget_type[] = "pageturn_base";

PageTurnBase::PageTurnBase(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_turn_state(ST_PAGETURN_IDLE), m_corner_width(128.0f),
	  m_corner_height(128.0f), m_corner_margin(24.0f), m_turn_length(280.0f),
	  m_unused_offset_x(-100.0f), m_unused_offset_y(-100.0f), m_current(0.0f, 0.0f),
	  m_start(-1.0f, -1.0f), m_touch_id(-1), m_unused_id(-1),
	  m_unused_count(0), reserved_flag(false), m_return_listened(false)
{
	/* through the vtable, as Sony's */
	Widget *self = this;

	if (!(m_event_enable & 0x400))
		self->EnableFocusEvent(true);
	m_event_enable |= 0xFFF0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	EnableFocusEvent(false);
}

bool PageTurnBase::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *PageTurnBase::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *PageTurnBase::TypeName()
{
	return m_widget_type;
}

PageTurnBase::~PageTurnBase()
{
}

int32_t PageTurnBase::ScePafWidget_85CB1F24(uint32_t id, math::v4 *pos)
{
	Environment *env = m_env;
	TouchState *touch = NULL;

	if (id < 8) {
		env->m_touch_state[0][id].m_id = id;
		env->m_touch_state[0][id].m_port = 0;
		touch = &env->m_touch_state[0][id];
	}
	if (touch == NULL)
		return -1;

	if ((m_update_flag & 7) != 0)
		UpdateMatrix();

	math::matrix rot = m_world_rot.toMatrix(0.0001f);
	math::v4 normal(rot.getImpl() * Vector4(0.0f, 0.0f, 1.0f, 0.0f));
	math::v4 origin = GetWorldMatrix().w();
	HitInfo info;

	if (CalcHitLineToPlane(touch->m_line_start, touch->m_line_end, origin, normal, &info) < 0 ||
	    m_scene == NULL || m_parent == NULL)
		return -1;
	/* the touch's position and where it hit, as one vector */
	*pos = *(const math::v4 *)&info.m_touch_pos;
	return 0;
}

int32_t PageTurnBase::HandleTouchEvent(Event *e)
{
	int32_t state = m_turn_state;

	if (state == ST_PAGETURN_DISABLE)
		return -1;
	if (m_env != NULL) {
		uint32_t detail = e->m_detail;
		uint32_t port = (detail >> 20) & 0xF;
		uint32_t id = (uint8_t)detail;

		switch (e->GetType()) {
		case 0x41:
		case 0x43:
		case 0x44:
			if (state != ST_PAGETURN_TOUCH && state != ST_PAGETURN_KEY) {
				math::v4 pos;

				if (ScePafWidget_85CB1F24(id, &pos) < 0)
					break;

				Vector4 size = m_size.getImpl();
				math::v2 offs(pos.getImpl().getXY() - mulPerElem(size.getXY(), Vector2(0.5f)));

				/* not on the corner's diagonal itself */
				if (offs.extract_x() + offs.extract_y() > -m_corner_margin)
					break;

				math::v4 inv(recipPerElem(maxPerElem(
					minPerElem(size, Vector4(3.4028236e+18f)), Vector4(1.0f))));
				Vector4 rate = mulPerElem(pos.getImpl() + mulPerElem(size, Vector4(0.5f)),
							  inv.getImpl());
				float x = rate.getX();
				float y = rate.getY();

				/* within the corner's width of the right edge and its height of the top */
				if (x > 1.0f || x < 1.0f - m_corner_width * inv.extract_x() || y > 1.0f ||
				    y < 1.0f - m_corner_height * inv.extract_y())
					break;
				if (ScePafWidget_D3D0A7C3(this, port, id, true, true) < 0)
					break;

				math::v2 hit(pos.getImpl().getXY());

				m_touch_id = id;
				m_current = hit;
				m_start = hit;
				SendEvent(this, CB_PAGETURN_BEGIN, e);
				ScePafWidget_A4245073(ST_PAGETURN_TOUCH);
			} else if (state == ST_PAGETURN_TOUCH) {
				Vector2 start = m_start.getImpl();
				Vector2 current = m_current.getImpl();
				math::v2 move(current - start);

				m_touch_id = id;

				int32_t rate = (int32_t)(length(move.getImpl()) * 100.0f / m_turn_length);

				/* up and right of where it started: no turn yet */
				if ((current.getX() > start.getX()).getAsBool() &&
				    (current.getY() > start.getY()).getAsBool())
					rate = 0;
				if (m_return_listened && move.extract_x() + move.extract_y() > 50.0f) {
					e->m_data[3] = rate;
					SendEvent(this, CB_PAGETURN_RETURN, e);
				} else {
					e->m_data[3] = rate;
					SendEvent(this, CB_PAGETURN_MOVE, e);
				}
			}
			break;
		case 0x42:
			if (state == ST_PAGETURN_TOUCH) {
				math::v2 start = m_start;
				math::v2 current = m_current;
				float turn_length = m_turn_length;

				m_touch_id = -1;

				Vector2 move = current.getImpl() - start.getImpl();

				if (m_return_listened &&
				    (float)move.getX() + (float)move.getY() > 50.0f)
					SendEvent(this, CB_PAGETURN_RETURN, e);
				else if (lengthSqr(move) > turn_length * turn_length)
					SendEvent(this, CB_PAGETURN_TURNED, e);
				else
					SendEvent(this, CB_PAGETURN_CANCEL, e);
			}
			ScePafWidget_A4245073(ST_PAGETURN_IDLE);
			ScePafWidget_931AD41E(this, false);
			break;
		case 0x47:
			ScePafWidget_931AD41E(this, false);
			break;
		}
	}
	e->Consume();
	Handler::HandleTouchEvent(e);
	return 0;
}

int32_t PageTurnBase::HandleKeycodeEvent(Event *e)
{
	Environment *env = m_env;

	if (env != NULL && ScePafWidget_A6B659BB(env, 0) && ((e->m_detail >> 24) & 1) != 0) {
		int32_t state = m_turn_state;

		if (state != ST_PAGETURN_DISABLE) {
			Vector4 size = m_size.getImpl();
			/* where a key drag starts, 60 inside the corner */
			math::v2 key_pos((float)size.getX() * 0.5f - 60.0f,
					 (float)size.getY() * 0.5f - 60.0f);

			switch (e->GetType()) {
			case 0x11:
				if (state != ST_PAGETURN_TOUCH && state != ST_PAGETURN_KEY) {
					m_current = key_pos;
					m_start = key_pos;
					SendEvent(this, CB_PAGETURN_BEGIN, e);
					ScePafWidget_A4245073(ST_PAGETURN_KEY);
				}
				break;
			case 0x13:
				if (state == ST_PAGETURN_TOUCH || state == ST_PAGETURN_KEY) {
					Vector2 move = m_current.getImpl() - m_start.getImpl();
					float turn_length = m_turn_length;
					float distance = lengthSqr(move);
					int32_t rate = (int32_t)(length(move) * 100.0f / turn_length);

					if (m_return_listened &&
					    (float)move.getX() + (float)move.getY() > 80.0f) {
						e->m_data[3] = rate;
						SendEvent(this, CB_PAGETURN_RETURN, e);
					} else if (distance > turn_length * turn_length) {
						SendEvent(this, CB_PAGETURN_TURNED, e);
						ScePafWidget_A4245073(ST_PAGETURN_IDLE);
					} else {
						e->m_data[3] = rate;
						SendEvent(this, CB_PAGETURN_MOVE, e);
					}
				}
				break;
			case 0x12:
				if (state == ST_PAGETURN_TOUCH || state == ST_PAGETURN_KEY) {
					Vector2 move = m_current.getImpl() - m_start.getImpl();

					if (m_return_listened &&
					    (float)move.getX() + (float)move.getY() > 80.0f)
						SendEvent(this, CB_PAGETURN_RETURN, e);
					else if (lengthSqr(move) > m_turn_length * m_turn_length)
						SendEvent(this, CB_PAGETURN_TURNED, e);
					else
						SendEvent(this, CB_PAGETURN_CANCEL, e);
				}
				ScePafWidget_A4245073(ST_PAGETURN_IDLE);
				break;
			case 0x14:
				SendEvent(this, CB_PAGETURN_CANCEL, e);
				ScePafWidget_A4245073(ST_PAGETURN_IDLE);
				break;
			}
			e->Consume();
		}
	}
	return Widget::HandleKeycodeEvent(e);
}

void PageTurnBase::UpdateAfterChildren()
{
	int32_t state = m_turn_state;

	if (state == ST_PAGETURN_TOUCH) {
		math::v4 pos;

		ScePafWidget_85CB1F24(m_touch_id, &pos);
		m_current = math::v2(pos.getImpl().getXY());
	} else if (state == ST_PAGETURN_KEY) {
		if (m_fit_to_scene || m_culled) {
			SendEvent(this, CB_PAGETURN_CANCEL, NULL);
			ScePafWidget_A4245073(ST_PAGETURN_IDLE);
		}

		Vector2 current = m_current.getImpl();
		Vector2 size = m_size.getImpl().getXY();
		Vector2 move = current - m_start.getImpl();

		/* back toward the corner a tenth of the way, until it leaves the page's half */
		if ((float)current.getX() > (float)size.getX() * -0.5f ||
		    (float)current.getY() > (float)size.getY() * -0.5f)
			m_current = math::v2(current + (-Vector2(0.5f) + move * 0.1f));
	}
}

uint32_t PageTurnBase::CheckState()
{
	uint32_t state = m_state;

	if (state == 1 || state == 3 || state == 0) {
		bool dirty = false;

		state = 0;
		if (m_animator != NULL)
			dirty = m_animator->IsDirty(0xFFFF);
		if (dirty || m_prepare_count != 0 || IsPrepareObjs())
			return 3;
	}
	return state;
}

void PageTurnBase::UpdateSize()
{
	Widget::UpdateSize();
}

void PageTurnBase::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	int32_t state = m_turn_state;

	if (state == ST_PAGETURN_DISABLE || m_touch_captures.size() != 0)
		return;
	m_touch_id = -1;
	if (state == ST_PAGETURN_TOUCH || state == ST_PAGETURN_KEY) {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(CB_PAGETURN_CANCEL, &ev);
		state = m_turn_state;
	}
	if (state != ST_PAGETURN_IDLE)
		ScePafWidget_A4245073(ST_PAGETURN_IDLE);
}

int32_t PageTurnBase::ScePafWidget_A4245073(int32_t state)
{
	int32_t old = m_turn_state;

	m_return_listened = false;
	for (map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.begin();
	     it != m_map_e_listener.end(); ++it) {
		if (it->first == CB_PAGETURN_RETURN || it->first == 0x30000004) {
			m_return_listened = true;
			break;
		}
	}
	if (old == state)
		return m_turn_state;
	switch (state) {
	case ST_PAGETURN_IDLE:
	case ST_PAGETURN_DISABLE:
		m_turn_state = state;
		break;
	case ST_PAGETURN_TOUCH:
	case ST_PAGETURN_KEY:
		if (m_env != NULL) {
			int16_t touch_id = m_touch_id;
			inputdevice::InputGestureListener *input =
				static_cast<inputdevice::InputGestureListener *>(m_env->GetTouchGestureListener(0).get());

			if (input != NULL)
				inputdevice::ScePafMisc_CB4DB658(input, touch_id, NULL);
		}
		if (m_state == 0)
			m_turn_state = state;
		break;
	default:
		if (m_state == 0)
			m_turn_state = state;
		break;
	}
	return m_turn_state;
}

int32_t PageTurnBase::ScePafWidget_D321F831()
{
	ScePafWidget_931AD41E(this, false);
	return ScePafWidget_A4245073(ST_PAGETURN_IDLE);
}

int32_t PageTurnBase::ScePafWidget_1FE12722()
{
	ScePafWidget_931AD41E(this, false);
	return ScePafWidget_A4245073(ST_PAGETURN_DISABLE);
}

const char PageTurnBaseStyleParam::m_param_name[] = "style_pageturn_base";

const char *PageTurnBaseStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *PageTurnBaseStyleParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewPageTurnBase(Widget *parent)
{
	return new PageTurnBase(parent, NULL);
}

static WidgetParam *NewPageTurnBaseParam()
{
	return new WidgetParam;
}

static VersatileParam *NewPageTurnBaseLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewPageTurnBaseFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewPageTurnBaseStyleParam()
{
	return new PageTurnBaseStyleParam;
}

static VersatileParam *NewPageTurnBaseHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewPageTurnBaseSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryPageTurnBase = {
	PageTurnBase::TypeName(),
	NewPageTurnBase,
	NewPageTurnBaseParam,
	NewPageTurnBaseLayoutParam,
	NewPageTurnBaseFocusParam,
	NewPageTurnBaseStyleParam,
	NewPageTurnBaseHandlerParam,
	NewPageTurnBaseSoundParam,
};
