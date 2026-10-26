/*
 * MotionHif: the host interface: the Vita's commands and the reports back.
 */
#ifndef MOTIONHIF_H
#define MOTIONHIF_H

#include <string.h>

extern "C" {
#include "cortexm3_macro.h"
}
#include "stm32f10x_type.h"
#include "motion.h"
#include "MotionFrameUtil.h"
#include "MotionProcessor.h"
#include "MotionProtoPacketEncoder.h"
#include "RingBuffer.h"

/* the commands (packet ids) the host sends */
enum {
	HIF_CMD_NOP,
	HIF_CMD_SYNC,
	HIF_CMD_START_MEASURE,
	HIF_CMD_READ_VERSION,
	HIF_CMD_READ_CALIB,
	HIF_CMD_WRITE_CALIB,
	HIF_CMD_SET_SAMPLE_TYPE,
	HIF_CMD_RESTART = 8,
	HIF_CMD_RESET,
	HIF_CMD_READ_CALIB_WRITING_STATUS,
	HIF_CMD_READ_DEVICE_INFO = 12,
	HIF_CMD_SWITCH_FRAME_SIZE = 14,
	HIF_CMD_SELECT_MEAS_TYPE,
	HIF_COMMANDS,
};

/* the packets sent back */
enum {
	HIF_PACKET_RESULT_CODE = 1,
	HIF_PACKET_MOTION_DATA,
	HIF_PACKET_VERSION,
	HIF_PACKET_CALIB_DATA,
	HIF_PACKET_CALIB_WRITING_STATUS,
	HIF_PACKET_DEVICE_INFO = 7,
};

/* a command with a one-byte argument */
struct HifPacket {
	u8 id;
	u8 size;
	u8 value;
	u8 sequence;			/* HIF_CMD_READ_CALIB: 0 starts the block */
};

struct ResultCode {
	s16 code;
	s16 command;

	void Set(s16 result, s16 cmd)
	{
		code = result;
		command = cmd;
	}
};

struct ResultCodePacket {
	u8 id;
	u8 size;
	ResultCode body;

	u8 Build(s16 code, s16 command)
	{
		id = HIF_PACKET_RESULT_CODE;
		size = 6;
		body.Set(code, command);
		return size;
	}
};

struct VersionPacket {
	u8 id;
	u8 size;
	u8 version[sizeof(VersionInfo)];

	void Build(const VersionInfo *info)
	{
		id = HIF_PACKET_VERSION;
		size = 10;
		memcpy(version, info, sizeof *info);
	}
};

/* a part of a calibration block: read (HIF_PACKET_CALIB_DATA) or written (HIF_CMD_WRITE_CALIB) */
struct CalibPacket {
	u8 id;
	u8 size;
	u8 block;
	u16 totalSize;
	u16 offset;
	u8 dataSize;
	u8 data[1];

	void Set(u8 packetId, u8 blockId, u16 total, u16 dataOffset)
	{
		id = packetId;
		block = blockId;
		totalSize = total;
		offset = dataOffset;
	}

	void SetHeader(u8 blockId, u16 total, u16 dataOffset)
	{
		Set(HIF_PACKET_CALIB_DATA, blockId, total, dataOffset);
	}

	u32 Capacity(u32 free)
	{
		return free - 8;
	}

	u8 SetData(const void *src, u32 n)
	{
		memcpy(data, src, n);
		dataSize = n;
		size = n + 8;
		return size;
	}

	void CopyData(void *dst, u32 n)
	{
		memcpy(dst, data, n);
	}
} __attribute__((packed));

struct CalibWritingStatusPacket {
	u8 id;
	u8 size;
	s8 status;

	void Build(u8 writingStatus)
	{
		id = HIF_PACKET_CALIB_WRITING_STATUS;
		size = 3;
		status = writingStatus;
	}
};

struct DeviceInfoPacket {
	u8 id;
	u8 size;
	u8 info[sizeof(DeviceInfo)];

	u8 Build(const DeviceInfo *deviceInfo)
	{
		id = HIF_PACKET_DEVICE_INFO;
		size = 36;
		memcpy(info, deviceInfo, sizeof *deviceInfo);
		return size;
	}
};

class MotionHif {
public:
	typedef void (MotionHif::*Command)(const HifPacket *packet);

	MotionHif(MotionProcessor *processor, MotionFrame *frames, u32 frameCount,
		  const u32 *thresholds);
	void Reset();
	u8 *BuildFrame(u32 *size);
	void ParseFrame(u8 *frame);
	void RegisterListener();
	virtual void OnFrame(const MotionFrame *frame);
	virtual void OnSend() {}
	virtual void OnReady() {}

	void SetFrameSize(u32 size)
	{
		frameSize_ = size;
	}

	u32 FrameSize()
	{
		return frameSize_;
	}

private:
	void ReportResultCode(s16 code, s16 command);
	void CmdNop(const HifPacket *packet);
	void CmdSync(const HifPacket *packet);
	void CmdStartMeasure(const HifPacket *packet);
	void CmdReadVersion(const HifPacket *packet);
	void CmdReadCalib(const HifPacket *packet);
	void CmdWriteCalib(const HifPacket *packet);
	void CmdSetSampleType(const HifPacket *packet);
	void CmdSwitchFrameSize(const HifPacket *packet);
	void CmdSelectMeasType(const HifPacket *packet);
	void CmdRestart(const HifPacket *packet);
	void CmdReset(const HifPacket *packet);
	void CmdReadCalibWritingStatus(const HifPacket *packet);
	void CmdReadDeviceInfo(const HifPacket *packet);
	void ReportCalibDataReadError(CalibPacket *packet, u8 block, s32 result);
	void CmdReadCalibWritingStatus();
	void EncodeRawFrame();
	void EncodeFrames();
	static void FrameCallback(void *arg, const MotionFrame *frame);
	static void WriteCalibCallback(void *arg, s32 result);
	void OnWriteCalibDone(s32 result);
	void CmdInvalid(const HifPacket *packet);
	static void SelfTestCallback(void *arg, s32 result);
	void OnSelfTestDone(s32 result);
	bool ReportDeviceInfo();

	void ClearFrames()
	{
		__SETPRIMASK();
		frames_.Clear();
		rawFrame_ = 0;
		__RESETPRIMASK();
	}

	void ClearReports()
	{
		deviceInfoPending_ = false;
		resultPending_ = false;
	}

	MotionProcessor *processor_;
	const u32 *thresholds_;		/* frames queued before the host is told, per sample type */
	u32 threshold_;
	s32 listenerId_;
	bool isMeasuring_;
	bool isMeasuretypeStop_;
	RingBuffer<MotionFrame> frames_;
	const MotionFrame *rawFrame_;	/* a calibrated frame, sent alone */
	u8 txBuffer_[MOTION_FRAME_MAX];
	MotionFrameWriter writer_;
	MotionProtoPacketEncoder encoder_;
	MotionProtoPacketPacker packer_;
	MotionFrameReader reader_;
	s8 calibWritingStatus_;		/* 3 none, 1 receiving, 2 writing, 0 written, -1 failed */
	u8 calibReadStatus_;		/* set, never read */
	bool deviceInfoPending_;
	bool resultPending_;
	s32 result_;
	u32 frameSize_;

	static const Command commands_[HIF_COMMANDS];
	static const s8 sampleTypes_[MOTION_TYPES];
	static const s8 measTypes_[4];
};

#endif
