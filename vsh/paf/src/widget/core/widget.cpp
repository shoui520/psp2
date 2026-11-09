/*
 * paf::ui::Widget, as far as it is converted: the rest of its functions are
 * still ScePafWidget's.
 */
#include <stddef.h>
#include <common/common_internal.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/panel_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <timer/timerqueue.h>
#include <widget/core/animlistener.h>
#include <widget/core/layouter.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>

namespace paf {

/* Sony's build calls the draw objects' push_back rather than inlining it. */
template<>
inline __attribute__((noinline)) int vector<ui::Widget::DrawObjEntry>::push_back(
	const ui::Widget::DrawObjEntry &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) ui::Widget::DrawObjEntry(val);
		result = ++count;
	}
	return result;
}

/* Sony's SetAnimListener has the lookup in the animation map inline. */
template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<int32_t, ui::AnimQueue *, less<int32_t> > >::iterator
_Tree<_Tmap_traits<int32_t, ui::AnimQueue *, less<int32_t> > >::find(const int32_t &key)
{
	return iterator(_Find(key));
}

/* Sony's RemoveChild has the unlinking inline, and its DoEvent the lookup in the listener map. */
template<>
inline __attribute__((always_inline)) list<ui::Widget *>::iterator
list<ui::Widget *>::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;

	node->next->prev = node->prev;
	node->prev->next = node->next;
	destroy(&node->value);
	operator delete(node);
	--count;
	return iterator(next);
}

template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<int32_t, ui::EventListenerList *, less<int32_t> > >::iterator
_Tree<_Tmap_traits<int32_t, ui::EventListenerList *, less<int32_t> > >::find(const int32_t &key)
{
	return iterator(_Find(key));
}

/* Sony's transition timer functions have the lookup in the widget timer maps inline. */
template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<ui::Widget *, Timer *, less<ui::Widget *> > >::iterator
_Tree<_Tmap_traits<ui::Widget *, Timer *, less<ui::Widget *> > >::find(ui::Widget *const &key)
{
	return iterator(_Find(key));
}

namespace ui {

/* the cross product of the first three components, the first vector's w kept (Sony's copy is
   out of line; name ours) */
static __attribute__((noinline)) math::v4 CrossKeepW(const math::v4 &a, const math::v4 &b)
{
	return math::v4(sce::Vectormath::Simd::Aos::Vector4(
		sce::Vectormath::Simd::Aos::cross(a.getImpl().getXYZ(), b.getImpl().getXYZ()),
		a.extract_w()));
}

/* the product of two rotations (Sony's copy is out of line; name ours) */
static __attribute__((noinline)) math::quaternion QuatMul(const math::quaternion &a, const math::quaternion &b)
{
	return math::quaternion(a.getImpl() * b.getImpl());
}

/* the quaternion with unit length, as Sony's copy of the vector length and reciprocal has it
   (name ours) */
static inline math::quaternion NormalizeQuat(const math::quaternion &q)
{
	math::v4 v = math::v4(sce::Vectormath::Simd::Aos::Vector4(q.getImpl())).unit();

	return math::quaternion(sce::Vectormath::Simd::Aos::Quat(v.getImpl()));
}

/* NotifyLayoutDirty and NotifyAnimatorDirty, as Sony's callers have them inline (names ours) */
static inline void LayoutDirty(Widget *w, uint32_t kind)
{
	if (((uint8_t)kind & w->m_layout_notify_mask) != 0 &&
	    (kind & 0xFF00 & w->m_layout_notify_mask) != 0) {
		if (w->m_parent != NULL && w->m_parent->m_layouter != NULL)
			w->m_parent->m_layouter->SetDirty(1);
		if (kind & 2) {
			if (w->m_layouter != NULL)
				w->m_layouter->SetDirty(1);
		}
	}
}

static inline void AnimatorDirty(Widget *w, uint32_t kind)
{
	if (((uint8_t)kind & w->m_animator_notify_mask) != 0 &&
	    (kind & 0xFF00 & w->m_animator_notify_mask) != 0) {
		if (w->m_animator != NULL)
			w->m_animator->SetDirty(1);
	}
}

AnimQueue *Widget::FindAnim(int32_t id)
{
	for (map<int32_t, AnimQueue *>::iterator it = m_anims.begin(); it != m_anims.end(); it++) {
		if ((it->first & 0xFFFFFF) == id) {
			AnimQueue *anim = it->second;

			for (int i = 0; i < 3; i++) {
				AnimQueueBase *queue = anim->m_queue[i];

				if (queue != NULL && !queue->IsFinished())
					return it->second;
			}
		}
	}
	return NULL;
}

math::v4 *Widget::GetSize(int32_t opt)
{
	if (opt == 0)
		return &m_size;

	bool front = opt == 2;
	AnimQueue *anim = FindAnim(ANIM_SIZE);

	if (anim == NULL)
		return &m_size;

	AnimQueueBase *queue = anim->m_queue[0];
	math::v4 *result = NULL;

	if (front) {
		if (queue != NULL) {
			AnimListener *listener = static_cast<AnimListener *>(queue->GetFront());

			if (listener != NULL)
				result = listener->GetTargetV4();
		}
	} else {
		if (queue != NULL) {
			AnimListener *listener = static_cast<AnimListener *>(queue->GetBack());

			if (listener != NULL)
				result = listener->GetTargetV4();
		}
	}
	if (result == NULL)
		return &m_size;
	return result;
}


const char Widget::m_widget_type[] = "widget_base";

bool Widget::IsInherit(const char *type) const
{
	return m_widget_type == type;
}

char *Widget::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Widget::TypeName()
{
	return m_widget_type;
}

void Widget::CountPrepareChildren(int32_t delta)
{
	int32_t count = m_prepare_count + delta;

	if (m_prepare_count != 0 && count == 0) {
		Event e(0x1008, this, 0x40000, 0);

		m_env->m_ev_queue.PushBack(e);
	}
	if (count >= 0) {
		if (count <= 0xFFFF)
			m_prepare_count = count;
		else
			m_prepare_count = 0xFFFF;
	} else {
		m_prepare_count = 0;
	}
	if (m_parent != NULL && static_cast<Widget *>(m_scene) != this)
		m_parent->CountPrepareChildren(delta);
}

void Widget::NotifyAnimatorDirty(uint32_t kind)
{
	AnimatorDirty(this, kind);
}

uint32_t Widget::ChangeState(uint32_t state, int32_t ev_type)
{
	uint32_t old = m_state;

	if (old != state && old != 2) {
		m_state = state;
		Widget *parent = m_parent;

		AnimatorDirty(this, 0x10FF);
		if (parent != NULL) {
			if (old == 3)
				parent->CountPrepareChildren(-1);
			else if (state == 3)
				parent->CountPrepareChildren(1);
		}

		int32_t info = -1;

		if (ev_type < 0) {
			if (state > 1) {
				if (state <= 2) {
					ev_type = 0x1002;
				} else if (state <= 3) {
					info = 0x20100003;
					ev_type = 0x1004;
				}
			} else if (state != 0) {
				ev_type = 0x1001;
			} else {
				info = 0x20100004;
				ev_type = 0x1003;
			}
		}
		if (m_env != NULL && ev_type > 0) {
			Event e(ev_type, this, 0x40000, 0);

			m_env->m_ev_queue.PushBack(e);
		}
		if (info > 0) {
			Event e(ev_type, 0x100000, 0x10000, 0);

			DoEvent(info, &e);
		}
	}
	return 0;
}

void Widget::NotifyLayoutDirty(uint32_t kind)
{
	LayoutDirty(this, kind);
}

ResolutionData *Widget::get_resolution_data(ResolutionData *array, uint32_t num, uint32_t id) const
{
	if (array == NULL || num == 0)
		return NULL;

	SceneResolution *resolution = ScePafWidget_CDC90BA2(m_scene->m_env_param, id);

	if (resolution == NULL)
		return array;

	uint32_t i;

	for (i = 0; i < num; i++) {
		if (array[i].m_id == resolution->m_id)
			goto found;
	}
	for (i = 0; i < num; i++) {
		if (array[i].m_id == resolution->m_base_id) {
found:
			ResolutionData *entry = &array[i];

			if (entry != NULL)
				return entry;
			return array;
		}
	}
	return array;
}

void Widget::SetUpdateParam(uint32_t param)
{
	uint32_t flags = m_update_flag;

	if ((param & flags) == 0) {
		if (param == 1) {
			m_update_flag = flags | 1;
			SetUpdateParam(2);
		} else if (param == 2) {
			m_update_flag = flags | 2;
			for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
				(*it)->SetUpdateParam(2);
		}
	}
}

/* the animation's last target, where one is running (inline in Sony's callers, name ours) */
static inline math::v4 *AnimLastTarget(AnimQueue *anim)
{
	if (anim != NULL) {
		AnimQueueBase *queue = anim->m_queue[0];

		if (queue != NULL) {
			AnimListener *listener = static_cast<AnimListener *>(queue->GetBack());

			if (listener != NULL)
				return listener->GetTargetV4();
		}
	}
	return NULL;
}

math::v4 Widget::GetAlignOffsetToCenter(int32_t opt)
{
	if (opt != 1)
		return m_align_offs;

	math::v4 offs = m_align_offs;
	math::v4 *target = AnimLastTarget(FindAnim(ANIM_ALIGN));

	if (target != NULL)
		offs = *target;
	target = AnimLastTarget(FindAnim(ANIM_SIZE));
	if (target != NULL)
		offs = calc_align_offset(*target, offs, m_align_x, m_align_y, m_align_z);
	return math::v4(offs);
}

/* GetAlignOffsetToRotatedCenter as Sony's SetPos_core has it inline (name ours) */
static inline math::v4 AlignOffsetToRotatedCenter(Widget *w, int32_t opt)
{
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();

	math::v4 center = w->GetAlignOffsetToCenter(opt);
	float32x4_t c0 = w->m_local_matrix.x().getImpl().get128();
	float32x4_t c1 = w->m_local_matrix.y().getImpl().get128();
	float32x4_t c2 = w->m_local_matrix.z().getImpl().get128();
	float32x4_t c3 = w->m_local_matrix.w().getImpl().get128();
	float32x4_t offs = center.getImpl().get128();
	float32x2_t low = vget_low_f32(offs);
	float32x2_t high = vget_high_f32(offs);
	float32x4_t a = vmlaq_lane_f32(vmulq_lane_f32(c0, low, 0), c2, high, 0);
	float32x4_t b = vmlaq_lane_f32(vmulq_lane_f32(c1, low, 1), c3, high, 1);

	return math::v4(sce::Vectormath::Simd::Aos::Vector4(vaddq_f32(a, b)));
}

math::v4 Widget::GetAlignOffsetToRotatedCenter(int32_t opt)
{
	return AlignOffsetToRotatedCenter(this, opt);
}

int32_t Widget::SetPos_core(const math::v4 &pos, uint32_t notify_layout, bool store)
{
	math::v4 value = pos;

	value.set_w(1.0f);
	if ((m_align_x | m_align_y | m_align_z) == 0 &&
	    m_align_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
		if (m_pos == value)
			return 0;
		m_pos = value;
	} else {
		value = value + AlignOffsetToRotatedCenter(this, 0);
		if (m_pos == value)
			return 0;
		m_pos = value;
	}
	LayoutDirty(this, notify_layout);
	SetUpdateParam(1);
	if (store) {
		if (m_pos_array != NULL) {
			ResolutionData *entry = get_resolution_data(m_pos_array, m_pos_num,
								    m_resolution_id);

			if (entry != NULL) {
				entry->m_value[0] = m_pos.extract_x();
				entry->m_value[1] = m_pos.extract_y();
				entry->m_value[2] = m_pos.extract_z();
			}
		}
	}
	return 0;
}

Camera *Widget::GetCamera() const
{
	if (m_scene != NULL)
		return m_scene->m_camera;
	return NULL;
}

/* an animation listener kept for a widget that defers them (names ours) */
class AnimPending {
public:
	Widget *m_widget;
	int32_t m_id;
	int32_t m_slot;
	AnimListener *m_listener;
};

extern "C" {
list<AnimPending> ScePafWidget_8A0DA7BF;
}

/* the timers of the widgets that hide and are destroyed when theirs runs out, and those of the
   widgets that show (names ours) */
static map<Widget *, Timer *> s_destroy_timers;
static map<Widget *, Timer *> s_show_timers;

/* the widget's timer in the map taken out and deleted, if it has one (inline in Sony's callers;
   name ours) */
static inline void DropTimer(map<Widget *, Timer *> &timers, Widget *widget)
{
	map<Widget *, Timer *>::iterator it = timers.find(widget);

	if (it != timers.end()) {
		delete it->second;
		timers.erase(it);
	}
}

extern "C" void ScePafWidget_C7962202(Widget *widget, bool on, bool force)
{
	if (widget->m_timer_pending) {
		if (!force)
			return;
		DropTimer(s_destroy_timers, widget);
		widget->m_timer_pending = false;
	}
	if (force || widget->m_transition_in != on) {
		widget->m_transition_in = on;
		for (list<Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); it++)
			ScePafWidget_C7962202(*it, on, force);
	}
}

extern "C" void ScePafWidget_83214DB9(Widget *widget, bool on, bool force)
{
	if (widget->m_timer_pending) {
		if (!force)
			return;

		bool was_set = widget->m_destroy_protected;

		DropTimer(s_destroy_timers, widget);
		widget->m_timer_pending = false;
		if (was_set)
			Widget::s_destroying_count--;
	}
	if (force || widget->m_destroy_protected != on) {
		if (on) {
			if (widget->m_focused)
				widget->ReleaseFocus();

			Widget *owner = widget;

			while (owner != NULL) {
				FocusRoot *root = owner->m_focus_root;

				if (root != NULL) {
					if (root->m_init_focus == widget)
						ScePafWidget_EC6175D6(widget, false);
					break;
				}
				if (widget->m_scene == owner)
					break;
				owner = owner->m_parent;
			}
		}
		widget->m_destroy_protected = on;
		for (list<Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); it++)
			ScePafWidget_83214DB9(*it, on, force);
	}
}

