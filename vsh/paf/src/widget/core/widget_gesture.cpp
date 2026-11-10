/*
 * The gesture listeners of the widget system (TOOL 0x81148608..0x81149fc4): the touch gesture
 * listeners a widget attaches to the Environment's. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <timer/timer.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/uiinputlistener.h>
#include <widget/core/widget.h>
#include <widget/core/widget_gesture.h>

namespace paf {
namespace ui {

/* the front touch panel's pixels per millimetre, read once (name ours) */
static float s_pixel_per_mm = -1.0f;

TouchGestureListener::TouchGestureListener(uint32_t port, uint32_t touch_id, void *user_data)
	: m_touch_id(touch_id), m_update_count(0), m_port(port)
{
	if (s_pixel_per_mm < 0.0f) {
		inputdevice::touchscreen::DeviceInfo info;

		inputdevice::touchscreen::GetDeviceInfo(0, &info);
		s_pixel_per_mm = info.pixel_per_mm;
	}
	SetUserData(user_data);
}

int32_t MultiTouchGestureListener::OnUpdate(const inputdevice::Data *data, uint32_t port_id)
{
	uint32_t attached = m_attached_bitmap;

	if (attached != m_attached_bitmap_pre) {
		const inputdevice::touchscreen::Data *touch;

		inputdevice::touchscreen::GetData(m_port, port_id, &touch);
		attached = m_attached_bitmap;
	}

	int32_t result;

	if (m_update_count != 0) {
		result = m_last_result;
		m_update_count++;
	} else {
		uint32_t bits = attached;
		uint32_t id = 0;
		int32_t held = 0;

		if (bits != 0) {
			do {
				if (bits & 1) {
					const inputdevice::touchscreen::Data *touch;

					inputdevice::touchscreen::GetData(m_port, id, &touch);
					if (touch != NULL &&
					    touch->state == inputdevice::touchscreen::Data::STATE_RELEASE)
						Detach(id);
				}
				bits >>= 1;
				id++;
			} while (bits != 0);
			attached = m_attached_bitmap;
		}

		uint32_t bitmap_pre = m_touch_bitmap;

		for (uint32_t b = bitmap_pre; b != 0; b >>= 1) {
			if (b & 1)
				held++;
		}

		int32_t active = RefreshActiveTouch(attached);

		result = OnUpdateMultiTouch(m_touch_bitmap, bitmap_pre, active - held);
		m_last_result = result;
		m_attached_bitmap_pre = m_attached_bitmap;
		m_update_count++;
	}
	return result;
}

int32_t MultiTouchGestureListener::RefreshActiveTouch(uint32_t bitmap)
{
	uint32_t touches = m_touch_bitmap;
	int32_t count = 0;

	if (m_attached_bitmap == m_attached_bitmap_pre) {
		for (; touches != 0; touches >>= 1) {
			if (touches & 1)
				count++;
		}
		return count;
	}

	uint32_t keep = touches & bitmap;
	uint32_t max = m_touch_count_max;
	uint32_t held = 0;

	m_touch_bitmap = keep;

	uint32_t added = bitmap & ~keep;

	for (uint32_t b = keep; b != 0; b >>= 1) {
		if (b & 1)
			held++;
	}
	if (held < max && added != 0) {
		for (uint8_t id = 0; held < max; added >>= 1) {
			if (added == 0)
				break;
			if (added & 1) {
				keep |= 1 << id;
				m_touch_bitmap = keep;
				held++;
			}
			id++;
		}
	} else if (held > max) {
		uint32_t mask = 0x8000;

		do {
			if (mask & keep) {
				keep &= ~mask;
				m_touch_bitmap = keep;
				held--;
			}
			mask >>= 1;
		} while (held > max);
	}
	for (; keep != 0; keep >>= 1) {
		if (keep & 1)
			count++;
	}
	return count;
}

