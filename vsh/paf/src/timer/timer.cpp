/*
 * paf::Timer: a value running from 0 to 1 over a time, through an easing
 * function, ticked once per frame by s_tick_unit times s_tick_interval.
 *
 * The frame timing keeps the start times of the last six frames, newest first,
 * and how long the last six frames took to two points of the frame; the tick
 * interval is the last frames' average length in ticks, snapped to whole
 * ticks.
 */
#include <timer/timer.h>
#include <std/new>
#include <std/string.h>
#include <kernel/processmgr.h>
#include "../std/math.h"

extern "C" int sceDisplayWaitVblankStart(void);

using namespace paf;

/* The exported frame timing; no Sony names found for these NIDs. */
#define s_frame_wait_time	ScePafMisc_37595ACC
#define s_frame_wait_ratio	ScePafMisc_EA9E40E6
#define s_frame_start_time	ScePafMisc_256F9060
#define s_frame_end_time	ScePafMisc_1D135244
#define s_frame_end_average	ScePafMisc_7DB172ED
#define s_frame_mark_time	ScePafMisc_D20A15AF
#define s_frame_mark_average	ScePafMisc_8A118BBC

enum {
	/* the frames the frame timing averages */
	FRAME_HISTORY_NUM = 6
};

/* sceKernelGetProcessTimeWide's microseconds to 59.94 Hz frames' (1000/1001) */
#define PROCESS_TIME_SCALE	0.9990009990009991

#define PI	3.1415927f

float Timer::s_tick_unit = 1.0f / 60.0f;
float Timer::s_tick_inv = 60.0f;
float Timer::s_tick_interval = 1.0f;

extern "C" {
SceUInt64 s_frame_wait_time = 8000;
float s_frame_wait_ratio = -1.0f;
SceUInt64 s_frame_start_time[FRAME_HISTORY_NUM];
SceUInt64 s_frame_end_time[FRAME_HISTORY_NUM];
SceUInt64 s_frame_end_average;
SceUInt64 s_frame_mark_time[FRAME_HISTORY_NUM];
SceUInt64 s_frame_mark_average;
}

/*
 * The exponential curves are tables of 33 points, read linearly between them;
 * each is the "in" curve of its Func, FUNC_EXPO_IN to FUNC_EXPO_IN_16.
 */
static const float s_expo_in_table_16[33] = {
	0.0f, 0.003378f, 0.007232f, 0.011618f,
	0.016588f, 0.022199f, 0.028518f, 0.035616f,
	0.043573f, 0.052479f, 0.062434f, 0.073549f,
	0.085948f, 0.099769f, 0.115166f, 0.132310f,
	0.151394f, 0.172629f, 0.196254f, 0.222532f,
	0.251759f, 0.284263f, 0.320411f, 0.360610f,
	0.405316f, 0.455034f, 0.510330f, 0.571833f,
	0.640243f, 0.716343f, 0.801004f, 0.895196f,
	1.0f
};

static const float s_expo_in_table_15[33] = {
	0.0f, 0.002188f, 0.004704f, 0.007592f,
	0.010902f, 0.014686f, 0.019007f, 0.023935f,
	0.029550f, 0.035943f, 0.043218f, 0.051492f,
	0.060902f, 0.071598f, 0.083757f, 0.097577f,
	0.113284f, 0.131136f, 0.151427f, 0.174489f,
	0.200705f, 0.230507f, 0.264389f, 0.302913f,
	0.346719f, 0.396536f, 0.453193f, 0.517637f,
	0.590942f, 0.674336f, 0.769213f, 0.877165f,
	1.0f
};

static const float s_expo_in_table_14[33] = {
	0.0f, 0.001299f, 0.002810f, 0.004580f,
	0.006650f, 0.009064f, 0.011878f, 0.015154f,
	0.018965f, 0.023397f, 0.028548f, 0.034535f,
	0.041490f, 0.049571f, 0.058960f, 0.069866f,
	0.082538f, 0.097261f, 0.114368f, 0.134248f,
	0.157351f, 0.184204f, 0.215418f, 0.251703f,
	0.293889f, 0.342938f, 0.399971f, 0.466294f,
	0.543425f, 0.633131f, 0.737468f, 0.858829f,
	1.0f
};

static const float s_expo_in_table_13[33] = {
	0.0f, 0.000749f, 0.001639f, 0.002703f,
	0.003973f, 0.005485f, 0.007285f, 0.009425f,
	0.011968f, 0.014989f, 0.018577f, 0.022838f,
	0.027898f, 0.033906f, 0.041040f, 0.049512f,
	0.059575f, 0.071526f, 0.085722f, 0.102586f,
	0.122623f, 0.146430f, 0.174720f, 0.208338f,
	0.248293f, 0.295782f, 0.352227f, 0.419324f,
	0.499085f, 0.593906f, 0.706634f, 0.840656f,
	1.0f
};

static const float s_expo_in_table[33] = {
	0.0f, 0.000243f, 0.000539f, 0.000908f,
	0.001367f, 0.001938f, 0.002646f, 0.003524f,
	0.004614f, 0.005967f, 0.007644f, 0.009724f,
	0.012305f, 0.015506f, 0.019478f, 0.024407f,
	0.030522f, 0.038111f, 0.047531f, 0.059222f,
	0.073734f, 0.091749f, 0.114113f, 0.141879f,
	0.176352f, 0.219153f, 0.272297f, 0.338284f,
	0.420220f, 0.521963f, 0.648302f, 0.805185f,
	1.0f
};

