/*
 * MotionProtoPacketEncoder: the motion data packets of the host link.
 *
 * The packer puts up to eight frames of one format and sample type, with consecutive
 * counts, into a packet: after the header, each frame's fields (temperature, then
 * accelerometer, gyroscope, magnetometer, those the frame holds), four bits per frame
 * in the header saying which. A frame whose fields do not all fit keeps those that do
 * in this packet and the rest for the next one (held_). A calibrated frame goes alone,
 * as it is (MotionProtoPacketEncoder).
 */

#include <stdio.h>
#include <string.h>

#include "MotionHif.h"
#include "MotionProtoPacketEncoder.h"

bool MotionProtoPacketPacker::Begin(u8 *packet, u32 size)
{
	if (size < sizeof(MotionDataPacket) - 1)
		return false;
	packet_ = (MotionDataPacket *)packet;
	size_ = size;
	free_ = size - (sizeof(MotionDataPacket) - 1);
	data_ = packet_->data;
	count_ = 0;
	magUpdated_ = false;
	fields_ = 0;
	return true;
}

bool MotionProtoPacketPacker::AddFields(MotionFrame *frame, u32 *added, u32 *missing)
{
	const FrameRaw *raw = (const FrameRaw *)frame->samples_;
	u8 fields;

	*added = 0;
	*missing = 0;
	if (frame->format_ != format_)
		return false;
	if ((frame->status_ & 0xF) != sampleType_)
		return false;
	if ((u16)(frame->count_ - lastCount_) != 1)
		return false;
	fields = frame->fields_;
	if (fields & FRAME_TEMP) {
		if (free_ < 4) {
			*missing = FRAME_TEMP;
			return false;
		}
		AddPair(raw->temp);
		*added |= FRAME_TEMP;
		frame->fields_ &= ~FRAME_TEMP;
	}
	if (fields & FRAME_ACCEL) {
		if (frame->format_ == FRAME_FORMAT_RAW) {
			if (free_ < 12) {
				*missing = FRAME_ACCEL;
				return false;
			}
			AddAxes(raw->accel);
			AddAxes(raw->accelRef);
		} else {
			if (free_ < 6) {
				*missing = FRAME_ACCEL;
				return false;
			}
			AddAxes(raw->accel);
		}
		*added |= FRAME_ACCEL;
		frame->fields_ &= ~FRAME_ACCEL;
	}
	if (fields & FRAME_GYRO) {
		if (free_ < 6) {
			*missing = FRAME_GYRO;
			return false;
		}
		AddAxes(raw->gyro);
		*added |= FRAME_GYRO;
		frame->fields_ &= ~FRAME_GYRO;
	}
	if (fields & FRAME_MAG) {
		if (free_ < 6) {
			*missing = FRAME_MAG;
			return false;
		}
		AddAxes((const u16 *)raw->mag);
		*added |= FRAME_MAG;
		frame->fields_ &= ~FRAME_MAG;
	}
	return true;
}

bool MotionProtoPacketPacker::Add(MotionFrame *frame)
{
	MotionFrame *first = frame;

	if (count_ >= 8)
		return false;
	if (count_ != 0)
		return AddFrame(frame);
	if (held_ != 0) {
		/* the frame held back goes first, when this one follows it */
		if ((u16)(frame->count_ - held_->count_) == 1)
			first = held_;
		held_ = 0;
	}
	format_ = first->format_;
	sampleType_ = first->status_ & 0xF;
	firstCount_ = first->count_;
	lastCount_ = firstCount_ - 1;
	if (first != frame && !AddFrame(first))
		return false;
	return AddFrame(frame);
}

bool MotionProtoPacketPacker::AddFrame(MotionFrame *frame)
{
	u32 added;
	u32 missing;
	bool result = AddFields(frame, &added, &missing);

	if (result) {
		fields_ |= added << (count_ * 4);
		lastCount_ = frame->count_;
		status_ = frame->status_;
		count_++;
		if (IsMagUpdated(status_))
			magUpdated_ = true;
	} else {
		if (added != 0)
			fields_ |= (added | missing) << (count_ * 4);
		if (frame != &saved_) {
			memcpy(&saved_, frame, sizeof(saved_));
			held_ = &saved_;
		}
	}
	return result;
}

u32 MotionProtoPacketPacker::Finish()
{
	u32 size = size_ - free_;

	if (size == sizeof(MotionDataPacket) - 1)
		return 0;
	packet_->id = HIF_PACKET_MOTION_DATA;
	packet_->size = size;
	packet_->format = format_;
	if (magUpdated_)
		SetMagUpdated(&status_);
	packet_->status = status_;
	packet_->count = firstCount_;
	packet_->fields = fields_;
	return size;
}

u32 MotionProtoPacketEncoder::Encode(u8 *packet, u32 size, const MotionFrame *frame)
{
	MotionCalibratedPacket *out = (MotionCalibratedPacket *)packet;
	const FrameCalibrated *samples = (const FrameCalibrated *)frame->samples_;

	if (size < 61) {
		printf("MotionProtoPacketEncoder::Encode() size too small: %d.n", size);
		return 0;
	}
	out->id = HIF_PACKET_MOTION_DATA;
	out->size = 61;
	out->format = FRAME_FORMAT_CALIBRATED;
	out->gyro[0] = samples->gyro[0];
	out->gyro[1] = samples->gyro[1];
	out->gyro[2] = samples->gyro[2];
	out->accel[0] = samples->accel[0];
	out->accel[1] = samples->accel[1];
	out->accel[2] = samples->accel[2];
	/* Sony's: the three axes all go to the first */
	out->mag[0] = samples->mag[0];
	out->mag[0] = samples->mag[1];
	out->mag[0] = samples->mag[2];
	out->status = frame->status_;
	out->count = frame->count_;
	out->reserved = 0;
	out->sequence = sequence_++;
	return 61;
}
