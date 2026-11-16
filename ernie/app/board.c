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
	return BOARD_POWER_STEPS;
}

unsigned char boardOffSteps(void)
{
	return BOARD_POWER_STEPS;
}

unsigned char boardSuspendSteps(void)
{
	return BOARD_POWER_STEPS;
}
#endif

#if BOARD_MULTICN_BITS
/* the switch off: both low */
unsigned char boardMultiCnOutputOff(void)
{
	P13.0 = 0;
	P2.2 = 0;
	return 0;
}

/* P13.0 bit 0, P2.2 bit 1 */
unsigned char boardGetMultiCnOutput(void)
{
	return (P13.0 ? 1 : 0) | (P2.2 ? 2 : 0);
}

/* bit 0 to P13.0, bit 1 to P2.2; 1 for an output above 4 */
unsigned char boardSetMultiCnOutput(unsigned char output)
{
	if (output >= 5)
		return 1;
	P13.0 = (output & 1) ? 1 : 0;
	P2.2 = (output & 2) ? 1 : 0;
	return 0;
}

unsigned char boardMultiCnStop(void)
{
	P13.0 = 0;
	P2.2 = 0;
	return 0;
}
#elif !FW_SINCE(0x00090804)
/* 0x00090206: the switch is P13.0 alone, the dongle on (1) or off (0) */
unsigned char boardMultiCnOutputOff(void)
{
	P13.0 = 0;
	return 0;
}

unsigned char boardGetMultiCnOutput(void)
{
	return P13.0 ? 1 : 0;
}

/* 1 for an output above 1 */
unsigned char boardSetMultiCnOutput(unsigned char output)
{
	if (output >= 2)
		return 1;
	P13.0 = (output & 1) ? 1 : 0;
	return 0;
}

unsigned char boardMultiCnStop(void)
{
	P13.0 = 0;
	return 0;
}
#else
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

#endif

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
	return analogGetMultiCn() == BOARD_MULTICN_DONGLE_ID;
}
#endif

#if FW_SINCE(0x00090804)	/* 0x00090206 has no debug board */
/* a debug board on the multi-connector (UART3 in its debug mode): never on this board */
unsigned char boardMultiCnIsDebug(void)
{
	return 0;
}
#endif

#ifndef ERNIE_UPDATER
#if !FW_SINCE(0x00090804)
/* 0x00090206's hdwinit sets the ports through this */
void boardInit(void)
{
	boardPortInit();
}

#endif
/* the ports' levels and modes at the start (hdwinit) */
void boardPortInit(void)
{
	P0 = 0x00;
	PM0 = 0x88;
	P1 = 0x08;
	PM1 = 0x52;
	P2 = 0x00;
	PM2 = BOARD_PM2;
	P3 = 0x00;
	PM3 = 0x07;
	P4 = 0x00;
	PM4 = BOARD_PM4;
	P5 = 0x00;
	PM5 = 0x00;
	P6 = 0x05;
	PM6 = BOARD_PM6;
	P7 = 0x00;
	PM7 = BOARD_PM7;
	P8 = 0x00;
	PM8 = 0x00;
	P9 = 0x00;
	PM9 = BOARD_PM9;
	P11 = 0x00;
	PM11 = BOARD_PM11;
	P12 = BOARD_P12;
	PM12 = 0x5f;
	P13 = 0x00;
	PM13 = 0xfc;
#if FW_SINCE(0x00090804)
	P14 = 0x00;
#else
	/* 0x00090206: P14.3 an open-drain output, high */
	POM14 = 0x08;
	P14 = 0x08;
#endif
	PM14 = BOARD_PM14;
	P15 = 0x00;
	PM15 = BOARD_PM15;
}
#endif