/* Each custom function's parameters when the caller gives none. */
static const float s_default_param_custom_expo[4] = { 0.2f, 1.5f };
static const float s_default_param_custom_parabolic[4] = { 0.15f, -0.08f };
static const float s_default_param_custom_bounce[4] = { 0.7f, 5.0f };
static const float s_default_param_custom_elastic[4] = { 6.0f, 0.4f, 0.5f, 0.1f };
static const float s_default_param_custom_damped[4] = { 0.8f, 12.0f };
static const float s_default_param_custom_back[4] = { 0.1f };
static const float s_default_param_poly[4] = { 0.2f, 0.3f, 0.5f };

void Timer::InitTickUnit(float tick_unit)
{
	s_tick_unit = tick_unit * 0.001f;
	s_tick_interval = 1.0f;
	s_tick_inv = 1.0f / (tick_unit * 0.001f);
}

void Timer::SetCurrTickInterval(float tick_interval)
{
	s_tick_interval = tick_interval;
}

Timer::~Timer()
{
	if (m_param_x != NULL)
		delete[] m_param_x;
}

int Timer::check_custom_param(float *param)
{
	float ratio = param[0];
	float count = param[1];
	float duration = 0.0f;

	if (ratio >= 1.0f) {
		ratio = 1.0f;
		param[0] = 1.0f;
	}
	if (ratio < 0.0f) {
		param[0] = 0.01f;
		ratio = 0.01f;
	}
	if (count < 1.0f) {
		ratio = 1.0f;
		param[0] = 1.0f;
	}

	int num = (int)count;

	if (ratio < 1.0f)
		duration = (1.0f - ratio) / (1.0f - sce_paf_powf(ratio, (float)num));
	param[2] = duration;
	return 0;
}

/* A Bezier curve whose x does not rise monotonically falls back to linear. */
int Timer::check_bezier_param(float *param)
{
	float x1 = param[0] * 3.0f;
	float x2 = param[2] * 3.0f;
	float a = (x1 - x2) + 1.0f;
	float b = (param[0] * -6.0f + x2) / a;
	float c = x1 / a;
	float d = 0.0f / a;
	int ret = FUNC_BEZIER_3D;
	float p = (c - (b * b) / 3.0f) / 3.0f;
	float q = sce_paf_powf(((d + (sce_paf_powf(b, 3.0f) * 2.0f) / 27.0f) - (b * c) / 3.0f) * 0.5f,
			       2.0f);

	if (q + sce_paf_powf(p, 3.0f) < 0.0f) {
		sce_paf_memset(param, 0, sizeof(float) * 4);
		ret = FUNC_LINEAR;
	}
	return ret;
}

int Timer::init_func_param(int func, const float *param, float **ret_param, int *ret_param_num)
{
	float *pret_param = *ret_param;
	int pcount;

	if (pret_param != NULL) {
		delete[] pret_param;
		pret_param = NULL;
	}

	pcount = 0;
	if (func == FUNC_BEZIER_3D) {
		pcount = 4;
		pret_param = new float[4];
	} else if (func >= FUNC_CUSTOM_EXPO_IN && func <= FUNC_CUSTOM_PARABOLIC_OUTIN) {
		pcount = 2;
		pret_param = new float[2];
	} else if (func >= FUNC_CUSTOM_BOUNCE_IN && func <= FUNC_CUSTOM_BOUNCE_OUTIN) {
		pcount = 2;
		pret_param = new float[4];
	} else if (func >= FUNC_CUSTOM_DAMPED_IN && func <= FUNC_CUSTOM_DAMPED_OUTIN) {
		/* the third holds what check_custom_param works out */
		pcount = 2;
		pret_param = new float[4];
	} else if (func >= FUNC_CUSTOM_BACK_IN && func <= FUNC_CUSTOM_BACK_OUTIN) {
		pcount = 1;
		pret_param = new float[1];
	} else if (func >= FUNC_CUSTOM_ELASTIC_IN && func <= FUNC_CUSTOM_ELASTIC_OUTIN) {
		pcount = 4;
		pret_param = new float[4];
	} else if (func >= FUNC_POLY_IN && func <= FUNC_POLY_INOUT_54) {
		pcount = 4;
		pret_param = new float[4];
	}

	if (param == NULL) {
		if (func == FUNC_CUSTOM_EXPO_IN || func == FUNC_CUSTOM_EXPO_OUT ||
		    func == FUNC_CUSTOM_EXPO_INOUT || func == FUNC_CUSTOM_EXPO_OUTIN)
			param = s_default_param_custom_expo;
		else if (func == FUNC_CUSTOM_PARABOLIC_IN || func == FUNC_CUSTOM_PARABOLIC_OUT ||
			 func == FUNC_CUSTOM_PARABOLIC_INOUT || func == FUNC_CUSTOM_PARABOLIC_OUTIN)
			param = s_default_param_custom_parabolic;
		else if (func >= FUNC_CUSTOM_BOUNCE_IN && func <= FUNC_CUSTOM_BOUNCE_OUTIN)
			param = s_default_param_custom_bounce;
		else if (func >= FUNC_CUSTOM_ELASTIC_IN && func <= FUNC_CUSTOM_ELASTIC_OUTIN)
			param = s_default_param_custom_elastic;
		else if (func >= FUNC_CUSTOM_DAMPED_IN && func <= FUNC_CUSTOM_DAMPED_OUTIN)
			param = s_default_param_custom_damped;
		else if (func >= FUNC_CUSTOM_BACK_IN && func <= FUNC_CUSTOM_BACK_OUTIN)
			param = s_default_param_custom_back;
		else if (func >= FUNC_POLY_IN && func <= FUNC_POLY_INOUT_54)
			param = s_default_param_poly;
	}
	if (param != NULL)
		sce_paf_memcpy(pret_param, param, sizeof(float) * pcount);

	int ret = func;

	if (func == FUNC_BEZIER_3D) {
		ret = check_bezier_param(pret_param);
	} else if (func >= FUNC_CUSTOM_ELASTIC_IN && func <= FUNC_CUSTOM_ELASTIC_OUTIN) {
		/* at least one swing, and the three amplitudes adding up to 1 */
		if (pret_param[0] < 1.0f)
			pret_param[0] = 1.0f;
		if (__builtin_fabsf((pret_param[1] + pret_param[2]) + pret_param[3] - 1.0f) > 0.01f) {
			pret_param[1] = s_default_param_custom_elastic[1];
			pret_param[2] = s_default_param_custom_elastic[2];
			pret_param[3] = s_default_param_custom_elastic[3];
		}
	}
	if (func >= FUNC_CUSTOM_DAMPED_IN && func <= FUNC_CUSTOM_DAMPED_OUTIN)
		check_custom_param(pret_param);
	*ret_param = pret_param;
	*ret_param_num = pcount;
	return ret;
}

