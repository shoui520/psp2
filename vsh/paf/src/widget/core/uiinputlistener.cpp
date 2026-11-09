/*
 * The input listeners of the widget system (TOOL 0x81149fc4..0x8114b6d4): the gesture listeners of
 * the pad's keys, the pad listeners and UITouchListener, the listener of a touch port, which turns
 * its touches into touch events. The file name is ScePaf-RE's (its DWARF file list).
 */
#include <stddef.h>
#include <stdint.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/btrm/btrm.h>
#include <inputdevice/hidkeyboard/hidkeyboard.h>
#include <inputdevice/hidrm/hidrm.h>
#include <inputdevice/pad/pad.h>
#include <std/string.h>
#include <timer/timer.h>
#include <math/rect.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/uiinputlistener.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

/* the milliseconds from the time to the other, as Sony counts them (name ours) */
static inline __attribute__((always_inline)) uint32_t ElapsedMs(uint64_t now, uint64_t start)
{
	uint64_t diff = now - start;
	float seconds = (float)(double)diff / 1000000.0f;

	return (uint32_t)(seconds * 1000.0f);
}

/* the keycode of the key mask, 0 for none (Sony's PostGestureEvent has it inline, and 6B810963's
   code a copy; name ours) */
static int32_t PadKeycode(uint32_t mask)
{
	if (mask > 0x200) {
		switch (mask) {
		case 0x400:
			return 31;
		case 0x800:
			return 32;
		case 0x1000:
			return 39;
		case 0x10000:
			return 33;
		case 0x20000:
			return 34;
		case 0x40000:
			return 35;
		case 0x80000:
			return 36;
		default:
			return 0;
		}
	}
	if (mask > 0x3F) {
		switch (mask) {
		case 0x40:
			return 29;
		case 0x80:
			return 30;
		case 0x100:
			return 38;
		case 0x200:
			return 37;
		default:
			return 0;
		}
	}
	if (mask > 0xF) {
		if (mask > 0x1F) {
			if (mask <= 0x20)
				return 28;
		} else if (mask <= 0x10) {
			return 27;
		}
		return 0;
	}
	if (mask == 0)
		return 0;
	switch (mask) {
	case 1:
		return 1;
	case 2:
		return 2;
	case 4:
		return 3;
	case 8:
		return 4;
	default:
		return 0;
	}
}

/* the detail of a pad gesture event: the key code, the gesture, the port (names ours) */
union PadGestureDetail {
	uint32_t value;
	struct {
		uint32_t keycode : 8;
		uint32_t gesture : 7;
		uint32_t reserved : 5;
		uint32_t port : 4;
		uint32_t reserved2 : 8;
	} f;
};

/* the update of the pad keys, what the UIPadListener keeps for each port (TOOL 0x81315350 and
   0x81315410, and the port 0x813154d0; names ours) */
inputdevice::pad::Data s_pad_curr[8];
inputdevice::pad::Data s_pad_prev[8];
static int32_t s_pad_port;

int32_t PadGestureListener::on_update_core()
{
	int32_t port = s_pad_port;

	m_port = port;
	return OnPadUpdate((const inputdevice::pad::Data *)((const char *)s_pad_curr + 24 * port),
			   (const inputdevice::pad::Data *)((const char *)s_pad_prev + 24 * port));
}

int32_t PadGestureListener::PostGestureEvent(uint32_t gesture, uint32_t key, int32_t arg1,
					     int32_t arg2)
{
	UIInputGestureListener *input = static_cast<UIInputGestureListener *>(m_input);

	if (input == NULL)
		return -1;

	Environment *env = input->m_env;

	if (env == NULL)
		return -1;

	Widget *widget = (Widget *)m_user_data;

	if (widget == NULL || !IsValidUserData(m_user_data)) {
		widget = NULL;
		if (m_port < 8)
			widget = input->m_binded_widget[m_port];
		if (widget == NULL)
			return -1;
	}

	int32_t keycode = PadKeycode(key);

	if (keycode == 0)
		return -1;

	Event e(0x80, widget, 0x40000, 0);

	PadGestureDetail detail;

	detail.value = 0;
	detail.f.port = m_port;
	detail.f.keycode = keycode;
	detail.f.gesture = gesture;
	e.SetDetail(detail.value);
	e.m_data[2] = arg1;
	e.m_data[3] = arg2;
	env->m_ev_queue.PushBack(e);
	return 0;
}