extern "C" void ScePafWidget_78C4DBA4(Widget *widget)
{
	if (widget->m_hold_count > 0)
		return;
	if (widget->m_transition_out) {
		if (widget->m_prepare_count != 0)
			return;
		widget->m_transition_out = false;
	}
	if (!widget->m_timer_pending)
		return;

	map<Widget *, Timer *>::iterator it = s_destroy_timers.find(widget);

	if (it == s_destroy_timers.end()) {
		widget->m_timer_pending = false;
		return;
	}

	Timer *timer = it->second;

	if (timer->m_state == Timer::State_Finished) {
		delete timer;
		s_destroy_timers.erase(it);
		widget->m_timer_pending = false;
		if (widget->m_transition_in) {
			ScePafWidget_C7962202(widget, false, false);
		} else if (widget->m_destroy_protected) {
			ScePafWidget_83214DB9(widget, false, false);
			Widget::s_destroying_count--;
			widget->DestroyWidget();
		}
	} else if (!widget->m_transition_in && !widget->m_destroy_protected) {
		delete timer;
		s_destroy_timers.erase(it);
		widget->m_timer_pending = false;
	}
}

extern "C" int32_t ScePafWidget_8AF26B1F(Widget *widget, bool auto_delete, float time)
{
	if (widget->m_hold_count > 0)
		return -1;
	if (widget->m_destroy_protected)
		return -1;
	ScePafWidget_C7962202(widget, true, false);
	widget->m_timer_pending = true;
	widget->m_transition_out = auto_delete;
	s_destroy_timers[widget] = new Timer(time);
	return 0;
}

extern "C" int32_t ScePafWidget_5A9793B0(Widget *widget, float time)
{
	if (widget->m_hold_count > 0)
		return -1;
	if (widget->m_destroy_protected)
		return -1;
	if (widget->m_transition_in || widget->m_transition_out) {
		widget->m_transition_out = false;
		ScePafWidget_C7962202(widget, false, true);
	}
	ScePafWidget_83214DB9(widget, true, false);
	widget->m_timer_pending = true;
	s_destroy_timers[widget] = new Timer(time);
	Widget::s_destroying_count++;
	return 0;
}

extern "C" int32_t ScePafWidget_AC0C81D5(Widget *widget, float time)
{
	Timer *timer = new Timer(time);

	if (widget->m_show_timer_pending) {
		map<Widget *, Timer *>::iterator it = s_show_timers.find(widget);

		if (it != s_show_timers.end()) {
			delete it->second;
			it->second = timer;
			return 0;
		}
	}
	s_show_timers[widget] = timer;
	widget->m_show_timer_pending = true;
	if (widget->m_show_hold == 0)
		widget->ReleaseShowHolds(true);
	widget->m_show_hold = widget->m_show_hold + 1;
	return 0;
}

extern "C" void ScePafWidget_CEA2BE4E(void)
{
	map<Widget *, Timer *>::iterator it = s_destroy_timers.begin();

	while (it != s_destroy_timers.end()) {
		it->second->DoTick();
		it++;
	}
}

void Widget::UpdateShowTimer()
{
	Widget *widget = this;

	if (!widget->m_show_timer_pending || widget->m_hold_count > 0)
		return;
	if (widget->m_transition_in || widget->m_transition_out)
		return;

	map<Widget *, Timer *>::iterator it = s_show_timers.find(widget);

	if (it == s_show_timers.end()) {
		widget->m_show_timer_pending = false;
		return;
	}

	Timer *timer = it->second;

	timer->DoTick();
	if (timer->m_state == Timer::State_Finished) {
		delete timer;
		s_show_timers.erase(it);
		widget->m_show_timer_pending = false;
		if (widget->m_show_hold != 0)
			widget->m_show_hold = widget->m_show_hold - 1;
	}
}

/* a move set by hand is dropped while a drag is under way (inline in Sony's callers; name
   ours) */
static inline bool IsDragMoving(const Widget *w)
{
	return w->m_drag_state[0] != NULL && w->m_drag_mode != 0 && w->m_drag_mode != 0x1000;
}

void Widget::UpdateShowAlphaSum(float parent_sum)
{
	m_show_a_sum = m_show_alpha * parent_sum;
	if (m_children.size() != 0) {
		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
			(*it)->UpdateShowAlphaSum(m_show_a_sum);
	}
}

/* SetShowAlpha_core as Sony's SetShowAlpha has it inline (name ours) */
static inline void ShowAlphaCore(Widget *w, float alpha)
{
	Widget *parent = w->m_parent;

	if (alpha >= 0.0f) {
		w->m_show_alpha = alpha;
		if (alpha > 1.0f)
			w->m_show_alpha = 1.0f;
	}
	w->UpdateShowAlphaSum(parent != NULL ? parent->m_show_a_sum : 1.0f);
}

void Widget::SetShowAlpha_core(float alpha)
{
	ShowAlphaCore(this, alpha);
}

int32_t Widget::KillAnimType(int32_t type)
{
	int32_t count = 0;

	for (map<int32_t, AnimQueue *>::iterator it = m_anims.begin(); it != m_anims.end(); it++)
		count += it->second->EraseType(type, true);
	return count;
}

int32_t Widget::SetAnimListener(int32_t id, AnimListener *listener, int32_t slot)
{
	if (listener == NULL)
		return -1;
	if (m_hold_count > 0) {
		delete listener;
		return -1;
	}
	if (m_anim_deferred) {
		AnimPending pending;

		pending.m_widget = this;
		pending.m_id = id;
		pending.m_slot = slot;
		pending.m_listener = listener;
		ScePafWidget_8A0DA7BF.push_back(pending);
		return 0;
	}
	if ((id & 0xFF000000) != 0) {
		if ((id & 0x1000000) != 0)
			listener->m_keep_x = true;
		if ((id & 0x2000000) != 0)
			listener->m_keep_y = true;
		if ((id & 0x4000000) != 0)
			listener->m_keep_z = true;
		if ((id & 0x8000000) != 0)
			listener->m_keep_w = true;
	}

	AnimQueue *anim;
	map<int32_t, AnimQueue *>::iterator it = m_anims.find(id);

	if (it == m_anims.end()) {
		anim = new AnimQueue();
		m_anims.insert(pair<int32_t, AnimQueue *>(id, anim));
	} else {
		anim = it->second;
	}

	uint32_t slot_index = 0;

	switch (slot & 0xFF00) {
	case 0x100:
		slot_index = 1;
		break;
	case 0x200:
		slot_index = 2;
		break;
	default:
		slot_index = 0;
		break;
	}
	if (anim != NULL) {
		uint32_t mode = (uint8_t)slot;

		if (mode == 1) {
			AnimQueueBase *queue = anim->m_queue[slot_index];

			if (queue != NULL)
				queue->Clear();
		}
		if (mode == 2) {
			if (slot_index == 2) {
				AnimQueueBase *queue = anim->m_queue[0];

				anim->m_queue[0] = anim->m_queue[2];
				anim->m_queue[2] = queue;
				if (queue != NULL)
					queue->ClearAll();
			} else {
				for (int i = 0; i < 3; i++) {
					if (anim->m_queue[i] != NULL) {
						delete anim->m_queue[i];
						anim->m_queue[i] = NULL;
					}
				}
			}
		}
		if (mode == 3) {
			AnimQueueBase *queue = anim->m_queue[slot_index];

			if (queue != NULL) {
				TimerListener *front = queue->GetFront();

				if (front != NULL)
					front->ResetTimer();
			}
			anim->PushFront(listener, slot_index);
		} else {
			anim->PushBack(listener, slot_index);
		}
	}
	return 0;
}

int32_t Widget::SetAnimListener_withDelay(int32_t id, AnimListener *listener, float delay,
					  int32_t slot)
{
	int32_t type = (uint16_t)slot;

	if ((uint8_t)slot != 3 && delay > 0.0f) {
		SetAnimListener(id, new AnimListener(new Timer(delay), this, NULL, NULL, NULL), type);
		type = slot & 0xFF00;
	}
	SetAnimListener(id, listener, type);
	if (slot == 3 && delay > 0.0f)
		SetAnimListener(id, new AnimListener(new Timer(delay), this, NULL, NULL, NULL), type);
	return 0;
}

int32_t Widget::SetShowAlpha(float alpha, Timer *t, int32_t id, int32_t opt, float delay,
			     AnimCB cb_fin)
{
	if (t != NULL) {
		if (alpha > 1.0f)
			alpha = 1.0f;
		if (alpha < 0.0f)
			alpha = 0.0f;

		AnimListener *listener = new AnimListenerF1(t, this, m_show_alpha, alpha,
							    &AnimListener::s_prop_showalpha, cb_fin, NULL);

		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_SHOW_ALPHA;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_SHOW_ALPHA);
	ShowAlphaCore(this, alpha);
	return 0;
}

void Widget::SetPinchMode(uint32_t mode)
{
	if (m_pinch_mode != mode) {
		if (mode != 0) {
			if (m_drag_mode == 0)
				SetDragMode(0x1000);
			if (m_touch_gesture_mask != (m_touch_gesture_mask | 0x20))
				m_touch_gesture_mask = m_touch_gesture_mask | 0x20;
		} else if (m_touch_gesture_mask != (m_touch_gesture_mask & ~0x20)) {
			m_touch_gesture_mask = m_touch_gesture_mask & ~0x20;
		}

		SceFVector3 *limit = m_pinch_limit;

		m_pinch_mode = (uint16_t)mode;
		if ((mode & 0x400) != 0) {
			if (limit != NULL)
				m_size = math::v4(limit[0].x, limit[0].y, limit[0].z, 0.0f);
			else
				m_size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
			if (limit != NULL)
				m_size = math::v4(limit[1].x, limit[1].y, limit[1].z, 0.0f);
			else
				m_size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		} else if (limit == NULL) {
			m_pinch_limit = new SceFVector3[2];
			m_pinch_limit[0].x = 20.0f;
			m_pinch_limit[0].y = 20.0f;
			m_pinch_limit[0].z = 0.0f;
			m_pinch_limit[1].x = 1000.0f;
			m_pinch_limit[1].y = 1000.0f;
			m_pinch_limit[1].z = 1000.0f;
		}
	}
}

void Widget::SetDragMode(uint32_t mode)
{
	if (m_drag_mode != (uint16_t)mode) {
		if (mode == 0 && m_pinch_mode != 0)
			SetPinchMode(0);
		if (mode != 0) {
			if (m_touch_gesture_mask != (m_touch_gesture_mask | 0x10))
				m_touch_gesture_mask = m_touch_gesture_mask | 0x10;
		} else if (m_touch_gesture_mask != (m_touch_gesture_mask & ~0x10)) {
			m_touch_gesture_mask = m_touch_gesture_mask & ~0x10;
		}
		m_drag_mode = mode;
		if ((m_drag_mode & 0x100) != 0) {
			if (m_drag_limit == NULL) {
				m_drag_limit = new SceFVector3[2];
				m_drag_limit[0].x = -12288.0f;
				m_drag_limit[0].y = -12288.0f;
				m_drag_limit[0].z = -12288.0f;
				m_drag_limit[1].x = 10000.0f;
				m_drag_limit[1].y = 10000.0f;
				m_drag_limit[1].z = 10000.0f;
			}
		} else if ((m_drag_mode & 0x200) != 0) {
			if (m_drag_limit == NULL) {
				m_drag_limit = new SceFVector3[2];
				m_drag_limit[0].x = 20.0f;
				m_drag_limit[0].y = 20.0f;
				m_drag_limit[0].z = 0.0f;
				m_drag_limit[1].x = 1000.0f;
				m_drag_limit[1].y = 1000.0f;
				m_drag_limit[1].z = 1000.0f;
			}
		}
	}
}

int32_t Widget::SetSize(const math::v4 &size, Timer *t, int32_t id, int32_t opt, float delay,
			AnimCB cb_fin, void *usrdata)
{
	if (m_drag_state[0] != NULL && (m_drag_mode & 0x200) != 0) {
		delete t;
		return -1;
	}
	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, m_size, size,
							    &AnimListener::s_prop_size, cb_fin, usrdata);

		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_SIZE;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_SIZE);
	return SetSize_core(size, 0xFF02, false);
}

int32_t Widget::SetSize(const ResolutionData *size, uint32_t num, Timer *t, int32_t id,
			int32_t opt, float delay, AnimCB cb_fin, void *usrdata)
{
	ResolutionData *entry = get_resolution_data(const_cast<ResolutionData *>(size), num,
						    m_resolution_id);

	if (entry == NULL)
		return -1;
	if (m_size_array != NULL)
		delete[] m_size_array;
	m_size_num = 0;
	m_size_array = NULL;
	if (num > 0) {
		ResolutionData *copy = (ResolutionData *)operator new[](16 * num);

		sce_paf_memcpy(copy, size, 16 * num);
		m_size_array = copy;
		m_size_num = num;
	}
	return SetSize(math::v4(entry->m_value[0], entry->m_value[1], entry->m_value[2], 0.0f), t, id,
		       opt, delay, cb_fin, usrdata);
}

int32_t Widget::SetPosHermite(const math::v4 &pos, const math::v4 *const *tangents, Timer *t,
			      int32_t id, int32_t opt, float delay, AnimCB cb_fin, void *usrdata)
{
	if (IsDragMoving(this)) {
		delete t;
		return -1;
	}
	if (t != NULL) {
		AnimListenerV4 *listener = new AnimListenerV4(
			t, this, GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)), pos,
			&AnimListener::s_prop_move, cb_fin, usrdata);

		if (tangents != NULL) {
			math::v4 start = GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			listener->SetHermite(start, *tangents[0], pos, *tangents[1]);
		}
		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_MOVE;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_MOVE);
	return SetPos_core(pos, 0xFF01, false);
}

int32_t Widget::SetPos(const math::v4 &pos, Timer *t, int32_t id, int32_t opt, float delay,
		       AnimCB cb_fin, void *usrdata)
{
	return SetPosHermite(pos, NULL, t, id, opt, delay, cb_fin, usrdata);
}

