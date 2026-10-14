/*
 * Spectral band replication: the header, its defaults and the per-block
 * flags, and the channel elements' frame data (0x851904-0x85191e,
 * 0x852390-0x853076).
 */
#include "aac.h"

static const AacSbrHeader defaultHeader = {
	.timeSlots = 16,
	.timeStep = 2,
	.frameSize = AAC_FRAME_SAMPLES,
	.outputRate = 44100,
	.status = 2,
	.ampResolution = 1,
	.startFreq = 10,
	.stopFreq = 10,
	.freqScale = 2,
	.alterScale = 1,
	.noiseBands = 2,
	.limiterBands = 2,
	.limiterGains = 2,
	.interpolFreq = 1,
	.smoothingMode = 1,
	.reserved4 = { 1, 2, 0xc0, 0xbf },
};

int32_t aacSbrCheck(AacSbrHeader *header, uint32_t what)
{
	return header->mode == what;
}

void aacSbrChannelReset(AacSbrChannel *channel)
{
	channel->present = 0;
}

int32_t aacSbrPresent(AacSbrChannel *channel)
{
	return channel->present;
}

uint32_t aacSbrRate(AacSbrHeader *header)
{
	return header->outputRate;
}

#define SBR_MAX_PAYLOAD		269

/*
 * 0x8520ba: extension_payload() in a fill element: SBR data following an SCE or a
 * CPE is kept for the SBR decoder; anything else is skipped.
 */
int32_t aacSbrExtension(AacBitstream *bits, AacSbrChannel *channel, uint32_t previous)
{
	uint16_t count = 0, escape = 0;
	uint8_t type = 0, byte = 0;
	int32_t i;

	aacReadBits(bits, &count, 4);
	if (count == 15) {
		aacReadBits(bits, &escape, 8);
		count = escape + 14;
	}
	if (!count)
		return 0;

	aacReadByte(bits, &type, 4);
	if ((previous == AAC_ID_SCE || previous == AAC_ID_CPE) &&
	    (type == EXT_SBR_DATA || type == EXT_SBR_DATA_CRC) && count < SBR_MAX_PAYLOAD) {
		aacReadByte(bits, &byte, 4);
		channel->data[0] = byte;
		for (i = 1; i < count; i++) {
			aacReadByte(bits, &byte, 8);
			channel->data[i] = byte;
		}
		channel->element = previous;
		channel->extensionType = type;
		channel->present = count;
		return 0;
	}
	aacReadByte(bits, &byte, 4);
	for (i = 1; i < count; i++)
		aacReadByte(bits, &byte, 8);
	return 0;
}

/* the defaults, at a core rate and frame size */
void aacSbrSetRate(AacSbrHeader *header, uint32_t rate, uint32_t samples)
{
	AacSbrBands *bands = header->bands;

	*header = defaultHeader;
	bands->table[1] = bands->high;
	bands->table[0] = bands->low;
	header->outputRate = rate * 2;
	header->status = 2;
	header->bands = bands;
	header->frameSize = samples;
	header->timeSlots = samples >> (header->timeStep + 4);
}

/*
 * sbr_header() from bs_amp_res on; the previous header is kept in
 * `previous`. Returns whether the frequency bands changed.
 */
int32_t aacSbrHeaderParse(AacSbrHeader *header, AacSbrBits *bits, uint8_t type,
	AacSbrHeader *previous)
{
	uint32_t extra1, extra2, value;

	*previous = *header;
	header->ampResolution = aacSbrReadBits(bits, 1);
	header->startFreq = aacSbrReadBits(bits, 4);
	header->stopFreq = aacSbrReadBits(bits, 4);
	header->xoverBand = aacSbrReadBits(bits, 3);
	aacSbrReadBits(bits, 2);		/* bs_reserved */
	extra1 = aacSbrReadBits(bits, 1);
	extra2 = aacSbrReadBits(bits, 1);
	header->mode = type == 1 ? 2 : 1;

	if (extra1) {
		header->freqScale = aacSbrReadBits(bits, 2);
		header->alterScale = aacSbrReadBits(bits, 1);
		header->noiseBands = aacSbrReadBits(bits, 2);
	} else {
		header->freqScale = 2;
		header->alterScale = 1;
		header->noiseBands = 2;
	}
	if (extra2) {
		header->limiterBands = aacSbrReadBits(bits, 2);
		header->limiterGains = aacSbrReadBits(bits, 2);
		header->interpolFreq = aacSbrReadBits(bits, 1);
		value = aacSbrReadBits(bits, 1);
	} else {
		header->limiterBands = 2;
		header->limiterGains = 2;
		header->interpolFreq = 1;
		value = 1;
	}
	header->smoothingMode = value;

	return header->syncState != 2 ||
		header->startFreq != previous->startFreq ||
		header->stopFreq != previous->stopFreq ||
		header->xoverBand != previous->xoverBand ||
		header->freqScale != previous->freqScale ||
		header->alterScale != previous->alterScale ||
		header->noiseBands != previous->noiseBands;
}

