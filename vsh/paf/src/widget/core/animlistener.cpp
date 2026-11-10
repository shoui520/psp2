/*
 * paf::ui::AnimListener and its kinds (TOOL 0x8113820c..0x81138ef8): a timer listener that animates
 * a widget, or one of its draw objects, through a property (its type and the two functions that
 * set and get it). No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/animlistener.h>
#include <widget/core/layouter.h>
#include <widget/core/environment.h>
#include <widget/core/widget.h>
#include <math/rect.h>
#include <widget/w_scene.h>
#include <widget/w_supplier.h>

/* the panel object's revolution (panel_obj.h, which makes every includer a copy of its debug
   colours) */
namespace paf {
namespace graph {
class PanelObj;
math::v4 GetPanelRevolution(const PanelObj *obj);
}
}

extern "C" void ScePafGraphics_0C968320(paf::graph::PanelObj *obj, const paf::math::v4 &revolution);

using sce::Vectormath::Simd::Aos::Vector4;
using sce::Vectormath::Simd::Aos::mulPerElem;

namespace paf {
namespace ui {

AnimListener::AnimListener(Timer *t, Widget *w, AnimCB cb_anim, AnimCB cb_fin, void *data)
	: TimerListener(t, data), m_widget(w), m_cb_anim(cb_anim), m_cb_finish(cb_fin),
	  m_flags(FLAG_NONE), m_type(0xFFFF), m_semantic(SEMANTIC_NONE), m_slot_type(0)
{
	m_keep_x = false;
	m_keep_y = false;
	m_keep_z = false;
	m_keep_w = false;
	if (!Layouter::IsEnter())
		m_flags = FLAG_NOTIFY_LAYOUT;
}

/* the flags a property setter gives the widget: the layout told where the listener says so (as
   the bit what), and the tick's first or later (name ours) */
static inline __attribute__((always_inline)) uint32_t TickFlags(AnimListener *anim, uint32_t what)
{
	uint32_t flags = (anim->m_flags & AnimListener::FLAG_NOTIFY_LAYOUT) ? what : 0;

	if (anim->m_timer->m_first_tick)
		return flags | 0x100;
	return flags | 0x200;
}

void AnimListener::PropPosSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetPos_core(data, TickFlags(anim, 1), false);
}

math::v4 AnimListener::PropPosGet(AnimListener *anim)
{
	return anim->m_widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
}

void AnimListener::PropColorSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetColor_core(data);
}

math::v4 AnimListener::PropColorGet(AnimListener *anim)
{
	Widget *w = anim->m_widget;
	math::v4 color = math::v4(Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(w->m_color)))))),
		vdupq_n_f32(1.0f / 255.0f))));

	color.set_w(w->m_color_alpha);
	return color;
}

void AnimListener::PropRotSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetRot_core(data, 0, TickFlags(anim, 8));
}

math::v4 AnimListener::PropRotGet(AnimListener *anim)
{
	return anim->m_widget->GetRot(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
}

void AnimListener::PropQuatSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetQuat_core(math::quaternion(sce::Vectormath::Simd::Aos::Quat(data.getImpl())),
					       TickFlags(anim, 8));
}

math::v4 AnimListener::PropQuatGet(AnimListener *anim)
{
	math::quaternion quat = anim->m_widget->GetQuat(0, NULL, math::quaternion(0.0f, 0.0f, 0.0f, 1.0f));

	return math::v4(Vector4(quat.getImpl()));
}

void AnimListener::PropSizeSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetSize_core(data, TickFlags(anim, 2), false);
}

math::v4 AnimListener::PropSizeGet(AnimListener *anim)
{
	return *anim->m_widget->GetSize(0);
}

void AnimListener::PropScaleSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetScale_core(data, TickFlags(anim, 4));
}

math::v4 AnimListener::PropScaleGet(AnimListener *anim)
{
	return *anim->m_widget->GetScale(0);
}

void AnimListener::PropAlignSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetAlignOffset_core(data, TickFlags(anim, 16));
}

math::v4 AnimListener::PropAlignGet(AnimListener *anim)
{
	return anim->m_widget->GetAlignOffsetToCenter(0);
}

void AnimListener::PropOriginSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetOriginOffset_core(data, TickFlags(anim, 32));
}

math::v4 AnimListener::PropOriginGet(AnimListener *anim)
{
	return anim->m_widget->GetOriginOffset(0);
}

