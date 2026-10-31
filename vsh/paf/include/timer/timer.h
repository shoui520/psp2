/*
 * paf::Timer: a value running from 0 to 1 over a time, through an easing
 * function, ticked once per frame.
 *
 * PROVISIONAL: the members C++ callers use, until timer.c (0x81047a28..) is
 * converted to C++; its layout is the C Timer's (src/timer/timer.h).
 */
#ifndef PAF_TIMER_TIMER_H_CXX
#define PAF_TIMER_TIMER_H_CXX

#include <stddef.h>

namespace paf {

class Timer {
public:
	enum State {
		State_Standby = 0,
		State_Active = 1,
		State_Finished = 2,
		State_Hold = 3,
		State_Disabled = 4
	};

	Timer()
	{
		m_is_2d = 0;
		m_is_3d = 0;
		m_param_x_num = 0;
		m_param_x = NULL;
		m_mapped_x = 0.0f;
	}

	virtual ~Timer();

	static void InitTickUnit(float tick_unit);
	static void SetCurrTickInterval(float tick_interval);

	void InitTimer(float total, int func, int repeat, int repeat_count, const float *param);
	void ResetTimer();
	void SetFuncX(int func, const float *param);
	float DoTick();

	float m_total;
	float m_total_inv;
	float m_curr;
	int m_repeat_count : 8;
	unsigned int m_repeat : 4;
	unsigned int m_state : 4;
	bool m_first_tick : 1;
	bool m_nonpositive_total : 1;
	bool m_is_2d : 1;
	bool m_is_3d : 1;
	unsigned int m_func_x : 8;
	unsigned int m_param_x_num : 4;
	float *m_param_x;
	float m_mapped_x;
};

}

#endif /* PAF_TIMER_TIMER_H_CXX */
