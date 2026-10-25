/*
 * The battery: its gauge's registers over the I2C bus, while one answered at the power-on
 * (deviceVersions.battery.id not 0x00ff), and its bootloader's commands.
 */
#pragma sfr

#include <stddef.h>

#include "ernie.h"
#include "battery.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "iica.h"
#include "kernel.h"
#include "powerseq.h"

extern void sub_2A6BC(unsigned int);
extern unsigned char sub_2A0ED(unsigned char);

static struct IicaRequest batteryIica;
static unsigned char batteryBLBuffer[BATTERY_BL_SIZE - 1];
struct BatteryBLCommand batteryBLCommand;

void batteryInit(void)
{
	batteryBLCommand.command = 0;
	batteryBLCommand.length = 0;
	batteryBLCommand.buffer = batteryBLBuffer;
}

/* data[0] is the register */
unsigned char batteryWrite(unsigned char *data, unsigned char length)
{
	unsigned char status;
	ER ercd;
	FLGPTN ptn;

	if (deviceVersions.battery.id == 0x00ff)
		return 0;
	batteryIica.rxData = NULL;
	batteryIica.rxCount = 0;
	batteryIica.txData = data;
	batteryIica.txCount = length;
	batteryIica.address = sub_2C658()->cookieRegHigh ? 0xa4 : 0xaa;
	batteryIica.status = &status;
	iicaStart(&batteryIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
	return status;
}

/* none answered: data reads 0 0 */
unsigned char batteryRead(unsigned char *reg, unsigned char *data, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == 0x00ff) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
	batteryIica.rxData = data;
	batteryIica.rxCount = length;
	batteryIica.txData = reg;
	batteryIica.txCount = 1;
	batteryIica.address = sub_2C658()->cookieRegHigh ? 0xa4 : 0xaa;
	batteryIica.status = &status;
	iicaStart(&batteryIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
	return status;
}

/* a control command (register 0), then its answer */
unsigned char batteryControl(unsigned int command, unsigned char *data, unsigned char length)
{
	unsigned char result;

	if (deviceVersions.battery.id == 0x00ff) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
	elmoCommand[0] = 0;
	elmoCommand[1] = command & 0xff;
	elmoCommand[2] = command >> 8;
	result = batteryWrite(elmoCommand, 3);
	if (result == 0) {
		elmoCommand[0] = 0;
		result = batteryRead(elmoCommand, data, length);
		if (result != 0)
			;
	}
	return result;
}

/* the battery's identity (struct BatteryInfo's four words): 0xfffe for each where a battery
   without a gauge answered */
unsigned char batteryReadInfo(unsigned int *info)
{
	unsigned char result;
	unsigned char status;

	result = batteryControl(1, (unsigned char *)info, 2);
	if (result == 0) {
		result = batteryControl(3, (unsigned char *)(info + 1), 2);
		if (result == 0) {
			result = batteryControl(2, (unsigned char *)(info + 2), 2);
			if (result == 0) {
				elmoCommand[0] = 0x3e;
				elmoCommand[1] = 0x39;
				result = batteryWrite(elmoCommand, 2);
				if (result == 0) {
					elmoCommand[0] = 0x3f;
					elmoCommand[1] = 0;
					result = batteryWrite(elmoCommand, 2);
					if (result == 0) {
						sub_2A6BC(4);
						elmoCommand[0] = 0x40;
						result = batteryRead(elmoCommand,
								     (unsigned char *)(info + 3), 2);
					}
				}
			}
		}
	}
	if (result == 1) {
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0;
		result = batteryReadAt(elmoCommand, &status, 1);
		if (result == 0) {
			info[0] = 0xfffe;
			info[1] = 0xfffe;
			info[2] = 0xfffe;
			info[3] = 0xfffe;
		}
	}
	return result;
}

/* the gauge's remaining capacity (register 0x10) */
unsigned char batteryReadRemainCap(unsigned int *value)
{
	unsigned char reg;

	reg = 0x10;
	return batteryRead(&reg, (unsigned char *)value, 2);
}

/* its voltage (0x08) */
unsigned char batteryReadVolt(unsigned int *value)
{
	unsigned char reg;

	reg = 0x08;
	return batteryRead(&reg, (unsigned char *)value, 2);
}

/* its temperature (0x06) */
unsigned char batteryReadTemp(unsigned int *value)
{
	unsigned char reg;

	reg = 0x06;
	return batteryRead(&reg, (unsigned char *)value, 2);
}

/* its internal temperature (0x20), from version 0x0404 on; before it 0x0aac (0 degrees) */
unsigned char batteryReadBicTemp(unsigned int *value)
{
	unsigned char reg;

	if (deviceVersions.battery.version[1] >= 0x0404) {
		reg = 0x20;
		return batteryRead(&reg, (unsigned char *)value, 2);
	}
	*value = 0x0aac;
	return 0;
}

/* its state of charge in percent (0x2c) */
unsigned char batteryReadLifePercent(unsigned int *value)
{
	unsigned char reg;

	reg = 0x2c;
	return batteryRead(&reg, (unsigned char *)value, 2);
}

/* a register of the device at packet[0] (packet[1] the register); a timeout resets the bus */
unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == 0x00ff) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
	batteryIica.rxData = data;
	batteryIica.rxCount = length;
	batteryIica.txData = packet + 1;
	batteryIica.txCount = 1;
	batteryIica.address = *packet;
	batteryIica.status = &status;
	iicaStart(&batteryIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		iicaInit();
		status = 0x10;
	}
	return status;
}

