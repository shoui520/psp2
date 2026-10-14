/*
 * A channel's spectrum to its samples (0x850e0c-0x8510b0), and the
 * stream's synchronisation (0x8512cc-0x851412). The arithmetic is in the
 * VLIW kernels; this is their order.
 */
#include "aac.h"

#define ADIF_ID		0x41444946	/* "ADIF" */
#define ADTS_SYNC	0xfff0

static void aacPulseDecode(AacBitstream *bits, AacIcs *ics);
static void aacFilterbank(AacBitstream *bits, AacIcs *ics, uint32_t channel, int32_t *work);

void aacSceReconstruct(AacBitstream *bits, AacSingleChannelElement *sce, uint32_t channel,
	uint32_t value14, int32_t *spectra)
{
	int32_t *work = spectra + 2048;

	aacPulseDecode(bits, sce->ics);
	aacDequantize(bits, sce->ics, NULL, value14, work);
	aacTnsFilter(bits, sce->ics);
	aacImdct(bits, sce->ics, work);
	aacFilterbank(bits, sce->ics, channel, work);
	aacChannelOutput(bits, sce->ics, channel);
}

void aacCpeReconstruct(AacBitstream *bits, AacChannelPairElement *cpe, uint32_t channel,
	uint32_t value14, int32_t *spectra)
{
	int32_t *work = spectra + 2048;

	aacPulseDecode(bits, cpe->ics[0]);
	aacDequantize(bits, cpe->ics[0], cpe, value14, work);
	aacPulseDecode(bits, cpe->ics[1]);
	aacDequantize(bits, cpe->ics[1], cpe, value14, work);
	aacStereo(bits, cpe);
	aacTnsFilter(bits, cpe->ics[0]);
	aacTnsFilter(bits, cpe->ics[1]);
	aacImdct(bits, cpe->ics[0], work);
	aacImdct(bits, cpe->ics[1], work);
	aacFilterbank(bits, cpe->ics[0], channel, work);
	aacFilterbank(bits, cpe->ics[1], channel + 1, work);
	aacChannelOutput(bits, cpe->ics[0], channel);
	aacChannelOutput(bits, cpe->ics[1], channel + 1);
}

static void copyWords(int32_t *to, const int32_t *from, uint32_t count)
{
	while (count--)
		*to++ = *from++;
}

/*
 * The previous frame's overlap is windowed by its own shape, this frame's
 * first half by the new one; the second half is kept for the next frame.
 */
static void aacFilterbank(AacBitstream *bits, AacIcs *ics, uint32_t channel, int32_t *work)
{
	int32_t *overlap = bits->overlap[channel];
	uint8_t *sequence = &bits->windowSequence[channel];
	uint8_t *shape = &bits->windowShape[channel];
	const int32_t *const *previous, *const *current;

	previous = *shape ? &bits->tables->kbdWindowLong : &bits->tables->sineWindowLong;
	current = ics->info.windowShape ? &bits->tables->kbdWindowLong :
		&bits->tables->sineWindowLong;

	/* windows[0] is the long window, windows[1] the short */
	if (*sequence == AAC_EIGHT_SHORT_SEQUENCE)
		aacOverlapShort(overlap, work, previous[1]);
	else if (*sequence == AAC_LONG_START_SEQUENCE)
		aacOverlapStart(overlap, work, previous[1]);
	else
		aacOverlapLong(overlap, work, previous[0]);

	if (ics->info.windowSequence == AAC_EIGHT_SHORT_SEQUENCE) {
		aacWindowShort(ics->spectrum, work, work, previous[1], current[1]);
		copyWords(overlap + 32, ics->spectrum + 544, 480);
		copyWords(overlap, work + 1024, 32);
		copyWords(overlap + 96, work + 1056, 32);
	} else {
		if (ics->info.windowSequence == AAC_LONG_STOP_SEQUENCE)
			aacWindowStop(ics->spectrum, work, work, previous[1]);
		else
			aacWindowLong(ics->spectrum, work, work, previous[0]);
		copyWords(overlap, ics->spectrum + 256, 512);
	}
	copyWords(ics->spectrum, work, AAC_FRAME_SAMPLES);

	*sequence = ics->info.windowSequence;
	*shape = ics->info.windowShape;
}

/* pulse_data(): the pulses' amplitudes, added away from zero */
static void aacPulseDecode(AacBitstream *bits, AacIcs *ics)
{
	const uint32_t *swbOffset = bits->tables->swbOffsetLong[bits->samplingIndex];
	uint32_t numSwb = bits->tables->numSwb[0][bits->samplingIndex];
	int32_t k, i, count, value, *spectrum;

	k = swbOffset[aacMin(ics->pulse.pulseStartSfb, numSwb)];
	if (!ics->pulseDataPresent)
		return;
	spectrum = ics->spectrum;
	count = (uint8_t)(ics->pulse.numberPulse + 1);
	for (i = 0; i < count; i++) {
		k += aacMin(ics->pulse.pulseOffset[i], 1023 - k);
		value = spectrum[k];
		if (value > 0)
			spectrum[k] = value + ics->pulse.pulseAmp[i];
		else
			spectrum[k] = value - ics->pulse.pulseAmp[i];
	}
}

/*
 * Finds an ADTS syncword or an ADIF header within the first `minimum`
 * bytes; failing that, an ADTS syncword anywhere before the last header's
 * worth, which then starts the buffer. Returns 1 for ADIF, 2 for ADTS.
 */
int32_t aacSync(AacBitPosition *start, AacBitstream *bits, uint32_t minimum, uint32_t size)
{
	uint32_t window = 0;
	int32_t result = 0;

	start->bits = 8;
	start->byte = 0;
	while (start->byte < minimum) {
		window = (window << 8) + *bits->cursor++;
		bits->bytesRead++;
		start->byte++;
		if (window == ADIF_ID) {
			result = 1;
			start->byte -= 4;
			break;
		}
		if ((window & ADTS_SYNC) == ADTS_SYNC) {
			start->byte -= 2;
			bits->bytesRead -= 2;
			bits->cursor -= 2;
			bits->position = *start;
			result = 2;
			break;
		}
	}
	if (!result) {
		while (start->byte < size - 9) {
			window = (window << 8) + *bits->cursor++;
			bits->bytesRead++;
			start->byte++;
			if ((window & ADTS_SYNC) == ADTS_SYNC) {
				result = 2;
				start->byte -= 2;
				bits->bytesRead -= 2;
				bits->cursor -= 2;
				bits->position = *start;
				bits->position.byte = 0;
				bits->buffer = bits->cursor;
				break;
			}
		}
	}
	bits->end += start->byte;
	return result;
}

void aacBitstreamReset(AacBitstream *bits)
{
	bits->position.bits = 8;
	bits->position.byte = 0;
	bits->bytesRead = 0;
	bits->randomSeed = 0;
	bits->cursor = bits->buffer;
}

/*
 * The channel's samples (VLIW, 0x8b7340): clipped to 30 bits, then
 * doubled (cpclipi3.w 30, cpslai3.w 2, cpsrai3.w 1).
 */
void aacChannelOutput(AacBitstream *bits, AacIcs *ics, uint32_t channel)
{
	int32_t *out = bits->output[channel], *spectrum = ics->spectrum, i, value;

	for (i = 0; i < AAC_FRAME_SAMPLES; i++) {
		value = spectrum[i];
		if (value > 0x1fffffff)
			value = 0x1fffffff;
		else if (value < -0x20000000)
			value = -0x20000000;
		out[i] = value * 2;
	}
}
