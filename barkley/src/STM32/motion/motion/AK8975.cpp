/*
 * AK8975: AKM's magnetometer, on the I2C bus.
 *
 * Everything runs as a chain of steps, each started by the end of the bus operation
 * before it (step_). Boot powers the device down, reads its id and, in fuse ROM mode,
 * its sensitivity adjustments, then runs the self test (the internal magnetic field,
 * measured once DRDY rises) and checks the result against the data sheet's range.
 * Measure starts a single measurement and reads it when DRDY rises.
 */

#include <stdio.h>

#include "AK8975.h"

/* the registers */
enum {
	AK8975_WIA	= 0x00,
	AK8975_INFO	= 0x01,
	AK8975_HXL	= 0x03,
	AK8975_HYL	= 0x05,
	AK8975_HZL	= 0x07,
	AK8975_ST2	= 0x09,
	AK8975_CNTL	= 0x0A,
	AK8975_ASTC	= 0x0C,
	AK8975_ASAX	= 0x10,
	AK8975_ASAY	= 0x11,
	AK8975_ASAZ	= 0x12,
};

/* CNTL's modes */
enum {
	AK8975_MODE_POWER_DOWN	= 0x00,
	AK8975_MODE_SINGLE	= 0x01,
	AK8975_MODE_SELF_TEST	= 0x08,
	AK8975_MODE_FUSE_ROM	= 0xFF,
};

/* the three axes' measurement */
static const u8 measurementRegs[3] = { AK8975_HXL, AK8975_HYL, AK8975_HZL };
static const u8 sensitivityRegs[3] = { AK8975_ASAX, AK8975_ASAY, AK8975_ASAZ };

/* the self test's range per axis (the data sheet's, at 13 bits) */
static const struct {
	s16 min;
	s16 max;
} selfTestRange[3] = {
	{ -100, 100 },
	{ -100, 100 },
	{ -1000, -300 },
};

AK8975::AK8975(I2cMemIf *i2c, ExtiHandler *drdy)
	: i2c_(i2c), drdy_(drdy), state_(0)
{
}

void AK8975::Boot(u8 *info, Done::Func func, void *arg)
{
	static const u8 regs[2] = { AK8975_ASTC, AK8975_CNTL };
	static const u8 values[2] = { 0, AK8975_MODE_POWER_DOWN };

	state_ = 1;
	info_ = (AK8975Info *)info;
	done_.Set(func, arg);
	drdy_->Disable();
	SetBooting();
	WriteBytes(regs, values, 2, &AK8975::OnPowerDown);
}

void AK8975::OnPowerDown(s32 result)
{
	static const u8 regs[2] = { AK8975_WIA, AK8975_INFO };

	state_ = 2;
	if (result != 0)
		FinishBoot(result);
	else
		ReadBytes(regs, info_->id, 2, &AK8975::OnReadId);
}

void AK8975::OnReadId(s32 result)
{
	state_ = 3;
	if (result != 0)
		FinishBoot(result);
	else
		WriteByte(AK8975_CNTL, AK8975_MODE_FUSE_ROM, &AK8975::OnFuseRomMode);
}

void AK8975::OnFuseRomMode(s32 result)
{
	state_ = 4;
	if (result != 0)
		FinishBoot(result);
	else
		ReadBytes(sensitivityRegs, info_->sensitivity, 3, &AK8975::OnReadSensitivity);
}

void AK8975::OnReadSensitivity(s32 result)
{
	state_ = 5;
	if (result != 0)
		FinishBoot(result);
	else
		WriteByte(AK8975_CNTL, AK8975_MODE_POWER_DOWN, &AK8975::OnFuseRomExit);
}

void AK8975::OnFuseRomExit(s32 result)
{
	static const u8 regs[2] = { AK8975_ASTC, AK8975_CNTL };
	static const u8 values[2] = { 0x40, AK8975_MODE_SELF_TEST };	/* ASTC: SELF */

	state_ = 6;
	if (result != 0) {
		FinishBoot(result);
	} else {
		drdy_->ClearPending();
		drdy_->Start(1, SelfTestReadyCallback, this);
		WriteBytes(regs, values, 2, &AK8975::OnSelfTestStart);
	}
}