void Timer::SetFuncX(int func, const float *param)
{
	int param_num;

	m_func_x = init_func_param(func, param, &m_param_x, &param_num);
	m_param_x_num = param_num;
}

void ScePafMisc_A08B8D8A(SceUInt32 time)
{
	s_frame_wait_time = time;
}

void ScePafMisc_F8D63B8E(float ratio)
{
	s_frame_wait_ratio = ratio;
}

/* The frame times as if the last frames had each taken one tick. */
void ScePafMisc_A04B01AB(void)
{
	SceUInt64 tick = (SceUInt64)(Timer::s_tick_unit * 1000.0f * 1000.0f);

	s_frame_start_time[0] = (SceUInt64)((double)sceKernelGetProcessTimeWide() /
					    PROCESS_TIME_SCALE);
	for (int i = 1; i < FRAME_HISTORY_NUM; i++)
		s_frame_start_time[i] = s_frame_start_time[0] - i * tick;
}

/*
 * The end of a frame: its length is recorded, the display waited for when the
 * frame was shorter than the ratio asks, a new frame started and the tick
 * interval set from the last frames' lengths.
 */
int ScePafMisc_EF1C440B(void)
{
	SceUInt64 sum = 0;

	for (int i = FRAME_HISTORY_NUM - 2; i >= 0; i--) {
		SceUInt64 time = s_frame_end_time[i];

		s_frame_end_time[i + 1] = time;
		sum += time;
	}
	s_frame_end_time[0] = (SceUInt64)((double)sceKernelGetProcessTimeWide() / PROCESS_TIME_SCALE -
					  (double)s_frame_start_time[0]);
	s_frame_end_average = (sum + s_frame_end_time[0]) / FRAME_HISTORY_NUM;

	if (s_frame_wait_ratio > 0.0f) {
		if (s_frame_wait_ratio * (float)s_frame_end_average < (float)s_frame_mark_average)
			sceDisplayWaitVblankStart();
	}

	float total = 0.0f;

	for (int i = FRAME_HISTORY_NUM - 1; i > 0; i--) {
		SceUInt64 time = s_frame_start_time[i - 1];
		float length = (float)(time - s_frame_start_time[i]) * 0.000001f;

		if (length > 0.1f)
			length = 0.1f;
		s_frame_start_time[i] = time;
		total += length;
	}
	s_frame_start_time[0] = (SceUInt64)((double)sceKernelGetProcessTimeWide() /
					    PROCESS_TIME_SCALE);

	float length = (float)(s_frame_start_time[0] - s_frame_start_time[1]) * 0.000001f;

	if (length > 0.1f)
		length = 0.1f;

	float ticks = Timer::s_tick_inv * (1.0f / FRAME_HISTORY_NUM) * (total + length);
	float whole = (float)(int)ticks;
	float fraction = ticks - whole;

	if (fraction < 0.1f && whole > 0.9f)
		fraction = 0.0f;
	else if (fraction > 0.9f)
		fraction = 1.0f;
	Timer::s_tick_interval = whole + fraction;
	return 0;
}

/* A point within the frame: how long into the frame it came. */
int ScePafMisc_4357D271(void)
{
	SceUInt64 sum = 0;

	for (int i = FRAME_HISTORY_NUM - 2; i >= 0; i--) {
		SceUInt64 time = s_frame_mark_time[i];

		s_frame_mark_time[i + 1] = time;
		sum += time;
	}
	s_frame_mark_time[0] = (SceUInt64)((double)sceKernelGetProcessTimeWide() / PROCESS_TIME_SCALE -
					   (double)s_frame_start_time[0]);
	s_frame_mark_average = (sum + s_frame_mark_time[0]) / FRAME_HISTORY_NUM;
	return 0;
}

void Timer::ResetTimer()
{
	m_curr = 0.0f;
	m_mapped_x = 0.0f;
	m_state = State_Standby;
	m_first_tick = false;
}

