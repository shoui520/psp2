/*
 * The radio's synthesizer ratio for a channel: the integer and fractional
 * dividers of a channel's frequency over the reference clock, written to the
 * radio. Marvell published no source for it; the names say what the code does.
 */
#include <stdint.h>
#include "robin/rf.h"
#include "robin/rfbus.h"

#define RF_REG_SYNTH_INTEGER	0x029	/* the low byte of the integer divider */
#define RF_REG_SYNTH_BAND	0x02a	/* bit 7: bit 8 of the divider; bits 4-5: the band */
#define RF_REG_SYNTH_FRACTION2	0x02b	/* bits 16-21 of the fraction */
#define RF_REG_SYNTH_FRACTION1	0x02c
#define RF_REG_SYNTH_FRACTION0	0x02d

/*
 * Programs the dividers for a channel's frequency in MHz given the reference
 * clock in units of 100 kHz. The ratio is rounded to one sixteenth and its
 * remainder written as a 22-bit fraction. A frequency the radio's bands do not
 * cover returns 1 before the band is written.
 */
int rfSynthesizerSet(uint32_t frequency, uint32_t reference, int doubled)
{
	int64_t target = (int32_t)(10 * 16 * ((frequency <= 3000) + 1) * frequency);
	int64_t fraction;
	int ratio;
	int rounded;
	unsigned int integer;
	uint32_t band;
	int remainder;

	if (doubled == 0)
		target += target;
	ratio = (int)(target / reference / 3);
	rounded = (ratio / 16) * 16;
	remainder = ratio - rounded;
	if (2 * remainder >= 16)
		rounded += 16;
	integer = rounded / 16;
	fraction = ((target - (int)(integer * reference * 48)) << 21) / (int)(3 * reference);
	fraction += fraction < 0 ? -8 : 8;
	fraction /= 16;
	if (fraction != 0)
		fraction += 1;
	if (doubled != 0)
		integer *= 2;
	rfBusWrite(RF_REG_SYNTH_INTEGER, integer);
	if (integer - 40 < 0x20)
		band = 3;
	else if (integer - 72 < 0x40)
		band = 2;
	else if (integer - 136 <= 0x170)
		band = 1;
	else
		return 1;
	rfBusWrite(RF_REG_SYNTH_BAND, ((integer >> 1) & 0x80) | (band << 4));
	rfBusWrite(RF_REG_SYNTH_FRACTION2, ((uint32_t)fraction & 0x3f0000) >> 16);
	rfBusWrite(RF_REG_SYNTH_FRACTION1, ((uint32_t)fraction & 0xff00) >> 8);
	rfBusWrite(RF_REG_SYNTH_FRACTION0, (uint32_t)fraction);
	return 0;
}