int32_t Widget::SetPos(const ResolutionData *pos, uint32_t num, Timer *t, int32_t id,
		       int32_t opt, float delay, AnimCB cb_fin, void *usrdata)
{
	ResolutionData *entry = get_resolution_data(const_cast<ResolutionData *>(pos), num,
						    m_resolution_id);

	if (entry == NULL)
		return -1;
	if (m_pos_array != NULL)
		delete[] m_pos_array;
	m_pos_array = NULL;
	m_pos_num = 0;
	if (num > 0) {
		ResolutionData *copy = (ResolutionData *)operator new[](16 * num);

		sce_paf_memcpy(copy, pos, 16 * num);
		m_pos_array = copy;
		m_pos_num = num;
	}
	return SetPosHermite(math::v4(entry->m_value[0], entry->m_value[1], entry->m_value[2], 0.0f),
			     NULL, t, id, opt, delay, cb_fin, usrdata);
}

/* the nearest widget up the tree, this one included, that holds the focus of its children,
   short of the scene (name ours) */
static inline Widget *FindFocusRoot(Widget *widget)
{
	for (Widget *root = widget; root != NULL; root = root->m_parent) {
		if (root->m_focus_root != NULL)
			return root;
		if (static_cast<Widget *>(widget->m_scene) == root)
			return NULL;
	}
	return NULL;
}

extern "C" paf::ui::Widget *ScePafWidget_CFA1EB7A(Widget *widget)
{
	return FindFocusRoot(widget);
}

extern "C" void ScePafWidget_EC6175D6(Widget *widget, bool on)
{
	Widget *root = FindFocusRoot(widget);

	if (root == NULL)
		return;

	FocusRoot *focus = root->m_focus_root;

	if (on) {
		uint32_t state = focus->m_state;

		focus->m_init_focus = widget;
		if (state == 0)
			focus->m_state = (uint32_t)widget;
	} else if (focus->m_init_focus == widget) {
		focus->m_init_focus = NULL;
	}
}

void Widget::SetGestureMask(uint32_t mask, bool update)
{
	if (m_touch_gesture_mask != mask) {
		m_touch_gesture_mask = mask;
		if (update) {
			if (mask) {
				if (!m_hit_check)
					m_hit_check = 1;
			} else if (m_hit_check) {
				m_hit_check = 0;
			}
		}
	}
}

Widget *Widget::FindChild(Widget *child, int32_t option)
{
	if (child == NULL || child->m_hold_count > 0)
		return NULL;
	if (option == 2) {
		if (child->m_parent == this)
			return child;
		return NULL;
	}
	for (Widget *node = child; node != NULL; node = node->m_parent) {
		if (node == this)
			return child;
	}
	return NULL;
}

/* whether it is the widget its focus root focuses first */
bool Widget::IsInitFocus()
{
	Widget *root = FindFocusRoot(this);
	bool init = false;

	if (root != NULL && root->m_focus_root != NULL && root->m_focus_root->m_init_focus == this)
		init = true;
	return init;
}

void Widget::InitCreateParam(const CreateParam *param)
{
	if (param != NULL) {
		m_parent_protected = param->m_parent_protected;
		m_under_template = param->m_under_template;
	}
}

extern "C" void ScePafWidget_622193BD(Widget *widget, bool update)
{
	AdjustParam *param = widget->m_adjust_param;

	if (param != NULL) {
		if (--param->m_ref_count == 0)
			delete param;
		widget->m_adjust_param = NULL;
		if (update)
			widget->UpdateSize();
	}
}

int32_t Widget::SetParent(Widget *parent, Widget *sibling, int32_t option)
{
	if (parent == NULL)
		return -1;
	if (m_hold_count > 0)
		return -1;

	RestructInfo info;

	info.m_parent = parent;
	info.m_widget = this;
	info.m_sibling = sibling;
	info.m_option = option;
	s_list_restruct.push_back(info);
	return 0;
}

bool Widget::IsPrepareObjs()
{
	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_flag0 && it->m_obj->m_flags.m_prepare)
			return true;
	}
	return false;
}

int32_t Widget::SetEnvironment(Environment *env)
{
	if (m_env != env) {
		if (m_env != NULL)
			detach_environment();
		m_env = env;
		if (env != NULL) {
			m_event_queue = &env->m_ev_queue;
			if (m_touch_region_registered && env->RegisterTouchDeviceRegion(this) < 0)
				m_touch_region_registered = false;
		}
		if (m_children.size() != 0) {
			for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
				(*it)->SetEnvironment(env);
		}
	}
	return 0;
}

Widget *Widget::GetChild(int32_t index) const
{
	if ((uint32_t)index >= m_children.size())
		return NULL;

	list<Widget *>::iterator it = const_cast<list<Widget *> &>(m_children).begin();

	for (; index > 0; --index)
		++it;
	for (; index < 0; ++index)
		--it;
	return *it;
}

extern "C" int32_t ScePafWidget_D4759650(Widget *widget, int32_t delta)
{
	int32_t count = widget->m_touch_rear_children + delta;
	Widget *parent = widget->m_parent;

	if (count >= 0) {
		if (count <= 0xFFFF)
			widget->m_touch_rear_children = count;
		else
			widget->m_touch_rear_children = 0xFF;
	} else {
		widget->m_touch_rear_children = 0;
	}
	if (parent != NULL && static_cast<Widget *>(widget->m_scene) != widget)
		return ScePafWidget_D4759650(parent, delta);
	return (int32_t)parent;
}

int32_t Widget::EnableTouchRearEvent(bool on_off)
{
	if (m_touch_rear_event != on_off) {
		m_touch_rear_event = on_off;
		if (on_off)
			return ScePafWidget_D4759650(m_parent, 1);
		return ScePafWidget_D4759650(m_parent, -1);
	}
	return (int32_t)this;
}

bool Widget::RemoveChild(Widget *obj)
{
	bool found = false;
	int32_t index = 0;
	list<Widget *>::iterator it = m_children.begin();

	while (it != m_children.end()) {
		if (*it == obj && !found) {
			it = m_children.erase(it);
			found = true;

			int32_t prepare = obj->m_prepare_count + (obj->m_state == 3);

			if (prepare != 0)
				CountPrepareChildren(-prepare);

			int32_t focus = obj->m_focus_count + ((obj->m_event_enable & 0x400) != 0);

			if (focus != 0)
				CountFocusEnableChildren(-focus);

			int32_t rear = obj->m_touch_rear_children + (obj->IsTouchRearEventEnabled() != 0);

			if (rear > 0)
				ScePafWidget_D4759650(this, -rear);
			if (it == m_children.end())
				break;
		}
		(*it)->m_priority = index++;
		++it;
	}
	return found;
}

void Widget::RegisterChild(Widget *obj, Widget *sibling)
{
	if (sibling != NULL && sibling->m_parent == this) {
		int32_t index = 0;
		list<Widget *>::iterator pos = m_children.begin();

		if (pos != m_children.end()) {
			while (*pos != sibling) {
				++pos;
				index++;
				if (pos == m_children.end())
					goto insert;
			}
			index++;
		}
insert:
		list<Widget *>::iterator it = m_children.insert(pos, obj);

		while (it != m_children.end()) {
			(*it)->m_priority = index;
			++it;
			index++;
		}
	} else {
		obj->m_priority = m_children.size();
		m_children.push_back(obj);
	}
	obj->ApplyParentState(m_input_target, m_surface_draw, m_transition_in, m_destroy_protected,
			      m_layer + 1, m_show_a_sum, m_meta_a_sum);
	if (obj->m_prepare_count + (obj->m_state == 3) != 0)
		CountPrepareChildren(obj->m_prepare_count + (obj->m_state == 3));

	int32_t focus = obj->m_focus_count + ((obj->m_event_enable & 0x400) != 0);

	if (focus > 0)
		CountFocusEnableChildren(focus);
	if (obj->m_touch_rear_children + (obj->IsTouchRearEventEnabled() != 0) > 0)
		ScePafWidget_D4759650(this, 1);
	if (m_hold_count > 0) {
		obj->DestroyWidget();
		m_hold_count = s_base_destroy_count;
	}
}

void Widget::CountFocusEnableChildren(int32_t delta)
{
	int32_t count = m_focus_count + delta;
	Widget *parent = m_parent;

	if (count >= 0) {
		if (count <= 0xFFFF)
			m_focus_count = count;
		else
			m_focus_count = 0xFFFF;
	} else {
		m_focus_count = 0;
	}
	if (parent != NULL && static_cast<Widget *>(m_scene) != this)
		parent->CountFocusEnableChildren(delta);
}

bool Widget::UpdateSurfaceDraw(bool on)
{
	if (on == m_surface_draw)
		return false;
	m_surface_draw = on;

	graph::SurfaceDrawParam param;

	param.m_font_flags = 128;
	param.m_surface_draw = on;
	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it)
		it->m_obj->SetSurfaceDraw(param);
	return true;
}

void Widget::ApplyParentState(bool input_target, bool surface_draw, bool transition_in,
			      bool destroy_protected, uint32_t layer, float parent_show_sum,
			      float parent_meta_sum)
{
	m_input_target = input_target;
	UpdateSurfaceDraw(surface_draw);
	if (!m_timer_pending) {
		m_transition_in = transition_in;
		m_destroy_protected = destroy_protected;
	}
	m_layer_old = m_layer;
	m_layer = layer;
	m_show_a_sum = m_show_alpha * parent_show_sum;
	m_meta_a_sum = m_meta_alpha * parent_meta_sum;
	if (m_children.size() != 0) {
		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
			(*it)->ApplyParentState(m_input_target, m_surface_draw, m_transition_in,
						m_destroy_protected, m_layer + 1, m_show_a_sum,
						m_meta_a_sum);
	}
}

bool Widget::ReorderChild(Widget *obj, Widget *sibling)
{
	if (obj == sibling)
		return true;

	int32_t index = 0;
	bool found = false;
	list<Widget *>::iterator it = m_children.begin();

	if (it == m_children.end())
		return false;
	while (it != m_children.end()) {
		if (*it == obj && !found) {
			it = m_children.erase(it);
			found = true;
			if (it == m_children.end())
				break;
		}
		(*it)->m_priority = index++;
		++it;
	}
	if (!found)
		return false;
	if (sibling != NULL && sibling->m_parent == this) {
		index = 0;

		list<Widget *>::iterator pos = m_children.begin();

		if (pos != m_children.end()) {
			while (*pos != sibling) {
				++pos;
				index++;
				if (pos == m_children.end())
					goto insert;
			}
			index++;
		}
insert:
		it = m_children.insert(pos, obj);
		while (it != m_children.end()) {
			(*it)->m_priority = index;
			++it;
			index++;
		}
	} else {
		obj->m_priority = m_children.size();
		m_children.push_back(obj);
	}
	return true;
}

/* the ancestor of the widget that is a child of the parent, NULL when it is none (inline in
   Sony's SetBehind_core, SetForward_core and their steps; name ours) */
static inline Widget *ChildOfParent(Widget *parent, Widget *widget)
{
	if (widget != NULL) {
		for (;;) {
			Widget *above = widget->m_parent;

			if (above == parent)
				break;
			widget = above;
			if (above == NULL)
				return NULL;
		}
	}
	return widget;
}

int32_t Widget::ChangeResolution_core(uint32_t id, bool children, bool update)
{
	if (m_resolution_id != id) {
		m_resolution_id_next = 0xFF;
		if (m_scene != NULL) {
			if (m_scene->IsResolutionActMorph(m_resolution_id))
				m_resolution_id_pre = m_resolution_id;
		}
		m_resolution_id = id;
		if (this == m_scene && m_env != NULL) {
			m_scene->UpdateScreenSize(NULL);
			SetSize(m_size, NULL, ANIM_SIZE, 0, 0.0f, NULL, NULL);
		}
		delete m_resolution_timer_next;
		m_resolution_timer_next = NULL;
		if (update && m_scene != NULL && m_scene->IsResolutionActMorph(id) &&
		    IsInputTarget()) {
			ResolutionData *entry = get_resolution_data(m_pos_array, m_pos_num, id);

			if (entry != NULL)
				SetPosHermite(math::v4(entry->m_value[0], entry->m_value[1],
						       entry->m_value[2], 0.0f),
					      NULL, NULL, ANIM_MOVE, 0, 0.0f, NULL, NULL);
			entry = get_resolution_data(m_size_array, m_size_num, id);
			if (entry != NULL)
				SetSize(math::v4(entry->m_value[0], entry->m_value[1],
						 entry->m_value[2], 0.0f),
					NULL, ANIM_SIZE, 0, 0.0f, NULL, NULL);
			entry = get_resolution_data(m_rot_array, m_rot_num, id);
			if (entry != NULL)
				SetRot(math::v4(entry->m_value[0], entry->m_value[1],
						entry->m_value[2], 0.0f),
				       0, NULL, ANIM_ROT, 0, 0.0f, NULL, NULL);
			if (m_focus_shape != NULL)
				ScePafWidget_13B246E0(this, m_focus_shape);
			if (this != m_scene) {
				uint8_t type = m_transition;

				if (type != 0xFF && m_transition_complete) {
					m_transition_complete = false;
					if (type == 0xFF)
						DoTransitionReverse_core(0.0f, true);
					else
						ScePafCommon_B48C9BDC(0.0f, this,
								      (common::transition::Type)type,
								      false, true);
					ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
					type = m_transition;
					if (!m_transition_complete) {
						m_transition_complete = true;
						if (type == 0xFF)
							DoTransition_core(0.0f, true);
						else
							ScePafCommon_6CF3A3C8(
								0.0f, this,
								(common::transition::Type)type,
								false, true);
					}
					ScePafWidget_FF5BC957(this, NULL, 2, 0.0f, NULL);
				}
			}
			if (m_layouter != NULL)
				m_layouter->SetDirty(1);
		}
		if (children) {
			for (list<Widget *>::iterator it = m_children.begin();
			     it != m_children.end(); it++)
				(*it)->ChangeResolution_core(id, children, update);
		}
	}
	return 0;
}

