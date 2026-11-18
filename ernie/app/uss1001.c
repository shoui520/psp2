/*
 * USS-1001's ports: their levels, pull-ups, input and output modes and digital/analog modes at
 * the start (hdwinit, through boardPortInit), and the keys' pins (P5.0-1, P7.1-2, P8) made
 * inputs with their pull-ups and back.
 */
#pragma sfr

#include "ernie.h"
#include "board.h"

void boardPortInitUss1001(void);

/* the ports at the start (hdwinit) */
void boardPortInit(void)
{
	boardPortInitUss1001();
}

void boardPortInitUss1001(void)
{
	P0 = 0x00;
	PU0 = 0x00;
	PIM0 = 0x00;
	POM0 = 0x00;
	PMC0 = 0xf3;
	PM0 = 0x00;
	P1 = 0x08;
	PU1 = 0x00;
	PIM1 = 0x00;
	POM1 = 0x00;
	PM1 = 0x52;
	ADPC = 0x08;
	P2 = 0x00;
	PM2 = 0x7f;
	P3 = 0x10;
	PU3 = 0x00;
	PM3 = 0x0b;
	P4 = 0x00;
	PU4 = 0x00;
	PIM4 = 0x00;
	POM4 = 0x00;
	PM4 = 0x10;
	P5 = 0x00;
	PU5 = 0x00;
	PIM5 = 0x00;
	POM5 = 0x00;
	PM5 = 0x00;
	P6 = 0x05;
	PU6 = 0x00;
	PM6 = 0xca;
	P7 = 0x00;
	PU7 = 0x00;
	POM7 = 0x00;
	PM7 = 0x79;
	P8 = 0x00;
	PU8 = 0x00;
	PIM8 = 0x00;
	POM8 = 0x00;
	PM8 = 0x00;
	P9 = 0x04;
	PU9 = 0x00;
	POM9 = 0x00;
	PM9 = 0x08;
	P10 = 0x04;
	PU10 = 0x00;
	PMC10 = 0xfe;
	PM10 = 0xf8;
	P11 = 0x00;
	PU11 = 0x00;
	PM11 = 0xe0;
	P12 = 0x00;
	PU12 = 0x00;
	PMC12 = 0xfe;
	PM12 = 0x3f;
	P13 = 0x00;
	P14 = 0x00;
	PU14 = 0x00;
	PIM14 = 0x00;
	POM14 = 0x00;
	PMC14 = 0x7f;
	PM14 = 0x00;
	P15 = 0x00;
	PM15 = 0x80;
}

/* P5.0-1, P7.1-2 and P8 inputs with their pull-ups (the keys button1Keys reads) */
void inputKeysOpen(void)
{
	PU5 = PU5 | 0x03;
	PM5 = PM5 | 0x03;
	PU7 = PU7 | 0x06;
	PM7 = PM7 | 0x06;
	PU8 = 0xff;
	PM8 = 0xff;
}

/* and back to outputs at 0 */
void inputKeysClose(void)
{
	P5 = P5 & 0xfc;
	PM5 = PM5 & 0xfc;
	P7 = P7 & 0xf9;
	PM7 = PM7 & 0xf9;
	P8 = 0x00;
	PM8 = 0x00;
}
