/*
 * paf::ui::ButtonBase (DEBUGGER 0x810e109c..0x810e3944): what every button shares. Its states
 * (normal, disabled, pressed, pressed out, repeating) follow the touches, keys and gestures it
 * takes; it sends its button events and plays its five sounds, animates its glow object as its
 * state changes, fades to its disable color while disabled, and holds the text auto scroll a
 * long press starts. With its resource, style, handler and sound parameters. No record or string
 * names the file; the names of the static functions are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <inputdevice/inputlistener.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/animlistener.h>
#include <widget/core/environment.h>
#include <widget/core/layouter.h>
#include <widget/core/widget_gesture.h>
#include <widget/factory.h>
#include <widget/w_button_base.h>
#include <widget/w_scene.h>

namespace paf {
namespace ui {

/* the button whose long press holds the text auto scroll now (name ours) */
static ButtonBase *s_text_auto_scroll_button;

static void OnGlowPulseFinish(Timer *t, AnimListener *listener, void *data1, void *data2,
			      void *data3);
static void OnGlowAnimFinish(Timer *t, AnimListener *listener, void *data1, void *data2,
			     void *data3);

/* the draw object's color animated from src to dst, the finish callback called with data once
   it ends (inlined SetObjAnim with a finish callback; name ours) */
static void SetObjAnimCB(Widget *widget, int32_t obj, int32_t id, const math::v4 &dst, Timer *t,
			 int32_t slot, AnimCB cb_fin, void *data)
{
	graph::DrawObj *draw = widget->GetDrawObj(obj);

	if (draw == NULL)
		return;

	math::v4 src = draw->GetColor();

	widget->SetAnimListener(obj + id,
				new AnimListenerObj(t, widget, draw, src, dst,
						    &AnimListenerObj::s_prop_obj_color, cb_fin, data),
				slot);
}

/* the glow pulses, once (0) or breathing (1), and again from OnGlowPulseFinish (name ours) */
static void GlowPulse(ButtonBase *button, int32_t mode)
{
	graph::DrawObj *glow = button->GetDrawObj(button->m_glow_obj);

	if (glow == NULL)
		return;

	math::v4 color = glow->GetColor();
	math::v4 bright = color;

	bright.set_w(button->m_glow_max);
	if (mode <= 0) {
		if (mode < 0)
			return;

		math::v4 dim = color;

		dim.set_w(0.2f);
		button->SetAnimListener(button->m_glow_obj + 0x20300,
					new AnimListenerObj(new Timer(200.0f, Timer::FUNC_QUAD_OUT),
							    button, glow, color, bright,
							    &AnimListenerObj::s_prop_obj_color,
							    NULL, NULL),
					2);
		button->SetAnimListener(button->m_glow_obj + 0x20300,
					new AnimListenerObj(new Timer(1000.0f), button, glow, bright,
							    dim, &AnimListenerObj::s_prop_obj_color,
							    OnGlowPulseFinish, (void *)mode),
					0);
		return;
	}
	if (mode > 1)
		return;

	math::v4 half = color;

	half.set_w(0.5f);
	button->SetAnimListener(button->m_glow_obj + 0x20300,
				new AnimListenerObj(new Timer(800.0f, Timer::FUNC_QUAD_IN), button,
						    glow, color, half,
						    &AnimListenerObj::s_prop_obj_color, NULL, NULL),
				2);
	button->SetAnimListener(button->m_glow_obj + 0x20300,
				new AnimListenerObj(new Timer(800.0f, Timer::FUNC_QUAD_OUT), button,
						    glow, half, bright,
						    &AnimListenerObj::s_prop_obj_color, NULL, NULL),
				0);
	button->SetAnimListener(button->m_glow_obj + 0x20300,
				new AnimListenerObj(new Timer(800.0f, Timer::FUNC_QUAD_IN), button,
						    glow, bright, half,
						    &AnimListenerObj::s_prop_obj_color, NULL, NULL),
				0);
	color.set_w(0.24f);
	button->SetAnimListener(button->m_glow_obj + 0x20300,
				new AnimListenerObj(new Timer(800.0f, Timer::FUNC_QUAD_OUT), button,
						    glow, half, color,
						    &AnimListenerObj::s_prop_obj_color,
						    OnGlowPulseFinish, (void *)mode),
				0);
}

