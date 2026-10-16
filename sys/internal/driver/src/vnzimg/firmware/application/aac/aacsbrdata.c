/*
 * The SBR payload's CRC (0x86cab0-0x86cbca) and the decoding of a frame's
 * envelopes and noise floors against the previous frame (0x86cd14-0x86d5c6).
 */
#include "aac.h"

/* the CRC-10 of ISO/IEC 14496-3 4.6.18.2: x^10 + x^9 + x^5 + x^4 + x + 1 */
typedef struct SbrCrc {
	uint16_t crc;
	uint16_t mask;
	uint16_t poly;
} SbrCrc;

static uint16_t crcUpdate(SbrCrc *crc, uint32_t data, int32_t count)
{
	uint32_t bit = 1u << (count - 1);
	int32_t i;

	for (i = 0; i < count; i++, bit >>= 1) {
		int32_t top = (crc->crc & crc->mask) != 0;
		int32_t in = (data & bit) != 0;

		crc->crc <<= 1;
		if (top ^ in)
			crc->crc ^= crc->poly;
	}
	return crc->crc;
}

static uint32_t crcCompute(AacSbrBits *bits, int32_t count, SbrCrc *crc)
{
	int32_t words, i;

	crc->crc = 0;
	crc->poly = 0x233;
	crc->mask = 0x200;
	words = aacSbrDivide(count, 16);
	count -= words * 16;
	for (i = 0; i < words; i++)
		crcUpdate(crc, aacSbrReadBits(bits, 16), 16);
	crcUpdate(crc, aacSbrReadBits(bits, count), count);
	return crc->crc & 0x3ff;
}

/* bs_sbr_crc_bits against the `count` bits that follow, read from a copy */
int32_t aacSbrCrcCheck(AacSbrBits *bits, int32_t count, int32_t *work)
{
	AacSbrBits *copy = (AacSbrBits *)work;
	uint32_t crc;

	crc = aacSbrReadBits(bits, 10);
	aacSbrBitsCopy(bits, copy);
	if (copy->remaining < 1)
		return 0;
	return (crc ^ crcCompute(copy, aacMin(count, copy->remaining),
		(SbrCrc *)(work + 8))) == 0;
}

static int32_t fixedAdd(int32_t a, int32_t b)
{
	int64_t sum;

	aacFixedAdd(&sum, a, b, 56);
	return aacFixedToInt(&sum, 56);
}

/* 0x86cca0: the band of the other resolution's table a band of this one starts at */
static int32_t otherBand(int32_t difference, int32_t band, int32_t freqRes)
{
	if (freqRes)
		return band;
	if (difference >= 0)
		return band < difference ? band : 2 * band - difference;
	difference = -difference;
	return band < difference ? 3 * band : 2 * band + difference;
}

/* 0x86ccc8: a decoded value into the previous envelope, at the high resolution */
static void storePrevious(int32_t value, int32_t *previous, int32_t difference, int32_t band,
	int32_t freqRes)
{
	int32_t i;

	if (freqRes) {
		previous[band] = value;
		return;
	}
	if (difference >= 0) {
		if (band < difference) {
			previous[band] = value;
			return;
		}
		i = 2 * band - difference;
		previous[i] = value;
		previous[i + 1] = value;
		return;
	}
	difference = -difference;
	if (band < difference) {
		i = 3 * band;
		previous[i] = value;
		previous[i + 1] = value;
		previous[i + 2] = value;
		return;
	}
	i = 2 * band + difference;
	previous[i] = value;
	previous[i + 1] = value;
}

/* a lost frame's stand-in: one envelope, fading towards the previous */
static void conceal(AacSbrHeader *header, AacSbrFrame *frame, AacSbrPrevious *previous)
{
	AacSbrGrid *grid = &frame->grid;
	int32_t timeSlots = header->timeSlots;
	int32_t target = 0, step = 1, i;

	frame->ampResolution = previous->ampResolution;
	frame->coupling = previous->coupling;
	for (i = 0; i < 5; i++)
		frame->invfMode[i] = previous->invfMode[i];
	grid->noiseBorders[1] = timeSlots;
	grid->noiseBorders[0] = previous->lastBorder - timeSlots;
	grid->borders[1] = timeSlots;
	grid->borders[0] = previous->lastBorder - timeSlots;
	grid->numNoise = 1;
	grid->pointer = -1;
	grid->freqRes[0] = 1;
	grid->numEnvelopes = 1;
	frame->dfEnvelope[0] = 1;
	frame->numEnvelopeValues = header->bands->numHigh;

	if (frame->coupling == 2)
		target = 12;
	if (!frame->ampResolution) {
		target = aacFixedShiftLeft(target, 1, 56);
		step = aacFixedShiftLeft(1, 1, 56);
	}
	for (i = 0; i < frame->numEnvelopeValues; i++)
		frame->envelope[i] = target < previous->envelope[i] ? -step : step;

	frame->dfNoise[0] = 1;
	for (i = 0; i < header->bands->numNoise; i++)
		frame->noise[i] = 0;
	aacSbrFill((int8_t *)frame->harmonics, 0, 48);
}

