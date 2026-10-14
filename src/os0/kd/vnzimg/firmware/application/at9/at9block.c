/*
 * An ATRAC9 block (0x885ba8-0x8892c0): the band parameters, the gradient,
 * the scale factors of each channel, its precisions, the spectrum read
 * from the codebooks and scaled by the scale factors. The first channel of a
 * stereo block carries all the units, the second only the first
 * `stereoUnits` of its own; the rest come from the first by intensity
 * stereo.
 */
#include "at9.h"

#define MAX_COEFFICIENTS	256
#define MAX_PRECISION		15
#define SCALE_FACTOR_MAX	31

/* 0x885fe8: the channels a block of the type has, or -1 */
int32_t at9ChannelsOfBlock(int32_t type)
{
	if (type == AT9_BLOCK_MONO)
		return 1;
	return type == AT9_BLOCK_STEREO ? 2 : -1;
}

/* 0x8881d0 and 0x888108: a symbol of the next 9 or 17 bits */
static int32_t readScalar(At9Reader *reader, const At9ScalarBook *book)
{
	int32_t symbol = book->table[at9Peek(reader, book->width)];

	reader->bit += book->lengths[symbol];
	return symbol;
}

static int32_t readScalarWide(At9Reader *reader, const At9ScalarBook *book)
{
	int32_t symbol = book->table[at9PeekWide(reader, book->width)];

	reader->bit += book->lengths[symbol];
	return symbol;
}

/* the record of the next bits of a vector codebook, taken */
static const int8_t *readRecord(At9Reader *reader, const At9VectorBook *book, int32_t size,
	int32_t wide)
{
	uint32_t index = wide ? at9PeekWide(reader, book->width) : at9Peek(reader, book->width);
	const int8_t *record = book->records + size * book->table[index];

	reader->bit += record[0];
	return record;
}

/* 0x886028 */
static int32_t readBandParams(At9DecoderState *decoder, At9Block *block, int32_t type)
{
	int32_t count, stereo, extension;

	count = at9Read(&decoder->reader, 4);
	if (at9MaxBand[decoder->rateIndex] < count)
		return AT9_ERROR_BAND_COUNT;
	block->quantUnits = at9BandToQuantUnits[count];
	if (type == AT9_BLOCK_STEREO) {
		stereo = at9Read(&decoder->reader, 4);
		if (count < stereo)
			return AT9_ERROR_STEREO_BAND;
		block->stereoUnits = at9BandToQuantUnits[stereo];
	} else {
		block->stereoUnits = block->quantUnits;
	}
	block->hasExtension = at9ReadBit(&decoder->reader);
	if (block->hasExtension) {
		extension = at9Read(&decoder->reader, 4);
		if (extension < count)
			return AT9_ERROR_EXTENSION_BAND;
		block->extensionUnits = at9BandToQuantUnits[extension];
	} else {
		block->extensionUnits = block->quantUnits;
	}
	return 0;
}

/* 0x886240: the gradient the precisions fall along: from a unit and value to another */
static int32_t readGradientParams(At9DecoderState *decoder, At9Params *params)
{
	params->gradientMode = at9Read(&decoder->reader, 2);
	if (params->gradientMode == 0) {
		params->gradientStartUnit = at9Read(&decoder->reader, 6);
		params->gradientEndUnit = at9Read(&decoder->reader, 6);
		params->gradientStartValue = at9Read(&decoder->reader, 5);
		params->gradientEndValue = at9Read(&decoder->reader, 5);
		params->gradientEndUnit++;
	} else {
		params->gradientEndUnit = 31;
		params->gradientEndValue = 31;
		params->gradientStartUnit = at9Read(&decoder->reader, 5);
		params->gradientStartValue = at9Read(&decoder->reader, 5);
	}
	params->gradientBoundary = at9Read(&decoder->reader, 4);
	if (params->block->quantUnits < params->gradientBoundary)
		return AT9_ERROR_GRADIENT_BOUNDARY;
	if (params->gradientStartUnit >= 48)
		return AT9_ERROR_GRADIENT_START;
	if (params->gradientEndUnit >= 49)
		return AT9_ERROR_GRADIENT_END;
	return params->gradientStartUnit > params->gradientEndUnit ? AT9_ERROR_GRADIENT_ORDER : 0;
}

