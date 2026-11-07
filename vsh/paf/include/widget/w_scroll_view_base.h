/*
 * paf::ui::ScrollViewBase: the base of the widgets that scroll their contents. Its children sit
 * in a ScrollViewParent it moves under its view box, dragged by touch, moved by the analog stick
 * and by keys, snapped to the snap points added to it, with scroll bars a controller lays out;
 * and ScrollView, the one whose snap points are its children.
 * Names after the exports, the parameters' names and ScePaf-RE; the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SCROLL_VIEW_BASE_H
#define PAF_WIDGET_W_SCROLL_VIEW_BASE_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/map>
#include <std/vector>
#include <timer/timer.h>
#include <widget/core/layouter.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ScrollBar;
class ScrollViewBase;
class ScrollViewOverScroll;
class ScrollViewParent;
class ScrollViewTimer;
class ScrollViewDebug;

/* a point the view can snap to */
class ScrollViewSnapPoint {
public:
	ScrollViewSnapPoint(const math::v2 &pos);

	math::v2 m_pos;
};

/* a focus kept within the view's limits: where it is kept, how far it was past them and
   along which axes (MoveDir; names ours) */
class ScrollViewFocusLimit {
public:
	math::v2 m_pos;
	math::v2 m_over;
	uint32_t m_dir;
};

/* what ScrollViewBase gives its controller to lay its scroll bars out (name ours) */
class ScrollViewBarLayout {
public:
	ScrollBar *m_horizontal_bar;
	ScrollBar *m_vertical_bar;
	/* the view's size */
	math::v2 m_view_size;
	/* the horizontal and the vertical bar's margins, with the view's padding */
	math::v4 m_horizontal_margin;
	math::v4 m_vertical_margin;
	/* whether the view moves along x and along y */
	bool m_movable_x;
	bool m_movable_y;
};

/* what ScrollViewBase gives its controller to set its scroll bars' values (name ours) */
class ScrollViewBarRange {
public:
	ScrollBar *m_horizontal_bar;
	ScrollBar *m_vertical_bar;
	/* how far the view is from the contents' start, and the contents' length past it */
	math::v2 m_value;
	math::v2 m_max;
	/* the view's size */
	math::v2 m_view_size;
	/* whether the bars show at once, else they fade out over the time (names ours) */
	bool m_no_fade;
	float m_fade_time;
	/* whether the contents outgrew the view since the bars were last shown */
	bool m_reset;
};

/* what lays a ScrollViewBase's scroll bars out and sets their values */
class ScrollViewScrollBarController {
public:
	ScrollViewScrollBarController()
	{
	}

	virtual ~ScrollViewScrollBarController()
	{
	}

	/* the bars sized to the view and placed along its edges */
	virtual void UpdateLayout(const ScrollViewBarLayout &layout);
	/* the bars' values set, each hidden first where the range is reset */
	virtual void UpdateRange(const ScrollViewBarRange &range);
	/* the bar's jet tail shown or hidden, and its least length (scaled by rate, from the
	   slider's length) set (names ours) */
	virtual void ShowJetTail(ScrollBar *bar, bool show);
	virtual void SetJetTailLength(ScrollBar *bar, float rate);
	/* the bar's values set and it shown, fading out over the time, where the contents are
	   longer than the view; else it is hidden */
	virtual void UpdateScrollBar(ScrollBar *bar, float value, float max, float visible,
				     float time);
};

/* a timer a ScrollViewBase moves its view by, telling it when it is deleted */
class ScrollViewTimer : public Timer {
public:
	enum TimerType {
		TIMER_TYPE_MOVE = 0,
		TIMER_TYPE_MOTION = 1,
		TIMER_TYPE_SNAP_AUTO = 2
	};

	ScrollViewTimer(ScrollViewBase *widget, TimerType type, float total, int32_t func,
			int32_t repeat, int32_t rep_count, float *param, int32_t userdata);
	/* the timer made from the template's time, function and repeats (names ours) */
	ScrollViewTimer(ScrollViewBase *widget, TimerType type, const Timer &timer);
	virtual ~ScrollViewTimer();

	ScrollViewBase *m_widget;
	TimerType m_type;
	/* whether the view's layout is left as it is when it is deleted (name ours) */
	bool m_keep_layout;
	int32_t m_userdata;
};

/* the parameters a ScrollViewBase moves its view by (names ours): how fast it slows, how far a
   flick throws it, how it is pulled back past its ends and snaps */
