/*
 * I2cMemIf: the I2C bus to the sensors, with DMA: a register of a device read or
 * written, a byte or a halfword at a time, the end reported to a callback.
 */
#ifndef I2CMEMIF_H
#define I2CMEMIF_H

#include "stm32f10x_type.h"
#include "motion.h"
#include "Callback.h"

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

class I2cMemIf {
public:
	typedef Callback<void (s32)> Done;

	void WriteByte(u8 reg, u8 value, Done::Func func, void *arg);
	void ReadByte(u8 reg, Done::Func func, void *arg);
	void ReadHalfword(u8 reg, Done::Func func, void *arg);
	void GetStatus(I2cStatus *status);
	u8 ByteData();
	u16 HalfwordData();
	void Enable();
	void Disable();
	void DmaCallbackSend();
	void DmaCallbackReceive();
	void IsrError();
};

#endif
