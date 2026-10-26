/*
 * I2cRegIf: a device's registers over I2cMemIf, a byte or a halfword at a time, the
 * end reported to a callback with the result.
 */
#ifndef I2CREGIF_H
#define I2CREGIF_H

#include "I2cMemIf.h"

/* the transfer under way, and the bus's state, for an error report */
struct I2cStatus {
	u8 reg;
	s8 write;			/* R(0)/W(1) */
	u16 size;
	union {
		u8 byte;
		u16 halfword;
	} data;
	s8 state;
	s32 result;
	u16 sr1;
	u16 sr2;
};

/* a register transfer's buffer: the register's address, then its data */
class I2cRegPacket {
public:
	void Set(u8 reg, u8 value)
	{
		reg_ = reg;
		data_.byte = value;
	}

	void SetReg(u8 reg)
	{
		reg_ = reg;
	}

	u8 *Buffer()
	{
		return &reg_;
	}

	u8 Reg()
	{
		return reg_;
	}

	u8 Byte()
	{
		return data_.byte;
	}

	u16 Halfword()
	{
		return data_.halfword;
	}

	u8 reserved_;
	u8 reg_;
	union {
		u8 byte;
		u16 halfword;
	} data_;
};

/* the bus's set-up: 380 kHz, fast mode */
extern const I2C_InitTypeDef i2cRegIfInit;

class I2cRegIf : public I2cMemIf {
public:
	typedef Callback<void (s32)> Done;

	/* the AK8975 (address 0x0C), its registers' addresses a byte */
	I2cRegIf(s8 port) : I2cMemIf(port, 0x0C << 1, 1) {}

	s32 Init(u8 eventPreemption, u8 eventSub, u8 errorPreemption, u8 errorSub, u32 txPriority,
		 u32 rxPriority)
	{
		return I2cMemIf::Init(&i2cRegIfInit, eventPreemption, eventSub, errorPreemption,
				      errorSub, txPriority, rxPriority);
	}

	void WriteByte(u8 reg, u8 value, Done::Func func, void *arg);
	void ReadByte(u8 reg, Done::Func func, void *arg);
	void ReadHalfword(u8 reg, Done::Func func, void *arg);
	void GetStatus(I2cStatus *status);

	u8 ByteData()
	{
		return packet_.data_.byte;
	}

	u16 HalfwordData()
	{
		return packet_.data_.halfword;
	}

	/* the port set up again, after the STOP mode */
	void Enable()
	{
		Reinit(&i2cRegIfInit);
	}

	void Disable()
	{
		DeInit();
	}

private:
	static void WriteCallback(void *arg, Request *request);
	void OnWrite(Request *request);
	static void ReadCallback(void *arg, Request *request);
	void OnRead(Request *request);

	I2cRegPacket packet_;
	u32 reserved_;
	Done done_;
};

#endif
