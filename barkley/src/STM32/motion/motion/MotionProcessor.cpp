/*
 * MotionProcessor: the sensors' sampling and the frames made from it.
 *
 * The ADC sampler reports each sample (AdcSample) to the handler of the measurement
 * type, which averages it and, at the type's periods, puts the averages in the frame;
 * a frame with any field is passed to the listeners. The AK8975 measures on its own
 * (two buffers, one being filled while the other is handled), its results going to the
 * type's magnetometer handler. Stopping the magnetometer is asynchronous: a request
 * (MOTION_REQUEST_*) is kept until its measurement ends.
 */

#include <stdio.h>
#include <string.h>

extern "C" {
#include "cortexm3_macro.h"
}
#include "MotionProcessor.h"
#include "AdcSampler.h"
#include "AK8975.h"
#include "Flash.h"
#include "Gpio.h"
#include "System.h"

#if MOTION_IMAGE == 0
/*
 * Image 00 (branch U0652): the processor starts and stops; the magnetometer boots once
 * the ADC has run bootWait samples and then measures without end, a failure leaving it
 * stopped.
 */
const MotionProcessor::TypeHandlers MotionProcessor::typeHandlers_[MOTION_TYPES] = {
	{ &MotionProcessor::OnSampleRaw, &MotionProcessor::OnMagRaw, 0 },
	{ &MotionProcessor::OnSampleAverage, &MotionProcessor::OnMagAverage, 1 },
	{ &MotionProcessor::OnSampleNone, &MotionProcessor::OnMagOnly, 2 },
	{ &MotionProcessor::OnSampleCalibrated, &MotionProcessor::OnMagCalibrated, 3 },
	{ &MotionProcessor::OnSampleRaw, &MotionProcessor::OnMagRaw, 0 },
	{ &MotionProcessor::OnSampleAverage, &MotionProcessor::OnMagAverage, 1 },
};

MotionProcessor::MotionProcessor(AdcSampler *sampler, AK8975 *ak8975, ExtiPin *boardPin,
				 Listener *listeners, u32 listenerCount, const MotionConfig *config)
	: sampler_(sampler), ak8975_(ak8975), config_(config), accelScale_(config->accelScale),
	  bootWait_(config->bootWait), listeners_(listeners), listenerCount_(listenerCount),
	  magRunning_(false), magRaw_(0), magSync_(false), magSynced_(false), state_(-1),
	  stopped_(false), frameCount_(0), params_((const CalibParams *)config->calibBlocks[2]),
	  boardPin_(boardPin)
{
	for (u32 i = 0; i < listenerCount; i++)
		listeners_[i].Clear();
	for (u32 i = 0; i < 3; i++) {
		accel_[i].Init(config->accelBits);
		gyro_[i].Init(config->gyroBits);
		mag_[i].Init(config->magBits);
	}
	temp_.Init(config->tempBits);
	SetMeasType(0);
}

void MotionProcessor::Init()
{
	ReadDeviceInfo();
	ReadChecksum();
}

void MotionProcessor::StartSampling()
{
	status_.ClearState();
	sampler_->Start(BootSampleCallback, this);
}

void MotionProcessor::Start()
{
	if (stopped_) {
		stopped_ = false;
		ResetAverages();
		sampler_->Resume();
		magRunning_ = true;
		magBuffer_ = 0;
		ak8975_->Measure(magData_[magBuffer_], MeasureCallback, this);
	}
}

void MotionProcessor::SetMeasType(s8 type)
{
	__SETPRIMASK();
	sampleHandler_ = typeHandlers_[type].sample;
	magHandler_ = typeHandlers_[type].mag;
	frame_.SetFormat(typeHandlers_[type].frameFormat);
	frameDivider_.Set(config_->types[type].framePeriod, 0);
	gyroDivider_.Set(config_->types[type].gyroPeriod, 0);
	magDivider_.Set(config_->types[type].magPeriod, 0);
	tempDivider_.Set(config_->types[type].tempPeriod, 0);
	status_.SetSampleType(config_->types[type].sampleType);
	ResetAverages();
	__RESETPRIMASK();
}

void MotionProcessor::GetVersion(VersionInfo *version)
{
	version->userVersion = info_.userVersion;
	version->hwInfo = info_.hwInfo;
	version->serial = info_.serial;
}

s32 MotionProcessor::WriteCalib(u8 block, const u8 *src, u32 size, Done::Func func, void *arg)
{
	Stop();
	calibBlock_ = block;
	calibSrc_ = src;
	calibSize_ = size;
	calibDone_.Set(func, arg);
	if (!PostTask(WriteCalibTask, this))
		return MOTION_ERROR_BUSY;
	return MOTION_OK;
}

void MotionProcessor::WriteCalibTask(void *arg)
{
	static_cast<MotionProcessor *>(arg)->WriteCalibNow();
}

void MotionProcessor::WriteCalibNow()
{
	s32 written = Flash::Program((u32)config_->calibBlocks[calibBlock_], calibSrc_, calibSize_);
	s32 result;

	if (written < 0 || written != (s32)calibSize_)
		result = MOTION_ERROR_FLASH;
	else
		result = MOTION_OK;
	calibDone_(result);
}

s32 MotionProcessor::ReadCalib(u8 block, u8 *dst, u32 dstSize, u32 *size)
{
	Stop();
	const CalibBlock *calib = config_->calibBlocks[block];

	if (block >= 4) {
		*size = 0;
		return MOTION_ERROR_INVALID;
	}
	if (!calib->IsValid()) {
		*size = 0;
		return MOTION_ERROR_INVALID;
	}
	*size = calib->Size();
	if (dstSize < *size) {
		printf("Read Calib ERROR, buffer size=%u.\n", dstSize);
		*size = 0;
		return MOTION_ERROR_INVALID;
	}
	memcpy(dst, calib, *size);
	return MOTION_OK;
}

void MotionProcessor::RestartSystem()
{
	ScheduleReset();
}

void MotionProcessor::ResetSystem()
{
	StartWatchdog();
}

s32 MotionProcessor::AddListener(Listener::Func func, void *arg)
{
	for (u32 i = 0; i < listenerCount_; i++) {
		if (!listeners_[i].IsSet()) {
			listeners_[i].Set(func, arg);
			return i;
		}
	}
	return -1;
}

