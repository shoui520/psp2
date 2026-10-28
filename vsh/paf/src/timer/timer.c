#include "timer.h"

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