/* 0x886468: which channel of a stereo block is coded whole, the intensity signs, reserved bits */
static void readStereoParams(At9DecoderState *decoder, At9Params *params, int32_t type)
{
	At9Block *block = params->block;
	int32_t channels = at9ChannelsOfBlock(type), i;

	params->primaryChannel = 0;
	if (type == AT9_BLOCK_STEREO) {
		params->primaryChannel = at9ReadBit(&decoder->reader);
		if (at9ReadBit(&decoder->reader)) {
			for (i = block->stereoUnits; i < block->quantUnits; i++)
				params->stereoSigns[i] = at9ReadBit(&decoder->reader);
		} else {
			memset(params->stereoSigns, 0, block->quantUnits);
		}
	}
	if (block->hasExtension)
		for (i = 0; i < channels; i++)
			at9ReadBit(&decoder->reader);
	if (at9ReadBit(&decoder->reader)) {
		at9Read(&decoder->reader, 2);
		decoder->reader.bit += at9Read(&decoder->reader, 5);
	}
}

/* 0x887dd8: the gradient of the units, interpolated between its ends */
static void buildGradient(At9Params *params)
{
	int32_t quantUnits = params->block->quantUnits;
	int32_t unit = 0, end, units, delta, k = 0;
	const int16_t *curve;

	end = min(params->gradientStartUnit, quantUnits);
	for (; unit < end; unit++)
		params->gradient[unit] = params->gradientStartValue;

	units = params->gradientEndUnit - params->gradientStartUnit;
	end = min(params->gradientEndUnit, quantUnits);
	if (units >= 1) {
		curve = at9GradientCurves[units - 1];
		delta = params->gradientEndValue - params->gradientStartValue;
		if (delta > 0) {
			for (; unit < end; unit++, k++)
				params->gradient[unit] = params->gradientStartValue +
					((curve[k] * (delta - 1)) >> 10) + 1;
		} else if (-1 - delta >= 0) {
			for (; unit < end; unit++, k++)
				params->gradient[unit] = params->gradientStartValue -
					((curve[k] * (-1 - delta)) >> 10) - 1;
		}
	}
	for (; unit < quantUnits; unit++)
		params->gradient[unit] = params->gradientEndValue;
}

/*
 * Scale factors: the codings. A unit's value is 0 to 31. The first
 * channel's value after a baseline (the weights of a curve for the "delta"
 * codings, the last frame's, the other channel's) plus the codes' sum.
 */

/* 0x8868a0: a start value, a curve, and deltas */
static int32_t readDeltaOffset(At9DecoderState *decoder, At9Params *params, At9Channel *channel)
{
	At9Reader *reader = &decoder->reader;
	int32_t count = params->block->extensionUnits;
	int32_t result = 0, row, base, width, i, value;
	const uint8_t *weights;
	const At9ScalarBook *book;

	row = at9Read(reader, 3);
	weights = at9ScaleFactorOffsets[row];
	base = at9Read(reader, 5);
	width = at9Read(reader, 2) + 3;
	channel->scaleFactors[0] = at9Read(reader, width);
	book = &at9ScaleFactorBook[width - 1];
	for (i = 1; i < count; i++) {
		channel->scaleFactors[i] = (channel->scaleFactors[i - 1] + readScalar(reader, book)) &
			book->limit;
		value = (int8_t)(channel->scaleFactors[i - 1] + base - weights[i - 1]);
		channel->scaleFactors[i - 1] = value;
		if (value < 0 || value >= 32)
			result = AT9_ERROR_SCALE_FACTOR_RANGE;
	}
	value = (int8_t)(channel->scaleFactors[count - 1] + base - weights[count - 1]);
	channel->scaleFactors[count - 1] = value;
	if (value < 0 || value >= 32)
		result = AT9_ERROR_SCALE_FACTOR_RANGE;
	return result;
}