int32_t Widget::SetParent_core(Widget *parent, Widget *sibling)
{
	if (m_hold_count > 0)
		return -1;

	Widget *old_parent = m_parent;

	if (old_parent == parent) {
		LayoutDirty(this, 0xFFFF);
		old_parent->ReorderChild(this, sibling);
		return 0;
	}

	bool focus_auto_off = false;
	Environment *env = m_env;

	if (env != NULL) {
		focus_auto_off = env->m_focus_auto_off;
		if (env->m_focus_auto_off && this != env->m_focus_cursor.m_target)
			env->m_focus_auto_off = false;
	}

	Widget *root = FindFocusRoot(this);
	bool focused = false;
	bool is_init_focus = false;

	if (root != NULL) {
		FocusRoot *focus = root->m_focus_root;

		if (focus != NULL)
			is_init_focus = focus->m_init_focus == this;
	}
	if (old_parent != NULL) {
		LayoutDirty(this, 0xFFFF);
		root = FindFocusRoot(this);

		bool release = false;

		if (root != NULL && root->m_focus_root != NULL) {
			Widget *holder = (Widget *)root->m_focus_root->m_state;

			if (holder != NULL && holder->m_hold_count <= 0) {
				for (Widget *w = holder; w != NULL; w = w->m_parent) {
					if (w == this) {
						release = true;
						break;
					}
				}
			}
		}
		if (!release && root == this) {
			root = FindFocusRoot(old_parent);
			if (root != NULL && (Widget *)root->m_focus_root->m_state == this)
				release = true;
		}
		if (release)
			root->SetFocusRootState(0);
		focused = m_focused;
		if (is_init_focus)
			ScePafWidget_EC6175D6(this, false);
		m_parent->RemoveChild(this);
		m_parent = NULL;
	}
	if (parent == NULL) {
		SetScene_core(NULL);
		m_parent = parent;
		return 0;
	}
	m_parent = parent;
	parent->RegisterChild(this, sibling);
	SetScene_core(m_parent->m_scene);
	SetEnvironment(m_parent->m_env);
	SetUpdateParam(2);
	if (focused)
		SetFocus(NULL);
	if (is_init_focus)
		ScePafWidget_EC6175D6(this, true);

	Widget *new_parent = m_parent;

	if (focus_auto_off)
		m_env->m_focus_auto_off = true;
	ChangeResolution_core(new_parent->m_resolution_id, true, true);
	if (m_drag_state[0] != NULL)
		ScePafWidget_E17C2B53(this, m_drag_state[0], NULL, 1.0f);
	LayoutDirty(this, 0xFFFF);
	return 0;
}

/* a layer's count in the scene changed, the weights marked to be made again (inline in Sony's
   code; name ours) */
static inline void AddLayerRange(Scene *scene, uint32_t layer, int32_t delta)
{
	if (layer < 16) {
		uint32_t count = scene->m_layer_range[layer];

		scene->m_layer_weight_dirty = true;
		scene->m_layer_range[layer] = count + delta;
	}
}

void Widget::detach_scene()
{
	Scene *scene = m_scene;

	if (scene != NULL) {
		AddLayerRange(scene, m_layer_old, -1);
		if (m_draw_target == 1) {
			scene->RemoveSurfaceWidget(this);
			scene = m_scene;
		}

		Widget *focus = NULL;

		if (scene->m_debug_mode != 0)
			focus = scene->m_debug_focus;
		if (focus == this)
			scene->SetDebugFocus(NULL);
	}
}

int32_t Widget::SetScene_core(Scene *scene)
{
	if ((m_scene != scene || m_layer != m_layer_old) && m_hold_count <= 0) {
		if (m_scene != NULL)
			detach_scene();
		m_scene = scene;
		if (scene != NULL) {
			AddLayerRange(scene, m_layer, 1);
			if (m_draw_target == 1) {
				if (scene->m_surface_widgets.size() == 0)
					scene->CreateBgLayer();

				Scene::SurfaceWidget surface = {};

				surface.m_widget = this;
				scene->m_surface_widgets.push_back(surface);
			}
		}
		m_layer_old = m_layer;
		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end();
		     it++)
			(*it)->SetScene_core(scene);
	}
	return 0;
}

int32_t Widget::get_resolution_data2(const ResolutionData *array, uint32_t num, uint32_t id_a,
				     uint32_t id_b, const ResolutionData **a,
				     const ResolutionData **b) const
{
	if (array == NULL)
		return -1;
	if (m_env == NULL)
		return -1;
	*a = NULL;
	*b = NULL;

	SceneResolution *resolution_a = ScePafWidget_CDC90BA2(m_scene->m_env_param, id_a);
	SceneResolution *resolution_b = ScePafWidget_CDC90BA2(m_scene->m_env_param, id_b);

	if (resolution_a == NULL || resolution_b == NULL)
		return -1;

	uint32_t key_a = resolution_a->m_resource.m_value;
	uint32_t key_b = resolution_b->m_resource.m_value;
	uint32_t i;

	for (i = 0; i < num; i++) {
		if (array[i].m_id == key_a)
			*a = &array[i];
		else if (array[i].m_id == key_b)
			*b = &array[i];
		if (*a != NULL && *b != NULL)
			return 0;
	}
	key_a = resolution_a->m_base_resource.m_value;
	key_b = resolution_b->m_base_resource.m_value;
	for (i = 0; i < num; i++) {
		if (array[i].m_id == key_a)
			*a = &array[i];
		else if (array[i].m_id == key_b)
			*b = &array[i];
		if (*a != NULL && *b != NULL)
			return 0;
	}
	return -1;
}

int32_t Widget::SetBehind_core(Widget *w)
{
	Widget *parent = m_parent;

	if (parent == NULL)
		return -1;
	w = ChildOfParent(parent, w);
	if (w == NULL)
		return -1;
	SetParent_core(parent, w);
	return 0;
}

int32_t Widget::SetForward_core(Widget *w)
{
	Widget *parent = m_parent;

	if (parent == NULL)
		return -1;
	w = ChildOfParent(parent, w);
	if (w == NULL)
		return -1;

	list<Widget *>::iterator it = parent->m_children.begin();

	while (it != parent->m_children.end()) {
		if (*it == w)
			break;
		++it;
	}
	++it;
	if (it == parent->m_children.end())
		SetParent_core(parent, NULL);
	else
		SetParent_core(parent, *it);
	return 0;
}

int32_t Widget::SetBehindOneStep_core()
{
	Widget *parent = m_parent;

	if (parent == NULL)
		return -1;

	list<Widget *>::iterator it = parent->m_children.begin();
	list<Widget *>::iterator prev = it;

	while (it != parent->m_children.end()) {
		if (*it == this)
			break;
		prev = it;
		++it;
	}

	Widget *sibling = ChildOfParent(parent, *prev);

	if (sibling == NULL)
		return -1;
	SetParent_core(parent, sibling);
	return 0;
}

int32_t Widget::SetForwardOneStep_core()
{
	Widget *parent = m_parent;

	if (parent == NULL)
		return -1;

	list<Widget *>::iterator it = parent->m_children.begin();

	if (it != parent->m_children.end()) {
		while (*it != this) {
			++it;
			if (it == parent->m_children.end())
				goto done;
		}
		++it;
	}
done:
	if (it == parent->m_children.end())
		return 1;
	return SetForward_core(*it);
}

int32_t Widget::SetBehindMost_core()
{
	Widget *parent = m_parent;

	if (parent == NULL)
		return -1;
	SetParent_core(parent, *parent->m_children.begin());
	return 0;
}

int32_t Widget::SetForwardMost_core()
{
	Widget *parent = m_parent;

	if (parent == NULL)
		return -1;
	SetParent_core(parent, NULL);
	return 0;
}

int32_t Widget::SetBehind(Widget *w)
{
	if (w != NULL && w != this) {
		if (m_parent != NULL) {
			int32_t result = -1;

			if (m_hold_count <= 0) {
				RestructInfo info;

				info.m_parent = m_parent;
				info.m_widget = this;
				info.m_sibling = w;
				info.m_option = 1;
				s_list_restruct.push_back(info);
				result = 0;
			}
			return result;
		}
		return -1;
	}
	return -1;
}

int32_t Widget::SetForward(Widget *w)
{
	if (w != NULL && w != this) {
		if (m_parent != NULL) {
			int32_t result = -1;

			if (m_hold_count <= 0) {
				RestructInfo info;

				info.m_parent = m_parent;
				info.m_widget = this;
				info.m_sibling = w;
				info.m_option = 2;
				s_list_restruct.push_back(info);
				result = 0;
			}
			return result;
		}
		return -1;
	}
	return -1;
}

int32_t Widget::KillAnim(int32_t id)
{
	map<int32_t, AnimQueue *>::iterator it = m_anims.begin();

	if (it == m_anims.end())
		return -1;
	while ((it->first & 0xFFFFFF) != id) {
		++it;
		if (it == m_anims.end())
			return -1;
	}

	AnimQueue *anim = it->second;

	if (anim != NULL) {
		for (int i = 0; i < 3; i++) {
			AnimQueueBase *queue = anim->m_queue[i];

			if (queue != NULL)
				queue->HoldAll();
		}
	}
	return 0;
}

int32_t Widget::SetObjAnim(int32_t obj, int32_t id, const math::v4 &value, Timer *t,
			   int32_t opt)
{
	graph::DrawObj *drawobj = GetDrawObj(obj);
	AnimPropertyV4 *prop;
	math::v4 start;

	if (drawobj == NULL)
		return -1;
	switch (id & 0xFFFFFF) {
	case 0x20100:
		prop = &AnimListenerObj::s_prop_obj_move;
		start = math::v4(math::v3(drawobj->m_pos), 1.0f);
		break;
	case 0x20200:
		prop = &AnimListenerObj::s_prop_obj_size;
		start = math::v4(math::v3(drawobj->m_size), 1.0f);
		break;
	case 0x20300:
		prop = &AnimListenerObj::s_prop_obj_color;
		start = drawobj->GetColor();
		break;
	case 0x20400:
		prop = &AnimListenerObj::s_prop_obj_rot;
		if (drawobj->m_rot != NULL)
			start = math::v4(*drawobj->m_rot, 1.0f);
		else
			start = math::v4(0.0f, 0.0f, 0.0f, 1.0f);
		break;
	case 0x20600:
		if (!drawobj->IsInherit(graph::PlaneObj::DrawObjType()))
			return -1;
		prop = &AnimListenerObj::s_prop_obj_texregion;
		if (static_cast<graph::PlaneObj *>(drawobj)->m_tex_region != NULL)
			start = *static_cast<graph::PlaneObj *>(drawobj)->m_tex_region;
		else
			start = math::v4(0.0f, 0.0f, 1.0f, 1.0f);
		break;
	case 0x20700:
		if (!drawobj->IsInherit(graph::PanelObj::DrawObjType()))
			return -1;
		prop = &AnimListenerObj::s_prop_obj_revolution;
		start = graph::GetPanelRevolution(static_cast<graph::PanelObj *>(drawobj));
		break;
	default:
		return -1;
	}
	SetAnimListener(obj + id, new AnimListenerObj(t, this, drawobj, start, value, prop, NULL, NULL),
			opt);
	return 0;
}

int32_t Widget::SetTexAnim(int32_t obj, int32_t column_num, int32_t row_num, int32_t start_frame,
			   int32_t end_frame, Timer *t, int32_t option, float delay,
			   AnimCB cb_fin, void *userdata)
{
	graph::PlaneObj *plane;

	if (GetDrawObj(obj) != NULL &&
	    (plane = static_cast<graph::PlaneObj *>(GetDrawObj(obj)))->IsInherit(
		    graph::PlaneObj::DrawObjType()) &&
	    (plane = static_cast<graph::PlaneObj *>(GetDrawObj(obj))) != NULL) {
		if (t == NULL) {
			KillAnimType(obj + 0x20500);

			float column_ratio = 1.0f / (float)column_num;
			float row_ratio = 1.0f / (float)row_num;

			plane->SetTexRegion(math::v4(column_ratio * (float)(end_frame % column_num),
						     row_ratio * (float)(end_frame / column_num % row_num),
						     column_ratio, row_ratio));
			return 0;
		}

		AnimListener *listener = new TexAnimListenerObj(
			t, this, plane, column_num, row_num, start_frame, end_frame, cb_fin, userdata);

		SetAnimListener_withDelay(obj + 0x20500, listener, delay, option);
		return 0;
	}
	return -1;
}

void Widget::KillObjAnim(int32_t obj, int32_t id)
{
	KillAnim(obj + id);
}

extern "C" void ScePafWidget_66847A11(Widget *widget, bool on)
{
	if (widget->UpdateSurfaceDraw(on)) {
		for (list<Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); ++it)
			ScePafWidget_66847A11(*it, on);
	}
}

int32_t Widget::SetColor_core(const math::v4 &color)
{
	float32x4_t one = vdupq_n_f32(1.0f);
	float32x4_t value = vmaxq_f32(vminq_f32(color.getImpl().get128(), one), vdupq_n_f32(0.0f));
	uint32x4_t equal = vceqq_f32(value, one);
	uint32x2_t both = vand_u32(vand_u32(vdup_lane_u32(vget_high_u32(equal), 0),
					    vrev64_u32(vget_low_u32(equal))),
				   vget_low_u32(equal));
	uint32_t bytes = 0xFFFFFFFF;

	if (vget_lane_u32(vdup_lane_u32(both, 0), 0) == 0) {
		uint16x4_t narrow = vqmovn_u32(vcvtq_u32_f32(vmulq_f32(value, vdupq_n_f32(255.0f))));

		bytes = vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(narrow, narrow))), 0);
	}
	m_color = bytes;
	m_color_alpha = vgetq_lane_f32(value, 3);
	return 0;
}