ButtonBase::ButtonBase(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_glow_obj(-1), m_glow_max(0.85f), m_btn_mode(BTN_MODE_BASIC),
	  m_btn_state(ST_BTN_NORMAL), m_disable_color(NULL), m_enable(true), m_active(true),
	  m_enable_long_press(false), m_enable_repeat(false), m_keep_press(false),
	  m_long_press_flag(true), m_disable_fade(0), reserved(0), m_long_press_msec(0),
	  m_repeat_begin(0), m_repeat_msec(0), m_repeat_count(0), m_glow_anim(0),
	  m_long_press_text_auto_scroll(false), m_text_auto_scroll_locked(false)
{
	if (!(m_event_enable & 0x400))
		static_cast<Widget *>(this)->EnableFocusEvent(true);
	m_event_enable |= 0xFFF0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	SetGestureMask(1, false);
	m_hit_check = 0;
	m_enable_tap = true;
	m_sound_mask = 0x3E;
	m_sound_list.clear();
}

const char ButtonBase::m_widget_type[] = "button_base";

ButtonBaseParam::ButtonBaseParam() : m_enable(true)
{
}

const char ButtonBaseHandlerParam::m_param_name[] = "handler_button_base";

const WidgetHandlerParam::EventList ButtonBaseHandlerParam::c_eventList[] = {
	{ "on_press", ButtonBase::CB_BTN_PRESS },
	{ "on_press_out", ButtonBase::CB_BTN_PRESS_OUT },
	{ "on_long_press", ButtonBase::CB_BTN_LONG_PRESS },
	{ "on_decide", ButtonBase::CB_BTN_DECIDE },
};

ButtonBaseHandlerParam::ButtonBaseHandlerParam()
{
	for (uint32_t i = 0; i < sizeof(c_eventList) / sizeof(c_eventList[0]); i++) {
		EventEntry entry = { c_eventList[i].name, c_eventList[i].type, NULL };

		m_event_list.push_back(entry);
	}
}

const char ButtonBaseSoundParam::m_param_name[] = "sound_button_base";

/* the sounds a button's sound parameter names, by index */
static const struct {
	const char *name;
	uint32_t index;
} s_button_sounds[] = {
	{ "press", ButtonBase::SOUND_PRESS },
	{ "decide", ButtonBase::SOUND_DECIDE },
	{ "long_press", ButtonBase::SOUND_LONG_PRESS },
	{ "move_out", ButtonBase::SOUND_MOVE_OUT },
	{ "multi_tap", ButtonBase::SOUND_MULTI_TAP },
};

ButtonBaseSoundParam::ButtonBaseSoundParam()
{
	for (int32_t i = 0; i < 5; i++)
		m_sound_list.push_back(Sound(s_button_sounds[i].name, s_button_sounds[i].index));
}

bool ButtonBase::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ButtonBase::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ButtonBase::TypeName()
{
	return m_widget_type;
}

ButtonBase::~ButtonBase()
{
	if (m_disable_color != NULL)
		delete m_disable_color;
	m_disable_color = (math::v4 *)-1;
	if (s_text_auto_scroll_button == this)
		s_text_auto_scroll_button = NULL;
}

WidgetSoundParam::Sound *ButtonBase::GetSound(int32_t id)
{
	for (list<WidgetSoundParam::Sound>::iterator it = m_sound_list.begin();
	     it != m_sound_list.end(); ++it) {
		if (it->m_index == (uint32_t)id)
			return &*it;
	}
	return FindSound(id);
}

void ButtonBase::AddSound(uint32_t index, WidgetSoundParam::Sound &sound)
{
	if (index < 6) {
		sound.m_index = index;
		m_sound_list.push_back(sound);
	}
}

int32_t ButtonBase::HandleTouchEvent(Event *e)
{
	if (m_env == NULL)
		return -1;
	uint32_t detail = e->m_detail;
	uint32_t port = (detail >> 20) & 0xF;
	uint8_t id = detail;

	if (IsButtonEnabled()) {
		switch (e->GetType()) {
		case 0x41:
			if (IsInputTarget()) {
				ScePafWidget_D3D0A7C3(this, port, id, true, true);
				ScePafWidget_2B1A1493(this, SOUND_PRESS);
				ChangeButtonState(ST_BTN_PRESS);
				e->Consume();
			}
			break;
		case 0x44:
			if ((m_btn_mode == BTN_MODE_BASIC && m_btn_state == ST_BTN_PRESS_OUT) ||
			    m_btn_mode == BTN_MODE_PRESSOVER) {
				ScePafWidget_D3D0A7C3(this, port, id, true, true);
				ScePafWidget_2B1A1493(this, SOUND_PRESS);
				ChangeButtonState(ST_BTN_PRESS);
				e->Consume();
			}
			break;
		case 0x45:
			if (m_btn_mode == BTN_MODE_BASIC) {
				remove_touch_gesture_mask(port, id, m_touch_gesture_mask);
				if (m_btn_state != ST_BTN_NORMAL) {
					ScePafWidget_2B1A1493(this, SOUND_MOVE_OUT);
					ChangeButtonState(ST_BTN_PRESS_OUT);
				}
				e->Consume();
			} else if (m_btn_mode == BTN_MODE_PRESSOVER || m_btn_mode == 2) {
				ScePafWidget_5ABB3393(this, port, id, false);
				if (m_btn_state != ST_BTN_NORMAL) {
					ScePafWidget_2B1A1493(this, SOUND_MOVE_OUT);
					ChangeButtonState(ST_BTN_NORMAL);
				}
				e->Consume();
			}
			if (m_btn_mode == 2) {
				m_hit_detect_mode = 0;
				m_hit_detect_b_mode = 0;
			}
			break;
		case 0x42:
			EndLongPressTextAutoScroll(e);
			ScePafWidget_5ABB3393(this, port, id, false);
			if (!m_keep_press)
				ChangeButtonState(ST_BTN_NORMAL);
			e->Consume();
			if (m_btn_mode == 2) {
				m_hit_detect_mode = 0;
				m_hit_detect_b_mode = 0;
			}
			break;
		case 0x47:
			EndLongPressTextAutoScroll(e);
			ScePafWidget_5ABB3393(this, port, id, false);
			ChangeButtonState(ST_BTN_NORMAL);
			e->Consume();
			if (m_btn_mode == 2) {
				m_hit_detect_mode = 0;
				m_hit_detect_b_mode = 0;
			}
			break;
		case 0x43:
			UpdateLongPressTextAutoScroll(e);
			break;
		}
	}
	Handler::HandleTouchEvent(e);
	return 0;
}

int32_t ButtonBase::HandleKeycodeEvent(Event *e)
{
	uint32_t detail = e->m_detail;

	if (!(detail & 0x1000000) && (detail & 0xFF) != 27)
		return Handler::HandleKeycodeEvent(e);
	if (IsButtonEnabled()) {
		uint32_t type = e->GetType();

		if (type > 0x12) {
			if (type == 0x14) {
				uint32_t key = detail & 0xFF;

				ReleasePadCaptureDir(key, true);
				ReleasePadCaptureButton(key, true);
				ChangeButtonState(ST_BTN_NORMAL);
				e->Consume();
			}
		} else if (type > 0x10) {
			if (type <= 0x11) {
				uint32_t mask;

				if (IsInputTarget() ||
				    (reserved3o && (mask = KeycodeToMask(detail & 0xFF),
						    detail = e->m_detail, m_bound_keycode) &&
				     (mask & m_keycode) != 0)) {
					CapturePad((detail >> 20) & 0xF, detail & 0xFF, true, true);
					ScePafWidget_2B1A1493(this, SOUND_PRESS);
					ChangeButtonState(ST_BTN_PRESS);
				}
				e->Consume();
			} else {
				uint32_t key = detail & 0xFF;

				ReleasePadCaptureDir(key, false);
				ReleasePadCaptureButton(key, false);
				ChangeButtonState(ST_BTN_NORMAL);
				e->Consume();
			}
		}
	}
	return Handler::HandleKeycodeEvent(e);
}

void ButtonBase::SetGlowAnim(int32_t type, bool store)
{
	if (m_glow_obj < 0)
		return;

	graph::DrawObj *glow = GetDrawObj(m_glow_obj);

	if (glow == NULL)
		return;
	KillObjAnim(m_glow_obj, 0x20300);

	int32_t obj = m_glow_obj;
	math::v4 color = glow->GetColor();
	math::v4 from = color;
	math::v4 to = color;

	if (store)
		m_glow_anim = type;
	switch (type) {
	case GLOW_OFF:
		to.set_w(m_glow_max * 0.0f);
		SetObjAnimCB(this, obj, 0x20300, to, new Timer(400.0f, Timer::FUNC_QUAD_OUT), 2,
			     OnGlowAnimFinish, this);
		break;
	case GLOW_HIDE:
		to.set_w(m_glow_max * 0.0f);
		glow->SetColor(to);
		break;
	case GLOW_PRESS:
		to.set_w(m_glow_max);
		SetObjAnim(obj, 0x20300, to, new Timer(100.0f, Timer::FUNC_QUAD_OUT), 2);
		break;
	case GLOW_REPEAT:
		to.set_w(m_glow_max * 0.5f);
		SetObjAnim(obj, 0x20300, to, new Timer(100.0f, Timer::FUNC_QUAD_OUT), 2);
		break;
	case GLOW_PRESS_OUT:
		to.set_w(m_glow_max * 0.3f);
		SetObjAnimCB(this, obj, 0x20300, to, new Timer(300.0f, Timer::FUNC_QUAD_OUT), 2,
			     OnGlowAnimFinish, this);
		break;
	case GLOW_DECIDE: {
		float max = m_glow_max + 0.15f;

		if (max > 1.0f)
			max = 1.0f;
		from.set_w(max);
		SetObjAnim(obj, 0x20300, from, new Timer(50.0f, Timer::FUNC_QUAD_OUT), 2);
		to.set_w(m_glow_max * 0.0f);
		SetObjAnimCB(this, m_glow_obj, 0x20300, to,
			     new Timer(600.0f, Timer::FUNC_QUAD_IN), 0, OnGlowAnimFinish, this);
		break;
	}
	case GLOW_FLASH:
		from.set_w(m_glow_max * 0.5f);
		SetObjAnim(obj, 0x20300, from, new Timer(100.0f, Timer::FUNC_QUAD_IN), 2);
		from.set_w(m_glow_max);
		SetObjAnim(m_glow_obj, 0x20300, from, new Timer(160.0f, Timer::FUNC_QUAD_OUT), 0);
		to.set_w(m_glow_max * 0.0f);
		SetObjAnimCB(this, m_glow_obj, 0x20300, to,
			     new Timer(600.0f, Timer::FUNC_QUAD_IN), 0, OnGlowAnimFinish, this);
		break;
	case GLOW_PULSE:
		GlowPulse(this, 0);
		break;
	case GLOW_BREATHE:
		GlowPulse(this, 1);
		break;
	}
}

int32_t ButtonBase::HandleGestureEvent(Event *e)
{
	if (!IsButtonEnabled())
		return Widget::HandleGestureEvent(e);

	uint32_t detail = e->m_detail;
	uint32_t port = (detail >> 20) & 0xF;

	switch ((detail >> 8) & 0x7F) {
	case 0:
		DoButtonEvent(CB_BTN_DECIDE, e);
		ScePafWidget_2B1A1493(this, SOUND_DECIDE);
		ChangeButtonState(ST_BTN_NORMAL);
		SetGlowAnim(GLOW_DECIDE, false);
		break;
	case 1:
		if (m_btn_mode == 2 && m_touch_captures.size() != 0) {
			m_hit_detect_b_mode = 2;
			m_hit_detect_mode = 2;
		}
		if (m_btn_mode == BTN_MODE_BASIC) {
			remove_touch_gesture_mask(port, detail & 0xFF, m_touch_gesture_mask);
			if (m_btn_state != ST_BTN_NORMAL) {
				ScePafWidget_2B1A1493(this, SOUND_MOVE_OUT);
				ChangeButtonState(ST_BTN_PRESS_OUT);
			}
		} else if (m_btn_mode == BTN_MODE_PRESSOVER || m_btn_mode == 2) {
			ScePafWidget_5ABB3393(this, port, detail & 0xFF, false);
			if (m_btn_state != ST_BTN_NORMAL) {
				ScePafWidget_2B1A1493(this, SOUND_MOVE_OUT);
				ChangeButtonState(ST_BTN_NORMAL);
			}
		}
		break;
	case 2:
		DoButtonEvent(CB_BTN_MULTI_TAP, e);
		ScePafWidget_2B1A1493(this, SOUND_MULTI_TAP);
		ChangeButtonState(ST_BTN_NORMAL);
		SetGlowAnim(GLOW_DECIDE, false);
		break;
	case 4:
		BeginLongPressTextAutoScroll(e);
		DoButtonEvent(CB_BTN_LONG_PRESS, e);
		ScePafWidget_2B1A1493(this, SOUND_LONG_PRESS);
		if (m_touch_gesture_mask & 8) {
			ChangeButtonState(ST_BTN_PRESS_REPEAT);
			m_repeat_count = 5;
		} else {
			ChangeButtonState(ST_BTN_NORMAL);
		}
		break;
	case 5:
		DoButtonEvent(CB_BTN_PRESS_REPEAT, e);
		ScePafWidget_2B1A1493(this, SOUND_LONG_PRESS);
		m_repeat_count = 5;
		SetGlowAnim(GLOW_REPEAT, false);
		break;
	case 6:
		DoButtonEvent(CB_BTN_PRESS_REPEAT_END, e);
		ChangeButtonState(ST_BTN_NORMAL);
		break;
	default:
		return Widget::HandleGestureEvent(e);
	}
	e->Consume();
	return 0;
}

/* 3 (ready) for states 0, 1 and 3 while its animator, its children or its draw objects still
   prepare, else 0 */
uint32_t ButtonBase::CheckState()
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

void ButtonBase::UpdateSize()
{
	Widget::UpdateSize();
}

/* a touch captured holds for the long press or the repeat; all let go, it is normal again */
void ButtonBase::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	if (!IsButtonEnabled())
		return;
	if (m_touch_captures.size() == 0) {
		if (m_btn_state != ST_BTN_NORMAL)
			ChangeButtonState(ST_BTN_NORMAL);
		return;
	}
	if ((!m_enable_long_press || m_long_press_msec == 0) &&
	    (!m_enable_repeat || m_repeat_begin == 0))
		return;

	inputdevice::InputGestureListener *input = m_env->GetTouchGestureListener(port).get();

	if (input == NULL)
		return;

	common::SharedPtr<inputdevice::GestureListener> listener =
		input->GetGestureListener(id, 0xF, this);

	if (listener.get() != NULL) {
		HoldGestureListener *hold = (HoldGestureListener *)listener.get();

		if (m_enable_repeat) {
			hold->m_param.m_hold_time = m_repeat_begin;
			hold->m_param.m_repeat_time = m_repeat_msec;
		} else {
			hold->m_param.m_hold_time = m_long_press_msec;
		}
	}
}

