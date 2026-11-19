/*
 * The battery: its gauge's registers over the I2C bus, while one answered at the power-on
 * (deviceVersions.battery.id not BATTERY_NONE), and its bootloader's commands.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "board.h"
#include "battery.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "cookie.h"
#include "eventlog.h"
#include "iica.h"
#include "kernel.h"
#include "powerseq.h"
#include "powerwait.h"

/* the battery's address on the I2C bus: the Cookie's where the settings move it (DOL-1001 moves
   the battery only, its Cookie stays at 0xa8) */
#if BOARD_COOKIE_REG_HIGH || BOARD_DOL1001
#define BATTERY_ADDRESS		(configGet()->cookieRegHigh ? 0xa4 : 0xaa)
#else
#define BATTERY_ADDRESS		0xaa
#endif

/* the charge stopped while the bootloader runs: on the slim out of the manual charge mode */
#if SOURCE_0103
#define BATTERY_CHARGE(on)	cookieUpdateBatteryCharge(on)
#else
#define BATTERY_CHARGE(on)	cookieCtrlBatteryCharge(on)
#endif

/* the battery's id when none answered at the power-on */
#if SOURCE_0103
#define BATTERY_NONE		0xffff
#else
#define BATTERY_NONE		0x00ff
#endif

static struct IicaRequest batteryIica;
static unsigned char batteryBLBuffer[BATTERY_BL_SIZE - 1];
struct BatteryBLCommand batteryBLCommand;
#if SOURCE_0103
union BatteryFaults batteryFaults;
#endif

void batteryInit(void)
{
	batteryBLCommand.command = 0;
	batteryBLCommand.length = 0;
	batteryBLCommand.buffer = batteryBLBuffer;
#if SOURCE_0103
	memset(batteryFaults.byte, 0, sizeof(batteryFaults));
#endif
}

#if SOURCE_0103
/* after batteryReadInfo answered (charge.c): nothing */
void batteryInfoRead(void)
{
}
#endif