/* bs_add_harmonic_flag and bs_add_harmonic[]; returns the bits read */
int32_t aacSbrHarmonics(AacSbrHeader *header, AacSbrFrame *frame, AacSbrBits *bits)
{
	int32_t count = 1, i;

	if (aacSbrReadBits(bits, 1)) {
		for (i = 0; i < header->bands->numHigh; i++) {
			frame->harmonics[i] = aacSbrReadBits(bits, 1);
			count++;
		}
	} else {
		for (i = 0; i < 48; i++)
			frame->harmonics[i] = 0;
	}
	return count;
}

#define EXTENSION_ID_PS	2

/*
 * bs_extended_data: parametric stereo goes to `ps` once per frame, and
 * only in an SCE's stream; anything else is skipped.
 */
static int32_t extendedData(AacSbrHeader *header, AacPs *ps, AacSbrBits *bits)
{
	int32_t count, bytes, parsed = 0, i;

	if (!aacSbrReadBits(bits, 1))
		return 1;

	count = aacSbrReadBits(bits, 4);
	if (count == 15)
		count += aacSbrReadBits(bits, 8);
	count *= 8;

	while (count >= 8) {
		uint32_t id = aacSbrReadBits(bits, 2);

		count -= 2;
		bytes = count >> 3;
		if (id == EXTENSION_ID_PS) {
			if (parsed >= 1)
				return 0;
			if (header->mode != 1 && header->mode != 4)
				return 0;
			if (!ps->disabled) {
				count -= aacPsParse(ps, bits, count);
				parsed++;
				header->mode = 4;
				continue;
			}
		}
		for (i = 0; i < bytes; i++)
			aacSbrReadBits(bits, 8);
		count -= bytes * 8;
	}
	if (count < 0) {
		header->error = 1;
		return 0;
	}
	aacSbrReadBits(bits, count);
	return 1;
}

/* the 18-byte grids of SBR_FIXFIX with 16 time slots, by L_E */
static const AacSbrGrid fixfix1 = {
	.numEnvelopes = 1, .borders = { 0, 16 }, .freqRes = { 1 },
	.pointer = -1, .numNoise = 1, .noiseBorders = { 0, 16 },
};
static const AacSbrGrid fixfix2 = {
	.numEnvelopes = 2, .borders = { 0, 8, 16 }, .freqRes = { 1, 1 },
	.pointer = -1, .numNoise = 2, .noiseBorders = { 0, 8, 16 },
};
static const AacSbrGrid fixfix4 = {
	.numEnvelopes = 4, .borders = { 0, 4, 8, 12, 16 }, .freqRes = { 1, 1, 1, 1 },
	.pointer = -1, .numNoise = 2, .noiseBorders = { 0, 8, 16 },
};

/* bs_pointer: ceil(log2(count + 1)) bits */
static uint32_t readPointer(AacSbrBits *bits, int32_t count)
{
	int64_t value;

	aacFixedFromInt(&value, count, 56);
	return aacSbrReadBits(bits, 27 - aacFixedNorm64(&value));
}

/* bs_rel_bord: 2, 4, 6 or 8 slots */
static int32_t readRelative(AacSbrBits *bits)
{
	return 2 * aacSbrReadBits(bits, 2) + 2;
}