void Timer::InitTimer(float total, int func, int repeat, int repeat_count, const float *param)
{
	ResetTimer();
	if (total <= 0.0f)
		m_nonpositive_total = true;
	else
		m_nonpositive_total = false;
	m_total = total * 0.001f;

	int ticks = (int)(m_total * s_tick_inv);

	if (ticks <= 0)
		ticks = 1;

	float ctotal = ((float)ticks * s_tick_unit) * 0.999999f;
	int param_num;

	m_repeat = repeat;
	m_repeat_count = repeat_count;
	m_total = ctotal;
	m_total_inv = 1.0f / ctotal;
	m_func_x = init_func_param(func, param, &m_param_x, &param_num);
	m_param_x_num = param_num;
}

math::v4 Timer::GetValueV4()
{
	math::v4 value(m_mapped_x, m_mapped_x, m_mapped_x, m_mapped_x);

	if (m_is_2d || m_is_3d)
		value.set_y(static_cast<Timer2D *>(this)->m_mapped_y);
	if (m_is_3d)
		value.set_z(static_cast<Timer3D *>(this)->m_mapped_z);
	return value;
}

void Timer::ForceFinish()
{
	if (m_state == State_Hold)
		return;
	m_state = State_Finished;
	if (m_repeat == REPEAT_RBACK) {
		m_curr = 0.0f;
		m_mapped_x = 0.0f;
	} else {
		m_curr = m_total;
		m_mapped_x = 1.0f;
	}
}

float Timer::DoFuncLinear(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate;
}

/*
 * Each Func has an "in" curve, rising slowly at first; its "out" curve is the
 * "in" curve turned around (1 - in(1 - rate)), "inout" runs the "in" curve
 * over the first half and the "out" curve over the second, and "outin" the
 * other way round. Every curve is 0 at or below 0 and 1 at or above 1.
 */

/* The custom "back" curve: overshooting backwards, as far as param[0] asks. */
static float DoFuncCustomBackIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;

	float strength = (param[0] * 10.0f - 1.0f) * 2.0f;
	float overshoot = strength * (0.47f - strength * 0.01f) + 2.70158f;

	return rate * rate * (rate * overshoot - (overshoot - 1.0f));
}

float DoFuncCustomBackOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return (1.0f - DoFuncCustomBackIn(1.0f - rate * 2.0f, param)) * 0.5f;
	return DoFuncCustomBackIn(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
}

float DoFuncCustomBackInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncCustomBackIn(rate * 2.0f, param) * 0.5f;
	return (1.0f - DoFuncCustomBackIn(2.0f - rate * 2.0f, param)) * 0.5f + 0.5f;
}

float DoFuncCustomBackOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncCustomBackIn(1.0f - rate, param);
}

/*
 * The custom damped curve: param[1] half swings around 1, each param[0] the
 * size of the last; param[2] is the first swing's length (check_custom_param),
 * 0 when they do not shrink.
 */
float DoFuncCustomDampedOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;

	float ratio = param[0];
	float duration = param[2];
	float start = 0.0f;
	int count = (int)param[1];
	float span;

	if (ratio < 1.0f) {
		span = duration / (1.0f - ratio);
	} else {
		duration = 0.0f;
		span = 1.0f;
	}

	float direction = -1.0f;
	float size = 1.0f;
	int step;

	for (step = 1; step < count + 1; step++) {
		if (duration == 0.0f) {
			size = 1.0f - (float)(step - 1) / (float)count;
			ratio = (1.0f - (float)step / (float)count) / size;
		}
		float next_size = ratio * size;
		float stop;

		start = span * (1.0f - size);
		stop = span * (1.0f - next_size);
		if (rate >= start && rate < stop)
			break;
		size = next_size;
		direction = -direction;
	}
	if (duration == 0.0f) {
		size = 1.0f;
		duration = 1.0f / (float)count;
		ratio = size;
	}

	float phase = (rate - start) / (size * duration) * PI;
	float next_size = ratio * size;
	float center, amplitude, cosine;

	if (count == step) {
		float end = size * direction + 1.0f;

		center = (end + 1.0f) * 0.5f;
		cosine = sce_paf_cosf(phase);
		amplitude = __builtin_fabsf(1.0f - end);
	} else {
		center = direction * 0.5f * (1.0f - ratio) * size + 1.0f;
		cosine = sce_paf_cosf(phase);
		amplitude = next_size + size;
	}
	return center + amplitude * direction * cosine * 0.5f;
}

static float DoFuncCustomDampedIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncCustomDampedOut(1.0f - rate, param);
}

float DoFuncCustomDampedOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	float value;

	if (rate < 0.5f)
		value = (1.0f - DoFuncCustomDampedIn(1.0f - rate * 2.0f, param)) * 0.5f;
	else
		value = DoFuncCustomDampedIn(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
	return value;
}

float DoFuncCustomDampedInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	float value;

	if (rate < 0.5f)
		value = DoFuncCustomDampedIn(rate * 2.0f, param) * 0.5f;
	else
		value = (1.0f - DoFuncCustomDampedIn(2.0f - rate * 2.0f, param)) * 0.5f + 0.5f;
	return value;
}

float Timer::DoFuncCustomElasticIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;

	float rate2 = rate * rate;
	float swing = param[0];
	float amplitude2 = param[1];
	float amplitude4 = param[2];
	float amplitude6 = param[3];
	float rate4 = rate2 * rate2;

	return (amplitude2 * rate2 + amplitude4 * rate4 + amplitude6 * rate4 * rate2) *
	       sce_paf_sinf(sce_paf_asinf(rate) + swing * PI * (rate - 1.0f));
}

