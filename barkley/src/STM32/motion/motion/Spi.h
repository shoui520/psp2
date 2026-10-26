/*
 * Spi: an SPI port, its error interrupt and the DMA channels that send and receive
 * its data, each transfer a request reported to its caller. SpiSlave: the port as the
 * host's slave, a transfer framed by the host's chip select (an EXTI line): its edge
 * ends the transfer.
 */
#ifndef SPI_H
#define SPI_H

extern "C" {
#include "stm32f10x_lib.h"
}
#include "Dma.h"
#include "Gpio.h"

/* a port's registers, DMA channels (transmit, receive) and interrupt (vector number) */
struct SpiPortInfo {
	SPI_TypeDef *regs;
	s8 txDma;
	s8 rxDma;
	s8 irq;
};

extern const SpiPortInfo spiPorts[2];

class Spi {
public:
	Spi(s8 port);
	void Init(const SPI_InitTypeDef *init, u8 preemption, u8 sub, u32 txPriority, u32 rxPriority);
	void Reset();

	void Send(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		txRequest_.Set(buf, size, func, arg);
		DmaSend(buf, size);
	}

	void Receive(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		rxRequest_.Set(buf, size, func, arg);
		DmaReceive(buf, size);
	}

	void Enable()
	{
		SPI_Cmd(regs_, ENABLE);
	}

	void Disable()
	{
		SPI_Cmd(regs_, DISABLE);
	}

protected:
	/* a DMA channel's end */
	virtual void DmaSendDone(Request *request) {}
	virtual void DmaReceiveDone(Request *request) {}

	s8 port_;
	SPI_TypeDef *regs_;
	DmaNormal tx_;
	DmaNormal rx_;
	Request txRequest_;
	Request rxRequest_;
	const SPI_InitTypeDef *init_;

private:
	void InitDma(u8 preemption, u8 sub, u32 txPriority, u32 rxPriority);

	void DmaSend(void *buf, u16 size)
	{
		SPI_I2S_DMACmd(regs_, SPI_I2S_DMAReq_Tx, ENABLE);
		tx_.Start(buf, size, DmaSendCallback, this);
	}

	void DmaReceive(void *buf, u16 size)
	{
		SPI_I2S_DMACmd(regs_, SPI_I2S_DMAReq_Rx, ENABLE);
		rx_.Start(buf, size, DmaReceiveCallback, this);
	}

	static void DmaSendCallback(void *arg, Request *request);
	static void DmaReceiveCallback(void *arg, Request *request);
	static void IsrCallback(void *arg);
	void Isr();
};

class SpiSlave : public Spi {
public:
	SpiSlave(s8 port, ExtiHandler *cs) : Spi(port)
	{
		cs_ = cs;
	}

	void Init(const SPI_InitTypeDef *init, u8 preemption, u8 sub, u32 txPriority, u32 rxPriority);
	void Stop();
	void Restart();

	/* a transfer: the data sent, while the host's chip select is awaited */
	void Send(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		error_ = 0;
		Spi::Send(buf, size, func, arg);
		cs_->Enable();
	}

private:
	virtual void DmaSendDone(Request *request);
	virtual void DmaReceiveDone(Request *request);
	static void CsCallback(void *arg, s8 level);
	void OnCs(s8 level);
	static void StopCsCallback(void *arg, s8 level);
	void OnStopCs(s8 level);

	ExtiHandler *cs_;
	s32 error_;
};

#endif
