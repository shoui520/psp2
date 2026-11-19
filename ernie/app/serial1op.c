/*
 * The serial link's mode 1 operations that answer later (serial1Command0940, 0941, 0180-0182):
 * task 5 runs the one in serial1State and sends its answer, the data in serial1Args.
 */
#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "battery.h"
#include "charge.h"
#include "cookie.h"
#include "elmo.h"
#include "serial.h"
#include "serial1.h"
#if SOURCE_0103
#include "powerseq.h"
#include "thermal.h"

/* the charge cases answer only in the manual charge mode; DOL-1001 always answers */
#if BOARD_DOL1001
#define SERIAL1_MANUAL_MODE	1
#else
#define SERIAL1_MANUAL_MODE	(chargeGetManualMode() == 1)
#endif
#endif

/* a battery temperature in 0.1 K less this is in 0.1 degrees C */
#define SERIAL1_KELVIN		0x0aac

#if SOURCE_0103
/* the answers, when the command is still waiting: with status 0x60 when result is not 0, ... */
#define SERIAL1_ANSWER_RESULT(answerCommand, answer, length)					\
	if (serial.state == SERIAL_ANSWER_LATER) {					\
		serial.command = (answerCommand);						\
		if (result)								\
			serial.status = 0x60;						\
		else									\
			serial.status = 0;						\
		serial.data = (answer);							\
		serial.dataLength = (length);						\
		serial1State = 0;							\
		serialSetAnswer(SERIAL_ANSWER);						\
		serialPostAnswer();							\
	}

/* ... with result as the status and no data, ... */
#define SERIAL1_ANSWER_STATUS(answerCommand)							\
	if (serial.state == SERIAL_ANSWER_LATER) {					\
		serial.command = (answerCommand);						\
		serial.status = result;							\
		serial.data = NULL;							\
		serial.dataLength = 0;							\
		serial1State = 0;							\
		serialSetAnswer(SERIAL_ANSWER);						\
		serialPostAnswer();							\
	}

/* ... or with status 0 and serial1Args' first byte */
#define SERIAL1_ANSWER_BYTE(answerCommand)							\
	if (serial.state == SERIAL_ANSWER_LATER) {					\
		serial.command = (answerCommand);						\
		serial.status = 0;							\
		serial.data = serial1Args.byte;						\
		serial.dataLength = 1;							\
		serial1State = 0;							\
		serialSetAnswer(SERIAL_ANSWER);						\
		serialPostAnswer();							\
	}
#endif

