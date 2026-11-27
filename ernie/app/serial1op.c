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

/* DOL-1002's operation, and USS-1002's from it */
#define SERIAL1_OP_0104		(BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104)

/* USS-1002 (the slim's 0x0105): the switch runs whether or not the command waits, the gauge's
   calibration operations 0x21-0x23, no thermal statistics, the charge cases only in the manual
   charge mode */
#define SERIAL1_OP_USS		(BOARD_USS1001 && SOURCE_0104)

#if SERIAL1_OP_0104
/* DOL-1002 answers only while the command waits, through serial1Answer, and reads 0x0180's and
   0x0181's registers in functions of their own, which 0x0954 reads again */
#define SERIAL1_STATUS(result)	((result) ? 0x60 : 0)

/* 0x0180's answer: the battery's readings and two of its registers */
struct Serial1Battery {
	unsigned int	remainCap;
	unsigned int	volt;
	unsigned int	temp;		/* in 0.1 degrees C */
	unsigned int	bicTemp;
	unsigned int	reg2c;
	unsigned int	reg14;
};

/* 0x0181's: the gauge's control 0 and five of its registers */
struct Serial1Gauge {
	unsigned int	control;
	unsigned int	reg0a;
	unsigned int	reg28;
	unsigned int	reg2a;
	unsigned int	reg12;
	unsigned int	reg22;
};

/* 0x0954's */
struct Serial1Status {
	struct Serial1Battery	battery;
	struct Serial1Gauge	gauge;
	unsigned char		cookieStatus1[2];	/* cookieReadStatus1's */
	unsigned char		cookieStatus2[2];
	unsigned char		cookieReg4;	/* the Cookie's registers */
	unsigned char		cookieReg6;
	unsigned char		cookieReg7;
	unsigned char		cookieReg8;
	unsigned char		reserved[8];
};

static unsigned char serial1Read0180(struct Serial1Battery *out);
static unsigned char serial1Read0181(struct Serial1Gauge *out);
static unsigned char serial1Read0954(struct Serial1Status *out, unsigned char parts);

/* the operation's answer: command with bit 15 set, its status, data and length */
static void serial1Answer(unsigned int command, unsigned char status, unsigned char *data,
			  unsigned int length)
{
	serial.command = command | 0x8000;
	serial.status = status;
	serial.data = data;
	serial.dataLength = length;
	serial1State = 0;
	serialSetAnswer(SERIAL_ANSWER);
	serialPostAnswer();
}

