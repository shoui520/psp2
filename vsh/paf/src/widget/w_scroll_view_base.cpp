/*
 * paf::ui::ScrollViewBase (DEBUGGER 0x81116228..0x81122cec): the base of the widgets that scroll
 * their contents, with its view parent, its timers, its scroll bar controller, its parameters,
 * and the suppliers that hand the analog sticks' and the touch panel's input to every scroll
 * view. No record names the file; the suppliers' names are their strings'
 * ("AnalogScrollSupplier::", "AnalogScrollListener::").
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <inputdevice/inputlistener.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/memory>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>
#include <timer/timer.h>
#include <timer/timerqueue.h>
#include <widget/core/animlistener.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/core/layouter.h>
#include <widget/core/uiinputlistener.h>
#include <widget/core/widget_gesture.h>
#include <widget/factory.h>
#include <widget/w_camera.h>
#include <widget/w_scene.h>
#include <widget/w_scroll_bar.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_text_box.h>
#include <widget/text_box_ime_listener.h>

namespace paf {
namespace ui {

/* the analog sticks' listener of the scroll views: it finds the view under the focus and moves
   it (names ours, after its string "AnalogScrollListener::") */
class AnalogScrollListener : public UIInputGestureListener {
public:
	AnalogScrollListener(Environment *env)
		: UIInputGestureListener(env), m_listener(this), m_dir(ScrollViewBase::MoveDir_None),
		  m_moved(false)
	{
		m_type = inputdevice::DEVICE_TYPE_ANALOG;
	}

	virtual ~AnalogScrollListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);

	/* itself (no reader found) */
	UIInputGestureListener *m_listener;
	/* the view it moves, and the axis it moves it along */
	ScrollViewBase *m_view;
	uint32_t m_dir;
	/* set while it moves the view */
	bool m_moved;
};

/* the scroll views the analog sticks move, and their listener, made with the first view */
class AnalogScrollSupplier {
public:
	AnalogScrollSupplier()
	{
	}

	~AnalogScrollSupplier()
	{
	}

	void Register(ScrollViewBase *view)
	{
		string func(__func__);
		string scope = "AnalogScrollSupplier::" + func;

		if (m_views.size() == 0) {
			m_listener = common::SharedPtr<inputdevice::InputListener>(
				new AnalogScrollListener(view->m_env));
			inputdevice::AddInputListener(m_listener);
		}
		m_views.push_back(view);
	}

	void Unregister(ScrollViewBase *view)
	{
		string func(__func__);
		string scope = "AnalogScrollSupplier::" + func;
		vector<ScrollViewBase *>::iterator it;

		for (it = m_views.begin(); it != m_views.end(); ++it)
			if (*it == view)
				break;
		if (it != m_views.end())
			m_views.erase(it);
		if (m_views.size() == 0)
			inputdevice::DelInputListener(m_listener);
	}

	vector<ScrollViewBase *> m_views;
	common::SharedPtr<inputdevice::InputListener> m_listener;
};

/* the front touch port's listener of the scroll views: each update their view parents are put
   back in place (name ours) */
class TouchScrollListener : public UITouchListener {
public:
	TouchScrollListener(Environment *env) : UITouchListener(env, 0)
	{
	}

	virtual ~TouchScrollListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);
};

/* the touch panel's sizes and the speeds the scroll views tell gestures by, from the panels'
   resolution, made once (names ours) */
class TouchScrollConfig {
public:
	TouchScrollConfig()
	{
		inputdevice::touchscreen::GetDeviceInfo(0, &m_front);
		inputdevice::touchscreen::GetDeviceInfo(1, &m_back);
		m_drag_distance = m_front.pixel_per_mm * 1.2727273f;
		m_hold_distance = m_front.pixel_per_mm * 1.2f;
		m_mochi_speed = m_front.pixel_per_mm * 727.2727f;
		m_flick_speed = m_front.pixel_per_mm * 909.0909f;
		m_axis_speed = m_front.pixel_per_mm * 727.2727f;
	}

	static TouchScrollConfig &Get()
	{
		static TouchScrollConfig s_config;

		return s_config;
	}

	inputdevice::touchscreen::DeviceInfo m_front;
	inputdevice::touchscreen::DeviceInfo m_back;
	ScrollViewScrollParam m_param;
	/* how far a touch moves along an axis before it drags along it */
	float m_drag_distance;
	float m_hold_distance;
	/* the least (squared) speed a mochi gesture keeps its velocity at */
	float m_mochi_speed;
	/* the least (squared) speed a release flicks the views at */
	float m_flick_speed;
	/* the speed under which a view dragged along one axis moves along the axes it may */
	float m_axis_speed;
};

/* the velocity along the axes (ScrollViewBase::MoveDir), 0 along the others */
static inline math::v2 MaskMoveDir(const math::v2 &v, int32_t dir)
{
	switch (dir) {
	case ScrollViewBase::MoveDir_X:
		return math::v2(v.extract_x(), 0.0f);
	case ScrollViewBase::MoveDir_Y:
		return math::v2(0.0f, v.extract_y());
	case ScrollViewBase::MoveDir_XY:
		return v;
	default:
		return math::v2(0.0f, 0.0f);
	}
}

/* -1, 0 or 1 */
/* whether the two are equal, compared lane by lane */
static inline bool IsSame(const math::v2 &a, const math::v2 &b)
{
	return ((a.getImpl().getX() == b.getImpl().getX()) &
		(a.getImpl().getY() == b.getImpl().getY()))
		.getAsBool();
}

static inline float Sign(float x)
{
	if (x == 0.0f)
		return 0.0f;
	return x < 0.0f ? -1.0f : 1.0f;
}

/* a move of a mochi gesture: how far, over how long (us), and whether it was the last before
   the touch stopped (names ours) */
class MochiPoint {
public:
	enum {
		FLAG_LAST = 1
	};

	MochiPoint(const math::v2 &move, uint64_t time, bool last)
		: m_move(move), m_time(time), m_flags(last ? FLAG_LAST : 0)
	{
	}

	math::v2 m_move;
	uint64_t m_time;
	/* FLAG_LAST: it was the last before the touch stopped */
	uint32_t m_flags;
};

/* the moves a mochi gesture went through, where it started and where it is (names ours) */
class MochiHistory {
public:
	/* how fast it moves: the moves of the last 134 ms each way it kept, weighted (name ours) */
	__attribute__((noinline)) math::v2 GetVelocity()
	{
		using sce::Vectormath::Simd::Aos::Vector2;
		using sce::Vectormath::Simd::Aos::divPerElem;
		using sce::Vectormath::Simd::Aos::maxPerElem;
		using sce::Vectormath::Simd::floatInVec;

		if (m_points.size() == 0)
			return math::v2(0.0f, 0.0f);

		math::v2 last(0.0f, 0.0f);
		MochiPoint &back = m_points.back();
		if ((back.m_flags & MochiPoint::FLAG_LAST) == 0)
			last = back.m_move * 1000000.0f / (float)back.m_time;

		math::v2 velocity = last;
		if (m_points.size() >= 2) {
			MochiPoint &prev = m_points.at(m_points.size() - 2);
			math::v2 before(0.0f, 0.0f);
			if ((prev.m_flags & MochiPoint::FLAG_LAST) == 0)
				before = prev.m_move * 1000000.0f / (float)prev.m_time;
			velocity = last + before;
		}

		if (lengthSqr(velocity.getImpl()) < TouchScrollConfig::Get().m_mochi_speed)
			return math::v2(0.0f, 0.0f);

		const math::v2 one(1.0f, 1.0f);
		float sign_x = 0.0f;
		float sign_y = 0.0f;
		for (int32_t i = m_points.size() - 1; i >= 0; i--) {
			math::v2 move = m_points.at(i).m_move;

			if (sign_x == 0.0f)
				sign_x = Sign(move.extract_x());
			if (sign_y == 0.0f)
				sign_y = Sign(move.extract_y());
			if (sign_x != 0.0f && sign_y != 0.0f)
				break;
		}

		math::v2 sum(0.0f, 0.0f);
		math::v2 weight(0.0f, 0.0f);
		math::v2 tail(0.0f, 0.0f);
		uint64_t time = 0;
		for (int32_t i = m_points.size() - 1; i >= 0; i--) {
			MochiPoint &point = m_points.at(i);
			math::v2 move = point.m_move;
			math::v2 w = one;
			math::v2 v;

			if (move.getImpl().getX() == floatInVec(0.0f) &&
			    move.getImpl().getY() == floatInVec(0.0f)) {
				v = math::v2(0.0f, 0.0f);
				if (sign_x == 0.0f)
					w.set_x(0.0f);
				if (sign_y == 0.0f)
					w.set_y(0.0f);
			} else {
				float dx = Sign(move.extract(0));
				float dy = Sign(move.extract(1));

				if (sign_x == 0.0f) {
					w.set_x(0.0f);
				} else if (sign_x != dx && dx != 0.0f) {
					w.set_x(0.0f);
					sign_x = 0.0f;
				}
				if (sign_y == 0.0f) {
					w.set_y(0.0f);
				} else if (sign_y != dy && dy != 0.0f) {
					w.set_y(0.0f);
					sign_y = 0.0f;
				}
				v = move * 1000000.0f / (float)point.m_time * w;
				if ((point.m_flags & MochiPoint::FLAG_LAST) != 0)
					tail = w * v;
			}
			sum = sum + v;
			weight = weight + w;
			time += point.m_time;
			if (time >= 134000)
				break;
		}

		return math::v2(divPerElem(sum.getImpl(), maxPerElem(one.getImpl(),
								      weight.getImpl()))) +
		       tail;
	}

	vector<MochiPoint> m_points;
	/* the frame the last move was taken in */
	uint64_t m_time;
	/* the frame the touch began in (name ours) */
	uint64_t m_start_time;
};

/* the gesture listener of the touch a view tracks: it takes the touch over once the touch
   moves along the axes fast enough (names ours) */
class ScrollGestureListener : public TouchGestureListener {
public:
	ScrollGestureListener(uint32_t touch_id, ScrollViewBase *view)
		: TouchGestureListener(0, touch_id, view), m_axis(1.0f, 1.0f)
	{
	}

	virtual int32_t OnUpdateTouch(uint32_t port_id)
	{
		const inputdevice::touchscreen::Data *data;

		inputdevice::touchscreen::GetData(m_port, m_touch_id, &data);
		if (data->state != inputdevice::touchscreen::Data::STATE_RELEASE)
			/* not released yet */
			return 1;

		float vx;
		float vy;
		inputdevice::touchscreen::DeviceInfo info;

		ScePafMisc_88F4A40B(m_port, m_touch_id, 50, &vx, &vy);
		inputdevice::touchscreen::GetDeviceInfo(m_port, &info);

		math::v2 speed(sce::Vectormath::Simd::Aos::absPerElem(
			(m_axis * math::v2(vx, vy) / info.pixel_per_mm).getImpl()));

		if (speed.extract_x() > 0.012f || speed.extract_y() > 0.012f) {
			ScePafWidget_C5939A92(this, 3, m_touch_id, 0, vx * 1000.0f, vy * 1000.0f, 1, 0);
			return UPDATE_DETACH_SELF | UPDATE_DETACH_LOWER;
		}
		return UPDATE_DETACH_SELF;
	}

	/* the axes the touch's velocity counts along */
	math::v2 m_axis;
};

/* what the scroll views share of the touch panel: the views the touches hold, the snap points'
   IDs, the views it updates and their listener (names ours) */
class TouchScrollSupplier {
public:
	TouchScrollSupplier() : m_next_snap_id(0), m_touch_state(0)
	{
		m_reset = false;
		m_time = get_time_internal();
	}

	~TouchScrollSupplier()
	{
	}

	void Register(ScrollViewBase *view)
	{
		if (m_views.size() == 0) {
			m_listener = common::SharedPtr<inputdevice::InputListener>(
				new TouchScrollListener(view->m_env));
			inputdevice::AddInputListener(m_listener);
		}
		m_views.push_back(view);
	}

	void Unregister(ScrollViewBase *view)
	{
		vector<ScrollViewBase *>::iterator it;

		for (it = m_views.begin(); it != m_views.end(); ++it)
			if (*it == view)
				break;
		if (it != m_views.end())
			m_views.erase(it);
		if (m_views.size() == 0) {
			inputdevice::DelInputListener(m_listener);
			m_listener.reset();
		}
	}

	/* the touch the view holds let go of: the flick stopped where it is the first (name
	   ours) */
	void Release(ScrollViewBase *view);
	/* the touch's position kept where the view holds it last, and the time without a move
	   taken into the mochi gesture's moves (name ours) */
	void UpdateTouch(ScrollViewBase *view);
	/* the gesture listener taken off the touch (name ours) */
	void DetachGesture();
	/* a gesture listener attached to the touch the first view tracks, counting its velocity
	   along the axes (MoveDir; name ours) */
	void AttachGesture(uint32_t dir);
	/* a widget after the last view, on its touch, takes gesture 5 (name ours) */
	bool IsGestureTaken();
	/* every view scrolls by analog and has no touch scroll bar (name ours) */
	bool IsAnalogOnly();
	/* the axes the views move along, without those they may not where masked is set (MoveDir;
	   name ours) */
	__attribute__((noinline)) uint32_t GetMovableDir(bool masked);
	/* the last view's touch is on one of its touch scroll bars (name ours) */
	bool IsOnScrollBar();
	/* each view's parent put back in place, and its matrices made again where they changed */
	void Update();
	/* the flick under way stopped, the views' velocities cleared (name ours) */
	void StopFlick();

	/* a view a touch holds, and the touch (names ours) */
	class TouchView {
	public:
		TouchView() : m_view(NULL), m_id(-1)
		{
		}

		TouchView(ScrollViewBase *view) : m_view(view), m_id(-1)
		{
		}

		ScrollViewBase *m_view;
		int32_t m_id;
	};

