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
	static void PropObjColorSet(AnimListener *anim, math::v4 data);
	static math::v4 PropObjColorGet(AnimListener *anim);

	/* the draw object's color, position, size and rotation */
	static AnimPropertyV4 s_prop_obj_move;
	static AnimPropertyV4 s_prop_obj_color;
	static AnimPropertyV4 s_prop_obj_size;
	static AnimPropertyV4 s_prop_obj_rot;

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

class AnimQueueBase : public TimerQueue {
public:
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
