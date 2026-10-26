/*
 * What depends on the board (DEM-3000L): the counts of the Elmo mask steps the power sequences
 * take, the multi-connector's output switch and the ports' first state.
 */
#pragma sfr

#include "ernie.h"
#include "analog.h"
#include "board.h"
#ifdef ERNIE_UPDATER
#include "../updater/updater.h"
#endif

/* the updater has these in its own board.c */
#ifndef ERNIE_UPDATER
/* nothing on this board */
void boardNone(int value)
{
}

/* the Elmo mask steps of the resume (powerJob's), off and suspend sequences */
unsigned char boardResumeSteps(void)
{
	return 3;
}

unsigned char boardOffSteps(void)
{
	return 3;
}

unsigned char boardSuspendSteps(void)
{
	return 3;
}
#endif

/* the switch off at the start: P2.2 low, then P13.0 */
unsigned char boardMultiCnOutputOff(void)
{
	unsigned char wait;

#if BOARD_HOST_OUTPUT
	P2.2 = 0;
#endif
	wait = 2;
	while (wait--)
		;
	P13.0 = 0;
	return 0;
}

unsigned char boardGetMultiCnOutput(void)
{
	if (P13.0)
		return BOARD_MULTICN_DONGLE;
#if BOARD_HOST_OUTPUT
	if (P2.2)
		return BOARD_MULTICN_HOST;
#endif
	return BOARD_MULTICN_OFF;
}

/* 0 the host, 1 the dongle, 2-4 off; 1 for any other */
unsigned char boardSetMultiCnOutput(unsigned char output)
{
	unsigned char wait;

	if (output >= 5)
		return 1;
	switch (output) {
	case BOARD_MULTICN_HOST:
		P13.0 = 0;
		wait = 2;
		while (wait--)
			;
#if BOARD_HOST_OUTPUT
		P2.2 = 1;
#endif
		break;
	case BOARD_MULTICN_DONGLE:
		P13.0 = 1;
		wait = 2;
		while (wait--)
			;
#if BOARD_HOST_OUTPUT
		P2.2 = 1;
#endif
		break;
	default:
#if BOARD_HOST_OUTPUT
		P2.2 = 0;
#endif
		wait = 2;
		while (wait--)
			;
		P13.0 = 0;
		break;
	}
	return 0;
}

unsigned char boardMultiCnStop(void)
{
	unsigned char wait;

#if BOARD_HOST_OUTPUT
	P2.2 = 0;
#endif
	wait = 2;
	while (wait--)
		;
	P13.0 = 0;
	return 0;
}

#ifdef ERNIE_UPDATER
/* the updater's: something on the multi-connector, as its inputs report it */
unsigned char boardMultiCnIsDongle(void)
{
	return (unsigned char)input1Bit5Active();
}
#else
/* a dongle on the multi-connector (its ID 2) */
unsigned char boardMultiCnIsDongle(void)
{
	return analogGetMultiCn() == 2;
}
#endif

/* a debug board on the multi-connector (UART3 in its debug mode): never on this board */
unsigned char boardMultiCnIsDebug(void)
{
	return 0;
}

#ifndef ERNIE_UPDATER
/* the ports' levels and modes at the start (hdwinit) */
void boardPortInit(void)
{
	P0 = 0x00;
	PM0 = 0x88;
	P1 = 0x08;
	PM1 = 0x52;
	P2 = 0x00;
	PM2 = 0x01;
	P3 = 0x00;
	PM3 = 0x07;
	P4 = 0x00;
	PM4 = 0x40;
	P5 = 0x00;
	PM5 = 0x00;
	P6 = 0x05;
	PM6 = 0x92;
	P7 = 0x00;
	PM7 = BOARD_PM7;
	P8 = 0x00;
	PM8 = 0x00;
	P9 = 0x00;
	PM9 = BOARD_PM9;
	P11 = 0x00;
	PM11 = 0xf1;
	P12 = 0x00;
	PM12 = 0x5f;
	P13 = 0x00;
	PM13 = 0xfc;
	P14 = 0x00;
	PM14 = 0xe4;
	P15 = 0x00;
	PM15 = 0xf8;
}
#endif
