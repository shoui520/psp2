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
#include <std/map>
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
#include <graphics/gutil.h>
#include <std/vector>

namespace paf {
namespace graph {
class Surface;
class DisplaySurface;
}

namespace gutil {
class WinHandle;
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
	/* of the base's surface, viewport and scissor (NULL: none); the exports C0ABDE05 (C1) and
	   8A0CAAE7 (C2) */
	Screen(Screen *base);
	virtual ~Screen();

	static Screen *GetRootScreen()
	{
		return s_root_screen;
	}

	/* the root screen made of a surface of the window's size, with the viewport size given
	   (below 1: the surface's) (0x81148184; name ours), and given up (0x811483c8) */
	static int32_t Init(gutil::WinHandle whandle, int32_t vp_width, int32_t vp_height);
	static int32_t Term();
	/* the root screen given up for another, which is the one the root surface is of */
	static void SetRootScreen(Screen *screen);
	/* the frame on the root surface ended and shown (0x811484c2) */
	static int32_t Swap();
	/* the screen made the target of the draws that follow, and given up (0x81147e52,
	   0x81147ff4): 0, or 1 when it is the target already, -1 for none */
	static int32_t PushTarget(Screen *screen);
	static int32_t PopTarget(Screen *screen);

	/* the surface given in place of the one it had, which is returned (0x81147828) */
	intrusive_ptr<graph::Surface> SetSurface(intrusive_ptr<graph::Surface> surface);

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
	gutil::WinHandle m_whandle;
	/* the YUV conversion the surface is drawn with (ScreenSurface's, names ours) */
	uint32_t m_yuv_csc_index;
	int32_t m_yuv_profile;

	static Screen *s_root_screen;
	/* where widgets whose camera does not roll the screen are drawn from */
	static math::v4 s_draw_offset;
	/* the root screen's surface */
	static graph::DisplaySurface *s_root_surface;
	/* the screens the draws go to, the last the target */
	static vector<Screen *> s_target_list;
};

class Widget;
class Camera;
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

/* a widget in an ordered list of the environment's (32 bytes; names ours) */
class OrderEntry {
public:
	OrderEntry() : m_widget(NULL), m_mark(-1), m_order(-1), m_depth(-1), m_group(-1)
	{
	}

	/* the entry for the widget with the mark: its scene's priorities and its depth in the scene
	   (0x8113bcd4); -1 for no widget (the entry then has none of them) */
	int32_t Init(Widget *widget, int32_t mark);

	/* the scene's priority first, then the widget's depth: the entries are sorted by it */
	bool operator<(const OrderEntry &rhs) const
	{
		return m_order == rhs.m_order ? m_depth < rhs.m_depth : m_order < rhs.m_order;
	}

	Widget *m_widget;
	/* the mark the list's user gave it: UpdateFocus leaves 0x0f0f0f0f on the entry it chose */
	int32_t m_mark;
	/* 255 less the scene's widget priority, over the scene's priority shifted up a byte */
	uint32_t m_order;
	/* 64 bits: all ones less the widget's depth (GetLayerDepth), or all ones */
	uint64_t m_depth;
	/* the scene's entry */
	uint32_t m_group;
};

/* the environment keeps three of these: the modal widgets, the widgets of m_ordered and the
   focus candidates; entries sorted after one is added (names ours) */
class ModalWidgetList {
public:
	/* takes the widget out: -1 when it is not in the list (0x8113f74c) */
	int32_t Remove(Widget *widget);
	/* adds the widget with the mark, 0 where it has no order (0x8113f4e4) */
	OrderEntry *Add(Widget *widget, int32_t mark);
	/* sorts the entries where one was added or taken out (0x8113bbba) */
	void Sort();
	/* the entries made again from their widgets and sorted (0x8113bd48; name ours) */
	void Refresh();

	/* the widget the list is for (name ours, meaning open) */
	Widget *m_widget;
	bool m_unsorted;
	vector<OrderEntry> m_entries;
};

/* where a touch hit a widget (0x90 bytes, filled by Environment::GetHitStack; Sony's class
   name from the export, the fields' names ours) */
class HitInfo {
public:
	HitInfo() : m_widget_rot(0.0f, 0.0f, 0.0f, 1.0f)
	{
		sce_paf_memset(this, 0, sizeof(HitInfo));
	}