void MotionProcessor::ResetAverages()
{
	for (u32 i = 0; i < 3; i++) {
		accel_[i].Reset();
		gyro_[i].Reset();
		mag_[i].Reset();
	}
	temp_.Reset();
}

void MotionProcessor::OnSampleRaw(const AdcSample *sample)
{
	bool framed = false;

	if (frameDivider_.Tick()) {
		frame_.SetCount(frameCount_);
		frameCount_++;
		frame_.SetStatus(status_.Get());
		frame_.SetAccel<FrameRaw>(sample);
		framed = true;
	}
	if (gyroDivider_.Tick())
		frame_.SetGyro<FrameRaw>(sample);
	if (tempDivider_.Tick() && framed) {
		temperature_[0] = 0;
		temperature_[1] = sample->temp;
		frame_.SetTemp<FrameRaw>(temperature_);
	}
	if (magDivider_.Tick() && magRaw_ != 0) {
		__SETPRIMASK();
		frame_.SetMag<FrameRaw>((const s16 *)(magRaw_ + 1));
		magRaw_ = 0;
		__RESETPRIMASK();
	}
}

void MotionProcessor::OnSampleAverage(const AdcSample *sample)
{
	u32 accel[3];
	bool framed = false;
	u16 accelValue[3];
	u16 gyroValue[3];
	s16 mag[3];

	ScaleAccel(sample, accel);
	accel_[0].Add(accel[0]);
	accel_[1].Add(accel[1]);
	accel_[2].Add(accel[2]);
	gyro_[0].Add(sample->gyro[0]);
	gyro_[1].Add(sample->gyro[1]);
	gyro_[2].Add(sample->gyro[2]);
	temp_.Add(sample->temp);
	if (frameDivider_.Tick()) {
		frame_.SetCount(frameCount_);
		frameCount_++;
		frame_.SetStatus(status_.Get());
		accelValue[0] = accel_[0].Get();
		accelValue[1] = accel_[1].Get();
		accelValue[2] = accel_[2].Get();
		frame_.SetAccel<FrameAverage>(accelValue);
		framed = true;
	}
	if (gyroDivider_.Tick()) {
		gyroValue[0] = gyro_[0].Get();
		gyroValue[1] = gyro_[1].Get();
		gyroValue[2] = gyro_[2].Get();
		frame_.SetGyro<FrameAverage>(gyroValue);
	}
	if (tempDivider_.Tick() && framed) {
		temperature_[0] = 0;
		temperature_[1] = temp_.Get();
		frame_.SetTemp<FrameAverage>(temperature_);
	}
	if (magDivider_.Tick() && mag_[0].Count() != 0) {
		__SETPRIMASK();
		mag[0] = mag_[0].Get();
		mag[1] = mag_[1].Get();
		mag[2] = mag_[2].Get();
		__RESETPRIMASK();
		frame_.SetMag<FrameAverage>(mag);
	}
}

void MotionProcessor::OnSampleCalibrated(const AdcSample *sample)
{
	u32 accel[3];
	u16 value[3];
	s16 out[3];

	ScaleAccel(sample, accel);
	accel_[0].Add(accel[0]);
	accel_[1].Add(accel[1]);
	accel_[2].Add(accel[2]);
	gyro_[0].Add(sample->gyro[0]);
	gyro_[1].Add(sample->gyro[1]);
	gyro_[2].Add(sample->gyro[2]);
	temp_.Add(sample->temp);
	if (!frameDivider_.Tick())
		return;
	frame_.SetCount(frameCount_);
	frameCount_++;
	frame_.SetStatus(status_.Get());
	value[0] = accel_[0].Get();
	value[1] = accel_[1].Get();
	value[2] = accel_[2].Get();
	CalibrateAccel(value, out);
	frame_.SetAccel<FrameCalibrated>(out);
	value[0] = gyro_[0].Get();
	value[1] = gyro_[1].Get();
	value[2] = gyro_[2].Get();
	CalibrateGyro(value, out);
	frame_.SetGyro<FrameCalibrated>(out);
	__SETPRIMASK();
	temperature_[0] = 0;
	__RESETPRIMASK();
	temperature_[1] = temp_.Get();
	frame_.SetTemp<FrameCalibrated>(temperature_);
	if (mag_[0].Count() != 0) {
		__SETPRIMASK();
		out[0] = mag_[0].Get();
		out[1] = mag_[1].Get();
		out[2] = mag_[2].Get();
		__RESETPRIMASK();
		frame_.SetMag<FrameCalibrated>(out);
	}
}

void MotionProcessor::ScaleAccel(const AdcSample *sample, u32 *accel)
{
	accel[0] = Ratio(accelScale_, sample->accel[0][0], sample->accel[0][1]);
	accel[1] = Ratio(accelScale_, sample->accel[1][0], sample->accel[1][1]);
	accel[2] = Ratio(accelScale_, sample->accel[2][0], sample->accel[2][1]);
}

void MotionProcessor::Transform(s32 *out, const s32 *matrix, const s32 *in)
{
	out[0] = matrix[0] * in[0] + matrix[1] * in[1] + matrix[2] * in[2];
	out[1] = matrix[3] * in[0] + matrix[4] * in[1] + matrix[5] * in[2];
	out[2] = matrix[6] * in[0] + matrix[7] * in[1] + matrix[8] * in[2];
}

void MotionProcessor::CalibrateAccel(const u16 *in, s16 *out)
{
	s32 result[3];
	s32 value[3];
	s32 matrix[9];
	s32 shift;

	for (u32 i = 0; i < 3; i++) {
		s32 v = in[i] - params_->accelOffset[i];
		u32 negative;

		if (v >= params_->accelDeadLow[i] && params_->accelDeadHigh[i] >= v)
			v = 0;
		value[i] = v;
		negative = v * params_->accelMatrix[0][i][i] < 0;
		matrix[i] = params_->accelMatrix[negative][0][i];
		matrix[i + 3] = params_->accelMatrix[negative][1][i];
		matrix[i + 6] = params_->accelMatrix[negative][2][i];
	}
	Transform(result, matrix, value);
	shift = params_->accelShift;
	out[0] = result[0] >> shift;
	out[1] = result[1] >> shift;
	out[2] = result[2] >> shift;
}