float Timer::DoFuncCustomElasticOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	float value;

	if (rate < 0.5f)
		value = (1.0f - DoFuncCustomElasticIn(1.0f - rate * 2.0f, param)) * 0.5f;
	else
		value = DoFuncCustomElasticIn(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
	return value;
}

float Timer::DoFuncCustomElasticInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	float value;

	if (rate < 0.5f)
		value = DoFuncCustomElasticIn(rate * 2.0f, param) * 0.5f;
	else
		value = (1.0f - DoFuncCustomElasticIn(2.0f - rate * 2.0f, param)) * 0.5f + 0.5f;
	return value;
}

float Timer::DoFuncCustomElasticOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncCustomElasticIn(1.0f - rate, param);
}

/* Two parabolas meeting at param[0], at the height param[1]. */
float Timer::DoFuncCustomParabolicIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;

	float peak = param[0];
	float height = param[1];

	if (rate < peak) {
		float x = 1.0f / peak * rate - 1.0f;

		return height - height * x * x;
	}
	return (1.0f - height) / ((peak - 1.0f) * (peak - 1.0f)) * (rate - peak) * (rate - peak) + height;
}

float Timer::DoFuncCustomParabolicOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return (1.0f - DoFuncCustomParabolicIn(1.0f - rate * 2.0f, param)) * 0.5f;
	return DoFuncCustomParabolicIn(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
}

float Timer::DoFuncCustomParabolicInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncCustomParabolicIn(rate * 2.0f, param) * 0.5f;
	return (1.0f - DoFuncCustomParabolicIn(2.0f - rate * 2.0f, param)) * 0.5f + 0.5f;
}

float Timer::DoFuncCustomParabolicOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncCustomParabolicIn(1.0f - rate, param);
}

/* An exponential, param[0] * (1 - rate) to the power rate * param[1]. */
float Timer::DoFuncCustomExpoIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - sce_paf_powf(param[0] * (1.0f - rate), rate * param[1]);
}

float Timer::DoFuncCustomExpoOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	float value;

	if (rate < 0.5f)
		value = (1.0f - DoFuncCustomExpoIn(1.0f - rate * 2.0f, param)) * 0.5f;
	else
		value = DoFuncCustomExpoIn(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
	return value;
}

float Timer::DoFuncCustomExpoInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	float value;

	if (rate < 0.5f)
		value = DoFuncCustomExpoIn(rate * 2.0f, param) * 0.5f;
	else
		value = (1.0f - DoFuncCustomExpoIn(2.0f - rate * 2.0f, param)) * 0.5f + 0.5f;
	return value;
}

float Timer::DoFuncCustomExpoOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncCustomExpoIn(1.0f - rate, param);
}

/* A polynomial in rate squared, param[0] to param[3] its four terms. */
static float DoFuncPolyIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;

	float rate2 = rate * rate;

	return param[1] * (rate2 * rate2) + param[0] * rate2 + param[2] * (rate2 * rate2) * rate2 +
	       param[3] * (rate2 * rate2) * (rate2 * rate2);
}

float DoFuncPolyInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncPolyIn(rate * 2.0f, param) * 0.5f;
	return (1.0f - DoFuncPolyIn(2.0f - rate * 2.0f, param)) * 0.5f + 0.5f;
}

float DoFuncPolyOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncPolyIn(1.0f - rate, param);
}

float Timer::DoFuncBounceIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;

	float x = 1.0f - rate;

	if (rate > 0.636364f)
		return x * -7.5625f * x + 1.0f;
	if (rate > 0.272727f)
		return (x - 0.545454f) * -7.5625f * (x - 0.545454f) + 0.25f;
	if (rate > 0.090909f)
		return (x - 0.818182f) * -7.5625f * (x - 0.818182f) + 0.0625f;
	return (x - 0.954545f) * -7.5625f * (x - 0.954545f) + 0.015625f;
}

float Timer::DoFuncBounceInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncBounceIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncBounceIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

/* The point of table at rate, between its two nearest points. */
static inline float InterpolateTable(const float *table, float rate)
{
	float position = rate * 32.0f;
	int index = (int)position;
	float fraction = position - (float)index;

	return table[index] * (1.0f - fraction) + table[index + 1] * fraction;
}

float Timer::DoFuncExpoIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate == 0.0f)
		return 0.0f;
	return InterpolateTable(s_expo_in_table, rate);
}

float Timer::DoFuncElasticIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate == 0.0f)
		return 0.0f;
	if (rate == 1.0f)
		return 1.0f;

	float amplitude = DoFuncExpoIn(rate);

	return amplitude * sce_paf_sinf(sce_paf_asinf(rate) + (rate - 1.0f) * 20.94395f);
}

float Timer::DoFuncElasticInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncElasticIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncElasticIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncBackIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate * rate * (rate * 2.70158f - 1.70158f);
}

float Timer::DoFuncBackInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncBackIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncBackIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncCirqIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - __builtin_sqrtf(1.0f - rate * rate);
}

float Timer::DoFuncCirqInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncCirqIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncCirqIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

static float DoFuncExpoIn13(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate == 0.0f)
		return 0.0f;
	return InterpolateTable(s_expo_in_table_13, rate);
}