int32_t PadTapListener::OnPadUpdate(const inputdevice::pad::Data *curr,
				    const inputdevice::pad::Data *prev)
{
	switch (m_state) {
	case 0: {
		uint32_t pressed = curr->buttons & ~prev->buttons;

		if (pressed == 0)
			pressed = curr->buttons;
		if (pressed != 0) {
			m_key = pressed;
			m_state = 1;
			m_count = 0;
			m_time = get_time_internal();
			if (m_param.m_on_press)
				m_count++;
		}
		return 1;
	}
	case 1: {
		bool on_press = m_param.m_on_press;
		uint32_t key = m_key;

		if (on_press && m_count == m_target) {
			PostGestureEvent(0, key, 1, 0);
			return 112;
		}
		if ((prev->buttons & ~curr->buttons) != key)
			return 1;

		int32_t target = m_target;

		if (target > 1) {
			if (m_param.m_press_time < ElapsedMs(get_time_internal(), m_time))
				return 16;
			on_press = m_param.m_on_press;
			target = m_target;
			key = m_key;
		}

		int32_t count = m_count;

		m_state = 2;
		if (!on_press) {
			m_count = ++count;
			if (count == target) {
				if (count == 1)
					PostGestureEvent(0, key, 1, 0);
				else
					PostGestureEvent(2, key, count, 0);
				return 112;
			}
		}
		m_pending = count;
		m_time = get_time_internal();
		return 1;
	}
	case 2: {
		uint32_t released = prev->buttons & ~curr->buttons;
		uint32_t gap = m_param.m_gap_time;

		if (released == m_key) {
			if (gap < ElapsedMs(get_time_internal(), m_time))
				return 16;
			m_state = 1;
			m_time = get_time_internal();
			if (!m_param.m_on_press)
				return 1;

			int32_t count = ++m_count;

			if (count != m_target)
				return 1;
			if (count == 1)
				PostGestureEvent(0, m_key, 1, 0);
			else
				PostGestureEvent(2, m_key, count, 0);
			return 112;
		}
		if (gap >= ElapsedMs(get_time_internal(), m_time))
			return 1;

		int32_t pending = m_pending;

		if (pending == 1)
			PostGestureEvent(0, m_key, 1, 0);
		else
			PostGestureEvent(2, m_key, pending, 0);
		return 112;
	}
	default:
		return 1;
	}
}

int32_t PadLongPressListener::OnPadUpdate(const inputdevice::pad::Data *curr,
					  const inputdevice::pad::Data *prev)
{
	switch (m_state) {
	case 0: {
		uint32_t pressed = curr->buttons & ~prev->buttons;

		if (pressed == 0)
			pressed = curr->buttons;
		if (pressed != 0) {
			m_key = pressed;
			m_state = 1;
			m_time = get_time_internal();
		}
		return 1;
	}
	case 1: {
		if (curr->buttons != m_key)
			return 16;

		uint32_t hold = m_param.m_hold_time;

		if (hold >= ElapsedMs(get_time_internal(), m_time))
			return 1;
		PostGestureEvent(4, m_key, 0, 0);
		if (!m_repeat)
			return m_param.m_result;
		m_time = get_time_internal();
		m_state = 2;
		return 96;
	}
	case 2: {
		if (curr->buttons != m_key) {
			PostGestureEvent(6, m_key, 0, 0);
			return 112;
		}

		uint32_t repeat = m_param.m_repeat_time;

		if (repeat < ElapsedMs(get_time_internal(), m_time)) {
			PostGestureEvent(5, m_key, 0, 0);
			m_time = get_time_internal();
			return 1;
		}
		return 1;
	}
	default:
		return 1;
	}
}


/* the detail of a key event: the key code, the port, whether the key repeats and the pad type
   (names ours) */
union PadKeyDetail {
	uint32_t value;
	struct {
		uint32_t keycode : 20;
		uint32_t port : 4;
		uint32_t flag : 1;
		uint32_t mode : 3;
		uint32_t reserved : 4;
	} f;
};