/* the pad's gesture listener of the port, the button its user data (inline in Sony's code,
   name ours) */
static inline common::SharedPtr<inputdevice::GestureListener> GetPadGesture(Environment *env,
									    uint32_t port,
									    void *user)
{
	inputdevice::InputGestureListener *input = env->m_pad_gesture_listener;

	if (input == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();
	return input->GetGestureListener(port, 0xF, user);
}

/* a key captured holds for the long press or the repeat, as a touch does */
void ButtonBase::OnPadCaptureChanged(bool captured, int32_t group, uint32_t port, uint32_t key)
{
	if (!IsButtonEnabled())
		return;
	if ((m_enable_long_press && m_long_press_msec != 0) ||
	    (m_enable_repeat && m_repeat_begin != 0)) {
		common::SharedPtr<inputdevice::GestureListener> listener =
			GetPadGesture(m_env, port, this);

		if (listener.get() != NULL) {
			PadHoldGestureListener *hold = (PadHoldGestureListener *)listener.get();

			if (m_enable_repeat) {
				hold->m_param.m_hold_time = m_repeat_begin;
				hold->m_param.m_repeat_time = m_repeat_msec;
			} else {
				hold->m_param.m_hold_time = m_long_press_msec;
			}
		}
	}
	if (!captured && m_btn_state != ST_BTN_NORMAL)
		ChangeButtonState(ST_BTN_NORMAL);
}

/* the color, the RGB of its bytes times the mix of white and the disable color at the fade (0 to
   200), and the alpha times the mix's (inline in Sony's code, name ours) */
static inline uint32_t FadeColor(const math::v4 *disable_color, uint32_t color, uint32_t fade,
				 float &alpha)
{
	float t = (float)fade / 200.0f;
	math::v4 white(1.0f, 1.0f, 1.0f, 1.0f);
	math::v4 target(1.0f, 1.0f, 1.0f, 0.3f);

	if (disable_color != NULL)
		target = *disable_color;

	math::v4 mix = white * (1.0f - t) + target * t;
	math::v4 rgb(math::v3(mix.getImpl().getXYZ()), white.extract_w());
	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));
	uint16x4_t out = vqmovn_u32(vcvtq_u32_f32(vmulq_f32(
		vmulq_f32(vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
				    vdupq_n_f32(1.0f / 255.0f)),
			  rgb.getImpl().get128()),
		vdupq_n_f32(255.0f))));

	alpha *= mix.extract_w();
	return vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(out, out))), 0);
}

