/*
 * paf::ui::Environment, as far as converted C++ uses it: the widget system's
 * state for one framework.
 */
#ifndef PAF_WIDGET_CORE_ENVIRONMENT_H
#define PAF_WIDGET_CORE_ENVIRONMENT_H

#include <stddef.h>
#include <stdint.h>
#include <std/memory>
#include <std/string>
#include <std/list>
#include <std/string.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <versatileparam/idparam.h>
#include <graphics/surface/surface.h>
#include <widget/core/event.h>
#include <common/shared_ptr.h>
#include <inputdevice/inputlistener.h>

namespace paf {
namespace graph {
class Surface;
class DisplaySurface;
}

namespace ui {

/*
 * A render target: its clear colour, viewport and scissor, and its surface. The
 * viewport and scissor are rectangles here, where older builds kept four ints
 * each (ScePaf-RE's vp_* and sp_*).
 */
class Screen {
public:
	Screen();
	Screen(intrusive_ptr<graph::Surface> surface);
	virtual ~Screen();

	static Screen *GetRootScreen()
	{
		return s_root_screen;
	}

	/* the surface made the target of the draws that follow, 0 when it is not cleared yet;
	   cleared; and the draws finished */
	int32_t OnTarget();
	void OnClear();
	void OnFinish();
	/* whether the surface is the size of the viewport (0x81147d50; name ours) */
	bool IsFullSize();
	/* the rectangle in the surface's coordinates, in its pixels (0x81147baa) and in the
	   pixels it shows at (0x81147c68) (names ours) */
	math::rect ToSurfaceRect(const math::rect &rect);
	math::rect ToPixelRect(const math::rect &rect);

	intrusive_ptr<graph::Surface> GetSurface()
	{
		return m_surface;
	}

	int32_t padding;
	math::v4 clear_color;
	bool is_auto_delete;
	bool is_cleared;
	math::rect vp;
	math::rect sp;
	int32_t clear_policy;
	intrusive_ptr<graph::Surface> m_surface;
	uint8_t m_whandle[0xc];
	int32_t reserved[2];

	static Screen *s_root_screen;
	/* where widgets whose camera does not roll the screen are drawn from */
	static math::v4 s_draw_offset;
	/* the root screen's surface */
	static graph::DisplaySurface *s_root_surface;
};

class Widget;
class SceneResolutionList;

/*
 * What the resource of a resolution is read with: the name of its strings' and
 * textures' variant and its value (names ours).
 */
class ResolutionResource {
public:
	ResolutionResource() : m_name(s_empty_name), m_value(0)
	{
	}

	string m_name;
	uint32_t m_value;
};

/* the screen resolutions the layouts are made for, and the one to use */
class EnvironmentParam {
public:
	EnvironmentParam(SceneResolutionList *resolution_table = NULL);
	virtual ~EnvironmentParam();
	/* the layout resolution for a screen of 1920x1088 or more */
	virtual uint32_t GetResolutionIdHD(uint32_t width, uint32_t height, int32_t orientation);
	/* the layout resolution for a screen type below 4 */
	virtual uint32_t GetResolutionId(uint32_t type, int32_t orientation);
	virtual EnvironmentParam *Clone() const;

	/* the largest resolution the display fits */
	void GetScreenSize(uint32_t *width, uint32_t *height);

	SceneResolutionList *m_resolution_list;
	uint8_t reserved[0xc];
};

/* the modal widgets, 32 bytes each, sorted after one is added when one was removed (names ours) */
class ModalWidgetList {
public:
	/* takes the widget out: -1 when it is not in the list (0x8113f74c) */
	int32_t Remove(Widget *widget);

	uint8_t reserved[4];
	bool m_unsorted;
	void *m_entries;
	uint32_t m_count;
};

/* where a touch hit a widget (0x90 bytes, filled by Environment::GetHitStack; Sony's class
   name from the export, the fields' names ours) */
class HitInfo {
public:
	HitInfo() : reserved3(0.0f, 0.0f, 0.0f, 1.0f)
	{
		sce_paf_memset(this, 0, sizeof(HitInfo));
	}

