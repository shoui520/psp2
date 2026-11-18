/*
 * USS-1001's data transfer controller. Its vector table (the control data's place for each
 * interrupt source) and the control data sit at 0xfbb00 and 0xfbb40, DTCBAR naming the table's
 * page. One channel is set up: control data 0 and 1, started by source 0x0c.
 */
#pragma sfr

#include <string.h>
#include "ernie.h"
#include "dtc.h"
#include "kernel.h"

const struct DtcChannel dtcChannels[] = {
	{ 0, 2, 0x0c, 0 },
};

#define DTC_CHANNELS	(sizeof(dtcChannels) / sizeof(dtcChannels[0]))

unsigned char dtcVectors[DTC_VECTORS];
struct DtcControl dtcControl[DTC_CONTROLS];

/* turns the controller on, every source off, and points it at the vector table */
void dtcOpen(void)
{
	if (!DTCEN) {
		DTCEN = 1;
		DTCEN0 = 0x00;
		DTCEN1 = 0x00;
		DTCEN2 = 0x00;
		DTCEN3 = 0x00;
		DTCEN4 = 0x00;
		if (DTCBAR != (unsigned char)(((unsigned int)dtcVectors >> 8) & 0xff))
			DTCBAR = (unsigned char)(((unsigned int)dtcVectors >> 8) & 0xff);
	}
}

/* copies a channel's control data in and lets its source start transfers: 0, or 1 and 2 */
unsigned char dtcStart(unsigned char channel, struct DtcControl *control)
{
	unsigned char i;
	unsigned char vector;
	unsigned char *enable;

	for (i = 0; i < DTC_CHANNELS; i++) {
		if (dtcChannels[i].control == channel) {
			if (dtcChannels[i].vector >= 0x27)
				return 1;
			if (dtcChannels[i].control + dtcChannels[i].blocks >= 3)
				return 2;
			memcpy(&dtcControl[dtcChannels[i].control], control,
			       dtcChannels[i].blocks * sizeof(struct DtcControl));
			vector = dtcChannels[i].vector;
			dtcVectors[vector] = (unsigned char)(dtcChannels[i].control * sizeof(struct DtcControl) +
							 sizeof(dtcVectors));
			enable = (unsigned char *)&DTCEN0 + (vector >> 3);
			dis_dsp();
			*enable = *enable | (unsigned char)(0x80 >> (vector & 7));
			ena_dsp();
			return 0;
		}
	}
	return 2;
}

/* stops a channel's source starting transfers: 0, or 1 and 2 */
unsigned char dtcStop(unsigned char channel)
{
	unsigned char i;
	unsigned char vector;
	unsigned char *enable;

	for (i = 0; i < DTC_CHANNELS; i++) {
		if (dtcChannels[i].control == channel) {
			if (dtcChannels[i].vector >= 0x27)
				return 1;
			if (dtcChannels[i].control + dtcChannels[i].blocks >= 2)
				return 2;
			vector = dtcChannels[i].vector;
			enable = (unsigned char *)&DTCEN0 + (vector >> 3);
			dis_dsp();
			*enable = *enable & ~(unsigned char)(0x80 >> (vector & 7));
			ena_dsp();
			return 0;
		}
	}
	return 2;
}

/* every source off, and the controller */
void dtcClose(void)
{
	DTCEN0 = 0x00;
	DTCEN1 = 0x00;
	DTCEN2 = 0x00;
	DTCEN3 = 0x00;
	DTCEN4 = 0x00;
	DTCEN = 0;
}
