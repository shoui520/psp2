/*
 * I2c: an I2C port, its interrupts (the event's and the error's, each a virtual
 * method) and the DMA channels that send and receive its data.
 */
#ifndef I2C_H
#define I2C_H

extern "C" {
#include "stm32f10x_lib.h"
}
#include "Dma.h"

/* a port's registers and interrupts (vector numbers) */
struct I2cPortInfo {
	I2C_TypeDef *regs;
	s8 eventIrq;
	s8 errorIrq;
};

extern const I2cPortInfo i2cPorts[2];

/* each port's DMA channels: transmit, receive */
extern const s8 i2cDmaChannels[2][2];

class I2cPort {
public:
	I2cPort(s8 port)
	{
		port_ = port;
		regs_ = i2cPorts[port].regs;
	}

	virtual void IsrEvent() {}
	virtual void IsrError() {}

protected:
	s8 port_;
	I2C_TypeDef *regs_;
};

class I2c : public I2cPort {
public:
	I2c(s8 port)
		: I2cPort(port), tx_(i2cDmaChannels[port][0]), rx_(i2cDmaChannels[port][1])
	{
	}

	void Init(const I2C_InitTypeDef *init, u8 eventPreemption, u8 eventSub,
		  u8 errorPreemption, u8 errorSub, u32 txPriority, u32 rxPriority);
	void Reinit(const I2C_InitTypeDef *init);
	void DeInit();
	s32 GetError();

	/* the event and error interrupts */
	void EnableEventIt()
	{
		I2C_ITConfig(regs_, I2C_IT_EVT | I2C_IT_ERR, ENABLE);
	}

	/* and the buffer's */
	void EnableBufferIt()
	{
		I2C_ITConfig(regs_, I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);
	}

	void DisableBufferIt()
	{
		I2C_ITConfig(regs_, I2C_IT_BUF, DISABLE);
	}

	void DisableIt()
	{
		I2C_ITConfig(regs_, I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, DISABLE);
	}

	void Disable()
	{
		I2C_Cmd(regs_, DISABLE);
	}

	void Reset()
	{
		I2C_Cmd(regs_, DISABLE);
		I2C_Cmd(regs_, ENABLE);
	}

	void ClearStatus()
	{
		regs_->SR1 = 0;
	}

	void DmaSend(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		I2C_DMACmd(regs_, ENABLE);
		tx_.Start(buf, size, func, arg);
	}

	void DmaReceive(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		I2C_DMACmd(regs_, ENABLE);
		rx_.Start(buf, size, func, arg);
	}

protected:
	DmaNormal tx_;
	DmaNormal rx_;
};

#endif