/* 0x886b80: a fixed number of bits each, after a start value */
static int32_t readConstantLength(At9DecoderState *decoder, At9Params *params, At9Channel *channel)
{
	At9Reader *reader = &decoder->reader;
	int32_t count = params->block->extensionUnits;
	int32_t result = 0, width, offset, i;
	int8_t value;

	width = at9Read(reader, 2) + 2;
	if (width >= 5) {
		for (i = 0; i < count; i++)
			channel->scaleFactors[i] = at9Read(reader, 5);
		return 0;
	}
	offset = at9Read(reader, 5);
	for (i = 0; i < count; i++) {
		value = at9Read(reader, width) + offset;
		channel->scaleFactors[i] = value;
		if (value >= 32)
			result = AT9_ERROR_SCALE_FACTOR_RANGE;
	}
	return result;
}

/* 0x886d58: the second channel's distance from the first, the signed codebook's */
static int32_t readDistanceFromFirst(At9DecoderState *decoder, At9Params *params,
	At9Channel *channel)
{
	At9Reader *reader = &decoder->reader;
	int32_t count = params->block->extensionUnits;
	const At9Channel *first = channel - 1;
	const At9ScalarBook *book = &at9ScaleFactorSignedBook[at9Read(reader, 2)];
	int32_t i;

	for (i = 0; i < count; i++)
		channel->scaleFactors[i] = (first->scaleFactors[i] + readScalar(reader, book)) &
			SCALE_FACTOR_MAX;
	return 0;
}

/* 0x887098: deltas from a start value, offset by the first channel's */
static int32_t readDeltaFromFirst(At9DecoderState *decoder, At9Params *params,
	At9Channel *channel)
{
	At9Reader *reader = &decoder->reader;
	int32_t count = params->block->extensionUnits;
	const At9Channel *first = channel - 1;
	const At9ScalarBook *book;
	int32_t result = 0, bias, width, i, value;

	bias = at9Read(reader, 5) - 16;
	width = at9Read(reader, 2) + 1;
	channel->scaleFactors[0] = at9Read(reader, width);
	book = &at9ScaleFactorBook[width - 1];
	for (i = 1; i < count; i++) {
		channel->scaleFactors[i] = (channel->scaleFactors[i - 1] + readScalar(reader, book)) &
			book->limit;
		value = (int8_t)(channel->scaleFactors[i - 1] + first->scaleFactors[i - 1] + bias);
		channel->scaleFactors[i - 1] = value;
		if (value < 0 || value >= 32)
			result = AT9_ERROR_SCALE_FACTOR_RANGE;
	}
	value = (int8_t)(channel->scaleFactors[count - 1] + first->scaleFactors[count - 1] + bias);
	channel->scaleFactors[count - 1] = value;
	if (value < 0 || value >= 32)
		result = AT9_ERROR_SCALE_FACTOR_RANGE;
	return result;
}

/* 0x887338: the distance from the last frame's, the units it had; the rest 5 bits each */
static int32_t readDistanceFromPrevious(At9DecoderState *decoder, At9Params *params,
	At9Work *work, At9Channel *channel)
{
	At9Reader *reader = &decoder->reader;
	int32_t count = params->block->extensionUnits;
	const At9ScalarBook *book;
	int32_t known, i;

	if (!params->notFirst)
		return AT9_ERROR_PREVIOUS_MISSING;
	known = min(params->block->quantUnitsPrevious, count);
	book = &at9ScaleFactorSignedBook[at9Read(reader, 2)];
	for (i = 0; i < known; i++)
		channel->scaleFactors[i] = (work->scaleFactors[i] + readScalar(reader, book)) &
			SCALE_FACTOR_MAX;
	for (i = known; i < count; i++)
		channel->scaleFactors[i] = at9Read(reader, 5);
	return 0;
}

