#include "timer.h"
#include "../std/math.h"
#include "../std/string.h"

float TimerTickUnit = 1.0f / 60.0f;
float TimerTickInv = 60.0f;
float TimerTickInterval = 1.0f;

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