extern "C" int32_t ScePafWidget_C5939A92(TouchGestureListener *listener, int32_t gesture,
					 uint32_t touch_id, int32_t arg, int32_t data2,
					 int32_t data3, int32_t front, int32_t flag)
{
	UIInputGestureListener *input = (UIInputGestureListener *)listener->m_input;

	if (input == NULL)
		return -1;

	Environment *env = input->m_env;

	if (env == NULL)
		return -1;

	Handler *target = (Handler *)listener->m_user_data;

	if (target == NULL || !inputdevice::GestureListener::IsValidUserData(target)) {
		uint32_t id = listener->m_touch_id;

		target = NULL;
		if (id < UIInputGestureListener::BIND_NUM)
			target = input->m_binded_widget[id];
		if (target == NULL)
			return -1;
	}

	Event e(0x80, target, 0x40000, 0, 0, 0, 0, 0);
	uint32_t detail = (uint8_t)listener->m_touch_id | (e.m_detail & 0xFFFFFF00);

	e.m_data[0] = touch_id;
	detail = (detail & 0xFF0FFFFF) | ((listener->m_port & 0xF) << 20);
	e.m_data[1] = arg;
	e.m_data[2] = data2;
	e.m_data[3] = data3;
	detail = (detail & 0xFFFF80FF) | ((gesture & 0x7F) << 8);
	e.m_detail = detail;
	if ((uint8_t)flag != 0)
		e.m_detail = detail | 0x8000;
	if ((uint8_t)front != 0)
		env->m_ev_queue.PushFront(e);
	else
		env->m_ev_queue.PushBack(e);
	return 0;
}

MoveGestureListener::MoveGestureListener(uint32_t port, uint32_t touch_id, void *user_data)
	: TouchGestureListener(port, touch_id, user_data), m_state(0), m_start_time(get_time_internal()),
	  m_touch(NULL), m_tap_count(0), m_tap_target(0), m_last_tap_count(0), m_start_x(0), m_start_y(0),
	  m_param(), m_hold_sent(false)
{
}

/* the milliseconds from the time to the other, as Sony counts them (name ours) */
static inline __attribute__((always_inline)) uint32_t ElapsedMs(uint64_t now, uint64_t start)
{
	uint64_t diff = now - start;
	float seconds = (float)(double)diff / 1000000.0f;

	return (uint32_t)(seconds * 1000.0f);
}

bool MoveGestureListener::IsMoved(int32_t x, int32_t y)
{
	float limit = m_param.m_move_tolerance * s_pixel_per_mm;

	if ((float)(y - m_start_y) * (float)(y - m_start_y) + (float)(x - m_start_x) * (float)(x - m_start_x) <=
	    limit * limit)
		return false;

	math::v4 tlbr = m_param.m_rect.get_TLBR();
	float top = tlbr.extract_x();
	float left = tlbr.extract_y();
	float bottom = tlbr.extract_z();
	float right = tlbr.extract_w();

	return m_tap_count != 0 || top - bottom <= 0.0f || (float)y > top || (float)y < bottom ||
	       (float)x < left || (float)x > right;
}

