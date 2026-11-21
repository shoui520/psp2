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

#if BOARD_USS1001
/*
 * The slim's (USS-1001): analog input 0x0010's level by its whole result, as the application's
 * (app/analog.c), read once from each main loop's multi connector check.
 */
#include "adc.h"

struct AnalogLevelWide {
	unsigned int	limit;
	unsigned int	level;
};

/* its result, its level, the one before and the one reported (0xfe: none yet) */
static struct {
	unsigned int	input;
	unsigned char	level;		/* of the last reading */
	unsigned char	previous;	/* of the one before */
	unsigned char	reported;
} analogInput10;

/* a reset starts the levels afresh; the application's are kept otherwise */
void analogInput10Init(void)
{
	if (updReset) {
		analogInput10.reported = 0xfe;
		analogInput10.level = 0xfe;
		analogInput10.previous = 0xfe;
	}
	analogInput10.input = 0;
}

/* the level of the first of count levels whose limit the input reaches; 0xff if none */
static unsigned int analogLevelFindWide(unsigned int input, const struct AnalogLevelWide *levels,
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

/* the level reported: a new one once two readings in a row agree on it, then sleep factor 14
   and event 0x7a */
unsigned char analogReadMultiCn(void)
{
	struct AnalogLevelWide levels[12] = {
		{ 0xe900, 0 }, { 0xde40, 1 }, { 0xce00, 0xff }, { 0xc440, 2 }, { 0xb640, 3 },
		{ 0xa1c0, 0xff }, { 0x9140, 4 }, { 0x8840, 0xff }, { 0x7a80, 5 }, { 0x0580, 0xff },
		{ 0x0140, 6 }, { 0x0000, 7 },
	};

	adcOpen();
	analogInput10.input = adcRead(6);
	analogInput10.level = analogLevelFindWide(analogInput10.input, levels, 12);
	if (analogInput10.level == analogInput10.previous &&
	    analogInput10.level != analogInput10.reported) {
		analogInput10.reported = analogInput10.level;
		if (analogInput10.reported == 0)
			sleepFactorSet14(0);
		else
			sleepFactorSet14(1);
		updEventSet(0x7a);
	}
	analogInput10.previous = analogInput10.level;
	adcClose();
	return analogInput10.reported;
}
#else
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
#endif
