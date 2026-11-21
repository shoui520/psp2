/*
 * USS-1001's multi-connector: a switch of two bits on P5.6 and P5.7 and the serial lines
 * (app/serialline.c) to the host. The outputs boardSetMultiCnOutput takes and
 * boardGetMultiCnOutput gives: 0 and 1 the switch at 0 or 3 with the lines off, 2 the switch at
 * 1 with the lines on, 3 the switch at 1, the lines off and the serial link opened on UART4;
 * boardGetMultiCnOutput gives 0xff for any other state.
 */
#pragma sfr
#if (ERNIE_HW >> 20) == 0x007	/* BOARD_DOL1001 (board.h) */
#ifndef ERNIE_UPDATER
#pragma rtos_interrupt INTP8 intp8Interrupt
#endif
#endif

#include "ernie.h"
#include "analog.h"
#include "board.h"
#include "input.h"
#include "kernel.h"
#include "powerseq.h"
#include "serial.h"
#include "serial1.h"
#include "serialline.h"
#include "state.h"
#include "task5.h"

extern unsigned char input1Bit5Active(void);


#if BOARD_USS1001
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
#if SOURCE_0104	/* USS-1002's returns after it (call, not br) */
	return;
#endif
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
#if SOURCE_0104	/* USS-1002's mode has no flag bit */
		if (boardGetMultiCnOutput() != 3 || serialGetMode() != 1) {
#elif FW_SINCE(0x01030511)
		if (boardGetMultiCnOutput() != 3 || (serialGetMode() & SERIAL_MODE) != 1) {
#else
		if (boardGetMultiCnOutput() != 3) {	/* 0x01030505: the serial mode not checked */
#endif
			serialClose();
			serial1Reset();
			boardMultiCnSetSwitch(1);
			boardMultiCnSetSerial(0);
			serialOpen1(2);
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
#else
/* DOL-1001: the fat board's switch (board.c) for the slim's outputs, the serial links as the fat
   models' task 2 sets them */
void boardMultiCnUpdate(void)
{
	unsigned char state;

	state = serialGetMode();
	if (input1Bit5Active()) {
		if (boardMultiCnIsDongle()) {
			boardMultiCnSetSwitch(0);
			switch (state) {
			case 0:
			case 3:
				serialClose3();
				serial1Reset();
				serialOpen1(2);
				break;
			case 1:
				break;
			}
		} else if (boardMultiCnIsDebug()) {
			boardMultiCnSetSwitch(0);
			switch (state) {
			case 0:
			case 1:
				serialClose1();
				serialOpen3(3);
				break;
			case 3:
				break;
			}
		} else {
			serialClose3();
			serialClose1();
			boardMultiCnSwitchStop();
#if UPD_0103	/* the updater's: the multi-connector's power too */
			boardMultiCnPower(1);
#endif
		}
	} else {
		serialClose3();
		serialClose1();
		boardMultiCnSwitchStop();
#if UPD_0103
		boardMultiCnPower(1);
#endif
	}
}

unsigned char boardGetMultiCnOutput(void)
{
	return boardMultiCnGetSwitch();
}

/* 3 the host's side and the power on in the resume, 2 the dongle's and the power off, any other
   the switch off */
unsigned char boardSetMultiCnOutput(unsigned char output)
{
	unsigned char result;

	result = 0;
	switch (output) {
	case 3:
		result = boardMultiCnSetSwitch(0);
		if (stateGet() == STATE_RESUME)
			boardMultiCnPower(1);
		break;
	case 2:
		result = boardMultiCnSetSwitch(1);
		if (stateGet() == STATE_RESUME)
			boardMultiCnPower(0);
		break;
	case 0xff:
	default:
		result = boardMultiCnSetSwitch(4);
		break;
	}
	return result;
}

unsigned char boardMultiCnOutputOff(void)
{
	unsigned char result;

	result = 0;
	serialClose();
	serial1Reset();
	boardMultiCnSwitchOff();
	return result;
}

unsigned char boardMultiCnStop(void)
{
	return boardSetMultiCnOutput(0);
}
#endif

unsigned char boardMultiCnIsDongle(void)
{
	return analogGetMultiCn() == BOARD_MULTICN_DONGLE_ID;
}

unsigned char boardMultiCnIsDebug(void)
{
	return 0;
}

#if BOARD_MULTICN_LEVELS
/* the slim's 0x0105: the multi-connector's level (analogGetMultiCn) tested here */
unsigned char boardMultiCnIsLevel2(void)
{
	return analogGetMultiCn() == 2;
}

unsigned char boardMultiCnIsNone(void)
{
	return analogGetMultiCn() == 0xff;
}

unsigned char boardMultiCnIsLevel0(void)
{
	return analogGetMultiCn() < 1;
}

/* not read yet */
unsigned char boardMultiCnIsUnread(void)
{
	return analogGetMultiCn() == 0xfe;
}
#endif

/* power device job 0x20 (on) or 0x10 (off), then 25 or 10 ticks */
unsigned char boardMultiCnPower(unsigned char on)
{
#if UPD_0103
	/* the updater's: the sequences themselves, as Sony's has them (0: powerMultiCnOn) */
	if (on == 0)
		powerMultiCnOn();
	else
		powerMultiCnOff();
#else
	if (on == 0) {
		powerDeviceSetPending(0x10);
		dly_tsk(10);
	} else {
		powerDeviceSetPending(0x20);
		dly_tsk(25);
	}
#endif
	return 0;
}

#if BOARD_DOL1001 && !UPD_0103
/* DOL-1001's INTP8 (P7.4's edge): task 5's sleep factor 21 (flag 4, 0x0200) */
void intp8Interrupt(void)
{
	task5SetFlag(4, 0x0200);
}
#endif