int32_t MoveGestureListener::OnUpdateTouch(uint32_t id)
{
	uint32_t touch_id = m_touch_id;

	if (touch_id < 8 && ((inputdevice::InputGestureListener *)m_input)->m_capture[touch_id].get() != NULL)
		return GestureListener::UPDATE_DETACH_SELF;

	const inputdevice::touchscreen::Data *touch;

	inputdevice::touchscreen::GetData(m_port, id, &touch);

	int32_t state = touch->state;
	int32_t x = touch->center_orig_x;
	int32_t y = touch->center_orig_y;

	m_touch = touch;
	switch (m_state) {
	case 0:
		if (state != 1 && state != 3)
			return 1;
		m_start_x = x;
		m_state = 1;
		m_tap_count = 0;
		m_start_y = y;
		m_start_time = touch != NULL ? touch->timestamp : get_time_internal();
		return 1;
	case 1: {
		if (state == 3) {
			if (IsMoved(x, y)) {
				ScePafWidget_C5939A92(this, 1, m_start_x, m_start_y, 0, 0, 0, 0);
				return GestureListener::UPDATE_DETACH_SELF;
			}
			if (m_tap_target > 1 && !m_hold_sent) {
				uint64_t now = m_touch != NULL ? m_touch->timestamp : get_time_internal();

				if (m_param.m_interval_a < ElapsedMs(now, m_start_time)) {
					m_tap_count = 0;
					m_hold_sent = true;
					ScePafWidget_C5939A92(this, 25, m_start_x, m_start_y, 0, 0, 0, 0);
				}
			}
			return 1;
		}
		if (state != 2)
			return 1;
		if (IsMoved(x, y)) {
			ScePafWidget_C5939A92(this, 1, m_start_x, m_start_y, 0, 0, 0, 0);
			return GestureListener::UPDATE_DETACH_SELF;
		}

		uint32_t target = m_tap_target;

		if (target > 1 && m_tap_count != 0) {
			uint64_t now = m_touch != NULL ? m_touch->timestamp : get_time_internal();

			if (m_param.m_hold_time < ElapsedMs(now, m_start_time))
				return GestureListener::UPDATE_DETACH_SELF;
		}
		m_state = 2;

		uint32_t taps = m_tap_count + 1;

		m_tap_count = taps;
		m_start_x = x;
		m_start_y = y;
		if (taps != target && !m_hold_sent) {
			m_last_tap_count = taps;
			m_start_time = m_touch != NULL ? m_touch->timestamp : get_time_internal();
			return 1;
		}
		if (taps == 1)
			ScePafWidget_C5939A92(this, 0, x, y, 1, 0, 0, 0);
		else
			ScePafWidget_C5939A92(this, 2, x, y, taps, 0, 0, 0);
		return 112;
	}
	case 2:
		if (state == 1) {
			float limit = m_param.m_move_scale * s_pixel_per_mm;

			if ((float)(y - m_start_y) * (float)(y - m_start_y) +
			    (float)(x - m_start_x) * (float)(x - m_start_x) > limit * limit)
				return GestureListener::UPDATE_DETACH_SELF;

			uint32_t wait = m_param.m_interval_b;
			uint64_t now = touch != NULL ? touch->timestamp : get_time_internal();

			if (wait < ElapsedMs(now, m_start_time))
				return GestureListener::UPDATE_DETACH_SELF;
			m_start_x = x;
			m_state = 1;
			m_start_y = y;
			m_start_time = m_touch != NULL ? m_touch->timestamp : get_time_internal();
			return 1;
		}
		{
			uint32_t wait = m_param.m_interval_b;
			uint64_t now = touch != NULL ? touch->timestamp : get_time_internal();

			if (wait >= ElapsedMs(now, m_start_time))
				return 1;
		}
		if (m_last_tap_count == 1)
			ScePafWidget_C5939A92(this, 0, m_start_x, m_start_y, 1, 0, 0, 0);
		else
			ScePafWidget_C5939A92(this, 2, m_start_x, m_start_y, m_last_tap_count, 0, 0, 0);
		return 112;
	default:
		return 1;
	}
}

/* the touch's capture by a listener other than this one (inline in Sony's destructors and
   updates; name ours) */
static inline __attribute__((always_inline)) bool CapturedByOther(TouchGestureListener *listener,
								  uint32_t id)
{
	inputdevice::InputGestureListener *input = (inputdevice::InputGestureListener *)listener->m_input;

	if (input != NULL && id < 8 && input->m_capture[id].get() != NULL) {
		common::SharedPtr<inputdevice::GestureListener> held = input->m_capture[id];

		return held.get() != listener;
	}
	return false;
}

TouchTapListener::TouchTapListener(uint32_t port, uint32_t touch_id, void *user_data)
	: MultiTouchGestureListener(port, touch_id, user_data, 1), m_state(0), m_x(0), m_y(0),
	  reserved68(false), m_other(false), m_param()
{
}

TouchTapListener::~TouchTapListener()
{
	bool other = CapturedByOther(this, m_touch_id);

	if (m_state == 2) {
		if (other)
			ScePafWidget_C5939A92(this, 18, m_touch_id, 0, 0, 0, 0, 0);
		ScePafWidget_C5939A92(this, 15, m_touch_id, 0, 0, 0, 0, 1);
	} else if (m_state == 3) {
		ScePafWidget_C5939A92(this, 15, m_touch_id, 0, 0, 0, 0, 1);
	}
}

