/*
 * The gesture listeners of the widget system (TOOL 0x81148608..0x81149fc4): the touch gesture
 * listeners a widget attaches to the Environment's. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/touchscreen/touchscreen.h>
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

}
}
