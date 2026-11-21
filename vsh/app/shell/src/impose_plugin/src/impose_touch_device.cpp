/*
 * impose_touch_device.cpp: impose_plugin's touch device: the controller's sticks, or a touch
 * pad, moving the touch emulation's points, given to the touch driver as touches of the front
 * and the back panel (0x81144204-0x811450A4). The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <thread/thread.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/pad/pad.h>
#include <libkernel/libkernel.h>
#include <ctrl.h>
#include <touch.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

/* PLACEHOLDER: the application in front while the touch emulation stays off */
#define IMPOSE_TOUCH_APP_ID_OFF		0x1002

/* the buttons: the sticks' (L3, R3), and the touch pad's click (PLACEHOLDER names) */
#define IMPOSE_TOUCH_BUTTON_L3		0x2
#define IMPOSE_TOUCH_BUTTON_R3		0x4
#define IMPOSE_TOUCH_BUTTON_L2		0x100
#define IMPOSE_TOUCH_BUTTON_R2		0x200
#define IMPOSE_TOUCH_BUTTON_CIRCLE	0x2000
#define IMPOSE_TOUCH_BUTTON_CROSS	0x4000
#define IMPOSE_TOUCH_BUTTON_PAD		0x4000000

/* the touch IDs the driver takes */
#define IMPOSE_TOUCH_ID_MAX		128

/* the touch pad's active area, in its own units */
#define IMPOSE_TOUCH_PAD_MARGIN_X	60
#define IMPOSE_TOUCH_PAD_MIN_Y		120
#define IMPOSE_TOUCH_PAD_MAX_Y		821

/* the state of the previous stick sample, and the points' speeds (0x815448D4-0x815448E4) */
static SceInt32 s_lastState;
static float s_speedX[2];
static float s_speedY[2];

/* the panels' areas, the first point's buttons by the enter button (0x81144204) */
ImposeTouchDevice::ImposeTouchDevice()
	: m_state(0), m_front(), m_area(), m_frontPanel(), m_backPanel(), m_reportId(0),
	  m_buttons(0), m_touchButtons(), m_cancelButtons(0), m_suspendState(0),
	  m_cancelHeld(false), m_touchHeld(false), m_mode(0)
{
	SceTouchPanelInfo info;

	sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &info);
	m_frontPanel.minX = info.minAaX;
	m_frontPanel.maxX = info.maxAaX;
	m_frontPanel.minY = info.minAaY;
	m_frontPanel.maxY = info.maxAaY;
	sceTouchGetPanelInfo(SCE_TOUCH_PORT_BACK, &info);
	m_backPanel.minX = info.minAaX;
	m_backPanel.maxX = info.maxAaX;
	m_backPanel.minY = info.minAaY;
	m_backPanel.maxY = info.maxAaY;
	m_front = m_frontPanel;
	m_area.maxX = m_front.maxX < m_backPanel.maxX ? m_front.maxX : m_backPanel.maxX;
	m_area.minX = m_front.minX < m_backPanel.minX ? m_backPanel.minX : m_front.minX;
	m_area.maxY = m_front.maxY < m_backPanel.maxY ? m_front.maxY : m_backPanel.maxY;
	m_area.minY = m_front.minY < m_backPanel.minY ? m_backPanel.minY : m_front.minY;

	SceUInt32 touch = IMPOSE_TOUCH_BUTTON_CROSS | IMPOSE_TOUCH_BUTTON_L2;
	SceUInt32 cancel = IMPOSE_TOUCH_BUTTON_CIRCLE;

	if (ScePafMisc_9CB5D72E() == 0) {
		touch = IMPOSE_TOUCH_BUTTON_CIRCLE | IMPOSE_TOUCH_BUTTON_L2;
		cancel = IMPOSE_TOUCH_BUTTON_CROSS;
	}
	m_touchButtons[0] = touch;
	m_touchButtons[1] = IMPOSE_TOUCH_BUTTON_R2;
	m_cancelButtons = cancel;
}

