/*
 * I2c: an I2C port reset and set up, its event and error interrupts calling the
 * object's methods, its DMA channels moving a byte at a time to and from the data
 * register.
 */

#include "I2c.h"
#include "stm32f10x_it.h"

/* the errors SR1 reports, in the order they are looked for */
enum {
	I2C_ERROR_BUS		= 401,	/* BERR */
	I2C_ERROR_ARBITRATION	= 402,	/* ARLO */
	I2C_ERROR_ACK		= 403,	/* AF */
	I2C_ERROR_OVERRUN	= 404,	/* OVR */
	I2C_ERROR_PEC		= 405,	/* PECERR */
	I2C_ERROR_TIMEOUT	= 406,	/* TIMEOUT */
	I2C_ERROR_SMBALERT	= 407,	/* SMBALERT */
};

const I2cPortInfo i2cPorts[2] = {
	{ I2C1, I2C1_EV_IRQChannel + 16, I2C1_ER_IRQChannel + 16 },
	{ I2C2, I2C2_EV_IRQChannel + 16, I2C2_ER_IRQChannel + 16 },
};

const s8 i2cDmaChannels[2][2] = {
	{ 5, 6 },	/* DMA1_Channel6, DMA1_Channel7 */
	{ 3, 4 },	/* DMA1_Channel4, DMA1_Channel5 */
};

/* the error SR1 shows, or -1 */
s32 I2c::GetError()
{
	u16 sr1 = regs_->SR1;

	if (sr1 & I2C_SR1_BERR)
		return I2C_ERROR_BUS;
	if (sr1 & I2C_SR1_ARLO)
		return I2C_ERROR_ARBITRATION;
	if (sr1 & I2C_SR1_AF)
		return I2C_ERROR_ACK;
	if (sr1 & I2C_SR1_OVR)
		return I2C_ERROR_OVERRUN;
	if (sr1 & I2C_SR1_PECERR)
		return I2C_ERROR_PEC;
	if (sr1 & I2C_SR1_TIMEOUT)
		return I2C_ERROR_TIMEOUT;
	if (sr1 & I2C_SR1_SMBALERT)
		return I2C_ERROR_SMBALERT;
	return -1;
}

void I2c::InitPort(const I2C_InitTypeDef *init, u8 eventPreemption, u8 eventSub,
		   u8 errorPreemption, u8 errorSub)
{
	I2C_SoftwareResetCmd(regs_, ENABLE);
	I2C_SoftwareResetCmd(regs_, DISABLE);
	I2C_Init(regs_, (I2C_InitTypeDef *)init);
	RegisterIrqHandler(i2cPorts[port_].eventIrq, IsrEventCallback, this, eventPreemption, eventSub);
	RegisterIrqHandler(i2cPorts[port_].errorIrq, IsrErrorCallback, this, errorPreemption, errorSub);
	I2C_Cmd(regs_, ENABLE);
}

#if MOTION_IMAGE != 0
void I2c::ReinitPort(const I2C_InitTypeDef *init)
{
	I2C_Cmd(regs_, DISABLE);
	I2C_SoftwareResetCmd(regs_, ENABLE);
	I2C_SoftwareResetCmd(regs_, DISABLE);
	I2C_Init(regs_, (I2C_InitTypeDef *)init);
	I2C_Cmd(regs_, ENABLE);
}

/* the port's interrupts' pending bits cleared */
void I2c::DeInit()
{
	switch (port_) {
	case 0:
		ClearPendingIrq(I2C1_EV_IRQChannel);
		ClearPendingIrq(I2C1_ER_IRQChannel);
		break;
	case 1:
		ClearPendingIrq(I2C2_EV_IRQChannel);
		ClearPendingIrq(I2C2_ER_IRQChannel);
		break;
	}
}
#endif

void I2c::IsrEventCallback(void *arg)
{
	static_cast<I2c *>(arg)->IsrEvent();
}

void I2c::IsrErrorCallback(void *arg)
{
	static_cast<I2c *>(arg)->IsrError();
}

/* the DMA's interrupts at the error interrupt's priority */
void I2c::Init(const I2C_InitTypeDef *init, u8 eventPreemption, u8 eventSub,
	       u8 errorPreemption, u8 errorSub, u32 txPriority, u32 rxPriority)
{
	InitPort(init, eventPreemption, eventSub, errorPreemption, errorSub);
	InitDma(errorPreemption, errorSub, txPriority, rxPriority);
}

#if MOTION_IMAGE != 0
void I2c::Reinit(const I2C_InitTypeDef *init)
{
	ReinitPort(init);
}
#endif

/* both channels move bytes between the data register and memory, one at a time */
void I2c::InitDma(u8 preemption, u8 sub, u32 txPriority, u32 rxPriority)
{
	DMA_InitTypeDef init;

	init.DMA_PeripheralBaseAddr = (u32)&regs_->DR;
	init.DMA_MemoryBaseAddr = 0;
	init.DMA_DIR = DMA_DIR_PeripheralDST;
	init.DMA_BufferSize = 1;
	init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	init.DMA_MemoryInc = DMA_MemoryInc_Enable;
	init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	init.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	init.DMA_Mode = DMA_Mode_Normal;
	init.DMA_Priority = txPriority;
	init.DMA_M2M = DMA_M2M_Disable;
	tx_.Init(&init, preemption, sub, txPriority);
	init.DMA_DIR = DMA_DIR_PeripheralSRC;
	init.DMA_Priority = rxPriority;
	rx_.Init(&init, preemption, sub, rxPriority);
}