/*
 * After a lost frame the first envelope starts where the last one ended,
 * its energy spread over the longer span.
 */
static void stretchFirstEnvelope(AacSbrHeader *header, AacSbrFrame *frame,
	AacSbrPrevious *previous)
{
	AacSbrGrid *grid = &frame->grid;
	AacSbrBands *bands = header->bands;
	int32_t start = previous->lastBorder - header->timeSlots;
	int32_t length = grid->borders[1] - start, sent = grid->borders[1] - grid->borders[0];
	int32_t ratio, n, i;

	if (length < 1) {
		start = grid->borders[0];
		length = sent;
	}
	ratio = aacSbrLog2Ratio(length, sent);
	grid->noiseBorders[0] = start;
	grid->borders[0] = start;
	ratio >>= 25 - frame->ampResolution;
	if (frame->coupling == 2)
		return;
	n = grid->freqRes[0] ? bands->numHigh : bands->numLow;
	for (i = 0; i < n; i++)
		frame->envelope[i] = fixedAdd(frame->envelope[i], ratio);
}

/* the delta coding undone, in frequency or against the previous frame */
static void envelopeDeltas(AacSbrHeader *header, AacSbrFrame *frame, AacSbrPrevious *previous)
{
	AacSbrBands *bands = header->bands;
	int32_t difference = 2 * bands->numLow - bands->numHigh;
	int32_t *value = frame->envelope;
	int32_t env, freqRes, n, j;

	for (env = 0; env < frame->grid.numEnvelopes; env++) {
		freqRes = frame->grid.freqRes[env];
		n = freqRes ? header->bands->numHigh : header->bands->numLow;
		if (!frame->dfEnvelope[env]) {
			storePrevious(*value++, previous->envelope, difference, 0, freqRes);
			for (j = 1; j < n; j++, value++) {
				*value += value[-1];
				storePrevious(*value, previous->envelope, difference, j, freqRes);
			}
		} else {
			for (j = 0; j < n; j++, value++) {
				*value += previous->envelope[otherBand(difference, j, freqRes)];
				storePrevious(*value, previous->envelope, difference, j, freqRes);
			}
		}
	}
}

/* clamps to the quantiser's range; nonzero if a value was above it */
static int32_t envelopeClamp(AacSbrHeader *header, AacSbrFrame *frame, AacSbrPrevious *previous)
{
	int32_t maximum = frame->ampResolution == 1 ? 35 : 70;
	int32_t over = 0, i;

	for (i = 0; i < frame->numEnvelopeValues; i++) {
		if (maximum < frame->envelope[i])
			over = 1;
		frame->envelope[i] = aacMax(0, frame->envelope[i]);
	}
	for (i = 0; i < header->bands->numHigh; i++) {
		int32_t value = previous->envelope[i];

		previous->envelope[i] = value < 0 ? 0 : aacMin(value, maximum);
	}
	return over;
}

/* 2^(value * a) as SBR_FLOAT: a = 1 or 1/2 by the amplitude resolution */
static void envelopeToFloat(AacSbrFrame *frame, int32_t ampResolution)
{
	int32_t shift = 1 - ampResolution, i;

	for (i = 0; i < frame->numEnvelopeValues; i++) {
		int32_t value = frame->envelope[i];
		int32_t mantissa = value & shift ? 0x05a82799 : 0x04000000;	/* sqrt(2), 1 */

		frame->envelope[i] = SBR_FLOAT(mantissa, (value >> shift) + 23);
	}
}

static void envelopeDecode(AacSbrHeader *header, AacSbrFrame *frame, AacSbrPrevious *previous,
	AacSbrPrevious *other, int32_t *work)
{
	AacSbrBands *bands;
	int32_t i;

	/* a frame that does not continue the last one is an error, or starts afresh */
	if (!header->previousError && !header->error &&
	    frame->grid.borders[0] != previous->lastBorder - header->timeSlots) {
		if (frame->dfEnvelope[0] == 1)
			header->error = 1;
		else
			header->previousError = 1;
	}

	if (header->error) {
		conceal(header, frame, previous);
		envelopeDeltas(header, frame, previous);
		envelopeToFloat(frame, frame->ampResolution);
		return;
	}

	if (header->previousError) {
		stretchFirstEnvelope(header, frame, previous);
		if (frame->coupling != previous->coupling) {
			bands = header->bands;
			for (i = 0; i < bands->numHigh; i++) {
				if (previous->coupling == 2) {
					previous->envelope[i] = other->envelope[i];
				} else if (frame->coupling == 1) {
					int64_t sum, half;

					aacFixedAdd(&sum, previous->envelope[i], other->envelope[i], 56);
					aacFixedShiftRight64(&half, &sum, 1, 56);
					previous->envelope[i] = aacFixedToInt(&half, 56);
				} else if (frame->coupling == 2) {
					previous->envelope[i] = 12;
				}
			}
		}
	}

	aacSbrCopy((int8_t *)work, (const int8_t *)previous->envelope, 192);
	envelopeDeltas(header, frame, previous);
	if (!envelopeClamp(header, frame, previous)) {
		envelopeToFloat(frame, frame->ampResolution);
		return;
	}

	/* out of range: the frame is treated as lost */
	header->error = 1;
	aacSbrCopy((int8_t *)previous->envelope, (const int8_t *)work, 192);
	envelopeDecode(header, frame, previous, other, work + 48);
}