static float DoFuncExpoIn14(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate == 0.0f)
		return 0.0f;
	return InterpolateTable(s_expo_in_table_14, rate);
}

static float DoFuncExpoIn15(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate == 0.0f)
		return 0.0f;
	return InterpolateTable(s_expo_in_table_15, rate);
}

static float DoFuncExpoIn16(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate == 0.0f)
		return 0.0f;
	return InterpolateTable(s_expo_in_table_16, rate);
}

float DoFuncExpoInOut44(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncExpoIn16(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncExpoIn16(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float DoFuncExpoInOut43(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncExpoIn15(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncExpoIn15(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float DoFuncExpoInOut42(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncExpoIn14(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncExpoIn14(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float DoFuncExpoInOut41(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncExpoIn13(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncExpoIn13(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncExpoInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncExpoIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncExpoIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncSineIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - sce_paf_cosf(rate * PI * 0.5f);
}

float Timer::DoFuncSineInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncSineIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncSineIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncQuintIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate * rate * rate * rate * rate;
}

float Timer::DoFuncQuintInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncQuintIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncQuintIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncQuartIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate * rate * rate * rate;
}

float Timer::DoFuncQuartInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncQuartIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncQuartIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncCubicIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate * rate * rate;
}

float Timer::DoFuncCubicInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncCubicIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncCubicIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncFaster(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate * rate;
}

float Timer::DoFuncQuadIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return DoFuncFaster(rate);
}

float Timer::DoFuncQuadInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return DoFuncQuadIn(rate * 2.0f) * 0.5f;
	return (1.0f - DoFuncQuadIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float Timer::DoFuncBounceOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncBounceIn(1.0f - rate);
}

float Timer::DoFuncElasticOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncElasticIn(1.0f - rate);
}

float Timer::DoFuncBackOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncBackIn(1.0f - rate);
}

float Timer::DoFuncCirqOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncCirqIn(1.0f - rate);
}

float DoFuncExpoOut30(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncExpoIn16(1.0f - rate);
}

float DoFuncExpoOut29(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncExpoIn15(1.0f - rate);
}

float DoFuncExpoOut28(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncExpoIn14(1.0f - rate);
}

float DoFuncExpoOut27(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncExpoIn13(1.0f - rate);
}

float Timer::DoFuncExpoOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncExpoIn(1.0f - rate);
}

float Timer::DoFuncSineOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncSineIn(1.0f - rate);
}

float Timer::DoFuncQuintOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncQuintIn(1.0f - rate);
}

float Timer::DoFuncQuartOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncQuartIn(1.0f - rate);
}

float Timer::DoFuncCubicOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncCubicIn(1.0f - rate);
}

float Timer::DoFuncQuadOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - DoFuncQuadIn(1.0f - rate);
}

float Timer::DoFuncSwing(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return sce_paf_cosf(rate * PI) * -0.5f + 0.5f;
}

float Timer::Cbrt(float input)
{
	if (input < 0.0f)
		return -sce_paf_powf(-input, 1.0f / 3.0f);
	return sce_paf_powf(input, 1.0f / 3.0f);
}

/*
 * A cubic Bezier curve from (0, 0) to (1, 1) through the control points
 * (param[0], param[1]) and (param[2], param[3]): its x is solved for the time
 * by Cardano's formula, and the y at that time returned.
 */
float Timer::DoFuncBezier3d(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;

	float x1 = param[0] * 3.0f;
	float x2 = param[2] * 3.0f;
	float a = (x1 - x2) + 1.0f;
	float b = (param[0] * -6.0f + x2) / a;
	float c = x1 / a;
	float d = -rate / a;
	float p = (c - (b * b) / 3.0f) / 3.0f;
	float q = (d + (sce_paf_powf(b, 3.0f) * 2.0f) / 27.0f) - (b * c) / 3.0f;
	float half_q = q * 0.5f;
	float root = __builtin_sqrtf(sce_paf_powf(half_q, 2.0f) + sce_paf_powf(p, 3.0f));
	float shift = b / 3.0f;
	float roots = Cbrt(root - half_q) + Cbrt(-q * 0.5f - root);
	float y1 = param[1];
	float y2 = param[3];
	float t = roots - shift;
	float t3 = sce_paf_powf(t, 3.0f);

	return (y1 * -6.0f + y2 * 3.0f) * sce_paf_powf(t, 2.0f) + ((y1 * 3.0f - y2 * 3.0f) + 1.0f) * t3 +
	       y1 * 3.0f * t;
}

float Timer::DoFuncExponent(float rate, float total, float initval, const float *param)
{
	return DoFuncExpoOut29(rate);
}

float Timer::DoFuncAngular(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return (sce_paf_sinf((rate - 0.5f) * PI) + 1.0f) * 0.5f;
}

float Timer::DoFuncSlower(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return __builtin_sqrtf(rate);
}

