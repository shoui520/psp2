/*
 * SceHdmi device layer: the AD80244 HDMI transmitter (Sony's
 * device/adi/hdmi_ad80244.c). Its register maps are reached over I2C:
 * the main map at 0x7a, the CEC map at 0x7c and the packet memory at 0x70.
 * Internal names are ours unless a comment says otherwise.
 */
#include <scetypes.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "../../hdmi_internal.h"
#include <sysmem/sysclib.h>

/* I2C bus and slave addresses */
#define AD80244_BUS		1
#define AD80244_PACKET		0x70	/* InfoFrame / packet memory */
#define AD80244_MAIN		0x7a
#define AD80244_CEC		0x7c

/* main map registers */
#define AD80244_REG_CHIP_REVISION	0x00
#define AD80244_REG_N			0x01	/* audio clock regeneration N, 3 bytes */
#define AD80244_REG_AUDIO_CONFIG	0x0a	/* 0x0a..0x0d */
#define AD80244_REG_AUDIO_MUTE		0x0d
#define AD80244_REG_CSC_MODE		0x16
#define AD80244_REG_VIDEO_CONFIG	0x17
#define AD80244_REG_CSC			0x18	/* 24 coefficient bytes */
#define AD80244_REG_PIXEL_REPETITION	0x3b
#define AD80244_REG_VIC			0x3c
#define AD80244_REG_PACKET_ENABLE1	0x40
#define AD80244_REG_PLUG_STATUS		0x42
#define AD80244_REG_PACKET_ENABLE2	0x44
#define AD80244_REG_INFOFRAME_CTRL1	0x49
#define AD80244_REG_INFOFRAME_CTRL2	0x4a
#define AD80244_REG_AV_MUTE		0x4b
#define AD80244_REG_COLOR_DEPTH		0x4c
#define AD80244_REG_INTR		0x92	/* HdmiIntrRegs, 6 bytes */
#define AD80244_REG_INTR_STATUS1	0x96
#define AD80244_REG_INTR_STATUS2	0x97
#define AD80244_REG_PLL_STATUS		0x9e
#define AD80244_REG_TMDS_POWER		0xa1
#define AD80244_REG_HDCP_CTRL		0xaf
#define AD80244_REG_AKSV		0xb0
#define AD80244_REG_HDCP_KEYS		0xba
#define AD80244_REG_BCAPS		0xbe
#define AD80244_REG_BKSV		0xbf
#define AD80244_REG_EDID_SEGMENT	0xc4
#define AD80244_REG_BKSV_COUNT		0xc7
#define AD80244_REG_EDID_CTRL		0xc9
#define AD80244_REG_BSTATUS		0xca
#define AD80244_REG_VIDEO_MUTE		0xd5
#define AD80244_REG_CEC_ADDRESS		0xe1

/* AD80244_REG_INTR_STATUS2 */
#define AD80244_INTR_BKSV_FLAG		0x40

/* SetFrame/SetPacketCtrl: the device needs an HDMI sink */
#define AD80244_ERROR_NOT_HDMI_PACKET	0x80000022
#define AD80244_ERROR_NOT_HDMI_FRAME	0x80000023

/* sceHdmiDeviceAD80244SetCSC: the colour space converter's in- and output */
typedef struct HdmiCsc {
	int input;			/* HdmiDeviceVideoConfig.inputCsc */
	int output;			/* HdmiDeviceVideoConfig.outputCsc */
} HdmiCsc;

/* {register, value}: what HdmiDeviceAD80244SetFixedRegisters writes */
typedef struct Ad80244Register {
	SceUInt8 reg;
	SceUInt8 value;
} Ad80244Register;

static const Ad80244Register fixedRegisters[] = {
	{ 0x92, 0x00 }, { 0x93, 0x00 }, { 0x94, 0x00 },
	{ 0x95, 0x00 }, { 0x96, 0x00 }, { 0x97, 0x00 },
};