void AnimListener::PropAnchorSet(AnimListener *anim, math::v4 data)
{
	anim->m_widget->SetAnchorOffset_core(data, TickFlags(anim, 64));
}

math::v4 AnimListener::PropAnchorGet(AnimListener *anim)
{
	return anim->m_widget->GetAnchorOffset(0);
}

void AnimListener::PropMetaAlphaSet(AnimListener *anim, float data)
{
	anim->m_widget->SetMetaAlpha_core(data);
}

float AnimListener::PropMetaAlphaGet(AnimListener *anim)
{
	return anim->m_widget->m_meta_alpha;
}

void AnimListener::PropShowAlphaSet(AnimListener *anim, float data)
{
	anim->m_widget->SetShowAlpha_core(data);
}

float AnimListener::PropShowAlphaGet(AnimListener *anim)
{
	return anim->m_widget->m_show_alpha;
}

/* the properties' tables, in the order of Sony's data; the second rotations turn the shorter
   way round, and the revolution is the panel object's (names ours) */
AnimPropertyV4 AnimListener::s_prop_move = { 0x10000, AnimListener::PropPosSet, AnimListener::PropPosGet };
AnimPropertyV4 AnimListener::s_prop_color = { 0x10001, AnimListener::PropColorSet,
					      AnimListener::PropColorGet };
AnimPropertyV4 AnimListener::s_prop_size = { 0x10004, AnimListener::PropSizeSet,
					     AnimListener::PropSizeGet };
AnimPropertyV4 AnimListener::s_prop_scale = { 0x10005, AnimListener::PropScaleSet,
					      AnimListener::PropScaleGet };
AnimPropertyV4 AnimListener::s_prop_rot = { 0x10006, AnimListener::PropRotSet, AnimListener::PropRotGet };
AnimPropertyV4 AnimListener::s_prop_rot_short = { 0x10006, AnimListener::PropRotSet, AnimListener::PropRotGet };
AnimPropertyV4 AnimListener::s_prop_quat = { 0x10007, AnimListener::PropQuatSet,
					     AnimListener::PropQuatGet };
AnimPropertyV4 AnimListener::s_prop_align = { 0x10008, AnimListener::PropAlignSet,
					      AnimListener::PropAlignGet };
AnimPropertyV4 AnimListener::s_prop_origin = { 0x10009, AnimListener::PropOriginSet,
					       AnimListener::PropOriginGet };
AnimPropertyV4 AnimListener::s_prop_anchor = { 0x1000A, AnimListener::PropAnchorSet,
					       AnimListener::PropAnchorGet };
AnimPropertyF1 AnimListener::s_prop_metaalpha = { 0x10003, AnimListener::PropMetaAlphaSet,
						  AnimListener::PropMetaAlphaGet };
AnimPropertyF1 AnimListener::s_prop_showalpha = { 0x10002, AnimListener::PropShowAlphaSet,
						  AnimListener::PropShowAlphaGet };
AnimPropertyV4 AnimListenerObj::s_prop_obj_move = { 0x20100, AnimListenerObj::PropObjPosSet,
						    AnimListenerObj::PropObjPosGet };
AnimPropertyV4 AnimListenerObj::s_prop_obj_color = { 0x20300, AnimListenerObj::PropObjColorSet,
						     AnimListenerObj::PropObjColorGet };
AnimPropertyV4 AnimListenerObj::s_prop_obj_size = { 0x20200, AnimListenerObj::PropObjSizeSet,
						    AnimListenerObj::PropObjSizeGet };
AnimPropertyV4 AnimListenerObj::s_prop_obj_rot = { 0x20400, AnimListenerObj::PropObjRotSet,
						   AnimListenerObj::PropObjRotGet };
static AnimPropertyV4 s_prop_obj_rot_short = { 0x20400, AnimListenerObj::PropObjRotSet,
					AnimListenerObj::PropObjRotGet };
AnimPropertyV4 AnimListenerObj::s_prop_obj_texregion = { 0x20600, AnimListenerObj::PropObjTexRegionSet,
							 AnimListenerObj::PropObjTexRegionGet };
AnimPropertyV4 AnimListenerObj::s_prop_obj_revolution = { 0x20700, AnimListenerObj::PropObjRevolutionSet,
							  AnimListenerObj::PropObjRevolutionGet };


/* the angles of the second rotation property made the shorter way round: each one that is
   further than a half turn from its target is moved a turn nearer (name ours) */
