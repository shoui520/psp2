/*
 * I2cRegIf: a register transfer sends the register's address and the data from one
 * buffer, or sends the address and receives the data after it; the end is reported
 * with the request's result.
 */

#include "I2cRegIf.h"

const I2C_InitTypeDef i2cRegIfInit = {
	I2C_Mode_I2C,
	I2C_DutyCycle_2,
	0,
	I2C_Ack_Enable,
	I2C_AcknowledgedAddress_7bit,
	380000,
};

void I2cRegIf::WriteByte(u8 reg, u8 value, Done::Func func, void *arg)
{
	done_.Set(func, arg);
	packet_.Set(reg, value);
	Write(packet_.Buffer(), 1, WriteCallback, this);
}

void I2cRegIf::ReadByte(u8 reg, Done::Func func, void *arg)
{
	done_.Set(func, arg);
	packet_.SetReg(reg);
	Read(packet_.Buffer(), 1, ReadCallback, this);
}

void I2cRegIf::ReadHalfword(u8 reg, Done::Func func, void *arg)
{
	done_.Set(func, arg);
	packet_.SetReg(reg);
	Read(packet_.Buffer(), 2, ReadCallback, this);
}

/* the last failed transfer: its register, direction, size and data, and the bus's state */
void I2cRegIf::GetStatus(I2cStatus *status)
{
	const I2cMemIfStatus *last = Status();

	status->reg = packet_.Reg();
	status->write = last->write;
	status->size = last->size;
	if (last->size == 1)
		status->data.byte = packet_.Byte();
	else
		status->data.halfword = packet_.Halfword();
	status->state = last->state;
	status->result = last->result;
	status->sr1 = last->sr1;
	status->sr2 = last->sr2;
}

void I2cRegIf::WriteCallback(void *arg, Request *request)
{
	static_cast<I2cRegIf *>(arg)->OnWrite(request);
}

void I2cRegIf::OnWrite(Request *request)
{
	done_(request->Result());
}

void I2cRegIf::ReadCallback(void *arg, Request *request)
{
	static_cast<I2cRegIf *>(arg)->OnRead(request);
}

void I2cRegIf::OnRead(Request *request)
{
	done_(request->Result());
}
