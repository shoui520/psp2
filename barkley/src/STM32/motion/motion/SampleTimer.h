/*
 * SampleTimer: a timer counting at the clock given, its period the sample rate, one
 * of its channels a PWM output whose compare event starts the ADC's conversions.
 * TimerChannel: a timer's output-compare channel (TimerChannel3: channel 3).
 */
#ifndef SAMPLETIMER_H
#define SAMPLETIMER_H

extern "C" {
#include "stm32f10x_lib.h"
}

/* the timers' registers, by number (TIM1-TIM4) */
extern TIM_TypeDef *const timers[4];

class TimerPort {
public:
	TimerPort(s8 timer)
	{
		regs_ = timers[timer];
	}

protected:
	TIM_TypeDef *regs_;
};

class TimerChannel : public TimerPort {
public:
	TimerChannel(s8 timer) : TimerPort(timer) {}
	virtual void Init(TIM_OCInitTypeDef *init) {}
	virtual void SetCompare(u16 compare) {}
	virtual void PreloadConfig(u16 preload) {}
};

class TimerChannel3 : public TimerChannel {
public:
	TimerChannel3(s8 timer) : TimerChannel(timer) {}

	virtual void Init(TIM_OCInitTypeDef *init)
	{
		TIM_OC3Init(regs_, init);
	}

	virtual void SetCompare(u16 compare)
	{
		TIM_SetCompare3(regs_, compare);
	}

	virtual void PreloadConfig(u16 preload)
	{
		TIM_OC3PreloadConfig(regs_, preload);
	}
};

class SampleTimer : public TimerPort {
public:
	SampleTimer(s8 timer, TimerChannel *channel) : TimerPort(timer)
	{
		channel_ = channel;
	}

	void Init(u16 prescaler, u32 clock, u32 rate);
	void Start();
	void Stop();
	void Resume();

	/* the count from 0, the timer as it is */
	void Reset()
	{
		TIM_SetCounter(regs_, 0);
	}

private:
	TimerChannel *channel_;
	u32 clock_;
};

#endif
