/*
 * Spi: an SPI port whose data the DMA moves, eight bits at a time; the interrupt
 * reports the port's errors.
 *
 * SpiSlave: the host is the master. A transfer is set up before the host selects the
 * port; the chip select's edge, the host done, ends it: the DMA channels' counts left
 * and the port's status say whether all went through.
 */

#include <stdio.h>

#include "Spi.h"
#include "stm32f10x_it.h"

/* the reasons a transfer failed */
enum {
	SPI_ERROR_SEND		= 501,	/* the transmit DMA's */
	SPI_ERROR_RECEIVE	= 502,	/* the receive DMA's */
	SPI_ERROR_INCOMPLETE	= 503,	/* data left, or a status other than TXE */
	SPI_ERROR_RXNE		= 504,	/* a byte received and not read */
	SPI_ERROR_TXE		= 505,
};

const SpiPortInfo spiPorts[2] = {
	{ SPI1, 2, 1, SPI1_IRQChannel + 16 },	/* DMA1_Channel3, DMA1_Channel2 */
	{ SPI2, 4, 3, SPI2_IRQChannel + 16 },	/* DMA1_Channel5, DMA1_Channel4 */
};

Spi::Spi(s8 port)
	: port_(port), regs_(spiPorts[port].regs), tx_(spiPorts[port].txDma), rx_(spiPorts[port].rxDma)
{
}

void Spi::Init(const SPI_InitTypeDef *init, u8 preemption, u8 sub, u32 txPriority, u32 rxPriority)
{
	init_ = init;
	SPI_Init(regs_, (SPI_InitTypeDef *)init);
	InitDma(preemption, sub, txPriority, rxPriority);
	RegisterIrqHandler(spiPorts[port_].irq, IsrCallback, this, preemption, sub);
	SPI_Cmd(regs_, ENABLE);
}

/* both channels move bytes between the data register and memory, one at a time */
void Spi::InitDma(u8 preemption, u8 sub, u32 txPriority, u32 rxPriority)
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

void Spi::Reset()
{
	SPI_I2S_DeInit(regs_);
	SPI_Init(regs_, (SPI_InitTypeDef *)init_);
	SPI_Cmd(regs_, ENABLE);
}

void Spi::DmaSendCallback(void *arg, Request *request)
{
	static_cast<Spi *>(arg)->DmaSendDone(request);
}

void Spi::DmaReceiveCallback(void *arg, Request *request)
{
	static_cast<Spi *>(arg)->DmaReceiveDone(request);
}

void Spi::IsrCallback(void *arg)
{
	static_cast<Spi *>(arg)->Isr();
}

void Spi::Isr()
{
	if (SPI_I2S_GetITStatus(regs_, SPI_I2S_IT_ERR) != RESET) {
		(void)regs_->SR;	/* Sony's: read once more than printed */
		printf("Spi::SpiIsr() ERROR:%04X.\n", regs_->SR);
	}
}

void SpiSlave::Init(const SPI_InitTypeDef *init, u8 preemption, u8 sub, u32 txPriority, u32 rxPriority)
{
	cs_->Disable();
	cs_->SetTrigger(0, CsCallback, this);
	Spi::Init(init, preemption, sub, txPriority, rxPriority);
}

/* the port off; the host's next chip select is let pass */
void SpiSlave::Stop()
{
	cs_->Disable();
	cs_->SetTrigger(0, StopCsCallback, this);
	cs_->Enable();
	Disable();
}

void SpiSlave::Restart()
{
	cs_->Disable();
	cs_->SetTrigger(0, CsCallback, this);
	Enable();
}

void SpiSlave::DmaSendDone(Request *request)
{
	s32 result;

	SPI_I2S_DMACmd(regs_, SPI_I2S_DMAReq_Tx, DISABLE);
	result = request->Result();
	if (result != 0) {
		printf("Spi Dma Send Error: %d.\n", result);
		error_ = SPI_ERROR_SEND;
	}
}

void SpiSlave::DmaReceiveDone(Request *request)
{
	s32 result;

	SPI_I2S_DMACmd(regs_, SPI_I2S_DMAReq_Rx, DISABLE);
	result = request->Result();
	if (result != 0) {
		printf("Spi Dma Recv Error: %d.\n", result);
		error_ = SPI_ERROR_RECEIVE;
	}
}

void SpiSlave::CsCallback(void *arg, s8 level)
{
	static_cast<SpiSlave *>(arg)->OnCs(level);
}

/* the host done: what the channels did not move, and the port's state, fail it */
void SpiSlave::OnCs(s8 level)
{
	cs_->Disable();
	txRequest_.remaining_ = tx_.Remaining();
	rxRequest_.remaining_ = rx_.Remaining();
	if (error_ == 0) {
		if (txRequest_.remaining_ != 0) {
			tx_.Abort();
			error_ = SPI_ERROR_INCOMPLETE;
		}
		if (rxRequest_.remaining_ != 0) {
			rx_.Abort();
			error_ = SPI_ERROR_INCOMPLETE;
		}
		u16 status = regs_->SR & 0xF3;	/* the flags but CHSIDE and UDR */

		if (status != SPI_I2S_FLAG_TXE) {
			if (status & SPI_I2S_FLAG_RXNE)
				error_ = SPI_ERROR_RXNE;
			else if (status & SPI_I2S_FLAG_TXE)
				error_ = SPI_ERROR_TXE;
			else
				error_ = SPI_ERROR_INCOMPLETE;
		}
	}
	if (error_ != 0)
		Reset();
	txRequest_.Complete(error_);
	rxRequest_.Complete(error_);
}

void SpiSlave::StopCsCallback(void *arg, s8 level)
{
	static_cast<SpiSlave *>(arg)->OnStopCs(level);
}

void SpiSlave::OnStopCs(s8 level)
{
	cs_->Disable();
}
