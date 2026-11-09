/*
 * The gesture listeners a widget attaches for a drag, for a rear drag, pinch and tap, and for
 * a rear touch held while the device is not still (ScePafWidget 712AD1C7, F4625FAD, 5E7757AA
 * and DE7EC3CF make them). No Sony name was found for the file, its classes or their fields:
 * the names are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <motion.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <std/math.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/widget_gesture.h>

using namespace paf;
using namespace paf::ui;

/* the milliseconds since the time, as a float (inline in Sony's; name ours) */
static inline __attribute__((always_inline)) float ElapsedMs(const SceUInt64 &start)
{
	float seconds = (float)(double)(get_time_internal() - start) / 1000000.0f;

	return seconds * 1000.0f;
}

/* the touch is held by another listener of the InputGestureListener (inline in Sony's; name
   ours) */
static inline __attribute__((always_inline)) bool HeldByOther(TouchGestureListener *listener,
							      uint32_t id)
{
	inputdevice::InputGestureListener *input = (inputdevice::InputGestureListener *)listener->m_input;
	bool other = false;

	if (id < 8 && input->m_capture[id].get() != NULL) {
		common::SharedPtr<inputdevice::GestureListener> held = input->m_capture[id];

		if (held.get() != listener)
			other = true;
	}
	return other;
}

/* the touches of the bitmap held by other listeners (inline in Sony's; name ours) */
static inline __attribute__((always_inline)) bool AnyHeldByOther(TouchGestureListener *listener,
								 uint32_t bitmap)
{
	bool other = false;
	uint32_t id = 0;

	for (; bitmap != 0; bitmap >>= 1, id++) {
		if ((bitmap & 1) != 0 && HeldByOther(listener, id))
			other = true;
	}
	return other;
}

/* the touches of the bitmap (inline in Sony's; name ours) */
static inline __attribute__((always_inline)) int32_t CountTouches(uint32_t bitmap)
{
	int32_t count = 0;

	for (; bitmap != 0; bitmap >>= 1) {
		if ((bitmap & 1) != 0)
			count++;
	}
	return count;
}

DragGestureListener::DragGestureListener(uint32_t port, uint32_t touch_id, void *user_data)
	: MultiTouchGestureListener(port, touch_id, user_data, 1), m_drag(NULL), m_param(),
	  m_flick(NULL), m_pixel_per_mm(0.0f), m_state(STATE_NONE), m_start_x(0.0f), m_start_y(0.0f),
	  m_x(0.0f), m_y(0.0f), m_prev_x(0.0f), m_prev_y(0.0f), m_straight(true)
{
	inputdevice::touchscreen::DeviceInfo info;

	inputdevice::touchscreen::GetDeviceInfo(0, &info);
	m_param.m_start_distance[0] = 20.0f;
	m_param.m_move_tolerance = 2.3f;
	m_pixel_per_mm = info.pixel_per_mm;
	m_param.m_start_distance[1] = 50.0f;
	m_param.m_end_distance[0] = 50.0f;
	m_param.m_start_distance[2] = 15.0f;
	m_param.m_end_distance[1] = 50.0f;
	m_param.m_end_distance[2] = 80.0f;
	m_param.m_end_speed[1] = 500.0f;
	m_param.m_end_speed[0] = 230.0f;
	m_flick = &DragFlickParam::Get();
}

DragGestureListener::~DragGestureListener()
{
	delete m_drag;
	m_drag = (GestureDragRatio *)-1;
}

void DragGestureListener::SetParam(const DragGestureParam *param)
{
	if (param != NULL && m_state == STATE_NONE)
		m_param = *param;
	if (m_param.m_move_tolerance < 1.1920929e-7f)
		m_param.m_move_tolerance = 2.3f;
	if (m_param.m_start_distance[0] < 1.1920929e-7f)
		m_param.m_start_distance[0] = 20.0f;
	if (m_param.m_start_distance[1] < 1.1920929e-7f)
		m_param.m_start_distance[1] = 50.0f;
	if (m_param.m_start_distance[2] < 1.1920929e-7f)
		m_param.m_start_distance[2] = 15.0f;
	if (m_param.m_end_distance[0] < 1.1920929e-7f)
		m_param.m_end_distance[0] = 50.0f;
	/* Sony's: the third end distance tested, the first set */
	if (m_param.m_end_distance[2] < 1.1920929e-7f)
		m_param.m_end_distance[0] = 80.0f;
	if (m_param.m_end_distance[1] < 1.1920929e-7f)
		m_param.m_end_distance[1] = 50.0f;
	if (m_param.m_end_speed[0] < 1.1920929e-7f)
		m_param.m_end_speed[0] = 230.0f;
	if (m_param.m_end_speed[1] < 1.1920929e-7f)
		m_param.m_end_speed[1] = 500.0f;
}

void DragGestureListener::GetParam(DragGestureParam *param)
{
	*param = m_param;
}