float Timer::DoFunc(float rate, int func, float pre, float *param) const
{
	switch (func) {
	case FUNC_LINEAR:
		return DoFuncLinear(rate);
	case FUNC_SLOWDOWN:
		return DoFuncSlower(rate);
	case FUNC_FASTUP:
		return DoFuncFaster(rate);
	case FUNC_ANGULAR:
		return DoFuncAngular(rate);
	case FUNC_EXPONENT:
		return DoFuncExponent(rate, m_total, pre, param);
	case FUNC_BEZIER_3D:
		return DoFuncBezier3d(rate, param);
	case FUNC_SWING:
		return DoFuncSwing(rate);
	case FUNC_QUAD_IN:
		return DoFuncQuadIn(rate);
	case FUNC_CUBIC_IN:
		return DoFuncCubicIn(rate);
	case FUNC_QUART_IN:
		return DoFuncQuartIn(rate);
	case FUNC_QUINT_IN:
		return DoFuncQuintIn(rate);
	case FUNC_SINE_IN:
		return DoFuncSineIn(rate);
	case FUNC_EXPO_IN:
		return DoFuncExpoIn(rate);
	case FUNC_EXPO_IN_13:
		return DoFuncExpoIn13(rate);
	case FUNC_EXPO_IN_14:
		return DoFuncExpoIn14(rate);
	case FUNC_EXPO_IN_15:
		return DoFuncExpoIn15(rate);
	case FUNC_EXPO_IN_16:
		return DoFuncExpoIn16(rate);
	case FUNC_CIRQ_IN:
		return DoFuncCirqIn(rate);
	case FUNC_BACK_IN:
		return DoFuncBackIn(rate);
	case FUNC_ELASTIC_IN:
		return DoFuncElasticIn(rate);
	case FUNC_BOUNCE_IN:
		return DoFuncBounceIn(rate);
	case FUNC_QUAD_OUT:
		return DoFuncQuadOut(rate);
	case FUNC_CUBIC_OUT:
		return DoFuncCubicOut(rate);
	case FUNC_QUART_OUT:
		return DoFuncQuartOut(rate);
	case FUNC_QUINT_OUT:
		return DoFuncQuintOut(rate);
	case FUNC_SINE_OUT:
		return DoFuncSineOut(rate);
	case FUNC_EXPO_OUT:
		return DoFuncExpoOut(rate);
	case FUNC_EXPO_OUT_27:
		return DoFuncExpoOut27(rate);
	case FUNC_EXPO_OUT_28:
		return DoFuncExpoOut28(rate);
	case FUNC_EXPO_OUT_29:
		return DoFuncExpoOut29(rate);
	case FUNC_EXPO_OUT_30:
		return DoFuncExpoOut30(rate);
	case FUNC_CIRQ_OUT:
		return DoFuncCirqOut(rate);
	case FUNC_BACK_OUT:
		return DoFuncBackOut(rate);
	case FUNC_ELASTIC_OUT:
		return DoFuncElasticOut(rate);
	case FUNC_BOUNCE_OUT:
		return DoFuncBounceOut(rate);
	case FUNC_QUAD_INOUT:
		return DoFuncQuadInOut(rate);
	case FUNC_CUBIC_INOUT:
		return DoFuncCubicInOut(rate);
	case FUNC_QUART_INOUT:
		return DoFuncQuartInOut(rate);
	case FUNC_QUINT_INOUT:
		return DoFuncQuintInOut(rate);
	case FUNC_SINE_INOUT:
		return DoFuncSineInOut(rate);
	case FUNC_EXPO_INOUT:
		return DoFuncExpoInOut(rate);
	case FUNC_EXPO_INOUT_41:
		return DoFuncExpoInOut41(rate);
	case FUNC_EXPO_INOUT_42:
		return DoFuncExpoInOut42(rate);
	case FUNC_EXPO_INOUT_43:
		return DoFuncExpoInOut43(rate);
	case FUNC_EXPO_INOUT_44:
		return DoFuncExpoInOut44(rate);
	case FUNC_CIRQ_INOUT:
		return DoFuncCirqInOut(rate);
	case FUNC_BACK_INOUT:
		return DoFuncBackInOut(rate);
	case FUNC_ELASTIC_INOUT:
		return DoFuncElasticInOut(rate);
	case FUNC_BOUNCE_INOUT:
		return DoFuncBounceInOut(rate);
	case FUNC_POLY_IN:
	case FUNC_POLY_IN_52:
		return DoFuncPolyIn(rate, param);
	case FUNC_POLY_OUT:
	case FUNC_POLY_OUT_53:
		return DoFuncPolyOut(rate, param);
	case FUNC_POLY_INOUT:
	case FUNC_POLY_INOUT_54:
		return DoFuncPolyInOut(rate, param);
	case FUNC_CUSTOM_EXPO_IN:
		return DoFuncCustomExpoIn(rate, param);
	case FUNC_CUSTOM_EXPO_OUT:
		return DoFuncCustomExpoOut(rate, param);
	case FUNC_CUSTOM_EXPO_INOUT:
		return DoFuncCustomExpoInOut(rate, param);
	case FUNC_CUSTOM_EXPO_OUTIN:
		return DoFuncCustomExpoOutIn(rate, param);
	case FUNC_CUSTOM_PARABOLIC_IN:
		return DoFuncCustomParabolicIn(rate, param);
	case FUNC_CUSTOM_PARABOLIC_OUT:
		return DoFuncCustomParabolicOut(rate, param);
	case FUNC_CUSTOM_PARABOLIC_INOUT:
		return DoFuncCustomParabolicInOut(rate, param);
	case FUNC_CUSTOM_PARABOLIC_OUTIN:
		return DoFuncCustomParabolicOutIn(rate, param);
	case FUNC_CUSTOM_ELASTIC_IN:
		return DoFuncCustomElasticIn(rate, param);
	case FUNC_CUSTOM_ELASTIC_OUT:
		return DoFuncCustomElasticOut(rate, param);
	case FUNC_CUSTOM_ELASTIC_INOUT:
		return DoFuncCustomElasticInOut(rate, param);
	case FUNC_CUSTOM_ELASTIC_OUTIN:
		return DoFuncCustomElasticOutIn(rate, param);
	case FUNC_CUSTOM_DAMPED_IN:
		return DoFuncCustomDampedIn(rate, param);
	case FUNC_CUSTOM_DAMPED_OUT:
		return DoFuncCustomDampedOut(rate, param);
	case FUNC_CUSTOM_DAMPED_INOUT:
		return DoFuncCustomDampedInOut(rate, param);
	case FUNC_CUSTOM_DAMPED_OUTIN:
		return DoFuncCustomDampedOutIn(rate, param);
	case FUNC_CUSTOM_BACK_IN:
		return DoFuncCustomBackIn(rate, param);
	case FUNC_CUSTOM_BACK_OUT:
		return DoFuncCustomBackOut(rate, param);
	case FUNC_CUSTOM_BACK_INOUT:
		return DoFuncCustomBackInOut(rate, param);
	case FUNC_CUSTOM_BACK_OUTIN:
		return DoFuncCustomBackOutIn(rate, param);
	default:
		/* the custom bounce (no curve) and anything unknown run linearly */
		return DoFuncLinear(rate);
	}
}

