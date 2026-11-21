/*
 * What depends on the board (DEM-3000L), as the updater has it: the counts of the Elmo mask
 * steps the power sequences take, and the ports' first state.
 */
#pragma sfr

#include "ernie.h"
#include "board.h"

#if SOURCE_0103
/* the PS TV's (0x01030603): the resume's and the off's four Elmo mask steps (the application's
   powerseq.c), DOL-1001's ports, and the keys' ports left alone */

/* DOL-1001's stand-in for the lines it does not have (the application's board.c's) */
unsigned char boardDummyPin;

unsigned char boardResumeSteps(void)
{
	return 4;
}

unsigned char boardOffSteps(void)
{
	return 4;
}

/* the application's (board.c's) DOL-1001 ports */
void boardPortInit(void)
{
	P0 = 0x00;
	PM0 = 0x80;
	P1 = 0x08;
	PM1 = 0x52;
	P2 = 0x00;
	PM2 = 0x01;
	P3 = 0x00;
	PM3 = 0x0b;
	P4 = 0x00;
	PM4 = 0x40;
	P5 = 0x00;
	PM5 = 0x00;
	P6 = 0x05;
	PM6 = 0x22;
	P7 = 0x00;
	PM7 = 0xb8;
	P8 = 0x00;
	PM8 = 0x00;
	P9 = 0x00;
	PM9 = 0x08;
	P11 = 0x00;
	PM11 = 0xf0;
	P12 = 0x00;
	PM12 = 0x1f;
	P13 = 0x00;
	PM13 = 0xfc;
	P14 = 0x00;
	PM14 = 0xc0;
	P15 = 0x00;
	PM15 = 0x08;
}

void keyPortsEnable(void)
{
}

void keyPortsDisable(void)
{
}
#else
/* the Elmo mask steps of the resume, off and suspend sequences */
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
	PM7 = 0xfd;
	P8 = 0x00;
	PM8 = 0x00;
	P9 = 0x00;
	PM9 = 0x08;
	P11 = 0x00;
	PM11 = 0xf1;
	P12 = 0x00;
	PM12 = 0x5f;
	P13 = 0x00;
	PM13 = 0xfc;
	P14 = 0x00;
	PM14 = 0xec;
	P15 = 0x00;
	PM15 = 0xf8;
}
#endif /* SOURCE_0103 */