	uint8_t reserved1[0x10];
	/* the touch's position, and where it hit (copied by AppIconBase at a touch down) */
	math::v2 m_touch_pos;
	math::v2 m_hit_pos;
	uint8_t reserved2[0x30];
	math::v4 reserved3;
	uint8_t reserved4[0x24];
	/* the touch panel, 0 front or 1 rear (name ours) */
	uint32_t m_port;
	/* the touch ID (the Environment's m_touch_hits; name ours) */
	uint32_t m_touch_id;
	Widget *m_widget;
};

/* 0x80 bytes, at 0x330 of the Environment (names ours) */
class TouchState {
public:
	/* the panel, 0 front or 1 rear, and the touch ID, set by the widget taking it */
	uint32_t m_port;
	uint32_t m_id;
	/* the touch's last event type (0x42 released) */
	uint32_t m_type;
	/* m_line_start and m_line_end are taken through a camera (ScrollViewBase::GetTrackPos) */
	bool m_has_line;
	/* where the touch is (DirectBar's drag out of its bar) */
	math::v2 m_pos;
	float reserved_b;
	/* the camera's distance the line is taken to */
	float m_depth;
	/* the line through the scene under the touch, from its start to its end (ImageSlideBar's
	   CalcHitLineToPlane) */
	math::v4 m_line_start;
	math::v4 m_line_end;
	/* the world matrix of the camera the line is taken through */
	math::matrix m_camera_matrix;
};

/* a widget's hold on a touch, kept by the Environment for each port and touch ID (names
   ours) */
class TouchCapture {
public:
	uint32_t m_port;
	uint32_t m_id;
	Widget *m_widget;
	void *m_touch;
};

/* the focus: the widget drawn as the focus cursor and the widget it is on (Sony's class name,
   from its exports; the members' names ours) */
class FocusInfo {
public:
	/* whether the cursor is moving or resizing (0x81142952) */
	bool IsMoving();
	/* the widget taken out of the focus history */
	void RemoveFocusHistory(Widget *widget);
	/* the widget the focus left for the one going the other way of the direction, NULL for
	   none */
	Widget *FindFocusHistory(Widget *widget, uint32_t dir);
	/* the move from the widget to the next one in the direction kept */
	void AddFocusHistory(Widget *widget, Widget *next, uint32_t dir);

	Widget *m_cursor;
	Widget *m_target;
};

class Environment {
public:
	typedef int32_t (*SoundCallback)(const char *buffer1, uint32_t size1, const char *buffer2,
					 uint32_t size2, int32_t samplerate, float vol,
					 bool ignore_init, void *data);
	typedef Widget *(*CreateWidgetCallback)(Widget *parent, const char *type,
						const IDParam &id, const IDParam &style,
						void *data);

	virtual ~Environment();

	/* whether nothing the widgets still run keeps the framework from ending */
	bool IsTermAvailable();
	/* lets the widgets end: m_term_requested once nothing keeps them */
	int32_t RequestTerm();
	/* the widget that holds the port's pad keys of the low group (0x1f) or the high group
	   (0x3e0) lets go of them: -1 where none holds them (0x8113f4be, 0x8113f470; names
	   ours) */
	int32_t ReleasePadCaptureLow(uint32_t port);
	int32_t ReleasePadCaptureHigh(uint32_t port);
	/* a frame's steps, in the order the Framework runs them */
	void OnUpdatePre();
	void EventLoop();
	void OnUpdatePost();
	void ShowFocus(int32_t port);
	/* where the touch that hit the widget hit it: 0, or -1 when none did */
	int32_t GetHitStack(HitInfo *info, Widget *widget);
	/* the display area scale from the system settings, once */
	void ApplyDisplayAreaSettings();
	/* the display area scale read again, once it has been applied (name ours) */
	void UpdateDisplayAreaSettings();
	/* nothing is drawn now (0x8113dcca; name ours) */
	bool IsDrawSkipped();
	/* the widget is not below the modal widgets in depth (0x8113fa84; name ours) */
	bool IsOverModal(Widget *widget);
	/* its camera's roll, 0 without one (0x8113dce2; name ours) */
	float GetRollScreen();