void serial1Operation(void)
{
	unsigned char result;
#if FW_SINCE(0x00090903)
	unsigned char reg;
#endif
#if SOURCE_0103
	unsigned char current;		/* a current's Cookie setting (0x0185, 0x0187) */
	struct BatteryInfo info;
#endif

	switch (serial1State) {
#if SOURCE_0103
	case 0x0a:	/* 0x0930: the Elmo's version */
		result = elmoReadVersion(serial1Args.byte);
		SERIAL1_ANSWER_RESULT(0x8930, serial1Args.byte, 2);
		break;
	case 0x0b:	/* 0x0931: the Cookie's */
		result = cookieReadVersion(serial1Args.byte);
		SERIAL1_ANSWER_RESULT(0x8931, serial1Args.byte, 2);
		break;
	case 0x0c:	/* 0x0932: the battery's identity */
		result = batteryReadInfo(serial1Args.word);
		SERIAL1_ANSWER_RESULT(0x8932, serial1Args.byte, 12);
		break;
#endif
	case 1:		/* 0x0940: the Elmo's register byte[0], into byte[1] */
		result = elmoRead(&serial1Args.byte[0], &serial1Args.byte[1]);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8940;
			if (result)
				serial.status = 0x60;
			else
				serial.status = 0;
			serial.data = &serial1Args.byte[1];
			serial.dataLength = 2;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 2:		/* 0x0941: the three bytes to the Elmo */
		result = elmoWrite(serial1Args.byte, 3);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8941;
			if (result)
				serial.status = 0x60;
			else
				serial.status = 0;
			serial.data = NULL;
			serial.dataLength = 0;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
#if FW_SINCE(0x0100060B)
		break;
#else
		/* no break in Sony's before 0x0100060B: operation 3 runs too */
#endif
	case 3:		/* 0x0942: the Cookie's register */
		result = cookieRead(&serial1Args.byte[0], &serial1Args.byte[1]);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8942;
			if (result)
				serial.status = 0x60;
			else
				serial.status = 0;
			serial.data = &serial1Args.byte[1];
			serial.dataLength = 2;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 4:		/* 0x0943: to the Cookie */
		result = cookieWrite(serial1Args.byte, 3);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8943;
			if (result)
				serial.status = 0x60;
			else
				serial.status = 0;
			serial.data = NULL;
			serial.dataLength = 0;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
#if FW_SINCE(0x0100060B)
		break;
#else
		/* no break in Sony's before 0x0100060B: operation 5 runs too */
#endif
	case 5:		/* 0x0944: the battery's register */
		result = batteryRead(&serial1Args.byte[0], &serial1Args.byte[1], 2);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8944;
			if (result)
				serial.status = 0x60;
			else
				serial.status = 0;
			serial.data = &serial1Args.byte[1];
			serial.dataLength = 2;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 6:		/* 0x0945: to the battery */
		result = batteryWrite(serial1Args.byte, 3);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8945;
			if (result)
				serial.status = 0x60;
			else
				serial.status = 0;
			serial.data = NULL;
			serial.dataLength = 0;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
#if FW_SINCE(0x00090903)
	case 7:		/* 0x0180: the battery's readings, and its registers 0x2c and 0x14 */
		serial.status = 0;
		serial1Args.word[0] = charge.batteryRemainCap;
		serial1Args.word[1] = charge.batteryVolt;
		serial1Args.word[2] = charge.batteryTemp - SERIAL1_KELVIN;
		serial1Args.word[3] = charge.batteryBicTemp - SERIAL1_KELVIN;
		reg = 0x2c;
		if (batteryRead(&reg, (unsigned char *)&serial1Args.word[4], 2))
			serial.status = 0x60;
		reg = 0x14;
		if (batteryRead(&reg, (unsigned char *)&serial1Args.word[5], 2))
			serial.status = 0x60;
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8180;
			serial.data = serial1Args.byte;
			serial.dataLength = 12;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 8:		/* 0x0181: the gauge's control 0, and its registers 0x0a, 0x28, 0x2a */
		serial.status = 0;
		if (batteryControl(0, serial1Args.byte, 2))
			serial.status = 0x60;
		reg = 0x0a;
		if (batteryRead(&reg, (unsigned char *)&serial1Args.word[1], 2))
			serial.status = 0x60;
		reg = 0x28;
		if (batteryRead(&reg, (unsigned char *)&serial1Args.word[2], 2))
			serial.status = 0x60;
		reg = 0x2a;
		if (batteryRead(&reg, (unsigned char *)&serial1Args.word[3], 2))
			serial.status = 0x60;
		memset(&serial1Args.word[4], 0, 4);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8181;
			serial.data = serial1Args.byte;
			serial.dataLength = 12;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 9:		/* 0x0182: the gauge's control 0x0015 */
		result = batteryControl0015();
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8182;
			if (result)
				serial.status = 0x60;
			else
				serial.status = 0;
			serial.data = NULL;
			serial.dataLength = 0;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
#endif
#if SOURCE_0103
	case 0x0d:	/* 0x0953: the battery's faults */
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8953;
			batteryGetFaults(serial1Args.byte);
			serial.data = serial1Args.byte;
			serial.dataLength = 8;
			serial.status = 0;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 0x0e:	/* 0x0183: the manual charge mode on or off; 0x53 refused */
		result = chargeSetManualMode(serial1Args.byte[0] ? 1 : 0);
		if (result == 0xff)
			result = 0x53;
		else if (result)
			result = 0x60;
		else
			result = 0;
		SERIAL1_ANSWER_STATUS(0x8183);
		break;
	case 0x0f:	/* 0x0184: in the manual charge mode, the charge on or off */
		if (SERIAL1_MANUAL_MODE) {
			result = cookieCtrlBatteryCharge(serial1Args.byte[0] ? 1 : 0);
			if (result)
				result = 0x60;
			else
				result = 0;
		} else {
			result = 0xff;
		}
		SERIAL1_ANSWER_STATUS(0x8184);
		break;
	case 0x10:	/* 0x0185: and the charging current; 0x33 a current it has no setting for */
		if (SERIAL1_MANUAL_MODE) {
			result = cookieChargingCurrentEncode(&current, serial1Args.byte[0]);
			if (result) {
				result = 0x33;
			} else {
				result = cookieCtrlChargingCurrent(current);
				if (result)
					result = 0x60;
				else
					result = 0;
			}
		} else {
			result = 0xff;
		}
		SERIAL1_ANSWER_STATUS(0x8185);
		break;
#if BOARD_DOL1001
	case 0x11:	/* 0x0186: the AC supply's current */
		if (SERIAL1_MANUAL_MODE) {
			result = cookieSupplyCurrentEncode(&current, serial1Args.byte[0]);
			if (result) {
				result = 0x33;
			} else {
				result = cookieCtrlSupplyCurrent(current);
				if (result)
					result = 0x60;
				else
					result = 0;
			}
		} else {
			result = 0xff;
		}
		SERIAL1_ANSWER_STATUS(0x8186);
		break;
#else
	case 0x11:	/* 0x0186: nothing, in the manual charge mode */
		if (SERIAL1_MANUAL_MODE)
			result = 0;
		else
			result = 0xff;
		SERIAL1_ANSWER_STATUS(0x8186);
		break;
#endif
	case 0x12:	/* 0x0187: the USB supply's current */
		if (SERIAL1_MANUAL_MODE) {
			result = cookieSupplyCurrentUsbEncode(&current, serial1Args.byte[0]);
			if (result) {
				result = 0x33;
			} else {
				result = cookieCtrlSupplyCurrentUsb(current);
				if (result)
					result = 0x60;
				else
					result = 0;
			}
		} else {
			result = 0xff;
		}
		SERIAL1_ANSWER_STATUS(0x8187);
		break;
	case 0x13:	/* 0x0188: the AC's charge */
		if (SERIAL1_MANUAL_MODE) {
			result = cookieSetChargeAc(serial1Args.byte[0] ? 1 : 0);
			if (result)
				result = 0x60;
			else
				result = 0;
		} else {
			result = 0xff;
		}
		SERIAL1_ANSWER_STATUS(0x8188);
		break;
	case 0x14:	/* 0x0189: the Cookie's register 6 bit 3 */
		if (SERIAL1_MANUAL_MODE) {
			result = cookieCtrlReg6Bit3(serial1Args.byte[0] ? 1 : 0);
			if (result)
				result = 0x60;
			else
				result = 0;
		} else {
			result = 0xff;
		}
		SERIAL1_ANSWER_STATUS(0x8189);
		break;
	case 0x15:	/* 0x018a: the Cookie's register 1 */
		result = cookieReadReg1(serial1Args.byte);
		if (result)
			serial.status = 0x60;
		else
			serial.status = 0;
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x818a;
			serial.status = result;
			serial.data = serial1Args.byte;
			serial.dataLength = 4;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 0x16:	/* 0x018b: the manual charge mode */
		serial1Args.byte[0] = chargeGetManualMode();
		SERIAL1_ANSWER_BYTE(0x818b);
		break;
	case 0x17:	/* 0x018c: the charging current set, 0 for one it has no value for */
		if (cookieChargingCurrentDecode(serial1Args.byte, cookieGetChargingCurrent()))
			serial1Args.byte[0] = 0;
		SERIAL1_ANSWER_BYTE(0x818c);
		break;
	case 0x18:	/* 0x018d: the supply's */
		if (cookieSupplyCurrentDecode(serial1Args.byte, cookieGetSupplyCurrent()))
			serial1Args.byte[0] = 0;
		SERIAL1_ANSWER_BYTE(0x818d);
		break;
	case 0x19:	/* 0x018e: the USB supply's */
		if (cookieSupplyCurrentUsbDecode(serial1Args.byte, cookieGetSupplyCurrentUsb()))
			serial1Args.byte[0] = 0;
		SERIAL1_ANSWER_BYTE(0x818e);
		break;
	case 0x1a:	/* 0x0108: the devices' versions, read again; status 0x98 when one failed */
		memset(serial1Args.byte, 0, sizeof(serial1Args.byte));
		serial1Args.device[0].type = 0x10;
		result = serial1Args.device[0].result =
			elmoReadVersion((unsigned char *)serial1Args.device[0].data);
		if (deviceVersions.elmo[0] == 0xff)
			serial1Args.device[0].result = 0xff;
		serial1Args.device[1].type = 0x20;
		serial1Args.device[1].result =
			cookieReadVersion((unsigned char *)serial1Args.device[1].data);
		result |= serial1Args.device[1].result;
		if (deviceVersions.cookie[0] == 0xff)
			serial1Args.device[1].result = 0xff;
		serial1Args.device[2].type = 0x30;
		serial1Args.device[3].type = 0x31;
		serial1Args.device[2].result = serial1Args.device[3].result =
			batteryReadInfo((unsigned int *)&info);
		serial1Args.device[2].data[0] = info.version[0];
		serial1Args.device[2].data[1] = info.version[1];
		serial1Args.device[2].data[2] = info.version[2];
		serial1Args.device[3].data[0] = info.id;
		serial1Args.device[3].data[1] = info.version[3];
		serial1Args.device[3].data[2] = info.version[4];
		result |= serial1Args.device[2].result;
		if (deviceVersions.battery.id == 0xffff)
			serial1Args.device[2].result = serial1Args.device[3].result = 0xff;
		serial1Args.device[4].type = 0x40;
#if BOARD_DOL1001
		serial1Args.device[4].result =
			thermalReadManufacturer((unsigned char *)serial1Args.device[4].data);
		serial1Args.device[4].result |=
			thermalReadDevice((unsigned char *)serial1Args.device[4].data + 1);
		result |= serial1Args.device[4].result;
#else
		serial1Args.device[4].result = 0xff;
		memset(serial1Args.device[4].data, 0xff, 2);
#endif
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x8108;
			if (result)
				serial.status = 0x98;
			else
				serial.status = 0;
			serial.data = serial1Args.byte;
			serial.dataLength = sizeof(serial1Args.byte);
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
#endif
#if BOARD_DOL1001
	case 0x1b:	/* 0x01e0: the temperature statistics */
		thermalStatsGet(&serial1Args.thermal);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x81e0;
			serial.status = 0;
			serial.data = serial1Args.byte;
			serial.dataLength = 0x10;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	case 0x1c:	/* 0x01e1: the statistics cleared */
		thermalStatsClear(serial1Args.byte[0]);
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command = 0x81e1;
			serial.status = 0;
			serial.data = NULL;
			serial.dataLength = 0;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
#endif
	default:
		if (serial.state == SERIAL_ANSWER_LATER) {
			serial.command |= 0x8000;
			serial.status = 0x60;
			serial.data = NULL;
			serial.dataLength = 0;
			serial1State = 0;
			serialSetAnswer(SERIAL_ANSWER);
			serialPostAnswer();
		}
		break;
	}
}