	/* the touch begun on the views: where it is, their snap points, and the velocities they
	   had as its first move (name ours) */
	void BeginTouch();
	/* the view and the scroll views it is in, outermost first (name ours) */
	void GetTouchViews(vector<TouchView> &views, ScrollViewBase *view);
	/* the touch taken by the view where it is the topmost scroll view on it: it captures the
	   touch where it moves already or a flick goes on, else a gesture listener waits for the
	   touch to move (name ours) */
	bool TakeTouch(ScrollViewBase *view, uint32_t touch_id);
	/* the touch begun dragging the views by the move: along the axes they move, the one it
	   moved most along where they move along both, or both once it moved far enough (name
	   ours) */
	void BeginDrag(const math::v2 &move, bool masked);
	/* the touch moved further than the drag distance along an axis a view moves along (name
	   ours) */
	bool IsDragged();
	/* the views dragged by the touch's move, the drag begun once the touch moved far enough,
	   and the part they moved taken into the mochi gesture's moves (name ours) */
	void Drag(const math::v2 &move);
	/* the views dragged where the touch moved since the frame's drag (name ours) */
	void UpdateDrag();
	/* the touch captured by the first view where the view is the last and no widget
	   captures it yet (name ours) */
	bool CaptureTouch(ScrollViewBase *view);
	/* the touch captured by the first view and the views dragged, once it moved far enough
	   (name ours) */
	bool GrabTouch(ScrollViewBase *view);
	/* the mochi gesture begun along the axes, at the velocity and its scale: the views'
	   touches let go, the scroll bars of the axes their jet tails shown (name ours) */
	void BeginMochi(uint32_t dir, const math::v2 &velocity, float scale);

	/* the views' flicks stopped (name ours) */
	void StopViews()
	{
		for (vector<TouchView>::iterator it = m_touch_views.begin();
		     it != m_touch_views.end(); ++it) {
			ScrollViewBase *view = it->m_view;
			ScrollViewTimer *timer = view->m_focus_timer;

			if (timer == NULL || timer->m_type == ScrollViewTimer::TIMER_TYPE_MOTION)
				view->m_view_parent->KillAnim(0x10000);
			view->m_velocity = math::v2(0.0f, 0.0f);
		}
	}

	/* the views the touches hold */
	vector<TouchView> m_touch_views;
	/* the ID the next snap point takes */
	int32_t m_next_snap_id;
	uint8_t reserved0[4];
	/* the moves of the mochi gesture */
	MochiHistory m_mochi;
	vector<ScrollViewBase *> m_views;
	common::SharedPtr<inputdevice::InputListener> m_listener;
	/* where the touch began on the view's plane (name ours) */
	math::v2 m_touch_start;
	/* where the touch is on the view's plane */
	math::v2 m_touch_pos;
	bool m_dragging;
	bool m_drag_locked;
	bool m_reset;
	/* its gesture listener is attached to the touch (name ours) */
	bool m_gesture_attached;
	/* what the touch does (2 drags, 3 moves no view) */
	int32_t m_touch_state;
	/* the mochi gesture's velocity and its scale (name ours) */
	math::v2 m_mochi_velocity;
	float m_mochi_scale;
	uint8_t reserved2[4];
	uint64_t m_time;
	/* the frame the touch last dragged the views in, 0 from its begin (name ours) */
	uint64_t m_drag_time;
	/* the axes the touch drags along (ScrollViewBase::MoveDir) */
	uint32_t m_drag_dir;
};

static AnalogScrollSupplier s_analog_scroll_supplier;
static TouchScrollSupplier s_touch_scroll_supplier;

/* the view parent's layouter: the view laid out again with it (names ours) */
class ScrollViewParentLayouter : public Layouter {
public:
	ScrollViewParentLayouter(ScrollViewBase *view) : Layouter(NULL, NULL), m_view(view)
	{
	}

	virtual ~ScrollViewParentLayouter()
	{
	}

	virtual int32_t OnUpdate_core()
	{
		m_view->OnViewLayoutUpdated(m_dirty != 0);
		return Layouter::OnUpdate_core();
	}

	ScrollViewBase *m_view;
};

/* the view's own layouter: its parts laid out again with it (names ours) */
class ScrollViewBaseLayouter : public Layouter {
public:
	ScrollViewBaseLayouter(ScrollViewBase *view) : Layouter(NULL, NULL), m_view(view)
	{
	}

	virtual ~ScrollViewBaseLayouter()
	{
	}

	virtual int32_t OnUpdate_core()
	{
		m_view->OnPartsLayoutUpdated(m_dirty != 0);
		return Layouter::OnUpdate_core();
	}

	ScrollViewBase *m_view;
};

/* the view's focus moves only to a widget outside its view parent, and only while it moves */
static int32_t ScrollViewBaseFocusCB(Focuser *focuser, Widget *widget, void *data, void *move);

ScrollViewScrollParam::ScrollViewScrollParam()
{
	m_value[0] = 20.0f;
	m_value[1] = 15.0f;
	m_value[2] = 50.0f;
	m_value[3] = 1200.0f;
	m_value[4] = 800.0f;
	m_value[5] = 2.3f;
	m_value[8] = 50.0f;
	m_value[6] = 230.0f;
	m_value[9] = 50.0f;
	m_value[7] = 500.0f;
	m_value[12] = 15.0f;
	m_value[10] = 80.0f;
	m_value[13] = 50.0f;
	m_value[11] = 30.0f;
	m_value[14] = 1200.0f;
	m_value[15] = 600.0f;
	m_value[16] = 8.0f;
	m_value[17] = 230.0f;
	m_value[18] = 500.0f;
	m_value[19] = 40.0f;
	m_value[20] = 40.0f;
	m_value[21] = 80.0f;
	m_value[22] = 20.0f;
	m_bounce = true;
}

uint32_t ScrollViewBase::GetMovableDirection(bool masked)
{
	if (masked)
		return m_movable_dir & ~m_movable_dir_off;
	return m_movable_dir;
}

void ScrollViewBase::SetOverScroll(ScrollViewOverScroll *over_scroll)
{
	delete m_over_scroll;
	m_over_scroll = over_scroll;
	if (over_scroll != NULL)
		over_scroll->m_view = this;
	m_over_scroll_length = 20.0f;
}

void ScrollViewBase::EnableOverScroll(bool on)
{
	if (on) {
		SetOverScroll(new ScrollViewOverScrollStretch);
		m_over_scroll_length = 16.0f;
	} else {
		SetOverScroll(NULL);
		m_over_scroll_length = 20.0f;
	}
}

void ScrollViewBase::SetScrollBarController(ScrollViewScrollBarController *controller)
{
	delete m_bar_controller;
	m_bar_controller = controller;
	UpdateScrollLayout();
	UpdateScrollRange();
}

ScrollBar *ScrollViewBase::CreateScrollBar(int32_t type)
{
	math::v4 size(0.0f, 0.0f, 0.0f, 0.0f);
	const char *style = NULL;
	ScrollBar *bar = NULL;

	if (type == ScrollBar::SCROLL_TYPE_HORIZONTAL)
		style = "_common_style_scrollbar_horizontal";
	{
		Environment *env = m_env;
		IDParam id("");
		IDParam style_id(style);

		if (env->m_create_widget_callback != NULL)
			bar = static_cast<ScrollBar *>(env->m_create_widget_callback(
				this, "scrollbar", id, style_id, env->m_create_widget_callback_data));
	}
	if (bar != NULL) {
		graph::DrawObj *obj = bar->GetDrawObj(ScrollBar::OBJ_BASE);

		if (obj != NULL) {
			intrusive_ptr<graph::Surface> surf;

			obj->GetTexture(surf, 0);
			if (surf.get() != NULL) {
				if (type != ScrollBar::SCROLL_TYPE_VERTICAL)
					size.set_y(surf->GetHeight());
				else
					size.set_x(surf->GetWidth());
			}
		}
	} else {
		CreateParam param;

		param.m_parent_protected = true;
		bar = new ScrollBar(this, &param);
		bar->SetObjColor(math::v4(0.0f, 0.0f, 0.0f, 0.5f), ScrollBar::OBJ_SLIDER);
		bar->SetObjColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f), ScrollBar::OBJ_BASE);
		bar->SetSliderMinSize(14.0f);
	}
	if (size == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
		if (type != ScrollBar::SCROLL_TYPE_VERTICAL)
			size.set_y(8.0f);
		else
			size.set_x(8.0f);
	}
	bar->SetSize(size);
	bar->SetScrollType(static_cast<ScrollBar::ScrollType>(type));
	return bar;
}

void ScrollViewBase::SetDefaultScrollBar()
{
	SetVerticalScrollBar(CreateScrollBar(ScrollBar::SCROLL_TYPE_VERTICAL));
	SetHorizontalScrollBar(CreateScrollBar(ScrollBar::SCROLL_TYPE_HORIZONTAL));
}