/* the same again, the copy Sony's UIPadListener::PostPadEvent has (name ours) */
static int32_t PadKeycodeForEvent(uint32_t mask)
{
	if (mask > 0x200) {
		switch (mask) {
		case 0x400:
			return 31;
		case 0x800:
			return 32;
		case 0x1000:
			return 39;
		case 0x10000:
			return 33;
		case 0x20000:
			return 34;
		case 0x40000:
			return 35;
		case 0x80000:
			return 36;
		default:
			return 0;
		}
	}
	if (mask > 0x3F) {
		switch (mask) {
		case 0x40:
			return 29;
		case 0x80:
			return 30;
		case 0x100:
			return 38;
		case 0x200:
			return 37;
		default:
			return 0;
		}
	}
	if (mask > 0xF) {
		if (mask > 0x1F) {
			if (mask <= 0x20)
				return 28;
		} else if (mask <= 0x10) {
			return 27;
		}
		return 0;
	}
	if (mask == 0)
		return 0;
	switch (mask) {
	case 1:
		return 1;
	case 2:
		return 2;
	case 4:
		return 3;
	case 8:
		return 4;
	default:
		return 0;
	}
}

int32_t UIPadListener::PostPadEvent(uint32_t type, uint32_t port, uint32_t key, bool flag,
				    int32_t mode)
{
	if (m_env == NULL)
		return -1;

	int32_t keycode = PadKeycodeForEvent(key);

	if (keycode == 0)
		return -1;

	Event e(type, 0x100000, 0x40000, 0, 0, 0, 0, 0);
	PadKeyDetail detail;

	detail.value = ((flag & 1) << 24) | keycode;
	detail.f.port = port;
	detail.f.mode = mode;
	e.SetDetail(detail.value);
	m_env->m_ev_queue.PushBack(e);
	return 0;
}

void UIPadListener::UpdatePad(uint32_t *prev, uint32_t *curr, int32_t *port, int32_t *mode,
			      const inputdevice::Data *data)
{
	uint32_t keys = *prev;

	if (*port == 0) {
		uint32_t now = *curr;

		if (((now & 1) != 0 && ((now & 4) != 0 || (now & 8) != 0)) ||
		    ((now & 2) != 0 && ((now & 4) != 0 || (now & 8) != 0)))
			now &= 0xFFFFFFF0;
		*curr = now;
		keys = *prev;
		if (((keys & 1) != 0 && ((keys & 4) != 0 || (keys & 8) != 0)) ||
		    ((keys & 2) != 0 && ((keys & 4) != 0 || (keys & 8) != 0)))
			keys &= 0xFFFFFFF0;
		*prev = keys;
	}
	*prev = m_prev_mask | keys;

	bool analog = m_analog_to_dpad;
	uint32_t now = *curr;

	if (analog && (now & 0xF) == 0) {
		uint32_t dirs = 0;
		const inputdevice::pad::Data *pad = data->m_pad_data;
		int32_t dx = sce_paf_abs(pad->lx - 128);
		int32_t dy = sce_paf_abs(pad->ly - 128);

		if (dx >= 79) {
			if (pad->lx < 128 && dx > dy)
				dirs = 4;
			if (pad->lx > 128 && dx > dy)
				dirs |= 8;
		}
		if (dy >= 79) {
			if (pad->ly < 128 && dx < dy)
				dirs |= 1;
			if (pad->ly > 128 && dx < dy)
				dirs |= 2;
		}
		now |= dirs;
		*curr = now;
		if (now != 0)
			inputdevice::pad::ResetIdleTime();
		now = *curr;
	}
	if ((now & 0x3F) == 0) {
		for (int32_t i = 0; i < 13; i++) {
			uint32_t keycode = data->m_keyboard_data->keycode[i];

			if (keycode != 0) {
				switch (keycode) {
				case 1:
					now |= 1;
					break;
				case 2:
					now |= 2;
					break;
				case 3:
					now |= 4;
					break;
				case 4:
					now |= 8;
					break;
				case 5:
					now |= 0x10;
					break;
				case 6:
					now |= 0x20;
					break;
				default:
					break;
				}
				*curr = now;
				*port = 0;
				inputdevice::pad::SetCurrPort(0);
				*mode = 1;
				now = *curr;
			}
		}
	}
	m_prev_mask = now;
}


