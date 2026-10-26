/*
 * AK8975: AKM's magnetometer, on the I2C bus: its fuse ROM, its measurements and its
 * self test.
 */
#ifndef AK8975_H
#define AK8975_H

#include "stm32f10x_type.h"
#include "Callback.h"
#include "Gpio.h"
#include "I2cRegIf.h"

/* what Boot reads of the device (DeviceInfo's magInfo) */
struct AK8975Info {
	u8 id[2];			/* WIA, INFO */
	u8 sensitivity[3];		/* the fuse ROM's ASAX, ASAY, ASAZ */
	u8 status;			/* AK8975_STATUS_* */
	s16 selfTest[3];		/* the self test's measurement */
};

/* AK8975Info's status */
enum {
	AK8975_STATUS_BOOTING		= 1 << 0,
	AK8975_STATUS_FAILED		= 1 << 1,
	AK8975_STATUS_SELF_TEST_ERROR	= 1 << 2,	/* ST2: data error or overflow */
	AK8975_STATUS_SELF_TEST_RANGE	= 1 << 3,	/* outside the data sheet's range */
};

class AK8975 {
public:
	typedef Callback<void (s32)> Done;
	typedef Callback<void (s32, u8 *)> Measured;

	AK8975(I2cRegIf *i2c, ExtiHandler *drdy);
	void Boot(u8 *info, Done::Func func, void *arg);
	void Measure(u8 *data, Measured::Func func, void *arg);
#if MOTION_IMAGE != 0
	void AbortBoot();
	void AbortMeasure();

	void Enable()
	{
		i2c_->Enable();
	}

	void Disable()
	{
		i2c_->Disable();
	}
#endif

private:
	typedef void (AK8975::*Step)(s32 result);

	void OnPowerDown(s32 result);
	void OnReadId(s32 result);
	void OnFuseRomMode(s32 result);
	void OnReadSensitivity(s32 result);
	void OnFuseRomExit(s32 result);
	void FinishBoot(s32 result);
	void OnMeasureStart(s32 result);
	void OnSelfTestStart(s32 result);
	static void SelfTestReadyCallback(void *arg, s8 level);
	void OnSelfTestReady(s8 level);
	void OnReadSelfTest(s32 result);
	void OnReadSelfTestStatus(s32 result);
	void OnSelfTestEnd(s32 result);
	bool CheckSelfTest();
	static void MeasureReadyCallback(void *arg, s8 level);
	void OnMeasureReady(s8 level);
	void OnReadMeasurement(s32 result);
	void OnReadMeasureStatus(s32 result);
	void DumpStatus();
	static void StepCallback(void *arg, s32 result);
	void WriteByte(u8 reg, u8 value, Step step);
	void ReadByte(u8 reg, Step step);
	void SetTransfer(const u8 *regs, u8 *buffer, u32 count, Step step);
	void WriteBytes(const u8 *regs, const u8 *values, u32 count, Step step);
	static void WriteCallback(void *arg, s32 result);
	void OnWrite(s32 result);
	void ReadBytes(const u8 *regs, u8 *buffer, u32 count, Step step);
	static void ReadCallback(void *arg, s32 result);
	void OnRead(s32 result);
	void ReadHalfwords(const u8 *regs, u8 *buffer, u32 count, Step step);
	static void ReadHalfwordCallback(void *arg, s32 result);
	void OnReadHalfword(s32 result);
	void SetError(u8 flags);

	void SetBooting()
	{
		info_->status = AK8975_STATUS_BOOTING;
	}

	void ClearBooting()
	{
		info_->status &= ~AK8975_STATUS_BOOTING;
	}

	u8 ByteData()
	{
		return i2c_->ByteData();
	}

	void Next(s32 result)
	{
		(this->*step_)(result);
	}

#if MOTION_IMAGE != 0
	static void IgnoreDone(void *arg, s32 result) {}
	static void IgnoreMeasured(void *arg, s32 result, u8 *data) {}
#endif

	I2cRegIf *i2c_;
	ExtiHandler *drdy_;
	u32 reserved_;
	u8 *data_;			/* Measure's: ST2 >> 2, then HX, HY, HZ */
	Done done_;
	Measured measured_;
	u32 reserved2_[2];
	AK8975Info *info_;
	s8 state_;			/* the boot's step, for DumpStatus */
	s32 result_;
	Step step_;			/* called when the bus operation ends */
	u32 reserved3_[2];
	const u8 *regs_;		/* a transfer of several registers */
	u8 *buffer_;
	u32 count_;
};

#endif