ScrollViewParent::ScrollViewParent(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_transform_offset(0.0f, 0.0f, 0.0f, 0.0f),
	  m_transform_scale(1.0f, 1.0f, 1.0f, 0.0f), m_transform(false), m_transformed(false)
{
}

ScrollViewBase::ScrollViewBase(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_track_port(-1), m_track_state(0), m_velocity(0.0f, 0.0f),
	  m_bar_controller(NULL), m_vertical_bar(NULL), m_horizontal_bar(NULL),
	  m_vertical_bar_margin(6.0f, 6.0f, 8.0f, 0.0f),
	  m_horizontal_bar_margin(6.0f, 6.0f, 8.0f, 0.0f), m_view_box(0.0f, 0.0f, 0.0f, 0.0f),
	  m_view_box_size(0.0f, 0.0f), m_scroll_box(0.0f, 0.0f, 0.0f, 0.0f),
	  m_scroll_box_size(0.0f, 0.0f), m_scroll_center(0.0f, 0.0f),
	  m_padding(0.0f, 0.0f, 0.0f, 0.0f), m_padding_used(0.0f, 0.0f, 0.0f, 0.0f),
	  m_padding_sign(0.0f, 0.0f, 0.0f, 0.0f), m_view_size(0.0f, 0.0f),
	  m_view_half(0.0f, 0.0f, 0.0f, 0.0f), m_view_limit(0.0f, 0.0f, 0.0f, 0.0f),
	  m_fit_dir(MoveDir_XY), m_movable_dir(MoveDir_None), m_movable_dir_y(MoveDir_None),
	  m_movable_dir_on(MoveDir_None), m_movable_dir_off(MoveDir_None), m_move_dir(0),
	  m_scroll_flags(0), m_world_rot_pre(m_world_rot), m_focus_pos(0.0f, 0.0f),
	  m_focus_target(0.0f, 0.0f), m_focus_start(0.0f, 0.0f), m_focus_timer(NULL),
	  m_block_count(0), m_debug(NULL), m_scroll_param(NULL), m_over_scroll(NULL),
	  m_anchor(NULL), m_enable_analog(true), m_limit_focus(true), m_snap_power(25.0f),
	  m_preferred_snap_id(-1), m_snap_time(200.0f), m_key_step(4.0f),
	  m_over_scroll_length(20.0f), m_bar_fade_time(200.0f), m_bar_reset(true),
	  m_scroll_key(true), m_scroll_touch(true), m_scroll_analog(true), m_scroll_lock(false),
	  m_ime_padding(false), m_ime_height(0.0f), m_scroll_ended(false), m_drag_dir(0),
	  m_touch_mask_on(0), m_focus_wrap(false), m_focus_lock(false)
{
	if (!(m_event_enable & 0x400))
		EnableFocusEvent(true);
	m_hit_find_mode = 0;
	m_event_enable |= 0xf4d0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_check = 0;
	m_clip_children = true;
	SetLayouter(new ScrollViewBaseLayouter(this));
	if (param == NULL || param->m_create_drawobj)
		AddDrawObj(DrawObjEntry(common::SharedPtr<graph::DrawObj>(
						new graph::PlaneObj(graph::PlaneObjStyle())),
					OBJ_BG),
			   false);

	CreateParam parent_param;

	parent_param.m_parent_protected = true;
	m_view_parent = new ScrollViewParent(this, &parent_param);
	m_view_parent->m_debug_events = false;
	m_view_parent->m_event_enable |= 0x40;
	m_view_parent->SetLayouter(new ScrollViewParentLayouter(this));
	m_view_parent->m_layout_notify_mask |= 0x300;
	SetDefaultScrollBar();
	SetScrollBarController(new ScrollViewScrollBarController);
	ScePafWidget_82A61BBC(this);
	s_analog_scroll_supplier.Register(this);
	s_touch_scroll_supplier.Register(this);
	EnableOverScroll(true);
	ScePafWidget_00AA6519(this);
	m_pre_dispatch = true;

	Focuser *focuser = new Focuser(ScrollViewBaseFocusCB);

	delete m_focuser;
	m_focuser = focuser;
	if (focuser != NULL)
		focuser->BindWidget(this);
}

ScrollViewBaseParam::ScrollViewBaseParam()
	: m_snap_power(25.0f), m_snap_time(200.0f), m_scroll_lock(false), m_bar_fade_time(200.0f),
	  m_move_dir(ScrollViewBase::MoveDir_None)
{
}

ScrollViewBaseStyleParam::ScrollViewBaseStyleParam()
	: m_vertical_bar_margin(6.0f, 6.0f, 8.0f, 0.0f), m_horizontal_bar_margin(6.0f, 6.0f, 8.0f, 0.0f)
{
	m_bg_obj.m_name = "bg_obj";
	m_bg_obj.m_index = ScrollViewBase::OBJ_BG;
	m_drawobj_param_list.push_back(&m_bg_obj);
}

/* the sounds its sound parameter names, by index */
static const struct {
	const char *name;
	uint32_t index;
} s_scroll_view_base_sounds[] = {
	{ "press", ScrollViewBase::SOUND_PRESS },
	{ "snapped", ScrollViewBase::SOUND_SNAP },
};

ScrollViewBaseSoundParam::ScrollViewBaseSoundParam()
{
	for (uint32_t i = 0; i < 2; i++)
		m_sound_list.push_back(Sound(s_scroll_view_base_sounds[i].name,
					     s_scroll_view_base_sounds[i].index));
}

const char ScrollViewParent::m_widget_type[] = "scroll_view_parent";

bool ScrollViewParent::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ScrollViewParent::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

const char *ScrollViewParent::TypeName()
{
	return m_widget_type;
}

void ScrollViewParent::SetTransform(const math::v4 &offset, const math::v4 &scale)
{
	m_transform_offset = offset;
	m_transform_scale = scale;
	/* only x, y and z count */
	if (math::v3(offset.getImpl().getXYZ()) == math::v3(0.0f) &&
	    math::v3(scale.getImpl().getXYZ()) == math::v3(1.0f))
		m_transform = false;
	else
		m_transform = true;
}

void ScrollViewParent::ResetTransform()
{
	if (m_transformed) {
		SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		SetUpdateParam(1);
		m_transformed = false;
	}
}

ScrollViewParent::~ScrollViewParent()
{
}

bool ScrollViewParent::RemoveChild(Widget *child)
{
	bool removed = Widget::RemoveChild(child);

	if (removed)
		static_cast<ScrollViewBase *>(m_parent)->OnChildRemoved(child);
	return removed;
}

void ScrollViewParent::RegisterChild(Widget *child, Widget *prev)
{
	Widget::RegisterChild(child, prev);
	static_cast<ScrollViewBase *>(m_parent)->OnChildRegistered(child);
}

void ScrollViewParent::UpdatePost()
{
	if (m_transform) {
		if (!(m_transform_scale == m_scale))
			SetScale(m_transform_scale);
		UpdateMatrix();

		math::v4 offset = m_transform_offset;

		offset.set_w(1.0f);
		m_local_matrix = math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
					      math::v4(0.0f, 1.0f, 0.0f, 0.0f),
					      math::v4(0.0f, 0.0f, 1.0f, 0.0f), offset) *
				 m_local_matrix;
		SetUpdateParam(2);
		m_transformed = true;
	}
}

ScrollViewTimer::ScrollViewTimer(ScrollViewBase *widget, TimerType type, float total, int32_t func,
				 int32_t repeat, int32_t rep_count, float *param, int32_t userdata)
	: Timer(total, func, repeat, rep_count, param), m_widget(widget), m_type(type),
	  m_keep_layout(true), m_userdata(-1)
{
}

ScrollViewTimer::ScrollViewTimer(ScrollViewBase *widget, TimerType type, const Timer &timer)
	: Timer(timer.GetTotalTime(), timer.m_func_x, timer.m_repeat, timer.m_repeat_count,
		timer.m_param_x),
	  m_widget(widget), m_type(type), m_keep_layout(true)
{
	m_total = timer.m_total;
	m_total_inv = timer.m_total_inv;
}

ScrollViewTimer::~ScrollViewTimer()
{
	if (m_widget != NULL)
		m_widget->OnDestroyTimer(this);
	m_widget = reinterpret_cast<ScrollViewBase *>(-1);
}

ScrollViewSnapPoint::ScrollViewSnapPoint(const math::v2 &pos) : m_pos(pos)
{
}

void ScrollViewScrollBarController::UpdateLayout(const ScrollViewBarLayout &layout)
{
	math::v2 view_size = layout.m_view_size;
	ScrollBar *h_bar = layout.m_horizontal_bar;
	math::v4 h_margin = layout.m_horizontal_margin;
	math::v4 v_margin = layout.m_vertical_margin;
	ScrollBar *v_bar = layout.m_vertical_bar;
	bool movable_x = layout.m_movable_x;
	bool movable_y = layout.m_movable_y;

	if (h_bar != NULL) {
		float v_width;

		/* the vertical bar takes its width off the horizontal one's end */
		if (v_bar != NULL && movable_y)
			v_width = v_bar->GetSize(0)->extract_x() + v_margin.extract_z();
		else
			v_width = 0.0f;

		float end = h_margin.extract_y() + v_width;
		float length = view_size.extract_x() - (h_margin.extract_x() + end);
		float width = 0.0f;

		if (length > 0.0f)
			width = length;

		float x = (h_margin.extract_x() - end) * 0.5f;
		math::v4 size = *h_bar->GetSize(0);

		size.set_x(width);
		h_bar->SetSize(size);
		h_bar->SetPos(math::v4(x,
				       size.extract_y() * 0.5f + view_size.extract_y() * -0.5f +
					       h_margin.extract_z(),
				       0.0f, 0.0f));
	}
	if (v_bar != NULL) {
		float h_height;

		/* the horizontal bar takes its height off the vertical one's end */
		if (h_bar != NULL && movable_x)
			h_height = h_bar->GetSize(0)->extract_y() + h_margin.extract_z();
		else
			h_height = 0.0f;

		float end = v_margin.extract_y() + h_height;
		float length = view_size.extract_y() - (end + v_margin.extract_x());
		float height = 0.0f;

		if (length > 0.0f)
			height = length;

		float y = (end - v_margin.extract_x()) * 0.5f;
		math::v4 size = *v_bar->GetSize(0);

		size.set_y(height);
		v_bar->SetSize(size);
		v_bar->SetPos(math::v4(view_size.extract_x() * 0.5f - size.extract_x() * 0.5f -
					       v_margin.extract_z(),
				       y, 0.0f, 0.0f));
	}
}

/* the widget as a ScrollBarTouch, NULL where it is none */
static inline ScrollBarTouch *ToScrollBarTouch(Widget *widget)
{
	if (widget != NULL && widget->IsInherit(ScrollBarTouch::TypeName()))
		return static_cast<ScrollBarTouch *>(widget);
	return NULL;
}

/* the widget hidden at once, its transition reversed */
static inline void HideNow(Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

void ScrollViewScrollBarController::UpdateRange(const ScrollViewBarRange &range)
{
	if (range.m_horizontal_bar != NULL) {
		if (range.m_reset)
			HideNow(range.m_horizontal_bar);

		ScrollBar *bar = range.m_horizontal_bar;
		float value = range.m_value.extract_x();
		float max = range.m_max.extract_x();
		float visible = range.m_view_size.extract_x();
		ScrollBar *touch = NULL;

		if (bar != NULL && bar->IsInherit(ScrollBarTouch::TypeName()))
			touch = bar;
		UpdateScrollBar(bar, value, max, visible,
				touch != NULL || range.m_no_fade ? 0.0f : range.m_fade_time);
	}
	if (range.m_vertical_bar != NULL) {
		if (range.m_reset)
			HideNow(range.m_vertical_bar);

		ScrollBar *bar = range.m_vertical_bar;
		float value = range.m_value.extract_y();
		float max = range.m_max.extract_y();
		float visible = range.m_view_size.extract_y();
		ScrollBar *touch = NULL;

		if (bar != NULL && bar->IsInherit(ScrollBarTouch::TypeName()))
			touch = bar;
		UpdateScrollBar(bar, value, max, visible,
				touch != NULL || range.m_no_fade ? 0.0f : range.m_fade_time);
	}
}

void ScrollViewScrollBarController::ShowJetTail(ScrollBar *bar, bool show)
{
	if (bar != NULL) {
		if (show)
			bar->SetJetTailMinLength(0.0f);
		bar->ShowJetTail(show);
	}
}

void ScrollViewScrollBarController::SetJetTailLength(ScrollBar *bar, float rate)
{
	if (bar != NULL) {
		graph::DrawObj *slider = bar->GetDrawObj(ScrollBar::OBJ_SLIDER);
		float length;

		if (slider != NULL) {
			int32_t axis = bar->m_scroll_type == ScrollBar::SCROLL_TYPE_HORIZONTAL ? 0 : 1;
			float max = slider->m_size.extract(axis) * 1.5f;

			length = max < 90.0f ? max : 90.0f;
		} else {
			length = 90.0f;
		}
		bar->SetJetTailMinLength(length * rate);
	}
}

void ScrollViewScrollBarController::UpdateScrollBar(ScrollBar *bar, float value, float max,
						    float visible, float time)
{
	if (max > visible) {
		bool shown = bar->m_show_a_sum >= 0.02f;

		if (!bar->m_transition_complete) {
			bar->m_transition = 0xff;
			bar->m_transition_complete = true;
			bar->DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(bar, NULL, 2, 0.0f, NULL);

		/* the bar fades the more it is turned away, to nothing at half a turn */
		if (bar->m_update_flag & 7)
			bar->UpdateMatrix();

		math::v3 rot = bar->m_world_rot.toMatrix(0.0001f).extractEulerAnglesYXZ();
		sce::Vectormath::Simd::Aos::Vector3 turn =
			sce::Vectormath::Simd::Aos::absPerElem(rot.getImpl());
		float angle = sce::Vectormath::Simd::fmaxf(turn.getX(),
							 sce::Vectormath::Simd::fmaxf(turn.getY(),
										    turn.getZ()));

		angle = angle < 0.0f ? 0.0f : angle;
		angle = angle < 3.14159274f ? angle : 3.14159274f;

		float alpha = 1.0f - angle / 3.14159274f;

		if (time == 0.0f) {
			bar->SetMetaAlpha(alpha);
		} else if (time > 0.0f && Timer::s_tick_unit >= time) {
			bar->SetMetaAlpha(0.0f);
		} else {
			if (time < 0.0f)
				time = 200.0f;

			/* it fades out no sooner than the fade under way ends */
			AnimQueue *anim = bar->FindAnim(Widget::ANIM_META_ALPHA);

			if (anim != NULL) {
				AnimQueueBase *queue = anim->m_queue[0];
				float rest = queue != NULL ? queue->GetRestTime() : 0.0f;

				time = rest < time ? time : rest;
			}
			if (bar->m_max_value != max || bar->m_visible_amount != visible ||
			    bar->GetValue() != value || !shown)
				bar->SetMetaAlpha(alpha);
			if (bar->FindAnim(Widget::ANIM_META_ALPHA) == NULL && bar->m_meta_alpha != 0.0f)
				bar->SetMetaAlpha(0.0f, new Timer(time, Timer::FUNC_QUAD_IN),
						  Widget::ANIM_META_ALPHA, 2);
		}
		bar->SetMaxValue(max);
		bar->SetVisibleAmount(visible);
		bar->SetValue(value, false);
	} else {
		HideNow(bar);
	}
}

const char ScrollViewBase::m_widget_type[] = "scroll_view_base";

bool ScrollViewBase::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ScrollViewBase::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

const char *ScrollViewBase::TypeName()
{
	return m_widget_type;
}

void ScrollViewBase::ResetScrollBar(ScrollBar *&scroll_bar, ScrollBar *next_bar)
{
	if (scroll_bar != NULL) {
		if (ToScrollBarTouch(scroll_bar) != NULL)
			scroll_bar->DeleteEventCallback(ScrollBarTouch::CB_SCROLLBAR_TOUCH_CHANGED,
							CBScrollBarChanged, this);
		scroll_bar->DestroyWidget();
	}
	scroll_bar = next_bar;
	if (next_bar != NULL) {
		if (next_bar->m_parent != this)
			next_bar->SetParent(this, NULL, 0);
		/* to the end of its siblings */
		scroll_bar->SetParent(scroll_bar->m_parent, NULL, 6);

		if (ToScrollBarTouch(scroll_bar) != NULL)
			scroll_bar->AddEventListener(ScrollBarTouch::CB_SCROLLBAR_TOUCH_CHANGED,
						     new EventCBListener(CBScrollBarChanged, this),
						     false);
	}
	UpdateScrollLayout();
	UpdateScrollRange();
}

void ScrollViewBase::SetVerticalScrollBar(ScrollBar *scroll_bar)
{
	ResetScrollBar(m_vertical_bar, scroll_bar);
}

void ScrollViewBase::SetHorizontalScrollBar(ScrollBar *scroll_bar)
{
	ResetScrollBar(m_horizontal_bar, scroll_bar);
}

void ScrollViewBase::SetVerticalBarMargin(const math::v4 &margin)
{
	if (!(margin == m_vertical_bar_margin)) {
		m_vertical_bar_margin = margin;
		UpdateScrollLayout();
	}
}

void ScrollViewBase::SetHorizontalBarMargin(const math::v4 &margin)
{
	if (!(margin == m_horizontal_bar_margin)) {
		m_horizontal_bar_margin = margin;
		UpdateScrollLayout();
	}
}

void ScrollViewBase::SetBarFadeTime(float time)
{
	if (time < 0.0f)
		time = 200.0f;
	if (m_bar_fade_time != time) {
		m_bar_fade_time = time;
		UpdateScrollRange();
	}
}

void ScrollViewBase::ResetBarShown()
{
	m_bar_reset = true;
	UpdateScrollRange();
}

ScrollViewBase::Snap::Snap(const ScrollViewSnapPoint &snap_point) : ScrollViewSnapPoint(snap_point)
{
	m_id = s_touch_scroll_supplier.m_next_snap_id++;
}

int32_t ScrollViewBase::AddSnapPoint(const ScrollViewSnapPoint &snap_point)
{
	m_snap_list.push_back(Snap(snap_point));
	ScePafWidget_A0538225(-1);
	return m_snap_list.back().m_id;
}

list<ScrollViewBase::Snap>::iterator ScrollViewBase::FindSnapPoint(int32_t snap_id)
{
	if (snap_id != -1) {
		for (list<Snap>::iterator it = m_snap_list.begin(); it != m_snap_list.end(); ++it) {
			if (it->m_id == snap_id)
				return it;
		}
	}
	return m_snap_list.end();
}

void ScrollViewBase::DelSnapPoint(int32_t snap_id)
{
	list<Snap>::iterator found = FindSnapPoint(snap_id);

	for (list<Snap>::iterator it = m_snap_list.begin(); it != m_snap_list.end(); ++it) {
		if (it->m_id == snap_id) {
			if (snap_id == m_preferred_snap_id)
				m_preferred_snap_id = -1;
			m_snap_list.erase(it);
			break;
		}
	}
	ScePafWidget_A0538225(-1);
}

ScrollViewSnapPoint ScrollViewBase::GetSnapPoint(int32_t snap_id)
{
	list<Snap>::iterator it = FindSnapPoint(snap_id);

	if (it == m_snap_list.end())
		return ScrollViewSnapPoint(math::v2(0.0f, 0.0f));
	return *it;
}

int32_t ScrollViewBase::FindSnapID(const ScrollViewSnapPoint &snap_point)
{
	for (list<Snap>::iterator it = m_snap_list.begin(); it != m_snap_list.end(); ++it) {
		if (it->m_pos == snap_point.m_pos)
			return it->m_id;
	}
	return -1;
}

void ScrollViewBase::UpdateSnapPoint(int32_t snap_id, const ScrollViewSnapPoint &snap_point)
{
	list<Snap>::iterator it = FindSnapPoint(snap_id);

	if (it != m_snap_list.end()) {
		it->m_pos = snap_point.m_pos;
		ScePafWidget_A0538225(-1);
	}
}

void ScrollViewBase::SetSnapPower(float snap_power)
{
	float power = 0.0f;

	if (snap_power > 0.0f)
		power = snap_power;
	m_snap_power = power;
}

float ScrollViewBase::GetSnapPower()
{
	return m_snap_power;
}

void ScrollViewBase::SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3)
{
	Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, v0, v1, v2, v3);

	DoEvent(type, &e);
}

void ScrollViewBase::DoSnap(int32_t snap_id, Timer *timer)
{
	list<Snap>::iterator it = FindSnapPoint(snap_id);

	if (it != m_snap_list.end()) {
		SetViewFocus(it->m_pos, timer, true, true);
		SendEvent(CB_SCROLL_VIEW_BASE_SNAPPED, it->m_id, 0, 0, 0);
		ScePafWidget_2B1A1493(this, SOUND_SNAP);
	}
}

void ScrollViewBase::SetPreferredSnapPoint(int32_t snap_id, float time, int32_t func)
{
	list<Snap>::iterator it = FindSnapPoint(snap_id);
	int32_t preferred = -1;

	if (it != m_snap_list.end()) {
		m_preferred_snap_time = time;
		preferred = snap_id;
		m_preferred_snap_func = func;
	}
	m_preferred_snap_id = preferred;
}

void TouchScrollSupplier::StopFlick()
{
	if (m_touch_views.size() == 0 || m_touch_state == 3)
		return;

	math::v2 view_velocity(0.0f, 0.0f);
	math::v2 velocity;

	m_touch_state = 3;
	if (m_drag_locked)
		velocity = math::v2(0.0f, 0.0f);
	else
		velocity = MaskMoveDir(m_mochi.GetVelocity(), m_drag_dir);

	math::v2 flick = m_dragging ? math::v2(0.0f, 0.0f) : velocity;

	if (m_drag_locked) {
		for (vector<TouchView>::iterator it = m_touch_views.begin();
		     it != m_touch_views.end(); ++it) {
			it->m_view->m_velocity = math::v2(0.0f, 0.0f);

			ScrollViewBase *view = it->m_view;
			if (view->m_bar_controller != NULL) {
				view->m_bar_controller->ShowJetTail(view->m_horizontal_bar, false);
				view->m_bar_controller->ShowJetTail(view->m_vertical_bar, false);
			}
		}
	}
	m_drag_locked = false;
	m_reset = false;

	float speed = lengthSqr(velocity.getImpl());
	if (speed < TouchScrollConfig::Get().m_flick_speed) {
		math::v2 dir(0.0f, 0.0f);

		for (int32_t i = m_touch_views.size() - 1; i >= 0; i--) {
			TouchView &touch = m_touch_views.at(i);
			touch.m_view->ScePafWidget_D9FE36A6(-1, dir, flick);
		}
	} else {
		math::v2 back = velocity * -1.0f;
		math::v2 size(sce::Vectormath::Simd::Aos::absPerElem(back.getImpl()));
		math::v2 dir;

		if (size.extract_y() > size.extract_x())
			dir = math::v2(0.0f, back.extract_y());
		else
			dir = math::v2(back.extract_x(), 0.0f);
		for (int32_t i = m_touch_views.size() - 1; i >= 0; i--) {
			TouchView &touch = m_touch_views.at(i);
			touch.m_view->ScePafWidget_D9FE36A6(touch.m_id, dir, flick);
		}
	}

	if (m_drag_dir != ScrollViewBase::MoveDir_None) {
		ScrollViewBase *view = m_touch_views[m_touch_views.size() - 1].m_view;

		view_velocity = velocity;
		if (m_dragging && view->m_drag_dir != ScrollViewBase::MoveDir_None) {
			float axis_speed = length(MaskMoveDir(velocity, view->m_drag_dir).getImpl());
			if (axis_speed < TouchScrollConfig::Get().m_axis_speed)
				view_velocity = MaskMoveDir(velocity, view->m_movable_dir);
		}
		view->m_velocity = view_velocity;
	}
	m_touch_views.resize(0);
	m_touch_state = 0;
}

void ScrollViewBase::SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b)
{
	if (flag_b)
		s_touch_scroll_supplier.StopFlick();
	m_velocity = math::v2(0.0f, 0.0f);
	if (timer != NULL) {
		m_focus_timer = new ScrollViewTimer(this, ScrollViewTimer::TIMER_TYPE_MOTION, *timer);
		m_focus_timer->m_keep_layout = flag_a;
		delete timer;
	} else {
		m_focus_timer = NULL;
	}
	MoveViewParent(pos * -1.0f, m_focus_timer);
}