/* one of the pad's repeat steps, as Sony's two OnUpdates have it inline (name ours) */
static inline float PadTick()
{
	return Timer::s_tick_unit * Timer::s_tick_interval;
}

void UIPadListener::OnUpdate(const inputdevice::Data *data)
{
	sce_paf_memcpy(s_pad_prev, s_pad_curr, sizeof(s_pad_prev));
	s_pad_port = inputdevice::pad::GetCurrPort();
	if (s_pad_port < 8) {
		int32_t mode = m_mode;
		int32_t type = inputdevice::pad::GetPortType(s_pad_port);

		if (type == 1)
			mode = 2;
		else if (type == 2)
			mode = 3;

		uint32_t curr_mask = data->m_pad_data->buttons & 0xFFFFDFFF;
		uint32_t prev_mask = data->m_pad_data_pre->buttons & 0xFFFFDFFF;

		UpdatePad(&prev_mask, &curr_mask, &s_pad_port, &mode, data);

		uint32_t pressed = curr_mask & ~prev_mask;
		uint32_t held = m_hold_key;
		uint32_t released = prev_mask & ~curr_mask;

		if (pressed != 0) {
			uint32_t first = 1;

			if ((pressed & 1) == 0) {
				uint32_t next;

				do {
					next = pressed & (2 * first);
					first *= 2;
				} while (next == 0);
			}
			if (held != 0) {
				if (held == 1 || held == 2 || held == 4 || held == 8) {
					PostPadEvent(18, s_pad_port, held, false, mode);
				} else {
					inputdevice::ScePafMisc_CB4DB658(this, s_pad_port, NULL);
					PostPadEvent(20, s_pad_port, held, false, mode);
				}
			}
			m_hold_key = first;
			PostPadEvent(17, s_pad_port, first, false, mode);
			m_repeat_count = 0;
			m_repeat_delay = 300.0f;
			m_repeat_interval = 80.0f;
			held = m_hold_key;
		}
		if (released != 0 && (released & held) != 0) {
			if ((inputdevice::pad::GetMask() & m_hold_key) != 0) {
				inputdevice::ScePafMisc_CB4DB658(this, s_pad_port, NULL);
				PostPadEvent(20, s_pad_port, m_hold_key, false, mode);
			} else {
				PostPadEvent(18, s_pad_port, m_hold_key, false, mode);
			}
			held = 0;
			m_hold_key = 0;
		}
		if (held != 0) {
			float delay = m_repeat_delay;
			float tick = PadTick();

			if (delay <= 0.0f) {
				float interval = m_repeat_interval - tick * 1000.0f;

				m_repeat_interval = interval;
				if (interval <= 0.0f) {
					PostPadEvent(19, s_pad_port, held, false, mode);
					m_repeat_count++;
					held = m_hold_key;
					if ((float)m_repeat_count < 10.0f)
						m_repeat_interval = 80.0f;
					else
						m_repeat_interval = 40.0f;
				}
			} else {
				float remain = delay - tick * 1000.0f;

				m_repeat_delay = remain;
				if (remain <= 0.0f) {
					PostPadEvent(19, s_pad_port, held, false, mode);
					held = m_hold_key;
					m_repeat_count++;
				}
			}
		}
		s_pad_curr[s_pad_port].buttons = held;
		m_mode = mode;
		if (held == 0)
			m_mode = 0;
		if ((inputdevice::pad::GetMask() & m_hold_key) != 0) {
			inputdevice::ScePafMisc_CB4DB658(this, s_pad_port, NULL);
			PostPadEvent(20, s_pad_port, m_hold_key & inputdevice::pad::GetMask(), false,
				     mode);
			m_hold_key = 0;
		}
	}
}