static void ShortenAngles(math::v4 &from, const math::v4 &to)
{
	const float two_pi = 6.2831855f;
	Vector4 diff = to.getImpl() - from.getImpl();
	Vector4 turn = mulPerElem(Vector4(3.1415927f), Vector4(2.0f));
	Vector4 abs_diff = sce::Vectormath::Simd::Aos::absPerElem(diff);
	Vector4 abs_lo = sce::Vectormath::Simd::Aos::absPerElem(diff - turn);
	Vector4 abs_hi = sce::Vectormath::Simd::Aos::absPerElem(diff + turn);

	if (abs_diff.getX() <= abs_lo.getX()) {
		if (abs_diff.getX() > abs_hi.getX())
			from.set_x(from.extract_x() - two_pi);
	} else {
		from.set_x(from.extract_x() + two_pi);
	}
	if (abs_diff.getY() <= abs_lo.getY()) {
		if (abs_diff.getY() > abs_hi.getY())
			from.set_y(from.extract_y() - two_pi);
	} else {
		from.set_y(from.extract_y() + two_pi);
	}
	if (abs_diff.getZ() <= abs_lo.getZ()) {
		if (abs_diff.getZ() > abs_hi.getZ())
			from.set_z(from.extract_z() - two_pi);
	} else {
		from.set_z(from.extract_z() + two_pi);
	}
}

void AnimListenerV4::OnTick(void *data1, void *data2)
{
	if (m_widget != NULL) {
		if (m_prop != NULL) {
			if (m_timer->m_first_tick) {
				m_anim_data[0] = m_prop->m_cb_get(this);
				if ((m_prop->m_type == 0x10006 && m_prop == &s_prop_rot_short) ||
				    (m_prop->m_type == 0x20400 && m_prop == &s_prop_obj_rot_short))
					ShortenAngles(m_anim_data[0], m_anim_data[1]);
			}

			math::v4 t = m_timer->GetValueV4();
			Vector4 p0 = m_anim_data[0].getImpl();
			Vector4 p1 = m_anim_data[1].getImpl();
			Vector4 result;

			if (m_prop->m_type == 0x10007) {
				math::quaternion q = math::quaternion::Slerp(
					math::quaternion(sce::Vectormath::Simd::Aos::Quat(p0)),
					math::quaternion(sce::Vectormath::Simd::Aos::Quat(p1)), t.extract_x(),
					1.0e-4f);

				result = Vector4(q.getImpl()) / sce::Vectormath::Simd::Aos::length(q.getImpl());
			} else if (m_herm_data != NULL) {
				Vector4 h0 = m_herm_data[0].getImpl();
				Vector4 h1 = m_herm_data[1].getImpl();
				Vector4 tv = t.getImpl();
				Vector4 t2 = mulPerElem(tv, tv);
				Vector4 a = mulPerElem(p0, Vector4(-3.0f)) - mulPerElem(h0, Vector4(2.0f)) +
					    mulPerElem(p1, Vector4(3.0f));
				Vector4 d = mulPerElem(((mulPerElem(p0, Vector4(2.0f)) + h0) -
							mulPerElem(p1, Vector4(2.0f))) + h1,
						       mulPerElem(t2, tv)) +
					    mulPerElem(a - h1, t2) + mulPerElem(h0, tv);

				result = d + p0;
			} else {
				result = mulPerElem(p1 - p0, t.getImpl()) + p0;
			}
			if (m_timer->m_state == Timer::State_Finished)
				result = p1;
			if (m_keep_x || m_keep_y || m_keep_z || m_keep_w) {
				math::v4 cur = m_prop->m_cb_get(this);

				if (m_keep_x)
					result.setX(cur.extract_x());
				if (m_keep_y)
					result.setY(cur.extract_y());
				if (m_keep_z)
					result.setZ(cur.extract_z());
				if (m_keep_w)
					result.setW(cur.extract_w());
			}
			m_prop->m_cb_set(this, math::v4(result));
		}
		if (m_cb_anim != NULL)
			m_cb_anim(m_timer, this, m_widget, &m_anim_data[0], m_data);
	}
}

