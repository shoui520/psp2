/*
 * paf::TimerListener: a Timer ticked by a listener list or queue, with a
 * callback on each tick and one when the timer finishes.
 */
#ifndef PAF_TIMER_TIMERLISTENER_H
#define PAF_TIMER_TIMERLISTENER_H

#include <timer/timer.h>

namespace paf {

class TimerListener {
public:
	TimerListener(Timer *t, void *data)
	{
		m_timer = t;
		m_data = data;
	}

	virtual ~TimerListener();

	virtual void OnTick(void *data1, void *data2)
	{
	}

	virtual void OnFinish(void *data1, void *data2)
	{
	}

	float DoTick(void *data1, void *data2);
	void ResetTimer();

	bool IsFinished()
	{
		if (m_timer == NULL)
			return true;
		return m_timer->IsAllFinished();
	}

	float GetTotalTime() const
	{
		if (m_timer == NULL)
			return 0.0f;
		return m_timer->GetTotalTime();
	}

	/* the time its timer has run (name ours) */
	float GetCurrTime() const
	{
		if (m_timer == NULL)
			return 0.0f;
		return m_timer->GetCurrTime();
	}

	Timer *m_timer;
	void *m_data;
};

}

#endif /* PAF_TIMER_TIMERLISTENER_H */
