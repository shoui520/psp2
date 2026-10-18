/*
 * SceHdmi: video identification codes and the transmitter timings each one
 * needs. Internal names are unknown unless a comment says otherwise.
 */
#include <psp2kern/types.h>
#include <lowio/i2c.h>
#include "hdmi_internal.h"

/* I2C addresses of the transmitter's register maps (bus 1). */
#define ADV_MAIN	0x7c
#define ADV_CEC		0x7a

static inline void writeReg(SceUInt32 addr, SceUInt8 reg, SceUInt8 value)
{
	SceUInt8 buf[2];

	buf[0] = reg;
	buf[1] = value;
	SceI2cForDriver_CA94A759(1, addr, buf, 2);
}

/* Program the transmitter's timing generator for a video identification code. */
int hdmiSetVicTiming(int vic)
{
	switch (vic) {
	case 1:
	case 6:
	case 7:
	case 8:
	case 9:
	case 21:
	case 22:
	case 23:
	case 24:
	case 80:
	case 81:
	case 83:
		break;
	case 2:
	case 3:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0x35);
		writeReg(ADV_MAIN, 0x29, 0xa0);
		writeReg(ADV_MAIN, 0x2a, 0x03);
		writeReg(ADV_MAIN, 0x2b, 0xe0);
		writeReg(ADV_MAIN, 0x2c, 0x01);
		writeReg(ADV_MAIN, 0x2d, 0x00);
		writeReg(ADV_MAIN, 0x2e, 0x03);
		writeReg(ADV_MAIN, 0x2f, 0xc0);
		writeReg(ADV_MAIN, 0x30, 0x20);
		writeReg(ADV_MAIN, 0x31, 0xd0);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x60);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x90);
		writeReg(ADV_MAIN, 0x36, 0x01);
		writeReg(ADV_MAIN, 0x37, 0xe0);
		break;
	case 4:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0x67);
		writeReg(ADV_MAIN, 0x29, 0x20);
		writeReg(ADV_MAIN, 0x2a, 0x02);
		writeReg(ADV_MAIN, 0x2b, 0x80);
		writeReg(ADV_MAIN, 0x2c, 0x06);
		writeReg(ADV_MAIN, 0x2d, 0xe0);
		writeReg(ADV_MAIN, 0x2e, 0x0d);
		writeReg(ADV_MAIN, 0x2f, 0xc0);
		writeReg(ADV_MAIN, 0x30, 0x2e);
		writeReg(ADV_MAIN, 0x31, 0xe0);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x50);
		writeReg(ADV_MAIN, 0x36, 0x01);
		writeReg(ADV_MAIN, 0x37, 0x40);
		break;
	case 5:
		writeReg(ADV_MAIN, 0x25, 0x92);
		writeReg(ADV_MAIN, 0x28, 0x89);
		writeReg(ADV_MAIN, 0x29, 0x80);
		writeReg(ADV_MAIN, 0x2a, 0x02);
		writeReg(ADV_MAIN, 0x2b, 0xc0);
		writeReg(ADV_MAIN, 0x2c, 0x05);
		writeReg(ADV_MAIN, 0x2d, 0x80);
		writeReg(ADV_MAIN, 0x2e, 0x09);
		writeReg(ADV_MAIN, 0x2f, 0x40);
		writeReg(ADV_MAIN, 0x30, 0x46);
		writeReg(ADV_MAIN, 0x31, 0x50);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x20);
		writeReg(ADV_MAIN, 0x36, 0x00);
		writeReg(ADV_MAIN, 0x37, 0xf0);
		break;
	case 17:
	case 18:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0x36);
		writeReg(ADV_MAIN, 0x29, 0x00);
		writeReg(ADV_MAIN, 0x2a, 0x04);
		writeReg(ADV_MAIN, 0x2b, 0x00);
		writeReg(ADV_MAIN, 0x2c, 0x00);
		writeReg(ADV_MAIN, 0x2d, 0xc0);
		writeReg(ADV_MAIN, 0x2e, 0x04);
		writeReg(ADV_MAIN, 0x2f, 0x40);
		writeReg(ADV_MAIN, 0x30, 0x27);
		writeReg(ADV_MAIN, 0x31, 0x10);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x50);
		writeReg(ADV_MAIN, 0x36, 0x02);
		writeReg(ADV_MAIN, 0x37, 0x70);
		break;
	case 19:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0x7b);
		writeReg(ADV_MAIN, 0x29, 0xc0);
		writeReg(ADV_MAIN, 0x2a, 0x02);
		writeReg(ADV_MAIN, 0x2b, 0x80);
		writeReg(ADV_MAIN, 0x2c, 0x1b);
		writeReg(ADV_MAIN, 0x2d, 0x80);
		writeReg(ADV_MAIN, 0x2e, 0x0d);
		writeReg(ADV_MAIN, 0x2f, 0xc0);
		writeReg(ADV_MAIN, 0x30, 0x2e);
		writeReg(ADV_MAIN, 0x31, 0xe0);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x50);
		writeReg(ADV_MAIN, 0x36, 0x01);
		writeReg(ADV_MAIN, 0x37, 0x40);
		break;
	case 20:
		writeReg(ADV_MAIN, 0x25, 0x92);
		writeReg(ADV_MAIN, 0x28, 0xa5);
		writeReg(ADV_MAIN, 0x29, 0x00);
		writeReg(ADV_MAIN, 0x2a, 0x02);
		writeReg(ADV_MAIN, 0x2b, 0xc0);
		writeReg(ADV_MAIN, 0x2c, 0x21);
		writeReg(ADV_MAIN, 0x2d, 0x00);
		writeReg(ADV_MAIN, 0x2e, 0x09);
		writeReg(ADV_MAIN, 0x2f, 0x40);
		writeReg(ADV_MAIN, 0x30, 0x46);
		writeReg(ADV_MAIN, 0x31, 0x50);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x20);
		writeReg(ADV_MAIN, 0x36, 0x00);
		writeReg(ADV_MAIN, 0x37, 0xf0);
		break;
	case 32:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0xab);
		writeReg(ADV_MAIN, 0x29, 0xe0);
		writeReg(ADV_MAIN, 0x2a, 0x02);
		writeReg(ADV_MAIN, 0x2b, 0xc0);
		writeReg(ADV_MAIN, 0x2c, 0x27);
		writeReg(ADV_MAIN, 0x2d, 0xe0);
		writeReg(ADV_MAIN, 0x2e, 0x09);
		writeReg(ADV_MAIN, 0x2f, 0x40);
		writeReg(ADV_MAIN, 0x30, 0x46);
		writeReg(ADV_MAIN, 0x31, 0x50);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x40);
		writeReg(ADV_MAIN, 0x36, 0x02);
		writeReg(ADV_MAIN, 0x37, 0x40);
		break;
	case 33:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0xa5);
		writeReg(ADV_MAIN, 0x29, 0x00);
		writeReg(ADV_MAIN, 0x2a, 0x02);
		writeReg(ADV_MAIN, 0x2b, 0xc0);
		writeReg(ADV_MAIN, 0x2c, 0x21);
		writeReg(ADV_MAIN, 0x2d, 0x00);
		writeReg(ADV_MAIN, 0x2e, 0x09);
		writeReg(ADV_MAIN, 0x2f, 0x40);
		writeReg(ADV_MAIN, 0x30, 0x46);
		writeReg(ADV_MAIN, 0x31, 0x50);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x40);
		writeReg(ADV_MAIN, 0x36, 0x02);
		writeReg(ADV_MAIN, 0x37, 0x40);
		break;
	case 34:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0x89);
		writeReg(ADV_MAIN, 0x29, 0x80);
		writeReg(ADV_MAIN, 0x2a, 0x02);
		writeReg(ADV_MAIN, 0x2b, 0xc0);
		writeReg(ADV_MAIN, 0x2c, 0x05);
		writeReg(ADV_MAIN, 0x2d, 0x80);
		writeReg(ADV_MAIN, 0x2e, 0x09);
		writeReg(ADV_MAIN, 0x2f, 0x40);
		writeReg(ADV_MAIN, 0x30, 0x46);
		writeReg(ADV_MAIN, 0x31, 0x50);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x50);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x40);
		writeReg(ADV_MAIN, 0x36, 0x02);
		writeReg(ADV_MAIN, 0x37, 0x40);
		break;
	case 82:
		writeReg(ADV_MAIN, 0x25, 0x12);
		writeReg(ADV_MAIN, 0x28, 0x54);
		writeReg(ADV_MAIN, 0x29, 0x00);
		writeReg(ADV_MAIN, 0x2a, 0x08);
		writeReg(ADV_MAIN, 0x2b, 0x80);
		writeReg(ADV_MAIN, 0x2c, 0x01);
		writeReg(ADV_MAIN, 0x2d, 0x80);
		writeReg(ADV_MAIN, 0x2e, 0x0a);
		writeReg(ADV_MAIN, 0x2f, 0x00);
		writeReg(ADV_MAIN, 0x30, 0x32);
		writeReg(ADV_MAIN, 0x31, 0x60);
		writeReg(ADV_MAIN, 0x32, 0x00);
		writeReg(ADV_MAIN, 0x33, 0x60);
		writeReg(ADV_MAIN, 0x34, 0x00);
		writeReg(ADV_MAIN, 0x35, 0x30);
		writeReg(ADV_MAIN, 0x36, 0x01);
		writeReg(ADV_MAIN, 0x37, 0xd0);
		break;
	default:
		return 1;
	}
	writeReg(ADV_MAIN, 0x27, 0x90);
	writeReg(ADV_MAIN, 0x27, 0xd0);
	writeReg(ADV_MAIN, 0x26, 0x1c);
	writeReg(ADV_MAIN, 0x26, 0x3c);
	writeReg(ADV_CEC, 0x0a, 0x41);
	writeReg(ADV_CEC, 0x15, 0x20);
	writeReg(ADV_MAIN, 0x07, 0x80);

	return 0;
}