int32_t Widget::SetColor(const math::v4 &color, Timer *t, int32_t id, int32_t opt, float delay,
			 AnimCB cb_fin, void *usrdata)
{
	if (t == NULL) {
		KillAnimType(ANIM_COLOR);
		return SetColor_core(color);
	}

	AnimListener *listener = new AnimListenerV4(t, this, GetColor(), color,
						    &AnimListener::s_prop_color, cb_fin, usrdata);

	if ((id & 0xFF0000) != 0)
		id = (id & 0xFF000000) | ANIM_COLOR;
	SetAnimListener_withDelay(id, listener, delay, opt);
	return 0;
}

int32_t Widget::SetMetaAlpha_core(float alpha)
{
	Widget *parent = m_parent;
	float meta = m_meta_alpha;
	bool has_children = m_children.size() != 0;

	if (alpha >= 0.0f) {
		meta = alpha;
		m_meta_alpha = alpha;
		if (alpha > 1.0f) {
			meta = 1.0f;
			m_meta_alpha = 1.0f;
		}
	}
	m_meta_a_sum = meta * (parent != NULL ? parent->m_meta_a_sum : 1.0f);
	if (has_children) {
		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
			(*it)->SetMetaAlpha_core(-1.0f);
	}
	return 0;
}

int32_t Widget::SetMetaAlpha(float alpha, Timer *t, int32_t id, int32_t opt, float delay,
			     AnimCB cb_fin, void *usrdata)
{
	if (t != NULL) {
		AnimListener *listener = new AnimListenerF1(t, this, m_meta_alpha, alpha,
							    &AnimListener::s_prop_metaalpha, cb_fin, usrdata);

		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_META_ALPHA;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_META_ALPHA);
	return SetMetaAlpha_core(alpha);
}

void Widget::SetResolutionPos_core(const ResolutionData *pos, int32_t num)
{
	if (m_pos_array != NULL)
		delete[] m_pos_array;
	m_pos_num = 0;
	m_pos_array = NULL;
	if (num > 0) {
		ResolutionData *copy = (ResolutionData *)operator new[](16 * num);

		sce_paf_memcpy(copy, pos, 16 * num);
		m_pos_array = copy;
		m_pos_num = num;
	}
}

int32_t Widget::SetPosCenter(const math::v4 &v)
{
	return SetPosHermite(v - AlignOffsetToRotatedCenter(this, 0), NULL, NULL, ANIM_MOVE, 0, 0.0f,
			     NULL, NULL);
}

int32_t Widget::SetScale_core(const math::v4 &scale, uint32_t notify_layout)
{
	float32x4_t clamped = vmaxq_f32(vdupq_n_f32(0.0001f), scale.getImpl().get128());
	math::v4 value(sce::Vectormath::Simd::Aos::Vector4(vcombine_f32(
		vget_low_f32(clamped), vset_lane_f32(0.0f, vget_high_f32(clamped), 1))));
	math::v4 pos;

	if (m_scale == value)
		return 0;
	if ((m_align_x | m_align_y | m_align_z) != 0 ||
	    !(m_align_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f)))
		pos = GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	m_scale = value;
	LayoutDirty(this, notify_layout);
	SetUpdateParam(1);
	if ((m_align_x | m_align_y | m_align_z) != 0 ||
	    !(m_align_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f)))
		SetPos_core(pos, 1, false);
	return 0;
}

int32_t Widget::SetScale(const math::v4 &scale, Timer *t, int32_t id, int32_t opt, float delay,
			 AnimCB cb_fin, void *usrdata)
{
	if (IsDragMoving(this)) {
		delete t;
		return -1;
	}
	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, m_scale, scale,
							    &AnimListener::s_prop_scale, cb_fin, usrdata);

		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_SCALE;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_SCALE);
	return SetScale_core(scale, 0xFF04);
}

int32_t Widget::SetQuat_core(const math::quaternion &quat, uint32_t notify_layout)
{
	if (quat == math::quaternion(0.0f, 0.0f, 0.0f, 0.0f))
		return -1;

	math::quaternion value = NormalizeQuat(quat);
	bool equal = m_quat == value;

	m_rot_euler = false;
	if (equal)
		return 0;

	math::v4 pos;

	if ((m_align_x | m_align_y | m_align_z) != 0 ||
	    !(m_align_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f)))
		pos = GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	m_quat = value;
	LayoutDirty(this, notify_layout);
	SetUpdateParam(1);
	if ((m_align_x | m_align_y | m_align_z) != 0 ||
	    !(m_align_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f)))
		SetPos_core(pos, 1, false);
	return 0;
}

int32_t Widget::SetRot_core(const math::v4 &rot, int32_t option, uint32_t notify_layout)
{
	const float two_pi = 6.2831855f;
	float x = rot.extract_x() - (float)(int)(rot.extract_x() / two_pi) * two_pi;
	float y = rot.extract_y() - (float)(int)(rot.extract_y() / two_pi) * two_pi;
	float z = rot.extract_z() - (float)(int)(rot.extract_z() / two_pi) * two_pi;

	m_rot = math::v4(x, y, z, rot.extract_w());

	math::quaternion quat;

	if (option == 1)
		quat = math::quaternion::FromEulerAnglesZYX(math::v3(x, y, z));
	else if (option == 2)
		quat = math::quaternion::FromEulerAnglesXYZ(math::v3(x, y, z));
	else
		quat = math::quaternion::FromEulerAnglesYXZ(math::v3(x, y, z));

	int32_t result = SetQuat_core(quat, notify_layout);

	m_rot_euler = true;
	return result;
}

int32_t Widget::SetQuat(const math::quaternion &quat, Timer *t, int32_t id, int32_t opt, float delay,
			AnimCB cb_fin, void *usrdata)
{
	if (IsDragMoving(this)) {
		delete t;
		return -1;
	}
	KillAnimType(ANIM_ROT);
	if (t == NULL) {
		KillAnimType(ANIM_QUAT);
		return SetQuat_core(quat, 0xFF08);
	}

	AnimListener *listener = new AnimListenerV4(
		t, this, reinterpret_cast<const math::v4 &>(m_quat),
		reinterpret_cast<const math::v4 &>(quat), &AnimListener::s_prop_quat, cb_fin, usrdata);

	if ((id & 0xFF0000) != 0)
		id = (id & 0xFF000000) | ANIM_QUAT;
	SetAnimListener_withDelay(id, listener, delay, opt);
	return 0;
}

/* the animation's first or last target, where one is running (name ours) */
static inline math::v4 *AnimEndTarget(AnimQueue *anim, bool front)
{
	if (anim != NULL) {
		AnimQueueBase *queue = anim->m_queue[0];

		if (queue != NULL) {
			AnimListener *listener = static_cast<AnimListener *>(front ? queue->GetFront() : queue->GetBack());

			if (listener != NULL)
				return listener->GetTargetV4();
		}
	}
	return NULL;
}

/* the Euler angles of the rotation in the order of the option (ZYX for 1, XYZ for 2, else YXZ),
   w 0 (inline in Sony; name ours) */
static inline __attribute__((always_inline)) math::v4 QuatToEuler(const math::quaternion &quat, uint8_t option)
{
	math::v3 angles;

	if (option > 1) {
		if (option <= 2)
			angles = quat.toMatrix(0.0001f).extractEulerAnglesXYZ();
		else
			angles = quat.toMatrix(0.0001f).extractEulerAnglesYXZ();
	} else if (option != 0) {
		angles = quat.toMatrix(0.0001f).extractEulerAnglesZYX();
	} else {
		angles = quat.toMatrix(0.0001f).extractEulerAnglesYXZ();
	}
	return math::v4(angles, 0.0f);
}

int32_t Widget::SetSize_core_force(const math::v4 &size, uint32_t notify_layout, bool store)
{
	math::v4 value = size;

	value.set_w(0.0f);
	if (m_size == value)
		return 0;

	math::v4 pos;

	if ((m_align_x | m_align_y | m_align_z) != 0 ||
	    !(m_align_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f)))
		pos = GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	m_size = value;
	if ((m_align_x | m_align_y | m_align_z) != 0 ||
	    !(m_align_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
		AnimQueue *anim = FindAnim(ANIM_ALIGN);

		if (anim != NULL) {
			math::v4 *target = AnimEndTarget(anim, false);

			*target = calc_align_offset(m_size, *target, m_align_x, m_align_y, m_align_z);
		} else {
			m_align_offs = calc_align_offset(m_size, m_align_offs, m_align_x, m_align_y,
							 m_align_z);
			SetPos_core(pos, 1, false);
		}
	}
	if ((m_origin_x | m_origin_y | m_origin_z) != 0 ||
	    !(m_origin_offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
		AnimQueue *anim = FindAnim(ANIM_ORIGIN);

		if (anim != NULL) {
			math::v4 *target = AnimEndTarget(anim, false);

			*target = calc_origin_offset(m_size, *target, m_origin_x, m_origin_y, m_origin_z);
		} else {
			m_origin_offs = calc_origin_offset(m_size, m_origin_offs, m_origin_x,
							   m_origin_y, m_origin_z);
		}
	}
	SetUpdateParam(2);
	LayoutDirty(this, notify_layout);
	AnimatorDirty(this, 0xFF02);
	UpdateSize();
	if (store && m_size_array != NULL) {
		ResolutionData *entry = get_resolution_data(m_size_array, m_size_num, m_resolution_id);

		if (entry != NULL) {
			entry->m_value[0] = m_size.extract_x();
			entry->m_value[1] = m_size.extract_y();
			entry->m_value[2] = m_size.extract_z();
		}
	}
	return 0;
}

int32_t Widget::SetSize_core(const math::v4 &size, uint32_t notify_layout, bool force)
{
	math::v4 value = size;

	if (m_adjust_x != 0 || m_adjust_y != 0 || m_adjust_z != 0) {
		float width, height;

		if (m_adjust_x == 1)
			value.set_x(m_size.extract_x());
		if (m_adjust_y == 1)
			value.set_y(m_size.extract_y());
		if (m_adjust_z == 1)
			value.set_z(m_size.extract_z());
		if (m_scene != NULL) {
			math::v4 *scene_size = m_scene->GetSize(0);

			width = scene_size->extract_x();
			height = scene_size->extract_y();
		} else {
			math::rect screen(Screen::s_root_screen->sp);

			width = screen.width();
			height = screen.height();
		}
		if (this == m_scene) {
			SceneResolution *resolution =
				ScePafWidget_CDC90BA2(m_scene->m_env_param, m_scene->m_resolution_id);

			if (resolution != NULL && m_scene->IsResolutionActMorph(m_scene->m_resolution_id)) {
				width = (float)resolution->m_width;
				height = (float)resolution->m_height;
			} else {
				math::v2 vp_size = m_scene->m_screen->vp.size();

				width = vp_size.extract_x();
				height = vp_size.extract_y();
			}
		}
		if (m_adjust_x == 4)
			value.set_x(width);
		if (m_adjust_y == 4)
			value.set_y(height);
	}
	return SetSize_core_force(value, notify_layout, force);
}

int32_t Widget::SetRot(const math::v4 &rot, int32_t option, Timer *t, int32_t id, int32_t opt,
		       float delay, AnimCB cb_fin, void *usrdata)
{
	if (IsDragMoving(this)) {
		delete t;
		return -1;
	}
	KillAnimType(ANIM_QUAT);
	if (t == NULL) {
		KillAnimType(ANIM_ROT);
		return SetRot_core(rot, option, 0xFF08);
	}

	AnimPropertyV4 *prop = &AnimListener::s_prop_rot;

	if ((option & 0x100) != 0)
		prop = &AnimListener::s_prop_rot_short;

	AnimListener *listener = new AnimListenerV4(
		t, this, GetRot(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0), rot, prop, cb_fin, usrdata);

	if ((id & 0xFF0000) != 0)
		id = (id & 0xFF000000) | ANIM_ROT;
	SetAnimListener_withDelay(id, listener, delay, opt);
	return 0;
}

math::v4 Widget::GetRot(int32_t opt, Widget *ref, const math::v4 &off_rot, int32_t option)
{
	if (opt != 0) {
		bool front = opt == 2;
		math::v4 *rot = AnimEndTarget(FindAnim(ANIM_ROT), front);

		if (rot != NULL)
			return *rot;

		math::quaternion quat = m_quat;

		if (FindAnim(ANIM_QUAT) != NULL) {
			math::v4 *target = AnimEndTarget(FindAnim(ANIM_QUAT), front);

			if (target != NULL)
				quat = reinterpret_cast<math::quaternion &>(*target);
			return QuatToEuler(quat, option);
		}
		if (m_rot_euler)
			return math::v4(m_rot.extract_x(), m_rot.extract_y(), m_rot.extract_z(), 0.0f);
		return QuatToEuler(m_quat, option);
	}
	if (ref != NULL) {
		UpdateMatrix();
		ref->UpdateMatrix();

		math::quaternion own = m_world_rot;
		math::quaternion rel = QuatMul(ref->m_world_rot.inverse(), own);

		return QuatToEuler(rel, option);
	}
	if (m_rot_euler)
		return math::v4(m_rot.extract_x(), m_rot.extract_y(), m_rot.extract_z(), 0.0f);
	return QuatToEuler(m_quat, option);
}

math::v4 Widget::GetPos(int32_t opt, Widget *ref, const math::v4 &off_pos)
{
	if (opt != 0) {
		math::v4 rotated = AlignOffsetToRotatedCenter(this, opt);
		AnimQueue *anim = FindAnim(ANIM_MOVE);
		math::v4 *target = NULL;

		if (anim != NULL) {
			AnimQueueBase *queue = anim->m_queue[0];

			if (queue != NULL) {
				AnimListener *listener = static_cast<AnimListener *>(queue->GetBack());

				if (listener != NULL)
					target = listener->GetTargetV4();
			}
		}
		if (target != NULL)
			return *target - rotated;
		return m_pos - rotated;
	}
	if (ref == NULL)
		return m_pos - AlignOffsetToRotatedCenter(this, 0);

	UpdateMatrix();
	ref->UpdateMatrix();

	math::v4 offs = off_pos - GetAlignOffsetToCenter(0);
	float sx = m_flip_x ? -1.0f : 1.0f;
	float sy = m_flip_y ? -1.0f : 1.0f;
	float sz = m_flip_z ? -1.0f : 1.0f;

	if (ref->m_inverse_matrix == NULL) {
		ref->m_inverse_matrix = new math::matrix(math::matrix::Identity());
		ref->SetUpdateParam(2);
	}
	if ((ref->m_update_flag & 7) != 0)
		ref->UpdateMatrix();

	math::v4 moved = math::v4(m_world_matrix.getImpl() *
				  sce::Vectormath::Simd::Aos::Vector4(offs.extract_x() * sx,
								      offs.extract_y() * sy,
								      offs.extract_z() * sz, 1.0f));
	math::v4 local = math::v4(ref->m_inverse_matrix->getImpl() * moved.getImpl());
	float x = local.extract_x() * (ref->m_flip_x ? -1.0f : 1.0f);
	float y = local.extract_y() * (ref->m_flip_y ? -1.0f : 1.0f);
	float z = local.extract_z() * (ref->m_flip_z ? -1.0f : 1.0f);

	return math::v4(x, y, z, local.extract_w());
}

math::v4 Widget::GetPosCenter(int32_t opt, Widget *ref, const math::v4 &off_pos)
{
	if (ref != NULL) {
		math::v4 shifted = off_pos + AlignOffsetToRotatedCenter(this, opt);

		return GetPos(opt, ref, shifted);
	}
	if (opt == 0)
		return m_pos;

	bool front = opt == 2;
	math::v4 *target = NULL;
	AnimQueue *anim = FindAnim(ANIM_MOVE);

	if (anim != NULL) {
		AnimQueueBase *queue = anim->m_queue[0];
		AnimListener *listener = NULL;

		if (front) {
			if (queue != NULL)
				listener = static_cast<AnimListener *>(queue->GetFront());
		} else {
			if (queue != NULL)
				listener = static_cast<AnimListener *>(queue->GetBack());
		}
		if (listener != NULL)
			target = listener->GetTargetV4();
	}
	if (target != NULL)
		return *target + AlignOffsetToRotatedCenter(this, 1);
	return m_pos;
}

math::quaternion Widget::GetQuat(int32_t opt, Widget *ref, const math::quaternion &off_quat)
{
	if (opt == 0) {
		if (ref == NULL)
			return m_quat;

		UpdateMatrix();
		ref->UpdateMatrix();

		math::quaternion own = m_world_rot;
		math::quaternion rel = QuatMul(ref->m_world_rot.inverse(), own);

		return NormalizeQuat(rel);
	}

	bool front = opt == 2;
	math::v4 *target = NULL;
	AnimQueue *anim = FindAnim(ANIM_QUAT);

	if (anim != NULL) {
		AnimQueueBase *queue = anim->m_queue[0];
		AnimListener *listener = NULL;

		if (front) {
			if (queue != NULL)
				listener = static_cast<AnimListener *>(queue->GetFront());
		} else {
			if (queue != NULL)
				listener = static_cast<AnimListener *>(queue->GetBack());
		}
		if (listener != NULL)
			target = listener->GetTargetV4();
		if (target != NULL)
			return reinterpret_cast<math::quaternion &>(*target);
	}

	anim = FindAnim(ANIM_ROT);
	if (anim == NULL)
		return m_quat;

	AnimQueueBase *queue = anim->m_queue[0];
	AnimListener *listener = NULL;

	target = NULL;
	if (front) {
		if (queue != NULL)
			listener = static_cast<AnimListener *>(queue->GetFront());
	} else {
		if (queue != NULL)
			listener = static_cast<AnimListener *>(queue->GetBack());
	}
	if (listener != NULL)
		target = listener->GetTargetV4();
	if (target == NULL)
		return m_quat;
	return math::quaternion::FromEulerAnglesYXZ(math::v3(target->getImpl().getXYZ()));
}

math::v4 *Widget::GetScale(int32_t opt)
{
	if (opt == 0)
		return &m_scale;

	bool front = opt == 2;
	AnimQueue *anim = FindAnim(ANIM_SCALE);

	if (anim == NULL)
		return &m_scale;

	AnimQueueBase *queue = anim->m_queue[0];
	math::v4 *result = NULL;

	if (front) {
		if (queue != NULL) {
			AnimListener *listener = static_cast<AnimListener *>(queue->GetFront());

			if (listener != NULL)
				result = listener->GetTargetV4();
		}
	} else {
		if (queue != NULL) {
			AnimListener *listener = static_cast<AnimListener *>(queue->GetBack());

			if (listener != NULL)
				result = listener->GetTargetV4();
		}
	}
	if (result == NULL)
		return &m_scale;
	return result;
}

void Widget::UpdateSize()
{
	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_flag1)
			it->m_obj->SetSize(math::v3(m_size.getImpl().getXYZ()));
	}
}

