/*
 * AdcSampler: the accelerometer, gyroscope and temperature samples, taken by the ADC
 * on a timer and moved to memory by DMA, reported in blocks.
 */
#ifndef ADCSAMPLER_H
#define ADCSAMPLER_H

#include "stm32f10x_type.h"
#include "Callback.h"
#include "MotionFrameUtil.h"

class SampleTimer;
class Adc;

class AdcSampler {
public:
	typedef Callback<void (s32, const AdcSample *)> Func;

	AdcSampler(SampleTimer *timer, Adc *adc, AdcSample *buffer, u32 count);
	void Init(u8 channel, u8 rank, u32 period, u16 prescaler, u32 clock, u32 rate);
	void Start(Func::Func func, void *arg);
	void StartTriggered(Func::Func func, void *arg);
	void StartSelfTest(Func::Func func, void *arg);

	void SetCallback(Func::Func func, void *arg)
	{
		callback_.Set(func, arg);
	}

	void Stop();

	void StopSelfTest()
	{
		Stop();
	}

	void Resume();
	void PowerDown();
	void PowerUp();

private:
	SampleTimer *timer_;
	Adc *adc_;
	Func callback_;
	AdcSample *buffer_;
	u32 count_;
};

#endif