/* 0x887708: deltas from a start value, offset by the last frame's */
static int32_t readDeltaFromPrevious(At9DecoderState *decoder, At9Params *params,
	At9Work *work, At9Channel *channel)
{
	At9Reader *reader = &decoder->reader;
	int32_t count = params->block->extensionUnits;
	const At9ScalarBook *book;
	int32_t result = 0, known, bias, width, i, value;

	if (!params->notFirst)
		return AT9_ERROR_PREVIOUS_MISSING;
	known = min(params->block->quantUnitsPrevious, count);
	bias = at9Read(reader, 5) - 16;
	width = at9Read(reader, 2) + 1;
	channel->scaleFactors[0] = at9Read(reader, width);
	book = &at9ScaleFactorBook[width - 1];
	for (i = 1; i < known; i++) {
		channel->scaleFactors[i] = (channel->scaleFactors[i - 1] + readScalar(reader, book)) &
			book->limit;
		value = (int8_t)(channel->scaleFactors[i - 1] + (int8_t)work->scaleFactors[i - 1] + bias);
		channel->scaleFactors[i - 1] = value;
		if (value < 0 || value >= 32)
			result = AT9_ERROR_SCALE_FACTOR_RANGE;
	}
	value = (int8_t)(channel->scaleFactors[known - 1] + (int8_t)work->scaleFactors[known - 1] + bias);
	channel->scaleFactors[known - 1] = value;
	if (value < 0 || value >= 32)
		result = AT9_ERROR_SCALE_FACTOR_RANGE;
	for (i = known; i < count; i++)
		channel->scaleFactors[i] = at9Read(reader, 5);
	return result;
}

/* 0x886680: the coding of a channel's scale factors, and its values kept for the next frame */
static int32_t readScaleFactors(At9DecoderState *decoder, At9Params *params, At9Work *work,
	At9Channel *channel, int32_t second)
{
	int32_t mode = at9Read(&decoder->reader, 2), result;

	if (!second) {
		if (mode == 0)
			result = readDeltaOffset(decoder, params, channel);
		else if (mode == 1)
			result = readConstantLength(decoder, params, channel);
		else if (mode == 2)
			result = readDistanceFromPrevious(decoder, params, work, channel);
		else
			result = readDeltaFromPrevious(decoder, params, work, channel);
	} else {
		if (mode == 0)
			result = readDeltaOffset(decoder, params, channel);
		else if (mode == 1)
			result = readDistanceFromFirst(decoder, params, channel);
		else if (mode == 2)
			result = readDeltaFromFirst(decoder, params, channel);
		else
			result = readDistanceFromPrevious(decoder, params, work, channel);
	}
	if (decoder->framesPerSuperframe != 1 &&
		decoder->frameIndex + 1 != decoder->framesPerSuperframe)
		memcpy(work->scaleFactors, channel->scaleFactors, AT9_MAX_UNITS);
	return result;
}

/* 0x887fd8: where a scale factor steps by 2 or more, the units beside it gain precision */
static void buildPrecisionMask(At9Params *params, const At9Channel *channel)
{
	int32_t units = params->block->quantUnits, i, delta;

	memset(params->precisionMask, 0, units);
	for (i = 1; i < units; i++) {
		delta = channel->scaleFactors[i] - channel->scaleFactors[i - 1];
		if (delta >= 1) {
			params->precisionMask[i] += min(delta - 1, 5);
		} else if (delta != 0) {
			params->precisionMask[i - 1] -= max(delta + 1, -5);
		}
	}
}

/*
 * 0x887a50: the precision of each unit, and the part above 15, from the
 * scale factor less the gradient (with the mask added and scaled in the
 * gradient modes 1 to 3), at least 1, and one more below the boundary.
 */