void MotionProcessor::CalibrateGyro(const u16 *in, s16 *out)
{
	s32 result[3];
	s32 value[3];
	s32 shift;

	for (u32 i = 0; i < 3; i++)
		value[i] = in[i] - params_->gyroOffset[i];
	Transform(result, &params_->gyroMatrix[0][0], value);
	shift = params_->gyroShift;
	out[0] = result[0] >> shift;
	out[1] = result[1] >> shift;
	out[2] = result[2] >> shift;
}

void MotionProcessor::OnMagRaw(const u8 *data)
{
	__SETPRIMASK();
	magRaw_ = data;
	__RESETPRIMASK();
}

void MotionProcessor::OnMagAverage(const u8 *data)
{
	__SETPRIMASK();
	mag_[0].Add(*(const s16 *)(data + 1));
	mag_[1].Add(*(const s16 *)(data + 3));
	mag_[2].Add(*(const s16 *)(data + 5));
	__RESETPRIMASK();
}

/* the hardware information's bit 7 set on the board type 0 */
void MotionProcessor::ReadDeviceInfo()
{
	u32 serial;
	u8 board;

	info_.userVersion = UserVersion();
	info_.loaderVersion = LOADER_INFO->version;
	board = BoardType() == 1 ? 0 : 0x80;
	info_.hwInfo = LOADER_INFO->hwInfo[0] | LOADER_INFO->hwInfo[1] << 8 | board;
	serial = 0xFFFFFFFF;
	for (u32 i = 0; i < 2; i++) {
		const CalibBlock *calib = config_->calibBlocks[i];

		if (calib->IsValid()) {
			info_.calibVersion[i][0] = calib->version[0];
			info_.calibVersion[i][1] = calib->version[1];
			serial = calib->serial;
		} else {
			info_.calibVersion[i][0] = 0xFF;
			info_.calibVersion[i][1] = 0xFF;
		}
	}
	info_.serial = serial;
}

void MotionProcessor::NotifyListeners()
{
	for (u32 i = 0; i < listenerCount_; i++)
		listeners_[i].Notify(&frame_);
}

void MotionProcessor::BootSampleCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnBootSample(result, sample);
}

void MotionProcessor::OnBootSample(s32 result, const AdcSample *sample)
{
	if (bootWait_ != 0) {
		if (--bootWait_ == 0)
			BootMag();
	} else {
		sampler_->SetCallback(SampleCallback, this);
	}
}

void MotionProcessor::SampleCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnSample(result, sample);
}

void MotionProcessor::OnSample(s32 result, const AdcSample *sample)
{
	frame_.ClearFields();
	(this->*sampleHandler_)(sample);
	if (frame_.Fields())
		NotifyListeners();
}

/* the calibration blocks reported first */
void MotionProcessor::BootMag()
{
	for (u32 i = 0; i < 2; i++) {
		const CalibBlock *calib = config_->calibBlocks[i];

		if (calib->IsValid()) {
			printf("Calib Status: Check BlockId:%d(0x%08X).\n", i, calib);
			if (calib->count >= 26)
				printf("Calib Status: BlockId:%d(0x%08X) record num(%d) overflow.\n",
				       i, calib, calib->count);
		} else {
			printf("Calib Status: Invalid Calib BlockId:%d, (0x%08X).\n", i, calib);
		}
	}
	magRunning_ = true;
	ak8975_->Boot(info_.magInfo, BootCallback, this);
}

void MotionProcessor::BootCallback(void *arg, s32 result)
{
	static_cast<MotionProcessor *>(arg)->OnBoot(result);
}

void MotionProcessor::OnBoot(s32 result)
{
	if (result != 0) {
		printf("MotionProcessor: AK8975 boot failed.\n");
		magRunning_ = false;
	} else {
		status_.SetState(state_);
		state_ = -1;
		magBuffer_ = 0;
		ak8975_->Measure(magData_[0], MeasureCallback, this);
	}
}

void MotionProcessor::MeasureCallback(void *arg, s32 result, u8 *data)
{
	static_cast<MotionProcessor *>(arg)->OnMeasure(result, data);
}

void MotionProcessor::OnMeasure(s32 result, u8 *data)
{
	u32 next;

	if (result != 0) {
		printf("MotionProcessor: AMI304 measure failed %d.\n", result);
		magRunning_ = false;
		status_.ClearState();
	} else {
		next = magBuffer_ ^ 1;
		ak8975_->Measure(magData_[next], MeasureCallback, this);
		if (magSync_) {
			magSynced_ = true;
			__SETPRIMASK();
			temperature_[0] = 0;
			__RESETPRIMASK();
		}
		(this->*magHandler_)(magData_[magBuffer_]);
		magBuffer_ = next;
	}
}

void MotionProcessor::ReadChecksum()
{
	checksum_ = FirmwareChecksum();
}

void MotionProcessor::ResetBoardType()
{
	if (boardPin_->Get())
		boardType_ = 0;
	else
		boardType_ = 1;
}

s32 MotionProcessor::StartSelfTest(Done::Func func, void *arg)
{
	ResetSelfTest();
	selfTestDone_.Set(func, arg);
	sampler_->StartSelfTest(SelfTestCallback, this);
	return MOTION_OK;
}

const u16 MotionProcessor::selfTestRange_[3][2] = {
	{ 0x0064, 0xFF9C },
	{ 0x0064, 0xFF9C },
	{ 0x0064, 0xFF9C },
};

void MotionProcessor::FinishSelfTest()
{
	u32 sum[3];
	bool passed = true;

	sampler_->StopSelfTest();
	memset(sum, 0, sizeof(sum));
	for (u8 i = 0; i < 16; i++) {
		sum[0] += selfTest_[0][i];
		sum[1] += selfTest_[1][i];
		sum[2] += selfTest_[2][i];
	}
	for (u8 i = 0; i < 3; i++) {
		sum[i] >>= 4;
		if (selfTestRange_[i][0] >= sum[i] || sum[i] >= selfTestRange_[i][1])
			passed = false;
	}
	info_.selfTestFailed = !passed;
	info_.accelRef[0] = sum[0];
	info_.accelRef[1] = sum[1];
	info_.accelRef[2] = sum[2];
	sampler_->Start(SampleCallback, this);
	selfTestDone_(MOTION_OK);
}

void MotionProcessor::SelfTestCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnSelfTestSample(result, sample);
}

void MotionProcessor::OnSelfTestSample(s32 result, const AdcSample *sample)
{
	if (selfTestCount_ < 16) {
		selfTest_[0][selfTestCount_] = sample->accel[0][1];
		selfTest_[1][selfTestCount_] = sample->accel[1][1];
		selfTest_[2][selfTestCount_++] = sample->accel[2][1];
	}
	if (selfTestCount_ == 16)
		FinishSelfTest();
}