math::v2 ScrollViewBase::GetViewFocus()
{
	math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	return math::v2(pos.getImpl().getXY()) * -1.0f;
}

math::v2 ScrollViewBase::GetViewFocusTarget()
{
	math::v4 pos = m_view_parent->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	return math::v2(pos.getImpl().getXY()) * -1.0f;
}

void ScrollViewBase::BlockUpdateViewPort()
{
	m_block_count++;
}

void ScrollViewBase::UnblockUpdateViewPort()
{
	if (--m_block_count == 0) {
		ScePafWidget_A0538225(-1);
		UpdateScrollRange();
	}
}

void ScrollViewBase::SetSnapTime(float time)
{
	if (time < 0.0f)
		time = 200.0f;
	m_snap_time = time;
}

void ScrollViewDebug::SetMode(int32_t mode)
{
	m_mode = mode;
}

ScrollViewDebug::ScrollViewDebug(ScrollViewBase *view)
	: m_view(view), m_mode(1), m_box_pos(0.0f, 0.0f), m_box_size(0.0f, 0.0f)
{
	Widget::CreateParam param;

	param.m_parent_protected = true;
	m_root = new Widget(m_view->m_view_parent, &param);
	/* to the front of its siblings */
	m_root->SetParent(m_root->m_parent, NULL, 5);
	for (int32_t i = 0; i < 4; i++) {
		m_marker[i].m_widget = new Widget(m_root, &param);
		m_marker[i].m_index = i;
		m_marker[i].m_pos = math::v2(0.0f, 0.0f);
	}
}

ScrollViewDebug::~ScrollViewDebug()
{
	m_root->DestroyWidget();
}

void ScrollViewBase::SetDebugMode(int32_t mode)
{
	ScrollViewDebug *debug = m_debug;

	if (mode != 0) {
		if (debug == NULL) {
			debug = new ScrollViewDebug(this);
			m_debug = debug;
		}
		debug->SetMode(mode);
	} else if (debug != NULL) {
		delete debug;
		m_debug = NULL;
	}
}

void ScrollViewBase::SetMovableDirOn(uint32_t dir)
{
	if (m_movable_dir_on != dir) {
		m_movable_dir_on = dir;
		UpdateMovableDirection();
		UpdateScrollLayout();
	}
}

void ScrollViewBase::SetMovableDirOff(uint32_t dir)
{
	m_movable_dir_off = dir;
}

void ScrollViewBase::SetMoveDirection(uint32_t dir)
{
	if (m_move_dir != dir) {
		m_move_dir = dir;
		UpdateMovableDirection();
		UpdateScrollLayout();
	}
}

void ScrollViewBase::SetScrollParam(const ScrollViewScrollParam *param)
{
	if (m_scroll_param != NULL)
		delete m_scroll_param;
	m_scroll_param = new ScrollViewScrollParam;
	if (m_scroll_param != NULL)
		*m_scroll_param = *param;
}

void ScrollViewBase::EnableAnchor(bool on)
{
	if (on) {
		if (m_anchor == NULL) {
			m_anchor = new math::v2;
			*m_anchor = math::v2(GetOriginWorldMatrix().getImpl().getCol3().getXY());
		}
	} else if (m_anchor != NULL) {
		delete m_anchor;
		m_anchor = NULL;
	}
}

void ScrollViewBase::SetKeyStep(float step)
{
	m_key_step = step;
}

int32_t ScrollViewBase::SetString(const wstring &str, int32_t index)
{
	graph::DrawObj *draw_obj = GetDrawObj(index);

	if (draw_obj != NULL && draw_obj->IsInherit(graph::TextObj::DrawObjType()))
		return static_cast<graph::TextObj *>(draw_obj)->SetString(str);
	return 0x80AF4101;
}

int32_t ScrollViewBase::GetString(wstring &str, int32_t index)
{
	graph::DrawObj *draw_obj = GetDrawObj(index);

	if (draw_obj != NULL && draw_obj->IsInherit(graph::TextObj::DrawObjType()))
		return static_cast<graph::TextObj *>(draw_obj)->GetString(str);
	return 0x80AF4101;
}

int32_t ScrollViewBase::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj == NULL)
		return 0x80AF4101;
	draw_obj->SetTexture(surf, index);
	return 0;
}

int32_t ScrollViewBase::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj == NULL)
		return 0x80AF4101;
	draw_obj->GetTexture(surf, index);
	return 0;
}

Widget *ScrollViewBase::GetInternalWidget()
{
	return m_view_parent;
}

void TouchScrollSupplier::Release(ScrollViewBase *view)
{
	vector<TouchView>::iterator it;

	for (it = m_touch_views.begin(); it != m_touch_views.end(); ++it)
		if (it->m_view == view)
			break;
	if (it != m_touch_views.end()) {
		if (it == m_touch_views.begin())
			StopFlick();
		else
			m_touch_views.erase(it);
	}
}

void ScrollViewBase::DestroyWidget()
{
	s_touch_scroll_supplier.Unregister(this);
	s_analog_scroll_supplier.Unregister(this);
	ScePafWidget_A89BE22F(this);
	s_touch_scroll_supplier.Release(this);
	if (m_debug != NULL) {
		delete m_debug;
		m_debug = NULL;
	}
	Widget::DestroyWidget();
}

/* its state, but 3 (ready) or 0 for states 0, 1 and 3 as its draw objects are ready */
uint32_t ScrollViewBase::CheckState()
{
	uint32_t state = m_state;

	if (state != 1 && state != 0 && state != 3)
		return state;
	return IsPrepareObjs() ? 3 : 0;
}

void ScrollViewBase::UpdateAnchor()
{
	math::v2 *anchor = m_anchor;

	if (anchor != NULL) {
		math::v2 pos(GetOriginWorldMatrix().getImpl().getCol3().getXY());

		if (!(*anchor == pos)) {
			SetViewFocus(GetViewFocus() - *anchor + pos, NULL, true, true);
			*anchor = pos;
		}
	}
}

void ScrollViewBase::UpdateViewLimit()
{
	using sce::Vectormath::Simd::Aos::Vector4;
	using sce::Vectormath::Simd::Aos::mulPerElem;

	Vector4 size(m_view_size.getImpl(), m_view_size.getImpl());
	math::v2 widget_size(m_size.getImpl().getXY());

	m_fit_dir = MoveDir_None;
	m_view_half = math::v4(mulPerElem(size, Vector4(-0.5f, 0.5f, 0.5f, -0.5f)));

	math::v4 limit((mulPerElem(size, Vector4(0.5f, -0.5f, -0.5f, 0.5f)) +
			m_scroll_box.getImpl()) * -1.0f);

	m_view_limit = limit;
	if (widget_size.getImpl().getX() > m_scroll_box_size.getImpl().getX()) {
		m_fit_dir = MoveDir_X;
		limit.set_x(m_scroll_center.extract_x());
		m_view_limit = limit;
	}
	if (widget_size.getImpl().getY() > m_scroll_box_size.getImpl().getY()) {
		m_fit_dir |= MoveDir_Y;
		limit.set_y(m_scroll_center.extract_y());
		m_view_limit = limit;
	}
}

void ScrollViewBase::UpdateSize()
{
	Widget::UpdateSize();
	m_view_parent->SetSize(m_size);
	m_view_size = math::v2(m_size.getImpl().getXY());
	UpdateViewLimit();
	UpdateMovableDirection();
	UpdateScrollLayout();
	ScePafWidget_A0538225(-1);
	UpdateScrollRange();
	UpdateAnchor();
}

void ScrollViewBase::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	if (m_touch_captures.size() == 0)
		s_touch_scroll_supplier.StopFlick();
}

int32_t ScrollViewBase::HandleTouchFwEvent(Event *e)
{
	if (e != NULL) {
		uint32_t port = (e->m_detail >> 20) & 0xF;

		if (port == 0) {
			switch (e->GetType()) {
			case 0x41:
			case 0x44:
				if (TrackBegin(e))
					e->Consume();
				break;
			case 0x42:
				if (TrackEnd(e))
					e->Consume();
				break;
			case 0x47:
				if (TrackEnd(e))
					e->Consume();
				break;
			}
		} else if (port == 1) {
			switch (e->GetType()) {
			case 0x42:
				if (ScePafWidget_3196DEE8(e))
					e->Consume();
				break;
			case 0x47:
				if (ScePafWidget_3196DEE8(e))
					e->Consume();
				break;
			}
		}
	}
	return Handler::HandleTouchFwEvent(e);
}