static void buildPrecisions(At9Params *params, At9Channel *channel)
{
	const uint8_t *boundary = at9GradientCurveBoundary[params->gradientBoundary];
	int32_t mode = params->gradientMode, i, value, total;
	int8_t distance, sum;

	if (mode != 0)
		buildPrecisionMask(params, channel);
	for (i = 0; i < AT9_MAX_UNITS; i++) {
		distance = channel->scaleFactors[i] - params->gradient[i];
		sum = distance + params->precisionMask[i];
		if (mode == 1)
			value = (distance + params->precisionMask[i]) >> 1;
		else if (mode == 2)
			value = (int8_t)(sum * 3) >> 3;
		else if (mode == 3)
			value = sum >> 2;
		else
			value = distance;
		value = max(value, 1);
		total = (int8_t)(value + boundary[i]);
		channel->precision[i] = min(total, MAX_PRECISION);
		channel->precisionFine[i] = min(max(total - MAX_PRECISION, 0), MAX_PRECISION);
	}
}

/*
 * 0x888400: the stride of a unit's codebooks by its coefficients: 4, 8 or
 * 16 of them.
 */
static int32_t codebookStride(int32_t coefficients)
{
	if (coefficients == 4)
		return 0;
	return coefficients != 8 ? 2 : 1;
}

/*
 * 0x888440: the units with a peak in their scale factor (3 above the
 * lower neighbour, or above both together; from unit 12 also 2 above it and
 * not below the first 12's mean) use the second set of codebooks.
 */
static void buildCodebookSet(At9Channel *channel, int32_t units)
{
	int8_t *set = AT9_CODEBOOK_SET(channel);
	const int8_t *factors = channel->scaleFactors;
	int32_t average = 0, i, minimum;

	memset(set + 8, 0, 24);
	if (units >= 13) {
		for (i = 0; i < 12; i++)
			average += factors[i];
		average = (average * 2731 + 16384) >> 15;
	}
	if (units - 1 >= 9) {
		for (i = 8; i <= units - 2; i++) {
			minimum = min(factors[i - 1], factors[i + 1]);
			if (factors[i] - minimum >= 3 ||
				factors[i] * 2 - factors[i + 1] - factors[i - 1] >= 3)
				set[i] = 1;
		}
	}
	if (factors[units - 1] - factors[units - 2] >= 3)
		set[units - 1] = 1;
	if (units - 1 >= 13) {
		for (i = 12; i <= units - 2; i++) {
			if (set[i])
				continue;
			minimum = min(factors[i - 1], factors[i + 1]);
			if (factors[i] - minimum >= 2 &&
				factors[i] >= average - (at9UnitCoefficientCounts[i] == 16))
				set[i] = 1;
		}
	}
	if (units >= 13 && set[units - 1] == 0 &&
		factors[units - 1] - factors[units - 2] >= 2 &&
		factors[units - 1] >= average - (at9UnitCoefficientCounts[units - 1] == 16))
		set[units - 1] = 1;
}

/* a code's value at the precision: the factor times it shifted to the top, the high word */
static inline int32_t scaled(int32_t value, int32_t shift, int32_t factor)
{
	return (int32_t)(((int64_t)factor * (int32_t)((uint32_t)value << shift)) >> 32);
}

/* a number of bits for one coefficient, from the reader that reaches that far */
static inline int32_t readBits(At9Reader *reader, int32_t count)
{
	return count < 10 ? at9Read(reader, count) : at9ReadWide(reader, count);
}

/*
 * 0x888788: the coefficients of the units. A unit has 2 coefficients below
 * unit 8, then 4, 8 or 16. A precision of 1 to 6 bits (the bits are the
 * precision + 1) is coded with a codebook, 7 and over as they are; each
 * coefficient is the code at the unit's factor.
 */