int32_t DragGestureListener::OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre,
						int32_t diff)
{
	int32_t ret = 1;
	int32_t count = CountTouches(m_attached_bitmap);
	int32_t id = get_id_from_bitmap(bitmap, 0);
	int32_t id_pre = get_id_from_bitmap(bitmap_pre, 0);
	const inputdevice::touchscreen::Data *touch;
	float x = 0.0f;
	float y = 0.0f;
	float x_pre = 0.0f;
	float y_pre = 0.0f;

	if (id >= 0) {
		inputdevice::touchscreen::GetData(m_port, id, &touch);
		x = (float)touch->center_orig_x;
		y = (float)touch->center_orig_y;
	}
	if (id_pre >= 0) {
		inputdevice::touchscreen::GetDataPre(m_port, id_pre, &touch);
		x_pre = (float)touch->center_orig_x;
		y_pre = (float)touch->center_orig_y;
	}

	if (id == id_pre) {
		float dy = y - m_y;
		float dx = x - m_x;
		float dist = dy * dy + dx * dx;
		float tolerance = m_param.m_move_tolerance * m_pixel_per_mm;

		if (dist > tolerance * tolerance) {
			float ax = __builtin_fabsf(x - m_start_x);
			float ay = __builtin_fabsf(y - m_start_y);
			float slope = sce_paf_tanf(m_flick->m_value[0] * 3.1415927f / 180.0f);
			int32_t dir = m_param.m_dir;
			float last_x = m_x;
			float last_y = m_y;
			float prev_x = m_prev_x;
			float prev_y = m_prev_y;

			if (dir == 2) {
				if (ay <= ax * slope) {
					m_param.m_dir = 0;
					dir = 0;
				} else if (ax <= ay * slope) {
					m_param.m_dir = 1;
					dir = 1;
				} else {
					m_straight = false;
				}
			}
			if (dir == 0) {
				if (ay > ax * slope)
					m_straight = false;
			} else if (dir == 1) {
				if (ax > ay * slope)
					m_straight = false;
			}

			float px = last_x - prev_x;
			float py = last_y - prev_y;
			float limit = sce_paf_cosf(m_flick->m_value[1] * 3.1415927f / 180.0f);

			last_x = m_x;
			last_y = m_y;
			if ((m_start_x != last_x || m_start_y != last_y) &&
			    (dy * py + dx * px) / __builtin_sqrtf(dist * (py * py + px * px)) < limit)
				m_straight = false;
			m_y = y;
			m_prev_x = last_x;
			m_prev_y = last_y;
			m_x = x;
		}
	}

	bool other = HeldByOther(this, m_touch_id);

	switch (m_state) {
	case STATE_NONE:
		if (count == 0 || other || count != 1) {
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else {
			m_start_x = x;
			m_start_y = y;
			m_x = x;
			m_y = y;
			m_press_time.m_start = get_time_internal();
			m_state = STATE_PRESS;
		}
		break;
	case STATE_PRESS:
		if (count == 0 || other || count != 1 || id != id_pre || !m_straight) {
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else {
			float sx = x - m_start_x;
			float sy = y - m_start_y;
			float tolerance = m_param.m_move_tolerance * m_pixel_per_mm;

			if (sy * sy + sx * sx <= tolerance * tolerance) {
				if (ElapsedMs(m_press_time.m_start) >= m_param.m_hold_time)
					ret = GestureListener::UPDATE_DETACH_SELF;
			} else {
				float vx;
				float vy;

				ScePafMisc_88F4A40B(m_port, id, 50, &vx, &vy);

				float speed = m_param.m_flick_speed * m_pixel_per_mm;

				if ((vy * 1000.0f) * (vy * 1000.0f) + (vx * 1000.0f) * (vx * 1000.0f) <=
				    speed * speed) {
					ret = GestureListener::UPDATE_DETACH_SELF;
				} else {
					vy = vy * 1000.0f;
					vx = vx * 1000.0f;

					float width = m_param.m_rect.width();
					float height = m_param.m_rect.height();

					if (width <= 0.0f || height <= 0.0f) {
						ret = GestureListener::UPDATE_DETACH_SELF;
					} else {
						m_drag = new GestureDragRatio(m_param.m_dir,
									      m_param.m_touch_count,
									      width, height,
									      m_pixel_per_mm,
									      m_param.m_end_distance[0],
									      m_param.m_end_distance[1]);
						if (m_drag == NULL || !m_drag->IsEnabled()) {
							ret = GestureListener::UPDATE_DETACH_SELF;
						} else {
							m_drag->Begin(m_start_x, m_start_y, x, y);
							m_drag_time.m_start = get_time_internal();
							m_drag_x = x;
							m_drag_y = y;
							m_state = STATE_DRAG;
							m_fast = false;
							m_faster = false;
							ScePafWidget_C5939A92(this, 7, 0, 0, 0, 0, 0, 0);
						}
					}
				}
			}
		}
		break;
	case STATE_DRAG:
		if (count == 0) {
			m_drag->End(x_pre, y_pre);
			ScePafWidget_C5939A92(this, 9, 0, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else if (count != 1 || id != id_pre) {
			m_drag->End(x_pre, y_pre);
			ScePafWidget_C5939A92(this, 9, 0, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else if (!m_straight) {
			m_drag->End(x, y);
			ScePafWidget_C5939A92(this, 9, 0, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else {
			bool far = false;
			bool farther = false;
			float dx = m_drag->GetX() - m_drag->GetStartX();
			float dy = m_drag->GetY() - m_drag->GetStartY();
			float scale = m_pixel_per_mm;
			float dist_x = m_param.m_start_distance[0] * scale;
			float dist_x2 = m_param.m_start_distance[1] * scale;
			float dist_y = m_param.m_start_distance[2] * scale;

			switch (m_param.m_dir) {
			case 0:
				if (dx * dx > dist_x * dist_x)
					far = true;
				if (dx * dx > dist_x2 * dist_x2)
					farther = true;
				break;
			case 1:
				if (dy * dy > dist_y * dist_y)
					far = true;
				break;
			case 2:
				if (dy * dy + dx * dx > dist_y * dist_y + dist_x * dist_x)
					far = true;
				break;
			}
			if (!m_faster) {
				float vx;
				float vy;

				ScePafMisc_88F4A40B(m_port, id, 50, &vx, &vy);

				float fast = m_param.m_end_speed[0] * m_pixel_per_mm;
				float faster = m_param.m_end_speed[1] * m_pixel_per_mm;

				vx = vx * 1000.0f;
				vy = vy * 1000.0f;

				float speed = vy * vy + vx * vx;

				if (speed > fast * fast)
					m_fast = true;
				if (speed > faster * faster)
					m_faster = true;
			}
			if (ElapsedMs(m_drag_time.m_start) > m_flick->m_value[2] && far) {
				if (farther) {
					if (!m_faster) {
						m_drag->End(x, y);
						ScePafWidget_C5939A92(this, 9, 0, 0, 0, 0, 0, 0);
						ret = GestureListener::UPDATE_DETACH_SELF;
						break;
					}
					m_drag->SetFarX(m_param.m_end_distance[2]);
				} else if (!m_fast) {
					m_drag->End(x, y);
					ScePafWidget_C5939A92(this, 9, 0, 0, 0, 0, 0, 0);
					ret = GestureListener::UPDATE_DETACH_SELF;
					break;
				}
				m_drag->Move(x, y);
				m_flick_time.m_start = get_time_internal();
				m_state = STATE_FLICK;
				ScePafWidget_C5939A92(this, 10, 0, 0, 0, 0, 0, 0);
				ret = GestureListener::UPDATE_CAPTURE;
			} else {
				m_drag->Move(x, y);
				ScePafWidget_C5939A92(this, 8, 0, 0, 0, 0, 0, 0);
			}
		}
		break;
	case STATE_FLICK:
		if (count == 0) {
			m_drag->End(x_pre, y_pre);
			ScePafWidget_C5939A92(this, 12, 0, 0, 0, 0, 0, 0);
			ret = 18;
		} else if (id == id_pre) {
			m_drag->Move(x, y);
			ScePafWidget_C5939A92(this, 11, 0, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_CAPTURE;
		} else {
			m_drag->End(x_pre, y_pre);
			m_state = STATE_FLICK_END;
			ScePafWidget_C5939A92(this, 12, 0, 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_CAPTURE;
		}
		break;
	case STATE_FLICK_END:
		ret = count == 0 ? 18 : GestureListener::UPDATE_CAPTURE;
		break;
	}
	return ret;
}

int32_t DragGestureListener::GetDragState(float *start_x, float *start_y, float *x, float *y,
					  float *ratio)
{
	int32_t ret = 0;

	if (m_state == STATE_DRAG || m_state == STATE_FLICK) {
		if (m_drag != NULL) {
			*start_x = m_drag->GetStartX();
			*start_y = m_drag->GetStartY();
			*x = m_drag->GetX();
			*y = m_drag->GetY();
			*ratio = 1.0f;
			ret = 1;
		}
		if (ret != 0) {
			*ratio = 1.0f;
			if (m_state == STATE_DRAG) {
				float dx = m_drag->GetX() - m_drag->GetStartX();
				float dy = m_drag->GetY() - m_drag->GetStartY();
				float scale = m_pixel_per_mm;
				float dist_x = m_param.m_start_distance[0] * scale;
				float dist_y = m_param.m_start_distance[2] * scale;

				switch (m_param.m_dir) {
				case 0:
					if (dist_x > 1.1920929e-7f)
						*ratio = (dx / dist_x) * (dx / dist_x);
					break;
				case 1:
					if (dist_y > 1.1920929e-7f)
						*ratio = (dy / dist_y) * (dy / dist_y);
					break;
				case 2:
					if (dist_x > 1.1920929e-7f || dist_y > 1.1920929e-7f)
						*ratio = (dy * dy + dx * dx) /
							 (dist_y * dist_y + dist_x * dist_x);
					break;
				}
				if (*ratio > 1.0f)
					*ratio = 1.0f;

				float ease;

				if (m_flick->m_value[4] >= m_flick->m_value[2]) {
					float ms = ElapsedMs(m_drag_time.m_start);

					ease = 1.0f;
					if (ms < m_flick->m_value[2])
						ease = 0.0f;
				} else {
					ease = (ElapsedMs(m_drag_time.m_start) - m_flick->m_value[4]) /
					       (m_flick->m_value[2] - m_flick->m_value[4]);
					if (ease < 0.0f)
						ease = 0.0f;
					if (ease > 1.0f)
						ease = 1.0f;
				}
				*ratio = *ratio * (ease * ease);
			}
		}
	}
	return ret;
}

bool DragGestureListener::ScaleFlick(float *x, float *y)
{
	if (m_state != STATE_FLICK)
		return false;
	m_drag->GetRatio(x, y);

	float scale = 1.0f;

	if (m_flick->m_value[3] > 1.1920929e-7f) {
		float t = ElapsedMs(m_flick_time.m_start) / m_flick->m_value[3];

		scale = t * t;
		if (t >= 1.0f)
			scale = 1.0f;
	}
	*x = *x * scale;
	*y = *y * scale;
	return true;
}

/* the gyro's last ten readings of each axis, the newest first, and whether each axis has any
   (names ours) */
static SceMotionSensorState s_sensor_state;
static float s_gyro_history[10][3];
static uint8_t s_gyro_ready[3];

/* the most the differences of an axis add up to, under which an axis counts as still, and how
   many updates it stays so before it changes (set on every read; names ours) */
static float s_gyro_sum_max = 100.0f;
static float s_gyro_still = 0.01f;
static int32_t s_gyro_frames = 5;
/* each axis: still, below the threshold now and the update before, and the updates it was
   below and above it (names ours) */
static uint8_t s_axis_still[3] = { 0, 0, 0 };
static uint8_t s_axis_below[3] = { 0, 0, 0 };
static uint8_t s_axis_below_pre[3] = { 0, 0, 0 };
static uint16_t s_axis_below_count[3] = { 0, 0, 0 };
static uint16_t s_axis_above_count[3] = { 0, 0, 0 };

/* the axis's reading added to its history, and how far the newest is from the others (name
   ours) */
static int32_t AddGyroSample(uint8_t axis, float value, float *sum)
{
	int32_t ready = s_gyro_ready[axis];
	float max = s_gyro_sum_max;

	if (ready == 0) {
		for (uint8_t i = 0; i < 10; i++)
			s_gyro_history[i][axis] = value;
		ready = 1;
		s_gyro_ready[axis] = 1;
	} else {
		for (uint8_t i = 9; i != 0; i--)
			s_gyro_history[i][axis] = s_gyro_history[i - 1][axis];
		s_gyro_history[0][axis] = value;
	}

	float total = 0.0f;

	for (uint8_t i = 1; i < 9; i++) {
		float d = s_gyro_history[0][axis] - s_gyro_history[i][axis];

		total += d > 0.0f ? d : -d;
	}
	if (total > max)
		total = max;
	if (axis == 0)
		sum[0] = total;
	else if (axis == 1)
		sum[1] = total;
	else if (axis == 2)
		sum[2] = total;
	return ready;
}

/* the axis still once it stayed below the threshold for more than the frames, moving once it
   stayed above (name ours) */
static int32_t IsGyroStill(uint8_t axis, float value, const float *sum)
{
	int32_t still = s_axis_still[axis];
	int32_t frames = s_gyro_frames;
	float total;
	bool below;
	uint16_t below_count;
	uint16_t above_count;

	if (axis == 0)
		total = sum[0];
	else if (axis == 1)
		total = sum[1];
	else
		total = sum[2];
	if (total < s_gyro_still * 10.0f) {
		below = true;
		s_axis_below[axis] = 1;
		if (s_axis_below_pre[axis] == 1) {
			below_count = ++s_axis_below_count[axis];
		if (below_count > 0xffff) {
			s_axis_below_count[axis] = 0xffff;
			below_count = 0xffff;
		}
			goto reset_above;
		}
	} else {
		below = false;
		s_axis_below[axis] = 0;
	}
	below_count = 0;
	s_axis_below_count[axis] = 0;
	if (!below && s_axis_below_pre[axis] == 0) {
		above_count = ++s_axis_above_count[axis];
		if (above_count > 0xffff) {
			s_axis_above_count[axis] = 0xffff;
			above_count = 0xffff;
		}
		goto check;
	}
reset_above:
	above_count = 0;
	s_axis_above_count[axis] = 0;
check:
	if (still != 0) {
		if (above_count > frames) {
			still = 0;
			s_axis_still[axis] = 0;
		}
	} else if (below_count > frames) {
		still = 1;
		s_axis_still[axis] = 1;
	}
	s_axis_below_pre[axis] = below;
	return still;
}

MotionGestureListener::MotionGestureListener(uint32_t port, uint32_t touch_id, void *user_data)
	: MultiTouchGestureListener(port, touch_id, user_data, 1)
{
	SetTouchCountMax(8);
	m_still = false;
	m_sampling = false;
	m_result = 1;
	m_dir[0] = 0.0f;
	m_dir[1] = 0.0f;
	m_dir[2] = -1.0f;
	m_dir[3] = 0.0f;
	if (MotionParam::Get().m_enabled) {
		m_start_time.m_start = get_time_internal();
		m_still = false;
		m_count = 0;
	}
}

MotionGestureListener::~MotionGestureListener()
{
	if (MotionParam::Get().m_enabled)
		sceMotionStopSampling();

	bool sampling = m_sampling;

	if (sampling) {
		sampling = false;
		m_sampling = false;
	}
	MotionParam::Get().m_sampling = sampling;
}

int32_t MotionGestureListener::OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre,
						  int32_t diff)
{
	const inputdevice::touchscreen::Data *touch;
	int32_t ret = 1;

	for (int32_t i = 0; i < 8; i++) {
		inputdevice::touchscreen::GetData(0, i, &touch);
		if (touch->state != 0)
			ret = 2;
	}
	if (MotionParam::Get().m_enabled) {
		int32_t force = 0;
		int32_t still = 1;

		for (int32_t i = 0; i < 8; i++) {
			inputdevice::touchscreen::GetData(1, i, &touch);
			if (touch->state != 0 && force < touch->force)
				force = touch->force;
		}

		float ms = ElapsedMs(m_start_time.m_start);
		bool sampling = m_sampling;

		if (ms >= 1000.0f) {
			if (m_sampling) {
stop:
				sceMotionStopSampling();
				m_sampling = false;
				sampling = false;
			}
		} else {
			if (force > 10 && !m_sampling && !m_still) {
				sceMotionSetTiltCorrection(0);
				sceMotionSetDeadband(0);
				sceMotionStartSampling();
				m_sampling = true;
				sampling = true;
			} else if (!m_sampling) {
				goto done;
			}
			if (!m_still) {
				float sum[3];

				sceMotionGetSensorState(&s_sensor_state, 1);
				s_gyro_sum_max = 100.0f;
				s_gyro_still = 0.01f;
				s_gyro_frames = 5;
				if (AddGyroSample(0, s_sensor_state.gyro.x, sum) != 0)
					still = IsGyroStill(0, s_sensor_state.gyro.x, sum) & 1;
				if (AddGyroSample(1, s_sensor_state.gyro.y, sum) != 0)
					still &= IsGyroStill(1, s_sensor_state.gyro.y, sum);
				if (AddGyroSample(2, s_sensor_state.gyro.z, sum) != 0)
					still &= IsGyroStill(2, s_sensor_state.gyro.z, sum);

				int32_t count = m_count;

				sampling = m_sampling;
				if (count > 10 && still != 0)
					m_still = true;
				m_count = count + 1;
				goto done;
			}
			if (sampling)
				goto stop;
		}
done:
		if (m_still)
			ret = 2;
		MotionParam::Get().m_sampling = sampling;
	}
	m_result = ret;
	return ret;
}

PinchGestureListener::PinchGestureListener(uint32_t port, uint32_t touch_id, void *user_data)
	: MultiTouchGestureListener(port, touch_id, user_data, 1)
{
	m_pinch_touches = 0;
	m_move_y = 0.0f;
	m_move_x = 0.0f;
	m_pos[7] = 0.0f;
	m_pos[6] = 0.0f;
	m_pos[5] = 0.0f;
	m_pos[4] = 0.0f;
	m_pos[3] = 0.0f;
	m_pos[2] = 0.0f;
	m_pos[1] = 0.0f;
	m_pos[0] = 0.0f;
	m_pinch_state = 0;
	SetTouchCountMax(8);

	inputdevice::touchscreen::DeviceInfo info;

	inputdevice::touchscreen::GetDeviceInfo(0, &info);
	m_pixel_per_mm = info.pixel_per_mm;
}

PinchGestureListener::~PinchGestureListener()
{
	bool other = AnyHeldByOther(this, m_touch_bitmap);

	if (m_pinch_state == 2) {
		if (other)
			ScePafWidget_C5939A92(this, 18, 0, 0, 0, 0, 0, 0);
		else
			ScePafWidget_C5939A92(this, 15, 0, 0, 0, 0, 0, 0);
	}
}

int32_t PinchGestureListener::GetVelocity(float *x, float *y)
{
	uint32_t touches = m_touch_bitmap;
	float fastest = 0.0f;
	uint32_t id = 0;
	int32_t found = -1;

	for (; touches != 0; touches >>= 1, id++, x++, y++) {
		if ((touches & 1) != 0) {
			ScePafMisc_88F4A40B(m_port, id, 50, x, y);
			*x = *x * 1000.0f;
			*y = *y * 1000.0f;

			float speed = *y * *y + *x * *x;

			if (fastest < speed) {
				found = id;
				fastest = speed;
			}
		} else {
			*y = 0.0f;
			*x = 0.0f;
		}
	}
	return found;
}

bool PinchGestureListener::GetMove(float *x, float *y, float *speed_x, float *speed_y)
{
	float vx[8];
	float vy[8];
	bool ret = false;

	*y = 0.0f;
	m_pinch_touches = 0;
	*x = 0.0f;
	*speed_y = 0.0f;
	*speed_x = 0.0f;

	int32_t fastest = GetVelocity(vx, vy);

	if (fastest >= 0) {
		const inputdevice::touchscreen::Data *touch;

		inputdevice::touchscreen::GetData(m_port, fastest, &touch);

		uint32_t touches = m_touch_bitmap;
		int32_t id = 0;
		float fx = (float)touch->center_orig_x;
		float fy = (float)touch->center_orig_y;

		for (uint32_t bits = touches; bits != 0; bits >>= 1, id++) {
			if ((bits & 1) != 0) {
				float limit = m_pixel_per_mm * 8.0f;

				if (vy[id] * vy[id] + vx[id] * vx[id] > limit * limit) {
					if (id != fastest) {
						*speed_y = 0.0f;
						*speed_x = 0.0f;
						m_pinch_touches = 0;
						break;
					}
					*speed_x = vx[id];
					*speed_y = vy[id];
					m_pinch_touches = 1 << id;
				}
			}
		}

		uint32_t pinch = m_pinch_touches;

		if (pinch != 0) {
			uint32_t bits = touches;

			for (uint32_t i = 0; bits != 0; bits >>= 1, i++) {
				if ((bits & 1) != 0 && i != (uint32_t)fastest) {
					inputdevice::touchscreen::GetData(m_port, i, &touch);

					float dx = (float)touch->center_orig_x - fx;
					float dy = (float)touch->center_orig_y - fy;
					float limit = m_pixel_per_mm * 25.0f;

					if (dy * dy + dx * dx < limit * limit) {
						*speed_y = 0.0f;
						pinch = 0;
						*speed_x = 0.0f;
						m_pinch_touches = 0;
						goto count;
					}
				}
			}
			pinch = m_pinch_touches;
		}
count:
		int32_t count = CountTouches(pinch);

		if (count > 0) {
			*speed_x = *speed_x / (float)count;
			*speed_y = *speed_y / (float)count;
			ret = true;
			*x = Timer::s_tick_unit * Timer::s_tick_interval * *speed_x;
			*y = Timer::s_tick_unit * Timer::s_tick_interval * *speed_y;
		}
	}
	return ret;
}

int32_t PinchGestureListener::OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre,
						 int32_t diff)
{
	bool other = AnyHeldByOther(this, bitmap);
	int32_t ret = 1;
	int32_t count = CountTouches(m_touch_bitmap);

	m_pos[4] = m_pos[0];
	m_pos[5] = m_pos[1];
	m_pos[6] = m_pos[2];
	m_pos[7] = m_pos[3];

	bool moved = GetMove(&m_pos[0], &m_pos[1], &m_pos[2], &m_pos[3]);
	float move_x = m_move_x + m_pos[0];
	float move_y = m_move_y + m_pos[1];

	m_move_x = move_x;
	m_move_y = move_y;
	switch (m_pinch_state) {
	case 0:
		if (count != 0)
			m_pinch_state = ret;
		else
			ret = GestureListener::UPDATE_DETACH_SELF;
		break;
	case 1:
		if (count != 0) {
			float limit = m_pixel_per_mm * 5.0f;

			if (!other && moved && move_y * move_y + move_x * move_x > limit * limit) {
				ret = GestureListener::UPDATE_CAPTURE;
				m_pinch_state = 2;
				ScePafWidget_C5939A92(this, 13, 0, 0, 0, 0, 0, 0);
			}
		} else {
			ret = GestureListener::UPDATE_DETACH_SELF;
		}
		break;
	case 2:
		if (count != 0) {
			if (other) {
				m_move_x = 0.0f;
				m_pinch_state = 3;
				m_move_y = 0.0f;
				ScePafWidget_C5939A92(this, 18, 0, 0, 0, 0, 0, 0);
				ret = GestureListener::UPDATE_CAPTURE;
			} else if (moved) {
				ScePafWidget_C5939A92(this, 14, 0, 0, 0, 0, 0, 0);
				ret = GestureListener::UPDATE_CAPTURE;
			} else {
				const inputdevice::touchscreen::Data *touch;
				uint32_t touches = m_pinch_touches;
				int32_t held = ret;
				uint32_t id = 0;

				for (; touches != 0; touches >>= 1, id++) {
					if ((touches & 1) != 0) {
						inputdevice::touchscreen::GetData(m_port, id, &touch);
						if (touch->state == 1 || touch->state == 3)
							held = 0;
					}
				}
				if (held == 0) {
					m_move_x = 0.0f;
					m_pinch_state = 3;
					m_move_y = 0.0f;
					ScePafWidget_C5939A92(this, 18, 0, 0, 0, 0, 0, 0);
				} else {
					m_pinch_state = ret;
					m_move_x = 0.0f;
					m_move_y = 0.0f;
					ScePafWidget_C5939A92(this, 15, 0, 0, 0, 0, 0, 0);
				}
				ret = GestureListener::UPDATE_CAPTURE;
			}
		} else {
			m_pinch_state = 0;
			m_move_x = 0.0f;
			m_move_y = 0.0f;
			if (other)
				ScePafWidget_C5939A92(this, 18, 0, 0, 0, 0, 0, 0);
			else
				ScePafWidget_C5939A92(this, 15, 0, 0, 0, 0, 0, 0);
			ret = 18;
		}
		break;
	case 3:
		if (count != 0) {
			if (!other && moved) {
				ret = GestureListener::UPDATE_CAPTURE;
				m_pinch_state = 2;
				ScePafWidget_C5939A92(this, 17, 0, 0, 0, 0, 0, 0);
			}
		} else {
			ret = 18;
		}
		break;
	}
	return ret;
}

bool PinchGestureListener::GetPinchPos(float *x0, float *y0, float *x1, float *y1)
{
	bool ret = false;

	switch (m_pinch_state) {
	case 2:
		ret = true;
		*x0 = m_pos[0];
		*y0 = m_pos[1];
		*x1 = m_pos[2];
		*y1 = m_pos[3];
		break;
	case 0:
	case 1:
	case 3:
		ret = true;
		*x0 = m_pos[4];
		*y0 = m_pos[5];
		*x1 = m_pos[6];
		*y1 = m_pos[7];
		break;
	default:
		*y0 = 0.0f;
		*x0 = 0.0f;
		*y1 = 0.0f;
		*x1 = 0.0f;
		break;
	}
	return ret;
}

RearDragGestureListener::RearDragGestureListener(uint32_t port, uint32_t touch_id,
						 void *user_data)
	: DragGestureListener(port, touch_id, user_data), m_rear_param(), m_drag_touches(0),
	  m_fast_touches(0), m_fast_touches_pre(0), m_fast_x(0.0f), m_fast_y(0.0f), m_move_x(0.0f),
	  m_move_y(0.0f), m_moved(false)
{
	SetTouchCountMax(8);
	for (int32_t i = 0; i < 8; i++) {
		m_press_time_of[i].m_start = get_time_internal();
		m_fast_time[i].m_start = get_time_internal();
	}
	m_param.m_move_tolerance = 8.0f;
	m_rear_param.m_move_tolerance = 8.0f;
	m_param.m_start_distance[0] = 30.0f;
	m_param.m_start_distance[1] = 30.0f;
	m_rear_param.m_start_distance[0] = 30.0f;
	m_rear_param.m_start_distance[1] = 30.0f;
	m_param.m_start_distance[2] = 15.0f;
	m_param.m_end_distance[0] = 40.0f;
	m_param.m_end_distance[1] = 40.0f;
	m_param.m_end_distance[2] = 40.0f;
	m_rear_param.m_start_distance[2] = 15.0f;
	m_rear_param.m_end_distance[0] = 40.0f;
	m_rear_param.m_end_distance[1] = 40.0f;
	m_rear_param.m_end_distance[2] = 40.0f;
	m_param.m_end_speed[0] = 230.0f;
	m_param.m_end_speed[1] = 230.0f;
	m_rear_param.m_end_speed[0] = 230.0f;
	m_rear_param.m_end_speed[1] = 230.0f;
	m_flick = &DragFlickParam::Get();
}

RearDragGestureListener::~RearDragGestureListener()
{
}

void RearDragGestureListener::SetParam(const DragGestureParam *param)
{
	m_rear_param = *param;
	if (m_rear_param.m_move_tolerance < 1.1920929e-7f)
		m_rear_param.m_move_tolerance = 8.0f;
	if (m_rear_param.m_start_distance[0] < 1.1920929e-7f)
		m_rear_param.m_start_distance[0] = 30.0f;
	if (m_rear_param.m_start_distance[1] < 1.1920929e-7f)
		m_rear_param.m_start_distance[1] = 30.0f;
	if (m_rear_param.m_start_distance[2] < 1.1920929e-7f)
		m_rear_param.m_start_distance[2] = 15.0f;
	if (m_rear_param.m_end_distance[0] < 1.1920929e-7f)
		m_rear_param.m_end_distance[0] = 40.0f;
	if (m_rear_param.m_end_distance[2] < 1.1920929e-7f)
		m_rear_param.m_end_distance[2] = 40.0f;
	if (m_rear_param.m_end_distance[1] < 1.1920929e-7f)
		m_rear_param.m_end_distance[1] = 40.0f;
	if (m_rear_param.m_end_speed[0] < 1.1920929e-7f)
		m_rear_param.m_end_speed[0] = 230.0f;
	if (m_rear_param.m_end_speed[1] < 1.1920929e-7f)
		m_rear_param.m_end_speed[1] = 230.0f;
}

void RearDragGestureListener::GetParam(DragGestureParam *param)
{
	*param = m_rear_param;
}

int32_t RearDragGestureListener::OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre,
						    int32_t diff)
{
	int32_t ret = 1;
	bool other = AnyHeldByOther(this, bitmap);
	uint32_t pressed;
	uint32_t moved;
	uint32_t released;
	float fast_x;
	float fast_y;
	int32_t fast;

	GetTouchBits(&pressed, &moved, &released);
	m_drag_touches &= moved | pressed;
	switch (m_state) {
	case STATE_NONE:
	case STATE_PRESS:
		fast = GetFastMove(bitmap, &fast_x, &fast_y);
		break;
	case STATE_DRAG:
	case STATE_FLICK:
		fast = GetFastMove(m_drag_touches, &fast_x, &fast_y);
		break;
	default:
		m_fast_touches = 0;
		fast = 0;
		fast_x = 0.0f;
		fast_y = 0.0f;
		break;
	}

	float tolerance = m_param.m_move_tolerance * m_pixel_per_mm;
	float move_y = m_move_y + fast_y;
	float move_x = m_move_x + fast_x;
	float dy = move_y - m_y;
	float dx = move_x - m_x;

	m_move_x = move_x;
	m_move_y = move_y;

	float dist = dy * dy + dx * dx;

	if (dist > tolerance * tolerance) {
		float ax = __builtin_fabsf(move_x);
		float ay = __builtin_fabsf(move_y);
		float slope = sce_paf_tanf(m_flick->m_value[0] * 3.1415927f / 180.0f);
		int32_t dir = m_param.m_dir;
		float last_x = m_x;
		float last_y = m_y;
		float prev_x = m_prev_x;
		float prev_y = m_prev_y;

		if (dir == 2) {
			if (ay <= ax * slope) {
				m_param.m_dir = 0;
				dir = 0;
			} else if (ax <= ay * slope) {
				m_param.m_dir = 1;
				dir = 1;
			} else {
				m_straight = false;
			}
		}
		if (dir == 0) {
			if (ay > ax * slope)
				m_straight = false;
		} else if (dir == 1) {
			if (ax > ay * slope)
				m_straight = false;
		}

		float px = last_x - prev_x;
		float py = last_y - prev_y;
		float limit = sce_paf_cosf(m_flick->m_value[1] * 3.1415927f / 180.0f);

		last_x = m_x;
		last_y = m_y;
		if ((last_x != 0.0f || last_y != 0.0f) &&
		    (dy * py + dx * px) / __builtin_sqrtf(dist * (py * py + px * px)) < limit)
			m_straight = false;
		m_y = m_move_y;
		m_prev_x = last_x;
		m_prev_y = last_y;
		m_x = m_move_x;
	}

	int32_t count = CountTouches(m_touch_bitmap);

	switch (m_state) {
	case STATE_NONE:
		if (count == 0) {
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else {
			Reset();
			m_state = STATE_PRESS;
		}
		break;
	case STATE_PRESS:
		if (count == 0) {
			ret = GestureListener::UPDATE_DETACH_SELF;
		} else if (other) {
			Reset();
		} else if (fast == 1) {
			uint32_t bits = m_fast_touches & ~m_fast_touches_pre;
			uint32_t id;

			for (id = 0; bits != 0; bits >>= 1, id++) {
				if ((bits & 1) != 0 &&
				    ElapsedMs(m_press_time_of[id].m_start) < m_param.m_hold_time)
					m_drag_touches |= 1 << id;
			}
			bits = m_drag_touches;
			for (id = 0; bits != 0; bits >>= 1, id++) {
				if ((bits & 1) != 0 && ElapsedMs(m_fast_time[id].m_start) > 100.0f)
					m_drag_touches &= ~(1 << id);
			}
			if (m_drag_touches == 0 || !m_straight) {
				Reset();
				break;
			}

			float limit = m_param.m_move_tolerance * m_pixel_per_mm;

			if (m_move_y * m_move_y + m_move_x * m_move_x <= limit * limit)
				break;

			float seconds = ElapsedMs(m_press_time.m_start) * 0.001f;
			float speed = (fast_y * fast_y + fast_x * fast_x) / seconds / seconds;
			float flick = m_param.m_flick_speed * m_pixel_per_mm;

			if (speed > flick * flick) {
				float width = m_param.m_rect.width();
				float height = m_param.m_rect.height();

				if (width > 0.0f && height > 0.0f) {
					if (m_drag != NULL) {
						delete m_drag;
						m_drag = NULL;
					}
					m_drag = new GestureDragRatioTouch(m_param.m_dir,
									   m_param.m_touch_count,
									   width, height,
									   m_pixel_per_mm,
									   m_param.m_end_distance[0],
									   m_param.m_end_distance[1]);
					if (m_drag != NULL && m_drag->IsEnabled()) {
						const inputdevice::touchscreen::Data *touch;

						bits = m_drag_touches;
						for (id = 0; bits != 0; bits >>= 1, id++) {
							if ((bits & 1) == 0)
								continue;
							inputdevice::touchscreen::GetData(m_port, id,
											  &touch);
							m_fast_x = m_fast_x +
								   (float)touch->center_orig_x;
							m_fast_y = m_fast_y +
								   (float)touch->center_orig_y;
						}

						uint32_t touches = CountTouches(m_drag_touches);

						if (touches != 0) {
							m_fast_x = m_fast_x / (float)touches;
							m_fast_y = m_fast_y / (float)touches;
						}
						m_drag->Begin(m_fast_x, m_fast_y, m_fast_x + m_move_x,
							      m_fast_y + m_move_y);
						m_time.m_start = get_time_internal();
						m_drag_time.m_start = get_time_internal();
						m_state = STATE_DRAG;
						m_drag_x = m_fast_x + m_move_x;
						m_drag_y = m_fast_y + m_move_y;
						ScePafWidget_C5939A92(this, 7,
								      get_id_from_bitmap(bitmap, 0), 0,
								      0, 0, 0, 0);
						m_fast = false;
						break;
					}
				}
			}
			Reset();
		} else if (fast == 2) {
			Reset();
		}
		break;
	case STATE_DRAG:
		if (count == 0) {
			m_drag->End(m_fast_x + m_move_x, m_fast_y + m_move_y);
			ScePafWidget_C5939A92(this, 9, get_id_from_bitmap(bitmap, 0), 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_DETACH_SELF;
			break;
		}
		if (other) {
			Reset();
			m_state = STATE_PRESS;
			m_drag->End(m_fast_x + m_move_x, m_fast_y + m_move_y);
			ScePafWidget_C5939A92(this, 9, get_id_from_bitmap(bitmap, 0), 0, 0, 0, 0, 0);
			break;
		}
		switch (fast) {
		case 0:
		case 1:
			if (fast == 1)
				m_time.m_start = get_time_internal();
			if (m_drag_touches == 0 || !m_straight) {
				Reset();
				m_state = STATE_PRESS;
				m_drag->End(m_fast_x + m_move_x, m_fast_y + m_move_y);
				ScePafWidget_C5939A92(this, 9, get_id_from_bitmap(bitmap, 0), 0, 0, 0,
						      0, 0);
				break;
			}
			if (!m_moved) {
				float dx = m_drag->GetX() - m_drag->GetStartX();
				float dy = m_drag->GetY() - m_drag->GetStartY();
				float dist_x = m_param.m_start_distance[0] * m_pixel_per_mm;
				float dist_y = m_param.m_start_distance[2] * m_pixel_per_mm;

				switch (m_param.m_dir) {
				case 0:
					m_moved = dx * dx > dist_x * dist_x;
					break;
				case 1:
					m_moved = dy * dy > dist_y * dist_y;
					break;
				case 2:
					m_moved = dy * dy + dx * dx > dist_y * dist_y + dist_x * dist_x;
					break;
				}
			}
			if (!m_fast) {
				float ddy = m_fast_y + m_move_y - m_drag_y;
				float ddx = m_fast_x + m_move_x - m_drag_x;
				float dist = ddy * ddy + ddx * ddx;
				float seconds = ElapsedMs(m_drag_time.m_start) * 0.001f;
				float speed = m_param.m_end_speed[0] * m_pixel_per_mm;

				if (dist / seconds / seconds > speed * speed)
					m_fast = true;
			}
			if (m_moved) {
				if (ElapsedMs(m_drag_time.m_start) > m_flick->m_value[2]) {
					if (m_fast) {
						m_flick_time.m_start = get_time_internal();
						m_state = STATE_FLICK;
						ScePafWidget_C5939A92(this, 10,
								      get_id_from_bitmap(bitmap, 0), 0,
								      0, 0, 0, 0);
						ret = GestureListener::UPDATE_CAPTURE;
					} else {
						Reset();
						m_state = STATE_PRESS;
						m_drag->End(m_fast_x + m_move_x, m_fast_y + m_move_y);
						ScePafWidget_C5939A92(this, 9,
								      get_id_from_bitmap(bitmap, 0), 0,
								      0, 0, 0, 0);
					}
				} else {
					ScePafWidget_C5939A92(this, 8, get_id_from_bitmap(bitmap, 0),
							      0, 0, 0, 0, 0);
				}
			} else if (ElapsedMs(m_time.m_start) <= m_flick->m_value[5]) {
				ScePafWidget_C5939A92(this, 8, get_id_from_bitmap(bitmap, 0), 0, 0, 0,
						      0, 0);
			} else {
				Reset();
				m_state = STATE_PRESS;
				m_drag->End(m_fast_x + m_move_x, m_fast_y + m_move_y);
				ScePafWidget_C5939A92(this, 9, get_id_from_bitmap(bitmap, 0), 0, 0, 0,
						      0, 0);
			}
			m_drag->Move(m_fast_x + m_move_x, m_fast_y + m_move_y);
			break;
		case 2:
			Reset();
			m_state = STATE_PRESS;
			m_drag->End(m_fast_x + m_move_x, m_fast_y + m_move_y);
			ScePafWidget_C5939A92(this, 9, get_id_from_bitmap(bitmap, 0), 0, 0, 0, 0, 0);
			break;
		}
		break;
	case STATE_FLICK: {
		float x = m_fast_x + m_move_x;
		float y = m_fast_y + m_move_y;

		if (count == 0) {
			m_drag->End(x, y);
			ScePafWidget_C5939A92(this, 12, get_id_from_bitmap(bitmap, 0), 0, 0, 0, 0, 0);
			ret = 18;
		} else if (other) {
			m_drag->End(x, y);
			Reset();
			m_state = STATE_PRESS;
			ScePafWidget_C5939A92(this, 12, get_id_from_bitmap(bitmap, 0), 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_CAPTURE;
		} else if (m_drag_touches != 0) {
			m_drag->Move(x, y);
			ScePafWidget_C5939A92(this, 11, get_id_from_bitmap(bitmap, 0), 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_CAPTURE;
		} else {
			m_drag->End(x, y);
			Reset();
			m_state = STATE_PRESS;
			ScePafWidget_C5939A92(this, 12, get_id_from_bitmap(bitmap, 0), 0, 0, 0, 0, 0);
			ret = GestureListener::UPDATE_CAPTURE;
		}
		break;
	}
	}
	m_press_time.m_start = get_time_internal();
	return ret;
}

int32_t RearDragGestureListener::GetDragState(float *start_x, float *start_y, float *x, float *y,
					      float *ratio)
{
	int32_t ret = DragGestureListener::GetDragState(start_x, start_y, x, y, ratio);

	if (ret != 0) {
		float ease;
		float from_x;
		float to_x;

		if (m_flick->m_value[4] >= m_flick->m_value[2]) {
			float ms = ElapsedMs(m_drag_time.m_start);

			ease = 1.0f;
			from_x = *start_x;
			to_x = *x;
			if (ms < m_flick->m_value[2])
				ease = 0.0f;
		} else {
			float t = ElapsedMs(m_drag_time.m_start);

			from_x = *start_x;
			to_x = *x;
			ease = (t - m_flick->m_value[4]) / (m_flick->m_value[2] - m_flick->m_value[4]);
			if (ease < 0.0f) {
				ease = 0.0f;
			} else if (ease < 1.0f) {
				ease = ease * ease;
			} else if (ease >= 1.25f) {
				float e = 0.8f;

				if (ease < 4.0f)
					e = (ease - 4.0f) * -0.45f / 2.75f + 0.8f;
				ease = e;
			}
		}
		*x = from_x + (to_x - from_x) * ease;
		*y = *start_y + (*y - *start_y) * ease;
	}
	if (m_state == STATE_DRAG)
		*ratio = 0.0f;
	return ret;
}

int32_t RearDragGestureListener::Reset()
{
	m_param.m_move_tolerance = m_rear_param.m_move_tolerance;
	m_param.m_hold_time = m_rear_param.m_hold_time;
	m_param.m_rect = m_rear_param.m_rect;
	m_param.m_dir = m_rear_param.m_dir;
	m_param.m_start_distance[0] = m_rear_param.m_start_distance[0];
	m_param.m_touch_count = m_rear_param.m_touch_count;
	m_param.m_start_distance[1] = m_rear_param.m_start_distance[1];
	m_param.m_start_distance[2] = m_rear_param.m_start_distance[2];
	m_param.m_end_distance[0] = m_rear_param.m_end_distance[0];
	m_param.m_end_distance[1] = m_rear_param.m_end_distance[1];
	m_param.m_end_distance[2] = m_rear_param.m_end_distance[2];
	m_param.m_flick_speed = m_rear_param.m_flick_speed;
	m_param.m_end_speed[0] = m_rear_param.m_end_speed[0];
	m_param.m_end_speed[1] = m_rear_param.m_end_speed[1];
	m_moved = false;
	m_fast_x = 0.0f;
	m_fast_y = 0.0f;
	m_move_x = 0.0f;
	m_move_y = 0.0f;
	m_prev_x = 0.0f;
	m_prev_y = 0.0f;
	m_x = 0.0f;
	m_y = 0.0f;
	m_drag_touches = 0;
	m_straight = true;
	return 0;
}

void RearDragGestureListener::GetTouchBits(uint32_t *pressed, uint32_t *moved, uint32_t *released)
{
	uint32_t id = 0;

	*released = 0;
	*moved = 0;
	*pressed = 0;

	uint32_t touches = m_touch_bitmap;

	for (; touches != 0; touches >>= 1, id++) {
		if ((touches & 1) != 0) {
			const inputdevice::touchscreen::Data *touch;

			inputdevice::touchscreen::GetData(m_port, id, &touch);

			uint32_t bit = 1 << id;

			switch (touch->state) {
			case 1:
				*pressed |= bit;
				m_press_time_of[id].m_start = get_time_internal();
				break;
			case 2:
				*released |= bit;
				break;
			case 3:
				*moved |= bit;
				break;
			}
		}
	}
}

int32_t RearDragGestureListener::GetFastMove(uint32_t bitmap, float *x, float *y)
{
	float vx[8];
	float vy[8];
	int32_t ret = 0;
	int32_t count = 0;

	m_fast_touches_pre = m_fast_touches;
	m_fast_touches = 0;
	*y = 0.0f;
	*x = 0.0f;

	int32_t fastest = GetVelocity(vx, vy);

	if (fastest >= 0) {
		float fy = vy[fastest];
		float fx = vx[fastest];
		uint32_t fast = m_fast_touches;
		float speed = __builtin_sqrtf(fy * fy + fx * fx);
		uint32_t bits = bitmap;
		uint8_t id = 0;

		for (; bits != 0; bits >>= 1, id++) {
			if ((bits & 1) != 0) {
				float ty = vy[id];
				float tx = vx[id];
				float s = ty * ty + tx * tx;
				float limit = m_pixel_per_mm * 6.0f;

				if (s > limit * limit) {
					if ((ty * fy + tx * fx) / (speed * __builtin_sqrtf(s)) <= 0.5f) {
						m_fast_touches = 0;
						fast = 0;
						ret = 2;
						break;
					}
					fast |= 1 << id;
					m_fast_touches = fast;
				}
			}
		}
		if (fast != 0) {
			count = CountTouches(fast);
			if (count > 0) {
				*x = Timer::s_tick_unit * Timer::s_tick_interval * fx;
				*y = Timer::s_tick_unit * Timer::s_tick_interval * fy;
				for (uint32_t i = 0; fast != 0; fast >>= 1, i++) {
					if ((fast & 1) != 0)
						m_fast_time[i].m_start = get_time_internal();
				}
				ret = 1;
			}
		}
	}
	return ret;
}

int32_t RearDragGestureListener::GetVelocity(float *x, float *y)
{
	uint32_t touches = m_touch_bitmap;
	float fastest = 0.0f;
	uint32_t id = 0;
	int32_t found = -1;

	for (; touches != 0; touches >>= 1, id++, x++, y++) {
		if ((touches & 1) != 0) {
			ScePafMisc_88F4A40B(m_port, id, 50, x, y);
			*x = *x * 1000.0f;
			*y = *y * 1000.0f;

			float speed = *y * *y + *x * *x;

			if (fastest < speed) {
				found = id;
				fastest = speed;
			}
		} else {
			*y = 0.0f;
			*x = 0.0f;
		}
	}
	return found;
}

RearTapGestureListener::RearTapGestureListener(uint32_t port, uint32_t touch_id, void *user_data)
	: TouchGestureListener(port, touch_id, user_data), m_touches(0), m_tap_touches(0), m_taps(),
	  m_tap_count(0), m_param()
{
	inputdevice::touchscreen::DeviceInfo info;

	inputdevice::touchscreen::GetDeviceInfo(1, &info);
	m_pixel_per_mm = info.pixel_per_mm;
	inputdevice::touchscreen::GetDeviceInfo(1, &info);
	ScePafMisc_8424A840(&m_right, &m_top, info.active_max.extract_x(),
			    info.active_min.extract_y());
	ScePafMisc_8424A840(&m_left, &m_bottom, info.active_min.extract_x(),
			    info.active_max.extract_y());

	float margin = m_param.m_margin * m_pixel_per_mm;

	m_left = m_left + margin;
	m_bottom = m_bottom + margin;
	m_right = m_right - margin;
	m_top = m_top - margin;
}

RearTapGestureListener::~RearTapGestureListener()
{
	vector<RearTap *>::iterator it = m_taps.begin();

	while (it != m_taps.end()) {
		delete *it;
		it = m_taps.erase(it);
	}
}

int32_t RearTapGestureListener::UpdateTaps(uint32_t touches, uint32_t touches_pre)
{
	int32_t ret = 1;

	if (AnyHeldByOther(this, touches))
		return GestureListener::UPDATE_DETACH_SELF;

	const inputdevice::touchscreen::Data *touch;
	uint32_t pressed = touches & ~touches_pre;
	uint32_t id = 0;

	for (; pressed != 0; pressed >>= 1, id++) {
		if ((pressed & 1) == 0)
			continue;
		inputdevice::touchscreen::GetData(m_port, id, &touch);

		float x = (float)touch->center_orig_x;

		if (!(x >= m_left && x <= m_right))
			continue;

		float y = (float)touch->center_orig_y;

		if (!(y >= m_bottom && y <= m_top))
			continue;
		m_tap_touches |= 1 << id;
		m_press_time[id].m_start = get_time_internal();

		math::v2 pos((float)touch->center_orig_x, (float)touch->center_orig_y);

		m_press_pos[id][0] = pos.extract_x();
		m_press_pos[id][1] = pos.extract_y();

		float limit = m_param.m_tap_distance * m_pixel_per_mm;

		for (vector<RearTap *>::iterator it = m_taps.begin(); it != m_taps.end(); ++it) {
			math::v2 d = (*it)->m_pos - pos;

			if ((d * d).extract_x() + (d * d).extract_y() < limit * limit) {
				(*it)->m_pressed = true;
				break;
			}
		}
	}

	uint32_t taps = m_tap_touches;

	for (id = 0; taps != 0; taps >>= 1, id++) {
		if ((taps & 1) != 0) {
			inputdevice::touchscreen::GetData(m_port, id, &touch);
			if (touch->state > 0 && touch->state <= 3) {
				float dx = m_press_pos[id][0] - (float)touch->center_orig_x;
				float dy = m_press_pos[id][1] - (float)touch->center_orig_y;
				float limit = m_param.m_tap_distance * m_pixel_per_mm;

				if (dy * dy + dx * dx > limit * limit) {
					m_tap_touches ^= 1 << id;
					taps ^= 1;
				}
			}
		}
		if ((taps & 1) != 0 &&
		    ElapsedMs(m_press_time[id].m_start) > (float)m_param.m_hold_time) {
			m_tap_touches ^= 1 << id;
			taps ^= 1;
		}
	}

	uint32_t released = touches_pre & ~touches;

	for (id = 0; released != 0; released >>= 1, id++) {
		if ((released & 1) == 0 || ((1 << id) & m_tap_touches) == 0)
			continue;
		inputdevice::touchscreen::GetData(m_port, id, &touch);

		math::v2 pos((float)touch->center_orig_x, (float)touch->center_orig_y);
		float limit = m_param.m_tap_distance * m_pixel_per_mm;
		vector<RearTap *>::iterator it;

		for (it = m_taps.begin(); it != m_taps.end(); ++it) {
			math::v2 d = (*it)->m_pos - pos;

			if ((d * d).extract_x() + (d * d).extract_y() < limit * limit)
				break;
		}
		if (it == m_taps.end()) {
			if (m_tap_count == 1) {
				ScePafWidget_C5939A92(this, 0, (int32_t)pos.extract_x(),
						      (int32_t)pos.extract_y(), 1, 0, 0, 0);
			} else if (m_tap_count > 1) {
				RearTap *tap = new RearTap(pos);

				if (tap != NULL) {
					tap->m_time = get_time_internal();
					tap->m_pressed = false;
					tap->m_count = 1;
					m_taps.push_back(tap);
				}
			}
		} else {
			RearTap *tap = *it;

			tap->m_time = get_time_internal();
			tap->m_pressed = false;
			tap->m_count++;
			if (m_tap_count == tap->m_count) {
				ScePafWidget_C5939A92(this, 2, (int32_t)tap->m_pos.extract_x(),
						      (int32_t)tap->m_pos.extract_y(), tap->m_count, 0, 0,
						      0);
				delete *it;
				m_taps.erase(it);
			}
		}
	}

	vector<RearTap *>::iterator it = m_taps.begin();

	while (it != m_taps.end()) {
		RearTap *tap = *it;

		if (tap->m_pressed) {
			if (ElapsedMs(tap->m_time) <= (float)(m_param.m_interval + m_param.m_hold_time)) {
				++it;
			} else {
				delete *it;
				it = m_taps.erase(it);
			}
		} else if (ElapsedMs(tap->m_time) <= (float)m_param.m_interval) {
			++it;
		} else {
			if (tap->m_count == 1)
				ScePafWidget_C5939A92(this, 0, (int32_t)tap->m_pos.extract_x(),
						      (int32_t)tap->m_pos.extract_y(), 1, 0, 0, 0);
			else if (tap->m_count > 1)
				ScePafWidget_C5939A92(this, 2, (int32_t)tap->m_pos.extract_x(),
						      (int32_t)tap->m_pos.extract_y(), tap->m_count, 0,
						      0, 0);
			delete *it;
			it = m_taps.erase(it);
		}
	}
	if ((touches | m_taps.size()) == 0)
		ret = GestureListener::UPDATE_DETACH_SELF;
	return ret;
}

int32_t RearTapGestureListener::OnUpdateTouch(uint32_t id)
{
	const inputdevice::touchscreen::Data *touch;

	uint32_t touches = m_touches;

	inputdevice::touchscreen::GetData(m_port, id, &touch);

	uint32_t bit = 1 << id;

	switch (touch->state) {
	case 0:
	case 2:
	case 4:
		m_touches &= ~bit;
		break;
	case 1:
	case 3:
		m_touches |= bit;
		break;
	}
	return UpdateTaps(m_touches, touches);
}

void RearTapGestureListener::SetParam(const RearTapParam *param)
{
	float tolerance = param->m_move_tolerance;
	uint32_t hold_time = param->m_hold_time;

	m_param.m_move_tolerance = tolerance;

	float distance = param->m_tap_distance;

	m_param.m_hold_time = hold_time;

	uint32_t interval = param->m_interval;

	m_param.m_tap_distance = distance;

	float margin = param->m_margin;

	m_param.m_interval = interval;
	m_param.m_margin = margin;
	if (tolerance < 1.1920929e-7f)
		m_param.m_move_tolerance = 4.0f;
	if (hold_time == 0)
		m_param.m_hold_time = 250;
	if (distance < 1.1920929e-7f)
		m_param.m_tap_distance = 4.0f;
	if (interval == 0)
		m_param.m_interval = 200;
	if (margin < 0.0f)
		m_param.m_margin = 5.0f;

	inputdevice::touchscreen::DeviceInfo info;

	inputdevice::touchscreen::GetDeviceInfo(1, &info);
	ScePafMisc_8424A840(&m_right, &m_top, info.active_max.extract_x(),
			    info.active_min.extract_y());
	ScePafMisc_8424A840(&m_left, &m_bottom, info.active_min.extract_x(),
			    info.active_max.extract_y());

	float m = m_param.m_margin * m_pixel_per_mm;

	m_left = m_left + m;
	m_bottom = m_bottom + m;
	m_right = m_right - m;
	m_top = m_top - m;
}