/* The colour space converter's coefficients, by input and output CSC. */
static const SceUInt8 cscRgbFullToYcc709[24] = {
	0x87, 0x06, 0x19, 0x9e, 0x1f, 0x5d, 0x08, 0x00, 0x02, 0xed, 0x09, 0xd2,
	0x00, 0xfd, 0x01, 0x00, 0x1e, 0x63, 0x1a, 0x98, 0x07, 0x06, 0x08, 0x00,
};
static const SceUInt8 cscYcc601ToYcc709[24] = {
	0xaa, 0xf7, 0x08, 0x00, 0x00, 0x00, 0x1a, 0x84, 0x1a, 0x6a, 0x08, 0x00,
	0x1d, 0x50, 0x04, 0x22, 0x00, 0x00, 0x08, 0x00, 0x0d, 0xdb, 0x19, 0x12,
};
static const SceUInt8 cscRgbFullToYcc601[24] = {
	0x87, 0x06, 0x1a, 0x1e, 0x1e, 0xde, 0x08, 0x00, 0x04, 0x1c, 0x08, 0x10,
	0x01, 0x91, 0x01, 0x00, 0x1d, 0xa2, 0x1b, 0x59, 0x07, 0x06, 0x08, 0x00,
};
static const SceUInt8 cscRgbLimitedToYcc601[24] = {
	0x88, 0x2d, 0x19, 0x27, 0x1e, 0xad, 0x08, 0x00, 0x03, 0xa9, 0x09, 0x64,
	0x01, 0xd2, 0x00, 0x00, 0x1d, 0x40, 0x1a, 0x94, 0x08, 0x2d, 0x08, 0x00,
};
static const SceUInt8 cscRgbLimitedToYcc709[24] = {
	0x88, 0x2d, 0x18, 0x94, 0x1f, 0x40, 0x08, 0x00, 0x03, 0x68, 0x0b, 0x70,
	0x01, 0x26, 0x00, 0x00, 0x1e, 0x21, 0x19, 0xb3, 0x08, 0x2d, 0x08, 0x00,
};
static const SceUInt8 cscYcc601ToRgbFull[24] = {
	0xe6, 0x62, 0x04, 0xa8, 0x00, 0x00, 0x1c, 0x84, 0x1c, 0xbf, 0x04, 0xa8,
	0x1e, 0x70, 0x02, 0x1e, 0x00, 0x00, 0x04, 0xa8, 0x08, 0x12, 0x1b, 0xac,
};
static const SceUInt8 cscYcc709ToYcc709[24] = {
	0xac, 0x51, 0x08, 0x00, 0x00, 0x00, 0x19, 0xd7, 0x1c, 0x54, 0x08, 0x00,
	0x1e, 0x89, 0x02, 0x91, 0x00, 0x00, 0x08, 0x00, 0x0e, 0x87, 0x18, 0xbc,
};
static const SceUInt8 cscYcc709ToRgbFull[24] = {
	0xe7, 0x2c, 0x04, 0xa7, 0x00, 0x00, 0x1c, 0x1f, 0x1d, 0xdd, 0x04, 0xa7,
	0x1f, 0x26, 0x01, 0x34, 0x00, 0x00, 0x04, 0xa7, 0x08, 0x75, 0x1b, 0x7a,
};
static const SceUInt8 cscIdentity[24] = {
	0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};

/* The chip ids HdmiDeviceAD80244GetChipId knows: CEC map 0x00..0x02, revision. */
static const char chipIds[2][8] = { "u3\x03\x14", "u3\x01\x14" };

/* Write one register. */
static inline int writeRegister(SceUInt8 slave, SceUInt8 reg, SceUInt8 value)
{
	SceUInt8 data = value;

	return sceHdmiDeviceI2cWrite(AD80244_BUS, slave, reg, &data, 1);
}

/* Change the bits of mask in one register to those of value. */
static inline int updateRegister(SceUInt8 slave, SceUInt8 reg, SceUInt8 mask, SceUInt8 value)
{
	SceUInt8 data;
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, slave, reg, &data, 1);
	if (ret < 0)
		return ret;
	data = (data & ~mask) | (mask & value);
	return sceHdmiDeviceI2cWrite(AD80244_BUS, slave, reg, &data, 1);
}

int HdmiDeviceAD80244SetCecAddress(HdmiDevice *device)
{
	return writeRegister(AD80244_MAIN, AD80244_REG_CEC_ADDRESS, AD80244_CEC);
}

int HdmiDeviceAD80244SetFixedRegisters(HdmiDevice *device)
{
	int i;

	for (i = 0; i < 6; i++)
		sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, fixedRegisters[i].reg,
				      &fixedRegisters[i].value, 1);
	return 0;
}

int HdmiDeviceAD80244Reset(HdmiDevice *device)
{
	SceUInt8 data[256];
	int ret;

	memset(data, 0, sizeof(data));
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_PACKET, 0, data, 0xfe);
	if (ret < 0)
		return ret;
	return updateRegister(AD80244_MAIN, AD80244_REG_TMDS_POWER, 0x3c, 0x3c);
}