class ScrollViewScrollParam {
public:
	ScrollViewScrollParam();

	float m_value[23];
	bool m_bounce;
};

class ScrollViewBase : public Widget {
public:
	enum {
		OBJ_BG = 0,
		OBJ_SCROLL_VIEW_BASE_END = 1
	};

	enum {
		CB_SCROLL_VIEW_BASE_BEGIN = 0x10000000,
		CB_SCROLL_VIEW_BASE_FOCUS_CHANGED = 0x10000001,
		CB_SCROLL_VIEW_BASE_SNAPPED = 0x10000002,
		CB_SCROLL_VIEW_BASE_PREFER_SNAPPED = 0x10000003,
		CB_SCROLL_VIEW_BASE_SNAP_POINT_DETECTED = 0x10000004,
		CB_SCROLL_VIEW_BASE_END = 0x10000005
	};

	enum MoveDir {
		MoveDir_None = 0,
		MoveDir_X = 1,
		MoveDir_Y = 2,
		MoveDir_XY = 3
	};

	/* its sounds, after its sound parameter's names */
	enum {
		SOUND_PRESS = 1,
		SOUND_SNAP = 2
	};

	class Snap : public ScrollViewSnapPoint {
	public:
		Snap(const ScrollViewSnapPoint &snap_point);

		int32_t m_id;
	};

	static const char m_widget_type[];
	static const char *TypeName();

	static void CBScrollBarChanged(int32_t type, Handler *self, Event *e, void *data);

	ScrollViewBase(Widget *parent, CreateParam *param);
	virtual ~ScrollViewBase();