/* a write to the device at packet[0]: the rest of the length bytes */
unsigned char batteryWriteAt(unsigned char *packet, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == 0x00ff)
		return 0;
	if (length != 0) {
		batteryIica.txCount = length - 1;
		batteryIica.rxData = NULL;
		batteryIica.rxCount = 0;
		batteryIica.txData = packet + 1;
		batteryIica.address = *packet;
		batteryIica.status = &status;
		iicaStart(&batteryIica);
		ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
		if (ercd == E_OK) {
			if (ptn & 0x10)
				clr_flg(4, ~0x10);
		} else if (ercd == E_TMOUT) {
			iicaInit();
			status = 0x10;
		}
	} else {
		status = 0;
	}
	return status;
}

/* 0x09b0: the gauge's control command 0x0f00 (its bootloader), checked at 0x16 where a battery
   without a gauge answered */
unsigned char batteryStartBLMode(void)
{
	unsigned char result;
	unsigned char data;

	elmoCommand[0] = 0;
	elmoCommand[1] = 0;
	elmoCommand[2] = 0x0f;
	result = batteryWrite(elmoCommand, 3);
	if (result == 1) {
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0;
		result = batteryReadAt(elmoCommand, &data, 1);
		if (result != 0)
			;
	}
	if (result == 0) {
		sub_2A0ED(0);
		chargeTimerStop();
	}
	return result;
}

/* 0x09b1: the bootloader's commands 0x00 0x0f, 0x64 0x0f and 0x65 0x00 */
unsigned char batteryStopBLMode(void)
{
	unsigned char result;

	chargeTimerStart(0x0a);
	elmoCommand[0] = 0x16;
	elmoCommand[1] = 0;
	elmoCommand[2] = 0x0f;
	result = batteryWriteAt(elmoCommand, 3);
	if (result == 0) {
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0x64;
		elmoCommand[2] = 0x0f;
		result = batteryWriteAt(elmoCommand, 3);
		if (result == 0) {
			elmoCommand[0] = 0x16;
			elmoCommand[1] = 0x65;
			elmoCommand[2] = 0;
			result = batteryWriteAt(elmoCommand, 3);
		}
	}
	sub_2A0ED(1);
	return result;
}

/* 0x0989: the gauge's control command 0x0041 (its reset) */
unsigned char batterySWReset(void)
{
	chargeTimerStart(0x0a);
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x41;
	elmoCommand[2] = 0;
	return batteryWrite(elmoCommand, 3);
}

/* 0x08b0: the battery's bootloader line */
void batteryBLLineOn(void)
{
	P96 = 1;
}

void batteryBLLineOff(void)
{
	P96 = 0;
}

/* at the power-off: the gauge's control command 0x0011 (its hibernation), from version 0x0408 */
unsigned char batteryHibernate(void)
{
	if (deviceVersions.battery.version[1] >= 0x0408) {
		elmoCommand[0] = 0;
		elmoCommand[1] = 0x11;
		elmoCommand[2] = 0;
		return batteryWrite(elmoCommand, 3);
	}
	return 0;
}

/* 0x098d: the gauge's control command 0x0015 */
unsigned char batteryControl0015(void)
{
	chargeTimerStart(0x0a);
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x15;
	elmoCommand[2] = 0;
	return batteryWrite(elmoCommand, 3);
}
