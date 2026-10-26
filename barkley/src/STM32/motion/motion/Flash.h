/*
 * Flash: the calibration blocks in the MCU's flash, and writing them.
 */
#ifndef FLASH_H
#define FLASH_H

#include "stm32f10x_type.h"

/* a calibration block: a header, then count 20-byte entries */
struct CalibBlock {
	u8 version[2];
	u16 reserved;
	u32 serial;
	u16 reserved2;
	u16 count;

	bool IsValid() const;

	u32 Size() const
	{
		return 20 * count + 12;
	}
};

/* the accelerometer's and the gyroscope's calibration (a calibration block's data) */
struct CalibParams {
	u32 reserved[4];
	s32 accelShift;
	s32 accelOffset[3];
	s32 accelDeadLow[3];
	s32 accelDeadHigh[3];
	s32 accelMatrix[2][3][3];	/* for a positive and a negative input */
	s32 reserved2[9];
	s32 gyroShift;
	s32 gyroOffset[3];
	s32 reserved3[18];
	s32 gyroMatrix[3][3];
};

class Flash {
public:
	static s32 Program(u32 dst, const u8 *src, u32 size);
	static s32 Write(u32 dst, const u8 *src, u32 size);
};

#endif