	/* where the touch is, as SpinBase's drum takes it when the touch goes down (name ours) */
	math::v4 m_pos;
	/* the touch's position, and where it hit (copied by AppIconBase at a touch down) */
	math::v2 m_touch_pos;
	math::v2 m_hit_pos;
	/* the two halves read as one position (the hit in the widget's space; name ours) */
	math::v4 &LocalPos()
	{
		return *reinterpret_cast<math::v4 *>(&m_touch_pos);
	}
	/* the hit taken through the widget's local matrix (CalcHitLineToPlane; name ours) */
	math::v4 m_local_hit;
	uint8_t reserved2[0x10];
	/* the widget's world position, world rotation, size and align offset when the hit was
	   found (names ours) */
	math::v4 m_widget_pos;
	math::v4 m_widget_rot;
	math::v4 m_widget_size;
	math::v4 m_widget_scale;
	/* set once the hit on the touch's line is found (Text's touch position finds it where it
	   is not; name ours) */
	uint32_t m_hit_valid;
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
	/* the touch's state (the Environment's m_touch_state for the port and ID; the drum of
	   SpinBase reads its line end) */
	TouchState *m_touch;
};

/* the focus: the widget drawn as the focus cursor and the widget it is on (Sony's class name,
   from its exports; the members' names ours) */
class FocusInfo {
public:
	/* whether the cursor is moving or resizing (0x81142952) */
	bool IsMoving();
	/* the cursor moved after the widget it is on, the time since the last frame given (0x81142982;
	   name ours) */
	void Update(float time);
	/* the cursor shown, where it may be (0x81143f5c) */
	void Show();
	/* the cursor hidden (0x81143fe2) */
	void Hide();
	/* the highlight of the widget the cursor is on and of the one it left, set (0x81141b28;
	   name ours) */
	void UpdateHighlight();
	/* the name of the widget the cursor is on written on the automatic tester's TTY (0x811438a0;
	   name ours) */
	void PrintTarget();
	/* the widget taken out of the focus history */
	void RemoveFocusHistory(Widget *widget);
	/* the widget the focus left for the one going the other way of the direction, NULL for
	   none */
	Widget *FindFocusHistory(Widget *widget, uint32_t dir);
	/* the move from the widget to the next one in the direction kept */
	void AddFocusHistory(Widget *widget, Widget *next, uint32_t dir);

	/* the widget the cursor goes under where it has no target (set by the cursor's creation,
	   0x81141750; name ours) */
	static Widget *s_cursor_parent;
	/* the cursor leaving its target for the next one (NULL for none): the widget groups told, the
	   position and shape of the target kept, the cursor sized to it and the target's cursor
	   state cleared; -1 where there is nothing to leave (0x8114439c; name ours) */
	int32_t ClearTarget(Widget *next);
	/* the cursor moved onto the widget: the old target left, the new one told, and the cursor
	   put under it (0x81144516; name ours) */
	int32_t SetTarget(Widget *next);

	/* a move of the focus: the widget it left, the one it went to and the direction (name ours) */
	class History {
	public:
		Widget *m_widget;
		Widget *m_next;
		uint32_t m_dir;
	};

	Widget *m_cursor;
	Widget *m_target;
	/* the widget the cursor was on before (UpdateHighlight; name ours) */
	Widget *m_previous;
	uint8_t reserved0[4];
	/* where the target was (its world position) when the cursor left it (name ours) */
	math::v4 m_target_pos;
	/* cleared with the other port's where a scroll view takes a touch (name ours, meaning open) */
	math::v4 m_touch_move;
	uint8_t reserved1[32];
	/* the size of the target's focus shape (GetFocusShapeSize) when the cursor left it, which
	   the cursor is sized to (name ours) */
	math::v4 m_shape;
	uint8_t reserved1a[16];
	/* how the cursor moved last; 1, 6 and 7 hide it at once (Hide; name ours, meaning open) */
	uint32_t m_move_mode;
	uint8_t reserved1b[4];
	/* when the cursor left the last target for none, in microseconds (name ours) */
	uint64_t m_leave_time;
	/* the cursor is drawn with its widget only while it moves or the widget turns its focus
	   directions off (Camera's draw walk; name ours) */
	/* set while the cursor is shown; the shown state before it; set where the cursor may be shown
	   (Show, Hide, A6B659BB; names ours) */
	bool m_shown : 1;
	bool m_was_shown : 1;
	bool m_enabled : 1;
	/* m_target_pos and m_shape hold the last target's (name ours) */
	bool m_left_saved : 1;
	bool m_moving_only : 1;
	uint8_t reserved3 : 3;
	uint8_t reserved4[3];
	/* the last moves the focus made (one is kept) and how many */
	History m_history[1];
	uint32_t m_history_count;
	/* the texture the cursor is drawn with, released with it */
	graph::Surface *m_texture;
	/* the cursor's widget gave no texture and the default one is used (Show; name ours) */
	bool m_default_texture;
	uint8_t reserved5[7];
};

/* an entry of the environment's callback list: a function that ends it (names ours) */
class EnvCallback {
public:
	void (*m_destroy)(EnvCallback *callback);

