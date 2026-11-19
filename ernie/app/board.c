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
#elif BOARD_102603 && !FW_SINCE(0x00090804)
extern unsigned char input1Bit5Active(void);
#endif

#if BOARD_DOL1001 && !defined(ERNIE_UPDATER)
unsigned char boardDummyPin;
#endif

/* the updater has these in its own board.c */
#ifndef ERNIE_UPDATER
#if !SOURCE_0103	/* not in the slim's */
/* nothing on this board */
void boardNone(int value)
{
}
#endif

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

#if BOARD_DOL1001
/* DOL-1001: the fat board's switch, under multicn.c's outputs (which take the names) */
#define boardMultiCnOutputOff	boardMultiCnSwitchOff
#define boardGetMultiCnOutput	boardMultiCnGetSwitch
#define boardSetMultiCnOutput	boardMultiCnSetSwitch
#define boardMultiCnStop	boardMultiCnSwitchStop
#endif

#if BOARD_USS1001
/* the slim's multi-connector is in multicn.c */
#elif BOARD_MULTICN_BITS
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

/* bit 0 to P13.0, bit 1 to P2.2; 1 for an output above 4 (above 3 in 0x00090206) */
unsigned char boardSetMultiCnOutput(unsigned char output)
{
#if FW_SINCE(0x00090804)
	if (output >= 5)
#else
	if (output >= 4)
#endif
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
#if BOARD_DOL1001
#undef boardMultiCnOutputOff
#undef boardGetMultiCnOutput
#undef boardSetMultiCnOutput
#undef boardMultiCnStop
#endif

#if SOURCE_0103
/* the slim's in multicn.c */
#elif defined(ERNIE_UPDATER) || (HW_IS(0x00102603) && !FW_SINCE(0x00090804))
/* the updater's (and 0x00102603's 0x00090206): something on the multi-connector, as its
   inputs report it */
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

#if FW_SINCE(0x00090804) && !SOURCE_0103	/* 0x00090206 has no debug board */
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
#if HW_IS(0x00102603) && !FW_SINCE(0x00090206)
	boardPortInit102603();	/* 0x00090009 */
#else
	boardPortInit();
#endif
}

#endif
#if !FW_SINCE(0x00090206)
/* 0x00090009 keeps the ports of three other boards, which nothing calls */
/* an early board's ports, with the A/D pins (ADPC 9) */
void boardPortInitA(void)
{
	P0 = 0x00;
	PM0 = 0x08;
	P1 = 0x08;
	PM1 = 0xd2;
	ADPC = 0x09;
	P2 = 0x00;
	PM2 = 0x0f;
	P3 = 0x00;
	PM3 = 0x07;
	P4 = 0x00;
	PM4 = 0xd0;
	P5 = 0x00;
	PM5 = 0xff;
	P6 = 0x05;
	PM6 = 0xb2;
	P7 = 0x00;
	PM7 = 0xfc;
	P8 = 0x00;
	PM8 = 0xff;
	P9 = 0x00;
	PM9 = 0x20;
	P11 = 0x00;
	PM11 = 0x16;
	P12 = 0x00;
	PM12 = 0x7f;
	P13 = 0x00;
	PM13 = 0xfc;
	POM14 = 0x08;
	P14 = 0x08;
	PM14 = 0xc6;
	P15 = 0x00;
	PM15 = 0xfe;
}

/* a board like 0x00102603's, with P11.1 high and PM5 0x3f */
void boardPortInitB(void)
{
	P0 = 0x00;
	PM0 = 0x88;
	P1 = 0x08;
	PM1 = 0x52;
	P2 = 0x00;
	PM2 = 0xab;
	P3 = 0x00;
	PM3 = 0x07;
	P4 = 0x00;
	PM4 = 0xd0;
	P5 = 0x00;
	PM5 = 0x3f;
	P6 = 0x05;
	PM6 = 0x12;
	P7 = 0x00;
	PM7 = 0xfd;
	P8 = 0x00;
	PM8 = 0xff;
	P9 = 0x00;
	PM9 = 0x08;
	P11 = 0x02;
	PM11 = 0xfd;
	P12 = 0x80;
	PM12 = 0x5f;
	P13 = 0x00;
	PM13 = 0xfc;
	POM14 = 0x08;
	P14 = 0x08;
	PM14 = 0xe6;
	P15 = 0x00;
	PM15 = 0xff;
}