	/* the gesture listener of the touch port, NULL for another port (inline in Sony's callers,
	   name ours) */
	common::SharedPtr<inputdevice::InputGestureListener> GetTouchGestureListener(uint32_t port)
	{
		if (port == 0)
			return m_touch_gesture_listener_front;
		if (port == 1)
			return m_touch_gesture_listener_rear;
		return common::SharedPtr<inputdevice::InputGestureListener>();
	}

	/* set: the widgets the focus leaves are kept in its history (name ours) */
	bool m_focus_history_on;
	uint8_t reserved[0x3f];
	/* the display area scale the root surface shows at (names ours) */
	float m_display_area_scale;
	bool m_display_area_applied;
	bool m_display_area_changed;
	uint8_t reserved8[0x2e];
	/* the gesture listener of the pad (name ours) */
	inputdevice::InputGestureListener *m_pad_gesture_listener;
	uint8_t reserved8a[4];
	/* the gesture listeners of the front (0) and rear (1) touch ports (names ours) */
	common::SharedPtr<inputdevice::InputGestureListener> m_touch_gesture_listener_front;
	common::SharedPtr<inputdevice::InputGestureListener> m_touch_gesture_listener_rear;
	uint8_t reserved8b[0x24];
	/* the Framework's InitParam::screen_orientation_mode and env_param */
	int32_t m_screen_orientation_mode;
	uint8_t reserved0[4];
	EnvironmentParam *m_env_param;
	uint8_t reserved7[4];
	/* the widget whose hit rect is the screen a rear touch scrolls within (TouchScrollSupplier's
	   rear mochi gesture; name ours) */
	Widget *m_root_widget;
	uint8_t reserved7a[0xd];
	/* set by RequestTerm */
	bool m_term_requested;
	/* set by the power callback while the system suspends: the frame is not drawn (name
	   ours) */
	bool m_suspended;
	uint8_t reserved1[1];
	/* the Framework's InitParam::env_option */
	bool m_env_option;
	uint8_t reserved2[3];
	SoundCallback m_sound_callback;
	void *m_sound_callback_data;
	CreateWidgetCallback m_create_widget_callback;
	void *m_create_widget_callback_data;
	uint8_t reserved3[4];
	/* the modal widget on top: a button below it in depth is not pressed while it is modal with
	   option 1 (name ours) */
	Widget *m_top_modal_widget;
	/* RegisterModalWidget's widgets (name ours) */
	ModalWidgetList m_modal_widgets;
	uint8_t reserved3b[8];
	/* the scene the display area scale is applied to (Camera's DrawScene; name ours) */
	Widget *m_display_area_scene;
	uint8_t reserved3c[0x44];
	/* set before the focus is first shown */
	int32_t m_focus_mode;
	/* set: a scroll view does not take the focus by itself (name ours, meaning open) */
	bool m_focus_auto_off;
	uint8_t reserved4[7];
	/* the focus cursor and the widget it is on */
	FocusInfo m_focus_cursor;
	uint8_t reserved4a[0x18];
	/* cleared with m_rear_touch_move when a scroll view takes a touch (names ours, meaning
	   open) */
	math::v4 m_front_touch_move;
	uint8_t reserved4e[0x50];
	/* the focus cursor is drawn with its widget only while it moves or the widget turns its
	   focus directions off (Camera's draw walk; name ours) */
	uint8_t reserved4b : 4;
	bool m_focus_cursor_moving_only : 1;
	uint8_t reserved4c : 3;
	uint8_t reserved4d[0x3f];
	math::v4 m_rear_touch_move;
	uint8_t reserved4f[0xa0];
	/* the events passed on at the next frame (names after ScePaf-RE) */
	EventQueue m_ev_queue;
	list<Event> m_ev_subrouting;
	list<Event> m_ev_doevent;
	/* the scenes, updated each frame (name after ScePaf-RE) */
	list<Widget *> m_scenes_onupdate;
	/* the screen SetScreen gives */
	Screen *m_screen;
	/* a state per touch ID, the front panel's 0 to 7 and the rear's 8 to 15: the widget
	   taking a touch sets its port and ID (names ours) */
	TouchState m_touch_state[16];
	/* where each touch ID last hit (name ours) */
	HitInfo m_touch_hit[8];
	uint8_t reserved5b[0xd80];
	/* the widget capturing each touch of the front and rear panels (name ours) */
	TouchCapture m_touch_capture[2][8];
	uint8_t reserved5d[0xf00];
	/* the widgets the touches are on (TouchScrollSupplier's checks; name ours) */
	list<HitInfo> m_touch_hits;
	uint8_t reserved5c[0x32];
	/* each frame waits for the GPU once it is drawn (Framework::Option_4) */
	bool m_finish_after_draw;
	uint8_t reserved6[0x35];
};

}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* the screen size of each resolution, qHD, HD and full HD (EnvironmentParam's rodata, defined with
   the stand-ins in state.c until its file is C; names ours) */
extern const uint32_t g_screen_width_qhd;
extern const uint32_t g_screen_height_qhd;
extern const uint32_t g_screen_width_hd;
extern const uint32_t g_screen_height_hd;
extern const uint32_t g_screen_width_full_hd;
extern const uint32_t g_screen_height_full_hd;

/* the resource variant of the parameter's resolution with the ID, or the empty one */
paf::ui::ResolutionResource ScePafWidget_E963EAEE(const paf::ui::EnvironmentParam *param,
						  uint32_t id);
/* constructs the Environment at mem */
paf::ui::Environment *ScePafWidget_393F0EF0(void *mem, paf::ui::EnvironmentParam *param,
					    int32_t screen_orientation_mode);
/* whether the buttons move the focus */
void ScePafWidget_75FB31F1(paf::ui::Environment *env, bool enable);
void ScePafWidget_B4A4660B(paf::ui::Environment *env, int32_t enable, int32_t port, int32_t mode);
/* whether the port's focus may be shown */
bool ScePafWidget_A6B659BB(paf::ui::Environment *env, int32_t port);
/* bit 0 of the port's focus state; for a port below 0, whether any port has it */
bool ScePafWidget_529FB877(paf::ui::Environment *env, int32_t port);
/* the touches of the port (0 front, 1 rear) let go of by the widgets that capture them, all
   but the widget's, their releases handled where release is set: -1 for another port (no
   name found) */
int32_t ScePafWidget_ABF05727(paf::ui::Environment *env, uint32_t port, bool release,
			      paf::ui::Widget *keep);
/* updates the input devices */
int32_t ScePafWidget_909850C4(void);
/* sets the focus frame texture no (below 9); with none, reads all of them from the common resource */
void ScePafWidget_B6C64486(const paf::intrusive_ptr<paf::graph::Surface> &texture, int32_t no);
/* begins a frame on the root surface */
void ScePafWidget_9D4A5A0E(paf::ui::Environment *env);
void ScePafWidget_DC1B19DB(paf::ui::Environment *env);
/* ends the frame on the root surface */
void ScePafWidget_851911CB(paf::ui::Environment *env);
/* shows the frame drawn; below 0 on failure */
int32_t ScePafWidget_F67517A0(paf::ui::Environment *env);
void ScePafWidget_8E61FE0C(paf::ui::Environment *env);
/* always 0 */
int32_t ScePafWidget_998F8F48(void);
/* loads the effects whose bits are set from the folder */
int32_t ScePafWidget_BA6EB0C6(uint32_t effects, const paf::string &dir);
}

#endif
