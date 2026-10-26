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
#include "stm32f10x_it.h"

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

	/* the function the request reports to, the transfer left as it is */
	void SetDone(Request::Done::Func func, void *arg)
	{
		request_.done_.Set(func, arg);
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
	void Configure(const DMA_InitTypeDef *init, u32 priority, u32 mode);
	void RegisterIrq(IrqHandler::Func func, void *arg, u8 preemption, u8 sub);

	/* the request set and the channel on: the transfer runs */
	void Run(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		request_.Set(buf, size, func, arg);
		DMA_Cmd(regs_, ENABLE);
	}

	/* the bytes of each transfer (PSIZE) */
	u32 ElementSize()
	{
		return 1 << ((regs_->CCR & 0x300) >> 8);
	}

	s8 channel_;
	DMA_Channel_TypeDef *regs_;
	Request request_;
	const DmaChannelInfo *info_;
};

/* a buffer once: the request completes at its end */
class DmaNormal : public Dma {
public:
	DmaNormal(s8 channel) : Dma(channel) {}
	void Init(const DMA_InitTypeDef *init, u8 preemption, u8 sub, u32 priority);

private:
	static void IsrCallback(void *arg);
	void Isr();
};

/* a buffer round and round: the request reports each half as it fills */
class DmaCirc : public DmaNormal {
public:
	DmaCirc(s8 channel) : DmaNormal(channel) {}
	void Init(const DMA_InitTypeDef *init, u8 preemption, u8 sub, u32 priority);
	void Start(void *buf, u16 size, Request::Done::Func func, void *arg);

	/* the half and whole transfer interrupts too */
	void EnableIt()
	{
		DMA_ITConfig(regs_, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE, ENABLE);
	}

	void Stop()
	{
		Halt();
	}

private:
	void Halt()
	{
		Dma::Stop();
	}

	static void IsrCallback(void *arg);
	void Isr();

	u8 *half_;		/* the second half */
};

#endif
