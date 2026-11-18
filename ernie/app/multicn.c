/*
 * USS-1001's multi-connector: a switch of two bits on P5.6 and P5.7 and the serial lines
 * (app/serialline.c) to the host. The outputs boardSetMultiCnOutput takes and
 * boardGetMultiCnOutput gives: 0 and 1 the switch at 0 or 3 with the lines off, 2 the switch at
 * 1 with the lines on, 3 the switch at 1, the lines off and the serial link opened on UART4;
 * boardGetMultiCnOutput gives 0xff for any other state.
 */
#pragma sfr

#include "ernie.h"
#include "analog.h"
#include "board.h"
#include "kernel.h"
#include "serial.h"
#include "serial1.h"
#include "serialline.h"
#include "state.h"
#include "task5.h"

extern unsigned char serialOpen4(int baud);

unsigned char boardMultiCnSerialInit(void)
{
	serialLineInit();
	serialLineSet(SERIAL_LINES_HOST, 0);
	return 0;
}

/* 0 the host's lines off, 1 on, 4 one of them only */
unsigned char boardMultiCnSerialState(void)
{
	switch (serialLineGet() & SERIAL_LINES_HOST) {
	case SERIAL_LINES_HOST:
		return 1;
	case 0:
		return 0;
	default:
		return 4;
	}
}

/* the host's lines on (1) or off (0, 2-4); 1 for any other */
unsigned char boardMultiCnSetSerial(unsigned char on)
{
	if (on >= 5)
		return 1;
	switch (on) {
	case 0:
		serialLineSet(SERIAL_LINES_HOST, 0);
		break;
	case 1:
		serialLineSet(SERIAL_LINES_HOST, 1);
		break;
	default:
		serialLineSet(SERIAL_LINES_HOST, 0);
		break;
	}
	return 0;
}

unsigned char boardMultiCnSerialOff(void)
{
	serialLineSet(SERIAL_LINES_HOST, 0);
	return 0;
}

unsigned char boardMultiCnSwitchOff(void)
{
	P5.6 = 0;
	P5.7 = 0;
	return 0;
}

/* P5.6 bit 0, P5.7 bit 1 */
unsigned char boardMultiCnGetSwitch(void)
{
	return (P5 & 0xc0) >> 6;
}

/* bit 0 to P5.6, bit 1 to P5.7; 1 for a value above 4 */
unsigned char boardMultiCnSetSwitch(unsigned char value)
{
	if (value >= 5)
		return 1;
	P5.6 = (value & 1) ? 1 : 0;
	P5.7 = (value & 2) ? 1 : 0;
	return 0;
}

/* the output for what analogGetMultiCn finds on the connector, then power device job 0x40 */
void boardMultiCnUpdate(void)
{
	switch (analogGetMultiCn()) {
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		boardSetMultiCnOutput(3);
		break;
	case 5:
	case 8:
	case 0:
	default:
		serialClose3();
		serialClose1();
		boardSetMultiCnOutput(0);
		if (stateGet() == STATE_RESUME)
			boardMultiCnPower(1);
		break;
	}
	powerDeviceSetPending(0x40);
}

unsigned char boardGetMultiCnOutput(void)
{
	switch (boardMultiCnGetSwitch()) {
	case 0:
		return 0;
	case 3:
		return 1;
	case 1:
		switch (boardMultiCnSerialState()) {
		case 1:
			return 2;
		case 0:
			return 3;
		default:
			return 0xff;
		}
	default:
		return 0xff;
	}
}

/* 1 for an output above 3 */
unsigned char boardSetMultiCnOutput(unsigned char output)
{
	if (output >= 4)
		return 1;
	switch (output) {
	case 0:
		boardMultiCnSerialOff();
		boardMultiCnSetSwitch(0);
		break;
	case 1:
		boardMultiCnSerialOff();
		boardMultiCnSetSwitch(3);
		break;
	case 2:
		boardMultiCnSetSwitch(1);
		boardMultiCnSetSerial(1);
		boardMultiCnPower(0);
		break;
	case 3:
		if (boardGetMultiCnOutput() != 3 || (serialGetMode() & SERIAL_MODE) != 1) {
			serialClose();
			serial1Reset();
			boardMultiCnSetSwitch(1);
			boardMultiCnSetSerial(0);
			serialOpen4(2);
		}
		boardMultiCnPower(1);
		break;
	case 0xff:
		break;
	}
	return 0;
}

unsigned char boardMultiCnOutputOff(void)
{
	unsigned char result;

	result = 0;
	serialClose();
	serial1Reset();
	boardMultiCnSerialInit();
	boardMultiCnSwitchOff();
	return result;
}

unsigned char boardMultiCnStop(void)
{
	return boardSetMultiCnOutput(0);
}

unsigned char boardMultiCnIsDongle(void)
{
	return analogGetMultiCn() == 4;
}

unsigned char boardMultiCnIsDebug(void)
{
	return 0;
}

/* power device job 0x20 (on) or 0x10 (off), then 25 or 10 ticks */
unsigned char boardMultiCnPower(unsigned char on)
{
	if (on == 0) {
		powerDeviceSetPending(0x10);
		dly_tsk(10);
	} else {
		powerDeviceSetPending(0x20);
		dly_tsk(25);
	}
	return 0;
}
