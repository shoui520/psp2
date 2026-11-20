/*
 * USS-1001's ports: their levels, pull-ups, input and output modes and digital/analog modes at
 * the start (hdwinit, through boardPortInit), and the keys' pins (P5.0-1, P7.1-2, P8) made
 * inputs with their pull-ups and back; INTP9, the USB supply's line, wakes task 5
 * (chargeUsbChanged).
 */
#pragma rtos_interrupt INTP9 intp9Interrupt
#pragma sfr

#include "ernie.h"
#include "board.h"
#include "task.h"

void boardPortInitUss1001(void);

/* the USB supply's line changed: task 5's event 0x0400 */
void intp9Interrupt(void)
{
	task5ISetFlag(0x0400);
}

/* the ports at the start (hdwinit) */
void boardPortInit(void)
{
	boardPortInitUss1001();
}

/* USS-1002's: the analog inputs up to ANI10 (the gauge's), P2.7 and P15.0-1 inputs, P3.3 an
   output */
#if SOURCE_0104
#define PORT_ADPC		0x0b
#define PORT_PM2		0xff
#define PORT_PM3		0x03
#define PORT_PM15		0x83
#else
#define PORT_ADPC		0x08
#define PORT_PM2		0x7f
#define PORT_PM3		0x0b
#define PORT_PM15		0x80
#endif

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
	ADPC = PORT_ADPC;
	P2 = 0x00;
	PM2 = PORT_PM2;
	P3 = 0x10;
	PU3 = 0x00;
	PM3 = PORT_PM3;
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
	PM15 = PORT_PM15;
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