void hdmiResetVicTiming(void)
{
	writeReg(ADV_MAIN, 0x16, 0x20);
	writeReg(ADV_MAIN, 0x1c, 0x30);
	writeReg(ADV_MAIN, 0x20, 0x30);
	videoMode.vic = 0;
}

int hdmiGetVic(void)
{
	return videoMode.vic;
}

/* Standard definition codes: ITU-R BT.601 colorimetry. */
int hdmiIsSdVic(int vic)
{
	return vicIsSd(vic);
}

int hdmiGetVicAspectRatio(int vic)
{
	return vicAspectRatio(vic);
}

/* Pixel clock of a code, as the index of the audio clock regeneration tables. */
int hdmiGetVicClockClass(int vic)
{
	switch (vic) {
	case 1:
		return 1;
	case 2: case 3: case 6: case 7: case 8: case 9: case 17: case 18: case 21:
	case 22: case 23: case 24:
		return 3;
	case 10: case 11: case 12: case 13: case 14: case 15: case 25: case 26:
	case 27: case 28: case 29: case 30:
		return 5;
	case 4: case 5: case 32: case 34:
		return 7;
	case 19: case 20: case 33:
		return 8;
	case 16:
		return 9;
	case 31:
		return 10;
	case 80: case 81: case 82: case 83:
		return 11;
	default:
		return 0;
	}
}
