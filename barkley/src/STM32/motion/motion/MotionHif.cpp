/*
 * MotionHif: the host interface: the Vita's commands and the reports back.
 *
 * The link (HifSpi) exchanges frames with the host: each frame it receives is parsed
 * into its commands (ParseFrame), and each frame it sends is built here (BuildFrame):
 * the reports pending (the device information, a result), then, while measuring, the
 * motion frames the processor completed: a calibrated frame alone, the others packed
 * from the queue. The host is told (OnReady) when a calibrated frame or the sample
 * type's number of frames is waiting.
 */

#include <stdio.h>
#include <string.h>

#include "MotionHif.h"

#if MOTION_IMAGE == 0
const MotionHif::Command MotionHif::commands_[HIF_CMD_SELECT_MEAS_TYPE] = {
#else
const MotionHif::Command MotionHif::commands_[HIF_COMMANDS] = {
#endif
	&MotionHif::CmdNop,
	&MotionHif::CmdSync,
	&MotionHif::CmdStartMeasure,
	&MotionHif::CmdReadVersion,
	&MotionHif::CmdReadCalib,
	&MotionHif::CmdWriteCalib,
	&MotionHif::CmdSetSampleType,
	&MotionHif::CmdInvalid,
	&MotionHif::CmdRestart,
	&MotionHif::CmdReset,
	&MotionHif::CmdReadCalibWritingStatus,
	&MotionHif::CmdInvalid,
	&MotionHif::CmdReadDeviceInfo,
	&MotionHif::CmdInvalid,
	&MotionHif::CmdSwitchFrameSize,
#if MOTION_IMAGE != 0
	&MotionHif::CmdSelectMeasType,
#endif
};

#if MOTION_IMAGE == 0
const s8 MotionHif::sampleTypes_[MOTION_TYPES] = { 0, 1, 2, 3, 4, 5 };
#else
const s8 MotionHif::sampleTypes_[MOTION_TYPES] = { 0, 1, 2, 3, 4, 5, 6, 7 };

const s8 MotionHif::measTypes_[4] = {
	MOTION_STATE_RUN, MOTION_STATE_STANDBY, MOTION_STATE_IDLE, MOTION_STATE_SLEEP,
};
#endif

/* a calibration block on its way to or from the flash */
static u8 calibBuffer[0x200];

MotionHif::MotionHif(MotionProcessor *processor, MotionFrame *frames, u32 frameCount,
		     const u32 *thresholds)
	: processor_(processor), thresholds_(thresholds), threshold_(thresholds[1]),
#if MOTION_IMAGE == 0
	  listenerId_(-1), frames_(frames, frameCount),
#else
	  listenerId_(-1), isMeasuretypeStop_(false), frames_(frames, frameCount),
#endif
	  calibWritingStatus_(3), calibReadStatus_(2)
{
	writer_.SetMaxSize(56);
	reader_.SetMaxSize(56);
}

void MotionHif::Reset()
{
	isMeasuring_ = false;
	ClearFrames();
	writer_.Begin(txBuffer_);
#if MOTION_IMAGE == 0
	deviceInfoPending_ = false;
#else
	ClearReports();
#endif
}

u8 *MotionHif::BuildFrame(u32 *size)
{
	if (deviceInfoPending_ && ReportDeviceInfo())
		deviceInfoPending_ = false;
#if MOTION_IMAGE != 0
	if (resultPending_) {
		ReportResultCode(result_, HIF_CMD_WRITE_CALIB);
		resultPending_ = false;
	}
#endif
	if (isMeasuring_) {
		OnSend();
		if (rawFrame_ != 0) {
			EncodeRawFrame();
			rawFrame_ = 0;
		} else {
			EncodeFrames();
			if (frames_.Count() >= threshold_)
				OnReady();
		}
	}
	*size = writer_.Finish();
	writer_.Begin(txBuffer_);
	return txBuffer_;
}

void MotionHif::ParseFrame(u8 *frame)
{
	s32 result = reader_.Open(frame);
	const HifPacket *packet;

	if (result != 0) {
		printf("Frame Error: %d.\n", result);
		ReportResultCode(result, HIF_CMD_NOP);
		return;
	}
	while ((packet = (const HifPacket *)reader_.Next()) != 0) {
		/* image 00: an id of HIF_CMD_SELECT_MEAS_TYPE reads past its table */
		if (packet->id < HIF_COMMANDS && commands_[packet->id] != &MotionHif::CmdInvalid) {
			(this->*commands_[packet->id])(packet);
		} else {
			printf("Invalid Packet ID: %d.\n", packet->id);
			ReportResultCode(MOTION_ERROR_PACKET_ID, packet->id);
		}
	}
}

void MotionHif::ReportResultCode(s16 code, s16 command)
{
	u32 free;
	u8 *payload = writer_.GetPayload(&free);

	if (free >= sizeof(ResultCodePacket))
		writer_.AddPayloadSize(((ResultCodePacket *)payload)->Build(code, command));
	else
		printf("MotionHif::ReportResultCode() fail, size=%u.\n", free);
}

void MotionHif::CmdNop(const HifPacket *packet)
{
}

void MotionHif::CmdSync(const HifPacket *packet)
{
}

void MotionHif::CmdStartMeasure(const HifPacket *packet)
{
	if (isMeasuring_)
		return;
	isMeasuring_ = true;
#if MOTION_IMAGE != 0
	isMeasuretypeStop_ = false;
#endif
	ClearFrames();
	processor_->Start();
}

void MotionHif::CmdReadVersion(const HifPacket *packet)
{
	VersionInfo version;
	u32 free;
	u8 *payload = writer_.GetPayload(&free);

	if (free >= sizeof(VersionPacket)) {
		processor_->GetVersion(&version);
		((VersionPacket *)payload)->Build(&version);
		writer_.AddPayloadSize(sizeof(VersionPacket));
	} else {
		printf("MotionHif::CmdReadVersion() fail(), size=%u.\n", free);
		ReportResultCode(MOTION_ERROR_NO_SPACE, HIF_CMD_READ_VERSION);
	}
}

void MotionHif::CmdReadCalib(const HifPacket *packet)
{
	static u32 calibSize;
	static u32 calibOffset;
	CalibPacket *reply;
	u32 free;
	u32 n;
	u32 size;
	s32 result;

	isMeasuring_ = false;
#if MOTION_IMAGE != 0
	processor_->Stop();		/* image 00: ReadCalib stops it */
#endif
	reply = (CalibPacket *)writer_.GetPayload(&free);
	if (free < 12) {
		printf("MotionHif::CmdReadCalib ERROR, size=%u.\n", free);
		ReportResultCode(MOTION_ERROR_NO_SPACE, HIF_CMD_READ_CALIB);
		return;
	}
	if (packet->sequence == 0) {
		result = processor_->ReadCalib(packet->value, calibBuffer, sizeof(calibBuffer), &calibSize);
		if (result != MOTION_OK) {
			ReportCalibDataReadError(reply, packet->value, result);
			return;
		}
		calibOffset = 0;
	}
	reply->SetHeader(packet->value, calibSize, calibOffset);
	n = reply->Capacity(free);
#if MOTION_IMAGE == 0
	if (n + calibOffset >= calibSize)
		n = calibSize - calibOffset;
	size = reply->SetData(calibBuffer + calibOffset, n);
	calibOffset += n;
	writer_.AddPayloadSize(size);
#else
	if (calibSize >= calibOffset) {
		if (n + calibOffset >= calibSize)
			n = calibSize - calibOffset;
		size = reply->SetData(calibBuffer + calibOffset, n);
		calibOffset += n;
		writer_.AddPayloadSize(size);
	} else {
		ReportCalibDataReadError(reply, packet->value, MOTION_ERROR_INVALID);
	}
#endif
}

void MotionHif::CmdWriteCalib(const HifPacket *packet)
{
	CalibPacket *request = (CalibPacket *)packet;
	u32 offset;
	u32 total;
	u32 n;
	bool last;

	isMeasuring_ = false;
	processor_->Stop();
	if (request->block >= 4) {
		printf("MotionHif::CmdWriteCalib() block %d invalid.\n", request->block);
		ReportResultCode(MOTION_ERROR_INVALID, HIF_CMD_WRITE_CALIB);
		return;
	}
	offset = request->offset;
	total = request->totalSize;
	n = request->dataSize;
	if (total > sizeof(calibBuffer)) {
		printf("Write Calib ERROR, totalSize=%d.\n", request->totalSize);
		ReportResultCode(MOTION_ERROR_SIZE, HIF_CMD_WRITE_CALIB);
		return;
	}
	if (n + offset < total) {
		last = false;
	} else {
		n = total - offset;
		last = true;
	}
	request->CopyData(calibBuffer + offset, n);
	if (last) {
		if (processor_->WriteCalib(request->block, calibBuffer, total, WriteCalibCallback, this) != MOTION_OK) {
			printf("Write Calib ERROR, flashing.\n");
			calibWritingStatus_ = -1;
		} else {
			calibWritingStatus_ = 2;
		}
	} else {
		calibWritingStatus_ = 1;
	}
	CmdReadCalibWritingStatus();
}

void MotionHif::CmdSetSampleType(const HifPacket *packet)
{
	u8 type = packet->value;

	if (type >= MOTION_TYPES) {
		printf("MotionHif::CmdSetSampleType() type %d invalid.\n", type);
		ReportResultCode(MOTION_ERROR_SAMPLE_TYPE, HIF_CMD_SET_SAMPLE_TYPE);
		return;
	}
	threshold_ = thresholds_[type];
	processor_->SetMeasType(sampleTypes_[type]);
}

void MotionHif::CmdSwitchFrameSize(const HifPacket *packet)
{
	u16 size = packet->value;

	if (size >= 59 && size <= MOTION_FRAME_MAX) {
		SetFrameSize(packet->value);
		writer_.SetMaxSize(packet->value - (MOTION_FRAME_HEADER + MOTION_FRAME_TRAILER));
		reader_.SetMaxSize(packet->value - (MOTION_FRAME_HEADER + MOTION_FRAME_TRAILER));
	} else {
		printf("MotionHif::CmdSwitchFrameSize() FrameSize %d invalid.\n", size);
		ReportResultCode(MOTION_ERROR_FRAME_LENGTH, HIF_CMD_SWITCH_FRAME_SIZE);
	}
}

#if MOTION_IMAGE != 0
void MotionHif::CmdSelectMeasType(const HifPacket *packet)
{
	u16 index = packet->value;
	s8 type;

	if (index >= 4) {
		printf("MotionHif::CmdSelectMeasType() MeasType %d invalid.\n", index);
		ReportResultCode(MOTION_ERROR_MEAS_TYPE, HIF_CMD_SELECT_MEAS_TYPE);
		return;
	}
	type = measTypes_[index];
	if (isMeasuring_ || isMeasuretypeStop_ || (!isMeasuring_ && type == MOTION_STATE_SLEEP)) {
		switch (type) {
		case MOTION_STATE_RUN:
		case MOTION_STATE_STANDBY:
		case MOTION_STATE_IDLE:
			if (isMeasuretypeStop_) {
				isMeasuring_ = true;
				isMeasuretypeStop_ = false;
			}
			break;
		case MOTION_STATE_SLEEP:
			ClearFrames();
			isMeasuretypeStop_ = true;
			break;
		}
		processor_->SetState(type);
	} else {
		printf("MotionHif::CmdSelectMeasType() ErrTransition isMeasuring_(%d),isMeasuretypeStop_(%d),type(%d).\n",
		       isMeasuring_, isMeasuretypeStop_, type);
		ReportResultCode(MOTION_ERROR_TRANSITION, HIF_CMD_SELECT_MEAS_TYPE);
	}
}
#endif

void MotionHif::CmdRestart(const HifPacket *packet)
{
	isMeasuring_ = false;
#if MOTION_IMAGE == 0
	deviceInfoPending_ = false;
#else
	isMeasuretypeStop_ = false;
	ClearReports();
#endif
	MotionProcessor::RestartSystem();
}

void MotionHif::CmdReset(const HifPacket *packet)
{
	isMeasuring_ = false;
#if MOTION_IMAGE == 0
	deviceInfoPending_ = false;
#else
	isMeasuretypeStop_ = false;
	ClearReports();
#endif
	MotionProcessor::ResetSystem();
}

void MotionHif::CmdReadCalibWritingStatus(const HifPacket *packet)
{
	CmdReadCalibWritingStatus();
}

void MotionHif::CmdReadDeviceInfo(const HifPacket *packet)
{
#if MOTION_IMAGE != 0
	if (isMeasuretypeStop_) {
		printf("MotionHif::CmdReadDeviceInfo() Now Measure Stopping.\n");
		ReportResultCode(MOTION_ERROR_STOPPING, HIF_CMD_READ_DEVICE_INFO);
		return;
	}
#endif
	isMeasuring_ = false;
	ClearFrames();
	processor_->Stop();
	processor_->StartSelfTest(SelfTestCallback, this);
}

void MotionHif::ReportCalibDataReadError(CalibPacket *packet, u8 block, s32 result)
{
	printf("ReportCalibDataReadError(), block=%u\n", block);
	packet->SetHeader(block, 0, 0);
	writer_.AddPayloadSize(packet->SetData(&result, sizeof(result)));
}

void MotionHif::CmdReadCalibWritingStatus()
{
	u32 free;
	u8 *payload = writer_.GetPayload(&free);

	if (free >= sizeof(CalibWritingStatusPacket)) {
		((CalibWritingStatusPacket *)payload)->Build(calibWritingStatus_);
		writer_.AddPayloadSize(sizeof(CalibWritingStatusPacket));
	} else {
		printf("MotionHif::CmdReadCalibWritingStatus() fail, size=%u.\n", free);
		ReportResultCode(MOTION_ERROR_NO_SPACE, HIF_CMD_WRITE_CALIB);
	}
}

void MotionHif::EncodeRawFrame()
{
	u32 free;
	u8 *payload = writer_.GetPayload(&free);

	free = encoder_.Encode(payload, free, rawFrame_);
	writer_.AddPayloadSize(free);
}

void MotionHif::EncodeFrames()
{
	u32 free;
	u8 *payload = writer_.GetPayload(&free);
	MotionFrame *frame;

	if (!packer_.Begin(payload, free))
		return;
	do
		frame = frames_.Pop();
	while (frame != 0 && packer_.Add(frame));
	free = packer_.Finish();
	writer_.AddPayloadSize(free);
}

void MotionHif::RegisterListener()
{
	if (listenerId_ == -1)
		listenerId_ = processor_->AddListener(FrameCallback, this);
}

void MotionHif::FrameCallback(void *arg, const MotionFrame *frame)
{
	static_cast<MotionHif *>(arg)->OnFrame(frame);
}

void MotionHif::OnFrame(const MotionFrame *frame)
{
	const MotionFrame *waiting;

	if (frame->format_ == 3) {
		waiting = rawFrame_;
		rawFrame_ = frame;
		if (waiting == 0)
			OnReady();
	} else {
		__SETPRIMASK();
		frames_.Push(frame);
		if (frames_.Count() == threshold_)
			OnReady();
		__RESETPRIMASK();
		rawFrame_ = 0;
	}
}

void MotionHif::WriteCalibCallback(void *arg, s32 result)
{
	static_cast<MotionHif *>(arg)->OnWriteCalibDone(result);
}

void MotionHif::OnWriteCalibDone(s32 result)
{
#if MOTION_IMAGE == 0
	if (result != MOTION_OK)
		calibWritingStatus_ = -1;
	else
		calibWritingStatus_ = 0;
#else
	if (result != MOTION_OK) {
		if (result == MOTION_ERROR_VERIFY)
			result_ = MOTION_ERROR_VERIFY;
		else
			result_ = MOTION_ERROR_FLASH;
		resultPending_ = true;
		calibWritingStatus_ = -1;
	} else {
		calibWritingStatus_ = 0;
	}
#endif
}

void MotionHif::CmdInvalid(const HifPacket *packet)
{
}

void MotionHif::SelfTestCallback(void *arg, s32 result)
{
	static_cast<MotionHif *>(arg)->OnSelfTestDone(result);
}

void MotionHif::OnSelfTestDone(s32 result)
{
	if (result == MOTION_OK)
		deviceInfoPending_ = true;
}

bool MotionHif::ReportDeviceInfo()
{
	u32 free;
	u8 *payload = writer_.GetPayload(&free);

	if (free >= sizeof(DeviceInfoPacket)) {
		writer_.AddPayloadSize(((DeviceInfoPacket *)payload)->Build(processor_->GetDeviceInfo()));
		return true;
	}
	printf("MotionHif::ReportDeviceInfo fail, size=%u.\n", free);
	return false;
}
