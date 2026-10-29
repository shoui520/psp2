#include "timer.h"
#include "../std/math.h"
#include "../std/new.h"
#include "../std/string.h"

float TimerTickUnit = 1.0f / 60.0f;
float TimerTickInv = 60.0f;
float TimerTickInterval = 1.0f;

static const float s_default_params_0[4] = { 0.2f, 1.5f, 0.0f, 0.0f };
static const float s_default_params_1[4] = { 0.15f, -0.08f, 0.0f, 0.0f };
static const float s_default_params_2[4] = { 0.7f, 5.0f, 0.0f, 0.0f };
static const float s_default_params_3[4] = { 6.0f, 0.4f, 0.5f, 0.1f };
static const float s_default_params_4[4] = { 0.8f, 12.0f, 0.0f, 0.0f };
static const float s_default_params_5[4] = { 0.1f, 0.0f, 0.0f, 0.0f };
static const float s_default_params_6[4] = { 0.2f, 0.3f, 0.5f, 0.0f };

/* The first two words precede the ARM C++ ABI vtable address point. */
const TimerClass timerClass = {
	0, NULL, { TimerDestroy, TimerDelete }
};

Timer *TimerDestroy(Timer *self)
{
	float *param = self->m_param_x;

	self->vtable = &timerClass.methods;
	if (param)
		PafDeleteArray(param);
	return self;
}

void TimerDelete(Timer *self)
{
	TimerDestroy(self);
	PafDelete(self);
}

Timer *TimerDestroyBase(Timer *self)
{
	return TimerDestroy(self);
}

void TimerInitTickUnit(float tick_unit)
{
	float unit = tick_unit * 0.001f;
	float inv = 1.0f / unit;

	TimerTickUnit = unit;
	TimerTickInterval = 1.0f;
	TimerTickInv = inv;
}

void TimerSetCurrTickInterval(float tick_interval)
{
	TimerTickInterval = tick_interval;
}

void TimerResetTimer(Timer *self)
{
	self->m_curr = 0.0f;
	self->m_mapped_x = 0.0f;
	self->m_state = STATE_STANDBY;
	self->m_first_tick = 0;
}

int TimerCheckCustomParam(Timer *self, float *param)
{
	float ratio = param[0];
	float countValue = param[1];
	float duration = 0.0f;
	int count;

	(void)self;
	if (ratio >= 1.0f) {
		ratio = 1.0f;
		param[0] = ratio;
	}
	if (ratio < 0.0f) {
		ratio = 0.01f;
		param[0] = ratio;
	}
	if (countValue < 1.0f) {
		/* The original resets the ratio, leaving the count unchanged. */
		ratio = 1.0f;
		param[0] = ratio;
	}
	count = FloatToInt(countValue);
	if (ratio < 1.0f) {
		float power = sce_paf_powf(ratio, (float)count);

		duration = (1.0f - ratio) / (1.0f - power);
	}
	param[2] = duration;
	return 0;
}

int TimerCheckBezierParam(Timer *self, float *param)
{
	float x1, x2, threeX1, threeX2, cubic, quadratic;
	float b, c, d, p, cubeB, bc, twiceCubeB;
	float halfQ, halfQSquared, pCubed;

	(void)self;
	x1 = param[0];
	x2 = param[2];
	threeX1 = x1 * 3.0f;
	threeX2 = x2 * 3.0f;
	quadratic = x1 * -6.0f + x2 * 3.0f;
	cubic = (threeX1 - threeX2) + 1.0f;
	b = quadratic / cubic;
	c = threeX1 / cubic;
	d = 0.0f / cubic;
	p = (c - (b * b) / 3.0f) / 3.0f;
	cubeB = sce_paf_powf(b, 3.0f);
	bc = b * c;
	twiceCubeB = cubeB * 2.0f;
	bc /= 3.0f;
	twiceCubeB /= 27.0f;
	d += twiceCubeB;
	d -= bc;
	halfQ = d * 0.5f;
	halfQSquared = sce_paf_powf(halfQ, 2.0f);
	pCubed = sce_paf_powf(p, 3.0f);
	if (halfQSquared + pCubed < 0.0f) {
		sce_paf_memset(param, 0, 4 * sizeof(*param));
		return FUNC_LINEAR;
	}
	return FUNC_BEZIER_3D;
}