void UIPadHoldListener::OnUpdate(const inputdevice::Data *data)
{
	sce_paf_memcpy(s_pad_prev, s_pad_curr, sizeof(s_pad_prev));
	s_pad_port = inputdevice::pad::GetCurrPort();
	if (s_pad_port < 8) {
		int32_t mode = m_mode;
		int32_t type = inputdevice::pad::GetPortType(s_pad_port);

		if (type == 1)
			mode = 2;
		else if (type == 2)
			mode = 3;

		uint32_t curr_mask = data->m_pad_data->buttons & 0xFFFFDFFF;
		uint32_t prev_mask = data->m_pad_data_pre->buttons & 0xFFFFDFFF;

		UpdatePad(&prev_mask, &curr_mask, &s_pad_port, &mode, data);

		uint32_t pressed = curr_mask & ~prev_mask;
		uint32_t released = prev_mask & ~curr_mask;

		if (pressed != 0) {
			m_hold_key = curr_mask;
			PostPadEvent(17, s_pad_port, pressed, false, mode);
			m_repeat_count = 0;
			m_repeat_delay = 300.0f;
			m_repeat_interval = 80.0f;
		}

		uint32_t held = m_hold_key;

		if (released != 0 && (released & held) != 0) {
			if ((inputdevice::pad::GetMask() & released) != 0) {
				inputdevice::ScePafMisc_CB4DB658(this, s_pad_port, NULL);
				PostPadEvent(20, s_pad_port, released, false, mode);
			} else {
				PostPadEvent(18, s_pad_port, released, false, mode);
			}
			held = curr_mask;
			m_hold_key = curr_mask;
		}
		if (held != 0) {
			float delay = m_repeat_delay;
			float tick = PadTick();

			if (delay <= 0.0f) {
				float interval = m_repeat_interval - tick * 1000.0f;

				m_repeat_interval = interval;
				if (interval <= 0.0f) {
					PostPadEvent(19, s_pad_port, held, false, mode);
					m_repeat_count++;
					held = m_hold_key;
					if ((float)m_repeat_count < 10.0f)
						m_repeat_interval = 80.0f;
					else
						m_repeat_interval = 40.0f;
				}
			} else {
				float remain = delay - tick * 1000.0f;

				m_repeat_delay = remain;
				if (remain <= 0.0f) {
					PostPadEvent(19, s_pad_port, held, false, mode);
					held = m_hold_key;
					m_repeat_count++;
				}
			}
		}
		s_pad_curr[s_pad_port].buttons = held;
		m_mode = mode;
		if (held == 0)
			m_mode = 0;
	}
}

/* a key event of the type queued for the key code (inline in Sony's UIPadHoldListener::
   PostPadEvent; name ours) */
static inline void PostKeyEvent(Environment *env, uint32_t type, uint32_t detail)
{
	Event e(type, 0x100000, 0x40000, 0, 0, 0, 0, 0);

	e.SetDetail(detail);
	env->m_ev_queue.PushBack(e);
}

int32_t UIPadHoldListener::PostPadEvent(uint32_t type, uint32_t port, uint32_t key, bool flag,
					int32_t mode)
{
	uint32_t detail = ((port & 0xF) << 20) | ((flag & 1) << 24);

	if ((key & 1) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 1);
	if ((key & 2) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 2);
	if ((key & 4) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 3);
	if ((key & 8) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 4);
	if ((key & 0x10) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x1B);
	if ((key & 0x20) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x1C);
	if ((key & 0x40) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x1D);
	if ((key & 0x80) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x1E);
	if ((key & 0x1000) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x27);
	if ((key & 0x400) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x1F);
	if ((key & 0x800) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x20);
	if ((key & 0x10000) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x21);
	if ((key & 0x20000) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x22);
	if ((key & 0x40000) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x23);
	if ((key & 0x80000) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x24);
	if ((key & 0x100) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x26);
	if ((key & 0x200) != 0 && m_env != NULL)
		PostKeyEvent(m_env, type, detail | 0x25);
	return 0;
}

void UITouchListener::Init()
{
	uint32_t width, height;

	m_touch_down = 0;

	Screen *screen = m_env->m_screen;

	inputdevice::touchscreen::GetScreenResolution(width, height);
	m_screen_width = width;
	m_screen_height = height;

	math::rect vp(screen->vp);

	m_scale_x = vp.width() / (float)m_screen_width;

	m_scale_y = vp.height() / (float)m_screen_height;
	for (int32_t i = 0; i < BIND_NUM; i++)
		m_binded_widget[i] = NULL;
}

