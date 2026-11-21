/*
 * 0x0100060D's multi connector read: its level on ANI11 (analogMultiCnLevels' limits), read
 * until two readings in a row agree on a new level, at most ten times 8 ms apart.
 */
#pragma sfr

#include "ernie.h"
#include "analog.h"
#if SOURCE_0103
#include "adc.h"
#endif
#include "sleepfactor.h"
#include "updater.h"

/* the level of the first of count levels whose limit the input reaches; 0xff if none */
unsigned char analogLevelFind(unsigned char input, const struct AnalogLevel *levels,
			      unsigned char count)
{
	unsigned char i;

	for (i = 0; i < count; i++) {
		if (levels->limit <= input)
			return levels->level;
		levels++;
	}
	return 0xff;
}

/* the multi connector's level, 0xff if no two readings agreed */
unsigned char analogReadMultiCn(void)
{
	unsigned char input;
	unsigned char last;
	unsigned char found;
	unsigned char level;
	unsigned char i;
	struct AnalogLevel levels[7] = {
		{ 0xe3, 6 }, { 0xc4, 5 }, { 0x99, 4 }, { 0x69, 3 }, { 0x40, 2 }, { 0x1d, 1 },
		{ 0x00, 0 },
	};

	sleepFactorSet6(0);
	level = 0xff;
	found = 0xff;
	last = 0xff;
	input = 0;
#if SOURCE_0103	/* the PS TV's: the application's adc.c, polled */
	adcOpen();
#else
	ADCEN = 1;
	ADCE = 1;
	ADPC = 0x0b;
	ADM = 0x03;
	ADMK = 1;
#endif
	for (i = 0; i < 10; i++) {
#if SOURCE_0103
		input = adcRead(0x0b) >> 8;
#else
		ADIF = 0;
		ADS = 0x0b;
		ADCS = 1;
		while (!ADIF)
			;
		input = ADCRH;
#endif
		found = analogLevelFind(input, levels, 7);
		if (found == last && found != level) {
			level = found;
			sleepFactorSet6(1);
			sleepFactorSet5(1);
			break;
		}
		last = found;
		powerWaitLong(8);
	}
#if SOURCE_0103
	adcClose();
#else
	ADCS = 0;
	ADCE = 0;
	ADIF = 0;
	ADCEN = 0;
#endif
	return level;
}