int HdmiDeviceAD80244GetChipId(HdmiDevice *device, SceUInt8 *id, SceUInt8 *length)
{
	char chipIdRev3[5];
	char chipIdRev1[5];
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_CEC, 0, id, 3);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_CHIP_REVISION, id + 3, 1);
	if (ret < 0)
		return ret;
	*length = 4;
	__builtin_memcpy(chipIdRev3, chipIds[0], sizeof(chipIdRev3));
	__builtin_memcpy(chipIdRev1, chipIds[1], sizeof(chipIdRev1));
	if (memcmp(id, chipIdRev1, 4) != 0 && memcmp(id, chipIdRev3, 4) != 0)
		return -1;
	*device->chipVersion = 5;
	return 0;
}

int HdmiDeviceAD80244PowerOn(HdmiDevice *device)
{
	int ret;
	SceUInt8 data[1];

	data[0] = 0x10;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, 0x41, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x89;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x03, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0xd0;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x15, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0xd0;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x17, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x11;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x57, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x20;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x24, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0xe0;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, 0x9a, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x70;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_HDCP_KEYS, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x82;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, 0xde, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x80;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, 0xe5, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x3c;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x26, data, 1);
	if (ret < 0)
		return ret;
	updateRegister(AD80244_CEC, 0x05, 0x30, 0x00);
	updateRegister(AD80244_MAIN, 0xe4, 0xc0, 0xc0);
	updateRegister(AD80244_MAIN, 0xd6, 0x40, 0x40);
	data[0] = 0xc0;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x10, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x08;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x11, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 0x30;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_CEC, 0x20, data, 1);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, 0xe7, data, 1);
	if (ret < 0)
		return ret;
	data[0] |= 0x20;
	return sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, 0xe7, data, 1);
}

int HdmiDeviceAD80244GetIntrStatus(HdmiIntrRegs *regs)
{
	return sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_INTR, regs, 6);
}

int HdmiDeviceAD80244ClearIntrStatus(const HdmiIntrRegs *regs)
{
	HdmiIntrRegs status = *regs;

	status.reg[HDMI_INTR_CEC] = 0;
	status.reg[HDMI_INTR_STATUS2] &= ~0x3f;
	return sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_INTR_STATUS1,
				     &status.reg[HDMI_INTR_STATUS1], 2);
}

int HdmiDeviceAD80244ClearBksvFlag(int unused)
{
	return writeRegister(AD80244_MAIN, AD80244_REG_INTR_STATUS2, AD80244_INTR_BKSV_FLAG);
}

/* Sony's name. */
int sceHdmiDeviceAD80244GetIntrMask(HdmiIntrRegs *regs)
{
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_INTR, regs, 6);
	regs->reg[HDMI_INTR_CEC] = 0;
	regs->reg[HDMI_INTR_STATUS1] = 0;
	regs->reg[HDMI_INTR_STATUS2] = 0;
	return ret;
}

/* Sony's name. */
int sceHdmiDeviceAD80244SetIntrMask(HdmiIntrRegs *regs)
{
	regs->reg[HDMI_INTR_CEC] = 0;
	regs->reg[HDMI_INTR_STATUS1] = 0;
	regs->reg[HDMI_INTR_STATUS2] = 0;
	regs->reg[HDMI_INTR_ENABLE2] |= 0x80;
	regs->reg[HDMI_INTR_ENABLE1] |= 0xc6;
	return sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_INTR, regs, 6);
}

int HdmiDeviceAD80244ReadPlgStat(SceUInt8 *status)
{
	return sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_PLUG_STATUS, status, 1);
}

int HdmiDeviceAD80244EdidReread(HdmiDevice *device)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_EDID_CTRL, 0x1f, 0x11);
}

int HdmiDeviceAD80244SetEdidSegment(HdmiDevice *device, SceUInt32 segment)
{
	SceUInt8 current = 0;
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_EDID_SEGMENT, &current, 1);
	if (ret < 0)
		return ret;
	if (current == segment)
		return HdmiDeviceHwRestartEdid(device);
	return updateRegister(AD80244_MAIN, AD80244_REG_EDID_SEGMENT, 0xff, segment);
}

