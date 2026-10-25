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

/* a battery temperature in 0.1 K less this is in 0.1 degrees C */
#define SERIAL1_KELVIN		0x0aac

void serial1Operation(void)
{
	unsigned char result;
	unsigned char reg;

	switch (serial1State) {
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
		/* no break in Sony's: operation 3 runs too */
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
		/* no break in Sony's: operation 5 runs too */
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