/* its draw objects but the glow, faded to the disable color as it is disabled and back as it is
   enabled; a text drawn darker while disabled. Nonzero while the glow shows */
int32_t ButtonBase::OnDraw(graph::DrawInfo &info)
{
	uint32_t color = m_color;
	uint32_t fade = m_disable_fade;
	float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

	if (m_btn_state == ST_BTN_DISABLE) {
		if (fade < 200) {
			fade = (uint8_t)(uint32_t)((float)fade + Timer::s_tick_interval * 20.0f);
			m_disable_fade = fade;
		}
		if (fade > 200) {
			fade = 200;
			m_disable_fade = 200;
		}
		color = FadeColor(m_disable_color, color, fade, alpha);
	} else if (fade >= 20) {
		fade = (uint8_t)(uint32_t)((float)fade - Timer::s_tick_interval * 20.0f);
		m_disable_fade = fade;
		color = FadeColor(m_disable_color, color, fade, alpha);
		if (fade < 20)
			m_disable_fade = 0;
	}

	uint32_t dark = ((((color >> 8) & 0xFF) * 230 + 229) & 0xFF00) |
			(((((color >> 16) & 0xFF) * 230 + 229) >> 8) & 0xFF) << 16 |
			((color & 0xFF) * 230 + 229) >> 8 | 0xFF000000;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == m_glow_obj)
			continue;
		if (it->m_obj->GetDrawObjType() == graph::TextObj::DrawObjType() &&
		    m_btn_state == ST_BTN_DISABLE)
			it->m_obj->OnDraw(m_world_matrix, dark, alpha, info);
		else
			it->m_obj->OnDraw(m_world_matrix, color, alpha, info);
	}
	return m_glow_obj >= 0 && alpha > 0.0f;
}