int32_t ScrollViewBase::HandleTouchEvent(Event *e)
{
	if (e != NULL) {
		uint32_t port = (e->m_detail >> 20) & 0xF;

		if (port == 0) {
			switch (e->GetType()) {
			case 0x41:
				if (TrackPress(e))
					e->Consume();
				break;
			case 0x42:
				if (TrackRelease(e))
					e->Consume();
				break;
			case 0x43:
				if (TrackMove(e))
					e->Consume();
				break;
			case 0x44:
				if (TrackLongPress(e))
					e->Consume();
				break;
			case 0x46:
				if (TrackGrab(e))
					e->Consume();
				break;
			}
		} else if (port == 1) {
			switch (e->GetType()) {
			case 0x41:
				if (ScePafWidget_46CD91F6(e))
					e->Consume();
				break;
			case 0x42:
				if (ScePafWidget_3196DEE8(e))
					e->Consume();
				break;
			}
		}
	}
	return Widget::HandleTouchEvent(e);
}

int32_t ScrollViewBase::HandleGestureEvent(Event *e)
{
	if (e != NULL) {
		uint32_t detail = e->m_detail;
		uint32_t port = (detail >> 20) & 0xF;

		if (port == 0) {
			switch ((detail >> 8) & 0x7F) {
			case 3: {
				if (OnFlick(e))
					e->Consume();

				Event end(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);
				DoEvent(CB_SCROLL_VIEW_BASE_END, &end);
				break;
			}
			case 7:
				if (TakeTrackedGesture(e))
					e->Consume();
				break;
			case 10:
				if (MochiBegin(e))
					e->Consume();
				break;
			case 11:
				if (MochiMove(e))
					e->Consume();
				break;
			case 0x10:
				if (TrackDrag(e))
					e->Consume();
				break;
			case 0x16:
				if (TrackPan(e))
					e->Consume();
				break;
			}
		} else if (port == 1) {
			switch ((detail >> 8) & 0x7F) {
			case 7:
				if (ScePafWidget_0BC06625(e))
					e->Consume();
				break;
			case 10:
				if (ScePafWidget_2141506E(e))
					e->Consume();
				break;
			case 11:
				if (ScePafWidget_0ECFFCE1(e))
					e->Consume();
				break;
			case 13:
				if (ScePafWidget_3403FD90(e))
					e->Consume();
				break;
			case 14:
				if (ScePafWidget_22F22B80(e))
					e->Consume();
				break;
			}
		}
	}
	return Widget::HandleGestureEvent(e);
}

ScrollViewBase *ScrollViewBase::GetParentView()
{
	for (Widget *parent = m_parent; parent != NULL; parent = parent->m_parent)
		if (parent->IsInherit(m_widget_type))
			return static_cast<ScrollViewBase *>(parent);
	return NULL;
}

int32_t ScrollViewBase::HandleKeycodeEvent(Event *e)
{
	if (e != NULL) {
		switch (e->m_detail & 0xFF) {
		case 1:
			if (OnUpKey(e))
				e->Consume();
			break;
		case 2:
			if (OnDownKey(e))
				e->Consume();
			break;
		case 3:
			if (OnLeftKey(e))
				e->Consume();
			break;
		case 4:
			if (OnRightKey(e))
				e->Consume();
			break;
		default:
			if (m_pre_dispatch && (e->m_type & 0x40000000) != 0 &&
			    (e->m_type & Event::ROUTE_TERM) != 0)
				return 0;
		}

		uint32_t type = e->GetType();

		if (type == 0x11) {
			uint32_t detail = e->m_detail;

			switch ((uint32_t)(detail & 0xFF)) {
			case 1:
			case 2:
			case 3:
			case 4: {
				Environment *env = m_env;
				uint32_t port = (detail >> 20) & 0xF;

				env->ReleasePadCaptureLow(port);
				env->ReleasePadCaptureHigh(port);
				ScePafWidget_ABF05727(m_env, 0, true, NULL);
				ScePafWidget_ABF05727(m_env, 1, true, NULL);
				break;
			}
			}
			m_focus_lock = true;

			ScrollViewBase *parent = GetParentView();
			if (parent != NULL)
				parent->m_focus_lock = true;
		} else if (type == 0x12 || type == 0x14) {
			m_focus_lock = false;

			ScrollViewBase *parent = GetParentView();
			if (parent != NULL)
				parent->m_focus_lock = false;
		}
	}
	if (m_track_port != -1)
		return 0;
	return Widget::HandleKeycodeEvent(e);
}

int32_t ScrollViewBase::HandleFocusSwitchEvent(Event *e)
{
	int32_t ret = Widget::HandleFocusSwitchEvent(e);

	if ((e->m_type & Event::STATE_CONSUME) != 0) {
		Environment *env = m_env;

		if (env != NULL && FindChild(env->m_focus_cursor.m_target, 0) != NULL &&
		    m_env->m_focus_history_on)
			m_env->m_focus_cursor.RemoveFocusHistory(this);
	}
	return ret;
}

int32_t ScrollViewBase::HandleFocusEvent(Event *e)
{
	if (e != NULL) {
		switch (e->GetType()) {
		case 0x401:
			if (OnFocusIn(e))
				e->Consume();
			break;
		case 0x402:
			if (OnFocusEnter(e))
				e->Consume();
			break;
		case 0x403:
			if (OnFocusLeave(e))
				e->Consume();
			break;
		case 0x404:
			if (OnFocusOut(e))
				e->Consume();
			break;
		}
	}
	return Handler::HandleFocusEvent(e);
}

int32_t ScrollViewBase::HandleCommandEvent(Event *e)
{
	if (e != NULL) {
		switch (e->GetType()) {
		case TextBoxImeListener::EVENT_EDIT_BEGIN:
			if (OnEditBegin(e))
				e->Consume();
			break;
		case TextBoxImeListener::EVENT_EDIT_END:
			if (OnEditEnd(e))
				e->Consume();
			break;
		case TextBoxImeListener::EVENT_IME_OPEN:
		case TextBoxImeListener::EVENT_IME_CHANGE_SIZE:
		case TextBoxImeListener::EVENT_IME_EDIT:
			if (OnImeEvent(e))
				e->Consume();
			break;
		}
	}
	return Handler::HandleCommandEvent(e);
}

void ScrollViewBase::OnViewLayoutUpdated(bool is_dirty)
{
	if (is_dirty) {
		ScePafWidget_A0538225(-1);
		UpdateScrollRange();
	}
}

void ScrollViewBase::SendFocusChangeEvent()
{
	math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 focus(pos.getImpl().getXY());

	if (!(focus == m_focus_pos)) {
		m_focus_pos = focus;

		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);
		DoEvent(CB_SCROLL_VIEW_BASE_FOCUS_CHANGED, &e);
	}
}

void ScrollViewBase::OnPartsLayoutUpdated(bool is_dirty)
{
	if (is_dirty) {
		ScePafWidget_A0538225(-1);
		UpdateScrollRange();
		SendFocusChangeEvent();
	}
}

ScrollViewBase::~ScrollViewBase()
{
	if (m_bar_controller != NULL)
		delete m_bar_controller;
	m_bar_controller = (ScrollViewScrollBarController *)-1;
	if (m_scroll_param != NULL)
		delete m_scroll_param;
	m_scroll_param = (ScrollViewScrollParam *)-1;
	if (m_over_scroll != NULL)
		delete m_over_scroll;
	m_over_scroll = (ScrollViewOverScroll *)-1;
	if (m_anchor != NULL)
		delete m_anchor;
	m_anchor = (math::v2 *)-1;
}

void ScrollViewBase::UpdateAfterChildren()
{
	ScePafWidget_22EF63CE();
	ScePafWidget_B6CECFDF();
}

math::v2 ScrollViewBase::GetTrackPos()
{
	uint32_t id = m_track_port;
	TouchState *state = NULL;

	if (id != (uint32_t)-1 && id < 8) {
		m_env->m_touch_state[id].m_state = 0;
		m_env->m_touch_state[id].m_id = id;
		state = &m_env->m_touch_state[id];
	}
	if (state != NULL) {
		Camera *camera = m_scene->m_camera;

		if (camera != NULL) {
			TouchState touch = *state;
			const math::matrix &camera_world = camera->GetWorldMatrix();

			ScePafWidget_72F479CA(camera);

			math::matrix world = camera_world;

			touch.m_depth = camera->m_distance;
			touch.m_line_start = world.w();
			touch.m_has_line = true;
			touch.m_camera_matrix = world;
			touch.m_line_end = math::v4(world.getImpl() *
						    sce::Vectormath::Simd::Aos::Vector4(
							    -touch.m_pos.extract_x(),
							    touch.m_pos.extract_y(), touch.m_depth,
							    1.0f));

			HitInfo info;

			/* cleared once more after its constructor did */
			sce_paf_memset(&info, 0, sizeof(HitInfo));
			CalcHitLineToPlane(touch.m_line_start, touch.m_line_end,
					   math::v4(0.0f, 0.0f, 0.0f, 0.0f),
					   math::v4(0.0f, 0.0f, 1.0f, 0.0f), &info);
			return info.m_touch_pos;
		}
	}
	return math::v2(0.0f, 0.0f);
}

void TouchScrollSupplier::UpdateTouch(ScrollViewBase *view)
{
	if (m_touch_views.size() != 0 && m_touch_views.back().m_view == view) {
		m_touch_pos = m_touch_views.begin()->m_view->GetTrackPos();
		if (m_mochi.m_points.size() != 0) {
			uint64_t time = ScePafMisc_256F9060[0];

			if (m_mochi.m_time != time) {
				m_mochi.m_points.push_back(
					MochiPoint(math::v2(0.0f, 0.0f), time - m_mochi.m_time, false));
				if (m_mochi.m_points.size() >= 8)
					m_mochi.m_points.erase(m_mochi.m_points.begin(),
							       m_mochi.m_points.begin() + 1);
				m_mochi.m_time = time;
			}
		}
	}
}

void ScrollViewBase::UpdateWorldRot()
{
	if (m_hold_count <= 0 && !(m_world_rot == m_world_rot_pre)) {
		m_world_rot_pre = m_world_rot;
		UpdateScrollRange();
	}
}

ScrollViewFocusLimit ScrollViewBase::LimitFocus(const math::v2 &focus, bool center_fit)
{
	ScrollViewFocusLimit ret;

	if (!m_limit_focus) {
		ret.m_pos = focus;
		ret.m_over = math::v2(0.0f, 0.0f);
		ret.m_dir = MoveDir_None;
		return ret;
	}

	uint32_t fit = m_fit_dir;

	if (fit == MoveDir_XY) {
		if (center_fit) {
			math::v2 center = m_scroll_center;

			ret.m_pos = center;
			ret.m_dir = (m_scroll_center.extract_x() != focus.extract_x()) |
				    (m_scroll_center.extract_y() != focus.extract_y());
			ret.m_over = focus - center;
		} else {
			ret.m_pos = focus;
			ret.m_over = math::v2(0.0f, 0.0f);
			ret.m_dir = MoveDir_None;
		}
		return ret;
	}

	using sce::Vectormath::Simd::Aos::Vector4;

	math::v4 over(m_scroll_box.getImpl() -
		      (m_view_half.getImpl() - Vector4(focus.getImpl(), focus.getImpl())));
	float focus_x = focus.extract_x();
	float focus_y = focus.extract_y();
	float x = focus_x;
	float y = focus_y;

	if ((fit & MoveDir_X) != 0) {
		if (center_fit)
			x = m_scroll_center.extract_x();
	} else if (over.extract_x() > 0.0f) {
		x = m_view_limit.extract_x();
	} else if (over.extract_z() < 0.0f) {
		x = m_view_limit.extract_z();
	}
	if ((fit & MoveDir_Y) != 0) {
		if (center_fit)
			y = m_scroll_center.extract_y();
	} else if (over.extract_w() > 0.0f) {
		y = m_view_limit.extract_w();
	} else if (over.extract_y() < 0.0f) {
		y = m_view_limit.extract_y();
	}

	if (x == focus_x || y == focus_y) {
		if (y == focus_y) {
			if (x == focus_x) {
				ret.m_pos = focus;
				ret.m_over = math::v2(0.0f, 0.0f);
				ret.m_dir = MoveDir_None;
			} else {
				ret.m_pos = math::v2(x, focus_y);
				ret.m_over = math::v2(focus_x - x, 0.0f);
				ret.m_dir = MoveDir_X;
			}
		} else {
			ret.m_pos = math::v2(focus_x, y);
			ret.m_over = math::v2(0.0f, focus_y - y);
			ret.m_dir = MoveDir_Y;
		}
	} else {
		math::v2 pos(x, y);

		ret.m_pos = pos;
		ret.m_over = focus - pos;
		ret.m_dir = MoveDir_XY;
	}
	return ret;
}

