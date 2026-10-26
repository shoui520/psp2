/*
 * MotionFrameUtil: the motion frames the processor fills for the Vita: a header (the
 * frame's format, the sensors' status, a counter, which fields it holds) and the
 * samples, laid out as the frame's format says; and the frames of the host link that
 * carry them: a size byte, packets (an id, a size, the data), an end mark and a sum.
 */
#ifndef MOTIONFRAMEUTIL_H
#define MOTIONFRAMEUTIL_H

#include "stm32f10x_type.h"
#include "Debug.h"

/* the file name IAR gave this header's asserts (its path in Sony's build) */
#define MOTIONFRAMEUTIL_FILE	"C:\\work\\svn\\NGP\\barkley\\trunk\\src\\STM32\\EWARMv5\\..\\motion\\motion\\include\\MotionFrameUtil.h"

/* what the ADC samples, every sample period: the accelerometer's axes, each beside its
   reference, the gyroscope's axes and the temperature */
struct AdcSample {
	u16 accel[3][2];
	u16 gyro[3];
	u16 temp;
};

/* the frame's samples: averages of the raw values (FrameRaw also gives the
   accelerometer's references) */
struct FrameRaw {
	u16 temp[2];
	u16 accel[3];
	u16 gyro[3];
	s16 mag[3];
	u16 accelRef[3];

	void SetTemp(const u16 *value)
	{
		temp[0] = value[0];
		temp[1] = value[1];
	}

	void SetAccel(const AdcSample *sample)
	{
		accel[0] = sample->accel[0][0];
		accel[1] = sample->accel[1][0];
		accel[2] = sample->accel[2][0];
		accelRef[0] = sample->accel[0][1];
		accelRef[1] = sample->accel[1][1];
		accelRef[2] = sample->accel[2][1];
	}

	void SetGyro(const AdcSample *sample)
	{
		gyro[0] = sample->gyro[0];
		gyro[1] = sample->gyro[1];
		gyro[2] = sample->gyro[2];
	}

	void SetMag(const s16 *value)
	{
		mag[0] = value[0];
		mag[1] = value[1];
		mag[2] = value[2];
	}
};

/* the averages (FrameAverage) or the calibrated values (FrameCalibrated) */
template <typename T>
struct FrameSamples {
	u16 temp[2];
	T accel[3];
	T gyro[3];
	s16 mag[3];

	void SetTemp(const u16 *value)
	{
		temp[0] = value[0];
		temp[1] = value[1];
	}

	void SetAccel(const T *value)
	{
		accel[0] = value[0];
		accel[1] = value[1];
		accel[2] = value[2];
	}

	void SetGyro(const T *value)
	{
		gyro[0] = value[0];
		gyro[1] = value[1];
		gyro[2] = value[2];
	}

	void SetMag(const s16 *value)
	{
		mag[0] = value[0];
		mag[1] = value[1];
		mag[2] = value[2];
	}
};

typedef FrameSamples<u16> FrameAverage;
typedef FrameSamples<s16> FrameCalibrated;

/* the fields a frame holds */
enum {
	FRAME_TEMP	= 1 << 0,
	FRAME_ACCEL	= 1 << 1,
	FRAME_GYRO	= 1 << 2,
	FRAME_MAG	= 1 << 3,
};

/* the frame formats: which samples a frame holds */
enum {
	FRAME_FORMAT_RAW,		/* FrameRaw */
	FRAME_FORMAT_AVERAGE,		/* FrameAverage */
	FRAME_FORMAT_MAG,
	FRAME_FORMAT_CALIBRATED,	/* FrameCalibrated */
};

/* the sensors' status byte of a frame: the sample type, the processor's state, the
   magnetometer's calibration */
class FrameStatus {
public:
	FrameStatus() : value_(0) {}

	void SetSampleType(u8 type)
	{
		value_ = type | (value_ & 0xF0);
	}

	void SetState(s8 state)
	{
		value_ = (value_ & 0xCF) | (state << 4);
	}

	void SetMagCalibrated()
	{
		value_ |= 0x40;
	}

	void SetMagUpdated()
	{
		value_ |= 0x80;
	}

	void ClearMag()
	{
		value_ &= 0x3F;
	}

	void ClearMagUpdated()
	{
		value_ &= ~0x80;
	}

	u8 Get() const
	{
		return value_;
	}

private:
	u8 value_;
};

class MotionFrame {
public:
	void SetFormat(u8 format)
	{
		format_ = format;
	}

	void SetStatus(u8 status)
	{
		status_ = status;
	}

	void SetCount(u16 count)
	{
		count_ = count;
	}

	u8 Fields() const
	{
		return fields_;
	}

	void ClearFields()
	{
		fields_ = 0;
	}

	template <class Samples, typename T> void SetTemp(const T *value)
	{
		reinterpret_cast<Samples *>(samples_)->SetTemp(value);
		fields_ |= FRAME_TEMP;
	}

	template <class Samples, typename T> void SetAccel(const T *value)
	{
		reinterpret_cast<Samples *>(samples_)->SetAccel(value);
		fields_ |= FRAME_ACCEL;
	}

	template <class Samples, typename T> void SetGyro(const T *value)
	{
		reinterpret_cast<Samples *>(samples_)->SetGyro(value);
		fields_ |= FRAME_GYRO;
	}

	template <class Samples, typename T> void SetMag(const T *value)
	{
		reinterpret_cast<Samples *>(samples_)->SetMag(value);
		fields_ |= FRAME_MAG;
	}

	u8 format_;
	u8 status_;
	u16 count_;
	u8 fields_;
	u8 reserved_;
	u16 samples_[14];
};

/* the frames of the host link */
enum {
	MOTION_FRAME_HEADER	= 1,		/* the size byte */
	MOTION_FRAME_TRAILER	= 2,		/* the end mark and the sum */
	MOTION_FRAME_MAX	= 128,
};

/* a frame ends with a zero and the sum of the bytes before it */
struct FrameTrailer {
	u8 eof;
	u8 checkSum;

	void Set(u8 mark, u8 sum)
	{
		eof = mark;
		checkSum = sum;
	}
};

class MotionFrameUtil {
protected:
	u8 CheckSum(const u8 *data, u32 size);
};

/* builds a frame of packets in a buffer */
class MotionFrameWriter : public MotionFrameUtil {
public:
	MotionFrameWriter() : buffer_(0), size_(0) {}

	void Begin(u8 *buffer)
	{
		buffer_ = buffer;
		size_ = 0;
	}

	void SetMaxSize(u32 size)
	{
		maxSize_ = size;
	}

	/* where the next packet goes, and the room left for it */
	u8 *GetPayload(u32 *free)
	{
		*free = maxSize_ - size_;
		return buffer_ + size_ + MOTION_FRAME_HEADER;
	}

	void AddPayloadSize(u32 size);
	u8 Finish();

private:
	u8 *buffer_;
	u32 size_;
	u32 maxSize_;
};

inline void MotionFrameWriter::AddPayloadSize(u32 size)
{
	size_ += size;
	if (maxSize_ < size_)
		Panic("%5s:%d @%5s: ", MOTIONFRAMEUTIL_FILE, 61, "AddPayloadSize");	/* line 61 of Sony's header */
}

/* reads the packets of a frame the host sent */
class MotionFrameReader : public MotionFrameUtil {
public:
	void SetMaxSize(u32 size)
	{
		maxSize_ = size;
	}

	s32 Open(u8 *frame);
	u8 *Next();

private:
	u32 remaining_;
	u8 *packet_;
	u32 maxSize_;
};

#endif