/* the glow drawn over the widget in its own color and alpha */
int32_t ButtonBase::ApplyDrawObjState(graph::DrawInfo &info)
{
	graph::DrawObj *glow = GetDrawObj(m_glow_obj);

	if (glow != NULL)
		glow->OnDraw(m_world_matrix, m_color, m_color_alpha * m_meta_a_sum * m_show_a_sum, info);
	return 0;
}

/* the text object's, as SetString gives it */
static inline graph::TextObj *GetTextObj(Widget *widget, int32_t obj)
{
	if (widget->GetDrawObj(obj) != NULL &&
	    widget->GetDrawObj(obj)->IsInherit(graph::TextObj::DrawObjType()))
		return (graph::TextObj *)widget->GetDrawObj(obj);
	return NULL;
}

int32_t ButtonBase::SetString(const wstring &str, int32_t obj)
{
	graph::TextObj *text = GetTextObj(this, obj);
	int32_t ret = 0x80AF4101;

	if (text != NULL)
		return text->SetString(str);
	return ret;
}

int32_t ButtonBase::GetString(wstring &str, int32_t obj)
{
	graph::TextObj *text = GetTextObj(this, obj);
	int32_t ret = 0x80AF4101;

	if (text != NULL)
		return text->GetString(str);
	return ret;
}