	virtual int32_t HandleTouchFwEvent(Event *e);
	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual int32_t HandleFocusEvent(Event *e);
	virtual int32_t HandleFocusSwitchEvent(Event *e);
	virtual int32_t HandleCommandEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual uint32_t CheckState();
	virtual Widget *GetInternalWidget();
	virtual void UpdateSize();
	virtual void UpdateAfterChildren();
	virtual void UpdatePost();
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index);
	virtual int32_t SetString(const wstring &str, int32_t index);
	virtual int32_t GetString(wstring &str, int32_t index);
	using Widget::SetTexture;
	using Widget::GetTexture;
	using Widget::SetString;
	using Widget::GetString;

	/* the slots after Widget's, from 0x170 (index 92) */
	virtual void SetVerticalScrollBar(ScrollBar *scroll_bar);
	virtual void SetHorizontalScrollBar(ScrollBar *scroll_bar);
	virtual void SetScrollBarController(ScrollViewScrollBarController *controller);
	virtual int32_t AddSnapPoint(const ScrollViewSnapPoint &snap_point);
	virtual void DelSnapPoint(int32_t snap_id);
	virtual void UpdateSnapPoint(int32_t snap_id, const ScrollViewSnapPoint &snap_point);
	virtual void SetSnapPower(float snap_power);
	virtual void DoSnap(int32_t snap_id, Timer *timer);
	virtual void SetPreferredSnapPoint(int32_t snap_id, float time, int32_t func);
	/* the view moved to the position, over the timer where one is given */
	virtual void SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b);
	virtual void OnViewLayoutUpdated(bool is_dirty);
	virtual void OnPartsLayoutUpdated(bool is_dirty);
	virtual bool TrackBegin(Event *e);
	virtual bool TrackEnd(Event *e);
	virtual bool TrackPress(Event *e);
	virtual bool TrackLongPress(Event *e);
	virtual bool TrackRelease(Event *e);
	virtual bool TrackGrab(Event *e);
	virtual bool TrackMove(Event *e);
	virtual bool TrackDrag(Event *e);
	/* the views moved by gesture 0x16, the touch's pan, by its move (E0FAE144; name ours) */
	virtual bool TrackPan(Event *e);
	/* a text box in the view begins editing: the view padded above the IME, the text box
	   kept in sight (TextBoxImeListener::EVENT_EDIT_BEGIN; 4770835D, name ours) */
	virtual bool OnEditBegin(Event *e);
	/* the text box ends editing: the IME padding off (EVENT_EDIT_END; 1DA7A46A, name ours) */
	virtual bool OnEditEnd(Event *e);
	/* the IME opened, resized or edited: the padding fitted to its rectangle, the text box
	   kept in sight (EVENT_IME_OPEN to EVENT_IME_EDIT; 423AF7BA, name ours) */
	virtual bool OnImeEvent(Event *e);
	/* the up, down, left and right keys pressed or repeated: the focus moved that way while
	   no touch is tracked (E6DD4083, F8E68E31, 422E30D5, 0F056788; names ours) */
	virtual bool OnUpKey(Event *e);
	virtual bool OnDownKey(Event *e);
	virtual bool OnLeftKey(Event *e);
	virtual bool OnRightKey(Event *e);
	/* the focus events 0x401 to 0x404 (HandleFocusEvent; names ours): the focus coming in
	   given to the widget the view's pending move finds; 0x402 and 0x403 not taken; the
	   focus going out moving the view on where it can still scroll, and passed to the
	   parent views (33742E18, 797EE334, 7B155BE4, 08D85CB4) */
	virtual bool OnFocusIn(Event *e);
	virtual bool OnFocusEnter(Event *e);
	virtual bool OnFocusLeave(Event *e);
	virtual bool OnFocusOut(Event *e);
	/* the tracked touch flicked: the views captured and dragged by the flick's velocity over
	   a frame (gesture 3; 2FDBFE08, name ours) */
	virtual bool OnFlick(Event *e);
	/* gesture 7 taken where it is on the tracked touch (CEBDCDB3; name ours) */
	virtual bool TakeTrackedGesture(Event *e);
	virtual bool MochiBegin(Event *e);
	virtual bool MochiMove(Event *e);
	virtual bool ScePafWidget_3196DEE8(Event *e);
	virtual bool ScePafWidget_46CD91F6(Event *e);
	virtual bool ScePafWidget_0BC06625(Event *e);
	virtual bool ScePafWidget_2141506E(Event *e);
	virtual bool ScePafWidget_0ECFFCE1(Event *e);
	virtual bool ScePafWidget_3403FD90(Event *e);
	virtual bool ScePafWidget_22F22B80(Event *e);
	virtual bool ScePafWidget_C8C679BB(Event *e);
	virtual bool ScePafWidget_7F653E08(Event *e);
	virtual void SetViewBox(const math::v4 &view_box);
	/* the padding between the view box and the contents' ends (name ours) */
	virtual void SetPadding(const math::v4 &padding);
	virtual void ScePafWidget_A0538225(int32_t snap_id);
	virtual void UpdateScrollRange();
	virtual void GetBarLayout(ScrollViewBarLayout &layout);
	virtual void UpdateScrollLayout();
	virtual void UpdateMovableDirection();
	/* the view parent moved to the position, over the timer (name ours) */
	virtual void MoveViewParent(const math::v2 &pos, Timer *timer);
	virtual void OnChildRegistered(Widget *child);
	virtual void OnChildRemoved(Widget *child);
	virtual void OnDestroyTimer(ScrollViewTimer *timer);
	virtual void OnScrollBarChanged(int32_t type, Handler *self, Event *e);
	/* the view moved by the drag's move, the part of it the view could not move left in move
	   (no name found) */
	virtual void ScePafWidget_D95AFF11(math::v2 &move, bool dragging, bool locked, bool reset);
	/* the touch it tracks (m_track_port; no name found) */
	virtual void ScePafWidget_E9C7617A(uint32_t touch_id);
	/* the touch released: the view flicked in the direction, at the velocity */
	virtual void ScePafWidget_D9FE36A6(int32_t id, const math::v2 &dir, const math::v2 &velocity);
	/* the focus moved in the direction, from the widget or the focus (no name found) */
	virtual bool ScePafWidget_AD706F2A(const math::v2 &dir, Widget *from);
	virtual void ScePafWidget_22EF63CE();
	virtual void ScePafWidget_B6CECFDF();

	ScrollViewSnapPoint GetSnapPoint(int32_t snap_id);
	int32_t FindSnapID(const ScrollViewSnapPoint &snap_point);
	float GetSnapPower();
	/* where the view is: its contents' position, negated */
	math::v2 GetViewFocus();
	math::v2 GetViewFocusTarget();
	void BlockUpdateViewPort();
	void UnblockUpdateViewPort();
	void SetDefaultScrollBar();
	/* a scroll bar of the type (ScrollBar::ScrollType) made for it: from the environment's
	   widget factory, else a plain one whose slider is 14 long at the least (ScePafWidget 2ADE6924; name ours) */
	ScrollBar *CreateScrollBar(int32_t type);
	/* the axes it moves along, without those it may not where masked is set (ScePafWidget
	   3546C104; name ours) */
	uint32_t GetMovableDirection(bool masked);
	/* what pulls the view back past its ends: the object (deleted with the view), or the
	   default one where on is set (ScePafWidget 5FDEA5C3, 14BD2C88; names ours) */
	void SetOverScroll(ScrollViewOverScroll *over_scroll);
	void EnableOverScroll(bool on);
	/* the margins about the vertical and the horizontal scroll bar, the bars laid out again when
	   they change (ScePafWidget 46F36554, A9702621; names ours) */
	void SetVerticalBarMargin(const math::v4 &margin);
	void SetHorizontalBarMargin(const math::v4 &margin);
	void SendFocusChangeEvent();
	/* the scroll bars' fade time, 200 where it is negative, and the bars hidden at the next
	   range update (ScePafWidget 50E4EA5C, A4E53390; names ours) */
	void SetBarFadeTime(float time);
	void ResetBarShown();
	/* the time it takes to snap, 200 where it is negative (ScePafWidget 0DCB8EA5; name ours) */
	void SetSnapTime(float time);
	/* its debug view: shown in the mode, or deleted for mode 0 (ScePafWidget 3F489314; name
	   ours) */
	void SetDebugMode(int32_t mode);
	/* the axes it may move along and those it may not, and the axes it is set to move along
	   (ScePafWidget 511C900C, FE7FE5A0, C539DCD4; names ours) */
	void SetMovableDirOn(uint32_t dir);
	void SetMovableDirOff(uint32_t dir);
	void SetMoveDirection(uint32_t dir);
	/* its scroll parameters, a copy of these (ScePafWidget C0459458; name ours) */
	void SetScrollParam(const ScrollViewScrollParam *param);
	/* its origin's world position kept while on (ScePafWidget E9BF2DBE; name ours) */
	void EnableAnchor(bool on);
	/* how far a key moves the view (ScePafWidget 93C21B53; name ours) */
	void SetKeyStep(float step);

	/* the view kept where its origin was in the world while anchoring is on (ScePafWidget
	   EA4E6C5D; name ours) */
	void UpdateAnchor();
	/* the half view, the positions the view is kept in and the axes it fits along, from its
	   size (ScePafWidget 2A1C86B1; name ours) */
	void UpdateViewLimit();
	/* the view moved to show the rectangle above the IME, over the timer where one is given;
	   the timer is the view's (ScePafWidget 11A7D5E8; name ours) */
	void ScrollToRect(const math::rect &rect, Timer *timer);
	/* the padding in use, the IME's included, and the scroll box from it (ScePafWidget
	   7D7D54F5; name ours) */
	void UpdatePadding();
	/* the padding, the limits, the layout and the range brought up to date with the IME (name
	   ours) */
	void UpdateImeLayout();
	/* the view padded above the IME while on (ScePafWidget E42A2262; name ours) */
	void SetImePadding(bool on);
	/* the IME's height taken from its rectangle (ScePafWidget 06315921; name ours) */
	void SetImeRect(const math::rect &rect);

	/* the nearest scroll view it sits in, NULL for none (ScePafWidget 5CB1000F; name ours) */
	ScrollViewBase *GetParentView();
	/* where the touch it tracks is on its plane (ScePafWidget A1ED1315; name ours) */
	math::v2 GetTrackPos();
	/* the scroll range updated where its world rotation changed (name ours) */
	void UpdateWorldRot();
	/* the focus kept within the view's limits, at the center along the axes that fit where
	   center_fit is set (ScePafWidget 1EB87BE1; name ours) */
	ScrollViewFocusLimit LimitFocus(const math::v2 &focus, bool center_fit);
	/* the widget the focus goes to in the direction, from the widget or the focus; NULL for
	   none or itself (name ours) */
	Widget *SearchFocus(const math::v2 &dir, Widget *from);

	void ResetScrollBar(ScrollBar *&scroll_bar, ScrollBar *next_bar);
	list<Snap>::iterator FindSnapPoint(int32_t snap_id);
	void SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3);

	/* the touch port it tracks, -1 for none */
	int32_t m_track_port;
	int32_t m_track_state;
	/* how fast the view moves */
	math::v2 m_velocity;
	/* the widget its children sit in, which it moves */
	ScrollViewParent *m_view_parent;
	ScrollViewScrollBarController *m_bar_controller;
	ScrollBar *m_vertical_bar;
	ScrollBar *m_horizontal_bar;
	/* the vertical and the horizontal bar's margins */
	math::v4 m_vertical_bar_margin;
	math::v4 m_horizontal_bar_margin;
	/* the box the view shows, and its size */
	math::v4 m_view_box;
	math::v2 m_view_box_size;
	/* the view box with the padding, its size and its center negated */
	math::v4 m_scroll_box;
	math::v2 m_scroll_box_size;
	math::v2 m_scroll_center;
	/* the padding, as set and as kept clear of what covers the view */
	math::v4 m_padding;
	math::v4 m_padding_used;
	math::v4 m_padding_sign;
	/* its size, half of it each way, and the positions the view is kept in */
	math::v2 m_view_size;
	math::v4 m_view_half;
	math::v4 m_view_limit;
	/* the axes along which the contents are smaller than the view (MoveDir) */
	uint32_t m_fit_dir;
	/* the axes it moves along, those it may and those it may not (MoveDir) */
	uint32_t m_movable_dir;
	uint32_t m_movable_dir_y;
	uint32_t m_movable_dir_on;
	uint32_t m_movable_dir_off;
	/* the axes it is set to move along (MoveDir; name ours) */
	uint32_t m_move_dir;
	uint32_t m_scroll_flags;
	/* its world rotation when the scroll range was last updated */
	math::quaternion m_world_rot_pre;
	math::v2 m_focus_pos;
	math::v2 m_focus_target;
	math::v2 m_focus_start;
	/* the timer the view moves by */
	ScrollViewTimer *m_focus_timer;
	/* how many times BlockUpdateViewPort is in force */
	int32_t m_block_count;
	/* what shows its view box and scroll range where debugging is on */
	ScrollViewDebug *m_debug;
	ScrollViewScrollParam *m_scroll_param;
	ScrollViewOverScroll *m_over_scroll;
	/* where its origin sat in the world when anchoring was turned on, else NULL (name ours) */
	math::v2 *m_anchor;
	bool m_enable_analog;
	/* the focus is kept within the view's limits (LimitFocus; name ours) */
	bool m_limit_focus;
	list<Snap> m_snap_list;
	float m_snap_power;
	/* the snap point it snaps to when it stops, its timer's time and function */
	int32_t m_preferred_snap_id;
	float m_preferred_snap_time;
	int32_t m_preferred_snap_func;
	/* the time it takes to snap (name ours) */
	float m_snap_time;
	float m_key_step;
	float m_over_scroll_length;
	/* the scroll bars' fade time */
	float m_bar_fade_time;
	bool m_bar_reset;
	bool m_scroll_key;
	/* the view scrolls by touch (BeginTouch reads it) */
	bool m_scroll_touch;
	bool m_scroll_analog;
	bool m_scroll_lock;
	bool m_ime_padding;
	float m_ime_height;
	bool m_scroll_ended;
	/* the axes a touch drags it along (MoveDir; name ours) */
	uint32_t m_drag_dir;
	uint32_t m_touch_mask_on;
	bool m_focus_wrap;
	bool m_focus_lock;
};