/* data[0] is the register */
unsigned char batteryWrite(unsigned char *data, unsigned char length)
{
	unsigned char status;
	ER ercd;
	FLGPTN ptn;

	if (deviceVersions.battery.id == BATTERY_NONE)
		return 0;
	batteryIica.rxData = NULL;
	batteryIica.rxCount = 0;
	batteryIica.txData = data;
	batteryIica.txCount = length;
	batteryIica.address = BATTERY_ADDRESS;
	batteryIica.status = &status;
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
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

	if (deviceVersions.battery.id == BATTERY_NONE) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
	batteryIica.rxData = data;
	batteryIica.rxCount = length;
	batteryIica.txData = reg;
	batteryIica.txCount = 1;
	batteryIica.address = BATTERY_ADDRESS;
	batteryIica.status = &status;
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
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

	if (deviceVersions.battery.id == BATTERY_NONE) {
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

#if !FW_SINCE(0x00090206)	/* 0x00090009's order */
/* a write to the device at packet[0]: the rest of the length bytes */
unsigned char batteryWriteAt(unsigned char *packet, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == BATTERY_NONE)
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

/* a register of the device at packet[0] (packet[1] the register); a timeout resets the bus */
unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == BATTERY_NONE) {
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
#endif

/* the battery's identity (struct BatteryInfo's four words): 0xfffe for each where a battery
   without a gauge answered */
#if SOURCE_0103
/* the slim's: the id, the type twice (the second word from 0x0416 on: 0xa8 for a 0x0802
   battery of type 0xa7 or 0xa8), the version, then the bootloader's (0xfffd before 0x0416); with
   none answered all 0xffff */
/* the words as Sony's sets them here: a struct's, not the array's. The gauge's control
   commands 1 (the device's type), 3 (the hardware's version), 2 (the firmware's) and 6; the
   data flash's class 0x39 */
struct BatteryInfoWords {
	unsigned int	id;		/* command 1 */
	unsigned int	type;		/* the hardware's, or 0xa8 */
	unsigned int	firmware;	/* command 2 */
	unsigned int	dataFlash;	/* class 0x39's first word */
	unsigned int	command6;	/* from firmware 0x0416 */
	unsigned int	hardware;	/* command 3 */
};
#define BATTERY_INFO(info)	((struct BatteryInfoWords *)(info))

unsigned char batteryReadInfo(unsigned int *info)
{
	unsigned char result;
	unsigned char status;

	if (deviceVersions.battery.id == BATTERY_NONE) {
		BATTERY_INFO(info)->id = BATTERY_INFO(info)->type = BATTERY_INFO(info)->firmware =
			BATTERY_INFO(info)->dataFlash = BATTERY_INFO(info)->command6 =
			BATTERY_INFO(info)->hardware = 0xffff;
		return 0;
	}
	result = batteryControl(1, (unsigned char *)&BATTERY_INFO(info)->id, 2);
	if (result == 0) {
		result = batteryControl(3,
				(unsigned char *)&BATTERY_INFO(info)->hardware, 2);
		if (result == 0) {
			result = batteryControl(2,
					(unsigned char *)&BATTERY_INFO(info)->firmware, 2);
			if (result == 0) {
				BATTERY_INFO(info)->type = BATTERY_INFO(info)->hardware;
				if (BATTERY_INFO(info)->firmware >= 0x0416) {
					result = batteryControl(6,
							(unsigned char *)&BATTERY_INFO(info)->command6, 2);
					if (result != 0)
						goto failed;
					if ((BATTERY_INFO(info)->hardware == 0xa7 ||
					     BATTERY_INFO(info)->hardware == 0xa8) &&
					    BATTERY_INFO(info)->command6 == 0x0802)
						BATTERY_INFO(info)->type = 0xa8;
				} else
					BATTERY_INFO(info)->command6 = 0xfffd;
				elmoCommand[0] = 0x3e;
				elmoCommand[1] = 0x39;
				result = batteryWrite(elmoCommand, 2);
				if (result == 0) {
					elmoCommand[0] = 0x3f;
					elmoCommand[1] = 0;
					result = batteryWrite(elmoCommand, 2);
					if (result == 0) {
						powerSleep2(4);
						elmoCommand[0] = 0x40;
						result = batteryRead(elmoCommand,
							(unsigned char *)&BATTERY_INFO(info)->dataFlash, 2);
					}
				}
			}
		}
	}
failed:
	if (result == 1) {
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0;
		result = batteryReadAt(elmoCommand, &status, 1);
		if (result == 0) {
			BATTERY_INFO(info)->id = 0xfffe;
			BATTERY_INFO(info)->type = 0xfffe;
			BATTERY_INFO(info)->firmware = 0xfffe;
			BATTERY_INFO(info)->dataFlash = 0xfffe;
			BATTERY_INFO(info)->command6 = 0xfffe;
			BATTERY_INFO(info)->hardware = 0xfffe;
		}
	}
	return result;
}
#else
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
						powerSleep2(4);
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
#endif

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

#if FW_SINCE(0x00090206)	/* 0x00090009 has no internal temperature */
/* its internal temperature (0x20), from version 0x0404 on; before it 0x0aac (0 degrees) */
unsigned char batteryReadBicTemp(unsigned int *value)
{
	unsigned char reg;

#if SOURCE_0103
	reg = 0x20;					/* the slim's: whatever the version */
	return batteryRead(&reg, (unsigned char *)value, 2);
#else
	if (deviceVersions.battery.version[1] >= 0x0404) {
		reg = 0x20;
		return batteryRead(&reg, (unsigned char *)value, 2);
	}
	*value = 0x0aac;
	return 0;
#endif
}
#endif

#if FW_SINCE(0x00090009)	/* 0x00080504 reads no state of charge */
/* its state of charge in percent (0x2c) */
unsigned char batteryReadLifePercent(unsigned int *value)
{
	unsigned char reg;

	reg = 0x2c;
	return batteryRead(&reg, (unsigned char *)value, 2);
}
#endif

#if FW_SINCE(0x00090206)
/* a register of the device at packet[0] (packet[1] the register); a timeout resets the bus */
unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == BATTERY_NONE) {
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
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
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

	if (deviceVersions.battery.id == BATTERY_NONE)
		return 0;
	if (length != 0) {
		batteryIica.txCount = length - 1;
		batteryIica.rxData = NULL;
		batteryIica.rxCount = 0;
		batteryIica.txData = packet + 1;
		batteryIica.address = *packet;
		batteryIica.status = &status;
#if FW_SINCE(0x0100060B)
		clr_flg(4, ~0x10);
#endif
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
#else	/* 0x00090009's order */
#if FW_SINCE(0x00090009)	/* 0x00080504 has no bootloader line and no hibernation */
/* 0x08b0: the battery's bootloader line */
void batteryBLLineOn(void)
{
#if !BOARD_102603
	P96 = 1;
#endif
}

void batteryBLLineOff(void)
{
#if !BOARD_102603
	P96 = 0;
#endif
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
#endif
#endif

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
#if FW_SINCE(0x00090804)
	if (result == 0) {
		BATTERY_CHARGE(0);
		chargeTimerStop();
	}
#endif
	return result;
}

/* 0x09b1: the bootloader's commands 0x00 0x0f, 0x64 0x0f and 0x65 0x00 */
unsigned char batteryStopBLMode(void)
{
	unsigned char result;

#if FW_SINCE(0x00090804)
	chargeTimerStart(0x0a);
#endif
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
#if FW_SINCE(0x00090804)
	BATTERY_CHARGE(1);
#endif
	return result;
}

/* 0x0989: the gauge's control command 0x0041 (its reset) */
unsigned char batterySWReset(void)
{
#if FW_SINCE(0x00090804)
	chargeTimerStart(0x0a);
#endif
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x41;
	elmoCommand[2] = 0;
	return batteryWrite(elmoCommand, 3);
}

#if SOURCE_0103
/* 0x08b0: the battery's bootloader line, P6.4 on the slim, recorded in the faults */
void batteryBLLineOn(void)
{
#if !BOARD_DOL1001	/* DOL-1001 has no bootloader line */
	P6.4 = 1;
	batteryFaults.byte[1] |= 0x01;
#if BOARD_EVENT_LOG
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
#endif
}

void batteryBLLineOff(void)
{
#if !BOARD_DOL1001
	P6.4 = 0;
	batteryFaults.byte[1] &= ~0x01;
#if BOARD_EVENT_LOG
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
#endif
}

/* at the power-off: the manual charge mode left, the faults recorded and cleared, then the
   gauge's control command 0x0011 (its hibernation) whatever the version */
unsigned char batteryHibernate(void)
{
	chargeSetManualMode(0);
	batteryFaults.byte[1] |= 0x02;
#if BOARD_EVENT_LOG
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
	batteryFaults.byte[0] = 0;
	batteryFaults.word[2] = 0;
	batteryFaults.word[3] = 0;
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x11;
	elmoCommand[2] = 0;
	return batteryWrite(elmoCommand, 3);
}
#elif FW_SINCE(0x00090206)
/* 0x08b0: the battery's bootloader line */
void batteryBLLineOn(void)
{
#if !BOARD_102603
	P96 = 1;
#endif
}

void batteryBLLineOff(void)
{
#if !BOARD_102603
	P96 = 0;
#endif
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
#endif

#if FW_SINCE(0x00090903)
/* 0x098d: the gauge's control command 0x0015 */
unsigned char batteryControl0015(void)
{
	chargeTimerStart(0x0a);
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x15;
	elmoCommand[2] = 0;
	return batteryWrite(elmoCommand, 3);
}
#endif

#if SOURCE_0103
/* the faults record's eight bytes */
void batteryGetFaults(unsigned char *faults)
{
	memcpy(faults, batteryFaults.byte, sizeof(batteryFaults));
}
#endif