int TimerInitFuncParam(Timer *self, int func, const float *param,
		       float **ret_param, int *ret_param_num)
{
	float *pret_param = *ret_param;
	int pcount = 0;
	int ret = func;

	if (pret_param)
		PafDeleteArray(pret_param);
	pret_param = NULL;
	if (func == FUNC_BEZIER_3D) {
		pcount = 4;
		pret_param = PafNewArray(4 * sizeof(*pret_param));
	} else if (func >= FUNC_CUSTOM_EXPO_IN &&
		   func <= FUNC_CUSTOM_PARABOLIC_OUTIN) {
		pcount = 2;
		pret_param = PafNewArray(2 * sizeof(*pret_param));
	} else if ((func >= FUNC_CUSTOM_BOUNCE_IN &&
		    func <= FUNC_CUSTOM_BOUNCE_OUTIN) ||
		   (func >= FUNC_CUSTOM_UNK_71 && func <= FUNC_CUSTOM_UNK_74)) {
		/* These functions copy two values into a four-float allocation. */
		pcount = 2;
		pret_param = PafNewArray(4 * sizeof(*pret_param));
	} else if (func >= FUNC_CUSTOM_UNK_75 && func <= FUNC_CUSTOM_UNK_78) {
		pcount = 1;
		pret_param = PafNewArray(sizeof(*pret_param));
	} else if ((func >= FUNC_CUSTOM_ELASTIC_IN &&
		    func <= FUNC_CUSTOM_ELASTIC_OUTIN) ||
		   (func >= FUNC_UNK_49 && func <= FUNC_UNK_54)) {
		pcount = 4;
		pret_param = PafNewArray(4 * sizeof(*pret_param));
	}

	if (!param) {
		if (func >= FUNC_CUSTOM_EXPO_IN && func <= FUNC_CUSTOM_EXPO_OUTIN)
			param = s_default_params_0;
		else if (func >= FUNC_CUSTOM_PARABOLIC_IN &&
			 func <= FUNC_CUSTOM_PARABOLIC_OUTIN)
			param = s_default_params_1;
		else if (func >= FUNC_CUSTOM_BOUNCE_IN &&
			 func <= FUNC_CUSTOM_BOUNCE_OUTIN)
			param = s_default_params_2;
		else if (func >= FUNC_CUSTOM_ELASTIC_IN &&
			 func <= FUNC_CUSTOM_ELASTIC_OUTIN)
			param = s_default_params_3;
		else if (func >= FUNC_CUSTOM_UNK_71 && func <= FUNC_CUSTOM_UNK_74)
			param = s_default_params_4;
		else if (func >= FUNC_CUSTOM_UNK_75 && func <= FUNC_CUSTOM_UNK_78)
			param = s_default_params_5;
		else if (func >= FUNC_UNK_49 && func <= FUNC_UNK_54)
			param = s_default_params_6;
	}
	if (param)
		sce_paf_memcpy(pret_param, param, pcount * sizeof(*pret_param));

	if (func == FUNC_BEZIER_3D) {
		ret = TimerCheckBezierParam(self, pret_param);
	} else if (func >= FUNC_CUSTOM_ELASTIC_IN &&
		   func <= FUNC_CUSTOM_ELASTIC_OUTIN) {
		float sum;

		if (pret_param[0] < 1.0f)
			pret_param[0] = 1.0f;
		sum = (pret_param[1] + pret_param[2]) + pret_param[3];
		if (__builtin_fabsf(sum - 1.0f) > 0.01f) {
			pret_param[1] = s_default_params_3[1];
			pret_param[2] = s_default_params_3[2];
			pret_param[3] = s_default_params_3[3];
		}
	}
	if (func >= FUNC_CUSTOM_UNK_71 && func <= FUNC_CUSTOM_UNK_74)
		TimerCheckCustomParam(self, pret_param);
	*ret_param = pret_param;
	*ret_param_num = pcount;
	return ret;
}

