/*
 * I2cMemIf: a device on the I2C bus read and written like a memory.
 *
 * A write sends START, the device's address, then by DMA the register address and the
 * data, then STOP. A read sends the register address the same way, then a repeated
 * START and the address for reading, and receives the data by DMA (one byte without:
 * the DMA cannot NACK it, so it is taken in the RXNE interrupt). status_.state follows
 * the steps for the error report.
 */

#include <stdio.h>

#include "I2cMemIf.h"

/* the reasons a transfer failed (I2c::GetError's are 401-407) */
enum {
	I2CMEMIF_ERROR_SEND_REGISTER	= 408,	/* DMA, the register address */
	I2CMEMIF_ERROR_SEND		= 409,	/* DMA, the data written */
	I2CMEMIF_ERROR_SEND_OTHER	= 410,
	I2CMEMIF_ERROR_RECEIVE		= 411,	/* DMA, the data read */
};

I2cMemIf::I2cMemIf(s8 port, u8 address, u16 regSize)
	: I2c(port), address_(address), regSize_(regSize)
{
}

s32 I2cMemIf::Init(const I2C_InitTypeDef *init, u8 eventPreemption, u8 eventSub,
		   u8 errorPreemption, u8 errorSub, u32 txPriority, u32 rxPriority)
{
	I2c::Init(init, eventPreemption, eventSub, errorPreemption, errorSub, txPriority, rxPriority);
	status_.state = 0;
	return 0;
}

s32 I2cMemIf::Reinit(const I2C_InitTypeDef *init)
{
	I2c::Reinit(init);
	tx_.EnableIt();
	rx_.EnableIt();
	status_.state = 0;
	return 0;
}

void I2cMemIf::DeInit()
{
	tx_.DisableIt();
	rx_.DisableIt();
	tx_.DeInit();
	rx_.DeInit();
	tx_.Stop();
	rx_.Stop();
	DisableIt();
	I2c::DeInit();
	Disable();
}

void I2cMemIf::Start(void *buf, u16 size, Request::Done::Func func, void *arg, s8 write)
{
	status_.state = 1;
	request_.Set(buf, size, func, arg, write);
	current_ = &request_;
	SetStep(&I2cMemIf::OnStart);
	I2C_GenerateSTART(regs_, ENABLE);
	EnableEventIt();
}

/* START sent: the register address goes by DMA once the device's address is acked */
void I2cMemIf::OnStart(u32 event)
{
	status_.state = 2;
	if (event != I2C_EVENT_MASTER_MODE_SELECT)
		return;
	DmaSend(current_->buf_, regSize_, DmaSendCallback, this);
	SetStep(&I2cMemIf::OnRegisterSent);
	I2C_Send7bitAddress(regs_, address_, I2C_Direction_Transmitter);
}

void I2cMemIf::OnRegisterSent(u32 event)
{
	status_.state = 3;
	if (event != I2C_EVENT_MASTER_BYTE_TRANSMITTED)
		return;
	if (current_->write_) {
		SetStep(&I2cMemIf::OnSendEnd);
		DmaSend(current_->buf_ + regSize_, (u16)current_->size_, DmaSendCallback, this);
	} else {
		SetStep(&I2cMemIf::OnRestart);
		I2C_GenerateSTART(regs_, ENABLE);
	}
}

/* the repeated START of a read */
void I2cMemIf::OnRestart(u32 event)
{
	status_.state = 4;
	if (event != I2C_EVENT_MASTER_MODE_SELECT)
		return;
	if (current_->size_ == 1) {
		I2C_AcknowledgeConfig(regs_, DISABLE);
		stopSent_ = false;
		SetStep(&I2cMemIf::OnReceiveByte);
		EnableBufferIt();
	} else {
		SetStep(&I2cMemIf::OnDmaWait);
		I2C_DMALastTransferCmd(regs_, ENABLE);
		DmaReceive(current_->buf_ + regSize_, (u16)current_->size_, DmaReceiveCallback, this);
	}
	I2C_Send7bitAddress(regs_, address_, I2C_Direction_Receiver);
}

/* a read of one byte: STOP once the address is acked, then the byte */
void I2cMemIf::OnReceiveByte(u32 event)
{
	status_.state = 6;
	if (event == I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) {
		if (!stopSent_) {
			I2C_GenerateSTOP(regs_, ENABLE);
			stopSent_ = true;
		}
	} else if (event & I2C_FLAG_RXNE) {
		if (!stopSent_) {
			I2C_GenerateSTOP(regs_, ENABLE);
			stopSent_ = true;
		}
		u8 data = I2C_ReceiveData(regs_);

		current_->buf_[regSize_] = data;
		I2C_AcknowledgeConfig(regs_, ENABLE);
		DisableBufferIt();
		Finish(0);
	}
}

/* the DMA's end ends the read */
void I2cMemIf::OnDmaWait(u32 event)
{
}

void I2cMemIf::OnSendEnd(u32 event)
{
	status_.state = 5;
	if (event != I2C_EVENT_MASTER_BYTE_TRANSMITTED) {
		printf("I2C: (0x%08X) [SEND] ignore.\n", event);
		return;
	}
	I2C_GenerateSTOP(regs_, ENABLE);
	Finish(0);
}

void I2cMemIf::Finish(s32 result)
{
	DisableIt();
	status_.state = 0;
	current_->Finish(result);
}

/* the transfer and the bus's state kept for GetStatus, the bus reset */
void I2cMemIf::Fail(s32 result)
{
	DisableIt();
	status_.write = current_->write_;
	status_.size = current_->size_;
	status_.result = result;
	status_.sr1 = regs_->SR1;
	status_.sr2 = regs_->SR2;
	ClearStatus();
	tx_.Stop();
	rx_.Stop();
	Reset();
	current_->Finish(result);
}

void I2cMemIf::DmaSendCallback(void *arg, Request *request)
{
	static_cast<I2cMemIf *>(arg)->DmaCallbackSend(request);
}

void I2cMemIf::DmaCallbackSend(Request *request)
{
	s32 result;

	status_.state = 5;
	I2C_DMACmd(regs_, DISABLE);
	result = request->Result();
	if (result == 0)
		return;
	printf("I2cMemIf::DmaCallbackSend() error: %d.\n", result);
	switch (status_.state) {
	case 3:
		Fail(I2CMEMIF_ERROR_SEND_REGISTER);
		break;
	case 5:
		Fail(I2CMEMIF_ERROR_SEND);
		break;
	default:
		Fail(I2CMEMIF_ERROR_SEND_OTHER);
		break;
	}
}

void I2cMemIf::DmaReceiveCallback(void *arg, Request *request)
{
	static_cast<I2cMemIf *>(arg)->DmaCallbackReceive(request);
}

void I2cMemIf::DmaCallbackReceive(Request *request)
{
	s32 result;
	u16 sr1;

	status_.state = 7;
	I2C_DMACmd(regs_, DISABLE);
	I2C_GenerateSTOP(regs_, ENABLE);
	sr1 = regs_->SR1;
	(void)sr1;
	result = request->Result();
	if (result != 0) {
		printf("I2cMemIf::DmaCallbackReceive() error: %d.\n", result);
		Fail(I2CMEMIF_ERROR_RECEIVE);
	} else {
		Finish(0);
	}
}

void I2cMemIf::IsrError()
{
	printf("I2cMemIf::IsrError() SR1: %04X.\n", regs_->SR1);
	Fail(GetError());
}