int HdmiDeviceAD80244SetFrameDirect(HdmiDevice *device, SceUInt32 frame, const SceUInt8 *data,
				    SceUInt32 size)
{
	SceUInt8 slave, offset;
	SceUInt32 max;
	int ret;

	switch (frame) {
	case 0x2:
		slave = AD80244_PACKET;
		offset = 0x01;
		max = 3;
		break;
	case 0x4:
		slave = AD80244_PACKET;
		offset = 0x00;
		max = 0;
		break;
	case 0x8:
		slave = AD80244_MAIN;
		offset = 0x4b;
		max = 1;
		break;
	case 0x10:
		slave = AD80244_PACKET;
		offset = 0x40;
		max = 0x1f;
		break;
	case 0x20:
		slave = AD80244_PACKET;
		offset = 0x60;
		max = 0x1f;
		break;
	case 0x40:
		slave = AD80244_PACKET;
		offset = 0x80;
		max = 0x1f;
		break;
	case 0x400:
		slave = AD80244_PACKET;
		offset = 0xa0;
		max = 0x1f;
		break;
	case 0x800:
		slave = AD80244_PACKET;
		offset = 0xc0;
		max = 0x1f;
		break;
	case 0x1000:
		slave = AD80244_MAIN;
		offset = 0x52;
		max = 0x1e;
		break;
	case 0x2000:
		slave = AD80244_PACKET;
		offset = 0x00;
		max = 0x1f;
		break;
	case 0x4000:
		slave = AD80244_MAIN;
		offset = 0x70;
		max = 0x0d;
		break;
	case 0x8000:
		slave = AD80244_PACKET;
		offset = 0x20;
		max = 0x1f;
		break;
	default:
		HDMI_ERR_PRINTF("[hdmiERR ] Unknown fid [0x%x]\n", frame);
		return -1;
	}
	if (size > max) {
		HDMI_ERR_PRINTF("[hdmiERR ] specified Packet Size is too large! %d more is cut down.\n",
				max);
		size = max;
	}
	/* the AVI and audio InfoFrames go to the main map, without their first byte */
	if (frame == 0x1000 || frame == 0x4000)
		ret = sceHdmiDeviceI2cWrite(AD80244_BUS, slave, offset, data + 1, size - 1);
	else
		ret = sceHdmiDeviceI2cWrite(AD80244_BUS, slave, offset, data, size);
	HDMI_PRINTF(HDMI_DBG_HW, "[hdmiHW  ] ", "HdmiDeviceHwSetFrameDirect:0x%08X\n", ret);
	HDMI_PRINTF(HDMI_DBG_HW, "[hdmiHW  ] ",
		    "DataAdr:0x%02X,Offset:0x%02X,data[0]=0x%02X,wsize=0x%02X\n",
		    slave, offset, data[0], size);
	return ret;
}

int HdmiDeviceAD80244SetFrame(HdmiDevice *device, SceUInt32 frames)
{
	SceUInt8 enable1, enable2, keep;
	SceUInt32 i, frame;
	int ret;

	if (device->sinkType != 2)
		return AD80244_ERROR_NOT_HDMI_FRAME;
	if (frames == HDMI_FRAME_ALL) {
		enable2 = 0;
		ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE1,
					    &enable2, 1);
		if (ret < 0)
			return ret;
		enable2 = 1;
		return sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE2,
					     &enable2, 1);
	}
	enable1 = 0;
	enable2 = 0;
	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE1, &enable1, 1);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE2, &enable2, 1);
	if (ret < 0)
		return ret;
	for (i = 0; i < 16; i++) {
		frame = 1 << i;
		if (!(frame & frames))
			continue;
		switch (frame) {
		case 0x2:	keep = 0xbf; goto enable2;
		case 0x4:	keep = 0xdf; goto enable2;
		case 0x8:	keep = 0x7f; break;
		case 0x10:	keep = 0xef; break;
		case 0x20:
		case 0x40:	keep = 0xf7; break;
		case 0x400:	keep = 0xfb; break;
		case 0x800:	keep = 0xfe; break;
		case 0x1000:	keep = 0xef; goto enable2;
		case 0x2000:	keep = 0xbf; break;
		case 0x4000:	keep = 0xf7; goto enable2;
		case 0x8000:	keep = 0xdf; break;
		default:
			HDMI_ERR_PRINTF("[hdmiERR ] Unknown fid [0x%x]\n", frame);
			continue;
		}
		enable1 &= keep;
		continue;
enable2:
		enable2 &= keep;
	}
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE1, &enable1, 1);
	if (ret < 0)
		return ret;
	return sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE2, &enable2, 1);
}

