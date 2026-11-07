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
#include <widget/core/layouter.h>
#include <widget/core/uiinputlistener.h>
#include <widget/factory.h>
#include <widget/w_scroll_bar.h>
#include <widget/w_scroll_view_base.h>

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
	math::v2 m_move;
	uint64_t m_time;
	uint32_t m_last : 1;
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
		if (!back.m_last)
			last = back.m_move * 1000000.0f / (float)back.m_time;

		math::v2 velocity = last;
		if (m_points.size() >= 2) {
			MochiPoint &prev = m_points.at(m_points.size() - 2);
			math::v2 before(0.0f, 0.0f);
			if (!prev.m_last)
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
				if (point.m_last)
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
	math::v2 m_start;
	math::v2 m_pos;
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

		ScrollViewBase *m_view;
		int32_t m_id;
	};

	/* the views the touches hold */
	vector<TouchView> m_touch_views;
	/* the ID the next snap point takes */
	int32_t m_next_snap_id;
	uint8_t reserved0[4];
	/* the moves of the mochi gesture */
	MochiHistory m_mochi;
	vector<ScrollViewBase *> m_views;
	common::SharedPtr<inputdevice::InputListener> m_listener;
	uint8_t reserved1[8];
	/* where the drag started */
	math::v2 m_drag_start;
	bool m_dragging;
	bool m_drag_locked;
	bool m_reset;
	/* what the touch does (2 drags, 3 moves no view) */
	int32_t m_touch_state;
	uint8_t reserved2[0x10];
	uint64_t m_time;
	math::v2 m_flick_velocity;
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
	  m_anchor(NULL), m_enable_analog(true), m_enable_touch(true), m_snap_power(25.0f),
	  m_preferred_snap_id(-1), m_snap_time(200.0f), m_key_step(4.0f),
	  m_over_scroll_length(20.0f), m_bar_fade_time(200.0f), m_bar_reset(true),
	  m_scroll_touch(true), m_scroll_key(true), m_scroll_analog(true), m_scroll_lock(false),
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
				if (ScePafWidget_2FDBFE08(e))
					e->Consume();

				Event end(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);
				DoEvent(CB_SCROLL_VIEW_BASE_END, &end);
				break;
			}
			case 7:
				if (ScePafWidget_CEBDCDB3(e))
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
				if (ScePafWidget_E0FAE144(e))
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
			if (ScePafWidget_E6DD4083(e))
				e->Consume();
			break;
		case 2:
			if (ScePafWidget_F8E68E31(e))
				e->Consume();
			break;
		case 3:
			if (ScePafWidget_422E30D5(e))
				e->Consume();
			break;
		case 4:
			if (ScePafWidget_0F056788(e))
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
			if (ScePafWidget_33742E18(e))
				e->Consume();
			break;
		case 0x402:
			if (ScePafWidget_797EE334(e))
				e->Consume();
			break;
		case 0x403:
			if (ScePafWidget_7B155BE4(e))
				e->Consume();
			break;
		case 0x404:
			if (ScePafWidget_08D85CB4(e))
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
		case 0x8001:
			if (ScePafWidget_4770835D(e))
				e->Consume();
			break;
		case 0x8002:
			if (ScePafWidget_1DA7A46A(e))
				e->Consume();
			break;
		case 0x8003:
		case 0x8004:
		case 0x8005:
			if (ScePafWidget_423AF7BA(e))
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

}
}
