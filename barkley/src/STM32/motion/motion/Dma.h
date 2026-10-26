/*
 * Dma: a channel of DMA1, its interrupt and the request it runs; DmaNormal runs a
 * buffer once, DmaCirc round and round.
 */
#ifndef DMA_H
#define DMA_H

extern "C" {
#include "stm32f10x_lib.h"
}
#include "Request.h"

/* a channel's registers, interrupt and its flags in DMA1's ISR and IFCR */
struct DmaChannelInfo {
	DMA_Channel_TypeDef *regs;
	s8 irq;				/* the vector number */
	u32 tc;
	u32 ht;
	u32 te;
};

extern const DmaChannelInfo dmaChannels[7];

/* a request's results (Request::Result) */
enum {
	DMA_ERROR_TRANSFER	= 301,	/* the channel's transfer error */
	DMA_ERROR_ABORT		= 302,
};

class Dma {
public:
	Dma(s8 channel);
	void DeInit();
	void Start(void *buf, u16 size, Request::Done::Func func, void *arg);

	void EnableIt()
	{
		DMA_ITConfig(regs_, DMA_IT_TC | DMA_IT_TE, ENABLE);
	}

	void DisableIt()
	{
		DMA_ITConfig(regs_, DMA_IT_TC | DMA_IT_TE, DISABLE);
	}

	void Disable()
	{
		DMA_Cmd(regs_, DISABLE);
	}

	void Stop()
	{
		Disable();
	}

	/* the transfers left (CNDTR) */
	s16 Remaining()
	{
		return regs_->CNDTR;
	}

	/* stopped before its end: the request fails */
	void Abort()
	{
		Disable();
		request_.Complete(DMA_ERROR_ABORT);
	}

protected:
	s8 channel_;
	DMA_Channel_TypeDef *regs_;
	Request request_;
	const DmaChannelInfo *info_;
};

class DmaNormal : public Dma {
public:
	DmaNormal(s8 channel) : Dma(channel) {}
	void Init(const DMA_InitTypeDef *init, u8 preemption, u8 sub, u32 priority);
};

#endif