int32_t ButtonBase::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
			       uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->SetTexture(surf, index);
	return 0;
}

int32_t ButtonBase::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->GetTexture(surf, index);
	return 0;
}

/* mode 2 takes no taps */
void ButtonBase::SetButtonMode(uint32_t mode)
{
	if (m_btn_mode != mode) {
		m_btn_mode = mode;
		if (mode == 2)
			m_enable_tap = false;
		else
			m_enable_tap = true;
	}
}

void ButtonBase::UpdateButtonState(bool enabled)
{
	if (enabled) {
		if (m_btn_state != ST_BTN_NORMAL)
			ChangeButtonState(ST_BTN_NORMAL);
	} else if (m_btn_state != ST_BTN_DISABLE) {
		ChangeButtonState(ST_BTN_DISABLE);
	}
}

void ButtonBase::Enable(bool immediate)
{
	if (!m_enable) {
		m_enable = true;
		UpdateButtonState(IsButtonEnabled());
		if (immediate)
			m_disable_fade = 0;
	}
}

void ButtonBase::Disable(bool immediate)
{
	if (m_enable) {
		m_enable = false;
		UpdateButtonState(IsButtonEnabled());
		if (immediate)
			m_disable_fade = 200;
	}
}

/* 300 and 5 ms where the halfwords stored are 0 */
void ButtonBase::EnableRepeat(uint32_t begin_msec, uint32_t repeat_msec)
{
	m_repeat_begin = begin_msec;
	m_enable_repeat = true;
	m_repeat_msec = repeat_msec;
	if (m_repeat_begin == 0)
		m_repeat_begin = 300;
	if (m_repeat_msec == 0)
		m_repeat_msec = 5;
	SetGestureMask(m_touch_gesture_mask | 8, false);
}

