/*
 * paf::ui::AnimListener: a timer listener that animates a widget and calls back on each tick and
 * at the end; paf::ui::AnimQueue: the three timer queues a widget keeps for an animation type.
 * Names after the exports and ScePaf-RE (widget/core/animlistener.h); as far as converted C++
 * uses them.
 */
#ifndef PAF_WIDGET_CORE_ANIMLISTENER_H
#define PAF_WIDGET_CORE_ANIMLISTENER_H

#include <stdint.h>
#include <timer/timerlistener.h>
#include <timer/timerqueue.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class AnimPropertyV4;
class AnimPropertyF1;

/* 0x24 bytes */
class AnimListener : public TimerListener {
public:
	enum {
		FLAG_NONE = 0,
		FLAG_NOTIFY_LAYOUT = 1
	};

	enum {
		SEMANTIC_NONE = 0,
		SEMANTIC_V4 = 1,
		SEMANTIC_FLOAT = 2
	};

	AnimListener(Timer *t, Widget *w, AnimCB cb_anim, AnimCB cb_fin, void *data);

	/* the properties of the widget it animates: the setter and getter of each (the second of
	   a rotation turns the shorter way round); a tick's first or later one tells the widget
	   so */
	static void PropPosSet(AnimListener *anim, math::v4 data);
	static math::v4 PropPosGet(AnimListener *anim);
	static void PropColorSet(AnimListener *anim, math::v4 data);
	static math::v4 PropColorGet(AnimListener *anim);
	static void PropRotSet(AnimListener *anim, math::v4 data);
	static math::v4 PropRotGet(AnimListener *anim);
	static void PropQuatSet(AnimListener *anim, math::v4 data);
	static math::v4 PropQuatGet(AnimListener *anim);
	static void PropSizeSet(AnimListener *anim, math::v4 data);
	static math::v4 PropSizeGet(AnimListener *anim);
	static void PropScaleSet(AnimListener *anim, math::v4 data);
	static math::v4 PropScaleGet(AnimListener *anim);
	static void PropAlignSet(AnimListener *anim, math::v4 data);
	static math::v4 PropAlignGet(AnimListener *anim);
	static void PropOriginSet(AnimListener *anim, math::v4 data);
	static math::v4 PropOriginGet(AnimListener *anim);
	static void PropAnchorSet(AnimListener *anim, math::v4 data);
	static math::v4 PropAnchorGet(AnimListener *anim);
	static void PropMetaAlphaSet(AnimListener *anim, float data);
	static float PropMetaAlphaGet(AnimListener *anim);
	static void PropShowAlphaSet(AnimListener *anim, float data);
	static float PropShowAlphaGet(AnimListener *anim);

	static AnimPropertyV4 s_prop_move;
	static AnimPropertyV4 s_prop_color;
	static AnimPropertyV4 s_prop_size;
	static AnimPropertyV4 s_prop_scale;
	static AnimPropertyV4 s_prop_rot;
	static AnimPropertyV4 s_prop_quat;
	static AnimPropertyV4 s_prop_align;
	static AnimPropertyV4 s_prop_origin;
	static AnimPropertyV4 s_prop_anchor;
	static AnimPropertyF1 s_prop_metaalpha;
	static AnimPropertyF1 s_prop_showalpha;
	/* the rotate property again as its own object, for the Euler order option with bit 0x100 of
	   SetRot (name ours) */
	static AnimPropertyV4 s_prop_rot_short;

	/* inline: the vtable and these are emitted where an AnimListener is made (Sony's copies in
	   ButtonBase's object) */
	virtual ~AnimListener()
	{
	}

	virtual void OnTick(void *data1, void *data2)
	{
		if (m_cb_anim != NULL)
			m_cb_anim(m_timer, this, m_widget, m_data, NULL);
	}

	virtual void OnFinish(void *data1, void *data2)
	{
		if (m_cb_finish != NULL)
			m_cb_finish(m_timer, this, m_widget, m_data, NULL);
	}

	/* the value it animates toward and from, by its semantic: none here */
	virtual float *GetTargetF()
	{
		return NULL;
	}

	virtual math::v4 *GetTargetV4()
	{
		return NULL;
	}

	virtual float *GetStartF()
	{
		return NULL;
	}

	virtual math::v4 *GetStartV4()
	{
		return NULL;
	}