void MotionProcessor::Stop()
{
	sampler_->Stop();
	stopped_ = true;
}
#else
const MotionProcessor::TypeHandlers MotionProcessor::typeHandlers_[MOTION_TYPES] = {
	{ &MotionProcessor::OnSampleRaw, &MotionProcessor::OnMagRaw, 0 },
	{ &MotionProcessor::OnSampleAverage, &MotionProcessor::OnMagAverage, 1 },
	{ &MotionProcessor::OnSampleNone, &MotionProcessor::OnMagOnly, 2 },
	{ &MotionProcessor::OnSampleCalibrated, &MotionProcessor::OnMagCalibrated, 3 },
	{ &MotionProcessor::OnSampleRaw, &MotionProcessor::OnMagRaw, 0 },
	{ &MotionProcessor::OnSampleAverage, &MotionProcessor::OnMagAverage, 1 },
	{ &MotionProcessor::OnSampleAverage, &MotionProcessor::OnMagAverage, 6 },
	{ &MotionProcessor::OnSampleAverage, &MotionProcessor::OnMagAverage, 7 },
};

MotionProcessor::MotionProcessor(AdcSampler *sampler, AK8975 *ak8975, Output *magPower,
				 ExtiPin *drdy, Output *stopLed, Output *gyroStandby,
				 Listener *listeners, u32 listenerCount, const MotionConfig *config)
	: measType_(1), state_(MOTION_STATE_RUN), sampler_(sampler), ak8975_(ak8975),
	  magPower_(magPower), stopLed_(stopLed), gyroStandby_(gyroStandby), config_(config),
	  accelScale_(config->accelScale), bootWait_(config->bootWait), wakeWait_(0),
	  listeners_(listeners), listenerCount_(listenerCount), sampling_(false),
	  magRunning_(false), magRaw_(0), magSync_(false), magSynced_(false), reserved_(0),
	  frameCount_(0), params_((const CalibParams *)config->calibBlocks[2]), drdy_(drdy),
	  request_(MOTION_REQUEST_NONE), settling_(false)
{
	for (u32 i = 0; i < listenerCount; i++)
		listeners_[i].Clear();
	for (u32 i = 0; i < 3; i++) {
		accel_[i].Init(config->accelBits);
		gyro_[i].Init(config->gyroBits);
		mag_[i].Init(config->magBits);
	}
	temp_.Init(config->tempBits);
	SetMeasType(1);
}

bool MotionProcessor::Init()
{
	ReadDeviceInfo();
	ReadChecksum();
	magPower_->Set(true);
	stopLed_->Set(false);
	gyroStandby_->Set(false);
	if (settleTimer_.StartChecked(config_->stopTimeout, SettledCallback, this)) {
		settling_ = true;
		return true;
	}
	return false;
}

bool MotionProcessor::StartSampling()
{
	status_.SetState(state_);
	sampler_->Start(BootSampleCallback, this);
	sampling_ = true;
	return true;
}

void MotionProcessor::Start()
{
	if (state_ == MOTION_STATE_SLEEP) {
		__SETPRIMASK();
		Wake();
		state_ = MOTION_STATE_RUN;
		status_.SetState(state_);
		__RESETPRIMASK();
	} else if (!sampling_) {
		ResetAverages();
		sampler_->StartTriggered(SampleCallback, this);
		sampler_->Resume();
		sampling_ = true;
		if (CanMeasure()) {
			magRunning_ = true;
			magBuffer_ = 0;
			magTimer_.Start(config_->measureTimeout, MeasureTimeoutCallback, this);
			ak8975_->Measure(magData_[magBuffer_], MeasureCallback, this);
		}
	}
}

void MotionProcessor::Stop()
{
	sampler_->Stop();
	sampling_ = false;
	if (magRunning_)
		SetRequest(MOTION_REQUEST_STOP);
}

void MotionProcessor::SetMeasType(s8 type)
{
	if (type >= MOTION_TYPES || (type == MOTION_TYPE_MAG_ONLY && measType_ == MOTION_TYPE_MAG_ONLY))
		return;
	__SETPRIMASK();
	sampleHandler_ = typeHandlers_[type].sample;
	magHandler_ = typeHandlers_[type].mag;
	frame_.SetFormat(typeHandlers_[type].frameFormat);
	frameDivider_.Set(config_->types[type].framePeriod, 0);
	gyroDivider_.Set(config_->types[type].gyroPeriod, 0);
	magDivider_.Set(config_->types[type].magPeriod, 0);
	tempDivider_.Set(config_->types[type].tempPeriod, 0);
	status_.ClearMag();
	refCalib_.Clear();
	measType_ = type;
	if (RefCalibEnabled()) {
		refCalibDivider_.Set(config_->refCalibPeriod, 0);
		refCalib_.Reset();
	}
	status_.SetSampleType(config_->types[type].sampleType);
	ResetAverages();
	__RESETPRIMASK();
}

void MotionProcessor::SetState(s8 state)
{
	__SETPRIMASK();
	if (state < 4 && state_ != state) {
		switch (state) {
		case MOTION_STATE_RUN:
			if (state_ == MOTION_STATE_SLEEP)
				Wake();
			else if (state_ == MOTION_STATE_STANDBY)
				WakeFromStandby();
			break;
		case MOTION_STATE_STANDBY:
			if (state_ == MOTION_STATE_RUN) {
				AbortMag();
				if (WriteCalibRequested())
					ProcessRequest();
				StopMag();
			} else if (state_ == MOTION_STATE_SLEEP) {
				Resume();
			}
			break;
		case MOTION_STATE_IDLE:
			goto out;
		case MOTION_STATE_SLEEP:
			if (state_ == MOTION_STATE_RUN || state_ == MOTION_STATE_STANDBY) {
				AbortMag();
				if (WriteCalibRequested())
					ProcessRequest();
				Sleep();
			}
			break;
		}
		state_ = state;
		status_.SetState(state_);
	} else if (state_ == MOTION_STATE_SLEEP && state == MOTION_STATE_SLEEP) {
		Sleep();
	}
out:
	__RESETPRIMASK();
}

