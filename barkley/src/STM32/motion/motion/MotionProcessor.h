/*
 * MotionProcessor: the sensors' sampling and the frames made from it: the ADC's
 * accelerometer, gyroscope and temperature samples, the AK8975's magnetic field,
 * averaged or calibrated as the measurement type says, and the device information.
 */
#ifndef MOTIONPROCESSOR_H
#define MOTIONPROCESSOR_H

#include "stm32f10x_type.h"
#include "Average.h"
#include "Callback.h"
#include "Listener.h"
#include "MotionFrameUtil.h"
#include "Timer.h"

class AdcSampler;
class AK8975;
class Output;
class ExtiPin;
class ExtiHandler;
struct CalibBlock;
struct CalibParams;

/* the results the processor and the host interface report */
enum {
	MOTION_OK			= 0,
	MOTION_ERROR_INVALID		= 1100,
	MOTION_ERROR_FRAME_SIZE		= 1102,
	MOTION_ERROR_FRAME_EOF		= 1103,
	MOTION_ERROR_FRAME_CHECKSUM	= 1104,
	MOTION_ERROR_PACKET_ID		= 1105,
	MOTION_ERROR_NO_SPACE		= 1106,
	MOTION_ERROR_FLASH		= 1107,
	MOTION_ERROR_SIZE		= 1108,
	MOTION_ERROR_SAMPLE_TYPE	= 1115,
	MOTION_ERROR_BUSY		= 1116,
	MOTION_ERROR_FRAME_LENGTH	= 1119,
	MOTION_ERROR_VERIFY		= 1120,
	MOTION_ERROR_MEAS_TYPE		= 1121,
	MOTION_ERROR_STOPPING		= 1122,
	MOTION_ERROR_TRANSITION		= 1123,
};

/* the processor's states */
enum {
	MOTION_STATE_RUN,
	MOTION_STATE_STANDBY,
	MOTION_STATE_IDLE,
	MOTION_STATE_SLEEP,
};

/* what the processor does once the magnetometer is stopped */
enum {
	MOTION_REQUEST_NONE,
	MOTION_REQUEST_STOP,
	MOTION_REQUEST_WRITE_CALIB,
	MOTION_REQUEST_STANDBY,
	MOTION_REQUEST_SLEEP,
};

/* the measurement types */
enum {
	MOTION_TYPES = 8,
	MOTION_TYPE_MAG_ONLY = 7,
};

/* the processor's parameters (flash, the image's constants) */
struct MotionConfig {
	const CalibBlock *calibBlocks[4];
	u32 bootWait;			/* ADC samples before the magnetometer boots */
	u32 wakeWait;			/* ... before it measures, after a wake-up */
	u32 standbyWait;		/* ... after a standby */
	s32 bootTimeout;
	s32 measureTimeout;
	s32 retryDelay;
	u32 accelScale;
	u8 accelBits;
	u8 gyroBits;
	u8 magBits;
	u8 tempBits;
	struct {
		u32 framePeriod;
		u32 gyroPeriod;
		u32 magPeriod;
		u32 tempPeriod;
		u32 sampleType;
	} types[MOTION_TYPES];
	u8 refCalibMode[MOTION_TYPES];	/* 0 none, 1 on, 2 once */
	s32 stopTimeout;
	s32 wakeTimeout;
	u32 refCalibPeriod;
	u32 stillThreshold;
};

/* the boot loader's information in the MCU's flash */
struct LoaderInfo {
	u16 version;
	u8 hwInfo[2];
};

#define LOADER_INFO	((const LoaderInfo *)0x08000200)

/* what the host reads as the device information */
struct DeviceInfo {
	u16 userVersion;
	u16 hwInfo;
	u32 serial;
	u16 loaderVersion;
	u8 calibVersion[2][2];
	u8 magInfo[12];
	u8 selfTestFailed;
	u16 accelRef[3];
	u8 reserved;
} __attribute__((packed));

struct VersionInfo {
	u16 userVersion;
	u16 hwInfo;
	u32 serial;
};

/* the accelerometer's references, measured while the device lies still */
class Statistic {
public:
	void Reset()
	{
		max_ = 0;
		min_ = 0xFFFF;
		count_ = 0;
		sum_ = 0;
	}

	void Add(u16 value)
	{
		sum_ += value;
		count_++;
	}