/* the long press scrolls the text from where it is held, stopping another button's scroll */
int32_t ButtonBase::BeginLongPressTextAutoScroll(Event *e)
{
	if (!m_long_press_text_auto_scroll)
		return SCE_PAF_ERROR_BUTTON_BUSY;
	m_text_auto_scroll_locked = true;
	if (s_text_auto_scroll_button != NULL)
		s_text_auto_scroll_button->RequestTextAutoScrollStop();
	s_text_auto_scroll_button = this;
	StartTextAutoScroll(Text::SCROLL_TYPE_HORIZONTAL_ONCE);
	SetTextAutoScrollOrigin(math::v2((float)e->m_data[0], (float)e->m_data[1]));
	return 0;
}

/* the scroll's speed from where it is held now */
int32_t ButtonBase::UpdateLongPressTextAutoScroll(Event *e)
{
	if (!IsTextAutoScrollLocked())
		return SCE_PAF_ERROR_BUTTON_BUSY;

	float speed = 0;

	if (GetTextAutoScrollSpeedByPos(math::v2((float)e->m_data[0], (float)e->m_data[1]), speed,
					false, true) == 0)
		SetTextAutoScrollSpeed(speed);
	return 0;
}

int32_t ButtonBase::EndLongPressTextAutoScroll(Event *e)
{
	if (!IsTextAutoScrollLocked())
		return SCE_PAF_ERROR_BUTTON_BUSY;
	m_text_auto_scroll_locked = false;
	return 0;
}

graph::DrawObj *ButtonBase::CreateDrawObj(int32_t obj)
{
	return NULL;
}

/* the glow object shown in white, the one before hidden */
void ButtonBase::SetGlowObj(int32_t obj)
{
	graph::DrawObj *glow = GetDrawObj(obj);

	if (glow != NULL) {
		glow->SetVisible(true);
		glow->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	} else if (m_glow_obj >= 0) {
		graph::DrawObj *prev = GetDrawObj(m_glow_obj);

		if (prev != NULL) {
			prev->SetVisible(false);
			prev->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		}
	}
	m_glow_obj = obj;
}

/* a pulse's end starts it again (names ours) */
static void OnGlowPulseFinish(Timer *t, AnimListener *listener, void *data1, void *data2,
				   void *data3)
{
	GlowPulse((ButtonBase *)data1, (int32_t)data2);
}

/* the glow's end starts the stored animation again */
static void OnGlowAnimFinish(Timer *t, AnimListener *listener, void *data1, void *data2,
				  void *data3)
{
	ButtonBase *button = (ButtonBase *)data2;

	if (button->m_glow_anim != 0)
		button->SetGlowAnim(button->m_glow_anim, false);
}

uint32_t ButtonBase::ChangeButtonState(ButtonState state)
{
	if (m_btn_state == state)
		return m_btn_state;
	switch (state) {
	case ST_BTN_NORMAL:
		m_btn_state = state;
		SetGlowAnim(GLOW_OFF, false);
		DoButtonEvent(CB_BTN_NORMAL, NULL);
		break;
	case ST_BTN_DISABLE:
		m_btn_state = state;
		SetGlowAnim(GLOW_OFF, false);
		ScePafWidget_931AD41E(this, false);
		ReleasePadCaptureDir(255, false);
		ReleasePadCaptureButton(255, false);
		DoButtonEvent(CB_BTN_DISABLE, NULL);
		m_disable_fade = 0;
		break;
	case ST_BTN_PRESS:
		if (m_state == 0) {
			m_btn_state = state;
			SetGlowAnim(GLOW_PRESS, false);
		}
		DoButtonEvent(CB_BTN_PRESS, NULL);
		break;
	case ST_BTN_PRESS_OUT:
		if (m_state == 0) {
			m_btn_state = state;
			SetGlowAnim(GLOW_PRESS_OUT, false);
		}
		DoButtonEvent(CB_BTN_PRESS_OUT, NULL);
		break;
	case ST_BTN_PRESS_REPEAT:
		if (m_state == 0) {
			m_btn_state = state;
			SetGlowAnim(GLOW_PRESS, false);
		}
		break;
	default:
		return m_btn_state;
	}
	return m_btn_state;
}

/* where its scene orders first: the scene's priority over the inverted priority of the scene as
   a widget (inline in Sony's code, name ours) */
static inline int32_t GetSceneOrder(Widget *widget)
{
	Scene *scene = widget->m_scene;

	return (int32_t)((0xFF - scene->m_priority) | scene->m_scene_priority << 8);
}

/* its inverted depth in its scene, the most for a widget outside one (inline in Sony's code,
   name ours) */