void AK8975::FinishBoot(s32 result)
{
	result_ = result;
	drdy_->Disable();
	ClearBooting();
	if (result == 0) {
		drdy_->ClearPending();
		drdy_->Start(1, MeasureReadyCallback, this);
	} else {
		SetError(AK8975_STATUS_FAILED);
		DumpStatus();
	}
	done_(result);
}

void AK8975::Measure(u8 *data, Measured::Func func, void *arg)
{
	data_ = data;
	measured_.Set(func, arg);
	drdy_->ClearPending();
	drdy_->Start(1, MeasureReadyCallback, this);
	WriteByte(AK8975_CNTL, AK8975_MODE_SINGLE, &AK8975::OnMeasureStart);
}

void AK8975::OnMeasureStart(s32 result)
{
	if (result != 0) {
		drdy_->Disable();
		measured_(result, 0);
	}
}

void AK8975::OnSelfTestStart(s32 result)
{
	state_ = 7;
	if (result != 0)
		FinishBoot(result);
}

void AK8975::SelfTestReadyCallback(void *arg, s8 level)
{
	static_cast<AK8975 *>(arg)->OnSelfTestReady(level);
}

void AK8975::OnSelfTestReady(s8 level)
{
	state_ = 8;
	ReadHalfwords(measurementRegs, (u8 *)info_->selfTest, 3, &AK8975::OnReadSelfTest);
}

void AK8975::OnReadSelfTest(s32 result)
{
	state_ = 9;
	if (result != 0)
		FinishBoot(result);
	else
		ReadByte(AK8975_ST2, &AK8975::OnReadSelfTestStatus);
}

void AK8975::OnReadSelfTestStatus(s32 result)
{
	static const u8 regs[2] = { AK8975_ASTC, AK8975_CNTL };
	static const u8 values[2] = { 0, AK8975_MODE_POWER_DOWN };

	state_ = 10;
	if (result != 0) {
		FinishBoot(result);
		return;
	}
	if (ByteData() != 0)
		SetError(AK8975_STATUS_SELF_TEST_ERROR);
	else if (!CheckSelfTest())
		SetError(AK8975_STATUS_SELF_TEST_RANGE);
	WriteBytes(regs, values, 2, &AK8975::OnSelfTestEnd);
}

void AK8975::OnSelfTestEnd(s32 result)
{
	state_ = 11;
	if (result != 0)
		FinishBoot(result);
	else
		FinishBoot(0);
}

/* the self test's values, adjusted by the fuse ROM's sensitivity, in the range */
bool AK8975::CheckSelfTest()
{
	s16 value[3];
	s16 adjustment[3];
	bool result = true;

	for (u8 i = 0; i < 3; i++) {
		adjustment[i] = (info_->sensitivity[i] - 128) * 5000 / 128 + 10000;
		value[i] = info_->selfTest[i] * adjustment[i] / 10000;
		if (value[i] < selfTestRange[i].min || selfTestRange[i].max < value[i])
			result = false;
	}
	return result;
}

void AK8975::MeasureReadyCallback(void *arg, s8 level)
{
	static_cast<AK8975 *>(arg)->OnMeasureReady(level);
}

void AK8975::OnMeasureReady(s8 level)
{
	ReadHalfwords(measurementRegs, data_ + 1, 3, &AK8975::OnReadMeasurement);
}

void AK8975::OnReadMeasurement(s32 result)
{
	if (result != 0) {
		drdy_->Disable();
		measured_(result, 0);
	} else {
		ReadByte(AK8975_ST2, &AK8975::OnReadMeasureStatus);
	}
}

void AK8975::OnReadMeasureStatus(s32 result)
{
	if (result != 0) {
		drdy_->Disable();
		measured_(result, 0);
	} else {
		u8 status = ByteData() >> 2;

		*data_ = status;
		drdy_->Disable();
		measured_(0, data_);
	}
}

