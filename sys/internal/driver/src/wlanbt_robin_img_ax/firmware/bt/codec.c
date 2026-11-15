/*
 * The PCM port's codec: its registers, written over the serial bus, the two
 * sets of defaults and the format the PCM stream is set up with
 * (include/robin/sbc.h). Marvell published no source; names are inferred from
 * use.
 */
#include <stdint.h>
#include "robin/nvram.h"
#include "robin/rfreg.h"
#include "robin/sbc.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/threadx.h"

/* The register settings: [0] when the codec clocks the port, [1] otherwise. */
const struct SbcCodecSetting sbcCodecSettings[2][SBC_CODEC_SETTINGS] = {	/* 0x544cc */
	{
	{ 0x00, 0, 0x1ff }, { 0x34, 0, 0x07e }, { 0x35, 0, 0x07e }, { 0x36, 0, 0x07e },
	{ 0x37, 0, 0x07e }, { 0x03, 0, 0x005 }, { 0x01, 0, 0x004 }, { 0x01, 0, 0x01d },
	{ 0x02, 0, 0x180 }, { 0x03, 0, 0x06f }, { 0x07, 0, 0x000 }, { 0x2d, 0, 0x125 },
	{ 0x2e, 0, 0x125 }, { 0x06, 0, 0x040 }, { 0x02, 0, 0x1bf }, { 0x2f, 0, 0x101 },
	{ 0x30, 0, 0x101 }, { 0x34, 0, 0x138 }, { 0x35, 0, 0x138 }, { 0x36, 0, 0x134 },
	{ 0x37, 0, 0x134 }, { 0x04, 0, 0x010 }, { 0x33, 0, 0x001 }, { 0x32, 0, 0x001 },
	{ 0x2c, 0, 0x133 }, { 0x39, 0, 0x001 },
	}, {
	{ 0x00, 0, 0x1ff }, { 0x34, 0, 0x07e }, { 0x35, 0, 0x07e }, { 0x36, 0, 0x07e },
	{ 0x37, 0, 0x07e }, { 0x03, 0, 0x005 }, { 0x01, 0, 0x004 }, { 0x01, 0, 0x03d },
	{ 0x02, 0, 0x080 }, { 0x03, 0, 0x06f }, { 0x07, 0, 0x006 }, { 0x24, 0, 0x015 },
	{ 0x25, 0, 0x007 }, { 0x26, 0, 0x15c }, { 0x27, 0, 0x052 }, { 0x2d, 0, 0x125 },
	{ 0x06, 0, 0x000 }, { 0x02, 0, 0x095 }, { 0x2f, 0, 0x105 }, { 0x30, 0, 0x000 },
	{ 0x34, 0, 0x130 }, { 0x04, 0, 0x119 }, { 0x33, 0, 0x001 }, { 0x32, 0, 0x001 },
	{ 0x2c, 0, 0x107 }, { 0x39, 0, 0x001 },
	},
};

/*
 * Writes a codec register: the device address, then the register number
 * with bit 8 of the value, then the value's low byte.
 */
__attribute__((noinline)) void btSbcTrackBuffer(unsigned int reg, unsigned int value)
{
	unsigned int first;

	first = (reg & 0x3f) << 1 | (value >> 8 & 1);
	SERIAL_SELECT = SERIAL_SELECT_START;
	RFBUS_DATA = SBC_CODEC_ADDRESS;
	RFBUS_DATA = first;
	SERIAL_SELECT = SERIAL_SELECT_STOP;
	RFBUS_DATA = value;
}

/*
 * Sets the codec up for a PCM stream (sbcPcmStreamHook): the defaults, and
 * when the codec clocks the port, its format (register 4), its sample rate
 * (register 7) and its clock (register 6) from the format words.
 */
void btSbcPcmStreamHandler(const uint32_t *config)
{
	unsigned int i, value, clock = 0x100, width;

	SBC_CODEC_POWER |= SBC_CODEC_POWER_HOLD;
	rfBusEnter0000();
	if ((config[0] >> 18 & 3) == SBC_PCM_CODEC_CLOCKS) {
		for (i = 0; i < SBC_CODEC_SETTINGS; i++)
			btSbcTrackBuffer(sbcCodecSettings[0][i].reg, sbcCodecSettings[0][i].value);
		value = 0;
		if ((config[0] & 0x1000) != 0)
			value = 0x100;
		if ((config[0] & 0x800) != 0)
			value |= 0x86;
		if ((config[0] & 0x10000000) != 0)
			value |= 1;
		switch (config[0] >> 7 & 0xf) {
		case 0:
			value |= 0x10;
			break;
		case 1:
			value |= 8;
			break;
		}
		btSbcTrackBuffer(4, value);
		switch (config[2]) {
		case 0:
			clock = 0x180;
			value = 5;
			break;
		case 1:
		case 2:
			value = 4;
			break;
		case 3:
			clock = 0xc0;
			value = 3;
			break;
		case 4:
		case 5:
			clock = 0x80;
			value = 2;
			break;
		case 6:
			clock = 0x60;
			value = 1;
			break;
		default:
			clock = 0x40;
			value = 0;
			break;
		}
		btSbcTrackBuffer(7, value);
		width = config[0] >> 13 & 0x1f;
		if (width == 23 || width == 31)
			value = clock | 8;
		else
			value = clock | 0xc;
		if ((config[0] & 0x100000) != 0)
			value |= 1;
		btSbcTrackBuffer(6, value);
	} else {
		for (i = 0; i < SBC_CODEC_SETTINGS; i++)
			btSbcTrackBuffer(sbcCodecSettings[1][i].reg, sbcCodecSettings[1][i].value);
	}
	rfBusSettle();
	rfBusLeave0000();
	SBC_CODEC_POWER &= ~SBC_CODEC_POWER_HOLD;
}

/* The PCM port's DMA: bit 1 of the status sets event 0x100, bit 0 event 0x200. */
int btSbcPcmDmaCallback(uintptr_t device, uintptr_t buffer, uintptr_t argument, unsigned int status)
{
	(void)device;
	(void)buffer;
	(void)argument;
	if ((status & 2) != 0)
		tx_event_flags_set(&sbcPcmEvents, 0x100, TX_OR);
	if ((status & 1) != 0)
		tx_event_flags_set(&sbcPcmEvents, 0x200, TX_OR);
	return 0;
}
