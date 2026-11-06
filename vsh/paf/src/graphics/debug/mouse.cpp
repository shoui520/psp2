/*
 * paf::gutil::debug::Mouse: a mouse made from the front touch panel. The first
 * finger is the first button and the first pointer, a second finger the third
 * button and the second pointer.
 */
#include <graphics/gutil.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <src/common/frame_count.h>

using namespace paf;
using namespace paf::gutil::debug;

enum {
	/* the front panel's slots */
	TOUCH_NUM = 8,
	/* the frame the touch handler is set in */
	HANDLER_FRAME = 4
};

static Mouse s_mouse;
/* the front panel's slots, as the touchscreen handed them after its last Update */
static inputdevice::touchscreen::Data s_touch[TOUCH_NUM];
/* the last frame a finger touched */
static uint32_t s_touch_frame = 0;

static void TouchHandler(inputdevice::touchscreen::Data *data);

Mouse::Mouse()
{
	for (int32_t i = 0; i < 3; i++)
		m_buttons[i].Clear();
	m_pointers[0].Clear();
	m_pointers[1].Clear();
	m_screen_width = 1;
	m_screen_height = 1;
}

void Mouse::Update()
{
	if (common::FrameCount() >= HANDLER_FRAME) {
		if (common::FrameCount() == HANDLER_FRAME) {
			inputdevice::touchscreen::SetDeviceHandler(inputdevice::touchscreen::PORT_FRONT,
								   TouchHandler);
		} else {
			/* a pointer touched down starts where it is */
			if (s_touch[0].state != 0) {
				if (s_touch_frame != common::FrameCount() - 1) {
					m_pointers[0].m_last_x = s_touch[0].x;
					m_pointers[0].m_last_y = s_touch[0].y;
				}
				m_pointers[0].m_x = s_touch[0].x;
				m_pointers[0].m_y = s_touch[0].y;
				m_buttons[0].m_down = true;
			} else {
				m_buttons[0].m_down = false;
			}
			if (s_touch[1].state != 0) {
				if (s_touch_frame != common::FrameCount() - 1) {
					m_pointers[1].m_last_x = s_touch[1].x;
					m_pointers[1].m_last_y = s_touch[1].y;
				}
				m_pointers[1].m_x = s_touch[1].x;
				m_pointers[1].m_y = s_touch[1].y;
				m_buttons[2].m_down = true;
			} else {
				m_buttons[2].m_down = false;
			}
			if (s_touch[0].state != 0 || s_touch[1].state != 0)
				s_touch_frame = common::FrameCount();
		}
	}
	for (int32_t i = 0; i < 3; i++)
		m_buttons[i].Update();
	m_pointers[0].Update();
	m_pointers[1].Update();
}

static void TouchHandler(inputdevice::touchscreen::Data *data)
{
	for (int32_t i = 0; i < TOUCH_NUM; i++)
		s_touch[i] = data[i];
}

Mouse *Mouse::GetInstance()
{
	return &s_mouse;
}
