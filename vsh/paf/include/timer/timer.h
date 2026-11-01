/*
 * paf::Timer: a value running from 0 to 1 over a time, through an easing
 * function, ticked once per frame. Timer2D and Timer3D run a second and a
 * third value over the same time, each through its own function.
 *
 * The frame tick (s_tick_unit seconds, scaled by s_tick_interval) is measured
 * from the last frames' times; see timer.cpp.
 */
#ifndef PAF_TIMER_TIMER_H_CXX
#define PAF_TIMER_TIMER_H_CXX

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <math/math.h>

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

	enum Repeat {
		REPEAT_NONE = 0,
		REPEAT_LOOP = 1,
		REPEAT_ROUND = 4,
		REPEAT_RBACK = 12
	};

	enum Func {
		FUNC_LINEAR,
		FUNC_SLOWDOWN,
		FUNC_FASTUP,
		FUNC_ANGULAR,
		FUNC_EXPONENT,
		FUNC_BEZIER_3D,
		FUNC_SWING,
		FUNC_QUAD_IN,
		FUNC_CUBIC_IN,
		FUNC_QUART_IN,
		FUNC_QUINT_IN,
		FUNC_SINE_IN,
		FUNC_EXPO_IN,
		FUNC_EXPO_IN_13,
		FUNC_EXPO_IN_14,
		FUNC_EXPO_IN_15,
		FUNC_EXPO_IN_16,
		FUNC_CIRQ_IN,
		FUNC_BACK_IN,
		FUNC_ELASTIC_IN,
		FUNC_BOUNCE_IN,
		FUNC_QUAD_OUT,
		FUNC_CUBIC_OUT,
		FUNC_QUART_OUT,
		FUNC_QUINT_OUT,
		FUNC_SINE_OUT,
		FUNC_EXPO_OUT,
		FUNC_EXPO_OUT_27,
		FUNC_EXPO_OUT_28,
		FUNC_EXPO_OUT_29,
		FUNC_EXPO_OUT_30,
		FUNC_CIRQ_OUT,
		FUNC_BACK_OUT,
		FUNC_ELASTIC_OUT,
		FUNC_BOUNCE_OUT,
		FUNC_QUAD_INOUT,
		FUNC_CUBIC_INOUT,
		FUNC_QUART_INOUT,
		FUNC_QUINT_INOUT,
		FUNC_SINE_INOUT,
		FUNC_EXPO_INOUT,
		FUNC_EXPO_INOUT_41,
		FUNC_EXPO_INOUT_42,
		FUNC_EXPO_INOUT_43,
		FUNC_EXPO_INOUT_44,
		FUNC_CIRQ_INOUT,
		FUNC_BACK_INOUT,
		FUNC_ELASTIC_INOUT,
		FUNC_BOUNCE_INOUT,
		FUNC_POLY_IN,
		FUNC_POLY_OUT,
		FUNC_POLY_INOUT,
		FUNC_POLY_IN_52,
		FUNC_POLY_OUT_53,
		FUNC_POLY_INOUT_54,
		FUNC_CUSTOM_EXPO_IN,
		FUNC_CUSTOM_EXPO_OUT,
		FUNC_CUSTOM_EXPO_INOUT,
		FUNC_CUSTOM_EXPO_OUTIN,
		FUNC_CUSTOM_PARABOLIC_IN,
		FUNC_CUSTOM_PARABOLIC_OUT,
		FUNC_CUSTOM_PARABOLIC_INOUT,
		FUNC_CUSTOM_PARABOLIC_OUTIN,
		FUNC_CUSTOM_BOUNCE_IN,
		FUNC_CUSTOM_BOUNCE_OUT,
		FUNC_CUSTOM_BOUNCE_INOUT,
		FUNC_CUSTOM_BOUNCE_OUTIN,
		FUNC_CUSTOM_ELASTIC_IN,
		FUNC_CUSTOM_ELASTIC_OUT,
		FUNC_CUSTOM_ELASTIC_INOUT,
		FUNC_CUSTOM_ELASTIC_OUTIN,
		FUNC_CUSTOM_DAMPED_IN,
		FUNC_CUSTOM_DAMPED_OUT,
		FUNC_CUSTOM_DAMPED_INOUT,
		FUNC_CUSTOM_DAMPED_OUTIN,
		FUNC_CUSTOM_BACK_IN,
		FUNC_CUSTOM_BACK_OUT,
		FUNC_CUSTOM_BACK_INOUT,
		FUNC_CUSTOM_BACK_OUTIN,
		FUNC_CUSTOM
	};

	Timer()
	{
		m_is_2d = false;
		m_is_3d = false;
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
	void ForceFinish();
	float DoTick();
	math::v4 GetValueV4();
	float DoFunc(float rate, int func, float pre, float *param) const;

	bool IsFinished() const
	{
		return m_state == State_Finished;
	}

	/* held, or finished with no repeat left */
	bool IsAllFinished() const
	{
		if (m_state == State_Hold)
			return true;
		return IsFinished() && m_repeat == REPEAT_NONE;
	}

	float GetTotalTime() const
	{
		return m_total * 1000.0f;
	}

	static float Cbrt(float input);

	static float DoFuncLinear(float rate);
	static float DoFuncSlower(float rate);
	static float DoFuncFaster(float rate);
	static float DoFuncAngular(float rate);
	static float DoFuncSwing(float rate);
	static float DoFuncExponent(float rate, float total, float initval, const float *param);
	static float DoFuncExponent_i(float curr, float total, const float *param);
	static float DoFuncBezier3d(float rate, const float *param);
	static float DoFuncQuadIn(float rate);
	static float DoFuncQuadOut(float rate);
	static float DoFuncQuadInOut(float rate);
	static float DoFuncCubicIn(float rate);
	static float DoFuncCubicOut(float rate);
	static float DoFuncCubicInOut(float rate);
	static float DoFuncQuartIn(float rate);
	static float DoFuncQuartOut(float rate);
	static float DoFuncQuartInOut(float rate);
	static float DoFuncQuintIn(float rate);
	static float DoFuncQuintOut(float rate);
	static float DoFuncQuintInOut(float rate);
	static float DoFuncSineIn(float rate);
	static float DoFuncSineOut(float rate);
	static float DoFuncSineInOut(float rate);
	static float DoFuncExpoIn(float rate);
	static float DoFuncExpoOut(float rate);
	static float DoFuncExpoInOut(float rate);
	static float DoFuncCirqIn(float rate);
	static float DoFuncCirqOut(float rate);
	static float DoFuncCirqInOut(float rate);
	static float DoFuncBackIn(float rate);
	static float DoFuncBackOut(float rate);
	static float DoFuncBackInOut(float rate);
	static float DoFuncElasticIn(float rate);
	static float DoFuncElasticOut(float rate);
	static float DoFuncElasticInOut(float rate);
	static float DoFuncBounceIn(float rate);
	static float DoFuncBounceOut(float rate);
	static float DoFuncBounceInOut(float rate);
	static float DoFuncCustomExpoIn(float rate, const float *param);
	static float DoFuncCustomExpoOut(float rate, const float *param);
	static float DoFuncCustomExpoInOut(float rate, const float *param);
	static float DoFuncCustomExpoOutIn(float rate, const float *param);
	static float DoFuncCustomParabolicIn(float rate, const float *param);
	static float DoFuncCustomParabolicOut(float rate, const float *param);
	static float DoFuncCustomParabolicInOut(float rate, const float *param);
	static float DoFuncCustomParabolicOutIn(float rate, const float *param);
	static float DoFuncCustomElasticIn(float rate, const float *param);
	static float DoFuncCustomElasticOut(float rate, const float *param);
	static float DoFuncCustomElasticInOut(float rate, const float *param);
	static float DoFuncCustomElasticOutIn(float rate, const float *param);

	static float s_tick_unit;
	static float s_tick_inv;
	static float s_tick_interval;

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

protected:
	int init_func_param(int func, const float *param, float **ret_param, int *ret_param_num);

private:
	int check_bezier_param(float *param);
	int check_custom_param(float *param);
};

