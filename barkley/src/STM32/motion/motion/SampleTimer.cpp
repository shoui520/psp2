/*
 * SampleTimer: the timer's period, clock / rate counts, and its channel's PWM output,
 * active low, its compare at the period's end.
 */

#include "SampleTimer.h"

TIM_TypeDef *const timers[4] = { TIM1, TIM2, TIM3, TIM4 };

void SampleTimer::Init(u16 prescaler, u32 clock, u32 rate)
{
	TIM_TimeBaseInitTypeDef timeBase;
	TIM_OCInitTypeDef oc;
	u16 period;

	clock_ = clock;
	period = clock / rate - 1;
	TIM_TimeBaseStructInit(&timeBase);
	timeBase.TIM_Prescaler = prescaler;
	timeBase.TIM_CounterMode = TIM_CounterMode_Up;
	timeBase.TIM_Period = period;
	timeBase.TIM_ClockDivision = TIM_CKD_DIV1;
	timeBase.TIM_RepetitionCounter = 0;
	TIM_OCStructInit(&oc);
	oc.TIM_OCMode = TIM_OCMode_PWM1;
	oc.TIM_OutputState = TIM_OutputState_Enable;
	oc.TIM_Pulse = period;
	oc.TIM_OCPolarity = TIM_OCPolarity_Low;
	channel_->Init(&oc);
	TIM_TimeBaseInit(regs_, &timeBase);
	channel_->PreloadConfig(TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(regs_, ENABLE);
	if ((u32)regs_ == TIM1_BASE || (u32)regs_ == TIM8_BASE)
		TIM_CtrlPWMOutputs(regs_, ENABLE);
}

void SampleTimer::Start()
{
	TIM_SetCounter(regs_, 0);
	TIM_Cmd(regs_, ENABLE);
}

void SampleTimer::Stop()
{
	TIM_Cmd(regs_, DISABLE);
}

void SampleTimer::Resume()
{
	TIM_Cmd(regs_, ENABLE);
}