int32_t TouchTapListener::OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre, int32_t diff)
{
	int32_t previous = m_touch_id;
	int32_t ret = 1;

	m_touch_id = get_id_from_bitmap(m_touch_bitmap, 0);
	if ((m_touch_id & previous) < 0) {
		m_state = 0;
		return GestureListener::UPDATE_DETACH_SELF;
	}

	const inputdevice::touchscreen::Data *touch;

	inputdevice::touchscreen::GetData(m_port, m_touch_id >= 0 ? m_touch_id : previous, &touch);

	bool pressed = false;

	if (m_touch_id != previous && touch != NULL) {
		pressed = true;
		m_x = touch->center_orig_x;
		m_y = touch->center_orig_y;
	}

	bool other = CapturedByOther(this, m_touch_id);
	int32_t count = 0;

	for (uint32_t bits = m_touch_bitmap; bits != 0; bits >>= 1) {
		if (bits & 1)
			count++;
	}

	if (m_state <= 1) {
		if (m_state >= 0) {
			if (m_state <= 0) {
				if (touch->state == 1 || touch->state == 3) {
					m_state = 1;
					m_x = touch->center_orig_x;
					m_y = touch->center_orig_y;
				} else {
					ret = GestureListener::UPDATE_DETACH_SELF;
				}
			} else if (count > 0) {
				if (!other) {
					float dx = (float)(touch->center_orig_x - m_x);
					float dy = (float)(touch->center_orig_y - m_y);
					float limit = m_param.m_move_tolerance * s_pixel_per_mm;

					if (dx * dx + dy * dy >= limit * limit || pressed) {
						m_state = 2;
						ScePafWidget_C5939A92(this, 13, m_touch_id, 0, 0, 0, 0, 0);
						ret = GestureListener::UPDATE_DETACH_LOWER;
					}
				}
			} else {
				ret = GestureListener::UPDATE_DETACH_SELF;
			}
		}
	} else if (m_state <= 2) {
		if (other) {
			m_state = 3;
			if (count <= 0)
				m_touch_id = previous;
			ScePafWidget_C5939A92(this, 18, m_touch_id, 0, 0, 0, 0, 0);
		} else if (count > 0) {
			if (pressed)
				ScePafWidget_C5939A92(this, 17, m_touch_id, 0, 0, 0, 0, 0);
			ScePafWidget_C5939A92(this, 14, m_touch_id, 0, 0, 0, 0, 0);
		} else {
			m_touch_id = previous;
			m_state = 0;
			ScePafWidget_C5939A92(this, 15, previous, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
		}
	} else if (m_state <= 3) {
		if (count > 0) {
			if (!other) {
				m_state = 2;
				ScePafWidget_C5939A92(this, 17, m_touch_id, 0, 0, 0, 0, 0);
			}
		} else {
			m_touch_id = previous;
			m_state = 0;
			ScePafWidget_C5939A92(this, 15, previous, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
		}
	}
	m_other = other;
	if (ret & GestureListener::UPDATE_DETACH_SELF)
		m_state = 0;
	return ret;
}

TouchLongPressListener::TouchLongPressListener(uint32_t port, uint32_t touch_id, void *user_data)
	: MultiTouchGestureListener(port, touch_id, user_data, 2), m_state(0), m_id0(-1), m_id1(-1),
	  m_x0(0), m_y0(0), m_x1(0), m_y1(0), reserved84(false), m_param()
{
}

TouchLongPressListener::~TouchLongPressListener()
{
	bool other = CapturedByOther(this, m_id0);

	if (!other)
		other = CapturedByOther(this, m_id1);
	if (m_state == 2) {
		if (other)
			ScePafWidget_C5939A92(this, 24, m_id0, m_id1, 0, 0, 0, 0);
		ScePafWidget_C5939A92(this, 21, m_id0, m_id1, 0, 0, 0, 1);
	} else if (m_state == 3) {
		ScePafWidget_C5939A92(this, 21, m_touch_id, 0, 0, 0, 0, 1);
	}
}

int32_t TouchLongPressListener::OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre,
						   int32_t diff)
{
	int32_t previous0 = m_id0;
	int32_t previous1 = m_id1;
	int32_t ret = 1;

	m_id0 = get_id_from_bitmap(m_touch_bitmap, 0);
	m_id1 = get_id_from_bitmap(m_touch_bitmap, 1);
	if ((m_id0 & previous0) < 0) {
		m_state = 0;
		return GestureListener::UPDATE_DETACH_SELF;
	}

	const inputdevice::touchscreen::Data *touch0;
	const inputdevice::touchscreen::Data *touch1;

	inputdevice::touchscreen::GetData(m_port, m_id0 >= 0 ? m_id0 : previous0, &touch0);
	inputdevice::touchscreen::GetData(m_port, m_id1 >= 0 ? m_id1 : previous1, &touch1);

	bool pressed0 = false;
	bool pressed1 = false;

	if (m_id0 != previous0 && touch0 != NULL) {
		pressed0 = true;
		m_x0 = touch0->center_orig_x;
		m_y0 = touch0->center_orig_y;
	}
	if (m_id1 != previous1 && touch1 != NULL) {
		pressed1 = true;
		m_x1 = touch1->center_orig_x;
		m_y1 = touch1->center_orig_y;
	}

	bool other = CapturedByOther(this, m_id0);

	if (!other)
		other = CapturedByOther(this, m_id1);

	int32_t count = 0;

	for (uint32_t bits = m_touch_bitmap; bits != 0; bits >>= 1) {
		if (bits & 1)
			count++;
	}

	if (m_state <= 1) {
		if (m_state >= 0) {
			if (m_state <= 0) {
				if (touch0->state == 1 || touch0->state == 3)
					m_state = 1;
				else
					ret = GestureListener::UPDATE_DETACH_SELF;
			} else if (count > 0) {
				if (count != 1 && !other) {
					ret = 2;
					m_state = 2;
					ScePafWidget_C5939A92(this, 19, m_id0, m_id1, 0, 0, 0, 0);
				}
			} else {
				ret = GestureListener::UPDATE_DETACH_SELF;
			}
		}
	} else if (m_state > 2) {
		if (m_state <= 3) {
			if (count > 0) {
				if (count != 1 && !other) {
					ret = 2;
					m_state = 2;
					ScePafWidget_C5939A92(this, 23, m_id0, m_id1, 0, 0, 0, 0);
				}
			} else {
				m_state = 0;
				ScePafWidget_C5939A92(this, 21, previous0, previous1, 0, 0, 0, 0);
				ret = GestureListener::UPDATE_DETACH_SELF;
			}
		}
	} else if (other) {
		m_state = 3;
		if (count != 0)
			ScePafWidget_C5939A92(this, 24, m_id0, m_id1, 0, 0, 0, 0);
		else
			ScePafWidget_C5939A92(this, 24, previous0, previous1, 0, 0, 0, 0);
	} else if (count >= 2) {
		if (pressed0 || pressed1)
			ScePafWidget_C5939A92(this, 23, m_id0, m_id1, 0, 0, 0, 0);
		ScePafWidget_C5939A92(this, 20, m_id0, m_id1, 0, 0, 0, 0);
		ret = 2;
	} else if (count == 1) {
		m_state = 1;
		ScePafWidget_C5939A92(this, 21, previous0, previous1, 0, 0, 0, 0);
	} else if (count == 0) {
		m_state = 0;
		ScePafWidget_C5939A92(this, 21, previous0, previous1, 0, 0, 0, 0);
		ret = GestureListener::UPDATE_DETACH_SELF;
	}
	if (ret & GestureListener::UPDATE_DETACH_SELF)
		m_state = 0;
	return ret;
}

HoldGestureListener::HoldGestureListener(uint32_t port, uint32_t touch_id, void *user_data)
	: TouchGestureListener(port, touch_id, user_data), m_state(0), m_create_time(get_time_internal()),
	  m_touch(NULL), m_x(0), m_y(0), m_short_hold(false), m_param()
{
}

HoldGestureListener::~HoldGestureListener()
{
	if (m_state == 2)
		ScePafWidget_C5939A92(this, 6, m_x, m_y, 0, 0, 0, 0);
}

int32_t HoldGestureListener::OnUpdateTouch(uint32_t id)
{
	uint32_t touch_id = m_touch_id;

	if (touch_id < 8 && ((inputdevice::InputGestureListener *)m_input)->m_capture[touch_id].get() != NULL) {
		m_state = 0;
		return GestureListener::UPDATE_DETACH_SELF;
	}

	const inputdevice::touchscreen::Data *touch;

	inputdevice::touchscreen::GetData(m_port, id, &touch);

	int32_t state = touch->state;
	int32_t x = touch->center_orig_x;
	int32_t y = touch->center_orig_y;

	m_touch = touch;
	switch (m_state) {
	case 0:
		if (state == 1 || state == 3) {
			m_state = 1;

			uint64_t stamp = touch->timestamp;

			m_press_time = stamp;
			m_x = x;
			m_y = y;
			if (touch == NULL)
				m_create_time = get_time_internal();
			else
				m_create_time = stamp;
		}
		return 1;
	case 1: {
		if (state != 3) {
			if (m_press_time != touch->timestamp && (state == 1 || state == 2)) {
				m_state = 0;
				return GestureListener::UPDATE_DETACH_SELF;
			}
			return 1;
		}

		math::v4 tlbr = m_param.m_rect.get_TLBR();
		float top = tlbr.extract_x();
		float left = tlbr.extract_y();
		float bottom = tlbr.extract_z();
		float right = tlbr.extract_w();
		float dx = (float)(x - m_x);
		float dy = (float)(y - m_y);
		float limit = (m_param.m_move_tolerance * s_pixel_per_mm) * (m_param.m_move_tolerance * s_pixel_per_mm);
		const inputdevice::touchscreen::Data *last = m_touch;

		if (top - bottom <= 0.0f) {
			if (dy * dy + dx * dx > limit) {
				m_state = 0;
				return GestureListener::UPDATE_DETACH_SELF;
			}
		} else if (dy * dy + dx * dx > limit &&
			   ((float)y > top || (float)y < bottom || (float)x < left || (float)x > right)) {
			m_state = 0;
			return GestureListener::UPDATE_DETACH_SELF;
		}

		uint32_t hold_time = m_param.m_hold_time;
		uint64_t now = last != NULL ? last->timestamp : get_time_internal();

		if (hold_time < ElapsedMs(now, m_create_time)) {
			ScePafWidget_C5939A92(this, 4, m_x, m_y, 0, 0, 0, 0);
			if (m_short_hold) {
				m_create_time = m_touch != NULL ? m_touch->timestamp : get_time_internal();
				m_state = 2;
				return 96;
			}
			return m_param.m_hold_result;
		}
		return 1;
	}
	case 2: {
		if (state != 3) {
			if (state == 1 || state == 2) {
				ScePafWidget_C5939A92(this, 6, m_x, m_y, 0, 0, 0, 0);
				m_state = 0;
				return 112;
			}
			return 1;
		}

		math::v4 tlbr = m_param.m_rect.get_TLBR();
		float top = tlbr.extract_x();
		float left = tlbr.extract_y();
		float bottom = tlbr.extract_z();
		float right = tlbr.extract_w();
		float dx = (float)(x - m_x);
		float dy = (float)(y - m_y);
		float limit = (m_param.m_move_tolerance * s_pixel_per_mm) * (m_param.m_move_tolerance * s_pixel_per_mm);
		const inputdevice::touchscreen::Data *last = m_touch;
		bool left_region;

		if (top - bottom <= 0.0f)
			left_region = dy * dy + dx * dx > limit;
		else
			left_region = dy * dy + dx * dx > limit &&
				      ((float)y > top || (float)y < bottom || (float)x < left || (float)x > right);
		if (left_region) {
			ScePafWidget_C5939A92(this, 6, m_x, m_y, 0, 0, 0, 0);
			m_state = 0;
			return 112;
		}

		uint32_t repeat_time = m_param.m_repeat_time;
		uint64_t now = last != NULL ? last->timestamp : get_time_internal();

		if (repeat_time >= ElapsedMs(now, m_create_time))
			return 1;
		ScePafWidget_C5939A92(this, 5, m_x, m_y, 0, 0, 0, 0);
		m_create_time = m_touch != NULL ? m_touch->timestamp : get_time_internal();
		return 1;
	}
	default:
		return 1;
	}
}

}
}
