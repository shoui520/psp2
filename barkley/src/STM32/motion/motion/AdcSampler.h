/*
 * AdcSampler: the accelerometer, gyroscope and temperature samples, taken by the ADC
 * on a timer and moved to memory by DMA, reported in blocks: each half of the buffer
 * as it fills, sample by sample.
 */
#ifndef ADCSAMPLER_H
#define ADCSAMPLER_H

#include "Adc.h"
#include "Callback.h"
#include "MotionFrameUtil.h"
#include "SampleTimer.h"

class AdcSampler {
public:
	typedef Callback<void (s32, const AdcSample *)> Func;

	AdcSampler(SampleTimer *timer, Adc *adc, AdcSample *buffer, u32 count);
	void Init(u8 preemption, u8 sub, u32 priority, u16 prescaler, u32 clock, u32 rate);
	void Start(Func::Func func, void *arg);
#if MOTION_IMAGE != 0
	void StartTriggered(Func::Func func, void *arg);
#endif
	void StartSelfTest(Func::Func func, void *arg);

	void SetCallback(Func::Func func, void *arg)
	{
		callback_.Set(func, arg);
	}

	void Stop()
	{
		timer_->Stop();
	}

	void StopSelfTest()
	{
#if MOTION_IMAGE == 0
		timer_->Stop();
#else
		Stop();
#endif
	}

	void Resume()
	{
		timer_->Resume();
	}

	void PowerDown()
	{
		adc_->PowerDown();
	}

	void PowerUp()
	{
		adc_->PowerUp();
	}

private:
	static void SampleCallback(void *arg, Request *request);
	void OnSamples(Request *request);
	static void SelfTestCallback(void *arg, Request *request);
	void OnSelfTest(Request *request);

	SampleTimer *timer_;
	Adc *adc_;
	Func callback_;
	AdcSample *buffer_;	/* two halves of count samples */
	u32 count_;
};

#endif