float Timer::DoTick()
{
	if (m_state == State_Disabled)
		return 0.0f;
	if (m_state == State_Hold)
		return m_curr * m_total_inv;

	float interval = s_tick_interval;

	if (m_first_tick)
		m_first_tick = false;

	/* a finished timer that repeats starts over */
	if (m_state == State_Finished && m_repeat != REPEAT_NONE && m_repeat_count != 0) {
		switch (m_repeat) {
		case REPEAT_RBACK:
			m_state = State_Active;
			m_curr = m_total;
			break;
		case REPEAT_ROUND:
			m_state = State_Active;
			m_curr = 0.0f;
			break;
		case REPEAT_LOOP:
			ResetTimer();
			break;
		}
	}

	float tick = s_tick_unit * interval;

	if (m_repeat == REPEAT_RBACK)
		m_curr -= tick;
	else
		m_curr += tick;

	float rate = m_curr * m_total_inv;

	switch (m_state) {
	case State_Standby:
		m_first_tick = true;
		m_state = State_Active;
		break;
	case State_Active:
		break;
	case State_Finished:
		m_curr = m_total;
		if (m_repeat == REPEAT_RBACK)
			m_mapped_x = 0.0f;
		else
			m_mapped_x = 1.0f;
		return rate;
	default:
		return rate;
	}

	m_mapped_x = DoFunc(rate, m_func_x, m_mapped_x, m_param_x);
	if (m_repeat != REPEAT_RBACK && m_curr >= m_total) {
		m_curr = m_total;
		m_state = State_Finished;
		m_mapped_x = 1.0f;

		int count = m_repeat_count;

		if (count > 0) {
			m_repeat_count = count - 1;
			count = m_repeat_count;
		}
		if (count == 0) {
			m_repeat = REPEAT_NONE;
			m_repeat_count = 0;
		} else if (m_repeat == REPEAT_ROUND) {
			/* a round trip comes back */
			m_repeat = REPEAT_RBACK;
		}
	} else if (m_repeat == REPEAT_RBACK && m_curr >= m_total) {
		m_state = State_Finished;
		m_curr = 0.0f;
		m_mapped_x = 0.0f;

		int count = m_repeat_count;

		if (count > 0) {
			m_repeat_count = count - 1;
			count = m_repeat_count;
		}
		if (count == 0) {
			m_repeat = REPEAT_NONE;
			m_repeat_count = 0;
		} else {
			m_repeat = REPEAT_ROUND;
		}
	}
	return rate;
}

float Timer::DoFuncExponent_i(float curr, float total, const float *param)
{
	return (curr + (1.0f - curr) * param[0]) * 1.0001f;
}

Timer2D::~Timer2D()
{
	if (m_param_y != NULL)
		delete[] m_param_y;
}

float Timer2D::DoTick()
{
	if (m_state == State_Hold)
		return m_curr * m_total_inv;

	float rate = Timer::DoTick();

	if (m_state == State_Active)
		m_mapped_y = DoFunc(rate, m_func_y, m_mapped_y, m_param_y);
	else
		m_mapped_y = m_mapped_x;
	return rate;
}

void Timer2D::SetFuncY(int func, const float *param)
{
	int param_num;

	m_func_y = init_func_param(func, param, &m_param_y, &param_num);
	m_param_y_num = param_num;
}

Timer3D::~Timer3D()
{
	if (m_param_z != NULL)
		delete[] m_param_z;
}

float Timer3D::DoTick()
{
	if (m_state == State_Hold)
		return m_curr * m_total_inv;

	float rate = Timer::DoTick();

	if (m_state == State_Active) {
		m_mapped_y = DoFunc(rate, m_func_y, m_mapped_y, m_param_y);
		m_mapped_z = DoFunc(rate, m_func_z, m_mapped_z, m_param_z);
	} else {
		m_mapped_y = m_mapped_x;
		m_mapped_z = m_mapped_x;
	}
	return rate;
}

void Timer3D::SetFuncZ(int func, const float *param)
{
	int param_num;

	m_func_z = init_func_param(func, param, &m_param_z, &param_num);
	m_param_z_num = param_num;
}