static void noiseClamp(AacSbrHeader *header, AacSbrFrame *frame)
{
	int32_t i;

	for (i = 0; i < header->bands->numNoise * frame->grid.numNoise; i++) {
		if (frame->noise[i] >= 36)
			frame->noise[i] = 35;
		else if (frame->noise[i] < 0)
			frame->noise[i] = 0;
	}
}

/* the noise floors' deltas undone, then 2^(6 - value) as SBR_FLOAT unless coupled */
static void noiseDecode(AacSbrHeader *header, AacSbrFrame *frame, AacSbrPrevious *previous)
{
	int32_t numNoise = header->bands->numNoise, envelopes = frame->grid.numNoise, i;
	int32_t *noise = frame->noise;

	if (!frame->dfNoise[0]) {
		for (i = 1; i < numNoise; i++)
			noise[i] += noise[i - 1];
	} else {
		for (i = 0; i < numNoise; i++)
			noise[i] += previous->noise[i];
	}
	if (envelopes >= 2) {
		if (!frame->dfNoise[1]) {
			for (i = numNoise + 1; i < 2 * numNoise; i++)
				noise[i] += noise[i - 1];
		} else {
			for (i = 0; i < numNoise; i++)
				noise[numNoise + i] += noise[i];
		}
	}
	noiseClamp(header, frame);
	for (i = 0; i < numNoise; i++)
		previous->noise[i] = noise[(envelopes - 1) * numNoise + i];
	if (frame->coupling)
		return;
	for (i = 0; i < numNoise * envelopes; i++)
		noise[i] = SBR_FLOAT(0x04000000, 45 - noise[i]);
}

/*
 * A coupled pair's level and balance into left and right:
 * L = 2^(level + 1) / (1 + 2^(12 - balance)) ... as floats.
 */
static void uncouple(AacSbrHeader *header, AacSbrFrame *left, AacSbrFrame *right)
{
	int32_t mantissa, i;
	int8_t exponent;

	for (i = 0; i < left->numEnvelopeValues; i++) {
		int32_t balance = right->envelope[i], level = left->envelope[i];
		int8_t balanceExponent = SBR_EXPONENT(balance) - 34;
		int32_t sum, product;
		int8_t sumExponent;

		aacSbrFloatAdd(SBR_MANTISSA(balance), balanceExponent, 0x04000000, 1, &sum,
			&sumExponent);
		aacSbrFloatDivide(SBR_MANTISSA(level), SBR_EXPONENT(level) - 15, sum, sumExponent,
			&mantissa, &exponent);
		{
			int64_t wide;

			aacFixedMultiply(&wide, SBR_MANTISSA(balance), mantissa, 56);
			product = aacFixedToInt(&wide, 56);
		}
		right->envelope[i] = SBR_FLOAT(fixedAdd(mantissa, 128), exponent + 16);
		left->envelope[i] = SBR_FLOAT(fixedAdd(product, 128),
			balanceExponent + exponent + 16);
	}

	for (i = 0; i < header->bands->numNoise * left->grid.numNoise; i++) {
		int32_t balance = right->noise[i] - 12;

		aacSbrFloatAdd(0x04000000, balance + 1, 0x04000000, 1, &mantissa, &exponent);
		aacSbrFloatDivide(0x04000000, 8 - left->noise[i], mantissa, exponent, &mantissa,
			&exponent);
		right->noise[i] = SBR_FLOAT(fixedAdd(mantissa, 128), exponent + 38);
		left->noise[i] = SBR_FLOAT(fixedAdd(mantissa, 128), balance + exponent + 38);
	}
}

/*
 * A frame's envelopes and noise floors decoded against each channel's
 * previous frame, with concealment of lost frames.
 */
void aacSbrDecodeData(AacSbrHeader *header, AacSbrFrame *left, AacSbrPrevious *leftPrevious,
	AacSbrFrame *right, AacSbrPrevious *rightPrevious, int32_t *work)
{
	int8_t error;

	envelopeDecode(header, left, leftPrevious, rightPrevious, work);
	noiseDecode(header, left, leftPrevious);
	if (!right)
		return;
	error = header->error;
	envelopeDecode(header, right, rightPrevious, leftPrevious, work);
	noiseDecode(header, right, rightPrevious);
	if (!error && header->error)
		envelopeDecode(header, left, leftPrevious, rightPrevious, work);
	if (left->coupling)
		uncouple(header, left, right);
}