/* (0x8114432C) */
ImposeTouchDevice::~ImposeTouchDevice()
{
}

/* the system impose given back where the pointers were on, unless the input is taken */
static inline void imposeTouchReleaseImpose(const ImposeTouchDevice *device, int state)
{
	thread::RMutex::main_thread_mutex.Lock();
	if (inputdevice::IsOwnershipGotten(0x6F, 0) == 0 &&
	    ((state != 0 && state != 4) || device->m_suspendState == 2))
		sceAppMgrSetSystemImposeState2(0);
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the pointers off and the buttons forgotten (0x8114433C) */
void ImposeTouchDevice::Reset()
{
	thread::RMutex::main_thread_mutex.Lock();
	imposeTouchReleaseImpose(this, m_state);
	m_state = 0;
	m_buttons = 0;
	m_suspendState = 0;
	m_cancelHeld = false;
	m_touchHeld = false;
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the touched points given to the touch driver, on the front and the back panel (0x811443AE) */
void ImposeTouchDevice::SetTouchData(const ImposeTouchPoint *points, int count)
{
	SceTouchData front;
	SceTouchData back;

	sce_paf_memset(&front, 0, sizeof(front));
	front.timeStamp = sceKernelGetSystemTimeWide();
	sce_paf_memset(&back, 0, sizeof(back));
	back.timeStamp = sceKernelGetSystemTimeWide();
	for (int i = 0; i < count; i++, points++) {
		int panel = points->panel;

		if ((panel == 0 || panel == 2 || panel == 3) && points->state == 2) {
			SceTouchReport *report = &front.report[front.reportNum++];

			report->id = points->frontId;
			report->force = 128;
			report->x = points->x;
			report->y = points->y;
		}
		if ((panel == 1 || panel == 2) && points->state == 2) {
			SceTouchReport *report = &back.report[back.reportNum++];
			int x = points->x;
			int y = points->y;

			report->force = 128;
			if (panel == 1) {
				report->id = points->frontId;
				report->x = (int)(m_backPanel.minX + (float)x / m_frontPanel.maxX *
						  (m_backPanel.maxX - m_backPanel.minX));
				report->y = (int)(m_backPanel.minY + (float)y / m_frontPanel.maxY *
						  (m_backPanel.maxY - m_backPanel.minY));
			} else {
				report->id = points->backId;
				report->x = x;
				report->y = y;
			}
		}
	}
	vshTouchSetTouchEmulationData(&front, &back);
}

/* a point moved by its stick: the speed built up past the dead zone, kept in the area
   (0x81144520) */
SceInt16 ImposeTouchDevice::MovePoint(int stick, float *speed, SceInt16 min, SceInt16 max,
				      SceInt16 pos)
{
	float value = stick / 255.0f * 2.0f - 1.0f;
	float target;

	if (value < -0.8745098f)
		target = -1.0f;
	else if (value > 0.8745098f)
		target = 1.0f;
	else if (value < -0.2509804f)
		target = (value - -0.2509804f) / 0.62352943f;
	else if (value > 0.2509804f)
		target = (value - 0.2509804f) / 0.62352943f;
	else
		target = 0.0f;

	float current = *speed;

	if (target * current < 0.0f)
		current = 0.0f;
	if (target == 0.0f) {
		current = 0.0f;
	} else {
		current += target * 0.022222223f;
		if (current < -1.0f)
			current = -1.0f;
		else if (current > 1.0f)
			current = 1.0f;
		if (target < 0.0f && target > current)
			current = target;
		else if (target > 0.0f && target < current)
			current = target;
	}

	int next = (int)(pos + current * 100.0f);

	if (next < min) {
		next = min;
		current = 0.0f;
	}
	if (next > max) {
		next = max;
		current = 0.0f;
	}
	*speed = current;
	return next;
}

/* the points moved by the sticks, touching while their buttons are held (0x811446CA) */
void ImposeTouchDevice::UpdateFromPad(const SceCtrlData2 *pad, ImposeTouchPoint *points,
				      int count)
{
	int stickX[2];
	int stickY[2];
	bool touched[2];

	stickX[0] = pad->lx;
	stickX[1] = pad->rx;
	stickY[0] = pad->ly;
	stickY[1] = pad->ry;
	touched[0] = (m_touchButtons[0] & pad->buttons) != 0;
	touched[1] = (pad->buttons & m_touchButtons[1]) != 0;
	if (m_touchHeld) {
		if (touched[0])
			touched[0] = false;
		else
			m_touchHeld = false;
	}
	if (s_lastState != m_state) {
		s_speedX[0] = 0.0f;
		s_speedX[1] = 0.0f;
		s_speedY[0] = 0.0f;
		s_speedY[1] = 0.0f;
	}
	/* Sony's: the states of two points past the two given cleared */
	points[2].state = 0;
	points[3].state = 0;
	if (m_state == 0) {
		for (int i = 0; i < count; i++)
			points[i].state = 0;
	} else if (m_state == 3) {
		for (int i = 0; i < count; i++) {
			points[i].x = MovePoint(stickX[i], &s_speedX[i], m_area.minX, m_area.maxX,
						points[i].x);
			points[i].y = MovePoint(stickY[i], &s_speedY[i], m_area.minY, m_area.maxY,
						points[i].y);
			if (points[i].state != 2 && touched[i]) {
				int id = m_reportId + 1;

				if (id >= IMPOSE_TOUCH_ID_MAX)
					id = 0;
				points[i].frontId = id;
				id++;
				m_reportId = id;
				if (id >= IMPOSE_TOUCH_ID_MAX) {
					m_reportId = 0;
					id = 0;
				}
				points[i].backId = id;
			}
			points[i].state = touched[i] ? 2 : 1;
			points[i].panel = 2;
		}
	} else {
		for (int i = 0; i < count; i++) {
			points[i].x = MovePoint(stickX[i], &s_speedX[i], m_front.minX, m_front.maxX,
						points[i].x);
			points[i].y = MovePoint(stickY[i], &s_speedY[i], m_front.minY, m_front.maxY,
						points[i].y);

			int state = m_state;

			if (points[i].state != 2 && touched[i]) {
				int id = ++m_reportId;

				if (id >= IMPOSE_TOUCH_ID_MAX) {
					id = 0;
					m_reportId = 0;
				}
				points[i].frontId = id;
				points[i].backId = 0;
			}
			points[i].state = touched[i] ? 2 : 1;
			points[i].panel = state == 2 ? 1 : 0;
		}
	}
	s_lastState = m_state;
	SetTouchData(points, count);
}

/* a touch pad's place, from its own units to the front panel's */
static inline SceInt16 imposeTouchPadX(SceInt16 x, SceInt16 width)
{
	if (x < IMPOSE_TOUCH_PAD_MARGIN_X)
		x = IMPOSE_TOUCH_PAD_MARGIN_X;
	if (width - IMPOSE_TOUCH_PAD_MARGIN_X < x)
		x = width - IMPOSE_TOUCH_PAD_MARGIN_X;
	return (int)((x - 60.0f) / (width - 2 * IMPOSE_TOUCH_PAD_MARGIN_X) * width);
}

static inline SceInt16 imposeTouchPadY(int y, SceInt16 height)
{
	if (y >= IMPOSE_TOUCH_PAD_MIN_Y) {
		if (y > IMPOSE_TOUCH_PAD_MAX_Y)
			y = IMPOSE_TOUCH_PAD_MAX_Y;
	} else {
		y = IMPOSE_TOUCH_PAD_MIN_Y;
	}
	return (int)((y - 120.0f) / 701.0f * height);
}

/* the points set from a touch pad's two touches, the first kept on its point (0x81144944) */
void ImposeTouchDevice::UpdateFromTouchPad(const SceTouchData *touch, bool touchFront,
					   bool touchBack, ImposeTouchPoint *points, int count)
{
	for (int i = 0; i < count; i++) {
		points[i].state = 0;
		points[i].backId = 0;
	}

	SceUInt8 id = touch->report[0].id;
	int reports = touch->reportNum;
	bool second = points[1].frontId == id;

	if (reports != 0) {
		SceInt16 width = m_front.maxX;
		SceInt16 height = m_front.maxY;

		if (second) {
			points[1].x = imposeTouchPadX(touch->report[0].x, width);
			points[1].frontId = id;
			points[1].y = imposeTouchPadY(touch->report[0].y, height);
			points[1].state = touchBack ? 2 : 1;
			points[1].panel = 3;
		} else {
			points[0].x = imposeTouchPadX(touch->report[0].x, width);
			points[0].frontId = id;
			points[0].y = imposeTouchPadY(touch->report[0].y, height);
			points[0].state = touchFront ? 2 : 1;
			points[0].panel = 3;
		}
		if (reports == 2) {
			if (second) {
				points[0].x = imposeTouchPadX(touch->report[1].x, width);
				points[0].y = imposeTouchPadY(touch->report[1].y, height);
				points[0].frontId = touch->report[1].id;
				points[0].state = touchFront ? 2 : 1;
				points[0].panel = 3;
			} else {
				points[1].x = imposeTouchPadX(touch->report[1].x, width);
				points[1].y = imposeTouchPadY(touch->report[1].y, height);
				points[1].frontId = touch->report[1].id;
				points[1].state = touchBack ? 2 : 1;
				points[1].panel = 3;
			}
		}
	}
	SetTouchData(points, count);
}

/* the touch pad's touches, or none while the mode leaves the touch pad off (0x81144C62) */
__attribute__((noinline)) void ImposeTouchDevice::UpdateFromTouch(const SceTouchData *touch, bool touchFront,
					bool touchBack, ImposeTouchPoint *points, int count)
{
	if (m_mode == 1 || m_mode == 2) {
		for (int i = 0; i < count; i++) {
			points[i].state = 0;
			points[i].backId = 0;
		}

		ImposeTouchPoint none[2] = { { 0 }, { 0 } };

		UpdateFromTouchPad(touch, true, true, none, 2);
	} else {
		UpdateFromTouchPad(touch, touchFront, touchBack, points, count);
	}
}

/* the state from the sticks' buttons (L3, R3), the cancel button, the mode, a touch pad's
   touches and the application in front (0x81144CEE) */
__attribute__((noinline)) void ImposeTouchDevice::UpdateState(const SceCtrlData2 *pad, const SceTouchData *touch)
{
	SceUInt32 buttons = pad->buttons;
	SceUInt32 pressed = buttons & ~m_buttons;
	SceUInt32 released = m_buttons & ~buttons;
	int current = m_state;

	if (m_suspendState == 2 && current != 0) {
		if (m_cancelButtons & buttons)
			m_cancelHeld = true;
		if (m_touchButtons[0] & buttons)
			m_touchHeld = true;
	}

	int state = current;

	switch (current) {
	case 1:
	case 2:
	case 3: {
		SceUInt32 l3 = pressed & IMPOSE_TOUCH_BUTTON_L3;
		SceUInt32 r3 = pressed & IMPOSE_TOUCH_BUTTON_R3;

		if (l3 != 0)
			state = (buttons & IMPOSE_TOUCH_BUTTON_R3) ? 3 : 0;
		if (r3 != 0)
			state = (buttons & IMPOSE_TOUCH_BUTTON_L3) ? 3 : 0;
		if (l3 != 0 && r3 != 0)
			state = 0;
		if (m_cancelButtons & released) {
			if (!m_cancelHeld)
				state = 0;
			m_cancelHeld = false;
		}
		break;
	}
	case 4:
		state = 0;
		break;
	default: {
		SceUInt32 l3 = pressed & IMPOSE_TOUCH_BUTTON_L3;
		SceUInt32 r3 = pressed & IMPOSE_TOUCH_BUTTON_R3;

		if (l3 != 0 && (buttons & IMPOSE_TOUCH_BUTTON_R3) == 0)
			state = 1;
		if (r3 != 0 && (buttons & IMPOSE_TOUCH_BUTTON_L3) == 0)
			state = 2;
		if (l3 != 0 && r3 != 0)
			state = 3;
		break;
	}
	}
	if (m_mode == 1)
		state = 0;
	if (touch->reportNum != 0)
		state = 4;

	SceAppMgrAppStatus status;

	if (sceAppMgrGetStatusByAppId(SHELL_APP_ID_FRONT, &status) == 0 &&
	    status.appId == IMPOSE_TOUCH_APP_ID_OFF)
		state = 0;
	if (state != m_state)
		m_state = state;
	m_buttons = buttons;
}

/* one sample of the controller and the touch pad: the state, the system impose while the
   pointers are on, and the points (0x81144E02) */
void ImposeTouchDevice::Update(ImposeTouchPoint *points, int count)
{
	thread::RMutex::main_thread_mutex.Lock();
	if (m_suspendState == 1) {
		thread::RMutex::main_thread_mutex.Unlock();
		return;
	}

	SceTouchData touch;

	if (sceTouchPeek2(SCE_TOUCH_PORT_BACK, 0, &touch, 1) < 0) {
		thread::RMutex::main_thread_mutex.Unlock();
		return;
	}

	SceCtrlData2 pad;

	int samples = sceCtrlPeekBufferPositive2(0, &pad, 1);

	if (samples < 0) {
		thread::RMutex::main_thread_mutex.Unlock();
		return;
	}
	if (samples == 0) {
		thread::RMutex::main_thread_mutex.Unlock();
		return;
	}

	int last = m_state;

	UpdateState(&pad, &touch);

	int state = m_state;

	thread::RMutex::main_thread_mutex.Lock();
	if (inputdevice::IsOwnershipGotten(0x6F, 0) == 0) {
		bool on = state != 0 && state != 4;
		bool wasOn = last != 0 && last != 4;

		if (on) {
			if (!wasOn || m_suspendState == 2)
				sceAppMgrSetSystemImposeState2(2);
		} else if (wasOn || m_suspendState == 2) {
			sceAppMgrSetSystemImposeState2(0);
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();
	if (m_state == 4) {
		bool touchFront = (pad.buttons & IMPOSE_TOUCH_BUTTON_PAD) != 0 ||
				  (pad.buttons & IMPOSE_TOUCH_BUTTON_L2) != 0;
		bool touchBack = (pad.buttons & IMPOSE_TOUCH_BUTTON_PAD) != 0 ||
				 (pad.buttons & IMPOSE_TOUCH_BUTTON_R2) != 0;

		UpdateFromTouch(&touch, touchFront, touchBack, points, count);
	} else {
		UpdateFromPad(&pad, points, count);
	}
	m_suspendState = 0;
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the touches lifted and the system impose given back (0x81144F88) */
void ImposeTouchDevice::Suspend()
{
	thread::RMutex::main_thread_mutex.Lock();
	if (m_suspendState == 1) {
		thread::RMutex::main_thread_mutex.Unlock();
		return;
	}
	m_suspendState = 1;

	ImposeTouchPoint none[0];

	SetTouchData(none, 0);
	imposeTouchReleaseImpose(this, m_state);
	thread::RMutex::main_thread_mutex.Unlock();
}

/* (0x8114502E) */
void ImposeTouchDevice::Resume()
{
	thread::RMutex::main_thread_mutex.Lock();
	if (m_suspendState == 1)
		m_suspendState = 2;
	thread::RMutex::main_thread_mutex.Unlock();
}

/* (0x8114505C) */
int ImposeTouchDevice::IsSuspended()
{
	thread::RMutex::main_thread_mutex.Lock();

	bool suspended = m_suspendState == 1;

	thread::RMutex::main_thread_mutex.Unlock();
	return suspended;
}

/* (0x81145084) */
void ImposeTouchDevice::SetMode(int mode)
{
	thread::RMutex::main_thread_mutex.Lock();
	m_mode = mode;
	thread::RMutex::main_thread_mutex.Unlock();
}
