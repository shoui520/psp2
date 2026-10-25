/*
 * The battery, as the updater has it: its gauge's registers over the I2C bus at 0xaa, each
 * transfer waited for by polling (iicaTransfer), while one answered at the power-on
 * (deviceVersions.battery.id not 0x00ff).
 */
#pragma NOP

#include <stddef.h>

#include "ernie.h"
#include "battery.h"
#include "iica.h"
#include "powerseq.h"
#include "updater.h"

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
	unsigned char wait;

	if (deviceVersions.battery.id == 0x00ff)
		return 0;
	batteryIica.rxData = NULL;
	batteryIica.rxCount = 0;
	batteryIica.txData = data;
	batteryIica.txCount = length;
	batteryIica.address = 0xaa;
	batteryIica.status = &status;
	iicaTransfer(&batteryIica);
	for (wait = 0xff; wait != 0; wait--)
		NOP();
	return status;
}

/* none answered: data reads 0 0 */
unsigned char batteryRead(unsigned char *reg, unsigned char *data, unsigned char length)
{
	unsigned char status;
	unsigned char wait;

	if (deviceVersions.battery.id == 0x00ff) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
	batteryIica.rxData = data;
	batteryIica.rxCount = length;
	batteryIica.txData = reg;
	batteryIica.txCount = 1;
	batteryIica.address = 0xaa;
	batteryIica.status = &status;
	iicaTransfer(&batteryIica);
	for (wait = 0xff; wait != 0; wait--)
		NOP();
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

/* the battery's identity (struct BatteryInfo's four words) */
unsigned char batteryReadInfo(unsigned int *info)
{
	unsigned char result;

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
						powerWaitMs(4);
						elmoCommand[0] = 0x40;
						result = batteryRead(elmoCommand,
								     (unsigned char *)(info + 3), 2);
					}
				}
			}
		}
	}
	if (result == 1)
		;
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
