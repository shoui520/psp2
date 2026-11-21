/*
 * The gesture listeners the render plane attaches to a view plane's touches: the multi-touch
 * listener whose gestures, with the L and R buttons, the view's finger pointer follows, and the
 * listener that smooths a touch's place. Sony's file name is not known (the module has no debug
 * records); this one and every name in it are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <inputdevice/pad/pad.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <std/stdlib.h>
#include "web_view.h"

namespace paf {
namespace ui {

/* the touch is held by a listener, and the listener (inline in Sony's; names ours) */
static inline bool IsCaptured(inputdevice::InputGestureListener *input, uint32_t id)
{
	return id < 8 && input->m_capture[id].get() != NULL;
}

static inline common::SharedPtr<inputdevice::GestureListener>
GetCapture(inputdevice::InputGestureListener *input, uint32_t id)
{
	if (id < 8)
		return input->m_capture[id];
	return common::SharedPtr<inputdevice::GestureListener>();
}

/* the touch held by another listener than this one (inline in Sony's; name ours) */
static inline bool CapturedByOther(TouchGestureListener *listener, uint32_t id)
{
	inputdevice::InputGestureListener *input = listener->m_input;

	if (IsCaptured(input, id)) {
		common::SharedPtr<inputdevice::GestureListener> held = GetCapture(input, id);

		return held.get() != listener;
	}
	return false;
}

WebViewMultiTouchListener::WebViewMultiTouchListener(uint32_t port, uint32_t touch_id,
						     void *user_data)
	: MultiTouchGestureListener(port, touch_id, user_data, 2), m_state(0), m_lPre(false),
	  m_rPre(false), m_button(2), m_second(false)
{
	inputdevice::touchscreen::DeviceInfo info;

	inputdevice::touchscreen::GetDeviceInfo(0, &info);
}

WebViewMultiTouchListener::~WebViewMultiTouchListener()
{
	bool other = m_input != NULL && CapturedByOther(this, m_touch_id);

	if (m_state == 2 || m_state == 3)
		ScePafWidget_C5939A92(this, m_gesture[4], m_touch_id, 0, 0, 0, 0, 0);
	(void)other;
}

