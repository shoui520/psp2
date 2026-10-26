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

class I2cRegIf : public I2cMemIf {
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
};

#endif