void Widget::SetResolutionSize_core(const ResolutionData *size, int32_t num)
{
	if (m_size_array != NULL)
		delete[] m_size_array;
	m_size_num = 0;
	m_size_array = NULL;
	if (num > 0) {
		ResolutionData *copy = (ResolutionData *)operator new[](16 * num);

		sce_paf_memcpy(copy, size, 16 * num);
		m_size_array = copy;
		m_size_num = num;
	}
}

void Widget::SetResolutionRot_core(const ResolutionData *rot, int32_t num)
{
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	m_rot_num = 0;
	m_rot_array = NULL;
	if (num > 0) {
		ResolutionData *copy = (ResolutionData *)operator new[](16 * num);

		sce_paf_memcpy(copy, rot, 16 * num);
		m_rot_array = copy;
		m_rot_num = num;
	}
}

int32_t Widget::SetRot(const ResolutionData *rot, uint32_t num, int32_t option, Timer *t,
		       int32_t id, int32_t opt, float delay, AnimCB cb_fin, void *usrdata)
{
	ResolutionData *entry = get_resolution_data(const_cast<ResolutionData *>(rot), num,
						    m_resolution_id);

	if (entry == NULL)
		return -1;
	if (m_rot_array != NULL)
		delete[] m_rot_array;
	m_rot_num = 0;
	m_rot_array = NULL;
	if (num > 0) {
		ResolutionData *copy = (ResolutionData *)operator new[](16 * num);

		sce_paf_memcpy(copy, rot, 16 * num);
		m_rot_array = copy;
		m_rot_num = num;
	}
	return SetRot(math::v4(entry->m_value[0], entry->m_value[1], entry->m_value[2], 0.0f), option,
		      t, id, opt, delay, cb_fin, usrdata);
}

void Widget::Update()
{
	if (m_hold_count <= 0) {
		uint32_t num = m_drawobjs.size();

		for (uint32_t i = 0; i != num; i++)
			m_drawobjs[i].m_obj->Update();
	}
}

int32_t Widget::OnDraw(graph::DrawInfo &info)
{
	if (!m_culled || m_draw_always) {
		float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

		for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it)
			it->m_obj->OnDraw(m_world_matrix, m_color, alpha, info);
	}
	return 0;
}

int32_t Widget::ApplyDrawObjState(graph::DrawInfo &info)
{
	return 0;
}

void Widget::UpdateMatrixTree()
{
	UpdateMatrix();
	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
		(*it)->UpdateMatrixTree();
}

void Widget::OnUpdatePost()
{
	UpdatePost();
	if ((m_update_flag & 7) != 0)
		UpdateMatrix();
	m_update_flag &= ~4;
	DrawObjEntry *entry = &m_drawobjs[0];

	for (uint32_t n = m_drawobjs.size(); n != 0; n--)
		(entry++)->m_obj->UpdatePost(m_world_matrix);
	if (m_children.size() != 0) {
		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
			(*it)->OnUpdatePost();
	}
}

int32_t Widget::UpdateDestroy()
{
	if (s_destroying_count == 0)
		return 0;
	if (m_children.size() != 0) {
		list<Widget *>::iterator it = m_children.begin();

		while (it != m_children.end()) {
			Widget *child = *it;

			it++;
			child->UpdateDestroy();
		}
	}

	int32_t hold = m_hold_count;

	if (hold <= 0)
		return 0;
	m_hold_count = hold - 1;
	if (hold != 1)
		return 0;
	if (m_children.size() != 0) {
		m_hold_count = 1;
		return 0;
	}
	delete this;
	s_destroying_count--;
	return 1;
}

math::v4 Widget::GetHitBoxSize(int32_t opt)
{
	uint32_t mode = m_hit_box_mode;

	if (mode == 0)
		return *GetSize(opt);
	if (mode == 4) {
		if ((m_event_enable & 0x20) == 0 && (m_event_enable & 0x40) == 0 && !m_clip_children)
			return *GetSize(opt);
		if (opt != 0)
			return ScePafWidget_301AA80F(this, *GetSize(opt));
		return ScePafWidget_301AA80F(this, m_size);
	}

	math::v4 box(0.0f, 0.0f, 0.0f, 0.0f);

	if (m_hit_box != NULL)
		box = m_hit_box[1];
	if (mode > 1) {
		if (mode <= 2)
			return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(box.getImpl(), GetSize(opt)->getImpl()));
		return box;
	}
	return box + *GetSize(opt);
}

math::v4 Widget::GetHitBoxCenter()
{
	if (m_hit_box_mode == 4) {
		if ((m_update_flag & 7) != 0)
			UpdateMatrix();
		return m_world_matrix.w();
	}

	if (m_hit_box == NULL || m_hit_box[0] == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
		if ((m_update_flag & 7) != 0)
			UpdateMatrix();
		return m_world_matrix.w();
	}

	float32x4_t center = m_hit_box[0].getImpl().get128();

	center = vsetq_lane_f32(0.0f, center, 3);
	if ((m_update_flag & 7) != 0)
		UpdateMatrix();

	float32x4_t pos = m_world_matrix.w().getImpl().get128();

	if ((m_update_flag & 7) != 0)
		UpdateMatrix();

	float32x4_t col3 = m_world_matrix.w().getImpl().get128();
	float32x2_t low = vget_low_f32(center);
	float32x2_t high = vget_high_f32(center);
	float32x4_t a = vmlaq_lane_f32(vmulq_lane_f32(m_world_matrix.x().getImpl().get128(), low, 0),
				       m_world_matrix.z().getImpl().get128(), high, 0);
	float32x4_t b = vmlaq_lane_f32(vmulq_lane_f32(m_world_matrix.y().getImpl().get128(), low, 1),
				       col3, high, 1);

	return math::v4(sce::Vectormath::Simd::Aos::Vector4(vaddq_f32(pos, vaddq_f32(a, b))));
}

int32_t Widget::SetAlignOffset_core(const math::v4 &offs, uint32_t notify_layout)
{
	math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 pos = GetPos(0, NULL, zero);

	m_align_offs = offs;
	SetPos_core(pos, 0, false);
	LayoutDirty(this, notify_layout);
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::SetAlign(uint32_t ax, uint32_t ay, uint32_t az, Timer *t)
{
	m_align_x = ax;
	m_align_y = ay;
	m_align_z = az;

	math::v4 offs = calc_align_offset(m_size, m_align_offs, ax, ay, az);

	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, GetAlignOffsetToCenter(0), offs,
							    &AnimListener::s_prop_align, NULL, NULL);

		SetAnimListener(ANIM_ALIGN, listener, 2);
		return 0;
	}
	KillAnimType(ANIM_ALIGN);
	return SetAlignOffset_core(offs, 0xFF10);
}

int32_t Widget::SetAlignOffset(const math::v4 &offs, Timer *t, int32_t id, int32_t opt,
			       float delay, AnimCB cb_fin, void *usrdata)
{
	if (IsDragMoving(this)) {
		delete t;
		return -1;
	}
	m_align_x = 3;
	m_align_y = 3;
	m_align_z = 3;
	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, GetAlignOffsetToCenter(0), offs,
							    &AnimListener::s_prop_align, cb_fin, usrdata);

		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_ALIGN;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_ALIGN);
	return SetAlignOffset_core(offs, 0xFF10);
}

math::v4 Widget::calc_align_offset(const math::v4 &size, const math::v4 &aoffs, uint32_t ax,
				   uint32_t ay, uint32_t az)
{
	if ((ax | ay | az) == 0)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);

	float x = aoffs.extract_x();
	float y = aoffs.extract_y();
	float z = aoffs.extract_z();
	float w = aoffs.extract_w();

	switch (ax) {
	case 1:
		x = 0.5f * size.extract_x();
		break;
	case 2:
		x = -size.extract_x() * 0.5f;
		break;
	case 0:
		x = 0.0f;
		break;
	default:
		break;
	}
	switch (ay) {
	case 1:
		y = 0.5f * size.extract_y();
		break;
	case 2:
		y = -size.extract_y() * 0.5f;
		break;
	case 0:
		y = 0.0f;
		break;
	default:
		break;
	}
	switch (az) {
	case 1:
		z = 0.5f * size.extract_z();
		break;
	case 2:
		z = -size.extract_z() * 0.5f;
		break;
	case 0:
		z = 0.0f;
		break;
	default:
		break;
	}
	return math::v4(x, y, z, w);
}

math::v4 Widget::GetOriginOffset(int32_t opt)
{
	if (opt != 1)
		return m_origin_offs;

	math::v4 offs = m_origin_offs;
	math::v4 *target = AnimLastTarget(FindAnim(ANIM_ORIGIN));

	if (target != NULL)
		offs = *target;
	target = AnimLastTarget(FindAnim(ANIM_SIZE));
	if (target != NULL)
		offs = calc_origin_offset(*target, offs, m_origin_x, m_origin_y, m_origin_z);
	return math::v4(offs);
}

