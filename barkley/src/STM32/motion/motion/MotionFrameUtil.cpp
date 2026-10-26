/*
 * MotionFrameUtil: a frame's sum is the low byte of the sum of its bytes up to the end
 * mark. A frame read is checked (its size, its end mark, its sum) before its packets
 * are taken one by one, up to the zero that ends them.
 */

#include <stdio.h>
#include <string.h>

#include "MotionFrameUtil.h"
#include "MotionProcessor.h"

u8 MotionFrameUtil::CheckSum(const u8 *data, u32 size)
{
	u32 sum = 0;

	while (size-- != 0)
		sum += *data++;
	return sum;
}

/* the size byte, the trailer, and the rest of the frame cleared; the frame's size */
u8 MotionFrameWriter::Finish()
{
	u32 length = size_ + MOTION_FRAME_HEADER;

	buffer_[0] = length + MOTION_FRAME_TRAILER;
	FrameTrailer *trailer = (FrameTrailer *)(buffer_ + size_ + MOTION_FRAME_HEADER);
	trailer->Set(0, CheckSum(buffer_, length));
	memset(buffer_ + size_ + MOTION_FRAME_HEADER + MOTION_FRAME_TRAILER, 0,
	       MOTION_FRAME_MAX - MOTION_FRAME_HEADER - MOTION_FRAME_TRAILER - size_);
	return buffer_[0];
}

s32 MotionFrameReader::Open(u8 *frame)
{
	u32 size = frame[0];

	if (size > maxSize_ + MOTION_FRAME_HEADER + MOTION_FRAME_TRAILER ||
	    size < MOTION_FRAME_HEADER + MOTION_FRAME_TRAILER) {
		printf("Frame Size Error: %u.\n", size);
		return MOTION_ERROR_FRAME_SIZE;
	}
	FrameTrailer *trailer = (FrameTrailer *)(frame + size - MOTION_FRAME_TRAILER);
	if (trailer->eof != 0) {
		printf("Frame EOF Error: packetId=%u.\n", trailer->eof);
		return MOTION_ERROR_FRAME_EOF;
	}
	u8 sum = CheckSum(frame, size - MOTION_FRAME_TRAILER);
	if (trailer->checkSum != sum) {
		printf("Frame CheckSum Error: packet=%u, calc=%u.\n", trailer->checkSum, sum);
		return MOTION_ERROR_FRAME_CHECKSUM;
	}
	remaining_ = size - MOTION_FRAME_HEADER;
	packet_ = frame + MOTION_FRAME_HEADER;
	return 0;
}

/* the next packet (its id, its size, its data), or 0 at the end */
u8 *MotionFrameReader::Next()
{
	u8 *packet = packet_;

	if (packet[0] == 0)
		return 0;
	u8 size = packet[1];
	packet_ += size;
	remaining_ -= size;
	return packet;
}