static void readSpectrum(At9DecoderState *decoder, At9Channel *channel, int32_t units)
{
	At9Reader *reader = &decoder->reader;
	const int8_t *set = AT9_CODEBOOK_SET(channel);
	int32_t *out = channel->spectrum;
	int32_t unit, count, k, bits, shift, factor, stride, value[4];
	const At9ScalarBook *scalar;
	const At9VectorBook *vector;
	const int8_t *record;

	for (unit = 0; unit < 8; unit++) {
		bits = channel->precision[unit] + 1;
		factor = at9SpectrumScale[bits - 1];
		shift = 32 - bits;
		if (bits >= 8) {
			value[0] = readBits(reader, bits);
			value[1] = readBits(reader, bits);
		} else if (bits >= 5) {
			scalar = &at9SpectrumLowBook[bits - 5];
			value[0] = readScalar(reader, scalar);
			value[1] = readScalar(reader, scalar);
		} else {
			record = readRecord(reader, &at9PairBook[bits - 2], 3, 0);
			value[0] = record[1];
			value[1] = record[2];
		}
		*out++ = scaled(value[0], shift, factor);
		*out++ = scaled(value[1], shift, factor);
	}

	for (unit = 8; unit < units; unit++) {
		count = at9UnitCoefficientCounts[unit];
		bits = channel->precision[unit] + 1;
		factor = at9SpectrumScale[bits - 1];
		shift = 32 - bits;
		stride = codebookStride(count);
		for (k = 0; k < count; k += 4) {
			if (bits >= 8) {
				value[0] = readBits(reader, bits);
				value[1] = readBits(reader, bits);
				value[2] = readBits(reader, bits);
				value[3] = readBits(reader, bits);
			} else if (bits == 2) {
				vector = &at9QuadBook[stride * 2 + set[unit]];
				record = readRecord(reader, vector, 5, 1);
				value[0] = record[1];
				value[1] = record[2];
				value[2] = record[3];
				value[3] = record[4];
			} else if (bits < 5) {
				vector = &at9PairHighBook[stride * 4 + set[unit] * 2 + bits - 3];
				record = readRecord(reader, vector, 3, 1);
				value[0] = record[1];
				value[1] = record[2];
				record = readRecord(reader, vector, 3, 1);
				value[2] = record[1];
				value[3] = record[2];
			} else {
				scalar = &at9SpectrumBook[stride * 12 + set[unit] * 6 + bits - 5];
				value[0] = readScalarWide(reader, scalar);
				value[1] = readScalarWide(reader, scalar);
				value[2] = readScalarWide(reader, scalar);
				value[3] = readScalarWide(reader, scalar);
			}
			*out++ = scaled(value[0], 32 - bits, factor);
			*out++ = scaled(value[1], 32 - bits, factor);
			*out++ = scaled(value[2], 32 - bits, factor);
			*out++ = scaled(value[3], 32 - bits, factor);
		}
	}
	if (unit < 30) {
		/* eight words at a time, so the end may be passed */
		do {
			out[0] = 0;
			out[1] = 0;
			out[2] = 0;
			out[3] = 0;
			out[4] = 0;
			out[5] = 0;
			out[6] = 0;
			out[7] = 0;
			out += 8;
		} while (out < channel->spectrum + MAX_COEFFICIENTS);
	}
}

/*
 * 0x889478: the units with a precision over 15 add the next bits of their
 * coefficients' values, at a finer factor.
 */
static void readSpectrumFine(At9DecoderState *decoder, At9Channel *channel, int32_t units)
{
	At9Reader *reader = &decoder->reader;
	int32_t *out = channel->spectrum;
	int32_t unit, count, k, bits, factor, i, value[4];

	for (unit = 0; unit < 8; unit++) {
		if (channel->precisionFine[unit] >= 1) {
			bits = channel->precisionFine[unit] + 1;
			factor = at9SpectrumScaleFine[channel->precisionFine[unit]];
			value[0] = at9ReadWide(reader, bits);
			value[1] = at9ReadWide(reader, bits);
			out[0] += scaled(value[0], 32 - bits, factor);
			out[1] += scaled(value[1], 32 - bits, factor);
		}
		out += 2;
	}
	for (unit = 8; unit < units; unit++) {
		count = at9UnitCoefficientCounts[unit];
		if (channel->precisionFine[unit] >= 1) {
			bits = channel->precisionFine[unit] + 1;
			factor = at9SpectrumScaleFine[channel->precisionFine[unit]];
			k = 0;
			do {
				for (i = 0; i < 4; i++)
					value[i] = at9ReadWide(reader, bits);
				for (i = 0; i < 4; i++)
					out[i] += scaled(value[i], 32 - bits, factor);
				out += 4;
				k += 4;
			} while (count > k);
		} else {
			out += count;
		}
	}
}