void ScrollViewDebug::Update()
{
	using sce::Vectormath::Simd::Aos::Vector4;
	using sce::Vectormath::Simd::Aos::divPerElem;
	using sce::Vectormath::Simd::Aos::mulPerElem;

	math::v4 parent_pos = m_view->m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 pos = math::v2(parent_pos.getImpl().getXY()) * -1.0f;
	ScrollViewBase *view = m_view;
	math::v2 size = view->m_view_size;

	if (IsSame(pos, m_box_pos) && IsSame(size, m_box_size))
		return;

	m_box_pos = pos;
	m_box_size = size;

	math::v4 box(Vector4(pos.getImpl(), pos.getImpl()) +
		     mulPerElem(Vector4(size.getImpl(), size.getImpl()),
				Vector4(-0.5f, 0.5f, 0.5f, -0.5f)));

	if (m_mode == 1 || m_mode == 3) {
		math::v4 view_box = view->m_view_box;

		if (view_box.extract_x() > box.extract_x()) {
			box.set_x(view_box.extract_x());
			box.set_z(view_box.extract_x() + size.extract_x());
		} else if (box.extract_z() > view_box.extract_z()) {
			box.set_x(view_box.extract_z() - size.extract_x());
			box.set_z(view_box.extract_z());
		}
		if (box.extract_y() > view_box.extract_y()) {
			box.set_y(view_box.extract_y());
			box.set_w(view_box.extract_y() - size.extract_y());
		} else if (view_box.extract_w() > box.extract_w()) {
			box.set_y(view_box.extract_w() + size.extract_y());
			box.set_w(view_box.extract_w());
		}
	}

	math::v2 depth(0.0f, 0.0f);
	math::v2 over(0.0f, 0.0f);

	if (m_mode == 3) {
		math::v4 focus = view->m_view_parent->GetPos(0, NULL,
							     math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		over = view->LimitFocus(math::v2(focus.getImpl().getXY()), false).m_over;
	}

	math::v2 cell(sce::Vectormath::Simd::Aos::Vector2(sce_vectormath_floorf(
		divPerElem(sce::Vectormath::Simd::Aos::Vector2(box.extract_z(), box.extract_y()),
			   size.getImpl())
			.get64())));
	int32_t cell_x = (int32_t)cell.extract_x();
	int32_t cell_y = (int32_t)cell.extract_y();
	int32_t abs_x = sce_paf_abs(cell_x);
	int32_t row = sce_paf_abs(cell_y) % 2 * 2;
	int32_t index = abs_x % 2 + row;
	Marker *markers[4];

	markers[0] = &m_marker[index];
	markers[1] = &m_marker[(abs_x + 1) % 2 + row];
	markers[2] = &m_marker[(index + 2) % 4];
	markers[3] = &m_marker[3 - index];

	math::v2 origin = cell * size;

	for (uint32_t i = 0; i < 4; i++) {
		static const math::v2 s_offset[4] = {
			math::v2(-0.5f, 0.5f),
			math::v2(0.5f, 0.5f),
			math::v2(-0.5f, -0.5f),
			math::v2(0.5f, -0.5f),
		};
		Marker *marker = markers[i];
		Widget *widget = marker->m_widget;
		math::v2 marker_pos = size * s_offset[i] + origin;

		widget->SetPos(math::v4(Vector4((marker_pos - over).getImpl(), depth.getImpl())),
			       NULL, 0x10000, 0, 0.0f, NULL, NULL);
		if (!IsSame(marker->m_pos, marker_pos) ||
		    !IsSame(math::v2(widget->GetSize(0)->getImpl().getXY()), size)) {
			widget->SetSize(math::v4(Vector4(size.getImpl(), depth.getImpl())), NULL,
					0x10004, 0, 0.0f, NULL, NULL);
			marker->m_pos = marker_pos;
			marker->m_cell_x = cell_x;
			marker->m_cell_y = cell_y;

			Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0,
				(int32_t)marker);

			m_view->DoEvent(CB_MARKER_MOVED, &e);
		}
	}
}

void ScrollViewBase::UpdatePost()
{
	math::v2 *anchor = m_anchor;

	if (anchor != NULL)
		*anchor = math::v2(GetOriginWorldMatrix().getImpl().getCol3().getXY());
	if (m_debug != NULL)
		m_debug->Update();
	if (!m_ime_padding)
		m_ime_height = 0.0f;
	UpdateWorldRot();
	if (m_track_port != -1 || m_track_state != 0)
		s_touch_scroll_supplier.UpdateTouch(this);
	if (m_over_scroll != NULL)
		m_over_scroll->Update();
}

/* the gesture listener attached to the touch of the front touch port with priority 21: -1
   where there is no listener to attach it to (name ours) */
static inline int32_t AttachTouchGesture(Environment *env, uint32_t touch_id,
					 common::SharedPtr<inputdevice::GestureListener> listener)
{
	inputdevice::InputGestureListener *input = env->GetTouchGestureListener(0).get();

	if (input == NULL || input->AttachGestureListener(touch_id, 21, listener) < 0)
		return -1;
	return 0;
}

void TouchScrollSupplier::DetachGesture()
{
	if (m_gesture_attached) {
		m_gesture_attached = false;
		if (m_touch_views.size() != 0) {
			ScrollViewBase *view = m_touch_views.begin()->m_view;

			if (view->m_track_port != -1) {
				inputdevice::InputGestureListener *input =
					view->m_env->GetTouchGestureListener(0).get();

				if (input != NULL)
					input->DetachGestureListener(view->m_track_port, 21, view);
			}
		}
	}
}

void TouchScrollSupplier::AttachGesture(uint32_t dir)
{
	DetachGesture();
	if (dir == ScrollViewBase::MoveDir_None || m_touch_views.size() == 0)
		return;

	ScrollViewBase *view = m_touch_views.begin()->m_view;

	if (view->m_track_port == -1)
		return;

	ScrollGestureListener *gesture = new ScrollGestureListener(view->m_track_port, view);
	common::SharedPtr<inputdevice::GestureListener> listener(gesture);

	if (AttachTouchGesture(view->m_env, view->m_track_port, listener) < 0)
		return;

	math::v2 axis;

	switch (dir) {
	case ScrollViewBase::MoveDir_X:
		axis = math::v2(1.0f, 0.0f);
		break;
	case ScrollViewBase::MoveDir_Y:
		axis = math::v2(0.0f, 1.0f);
		break;
	case ScrollViewBase::MoveDir_XY:
		axis = math::v2(1.0f, 1.0f);
		break;
	default:
		axis = math::v2(0.0f, 0.0f);
		break;
	}
	gesture->m_axis = axis;
	m_gesture_attached = true;
}

bool TouchScrollSupplier::IsGestureTaken()
{
	if (m_touch_views.size() == 0)
		return false;

	ScrollViewBase *view = m_touch_views.back().m_view;
	list<HitInfo> &hits = view->m_env->m_touch_hits;
	list<HitInfo>::iterator it = hits.begin();
	list<HitInfo>::iterator end = hits.end();
	list<HitInfo>::iterator hit;

	for (hit = it; hit != end; ++hit) {
		if (hit->m_widget == view && hit->m_touch_id == view->m_track_port) {
			it = ++hit;
			break;
		}
	}

	uint32_t touch_id = view->m_track_port;

	for (; it != end; ++it)
		if (it->m_touch_id == touch_id && (it->m_widget->m_touch_gesture_mask & 0x20) != 0)
			return true;
	return false;
}

bool TouchScrollSupplier::IsAnalogOnly()
{
	for (vector<TouchView>::iterator it = m_touch_views.begin(); it != m_touch_views.end();
	     ++it) {
		ScrollViewBase *view = it->m_view;

		if (!view->m_scroll_analog)
			return false;
		if (view->m_horizontal_bar != NULL &&
		    view->m_horizontal_bar->IsInherit(ScrollBarTouch::TypeName()))
			return false;
		if (view->m_vertical_bar != NULL &&
		    view->m_vertical_bar->IsInherit(ScrollBarTouch::TypeName()))
			return false;
	}
	return true;
}

uint32_t TouchScrollSupplier::GetMovableDir(bool masked)
{
	vector<TouchView>::iterator it = m_touch_views.begin();
	vector<TouchView>::iterator end = m_touch_views.end();
	uint32_t dir = ScrollViewBase::MoveDir_None;

	if (masked) {
		for (; it != end; ++it)
			dir |= it->m_view->m_movable_dir_y & ~it->m_view->m_movable_dir_off;
	} else {
		for (; it != end; ++it)
			dir |= it->m_view->m_movable_dir_y;
	}
	return dir;
}

/* the widget is on the touch */
static inline bool IsTouchOn(Widget *widget, uint32_t touch_id)
{
	list<HitInfo> &hits = widget->m_env->m_touch_hits;

	for (list<HitInfo>::iterator it = hits.begin(); it != hits.end(); ++it)
		if (it->m_widget == widget && it->m_touch_id == touch_id)
			return true;
	return false;
}

bool TouchScrollSupplier::IsOnScrollBar()
{
	if (m_touch_views.size() == 0)
		return false;

	ScrollViewBase *view = m_touch_views.back().m_view;

	if (view->m_horizontal_bar != NULL &&
	    view->m_horizontal_bar->IsInherit(ScrollBarTouch::TypeName()) &&
	    IsTouchOn(view->m_horizontal_bar, view->m_track_port))
		return true;
	if (view->m_vertical_bar == NULL ||
	    !view->m_vertical_bar->IsInherit(ScrollBarTouch::TypeName()))
		return false;
	return IsTouchOn(view->m_vertical_bar, view->m_track_port);
}

void TouchScrollSupplier::BeginTouch()
{
	m_drag_dir = ScrollViewBase::MoveDir_None;

	math::v2 pos = m_touch_views.begin()->m_view->GetTrackPos();

	m_dragging = false;
	m_drag_locked = false;
	m_touch_start = pos;
	m_touch_pos = pos;
	m_reset = false;
	m_touch_state = 1;
	m_drag_time = 0;
	DetachGesture();

	for (vector<TouchView>::iterator it = m_touch_views.begin(); it != m_touch_views.end();
	     ++it) {
		if (!it->m_view->m_scroll_touch) {
			m_drag_dir = ScrollViewBase::MoveDir_XY;
			m_touch_state = 2;
			break;
		}
	}

	math::v2 velocity(0.0f, 0.0f);

	for (vector<TouchView>::iterator it = m_touch_views.begin(); it != m_touch_views.end();
	     ++it) {
		ScrollViewBase *view = it->m_view;

		it->m_id = view->FindSnapID(view->GetViewFocusTarget());
		velocity = velocity + view->m_velocity;
		view->m_velocity = math::v2(0.0f, 0.0f);
	}

	m_mochi.m_points.clear();
	m_mochi.m_time = ScePafMisc_256F9060[0];
	m_mochi.m_start_time = ScePafMisc_256F9060[0];
	if (!IsSame(velocity, math::v2(0.0f, 0.0f))) {
		float time = Timer::s_tick_unit * 1000000.0f;

		m_mochi.m_points.push_back(
			MochiPoint(velocity * Timer::s_tick_unit, (uint64_t)time, true));
		if (m_drag_dir == ScrollViewBase::MoveDir_None &&
		    lengthSqr(velocity.getImpl()) > 200000.0f) {
			uint32_t dir = velocity.extract_x() != 0.0f;

			if (velocity.extract_y() != 0.0f)
				dir |= ScrollViewBase::MoveDir_Y;
			m_drag_dir = dir;
			m_touch_state = 2;
		}
	}
}

/* the scroll view the widget is in (name ours) */
static inline ScrollViewBase *GetParentScrollView(Widget *widget)
{
	for (Widget *parent = widget->m_parent; parent != NULL; parent = parent->m_parent)
		if (parent->IsInherit("scroll_view_base"))
			return (ScrollViewBase *)parent;
	return NULL;
}

void TouchScrollSupplier::GetTouchViews(vector<TouchView> &views, ScrollViewBase *view)
{
	vector<ScrollViewBase *> chain;

	chain.push_back(view);
	for (ScrollViewBase *parent = GetParentScrollView(view); parent != NULL;
	     parent = GetParentScrollView(parent))
		chain.push_back(parent);

	views.resize(0);
	if (views.capacity() < chain.size())
		views.reserve(chain.size());
	for (int32_t i = chain.size() - 1; i >= 0; i--)
		views.push_back(TouchView(chain.at(i)));
}