void AK8975::DumpStatus()
{
	I2cStatus status;

	i2c_->GetStatus(&status);
	printf("------- AK8975 Error Status -------\n");
	printf("Internal State : %d\n", state_);
	printf("Last Result    : %d\n", result_);
	printf("Reg Addr       : 0x%02X(%u)\n", status.reg, status.reg);
	printf("R(0)/W(1)      : %d\n", status.write);
	printf("Data Size      : %d\n", status.size);
	if (status.size == 1)
		printf("Byte Data      : 0x%02X(%u|%d)\n", status.data.byte, status.data.byte,
		       (s8)status.data.byte);
	else
		printf("Halfword Data  : 0x%04X(%u|%d)\n", status.data.halfword, status.data.halfword,
		       (s16)status.data.halfword);
	printf("I2C State      : %d\n", status.state);
	printf("I2C Result     : %d\n", status.result);
	printf("I2C SR1        : 0x%04X\n", status.sr1);
	printf("I2C SR2        : 0x%04X\n", status.sr2);
	printf("-----------------------------------\n");
}

void AK8975::StepCallback(void *arg, s32 result)
{
	static_cast<AK8975 *>(arg)->Next(result);
}

void AK8975::WriteByte(u8 reg, u8 value, Step step)
{
	step_ = step;
	i2c_->WriteByte(reg, value, StepCallback, this);
}

void AK8975::ReadByte(u8 reg, Step step)
{
	step_ = step;
	i2c_->ReadByte(reg, StepCallback, this);
}

void AK8975::SetTransfer(const u8 *regs, u8 *buffer, u32 count, Step step)
{
	regs_ = regs;
	buffer_ = buffer;
	count_ = count;
	step_ = step;
}

void AK8975::WriteBytes(const u8 *regs, const u8 *values, u32 count, Step step)
{
	SetTransfer(regs, (u8 *)values, count, step);
	i2c_->WriteByte(*regs, *values, WriteCallback, this);
}

void AK8975::WriteCallback(void *arg, s32 result)
{
	static_cast<AK8975 *>(arg)->OnWrite(result);
}

void AK8975::OnWrite(s32 result)
{
	if (result != 0) {
		(this->*step_)(result);
		return;
	}
	if (--count_ == 0) {
		(this->*step_)(result);
		return;
	}
	regs_++;
	buffer_++;
	i2c_->WriteByte(*regs_, *buffer_, WriteCallback, this);
}

void AK8975::ReadBytes(const u8 *regs, u8 *buffer, u32 count, Step step)
{
	SetTransfer(regs, buffer, count, step);
	i2c_->ReadByte(*regs, ReadCallback, this);
}

void AK8975::ReadCallback(void *arg, s32 result)
{
	static_cast<AK8975 *>(arg)->OnRead(result);
}

void AK8975::OnRead(s32 result)
{
	if (result != 0) {
		(this->*step_)(result);
		return;
	}
	u8 data = i2c_->ByteData();

	*buffer_++ = data;
	if (--count_ != 0) {
		regs_++;
		i2c_->ReadByte(*regs_, ReadCallback, this);
		return;
	}
	(this->*step_)(result);
}

void AK8975::ReadHalfwords(const u8 *regs, u8 *buffer, u32 count, Step step)
{
	SetTransfer(regs, buffer, count, step);
	i2c_->ReadHalfword(*regs, ReadHalfwordCallback, this);
}

void AK8975::ReadHalfwordCallback(void *arg, s32 result)
{
	static_cast<AK8975 *>(arg)->OnReadHalfword(result);
}

void AK8975::OnReadHalfword(s32 result)
{
	if (result != 0) {
		(this->*step_)(result);
		return;
	}
	u16 data = i2c_->HalfwordData();

	*(u16 *)buffer_ = data;
	buffer_ += 2;
	if (--count_ != 0) {
		regs_++;
		i2c_->ReadHalfword(*regs_, ReadHalfwordCallback, this);
		return;
	}
	(this->*step_)(result);
}

void AK8975::SetError(u8 flags)
{
	info_->status |= flags;
}

void AK8975::AbortBoot()
{
	drdy_->Disable();
	drdy_->ClearPending();
	done_.Set(IgnoreDone, this);
}

void AK8975::AbortMeasure()
{
	drdy_->Disable();
	drdy_->ClearPending();
	measured_.Set(IgnoreMeasured, this);
}
