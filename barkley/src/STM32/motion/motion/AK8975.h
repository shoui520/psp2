/*
 * AK8975: AKM's magnetometer, on the I2C bus: its fuse ROM, its measurements and its
 * self test.
 */
#ifndef AK8975_H
#define AK8975_H

#include "stm32f10x_type.h"
#include "Callback.h"

class I2cMemIf;
class ExtiHandler;

class AK8975 {
public:
	typedef Callback<void (s32)> Done;
	typedef Callback<void (s32, u8 *)> Measured;

	AK8975(I2cMemIf *i2c, ExtiHandler *drdy);
	void Boot(u8 *info, Done::Func func, void *arg);
	void Measure(u8 *data, Measured::Func func, void *arg);
	void AbortBoot();
	void AbortMeasure();
	void Enable();
	void Disable();

private:
	I2cMemIf *i2c_;
	ExtiHandler *drdy_;
	u32 reserved_[2];
	Done done_;
	Measured measured_;
	u32 reserved2_[4];
};

#endif
