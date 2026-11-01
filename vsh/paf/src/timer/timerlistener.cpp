/*
 * paf::TimerListener: ticks its timer (as a Timer, Timer2D or Timer3D) and
 * calls OnTick, then OnFinish once the timer is done.
 */
#include <timer/timerlistener.h>

namespace paf {

TimerListener::~TimerListener()
{
	delete m_timer;
	m_timer = NULL;
}

float TimerListener::DoTick(void *data1, void *data2)
{
	float ret;

	if (m_timer == NULL)
		return 0.0f;
	if (m_timer->m_is_3d)
		ret = static_cast<Timer3D *>(m_timer)->DoTick();
	else if (m_timer->m_is_2d)
		ret = static_cast<Timer2D *>(m_timer)->DoTick();
	else
		ret = m_timer->DoTick();
	OnTick(data1, data2);
	if (m_timer->IsAllFinished())
		OnFinish(data1, data2);
	return ret;
}

void TimerListener::ResetTimer()
{
	if (m_timer == NULL)
		return;
	if (m_timer->m_is_3d)
		static_cast<Timer3D *>(m_timer)->ResetTimer();
	else if (m_timer->m_is_2d)
		static_cast<Timer2D *>(m_timer)->ResetTimer();
	else
		m_timer->ResetTimer();
}

}