/* sbr_grid() (ISO/IEC 14496-3 4.6.18.3.3) */
static int32_t sbrGrid(AacSbrBits *bits, AacSbrHeader *header, AacSbrFrame *frame)
{
	AacSbrGrid *grid = &frame->grid;
	uint32_t timeSlots = header->timeSlots;
	uint32_t frameClass, freqRes, pointer;
	int32_t numEnvelopes = 0, numRelative = 0, border = 0, absolute, trailing;
	int32_t numLeading, numTrailing, i, n;

	frameClass = aacSbrReadBits(bits, 2);
	grid->frameClass = frameClass;

	switch (frameClass) {
	case SBR_FIXFIX: {
		const AacSbrGrid *preset = NULL;

		numEnvelopes = 1 << aacSbrReadBits(bits, 2);
		freqRes = aacSbrReadBits(bits, 1);
		if (numEnvelopes >= 6)
			return 0;
		if (numEnvelopes == 4)
			preset = &fixfix4;
		else if (numEnvelopes == 2)
			preset = &fixfix2;
		else if (numEnvelopes == 1)
			preset = &fixfix1;
		else
			return 0;
		if (timeSlots == 16)
			aacSbrCopy((int8_t *)grid, (const int8_t *)preset, sizeof(*grid));
		if (!freqRes)
			for (i = 0; i < numEnvelopes; i++)
				grid->freqRes[i] = 0;
		break;
	}

	case SBR_FIXVAR:
		absolute = aacSbrReadBits(bits, 2);
		numRelative = aacSbrReadBits(bits, 2);
		numEnvelopes = numRelative + 1;
		border = absolute + timeSlots;
		n = numEnvelopes;
		grid->borders[0] = 0;
		grid->borders[n] = border;
		for (i = 0; i < numRelative; i++) {
			border -= readRelative(bits);
			grid->borders[--n] = border;
		}
		pointer = readPointer(bits, numRelative + 1);
		if (pointer > (uint32_t)numRelative + 1)
			return 0;
		grid->pointer = pointer ? numRelative + 2 - pointer : -1;
		for (i = numRelative; i >= 0; i--)
			grid->freqRes[i] = aacSbrReadBits(bits, 1);
		if (pointer == 0 || pointer == 1)
			grid->noiseBorders[1] = grid->borders[numRelative];
		else
			grid->noiseBorders[1] = grid->borders[grid->pointer];
		break;

	case SBR_VARFIX:
		border = aacSbrReadBits(bits, 2);
		numRelative = aacSbrReadBits(bits, 2);
		numEnvelopes = numRelative + 1;
		grid->borders[0] = border;
		for (i = 1; i <= numRelative; i++) {
			border += readRelative(bits);
			grid->borders[i] = border;
		}
		grid->borders[i] = timeSlots;
		pointer = readPointer(bits, numRelative + 1);
		if (pointer > (uint32_t)numRelative + 1)
			return 0;
		grid->pointer = pointer == 0 || pointer == 1 ? -1 : (int32_t)pointer - 1;
		for (i = 0; i <= numRelative; i++)
			grid->freqRes[i] = aacSbrReadBits(bits, 1);
		if (pointer == 1)
			grid->noiseBorders[1] = grid->borders[numRelative];
		else if (pointer == 0)
			grid->noiseBorders[1] = grid->borders[1];
		else
			grid->noiseBorders[1] = grid->borders[grid->pointer];
		break;

	case SBR_VARVAR:
		absolute = aacSbrReadBits(bits, 2);
		trailing = aacSbrReadBits(bits, 2) + timeSlots;
		numLeading = aacSbrReadBits(bits, 2);
		numTrailing = aacSbrReadBits(bits, 2);
		numRelative = numLeading + numTrailing;
		numEnvelopes = numRelative + 1;
		if (numEnvelopes >= 6)
			return 0;
		border = absolute;
		grid->borders[0] = border;
		for (i = 1; i <= numLeading; i++) {
			border += readRelative(bits);
			grid->borders[i] = border;
		}
		n = numEnvelopes;
		border = trailing;
		grid->borders[n] = border;
		for (i = 0; i < numTrailing; i++) {
			border -= readRelative(bits);
			grid->borders[--n] = border;
		}
		pointer = readPointer(bits, numRelative + 1);
		if (pointer > (uint32_t)numRelative + 1)
			return 0;
		grid->pointer = pointer ? numRelative + 2 - pointer : -1;
		for (i = 0; i < numEnvelopes; i++)
			grid->freqRes[i] = aacSbrReadBits(bits, 1);
		grid->noiseBorders[0] = absolute;
		if (numEnvelopes == 1) {
			grid->noiseBorders[1] = trailing;
		} else {
			if (pointer == 0 || pointer == 1)
				grid->noiseBorders[1] = grid->borders[numEnvelopes - 1];
			else
				grid->noiseBorders[1] = grid->borders[grid->pointer];
			grid->noiseBorders[2] = trailing;
		}
		break;
	}

	grid->numEnvelopes = numEnvelopes;
	grid->numNoise = (numEnvelopes > 1) + 1;
	if (frameClass == SBR_FIXVAR || frameClass == SBR_VARFIX) {
		grid->noiseBorders[0] = grid->borders[0];
		grid->noiseBorders[grid->numNoise] = grid->borders[numEnvelopes];
	}
	return 1;
}