	~EnvCallback()
	{
		m_destroy(this);
	}
};

class Environment {
public:
	typedef int32_t (*SoundCallback)(const char *buffer1, uint32_t size1, const char *buffer2,
					 uint32_t size2, int32_t samplerate, float vol,
					 bool ignore_init, void *data);

	/* the focus cursor hidden on the port (-1: all) */
	void HideFocus(int32_t port);
	typedef Widget *(*CreateWidgetCallback)(Widget *parent, const char *type,
						const IDParam &id, const IDParam &style,
						void *data);

	virtual ~Environment();

	/* the widget kept as a touch device region (up to 6), at the first free slot or at the index
	   (0x8113f3ce, 0x8113f3a2); taken off again (0x8113f40a) */
	/* the widget made one of the modal widgets, ordered with the others: -1 for a mode above 1
	   (0x8113f594; names ours) */
	int32_t RegisterModalWidget(Widget *widget, uint32_t mode);
	/* whether the widget is not behind the modal widgets (nor the second one): 1 where none is set
	   (0x8113fa84; name ours) */
	bool IsInFrontOfModal(Widget *widget);
	/* the ordered lists made again where the layer weights changed, or whatever (0x8113bdba; name
	   ours) */
	int32_t RefreshOrderLists(bool force);
	int32_t RegisterTouchDeviceRegion(Widget *widget);
	int32_t RegisterTouchDeviceRegion(uint32_t index, Widget *widget);
	int32_t RemoveTouchDeviceRegion(Widget *widget);
	/* the widget forgotten as the hit of any touch (0x8113fecc) */
	void ClearHitInfo(Widget *widget);
	/* the number of widgets in the scene of the entry, or the iterators at the start and the
	   end of its children (0x8113ef94, 0x8113efda, 0x8113f032); the entry shown or hidden
	   (0x8113ec20, 0x8113eca4) */
	int32_t GetSceneNum(int32_t entry);
	int32_t GetSceneTop(int32_t entry, list<Widget *>::iterator &top);
	int32_t GetSceneEnd(int32_t entry, list<Widget *>::iterator &end);
	int32_t ShowSceneEntry(int32_t entry);
	int32_t HideSceneEntry(int32_t entry);
	/* whether nothing the widgets still run keeps the framework from ending */
	bool IsTermAvailable();
	/* lets the widgets end: m_term_requested once nothing keeps them */
	int32_t RequestTerm();
	/* lets the widgets end and runs frames until nothing keeps them (0x8113db36; name ours) */
	int32_t Term();
	/* a frame drawn: begun on the root surface, the scenes drawn, ended (0x8113c31c) */
	void OnDraw();
	/* the widget that holds the port's pad keys of the low group (0x1f) or the high group
	   (0x3e0) lets go of them: -1 where none holds them (0x8113f4be, 0x8113f470; names
	   ours) */
	/* the focus chosen again among the focus candidates in front of the modal limit, the cursor
	   put on it: the widget given first is taken where it is in the chosen group, the second
	   loses its mark (0x8113ce6e; names ours) */
	void UpdateFocus(Widget *prefer, Widget *drop);
	int32_t ReleasePadCaptureLow(uint32_t port);
	int32_t ReleasePadCaptureHigh(uint32_t port);
	/* the widget given the port's pad keys of the low group (0x8113f496) or the high group
	   (0x8113f448), the widget holding them let go first: -1 for a port of 5 or more */
	int32_t SetPadCaptureLow(uint32_t port, Widget *widget);
	int32_t SetPadCaptureHigh(uint32_t port, Widget *widget);
	/* a frame's steps, in the order the Framework runs them */
	void OnUpdatePre();
	void EventLoop();
	void OnUpdatePost();
	/* the scenes' restructures done (0x8113be56) */
	void UpdateRestructScene();
	/* the touch device regions of the widgets set on the panel (0x8113c3c2) */
	void OnUpdateTouchDeviceRegion();
	/* the screen the draws go to; the old one is deleted if it was auto-deleted (0x81139ca0) */
	int32_t SetScreen(Screen *screen);
	void ShowFocus(int32_t port);
	/* where the touch that hit the widget hit it: 0, or -1 when none did */
	int32_t GetHitStack(HitInfo *info, Widget *widget);
	/* the last hit stack of the touches, 0, or -1 when there is none (0x8113ff98) */
	int32_t GetLastHitStack(HitInfo *info);
	/* whether the widget is one of the hit stack's parents, or has a hit stack (0x8113dea6) */
	bool IsHitStackParent(Widget *widget);
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
	/* the scene added to the scenes of the entry, before the widget (NULL: at the end), and
	   taken out of them (0x8113ed32, 0x8113eea2; names ours) */
	int32_t AddScene(Widget *scene, int32_t entry, Widget *before);
	int32_t RemoveScene(Widget *scene);
	/* the camera roll the screen orientation turns the scenes by (0x8113ca44; name ours) */
	float GetOrientationRoll(int32_t orientation);

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
	uint8_t reserved_5[0x13];
	/* what the environment runs at its end, each ended with a call of its first word (name ours,
	   use not found) */
	list<EnvCallback> m_callbacks;
	uint8_t reserved_36[4];
	/* the scenes by their entry (ShowSceneEntry, GetSceneNum; name ours) */
	map<int32_t, Widget *> m_scene_entries;
	/* the transitions still under way (IsTermAvailable counts them; meaning open, element type
	   unknown; name ours) */
	list<uint32_t> m_pending;
	uint8_t reserved_64[1];
	/* set when a scene's layer weights are made again (Scene::UpdateLayerWeight; name ours) */
	bool m_layer_weight_changed;
	uint8_t reserved_42[2];
	/* the display area scale the root surface shows at (names ours) */
	float m_display_area_scale;
	bool m_display_area_applied;
	bool m_display_area_changed;
	uint8_t reserved8[0x2e];
	/* the gesture listener of the pad (name ours) */
	common::SharedPtr<inputdevice::InputGestureListener> m_pad_gesture_listener;
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
	/* the root scene's camera (name ours) */
	Camera *m_root_camera;
	/* the scenes' cameras turn with the screen orientation in modes 0 and 1
	   (Scene::ResetCameraRoll; name ours) */
	uint8_t m_camera_roll_mode;
	uint8_t reserved7g[3];
	/* the state the root surface was in after the frame began (DisplaySurface's m_frame_state;
	   name ours), and whether the frame is drawn: it was 1 (name ours) */
	int32_t m_frame_state;
	bool m_draw_frame;
	/* set by RequestTerm */
	bool m_term_requested;
	/* set by the power callback while the system suspends: the frame is not drawn unless
	   m_draw_when_suspended (name ours) */
	bool m_suspended;
	bool m_draw_when_suspended;
	/* the Framework's InitParam::env_option */
	bool m_env_option;
	uint8_t reserved2[3];
	SoundCallback m_sound_callback;
	void *m_sound_callback_data;
	CreateWidgetCallback m_create_widget_callback;
	void *m_create_widget_callback_data;
	/* a widget that takes the cancel key like the modal one on top does (HandleKeycodeEvent; name
	   ours, meaning open) */
	Widget *m_cancel_widget;
	/* the modal widget on top: a button below it in depth is not pressed while it is modal with
	   option 1 (name ours) */
	Widget *m_top_modal_widget;
	/* RegisterModalWidget's widgets (name ours) */
	ModalWidgetList m_modal_widgets;
	/* the scene the display area scale is applied to (Camera's DrawScene; name ours) */
	Widget *m_display_area_scene;
	/* a second modal widget: UpdateFocus takes the one of the two in front as the limit of the focus
	   (name ours, meaning open) */
	Widget *m_second_modal_widget;
	/* the widgets that have m_ordered (ScePafWidget 0253DC9C; name ours) */
	ModalWidgetList m_ordered_widgets;
	/* lists of widgets by ID, each with a widget it is for first (ScePafWidget 0775F78F and
	   D131E927; names ours, meaning open) */
	map<uint32_t, ModalWidgetList> m_widget_groups;
	/* the widget that has the focus (UpdateFocus; name ours) */
	Widget *m_focus_widget;
	/* the widgets that can take the focus, ordered for UpdateFocus (name ours) */
	ModalWidgetList m_focus_candidates;
	/* set before the focus is first shown */
	int32_t m_focus_mode;
	/* set: a scroll view does not take the focus by itself (name ours, meaning open) */
	bool m_focus_auto_off;
	bool reserved4_345;
	/* the buttons move the focus (ScePafWidget_75FB31F1; name ours) */
	bool m_focus_enabled;
	uint8_t reserved4[5];
	/* the focus cursor and the widget it is on, of the front and the rear panel */
	FocusInfo m_focus_cursor;
	FocusInfo m_focus_cursor_rear;
	/* the texture the scenes' debug cubes are drawn with (Scene::SetDebugMode; name ours) */
	graph::Surface *m_debug_texture;
	/* the widget holding each port's pad keys of capture 0x17d (the keys other than the
	   directions and enter) and of 0x17c (the low group); five ports (names ours) */
	Widget *m_pad_capture_button[5];
	Widget *m_pad_capture_dir[5];
	uint8_t reserved4j[4];
	/* the events passed on at the next frame (names after ScePaf-RE) */
	EventQueue m_ev_queue;
	list<Event> m_ev_subrouting;
	list<Event> m_ev_doevent;
	/* the scenes, updated each frame (name after ScePaf-RE) */
	list<Widget *> m_scenes_onupdate;
	/* the screen SetScreen gives */
	Screen *m_screen;
	/* a state per port (the front panel's and the rear's) and touch ID: the widget taking a
	   touch sets its port and ID (names ours) */
	TouchState m_touch_state[2][8];
	/* where each touch ID last hit (name ours) */
	HitInfo m_touch_hit[2][8];
	/* the last hit of each touch of the front and rear panels (ClearHitInfo; name ours) */
	HitInfo m_touch_hit_last[2][8];
	/* the widget capturing each touch of the front and rear panels (name ours) */
	TouchCapture m_touch_capture[2][8];
	uint8_t reserved5d[0xf00];
	/* the widgets the touches are on (TouchScrollSupplier's checks; name ours) */
	list<HitInfo> m_touch_hits;
	/* the widgets IsHitStackParent takes as the hit stack's parents, cleared with
	   m_touch_hits (name after IsHitStackParent) */
	list<Widget *> m_hit_stack;
	/* the widgets kept as touch device regions and which of the six slots are used (name ours) */
	uint32_t m_touch_region_mask;
	/* the slots used when OnUpdateTouchDeviceRegion last set the regions (name ours) */
	uint32_t m_touch_region_mask_pre;
	Widget *m_touch_region[6];
	uint8_t reserved5c[6];
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
/* the widget the list with the ID is for, 0 for none (no name found) */
paf::ui::Widget *ScePafWidget_0775F78F(paf::ui::Environment *env, uint32_t id);
/* a widget taken out of the list with the ID, or the list emptied with none (no name found) */
int32_t ScePafWidget_D131E927(paf::ui::Environment *env, uint32_t id, paf::ui::Widget *widget);
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
/* begins a frame on the root screen, and ends it (0x81148456, 0x81148418; the Environment's
   versions are F67517A0 and 8E61FE0C) */
int32_t ScePafWidget_0313D591(void);
int32_t ScePafWidget_323CEF70(void);
/* loads the effects whose bits are set from the folder */
int32_t ScePafWidget_BA6EB0C6(uint32_t effects, const paf::string &dir);
}

#endif