/* frames: unused, the enables come from device->packetCtrl */
int HdmiDeviceAD80244SetPacketCtrl(HdmiDevice *device, SceUInt32 frames)
{
	SceUInt8 enable1, enable2, bit;
	SceUInt32 i, frame;
	int deferred;
	int ret;

	if (device->sinkType != 2)
		return AD80244_ERROR_NOT_HDMI_PACKET;
	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE1, &enable1, 1);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE2, &enable2, 1);
	if (ret < 0)
		return ret;
	/* a packet 0x4 that is off now is turned on after the others */
	if (!(enable2 & 0x20))
		deferred = (device->packetCtrl >> 2) & 1;
	else
		deferred = 0;
	for (i = 0; i < 16; i++) {
		if (!(((SceUInt32)device->packetCtrl >> i) & 1))
			continue;
		if (((0x4U >> i) & 1) && deferred)
			continue;
		frame = 1 << i;
		switch (frame) {
		case 0x2:	bit = 0x40; goto enable2;
		case 0x4:	bit = 0x20; goto enable2;
		case 0x8:	bit = 0x80; break;
		case 0x10:	bit = 0x10; break;
		case 0x20:
		case 0x40:	bit = 0x08; break;
		case 0x400:	bit = 0x04; break;
		case 0x800:	bit = 0x01; break;
		case 0x1000:	bit = 0x10; goto enable2;
		case 0x2000:	bit = 0x40; break;
		case 0x4000:	bit = 0x08; goto enable2;
		case 0x8000:	bit = 0x20; break;
		default:
			HDMI_ERR_PRINTF("[hdmiERR ] Unknown fid [0x%x]\n", frame);
			continue;
		}
		enable1 |= bit;
		continue;
enable2:
		enable2 |= bit;
	}
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE1, &enable1, 1);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE2, &enable2, 1);
	if (ret < 0)
		return ret;
	if (deferred) {
		enable2 |= 0x20;
		ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_PACKET_ENABLE2,
					    &enable2, 1);
	}
	return ret;
}

/* Sony's name. */
int sceHdmiDeviceAD80244GetPlgStat(HdmiDevice *device)
{
	SceUInt8 status;
	int plugged;
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_PLUG_STATUS, &status, 1);
	if (ret != 0) {
		HDMI_ERR_PRINTF("[hdmiERR ] %s:ERROR:0x%08x.\n", __func__, ret);
		plugged = 0;
	} else {
		plugged = status & 0x40;		/* hot plug */
		if (plugged)
			plugged = (status & 0x20) ? 2 : 1;	/* rx sense */
	}
	return plugged;
}

/* Sony's name. */
int sceHdmiDeviceAD80244GetPllStat(HdmiDevice *device)
{
	SceUInt8 status;

	if (sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_PLL_STATUS, &status, 1) < 0)
		return 0;
	return (status >> 4) & 1;
}

/* Sony's name. */
int HdmiDeviceAD80244SetTMDSPower(HdmiDevice *device, int on)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_TMDS_POWER, 0x3c, on == 1 ? 0x00 : 0x3c);
}