/* whether a grid is one this decoder can use: 16 time slots, borders in order */
static int32_t gridCheck(const AacSbrGrid *grid, uint32_t timeSlots)
{
	int32_t numEnvelopes = grid->numEnvelopes, numNoise = grid->numNoise;
	int32_t first = grid->borders[0], last = grid->borders[numEnvelopes];
	int32_t i, j;

	if (numEnvelopes < 1 || numEnvelopes >= 6 || numNoise >= 3)
		return 0;
	if (timeSlots != 16)
		return 0;
	if (first < 0 || first >= last || first > 19 - (int32_t)timeSlots)
		return 0;
	if (last < (int32_t)timeSlots || last >= 20)
		return 0;
	for (i = 0; i < numEnvelopes; i++)
		if (grid->borders[i] >= grid->borders[i + 1])
			return 0;
	if (grid->pointer > numEnvelopes)
		return 0;
	if (numEnvelopes == 1 && numNoise >= 2)
		return 0;
	if (first != grid->noiseBorders[0] || last != grid->noiseBorders[numNoise])
		return 0;
	for (i = 0; i < numNoise; i++)
		if (grid->noiseBorders[i] >= grid->noiseBorders[i + 1])
			return 0;
	/* every noise border is an envelope border */
	for (i = 0; i < numNoise; i++) {
		for (j = 0; j < numEnvelopes; j++)
			if (grid->borders[j] == grid->noiseBorders[i])
				break;
		if (j == numEnvelopes)
			return 0;
	}
	return 1;
}

/* sbr_dtdf() */
static void sbrDtdf(AacSbrFrame *frame, AacSbrBits *bits)
{
	int32_t i;

	for (i = 0; i < frame->grid.numEnvelopes; i++)
		frame->dfEnvelope[i] = aacSbrReadBits(bits, 1);
	for (i = 0; i < frame->grid.numNoise; i++)
		frame->dfNoise[i] = aacSbrReadBits(bits, 1);
}

/* sbr_invf() */
static void sbrInvf(AacSbrHeader *header, AacSbrFrame *frame, AacSbrBits *bits)
{
	int32_t i;

	for (i = 0; i < header->bands->noiseBands; i++)
		frame->invfMode[i] = aacSbrReadBits(bits, 2);
}

static int32_t saturate(int32_t value)
{
	int64_t wide = value;

	return aacFixedSaturate(&wide);
}

/*
 * sbr_envelope(): the bands of each envelope go to numBands; at most 240
 * values in all. The balance of a coupled pair's right channel is sent
 * at half the resolution.
 */
static int32_t sbrEnvelope(AacSbrHeader *header, AacSbrFrame *frame, AacSbrBits *bits,
	uint8_t *numBands)
{
	const AacSbrBands *bands = header->bands;
	const int8_t (*time)[2], (*freq)[2];
	int32_t numEnvelopes = frame->grid.numEnvelopes;
	int32_t balance = frame->coupling == 2;
	int32_t ampResolution = header->ampResolution;
	int32_t balanceBits, startBits, shift, offset = 0, i, j;

	frame->numEnvelopeValues = 0;
	if (frame->grid.frameClass == SBR_FIXFIX && numEnvelopes == 1)
		ampResolution = 0;
	frame->ampResolution = ampResolution;
	if (ampResolution == 1) {
		balanceBits = 5;
		startBits = 6;
	} else {
		balanceBits = 6;
		startBits = 7;
	}

	for (i = 0; i < numEnvelopes; i++) {
		numBands[i] = frame->grid.freqRes[i] ? bands->numHigh : bands->numLow;
		frame->numEnvelopeValues += numBands[i];
	}
	if (frame->numEnvelopeValues > 240)
		return 0;

	if (balance) {
		shift = 1;
		if (ampResolution) {
			time = aacSbrBalanceTime30;
			freq = aacSbrBalanceFreq30;
		} else {
			time = aacSbrBalanceTime15;
			freq = aacSbrBalanceFreq15;
		}
	} else {
		shift = 0;
		if (ampResolution) {
			time = aacSbrEnvelopeTime30;
			freq = aacSbrEnvelopeFreq30;
		} else {
			time = aacSbrEnvelopeTime15;
			freq = aacSbrEnvelopeFreq15;
		}
	}

	for (i = 0; i < numEnvelopes; i++) {
		if (!frame->dfEnvelope[i]) {
			uint32_t start = aacSbrReadBits(bits, balance ? balanceBits : startBits);

			frame->envelope[offset] = saturate(start << shift);
		}
		for (j = 1 - frame->dfEnvelope[i]; j < numBands[i]; j++) {
			int32_t delta = aacSbrHuffman(frame->dfEnvelope[i] ? time : freq, bits);

			frame->envelope[offset + j] = saturate(delta << shift);
		}
		offset += numBands[i];
	}
	return 1;
}