void AnimListenerF1::OnTick(void *data1, void *data2)
{
	if (m_prop != NULL) {
		float p0 = m_anim_data[0];

		if (m_timer->m_first_tick) {
			p0 = m_prop->m_cb_get(this);
			m_anim_data[0] = p0;
		}

		float p1 = m_anim_data[1];
		float t = m_timer->m_mapped_x;
		float result = (p1 - p0) * t;

		if (m_herm_data != NULL) {
			float h0 = m_herm_data[0];
			float h1 = m_herm_data[1];
			float t2 = t * t;
			float t3 = t2 * t;
			float a = (h0 * 2.0f - p0 * -3.0f) + p1 * 3.0f;

			result = ((a - h1) * t2) + ((((p0 * 2.0f) + h0) - p1 * 2.0f) + h1) * t3;
			result = result + h0 * t;
		}
		result = result + p0;
		if (m_timer->m_state == Timer::State_Finished)
			result = p1;
		m_prop->m_cb_set(this, result);
	}
	if (m_cb_anim != NULL)
		m_cb_anim(m_timer, this, m_widget, &m_anim_data[0], m_data);
}

void AnimListenerV4::SetHermite(const math::v4 &src, const math::v4 &tangent0, const math::v4 &dst,
				const math::v4 &tangent1)
{
	math::v4 *herm = m_herm_data;

	if (herm == NULL) {
		herm = new math::v4[2];
		m_herm_data = herm;
	}
	herm[0] = tangent0;
	herm[1] = tangent1;
	m_anim_data[0] = src;
	m_anim_data[1] = dst;
}