/* the widget a ScrollViewBase's children sit in: it moves it, and scales it where it is
   transformed */
class ScrollViewParent : public Widget {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	ScrollViewParent(Widget *parent, CreateParam *param);
	virtual ~ScrollViewParent();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual bool RemoveChild(Widget *child);
	virtual void RegisterChild(Widget *child, Widget *prev);
	virtual void UpdatePost();

	/* its offset and scale, applied to its matrix each update (name ours) */
	void SetTransform(const math::v4 &offset, const math::v4 &scale);
	/* its scale made 1 again where it was transformed (name ours) */
	void ResetTransform();

	math::v4 m_transform_offset;
	math::v4 m_transform_scale;
	/* whether it is transformed, and whether its matrix was (names ours) */
	bool m_transform;
	bool m_transformed;
};

/* what a ScrollViewBase's view parent is moved and scaled by while it is pulled past its ends
   (names ours: no record or string names them) */
class ScrollViewOverScroll {
public:
	/* the view is set by ScrollViewBase::SetOverScroll */
	ScrollViewOverScroll()
	{
	}

	virtual ~ScrollViewOverScroll()
	{
	}

	/* the view parent transformed for where the view is */
	virtual void Update()
	{
	}

	ScrollViewBase *m_view;
};

/* the default: the view parent stretched along the axis it is pulled past its end on, and moved
   to keep the end in place */