void UITouchListener::OnUpdate(const inputdevice::Data *data)
{
	if (data == NULL)
		return;

	for (int32_t i = 0; i < PORT_NUM; i++)
		PreUpdateGesturePort(i);

	const inputdevice::touchscreen::Data *touch;

	if (m_port == 0) {
		touch = data->m_touch_data;
		m_touch_pre = data->m_touch_data_pre;
		m_touch = touch;
	} else if (m_port == 1) {
		touch = data->m_touch_rear_data;
		m_touch_pre = data->m_touch_rear_data_pre;
		m_touch = touch;
	} else {
		return;
	}

	uint32_t bit = 1;

	for (int32_t i = 0; i < PORT_NUM; i++, bit *= 2, touch++) {
		int32_t state = touch->state;
		uint32_t down = m_touch_down;
		uint32_t type;

		if (state == 0 && (down & bit) == 0)
			continue;

		if (state <= 2) {
			if (state > 0) {
				if (state <= 1) {
					m_touch_down = down | bit;
					type = 65;
				} else {
					m_touch_down = down & ~bit;
					type = 66;
				}
			} else {
				m_touch_down = down | bit;
				type = 67;
			}
		} else if (state != 4) {
			m_touch_down = down | bit;
			type = 67;
		} else {
			m_touch_down = down & ~bit;
			type = 71;
			inputdevice::ScePafMisc_CB4DB658(this, i, NULL);
		}

		Event e(type, 0x200000, 0x10000, 0, touch->center_orig_x, touch->center_orig_y,
			touch->force, 0);

		e.m_detail = (uint8_t)i | 0xD0000 | ((m_port & 0xF) << 20);
		m_env->m_ev_queue.PushBack(e);
	}
}


/* the detail of a Bluetooth remote's event: the key code in the low byte and a 4 in bits 16 to
   19 (names ours) */
union BtrmDetail {
	uint32_t value;
	struct {
		uint32_t keycode : 8;
		uint32_t reserved : 8;
		uint32_t kind : 4;
		uint32_t reserved2 : 12;
	} f;
};

int32_t UIBtrmListener::PostBtrmEvent(uint32_t key, int32_t state)
{
	int32_t code;

	if (m_env == NULL)
		return -1;
	if (key > 16) {
		if (key > 32) {
			if (key != 64)
				return -1;
			if (state == 0)
				code = 8;
			else if (state == 1)
				code = 9;
			else if (state == 2)
				code = 10;
			else
				return -1;
		} else if (key > 31) {
			if (state == 0)
				code = 5;
			else if (state == 1)
				code = 6;
			else if (state == 2)
				code = 7;
			else
				return -1;
		} else {
			return -1;
		}
	} else if (key <= 8) {
		switch (key) {
		case 1:
			code = 0;
			break;
		case 2:
			code = 1;
			break;
		case 4:
			code = 2;
			break;
		case 8:
			code = 3;
			break;
		default:
			return -1;
		}
	} else if (key <= 15) {
		return -1;
	} else {
		code = 4;
	}
	if (m_target != NULL) {
		Event e(0x100, 0x400000, 0x10000, 0, 0, 0, 0, 0);
		BtrmDetail detail;

		e.SetTarget(m_target);
		detail.value = e.m_detail;
		detail.f.keycode = code;
		detail.f.kind = 4;
		e.m_detail = detail.value;
		m_env->m_ev_queue.PushBack(e);
	} else {
		Event e(0x100, 0x100000, 0x10000, 0, 0, 0, 0, 0);
		BtrmDetail detail;

		detail.value = e.m_detail;
		detail.f.keycode = code;
		detail.f.kind = 4;
		e.m_detail = detail.value;
		m_env->m_ev_queue.PushBack(e);
	}
	return 0;
}