int32_t Widget::SetOriginOffset_core(const math::v4 &offs, uint32_t notify_layout)
{
	m_origin_offs = offs;
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::SetOrigin(uint32_t ax, uint32_t ay, uint32_t az, Timer *t)
{
	m_origin_x = ax;
	m_origin_y = ay;
	m_origin_z = az;

	math::v4 offs = calc_origin_offset(m_size, m_origin_offs, ax, ay, az);

	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, GetOriginOffset(0), offs,
							    &AnimListener::s_prop_origin, NULL, NULL);

		SetAnimListener(ANIM_ORIGIN, listener, 2);
		return 0;
	}
	KillAnimType(ANIM_ORIGIN);
	m_origin_offs = offs;
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::SetOriginOffset(const math::v4 &offs, Timer *t, int32_t id, int32_t opt,
				float delay, AnimCB cb_fin, void *usrdata)
{
	m_origin_x = 3;
	m_origin_y = 3;
	m_origin_z = 3;
	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, GetOriginOffset(0), offs,
							    &AnimListener::s_prop_origin, cb_fin, usrdata);

		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_ORIGIN;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_ORIGIN);
	m_origin_offs = offs;
	SetUpdateParam(1);
	return 0;
}

math::v4 Widget::calc_origin_offset(const math::v4 &size, const math::v4 &offs, uint32_t ax,
				    uint32_t ay, uint32_t az)
{
	if ((ax | ay | az) == 0)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);

	float x = offs.extract_x();
	float y = offs.extract_y();
	float z = offs.extract_z();
	float w = offs.extract_w();

	switch (ax) {
	case 1:
		x = 0.5f * size.extract_x();
		break;
	case 2:
		x = -size.extract_x() * 0.5f;
		break;
	case 0:
		x = 0.0f;
		break;
	default:
		break;
	}
	switch (ay) {
	case 1:
		y = 0.5f * size.extract_y();
		break;
	case 2:
		y = -size.extract_y() * 0.5f;
		break;
	case 0:
		y = 0.0f;
		break;
	default:
		break;
	}
	switch (az) {
	case 1:
		z = 0.5f * size.extract_z();
		break;
	case 2:
		z = -size.extract_z() * 0.5f;
		break;
	case 0:
		z = 0.0f;
		break;
	default:
		break;
	}
	return math::v4(x, y, z, w);
}

math::v4 Widget::GetAnchorOffset(int32_t opt)
{
	math::v4 result = m_anchor_offs;

	if (opt == 1) {
		AnimQueue *anim = FindAnim(ANIM_ANCHOR);

		if (anim != NULL) {
			AnimQueueBase *queue = anim->m_queue[0];

			if (queue != NULL) {
				AnimListener *listener = static_cast<AnimListener *>(queue->GetBack());

				if (listener != NULL) {
					math::v4 *target = listener->GetTargetV4();

					if (target != NULL)
						result = *target;
				}
			}
		}
	}
	return result;
}

int32_t Widget::SetAnchorOffset_core(const math::v4 &offs, uint32_t notify_layout)
{
	m_anchor_offs = offs;
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::SetAnchor(uint32_t ax, uint32_t ay, uint32_t az, Timer *t)
{
	Widget *parent = m_parent;

	if (parent == NULL)
		return -1;
	m_anchor_x = ax;
	m_anchor_y = ay;
	m_anchor_z = az;

	math::v4 offs = parent->calc_anchor_offset(*parent->GetSize(0), m_anchor_offs, ax, ay, az);

	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, GetAnchorOffset(0), offs,
							    &AnimListener::s_prop_anchor, NULL, NULL);

		SetAnimListener(ANIM_ANCHOR, listener, 2);
		return 0;
	}
	KillAnimType(ANIM_ANCHOR);
	m_anchor_offs = offs;
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::SetAnchorOffset(const math::v4 &offs, Timer *t, int32_t id, int32_t opt,
				float delay, AnimCB cb_fin, void *usrdata)
{
	m_anchor_x = 3;
	m_anchor_y = 3;
	m_anchor_z = 3;
	if (t != NULL) {
		AnimListener *listener = new AnimListenerV4(t, this, GetAnchorOffset(0), offs,
							    &AnimListener::s_prop_anchor, cb_fin, usrdata);

		if ((id & 0xFF0000) != 0)
			id = (id & 0xFF000000) | ANIM_ANCHOR;
		SetAnimListener_withDelay(id, listener, delay, opt);
		return 0;
	}
	KillAnimType(ANIM_ANCHOR);
	m_anchor_offs = offs;
	SetUpdateParam(1);
	return 0;
}

math::v4 Widget::calc_anchor_offset(const math::v4 &size, const math::v4 &offs, uint32_t ax,
				    uint32_t ay, uint32_t az)
{
	if ((ax | ay | az) == 0)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	if (ax == 4 && ay == 4 && az == 4)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);

	float x = offs.extract_x();
	float y = offs.extract_y();
	float z = offs.extract_z();
	float w = offs.extract_w();

	if (ax == 1) {
		x = 0.5f * size.extract_x();
	} else if (ax == 2) {
		x = -size.extract_x() * 0.5f;
	} else if (ax != 0) {
		if (ax == 4)
			x = m_origin_offs.extract_x();
	} else {
		x = 0.0f;
	}
	if (ay == 1) {
		y = 0.5f * size.extract_y();
	} else if (ay == 2) {
		y = -size.extract_y() * 0.5f;
	} else if (ay != 0) {
		if (ay == 4)
			y = m_origin_offs.extract_y();
	} else {
		y = 0.0f;
	}
	if (az == 1) {
		z = 0.5f * size.extract_z();
	} else if (az == 2) {
		z = -size.extract_z() * 0.5f;
	} else if (az != 0) {
		if (az == 4)
			z = m_origin_offs.extract_z();
	} else {
		z = 0.0f;
	}
	if (ax == 110 || ax == 111 || ay == 111 || ay == 110) {
		math::v4 scene_size = *m_scene->GetSize(0);

		if ((m_update_flag & 7) != 0)
			UpdateMatrix();

		math::v4 world = m_world_matrix.w();

		if (ax == 110)
			x = 0.5f * scene_size.extract_x() + world.extract_x();
		else if (ax == 111)
			x = -scene_size.extract_x() * 0.5f + world.extract_x();
		if (ay == 110)
			y = 0.5f * scene_size.extract_y() + world.extract_y();
		else if (ay == 111)
			y = -scene_size.extract_y() * 0.5f + world.extract_y();
	}
	return math::v4(x, y, z, w);
}

extern "C" void ScePafWidget_55D83675(Widget *widget, AdjustParam *param)
{
	AdjustParam *previous = widget->m_adjust_param;

	if (previous == param)
		return;
	if (previous != NULL && --previous->m_ref_count == 0)
		delete previous;
	widget->m_adjust_param = param;
	param->m_ref_count++;
	widget->UpdateSize();
}

extern "C" int32_t ScePafWidget_B1C23134(Widget *widget, const AdjustParam &param)
{
	if (widget->m_adjust_param == NULL)
		return -1;
	if (sce_paf_memcmp(widget->m_adjust_param, &param, sizeof(AdjustParam)) != 0) {
		int32_t refs = widget->m_adjust_param->m_ref_count;

		*widget->m_adjust_param = param;
		widget->m_adjust_param->m_ref_count = refs;
		widget->UpdateSize();
	}
	return 0;
}

int32_t Widget::SetAdjust(uint32_t ax, uint32_t ay, uint32_t az)
{
	m_adjust_x = ax;
	m_adjust_y = ay;
	m_adjust_z = az;
	UpdateLayoutAttribute();
	UpdateAdjustSize();
	return 0;
}

void Widget::UpdateAdjustChildSize(Widget *child)
{
	if (child == NULL || (child->m_adjust_x | child->m_adjust_y | child->m_adjust_z) == 0)
		return;

	float size[4];
	float parent_size[4];

	*(math::v4 *)size = *child->GetSize(0);
	if (m_scene != NULL) {
		*(math::v4 *)parent_size = *m_scene->GetSize(0);
	} else {
		math::rect screen(Screen::s_root_screen->sp);
		float width = screen.width();
		float height = screen.height();

		*(math::v4 *)parent_size = math::v4(width, height, 0.0f, 0.0f);
	}

	uint32_t ax = child->m_adjust_x;
	uint32_t ay = child->m_adjust_y;
	uint32_t az = child->m_adjust_z;

	if (ax == 1)
		size[0] = m_size.extract_x();
	else if (ax == 4)
		size[0] = parent_size[0];
	if (ay == 1)
		size[1] = m_size.extract_y();
	else if (ay == 4)
		size[1] = parent_size[1];
	if (az == 1)
		size[2] = m_size.extract_z();
	child->SetSize_core_force(*(math::v4 *)size, 0xFF02, false);
}


/* the draw object drawn to a surface as the widget is, with its scene's font flags (name
   ours) */
static inline void SetDrawObjSurfaceDraw(Widget *widget, graph::DrawObj *obj)
{
	graph::SurfaceDrawParam param;

	param.m_font_flags = widget->m_scene != NULL ? widget->m_scene->m_font_flags : 128;
	param.m_surface_draw = widget->m_surface_draw;
	obj->SetSurfaceDraw(param);
}

void Widget::AddDrawObj(const DrawObjEntry &entry, bool update)
{
	if (m_drawobjs.capacity() <= m_drawobjs.size())
		ReserveDrawObj(m_drawobjs.capacity() + 2);
	m_drawobjs.push_back(entry);
	if (entry.m_obj.get() != NULL)
		SetDrawObjSurfaceDraw(this, entry.m_obj.get());
	if (update)
		UpdateSize();
}

void Widget::ReplaceDrawObj(const DrawObjEntry &entry, bool update)
{
	vector<DrawObjEntry>::iterator it;

	for (it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == entry.m_attr.m_id)
			break;
	}
	if (it == m_drawobjs.end())
		return;
	*it = entry;
	if (entry.m_obj.get() != NULL)
		SetDrawObjSurfaceDraw(this, entry.m_obj.get());
	if (update)
		UpdateSize();
}

graph::DrawObj *Widget::GetDrawObj(int32_t obj)
{
	vector<DrawObjEntry>::iterator it;

	for (it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == obj)
			break;
	}
	if (it != m_drawobjs.end())
		return it->m_obj.get();
	return NULL;
}

int32_t Widget::EventExecBroadcast(Event *e)
{
	if (e == NULL)
		return -1;

	uint32_t type = e->m_type;
	int32_t result = 0;

	if ((type & 0x20) != 0)
		return ScePafWidget_EB1D1CBC(this, e);
	if ((type & 0x40) != 0) {
		if (((e->m_detail >> 20) & 0xF) == 0 || IsTouchRearEventEnabled() ||
		    m_touch_rear_children != 0)
			return ScePafWidget_EB1D1CBC(this, e);
	} else {
		HandlerProc(e);
	}
	return result;
}