void MotionProcessor::GetVersion(VersionInfo *version)
{
	version->userVersion = info_.userVersion;
	version->hwInfo = info_.hwInfo;
	version->serial = info_.serial;
}

s32 MotionProcessor::WriteCalib(u8 block, const u8 *src, u32 size, Done::Func func, void *arg)
{
	calibBlock_ = block;
	calibSrc_ = src;
	calibSize_ = size;
	calibDone_.Set(func, arg);
	if (magRunning_)
		SetRequest(MOTION_REQUEST_WRITE_CALIB);
	else if (!PostWriteCalib())
		return MOTION_ERROR_BUSY;
	return MOTION_OK;
}

void MotionProcessor::WriteCalibTask(void *arg)
{
	static_cast<MotionProcessor *>(arg)->WriteCalibNow();
}

void MotionProcessor::WriteCalibNow()
{
	s32 written = Flash::Program((u32)config_->calibBlocks[calibBlock_], calibSrc_, calibSize_);
	bool differs = false;
	s32 result;

	for (u32 i = 0; i < calibSize_; i++) {
		if (calibSrc_[i] != ((const u8 *)config_->calibBlocks[calibBlock_])[i]) {
			differs = true;
			break;
		}
	}
	if (written < 0 || written != (s32)calibSize_)
		result = MOTION_ERROR_FLASH;
	else if (differs)
		result = MOTION_ERROR_VERIFY;
	else
		result = MOTION_OK;
	calibDone_(result);
}

void MotionProcessor::Sleep()
{
	StopMagNow();
	StopSampling();
	PostSleep();
}

void MotionProcessor::StopMag()
{
	StopMagNow();
}

void MotionProcessor::StopMagNow()
{
	magRunning_ = false;
	ak8975_->Disable();
	magPower_->Set(false);
	if (SleepRequested())
		SetRequest(MOTION_REQUEST_NONE);
}

void MotionProcessor::StopSampling()
{
	sampler_->Stop();
	sampling_ = false;
	sampler_->PowerDown();
	gyroStandby_->Set(true);
	settleTimer_.Stop();
	settling_ = false;
}

void MotionProcessor::PostSleep()
{
	PostTask(SleepTask, this);
}

void MotionProcessor::Wake()
{
	ResetAverages();
	StartMagSampling();
	StartSampling(0);
}

void MotionProcessor::WakeFromStandby()
{
	ResetMagAverages();
	StartMagSampling();
}

void MotionProcessor::RestartMag()
{
	RestartMagSampling();
}

void MotionProcessor::Resume()
{
	ResetAverages();
	StartSampling(1);
}

void MotionProcessor::StartMagSampling()
{
	magPower_->Set(true);
	ak8975_->Enable();
	if (sampling_) {
		wakeWait_ = config_->wakeWait;
		sampler_->SetCallback(WakeSampleCallback, this);
	}
}

void MotionProcessor::RestartMagSampling()
{
	magPower_->Set(true);
	ak8975_->Enable();
	wakeWait_ = config_->wakeWait;
	sampler_->SetCallback(StandbySampleCallback, this);
}

void MotionProcessor::StartSampling(s8 mode)
{
	gyroStandby_->Set(false);
	if (settleTimer_.StartChecked(config_->wakeTimeout, SettledCallback, this))
		settling_ = true;
	sampler_->PowerUp();
	frameDivider_.Set(config_->types[measType_].framePeriod, 0);
	gyroDivider_.Set(config_->types[measType_].gyroPeriod, 0);
	magDivider_.Set(config_->types[measType_].magPeriod, 0);
	tempDivider_.Set(config_->types[measType_].tempPeriod, 0);
	if (measType_ != MOTION_TYPE_MAG_ONLY) {
		status_.ClearMag();
		refCalib_.Clear();
		if (RefCalibEnabled()) {
			refCalibDivider_.Set(config_->refCalibPeriod, 0);
			refCalib_.Reset();
		}
	}
	if (mode == 0) {
		wakeWait_ = config_->standbyWait;
		sampler_->Start(WakeSampleCallback, this);
		sampling_ = true;
	} else if (mode == 1) {
		sampler_->Start(SampleCallback, this);
		sampling_ = true;
	}
}

void MotionProcessor::SleepTask(void *arg)
{
	static_cast<MotionProcessor *>(arg)->EnterSleep();
}

void MotionProcessor::EnterSleep()
{
	stopLed_->Set(true);
	__SETPRIMASK();
	PrepareStop();
	__RESETPRIMASK();
	EnterStopMode();
	stopLed_->Set(false);
}

s32 MotionProcessor::ReadCalib(u8 block, u8 *dst, u32 dstSize, u32 *size)
{
	const CalibBlock *calib = config_->calibBlocks[block];

	if (block >= 4) {
		*size = 0;
		return MOTION_ERROR_INVALID;
	}
	if (!calib->IsValid()) {
		*size = 0;
		return MOTION_ERROR_INVALID;
	}
	*size = calib->Size();
	if (dstSize < *size) {
		printf("Read Calib ERROR, buffer size=%u.\n", dstSize);
		*size = 0;
		return MOTION_ERROR_INVALID;
	}
	memcpy(dst, calib, *size);
	return MOTION_OK;
}

void MotionProcessor::RestartSystem()
{
	ScheduleReset();
}

void MotionProcessor::ResetSystem()
{
	StartWatchdog();
}

s32 MotionProcessor::AddListener(Listener::Func func, void *arg)
{
	for (u32 i = 0; i < listenerCount_; i++) {
		if (!listeners_[i].IsSet()) {
			listeners_[i].Set(func, arg);
			return i;
		}
	}
	return -1;
}

void MotionProcessor::ProcessRequest()
{
	switch (request_) {
	case MOTION_REQUEST_STOP:
		break;
	case MOTION_REQUEST_WRITE_CALIB:
		if (!PostWriteCalib())
			calibDone_(MOTION_ERROR_BUSY);
		break;
	case MOTION_REQUEST_STANDBY:
		__SETPRIMASK();
		StopMag();
		__RESETPRIMASK();
		break;
	case MOTION_REQUEST_SLEEP:
		__SETPRIMASK();
		Sleep();
		__RESETPRIMASK();
		break;
	}
	SetRequest(MOTION_REQUEST_NONE);
}

void MotionProcessor::ResetAverages()
{
	for (u32 i = 0; i < 3; i++) {
		accel_[i].Reset();
		gyro_[i].Reset();
		mag_[i].Reset();
	}
	temp_.Reset();
}