static inline uint64_t GetInvertedDepth(Widget *widget)
{
	if (widget->m_scene != NULL && widget->m_parent != NULL)
		return ~widget->GetLayerDepth();
	return ~(uint64_t)0;
}

/* where it takes the keys bound to it, it is disabled below the top modal widget (modal with
   option 1); Sony's code compares the modal widget's depth with itself */
void ButtonBase::Update()
{
	if (m_bound_keycode && (m_keycode & 0x20) && m_env != NULL) {
		Widget *modal = m_env->m_top_modal_widget;

		if (modal != NULL && modal->m_modal_option == 1) {
			int32_t order = GetSceneOrder(modal);
			uint64_t depth = GetInvertedDepth(modal);

			if (order < GetSceneOrder(this) || depth < GetInvertedDepth(modal)) {
				if (m_active) {
					m_active = false;
					if (m_btn_state != ST_BTN_DISABLE)
						ChangeButtonState(ST_BTN_DISABLE);
				}
				Widget::Update();
				return;
			}
		}
		if (!m_active) {
			m_active = true;
			UpdateButtonState(IsButtonEnabled());
		}
	}
	Widget::Update();
}

const char ButtonBaseParam::m_param_name[] = "button_base_base";

const char *ButtonBaseParam::GetParamName() const
{
	return m_param_name;
}

int32_t ButtonBaseParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("enable", &m_enable, ResourceParam::SEMANTIC_BOOL);
	return 0;
}

/* the button enabled or disabled as the parameter says */
int32_t ButtonBaseParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	ButtonBase *button = (ButtonBase *)m_widget;

	if (m_enable != button->IsButtonEnabled()) {
		if (m_enable)
			button->Enable(false);
		else
			button->Disable(false);
	}
	return 0;
}

int32_t ButtonBaseParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;
	m_enable = ((ButtonBase *)m_widget)->IsButtonEnabled();
	return 0;
}

const char ButtonBaseStyleParam::m_param_name[] = "style_button_base";

const char *ButtonBaseStyleParam::GetParamName() const
{
	return m_param_name;
}

int32_t ButtonBaseStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("glow_max", &m_glow_max, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("key_focus", &m_key_focus, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("focus_in_group", &m_focus_in_group,
				   ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("direct_key", &m_direct_key, ResourceParam::SEMANTIC_INT);
	return 0;
}

/* the glow's largest alpha kept within 0 to 1, the focus off without key_focus and the direct
   key bound */
int32_t ButtonBaseStyleParam::SetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		float glow_max = m_glow_max;

		if (glow_max > 1.0f)
			glow_max = 1.0f;
		else if (glow_max < 0.0f)
			glow_max = 0.0f;
		((ButtonBase *)widget)->m_glow_max = glow_max;
		if (!m_key_focus)
			widget->EnableFocusEvent(false);
		if (!m_focus_in_group)
			widget->m_focus_in_group = false;
		if (m_direct_key != 0) {
			uint32_t keycode = 0;

			switch (m_direct_key) {
			case 1:
				keycode = 0x20;
				break;
			case 2:
				keycode = 0x40;
				break;
			case 3:
				keycode = 0x80;
				break;
			case 4:
				keycode = 0x400;
				break;
			case 5:
				keycode = 0x800;
				break;
			}
			ScePafWidget_21125870(widget, keycode);
		}
	}
	return 0;
}

int32_t ButtonBaseStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	return 0;
}

const char *ButtonBaseHandlerParam::GetParamName() const
{
	return m_param_name;
}

const char *ButtonBaseHandlerParam::ParamName()
{
	return m_param_name;
}

const char *ButtonBaseSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *ButtonBaseSoundParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewButtonBase(Widget *parent)
{
	return new ButtonBase(parent, NULL);
}

static WidgetParam *NewButtonBaseParam()
{
	return new ButtonBaseParam;
}

static VersatileParam *NewButtonBaseLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewButtonBaseFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewButtonBaseStyleParam()
{
	return new ButtonBaseStyleParam;
}

static VersatileParam *NewButtonBaseHandlerParam()
{
	return new ButtonBaseHandlerParam;
}

static VersatileParam *NewButtonBaseSoundParam()
{
	return new ButtonBaseSoundParam;
}

FactoryFuncTable g_factoryButtonBase = {
	ButtonBase::TypeName(),
	NewButtonBase,
	NewButtonBaseParam,
	NewButtonBaseLayoutParam,
	NewButtonBaseFocusParam,
	NewButtonBaseStyleParam,
	NewButtonBaseHandlerParam,
	NewButtonBaseSoundParam,
};