	Widget *m_widget;
	AnimCB m_cb_anim;
	AnimCB m_cb_finish;
	uint32_t m_flags;
	uint32_t m_type;
	uint32_t m_semantic : 8;
	uint32_t m_slot_type : 8;
	/* the axes whose value the property setter is given as the property has it (names ours) */
	bool m_keep_x : 1;
	bool m_keep_y : 1;
	bool m_keep_z : 1;
	bool m_keep_w : 1;
};

/* how an AnimListenerF1 reads and writes the float it animates: the animation type, and the
   two callbacks (names after AnimPropertyV4's) */
class AnimPropertyF1 {
public:
	typedef void (*SetFunc)(AnimListener *anim, float data);
	typedef float (*GetFunc)(AnimListener *anim);

	uint32_t m_type;
	SetFunc m_cb_set;
	GetFunc m_cb_get;
};

/* animates a float from m_anim_data[0] to m_anim_data[1] through its property (Sony's name
   from its vtable; members after AnimListenerV4's) */
class AnimListenerF1 : public AnimListener {
public:
	AnimListenerF1(Timer *t, Widget *w, float src, float dst, AnimPropertyF1 *prop,
		       AnimCB cb_fin, void *data)
		: AnimListener(t, w, NULL, cb_fin, data), m_herm_data(NULL), m_prop(prop)
	{
		m_anim_data[0] = src;
		m_anim_data[1] = dst;
		if (m_prop != NULL)
			m_type = m_prop->m_type;
		m_semantic = SEMANTIC_FLOAT;
	}

	virtual ~AnimListenerF1()
	{
		if (m_herm_data != NULL)
			delete[] m_herm_data;
	}

	virtual void OnTick(void *data1, void *data2);

	/* the start and the target of the animation and their tangents, which it goes through
	   as a hermite curve (names ours) */
	void SetHermite(float src, float tangent0, float dst, float tangent1);
	void GetHermite(float &src, float &tangent0, float &dst, float &tangent1);

	virtual float *GetTargetF()
	{
		return &m_anim_data[1];
	}

	virtual float *GetStartF()
	{
		return &m_anim_data[0];
	}

	float m_anim_data[2];
	float *m_herm_data;
	AnimPropertyF1 *m_prop;
};

/* how an AnimListenerV4 reads and writes what it animates: the animation type, and the two
   callbacks */
class AnimPropertyV4 {
public:
	typedef void (*SetFunc)(AnimListener *anim, math::v4 data);
	typedef math::v4 (*GetFunc)(AnimListener *anim);

	uint32_t m_type;
	SetFunc m_cb_set;
	GetFunc m_cb_get;
};

/* animates a v4 from m_anim_data[0] to m_anim_data[1] through its property (0x58 bytes with
   AnimListenerObj's draw object) */
class AnimListenerV4 : public AnimListener {
public:
	AnimListenerV4(Timer *t, Widget *w, const math::v4 &src, const math::v4 &dst,
		       AnimPropertyV4 *prop, AnimCB cb_fin, void *data)
		: AnimListener(t, w, NULL, cb_fin, data), m_herm_data(NULL), m_prop(prop)
	{
		m_anim_data[0] = src;
		m_anim_data[1] = dst;
		if (m_prop != NULL)
			m_type = m_prop->m_type;
		m_semantic = SEMANTIC_V4;
	}

	virtual ~AnimListenerV4()
	{
		delete[] m_herm_data;
	}

	virtual void OnTick(void *data1, void *data2);

	void SetHermite(const math::v4 &src, const math::v4 &tangent0, const math::v4 &dst,
			const math::v4 &tangent1);
	void GetHermite(math::v4 &src, math::v4 &tangent0, math::v4 &dst, math::v4 &tangent1);

	virtual math::v4 *GetTargetV4()
	{
		return &m_anim_data[1];
	}

	virtual math::v4 *GetStartV4()
	{
		return &m_anim_data[0];
	}

	int32_t m_padding;
	math::v4 m_anim_data[2];
	math::v4 *m_herm_data;
	AnimPropertyV4 *m_prop;
};