void MotionProcessor::ResetMagAverages()
{
	for (u32 i = 0; i < 3; i++)
		mag_[i].Reset();
}

void MotionProcessor::OnSampleRaw(const AdcSample *sample)
{
	bool framed = false;

	if (frameDivider_.Tick()) {
		frame_.SetCount(frameCount_);
		frameCount_++;
		frame_.SetStatus(status_.Get());
		frame_.SetAccel<FrameRaw>(sample);
		framed = true;
	}
	if (gyroDivider_.Tick())
		frame_.SetGyro<FrameRaw>(sample);
	if (tempDivider_.Tick() && framed) {
		temperature_[0] = 0;
		temperature_[1] = sample->temp;
		frame_.SetTemp<FrameRaw>(temperature_);
	}
	if (magDivider_.Tick() && magRaw_ != 0) {
		__SETPRIMASK();
		frame_.SetMag<FrameRaw>((const s16 *)(magRaw_ + 1));
		magRaw_ = 0;
		__RESETPRIMASK();
	}
}

void MotionProcessor::OnSampleAverage(const AdcSample *sample)
{
	u32 accel[3];
	bool framed = false;
	u16 accelValue[3];
	u16 gyroValue[3];
	s16 mag[3];

	ScaleAccel(sample, accel);
	accel_[0].Add(accel[0]);
	accel_[1].Add(accel[1]);
	accel_[2].Add(accel[2]);
	gyro_[0].Add(sample->gyro[0]);
	gyro_[1].Add(sample->gyro[1]);
	gyro_[2].Add(sample->gyro[2]);
	temp_.Add(sample->temp);
	if (frameDivider_.Tick()) {
		frame_.SetCount(frameCount_);
		frameCount_++;
		status_.ClearMagUpdated();
		frame_.SetStatus(status_.Get());
		accelValue[0] = accel_[0].Get();
		accelValue[1] = accel_[1].Get();
		accelValue[2] = accel_[2].Get();
		frame_.SetAccel<FrameAverage>(accelValue);
		framed = true;
	}
	if (RefCalibEnabled() && !settling_) {
		refCalib_.Add(sample);
		if (refCalibDivider_.Tick()) {
			if (framed) {
				if (IsStill())
					UpdateRefCalibration();
				refCalib_.Reset();
			} else {
				refCalibDivider_.Set(config_->refCalibPeriod, config_->refCalibPeriod - 1);
			}
		}
	}
	if (gyroDivider_.Tick()) {
		gyroValue[0] = gyro_[0].Get();
		gyroValue[1] = gyro_[1].Get();
		gyroValue[2] = gyro_[2].Get();
		frame_.SetGyro<FrameAverage>(gyroValue);
	}
	if (tempDivider_.Tick() && framed) {
		temperature_[0] = 0;
		temperature_[1] = temp_.Get();
		frame_.SetTemp<FrameAverage>(temperature_);
	}
	if (magDivider_.Tick() && mag_[0].Count() != 0) {
		__SETPRIMASK();
		mag[0] = mag_[0].Get();
		mag[1] = mag_[1].Get();
		mag[2] = mag_[2].Get();
		__RESETPRIMASK();
		frame_.SetMag<FrameAverage>(mag);
	}
}

void MotionProcessor::OnSampleCalibrated(const AdcSample *sample)
{
	u32 accel[3];
	u16 value[3];
	s16 out[3];

	ScaleAccel(sample, accel);
	accel_[0].Add(accel[0]);
	accel_[1].Add(accel[1]);
	accel_[2].Add(accel[2]);
	gyro_[0].Add(sample->gyro[0]);
	gyro_[1].Add(sample->gyro[1]);
	gyro_[2].Add(sample->gyro[2]);
	temp_.Add(sample->temp);
	if (!frameDivider_.Tick())
		return;
	frame_.SetCount(frameCount_);
	frameCount_++;
	frame_.SetStatus(status_.Get());
	value[0] = accel_[0].Get();
	value[1] = accel_[1].Get();
	value[2] = accel_[2].Get();
	CalibrateAccel(value, out);
	frame_.SetAccel<FrameCalibrated>(out);
	value[0] = gyro_[0].Get();
	value[1] = gyro_[1].Get();
	value[2] = gyro_[2].Get();
	CalibrateGyro(value, out);
	frame_.SetGyro<FrameCalibrated>(out);
	__SETPRIMASK();
	temperature_[0] = 0;
	__RESETPRIMASK();
	temperature_[1] = temp_.Get();
	frame_.SetTemp<FrameCalibrated>(temperature_);
	if (mag_[0].Count() != 0) {
		__SETPRIMASK();
		out[0] = mag_[0].Get();
		out[1] = mag_[1].Get();
		out[2] = mag_[2].Get();
		__RESETPRIMASK();
		frame_.SetMag<FrameCalibrated>(out);
	}
}

void MotionProcessor::ScaleAccel(const AdcSample *sample, u32 *accel)
{
	if (refCalib_.IsValid()) {
		accel[0] = Ratio(accelScale_, sample->accel[0][0], refCalib_.ref_[0]);
		accel[1] = Ratio(accelScale_, sample->accel[1][0], refCalib_.ref_[1]);
		accel[2] = Ratio(accelScale_, sample->accel[2][0], refCalib_.ref_[2]);
	} else {
		accel[0] = Ratio(accelScale_, sample->accel[0][0], sample->accel[0][1]);
		accel[1] = Ratio(accelScale_, sample->accel[1][0], sample->accel[1][1]);
		accel[2] = Ratio(accelScale_, sample->accel[2][0], sample->accel[2][1]);
	}
}

void MotionProcessor::Transform(s32 *out, const s32 *matrix, const s32 *in)
{
	out[0] = matrix[0] * in[0] + matrix[1] * in[1] + matrix[2] * in[2];
	out[1] = matrix[3] * in[0] + matrix[4] * in[1] + matrix[5] * in[2];
	out[2] = matrix[6] * in[0] + matrix[7] * in[1] + matrix[8] * in[2];
}