/* 0x00102603's ports */
void boardPortInit102603(void)
{
	P0 = 0x00;
	PM0 = 0x88;
	P1 = 0x08;
	PM1 = 0x52;
	P2 = 0x00;
	PM2 = 0xab;
	P3 = 0x00;
	PM3 = 0x07;
	P4 = 0x00;
	PM4 = 0xd0;
	P5 = 0x00;
	PM5 = 0x00;
	P6 = 0x05;
	PM6 = 0x12;
	P7 = 0x00;
	PM7 = 0xfd;
	P8 = 0x00;
	PM8 = 0x00;
	P9 = 0x00;
	PM9 = 0x08;
	P11 = 0x00;
	PM11 = 0xf2;
	P12 = 0x80;
	PM12 = 0x5f;
	P13 = 0x00;
	PM13 = 0xfc;
	POM14 = 0x08;
	P14 = 0x08;
	PM14 = 0xe6;
	P15 = 0x00;
	PM15 = 0xff;
}

#endif
#if !BOARD_USS1001	/* the slim's is in uss1001.c */
/* the ports' levels and modes at the start (hdwinit) */
#if BOARD_DOL1002
/* DOL-1002's port modes */
void boardPortInit(void)
{
	P0 = 0x00;
	PM0 = 0x80;
	P1 = 0x08;
	PM1 = 0x52;
	P2 = 0x00;
	PM2 = 0xff;
	P3 = 0x00;
	PM3 = 0x0b;
	P4 = 0x00;
	PM4 = 0x40;
	P5 = 0x00;
	PM5 = 0x00;
	P6 = 0x05;
	PM6 = 0x22;
	P7 = 0x00;
	PM7 = 0xb0;
	P8 = 0x00;
	PM8 = 0x00;
	P9 = 0x00;
	PM9 = 0x00;
	P11 = 0x00;
	PM11 = 0xf0;
	P12 = 0x00;
	PM12 = 0x1f;
	P13 = 0x00;
	PM13 = 0xfc;
	P14 = 0x00;
	PM14 = 0xc0;
	P15 = 0x00;
	PM15 = 0x0c;
}
#else
void boardPortInit(void)
{
	P0 = 0x00;
	PM0 = 0x88;
	P1 = 0x08;
	PM1 = 0x52;
	P2 = 0x00;
	PM2 = BOARD_PM2;
	P3 = 0x00;
	PM3 = BOARD_PM3;
	P4 = 0x00;
	PM4 = BOARD_PM4;
	P5 = 0x00;
	PM5 = BOARD_PM5;
	P6 = 0x05;
	PM6 = BOARD_PM6;
	P7 = 0x00;
	PM7 = BOARD_PM7;
	P8 = 0x00;
	PM8 = 0x00;
	P9 = 0x00;
	PM9 = BOARD_PM9;
	P11 = BOARD_P11;
	PM11 = BOARD_PM11;
	P12 = BOARD_P12;
	PM12 = BOARD_PM12;
	P13 = 0x00;
	PM13 = 0xfc;
#if FW_SINCE(0x00090804) || !BOARD_IS(0x00411000) || BOARD_PORTS_315200
	P14 = 0x00;
#else
	/* 0x00411000's 0x00090206 (not 0x00315200's): P14.3 an open-drain output, high */
	POM14 = 0x08;
	P14 = 0x08;
#endif
	PM14 = BOARD_PM14;
	P15 = 0x00;
	PM15 = BOARD_PM15;
}
#endif
#if BOARD_DOL1001
/* DOL-1001's keys are not read (input.c's on the others) */
void inputKeysOpen(void)
{
}

void inputKeysClose(void)
{
}
#endif
#endif	/* !BOARD_USS1001 */
#endif