void TimerSetFuncX(Timer *self, int func, const float *param)
{
	int param_count;

	self->m_func_x = TimerInitFuncParam(self, func, param, &self->m_param_x,
					 &param_count);
	self->m_param_x_num = param_count;
}

void TimerInitTimer(Timer *self, float total, int func, int repeat,
		    int rep_count, const float *param)
{
	float ctotal;
	int ticks, param_count;

	TimerResetTimer(self);
	self->m_nonpositive_total = total <= 0.0f;
	self->m_total = total * 0.001f;
	ticks = FloatToInt(self->m_total * TimerTickInv);
	if (ticks <= 0)
		ticks = 1;
	ctotal = ((float)ticks * TimerTickUnit) * 0.999999f;
	self->m_repeat = repeat;
	self->m_repeat_count = rep_count;
	self->m_total = ctotal;
	self->m_total_inv = 1.0f / ctotal;
	self->m_func_x = TimerInitFuncParam(self, func, param, &self->m_param_x,
					 &param_count);
	self->m_param_x_num = param_count;
}

float TimerDoTick(Timer *self)
{
	unsigned int state = self->m_state;
	signed char count = (signed char)self->m_repeat_count;
	unsigned int repeat;
	float interval;
	float delta;
	float rate;

	if (state == STATE_DISABLED)
		return 0.0f;
	if (state == STATE_HOLD)
		return self->m_curr * self->m_total_inv;

	interval = TimerTickInterval;
	if (self->m_first_tick)
		self->m_first_tick = 0;
	repeat = self->m_repeat;
	if (state == STATE_FINISHED && repeat != REPEAT_NONE && count != 0) {
		switch (repeat) {
		case REPEAT_RBACK:
			self->m_state = STATE_ACTIVE;
			self->m_curr = self->m_total;
			break;
		case REPEAT_ROUND:
			self->m_state = STATE_ACTIVE;
			self->m_curr = 0.0f;
			break;
		case REPEAT_LOOP:
			self->m_mapped_x = 0.0f;
			self->m_state = STATE_STANDBY;
			self->m_curr = 0.0f;
			self->m_first_tick = 0;
			break;
		}
	}

	delta = TimerTickUnit * interval;
	if (repeat == REPEAT_RBACK) {
		float curr = self->m_curr - delta;

		rate = curr * self->m_total_inv;
		self->m_curr = curr;
	} else {
		float curr = self->m_curr + delta;

		rate = curr * self->m_total_inv;
		self->m_curr = curr;
	}
	state = self->m_state;
	if (state == STATE_STANDBY) {
		self->m_first_tick = 1;
		self->m_state = STATE_ACTIVE;
	} else if (state == STATE_FINISHED) {
		self->m_curr = self->m_total;
		self->m_mapped_x = repeat == REPEAT_RBACK ? 0.0f : 1.0f;
		return rate;
	} else if (state != STATE_ACTIVE) {
		return rate;
	}

	self->m_mapped_x = TimerDoFunc(self, rate, self->m_func_x,
				     self->m_mapped_x, self->m_param_x);
	if (self->m_repeat != REPEAT_RBACK) {
		if (self->m_curr >= self->m_total) {
			self->m_curr = self->m_total;
			self->m_state = STATE_FINISHED;
			count = (signed char)self->m_repeat_count;
			self->m_mapped_x = 1.0f;
			if (count > 0)
				self->m_repeat_count = --count;
			if (count == 0) {
				self->m_repeat = REPEAT_NONE;
				self->m_repeat_count = 0;
			} else if (self->m_repeat == REPEAT_ROUND) {
				self->m_repeat = REPEAT_RBACK;
			}
		}
	} else if (self->m_curr >= self->m_total) {
		self->m_state = STATE_FINISHED;
		self->m_curr = 0.0f;
		count = (signed char)self->m_repeat_count;
		self->m_mapped_x = 0.0f;
		if (count > 0)
			self->m_repeat_count = --count;
		self->m_repeat = count == 0 ? REPEAT_NONE : REPEAT_ROUND;
		if (count == 0)
			self->m_repeat_count = 0;
	}
	return rate;
}
