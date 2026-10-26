/*
 * AdcSampler: the ADC converts the channels in this order on each timer event: the
 * temperature sensor (16) last, the accelerometer's and the gyroscope's as AdcSample
 * lays them out.
 */

#include "AdcSampler.h"

#define ADC_CHANNELS	10

static const AdcChannel adcChannels[ADC_CHANNELS] = {
	{ ADC_Channel_15, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_0, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_7, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_8, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_9, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_10, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_12, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_14, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_6, ADC_SampleTime_28Cycles5 },
	{ ADC_Channel_16, ADC_SampleTime_28Cycles5 },
};

AdcSampler::AdcSampler(SampleTimer *timer, Adc *adc, AdcSample *buffer, u32 count)
	: timer_(timer), adc_(adc), buffer_(buffer), count_(count)
{
}

/* the ADC's DMA at the priority given; the timer's rate the sample rate */
void AdcSampler::Init(u8 preemption, u8 sub, u32 priority, u16 prescaler, u32 clock, u32 rate)
{
	adc_->Init(adcChannels, ADC_CHANNELS, preemption, sub, priority);
	Adc::EnableTempSensor();
	timer_->Init(prescaler, clock, rate);
}

void AdcSampler::Start(Func::Func func, void *arg)
{
	callback_.Set(func, arg);
	adc_->Start(buffer_, count_ * sizeof(AdcSample), SampleCallback, this);
	timer_->Start();
}

void AdcSampler::SampleCallback(void *arg, Request *request)
{
	static_cast<AdcSampler *>(arg)->OnSamples(request);
}

/* the half just filled, sample by sample, or the error */
void AdcSampler::OnSamples(Request *request)
{
	s32 result = request->Result();

	if (result != 0) {
		callback_(result, 0);
	} else {
		const AdcSample *sample = (const AdcSample *)request->cur_;

		for (u32 i = 0; i < count_; i++) {
			callback_(0, sample);
			sample++;
		}
	}
}

#if MOTION_IMAGE == 0
/* the conversions again, their first half reported */
void AdcSampler::StartSelfTest(Func::Func func, void *arg)
{
	callback_.Set(func, arg);
	adc_->Start(buffer_, count_ * sizeof(AdcSample), SelfTestCallback, this);
	timer_->Start();
}
#else
/* the conversions from the next trigger; the timer is left stopped */
void AdcSampler::StartTriggered(Func::Func func, void *arg)
{
	callback_.Set(func, arg);
	adc_->Start(buffer_, count_ * sizeof(AdcSample), SampleCallback, this);
	timer_->Reset();
}

/* the next half, from the running conversions */
void AdcSampler::StartSelfTest(Func::Func func, void *arg)
{
	callback_.Set(func, arg);
	adc_->SetDone(SelfTestCallback, this);
	timer_->Resume();
}
#endif

void AdcSampler::SelfTestCallback(void *arg, Request *request)
{
	static_cast<AdcSampler *>(arg)->OnSelfTest(request);
}

/* one half, the timer stopped */
void AdcSampler::OnSelfTest(Request *request)
{
	StopSelfTest();
	s32 result = request->Result();

	if (result != 0) {
		callback_(result, 0);
	} else {
		const AdcSample *sample = (const AdcSample *)request->cur_;

		for (u32 i = 0; i < count_; i++) {
			callback_(0, sample);
			sample++;
		}
	}
}