bool TouchScrollSupplier::TakeTouch(ScrollViewBase *view, uint32_t touch_id)
{
	if (m_reset)
		StopFlick();
	if (m_touch_views.size() != 0)
		return m_touch_views.begin()->m_view->m_touch_captures.size() != 0;

	ScrollViewBase *top = NULL;
	list<HitInfo> &hits = view->m_env->m_touch_hits;
	list<HitInfo>::iterator end = hits.end();

	for (list<HitInfo>::iterator it = hits.begin(); it != end; ++it) {
		if (it->m_touch_id == touch_id && it->m_widget != NULL &&
		    it->m_widget->IsInherit("scroll_view_base") && it->m_widget != NULL)
			top = (ScrollViewBase *)it->m_widget;
	}
	if (top != view)
		return false;

	GetTouchViews(m_touch_views, view);

	bool moving = false;

	for (vector<TouchView>::iterator it = m_touch_views.begin(); it != m_touch_views.end();
	     ++it) {
		ScrollViewBase *v = it->m_view;

		v->ScePafWidget_E9C7617A(touch_id);
		if (!moving && v->m_view_parent->FindAnim(0x10000) != NULL) {
			ScrollViewTimer *timer = v->m_focus_timer;

			if (timer == NULL || timer->m_type != ScrollViewTimer::TIMER_TYPE_MOVE ||
			    timer->m_userdata >= 0)
				moving = true;
		}
	}

	BeginTouch();

	bool on_bar = IsOnScrollBar();
	ScrollViewBase *first = m_touch_views.begin()->m_view;
	uint32_t dir = GetMovableDir(false);

	if (dir != ScrollViewBase::MoveDir_None && IsAnalogOnly() && !on_bar) {
		ScePafWidget_712AD1C7(first, 0, touch_id, 64, first->CalcHitRect());

		inputdevice::InputGestureListener *input =
			first->m_env->GetTouchGestureListener(0).get();
		common::SharedPtr<inputdevice::GestureListener> listener =
			input != NULL ? input->GetGestureListener(touch_id, 50, first)
				      : common::SharedPtr<inputdevice::GestureListener>();

		if (listener.get() != NULL) {
			DragGestureListener *drag = (DragGestureListener *)listener.get();
			DragGestureParam param;

			drag->GetParam(&param);

			const ScrollViewScrollParam *scroll = first->m_scroll_param;

			if (dir == ScrollViewBase::MoveDir_X)
				param.m_dir = 0;
			else if (dir == ScrollViewBase::MoveDir_Y)
				param.m_dir = 1;
			else
				param.m_dir = 2;
			if (scroll == NULL)
				scroll = &TouchScrollConfig::Get().m_param;
			param.m_start_distance[0] = scroll->m_value[0];
			param.m_start_distance[2] = scroll->m_value[1];
			param.m_start_distance[1] = scroll->m_value[2];
			param.m_move_tolerance = scroll->m_value[5];
			param.m_end_speed[0] = scroll->m_value[6];
			param.m_flick_speed = scroll->m_value[22];
			param.m_end_speed[1] = scroll->m_value[7];
			param.m_end_distance[0] = scroll->m_value[8];
			param.m_end_distance[1] = scroll->m_value[9];
			param.m_end_distance[2] = scroll->m_value[10];
			if (scroll->m_bounce) {
				math::v4 size = *first->GetSize(0);

				if (param.m_dir == 1) {
					/* the distances scaled down for a view under 410.5 high, to
					   their least at 277 */
					float t = (size.extract_y() - 277.0f) / 133.5f;

					if (t < 0.0f)
						t = 0.0f;

					float scale = 1.0f;

					if (t < 1.0f)
						scale = t;
					if (param.m_start_distance[0] >= 12.0f)
						param.m_start_distance[0] =
							(param.m_start_distance[0] - 12.0f) * scale + 12.0f;
					if (param.m_start_distance[2] >= 12.0f)
						param.m_start_distance[2] =
							(param.m_start_distance[2] - 12.0f) * scale + 12.0f;
					if (param.m_end_speed[0] >= 150.0f)
						param.m_end_speed[0] =
							(param.m_end_speed[0] - 150.0f) * scale + 150.0f;
					if (param.m_end_distance[0] >= 30.0f)
						param.m_end_distance[0] =
							(param.m_end_distance[0] - 30.0f) * scale + 30.0f;
					if (param.m_end_distance[1] >= 30.0f)
						param.m_end_distance[1] =
							(param.m_end_distance[1] - 30.0f) * scale + 30.0f;
				}
			}
			drag->SetParam(&param);
		}
	}

	if (!on_bar && !IsGestureTaken() && (moving || m_mochi.m_points.size() != 0)) {
		ScePafWidget_2B1A1493(first, 1);
		ScePafWidget_D3D0A7C3(first, 0, touch_id, true, true);
		first->SetFocus(NULL);
		first->m_env->m_front_touch_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		first->m_env->m_rear_touch_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		return true;
	}
	AttachGesture(dir);
	return false;
}

/* the listener attached to the touch again, at priority 50 (name ours) */
static inline void ReattachTouchGesture(Environment *env, uint32_t touch_id,
					common::SharedPtr<inputdevice::GestureListener> listener)
{
	inputdevice::InputGestureListener *input = env->GetTouchGestureListener(0).get();

	if (input != NULL)
		input->AttachGestureListener(touch_id, 50, listener);
}

bool ScrollViewBase::TrackBegin(Event *e)
{
	if (e == NULL)
		return false;

	uint32_t track_port = m_track_port;

	if (track_port != -1) {
		inputdevice::InputGestureListener *input =
			m_env->GetTouchGestureListener(0).get();
		common::SharedPtr<inputdevice::GestureListener> listener =
			input != NULL ? input->GetGestureListener(track_port, 50, this)
				      : common::SharedPtr<inputdevice::GestureListener>();

		if (listener.get() != NULL)
			ReattachTouchGesture(m_env, (uint8_t)e->m_detail, listener);
		if ((m_drag_dir & m_touch_mask_on) != 0)
			return true;
	}
	return s_touch_scroll_supplier.TakeTouch(this, (uint8_t)e->m_detail);
}

void TouchScrollSupplier::BeginDrag(const math::v2 &move, bool masked)
{
	if (m_touch_state != 1)
		return;

	bool both = false;
	uint32_t dir = ScrollViewBase::MoveDir_None;

	for (vector<TouchView>::iterator it = m_touch_views.begin(); it != m_touch_views.end();
	     ++it) {
		ScrollViewBase *view = it->m_view;
		uint32_t off = masked ? view->m_movable_dir_off : 0;
		uint32_t d = view->m_movable_dir & ~off;

		if (d == ScrollViewBase::MoveDir_XY)
			both = true;
		dir |= d;
	}

	if (dir != ScrollViewBase::MoveDir_XY) {
		m_drag_dir = dir;
	} else {
		math::v2 a(sce::Vectormath::Simd::Aos::absPerElem(move.getImpl()));

		m_drag_dir = (a.getImpl().getX() > a.getImpl().getY()).getAsBool()
				     ? ScrollViewBase::MoveDir_X
				     : ScrollViewBase::MoveDir_Y;
		if (both) {
			float distance;

			if (masked) {
				distance = TouchScrollConfig::Get().m_hold_distance * 0.6f;
			} else {
				/* from 1.4 times the hold distance at the touch's begin down to
				   0.6 times it 134 ms on */
				float t = (float)(ScePafMisc_256F9060[0] - m_mochi.m_start_time) /
					  134000.0f;

				if (t < 0.0f)
					t = 0.0f;

				float s = 1.0f;

				if (t < 1.0f)
					s = t;

				float k = (1.0f - s) * 0.79999995f;

				distance = (k + 0.6f) * TouchScrollConfig::Get().m_hold_distance;
			}
			if (a.extract_x() > distance)
				m_drag_dir |= ScrollViewBase::MoveDir_X;
			if (a.extract_y() > distance)
				m_drag_dir |= ScrollViewBase::MoveDir_Y;
		}
	}
	StopViews();
	m_touch_state = 2;
}

bool TouchScrollSupplier::IsDragged()
{
	if (m_touch_views.size() == 0)
		return false;

	math::v2 pos = m_touch_views.begin()->m_view->GetTrackPos();
	math::v2 d(sce::Vectormath::Simd::Aos::absPerElem((m_touch_start - pos).getImpl()));
	uint32_t dir = ScrollViewBase::MoveDir_None;

	if (d.extract_x() > TouchScrollConfig::Get().m_drag_distance)
		dir = ScrollViewBase::MoveDir_X;
	if (d.extract_y() > TouchScrollConfig::Get().m_drag_distance)
		dir |= ScrollViewBase::MoveDir_Y;
	for (vector<TouchView>::iterator it = m_touch_views.begin(); it != m_touch_views.end();
	     ++it)
		if ((dir & it->m_view->m_movable_dir) != 0)
			return true;
	return false;
}

void TouchScrollSupplier::Drag(const math::v2 &move)
{
	DetachGesture();
	if (m_touch_state == 1) {
		if (!IsDragged()) {
			/* the flicks stopped once the touch is held 67 ms */
			if (ScePafMisc_256F9060[0] - m_mochi.m_start_time >= 67000)
				StopViews();
			return;
		}

		math::v2 pos = m_touch_views.begin()->m_view->GetTrackPos();

		BeginDrag(m_touch_start - pos, false);
		StopViews();
		m_touch_state = 2;
	} else if (m_touch_state != 2) {
		return;
	}

	math::v2 rest = MaskMoveDir(move, m_drag_dir);

	for (int32_t i = m_touch_views.size() - 1; i >= 0; i--) {
		if (IsSame(rest, math::v2(0.0f, 0.0f)))
			break;
		m_touch_views.at(i).m_view->ScePafWidget_D95AFF11(rest, m_dragging, m_drag_locked,
								 m_reset);
	}

	uint64_t last = m_mochi.m_time;
	uint64_t time = ScePafMisc_256F9060[0];
	math::v2 moved = MaskMoveDir(move - rest, m_drag_dir);

	if (last != time) {
		m_mochi.m_points.push_back(MochiPoint(moved, time - last, false));
		if (m_mochi.m_points.size() >= 8)
			m_mochi.m_points.erase(m_mochi.m_points.begin(),
					       m_mochi.m_points.begin() + 1);
		m_mochi.m_time = time;
	}
}

void TouchScrollSupplier::UpdateDrag()
{
	if (m_touch_views.size() == 0)
		return;

	ScrollViewBase *view = m_touch_views.begin()->m_view;

	if (view->m_touch_captures.size() != 0 && !m_drag_locked &&
	    m_drag_time != ScePafMisc_256F9060[0]) {
		math::v2 pos = view->GetTrackPos();

		if (!IsSame(m_touch_pos, pos) && !m_drag_locked)
			Drag(m_touch_views.begin()->m_view->GetTrackPos() - m_touch_pos);
	}
}

bool ScrollViewBase::TrackEnd(Event *e)
{
	if (e != NULL && m_track_port != (uint8_t)e->m_detail)
		return false;
	s_touch_scroll_supplier.UpdateDrag();
	s_touch_scroll_supplier.StopFlick();
	return false;
}

bool TouchScrollSupplier::CaptureTouch(ScrollViewBase *view)
{
	if (m_touch_views.size() == 0 || m_touch_views.back().m_view != view)
		return false;

	ScrollViewBase *first = m_touch_views.begin()->m_view;

	if (first->m_touch_captures.size() != 0)
		return false;

	uint32_t id = first->m_track_port;
	Environment *env = first->m_env;

	if (id < 8 && env->m_touch_capture[0][id].m_widget != NULL)
		return false;
	ScePafWidget_2B1A1493(first, 1);
	ScePafWidget_D3D0A7C3(first, 0, first->m_track_port, true, true);
	return true;
}

bool ScrollViewBase::TrackPress(Event *e)
{
	if (e == NULL || m_track_port != (uint8_t)e->m_detail)
		return true;
	s_touch_scroll_supplier.CaptureTouch(this);
	return true;
}

bool ScrollViewBase::TrackLongPress(Event *e)
{
	if (e == NULL)
		return true;
	if (m_track_port == -1)
		TrackBegin(e);
	if (m_track_port != (uint8_t)e->m_detail)
		return true;
	s_touch_scroll_supplier.CaptureTouch(this);
	return true;
}

bool ScrollViewBase::TrackRelease(Event *e)
{
	return true;
}

bool TouchScrollSupplier::GrabTouch(ScrollViewBase *view)
{
	if (m_touch_views.size() == 0 || m_touch_views.back().m_view != view)
		return false;

	ScrollViewBase *first = m_touch_views.begin()->m_view;

	if (first->m_touch_captures.size() != 0)
		return false;
	if (!IsDragged())
		return false;
	ScePafWidget_D3D0A7C3(first, 0, first->m_track_port, true, true);
	if (!m_drag_locked)
		Drag(m_touch_views.begin()->m_view->GetTrackPos() - m_touch_pos);
	return true;
}

bool ScrollViewBase::TrackGrab(Event *e)
{
	if (e == NULL)
		return false;
	if (m_track_port == -1)
		return TrackBegin(e);
	if (m_track_port == (uint8_t)e->m_detail)
		return s_touch_scroll_supplier.GrabTouch(this);
	return false;
}

bool ScrollViewBase::TrackMove(Event *e)
{
	if (e == NULL || m_track_port != (uint8_t)e->m_detail)
		return false;
	if (m_touch_captures.size() == 0)
		return false;

	TouchScrollSupplier &supplier = s_touch_scroll_supplier;

	if (!supplier.m_drag_locked)
		supplier.Drag(supplier.m_touch_views.begin()->m_view->GetTrackPos() -
			      supplier.m_touch_pos);
	return true;
}

bool ScrollViewBase::TrackDrag(Event *e)
{
	if (m_track_port == -1)
		return false;

	TouchScrollSupplier &supplier = s_touch_scroll_supplier;

	if (!supplier.m_drag_locked)
		supplier.Drag(supplier.m_touch_views.begin()->m_view->GetTrackPos() -
			      supplier.m_touch_pos);
	return true;
}

bool ScrollViewBase::TrackPan(Event *e)
{
	if (m_track_port == -1)
		return false;

	/* the gesture's move, in the event's first two data words */
	math::v2 move = *(const math::v2 *)&e->m_data[0];
	TouchScrollSupplier &supplier = s_touch_scroll_supplier;

	if (!supplier.m_drag_locked) {
		supplier.m_touch_state = 2;
		supplier.m_drag_dir = MoveDir_XY;
		supplier.m_dragging = true;
		supplier.Drag(move * -1.0f);
		supplier.m_mochi.m_points.clear();
		supplier.m_mochi.m_time = ScePafMisc_256F9060[0];
		supplier.m_mochi.m_start_time = ScePafMisc_256F9060[0];
	}
	return true;
}

void ScrollViewBase::ScrollToRect(const math::rect &rect, Timer *timer)
{
	if (!m_ime_padding) {
		if (timer != NULL)
			delete timer;
		return;
	}

	math::v4 size;

	if (m_scene != NULL) {
		size = *m_scene->GetSize(0);
	} else {
		math::rect screen(Screen::s_root_screen->sp);

		size = math::v4(screen.width(), screen.height(), 0.0f, 0.0f);
	}

	/* the part of the screen the IME leaves free */
	math::v4 cam = GetCamera()->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::rect free(0.0f, m_ime_height * 0.5f - 16.0f + cam.extract_y(), size.extract_x(),
			size.extract_y() - m_ime_height - 32.0f);
	math::rect clip = CalcClipRect();
	math::rect visible = ScePafMisc_B2795FE2(free, clip);
	float offset = 0.0f;

	if (visible.height() > rect.height()) {
		if (visible.bottom() > rect.bottom())
			offset = rect.bottom() - visible.bottom();
		if (visible.top() < rect.top())
			offset = rect.top() - visible.top();
	} else {
		if (visible.bottom() < rect.bottom())
			offset = rect.bottom() - visible.bottom();
		if (visible.top() > rect.top())
			offset = rect.top() - visible.top();
	}

	math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	SetViewFocus(math::v2(pos.getImpl().getXY()) * -1.0f + math::v2(0.0f, offset), timer, true,
		     true);
}