class Timer2D : public Timer {
public:
	virtual ~Timer2D();

	float DoTick();
	void SetFuncY(int func, const float *param);

	void ResetTimer()
	{
		Timer::ResetTimer();
		m_mapped_y = m_mapped_x;
	}

	unsigned int m_func_y : 8;
	unsigned int m_param_y_num : 4;
	float *m_param_y;
	float m_mapped_y;
};

class Timer3D : public Timer2D {
public:
	virtual ~Timer3D();

	float DoTick();
	void SetFuncZ(int func, const float *param);

	void ResetTimer()
	{
		Timer2D::ResetTimer();
		m_mapped_z = m_mapped_x;
	}

	unsigned int m_func_z : 8;
	unsigned int m_param_z_num : 4;
	float *m_param_z;
	float m_mapped_z;
};

}

/*
 * The frame timing, which sets the tick interval from the last frames' times;
 * no Sony names found for these NIDs.
 */
extern "C" {
void ScePafMisc_A08B8D8A(SceUInt32 time);
void ScePafMisc_F8D63B8E(float ratio);
void ScePafMisc_A04B01AB(void);
int ScePafMisc_EF1C440B(void);
int ScePafMisc_4357D271(void);
}

#endif /* PAF_TIMER_TIMER_H_CXX */