	void Clear()
	{
		Reset();
	}

	/* also the largest and the smallest */
	void AddRange(u16 value)
	{
		if (max_ < value)
			max_ = value;
		if (value < min_)
			min_ = value;
		sum_ += value;
		count_++;
	}

	u16 Mean() const
	{
		if (count_ == 0)
			return 0;
		return sum_ / count_;
	}

	u16 max_;
	u16 min_;

private:
	u32 sum_;
	u32 count_;
};

class RefCalibration {
public:
	void Clear()
	{
		valid_ = false;
	}

	void SetValid()
	{
		valid_ = true;
	}

	bool IsValid() const
	{
		return valid_;
	}

	void Reset()
	{
		for (u8 i = 0; i < 3; i++) {
			value_[i].Clear();
			refSum_[i].Clear();
		}
	}

	/* each axis's value, its range, and its reference */
	void Add(const AdcSample *sample)
	{
		value_[0].AddRange(sample->accel[0][0]);
		value_[1].AddRange(sample->accel[1][0]);
		value_[2].AddRange(sample->accel[2][0]);
		refSum_[0].Add(sample->accel[0][1]);
		refSum_[1].Add(sample->accel[1][1]);
		refSum_[2].Add(sample->accel[2][1]);
	}

private:
	bool valid_;

public:
	u16 ref_[3];

private:
	u8 reserved_[8];

public:
	Statistic value_[3];
	Statistic refSum_[3];
};

class MotionProcessor {
public:
	typedef void (MotionProcessor::*SampleHandler)(const AdcSample *sample);
	typedef void (MotionProcessor::*MagHandler)(const u8 *data);
	typedef Callback<void (s32)> Done;

	struct TypeHandlers {
		SampleHandler sample;
		MagHandler mag;
		u8 frameFormat;
	};

	MotionProcessor(AdcSampler *sampler, AK8975 *ak8975, Output *magPower,
			ExtiPin *drdy, Output *stopLed, Output *gyroStandby,
			Listener *listeners, u32 listenerCount, const MotionConfig *config);
	bool Init();
	bool StartSampling();
	void Start();
	void Stop();
	void SetMeasType(s8 type);
	void SetState(s8 state);
	void GetVersion(VersionInfo *version);
	s32 WriteCalib(u8 block, const u8 *src, u32 size, Done::Func func, void *arg);
	s32 ReadCalib(u8 block, u8 *dst, u32 dstSize, u32 *size);
	static void RestartSystem();
	static void ResetSystem();
	s32 AddListener(Listener::Func func, void *arg);
	s32 StartSelfTest(Done::Func func, void *arg);
	void ResetBoardType();

	s8 BoardType() const
	{
		return boardType_;
	}

	const DeviceInfo *GetDeviceInfo() const
	{
		return &info_;
	}

	const u16 *GetChecksum() const
	{
		return &checksum_;
	}

