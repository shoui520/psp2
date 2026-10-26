/*
 * Adc: the channels converted in a scan on each external trigger, the results moved
 * by DMA, a halfword each; the ADC calibrated once it is on.
 */

#include "Adc.h"

const AdcPortInfo adcPorts[2] = {
	{ ADC1, 0, 0 },		/* DMA1_Channel1 */
	{ ADC2, 7, 1 },		/* none */
};

Adc::Adc(s8 index, u32 trigger)
	: AdcPort(index), dma_(adcPorts[index].dma), trigger_(trigger)
{
}

void Adc::Calibrate()
{
	ADC_ResetCalibration(regs_);
	while (ADC_GetResetCalibrationStatus(regs_) != RESET)
		;
	ADC_StartCalibration(regs_);
	while (ADC_GetCalibrationStatus(regs_) != RESET)
		;
}

void Adc::Init(const AdcChannel *channels, u32 count, u8 preemption, u8 sub, u32 priority)
{
	Configure(channels, count);
	InitDma(preemption, sub, priority);
}

/* a scan of the channels on each trigger, then the ADC on and calibrated */
void Adc::Configure(const AdcChannel *channels, u32 count)
{
	ADC_InitTypeDef init;

	init.ADC_Mode = ADC_Mode_Independent;
	init.ADC_ScanConvMode = ENABLE;
	init.ADC_ContinuousConvMode = DISABLE;
	init.ADC_ExternalTrigConv = trigger_;
	init.ADC_DataAlign = ADC_DataAlign_Right;
	init.ADC_NbrOfChannel = count;
	ADC_Init(regs_, &init);
	for (u32 i = 0; i < count; i++) {
		ADC_RegularChannelConfig(regs_, channels->channel, i + 1, channels->sampleTime);
		channels++;
	}
	ADC_ExternalTrigConvCmd(regs_, ENABLE);
	ADC_DMACmd(regs_, ENABLE);
	ADC_Cmd(regs_, ENABLE);
	Calibrate();
}

/* the data register's halfwords into memory; DmaCirc sets the mode, Start the buffer */
void Adc::InitDma(u8 preemption, u8 sub, u32 priority)
{
	DMA_InitTypeDef init;

	init.DMA_PeripheralBaseAddr = (u32)&regs_->DR;
	init.DMA_DIR = DMA_DIR_PeripheralSRC;
	init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	init.DMA_MemoryInc = DMA_MemoryInc_Enable;
	init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	init.DMA_Priority = priority;
	init.DMA_M2M = DMA_M2M_Disable;
	dma_.Init(&init, preemption, sub, priority);
}
