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

	AnimListener(Timer *t, Widget *w, AnimCB cb_anim, AnimCB cb_fin, void *data);

	virtual ~AnimListener();

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