/* Sony's name. */
int sceHdmiDeviceAD80244SetCSC(HdmiDevice *device, const HdmiCsc *csc)
{
	const SceUInt8 *coefficients;
	int input = csc->input;
	int output = csc->output;
	SceUInt8 ycc;
	int ret;

	switch (output) {
	case 0:
	case 1:
		ycc = 0;
		break;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
		ycc = 1;
		break;
	default:
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	ret = updateRegister(AD80244_MAIN, AD80244_REG_CSC_MODE, 0x81, ycc);
	if (ret < 0)
		return ret;
	if (input == output) {
		coefficients = cscIdentity;
	} else {
		switch (input) {
		case 0:
			if (output == 4) {
				coefficients = cscRgbFullToYcc601;
			} else if (output == 8) {
				coefficients = cscRgbFullToYcc709;
			} else {
				HDMI_ERR_PRINTF("[hdmiERR ] [Hdmi]Unsupported CSC. in:%X out:%X\n",
						input, output);
				goto unsupported;
			}
			break;
		case 1:
			if (output == 4) {
				coefficients = cscRgbLimitedToYcc601;
			} else if (output == 8) {
				coefficients = cscRgbLimitedToYcc709;
			} else {
				HDMI_ERR_PRINTF("[hdmiERR ] [Hdmi]Unsupported CSC. in:%X out:%X\n",
						input, output);
				goto unsupported;
			}
			break;
		case 4:
			if (output == 0) {
				coefficients = cscYcc601ToRgbFull;
			} else if (output == 8) {
				coefficients = cscYcc601ToYcc709;
			} else {
				HDMI_ERR_PRINTF("[hdmiERR ] [Hdmi]Unsupported CSC. in:%X out:%X\n",
						input, output);
				goto unsupported;
			}
			break;
		case 8:
			if (output == 0) {
				coefficients = cscYcc709ToRgbFull;
			} else if (output == 8) {
				coefficients = cscYcc709ToYcc709;
			} else {
				HDMI_ERR_PRINTF("[hdmiERR ] [Hdmi]Unsupported CSC. in:%X out:%X\n",
						input, output);
				goto unsupported;
			}
			break;
		case 2:
		case 3:
		case 6:
		case 7:
			HDMI_ERR_PRINTF("[hdmiERR ] [Hdmi]Unsupported CSC. in:%X out:%X\n",
					input, output);
			/* fall through */
		default:
unsupported:
			SCE_DBG_ASSERT_FAIL(1382);
			return SCE_HDMI_ERROR_INVALID_ARGUMENT;
		}
	}
	return sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_CSC, coefficients, 24);
}

int HdmiDeviceAD80244SetVideoConfig(HdmiDevice *device, const HdmiDeviceVideoConfig *config,
				    const HdmiVideoTiming *timing)
{
	SceUInt8 pixelRepetition = timing->pixelRepetition;
	HdmiCsc csc;
	int colorDepth;
	SceUInt8 value;
	int ret;

	ret = updateRegister(AD80244_MAIN, AD80244_REG_PIXEL_REPETITION, 0x7e,
			     (pixelRepetition << 3) | (pixelRepetition << 1) | 0x40);
	if (ret < 0)
		return ret;
	ret = updateRegister(AD80244_MAIN, AD80244_REG_VIC, 0x3f, config->vic);
	if (ret < 0)
		return ret;
	ret = updateRegister(AD80244_MAIN, AD80244_REG_VIDEO_CONFIG, 0x02, config->aspectRatio << 1);
	if (ret < 0)
		return ret;
	ret = updateRegister(AD80244_MAIN, AD80244_REG_VIDEO_CONFIG, 0x60,
			     timing->syncPolarity << 5);
	if (ret < 0)
		return ret;
	/* 1080p 24, 25 and 30 Hz */
	if (config->vic == 32)
		value = 0x4;
	else if (config->vic == 33)
		value = 0x8;
	else if (config->vic == 34)
		value = 0xc;
	else
		value = 0;
	ret = updateRegister(AD80244_MAIN, AD80244_REG_INFOFRAME_CTRL2, 0x0c, value);
	if (ret < 0)
		return ret;
	csc.input = config->inputCsc;
	csc.output = config->outputCsc;
	ret = sceHdmiDeviceAD80244SetCSC(device, &csc);
	if (ret < 0)
		return ret;
	colorDepth = config->colorDepth;
	if (colorDepth == 0)
		value = 0;
	else if (colorDepth == 1)
		value = 5;
	else if (colorDepth == 2)
		value = 6;
	else
		value = 0;
	ret = updateRegister(AD80244_MAIN, AD80244_REG_COLOR_DEPTH, 0x0f, value);
	if (ret < 0)
		return ret;
	if (colorDepth == 0 || colorDepth == 1)
		value = colorDepth;
	else if (colorDepth == 2)
		value = 2;
	else
		value = 0;
	ret = updateRegister(AD80244_MAIN, AD80244_REG_INFOFRAME_CTRL1, 0x03, value);
	if (ret < 0)
		return ret;
	if (config->scanInfo == 1)
		value = 0x00;
	else if (config->scanInfo == 2)
		value = 0x54;
	else if (config->scanInfo == 3)
		value = 0xa8;
	else
		value = 0xfc;
	return updateRegister(AD80244_MAIN, AD80244_REG_INFOFRAME_CTRL1, 0xfc, value);
}

