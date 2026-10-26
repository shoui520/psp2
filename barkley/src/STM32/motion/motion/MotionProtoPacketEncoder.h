/*
 * MotionProtoPacketEncoder: the motion data packets of the host link: one calibrated
 * frame as it is (MotionProtoPacketEncoder), or up to eight frames of the other types
 * packed, each with the fields it brings (MotionProtoPacketPacker).
 */
#ifndef MOTIONPROTOPACKETENCODER_H
#define MOTIONPROTOPACKETENCODER_H

#include "stm32f10x_type.h"
#include "MotionFrameUtil.h"

/* the packed frames: a header, then each frame's fields in turn */
struct MotionDataPacket {
	u8 id;
	u8 size;
	u8 format;
	u8 status;
	u16 count;			/* the first frame's */
	u32 fields;			/* four bits per frame: the fields it brought */
	u8 data[1];
} __attribute__((packed));

/* a calibrated frame */
struct MotionCalibratedPacket {
	u8 id;
	u8 size;
	u8 format;
	s16 gyro[3];
	s16 accel[3];
	s16 mag[3];
	u8 status;
	u16 count;
	u8 reserved;
	u8 sequence;
} __attribute__((packed));

class MotionProtoPacketEncoder {
public:
	MotionProtoPacketEncoder() : sequence_(0) {}

	u32 Encode(u8 *packet, u32 size, const MotionFrame *frame);

private:
	u8 sequence_;
};

class MotionProtoPacketPacker {
public:
	MotionProtoPacketPacker() : held_(0) {}

	bool Begin(u8 *packet, u32 size);
	bool Add(MotionFrame *frame);
	u32 Finish();

private:
	bool AddFields(MotionFrame *frame, u32 *added, u32 *missing);
	bool AddFrame(MotionFrame *frame);

	void AddPair(const u16 *value)
	{
		u16 *data = (u16 *)data_;

		data[0] = value[0];
		data[1] = value[1];
		data_ += 4;
		free_ -= 4;
	}

	void AddAxes(const u16 *value)
	{
		u16 *data = (u16 *)data_;

		data[0] = value[0];
		data[1] = value[1];
		data[2] = value[2];
		data_ += 6;
		free_ -= 6;
	}

#if MOTION_IMAGE != 0
	bool IsMagUpdated(s8 status)
	{
		return status < 0;
	}

	void SetMagUpdated(u8 *status)
	{
		*status |= 0x80;
	}
#endif

	MotionDataPacket *packet_;
	u32 size_;
	u32 free_;
	u8 *data_;
	u32 count_;
	u32 fields_;
	u8 format_;
	u8 sampleType_;
	u16 firstCount_;
	u16 lastCount_;
	u8 status_;
	MotionFrame *held_;		/* a frame that did not fit, for the next packet */
	MotionFrame saved_;
#if MOTION_IMAGE != 0
	bool magUpdated_;		/* a frame's status had bit 7 (image 01) */
#endif
};

#endif
