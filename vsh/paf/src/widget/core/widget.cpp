/*
 * paf::ui::Widget, as far as it is converted: the rest of its functions are
 * still ScePafWidget's.
 */
#include <stddef.h>
#include <std/algorithm>
#include <std/math.h>
#include <common/common_internal.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/pad/pad.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/panel_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <timer/timerqueue.h>
#include <widget/core/animlistener.h>
#include <widget/core/layouter.h>
#include <widget/core/widget.h>
#include <widget/core/focus.h>
#include <widget/core/uiinputlistener.h>
#include <widget/core/widget_gesture.h>
#include <widget/core/show_held.h>
#include <widget/w_camera.h>
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

/* Sony's frame end calls the swapped draw objects' clear rather than inlining it. */
template<>
inline __attribute__((noinline)) void list<common::SharedPtr<graph::DrawObj> >::clear()
{
	if (head != NULL) {
		_Node *node = head->next;

		while (node != head) {
			_Node *next = node->next;

			destroy(&node->value);
			operator delete(node);
			node = next;
		}
		head->prev = head;
		head->next = head;
		count = 0;
	}
}

/* Sony has the lookup in the touch capture map inline. */
template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<uint32_t, ui::TouchCapture *, less<uint32_t> > >::iterator
_Tree<_Tmap_traits<uint32_t, ui::TouchCapture *, less<uint32_t> > >::find(const uint32_t &key)
{
	return iterator(_Find(key));
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

/* the inverse world matrix where it keeps one, of the world matrix moved back to its origin
   where it has one (0x8114be48; name ours) */
static __attribute__((noinline)) void UpdateInverseMatrix(Widget *w)
{
	math::matrix *inverse = w->m_inverse_matrix;

	if (inverse == NULL)
		return;

	math::v4 offs = w->m_origin_offs;

	if ((w->m_origin_x | w->m_origin_y | w->m_origin_z) == 0 &&
	    offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
		inverse->assign(w->m_world_matrix.inverse());
	} else {
		math::v4 offset(-offs.getImpl());

		offset.set_w(1.0f);
		inverse->assign((w->m_world_matrix * math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
								    math::v4(0.0f, 1.0f, 0.0f, 0.0f),
								    math::v4(0.0f, 0.0f, 1.0f, 0.0f),
								    offset))
					.inverse());
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

/* the local matrix made again from its position (flipped on the axes it flips), rotation and
   scale, where m_update_flag asks for it (0x8114c0aa; name ours) */
static __attribute__((noinline)) void UpdateLocalMatrix(Widget *w)
{
	if ((w->m_update_flag & 1) == 0)
		return;

	math::v4 pos = w->m_pos;

	Widget::s_upmat_count_L++;

	float flip_x = 1.0f;
	float flip_y = 1.0f;
	float flip_z = 1.0f;

	if (w->m_flip_x)
		flip_x = -1.0f;
	if (w->m_flip_y)
		flip_y = -1.0f;
	if (w->m_flip_z)
		flip_z = -1.0f;
	w->m_local_matrix = math::matrix(
		math::v4(1.0f, 0.0f, 0.0f, 0.0f), math::v4(0.0f, 1.0f, 0.0f, 0.0f),
		math::v4(0.0f, 0.0f, 1.0f, 0.0f),
		math::v4(pos.extract_x() * flip_x, pos.extract_y() * flip_y, pos.extract_z() * flip_z,
			 1.0f));

	math::matrix rotation = w->m_quat.toMatrix(0.0001f);
	math::matrix local = w->m_local_matrix;
	math::v4 scale(sce::Vectormath::Simd::Aos::maxPerElem(
		sce::Vectormath::Simd::Aos::Vector4(0.0001f), w->m_scale.getImpl()));

	w->m_update_flag = (w->m_update_flag & 0xFA) | 4;
	w->m_local_matrix = local * rotation *
			    math::matrix(math::v4(scale.extract_x(), 0.0f, 0.0f, 0.0f),
					 math::v4(0.0f, scale.extract_y(), 0.0f, 0.0f),
					 math::v4(0.0f, 0.0f, scale.extract_z(), 0.0f),
					 math::v4(0.0f, 0.0f, 0.0f, 1.0f));
}

static const math::matrix &WorldMatrix(Widget *w);

const math::matrix &Widget::UpdateMatrix()
{
	UpdateLocalMatrix(this);
	if ((m_update_flag & 2) != 0) {
		s_upmat_count_W++;

		math::matrix parent = math::matrix::Identity();
		math::quaternion parent_rot = math::quaternion::Identity();
		Widget *scene = m_scene;

		if (scene != NULL && this != scene) {
			uint32_t ax = m_anchor_x;
			Widget *owner = m_parent;
			uint32_t ay, az;

			if ((ax == 4 || ax == 100) && ((ay = m_anchor_y) == 4 || ay == 100) &&
			    ((az = m_anchor_z) == 4 || az == 100)) {
				/* anchored to its parent's origin, or to its grandparent's */
				if (ax == 4 && ay == 4 && az == 4)
					parent = owner->GetOriginWorldMatrix();
				else if (owner != scene)
					parent = owner->m_parent->GetOriginWorldMatrix();
			} else {
				parent = WorldMatrix(m_parent);
				if ((m_anchor_y | m_anchor_x | m_anchor_z) != 0) {
					Widget *base = m_parent;

					m_anchor_offs = base->calc_anchor_offset(*base->GetSize(0),
										 m_anchor_offs, m_anchor_x,
										 m_anchor_y, m_anchor_z);

					math::v4 offset(-m_anchor_offs.getImpl());

					offset.set_w(1.0f);
					parent = parent * math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
								       math::v4(0.0f, 1.0f, 0.0f, 0.0f),
								       math::v4(0.0f, 0.0f, 1.0f, 0.0f),
								       offset);
				}
			}
			parent_rot = m_parent->m_world_rot;
		}
		m_world_matrix = parent * m_local_matrix;
		UpdateInverseMatrix(this);

		math::quaternion rot = QuatMul(parent_rot, m_quat);

		m_update_flag = (m_update_flag & 0xF9) | 4;
		m_world_rot = rot;
	}
	return m_world_matrix;
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
map<Widget *, Timer *> s_destroy_timers;
static map<Widget *, Timer *> s_show_timers;

/* the draw objects SwapDrawObj took out, kept until the frame ends (name ours) */
list<common::SharedPtr<graph::DrawObj> > s_swapped_drawobjs;

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

extern "C" int32_t ScePafWidget_026C9E8C(Widget *widget, const math::v4 &pos, const math::v4 *start,
					 const math::v4 *target, Timer *t, int32_t id, int32_t opt,
					 float delay, AnimCB cb_fin, void *usrdata)
{
	const math::v4 *tangents[2] = { start, target };

	return widget->SetPosHermite(pos, tangents, t, id, opt, delay, cb_fin, usrdata);
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

extern "C" void ScePafWidget_A96A0B4A(Widget *widget, int32_t add)
{
	if (add != 0) {
		if (widget->m_show_hold == 0) {
			if (add < 0)
				return;
			widget->ReleaseShowHolds(true);
		}
		widget->m_show_hold = widget->m_show_hold + add;
	}
}

extern "C" bool ScePafWidget_DB0FFD33(Widget *widget)
{
	if (widget != NULL && (widget->m_drag_begun || (widget->IsInputTarget() && widget->m_meta_a_sum >= 0.5f)) &&
	    !widget->m_fit_to_scene && !widget->m_culled)
		return true;
	return false;
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

/* its world matrix turns it about one of the axes asked by more than the limit: either of the two
   terms that rotation about the axis puts in the matrix above it (name ours) */
bool Widget::IsRotated(bool x, bool y, bool z, float limit)
{
	const math::matrix &m = GetWorldMatrix();

	if (x) {
		float zy = __builtin_fabsf(m.z().extract_y());

		if (limit < __builtin_fabsf(m.y().extract_z()) || limit < zy)
			return true;
	}
	if (y) {
		float zx = __builtin_fabsf(m.z().extract_x());

		if (limit < __builtin_fabsf(m.x().extract_z()) || limit < zx)
			return true;
	}
	if (z) {
		float yx = __builtin_fabsf(m.y().extract_x());

		if (limit < __builtin_fabsf(m.x().extract_y()) || limit < yx)
			return true;
	}
	return false;
}

math::rect Widget::CalcClipRect()
{
	using sce::Vectormath::Simd::Aos::Vector2;
	using sce::Vectormath::Simd::Aos::Vector4;
	using sce::Vectormath::Simd::Aos::absPerElem;
	using sce::Vectormath::Simd::Aos::maxPerElem;

	Scene *scene = m_scene;

	/* without a camera: a point at its position */
	if (scene == NULL || scene->m_camera == NULL) {
		const math::matrix &world = GetWorldMatrix();

		return math::rect(math::v2(world.w().getImpl().getXY()), math::v2(0.0f, 0.0f));
	}

	/* turned out of the screen's plane: the whole scissor */
	bool rotated = IsRotated(true, true, false, 0.15707964f);

	if (rotated && m_scene != NULL && m_layout_rect == NULL)
		return math::rect(m_scene->m_screen->sp);

	Camera *camera = NULL;

	if (m_scene != NULL)
		camera = m_scene->m_camera;

	math::matrix origin = m_parent->GetOriginWorldMatrix();
	Vector4 center;
	Vector2 corner;
	Vector2 width, height;

	if (m_layout_rect == NULL) {
		/* its own size around its position */
		center = GetWorldMatrix().w().getImpl();

		const math::matrix &world = GetWorldMatrix();
		Vector2 size = m_size.getImpl().getXY();

		width = (world.getImpl() * Vector4(size.getX(), size.getY(), 0.0f, 0.0f)).getXY();
		corner = absPerElem(
			(world.getImpl() * Vector4(-size.getX(), size.getY(), 0.0f, 0.0f)).getXY());
	} else {
		/* the rectangle its layout gave it, in its parent */
		math::v2 middle = m_layout_rect->center();

		center = origin.getImpl() *
			 Vector4(middle.extract_x(), middle.extract_y(), 0.0f, 1.0f);

		math::v2 size = m_layout_rect->size();
		const math::matrix &world = GetWorldMatrix();

		corner = absPerElem((world.getImpl() * Vector4(size.extract_x(), -size.extract_y(),
								  0.0f, 0.0f))
					    .getXY());
		width = (world.getImpl() * Vector4(size.extract_x(), size.extract_y(), 0.0f, 0.0f))
				.getXY();
	}

	/* projected by the camera's distance over its depth */
	Vector2 extent = maxPerElem(corner, absPerElem(width));
	float depth = camera->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_z() -
		      center.getZ();

	ScePafWidget_72F479CA(camera);

	float scale;

	if (depth == 0.0f)
		scale = 0.0f;
	else
		scale = camera->m_distance / depth;

	math::v4 eye = camera->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 size(extent * scale);
	math::v2 middle((center - eye.getImpl()).getXY() * scale);

	return math::rect(middle, size);
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

/* a focus root with nothing in it, and one given back (Sony has both inline) */
static inline FocusRoot *NewFocusRoot()
{
	FocusRoot *root = (FocusRoot *)operator new(sizeof(FocusRoot));

	if (root != NULL) {
		root->m_state = 0;
		root->m_init_focus = NULL;
		root->m_focus_moved = 0;
		root->m_focus_hold = 0;
		root->reserved16 = 0;
		root->m_array = NULL;
		root->m_array_count = 0;
		root->m_array_limit = 0;
		root->m_pending = false;
	}
	return root;
}

static inline void DeleteFocusRoot(FocusRoot *root)
{
	if (root != NULL) {
		if (root->m_array != NULL)
			operator delete(root->m_array);
		root->m_array = (void *)-1;
		root->m_array_count = -1;
		root->m_array_limit = -1;
		operator delete(root);
	}
}

extern "C" void ScePafWidget_CB95F8F2(Widget *widget, int32_t target)
{
	Scene *scene = widget->m_scene;

	if (scene != NULL) {
		if (target == 1) {
			if (scene->m_surface_widgets.size() == 0)
				scene->CreateBgLayer();

			Scene::SurfaceWidget entry;

			__builtin_memset(&entry, 0, 20);
			entry.m_widget = widget;
			scene->m_surface_widgets.push_back(entry);
		}
		widget->m_draw_target = target;
		for (vector<Widget::DrawObjEntry>::iterator it = widget->m_drawobjs.begin();
		     it != widget->m_drawobjs.end(); ++it)
			it->m_obj->m_flags.m_draw_list = target & 3;
	}
}

extern "C" int32_t ScePafWidget_2B1A1493(Widget *widget, int32_t index)
{
	if (widget->m_env == NULL)
		return -1;
	if (((1 << index) & widget->m_sound_mask) == 0)
		return -1;

	WidgetSoundParam::Sound *sound = widget->GetSound(index);

	if (sound == NULL)
		return -1;
	if (sound->m_buffer1 == NULL)
		return -1;

	Environment *env = widget->m_env;

	if (env->m_sound_callback == NULL)
		return -1;
	env->m_sound_callback(sound->m_buffer1, sound->m_size1, sound->m_buffer2, sound->m_size2,
			      sound->m_samplerate, sound->reserved2, false, env->m_sound_callback_data);
	return 0;
}

void Widget::SetGroupCurrent(bool current)
{
	if (m_group_current != current) {
		m_group_current = current;
		if (!current && m_pad_button_port >= 0)
			ReleasePadCaptureButton(255, true);
	}
}

extern "C" int32_t ScePafWidget_D3D0A7C3(Widget *widget, uint32_t port, uint32_t id, bool apply,
					 bool reset_pad)
{
	Environment *env = widget->m_env;

	if (env == NULL)
		return -1;

	uint32_t key = id | (port << 8);
	map<uint32_t, TouchCapture *>::iterator it = widget->m_touch_captures.find(key);

	if (reset_pad) {
		for (int32_t i = 0; i < 5; i++) {
			env->ReleasePadCaptureLow(i);
			widget->m_env->ReleasePadCaptureHigh(i);
			env = widget->m_env;
		}
		if (ScePafWidget_A6B659BB(widget->m_env, 0)) {
			inputdevice::pad::Lock();
			inputdevice::pad::Unlock();
		}
	}
	if (apply) {
		if (it == widget->m_touch_captures.end()) {
			ScePafWidget_364E6985(widget->m_env, port, id, false);
		}
		ScePafWidget_ABF05727(widget->m_env, port, true, widget);
	}

	bool added = false;

	if (it == widget->m_touch_captures.end()) {
		TouchCapture *capture = ScePafWidget_451DE355(widget->m_env, port, id, widget);

		if (capture == NULL)
			return -1;
		widget->m_touch_captures[key] = capture;
		added = true;
	}
	if (widget->m_touch_captures.size() == 1) {
		if (widget->m_touch_gesture_mask != 0 && port == 0) {
			widget->apply_touch_gesture_mask(0, id, widget->m_touch_gesture_mask);
			if ((widget->m_event_enable & 0x400) != 0 && widget->IsTextEventEnabled() &&
			    widget->m_touch_gesture_mask != 0 && !widget->m_env->m_focus_auto_off) {
				widget->m_env->ReleaseFocusBeyond(widget);
				widget->SetFocus(NULL);
			}
		}
	} else if (widget->m_touch_captures.size() > 1) {
		uint32_t mask = widget->m_touch_gesture_mask;

		if ((mask & 0x10) != 0 && port == 0) {
			widget->apply_touch_gesture_mask(0, id, 0x10);
			mask = widget->m_touch_gesture_mask;
		}
		if ((mask & 0x20) != 0 && port == 0)
			widget->apply_touch_gesture_mask(0, id, 0x20);
	}
	if (added) {
		widget->OnTouchCaptureChanged(port, id, 0);

		Event e(0x801, 0x100000, 0x10000, 0, 0, 0, 0, 0);

		widget->DoEvent(537133066, &e);
		e.SetTarget(widget);
		e.Term();
		widget->m_env->m_ev_queue.PushBack(e);
	}
	return 0;
}

extern "C" int32_t ScePafWidget_0E453608(Widget *widget, DragState *drag, TouchInfo *touch)
{
	if (drag == NULL || touch == NULL)
		return -1;
	if (drag->m_id != touch->m_id)
		return -1;
	if (drag->m_port != touch->m_port)
		return -1;

	HitInfo hit;
	DragState *first = widget->m_drag_state[0];
	math::v4 normal = first->m_axis;
	math::v4 point = first->m_origin;
	int32_t result = widget->CalcHitLineToPlane(touch->m_line_start, touch->m_line_end, point, normal, &hit);

	drag->m_hit = hit;
	return result;
}

extern "C" int32_t ScePafWidget_093F1C89(Widget *widget, uint32_t port, uint32_t id, bool release)
{
	uint32_t key = id | (port << 8);
	map<uint32_t, TouchCapture *>::iterator it = widget->m_touch_captures.find(key);

	if (it == widget->m_touch_captures.end())
		return -1;
	widget->remove_touch_gesture_mask(port, id, 0x10);
	if (release)
		return widget->ReleaseTouchCapture(it, it->second, false);

	TouchCapture *capture = it->second;

	if (capture == NULL)
		return -1;

	DragState *first = widget->m_drag_state[0];

	if (first == NULL)
		return -1;
	if (first->m_capture == capture)
		return widget->ClearDragState(first);

	DragState *second = widget->m_drag_state[1];

	if (second != NULL && second->m_capture == capture)
		return widget->ClearDragState(second);
	return -1;
}

int32_t Widget::CapturePadButton(uint32_t port, uint32_t key, bool reset)
{
	if (port >= 5)
		return -1;
	if (m_env == NULL)
		return -1;

	Widget *holder = ScePafWidget_0775F78F(m_env, key);

	if (holder != this) {
		if (holder != NULL && reset)
			holder->ReleasePadCaptureButton(255, true);
		return 0;
	}

	bool added = false;
	Environment *env = m_env;
	Widget *current;

	if (port < 5 && (current = env->m_pad_capture_button[port]) != NULL && current == this) {
		if (reset)
			remove_pad_gesture_mask(port, m_touch_gesture_mask);
	} else {
		if (env->SetPadCaptureHigh(port, this) < 0)
			return -1;
		m_pad_button_port = port;
		added = true;
	}
	apply_pad_gesture_mask(port, m_touch_gesture_mask);
	if (added) {
		OnPadCaptureChanged(true, 0, port, key);

		Event e(0x801, 0x100000, 0x10000, 0, 0, 0, 0, 0);

		DoEvent(537133066, &e);
		e.SetTarget(this);
		e.Term();
		m_env->m_ev_queue.PushBack(e);
	}
	return 0;
}

int32_t Widget::CapturePadDir(uint32_t port, uint32_t key, bool reset)
{
	if (port >= 5)
		return -1;

	Environment *env = m_env;

	if (env == NULL)
		return -1;
	if (env->m_focus_cursor.m_target != this) {
		if (reset)
			env->m_focus_cursor.m_target->ReleasePadCaptureDir(27, true);
		return 0;
	}

	bool added = false;
	Widget *current = env->m_pad_capture_dir[port];

	if (current != NULL && current == this) {
		if (reset)
			remove_pad_gesture_mask(port, m_touch_gesture_mask);
	} else {
		if (env->SetPadCaptureLow(port, this) < 0)
			return -1;
		m_pad_dir_port = port;
		added = true;
	}
	apply_pad_gesture_mask(port, m_touch_gesture_mask);
	if (added) {
		OnPadCaptureChanged(true, 1, port, key);

		Event e(0x801, 0x100000, 0x10000, 0, 0, 0, 0, 0);

		DoEvent(537133066, &e);
		e.SetTarget(this);
		e.Term();
		m_env->m_ev_queue.PushBack(e);
	}
	return 0;
}

/* a direction key that m_keycode does not bind (inline in Sony's CapturePad; name ours) */
static __attribute__((always_inline)) bool IsPadDirKey(Widget *widget, uint32_t key)
{
	if (key == 1 || key == 2 || key == 3 || key == 4 || key == 27) {
		uint32_t mask = KeycodeToMask(key);

		if (!widget->m_bound_keycode || (mask & widget->m_keycode) == 0)
			return true;
	}
	return false;
}

int32_t Widget::CapturePad(uint32_t port, uint32_t key, bool reset, bool reset_touch)
{
	if (m_env == NULL)
		return -1;
	if (reset_touch) {
		ScePafWidget_ABF05727(m_env, 0, true, NULL);
		ScePafWidget_ABF05727(m_env, 1, true, NULL);
	}
	if (IsPadDirKey(this, key)) {
		if (reset && m_pad_dir_port < 0)
			m_env->ReleasePadCaptureHigh(port);
		return CapturePadDir(port, key, reset);
	} else if (!IsPadDirKey(this, key)) {
		if (reset && m_pad_button_port < 0)
			m_env->ReleasePadCaptureLow(port);
		return CapturePadButton(port, key, reset);
	}
	return -1;
}

extern "C" int32_t ScePafWidget_4E47E55A(Widget *widget, uint32_t mode, DragState *drag)
{
	if (widget->m_parent == NULL || widget->m_scene == NULL)
		return -1;
	if (drag == NULL)
		return -1;

	if (mode == 274) {
		Camera *camera = widget->m_scene->m_camera;

		drag->m_origin = drag->m_hit.m_pos;
		drag->m_axis = math::v4(camera->GetWorldMatrix().getImpl() *
					sce::Vectormath::Simd::Aos::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
	} else if (mode == 272) {
		math::v4 offs = widget->GetAlignOffsetToCenter(0);
		math::v4 local = drag->m_hit.LocalPos();
		math::matrix rotation = widget->GetQuat(0, NULL, math::quaternion()).toMatrix(0.0001f);
		math::v4 pos(rotation.getImpl() * (local + offs).getImpl());
		math::matrix origin = widget->m_parent->GetOriginWorldMatrix();

		drag->m_origin = math::v4(origin.getImpl() *
					  sce::Vectormath::Simd::Aos::Vector4(0.0f, 0.0f, pos.extract_z(), 1.0f));
		drag->m_axis = math::v4(origin.getImpl() *
					sce::Vectormath::Simd::Aos::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
	} else if (mode == 273) {
		drag->m_axis = math::v4(0.0f, 0.0f, 1.0f, 0.0f);
		drag->m_origin = math::v4(0.0f, 0.0f, drag->m_hit.m_pos.extract_z(), 1.0f);
	}
	return 0;
}

extern "C" int32_t ScePafWidget_E322D1F6(Widget *widget, uint32_t mode, DragState *drag)
{
	Widget *parent = widget->m_parent;

	if (parent == NULL || widget->m_scene == NULL)
		return -1;
	if (drag == NULL)
		return -1;

	math::v4 origin;

	switch (mode) {
	case 289:
		drag->m_axis = math::v4(parent->GetWorldRot().toMatrix(0.0001f).getImpl() *
					sce::Vectormath::Simd::Aos::Vector4(0.0f, 1.0f, 0.0f, 0.0f));
		break;
	case 290:
		drag->m_axis = math::v4(parent->GetWorldRot().toMatrix(0.0001f).getImpl() *
					sce::Vectormath::Simd::Aos::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
		break;
	case 291:
		drag->m_axis = math::v4(1.0f, 0.0f, 0.0f, 0.0f);
		origin = drag->m_hit.m_pos;
		goto set;
	case 292:
		drag->m_axis = math::v4(0.0f, 1.0f, 0.0f, 0.0f);
		origin = drag->m_hit.m_pos;
		goto set;
	case 293:
		drag->m_axis = math::v4(0.0f, 0.0f, 1.0f, 0.0f);
		origin = drag->m_hit.m_pos;
		goto set;
	default:
		drag->m_axis = math::v4(parent->GetWorldRot().toMatrix(0.0001f).getImpl() *
					sce::Vectormath::Simd::Aos::Vector4(1.0f, 0.0f, 0.0f, 0.0f));
		break;
	}
	origin = math::v4(widget->m_parent->GetOriginWorldMatrix().getImpl() *
			  drag->m_hit.m_local_hit.getImpl());
set:
	drag->m_origin = origin;
	return 0;
}

extern "C" int32_t ScePafWidget_BF92FEF0(Widget *widget, uint32_t mode, DragState *drag,
					 DragState *second)
{
	if (widget->m_parent == NULL || widget->m_scene == NULL)
		return -1;
	if (drag == NULL || second == NULL)
		return -1;

	math::v4 offs = widget->GetAlignOffsetToCenter(0);
	math::v4 local = second->m_hit.LocalPos();
	math::matrix rotation = widget->GetQuat(0, NULL, math::quaternion()).toMatrix(0.0001f);
	math::v4 pos(rotation.getImpl() * (local + offs).getImpl());
	math::matrix origin = widget->m_parent->GetOriginWorldMatrix();

	drag->m_origin = math::v4(origin.getImpl() *
				  sce::Vectormath::Simd::Aos::Vector4(0.0f, 0.0f, pos.extract_z(), 1.0f));
	drag->m_axis = math::v4(origin.getImpl() *
				sce::Vectormath::Simd::Aos::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
	return 0;
}

/* the touch's line through the scene taken again through the camera (inline in Sony's callers;
   name ours) */
static __attribute__((always_inline)) void UpdateTouchLine(TouchInfo *touch, Camera *camera)
{
	const math::matrix &camera_world = camera->GetWorldMatrix();

	ScePafWidget_72F479CA(camera);

	math::matrix world = camera_world;

	touch->m_depth = camera->m_distance;
	touch->m_has_line = true;
	touch->m_camera_matrix = world;
	touch->m_line_start = world.w();
	touch->m_line_end = math::v4(world.getImpl() *
				     sce::Vectormath::Simd::Aos::Vector4(-touch->m_pos.extract_x(),
									 touch->m_pos.extract_y(),
									 touch->m_depth, 1.0f));
}

extern "C" int32_t ScePafWidget_1E0A2904(Widget *widget, DragState *drag)
{
	using sce::Vectormath::Simd::Aos::Vector4;

	if (drag == NULL)
		return -1;

	TouchCapture *capture = drag->m_capture;
	TouchInfo *touch;

	if (capture == NULL || (touch = capture->m_touch) == NULL)
		return -1;

	Scene *scene = widget->m_scene;
	Camera *camera;

	if (scene == NULL || (camera = scene->m_camera) == NULL)
		return -1;
	UpdateTouchLine(touch, camera);

	HitInfo hit;
	math::v4 normal(widget->GetWorldMatrix().getImpl() * drag->m_hit.m_normal.getImpl());
	math::v4 pos(widget->GetWorldMatrix().getImpl() * drag->m_hit.LocalPos().getImpl());
	int32_t result = widget->CalcHitLineToPlane(touch->m_line_start, touch->m_line_end, pos,
						    normal, &hit);

	if (result < 0)
		return result;

	math::v4 offs = widget->GetAlignOffsetToCenter(0);
	math::v4 start_offs = drag->m_hit.m_widget_scale;
	math::v4 grown = drag->m_axis;
	math::v4 now = hit.LocalPos() + offs;

	drag->m_origin = now;

	math::v4 size = drag->m_hit.m_widget_size;
	math::v4 start = drag->m_hit.LocalPos() + start_offs;
	uint8_t quant_x = widget->m_drag_quant_x;
	uint8_t quant_y = widget->m_drag_quant_y;
	uint8_t quant_z = widget->m_drag_quant_z;
	SceFVector3 *limit = widget->m_drag_limit;
	math::v4 factor;

	factor.set_x(widget->m_align_x != 0 ? 1.0f : 2.0f);
	factor.set_y(widget->m_align_y != 0 ? 1.0f : 2.0f);
	factor.set_z(widget->m_align_z != 0 ? 1.0f : 2.0f);
	if (start.extract_x() * now.extract_x() < 0.0f)
		factor.set_x(factor.extract_x() * -1.0f);
	if (start.extract_y() * now.extract_y() < 0.0f)
		factor.set_y(factor.extract_y() * -1.0f);
	if (start.extract_z() * now.extract_z() < 0.0f)
		factor.set_z(factor.extract_z() * -1.0f);
	grown.set_x(grown.extract_x() + factor.extract_x() * __builtin_fabsf(now.extract_x()));
	grown.set_y(grown.extract_y() + factor.extract_y() * __builtin_fabsf(now.extract_y()));
	grown.set_z(grown.extract_z() + factor.extract_z() * __builtin_fabsf(now.extract_z()));

	math::v4 next;

	switch (widget->m_drag_mode) {
	case 513:
		next = grown;
		next.set_y(size.extract_y());
		next.set_z(size.extract_z());
		break;
	case 514:
		next = grown;
		next.set_x(size.extract_x());
		next.set_z(size.extract_z());
		break;
	case 515:
		next = grown;
		next.set_x(size.extract_x());
		next.set_y(size.extract_y());
		break;
	case 516:
		next = grown;
		next.set_z(size.extract_z());
		break;
	case 518: {
		float scale_x = 0.0f;
		float scale_y = 0.0f;
		float scale_z = 0.0f;

		if (size.extract_x() > 0.01f)
			scale_x = grown.extract_x() / size.extract_x();
		if (size.extract_y() > 0.01f)
			scale_y = grown.extract_y() / size.extract_y();
		if (size.extract_z() > 0.01f)
			scale_z = grown.extract_z() / size.extract_z();

		float scale = scale_x > scale_y ? scale_x : scale_y;

		if (scale > scale_z)
			scale_z = scale;
		next = size * scale_z;
		break;
	}
	default:
		next = grown;
		break;
	}
	if (quant_x != 0 || quant_y != 0 || quant_z != 0) {
		Vector4 quant(quant_x != 0 ? (float)quant_x : 1.0f, quant_y != 0 ? (float)quant_y : 1.0f,
			      quant_z != 0 ? (float)quant_z : 1.0f, 1.0f);
		Vector4 steps = divPerElem(next.getImpl() + quant * 0.5f, quant);

		next = math::v4(mulPerElem(Vector4(sce_vectormath_floorf4(steps.get128())), quant));
	}

	Vector4 lower(0.0f);

	if (limit != NULL)
		lower = Vector4(limit[0].x, limit[0].y, limit[0].z, 0.0f);

	Vector4 upper(0.0f);

	if (limit != NULL)
		upper = Vector4(limit[1].x, limit[1].y, limit[1].z, 0.0f);

	math::v4 clamped(maxPerElem(minPerElem(next.getImpl(), upper), lower));
	bool changed = !(next.extract_x() == clamped.extract_x() &&
			 next.extract_y() == clamped.extract_y() &&
			 next.extract_z() == clamped.extract_z());
	bool debug = false;

	if (scene != NULL)
		debug = scene->m_debug_mode == 2;
	widget->SetSize_core(clamped, 1026, debug);
	return changed;
}

/* the touch state of the port and ID, its port and ID set again; NULL outside the ports and IDs
   (inline in Sony's callers; name ours) */
static __attribute__((always_inline)) TouchInfo *GetTouchState(Environment *env, uint32_t port,
							       uint32_t id)
{
	if (id < 8 && port < 2) {
		TouchInfo *touch = &env->m_touch_state[port][id];

		touch->m_port = port;
		touch->m_id = id;
		return touch;
	}
	return NULL;
}

/* the frame with its z axis along the first vector and its x axis across the second, the w of both
   cleared first (0x8115c386; name ours) */
static __attribute__((noinline)) math::matrix MakeAxisMatrix(math::v4 &axis, math::v4 &dir)
{
	axis.set_w(0.0f);
	dir.set_w(0.0f);

	math::v4 z = axis.unit();

	z.set_w(axis.extract_w());

	math::v4 x = CrossKeepW(dir, z);
	math::v4 x_unit = x.unit();

	x_unit.set_w(x.extract_w());

	math::v4 y = CrossKeepW(z, x_unit);

	return math::matrix(x_unit, y, z, math::v4(0.0f, 0.0f, 0.0f, 1.0f));
}

extern "C" DragState *ScePafWidget_9697762E(Widget *widget, TouchCapture *capture,
					    const HitInfo *start)
{
	if (capture == NULL)
		return NULL;

	TouchInfo *touch = capture->m_touch;
	HitInfo hit;

	if (start == NULL) {
		int32_t found = widget->m_env->GetHitStack(&hit, widget);
		Scene *scene = widget->m_scene;
		Camera *camera = NULL;

		if (scene != NULL)
			camera = scene->m_camera;
		if ((found < 0 || hit.m_touch_id != touch->m_id) && camera != NULL) {
			UpdateTouchLine(touch, camera);
			if (widget->CalcHitBoxToLine(widget->GetHitBoxSize(0), touch->m_line_start,
						     touch->m_line_end, &hit) < 0) {
				math::v4 size = widget->GetHitBoxSize(0);
				math::v4 center = widget->GetWorldMatrix().w();

				widget->CalcHitBoxToLine(size, touch->m_line_start, center, &hit);
			}
		}
		if (widget->m_drag_state[0] == NULL && (widget->m_drag_mode & 0x200) != 0) {
			math::v4 normal(widget->GetWorldMatrix().getImpl() * hit.m_normal.getImpl());
			math::v4 pos(widget->GetWorldMatrix().getImpl() * hit.LocalPos().getImpl());

			widget->CalcHitLineToPlane(touch->m_line_start, touch->m_line_end, pos, normal,
						   &hit);

			math::v4 lower = widget->GetHitBoxSize(0) * -0.5f * 0.95f;
			math::v4 upper = widget->GetHitBoxSize(0) * 0.5f * 0.95f;
			math::v4 local(sce::Vectormath::Simd::Aos::maxPerElem(
				sce::Vectormath::Simd::Aos::minPerElem(hit.LocalPos().getImpl(),
								       upper.getImpl()),
				lower.getImpl()));

			local.set_w(1.0f);
			hit.LocalPos() = local;
			hit.m_pos = math::v4(widget->GetWorldMatrix().getImpl() * local.getImpl());
			if ((widget->m_update_flag & 7) != 0)
				widget->UpdateMatrix();
			hit.m_local_hit = math::v4(widget->m_local_matrix.getImpl() * local.getImpl());
		}
		start = &hit;
	}

	DragState *drag = widget->m_drag_state[1];

	if (drag != NULL) {
		if (drag->m_capture == capture) {
			if (start != NULL)
				drag->m_hit = *start;
			return drag;
		}
		drag = widget->m_drag_state[0];
		if (drag->m_capture == capture) {
			if (start != NULL)
				drag->m_hit = *start;
			return drag;
		}
		return NULL;
	}

	DragState *first = widget->m_drag_state[0];

	if (first == NULL && widget->m_drag_mode != 0) {
		first = ScePafWidget_DE6905E2(widget->m_env, capture);
		widget->m_drag_state[0] = first;
		if (first == NULL)
			return NULL;
		first->m_hit = *start;
		widget->KillAnimType(0x10000);
		first = widget->m_drag_state[0];
		if ((widget->m_drag_mode & 0x200) != 0) {
			math::v4 offs = widget->GetAlignOffsetToCenter(0);
			math::v4 from = start->LocalPos() + offs;
			math::v4 extent(sce::Vectormath::Simd::Aos::absPerElem(from.getImpl()));
			math::v4 align = widget->m_align_offs;

			first->m_origin = from;
			if ((widget->m_align_x | widget->m_align_y | widget->m_align_z) == 0 &&
			    align == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
				extent = extent * 2.0f;
			} else {
				if (align.extract_x() == 0.0f)
					extent.set_x(2.0f * extent.extract_x());
				if (align.extract_y() == 0.0f)
					extent.set_y(2.0f * extent.extract_y());
				if (align.extract_z() == 0.0f)
					extent.set_z(2.0f * extent.extract_z());
			}
			first->m_axis = *widget->GetSize(0) - extent;
			first = widget->m_drag_state[0];
		}
		return first;
	}
	if (first != NULL && widget->m_pinch_state == NULL && widget->m_pinch_mode != 0) {
		Environment *env = widget->m_env;

		if (env != NULL) {
			ScePafWidget_0E453608(widget, first, GetTouchState(env, first->m_port, first->m_id));
			env = widget->m_env;
		}

		DragState *second = ScePafWidget_DE6905E2(env, capture);

		widget->m_drag_state[1] = second;
		if (second == NULL)
			return NULL;
		second->m_hit = *start;
		widget->KillAnimType(0x10000);
		widget->KillAnimType(0x10004);

		PinchState *pinch = &widget->m_env->m_pinch_state[0];

		widget->m_pinch_state = pinch;
		pinch->m_state_a = 0;
		pinch->m_state_b = 0;
		pinch->m_state_c = 0;
		pinch->m_dir = widget->m_drag_state[1]->m_hit.m_local_hit -
			       widget->m_drag_state[0]->m_hit.m_local_hit;
		pinch->m_axis_z = math::v4(
			widget->GetQuat(0, NULL, math::quaternion()).toMatrix(0.0001f).getImpl() *
			sce::Vectormath::Simd::Aos::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
		pinch->m_axis_y = math::v4(
			widget->GetQuat(0, NULL, math::quaternion()).toMatrix(0.0001f).getImpl() *
			sce::Vectormath::Simd::Aos::Vector4(0.0f, 1.0f, 0.0f, 0.0f));
		pinch->m_quat = math::v4(sce::Vectormath::Simd::Aos::Vector4(
			widget->GetQuat(0, NULL, math::quaternion()).getImpl()));
		pinch->m_size = *widget->GetSize(0);
		pinch->m_rot_touch = math::v4(
			MakeAxisMatrix(pinch->m_axis_z, pinch->m_dir).extractEulerAnglesYXZ(), 0.0f);
		pinch->m_rot = widget->GetRot(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
		return widget->m_drag_state[1];
	}
	return NULL;
}

/* the rotation of the Euler angles, z, then x, then y (inline in Sony's callers; names ours) */
static __attribute__((always_inline)) math::matrix RotationZ(float angle)
{
	float s = sce_paf_sinf(angle);
	float c = sce_paf_cosf(angle);

	return math::matrix(math::v4(c, s, 0.0f, 0.0f), math::v4(-s, c, 0.0f, 0.0f),
			    math::v4(0.0f, 0.0f, 1.0f, 0.0f), math::v4(0.0f, 0.0f, 0.0f, 1.0f));
}

static __attribute__((always_inline)) math::matrix RotationX(float angle)
{
	float s = sce_paf_sinf(angle);
	float c = sce_paf_cosf(angle);

	return math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f), math::v4(0.0f, c, s, 0.0f),
			    math::v4(0.0f, -s, c, 0.0f), math::v4(0.0f, 0.0f, 0.0f, 1.0f));
}

static __attribute__((always_inline)) math::matrix RotationY(float angle)
{
	float s = sce_paf_sinf(angle);
	float c = sce_paf_cosf(angle);

	return math::matrix(math::v4(c, 0.0f, -s, 0.0f), math::v4(0.0f, 1.0f, 0.0f, 0.0f),
			    math::v4(s, 0.0f, c, 0.0f), math::v4(0.0f, 0.0f, 0.0f, 1.0f));
}

extern "C" int32_t ScePafWidget_8BDEB047(Widget *widget, DragState *first, DragState *second,
					 math::v4 *overflow)
{
	using sce::Vectormath::Simd::Aos::Vector4;

	if (first == NULL || second == NULL)
		return -1;

	PinchState *pinch = widget->m_pinch_state;
	Vector4 one(1.0f);

	if (pinch == NULL)
		return -1;

	Environment *env = widget->m_env;
	TouchInfo *touch_a = GetTouchState(env, first->m_port, first->m_id);
	TouchInfo *touch_b = GetTouchState(env, second->m_port, second->m_id);
	math::v4 normal = second->m_axis;
	math::v4 point = second->m_origin;
	HitInfo hit;

	widget->CalcHitLineToPlane(touch_a->m_line_start, touch_a->m_line_end, point, normal, &hit);

	math::v4 hit_a = hit.m_local_hit;

	widget->CalcHitLineToPlane(touch_a->m_line_start, touch_b->m_line_end, point, normal, &hit);

	math::v4 hit_b = hit.m_local_hit;
	math::v4 dir = hit_b - hit_a;
	math::v4 rot;

	if ((widget->m_pinch_mode & 0x400) != 0 || (widget->m_pinch_mode & 0x800) != 0) {
		math::v4 angles(MakeAxisMatrix(pinch->m_axis_z, dir).extractEulerAnglesYXZ(), 0.0f);

		rot = pinch->m_rot + (angles - pinch->m_rot_touch);
	} else {
		rot = pinch->m_rot;
	}

	SceFVector3 *limit = widget->m_pinch_limit;
	Vector4 scale;

	if ((widget->m_pinch_mode & 0x200) != 0 || (widget->m_pinch_mode & 0x400) != 0 ||
	    (widget->m_pinch_mode & 0x800) != 0) {
		scale = Vector4(dir.len() / pinch->m_dir.len());
	} else {
		Vector4 from = maxPerElem(minPerElem(absPerElem(pinch->m_dir.getImpl()), Vector4(100000.0f)),
					  Vector4(50.0f));
		Vector4 to = maxPerElem(Vector4(50.0f), absPerElem(dir.getImpl()));

		scale = maxPerElem(Vector4(0.1f), absPerElem(divPerElem(to, from)));
	}

	Vector4 size = mulPerElem(pinch->m_size.getImpl(), scale);
	Vector4 lower(0.0f);

	if (limit != NULL)
		lower = Vector4(limit[0].x, limit[0].y, limit[0].z, 0.0f);

	Vector4 upper(0.0f);

	if (limit != NULL)
		upper = Vector4(limit[1].x, limit[1].y, limit[1].z, 0.0f);

	Vector4 over = divPerElem(size, maxPerElem(one, upper));
	float over_x = over.getX();
	float over_y = over.getY();
	float over_z = over.getZ();

	if (over_x > 1.0f || over_y > 1.0f || over_z > 1.0f) {
		if (!(over_x < over_y || over_x < over_z)) {
			size = size / over.getX();
			scale = scale / over.getX();
		} else if (!(over_x > over_y || over_y < over_z)) {
			size = size / over.getY();
			scale = scale / over.getY();
		} else if (over_x <= over_z && over_y <= over_z) {
			size = size / over.getZ();
			scale = scale / over.getZ();
		}
	}

	Vector4 under = divPerElem(maxPerElem(one, size), maxPerElem(one, lower));
	float under_x = under.getX();
	float under_y = under.getY();
	float under_z = under.getZ();

	if (under_x < 1.0f || under_y < 1.0f || under_z < 1.0f) {
		if (!(under_x > under_y || under_x > under_z)) {
			size = size / under.getX();
			scale = scale / under.getX();
		} else if (!(under_x < under_y || under_y > under_z)) {
			size = size / under.getY();
			scale = scale / under.getY();
		} else if (under_x >= under_z && under_y >= under_z) {
			size = size / under.getZ();
			scale = scale / under.getZ();
		}
	}

	math::v4 new_size(size);

	widget->SetSize_core(new_size, 1026, false);

	int32_t result = 0;
	math::matrix rotation = RotationZ(rot.extract_z()) * RotationX(rot.extract_x()) *
				RotationY(rot.extract_y());
	math::v4 turned_b(rotation.getImpl() * second->m_hit.LocalPos().getImpl());
	math::v4 turned_a(rotation.getImpl() * first->m_hit.LocalPos().getImpl());
	SceFVector3 *pos_limit = widget->m_drag_limit;
	Vector4 pos = ((hit_b.getImpl() - mulPerElem(turned_b.getImpl(), scale)) +
		       (hit_a.getImpl() - mulPerElem(turned_a.getImpl(), scale))) * 0.5f;
	Vector4 pos_lower(0.0f);

	if (pos_limit != NULL)
		pos_lower = Vector4(pos_limit[0].x, pos_limit[0].y, pos_limit[0].z, 0.0f);

	Vector4 pos_upper(0.0f);

	if (pos_limit != NULL)
		pos_upper = Vector4(pos_limit[1].x, pos_limit[1].y, pos_limit[1].z, 0.0f);

	math::v4 clamped(maxPerElem(minPerElem(pos, pos_upper), pos_lower));

	if (!(pos.getX() == clamped.extract_x() && pos.getY() == clamped.extract_y() &&
	      pos.getZ() == clamped.extract_z())) {
		result = 1;
		if (overflow != NULL)
			*overflow = math::v4(clamped.getImpl() - pos);
	}
	widget->SetPos_core(clamped, 1025, false);
	if ((widget->m_pinch_mode & 0x400) != 0 || (widget->m_pinch_mode & 0x800) != 0)
		widget->SetRot_core(rot, 0, 1032);
	return result;
}

/* the position the drag's touch moves the widget to, from where it holds the widget, in its
   parent's space (inline in Sony's ScePafWidget_E17C2B53; name ours) */
static __attribute__((always_inline)) math::v4 DragPos(Widget *widget, const math::v4 &at)
{
	math::v4 center = widget->m_scene->GetPosCenter(0, widget->m_parent, at);

	if ((widget->m_update_flag & 7) != 0)
		widget->UpdateMatrix();

	math::v4 offs = widget->GetAlignOffsetToCenter(0);

	return center - math::v4(widget->m_local_matrix.getImpl() * offs.getImpl());
}

extern "C" int32_t ScePafWidget_E17C2B53(Widget *widget, DragState *drag, math::v4 *overflow,
					 float rate)
{
	using sce::Vectormath::Simd::Aos::Vector4;

	if (drag == NULL)
		return -1;

	TouchCapture *capture = drag->m_capture;
	TouchInfo *touch;

	if (capture == NULL || (touch = capture->m_touch) == NULL)
		return -1;

	Scene *scene = widget->m_scene;
	Camera *camera;

	if (scene == NULL || (camera = scene->m_camera) == NULL)
		return -1;
	UpdateTouchLine(touch, camera);

	math::v4 pos(0.0f, 0.0f, 0.0f, 0.0f);

	switch (widget->m_drag_mode) {
	case 272:
	case 273:
	case 274: {
		math::v4 normal = drag->m_axis;
		math::v4 point = drag->m_origin;
		HitInfo hit;
		int32_t found = widget->CalcHitLineToPlane(touch->m_line_start, touch->m_line_end, point,
							   normal, &hit);
		math::v4 held(widget->GetWorldRot().toMatrix(0.0001f).getImpl() *
			      drag->m_hit.LocalPos().getImpl());

		if (found < 0 || widget->m_scene == NULL || widget->m_parent == NULL)
			return -1;
		pos = DragPos(widget, hit.m_pos - held);
		break;
	}
	case 288:
	case 289:
	case 290:
	case 291:
	case 292:
	case 293: {
		math::v4 point = drag->m_origin;
		math::v4 axis = drag->m_axis;
		math::v4 end = touch->m_line_end;
		math::v4 dir = end - touch->m_line_start;
		math::v4 closest;

		widget->CalcDistLineToLine(point, axis, end, dir, &closest, NULL);

		math::v4 held(widget->GetWorldRot().toMatrix(0.0001f).getImpl() *
			      drag->m_hit.LocalPos().getImpl());

		if (widget->m_scene == NULL || widget->m_parent == NULL)
			return -1;
		pos = DragPos(widget, closest - held);
		break;
	}
	default:
		return -1;
	}

	SceFVector3 *limit = widget->m_drag_limit;

	if (widget->m_drag_quant_x != 0 || widget->m_drag_quant_y != 0 ||
	    widget->m_drag_quant_z != 0) {
		Vector4 quant(widget->m_drag_quant_x != 0 ? (float)widget->m_drag_quant_x : 1.0f,
			      widget->m_drag_quant_y != 0 ? (float)widget->m_drag_quant_y : 1.0f,
			      widget->m_drag_quant_z != 0 ? (float)widget->m_drag_quant_z : 1.0f, 1.0f);
		Vector4 steps = divPerElem(pos.getImpl() + quant * 0.5f, quant);

		pos = math::v4(mulPerElem(Vector4(sce_vectormath_floorf4(steps.get128())), quant));
	}

	Vector4 lower(0.0f);

	if (limit != NULL)
		lower = Vector4(limit[0].x, limit[0].y, limit[0].z, 0.0f);

	Vector4 upper(0.0f);

	if (limit != NULL)
		upper = Vector4(limit[1].x, limit[1].y, limit[1].z, 0.0f);

	math::v4 clamped(maxPerElem(minPerElem(pos.getImpl(), upper), lower));
	int32_t result = 0;

	if (!(pos.extract_x() == clamped.extract_x() && pos.extract_y() == clamped.extract_y() &&
	      pos.extract_z() == clamped.extract_z())) {
		result = 1;
		if (overflow != NULL)
			*overflow = clamped - pos;
	}

	bool debug = false;

	if (widget->m_scene != NULL)
		debug = widget->m_scene->m_debug_mode == 2;
	if (rate == 1.0f) {
		widget->SetPos_core(clamped, 1025, debug);
	} else {
		math::v4 now = widget->GetPosCenter(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 next = (clamped - now) * rate + now;

		widget->SetPos_core(next, 1025, debug);
	}
	return result;
}

/* the drag's event (0x80, its detail's bits 8 to 14 the kind: 0x10 a drag, 0x16 a pinch, its data
   what the limits held back) sent to each of the widget's ancestors up to its scene (inline in
   Sony's HandleGestureEvent; name ours) */
static __attribute__((always_inline)) void SendDragEvent(Widget *widget, uint32_t kind,
							 const math::v4 &overflow)
{
	Widget *w = widget->m_parent;
	Event ev(0x80, w, Event::ROUTE_TERM, 0);
	const int32_t *data = reinterpret_cast<const int32_t *>(&overflow);

	ev.m_detail = (ev.m_detail & 0xFFFF80FF) | (kind << 8);
	ev.m_data[0] = data[0];
	ev.m_data[1] = data[1];
	ev.m_data[2] = data[2];
	for (; w != NULL; w = w->m_parent) {
		ev.SetTarget(w);
		ev.Term();
		widget->m_env->m_ev_subrouting.push_back(ev);
		if (w == widget->m_scene)
			break;
	}
}

/* the capture of the touch of the port and ID, NULL where it holds none (inline in Sony's
   HandleGestureEvent; name ours) */
static __attribute__((always_inline)) TouchCapture *FindTouchCapture(Widget *widget, uint32_t key)
{
	map<uint32_t, TouchCapture *>::iterator it = widget->m_touch_captures.find(key);

	if (it != widget->m_touch_captures.end())
		return it->second;
	return NULL;
}

int32_t Widget::HandleGestureEvent(Event *e)
{
	uint32_t detail = e->m_detail;
	int32_t result = 0;
	uint32_t id = e->m_data[0];
	uint32_t type = (detail >> 8) & 0x7F;
	uint32_t id_b = e->m_data[1];
	TouchCapture *capture;

	switch (type) {
	case 17:
		if (m_pinch_state != NULL)
			break;
		if (m_drag_state[0] != NULL)
			ClearDragState(m_drag_state[0]);
		/* fall through */
	case 13:
		capture = FindTouchCapture(this, id | (((detail >> 20) & 0xF) << 8));
		if (m_drag_mode != 0)
			ScePafWidget_9697762E(this, capture, NULL);
		break;
	case 15:
	case 21:
		if (m_drag_state[1] != NULL)
			ClearDragState(m_drag_state[1]);
		if (m_drag_state[0] != NULL)
			ClearDragState(m_drag_state[0]);
		break;
	case 18:
		if (m_pinch_state == NULL && m_drag_state[0] != NULL)
			ClearDragState(m_drag_state[0]);
		break;
	case 14: {
		DragState *drag = m_drag_state[0];
		math::v4 overflow(0.0f, 0.0f, 0.0f, 0.0f);

		if (drag != NULL && m_drag_mode != 0 && id == drag->m_id) {
			uint32_t mode = m_drag_mode;

			if ((mode & 0x100) != 0) {
				if ((mode & 0x110) == 0x110) {
					ScePafWidget_4E47E55A(this, mode, drag);
					drag = m_drag_state[0];
				} else if ((mode & 0x120) == 0x120) {
					ScePafWidget_E322D1F6(this, mode, drag);
					drag = m_drag_state[0];
				}
				result = ScePafWidget_E17C2B53(this, drag, &overflow, 1.0f);
				mode = m_drag_mode;
			} else if ((mode & 0x200) != 0) {
				result = ScePafWidget_1E0A2904(this, drag);
				mode = m_drag_mode;
			}
			if ((mode & 0x1000) != 0 || result == 1)
				SendDragEvent(this, 0x10, overflow);
		}
		break;
	}
	case 19:
	case 23: {
		uint32_t port = ((detail >> 20) & 0xF) << 8;

		if (m_drag_state[1] != NULL)
			ClearDragState(m_drag_state[1]);
		if (m_drag_state[0] != NULL)
			ClearDragState(m_drag_state[0]);
		capture = FindTouchCapture(this, id | port);
		if (m_drag_mode != 0)
			ScePafWidget_9697762E(this, capture, NULL);
		capture = FindTouchCapture(this, id_b | port);
		if (m_pinch_mode != 0)
			ScePafWidget_9697762E(this, capture, NULL);
		break;
	}
	case 20:
		if (m_pinch_state != NULL) {
			math::v4 overflow(0.0f, 0.0f, 0.0f, 0.0f);

			ScePafWidget_BF92FEF0(this, m_pinch_mode, m_drag_state[0], m_drag_state[1]);
			if ((m_pinch_mode & 0x1000) == 0)
				result = ScePafWidget_8BDEB047(this, m_drag_state[0], m_drag_state[1],
							       &overflow);
			if ((m_pinch_mode & 0x1000) != 0 || result == 1)
				SendDragEvent(this, 0x16, overflow);
		}
		break;
	}

	int32_t ret = Handler::HandleGestureEvent(e);

	switch (type) {
	case 7:
		if ((e->m_type & Event::STATE_CONSUME) != 0)
			ScePafWidget_8A132DF7(e, this, false);
		break;
	case 10:
		if ((e->m_type & Event::STATE_CONSUME) != 0)
			ScePafWidget_8A132DF7(e, this, true);
		break;
	case 9:
	case 12:
		ScePafWidget_727885B3(e);
		break;
	}
	return ret;
}

extern "C" int32_t ScePafWidget_DE592034(Widget *widget, uint32_t port, uint32_t id, uint32_t drag_mode)
{
	widget->m_saved_drag_mode = widget->m_drag_mode;
	widget->SetDragMode(drag_mode);
	if (ScePafWidget_D3D0A7C3(widget, port, id, false, true) < 0)
		return -1;

	uint32_t key = id | (port << 8);
	map<uint32_t, TouchCapture *>::iterator it = widget->m_touch_captures.find(key);
	TouchCapture *capture = NULL;

	if (it != widget->m_touch_captures.end())
		capture = it->second;
	if (ScePafWidget_9697762E(widget, capture, NULL) != NULL)
		return 0;
	ScePafWidget_5ABB3393(widget, port, id, false);
	return -1;
}

extern "C" int32_t ScePafWidget_5ABB3393(Widget *widget, uint32_t port, uint32_t id, bool apply)
{
	uint32_t key = id | (port << 8);
	map<uint32_t, TouchCapture *>::iterator it = widget->m_touch_captures.find(key);

	if (it == widget->m_touch_captures.end())
		return -1;
	return widget->ReleaseTouchCapture(it, it->second, apply);
}

extern "C" int32_t ScePafWidget_931AD41E(Widget *widget, bool apply)
{
	if (widget->m_touch_captures.size() == 0)
		return -1;

	map<uint32_t, TouchCapture *>::iterator it = widget->m_touch_captures.begin();

	while (it != widget->m_touch_captures.end()) {
		widget->ReleaseTouchCapture(it, it->second, apply);
		it = widget->m_touch_captures.begin();
	}
	return 0;
}

extern "C" int32_t ScePafWidget_54DFB062(Widget *widget, int32_t port)
{
	int32_t count = 0;

	for (map<uint32_t, TouchCapture *>::iterator it = widget->m_touch_captures.begin();
	     it != widget->m_touch_captures.end(); ++it) {
		if ((it->first & 0xff00) == (uint32_t)(port << 8))
			count++;
	}
	return count;
}

extern "C" int32_t ScePafWidget_00AA6519(Widget *widget)
{
	if (widget->m_focus_root != NULL)
		return 0;

	Environment *env = widget->m_env;

	if (env == NULL)
		return -1;

	Widget *parent_root = NULL;
	Widget *focused = NULL;
	Widget *init = NULL;

	if (widget->m_parent != NULL)
		parent_root = FindFocusRoot(widget->m_parent);

	Widget *above = FindFocusRoot(widget);

	if (above != NULL) {
		focused = (Widget *)above->m_focus_root->m_state;
		if (focused != NULL && focused->m_hold_count <= 0) {
			for (Widget *node = focused; node != widget; node = node->m_parent) {
				if (node == NULL) {
					focused = NULL;
					break;
				}
			}
		} else {
			focused = NULL;
		}
	}
	if (parent_root != NULL)
		init = parent_root->m_focus_root->m_init_focus;

	bool auto_off = env->m_focus_auto_off;

	if (auto_off && focused != env->m_focus_cursor.m_target)
		env->m_focus_auto_off = false;
	if (parent_root != NULL) {
		parent_root->SetFocusRootState(0);
		env = widget->m_env;
	}
	if (focused != NULL && parent_root != NULL) {
		focused->ReleaseFocus();
		env = widget->m_env;
		if (widget != NULL) {
			env->m_focus_candidates.Sort();
			env->m_focus_candidates.Add(widget, -1);
		}
		DeleteFocusRoot(widget->m_focus_root);
		widget->m_focus_root = NewFocusRoot();
		if (init != NULL)
			ScePafWidget_EC6175D6(init, true);
		focused->SetFocus(NULL);
	} else {
		if (widget != NULL) {
			env->m_focus_candidates.Sort();
			env->m_focus_candidates.Add(widget, -1);
		}
		DeleteFocusRoot(widget->m_focus_root);
		widget->m_focus_root = NewFocusRoot();
		if (init != NULL)
			ScePafWidget_EC6175D6(init, true);
		widget->m_env->UpdateFocus(NULL, NULL);
	}
	if (auto_off)
		widget->m_env->m_focus_auto_off = auto_off;
	return 0;
}

extern "C" int32_t ScePafWidget_929C3278(Widget *widget)
{
	FocusRoot *root = widget->m_focus_root;

	if (root == NULL)
		return 0;
	if (widget->m_env == NULL)
		return -1;
	if (root->m_state != 0) {
		((Widget *)root->m_state)->ReleaseFocus();
		root = widget->m_focus_root;
	}
	DeleteFocusRoot(root);
	widget->m_focus_root = NULL;
	widget->SetFocusRootState(0);
	widget->m_env->m_focus_candidates.Remove(widget);
	return 0;
}

extern "C" int32_t ScePafWidget_0253DC9C(Widget *widget, bool on)
{
	Environment *env = widget->m_env;

	if (env == NULL)
		return -1;
	if (widget->m_ordered == on)
		return 0;
	widget->m_ordered = on;
	ScePafWidget_00AA6519(widget);
	env = widget->m_env;
	if (!on)
		return env->m_ordered_widgets.Remove(widget);
	if (widget == NULL)
		return -1;
	env->m_ordered_widgets.Sort();
	return env->m_ordered_widgets.Add(widget, -1) != NULL ? 0 : -1;
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

int32_t Widget::EventExecDispatch(Event *e)
{
	if (e == NULL)
		return -1;

	Widget *group = this;

	if (group != NULL) {
		while (!group->m_pre_dispatch) {
			if (static_cast<Widget *>(m_scene) != group) {
				group = group->m_parent;
				if (group != NULL)
					continue;
			}
			goto routed;
		}
		if (group != this && IsShowHeld(group) == 0) {
			Event copy(*e);

			copy.m_type = (e->m_type & 0x0fffffff) | 0x40000000;
			group->HandlerProc(&copy);
			if ((copy.m_type & Event::STATE_CONSUME) != 0)
				return 0;
		}
	}
routed:
	if (IsShowHeld(this) != 0) {
		inputdevice::Lock(127);
		inputdevice::Unlock(127);
		return 0;
	}

	int32_t result = HandlerProc(e);
	uint32_t type = e->m_type;

	if ((type & Event::STATE_CONSUME) == 0 && (type & Event::ROUTE_TERM) != 0 &&
	    this != static_cast<Widget *>(m_scene)) {
		e->m_type = (type & 0xfff0ffff) | Event::ROUTE_BACK;

		Event back(*e);

		for (Widget *node = m_parent; node != NULL; node = node->m_parent) {
			back.m_type = (back.m_type & 0xff0fffff) | Event::MODE_DIRECT;
			back.m_target = node;
			m_env->m_ev_subrouting.push_back(back);
			if (node == static_cast<Widget *>(m_scene))
				break;
		}
	}
	return result;
}

int32_t Widget::HandleKeycodeEvent(Event *e)
{
	Environment *env = m_env;

	if (env == NULL)
		return -1;

	uint32_t type = e->m_type;

	if ((type & 0x40000000) != 0)
		return Handler::HandleKeycodeEvent(e);
	if ((type & Event::STATE_CONSUME) == 0) {
		uint32_t detail = e->m_detail;

		if ((detail & 0x1000000) != 0 || (uint8_t)detail == 27) {
			if (m_modal_mode != 2 && (this == env->m_cancel_widget || this == env->m_top_modal_widget) &&
			    (uint8_t)detail == 28) {
				Event ev(0x1009, this, 0x40000, 0);

				type = e->m_type;
				if ((uint16_t)type > 0x11) {
					if ((uint16_t)type <= 0x12) {
						ev.m_type = (ev.m_type & 0xffff0000) | 0x100a;
						m_env->m_ev_queue.PushBack(ev);
						type = (e->m_type & 0xf0ffffff) | Event::STATE_CONSUME;
						e->m_type = type;
					}
				} else if ((uint16_t)type > 0x10) {
					ev.m_type = (ev.m_type & 0xffff0000) | 0x1009;
					m_env->m_ev_queue.PushBack(ev);
					type = (e->m_type & 0xf0ffffff) | Event::STATE_CONSUME;
					e->m_type = type;
				}
				if ((type & Event::STATE_CONSUME) != 0)
					return Handler::HandleKeycodeEvent(e);
			}
			if ((uint16_t)type > 0x12) {
				if ((uint16_t)type == 20) {
					uint32_t key = (uint8_t)e->m_detail;

					ReleasePadCaptureDir(key, true);
					ReleasePadCaptureButton(key, true);
					e->m_type = (e->m_type & 0xf0ffffff) | Event::STATE_CONSUME;
				}
			} else if ((uint16_t)type > 0x10) {
				if ((uint16_t)type <= 0x11) {
					uint32_t mask;

					if (ScePafWidget_DB0FFD33(this) ||
					    (m_keycode_capture &&
					     (mask = KeycodeToMask((uint8_t)e->m_detail), m_bound_keycode) &&
					     (mask & m_keycode) != 0)) {
						CapturePad((e->m_detail >> 20) & 0xf, (uint8_t)e->m_detail, true, true);
						e->m_type = (e->m_type & 0xf0ffffff) | Event::STATE_CONSUME;
					}
				} else {
					uint32_t key = (uint8_t)e->m_detail;

					ReleasePadCaptureDir(key, false);
					ReleasePadCaptureButton(key, false);
					e->m_type = (e->m_type & 0xf0ffffff) | Event::STATE_CONSUME;
				}
			}
		}
	}
	return Handler::HandleKeycodeEvent(e);
}

bool Widget::IsAnimRunning(int32_t id)
{
	for (map<int32_t, AnimQueue *>::iterator it = m_anims.begin(); it != m_anims.end(); ++it) {
		if ((it->first & 0xffffff) == id) {
			AnimQueue *queue = it->second;

			for (int32_t i = 0; i < 3; i++) {
				if (queue->m_queue[i] != NULL && !queue->m_queue[i]->IsFinished())
					return true;
			}
		}
	}
	if (m_parent != NULL)
		return m_parent->IsAnimRunning(id);
	return false;
}

void Widget::UpdateAnims()
{
	if (m_hold_count > 0)
		return;
	m_anim_deferred = true;

	map<int32_t, AnimQueue *>::iterator it = m_anims.begin();

	while (it != m_anims.end()) {
		AnimQueue *queue = it->second;

		if (queue != NULL) {
			if ((it->first & 0x10000000) != 0) {
				for (int32_t i = 0; i < 3; i++) {
					if (queue->m_queue[i] != NULL)
						queue->m_queue[i]->SetFinishedLoopBack(i == 2);
				}
			}

			bool running = false;

			for (int32_t i = 0; i < 3; i++) {
				if (queue->m_queue[i] != NULL && !queue->m_queue[i]->IsFinished()) {
					queue->DoTick(m_event_queue, this);
					++it;
					running = true;
					break;
				}
			}
			if (!running) {
				map<int32_t, AnimQueue *>::iterator done = it;

				++it;
				m_anims.erase(done);
				delete queue;
			}
		} else {
			++it;
		}
	}
	m_anim_deferred = false;
	if (ScePafWidget_8A0DA7BF.size() != 0) {
		for (list<AnimPending>::iterator pending = ScePafWidget_8A0DA7BF.begin();
		     pending != ScePafWidget_8A0DA7BF.end(); ++pending)
			pending->m_widget->SetAnimListener(pending->m_id, pending->m_listener, pending->m_slot);
		ScePafWidget_8A0DA7BF.clear();
	}
}

void Widget::OnUpdate()
{
	Update();
	if (m_focus_root != NULL)
		m_focus_root->m_focus_hold = 0;
	if (m_children.size() != 0) {
		list<Widget *>::iterator end = m_children.end();

		for (list<Widget *>::iterator it = m_children.begin(); it != end; ++it) {
			UpdateAdjustChildSize(*it);
			(*it)->OnUpdate();
		}
	}
	if (m_timer_pending)
		ScePafWidget_78C4DBA4(this);
	if (m_show_timer_pending)
		UpdateShowTimer();
	UpdateAfterChildren();

	FocusRoot *own = m_focus_root;

	if (own != NULL) {
		Widget *parent = m_parent;

		if (parent != NULL) {
			Widget *root = FindFocusRoot(parent);

			if (root != NULL) {
				if (m_env->m_focus_cursor.m_target != NULL || own->m_focus_moved <= 30) {
					if (own->m_focus_hold != 0) {
						root->m_focus_root->m_focus_hold = 1;
					} else if ((ScePafWidget_DB0FFD33(this) || IsAnimRunning(65539) ||
						    IsAnimRunning(65538) || m_transition_in || m_transition_out) &&
						   (m_focus_count != 0 || IsTextEventEnabled())) {
						root->m_focus_root->m_focus_hold = 1;

						Widget *above = FindFocusRoot(root);
						Widget *state = above != NULL ?
							(Widget *)above->m_focus_root->m_state : NULL;

						if (state == NULL || state == root) {
							if (ScePafWidget_DB0FFD33(this) && (m_event_enable & 0x400) != 0 &&
							    m_focus_in_group)
								root->SetFocusRootState((uint32_t)this);
						}
					}
				}
			}
		}
	}
	if (!m_transition_in && !m_transition_out) {
		if (m_hold_count <= 0)
			UpdateTimer();
		if (m_resolution_timer != NULL || m_resolution_timer_next != NULL)
			UpdateResolutionTimer();
		if (m_anims.size() != 0)
			UpdateAnims();
	}
	if (m_animator != NULL && m_hold_count <= 0)
		m_animator->OnUpdate();
	UpdateAdjustSize();
	if (m_layouter != NULL) {
		if (m_hold_count > 0)
			return;
		m_layouter->OnUpdate();
	}
	if (m_hold_count <= 0)
		ChangeState(CheckState(), -1);
}

/* the panel's edge correction and the size and limits made of it, taken again each frame
   (names ours): the correction's x and y and w, twice the w's magnitude, and the lowest y a
   size keeps its height at; the correction's x and twice w's magnitude added to a size */
static float s_edge_w;
static float s_edge_x;
static float s_edge_y;
static float s_edge_w2;
static float s_edge_min_y;
static math::v4 s_size_adjust(0.0f, 0.0f, 0.0f, 0.0f);

/* the frame's end: the panel's edge correction read, and the draw objects SwapDrawObj took out
   let go (name ours) */
extern "C" void ScePafWidget_D2E7815D()
{
	float correction[4];

	inputdevice::touchscreen::GetEdgeCorrection(0, correction);
	s_edge_w = correction[3];
	s_edge_x = correction[0];
	s_edge_y = correction[1];
	s_edge_w2 = __builtin_fabsf(correction[3]) * 2.0f;
	s_edge_min_y = correction[1] - s_edge_w2;
	s_size_adjust.set_x(correction[0]);
	s_size_adjust.set_y(s_edge_w2);
	s_swapped_drawobjs.clear();
}

void Widget::UpdateRestruct()
{
	if (s_list_restruct.size() == 0)
		return;

	map<Scene *, int32_t> scenes;

	for (list<RestructInfo>::iterator it = s_list_restruct.begin(); it != s_list_restruct.end(); ++it) {
		Widget *widget = it->m_widget;
		Widget *sibling = it->m_sibling;

		switch (it->m_option) {
		case 1: {
			Widget *parent = widget->m_parent;

			if (parent != NULL) {
				if (sibling != NULL) {
					for (;;) {
						Widget *above = sibling->m_parent;

						if (above == parent)
							break;
						sibling = sibling->m_parent;
						if (above == NULL)
							goto next;
					}
				}
				if (sibling != NULL)
					widget->SetParent_core(widget->m_parent, sibling);
			}
			break;
		}
		case 2:
			widget->SetForward_core(it->m_sibling);
			break;
		case 3:
			widget->SetBehindOneStep_core();
			break;
		case 4:
			widget->SetForwardOneStep_core();
			break;
		case 5:
			if (widget->m_parent != NULL) {
				list<Widget *>::iterator first = widget->m_parent->m_children.begin();

				widget->SetParent_core(widget->m_parent, *first);
			}
			break;
		case 6:
			if (widget->m_parent != NULL)
				widget->SetParent_core(widget->m_parent, NULL);
			break;
		default:
			widget->SetParent_core(it->m_parent, it->m_sibling);
			break;
		}
next:
		scenes[it->m_widget->m_scene] = 1;
	}
	s_list_restruct.clear();
	for (map<Scene *, int32_t>::iterator it = scenes.begin(); it != scenes.end(); ++it) {
		it->first->UpdateLayerWeight();
		if (it->first->m_env != NULL)
			it->first->m_env->m_layer_weight_changed = true;
	}
}

math::v4 ScePafWidget_301AA80F(Widget *widget, const math::v4 &size)
{
	math::v4 adjusted = size + s_size_adjust;

	if (s_edge_min_y > size.extract_y())
		adjusted.set_y(s_edge_y);
	return adjusted;
}


void Widget::detach_environment()
{
	Environment *env = m_env;

	if (env == NULL)
		return;
	if (m_texture_request_count > 0)
		env->RemoveTextureRequests(this);
	if (m_pad_dir_port >= 0)
		ReleasePadCaptureDir(255, true);
	if (m_bound_keycode) {
		if (m_pad_button_port >= 0)
			ReleasePadCaptureButton(255, true);
		if (m_keycode != 0)
			ScePafWidget_164BFFA5(this, m_keycode);
	}
	if (m_modal_mode != 2) {
		m_env->m_modal_widgets.Remove(this);
		m_modal_mode = 2;
	}
	if (m_ordered)
		ScePafWidget_0253DC9C(this, false);
	ScePafWidget_931AD41E(this, false);
	m_env->ClearHitInfo(this);
	if (m_env->m_focus_history_on)
		m_env->m_focus_cursor.RemoveFocusHistory(this);

	Widget *parent = m_parent;

	if (parent != NULL) {
		Widget *root = FindFocusRoot(parent);

		if (root != NULL) {
			Widget *state = NULL;
			Widget *node = root;

			for (;;) {
				FocusRoot *fr = node->m_focus_root;

				if (fr != NULL) {
					state = (Widget *)fr->m_state;
					break;
				}
				if (static_cast<Widget *>(root->m_scene) == node)
					break;
				node = node->m_parent;
				if (node == NULL)
					break;
			}
			if (state == this)
				root->SetFocusRootState(0);
		}
	}
	if (m_focused)
		ReleaseFocus();

	Widget *above = FindFocusRoot(this);

	if (above != NULL && above->m_focus_root->m_init_focus == this)
		ScePafWidget_EC6175D6(this, false);
	if (m_focus_root != NULL)
		ScePafWidget_929C3278(this);
	if (m_touch_region_registered)
		m_env->RemoveTouchDeviceRegion(this);
}

int32_t Widget::ReleaseTouchCapture(map<uint32_t, TouchCapture *>::iterator it, TouchCapture *capture,
				    bool apply)
{
	if (it == m_touch_captures.end())
		return -1;
	if (m_env == NULL || capture == NULL)
		return -1;

	uint32_t port = capture->m_port;
	uint32_t id = capture->m_id;

	if (m_drag_state[0] != NULL) {
		if (m_drag_state[0]->m_capture == capture)
			ClearDragState(m_drag_state[0]);
		else if (m_drag_state[1] != NULL && m_drag_state[1]->m_capture == capture)
			ClearDragState(m_drag_state[1]);
	}

	int32_t result = m_env->ReleaseTouchCapture(capture);

	if (result < 0)
		return result;
	m_touch_captures.erase(it);
	if (apply) {
		inputdevice::touchscreen::SetMask(port, 1 << id);

		inputdevice::InputGestureListener *listener = NULL;

		if (port == 0 || port == 1)
			listener =
				static_cast<inputdevice::InputGestureListener *>(m_env->GetTouchGestureListener(port).get());
		if (listener != NULL)
			ScePafMisc_CB4DB658(listener, id, NULL);
	}
	OnTouchCaptureChanged(port, id, apply);

	Event e(0x802, 0x100000, 0x10000, 0, 0, 0, 0, 0);

	DoEvent(537133067, &e);
	e.SetTarget(this);
	e.Term();
	m_env->m_ev_queue.PushBack(e);
	return result;
}

int32_t Widget::ClearDragState(DragState *state)
{
	Environment *env = m_env;

	if (env == NULL)
		return -1;
	if (state == NULL)
		return -1;

	DragState *first = m_drag_state[0];
	PinchState *pinch = m_pinch_state;

	if (pinch != NULL) {
		DragState *second = m_drag_state[1];

		if (state == first) {
			m_drag_state[0] = second;
			first = second;
			m_drag_state[1] = state;
			second = state;
		} else if (state != second) {
			return -1;
		}
		if (second->m_widget != NULL) {
			second->Reset();
			pinch = m_pinch_state;
			first = m_drag_state[0];
			env = m_env;
		}
		m_drag_state[1] = NULL;
		m_pinch_state = NULL;
		pinch->m_state_a = 0;
		pinch->m_state_b = 0;
		pinch->m_state_c = 0;
		if ((uint8_t)m_layout_notify_mask != 0 && (m_layout_notify_mask & 0x800) != 0) {
			if (m_parent != NULL && m_parent->m_layouter != NULL)
				m_parent->m_layouter->SetDirty(1);
			if (m_layouter != NULL)
				m_layouter->SetDirty(1);
		}

		uint32_t port = first->m_port;
		uint32_t id = first->m_id;

		if (env != NULL) {
			TouchInfo *touch = NULL;

			if (id < 8 && port < 2) {
				touch = &env->m_touch_state[port][id];
				touch->m_port = port;
				touch->m_id = id;
			}
			ScePafWidget_0E453608(this, first, touch);
		}
		return 0;
	}
	if (first != NULL) {
		if (state != first)
			return -1;
		if (first->m_widget != NULL)
			first->Reset();
		m_drag_state[0] = NULL;
		if ((uint8_t)m_layout_notify_mask != 0 && (m_layout_notify_mask & 0x800) != 0) {
			if (m_parent != NULL && m_parent->m_layouter != NULL)
				m_parent->m_layouter->SetDirty(1);
			if (m_layouter != NULL)
				m_layouter->SetDirty(1);
		}

		int32_t mode = (int16_t)m_saved_drag_mode;

		if (mode >= 0) {
			SetDragMode(mode);
			m_saved_drag_mode = -1;
		}
	}
	return 0;
}

void Widget::SetFocusDirection(int32_t dir, IDParam id)
{
	if (m_focuser != NULL) {
		FocusHint *hint = m_focuser->GetHint();

		if (dir != 0) {
			switch (dir) {
			case 1:
				hint->m_direction_id[1] = id;
				break;
			case 2:
				hint->m_direction_id[2] = id;
				break;
			case 3:
				hint->m_direction_id[3] = id;
				break;
			}
		} else {
			hint->m_direction_id[0] = id;
		}
	}
}

void Widget::SetFocusSearchRect(const math::rect &rect)
{
	if (m_focuser != NULL) {
		math::rect copy(rect);

		m_focuser->GetHint()->m_search_rect = copy;
	}
}

void Widget::EnableFocusSearchRect(bool on_off)
{
	if (m_focuser != NULL)
		m_focuser->GetHint()->m_search_rect_enable = on_off;
}

void Widget::SetFocusDirectionWidget(int32_t dir, Widget *widget)
{
	if (m_focuser == NULL)
		return;

	Widget *old = ScePafWidget_71349B04(m_focuser, dir);

	if (old != NULL) {
		FocusReferrers *referrers = ScePafWidget_71349B04(m_focuser, dir)->m_focus_referrers;

		if (referrers != NULL) {
			vector<Widget *>::iterator it = referrers->m_referrers.begin();
			vector<Widget *>::iterator end = referrers->m_referrers.end();

			for (; it != end; ++it) {
				if (*it == this)
					break;
			}
			if (it != end)
				referrers->m_referrers.erase(it);
		}
	}

	FocusDirectionWidget *data = m_focuser->GetDirectionWidget();

	if (dir != 0) {
		switch (dir) {
		case 1:
			data->m_widget[1] = widget;
			break;
		case 2:
			data->m_widget[2] = widget;
			break;
		case 3:
			data->m_widget[3] = widget;
			break;
		}
	} else {
		data->m_widget[0] = widget;
	}

	if (widget != NULL) {
		FocusReferrers *referrers = widget->m_focus_referrers;

		if (referrers == NULL) {
			referrers = new FocusReferrers;
			widget->m_focus_referrers = referrers;
		}

		vector<Widget *>::iterator it = referrers->m_referrers.begin();
		vector<Widget *>::iterator end = referrers->m_referrers.end();

		for (; it != end; ++it) {
			if (*it == this)
				break;
		}
		if (it == end)
			referrers->m_referrers.push_back(this);
	}
}

int32_t Widget::SetFocus(SetFocusInfo *info)
{
	if (m_hold_count > 0)
		return -1;
	if ((m_event_enable & 0x400) == 0)
		return -2;

	Environment *env = m_env;

	if (env == NULL)
		return -3;

	bool keep = false;
	bool notify = true;

	if (info != NULL) {
		notify = info->m_notify;
		keep = info->m_keep;
	}

	Widget *root = FindFocusRoot(this);

	if (root == NULL)
		return -1;

	Widget *target = env->m_focus_cursor.m_target;

	if (target != NULL && notify) {
		Widget *target_root = FindFocusRoot(target);

		if (target_root != NULL) {
			if (!target->m_focus_in_group || !target_root->m_focus_in_group)
				target_root->SetFocusRootState(0);
		}
	}
	root->SetFocusRootFocus(this, notify);

	FocusRoot *focus_root = root->m_focus_root;

	env = m_env;

	bool auto_off = env->m_focus_auto_off;

	if (focus_root != NULL)
		focus_root->m_pending = keep;
	if (auto_off)
		env->m_focus_cursor.SetTarget(this);
	return 0;
}

Widget *Widget::SearchFocusChildren(FocusSearchInfo *info, Widget *skip)
{
	if (info == NULL)
		return NULL;

	Widget *found = NULL;
	int32_t best = 0;

	info->m_distance = 0;
	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
		Widget *child = *it;

		if (child->m_hold_count > 0 || IsShowHeld(child))
			continue;
		if (!child->IsInputTarget() || child->m_meta_a_sum < 0.5f)
			continue;

		bool focusable = false;

		if ((child->m_event_enable & 0x400) != 0) {
			focusable = child->IsTextEventEnabled() != 0;
			child = *it;
		}

		bool target = focusable;

		if (info->m_dir == 255 && !child->m_focus_in_group)
			target = false;
		if (!target) {
			Widget *origin = info->m_origin;
			bool inside = false;

			if (origin != NULL && origin->m_hold_count <= 0) {
				for (Widget *w = origin; w != NULL; w = w->m_parent) {
					if (w == child) {
						inside = true;
						break;
					}
				}
			}
			if (inside)
				continue;
		}
		if (child->GetFocusPriority(info->m_dir) != 0)
			continue;
		child = *it;

		int32_t score = 0;

		if (target && !child->m_not_hit_target && !child->m_fit_to_scene && !child->m_culled &&
		    child->m_hit_detect_mode != 3) {
			Focuser *focuser = child->m_focuser;

			if (focuser != NULL) {
				score = focuser->ScePafWidget_97020684(info);
				child = *it;
			}
		}
		if (best < score) {
			best = score;
			found = child;
		}
		if (child != skip && child->m_focus_count != 0 && !child->m_composite_focus &&
		    child->m_children.size() != 0) {
			Widget *sub = child->SearchFocusChildren(info, NULL);

			if (best < info->m_distance) {
				best = info->m_distance;
				found = sub;
			}
		}
	}
	info->m_distance = best;
	return found;
}

/* the widget a direction's hint names, where it may take the focus */
static inline __attribute__((always_inline)) Widget *HintFocusTarget(Widget *widget, int32_t dir)
{
	Focuser *focuser = widget->m_focuser;

	if (focuser == NULL)
		return NULL;

	Widget *target = focuser->GetHintWidget(dir);

	if (target != NULL && target->m_hold_count <= 0 && (target->m_event_enable & 0x400) != 0 &&
	    target->IsTextEventEnabled() && ScePafWidget_DB0FFD33(target))
		return target;
	return NULL;
}

Widget *Widget::SearchFocus(FocusSearchInfo *info, Widget *from, uint32_t mode)
{
	if (info == NULL)
		return NULL;

	Widget *current = m_env->m_focus_cursor.m_target;

	if (current != NULL && current->GetFocusPriority(info->m_dir) != 0)
		return NULL;

	Widget *found = NULL;
	int32_t best = 0;

	info->m_distance = 0;

	Widget *root = FindFocusRoot(this);
	Widget *hinted = NULL;

	if (info->m_dir == 1 && (hinted = HintFocusTarget(this, 0)) != NULL) {
	} else if (info->m_dir == 2 && (hinted = HintFocusTarget(this, 1)) != NULL) {
	} else if (info->m_dir == 3 && (hinted = HintFocusTarget(this, 2)) != NULL) {
	} else if (info->m_dir == 4) {
		hinted = HintFocusTarget(this, 3);
	}
	if (hinted != NULL) {
		info->m_distance = 0x7FFFFFFF;
		return hinted;
	}

	if (mode <= 1) {
		if (!m_composite_focus && m_children.size() != 0) {
			Widget *sub = SearchFocusChildren(info, from);

			if (info->m_distance > 0) {
				found = sub;
				best = info->m_distance;
			}
		}
		if (root == this) {
			FocusRoot *focus_root = m_focus_root;
			Widget **array = (Widget **)focus_root->m_array;

			if (focus_root->m_array_count != 0) {
				for (Widget **it = array;
				     it != (Widget **)m_focus_root->m_array + m_focus_root->m_array_count;
				     ++it) {
					if (*it != NULL) {
						Widget *sub = (*it)->SearchFocusChildren(info, from);

						if (best < info->m_distance) {
							found = sub;
							best = info->m_distance;
						}
					}
				}
			}
		}
		if (mode == 1) {
			info->m_distance = best;
			return found;
		}
	}

	if (m_parent != NULL) {
		if (!m_focus_searching) {
			Widget *sub = m_parent->SearchFocus(info, this, 0);

			if (best < info->m_distance) {
				found = sub;
				best = info->m_distance;
			}
		}
	} else if (!m_focus_searching) {
		list<Widget *>::iterator scene = m_env->m_scenes_onupdate.begin();
		int32_t entry = -1;
		Widget *first = m_children.size() == 0 ? NULL : *m_children.begin();

		if (first != NULL && first->m_scene != NULL)
			entry = first->m_scene->m_entry;
		for (; scene != m_env->m_scenes_onupdate.end(); ++scene) {
			Widget *widget = *scene;
			bool under = false;

			if (widget != NULL && widget->m_hold_count <= 0) {
				for (Widget *w = widget; w != NULL; w = w->m_parent) {
					if (w == this) {
						under = true;
						break;
					}
				}
			}
			if (under)
				continue;
			if (widget->m_scene->m_entry == entry) {
				Widget *sub = widget->SearchFocus(info, this, 1);

				if (best < info->m_distance) {
					found = sub;
					best = info->m_distance;
				}
			}
		}
	}
	info->m_distance = best;
	return found;
}

Widget *Widget::SetFocusOption(const void *param, int32_t option)
{
	FocusSearchInfo *info = (FocusSearchInfo *)param;

	if (info == NULL) {
		FocusSearchInfo search(255);

		ScePafWidget_EA117CF8(&search, this);
		return SearchFocus(&search, NULL, option);
	}

	int32_t dir = info->m_dir;
	Widget *root = FindFocusRoot(this);
	Widget *init = root != NULL ? root->m_focus_root->m_init_focus : NULL;

	if (dir == 255) {
		Widget *state_root = FindFocusRoot(this);

		if (state_root == NULL || state_root->m_focus_root->m_state == 0) {
			if (init != NULL && init->m_hold_count <= 0 && (init->m_event_enable & 0x400) != 0 &&
			    init->IsTextEventEnabled()) {
				info->m_result = init;
				info->m_distance = 0x7FFFFFFF;
				return init;
			}
		}
	}
	if (info->m_origin == NULL) {
		if (info->m_dir == 255)
			ScePafWidget_EA117CF8(info, this);
		else {
			Widget *focus_root = FindFocusRoot(this);

			ScePafWidget_EA117CF8(info, focus_root != NULL ? (Widget *)focus_root->m_focus_root->m_state : NULL);
		}
	}
	return SearchFocus(info, NULL, option);
}

/* the keys of the keycode mask are captured in the environment's groups, each bit in its own
   (Sony has the bits tested in this order and each group's call unrolled) */
#define RELEASE_KEY_GROUP(bit, group) \
	if ((mask & (bit)) != 0) { \
		result = ScePafWidget_D131E927(widget->m_env, (group), widget); \
		if (result < 0) \
			return result; \
	}

#define CAPTURE_KEY_GROUP(bit, group) \
	if ((keycode & (bit)) != 0) { \
		result = ScePafWidget_66960CBF(widget->m_env, (group), widget); \
		if (result < 0) \
			return result; \
	}

extern "C" int32_t ScePafWidget_164BFFA5(Widget *widget, uint32_t mask)
{
	if (widget->m_env == NULL)
		return -1;
	if (!widget->m_bound_keycode)
		return -1;
	if (widget->m_pad_button_port >= 0)
		widget->ReleasePadCaptureButton(255, true);

	int32_t result = 0;

	RELEASE_KEY_GROUP(0x1, 1);
	RELEASE_KEY_GROUP(0x2, 2);
	RELEASE_KEY_GROUP(0x4, 3);
	RELEASE_KEY_GROUP(0x8, 4);
	RELEASE_KEY_GROUP(0x10, 27);
	RELEASE_KEY_GROUP(0x20, 28);
	RELEASE_KEY_GROUP(0x40, 29);
	RELEASE_KEY_GROUP(0x80, 30);
	RELEASE_KEY_GROUP(0x1000, 39);
	RELEASE_KEY_GROUP(0x400, 31);
	RELEASE_KEY_GROUP(0x800, 32);
	RELEASE_KEY_GROUP(0x10000, 33);
	RELEASE_KEY_GROUP(0x20000, 34);
	RELEASE_KEY_GROUP(0x40000, 35);
	RELEASE_KEY_GROUP(0x80000, 36);
	RELEASE_KEY_GROUP(0x200, 37);
	if ((mask & 0x100) == 0 || (result = ScePafWidget_D131E927(widget->m_env, 38, widget)) >= 0) {
		if (result == 0) {
			if (widget->m_keycode == mask) {
				widget->m_keycode = 0;
				widget->m_bound_keycode = false;
			} else {
				widget->m_keycode = widget->m_keycode & ~mask;
			}
		}
	}
	return result;
}

extern "C" int32_t ScePafWidget_21125870(Widget *widget, uint32_t keycode)
{
	if (widget->m_env == NULL)
		return -1;
	if (widget->m_bound_keycode) {
		if (widget->m_keycode == keycode)
			return 0;
		if (widget->m_keycode != 0)
			ScePafWidget_164BFFA5(widget, widget->m_keycode);
	}

	int32_t result = 0;

	CAPTURE_KEY_GROUP(0x1, 1);
	CAPTURE_KEY_GROUP(0x2, 2);
	CAPTURE_KEY_GROUP(0x4, 3);
	CAPTURE_KEY_GROUP(0x8, 4);
	CAPTURE_KEY_GROUP(0x10, 27);
	CAPTURE_KEY_GROUP(0x20, 28);
	CAPTURE_KEY_GROUP(0x40, 29);
	CAPTURE_KEY_GROUP(0x80, 30);
	CAPTURE_KEY_GROUP(0x1000, 39);
	CAPTURE_KEY_GROUP(0x400, 31);
	CAPTURE_KEY_GROUP(0x800, 32);
	CAPTURE_KEY_GROUP(0x10000, 33);
	CAPTURE_KEY_GROUP(0x20000, 34);
	CAPTURE_KEY_GROUP(0x40000, 35);
	CAPTURE_KEY_GROUP(0x80000, 36);
	CAPTURE_KEY_GROUP(0x200, 37);
	if ((keycode & 0x100) == 0 || (result = ScePafWidget_66960CBF(widget->m_env, 38, widget)) >= 0) {
		if (result == 0) {
			widget->m_bound_keycode = true;
			widget->m_keycode = keycode;
		}
	}
	return result;
}

#undef RELEASE_KEY_GROUP
#undef CAPTURE_KEY_GROUP

extern "C" int32_t ScePafWidget_42C5A4D4(Widget *widget, int32_t mode, int32_t option)
{
	Environment *env = widget->m_env;

	if (env == NULL)
		return -1;

	uint32_t previous = widget->m_modal_mode;

	widget->m_modal_option = option;
	if (mode == widget->m_modal_mode)
		return 0;
	if (mode == 2) {
		if (previous == 0)
			ScePafWidget_929C3278(widget);
		ScePafWidget_164BFFA5(widget, 0x20);
		widget->m_event_enable &= ~0x10;
		widget->m_modal_mode = mode;
		return widget->m_env->m_modal_widgets.Remove(widget);
	}
	if (previous != 2) {
		widget->m_modal_mode = 2;
		env->m_modal_widgets.Remove(widget);
		env = widget->m_env;
	}

	uint32_t register_mode;

	if (mode == 0) {
		register_mode = 0;
	} else if (mode == 1) {
		register_mode = 1;
	} else {
		return -1;
	}
	widget->m_modal_mode = mode;

	int32_t result = env->RegisterModalWidget(widget, register_mode);

	widget->m_event_enable |= 0x10;
	ScePafWidget_21125870(widget, 0x20);
	if ((result | mode) == 0)
		ScePafWidget_00AA6519(widget);
	return result;
}

void Widget::SetInputTarget(bool on)
{
	if (!on && m_input_target)
		ScePafWidget_931AD41E(this, true);
	m_input_target = on;
	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
		(*it)->SetInputTarget(on);
}

extern "C" int32_t ScePafWidget_FF5BC957(Widget *widget, Timer *t, int32_t opt, float delay,
					 AnimCB cb_fin)
{
	widget->SetInputTarget(true);
	if (t == NULL && delay > 0.0f)
		t = new Timer(0.0f);
	if (t == NULL || t->m_state == Timer::State_Disabled) {
		widget->KillAnimType(Widget::ANIM_SHOW_ALPHA);
		ShowAlphaCore(widget, 1.0f);
		if (cb_fin != NULL) {
			AnimListener listener(NULL, widget, NULL, NULL, NULL);

			cb_fin(NULL, &listener, NULL, NULL, NULL);
		}
	} else {
		widget->SetShowAlpha(1.0f, t, Widget::ANIM_SHOW_ALPHA, opt, delay, cb_fin);
	}
	return 0;
}

extern "C" int32_t ScePafWidget_4588699E(Widget *widget, Timer *t, int32_t opt, float delay,
					 AnimCB cb_fin)
{
	widget->SetInputTarget(false);
	if (t == NULL && delay > 0.0f)
		t = new Timer(0.0f);
	if (t == NULL || t->m_state == Timer::State_Disabled) {
		widget->KillAnimType(Widget::ANIM_SHOW_ALPHA);
		ShowAlphaCore(widget, 0.0f);
		if (cb_fin != NULL) {
			AnimListener listener(NULL, widget, NULL, NULL, NULL);

			cb_fin(NULL, &listener, NULL, NULL, NULL);
		}
	} else {
		widget->SetShowAlpha(0.0f, t, Widget::ANIM_SHOW_ALPHA, opt, delay, cb_fin);
	}
	return 0;
}

/*
 * The widget's focus cursor takes shape: each value the shape still has at its
 * default (or at the entry of the widget's resolution) takes that entry, and a
 * value set otherwise is written back into the entry; then the widget's cursor
 * is made a copy of the shape.
 */
extern "C" void ScePafWidget_13B246E0(Widget *widget, FocusShape *shape)
{
	ResolutionData *entry;

	if (shape->m_offset_array.ptr != NULL && shape->m_offset_array.count != 0) {
		entry = widget->get_resolution_data(shape->m_offset_array.ptr,
						    shape->m_offset_array.count,
						    widget->m_resolution_id);
		if (entry != NULL) {
			math::v3 value(entry->m_value[0], entry->m_value[1], entry->m_value[2]);

			if (shape->m_offset == value || shape->m_offset == math::v3::_000()) {
				if (!(value == math::v3::_000()))
					shape->m_offset = value;
			} else {
				entry->m_value[0] = shape->m_offset.extract_x();
				entry->m_value[1] = shape->m_offset.extract_y();
				entry->m_value[2] = shape->m_offset.extract_z();
			}
		}
	}
	if (shape->m_size_array.ptr != NULL && shape->m_size_array.count != 0) {
		entry = widget->get_resolution_data(shape->m_size_array.ptr,
						    shape->m_size_array.count,
						    widget->m_resolution_id);
		if (entry != NULL) {
			math::v2 value(entry->m_value[0], entry->m_value[1]);

			if (shape->m_size == value || shape->m_size == math::v2(0.0f, 0.0f)) {
				if (!(value == math::v2(0.0f, 0.0f)))
					shape->m_size = value;
			} else {
				entry->m_value[2] = 0.0f;
				entry->m_value[0] = shape->m_size.extract_x();
				entry->m_value[1] = shape->m_size.extract_y();
			}
		}
	}
	if (shape->m_rot_array.ptr != NULL && shape->m_rot_array.count != 0) {
		entry = widget->get_resolution_data(shape->m_rot_array.ptr,
						    shape->m_rot_array.count,
						    widget->m_resolution_id);
		if (entry != NULL) {
			math::v3 value(entry->m_value[0], entry->m_value[1], entry->m_value[2]);

			if (shape->m_rot == value || shape->m_rot == math::v3::_000()) {
				if (!(value == math::v3::_000()))
					shape->m_rot = value;
			} else {
				entry->m_value[0] = shape->m_rot.extract_x();
				entry->m_value[1] = shape->m_rot.extract_y();
				entry->m_value[2] = shape->m_rot.extract_z();
			}
		}
	}

	FocusShape *focus = widget->m_focus_shape;

	if (focus == NULL) {
		focus = new FocusShape;
		widget->m_focus_shape = focus;
	}
	vector<ResolutionData>(shape->m_offset_array).swap(focus->m_offset_array);
	vector<ResolutionData>(shape->m_size_array).swap(focus->m_size_array);
	vector<ResolutionData>(shape->m_rot_array).swap(focus->m_rot_array);
	focus->m_shape = shape->m_shape;
	focus->m_offset = shape->m_offset;
	focus->m_size = shape->m_size;
	focus->m_rot = shape->m_rot;
	focus->m_texture = shape->m_texture;
	focus->m_use_parts = shape->m_use_parts;
	focus->m_changed = shape->m_changed;
}

extern "C" float ScePafWidget_44B22ACA(Widget *widget, common::transition::Type type, float delay,
				       HandlerCB cb, void *data)
{
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
	if (widget->m_transition_complete) {
		delay = 0.0f;
	} else {
		widget->m_transition = type;
		widget->m_transition_complete = true;
		if (type == 255)
			delay = widget->DoTransition_core(delay, false);
		else
			delay = ScePafCommon_6CF3A3C8(delay, widget, type, false, false);
	}
	if (cb != NULL)
		widget->SetTimerListener(0x100FF01, delay, new EventCBListener(cb, data));
	return delay;
}

extern "C" float ScePafWidget_F1176ACC(Widget *widget, common::transition::Type type, float delay,
				       HandlerCB cb, void *data)
{
	widget->m_transition = type;
	widget->m_transition_complete = false;
	if (type == 255)
		delay = widget->DoTransitionReverse_core(delay, false);
	else
		delay = ScePafCommon_B48C9BDC(delay, widget, type, false, false);
	if (cb != NULL)
		widget->SetTimerListener(0x100FF01, delay, new EventCBListener(cb, data));
	ScePafWidget_4588699E(widget, NULL, 2, delay, NULL);
	return delay;
}

int32_t Widget::HandleFocusSwitchEvent(Event *e)
{
	Environment *env = m_env;

	if (env == NULL)
		return -1;

	uint32_t type = (uint16_t)e->m_type;

	if (type != 17 && type != 19)
		return -1;

	uint32_t detail = e->m_detail;
	uint32_t dir = (uint8_t)detail;
	int32_t listener_type;

	if (dir > 2) {
		if (dir <= 3)
			listener_type = 262151;
		else if (dir <= 4)
			listener_type = 262152;
		else
			return -1;
	} else if (dir == 0) {
		return -1;
	} else if (dir <= 1) {
		listener_type = 262149;
	} else {
		listener_type = 262150;
	}

	if (type == 17) {
		uint32_t port = (detail >> 20) & 0xF;

		if (port < 5) {
			Widget *holder = env->m_pad_capture_dir[port];

			if (holder != NULL) {
				holder->ReleasePadCaptureDir(27, true);
				env = m_env;
				detail = e->m_detail;
			}
		}
		env->ReleasePadCaptureHigh((detail >> 20) & 0xF);
		ScePafWidget_ABF05727(m_env, 0, true, 0);
		ScePafWidget_ABF05727(m_env, 1, true, 0);
		env = m_env;
	}

	Widget *cursor_target = env->m_focus_cursor.m_target;

	if (m_map_e_listener.find(listener_type) == m_map_e_listener.end()) {
		if (!env->m_focus_auto_off) {
			Widget *hint = NULL;

			if (dir == 1 && (hint = HintFocusTarget(this, 0)) != NULL) {
			} else if (dir == 2 && (hint = HintFocusTarget(this, 1)) != NULL) {
			} else if (dir == 3 && (hint = HintFocusTarget(this, 2)) != NULL) {
			} else if (dir == 4) {
				hint = HintFocusTarget(this, 3);
			}
			if (hint == NULL) {
				env = m_env;
				if (env != NULL && env->m_focus_history_on) {
					Widget *history = env->m_focus_cursor.FindFocusHistory(cursor_target, dir);

					if (history != NULL && history->m_hold_count <= 0 &&
					    (history->m_event_enable & 0x400) != 0 &&
					    history->IsTextEventEnabled() && ScePafWidget_DB0FFD33(history)) {
						SetFocusInfo set;

						set.m_mode = 0;
						set.m_notify = true;
						set.m_direction = math::v2(0.0f, 0.0f);
						set.m_keep = false;
						history->SetFocus(&set);
						e->m_type = (e->m_type & 0xF0FFFFFF) | Event::STATE_CONSUME;
						return 0;
					}
				}
			}

			FocusSearchInfo search(dir);
			Widget *found;

			ScePafWidget_EA117CF8(&search, cursor_target);
			if (m_focus_target) {
				found = SearchFocus(&search, this, 0);
				if (found == NULL) {
					search.m_search_outside = false;
					found = SearchFocus(&search, this, 0);
				}
			} else {
				int32_t search_dir = search.m_dir;

				found = NULL;
				if (search_dir == 1 && (found = HintFocusTarget(this, 0)) != NULL) {
				} else if (search_dir == 2 && (found = HintFocusTarget(this, 1)) != NULL) {
				} else if (search_dir == 3 && (found = HintFocusTarget(this, 2)) != NULL) {
				} else if (search_dir == 4) {
					found = HintFocusTarget(this, 3);
				}
			}
			if (found != NULL) {
				math::v2 direction(0.0f, 0.0f);
				SetFocusInfo set;

				if (dir > 2) {
					if (dir <= 3)
						direction = math::v2(-1.0f, 0.0f);
					else if (dir <= 4)
						direction = math::v2(1.0f, 0.0f);
				} else if (dir != 0) {
					if (dir <= 1)
						direction = math::v2(0.0f, 1.0f);
					else
						direction = math::v2(0.0f, -1.0f);
				}
				set.m_mode = 0;
				set.m_notify = true;
				set.m_direction = direction;
				set.m_keep = false;
				found->SetFocus(&set);
				if (found != this) {
					env = m_env;
					if (env->m_focus_history_on)
						env->m_focus_cursor.AddFocusHistory(cursor_target, found, dir);
				}
				e->m_type = (e->m_type & 0xF0FFFFFF) | Event::STATE_CONSUME;
			}
		}
	} else {
		Handler::HandleFocusSwitchEvent(e);
		env = m_env;

		Widget *target = env->m_focus_cursor.m_target;

		if (target != NULL && env->m_focus_history_on)
			env->m_focus_cursor.AddFocusHistory(cursor_target, target, dir);
	}
	return 0;
}

bool Widget::IsFocusFree()
{
	Environment *env = m_env;

	if (env == NULL)
		return false;
	if (ScePafWidget_DB0FFD33(env->m_focus_cursor.m_target))
		return false;

	Widget *root = FindFocusRoot(this);

	if (ScePafWidget_DB0FFD33(root != NULL ? (Widget *)root->m_focus_root->m_state : NULL))
		return false;
	if (m_focus_root == NULL || m_focus_root->m_pending == 0)
		return true;
	return false;
}

void Widget::UpdateAfterChildren()
{
	FocusRoot *focus_root = m_focus_root;

	if (focus_root == NULL)
		return;
	if (focus_root->m_focus_hold != 0)
		return;

	Environment *env = m_env;

	if (env == NULL || !ScePafWidget_A6B659BB(env, 0) || m_env->m_focus_auto_off ||
	    !ScePafWidget_DB0FFD33(this) || !m_focus_target ||
	    ((m_event_enable & 0x400) == 0 && m_focus_count == 0))
		return;
	if (!m_env->IsInFrontOfModal(this) || m_hold_count > 0 || !m_focus_in_group)
		return;

	if (!IsFocusFree()) {
		m_focus_root->m_focus_moved = 0;
		return;
	}
	if (m_focus_root->m_focus_moved % 5 == 0) {
		FocusSearchInfo info(255);

		ScePafWidget_EA117CF8(&info, this);

		Widget *found = SetFocusOption(&info, 1);

		if (found != NULL) {
			bool move = true;

			if (FindFocusRoot(found) != this) {
				Widget *root = FindFocusRoot(found);

				move = false;
				if (root != NULL && root->m_hold_count <= 0) {
					Widget *w = root;

					while (w != this) {
						w = w->m_parent;
						if (w == NULL)
							break;
					}
					move = w == this;
				}
			}
			if (move) {
				SetFocusInfo set;

				set.m_mode = 0;
				set.m_notify = false;
				set.m_direction = math::v2(0.0f, 0.0f);
				set.m_keep = false;
				found->SetFocus(&set);
				m_focus_root->m_focus_moved = 0;
			}
		}
	}
	focus_root = m_focus_root;
	focus_root->m_focus_moved++;
	if (focus_root->m_focus_moved >= 60)
		focus_root->m_focus_moved = 0;
}

extern "C" int32_t ScePafWidget_11D537AF(Widget *widget, uint32_t id, Timer *t, bool children)
{
	if (widget->m_resolution_id_next == id)
		return 0;
	if (t == NULL)
		return widget->ChangeResolution_core(id, children, true);
	delete widget->m_resolution_timer_next;
	widget->m_resolution_timer_next = t;
	widget->m_resolution_id_next = id;
	if (children) {
		for (list<Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); ++it)
			ScePafWidget_11D537AF(*it, id, new Timer(*t), children);
	}
	return 0;
}

extern "C" int32_t ScePafWidget_38CDFD1D(Widget *widget, Event *e)
{
	uint32_t enabled = widget->m_event_enable;

	if (widget->m_debug_events) {
		uint32_t mode = widget->m_scene->m_debug_mode;

		if (mode == 2 || mode == 3)
			enabled = (enabled & 0xFFFF0000) | 0xFFF0;
	}

	uint32_t kind = (uint16_t)(enabled & e->m_type);

	if (kind != 32 && kind != 64)
		return -1;

	uint32_t port = (e->m_detail >> 20) & 0xF;
	uint32_t id = (uint8_t)e->m_detail;

	switch ((uint16_t)e->m_type) {
	case 0x21:
	case 0x41:
		goto press;
	case 0x22:
	case 0x42:
		goto release;
	case 0x25:
	case 0x44:
		if (widget->m_hit_check != 1) {
press:
			if (widget->m_hit_check != 0 &&
			    ScePafWidget_D3D0A7C3(widget, port, id, true, true) == 0)
				e->m_type = (e->m_type & 0xF0FFFFFF) | Event::STATE_CONSUME;
		}
		break;
	case 0x26:
	case 0x45:
		if (widget->m_hit_check == 1 || widget->m_hit_check == 3)
			break;
release:
		if (widget->m_hit_check != 0) {
capture:
			if (ScePafWidget_5ABB3393(widget, port, id, false) == 0)
				e->m_type = (e->m_type & 0xF0FFFFFF) | Event::STATE_CONSUME;
		}
		break;
	case 0x28:
	case 0x47:
		goto capture;
	default:
		break;
	}
	return 0;
}

void Widget::ReleaseShowHolds(bool recursive)
{
	if (m_touch_captures.size() != 0)
		ScePafWidget_931AD41E(this, true);
	if (m_pad_dir_port >= 0)
		ReleasePadCaptureDir(255, true);
	if (m_pad_button_port >= 0)
		ReleasePadCaptureButton(255, true);
	if (recursive) {
		list<Widget *>::iterator it = m_children.begin();

		while (it != m_children.end()) {
			(*it)->ReleaseShowHolds(true);
			++it;
		}
	}
}

void Widget::SetCursorOn(bool on, Widget *from, Widget *to)
{
	if (m_cursor_on == on)
		return;
	m_cursor_on = on;
	if (on) {
		Event e(0x401, this, 0x40000, 0);

		e.m_data[0] = (int32_t)from;
		e.m_data[1] = (int32_t)to;
		m_env->m_ev_queue.PushBack(e);
		DoEvent(537133057, &e);
	} else {
		Event e(0x402, this, 0x40000, 0);

		e.m_data[0] = (int32_t)from;
		e.m_data[1] = (int32_t)to;
		m_env->m_ev_queue.PushBack(e);
		DoEvent(537133058, &e);
		if (m_pad_dir_port >= 0)
			ReleasePadCaptureDir(27, true);
	}
}

void Widget::NotifyCursorGroup(bool on, Widget *from, Widget *to)
{
	if (!m_pre_dispatch)
		return;
	if (on) {
		Event e(0x403, this, 0x40000, 0);

		e.m_data[0] = (int32_t)from;
		e.m_data[1] = (int32_t)to;
		m_env->m_ev_queue.PushBack(e);
		DoEvent(537133059, &e);
	} else {
		Event e(0x404, this, 0x40000, 0);

		e.m_data[0] = (int32_t)from;
		e.m_data[1] = (int32_t)to;
		m_env->m_ev_queue.PushBack(e);
		DoEvent(537133060, &e);
	}
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

/* the vector's lanes weighted by the matrix's columns, as Sony's CalcBound has it inline (name
   ours) */
static inline float32x4_t MulMatrixVec(const math::matrix &m, float32x4_t v)
{
	return (m.getImpl() * sce::Vectormath::Simd::Aos::Vector4(v)).get128();
}

math::v4 Widget::CalcBound(const math::v4 &size, const math::quaternion &quat)
{
	math::matrix m = NormalizeQuat(quat).toMatrix(0.0001f);
	float32x4_t extent = size.getImpl().get128();
	float32x4_t corner_a = vmulq_f32(math::v4(-1.0f, 1.0f, 1.0f, 0.0f).getImpl().get128(), extent);
	float32x4_t corner_b = vmulq_f32(math::v4(1.0f, 1.0f, 1.0f, 0.0f).getImpl().get128(), extent);
	float32x4_t bound_a = vabsq_f32(MulMatrixVec(m, corner_a));
	float32x4_t bound_b = vabsq_f32(MulMatrixVec(m, corner_b));
	float32x4_t bound;

	if (size.extract_z() > 1.0f) {
		float32x4_t corner_c = vmulq_f32(math::v4(1.0f, -1.0f, 1.0f, 0.0f).getImpl().get128(),
						 extent);
		float32x4_t corner_d = vmulq_f32(math::v4(-1.0f, -1.0f, 1.0f, 0.0f).getImpl().get128(),
						 extent);

		bound = vmaxq_f32(vmaxq_f32(vmaxq_f32(vabsq_f32(MulMatrixVec(m, corner_d)),
						      vabsq_f32(MulMatrixVec(m, corner_c))),
					    bound_b),
				  bound_a);
	} else {
		bound = vmaxq_f32(bound_b, bound_a);
	}
	return math::v4(sce::Vectormath::Simd::Aos::Vector4(bound));
}

math::v4 Widget::GetSizeBound(int32_t opt, Widget *ref)
{
	math::v4 size = *GetSize(opt);
	math::quaternion quat = GetQuat(opt, NULL, math::quaternion());

	if (ref != NULL)
		quat = GetQuat(opt, ref, math::quaternion());
	return CalcBound(size, quat);
}

/* the world rotation, made again first where m_update_flag asks for it (Sony's copy out of line,
   0x81242738; name ours) */
static __attribute__((noinline)) const math::quaternion &WorldRot(Widget *w)
{
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();
	return w->m_world_rot;
}

/* the world position (Sony's copy out of line, 0x81242754; name ours) */
static __attribute__((noinline)) math::v4 WorldPos(Widget *w)
{
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();
	return w->m_world_matrix.w();
}

/* the vector taken through the matrix (Sony's copy out of line, 0x81242778; name ours) */
static __attribute__((noinline)) math::v4 Transform(const math::matrix &m, const math::v4 &v)
{
	return math::v4(m.getImpl() * v.getImpl());
}

/* the hit box's center offset, zero without a hit box (Sony's copy out of line, 0x812427a0;
   name ours) */
static __attribute__((noinline)) math::v4 HitBoxOffset(Widget *w)
{
	if (w->m_hit_box != NULL)
		return w->m_hit_box[0];
	return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
}

/* the world and the local matrix (Sony's copies out of line, 0x812427bc and 0x812427d8; names
   ours) */
static __attribute__((noinline)) const math::matrix &WorldMatrix(Widget *w)
{
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();
	return w->m_world_matrix;
}

static __attribute__((noinline)) const math::matrix &LocalMatrix(Widget *w)
{
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();
	return w->m_local_matrix;
}

/* the hit at the point of the widget's space filled in, with the normal of the face it is on, as
   CalcHitBoxToLine has it inline at each of its hits (name ours) */
static __attribute__((always_inline)) void FillBoxHit(Widget *w, HitInfo *info, const math::v4 &local,
						     const math::v4 &normal)
{
	info->LocalPos() = math::v4(sce::Vectormath::Simd::Aos::Vector4(local.getImpl().getXYZ(), 1.0f));
	info->m_pos = Transform(WorldMatrix(w), info->LocalPos());
	info->m_local_hit = Transform(LocalMatrix(w), info->LocalPos());
	info->m_widget_pos = WorldPos(w);
	info->m_widget_rot = math::v4(sce::Vectormath::Simd::Aos::Vector4(WorldRot(w).getImpl()));
	info->m_widget_size = *w->GetSize(0);
	info->m_widget_scale = w->m_align_offs;
	info->m_hit_valid = 1;
	info->m_normal = normal;
}

int32_t Widget::CalcHitBoxToLine(const math::v4 &size, const math::v4 &start, const math::v4 &end,
				 HitInfo *info)
{
	math::v4 from = start;
	math::matrix inverse = WorldRot(this).toMatrix(0.0001f).transpose();
	math::v4 dir = end - from;
	math::v4 local = Transform(inverse, from - WorldPos(this));
	math::v4 pos = local - HitBoxOffset(this);
	math::v4 local_dir = Transform(inverse, dir);
	math::v4 half = size * 0.5f;
	float hx = half.extract_x();
	float hy = half.extract_y();
	float hz = half.extract_z();
	float hw = half.extract_w();

	if (__builtin_fabsf(local_dir.extract_x()) <= 0.001f &&
	    __builtin_fabsf(local_dir.extract_y()) <= 0.001f) {
		/* the line runs along z: it hits where the point is inside the box's face */
		if (__builtin_fabsf(pos.extract_x()) <= hx && __builtin_fabsf(pos.extract_y()) <= hy) {
			if (info == NULL)
				return 1;
			FillBoxHit(this, info, math::v4(pos.extract_x(), pos.extract_y(), hz, pos.extract_w()),
				   math::v4(0.0f, 0.0f, hz < 0.0f ? -1.0f : 1.0f, 0.0f));
			return 1;
		}
		return 0;
	}

	/* the line taken across each corner of the box */
	math::v4 a = CrossKeepW(local_dir, math::v4(hx, hy, hz, hw) - pos);
	math::v4 b = CrossKeepW(local_dir, math::v4(-hx, hy, hz, hw) - pos);
	math::v4 c = CrossKeepW(local_dir, math::v4(-hx, -hy, hz, hw) - pos);
	math::v4 d = CrossKeepW(local_dir, math::v4(hx, -hy, hz, hw) - pos);
	math::v4 e = CrossKeepW(local_dir, math::v4(hx, -hy, -hz, hw) - pos);
	math::v4 f = CrossKeepW(local_dir, math::v4(-hx, -hy, -hz, hw) - pos);
	math::v4 g = CrossKeepW(local_dir, math::v4(-hx, hy, -hz, hw) - pos);

	CrossKeepW(local_dir, math::v4(hx, hy, -hz, hw) - pos);

	/* the line passes outside the box where it goes round one of three faces' corners the
	   same way */
	if ((a.extract_z() * b.extract_z() > 0.0f && b.extract_z() * c.extract_z() > 0.0f &&
	     c.extract_z() * d.extract_z() > 0.0f && d.extract_z() * a.extract_z() > 0.0f) ||
	    (c.extract_y() * d.extract_y() > 0.0f && d.extract_y() * e.extract_y() > 0.0f &&
	     e.extract_y() * f.extract_y() > 0.0f && f.extract_y() * c.extract_y() > 0.0f) ||
	    (b.extract_x() * c.extract_x() > 0.0f && c.extract_x() * f.extract_x() > 0.0f &&
	     f.extract_x() * g.extract_x() > 0.0f && g.extract_x() * b.extract_x() > 0.0f))
		return 0;
	if (info == NULL)
		return 1;

	/* the faces toward the point: where the line crosses each within reach */
	float bx = hx;
	float by = hy;
	float bz = hz;

	if (pos.extract_x() < 0.0f)
		bx = -bx;
	if (pos.extract_y() < 0.0f)
		by = -by;
	if (pos.extract_z() < 0.0f)
		bz = -bz;

	float t;
	math::v4 hit;

	if (!(__builtin_fabsf(local_dir.extract_z()) < 0.001f)) {
		t = (bz - pos.extract_z()) / local_dir.extract_z();
		if (t > 0.2f && t < 5.0f) {
			hit = pos + local_dir * t;
			if (__builtin_fabsf(hit.extract_x()) <= __builtin_fabsf(bx) &&
			    __builtin_fabsf(hit.extract_y()) <= __builtin_fabsf(by)) {
				FillBoxHit(this, info, hit + HitBoxOffset(this),
					   math::v4(0.0f, 0.0f, bz < 0.0f ? -1.0f : 1.0f, 0.0f));
				return 1;
			}
		}
	}
	if (!(__builtin_fabsf(local_dir.extract_y()) < 0.001f)) {
		t = (by - pos.extract_y()) / local_dir.extract_y();
		if (t > 0.2f && t < 5.0f) {
			hit = pos + local_dir * t;
			if (__builtin_fabsf(hit.extract_x()) <= __builtin_fabsf(bx) &&
			    __builtin_fabsf(hit.extract_z()) <= __builtin_fabsf(bz)) {
				FillBoxHit(this, info, hit + HitBoxOffset(this),
					   math::v4(0.0f, by < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f));
				return 1;
			}
		}
	}
	if (!(__builtin_fabsf(local_dir.extract_x()) < 0.001f)) {
		t = (bx - pos.extract_x()) / local_dir.extract_x();
		if (t > 0.2f && t < 5.0f) {
			hit = pos + local_dir * t;
			if (__builtin_fabsf(hit.extract_z()) <= __builtin_fabsf(bz) &&
			    __builtin_fabsf(hit.extract_y()) <= __builtin_fabsf(by)) {
				FillBoxHit(this, info, hit + HitBoxOffset(this),
					   math::v4(bx < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f, 0.0f));
				return 1;
			}
		}
	}
	sce_paf_printf_dummy();
	return -1;
}

float32x2_t Widget::CalcDistPointToPlane(const math::v4 &point, const math::v4 &plane_point,
					 const math::v4 &normal, int32_t is_unit)
{
	using namespace sce::Vectormath::Simd;
	Aos::Vector4 n = normal.getImpl();

	if (is_unit == 0) {
		Aos::Vector4 scaled = n * recipf(Aos::length(n.getXYZ()));

		n = Aos::Vector4(scaled.getXYZ(), n.getW());
	}
	return vget_low_f32(Aos::dot(plane_point.getImpl() - point.getImpl(), n).get128());
}

/* the hit position taken into the widget's space and its own state filled in, as Sony's hit
   functions have it inline (name ours) */
static __attribute__((always_inline)) void FillHitInfo(Widget *w, HitInfo *info)
{
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();

	math::matrix rotation = w->m_world_rot.toMatrix(0.0001f);
	math::v4 local = rotation.transpose().getImpl() * (info->m_pos - w->GetWorldMatrix().w()).getImpl();

	info->LocalPos() = math::v4(sce::Vectormath::Simd::Aos::Vector4(local.getImpl().getXYZ(), 1.0f));
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();
	info->m_local_hit = math::v4(w->m_local_matrix.getImpl() * info->LocalPos().getImpl());
	info->m_widget_pos = w->GetWorldMatrix().w();
	if ((w->m_update_flag & 7) != 0)
		w->UpdateMatrix();
	info->m_widget_rot = math::v4(sce::Vectormath::Simd::Aos::Vector4(w->m_world_rot.getImpl()));
	info->m_widget_size = *w->GetSize(0);
	info->m_widget_scale = w->m_align_offs;
}

int32_t Widget::CalcHitLineToPlane(const math::v4 &start, const math::v4 &end, const math::v4 &pos,
				   const math::v4 &normal, HitInfo *info)
{
	float along = (pos - start).dot(normal);
	math::v4 dir = end - start;
	float rate = dir.dot(normal);

	if (__builtin_fabsf(rate) < 0.01f)
		return -1;
	if (info == NULL)
		return -1;

	math::v4 hit = start + dir * (along / rate);

	info->m_pos = math::v4(sce::Vectormath::Simd::Aos::Vector4(hit.getImpl().getXYZ(), 1.0f));
	FillHitInfo(this, info);
	return 1;
}

float Widget::CalcDistPointToLine(const math::v4 &point, const math::v4 &line_a,
				  const math::v4 &line_b, HitInfo *info)
{
	math::v4 normal(1.0f, 1.0f, 1.0f, 1.0f);
	float dist = 0.0f;
	math::v4 to_b = line_b - point;
	math::v4 to_a = line_a - point;

	if ((to_b.dot(to_b) > sce::Vectormath::Simd::floatInVec(0.01f)).getAsBool()) {
		math::v4 side = CrossKeepW(to_a, to_b);
		math::v4 across = CrossKeepW(side, line_b - line_a);
		sce::Vectormath::Simd::Aos::Vector4 scaled =
			across.getImpl() *
			sce::Vectormath::Simd::recipf(
				sce::Vectormath::Simd::Aos::length(across.getImpl().getXYZ()));

		normal = math::v4(sce::Vectormath::Simd::Aos::Vector4(scaled.getXYZ(), across.extract_w()));
		dist = vget_lane_f32(CalcDistPointToPlane(point, line_a, normal, 1), 0);
	}
	if (info != NULL) {
		math::v4 foot = point + normal * dist;

		info->m_pos = math::v4(sce::Vectormath::Simd::Aos::Vector4(foot.getImpl().getXYZ(), 1.0f));
		FillHitInfo(this, info);
	}
	return dist;
}

int32_t Widget::HitDetect_core(uint32_t mode, Event *e, uint32_t mask, const math::v4 &line_b,
			       const math::v4 &line_a, HitInfo *hit)
{
	if (m_debug_events) {
		uint32_t debug = m_scene->m_debug_mode;

		if (debug == 2 || debug == 3) {
			mask = 0xFFF0;
			mode = 0;
		}
	}

	uint32_t kind = (uint16_t)(e->m_type & mask);

	if (mode == 3 || mode == 2)
		return 0;
	if (!IsInputTarget() || IsShowHeld(this))
		return 0;

	uint32_t detail = e->m_detail;

	if (((detail >> 20) & 0xF) == 0) {
		if (!IsTouchFrontEventEnabled())
			return 0;
		detail = e->m_detail;
	}
	if (((detail >> 20) & 0xF) == 1 && !IsTouchRearEventEnabled())
		return 0;

	int32_t result = 1;

	if (kind != 32 && kind != 64)
		result = 2;

	HitInfo *near_hit = NULL;

	if (mode == 1)
		near_hit = hit;
	if ((m_update_flag & 7) != 0)
		UpdateMatrix();

	math::v4 point = m_world_matrix.w();

	if (m_hit_box != NULL) {
		math::v4 center = m_hit_box[0];

		if ((m_update_flag & 7) != 0)
			UpdateMatrix();
		point = point + (m_world_matrix.x() * center.extract_x() + m_world_matrix.y() * center.extract_y() +
				 m_world_matrix.z() * center.extract_z() + m_world_matrix.w() * center.extract_w());
	}

	float dist = CalcDistPointToLine(point, line_a, line_b, near_hit);
	math::v4 half = GetHitBoxSize(0) * 0.5f;
	float half_sq = (half.extract_x() * half.extract_x() + half.extract_y() * half.extract_y()) +
			(half.extract_z() * half.extract_z() + half.extract_w() * half.extract_w());

	if (dist * dist > half_sq) {
		if (near_hit != NULL) {
			near_hit->m_hit_valid = 0;
			near_hit->m_widget = NULL;
		}
		return 0;
	}
	if (mode == 1) {
		if (near_hit != NULL) {
			near_hit->m_hit_valid = 1;
			near_hit->m_widget = this;
		}
		return result;
	}

	HitInfo *far_hit = NULL;

	if (mode == 0)
		far_hit = hit;

	math::v4 size = half * 2.0f;

	if (CalcHitBoxToLine(size, line_a, line_b, far_hit) <= 0) {
		if (far_hit != NULL) {
			far_hit->m_hit_valid = 0;
			far_hit->m_widget = NULL;
		}
		return 0;
	}
	if (mode == 0) {
		if (far_hit != NULL) {
			far_hit->m_hit_valid = 1;
			far_hit->m_widget = this;
		}
		return result;
	}
	return 0;
}

/* the nearest hit HitDetectRouting found outside a hit box of mode 4, and its distance squared
   from the box's center and the box's area, which ScePafWidget_EB1D1CBC resets (names ours) */
static HitInfo s_nearest_hit;
static float s_nearest_dist = 100000000.0f;
static float s_nearest_area = 100000000.0f;

/* the children's order by depth from the camera (0x81162ea8) */
static bool DepthLess(Widget *a, Widget *b);

int32_t Widget::HitDetectRouting(Event *e, const math::v4 &line_b, const math::v4 &line_a,
				 TouchInfo *touch, HitInfo *hit)
{
	if (m_env == NULL)
		return -1;
	if (m_hold_count > 0)
		return -1;

	uint32_t mask = m_event_enable;
	uint32_t child_mode = m_hit_detect_mode;
	uint32_t self_mode = m_hit_detect_b_mode;
	uint32_t find_mode = m_hit_find_mode;

	if (m_debug_events) {
		uint32_t debug = m_scene->m_debug_mode;

		if (debug == 2 || debug == 3) {
			find_mode = 0;
			self_mode = 0;
			child_mode = 0;
			mask = 0xFFF0;
		}
	}

	bool clip_hit = false;
	uint32_t port = (e->m_detail >> 20) & 0xF;

	if (m_clip_children) {
		if (child_mode == 2)
			child_mode = 0;
		clip_hit = HitDetect_core(child_mode, e, mask, line_b, line_a, NULL) > 0;
	}
	hit->m_hit_valid = 0;
	hit->m_widget = NULL;
	if (m_children.size() != 0 && find_mode == 0 && (!m_clip_children || clip_hit) &&
	    (port != 1 || m_touch_rear_children != 0) && !IsShowHeld(this)) {
		if (m_scene->m_camera->m_draw_order == Camera::DRAW_ORDER_ZSORT) {
			if (m_children.size() != 0) {
				vector<Widget *> children;

				for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
					children.push_back(*it);
				sort(children.begin(), children.end(), DepthLess);
				for (uint32_t i = 0; i < children.size(); i++) {
					if (children[i]->HitDetectRouting(e, line_b, line_a, touch, hit) > 0 &&
					    hit->m_widget != NULL)
						return 2;
				}
			}
		} else {
			list<Widget *>::iterator it = m_children.end();

			do {
				--it;
				if ((*it)->HitDetectRouting(e, line_b, line_a, touch, hit) > 0 && hit->m_widget != NULL)
					return 2;
			} while (it != m_children.begin());
		}
	}
	if (HitDetect_core(self_mode, e, mask, line_b, line_a, hit) <= 0) {
		hit->m_widget = NULL;
		hit->m_hit_valid = 0;
		return 0;
	}

	/* a hit box of mode 4 takes the touch only inside its size; outside it the nearest such
	   hit is kept for when nothing takes the touch */
	math::v4 size = m_size;
	float dx = __builtin_fabsf(hit->LocalPos().extract_x());
	float dy = __builtin_fabsf(hit->LocalPos().extract_y());
	float dist = dx * dx + dy * dy;
	float area = size.extract_x() * size.extract_y();

	if (m_hit_box_mode != 4 ||
	    (size.extract_x() * 0.5f >= dx && size.extract_y() * 0.5f >= dy)) {
		if (s_nearest_hit.m_widget != NULL && s_nearest_dist + s_nearest_area <= dist + area)
			*hit = s_nearest_hit;
		return 2;
	}
	if (s_nearest_hit.m_widget == NULL || s_nearest_dist + s_nearest_area > dist + area) {
		s_nearest_hit = *hit;
		s_nearest_area = area;
		s_nearest_dist = dist;
	}
	hit->m_widget = NULL;
	hit->m_hit_valid = 0;
	return 2;
}

/* the event sent back to an ancestor, converted to a press where asked */
static __attribute__((always_inline)) void RouteBack(Widget *widget, Event *e, Event &ev, bool convert)
{
	ev.m_type = (ev.m_type & 0xFFF0FFFF) | Event::ROUTE_BACK;

	Environment *env = widget->m_env;

	if (convert && (widget->m_touch_gesture_mask & 0x30) == 0) {
		uint32_t type = e->m_type;

		if ((type & 0x40) != 0)
			ev.m_type = (ev.m_type & 0xFFFF0000) | 0x46;
		if ((type & 0x20) != 0)
			ev.m_type = (ev.m_type & 0xFFFF0000) | 0x27;
	}
	env->m_ev_subrouting.push_back(ev);
}

/* the event sent on to the widget's ancestors up to its scene, each one the touch line hits:
   one hit by its first mode gets it forward at the front of the routed events and, where its
   two modes are the same, back at the end; one hit by its second mode only gets it back; with
   convert (and no touch gestures 0x30 on the widget) the event goes back as a press (0x46
   rear, 0x27 front) (name ours) */
static __attribute__((noinline)) int32_t RouteToParents(Widget *widget, Event *e, const math::v4 &touch_p,
			      const math::v4 &cam_p, HitInfo *hit, bool convert)
{
	if (widget != widget->m_scene) {
		uint32_t port = (e->m_detail >> 20) & 0xF;
		uint32_t id = (uint8_t)e->m_detail;
		Event ev(*e);
		Widget *w = widget->m_parent;

		while (w != NULL) {
			uint32_t mode = w->m_hit_detect_mode;
			uint32_t mode_b = w->m_hit_detect_b_mode;

			ev.m_type = (ev.m_type & 0xFF0FFFFF) | Event::MODE_DIRECT;
			ev.m_target = w;

			int32_t result = w->HitDetect_core(mode, &ev, w->m_event_enable, touch_p, cam_p, hit);

			if (result > 0) {
				widget->m_env->AddHitStack(port, id, hit, true);
				if (result == 1) {
					ev.m_type = (ev.m_type & 0xFFF0FFFF) | Event::ROUTE_FORWARD;
					widget->m_env->m_ev_subrouting.push_front(ev);
					if (mode_b == mode)
						RouteBack(widget, e, ev, convert);
				}
			} else {
				int32_t result_b = w->HitDetect_core(mode_b, &ev, w->m_event_enable, touch_p, cam_p,
								     hit);

				if (result_b > 0) {
					widget->m_env->AddHitStack(port, id, hit, false);
					if (result_b == 1)
						RouteBack(widget, e, ev, convert);
				}
			}
			if (w == widget->m_scene)
				break;
			w = w->m_parent;
		}
	}
	return 0;
}

extern "C" int32_t ScePafWidget_08CE1C4B(Widget *widget, Event *e, uint32_t port, uint32_t id,
					 HitInfo *hit)
{
	if (widget->m_env == NULL)
		return -1;

	uint32_t type = e->m_type;
	HitInfo *last = NULL;
	Widget *now = hit->m_widget;
	Widget *old;
	int32_t result;

	if (id < 8)
		last = &widget->m_env->m_touch_hit_last[port][id];
	if ((uint16_t)type == 0x41 || (uint16_t)type == 0x21 || (old = last->m_widget) == now) {
		result = 0;
		if (now == NULL)
			result = 2;
	} else {
		Event ev(*e);

		result = 1;
		switch ((uint16_t)type) {
		case 0x43:
		case 0x23:
			/* a move: the old widget left, the new one entered */
			if (old != NULL) {
				ev.m_target = old;
				ev.m_type = (ev.m_type & 0xFF00FFFF) | Event::MODE_DIRECT | Event::ROUTE_TERM;
				if ((type & 0x20) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x26;
				if ((type & 0x40) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x45;
				widget->m_env->m_ev_subrouting.push_back(ev);
				now = hit->m_widget;
				type = e->m_type;
			}
			if (now != NULL) {
				ev.m_target = now;
				ev.m_type = (ev.m_type & 0xFF00FFFF) | Event::MODE_BROADCAST | Event::ROUTE_TERM;
				if ((type & 0x20) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x25;
				if ((type & 0x40) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x44;
				widget->m_env->m_ev_queue.PushFront(ev);
			}
			break;
		case 0x42:
		case 0x22:
			/* a release: the old widget left, the release and the enter sent to the new one */
			if (old != NULL) {
				ev.m_target = old;
				ev.m_type = (ev.m_type & 0xFF00FFFF) | Event::MODE_DIRECT | Event::ROUTE_TERM;
				if ((type & 0x20) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x26;
				if ((type & 0x40) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x45;
				widget->m_env->m_ev_subrouting.push_back(ev);
				now = hit->m_widget;
				type = e->m_type;
			}
			if ((type & 0x20) != 0)
				ev.m_type = (ev.m_type & 0xFFFF0000) | 0x22;
			if ((type & 0x40) != 0)
				ev.m_type = (ev.m_type & 0xFFFF0000) | 0x42;
			ev.SetTarget(now);
			ev.m_type = (ev.m_type & 0xFF00FFFF) | Event::MODE_BROADCAST | Event::ROUTE_TERM;
			widget->m_env->m_ev_queue.PushFront(ev);
			type = e->m_type;
			if (hit->m_widget != NULL) {
				ev.m_target = hit->m_widget;
				ev.m_type = (ev.m_type & 0xFF00FFFF) | Event::MODE_BROADCAST | Event::ROUTE_TERM;
				if ((type & 0x20) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x25;
				if ((type & 0x40) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x44;
				widget->m_env->m_ev_queue.PushFront(ev);
			}
			break;
		case 0x47:
		case 0x28:
			/* a cancel: sent to the old widget and the new one */
			if (old != NULL) {
				ev.m_target = old;
				ev.m_type = (ev.m_type & 0xFF00FFFF) | Event::MODE_DIRECT | Event::ROUTE_TERM;
				if ((type & 0x20) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x28;
				if ((type & 0x40) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x47;
				widget->m_env->m_ev_subrouting.push_back(ev);
				now = hit->m_widget;
				type = e->m_type;
			}
			if (now != NULL) {
				ev.m_target = now;
				ev.m_type = (ev.m_type & 0xFF00FFFF) | Event::MODE_DIRECT | Event::ROUTE_TERM;
				if ((type & 0x20) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x28;
				if ((type & 0x40) != 0)
					ev.m_type = (ev.m_type & 0xFFFF0000) | 0x47;
				widget->m_env->m_ev_queue.PushFront(ev);
			}
			break;
		}
	}
	if (id < 8 && hit != NULL) {
		HitInfo *kept = &widget->m_env->m_touch_hit[port][id];

		*kept = *hit;
		kept->m_touch_id = id;
		kept->m_port = port;
	}
	return result;
}

/* the touch's hit kept and the event sent to the widget: back through the hit change
   (ScePafWidget_08CE1C4B) unless grabbed, and where that sends nothing, forward and back to the
   widget itself and on to its ancestors (name ours) */
static __attribute__((noinline)) int32_t RouteHitEvent(Widget *widget, Event *e, const math::v4 &touch_p,
			     const math::v4 &cam_p, TouchInfo *touch, HitInfo *hit)
{
	uint32_t detail = e->m_detail;

	widget->m_env->AddHitStack((detail >> 20) & 0xF, (uint8_t)detail, hit, false);

	uint32_t kind = (uint16_t)e->m_type;

	if ((kind == 0x41 || kind == 0x21) && widget->m_debug_events)
		widget->m_scene->SetDebugFocus(widget);
	if ((e->m_type & Event::DETAIL_GRABBED) == 0) {
		e->m_type = (e->m_type & 0xFFF0FFFF) | Event::ROUTE_BACK;
		if (ScePafWidget_08CE1C4B(widget, e, (detail >> 20) & 0xF, (uint8_t)detail, hit) != 0)
			return 0;
	}

	Event ev(*e);

	ev.SetTarget(widget);
	ev.m_type = (ev.m_type & 0xFFF0FFFF) | Event::ROUTE_FORWARD;
	widget->m_env->m_ev_subrouting.push_back(ev);
	ev.m_type = (ev.m_type & 0xFFF0FFFF) | Event::ROUTE_BACK;
	widget->m_env->m_ev_subrouting.push_back(ev);
	RouteToParents(widget, e, touch_p, cam_p, hit, false);
	return 0;
}


/* a touch sent to the widget holding it (Environment::m_touch_capture): taken by its second
   hit-detect mode as grabbed, then on through the hit change (ScePafWidget_08CE1C4B) and, where
   that sends nothing, to the widget itself and on to its ancestors as a press */
int32_t Widget::GrabEventRouting(Event *e, const math::v4 &touch_p, const math::v4 &cam_p,
				 TouchInfo *t_info, HitInfo *h_info)
{
	uint32_t detail = e->m_detail;
	uint32_t port = (detail >> 20) & 0xF;
	uint32_t mask = m_event_enable;

	if (m_debug_events) {
		uint32_t debug = m_scene->m_debug_mode;

		if (debug == 2 || debug == 3)
			mask = 0xFFF0;
	}
	e->m_type = (e->m_type & 0x0FFFFFFF) | Event::DETAIL_GRABBED;

	bool taken = false;
	int32_t result = HitDetect_core(m_hit_detect_b_mode, e, mask, touch_p, cam_p, h_info);

	if (result > 0) {
		m_env->AddHitStack(port, (uint8_t)detail, h_info, false);
		taken = result == 1;
	}
	e->m_type = (e->m_type & 0xFFF0FFFF) | Event::ROUTE_TERM;

	int32_t moved = ScePafWidget_08CE1C4B(this, e, port, (uint8_t)detail, h_info);

	if (moved == 0 || moved == 2) {
		Event ev(*e);

		ev.SetTarget(this);
		ev.m_type = (ev.m_type & 0xFFF0FFFF) | Event::ROUTE_FORWARD;
		m_env->m_ev_subrouting.push_back(ev);
		ev.m_type = (ev.m_type & 0xFFF0FFFF) | Event::ROUTE_TERM;
		m_env->m_ev_subrouting.push_back(ev);
		if (moved == 0)
			RouteToParents(this, e, touch_p, cam_p, h_info, true);
	}
	return taken;
}

/* a touch event broadcast from the widget: the line under the touch taken through the scene's
   camera and kept for the touch; a touch held by the widget or a descendant goes to it
   (GrabEventRouting, or ScePafWidget_38CDFD1D while it drags), any other to the widget it hits
   (HitDetectRouting, else the nearest hit outside a hit box of mode 4) */
extern "C" int32_t ScePafWidget_EB1D1CBC(Widget *widget, Event *e)
{
	Environment *env = widget->m_env;

	if (env == NULL || widget->m_scene == NULL)
		return -1;
	env->m_touch_hits.clear();
	env->m_hit_stack.clear();

	uint32_t detail = e->m_detail;
	uint32_t port = (detail >> 20) & 0xF;
	uint32_t id = (uint8_t)detail;
	Camera *camera = widget->m_scene->m_camera;

	if (camera == NULL)
		return -1;

	TouchInfo touch;

	touch.m_pos = math::v2((float)e->m_data[0], (float)e->m_data[1]);
	touch.reserved_b = (float)e->m_data[2];

	const math::matrix &camera_world = camera->GetWorldMatrix();

	ScePafWidget_72F479CA(camera);

	math::matrix world = camera_world;

	touch.m_has_line = true;
	touch.m_depth = camera->m_distance;
	touch.m_type = (uint16_t)e->m_type;
	touch.m_line_start = world.w();
	touch.m_line_end = math::v4(world.getImpl() *
				    sce::Vectormath::Simd::Aos::Vector4(-touch.m_pos.extract_x(),
									touch.m_pos.extract_y(),
									touch.m_depth, 1.0f));
	touch.m_camera_matrix = world;
	if (id < 8 && port < 2) {
		TouchInfo *state = &widget->m_env->m_touch_state[port][id];

		*state = touch;
		state->m_port = port;
		state->m_id = id;
	}

	HitInfo hit;
	Widget *grab;
	int32_t result;

	if (port >= 2 || id >= 8 || (grab = widget->m_env->m_touch_capture[port][id].m_widget) == NULL ||
	    (e->m_type & Event::DETAIL_GRABBED) != 0) {
		result = widget->HitDetectRouting(e, touch.m_line_end, touch.m_line_start, &touch, &hit);

		Widget *target = hit.m_widget;

		if (target == NULL && s_nearest_hit.m_widget != NULL) {
			hit = s_nearest_hit;
			result = 1;
			target = s_nearest_hit.m_widget;
		}
		s_nearest_hit.m_widget = NULL;
		s_nearest_hit.m_hit_valid = 0;
		s_nearest_dist = 100000000.0f;
		s_nearest_area = 100000000.0f;
		if (target != NULL)
			RouteHitEvent(target, e, touch.m_line_end, touch.m_line_start, &touch, &hit);
		else
			result = 0;
	} else {
		Widget *held = NULL;

		if (grab->m_hold_count <= 0) {
			Widget *w = grab;

			do {
				if (w == widget) {
					held = grab;
					break;
				}
				w = w->m_parent;
			} while (w != NULL);
		}
		result = 0;
		if (held == grab) {
			Event ev(*e);

			e->m_type = (e->m_type & 0x0FFFFFFF) | Event::DETAIL_GRABBED;
			if (grab->m_drag_state[0] == NULL || grab->m_drag_mode == 0 || grab->m_drag_mode == 0x1000) {
				grab->GrabEventRouting(e, touch.m_line_end, touch.m_line_start, &touch, &hit);
				if (hit.m_widget == NULL) {
					/* nothing took it: a cancel (0x46 rear, 0x27 front) goes out */
					if ((ev.m_type & 0x40) != 0)
						ev.m_type = (ev.m_type & 0xFFFF0000) | 0x46;
					if ((ev.m_type & 0x20) != 0)
						ev.m_type = (ev.m_type & 0xFFFF0000) | 0x27;
					ev.m_type = (ev.m_type & 0x0F0FFFFF) | Event::DETAIL_GRABBED |
						    Event::MODE_BROADCAST;
					widget->m_env->m_ev_queue.PushFront(ev);
				}
			} else {
				ScePafWidget_38CDFD1D(grab, e);
			}
			result = 1;
		}
	}
	return result;
}

/* a touch event ended at the widget and sent back to its ancestors along the touch's line
   (Environment::EventLoop; name ours) */
int32_t Widget::RouteTouchEvent(Event *e)
{
	if (m_env == NULL || m_scene == NULL)
		return -1;

	Event ev(*e);

	ev.SetTarget(this);
	ev.m_type = (ev.m_type & 0xFFF0FFFF) | Event::ROUTE_TERM;
	m_env->m_ev_subrouting.push_back(ev);

	uint32_t detail = e->m_detail;
	uint32_t port = (detail >> 20) & 0xF;
	uint32_t id = (uint8_t)detail;
	Environment *env = m_env;
	TouchInfo *touch;

	if (id < 8 && port < 2) {
		touch = &env->m_touch_state[port][id];
		touch->m_port = port;
		touch->m_id = id;
	} else {
		touch = NULL;
	}

	HitInfo hit;

	RouteToParents(this, e, touch->m_line_end, touch->m_line_start, &hit, false);
	return 0;
}

float Widget::CalcDistLineToLine(const math::v4 &point_a, const math::v4 &dir_a,
				 const math::v4 &point_b, const math::v4 &dir_b,
				 math::v4 *closest_a, math::v4 *closest_b)
{
	float aa = dir_a.dot(dir_a).getAsFloat();
	float bb = dir_b.dot(dir_b).getAsFloat();
	math::v4 diff = point_a - point_b;
	float ab = dir_b.dot(dir_a).getAsFloat();
	float a_diff = dir_a.dot(diff).getAsFloat();
	float b_diff = dir_b.dot(diff).getAsFloat();
	float denom = ab * ab - aa * bb;
	float inverse = 0.0f;
	float t;
	float s;

	if (__builtin_fabsf(denom) >= 0.0001f)
		inverse = 1.0f / denom;
	t = (a_diff * bb - b_diff * ab) * inverse;
	s = (-(b_diff * aa) - -(a_diff * ab)) * inverse;

	math::v4 on_a = point_a + dir_a * t;
	math::v4 on_b = point_b + dir_b * s;

	if (closest_a != NULL)
		*closest_a = on_a;
	if (closest_b != NULL)
		*closest_b = on_b;
	return __builtin_sqrtf((on_a - on_b).dot(on_a - on_b));
}

math::rect Widget::CalcHitRect()
{
	Camera *camera = NULL;

	if (m_scene != NULL)
		camera = m_scene->m_camera;
	if (camera == NULL) {
		math::v4 center = GetHitBoxCenter();

		return math::rect(math::v2(center.extract_x(), center.extract_y()),
				  math::v2(0.0f, 0.0f));
	}

	math::v4 center = GetHitBoxCenter();
	math::v4 size = GetHitBoxSize(0);
	sce::Vectormath::Simd::Aos::Vector4 world_size =
		m_world_matrix.getImpl() *
		sce::Vectormath::Simd::Aos::Vector4(size.extract_x(), size.extract_y(),
						    size.extract_z(), 0.0f);
	math::v4 camera_pos = camera->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	float depth = camera_pos.extract_z() - center.extract_z();

	ScePafWidget_72F479CA(camera);

	float scale;

	if (depth == 0.0f)
		scale = 0.0f;
	else
		scale = camera->m_distance / depth;
	return math::rect(math::v2(center.extract_x(), center.extract_y()) * scale,
			  math::v2(world_size.getX(), world_size.getY()) * scale);
}

Widget *Widget::FindChild(const IDParam &id, int32_t option)
{
	bool none;

	{
		IDParam empty;

		none = id.m_idhash == empty.m_idhash;
	}
	if (none)
		return NULL;

	uint32_t hash = id.m_idhash;
	list<Widget *>::iterator it = m_children.begin();

	if (option == 2) {
		for (; it != m_children.end(); ++it) {
			Widget *child = *it;

			if (child->m_name.m_idhash == hash && child->m_hold_count <= 0)
				return child;
		}
		return NULL;
	}
	for (; it != m_children.end(); ++it) {
		Widget *child = *it;

		if (child->m_hold_count <= 0) {
			if (child->m_name.m_idhash == hash)
				return child;

			Widget *found = child->FindChild(id, option);

			hash = id.m_idhash;
			if (found != NULL) {
				if (option != 0)
					found = child;
				return found;
			}
		}
	}
	return NULL;
}

void Widget::remove_pad_gesture_mask(uint32_t port_id, uint32_t type)
{
	if (m_env != NULL && port_id < 8) {
		inputdevice::InputGestureListener *listener =
			static_cast<inputdevice::InputGestureListener *>(
				common::SharedPtr<inputdevice::InputListener>(
					m_env->m_pad_gesture_listener).get());

		if (listener != NULL) {
			if ((type & 4) != 0 || (type & 8) != 0)
				listener->DetachGestureListener(port_id, 15, this);
			if ((type & 1) != 0 || (type & 2) != 0)
				listener->DetachGestureListener(port_id, 10, this);
		}
	}
}

void Widget::remove_touch_gesture_mask(uint32_t port_id, uint32_t touch_id, uint32_t type)
{
	if (touch_id < 8 && m_env != NULL) {
		if (port_id != 0 && port_id != 1)
			return;

		inputdevice::InputGestureListener *listener =
			static_cast<inputdevice::InputGestureListener *>(m_env->GetTouchGestureListener(port_id).get());

		if (listener == NULL)
			return;
		if ((type & 0x10) != 0)
			listener->DetachGestureListener(touch_id, 20, this);
		if ((type & 0x20) != 0)
			listener->DetachGestureListener(touch_id, 25, this);
		if ((type & 0x40) != 0)
			listener->DetachGestureListener(touch_id, 50, this);
		if ((type & 4) != 0 || (type & 8) != 0)
			listener->DetachGestureListener(touch_id, 15, this);
		if ((type & 1) != 0 || (type & 2) != 0)
			listener->DetachGestureListener(touch_id, 10, this);
	}
}

common::SharedPtr<inputdevice::GestureListener>
Widget::ApplyTouchGesture(uint32_t port, uint32_t id, uint32_t mask,
			  const inputdevice::GestureParam *param)
{
	if (m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();
	apply_touch_gesture_mask(port, id, mask);

	if (m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	int32_t priority;

	if ((mask & 0x80) != 0)
		priority = 60;
	else if ((mask & 0x40) != 0)
		priority = 50;
	else if ((mask & 0x20) != 0)
		priority = 25;
	else if ((mask & 0x10) != 0)
		priority = 20;
	else if ((mask & 4) != 0 || (mask & 8) != 0)
		priority = 15;
	else if ((mask & 1) != 0 || (mask & 2) != 0)
		priority = 10;
	else
		priority = 0;

	inputdevice::InputGestureListener *input = NULL;

	if (port == 0 || port == 1)
		input =
			static_cast<inputdevice::InputGestureListener *>(m_env->GetTouchGestureListener(port).get());

	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(id, priority, this)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() != NULL && param != NULL)
		listener->SetGestureParam(param);
	return listener;
}

common::SharedPtr<inputdevice::GestureListener>
Widget::ApplyPadGesture(uint32_t port, uint32_t type, const inputdevice::GestureParam *param)
{
	if (m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();
	apply_pad_gesture_mask(port, type);

	if (m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	int32_t priority;

	if ((int8_t)type < 0)
		priority = 60;
	else if ((type & 0x40) != 0)
		priority = 50;
	else if ((type & 0x20) != 0)
		priority = 25;
	else if ((type & 0x10) != 0)
		priority = 20;
	else if ((type & 4) != 0 || (type & 8) != 0)
		priority = 15;
	else if ((type & 1) != 0 || (type & 2) != 0)
		priority = 10;
	else
		priority = 0;

	inputdevice::InputGestureListener *input =
		static_cast<inputdevice::InputGestureListener *>(m_env->m_pad_gesture_listener.get());
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(port, priority, this)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() != NULL && param != NULL)
		listener->SetGestureParam(param);
	return listener;
}

common::SharedPtr<inputdevice::GestureListener> Widget::apply_pad_gesture_tap(uint32_t port_id,
									       uint32_t type)
{
	if (m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input =
		static_cast<inputdevice::InputGestureListener *>(m_env->m_pad_gesture_listener.get());
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(port_id, 10, this)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() == NULL) {
		listener = common::SharedPtr<inputdevice::GestureListener>(
			new PadTapListener(this, port_id));

		bool failed = false;

		input =
			static_cast<inputdevice::InputGestureListener *>(m_env->m_pad_gesture_listener.get());
		if (input == NULL || input->AttachGestureListener(port_id, 10, listener) < 0)
			failed = true;
		if (failed)
			listener = common::SharedPtr<inputdevice::GestureListener>();
	}
	if (listener.get() != NULL) {
		PadTapListener *tap = static_cast<PadTapListener *>(listener.get());

		if ((type & 2) != 0)
			tap->m_target = 2;
		else if ((type & 1) != 0)
			tap->m_target = 1;
		tap->m_param.m_on_press = m_pad_tap_on_press;
	}
	return listener;
}

common::SharedPtr<inputdevice::GestureListener>
Widget::apply_pad_gesture_longpress(uint32_t port_id, uint32_t type)
{
	if (m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input =
		static_cast<inputdevice::InputGestureListener *>(m_env->m_pad_gesture_listener.get());
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(port_id, 15, this)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() == NULL) {
		listener = common::SharedPtr<inputdevice::GestureListener>(
			new PadLongPressListener(this, port_id));

		bool failed = false;

		input =
			static_cast<inputdevice::InputGestureListener *>(m_env->m_pad_gesture_listener.get());
		if (input == NULL || input->AttachGestureListener(port_id, 15, listener) < 0)
			failed = true;
		if (failed)
			listener = common::SharedPtr<inputdevice::GestureListener>();
	}
	if (listener.get() != NULL) {
		PadLongPressListener *hold = static_cast<PadLongPressListener *>(listener.get());

		if ((type & 8) != 0) {
			hold->m_repeat = true;
			hold->m_param.m_hold_time = 300;
		} else if ((type & 4) != 0) {
			hold->m_repeat = false;
			hold->m_param.m_hold_time = 500;
		}
	}
	return listener;
}

/* the gesture listener of the widget's touch port (port 0 the front panel, 1 the rear), NULL for
   another port */
static inline inputdevice::InputGestureListener *TouchGestureInput(Widget *widget, uint32_t port)
{
	return static_cast<inputdevice::InputGestureListener *>(
		widget->m_env->GetTouchGestureListener(port).get());
}

/* the listener attached to the touch with the priority by the port's gesture listener; false
   where the port has none or it refuses */
static inline bool AttachTouchGesture(Widget *widget, uint32_t port, uint32_t touch_id,
				      uint32_t priority,
				      common::SharedPtr<inputdevice::GestureListener> listener)
{
	bool failed = false;
	inputdevice::InputGestureListener *input = TouchGestureInput(widget, port);

	if (input == NULL || input->AttachGestureListener(touch_id, priority, listener) < 0)
		failed = true;
	return !failed;
}

common::SharedPtr<inputdevice::GestureListener> ScePafWidget_DE7EC3CF(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect)
{
	if (widget->m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input = TouchGestureInput(widget, port);
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(touch_id, 60, widget)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() == NULL && port == 1) {
		for (uint32_t i = 0; i < 8; i++) {
			input = TouchGestureInput(widget, port);

			common::SharedPtr<inputdevice::GestureListener> other =
				input != NULL ? input->GetGestureListener(i, 60, widget)
					      : common::SharedPtr<inputdevice::GestureListener>();

			if (other.get() != NULL && AttachTouchGesture(widget, port, touch_id, 60, other))
				return other;
		}
	}
	if (listener.get() == NULL) {
		if (port == 1 && widget->m_env->m_motion_noise_filter)
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new MotionGestureListener(port, touch_id, widget));
		if (listener.get() != NULL) {
			if (!AttachTouchGesture(widget, port, touch_id, 60, listener))
				listener = common::SharedPtr<inputdevice::GestureListener>();
		}
	}
	return listener;
}

common::SharedPtr<inputdevice::GestureListener> ScePafWidget_712AD1C7(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect)
{
	if (widget->m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input = TouchGestureInput(widget, port);
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(touch_id, 50, widget)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() == NULL && port == 1) {
		for (uint32_t i = 0; i < 8; i++) {
			input = TouchGestureInput(widget, port);

			common::SharedPtr<inputdevice::GestureListener> other =
				input != NULL ? input->GetGestureListener(i, 50, widget)
					      : common::SharedPtr<inputdevice::GestureListener>();

			if (other.get() != NULL && AttachTouchGesture(widget, port, touch_id, 50, other))
				return other;
		}
	}
	if (listener.get() == NULL) {
		if (port != 0)
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new RearDragGestureListener(port, touch_id, widget));
		else
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new DragGestureListener(port, touch_id, widget));

		if (!AttachTouchGesture(widget, port, touch_id, 50, listener))
			listener = common::SharedPtr<inputdevice::GestureListener>();
	}
	if (listener.get() != NULL) {
		DragGestureParam param;

		if (rect != NULL) {
			param.m_rect = *rect;
		} else {
			Camera *camera = widget->m_scene->m_camera;

			ScePafWidget_72F479CA(camera);
			param.m_rect.set(0.0f, 0.0f, camera->m_screen_width, camera->m_screen_height);
		}
		param.m_dir = 2;
		static_cast<DragGestureListener *>(listener.get())->SetParam(&param);
	}
	return listener;
}

common::SharedPtr<inputdevice::GestureListener> ScePafWidget_1975BA26(Widget *widget,
	uint32_t port, uint32_t touch_id)
{
	if (widget->m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input = TouchGestureInput(widget, port);
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(touch_id, 25, widget)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() != NULL)
		return listener;
	for (uint32_t i = 0; i < 8; i++) {
		input = TouchGestureInput(widget, port);
		listener = input != NULL ? input->GetGestureListener(i, 25, widget)
					 : common::SharedPtr<inputdevice::GestureListener>();
		if (listener.get() != NULL && listener->m_user_data == widget)
			break;
	}
	if (listener.get() == NULL)
		listener = common::SharedPtr<inputdevice::GestureListener>(
			new TouchLongPressListener(port, touch_id, widget));
	if (listener.get() != NULL) {
		if (!AttachTouchGesture(widget, port, touch_id, 25, listener))
			listener = common::SharedPtr<inputdevice::GestureListener>();
	}
	return listener;
}

common::SharedPtr<inputdevice::GestureListener> ScePafWidget_F4625FAD(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect)
{
	if (widget->m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input = TouchGestureInput(widget, port);
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(touch_id, 20, widget)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() != NULL)
		return listener;
	for (uint32_t i = 0; i < 8; i++) {
		input = TouchGestureInput(widget, port);
		listener = input != NULL ? input->GetGestureListener(i, 20, widget)
					 : common::SharedPtr<inputdevice::GestureListener>();
		if (listener.get() != NULL && listener->m_user_data == widget)
			break;
	}
	if (listener.get() == NULL) {
		if (port != 0)
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new PinchGestureListener(port, touch_id, widget));
		else
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new TouchTapListener(port, touch_id, widget));
		/* no tap or hold asked for: any move ends the tap */
		if (listener.get() != NULL && (type & 1) == 0 && (type & 2) == 0 && (type & 4) == 0 &&
		    (type & 8) == 0)
			static_cast<TouchTapListener *>(listener.get())->m_param.m_move_tolerance = 0;
	}
	if (listener.get() != NULL) {
		if (!AttachTouchGesture(widget, port, touch_id, 20, listener))
			listener = common::SharedPtr<inputdevice::GestureListener>();
	}
	return listener;
}

common::SharedPtr<inputdevice::GestureListener> ScePafWidget_4607555F(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect)
{
	if (widget->m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input = TouchGestureInput(widget, port);
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(touch_id, 15, widget)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() == NULL) {
		listener = common::SharedPtr<inputdevice::GestureListener>(
			new HoldGestureListener(port, touch_id, widget));

		if (!AttachTouchGesture(widget, port, touch_id, 15, listener))
			listener = common::SharedPtr<inputdevice::GestureListener>();
	}
	if (listener.get() != NULL) {
		HoldGestureListener *hold = static_cast<HoldGestureListener *>(listener.get());

		if (rect != NULL)
			hold->m_param.m_rect = *rect;
		if ((type & 8) != 0) {
			hold->m_short_hold = true;
			hold->m_param.m_hold_time = 300;
		} else if ((type & 4) != 0) {
			hold->m_short_hold = false;
			hold->m_param.m_hold_time = 500;
		}
	}
	return listener;
}

common::SharedPtr<inputdevice::GestureListener> ScePafWidget_5E7757AA(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect)
{
	if (widget->m_env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input = TouchGestureInput(widget, port);
	common::SharedPtr<inputdevice::GestureListener> listener =
		input != NULL ? input->GetGestureListener(touch_id, 10, widget)
			      : common::SharedPtr<inputdevice::GestureListener>();

	if (listener.get() == NULL) {
		if (port != 0)
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new RearTapGestureListener(port, touch_id, widget));
		else
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new MoveGestureListener(port, touch_id, widget));

		if (!AttachTouchGesture(widget, port, touch_id, 10, listener))
			listener = common::SharedPtr<inputdevice::GestureListener>();
	}
	if (listener.get() != NULL) {
		if (port == 0) {
			MoveGestureListener *move = static_cast<MoveGestureListener *>(listener.get());

			if (rect != NULL)
				move->m_param.m_rect = *rect;
			if ((type & 2) != 0)
				move->m_tap_target = 2;
			else if ((type & 1) != 0)
				move->m_tap_target = 1;
		} else if (port == 1) {
			RearTapGestureListener *tap = static_cast<RearTapGestureListener *>(listener.get());

			if ((type & 2) != 0)
				tap->m_tap_count = 2;
			else if ((type & 1) != 0)
				tap->m_tap_count = 1;
		}
	}
	return listener;
}

void Widget::apply_touch_gesture_mask(uint32_t port_id, uint32_t touch_id, uint32_t type)
{
	math::rect hit = CalcHitRect();
	math::rect *rect = NULL;

	if (m_enable_tap)
		rect = &hit;
	if ((type & 1) != 0 || (type & 2) != 0)
		ScePafWidget_5E7757AA(this, port_id, touch_id, type, rect);
	if ((type & 4) != 0 || (type & 8) != 0)
		ScePafWidget_4607555F(this, port_id, touch_id, type, rect);
	if ((type & 0x10) != 0)
		ScePafWidget_F4625FAD(this, port_id, touch_id, type, rect);
	if ((type & 0x20) != 0)
		ScePafWidget_1975BA26(this, port_id, touch_id);
	if ((type & 0x40) != 0)
		ScePafWidget_712AD1C7(this, port_id, touch_id, type, rect);
	if ((type & 0x80) != 0)
		ScePafWidget_DE7EC3CF(this, port_id, touch_id, type, rect);
}

void Widget::apply_pad_gesture_mask(uint32_t port_id, uint32_t type)
{
	if ((type & 1) != 0 || (type & 2) != 0)
		apply_pad_gesture_tap(port_id, type);
	if ((type & 4) != 0 || (type & 8) != 0)
		apply_pad_gesture_longpress(port_id, type);
}

/* the key mask of the keycode, 0 for none: KeycodeToMask again, the copy Sony's widget code has
   (name ours) */
static uint32_t PadKeyMask(int32_t keycode)
{
	switch (keycode) {
	case 1:
		return 1;
	case 2:
		return 2;
	case 3:
		return 4;
	case 4:
		return 8;
	case 27:
		return 16;
	case 28:
		return 32;
	case 29:
		return 64;
	case 30:
		return 128;
	case 31:
		return 1024;
	case 32:
		return 2048;
	case 33:
		return 0x10000;
	case 34:
		return 0x20000;
	case 35:
		return 0x40000;
	case 36:
		return 0x80000;
	case 37:
		return 512;
	case 38:
		return 256;
	case 39:
		return 4096;
	default:
		return 0;
	}
}

int32_t Widget::ReleasePadCaptureDir(uint32_t key, bool reset)
{
	int8_t port = m_pad_dir_port;

	if (port < 0)
		return -1;
	if (reset) {
		uint32_t mask = 31;

		if (key != 255)
			mask = PadKeyMask(key) & 0x1F;
		ScePafMisc_C4E8CBE6(mask);
		remove_pad_gesture_mask(m_pad_dir_port, m_touch_gesture_mask);
		port = m_pad_dir_port;
	}
	if ((uint32_t)port < 5 && this == m_env->m_pad_capture_dir[port]) {
		m_env->m_pad_capture_dir[port] = NULL;
		m_pad_dir_port = -1;
		OnPadCaptureChanged(false, 1, port, key);

		Event e(0x802, 0x100000, 0x10000, 0, 0, 0, 0, 0);

		DoEvent(537133067, &e);
		e.SetTarget(this);
		e.Term();
		m_env->m_ev_queue.PushBack(e);
		return 0;
	}
	return -1;
}

int32_t Widget::ReleasePadCaptureButton(uint32_t key, bool reset)
{
	int8_t port = m_pad_button_port;

	if (port < 0)
		return -1;
	if (reset) {
		uint32_t mask = 992;

		if (key != 255)
			mask = PadKeyMask(key) & 0x3E0;
		ScePafMisc_C4E8CBE6(mask);
		remove_pad_gesture_mask(m_pad_button_port, m_touch_gesture_mask);
		port = m_pad_button_port;
	}
	if ((uint32_t)port < 5 && this == m_env->m_pad_capture_button[port]) {
		m_env->m_pad_capture_button[port] = NULL;
		m_pad_button_port = -1;
		OnPadCaptureChanged(false, 0, port, key);

		Event e(0x802, 0x100000, 0x10000, 0, 0, 0, 0, 0);

		DoEvent(537133067, &e);
		e.SetTarget(this);
		e.Term();
		m_env->m_ev_queue.PushBack(e);
		return 0;
	}
	return -1;
}

int32_t Widget::SetFocusRootFocus(Widget *target, bool notify)
{
	Environment *env = m_env;

	if (env == NULL)
		return -1;

	Widget *old = m_focus_root != NULL ? (Widget *)m_focus_root->m_state : NULL;

	if (old != target) {
		old = m_focus_root != NULL ? (Widget *)m_focus_root->m_state : NULL;
		SetFocusRootState((uint32_t)target);
		if (old != NULL)
			old->m_focused = false;
		if (target != NULL) {
			target->m_focused = true;
			env = m_env;
			env->UpdateFocus(notify ? this : NULL, NULL);
			return 0;
		}
		env = m_env;
		env->UpdateFocus(NULL, notify ? this : NULL);
		return 0;
	}
	if (target == NULL)
		env->UpdateFocus(NULL, notify ? this : NULL);
	else
		env->UpdateFocus(notify ? this : NULL, NULL);
	return 0;
}

int32_t Widget::ReleaseFocus()
{
	Widget *root = FindFocusRoot(this);

	if (root == NULL)
		return -1;
	if ((Widget *)root->m_focus_root->m_state == this)
		return root->SetFocusRootFocus(NULL, true);
	return -1;
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

int32_t Widget::SetDrawObjMaskBit(const void *param, int32_t obj)
{
	return SetDrawObjMaskBit(param, obj, 0);
}

int32_t Widget::SetDrawObjMaskBit(const void *param, int32_t obj, uint32_t bit)
{
	vector<DrawObjEntry>::iterator it;

	for (it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == (uint32_t)obj)
			break;
	}
	if (it != m_drawobjs.end()) {
		uint32_t mask = 1 << bit;

		if ((it->m_attr.m_value & mask) != 0) {
			m_env->RemoveTextureRequest(this, obj, bit);
			if (m_texture_request_count != 0)
				m_texture_request_count--;
		}
		m_env->AddTextureRequest(this, static_cast<const intrusive_ptr<TextureSource> *>(param),
					 obj, bit);
		it->m_attr.m_value |= mask;
		m_texture_request_count++;
	}
	return 0;
}

int32_t Widget::OnTextureSource(const intrusive_ptr<TextureSource> *source, int32_t obj, int32_t bit)
{
	if (m_env == NULL)
		return -1;
	if ((*source)->GetType() != 2)
		return -1;

	int32_t result = 0;
	intrusive_ptr<graph::Surface> surface;

	(*source)->GetSurface(&surface);
	if (surface.get() != NULL) {
		SetTexture(surface, obj, bit);
		m_adjust_x = m_adjust_x;
		m_adjust_y = m_adjust_y;
		m_adjust_z = m_adjust_z;
		UpdateLayoutAttribute();
		UpdateAdjustSize();

		Event e(0x1005, this, 0x40000, 0, 0, 0, 0, 0);

		m_env->m_ev_queue.PushBack(e);
		DoEvent(0x20100005, &e);
	} else {
		result = (*source)->GetResult();

		Event e(0x1006, this, 0x40000, 0, 0, 0, 0, 0);

		e.m_data[0] = result;
		e.m_data[1] = obj;
		m_env->m_ev_queue.PushBack(e);
		DoEvent(0x20100006, &e);
		if (result != 0) {
			if ((*source)->Reserved9() == 0) {
				Event e2(0x1007, this, 0x40000, 0, 0, 0, 0, 0);

				e2.m_data[0] = result;
				e2.m_data[1] = obj;
				m_env->m_ev_queue.PushBack(e2);
				DoEvent(0x20100007, &e2);
			}
			if (result != (int32_t)0x80AF5022)
				return result;
		}
	}

	vector<DrawObjEntry>::iterator it = m_drawobjs.begin();

	while (it != m_drawobjs.end() && it->m_attr.m_id != (uint32_t)obj)
		++it;
	it->m_attr.m_value &= ~(1 << bit);
	if (m_texture_request_count != 0)
		m_texture_request_count--;
	return result;
}

intrusive_ptr<TextureSource> Widget::GetDrawObjMaskBit(int32_t obj)
{
	if (m_texture_request_count != 0 && m_env != NULL) {
		vector<DrawObjEntry>::iterator it;

		for (it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
			if (it->m_attr.m_id == (uint32_t)obj)
				break;
		}
		if (it == m_drawobjs.end())
			return intrusive_ptr<TextureSource>();

		uint32_t mask = it->m_attr.m_value;
		uint32_t bit = 0;

		while ((mask & 1) == 0 && bit < 4) {
			bit++;
			mask >>= 1;
		}
		return m_env->FindTextureRequest(this, it->m_attr.m_id, bit);
	}
	return intrusive_ptr<TextureSource>();
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

/* the entry of the resolution data for the resolution, or for the one it is based on; none where
   the scene knows neither (Sony's copy inline in UpdateResolutionTimer; unlike
   get_resolution_data it gives no first entry instead; name ours) */
static inline ResolutionData *FindResolutionData(Widget *w, ResolutionData *array, uint32_t num,
						 uint32_t id)
{
	if (array == NULL || num == 0)
		return NULL;

	SceneResolution *resolution = ScePafWidget_CDC90BA2(w->m_scene->m_env_param, id);

	if (resolution == NULL)
		return NULL;

	uint32_t i;

	for (i = 0; i < num; i++) {
		if (array[i].m_id == resolution->m_id)
			return &array[i];
	}
	for (i = 0; i < num; i++) {
		if (array[i].m_id == resolution->m_base_id)
			return &array[i];
	}
	return NULL;
}

void Widget::UpdateResolutionTimer()
{
	/* the next resolution's change started where none runs */
	if (m_resolution_timer == NULL && m_resolution_timer_next != NULL) {
		if (m_layouter != NULL)
			m_layouter->m_reso_dirty_begin = true;

		Timer *timer = m_resolution_timer_next;
		Scene *scene = m_scene;

		m_resolution_timer = timer;
		m_resolution_timer_next = NULL;
		m_resolution_id = m_resolution_id_next;
		m_resolution_id_next = 0xFF;
		if (this == scene && m_env != NULL) {
			/* the scene: its screen resized where the sizes change, and itself */
			SceneResolution *pre =
				ScePafWidget_CDC90BA2(scene->m_env_param, m_resolution_id_pre);
			SceneResolution *resolution =
				ScePafWidget_CDC90BA2(m_scene->m_env_param, m_scene->m_resolution_id);

			if (resolution != NULL &&
			    (pre == NULL || pre->m_viewport_width != resolution->m_viewport_width ||
			     pre->m_viewport_height != resolution->m_viewport_height ||
			     pre->m_scissor_width != resolution->m_scissor_width ||
			     pre->m_scissor_height != resolution->m_scissor_height))
				m_scene->UpdateScreenSize(new Timer(*timer));
			SetSize(m_size, new Timer(timer->GetTotalTime(), timer->m_func_x), ANIM_SIZE, 0,
				0.0f, NULL, NULL);
		} else if (scene != NULL && scene->IsResolutionActMorph(m_resolution_id)) {
			/* a widget: moved, sized and turned to the resolution's values */
			Timer *morph = m_resolution_timer;
			ResolutionData *entry =
				FindResolutionData(this, m_pos_array, m_pos_num, m_resolution_id);

			if (entry != NULL) {
				math::v4 pos(entry->m_value[0], entry->m_value[1], entry->m_value[2], 0.0f);
				Timer *t = new Timer(morph->GetTotalTime(), morph->m_func_x);

				SetPosHermite(pos, NULL, t, ANIM_MOVE, 2, 0.0f, NULL, NULL);
			}
			entry = FindResolutionData(this, m_size_array, m_size_num, m_resolution_id);
			if (entry != NULL) {
				math::v4 size(entry->m_value[0], entry->m_value[1], entry->m_value[2],
					      0.0f);
				Timer *t = new Timer(morph->GetTotalTime(), morph->m_func_x);

				SetSize(size, t, ANIM_SIZE, 2, 0.0f, NULL, NULL);
			}
			entry = FindResolutionData(this, m_rot_array, m_rot_num, m_resolution_id);
			if (entry != NULL) {
				math::v4 rot(entry->m_value[0], entry->m_value[1], entry->m_value[2], 0.0f);
				Timer *t = new Timer(morph->GetTotalTime(), morph->m_func_x);

				SetRot(rot, 0, t, ANIM_ROT, 2, 0.0f, NULL, NULL);
			}
			if (m_focus_shape != NULL)
				ScePafWidget_13B246E0(this, m_focus_shape);
			if (m_transition != 0xFF && m_transition_complete) {
				m_resolution_morph = true;
				ScePafWidget_F1176ACC(this, (common::transition::Type)m_transition, 0.0f,
						      NULL, NULL);
			}
		}
	}

	/* a finished change given up */
	Timer *timer = m_resolution_timer;

	if (timer != NULL && timer->m_state == Timer::State_Finished) {
		delete timer;
		m_resolution_timer = NULL;
		if (m_transition != 0xFF && m_resolution_morph) {
			ScePafWidget_44B22ACA(this, (common::transition::Type)m_transition, 0.0f, NULL,
					      NULL);
			m_resolution_morph = false;
		}
		if (m_scene != NULL && m_scene->IsResolutionActMorph(m_resolution_id))
			m_resolution_id_pre = m_resolution_id;
		if (m_layouter != NULL)
			m_layouter->m_reso_dirty_end = true;
	}
	if (m_resolution_timer != NULL)
		m_resolution_timer->DoTick();
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

int32_t Widget::CalcHitBoxToPlane(const math::v4 &size, const math::v4 &pos, const math::v4 &normal,
				  float *dist, HitInfo *info)
{
	using sce::Vectormath::Simd::Aos::Vector4;

	/* the plane in the hit box's space */
	math::matrix inverse = GetWorldRot().toMatrix(0.0001f).transpose();
	math::v4 local_normal(inverse.getImpl() * normal.getImpl());
	math::v4 offset(0.0f, 0.0f, 0.0f, 0.0f);

	if (m_hit_box != NULL)
		offset = m_hit_box[0];

	/* the corner of the box farthest against the normal */
	math::v4 half = size * 0.5f;
	float corner_x = -1.0f;
	float corner_y = -1.0f;
	float corner_z = -1.0f;

	if (local_normal.extract_x() < 0.00001f)
		corner_x = 1.0f;
	if (local_normal.extract_y() < 0.00001f)
		corner_y = 1.0f;
	if (local_normal.extract_z() < 0.00001f)
		corner_z = 1.0f;

	math::v4 corner(corner_x * half.extract_x(), corner_y * half.extract_y(),
			corner_z * half.extract_z(), 1.0f);

	if (dist == NULL)
		return -1;

	/* how far the corner lies before the plane: below 0 it crosses it */
	math::v4 local_pos(inverse.getImpl() * (pos - offset).getImpl());
	float distance = vget_lane_f32(CalcDistPointToPlane(corner, local_pos, local_normal, 1), 0);

	*dist = distance;

	int32_t result = 0;

	if (distance < 0.0f)
		result = 1;
	if (info == NULL)
		return result;

	math::v4 hit_offset(0.0f, 0.0f, 0.0f, 0.0f);

	if (m_hit_box != NULL)
		hit_offset = m_hit_box[0];

	math::v4 local = corner + hit_offset;

	info->LocalPos() = math::v4(Vector4(local.getImpl().getXYZ(), 1.0f));
	info->m_pos = math::v4(GetWorldMatrix().getImpl() * info->LocalPos().getImpl());
	if ((m_update_flag & 7) != 0)
		UpdateMatrix();
	info->m_local_hit = math::v4(m_local_matrix.getImpl() * info->LocalPos().getImpl());
	info->m_widget_size = *GetSize(0);
	info->m_widget_scale = m_align_offs;
	return result;
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

common::SharedPtr<graph::DrawObj> Widget::SwapDrawObj(common::SharedPtr<graph::DrawObj> obj, int32_t id)
{
	vector<DrawObjEntry>::iterator it;

	for (it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == id)
			break;
	}
	if (it == m_drawobjs.end())
		return common::SharedPtr<graph::DrawObj>();

	common::SharedPtr<graph::DrawObj> old = it->m_obj;

	s_swapped_drawobjs.push_back(old);
	it->m_obj = obj;
	if (obj.get() != NULL)
		SetDrawObjSurfaceDraw(this, obj.get());
	UpdateSize();
	return old;
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
	m_texture_request_count = 0;
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
	m_resolution_morph = true;
	m_pad_tap_on_press = true;
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
	m_saved_drag_mode = -1;
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
	m_keep_cb = NULL;
	m_keep_arg = NULL;
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


uint32_t Widget::s_upmat_count_L;
uint32_t Widget::s_upmat_count_W;
int32_t Widget::s_base_destroy_count = 1;
int32_t Widget::s_destroying_count;
int32_t Widget::s_instance_count;
list<RestructInfo> Widget::s_list_restruct;

int32_t Widget::GetBaseDestroyCount()
{
	return s_base_destroy_count;
}

/* the depth of the widget's position in its camera's view (name ours) */
static float CameraDepth(Widget *widget)
{
	const math::matrix &view = widget->m_scene->m_camera->GetWorldMatrix();
	math::v4 pos = widget->GetWorldMatrix().w();
	math::v4 depth = view.x() * pos.extract_x() + view.y() * pos.extract_y() +
			 view.z() * pos.extract_z() + view.w() * pos.extract_w();

	return depth.extract_z();
}

static bool DepthLess(Widget *a, Widget *b)
{
	return CameraDepth(a) < CameraDepth(b);
}
}
}

/* the adjustment a widget without its own is given (name ours) */
paf::ui::AdjustParam ScePafWidget_BE1CEA2E;