/* sbr_noise() */
static void sbrNoise(AacSbrHeader *header, AacSbrFrame *frame, AacSbrBits *bits)
{
	int32_t numNoise = header->bands->numNoise;
	int32_t balance = frame->coupling == 2;
	const int8_t (*time)[2], (*freq)[2];
	int32_t shift, i, j;

	if (balance) {
		shift = 1;
		time = aacSbrNoiseBalanceTime30;
		freq = aacSbrBalanceFreq30;
	} else {
		shift = 0;
		time = aacSbrNoiseTime30;
		freq = aacSbrEnvelopeFreq30;
	}

	for (i = 0; i < frame->grid.numNoise; i++) {
		int32_t *noise = &frame->noise[i * numNoise];

		if (frame->dfNoise[i]) {
			for (j = 0; j < numNoise; j++)
				noise[j] = saturate(aacSbrHuffman(time, bits) << shift);
			continue;
		}
		j = aacSbrReadBits(bits, 5);
		noise[0] = balance ? saturate(j << shift) : j;
		for (j = 1; j < numNoise; j++)
			noise[j] = saturate(aacSbrHuffman(freq, bits) << shift);
	}
}

/*
 * sbr_single_channel_element(); the bands of each envelope go to
 * numBands. Returns 0 on an error in the stream.
 */
int32_t aacSbrSingleChannelElement(AacSbrHeader *header, AacSbrFrame *frame, AacPs *ps,
	AacSbrBits *bits, uint8_t *numBands)
{
	frame->coupling = 0;
	if (aacSbrReadBits(bits, 1))		/* bs_data_extra */
		aacSbrReadBits(bits, 4);	/* bs_reserved */
	if (!sbrGrid(bits, header, frame))
		return 0;
	if (!gridCheck(&frame->grid, header->timeSlots))
		return 0;
	sbrDtdf(frame, bits);
	if (!frame->dfEnvelope[0])
		header->error = 0;
	sbrInvf(header, frame, bits);
	if (!sbrEnvelope(header, frame, bits, numBands))
		return 0;
	sbrNoise(header, frame, bits);
	aacSbrHarmonics(header, frame, bits);
	return extendedData(header, ps, bits) != 0;
}

/* sbr_channel_pair_element() */
int32_t aacSbrChannelPairElement(AacSbrHeader *header, AacSbrFrame *left, AacSbrFrame *right,
	AacSbrBits *bits, uint8_t *numBands)
{
	int32_t i;

	if (aacSbrReadBits(bits, 1)) {		/* bs_data_extra */
		aacSbrReadBits(bits, 4);
		aacSbrReadBits(bits, 4);
	}
	if (header->mode != 2) {
		header->syncState = 1;
		return 0;
	}

	if (aacSbrReadBits(bits, 1)) {		/* bs_coupling */
		left->coupling = 1;
		right->coupling = 2;
	} else {
		left->coupling = 0;
		right->coupling = 0;
	}
	if (!sbrGrid(bits, header, left) || !gridCheck(&left->grid, header->timeSlots))
		return 0;
	if (left->coupling) {
		aacSbrCopy((int8_t *)&right->grid, (const int8_t *)&left->grid,
			sizeof(right->grid));
	} else if (!sbrGrid(bits, header, right) ||
	    !gridCheck(&right->grid, header->timeSlots)) {
		return 0;
	}
	sbrDtdf(left, bits);
	sbrDtdf(right, bits);
	if (!left->dfEnvelope[0] && !right->dfEnvelope[0])
		header->error = 0;
	sbrInvf(header, left, bits);

	if (left->coupling) {
		for (i = 0; i < header->bands->noiseBands; i++)
			right->invfMode[i] = left->invfMode[i];
		if (!sbrEnvelope(header, left, bits, numBands))
			return 0;
		sbrNoise(header, left, bits);
		if (!sbrEnvelope(header, right, bits, numBands))
			return 0;
	} else {
		sbrInvf(header, right, bits);
		if (!sbrEnvelope(header, left, bits, numBands))
			return 0;
		if (!sbrEnvelope(header, right, bits, numBands))
			return 0;
		sbrNoise(header, left, bits);
	}
	sbrNoise(header, right, bits);
	aacSbrHarmonics(header, left, bits);
	aacSbrHarmonics(header, right, bits);
	return extendedData(header, NULL, bits) != 0;
}