	s8 State() const;
	bool CanMeasure() const
	{
		return state_ == MOTION_STATE_RUN || state_ == MOTION_STATE_IDLE;
	}

private:
	static void WriteCalibTask(void *arg);
	void WriteCalibNow();
	void Sleep();
	void StopMag();
	void StopMagNow();
	void StopSampling();
	void PostSleep();
	void Wake();
	void WakeFromStandby();
	void RestartMag();
	void Resume();
	void StartMagSampling();
	void RestartMagSampling();
	void StartSampling(s8 mode);
	static void SleepTask(void *arg);
	void EnterSleep();
	void ProcessRequest();
	void ResetAverages();
	void ResetMagAverages();
	void OnSampleRaw(const AdcSample *sample);
	void OnSampleAverage(const AdcSample *sample);
	void OnSampleCalibrated(const AdcSample *sample);
	void OnSampleNone(const AdcSample *sample) {}
	void ScaleAccel(const AdcSample *sample, u32 *accel);
	void Transform(s32 *out, const s32 *matrix, const s32 *in);
	void CalibrateAccel(const u16 *in, s16 *out);
	void CalibrateGyro(const u16 *in, s16 *out);
	void OnMagRaw(const u8 *data);
	void OnMagAverage(const u8 *data);
	void OnMagOnly(const u8 *data)
	{
		OnMagAverage(data);
	}
	void OnMagCalibrated(const u8 *data)
	{
		OnMagAverage(data);
	}
	void ReadDeviceInfo();
	void NotifyListeners();
	static void BootSampleCallback(void *arg, s32 result, const AdcSample *sample);
	void OnBootSample(s32 result, const AdcSample *sample);
	static void WakeSampleCallback(void *arg, s32 result, const AdcSample *sample);
	void OnWakeSample(s32 result, const AdcSample *sample);
	static void StandbySampleCallback(void *arg, s32 result, const AdcSample *sample);
	void OnStandbySample(s32 result, const AdcSample *sample);
	static void SampleCallback(void *arg, s32 result, const AdcSample *sample);
	void OnSample(s32 result, const AdcSample *sample);
	void BootMag();
	static void BootCallback(void *arg, s32 result);
	void OnBoot(s32 result);
	static void BootTimeoutCallback(void *arg);
	void OnBootTimeout();
	void RetryBoot();
	static void RetryBootCallback(void *arg);
	void OnRetryBoot();
	static void MeasureCallback(void *arg, s32 result, u8 *data);
	void OnMeasure(s32 result, u8 *data);
	static void MeasureTimeoutCallback(void *arg);
	void OnMeasureTimeout();
	void RetryMeasure();
	static void RetryMeasureCallback(void *arg);
	void OnRetryMeasure();
	void ReadChecksum();
	void FinishSelfTest();
	static void SelfTestCallback(void *arg, s32 result, const AdcSample *sample);
	void OnSelfTestSample(s32 result, const AdcSample *sample);
	void SetRequest(s8 request);
	bool PostWriteCalib();
	void AbortBoot();
	void AbortMeasure();
	void AbortMag();
	static void SettledCallback(void *arg);
	void OnSettled();
	bool IsStill();
	void UpdateRefCalibration();
	bool RefCalibEnabled() const;
	bool RefCalibOnce() const;
	u32 StillThreshold() const;

	/* value * scale / ref, rounded */
	u32 Ratio(u32 scale, u16 value, u16 ref) const
	{
		if (ref == 0)
			return 0;
		return (value * scale + ref / 2) / ref;
	}

	bool HasRequest() const
	{
		return request_ != MOTION_REQUEST_NONE;
	}

	bool WriteCalibRequested() const
	{
		return request_ == MOTION_REQUEST_WRITE_CALIB;
	}

	bool SleepRequested() const
	{
		return request_ == MOTION_REQUEST_STANDBY || request_ == MOTION_REQUEST_SLEEP;
	}

	void ResetSelfTest()
	{
		selfTestCount_ = 0;
	}

	s8 measType_;
	s8 state_;
	AdcSampler *sampler_;
	AK8975 *ak8975_;
	Output *magPower_;
	Output *stopLed_;
	Output *gyroStandby_;
	const MotionConfig *config_;
	u32 accelScale_;
	u32 bootWait_;
	u32 wakeWait_;
	DeviceInfo info_;
	u16 checksum_;			/* the firmware's, for the debug report */
	Listener *listeners_;
	u32 listenerCount_;
	Divider frameDivider_;
	Divider gyroDivider_;
	Divider magDivider_;
	Divider tempDivider_;
	Averager<u16> accel_[3];
	Averager<u16> gyro_[3];
	Averager<s16> mag_[3];
	Averager<u16> temp_;
	Divider refCalibDivider_;
	u16 temperature_[2];
	bool sampling_;
	bool magRunning_;
	u32 magBuffer_;
	u8 magData_[2][7];
	const u8 *magRaw_;
	bool magSync_;
	bool magSynced_;
	u32 calibBlock_;
	const u8 *calibSrc_;
	u32 calibSize_;
	Done calibDone_;
	Done selfTestDone_;
	u8 reserved_;
	FrameStatus status_;
	MotionFrame frame_;
	u16 frameCount_;
	const CalibParams *params_;
	SampleHandler sampleHandler_;
	MagHandler magHandler_;
	ExtiHandler *drdy_;
	s8 boardType_;
	u8 reserved2_;
	u8 selfTestCount_;
	u16 selfTest_[3][16];
	s8 request_;
	Timer magTimer_;
	Timer settleTimer_;
	bool settling_;
	RefCalibration refCalib_;

	static const TypeHandlers typeHandlers_[MOTION_TYPES];
	static const u16 selfTestRange_[3][2];
};

#endif