void MotionProcessor::CalibrateAccel(const u16 *in, s16 *out)
{
	s32 result[3];
	s32 value[3];
	s32 matrix[9];
	s32 shift;

	for (u32 i = 0; i < 3; i++) {
		s32 v = in[i] - params_->accelOffset[i];
		u32 negative;

		if (v >= params_->accelDeadLow[i] && params_->accelDeadHigh[i] >= v)
			v = 0;
		value[i] = v;
		negative = v * params_->accelMatrix[0][i][i] < 0;
		matrix[i] = params_->accelMatrix[negative][0][i];
		matrix[i + 3] = params_->accelMatrix[negative][1][i];
		matrix[i + 6] = params_->accelMatrix[negative][2][i];
	}
	Transform(result, matrix, value);
	shift = params_->accelShift;
	out[0] = result[0] >> shift;
	out[1] = result[1] >> shift;
	out[2] = result[2] >> shift;
}

void MotionProcessor::CalibrateGyro(const u16 *in, s16 *out)
{
	s32 result[3];
	s32 value[3];
	s32 shift;

	for (u32 i = 0; i < 3; i++)
		value[i] = in[i] - params_->gyroOffset[i];
	Transform(result, &params_->gyroMatrix[0][0], value);
	shift = params_->gyroShift;
	out[0] = result[0] >> shift;
	out[1] = result[1] >> shift;
	out[2] = result[2] >> shift;
}

void MotionProcessor::OnMagRaw(const u8 *data)
{
	__SETPRIMASK();
	magRaw_ = data;
	__RESETPRIMASK();
}

void MotionProcessor::OnMagAverage(const u8 *data)
{
	__SETPRIMASK();
	mag_[0].Add(*(const s16 *)(data + 1));
	mag_[1].Add(*(const s16 *)(data + 3));
	mag_[2].Add(*(const s16 *)(data + 5));
	__RESETPRIMASK();
}

void MotionProcessor::ReadDeviceInfo()
{
	u32 serial;

	info_.userVersion = UserVersion();
	info_.loaderVersion = LOADER_INFO->version;
	info_.hwInfo = LOADER_INFO->hwInfo[0] | LOADER_INFO->hwInfo[1] << 8;
	serial = 0xFFFFFFFF;
	for (u32 i = 0; i < 2; i++) {
		const CalibBlock *calib = config_->calibBlocks[i];

		if (calib->IsValid()) {
			info_.calibVersion[i][0] = calib->version[0];
			info_.calibVersion[i][1] = calib->version[1];
			serial = calib->serial;
		} else {
			info_.calibVersion[i][0] = 0xFF;
			info_.calibVersion[i][1] = 0xFF;
		}
	}
	info_.serial = serial;
}

void MotionProcessor::NotifyListeners()
{
	if (settling_)
		return;
	for (u32 i = 0; i < listenerCount_; i++)
		listeners_[i].Notify(&frame_);
}

void MotionProcessor::BootSampleCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnBootSample(result, sample);
}

void MotionProcessor::OnBootSample(s32 result, const AdcSample *sample)
{
	if (bootWait_ != 0) {
		if (--bootWait_ == 0)
			BootMag();
	} else {
		sampler_->SetCallback(SampleCallback, this);
	}
}

void MotionProcessor::StandbySampleCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnStandbySample(result, sample);
}

void MotionProcessor::OnStandbySample(s32 result, const AdcSample *sample)
{
	frame_.ClearFields();
	(this->*sampleHandler_)(sample);
	if (frame_.Fields())
		NotifyListeners();
	if (wakeWait_ != 0 && CanMeasure()) {
		if (--wakeWait_ == 0)
			BootMag();
	} else {
		sampler_->SetCallback(SampleCallback, this);
	}
}

void MotionProcessor::WakeSampleCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnWakeSample(result, sample);
}

void MotionProcessor::OnWakeSample(s32 result, const AdcSample *sample)
{
	frame_.ClearFields();
	(this->*sampleHandler_)(sample);
	if (frame_.Fields())
		NotifyListeners();
	if (wakeWait_ != 0 && CanMeasure()) {
		if (--wakeWait_ == 0) {
			magRunning_ = true;
			magBuffer_ = 0;
			magTimer_.Start(config_->measureTimeout, MeasureTimeoutCallback, this);
			ak8975_->Measure(magData_[magBuffer_], MeasureCallback, this);
			sampler_->SetCallback(SampleCallback, this);
		}
	} else {
		sampler_->SetCallback(SampleCallback, this);
	}
}

void MotionProcessor::SampleCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnSample(result, sample);
}

void MotionProcessor::OnSample(s32 result, const AdcSample *sample)
{
	frame_.ClearFields();
	(this->*sampleHandler_)(sample);
	if (frame_.Fields())
		NotifyListeners();
}

void MotionProcessor::BootMag()
{
	magRunning_ = true;
	magTimer_.Start(config_->bootTimeout, BootTimeoutCallback, this);
	ak8975_->Boot(info_.magInfo, BootCallback, this);
}

void MotionProcessor::BootCallback(void *arg, s32 result)
{
	static_cast<MotionProcessor *>(arg)->OnBoot(result);
}

void MotionProcessor::OnBoot(s32 result)
{
	magTimer_.Stop();
	if (result != 0) {
		printf("MotionProcessor: AK8975 boot failed.\n");
		RetryBoot();
	} else {
		magBuffer_ = 0;
		magTimer_.Start(config_->measureTimeout, MeasureTimeoutCallback, this);
		ak8975_->Measure(magData_[0], MeasureCallback, this);
	}
}

void MotionProcessor::BootTimeoutCallback(void *arg)
{
	static_cast<MotionProcessor *>(arg)->OnBootTimeout();
}

void MotionProcessor::OnBootTimeout()
{
	static bool shown;

	__SETPRIMASK();
	AbortBoot();
	__RESETPRIMASK();
	shown = !shown;
	if (shown)
		printf("MotionProcessor: AK8975 boot timeout.\n");
	RetryBoot();
}

void MotionProcessor::RetryBoot()
{
	if (HasRequest()) {
		magRunning_ = false;
		ProcessRequest();
	} else {
		__SETPRIMASK();
		StopMagNow();
		magTimer_.Start(config_->retryDelay, RetryBootCallback, this);
		__RESETPRIMASK();
	}
}

void MotionProcessor::RetryBootCallback(void *arg)
{
	static_cast<MotionProcessor *>(arg)->OnRetryBoot();
}

void MotionProcessor::OnRetryBoot()
{
	__SETPRIMASK();
	RestartMag();
	__RESETPRIMASK();
}