class ScrollViewOverScrollStretch : public ScrollViewOverScroll {
public:
	virtual ~ScrollViewOverScrollStretch()
	{
	}

	virtual void Update()
	{
		math::v4 offset;
		math::v4 scale;

		GetTransform(offset, scale);
		m_view->m_view_parent->SetTransform(offset, scale);
	}

	/* the offset and scale for where the view is (ScePafWidget DB6AF8D4) */
	void GetTransform(math::v4 &offset, math::v4 &scale);
};

/* what shows a ScrollViewBase's view box and scroll range: a widget over its view parent with
   four markers (names ours) */
class ScrollViewDebug {
public:
	enum {
		/* a marker moved to another cell (name ours) */
		CB_MARKER_MOVED = 0x20001001
	};

	class Marker {
	public:
		Widget *m_widget;
		int32_t m_index;
		/* the cell of the view's size it shows (names ours) */
		int32_t m_cell_x;
		int32_t m_cell_y;
		math::v2 m_pos;
	};

	ScrollViewDebug(ScrollViewBase *view);
	~ScrollViewDebug();

	void SetMode(int32_t mode);
	/* the markers moved to the cells around the view where it moved or changed its size
	   (ScePafWidget B35DEA06; name ours) */
	void Update();

	ScrollViewBase *m_view;
	int32_t m_mode;
	math::v2 m_box_pos;
	math::v2 m_box_size;
	Widget *m_root;
	Marker m_marker[4];
};

class ScrollViewBaseParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const TextureRef c_textureList[];
	static const char *ParamName();

	ScrollViewBaseParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	float m_snap_power;
	float m_snap_time;
	bool m_scroll_lock;
	float m_bar_fade_time;
	int32_t m_move_dir;
	/* the scroll bars' IDs */
	IDParam m_vertical_bar;
	IDParam m_horizontal_bar;
};

class ScrollViewBaseStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollViewBaseStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	graph::PlaneObjParam m_bg_obj;
	math::v4 m_vertical_bar_margin;
	math::v4 m_horizontal_bar_margin;
};

class ScrollViewBaseSoundParam : public WidgetSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollViewBaseSoundParam();
	virtual const char *GetParamName() const;
};

class ScrollView : public ScrollViewBase {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	ScrollView(Widget *parent, CreateParam *param);
	virtual ~ScrollView();

	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void OnViewLayoutUpdated(bool is_dirty);
	virtual void OnChildRegistered(Widget *child);
	virtual void OnChildRemoved(Widget *child);

	/* the slots after ScrollViewBase's, from 0x26c (index 155) */
	virtual void UpdateViewBox();
	virtual void UpdateAutoSnapPoint();
	virtual bool IsIgnored(Widget *child);

	Widget *GetViewParent();
	/* its children deleted */
	void Clear();
	void DelSnapChild(Widget *child);
	void AddSnapChild(Widget *child);
	void SetAutoSnap(bool auto_snap);
	int32_t GetSnapID(Widget *child);
	/* the view box fitted to its children, or not (name ours) */
	void SetAutoBound(bool auto_bound);
	void AddIgnoredChild(Widget *child);
	void DelIgnoredChild(Widget *child);
	/* the smallest and the largest view box, and the box it keeps in view (names ours) */
	void SetMinViewSize(const math::v2 &size);
	void SetMaxViewSize(const math::v2 &size);
	void SetManualViewBox(const math::v4 &box);
	/* ScrollViewBase's padding (ScePafWidget BDEF4D3F; name ours) */
	void SetViewPadding(const math::v4 &padding);

	/* its children's snap points, by child */
	map<Widget *, int32_t> m_snap_map;
	/* the children its view box leaves out */
	vector<Widget *> m_ignore_list;
	bool m_is_auto_snap;
	bool m_is_auto_bound;
	math::v2 m_min_view_size;
	math::v2 m_max_view_size;
	math::v4 m_manual_view_box;
};

class ScrollViewParam : public ScrollViewBaseParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollViewParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_auto_snap;
};

class ScrollViewStyleParam : public ScrollViewBaseStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollViewStyleParam()
		: m_min_view_size(0.0f, 0.0f), m_max_view_size(3.4028236e18f, 3.4028236e18f),
		  m_manual_view_box(0.0f, 0.0f, 0.0f, 0.0f), m_auto_bound(1)
	{
	}
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	math::v2 m_min_view_size;
	math::v2 m_max_view_size;
	math::v4 m_manual_view_box;
	int32_t m_auto_bound;
};

/* the focus taken off the widget's focus root's focus where it is not the environment's focus
   target (name ours) */
void ReleaseRootFocus(Widget *widget);

}
}

#endif
