/*
 * SceHdmi device layer: the transmitter's registers over I2C (Sony's
 * device/hdmi_device_i2c.c). Transfers longer than the controller takes are
 * split into 8-byte pieces.
 */
#include <psp2kern/types.h>
#include <psp2kern/lowio/i2c.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "../hdmi_internal.h"
#include <sysmem/sysclib.h>

#define HDMI_I2C_CHUNK		8	/* bytes the controller moves at once */
#define HDMI_I2C_WRITE_MAX	256

/* Sony's name. */
int sceHdmiDeviceI2cRead(int bus, SceUInt8 slave, SceUInt8 reg, void *data, SceSize size)
{
	SceUInt8 address;
	SceUInt8 left, offset, chunk;
	int ret;

	if (size <= HDMI_I2C_CHUNK) {
		ret = ksceI2cTransferWriteRead(bus, slave, &reg, 1, slave, data, size);
		if (ret >= 0)
			return ret;
	} else {
		left = size;
		if (left != 0) {
			offset = 0;
			chunk = HDMI_I2C_CHUNK;
			do {
				address = reg + offset;
				ret = ksceI2cTransferWriteRead(bus, slave, &address, 1, slave,
							       (SceUInt8 *)data + offset, chunk);
				if (ret != 0) {
					HDMI_ERR_PRINTF("[hdmiERR ] sceHdmiDeviceI2cRead() Error!!:0x%08X\n",
							ret);
					return ret;
				}
				left -= chunk;
				offset += chunk;
				if (left <= HDMI_I2C_CHUNK)
					chunk = left;
			} while (left != 0);
			return ret;
		}
		ret = -1;
	}
	SCE_DBG_PRINTF(0, 88, "[HDMI] sceI2cMasterTransmitReceive ERR : %08x\n", ret);
	return ret;
}

/* Sony's name. */
int sceHdmiDeviceI2cWrite(int bus, SceUInt8 slave, SceUInt8 reg, const void *data, SceSize size)
{
	SceUInt8 buffer[36];		/* the register, then the data */
	SceUInt8 left, offset, chunk;
	int ret;

	if (size > HDMI_I2C_WRITE_MAX)
		return -1;
	if (size < HDMI_I2C_CHUNK) {
		buffer[0] = reg;
		if (size != 0)
			memcpy(&buffer[1], data, size);
		ret = ksceI2cTransferWrite(bus, slave, buffer, size + 1);
		if (ret >= 0)
			return ret;
	} else {
		left = size;
		if (left != 0) {
			offset = 0;
			chunk = HDMI_I2C_CHUNK;
			do {
				buffer[0] = offset + reg;
				if (chunk > 1)
					memcpy(&buffer[1], (const SceUInt8 *)data + offset, chunk - 1);
				ret = ksceI2cTransferWrite(bus, slave, buffer, chunk);
				if (ret != 0) {
					HDMI_ERR_PRINTF("[hdmiERR ] sceHdmiDeviceI2cWrite() Error!!:0x%08X\n",
							ret);
					return ret;
				}
				left -= chunk - 1;
				offset += chunk - 1;
				if (left < HDMI_I2C_CHUNK)
					chunk = left + 1;
			} while (left != 0);
			return 0;
		}
		ret = -1;
	}
	SCE_DBG_PRINTF(0, 225, "[HDMI] sceI2cMasterTransmit ERR : %08x\n", ret);
	return ret;
}