void MotionProcessor::MeasureCallback(void *arg, s32 result, u8 *data)
{
	static_cast<MotionProcessor *>(arg)->OnMeasure(result, data);
}

void MotionProcessor::OnMeasure(s32 result, u8 *data)
{
	u32 next;

	magTimer_.Stop();
	if (result != 0) {
		printf("MotionProcessor: AK8975 measure failed %d.\n", result);
		if (HasRequest())
			ProcessRequest();
		else
			RetryMeasure();
	} else if (HasRequest()) {
		magRunning_ = false;
		ProcessRequest();
	} else {
		next = magBuffer_ ^ 1;
		magTimer_.Start(config_->measureTimeout, MeasureTimeoutCallback, this);
		ak8975_->Measure(magData_[next], MeasureCallback, this);
		if (magSync_) {
			magSynced_ = true;
			__SETPRIMASK();
			temperature_[0] = 0;
			__RESETPRIMASK();
		}
		(this->*magHandler_)(magData_[magBuffer_]);
		magBuffer_ = next;
	}
}

void MotionProcessor::MeasureTimeoutCallback(void *arg)
{
	static_cast<MotionProcessor *>(arg)->OnMeasureTimeout();
}

void MotionProcessor::OnMeasureTimeout()
{
	static bool shown;

	__SETPRIMASK();
	AbortMeasure();
	__RESETPRIMASK();
	shown = !shown;
	if (shown)
		printf("MotionProcessor: AK8975 Measurement timeout.\n");
	RetryMeasure();
}

void MotionProcessor::RetryMeasure()
{
	if (HasRequest()) {
		magRunning_ = false;
		ProcessRequest();
	} else {
		__SETPRIMASK();
		StopMagNow();
		magTimer_.Start(config_->retryDelay, RetryMeasureCallback, this);
		__RESETPRIMASK();
	}
}

void MotionProcessor::RetryMeasureCallback(void *arg)
{
	static_cast<MotionProcessor *>(arg)->OnRetryMeasure();
}

void MotionProcessor::OnRetryMeasure()
{
	__SETPRIMASK();
	WakeFromStandby();
	__RESETPRIMASK();
}

void MotionProcessor::ReadChecksum()
{
	checksum_ = FirmwareChecksum();
}

void MotionProcessor::ResetBoardType()
{
	boardType_ = 0;
}

s32 MotionProcessor::StartSelfTest(Done::Func func, void *arg)
{
	ResetSelfTest();
	selfTestDone_.Set(func, arg);
	__SETPRIMASK();
	sampler_->StartSelfTest(SelfTestCallback, this);
	__RESETPRIMASK();
	return MOTION_OK;
}

const u16 MotionProcessor::selfTestRange_[3][2] = {
	{ 0x0064, 0xFF9C },
	{ 0x0064, 0xFF9C },
	{ 0x0064, 0xFF9C },
};

void MotionProcessor::FinishSelfTest()
{
	u32 sum[3];
	bool passed = true;

	sampler_->StopSelfTest();
	sampling_ = false;
	memset(sum, 0, sizeof(sum));
	for (u8 i = 0; i < 16; i++) {
		sum[0] += selfTest_[0][i];
		sum[1] += selfTest_[1][i];
		sum[2] += selfTest_[2][i];
	}
	for (u8 i = 0; i < 3; i++) {
		sum[i] >>= 4;
		if (selfTestRange_[i][0] >= sum[i] || sum[i] >= selfTestRange_[i][1])
			passed = false;
	}
	info_.selfTestFailed = !passed;
	info_.accelRef[0] = sum[0];
	info_.accelRef[1] = sum[1];
	info_.accelRef[2] = sum[2];
	sampler_->StartTriggered(SampleCallback, this);
	selfTestDone_(MOTION_OK);
}

void MotionProcessor::SelfTestCallback(void *arg, s32 result, const AdcSample *sample)
{
	static_cast<MotionProcessor *>(arg)->OnSelfTestSample(result, sample);
}

void MotionProcessor::OnSelfTestSample(s32 result, const AdcSample *sample)
{
	if (selfTestCount_ < 16) {
		selfTest_[0][selfTestCount_] = sample->accel[0][1];
		selfTest_[1][selfTestCount_] = sample->accel[1][1];
		selfTest_[2][selfTestCount_++] = sample->accel[2][1];
	}
	if (selfTestCount_ == 16)
		FinishSelfTest();
}

void MotionProcessor::SetRequest(s8 request)
{
	if (request <= MOTION_REQUEST_SLEEP)
		request_ = request;
}

bool MotionProcessor::PostWriteCalib()
{
	return PostTask(WriteCalibTask, this);
}

void MotionProcessor::AbortBoot()
{
	ak8975_->AbortBoot();
}

void MotionProcessor::AbortMeasure()
{
	ak8975_->AbortMeasure();
}

void MotionProcessor::AbortMag()
{
	if (magRunning_) {
		AbortBoot();
		AbortMeasure();
	}
	magTimer_.Stop();
}

void MotionProcessor::SettledCallback(void *arg)
{
	static_cast<MotionProcessor *>(arg)->OnSettled();
}

void MotionProcessor::OnSettled()
{
	__SETPRIMASK();
	settling_ = false;
	__RESETPRIMASK();
}

bool MotionProcessor::IsStill()
{
	u8 still = true;

	for (u16 i = 0; i < 3; i++) {
		u16 mean = refCalib_.value_[i].Mean();
		s32 high = refCalib_.value_[i].max_ - mean;
		s32 low = refCalib_.value_[i].min_ - mean;

		if (StillThreshold() < (u32)(low * low + high * high))
			still = false;
	}
	return still;
}

void MotionProcessor::UpdateRefCalibration()
{
	if (RefCalibOnce() && refCalib_.IsValid())
		return;
	for (u8 i = 0; i < 3; i++)
		refCalib_.ref_[i] = refCalib_.refSum_[i].Mean();
	refCalib_.SetValid();
	status_.SetMagCalibrated();
	status_.SetMagUpdated();
	frame_.SetStatus(status_.Get());
}

bool MotionProcessor::RefCalibEnabled() const
{
	return config_->refCalibMode[measType_] != 0;
}

bool MotionProcessor::RefCalibOnce() const
{
	return (s8)config_->refCalibMode[measType_] == 2;
}

u32 MotionProcessor::StillThreshold() const
{
	return config_->stillThreshold;
}
#endif
