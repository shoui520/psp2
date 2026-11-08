/*
 * paf::ui::Widget, as far as it is converted: the rest of its functions are
 * still ScePafWidget's.
 */
#include <stddef.h>
#include <graphics/drawobj/draw_obj.h>
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

namespace ui {

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
	for (map<uint32_t, AnimQueue *>::iterator it = m_anims.begin(); it != m_anims.end(); it++) {
		if ((it->first & 0xFFFFFF) == (uint32_t)id) {
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

Camera *Widget::GetCamera() const
{
	if (m_scene != NULL)
		return m_scene->m_camera;
	return NULL;
}

int32_t Widget::KillAnimType(int32_t type)
{
	int32_t count = 0;

	for (map<uint32_t, AnimQueue *>::iterator it = m_anims.begin(); it != m_anims.end(); it++)
		count += it->second->EraseType(type, true);
	return count;
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

bool Widget::IsPrepareObjs()
{
	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_flag0 && it->m_obj->m_flags.m_prepare)
			return true;
	}
	return false;
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

int32_t Widget::KillAnim(int32_t id)
{
	map<uint32_t, AnimQueue *>::iterator it = m_anims.begin();

	if (it == m_anims.end())
		return -1;
	while ((it->first & 0xFFFFFF) != (uint32_t)id) {
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

void Widget::KillObjAnim(int32_t obj, int32_t id)
{
	KillAnim(obj + id);
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

void Widget::Update()
{
	if (m_hold_count <= 0) {
		uint32_t num = m_drawobjs.size();

		for (uint32_t i = 0; i != num; i++)
			m_drawobjs[i].m_obj->Update();
	}
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

int32_t Widget::SetAdjust(uint32_t ax, uint32_t ay, uint32_t az)
{
	m_adjust_x = ax;
	m_adjust_y = ay;
	m_adjust_z = az;
	UpdateLayoutAttribute();
	UpdateAdjustSize();
	return 0;
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

void Widget::SetBaseDestroyCount(int32_t count)
{
	s_base_destroy_count = count;
}


int32_t Widget::s_base_destroy_count;

int32_t Widget::GetBaseDestroyCount()
{
	return s_base_destroy_count;
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

int32_t Widget::SetOriginOffset_core(const math::v4 &offs, uint32_t notify_layout)
{
	m_origin_offs = offs;
	SetUpdateParam(1);
	return 0;
}

int32_t Widget::SetAnchorOffset_core(const math::v4 &offs, uint32_t notify_layout)
{
	m_anchor_offs = offs;
	SetUpdateParam(1);
	return 0;
}
}
}