/* an AnimListenerV4 of one of the widget's draw objects */
class AnimListenerObj : public AnimListenerV4 {
public:
	static void PropObjPosSet(AnimListener *anim, math::v4 data);
	static math::v4 PropObjPosGet(AnimListener *anim);
	static void PropObjColorSet(AnimListener *anim, math::v4 data);
	static math::v4 PropObjColorGet(AnimListener *anim);
	static void PropObjSizeSet(AnimListener *anim, math::v4 data);
	static math::v4 PropObjSizeGet(AnimListener *anim);
	static void PropObjRotSet(AnimListener *anim, math::v4 data);
	static math::v4 PropObjRotGet(AnimListener *anim);
	/* the plane object's texture region, and the panel object's revolution (names ours) */
	static void PropObjTexRegionSet(AnimListener *anim, math::v4 data);
	static math::v4 PropObjTexRegionGet(AnimListener *anim);
	static void PropObjRevolutionSet(AnimListener *anim, math::v4 data);
	static math::v4 PropObjRevolutionGet(AnimListener *anim);

	/* the draw object's color, position, size and rotation */
	static AnimPropertyV4 s_prop_obj_move;
	static AnimPropertyV4 s_prop_obj_color;
	static AnimPropertyV4 s_prop_obj_size;
	static AnimPropertyV4 s_prop_obj_rot;
	static AnimPropertyV4 s_prop_obj_texregion;

	AnimListenerObj(Timer *t, Widget *w, graph::DrawObj *obj, const math::v4 &src,
			const math::v4 &dst, AnimPropertyV4 *prop, AnimCB cb_fin, void *data)
		: AnimListenerV4(t, w, src, dst, prop, cb_fin, data)
	{
		m_obj = obj;
	}

	virtual ~AnimListenerObj()
	{
	}

	graph::DrawObj *m_obj;
};

/* animates the texture region of a plane object over the frames of a texture of a number of
   columns and rows (from Sony's vtable and the export names; members' names ours) */
class TexAnimListenerObj : public AnimListenerObj {
public:
	TexAnimListenerObj(Timer *t, Widget *w, graph::DrawObj *obj, int32_t columns, int32_t rows,
			   int32_t first, int32_t last, AnimCB cb_fin, void *data);

	virtual void OnTick(void *data1, void *data2);

	/* the region of the frame the timer's value is at (0 to 1) */
	void TexAnim(float rate);

	int32_t m_columns;
	int32_t m_rows;
	int32_t m_first;
	int32_t m_last;
	/* the frames to go through, the last frame counted */
	int32_t m_count;
	float m_column_ratio;
	float m_row_ratio;
};

class AnimQueueBase : public TimerQueue {
public:
	/* inline: its vtable and these are emitted where one is made */
	virtual ~AnimQueueBase()
	{
	}

	int32_t PushFront(TimerListener *t)
	{
		m_list.push_front(t);
		m_total += t->GetTotalTime();
		return 0;
	}

	/* the listeners of the type held, and how many (type is a listener's m_type) */
	int32_t EraseType(int32_t type, bool reserved);
	int32_t HoldType(int32_t type)
	{
		int32_t count = 0;

		for (list<TimerListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
			AnimListener *listener = static_cast<AnimListener *>(*it);

			if (listener->m_type == (uint32_t)type) {
				if (listener->m_timer != NULL)
					listener->m_timer->m_state = Timer::State_Hold;
				count++;
			}
		}
		return count;
	}

	/* the timers of all its listeners held, where they are (Widget::KillAnim; name ours) */
	void HoldAll()
	{
		for (list<TimerListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
			TimerListener *listener = *it;

			if (listener->m_timer != NULL)
				listener->m_timer->m_state = Timer::State_Hold;
		}
	}
	/* the finished listeners put at the back again, reset, their tangents made for the loop
	   where hermite */
	void SetFinishedLoopBack(bool hermite);
	/* the tangents of the listener now first made from the one that was before it (0x81138f42;
	   name ours) */
	void LoopBackTangents(AnimListener *prev);
	/* the target of its last listener */
	math::v4 *GetTargetV4()
	{
		AnimListener *l = (AnimListener *)GetBack();

		return l != NULL ? l->GetTargetV4() : NULL;
	}
};

class AnimQueue {
public:
	virtual ~AnimQueue();

	int32_t PushBack(TimerListener *listener, uint32_t slot);
	int32_t PushFront(TimerListener *listener, uint32_t slot);
	int32_t EraseFront(bool auto_delete, uint32_t slot);
	float DoTick(void *data1, void *data2);
	int32_t EraseType(int32_t type, bool reserved);

	/* no queue with a listener still running */
	bool IsFinished()
	{
		for (int i = 0; i < 3; i++) {
			if (m_queue[i] != NULL && !m_queue[i]->IsFinished())
				return false;
		}
		return true;
	}

	AnimQueueBase *m_queue[3];
};

}
}

#endif
