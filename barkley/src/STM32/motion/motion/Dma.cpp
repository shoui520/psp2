/*
 * Dma: a channel of DMA1. A transfer sets the memory address and count, the request,
 * and turns the channel on; the interrupt reports the end (DmaNormal) or each half of
 * the buffer (DmaCirc) to the request.
 */

#include <stdio.h>

#include "Dma.h"

const DmaChannelInfo dmaChannels[7] = {
	{ DMA1_Channel1, DMA1_Channel1_IRQChannel + 16, DMA1_IT_TC1, DMA1_IT_HT1, DMA1_IT_TE1 },
	{ DMA1_Channel2, DMA1_Channel2_IRQChannel + 16, DMA1_IT_TC2, DMA1_IT_HT2, DMA1_IT_TE2 },
	{ DMA1_Channel3, DMA1_Channel3_IRQChannel + 16, DMA1_IT_TC3, DMA1_IT_HT3, DMA1_IT_TE3 },
	{ DMA1_Channel4, DMA1_Channel4_IRQChannel + 16, DMA1_IT_TC4, DMA1_IT_HT4, DMA1_IT_TE4 },
	{ DMA1_Channel5, DMA1_Channel5_IRQChannel + 16, DMA1_IT_TC5, DMA1_IT_HT5, DMA1_IT_TE5 },
	{ DMA1_Channel6, DMA1_Channel6_IRQChannel + 16, DMA1_IT_TC6, DMA1_IT_HT6, DMA1_IT_TE6 },
	{ DMA1_Channel7, DMA1_Channel7_IRQChannel + 16, DMA1_IT_TC7, DMA1_IT_HT7, DMA1_IT_TE7 },
};

Dma::Dma(s8 channel)
	: channel_(channel), regs_(dmaChannels[channel].regs), info_(&dmaChannels[channel])
{
}

void DmaNormal::Init(const DMA_InitTypeDef *init, u8 preemption, u8 sub, u32 priority)
{
	Configure(init, priority, DMA_Mode_Normal);
	RegisterIrq(IsrCallback, this, preemption, sub);
	EnableIt();
}

/* the caller's settings, at the priority and in the mode given */
void Dma::Configure(const DMA_InitTypeDef *init, u32 priority, u32 mode)
{
	DMA_InitTypeDef config = *init;

	config.DMA_Priority = priority;
	config.DMA_Mode = mode;
	DMA_Init(regs_, &config);
}

void Dma::RegisterIrq(IrqHandler::Func func, void *arg, u8 preemption, u8 sub)
{
	RegisterIrqHandler(info_->irq, func, arg, preemption, sub);
}

void DmaNormal::IsrCallback(void *arg)
{
	static_cast<DmaNormal *>(arg)->Isr();
}

/* the channel off: an error, the end, or neither (what is left reported with either
   failure) */
void DmaNormal::Isr()
{
	Disable();
	if (DMA_GetITStatus(info_->te) != RESET) {
		DMA_ClearITPendingBit(info_->te);
		request_.Complete(DMA_ERROR_TRANSFER, Remaining());
	} else if (DMA_GetITStatus(info_->tc) != RESET) {
		DMA_ClearITPendingBit(info_->tc);
		request_.Complete(0);
	} else {
		request_.Complete(-1, Remaining());
	}
}

/* the channel's interrupt and flags cleared */
void Dma::DeInit()
{
	switch (channel_) {
	case 0:
		ClearPendingIrq(DMA1_Channel1_IRQChannel);
		break;
	case 1:
		ClearPendingIrq(DMA1_Channel2_IRQChannel);
		break;
	case 2:
		ClearPendingIrq(DMA1_Channel3_IRQChannel);
		break;
	case 3:
		ClearPendingIrq(DMA1_Channel4_IRQChannel);
		break;
	case 4:
		ClearPendingIrq(DMA1_Channel5_IRQChannel);
		break;
	case 5:
		ClearPendingIrq(DMA1_Channel6_IRQChannel);
		break;
	case 6:
		ClearPendingIrq(DMA1_Channel7_IRQChannel);
		break;
	}
	DMA_ClearITPendingBit(dmaChannels[channel_].tc);
	DMA_ClearITPendingBit(dmaChannels[channel_].ht);
	DMA_ClearITPendingBit(dmaChannels[channel_].te);
}

void Dma::Start(void *buf, u16 size, Request::Done::Func func, void *arg)
{
	regs_->CMAR = (u32)buf;
	regs_->CNDTR = size;
	Run(buf, size, func, arg);
}

void DmaCirc::Init(const DMA_InitTypeDef *init, u8 preemption, u8 sub, u32 priority)
{
	Configure(init, priority, DMA_Mode_Circular);
	RegisterIrq(IsrCallback, this, preemption, sub);
	EnableIt();
}

void DmaCirc::Start(void *buf, u16 size, Request::Done::Func func, void *arg)
{
	half_ = (u8 *)buf + size / 2 * ElementSize();
	Dma::Start(buf, size, func, arg);
}

void DmaCirc::IsrCallback(void *arg)
{
	static_cast<DmaCirc *>(arg)->Isr();
}

/* an error stops the channel; otherwise the half just filled is reported */
void DmaCirc::Isr()
{
	if (DMA_GetITStatus(info_->te) != RESET) {
		printf("DmaCirc[%d]::Isr() TE.\n", channel_);
		DMA_ClearITPendingBit(info_->te | info_->ht | info_->tc);
		Disable();
		request_.Complete(DMA_ERROR_TRANSFER);
		return;
	}
	if (DMA_GetITStatus(info_->ht) != RESET) {
		DMA_ClearITPendingBit(info_->ht);
		request_.Report(0, request_.buf_);
	}
	if (DMA_GetITStatus(info_->tc) != RESET) {
		DMA_ClearITPendingBit(info_->tc);
		request_.Report(0, half_);
	}
}
