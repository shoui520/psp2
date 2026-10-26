/*
 * Adc: an ADC converting a sequence of channels on each trigger (an external event),
 * the results moved to memory by a circular DMA channel.
 */
#ifndef ADC_H
#define ADC_H

extern "C" {
#include "stm32f10x_lib.h"
}
#include "Dma.h"

/* an ADC's registers and DMA channel (7: none) */
struct AdcPortInfo {
	ADC_TypeDef *regs;
	s8 dma;
	s8 reserved;
};

extern const AdcPortInfo adcPorts[2];

/* a channel of the sequence: its number and sample time */
struct AdcChannel {
	u8 channel;
	u8 sampleTime;
};

class AdcPort {
public:
	AdcPort(s8 index)
	{
		index_ = index;
		regs_ = adcPorts[index].regs;
	}

protected:
	s8 index_;
	ADC_TypeDef *regs_;
};

class Adc : public AdcPort {
public:
	Adc(s8 index, u32 trigger);
	void Init(const AdcChannel *channels, u32 count, u8 preemption, u8 sub, u32 priority);

	/* the conversions into buf, size halfwords round and round */
	void Start(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
#if MOTION_IMAGE != 0
		dma_.Stop();
#endif
		dma_.Start(buf, size, func, arg);
	}

	/* the function the next half is reported to */
	void SetDone(Request::Done::Func func, void *arg)
	{
		dma_.SetDone(func, arg);
	}

	void PowerDown()
	{
		dma_.Stop();
		ADC_Cmd(regs_, DISABLE);
		ADC_DMACmd(regs_, DISABLE);
	}

	void PowerUp()
	{
		ADC_DMACmd(regs_, ENABLE);
		ADC_Cmd(regs_, ENABLE);
	}

	static void EnableTempSensor()
	{
		ADC_TempSensorVrefintCmd(ENABLE);
	}

private:
	void Calibrate();
	void Configure(const AdcChannel *channels, u32 count);
	void InitDma(u8 preemption, u8 sub, u32 priority);

	DmaCirc dma_;
	u32 trigger_;		/* ADC_ExternalTrigConv_* */
};

#endif