int32_t WebViewMultiTouchListener::OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre,
						      int32_t diff)
{
	int32_t previous = m_touch_id;
	int32_t ret = 1;

	int32_t current = get_id_from_bitmap(m_touch_bitmap, 0);

	m_touch_id = current;
	if ((current & previous) < 0) {
		m_state = 0;
		return GestureListener::UPDATE_DETACH_SELF;
	}

	const inputdevice::touchscreen::Data *touch;

	inputdevice::touchscreen::GetData(m_port, current >= 0 ? current : previous, &touch);

	int32_t state = touch->state;
	bool pressed = false;

	if (m_touch_id != previous && touch != NULL) {
		pressed = true;
		m_x = touch->x;
		m_y = touch->y;
	}

	bool other = CapturedByOther(this, m_touch_id);
	int32_t count = 0;

	for (uint32_t bits = m_touch_bitmap; bits != 0; bits >>= 1) {
		if (bits & 1)
			count++;
	}

	const inputdevice::pad::Data *pad;
	const inputdevice::pad::Data *pad_pre;

	inputdevice::pad::Update(0.0f, &pad, &pad_pre);
	if (!inputdevice::pad::IsLocked())
		((inputdevice::pad::Data *)pad)->buttons |= inputdevice::pad::GetMask();

	uint32_t buttons = pad->buttons;

	if (buttons & inputdevice::pad::Data::PAD_L)
		m_l = true;
	else
		m_l = false;
	if (buttons & inputdevice::pad::Data::PAD_R)
		m_r = true;
	else
		m_r = false;

	if (m_state == 0 && (state == inputdevice::touchscreen::Data::STATE_PRESS ||
			     state == inputdevice::touchscreen::Data::STATE_MOVE))
		m_state = 1;
	if (m_state == 1) {
		if (count >= 2) {
			m_state = 0;
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else if (state == inputdevice::touchscreen::Data::STATE_PRESS ||
			   state == inputdevice::touchscreen::Data::STATE_MOVE) {
			if (m_l && !m_lPre) {
				m_button = 1;
				ret = 2;
				m_state = 2;
				ScePafWidget_C5939A92(this, m_gesture[0], m_touch_id, 1, 0, 0, 0, 0);
			} else if (m_r && !m_rPre) {
				m_button = 0;
				m_state = 3;
				ScePafWidget_C5939A92(this, m_gesture[0], m_touch_id, 0, 0, 0, 0, 0);
				ret = 2;
			} else if (!m_l && m_lPre) {
				m_button = 1;
				ret = 2;
				m_state = 2;
				ScePafWidget_C5939A92(this, m_gesture[0], m_touch_id, 1, 0, 0, 0, 0);
			} else if (!m_r && m_rPre) {
				m_button = 0;
				m_state = 3;
				ScePafWidget_C5939A92(this, m_gesture[0], m_touch_id, 0, 0, 0, 0, 0);
				ret = 2;
			} else if (m_button != 2) {
				ScePafWidget_C5939A92(this, m_gesture[1], m_touch_id, m_button, 0, 0, 0,
						      0);
				ret = 2;
			}
		} else if (state == inputdevice::touchscreen::Data::STATE_RELEASE) {
			if (m_state == 2 || m_state == 3)
				ScePafWidget_C5939A92(this, m_gesture[4], previous, m_button, 0, 0, 0,
						      0);
			m_state = 0;
			ret = GestureListener::UPDATE_DETACH_SELF;
		}
	} else if (m_state == 2) {
		if (state == inputdevice::touchscreen::Data::STATE_PRESS ||
		    state == inputdevice::touchscreen::Data::STATE_MOVE) {
			if (count >= 3) {
				m_state = 0;
				ScePafWidget_C5939A92(this, m_gesture[4], previous, m_button, 0, 0, 0, 0);
				ret = GestureListener::UPDATE_DETACH_SELF;
			} else if (count == 2) {
				ScePafWidget_C5939A92(this, m_gesture[1], m_touch_id, m_button, 0, 0, 0,
						      0);
				if (m_countPre == 2) {
					m_second = true;
					if (!m_secondPre)
						ScePafWidget_C5939A92(this, 13, m_touch_id + 1, 0, 0, 0, 0, 0);
					else
						ScePafWidget_C5939A92(this, 14, m_touch_id + 1, 0, 0, 0, 0, 0);
				}
				ret = 2;
			} else {
				m_second = false;
				if (pressed) {
					ScePafWidget_C5939A92(this, m_gesture[3], previous, 0, 0, 0, 0, 0);
					ScePafWidget_C5939A92(this, m_gesture[4], previous, 0, 0, 0, 0, 0);
					ret = GestureListener::UPDATE_DETACH_SELF;
				} else {
					if (m_countPre == 2 && m_secondPre)
						ScePafWidget_C5939A92(this, 15, m_touch_id + 1, 0, 0, 0, 0, 0);
					if ((m_r && !m_rPre) || (m_l && !m_lPre)) {
						m_state = 3;
						m_button = 0;
						ret = 2;
					} else {
						ScePafWidget_C5939A92(this, m_gesture[1], m_touch_id,
								      m_button, 0, 0, 0, 0);
						ret = 2;
					}
				}
			}
		} else if (state == inputdevice::touchscreen::Data::STATE_RELEASE) {
			m_state = 0;
			ScePafWidget_C5939A92(this, m_gesture[3], previous, 0, 0, 0, 0, 0);
			ScePafWidget_C5939A92(this, m_gesture[4], previous, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
		}
	} else if (m_state == 3) {
		if (state == inputdevice::touchscreen::Data::STATE_PRESS ||
		    state == inputdevice::touchscreen::Data::STATE_MOVE) {
			if (count >= 3) {
				m_state = 0;
				ScePafWidget_C5939A92(this, m_gesture[4], previous, m_button, 0, 0, 0, 0);
				ret = GestureListener::UPDATE_DETACH_SELF;
			} else if (count == 2) {
				ScePafWidget_C5939A92(this, m_gesture[1], m_touch_id, m_button, 0, 0, 0,
						      0);
				if (m_countPre == 2) {
					m_second = true;
					if (!m_secondPre)
						ScePafWidget_C5939A92(this, 13, m_touch_id + 1, 0, 0, 0, 0, 0);
					else
						ScePafWidget_C5939A92(this, 14, m_touch_id + 1, 0, 0, 0, 0, 0);
				}
				ret = 2;
			} else {
				m_second = false;
				if (pressed) {
					ScePafWidget_C5939A92(this, m_gesture[3], previous, 0, 0, 0, 0, 0);
					ScePafWidget_C5939A92(this, m_gesture[4], previous, 0, 0, 0, 0, 0);
					ret = GestureListener::UPDATE_DETACH_SELF;
				} else {
					if (m_countPre == 2 && m_secondPre)
						ScePafWidget_C5939A92(this, 15, m_touch_id + 1, 0, 0, 0, 0, 0);
					if ((m_r && !m_rPre) || (m_l && !m_lPre)) {
						m_button = 1;
						ret = 2;
						m_state = 2;
					} else {
						ScePafWidget_C5939A92(this, m_gesture[1], m_touch_id,
								      m_button, 0, 0, 0, 0);
						ret = 2;
					}
				}
			}
		} else if (state == inputdevice::touchscreen::Data::STATE_RELEASE) {
			m_state = 0;
			ScePafWidget_C5939A92(this, m_gesture[3], previous, 0, 0, 0, 0, 0);
			ScePafWidget_C5939A92(this, m_gesture[4], previous, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
		}
	}
	(void)other;
	if (ret & GestureListener::UPDATE_DETACH_SELF)
		m_state = 0;
	m_lPre = m_l;
	m_rPre = m_r;
	m_countPre = count;
	m_secondPre = m_second;
	return ret;
}

WebViewTouchListener::WebViewTouchListener(uint32_t port, uint32_t touch_id, void *user_data)
	: TouchGestureListener(port, touch_id, user_data), m_state(0)
{
	m_pos[0] = 0.0f;
	m_pos[1] = 0.0f;
}

WebViewTouchListener::~WebViewTouchListener()
{
}

/* the axis jitters by a pixel: three runs of one place each, a pixel apart, the last two places
   before them not one (the seven patterns of Sony's, as they are) */
#define H(k)	m_history[k][i]
#define ABS(a, b)	sce_paf_abs((int32_t)((a) - (b)))

int32_t WebViewTouchListener::OnUpdateTouch(uint32_t id)
{
	if (IsCaptured(m_input, m_touch_id))
		return GestureListener::UPDATE_DETACH_SELF;

	int32_t ret = 1;
	const inputdevice::touchscreen::Data *touch;

	inputdevice::touchscreen::GetData(m_port, id, &touch);

	int32_t state = touch->state;
	int32_t x = touch->center_orig_x;
	int32_t y = touch->center_orig_y;

	if (m_state == 0) {
		if (state == inputdevice::touchscreen::Data::STATE_PRESS ||
		    state == inputdevice::touchscreen::Data::STATE_MOVE) {
			m_state = 1;
			m_cur[0] = (float)x;
			m_cur[1] = (float)y;
			for (int32_t i = 0; i < 2; i++) {
				m_jitter[i] = false;
				m_jitterPre[i] = false;
				m_jitterOn[i] = false;
				m_history[0][i] = m_cur[i];
				m_targetPre[i] = m_cur[i];
				m_hold[i] = false;
				m_smoothedPre[i] = m_cur[i];
				m_basePre[i] = m_cur[i];
				m_holdPre[i] = false;
			}
			m_pos[0] = m_cur[0];
			m_pos[1] = m_cur[1];
		}
	} else if (m_state == 1) {
		int32_t axes = 2;
		float curX = (float)x;
		float curY = (float)y;

		for (int32_t i = 0; i < axes; i++) {
			m_history[13][i] = m_history[12][i];
			m_history[12][i] = m_history[11][i];
			m_history[11][i] = m_history[10][i];
			m_history[10][i] = m_history[9][i];
			m_history[9][i] = m_history[8][i];
			m_history[8][i] = m_history[7][i];
			m_history[7][i] = m_history[6][i];
			m_history[6][i] = m_history[5][i];
			m_history[5][i] = m_history[4][i];
			m_history[4][i] = m_history[3][i];
			m_history[3][i] = m_history[2][i];
			m_history[2][i] = m_history[1][i];
			m_history[1][i] = m_history[0][i];
			m_history[0][i] = m_cur[i];
		}
		m_cur[0] = curX;
		m_cur[1] = curY;
		for (int32_t i = 0; i < 2; i++) {
			int32_t still = 1;

			if (H(0) == m_cur[i])
				still = m_stillCount[i] + 1;
			m_stillCount[i] = still;
			if (m_stillCount[i] == 1)
				m_lastRun[i] = m_stillCountPre[i];
			else
				m_lastRun[i] = m_lastRunPre[i];
			if (m_cur[i] == H(0) && H(0) == H(1) && H(1) == H(2) && H(3) == H(4) &&
			    H(4) == H(5) && H(6) == H(7) && ABS(m_cur[i], H(3)) == 1 &&
			    ABS(H(3), H(6)) == 1 && ABS(m_cur[i], H(6)) == 2 && H(7) != H(8) &&
			    H(8) != H(10))
				m_jitter[i] = true;
			else if (m_cur[i] == H(0) && H(0) == H(1) && H(1) == H(2) && H(3) == H(4) &&
				 H(4) == H(5) && H(6) == H(7) && H(7) == H(8) &&
				 ABS(m_cur[i], H(3)) == 1 && ABS(H(3), H(6)) == 1 &&
				 ABS(m_cur[i], H(6)) == 2 && H(8) != H(9) && H(9) != H(11))
				m_jitter[i] = true;
			else if (m_cur[i] == H(0) && H(0) == H(1) && H(1) == H(2) && H(2) == H(3) &&
				 H(4) == H(5) && H(6) == H(7) && H(8) == H(9) && H(9) == H(10) &&
				 ABS(m_cur[i], H(4)) == 1 && ABS(H(4), H(8)) == 1 &&
				 ABS(m_cur[i], H(8)) == 2 && H(10) != H(11) && H(11) != H(13))
				m_jitter[i] = true;
			else if (m_cur[i] == H(0) && H(0) == H(1) && H(1) == H(2) && H(2) == H(3) &&
				 H(4) == H(5) && H(6) == H(7) && H(8) == H(9) &&
				 ABS(m_cur[i], H(4)) == 1 && ABS(H(4), H(8)) == 1 &&
				 ABS(m_cur[i], H(8)) == 2 && H(9) != H(10) && H(10) != H(12))
				m_jitter[i] = true;
			else if (m_cur[i] == H(0) && H(0) == H(1) && H(2) == H(3) && H(3) == H(4) &&
				 H(5) == H(6) && H(6) == H(7) && ABS(m_cur[i], H(2)) == 1 &&
				 ABS(H(2), H(5)) == 1 && ABS(m_cur[i], H(5)) == 2 && H(7) != H(8) &&
				 H(8) != H(10))
				m_jitter[i] = true;
			else if (m_cur[i] == H(0) && H(0) == H(1) && H(2) == H(3) && H(3) == H(4) &&
				 H(5) == H(6) && ABS(m_cur[i], H(2)) == 1 && ABS(H(2), H(5)) == 1 &&
				 ABS(m_cur[i], H(5)) == 2 && H(6) != H(7) && H(7) != H(9))
				m_jitter[i] = true;
			else if (m_cur[i] == H(0) && H(0) == H(1) && H(2) == H(3) && H(4) == H(5) &&
				 ABS(m_cur[i], H(2)) == 1 && ABS(H(2), H(4)) == 1 &&
				 ABS(m_cur[i], H(4)) == 2 && H(5) != H(6) && H(6) != H(8))
				m_jitter[i] = true;
			else
				m_jitter[i] = false;
			if (m_jitterPre[i])
				m_jitterOn[i] = false;
			else
				m_jitterOn[i] = m_jitter[i];
		}
		if (m_lastRun[0] < m_lastRunPre[0])
			m_moved[0] = true;
		else if (sce_paf_abs((int32_t)(m_history[2][0] - m_cur[0])) > 1 ||
			 sce_paf_abs((int32_t)(m_history[2][1] - m_cur[1])) > 1)
			m_moved[0] = true;
		else if ((m_history[0][0] != m_cur[0] && m_history[1][0] != m_history[0][0]) ||
			 (m_history[0][1] != m_cur[1] && m_history[1][1] != m_history[0][1]))
			m_moved[0] = true;
		else
			m_moved[0] = false;
		if (m_lastRun[1] < m_lastRunPre[1])
			m_moved[1] = true;
		else if (__builtin_fabsf(m_history[3][0] - m_cur[0]) > 1.0f ||
			 __builtin_fabsf(m_history[3][1] - m_cur[1]) > 1.0f)
			m_moved[1] = true;
		else if ((m_history[0][0] != m_cur[0] && m_history[1][0] != m_history[0][0]) ||
			 (m_history[0][1] != m_cur[1] && m_history[1][1] != m_history[0][1]))
			m_moved[1] = true;
		else
			m_moved[1] = false;
		for (int32_t i = 0; i < 2; i++) {
			if (m_jitterOn[i])
				m_hold[i] = true;
			else if (m_moved[i])
				m_hold[i] = false;
			else if (m_holdPre[i])
				m_hold[i] = true;
			else
				m_hold[i] = false;
			if (m_hold[i])
				m_base[i] = m_basePre[i];
			else
				m_base[i] = m_cur[i];
		}
		if ((m_holdPre[0] && !m_hold[0]) || (m_holdPre[1] && !m_hold[1])) {
			for (int32_t i = 0; i < axes; i++)
				m_target[i] = (m_targetPre[i] + (m_base[i] - m_cur[i])) +
					      (m_cur[i] - m_history[0][i]);
		} else {
			for (int32_t i = 0; i < axes; i++)
				m_target[i] = m_targetPre[i] + (m_base[i] - m_basePre[i]);
		}
		for (int32_t i = 0; i < 2; i++) {
			float d = m_history[0][i] - m_cur[i];

			if (d == 0.0f) {
				m_smoothed[i] = m_smoothedPre[i];
			} else if (__builtin_fabsf(d) < 20.0f) {
				m_smoothed[i] = m_smoothedPre[i] + (m_target[i] - m_targetPre[i]);
			} else {
				m_smoothed[i] = m_cur[i];
			}
			if (!m_hold[i] && m_holdPre[i] &&
			    __builtin_fabsf(m_cur[i] - m_smoothed[i]) > 0.0f) {
				float step = m_cur[i] - m_history[0][i];

				if (step > 0.0f)
					m_smoothed[i] = m_smoothed[i] + 1.0f;
				else if (step < 0.0f)
					m_smoothed[i] = m_smoothed[i] - 1.0f;
			}
		}
		if (__builtin_fabsf(m_cur[0] - m_smoothed[0]) > 20.0f ||
		    __builtin_fabsf(m_cur[1] - m_smoothed[1]) > 20.0f) {
			for (int32_t i = 0; i < axes; i++)
				m_smoothed[i] = m_cur[i];
		}
		for (int32_t i = 0; i < axes; i++)
			m_out[i] = m_smoothed[i];
		m_pos[0] = m_out[0];
		m_pos[1] = m_out[1];
		for (int32_t i = 0; i < axes; i++) {
			m_targetPre[i] = m_target[i];
			m_smoothedPre[i] = m_smoothed[i];
			m_stillCountPre[i] = m_stillCount[i];
			m_basePre[i] = m_base[i];
			m_lastRunPre[i] = m_lastRun[i];
			m_jitterPre[i] = m_jitter[i];
			m_holdPre[i] = m_hold[i];
		}
		if (state == inputdevice::touchscreen::Data::STATE_PRESS ||
		    state == inputdevice::touchscreen::Data::STATE_MOVE) {
		} else if (state == inputdevice::touchscreen::Data::STATE_RELEASE) {
			ret = GestureListener::UPDATE_DETACH_SELF;
			m_state = 0;
		}
	}
	return ret;
}

#undef H
#undef ABS

float WebViewTouchListener::GetX()
{
	return m_pos[0];
}

float WebViewTouchListener::GetY()
{
	return m_pos[1];
}

}
}