void serial1Operation(void)
{
	unsigned char result;
	unsigned char current;		/* a current's Cookie setting (0x0185-0x0187) */
	struct BatteryInfo info;

#if SERIAL1_OP_USS
	result = 0xff;
	{
#else
	if (serial.state == SERIAL_ANSWER_LATER) {
#endif
		switch (serial1State) {
		case 0x0a:	/* 0x0930: the Elmo's version */
			result = elmoReadVersion(serial1Args.byte);
			serial1Answer(0x0930, SERIAL1_STATUS(result), serial1Args.byte, 2);
			break;
		case 0x0b:	/* 0x0931: the Cookie's */
			result = cookieReadVersion(serial1Args.byte);
			serial1Answer(0x0931, SERIAL1_STATUS(result), serial1Args.byte, 2);
			break;
		case 0x0c:	/* 0x0932: the battery's identity */
			result = batteryReadInfo(serial1Args.word);
			serial1Answer(0x0932, SERIAL1_STATUS(result), serial1Args.byte, 12);
			break;
		case 1:		/* 0x0940: the Elmo's register byte[0], into byte[1] */
			result = elmoRead(&serial1Args.byte[0], &serial1Args.byte[1]);
			serial1Answer(0x0940, SERIAL1_STATUS(result), &serial1Args.byte[1], 2);
			break;
		case 2:		/* 0x0941: the three bytes to the Elmo */
			result = elmoWrite(serial1Args.byte, 3);
			serial1Answer(0x0941, SERIAL1_STATUS(result), NULL, 0);
			break;
		case 3:		/* 0x0942: the Cookie's register */
			result = cookieRead(&serial1Args.byte[0], &serial1Args.byte[1]);
			serial1Answer(0x0942, SERIAL1_STATUS(result), &serial1Args.byte[1], 2);
			break;
		case 4:		/* 0x0943: to the Cookie */
			result = cookieWrite(serial1Args.byte, 3);
			serial1Answer(0x0943, SERIAL1_STATUS(result), NULL, 0);
			break;
#if SERIAL1_OP_USS
		case 5:		/* 0x0944, 0x0945: no battery on the bus */
			serial1Answer(0x0944, 0x60, &serial1Args.byte[1], 0);
			break;
		case 6:
			serial1Answer(0x0944, 0x60, &serial1Args.byte[1], 0);
			break;
#else
		case 5:		/* 0x0944: the battery's register */
			result = batteryRead(&serial1Args.byte[0], &serial1Args.byte[1], 2);
			serial1Answer(0x0944, SERIAL1_STATUS(result), &serial1Args.byte[1], 2);
			break;
		case 6:		/* 0x0945: to the battery */
			result = batteryWrite(serial1Args.byte, 3);
			serial1Answer(0x0945, SERIAL1_STATUS(result), NULL, 0);
			break;
#endif
		case 7:		/* 0x0180: the battery's readings and registers */
			result = serial1Read0180((struct Serial1Battery *)serial1Args.word);
			serial1Answer(0x0180, SERIAL1_STATUS(result), serial1Args.byte, 12);
			break;
#if SERIAL1_OP_USS
		case 0x21:	/* 0x0191: the gauge's offsets, used and saved */
			batteryGetCalibration((struct BatteryCalibration *)serial1Args.word);
			serial1Answer(0x0191, 0, serial1Args.byte, 8);
			break;
		case 0x23:	/* 0x0192: the offsets loaded again, the charge from the load's
				   voltage first when byte[0] is 0 */
			if (serial1Args.byte[0] == 0)
				batteryChargeFromLoad();
			batteryLoadOffsets();
			batteryUpdate();
			chargeUpdate();
			serial1Answer(0x0192, 0, serial1Args.byte, 0);
			break;
		case 0x22:	/* 0x018f: the gauge calibrated in mode byte[0] */
			result = batteryCalibrate(serial1Args.byte[0],
						  (struct BatteryOffsets *)serial1Args.word);
			serial1Answer(0x018f, result, serial1Args.byte, 4);
			break;
#endif
		case 8:		/* 0x0181: the gauge's */
			result = serial1Read0181((struct Serial1Gauge *)serial1Args.word);
			serial1Answer(0x0181, SERIAL1_STATUS(result), serial1Args.byte, 12);
			break;
		case 9:		/* 0x0182: the gauge's control 0x0015 (USS-1002: the bootloader
				   mode stopped) */
#if SERIAL1_OP_USS
			result = batteryStopBLMode();
#else
			result = batteryControl0015();
#endif
			serial1Answer(0x0182, SERIAL1_STATUS(result), NULL, 0);
			break;
		case 0x0d:	/* 0x0953: the battery's faults */
			batteryGetFaults(serial1Args.byte);
			serial1Answer(0x0953, 0, serial1Args.byte, 8);
			break;
		case 0x0e:	/* 0x0183: the manual charge mode on or off; 0x53 refused */
			result = chargeSetManualMode(serial1Args.byte[0] ? 1 : 0);
			if (result == 0xff)
				result = 0x53;
			else if (result)
				result = 0x60;
			else
				result = 0;
			serial1Answer(0x0183, result, NULL, 0);
			break;
		case 0x0f:	/* 0x0184: the charge on or off */
#if SERIAL1_OP_USS
			if (chargeGetManualMode() == 1) {
#endif
				result = cookieCtrlBatteryCharge(serial1Args.byte[0] ? 1 : 0);
				if (result)
					result = 0x60;
				else
					result = 0;
#if SERIAL1_OP_USS
			} else {
				result = 0xff;
			}
#endif
			serial1Answer(0x0184, result, NULL, 0);
			break;
		case 0x10:	/* 0x0185: the charging current; 0x33 a current it has no setting for */
#if SERIAL1_OP_USS
			if (chargeGetManualMode() == 1) {
#endif
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
#if SERIAL1_OP_USS
			} else {
				result = 0xff;
			}
#endif
			serial1Answer(0x0185, result, NULL, 0);
			break;
		case 0x11:	/* 0x0186: the AC supply's current (USS-1002: nothing to set) */
#if SERIAL1_OP_USS
			if (chargeGetManualMode() == 1)
				result = 0;
			else
				result = 0xff;
#else
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
#endif
			serial1Answer(0x0186, result, NULL, 0);
			break;
		case 0x12:	/* 0x0187: the USB supply's current */
#if SERIAL1_OP_USS
			if (chargeGetManualMode() == 1) {
#endif
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
#if SERIAL1_OP_USS
			} else {
				result = 0xff;
			}
#endif
			serial1Answer(0x0187, result, NULL, 0);
			break;
		case 0x13:	/* 0x0188: the AC's charge */
#if SERIAL1_OP_USS
			if (chargeGetManualMode() == 1) {
#endif
				result = cookieSetChargeAc(serial1Args.byte[0] ? 1 : 0);
				if (result)
					result = 0x60;
				else
					result = 0;
#if SERIAL1_OP_USS
			} else {
				result = 0xff;
			}
#endif
			serial1Answer(0x0188, result, NULL, 0);
			break;
		case 0x14:	/* 0x0189: the Cookie's register 6 bit 3 */
#if SERIAL1_OP_USS
			if (chargeGetManualMode() == 1) {
#endif
				result = cookieCtrlReg6Bit3(serial1Args.byte[0] ? 1 : 0);
				if (result)
					result = 0x60;
				else
					result = 0;
#if SERIAL1_OP_USS
			} else {
				result = 0xff;
			}
#endif
			serial1Answer(0x0189, result, NULL, 0);
			break;
		case 0x15:	/* 0x018a: the Cookie's register 1 */
			result = cookieReadReg1(serial1Args.byte);
			serial1Answer(0x018a, SERIAL1_STATUS(result), serial1Args.byte, 4);
			break;
		case 0x16:	/* 0x018b: the manual charge mode */
			serial1Args.byte[0] = chargeGetManualMode();
			serial1Answer(0x018b, 0, serial1Args.byte, 1);
			break;
		case 0x17:	/* 0x018c: the charging current set, 0 for one it has no value for */
			if (cookieChargingCurrentDecode(serial1Args.byte, cookieGetChargingCurrent()))
				serial1Args.byte[0] = 0;
			serial1Answer(0x018c, 0, serial1Args.byte, 1);
			break;
		case 0x18:	/* 0x018d: the supply's */
			if (cookieSupplyCurrentDecode(serial1Args.byte, cookieGetSupplyCurrent()))
				serial1Args.byte[0] = 0;
			serial1Answer(0x018d, 0, serial1Args.byte, 1);
			break;
		case 0x19:	/* 0x018e: the USB supply's */
			if (cookieSupplyCurrentUsbDecode(serial1Args.byte, cookieGetSupplyCurrentUsb()))
				serial1Args.byte[0] = 0;
			serial1Answer(0x018e, 0, serial1Args.byte, 1);
			break;
		case 0x1a:	/* 0x0108: the devices' versions; status 0x98 when the Elmo's failed.
				   The Cookie and the battery report 0xff, no device 4 (0xfdfd) */
			memset(serial1Args.byte, 0, sizeof(serial1Args.byte));
			result = 0;
			serial1Args.device[0].type = 0x10;
			serial1Args.device[0].result =
				elmoReadVersion((unsigned char *)serial1Args.device[0].data);
			if (serial1Args.device[0].result)
				((unsigned char *)serial1Args.device[0].data)[0] =
					((unsigned char *)serial1Args.device[0].data)[1] = 0xff;
			result |= serial1Args.device[0].result;
			serial1Args.device[1].type = 0x20;
			serial1Args.device[1].result =
				cookieReadVersion((unsigned char *)serial1Args.device[1].data);
			if (serial1Args.device[1].result)
				((unsigned char *)serial1Args.device[1].data)[0] =
					((unsigned char *)serial1Args.device[1].data)[1] = 0xff;
#if SERIAL1_OP_USS
			result |= serial1Args.device[1].result;
#else
			serial1Args.device[1].result = 0xff;
#endif
			serial1Args.device[2].type = 0x30;
			serial1Args.device[3].type = 0x31;
			serial1Args.device[2].result = serial1Args.device[3].result =
				batteryReadInfo((unsigned int *)&info);
			if (serial1Args.device[2].result) {
				serial1Args.device[2].data[0] = serial1Args.device[2].data[1] =
					serial1Args.device[2].data[2] = serial1Args.device[3].data[0] =
					serial1Args.device[3].data[1] = serial1Args.device[3].data[2] = 0xffff;
			} else {
				serial1Args.device[2].data[0] = info.version[0];
				serial1Args.device[2].data[1] = info.version[1];
				serial1Args.device[2].data[2] = info.version[2];
				serial1Args.device[3].data[0] = info.id;
				serial1Args.device[3].data[1] = info.version[3];
				serial1Args.device[3].data[2] = info.version[4];
			}
			serial1Args.device[2].result = serial1Args.device[3].result = 0xff;
			serial1Args.device[4].type = 0x40;
			serial1Args.device[4].result = 0xff;
			((unsigned char *)serial1Args.device[4].data)[0] =
				((unsigned char *)serial1Args.device[4].data)[1] = 0xfd;
			serial1Answer(0x0108, result ? (unsigned char)0x98 : (unsigned char)0, serial1Args.byte, 0x28);
			break;
#if !SERIAL1_OP_USS
		case 0x1b:	/* 0x01e0: the temperature statistics */
			thermalStatsGet(&serial1Args.thermal);
			serial1Answer(0x01e0, 0, serial1Args.byte, 0x10);
			break;
		case 0x1c:	/* 0x01e1: the statistics cleared */
			thermalStatsClear(serial1Args.byte[0]);
			serial1Answer(0x01e1, 0, NULL, 0);
			break;
#endif
		case 0x1d:	/* 0x01e2: the temperature, after a zero byte (USS-1002: none) */
			serial1Args.byte[0] = 0;
#if SERIAL1_OP_USS
			serial1Args.byte[1] = 0;
#else
			result = thermalReadTemp((signed char *)&serial1Args.byte[1]);
#endif
			serial1Answer(0x01e2, SERIAL1_STATUS(result), serial1Args.byte, 2);
			break;
		case 0x1e:	/* 0x0955: the charge's serial mode */
			chargeSetSerialMode(serial1Args.byte[0]);
			result = 0;
			serial1Answer(0x0955, result, NULL, 0);
			break;
		case 0x1f:	/* 0x0954: the parts of 0x0180, 0x0181 and the Cookie byte[0] names */
			result = serial1Read0954((struct Serial1Status *)serial1Args.word,
						 serial1Args.byte[0]);
			serial1Answer(0x0954, result, serial1Args.byte, 0x28);
			break;
		case 0x20:	/* 0x01b2: a device reset (powerCtrlDeviceReset) */
			powerCtrlDeviceReset(serial1Args.byte[0], serial1Args.byte[1]);
			serial1Answer(0x01b2, 0, NULL, 0);
			break;
		default:
#if SERIAL1_OP_USS
			serial1Answer(serial.command, result, NULL, 0);
#else
			serial1Answer(serial.command, 0x60, NULL, 0);
#endif
			break;
		}
	}
}

/* 0x0180's: the battery's readings, and its registers 0x2c and 0x14 */
static unsigned char serial1Read0180(struct Serial1Battery *out)
{
	unsigned char result = 0;
	unsigned char reg;

	memset(out, 0, sizeof(*out));
	out->remainCap = charge.batteryRemainCap;
	out->volt = charge.batteryVolt;
	out->temp = charge.batteryTemp - SERIAL1_KELVIN;
	out->bicTemp = charge.batteryBicTemp - SERIAL1_KELVIN;
#if SERIAL1_OP_USS
	/* the gauge's percent and current in place of the registers */
	if (batteryReadLifePercent(&out->reg2c))
		result = 0x60;
	if (batteryReadElec(&out->reg14))
		result = 0x60;
#else
	reg = 0x2c;
	if (batteryRead(&reg, (unsigned char *)&out->reg2c, 2))
		result = 0x60;
	reg = 0x14;
	if (batteryRead(&reg, (unsigned char *)&out->reg14, 2))
		result = 0x60;
#endif
	return result;
}

/* 0x0181's: the gauge's control 0, and its registers 0x0a, 0x28, 0x2a, 0x22 and 0x12 */
static unsigned char serial1Read0181(struct Serial1Gauge *out)
{
	unsigned char result = 0;
	unsigned char reg;

	memset(out, 0, sizeof(*out));
#if SERIAL1_OP_USS
	/* the gauge's flags, life and capacities in place of the registers */
	if (batteryReadFlags(&out->control))
		result = 0x60;
	if (batteryReadInfo6C(&out->reg0a))
		result = 0x60;
	if (batteryReadSOH(&out->reg28))
		result = 0x60;
	if (batteryReadCycleCount(&out->reg2a))
		result = 0x60;
	if (batteryReadInfo1A(&out->reg22))
		result = 0x60;
	if (batteryReadFullCap2(&out->reg12))
		result = 0x60;
#else
	if (batteryControl(0, (unsigned char *)&out->control, 2))
		result = 0x60;
	reg = 0x0a;
	if (batteryRead(&reg, (unsigned char *)&out->reg0a, 2))
		result = 0x60;
	reg = 0x28;
	if (batteryRead(&reg, (unsigned char *)&out->reg28, 2))
		result = 0x60;
	reg = 0x2a;
	if (batteryRead(&reg, (unsigned char *)&out->reg2a, 2))
		result = 0x60;
	reg = 0x22;
	if (batteryRead(&reg, (unsigned char *)&out->reg22, 2))
		result = 0x60;
	reg = 0x12;
	if (batteryRead(&reg, (unsigned char *)&out->reg12, 2))
		result = 0x60;
#endif
	return result;
}

/* USS-1002 reads the Cookie's registers at their own numbers */
#if SERIAL1_OP_USS
#define SERIAL1_COOKIE_REG(reg)	(reg)
#else
#define SERIAL1_COOKIE_REG(reg)	cookieReg(reg)
#endif

/* 0x0954's: 0x0180's, then by parts: 2 the battery's readings read again, 1 0x0181's (at word
   6), 4 the Cookie's status and its registers 4, 6, 7 and 8 (at bytes 0x18-0x1f) */
static unsigned char serial1Read0954(struct Serial1Status *out, unsigned char parts)
{
	unsigned char result = 0;
	unsigned char reg;
	unsigned char value[2];		/* cookieRead's */

	memset(out, 0, sizeof(*out));
	result = serial1Read0180(&out->battery);
	if (result != 0)
		goto done;
	if (parts & 2) {
		out->battery.remainCap = 0;
		out->battery.volt = 0;
		out->battery.temp = 0;
		out->battery.bicTemp = 0;
		if (batteryReadRemainCap(&out->battery.remainCap))
			result = 0x60;
		if (batteryReadVolt(&out->battery.volt))
			result = 0x60;
		if (batteryReadTemp(&out->battery.temp))
			result = 0x60;
		out->battery.temp -= SERIAL1_KELVIN;
		if (batteryReadBicTemp(&out->battery.bicTemp))
			result = 0x60;
		out->battery.bicTemp -= SERIAL1_KELVIN;
		if (result != 0)
			goto done;
	}
	if (parts & 1) {
		result = serial1Read0181(&out->gauge);
		if (result != 0)
			goto done;
	}
	if (parts & 4) {
		if (cookieReadStatus1(&out->cookieStatus1[0]))
			result = 0x60;
		if (cookieReadStatus2(&out->cookieStatus2[0]))
			result = 0x60;
		reg = SERIAL1_COOKIE_REG(4);
		if (cookieRead(&reg, value))
			result = 0x60;
		else
			out->cookieReg4 = value[0];
		reg = SERIAL1_COOKIE_REG(6);
		if (cookieRead(&reg, value))
			result = 0x60;
		else
			out->cookieReg6 = value[0];
		reg = SERIAL1_COOKIE_REG(7);
		if (cookieRead(&reg, value))
			result = 0x60;
		else
			out->cookieReg7 = value[0];
		reg = SERIAL1_COOKIE_REG(8);
		if (cookieRead(&reg, value))
			result = 0x60;
		else
			out->cookieReg8 = value[0];
		if (result != 0)
			goto done;
	}
done:
	return result;
}
#elif !FW_SINCE(0x00080302)
/* 0x00080003 has no operations: its mode 1 commands answer at once */
#else
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
#if BOARD_DOL1001 && !ANALOG_THERMISTOR
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
#endif	/* SERIAL1_OP_0104 */