void ScrollViewBase::UpdatePadding()
{
	m_padding_used = m_padding;
	if (m_ime_padding) {
		math::v4 size = *m_scene->GetSize(0);
		math::v4 cam = GetCamera()->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::rect free(0.0f, m_ime_height * 0.5f - 16.0f + cam.extract_y(),
				size.extract_x(), size.extract_y() - m_ime_height - 32.0f);
		math::rect clip = CalcClipRect();
		float top = clip.top() - free.top();

		if (top > 0.0f && m_padding_used.extract_y() < top)
			m_padding_used.set_y(top);

		float bottom = free.bottom() - clip.bottom();

		if (bottom > 0.0f && m_padding_used.extract_w() < bottom)
			m_padding_used.set_w(bottom);
	}

	m_padding_sign = math::v4(sce::Vectormath::Simd::Aos::mulPerElem(
		m_padding_used.getImpl(), math::v4(-1.0f, 1.0f, 1.0f, -1.0f).getImpl()));
	m_scroll_box = m_view_box + m_padding_sign;

	sce::Vectormath::Simd::Aos::Vector4 box = m_scroll_box.getImpl();

	m_scroll_box_size = math::v2(sce::Vectormath::Simd::Aos::Vector2(
		box.getZ() - box.getX(), box.getY() - box.getW()));
	m_scroll_center = math::v2(sce::Vectormath::Simd::Aos::Vector2(
		box.getZ() + box.getX(), box.getY() + box.getW())) * -0.5f;
}

inline void ScrollViewBase::UpdateImeLayout()
{
	UpdatePadding();
	UpdateViewLimit();
	UpdateMovableDirection();
	UpdateScrollLayout();
	UpdateScrollRange();
	ScePafWidget_A0538225(-1);
}

void ScrollViewBase::SetImePadding(bool on)
{
	if (m_ime_padding != on) {
		m_ime_padding = on;
		UpdateImeLayout();
	}
	if (on)
		m_scroll_ended = GetCamera()->FindAnim(0x10000) != NULL;
}

bool ScrollViewBase::OnEditBegin(Event *e)
{
	if (e == NULL)
		return false;
	if (!m_scroll_lock)
		return false;

	SetImePadding(true);
	if (m_ime_height != 0.0f && e->GetDetail() != 0) {
		math::rect clip = ((Widget *)e->GetDetail())->CalcClipRect();

		ScrollToRect(clip, NULL);
	}
	return true;
}

bool ScrollViewBase::OnEditEnd(Event *e)
{
	if (e == NULL)
		return false;
	if (!m_scroll_lock)
		return false;

	SetImePadding(false);
	return true;
}

void ScrollViewBase::SetImeRect(const math::rect &rect)
{
	if (m_ime_height != rect.height()) {
		m_ime_height = rect.height();
		UpdateImeLayout();
	}
}

bool ScrollViewBase::OnImeEvent(Event *e)
{
	if (e == NULL)
		return false;
	if (!m_scroll_lock)
		return false;

	bool scroll = true;
	bool animate = false;
	/* the text box editing */
	Widget *widget = (Widget *)e->GetDetail();

	switch (e->GetType()) {
	case TextBoxImeListener::EVENT_IME_OPEN:
	case TextBoxImeListener::EVENT_IME_CHANGE_SIZE:
		SetImeRect(math::rect((float)e->m_data[0], (float)e->m_data[1], (float)e->m_data[2],
				      (float)e->m_data[3]));
		animate = true;
		if (widget != NULL && widget->IsInherit(TextBox::TypeName())) {
			bool mode;

			((TextBox *)widget)->GetEditAutoTransitionMode(mode);
			if (mode)
				scroll = false;
		}
		m_scroll_ended = GetCamera()->FindAnim(0x10000) != NULL;
		if (!scroll)
			break;
		/* fall through */
	case TextBoxImeListener::EVENT_IME_EDIT:
		if (widget != NULL) {
			Timer *timer = NULL;

			if (animate)
				timer = new Timer(m_snap_time, Timer::FUNC_EXPONENT);

			math::rect clip = widget->CalcClipRect();

			ScrollToRect(clip, timer);
		}
		break;
	}
	e->Consume();
	return true;
}

/* a key event that presses the key or repeats it (event types 17 and 19) */
static inline bool IsKeyPress(Event *e)
{
	if (e == NULL)
		return false;

	uint32_t type = e->GetType();

	return type == 17 || type == 19;
}

bool ScrollViewBase::OnUpKey(Event *e)
{
	if (m_track_port != -1)
		return false;
	if (IsKeyPress(e))
		return ScePafWidget_AD706F2A(math::v2(0.0f, 1.0f), NULL);
	return false;
}

bool ScrollViewBase::OnDownKey(Event *e)
{
	if (m_track_port != -1)
		return false;
	if (IsKeyPress(e))
		return ScePafWidget_AD706F2A(math::v2(0.0f, -1.0f), NULL);
	return false;
}

bool ScrollViewBase::OnLeftKey(Event *e)
{
	if (m_track_port != -1)
		return false;
	if (IsKeyPress(e))
		return ScePafWidget_AD706F2A(math::v2(-1.0f, 0.0f), NULL);
	return false;
}

bool ScrollViewBase::OnRightKey(Event *e)
{
	if (m_track_port != -1)
		return false;
	if (IsKeyPress(e))
		return ScePafWidget_AD706F2A(math::v2(1.0f, 0.0f), NULL);
	return false;
}

/* the focus of the widget's focus root, NULL for none (the focus root's m_state holds it) */
static inline Widget *GetRootFocus(Widget *widget)
{
	Widget *root = ScePafWidget_CFA1EB7A(widget);

	if (root == NULL)
		return NULL;
	return root->m_focus_root != NULL ? (Widget *)root->m_focus_root->m_state : NULL;
}

void ReleaseRootFocus(Widget *widget)
{
	Widget *focus = widget->m_env->m_focus_cursor.m_target;
	Widget *root_focus = GetRootFocus(widget);

	if (focus != root_focus)
		root_focus->ReleaseFocus();
}

/* the focus search's direction for the move (FocusSearchInfo::m_dir; 255 for none) */
static inline int32_t GetFocusDir(const math::v2 &dir)
{
	math::v2 d = dir;
	int32_t ret = 255;

	if (d.extract_x() > 0.0f)
		ret = 4;
	if (d.extract_x() < 0.0f)
		ret = 3;
	if (d.extract_y() > 0.0f)
		ret = 1;
	if (d.extract_y() < 0.0f)
		ret = 2;
	return ret;
}

Widget *ScrollViewBase::SearchFocus(const math::v2 &dir, Widget *from)
{
	math::v2 move = dir;

	if (m_env == NULL)
		return NULL;

	Widget *origin = from;

	if (origin == NULL) {
		origin = m_env->m_focus_cursor.m_target;
		if (origin == NULL || FindChild(origin, 0) == NULL)
			origin = GetRootFocus(this);
		if (!(dir.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f) &&
		      dir.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f)) &&
		    FindChild(origin, 0) == NULL)
			return NULL;
		if (origin == NULL)
			origin = this;
	}

	FocusSearchInfo info(255);

	info.m_dir = GetFocusDir(dir);
	ScePafWidget_EA117CF8(&info, origin);

	math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	if (LimitFocus(math::v2(pos.getImpl().getXY()) - dir, false).m_dir == 0 &&
	    ((move.extract_x() != 0.0f && (m_movable_dir & MoveDir_X) != 0) ||
	     (move.extract_y() != 0.0f && (m_movable_dir & MoveDir_Y) != 0)))
		/* the view can still scroll that way */
		info.m_enabled = false;

	bool searching = false;

	if (m_focus_searching)
		searching = true;
	else
		m_focus_searching = true;

	Widget *found;

	if (from != NULL && FindChild(from, 0) == NULL)
		found = SetFocusOption(&info, 1);
	else
		found = origin->SetFocusOption(&info, 0);
	if (!searching)
		m_focus_searching = false;
	if (found == this)
		found = NULL;
	return found;
}

bool ScrollViewBase::OnFocusIn(Event *e)
{
	math::v2 dir = m_focus_target;
	/* the widget the focus comes from */
	Widget *found = SearchFocus(dir, (Widget *)e->m_data[0]);

	if (found != NULL && found != this &&
	    (ScePafWidget_CFA1EB7A(found) == this ||
	     FindChild(ScePafWidget_CFA1EB7A(found), 0) != NULL)) {
		Widget *target = found;

		if (ScePafWidget_CFA1EB7A(found) != this)
			target = ScePafWidget_CFA1EB7A(this);

		SetFocusInfo info;

		info.m_mode = 0;
		info.m_notify = m_env != NULL && m_env->m_focus_cursor.m_target == this;
		info.m_direction = math::v2(0.0f, 0.0f);
		info.m_keep = false;
		target->SetFocus(&info);
		m_focus_start = m_focus_target;
		m_focus_target = math::v2(0.0f, 0.0f);
		if (m_focus_root != NULL)
			m_focus_root->m_focus_moved = 0;
		if (!m_focus_in_group)
			ReleaseRootFocus(this);
	}
	return false;
}

bool ScrollViewBase::OnFocusEnter(Event *e)
{
	return false;
}

bool ScrollViewBase::OnFocusLeave(Event *e)
{
	return false;
}

bool ScrollViewBase::OnFocusOut(Event *e)
{
	if (e == NULL)
		return false;

	if ((Widget *)e->m_data[0] != this && m_movable_dir != MoveDir_None &&
	    !(m_focus_target.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f) &&
	      m_focus_target.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f))) {
		math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		bool movable = false;

		if ((m_focus_target.extract_x() != 0.0f && (m_movable_dir & MoveDir_X) != 0) ||
		    (m_focus_target.extract_y() != 0.0f && (m_movable_dir & MoveDir_Y) != 0))
			movable = true;
		if (LimitFocus(math::v2(pos.getImpl().getXY()) - m_focus_target, false).m_dir != 0 ||
		    !movable) {
			m_focus_start = m_focus_target;
			m_focus_target = math::v2(0.0f, 0.0f);
		} else {
			/* the view moves on in the direction */
			SetFocusInfo info;

			info.m_mode = 0;
			info.m_notify = false;
			info.m_direction = m_focus_target;
			info.m_keep = false;
			SetFocus(&info);
		}
	}

	ScrollViewBase *parent = GetParentView();

	if (parent != NULL)
		parent->OnFocusOut(e);
	return true;
}

bool ScrollViewBase::OnFlick(Event *e)
{
	if (e == NULL || m_track_port != (e->m_detail & 0xFF))
		return false;

	/* the flick's velocity, in the event's last two data words */
	math::v2 velocity = *(const math::v2 *)&e->m_data[2];
	TouchScrollSupplier &supplier = s_touch_scroll_supplier;

	if (supplier.m_touch_views.size() != 0) {
		ScrollViewBase *view = supplier.m_touch_views.begin()->m_view;

		if (view->m_touch_captures.size() == 0) {
			supplier.m_drag_time = ScePafMisc_256F9060[0];
			ScePafWidget_D3D0A7C3(view, 0, view->m_track_port, true, true);

			math::v2 move = velocity * (Timer::s_tick_unit * Timer::s_tick_interval);

			supplier.BeginDrag(move, false);
			supplier.Drag(move);
		}
	}
	return true;
}

bool ScrollViewBase::TakeTrackedGesture(Event *e)
{
	return e != NULL && m_track_port == (e->m_detail & 0xFF);
}

void TouchScrollSupplier::BeginMochi(uint32_t dir, const math::v2 &velocity, float scale)
{
	for (vector<TouchView>::iterator it = m_touch_views.begin(); it != m_touch_views.end();
	     ++it)
		it->m_id = -1;

	uint32_t left = dir;

	for (int32_t i = m_touch_views.size() - 1; i >= 0; i--) {
		TouchView &touch_view = m_touch_views.at(i);

		touch_view.m_id = -1;

		ScrollViewBase *view = touch_view.m_view;
		uint32_t d = view->m_movable_dir & left;

		if (d != ScrollViewBase::MoveDir_None) {
			if (view->m_bar_controller != NULL) {
				if ((d & ScrollViewBase::MoveDir_X) != 0)
					view->m_bar_controller->ShowJetTail(view->m_horizontal_bar, true);
				if ((d & ScrollViewBase::MoveDir_Y) != 0)
					view->m_bar_controller->ShowJetTail(view->m_vertical_bar, true);
			}
			left &= ~d;
		}
	}
	m_touch_state = 2;
	m_mochi_scale = scale;
	m_drag_dir = dir;
	m_drag_locked = true;
	m_mochi_velocity = velocity;
	m_time = get_time_internal();
}

bool ScrollViewBase::MochiBegin(Event *e)
{
	if (e == NULL || m_track_port != (e->m_detail & 0xFF))
		return false;

	math::v2 velocity = s_touch_scroll_supplier.m_mochi.GetVelocity();
	float scale = 1.0f;

	if (m_touch_captures.size() == 0)
		ScePafWidget_D3D0A7C3(this, 0, m_track_port, true, true);

	uint32_t dir = MoveDir_XY;
	inputdevice::InputGestureListener *input = m_env->GetTouchGestureListener(0).get();
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(m_track_port, 50, this)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() != NULL) {
		DragGestureParam param;

		((DragGestureListener *)listener.get())->GetParam(&param);
		if (param.m_dir == 0)
			dir = MoveDir_X;
		else if (param.m_dir == 1)
			dir = MoveDir_Y;
		scale = DragFlickParam::Get().m_value[3];
	}
	s_touch_scroll_supplier.BeginMochi(dir, velocity, scale);
	return true;
}

}
}