void UIBtrmListener::OnUpdate(const inputdevice::Data *data)
{
	uint32_t curr = data->m_btrm_data->buttons;
	uint32_t prev = data->m_btrm_data_pre->buttons;
	uint32_t pressed = curr & ~prev;
	uint32_t released = prev & ~curr;

	if (pressed != 0) {
		uint32_t first = 1;
		uint32_t held = m_hold_key;

		if ((pressed & 1) == 0) {
			uint32_t next;

			do {
				next = pressed & (2 * first);
				first *= 2;
			} while (next == 0);
		}
		if (held != 0 && (held == 32 || held == 64))
			PostBtrmEvent(held, 1);
		m_hold_key = first;
		PostBtrmEvent(first, 0);
	}
	if (released != 0) {
		uint32_t held = m_hold_key;

		if ((released & held) != 0) {
			if (held == 32 || held == 64) {
				if ((inputdevice::btrm::GetMask() & held) != 0)
					PostBtrmEvent(held, 2);
				else
					PostBtrmEvent(held, 1);
			}
			m_hold_key = 0;
		}
	}
}

int32_t UIHidrmListener::PostHidrmEvent(uint32_t key, int32_t state)
{
	int32_t code;

	if (m_env == NULL)
		return -1;
	switch (key) {
	case 6:
		if (state == 0)
			code = 14;
		else if (state == 1)
			code = 15;
		else if (state == 2)
			code = 16;
		else
			return -1;
		break;
	case 7:
		if (state == 0)
			code = 11;
		else if (state == 1)
			code = 12;
		else if (state == 2)
			code = 13;
		else
			return -1;
		break;
	case 8:
		if (state == 0)
			code = 8;
		else if (state == 1)
			code = 9;
		else if (state == 2)
			code = 10;
		else
			return -1;
		break;
	case 9:
		if (state == 0)
			code = 5;
		else if (state == 1)
			code = 6;
		else if (state == 2)
			code = 7;
		else
			return -1;
		break;
	case 10:
		if (state == 0)
			code = 20;
		else if (state == 1)
			code = 21;
		else if (state == 2)
			code = 22;
		else
			return -1;
		break;
	case 11:
		if (state == 0)
			code = 17;
		else if (state == 1)
			code = 18;
		else if (state == 2)
			code = 19;
		else
			return -1;
		break;
	case 1:
		code = 0;
		break;
	case 2:
		code = 1;
		break;
	case 3:
		code = 2;
		break;
	case 4:
		code = 4;
		break;
	case 5:
		code = 3;
		break;
	default:
		if (key < 12 || key > 49)
			return -1;
		code = key + 11;
		break;
	}
	if (m_target != NULL) {
		Event e(0x100, 0x400000, 0x10000, 0, 0, 0, 0, 0);
		BtrmDetail detail;

		e.SetTarget(m_target);
		detail.value = e.m_detail;
		detail.f.keycode = code;
		detail.f.kind = 5;
		e.m_detail = detail.value;
		m_env->m_ev_queue.PushBack(e);
	} else {
		Event e(0x100, 0x100000, 0x10000, 0, 0, 0, 0, 0);
		BtrmDetail detail;

		detail.value = e.m_detail;
		detail.f.keycode = code;
		detail.f.kind = 5;
		e.m_detail = detail.value;
		m_env->m_ev_queue.PushBack(e);
	}
	return 0;
}

void UIHidrmListener::OnUpdate(const inputdevice::Data *data)
{
	for (int32_t i = 0; i < inputdevice::hidrm::Data::KEY_MAX; i++) {
		uint32_t curr = data->m_bdrmc_data->keycode[i];
		uint32_t prev = data->m_bdrmc_data_pre->keycode[i];
		bool pressed = curr != 0;
		bool released = prev != 0;

		for (int32_t j = 0; j < inputdevice::hidrm::Data::KEY_MAX; j++) {
			pressed = pressed && curr != data->m_bdrmc_data_pre->keycode[j];
			released = released && data->m_bdrmc_data->keycode[j] != prev;
		}

		if (pressed)
			PostHidrmEvent(curr, 0);
		if (released && (prev == 9 || prev == 8)) {
			inputdevice::hidrm::Data locked = inputdevice::hidrm::GetLockedData();

			if (locked.keycode[0] == prev)
				PostHidrmEvent(prev, 2);
			else
				PostHidrmEvent(prev, 1);
		}
	}
}

void UIMotionListener::OnUpdate(const inputdevice::Data *data)
{
}

void UIAnalogListener::OnUpdate(const inputdevice::Data *data)
{
}

}
}
