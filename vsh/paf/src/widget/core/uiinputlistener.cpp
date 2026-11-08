/*
 * paf::ui::UITouchListener (TOOL 0x8114b3a6..0x8114b57e): the listener of a touch port, which
 * turns its touches into touch events. The file name is ScePaf-RE's (its DWARF file list).
 */
#include <stddef.h>
#include <stdint.h>
#include <inputdevice/inputdevice.h>
#include <math/rect.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/uiinputlistener.h>

namespace paf {
namespace ui {

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

}
}