void AnimListenerV4::GetHermite(math::v4 &src, math::v4 &tangent0, math::v4 &dst,
				math::v4 &tangent1)
{
	if (m_herm_data != NULL) {
		tangent0 = m_herm_data[0];
		tangent1 = m_herm_data[1];
	} else {
		tangent0 = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		tangent1 = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	src = m_anim_data[0];
	dst = m_anim_data[1];
}

void AnimListenerF1::SetHermite(float src, float tangent0, float dst, float tangent1)
{
	if (m_herm_data == NULL)
		m_herm_data = new float[2];
	m_herm_data[0] = tangent0;
	m_herm_data[1] = tangent1;
	m_anim_data[0] = src;
	m_anim_data[1] = dst;
}

void AnimListenerF1::GetHermite(float &src, float &tangent0, float &dst, float &tangent1)
{
	if (m_herm_data != NULL) {
		tangent0 = m_herm_data[0];
		tangent1 = m_herm_data[1];
	} else {
		tangent0 = 0.0f;
		tangent1 = 0.0f;
	}
	src = m_anim_data[0];
	dst = m_anim_data[1];
}

void AnimListenerObj::PropObjPosSet(AnimListener *anim, math::v4 data)
{
	static_cast<AnimListenerObj *>(anim)->m_obj->SetPos(math::v3(data.getImpl().getXYZ()));
}

math::v4 AnimListenerObj::PropObjPosGet(AnimListener *anim)
{
	return math::v4(math::v3(static_cast<AnimListenerObj *>(anim)->m_obj->m_pos), 1.0f);
}

void AnimListenerObj::PropObjColorSet(AnimListener *anim, math::v4 data)
{
	static_cast<AnimListenerObj *>(anim)->m_obj->SetColor(data);
}

math::v4 AnimListenerObj::PropObjColorGet(AnimListener *anim)
{
	return static_cast<AnimListenerObj *>(anim)->m_obj->GetColor();
}

void AnimListenerObj::PropObjSizeSet(AnimListener *anim, math::v4 data)
{
	static_cast<AnimListenerObj *>(anim)->m_obj->SetSize(math::v3(data.getImpl().getXYZ()));
}

math::v4 AnimListenerObj::PropObjSizeGet(AnimListener *anim)
{
	return math::v4(math::v3(static_cast<AnimListenerObj *>(anim)->m_obj->m_size), 0.0f);
}

void AnimListenerObj::PropObjRotSet(AnimListener *anim, math::v4 data)
{
	static_cast<AnimListenerObj *>(anim)->m_obj->SetRot(math::v3(data.getImpl().getXYZ()));
}

math::v4 AnimListenerObj::PropObjRotGet(AnimListener *anim)
{
	graph::DrawObj *obj = static_cast<AnimListenerObj *>(anim)->m_obj;

	if (obj->m_rot != NULL)
		return math::v4(*obj->m_rot, 0.0f);
	return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
}

void AnimListenerObj::PropObjTexRegionSet(AnimListener *anim, math::v4 data)
{
	static_cast<graph::PlaneObj *>(static_cast<AnimListenerObj *>(anim)->m_obj)->SetTexRegion(data);
}

math::v4 AnimListenerObj::PropObjTexRegionGet(AnimListener *anim)
{
	graph::PlaneObj *obj = static_cast<graph::PlaneObj *>(static_cast<AnimListenerObj *>(anim)->m_obj);

	if (obj->m_tex_region != NULL)
		return *obj->m_tex_region;
	return math::v4(0.0f, 0.0f, 1.0f, 1.0f);
}

void AnimListenerObj::PropObjRevolutionSet(AnimListener *anim, math::v4 data)
{
	ScePafGraphics_0C968320((graph::PanelObj *)static_cast<AnimListenerObj *>(anim)->m_obj,
				data);
}

math::v4 AnimListenerObj::PropObjRevolutionGet(AnimListener *anim)
{
	return graph::GetPanelRevolution(
		(graph::PanelObj *)static_cast<AnimListenerObj *>(anim)->m_obj);
}

TexAnimListenerObj::TexAnimListenerObj(Timer *t, Widget *w, graph::DrawObj *obj, int32_t columns,
				       int32_t rows, int32_t first, int32_t last, AnimCB cb_fin,
				       void *data)
	: AnimListenerObj(t, w, obj, math::v4(0.0f, 0.0f, 0.0f, 0.0f),
			  math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, cb_fin, data),
	  m_columns(columns > 1 ? columns : 1), m_rows(rows > 1 ? rows : 1), m_first(first),
	  m_last(last), m_column_ratio(1.0f / (float)columns), m_row_ratio(1.0f / (float)rows)
{
	int32_t count = 0;

	if (last <= first) {
		if (last < first)
			count = last - first - 1;
	} else {
		count = last - first + 1;
	}
	m_count = count;
}

/* the texture region of the frame (inlined into OnTick in Sony's code) */
static inline __attribute__((always_inline)) void SetFrameRegion(TexAnimListenerObj *anim, float rate)
{
	graph::PlaneObj *obj = static_cast<graph::PlaneObj *>(anim->m_obj);
	int32_t columns = anim->m_columns;
	int32_t frame;

	if (rate == 1.0f)
		frame = anim->m_last;
	else
		frame = anim->m_first + (int32_t)(rate * (float)anim->m_count);
	obj->SetTexRegion(math::v4(anim->m_column_ratio * (float)(frame % columns),
				   anim->m_row_ratio * (float)(frame / columns % anim->m_rows),
				   anim->m_column_ratio, anim->m_row_ratio));
}

void TexAnimListenerObj::TexAnim(float rate)
{
	SetFrameRegion(this, rate);
}

void TexAnimListenerObj::OnTick(void *data1, void *data2)
{
	SetFrameRegion(this, m_timer->m_mapped_x);
}

int32_t AnimQueueBase::EraseType(int32_t type, bool reserved)
{
	return HoldType(type);
}

void AnimQueueBase::LoopBackTangents(AnimListener *prev)
{
	if (prev == NULL)
		return;

	AnimListener *front = static_cast<AnimListener *>(GetFront());

	if (front->m_semantic == AnimListener::SEMANTIC_V4) {
		if (prev->m_semantic == AnimListener::SEMANTIC_V4) {
			AnimListenerV4 *a = static_cast<AnimListenerV4 *>(prev);
			AnimListenerV4 *f = static_cast<AnimListenerV4 *>(front);
			Vector4 h1(0.0f);

			if (a->m_herm_data != NULL)
				h1 = a->m_herm_data[1].getImpl();

			Vector4 start = a->m_anim_data[0].getImpl();
			Vector4 target = a->m_anim_data[1].getImpl();
			Vector4 next = f->GetTargetV4()->getImpl();
			Vector4 slope = mulPerElem(next - target, Vector4(0.3f));
			Vector4 h0;

			if (sce::Vectormath::Simd::Aos::allElemEqual(h1, Vector4(0.0f)).getAsBool())
				h0 = mulPerElem(target - start, Vector4(0.85f));
			else
				h0 = mulPerElem(h1, Vector4(0.85f));
			h0 = h0 + slope;

			Vector4 end = h0 + target;

			if (f->m_herm_data == NULL)
				f->m_herm_data = new math::v4[2];
			f->m_herm_data[0] = math::v4(h0);
			f->m_herm_data[1] = math::v4(next - end);
			f->m_anim_data[0] = math::v4(target);
			f->m_anim_data[1] = math::v4(next);
		}
	} else if (front->m_semantic == AnimListener::SEMANTIC_FLOAT) {
		if (prev->m_semantic == AnimListener::SEMANTIC_FLOAT) {
			AnimListenerF1 *a = static_cast<AnimListenerF1 *>(prev);
			AnimListenerF1 *f = static_cast<AnimListenerF1 *>(front);
			float h1 = 0.0f;

			if (a->m_herm_data != NULL)
				h1 = a->m_herm_data[1];

			float start = a->m_anim_data[0];
			float target = a->m_anim_data[1];
			float next = *f->GetTargetF();

			if (h1 == 0.0f)
				h1 = target - start;

			float h0 = (h1 * 0.85f) + ((next - target) * 0.3f);

			if (f->m_herm_data == NULL)
				f->m_herm_data = new float[2];
			f->m_herm_data[0] = h0;
			f->m_herm_data[1] = next - (target + h0);
			f->m_anim_data[0] = target;
			f->m_anim_data[1] = next;
		}
	}
}

void AnimQueueBase::SetFinishedLoopBack(bool hermite)
{
	TimerListener *front = GetFront();
	int32_t count = 0;

	for (; front != NULL; front = GetFront()) {
		Timer *timer = front->m_timer;

		if (timer != NULL && timer->m_state != Timer::State_Hold) {
			if (timer->m_nonpositive_total) {
				if (count > 5)
					break;
				front->DoTick(NULL, NULL);
				EraseFront(false);
				front->ResetTimer();
				PushBack(front);
				count++;
			} else {
				if (timer->m_state != Timer::State_Finished || timer->m_repeat != 0)
					break;
				EraseFront(false);
				if (hermite)
					LoopBackTangents(static_cast<AnimListener *>(front));
				front->ResetTimer();
				PushBack(front);
			}
		} else {
			EraseFront(false);
			delete front;
		}
	}
}

AnimQueue::~AnimQueue()
{
	for (int32_t i = 0; i < 3; i++) {
		if (m_queue[i] != NULL) {
			delete m_queue[i];
			m_queue[i] = NULL;
		}
	}
}

int32_t AnimQueue::PushBack(TimerListener *listener, uint32_t slot)
{
	if (slot >= 3)
		return -1;
	if (m_queue[slot] == NULL)
		m_queue[slot] = new AnimQueueBase;
	m_queue[slot]->PushBack(listener);
	return 0;
}

int32_t AnimQueue::PushFront(TimerListener *listener, uint32_t slot)
{
	if (slot >= 3)
		return -1;
	if (m_queue[slot] == NULL)
		m_queue[slot] = new AnimQueueBase;
	m_queue[slot]->PushFront(listener);
	return 0;
}

int32_t AnimQueue::EraseFront(bool auto_delete, uint32_t slot)
{
	if (slot >= 3)
		return -1;
	if (m_queue[slot] == NULL)
		return -1;

	int32_t ret = m_queue[slot]->EraseFront(auto_delete);

	if (m_queue[slot]->Size() == 0) {
		delete m_queue[slot];
		m_queue[slot] = NULL;
	}
	return ret;
}

float AnimQueue::DoTick(void *data1, void *data2)
{
	AnimQueueBase *move = m_queue[0];
	AnimQueueBase *loop = m_queue[2];
	float total = 0.0f;

	if (move != NULL && loop != NULL) {
		AnimListener *front = static_cast<AnimListener *>(move->GetFront());

		if (front->m_semantic == AnimListener::SEMANTIC_V4) {
			AnimListenerV4 *a = static_cast<AnimListenerV4 *>(front);
			Vector4 start(0.0f);
			Vector4 cur(0.0f);

			if (a->m_prop != NULL)
				start = a->m_prop->m_cb_get(a).getImpl();
			front->DoTick(NULL, NULL);
			if (a->m_prop != NULL)
				cur = a->m_prop->m_cb_get(a).getImpl();

			AnimListenerV4 *next = static_cast<AnimListenerV4 *>(loop->GetFront());
			float time = a->m_timer != NULL ? a->m_timer->m_total * 1000.0f : 0.0f;
			Vector4 slope = mulPerElem(Vector4(time) * 0.08f, cur - start);
			Vector4 target = loop->GetTargetV4()->getImpl();
			Vector4 mid = mulPerElem(slope, Vector4(0.6f)) + start;

			if (next->m_herm_data == NULL)
				next->m_herm_data = new math::v4[2];
			next->m_herm_data[0] = math::v4(slope);
			next->m_herm_data[1] = math::v4(loop->GetTargetV4()->getImpl() - mid);
			next->m_anim_data[1] = math::v4(target);
			next->m_anim_data[0] = math::v4(start);
		} else if (front->m_semantic == AnimListener::SEMANTIC_FLOAT) {
			AnimListenerF1 *a = static_cast<AnimListenerF1 *>(front);
			float start = 0.0f;
			float cur = 0.0f;

			if (a->m_prop != NULL)
				start = a->m_prop->m_cb_get(a);
			front->DoTick(NULL, NULL);
			if (a->m_prop != NULL)
				cur = a->m_prop->m_cb_get(a);

			AnimListenerF1 *next = static_cast<AnimListenerF1 *>(loop->GetFront());
			float time = a->m_timer != NULL ? a->m_timer->m_total * 1000.0f : 0.0f;
			float slope = ((cur - start) * time) * 0.08f;
			float target = *static_cast<AnimListener *>(loop->GetBack())->GetTargetF();
			float mid = (slope * 0.6f) + start;

			if (next->m_herm_data == NULL)
				next->m_herm_data = new float[2];
			next->m_herm_data[0] = slope;
			next->m_herm_data[1] = target - mid;
			next->m_anim_data[0] = start;
			next->m_anim_data[1] = target;
		}
		move->ClearAll();
		loop = m_queue[2];
	}
	if (loop != NULL) {
		TimerListener *front = loop->GetFront();
		Timer *timer = front->m_timer;

		if ((timer == NULL || timer->m_state == Timer::State_Hold ||
		     (timer->m_state == Timer::State_Finished && timer->m_repeat == 0)) &&
		    loop->Size() >= 2) {
			loop->EraseFront(false);
			loop->LoopBackTangents(static_cast<AnimListener *>(front));
			delete front;
		}
	}
	for (int32_t i = 0; i < 3; i++) {
		if (m_queue[i] != NULL) {
			AnimListener *front = static_cast<AnimListener *>(m_queue[i]->GetFront());
			float ticked;

			if (front != NULL)
				front->m_slot_type = i;
			ticked = m_queue[i]->DoTick(data1, data2);
			if (ticked > total)
				total = ticked;
			if (m_queue[i]->Size() == 0) {
				delete m_queue[i];
				m_queue[i] = NULL;
			}
		}
	}
	return total;
}

int32_t AnimQueue::EraseType(int32_t type, bool reserved)
{
	int32_t count = 0;

	for (int32_t i = 0; i < 3; i++) {
		if (m_queue[i] != NULL)
			count += m_queue[i]->HoldType(type);
	}
	return count;
}


class TouchBlockList;

/* the plane over the screen that takes the rear touches meant for the widgets of its list and
   supplies them to the uppermost of those it covers (0x2b0 bytes; names ours) */
class TouchBlocker : public Supplier {
public:
	TouchBlocker(Environment *env, TouchBlockList *list);

	virtual int32_t HandlerProc(Event *e);
	virtual int32_t EventExecDirect(Event *e);

	/* the widgets it blocks for */
	TouchBlockList *m_list;
};

/* the one of the two the list holds first, given back as the other: NULL with neither in it
   (inline in Sony's code; name ours) */
static __attribute__((always_inline)) Widget *GetLater(list<Widget *> &l, Widget *a, Widget *b)
{
	for (list<Widget *>::iterator it = l.begin(); it != l.end(); ++it) {
		if (*it == a)
			return b;
		if (*it == b)
			return a;
	}
	return NULL;
}

/* the widget and its parents up to its root, the root first (inline in Sony's code; name ours) */
static __attribute__((always_inline)) void GetPath(Widget *widget, vector<Widget *> &path)
{
	vector<Widget *> up;

	for (Widget *w = widget; w != NULL; w = w->m_parent)
		up.push_back(w);
	if (path.capacity() < up.size())
		path.reserve(up.size());
	for (int32_t i = up.size() - 1; i >= 0; i--)
		path.push_back(up.at(i));
}

/* of two widgets the touch area covers, the one drawn above the other: the one of the later
   scene, the later child where their paths part, the one the area covers more of where they are
   children of one parent, the deeper one where one is in the other's path (inline in Sony's
   code; name ours) */
static __attribute__((always_inline)) Widget *GetUpper(Widget *a, Widget *b, const math::rect &area)
{
	Scene *scene_b = b->m_scene;
	Scene *scene_a = a->m_scene;

	if (scene_a != scene_b)
		return scene_a == GetLater(scene_a->m_env->m_scenes_onupdate, scene_a, scene_b) ? a : b;

	vector<Widget *> path_a;
	vector<Widget *> path_b;

	GetPath(a, path_a);
	GetPath(b, path_b);

	uint32_t depth = path_b.size();

	if (path_a.size() < depth)
		depth = path_a.size();
	for (uint32_t i = 1; i < depth; i++) {
		Widget *wa = path_a.at(i);
		Widget *wb = path_b.at(i);

		if (wa != wb) {
			if (wa == a && wb == b)
				return ScePafMisc_B2795FE2(area, a->CalcHitRect()).get_area() >
						       ScePafMisc_B2795FE2(area, b->CalcHitRect()).get_area()
					       ? a
					       : b;
			return wa == GetLater(wa->m_parent->m_children, wa, wb) ? a : b;
		}
	}
	return path_a.size() > path_b.size() ? a : b;
}

/* the widgets ScePafWidget_82A61BBC put behind the touch blocker, and the blocker, made with
   the first and destroyed with the last (names ours) */
class TouchBlockList : public vector<Widget *> {
public:
	TouchBlockList() : m_blocker(NULL)
	{
	}

	/* the widget taken out, the blocker destroyed with the last one */
	void Remove(Widget *widget)
	{
		iterator it;

		for (it = begin(); it != end(); ++it) {
			if (*it == widget)
				break;
		}
		if (it != end())
			erase(it);
		if (size() == 0 && m_blocker != NULL) {
			m_blocker->DestroyWidget();
			m_blocker = NULL;
		}
	}

	/* a widget's delete callback: it taken out */
	static void OnDeleteWidget(Widget *widget, void *data)
	{
		static_cast<TouchBlockList *>(data)->Remove(widget);
	}

	/* the uppermost of its widgets that take rear touches, are shown upright and are covered by the
	   blocker by more than 16 square units: NULL for none */
	Widget *FindTarget()
	{
		math::rect area = m_blocker->CalcHitRect();
		Widget *target = NULL;

		for (iterator it = begin(); it != end(); ++it) {
			Widget *w = *it;

			if (!w->IsTouchRearEventEnabled() || (w->m_event_enable & 0x40) == 0 ||
			    !w->IsInputTarget())
				continue;

			Widget *parent;

			for (parent = w->m_parent; parent != NULL; parent = parent->m_parent) {
				if (parent->m_hit_find_mode == 1)
					break;
			}
			if (parent != NULL)
				continue;

			Widget *modal = w->m_env->m_top_modal_widget;

			if (modal != NULL) {
				Widget *p;

				for (p = w; p != NULL; p = p->m_parent) {
					if (p == modal)
						break;
				}
				if (p == NULL)
					continue;
			}
			if (w->IsRotated(true, true, true, 0.15707964f))
				continue;
			if (!(ScePafMisc_B2795FE2(area, w->CalcHitRect()).get_area() <= 16.0f)) {
				if (target == NULL || GetUpper(w, target, area) != target)
					target = w;
			}
		}
		return target;
	}

	TouchBlocker *m_blocker;
};

static TouchBlockList s_touch_block_list;

TouchBlocker::TouchBlocker(Environment *env, TouchBlockList *list)
	: Supplier(env->m_root_widget, NULL), m_list(list)
{
	m_event_enable |= 0x40;
	m_touch_front_event = false;
	EnableTouchRearEvent(true);
	m_hit_detect_b_mode = 0;
	SetAdjust(1, 1, 0);
	SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_82A61BBC(Widget *widget)
{
	if (s_touch_block_list.m_blocker == NULL)
		s_touch_block_list.m_blocker = new TouchBlocker(widget->m_env, &s_touch_block_list);
	s_touch_block_list.push_back(widget);
	widget->AddDeleteCB(TouchBlockList::OnDeleteWidget, &s_touch_block_list);
	return widget->m_list_delete_cb.size();
}

void ScePafWidget_A89BE22F(Widget *widget)
{
	s_touch_block_list.Remove(widget);
}

namespace paf {
namespace ui {

/* a rear touch event gets the target the blocker's list gives first */
int32_t TouchBlocker::HandlerProc(Event *e)
{
	m_target = NULL;
	if (e != NULL && (e->m_type & 0x40) != 0 && ((e->m_detail >> 20) & 0xf) == 1)
		m_target = m_list->FindTarget();
	return Supplier::HandlerProc(e);
}

int32_t TouchBlocker::EventExecDirect(Event *e)
{
	m_target = NULL;
	if (e != NULL && (e->m_type & 0x40) != 0 && ((e->m_detail >> 20) & 0xf) == 1)
		m_target = m_list->FindTarget();
	return Supplier::EventExecDirect(e);
}

}
}