/*
 * 0x888298: each unit's coefficients scaled down by the unit's scale factor
 * (a shift of 31 less the scale factor, 31 at most).
 */
static void scaleSpectrum(At9Params *params, At9Channel *channel)
{
	int32_t *out = channel->spectrum;
	int32_t units = params->block->quantUnits, unit, count, shift, i;

	for (unit = 0; unit < 8; unit++) {
		shift = min(31 - channel->scaleFactors[unit], 31);
		out[0] >>= shift;
		out[1] >>= shift;
		out += 2;
	}
	for (unit = 8; unit < units; unit++) {
		count = at9UnitCoefficientCounts[unit];
		shift = min(31 - channel->scaleFactors[unit], 31);
		for (i = 0; i < count; i++)
			out[i] >>= shift;
		out += count;
	}
}

/*
 * 0x8892c0: the units from the stereo ones on are the coded channel's,
 * negated where the sign says so.
 */
static void applyIntensityStereo(At9Params *params, At9Channel *channels)
{
	const At9Block *block = params->block;
	const int32_t *source = channels[params->primaryChannel].spectrum;
	int32_t *destination = channels[params->primaryChannel == 0].spectrum;
	int32_t unit, i;

	for (unit = block->stereoUnits; unit < block->quantUnits; unit++)
		for (i = at9UnitCoefficientIndex[unit]; i < at9UnitCoefficientIndex[unit + 1]; i++)
			destination[i] = params->stereoSigns[unit] ? -source[i] : source[i];
}

/*
 * 0x885ba8: one block of the frame read: the parameters (the band counts
 * unless the last frame's are reused), the gradient, then for each
 * channel its scale factors, precisions and spectrum.
 */
int32_t at9DecodeBlock(At9DecoderState *decoder, At9Block *block, At9Channel *channels,
	int32_t type)
{
	At9Params params;
	int32_t count = at9ChannelsOfBlock(type), channel, error, units;
	uint32_t notFirst, reuse;

	memset(&params, 0, sizeof(params));
	notFirst = at9ReadBit(&decoder->reader);
	params.notFirst = notFirst;
	if ((decoder->frameIndex == 0 && notFirst == 1) || (decoder->frameIndex >= 1 && !notFirst))
		return AT9_ERROR_FRAME_SEQUENCE;
	reuse = at9ReadBit(&decoder->reader);
	if (!reuse) {
		error = readBandParams(decoder, block, type);
		if (error)
			return error;
	} else if (!notFirst) {
		return AT9_ERROR_FIRST_FRAME_REUSE;
	}
	params.block = block;
	error = readGradientParams(decoder, &params);
	if (error)
		return error;
	readStereoParams(decoder, &params, type);
	buildGradient(&params);

	for (channel = 0; channel < count; channel++) {
		At9Channel *current = &channels[channel];

		error = readScaleFactors(decoder, &params, (At9Work *)block->work[channel], current,
			channel);
		if (error)
			return error;
		buildPrecisions(&params, current);
		units = channel == params.primaryChannel ? block->quantUnits : block->stereoUnits;
		buildCodebookSet(current, units);
		readSpectrum(decoder, current, units);
		readSpectrumFine(decoder, current, units);
	}
	if (type == AT9_BLOCK_STEREO)
		applyIntensityStereo(&params, channels);
	for (channel = 0; channel < count; channel++)
		scaleSpectrum(&params, &channels[channel]);
	block->quantUnitsPrevious = block->quantUnits;
	if (block->hasExtension)
		block->quantUnitsPrevious = block->extensionUnits;
	at9Align(&decoder->reader);
	return error;
}