int32_t Widget::EventExecDirect(Event *e)
{
	if (e == NULL)
		return -1;

	int32_t result;
	uint32_t type = e->m_type;

	if ((type & 0x20) != 0 || (type & 0x40) != 0) {
		HitInfo info;

		result = m_env->GetHitStack(&info, this);
		type = e->m_type;
		if ((type & 0x10000) != 0) {
			result = HandlerProc(e);
			type = e->m_type;
		} else if ((type & 0x20000) != 0 || (type & 0x40000) != 0) {
			HandlerProc(e);
			result = ScePafWidget_38CDFD1D(this, e);
			type = e->m_type;
		}
		if ((type & 0x1000000) != 0) {
			Environment *env = m_env;

			env->m_ev_subrouting.clear();
			if ((e->m_type & 0x10000000) != 0) {
				Event cancel(0x46, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

				env = m_env;
				if ((e->m_type & 0x20) != 0)
					cancel.m_detail = (cancel.m_detail & 0xFFFF0000) | 0x27;
				env->m_ev_queue.Cancel(cancel, 1);
			}
		}
	} else {
		result = HandlerProc(e);
		if ((e->m_type & 0x1000000) != 0) {
			Environment *env = m_env;

			env->m_ev_subrouting.clear();
		}
	}
	return result;
}

int32_t Widget::EventExecDo(Event *e)
{
	int32_t result = Handler::DoEvent(e->m_dotype, e);

	if ((e->m_type & 0x1000000) != 0) {
		Environment *env = m_env;

		if (env != NULL) {
			env->m_ev_subrouting.clear();
			if ((e->m_type & 0x10000000) != 0) {
				Event cancel(0x46, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

				env = m_env;
				if ((e->m_type & 0x20) != 0)
					cancel.m_detail = (cancel.m_detail & 0xFFFF0000) | 0x27;
				env->m_ev_queue.Cancel(cancel, 1);
			}
		}
	}
	return result;
}

int32_t Widget::DoEvent(int32_t type, Event *e)
{
	if ((m_hold_count > 0 || m_destroy_protected) && (uint16_t)e->m_type != 0x1002)
		return -1;
	if (m_scene->m_debug_mode == 2 || m_scene->m_debug_mode == 3)
		return -1;

	map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.find(type);

	if (it != m_map_e_listener.end()) {
		Event ev(*e);

		ev.m_type = (e->m_type & 0xFF0FFFFF) | Event::MODE_DO;
		ev.m_dotype = type;
		ev.m_target = this;
		if ((type & 0x20000000) != 0)
			return EventExecDo(&ev);
		if (m_env != NULL)
			m_env->m_ev_doevent.push_back(ev);
	}
	return 0;
}

int32_t Widget::HandlerProc(Event *e)
{
	if (e == NULL)
		return -1;
	if (m_env != NULL)
		return Handler::HandlerProc(e);
	return -1;
}

int32_t Widget::HandleTouchEvent(Event *e)
{
	Handler::HandleTouchEvent(e);
	return 0;
}

void Widget::EnableFocusEvent(bool on_off)
{
	uint32_t enable = m_event_enable;

	if ((enable & 0x400) == 0 || !on_off) {
		if (on_off || (m_event_enable & 0x400) != 0) {
			m_event_enable = on_off ? (enable | 0x400) : (enable & ~0x400);
			if (!on_off && m_focused)
				ReleaseFocus();

			Widget *root = m_parent != NULL ? FindFocusRoot(m_parent) : NULL;

			if (!on_off && root != NULL) {
				Widget *holder = FindFocusRoot(root);
				uint32_t state = 0;

				if (holder != NULL)
					state = holder->m_focus_root->m_state;
				if (state == (uint32_t)this)
					root->SetFocusRootState(0);
			}
			if (m_parent != NULL)
				m_parent->CountFocusEnableChildren(on_off ? 1 : -1);
		}
	}
}

extern "C" int32_t ScePafWidget_A1193AAB(Widget *widget, bool on, bool option)
{
	widget->m_touch_region_registered = on;
	widget->m_touch_region_option = option;

	Environment *env = widget->m_env;

	if (env == NULL)
		return 0;
	if (!on) {
		env->RemoveTouchDeviceRegion(widget);
		return 0;
	}

	int32_t result = env->RegisterTouchDeviceRegion(widget);

	if (result < 0)
		widget->m_touch_region_registered = false;
	return result;
}

bool Widget::IsMorphResolution(uint32_t &id, uint32_t &pre, float &time, float &rate)
{
	uint32_t next;

	if (m_scene != NULL && m_scene->IsResolutionActMorph(m_resolution_id)) {
		id = m_resolution_id;
		next = m_resolution_id_pre;
	} else {
		id = m_resolution_id_pre;
		next = -1;
	}
	pre = next;

	Timer *timer = m_resolution_timer;

	if (timer != NULL) {
		time = timer->GetCurrTime();
		rate = timer->m_mapped_x;
	} else {
		time = 1.0f;
		rate = 1.0f;
	}
	return timer != NULL;
}

vector<Widget::DrawObjEntry>::iterator Widget::FindDrawObj(int32_t id)
{
	vector<DrawObjEntry>::iterator it = m_drawobjs.begin();
	vector<DrawObjEntry>::iterator end = m_drawobjs.end();

	while (it != end) {
		if (it->m_attr.m_id == (uint32_t)id)
			break;
		++it;
	}
	return it;
}

WidgetSoundParam::Sound *Widget::FindSound(int32_t id)
{
	WidgetSoundParam::Sound *sounds = m_sounds;

	if (sounds == NULL)
		return NULL;

	uint32_t num = m_sound_num;

	for (uint32_t i = 0; i < num; i++) {
		if (sounds[i].m_index == (uint32_t)id)
			return &sounds[i];
	}
	return NULL;
}

AdjustParam *Widget::GetHitRegion()
{
	if (m_adjust_param == NULL)
		ScePafWidget_55D83675(this, new AdjustParam());
	return m_adjust_param;
}

int32_t Widget::SetHitRegion(const AdjustParam &param)
{
	if (m_adjust_param == NULL)
		ScePafWidget_55D83675(this, new AdjustParam());
	return ScePafWidget_B1C23134(this, param);
}

math::v4 Widget::GetFocusShapePos()
{
	math::v4 pos = GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	if (m_focus_shape != NULL)
		pos = pos + math::v4(m_focus_shape->m_offset, 0.0f);
	return pos;
}

math::v4 Widget::GetFocusShapeWorldPos()
{
	if ((m_update_flag & 7) != 0)
		UpdateMatrix();

	math::v4 pos = m_world_matrix.w();

	if (m_focus_shape != NULL)
		pos = pos + math::v4(m_focus_shape->m_offset, 0.0f);
	return pos;
}

math::v4 Widget::GetFocusShapeSize()
{
	math::v4 size = *GetSize(1);

	if (m_focus_shape != NULL) {
		math::v2 shape_size = m_focus_shape->m_size;

		if (!(shape_size == math::v2(0.0f, 0.0f)))
			size = math::v4(shape_size.extract_x(), shape_size.extract_y(), 0.0f, 0.0f);
	}
	return size;
}

void Widget::MakeLookAtQuat(math::v4 *facing, math::v4 *up, math::quaternion *quat)
{
	facing->set_w(0.0f);
	up->set_w(0.0f);

	math::v4 forward = facing->unit();
	math::v4 right = CrossKeepW(*up, forward).unit();
	math::v4 above = CrossKeepW(forward, right);
	math::quaternion rotation(math::matrix(right, above, forward, math::v4(0.0f, 0.0f, 0.0f, 1.0f)));

	*quat = NormalizeQuat(rotation);
}

int32_t Widget::LookAt(const math::v4 &target, const math::v4 &up)
{
	math::v4 facing = target - m_pos;
	math::v4 above = up - m_pos;

	if (up.extract_w() == -10.0f)
		above = math::v4(0.0f, 1.0f, 0.0f, 0.0f);
	MakeLookAtQuat(&facing, &above, &m_quat);
	KillAnimType(65542);
	KillAnimType(65543);
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::LookAt(Widget *target, const math::v4 &target_offs, Widget *up, const math::v4 &up_offs)
{
	if (target == NULL)
		return -1;

	math::v4 facing = target->GetPos(0, m_parent, target_offs) - m_pos;
	math::v4 above(0.0f, 1.0f, 0.0f, 0.0f);

	if (up != NULL)
		above = up->GetPos(0, m_parent, up_offs) - m_pos;
	MakeLookAtQuat(&facing, &above, &m_quat);
	KillAnimType(65542);
	KillAnimType(65543);
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::SetPosture(const math::v4 &facing, const math::v4 &up, Widget *ref)
{
	math::v4 f;
	math::v4 u;

	if (ref != NULL) {
		UpdateMatrix();

		math::quaternion ref_rot = ref->m_world_rot;
		math::quaternion rel = QuatMul(m_world_rot.inverse(), ref_rot);
		math::matrix m = rel.toMatrix(0.0001f);

		f = math::v4(m.getImpl() * facing.getImpl());
		u = math::v4(m.getImpl() * up.getImpl());
	} else {
		f = facing;
		u = up;
	}
	MakeLookAtQuat(&f, &u, &m_quat);
	KillAnimType(65542);
	KillAnimType(65543);
	SetUpdateParam(1);
	return 0;
}

/* a pointer member deleted and set to -1, as Sony's destructors leave them (name ours) */
template<class T>
static inline __attribute__((always_inline)) void DeletePoison(T *&pointer)
{
	delete pointer;
	pointer = reinterpret_cast<T *>(-1);
}

template<class T>
static inline __attribute__((always_inline)) void DeleteArrayPoison(T *&pointer)
{
	delete[] pointer;
	pointer = reinterpret_cast<T *>(-1);
}

Widget::~Widget()
{
	if (s_list_restruct.size() != 0) {
		for (list<RestructInfo>::iterator it = s_list_restruct.begin(); it != s_list_restruct.end();
		     it++) {
			if (it->m_parent == this)
				common_internal::on_error();
			if (it->m_widget == this)
				common_internal::on_error();
			if (it->m_sibling == this)
				common_internal::on_error();
		}
	}
	while (m_children.size() != 0) {
		Widget *child = m_children.front();

		if (child == NULL)
			continue;
		delete child;
	}
	if ((uint8_t)m_layout_notify_mask != 0 && (m_layout_notify_mask & 0xFF00) != 0) {
		if (m_parent != NULL && m_parent->m_layouter != NULL)
			m_parent->m_layouter->SetDirty(1);
		if (m_layouter != NULL)
			m_layouter->SetDirty(1);
	}
	if (m_parent != NULL)
		m_parent->RemoveChild(this);
	m_parent = reinterpret_cast<Widget *>(-1);
	DeletePoison(m_layouter);
	DeletePoison(m_animator);
	DeletePoison(m_focuser);
	DeletePoison(m_focus_shape);
	m_focus_parts.clear();
	DeletePoison(m_layout_rect);
	DeleteArrayPoison(m_hit_box);
	DeletePoison(m_inverse_matrix);
	if (m_adjust_param != NULL) {
		if (m_adjust_param->m_ref_count-- == 1)
			delete m_adjust_param;
		m_adjust_param = NULL;
	}
	for (int32_t i = 0; i < 2; i++)
		DeletePoison(m_user_data2[i]);
	DeletePoison(m_resolution_timer_next);
	DeletePoison(m_resolution_timer);
	DeleteArrayPoison(m_pos_array);
	DeleteArrayPoison(m_size_array);
	DeleteArrayPoison(m_rot_array);
	DeleteArrayPoison(m_drag_limit);
	DeleteArrayPoison(m_pinch_limit);
	for (map<int32_t, AnimQueue *>::iterator it = m_anims.begin(); it != m_anims.end(); it++)
		delete it->second;
	m_anims.clear();
	if (m_timer_pending) {
		map<Widget *, Timer *>::iterator it = s_destroy_timers.find(this);

		if (it != s_destroy_timers.end()) {
			delete it->second;
			s_destroy_timers.erase(it);
		}
		if (m_destroy_protected) {
			m_destroy_protected = false;
			s_destroying_count--;
		}
	}
	if (m_show_timer_pending) {
		map<Widget *, Timer *>::iterator it = s_show_timers.find(this);

		if (it != s_show_timers.end()) {
			delete it->second;
			s_show_timers.erase(it);
		}
	}
	m_env = reinterpret_cast<Environment *>(-1);
	m_scene = reinterpret_cast<Scene *>(-1);
	m_adjust_param = reinterpret_cast<AdjustParam *>(-1);
	DeletePoison(m_focus_root);
	DeletePoison(m_focus_referrers);
	s_instance_count--;
}

Widget::Widget(Widget *parent, CreateParam *param) : Handler(0x5080)
{
	m_pos = math::v4(0.0f, 0.0f, 0.0f, 1.0f);
	m_scale = math::v4(1.0f, 1.0f, 1.0f, 0.0f);
	m_size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_rot = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_world_rot = math::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
	m_local_matrix = math::matrix::Identity();
	m_world_matrix = math::matrix::Identity();
	m_align_offs = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_anchor_offs = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_origin_offs = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_inverse_matrix = NULL;
	m_quat = math::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
	m_state = 0;
	m_user_data[0] = NULL;
	m_user_data[1] = NULL;
	m_user_data2[0] = NULL;
	m_user_data2[1] = NULL;
	m_env = NULL;
	m_parent = NULL;
	m_scene = NULL;
	reserved3b0 = 0x7F;
	m_dim_held = true;
	reserved3c0 = 0x7F;
	m_dim_held_enable = true;
	m_prepare_count = 0;
	m_focus_count = 0;
	m_touch_rear_children = 0;
	m_update_flag = 3;
	m_transition = 0xFF;
	m_keycode = 0;
	m_priority = 0;
	m_layer = 0;
	m_layer_old = 0;
	m_modal_mode = 2;
	m_modal_option = 0;
	m_show_hold = 0;
	reserved3x1 = 0;
	m_clip_mode = 0;
	reserved3x3 = 0;
	m_draw_target = 0;
	m_flip_x = false;
	m_flip_y = false;
	m_flip_z = false;
	reserved3x4 = false;
	m_touch_front_event = true;
	m_focus_in_group = true;
	m_focus_direction_off = true;
	m_focus_target = true;
	m_draw_before_children = true;
	m_debug_events = true;
	m_input_target = true;
	m_touch_gesture_mask = 0;
	m_show_alpha = 1.0f;
	m_show_a_sum = 1.0f;
	m_meta_alpha = 1.0f;
	m_meta_a_sum = 1.0f;
	m_color_alpha = 1.0f;
	m_color = 0xFFFFFFFF;
	m_layout_rect = NULL;
	m_resolution_timer = NULL;
	m_resolution_timer_next = NULL;
	m_pos_array = NULL;
	m_size_array = NULL;
	m_rot_array = NULL;
	m_resolution_id = 0xFF;
	m_resolution_id_pre = 0xFF;
	m_resolution_id_next = 0xFF;
	m_pos_num = 0;
	m_size_num = 0;
	m_rot_num = 0;
	m_drag_state[0] = NULL;
	m_drag_state[1] = NULL;
	m_hit_box = NULL;
	m_hit_box_mode = 4;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 2;
	m_hit_detect_b_mode = 2;
	m_hit_check = 0;
	reserved5c[0] = 0xFF;
	reserved5c[1] = 0xFF;
	m_align_x = 0;
	m_align_y = 0;
	m_align_z = 0;
	m_origin_x = 0;
	m_origin_y = 0;
	m_origin_z = 0;
	m_anchor_x = 4;
	m_anchor_y = 4;
	m_anchor_z = 4;
	m_adjust_x = 0;
	m_adjust_y = 0;
	m_adjust_z = 0;
	m_drag_quant_x = 0;
	m_drag_quant_y = 0;
	m_drag_quant_z = 0;
	m_drag_limit = NULL;
	m_pinch_limit = NULL;
	m_adjust_param = NULL;
	m_layouter = NULL;
	m_animator = NULL;
	m_focuser = NULL;
	m_focus_shape = NULL;
	m_focus_root = NULL;
	m_focus_referrers = NULL;
	m_layout_notify_mask = 0x9FF;
	m_animator_notify_mask = 0xFFFF;
	for (int32_t i = 0; i < 8; i++)
		reserved7b[i] = 0;
	m_sound_mask = 0;
	m_sound_num = 0;
	m_sounds = NULL;
	m_children.clear();
	m_anims.clear();
	m_touch_captures.clear();
	m_user_data[0] = NULL;
	m_user_data[1] = NULL;
	m_user_data2[0] = NULL;
	m_user_data2[1] = NULL;
	SetDragMode(0);
	SetPinchMode(0);
	if (param != NULL) {
		m_parent_protected = param->m_parent_protected;
		m_under_template = param->m_under_template;
	}
	if (parent != NULL) {
		SetParent_core(parent, NULL);
	} else {
		m_scene = NULL;
		m_update_flag = 0;
	}
	ChangeState(1, -1);

	Focuser *focuser = new Focuser(DefaultFocusCB);

	delete m_focuser;
	m_focuser = focuser;
	if (focuser != NULL)
		focuser->m_widget = this;
	s_instance_count++;
}

void Widget::SetBaseDestroyCount(int32_t count)
{
	s_base_destroy_count = count;
}


int32_t Widget::s_base_destroy_count;
int32_t Widget::s_destroying_count;
int32_t Widget::s_instance_count;
list<RestructInfo> Widget::s_list_restruct;

int32_t Widget::GetBaseDestroyCount()
{
	return s_base_destroy_count;
}
}
}
