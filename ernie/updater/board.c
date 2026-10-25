/*
 * What depends on the board (DEM-3000L), as the updater has it: the counts of the Elmo mask
 * steps the power sequences take, and the ports' first state.
 */
#pragma sfr

#include "ernie.h"
#include "board.h"

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