int HdmiDeviceAD80244SetAudioConfig(HdmiDevice *device, const HdmiDeviceAudioConfig *config)
{
	SceUInt32 source = config->source;
	SceUInt8 values[4];
	SceUInt8 masks[4];
	SceUInt8 n[3];
	int i;
	int ret;

	values[1] = 0;
	if (source < 2) {
		values[0] = source == 0 ? 0x10 : 0x18;
		values[1] = 0xc0;
		values[3] = 0x18;
		values[2] = 0x80;
	} else {
		values[0] = (source & 1) ? 0x04 : 0x00;
		values[3] = config->spdifLayout & 0x1f;
		values[2] = 0xc4 | (config->spdifFlags & 3);
	}
	n[0] = config->n[2];
	n[1] = config->n[1];
	n[2] = config->n[0];
	values[0] |= config->spdifMode & 3;
	masks[0] = 0x1f;
	masks[1] = 0xc0;
	masks[2] = 0xc7;
	masks[3] = 0x1f;
	ret = sceHdmiDeviceI2cWrite(AD80244_BUS, AD80244_MAIN, AD80244_REG_N, n, 3);
	if (ret < 0)
		return ret;
	for (i = 0; i < 4; i++) {
		ret = updateRegister(AD80244_MAIN, AD80244_REG_AUDIO_CONFIG + i, masks[i], values[i]);
		if (ret != 0)
			break;
	}
	return ret;
}

int HdmiDeviceAD80244SetAvMute(HdmiDevice *device, int mute)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_AV_MUTE, 0xc0, mute != 0 ? 0x40 : 0x80);
}

int HdmiDeviceAD80244SetAudioMute(HdmiDevice *device, int mute)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_AUDIO_MUTE, 0x40, mute == 0 ? 0x00 : 0x40);
}

int HdmiDeviceAD80244SetVideoMute(HdmiDevice *device, int mute)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_VIDEO_MUTE, 0x01, mute == 1);
}

/* Sony's name. */
int HdmiDeviceAD80244SetHDMIMode(HdmiDevice *device, int hdmi)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_HDCP_CTRL, 0x02, hdmi == 1 ? 0x02 : 0x00);
}

/* Sony's name. */
int HdmiDeviceAD80244AuthCtrl(HdmiDevice *device, int on)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_HDCP_CTRL, 0x80, on == 1 ? 0x80 : 0x00);
}

/* Sony's name. */
int HdmiDeviceAD80244SetCypher(HdmiDevice *device, int on)
{
	return updateRegister(AD80244_MAIN, AD80244_REG_HDCP_CTRL, 0x10, on == 1 ? 0x10 : 0x00);
}

int HdmiDeviceAD80244CheckRepeater(HdmiDevice *device)
{
	SceUInt8 bcaps;
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_BCAPS, &bcaps, 1);
	if (ret < 0)
		return ret;
	device->repeater = (bcaps >> 6) & 1;
	return ret;
}

int HdmiDeviceAD80244ReadAksv(HdmiDevice *device, SceUInt8 *aksv)
{
	int ret;

	ret = updateRegister(AD80244_MAIN, AD80244_REG_HDCP_KEYS, 0x04, 0x04);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_AKSV, aksv, 8);
	if (ret < 0)
		return ret;
	device->aksvValid = 1;
	return updateRegister(AD80244_MAIN, AD80244_REG_HDCP_KEYS, 0x04, 0x00);
}

int HdmiDeviceAD80244ReadBksv(HdmiDevice *device, SceUInt8 *bksv)
{
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_BKSV, bksv, 5);
	if (ret < 0)
		return ret;
	return writeRegister(AD80244_MAIN, AD80244_REG_INTR_STATUS2, AD80244_INTR_BKSV_FLAG);
}

int HdmiDeviceAD80244ReadBcaps(HdmiDevice *device, SceUInt8 *bcaps)
{
	return sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_BCAPS, bcaps, 1);
}

int HdmiDeviceAD80244ReadBstatus(HdmiDevice *device, SceUInt8 *bstatus)
{
	SceUInt8 low;
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_BSTATUS, bstatus, 2);
	if (ret >= 0) {
		low = bstatus[1];
		bstatus[1] = bstatus[0];
		bstatus[0] = low;
	}
	return ret;
}

int HdmiDeviceAD80244ReadBksvCount(HdmiDevice *device, SceUInt8 *count)
{
	int ret;

	ret = sceHdmiDeviceI2cRead(AD80244_BUS, AD80244_MAIN, AD80244_REG_BKSV_COUNT, count, 1);
	*count &= 0x7f;
	return ret;
}
