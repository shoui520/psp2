/*
 * The AAC stream's syntax (0x84e888-0x84fa62): the bit reader, the ADTS
 * header and the elements of a raw data block, each recording where it
 * starts and ends for the CRC.
 */
#include "aac.h"

/* the overlap of count channels, cleared */
void aacOverlapClear(AacBitstream *bits, uint32_t count)
{
	uint32_t ch, i;

	for (ch = 0; ch < count; ch++) {
		for (i = 0; i < AAC_OVERLAP_WORDS; i++)
			bits->overlap[ch][i] = 0;
		bits->windowShape[ch] = 0;
		bits->windowSequence[ch] = 0;
	}
}

/* to a recorded place; the byte after it is the next one fetched */
int32_t aacSeek(AacBitstream *bits, const AacBitPosition *position)
{
	bits->position = *position;
	bits->cursor = bits->buffer + bits->position.byte;
	bits->bytesRead = bits->position.byte;
	if (bits->end < bits->position.byte)
		return -1;
	bits->bytesRead = bits->position.byte + 1;
	return 0;
}

/* count bits (up to 16) */
int32_t aacReadBits(AacBitstream *bits, uint16_t *value, uint32_t count)
{
	int32_t left = bits->position.bits;
	const uint8_t *buffer = bits->buffer;
	uint32_t window = buffer[bits->position.byte];

	*value = 0;
	while (!((int32_t)count < left)) {
		if (!(bits->position.byte < bits->end))
			return -1;
		left += 8;
		bits->cursor++;
		bits->position.byte++;
		window = (window << 8) + buffer[bits->position.byte];
	}
	left -= count;
	*value = (window >> left) & aacBitMasks[count];
	bits->position.bits = left;
	return 0;
}

/* count bits (up to 8); one more byte at most */
int32_t aacReadByte(AacBitstream *bits, uint8_t *value, uint32_t count)
{
	int32_t left = bits->position.bits;
	const uint8_t *buffer = bits->buffer;
	uint16_t window = buffer[bits->position.byte];

	*value = 0;
	if (!((int32_t)count < left)) {
		if (!(bits->position.byte < bits->end))
			return -1;
		left += 8;
		bits->cursor++;
		bits->position.byte++;
		window = (window << 8) + buffer[bits->position.byte];
	}
	left -= count;
	*value = (window >> left) & (uint16_t)aacBitMasks[count];
	bits->position.bits = left;
	return 0;
}

/* a codeword of the book, a bit at a time down its tree */
int32_t aacReadHuffman(AacBitstream *bits, uint16_t *value, uint32_t book)
{
	const uint8_t *buffer = bits->buffer;
	uint32_t current = buffer[bits->position.byte];
	int32_t left = bits->position.bits;
	const uint16_t *tree;
	uint16_t node = 0, entry;

	if (left < 1) {
		if (!(bits->position.byte < bits->end))
			return -1;
		bits->cursor++;
		bits->position.byte++;
		left = 8;
	}
	tree = bits->tables->codebooks[book].tree;
	do {
		left--;
		node += (current >> left) & 1;
		if (left < 1) {
			if (!(bits->position.byte < bits->end))
				return -1;
			bits->cursor++;
			bits->position.byte++;
			left = 8;
			current = buffer[bits->position.byte];
		}
		entry = tree[node];
		node = entry;
	} while (!(entry & 0x8000));
	*value = entry & 0x7fff;
	bits->position.bits = left;
	return 0;
}

/* to the next byte; at the end the bits run out */
int32_t aacByteAlign(AacBitstream *bits)
{
	uint8_t skipped;

	if (bits->position.bits != 8 && aacReadByte(bits, &skipped, bits->position.bits))
		bits->position.bits = 0;
	return 0;
}

#define READ(field, count) \
	do { if (aacReadByte(bits, &(field), count)) return AAC_STATUS_UNDERFLOW; } while (0)
#define READ16(field, count) \
	do { if (aacReadBits(bits, &(field), count)) return AAC_STATUS_UNDERFLOW; } while (0)

int32_t aacAdtsHeader(AacBitstream *bits, AdtsHeader *header)
{
	header->start = bits->position;
	READ16(header->syncword, 12);
	READ(header->id, 1);
	READ(header->layer, 2);
	READ(header->protectionAbsent, 1);
	READ(header->profile, 2);
	READ(header->samplingIndex, 4);
	READ(header->privateBit, 1);
	READ(header->channelConfiguration, 3);
	READ(header->original, 1);
	READ(header->home, 1);
	READ(header->copyrightIdBit, 1);
	READ(header->copyrightIdStart, 1);
	READ16(header->frameLength, 13);
	READ16(header->bufferFullness, 11);
	READ(header->rawDataBlocks, 2);
	if (!header->protectionAbsent)
		READ16(header->crc, 16);
	bits->samplingIndex = header->samplingIndex;
	if (bits->samplingIndex < 3 || !(bits->samplingIndex < 12))
		return AAC_STATUS_SAMPLING_RATE;
	return 0;
}

int32_t aacProgramConfig(AacBitstream *bits, AacProgramConfig *pce)
{
	uint8_t i;

	pce->start = bits->position;
	READ(pce->elementInstanceTag, 4);
	READ(pce->objectType, 2);
	READ(pce->samplingIndex, 4);
	READ(pce->numFrontChannelElements, 4);
	READ(pce->numSideChannelElements, 4);
	READ(pce->numBackChannelElements, 4);
	READ(pce->numLfeChannelElements, 2);
	READ(pce->numAssocDataElements, 3);
	READ(pce->numValidCcElements, 4);
	READ(pce->monoMixdownPresent, 1);
	if (pce->monoMixdownPresent)
		READ(pce->monoMixdownElementNumber, 4);
	READ(pce->stereoMixdownPresent, 1);
	if (pce->stereoMixdownPresent)
		READ(pce->stereoMixdownElementNumber, 4);
	READ(pce->matrixMixdownIdxPresent, 1);
	if (pce->matrixMixdownIdxPresent) {
		READ(pce->matrixMixdownIdx, 2);
		READ(pce->pseudoSurroundEnable, 1);
	}
	for (i = 0; i < pce->numFrontChannelElements; i++) {
		READ(pce->frontElementIsCpe[i], 1);
		READ(pce->frontElementTagSelect[i], 4);
	}
	for (i = 0; i < pce->numSideChannelElements; i++) {
		READ(pce->sideElementIsCpe[i], 1);
		READ(pce->sideElementTagSelect[i], 4);
	}
	for (i = 0; i < pce->numBackChannelElements; i++) {
		READ(pce->backElementIsCpe[i], 1);
		READ(pce->backElementTagSelect[i], 4);
	}
	for (i = 0; i < pce->numLfeChannelElements; i++)
		READ(pce->lfeElementTagSelect[i], 4);
	for (i = 0; i < pce->numAssocDataElements; i++)
		READ(pce->assocDataElementTagSelect[i], 4);
	for (i = 0; i < pce->numValidCcElements; i++) {
		READ(pce->ccElementIsIndSw[i], 1);
		READ(pce->validCcElementTagSelect[i], 4);
	}
	if (aacByteAlign(bits))
		return AAC_STATUS_UNDERFLOW;
	READ(pce->commentFieldBytes, 8);
	for (i = 0; i < pce->commentFieldBytes; i++)
		READ(pce->comment[i], 8);
	pce->comment[i] = 0;
	pce->end = bits->position;
	return 0;
}

/* data_stream_element: the bytes are skipped */
int32_t aacDataStreamElement(AacBitstream *bits, AacDataStreamElement *dse)
{
	uint8_t escape, skipped;
	int32_t i;

	dse->start = bits->position;
	READ(dse->elementInstanceTag, 4);
	READ(dse->dataByteAlignFlag, 1);
	READ16(dse->count, 8);
	if (dse->count == 255) {
		READ(escape, 8);
		dse->count += escape;
	}
	if (dse->dataByteAlignFlag && aacByteAlign(bits))
		return AAC_STATUS_UNDERFLOW;
	for (i = 0; i < dse->count; i++)
		READ(skipped, 8);
	dse->end = bits->position;
	return 0;
}

int32_t aacSingleChannelElement(AacBitstream *bits, AacSingleChannelElement *sce)
{
	const AacIcs *ics;
	const uint8_t *cb;
	int32_t i, count;

	aacIcsReset(sce->ics);
	sce->start = bits->position;
	READ(sce->elementInstanceTag, 4);
	i = aacIndividualChannelStream(bits, sce->ics, 0);
	if (i)
		return i;
	ics = sce->ics;
	count = ics->info.maxSfb * ics->info.numWindowGroups;
	for (i = 0, cb = ics->section.sfbCb; i < count; i++, cb++) {
		if (*cb == AAC_INTENSITY_HCB || *cb == AAC_INTENSITY_HCB2)
			return AAC_STATUS_SCE_INTENSITY;
	}
	sce->end = bits->position;
	return 0;
}

/* lfe_channel_element: the same syntax */
int32_t aacLfeChannelElement(AacBitstream *bits, AacSingleChannelElement *lfe)
{
	return aacSingleChannelElement(bits, lfe);
}

int32_t aacChannelPairElement(AacBitstream *bits, AacChannelPairElement *cpe)
{
	AacIcs *left = cpe->ics[0], *right;
	int32_t result, i, g, sfb, maxSfb, groups;

	aacIcsReset(left);
	cpe->start = bits->position;
	READ(cpe->elementInstanceTag, 4);
	READ(cpe->commonWindow, 1);
	if (cpe->commonWindow) {
		result = aacIcsInfo(bits, &cpe->ics[0]->info);
		if (result)
			return result;
		READ(cpe->msMaskPresent, 2);
		if (cpe->msMaskPresent == 3)
			return AAC_STATUS_MS_RESERVED;
		if (cpe->msMaskPresent == 1) {
			i = 0;
			maxSfb = cpe->ics[0]->info.maxSfb;
			groups = cpe->ics[0]->info.numWindowGroups;
			for (g = 0; g < groups; g++) {
				for (sfb = 0; sfb < maxSfb; sfb++)
					READ(cpe->msUsed[i++], 1);
			}
		}
	} else {
		cpe->msMaskPresent = 0;
	}
	cpe->ics[0]->start = bits->position;
	result = aacIndividualChannelStream(bits, cpe->ics[0], cpe->commonWindow);
	if (result)
		return result;
	aacIcsReset(cpe->ics[1]);
	if (cpe->commonWindow)
		cpe->ics[1]->info = cpe->ics[0]->info;
	aacSpectrumCopy(cpe);
	right = cpe->ics[1];
	right->start = bits->position;
	result = aacIndividualChannelStream(bits, right, cpe->commonWindow);
	if (result)
		return result;
	cpe->end = bits->position;
	return 0;
}

int32_t aacCouplingChannelElement(AacBitstream *bits, AacCouplingChannelElement *cce)
{
	int32_t gainLists, c, g, sfb, result;
	uint8_t *present;
	uint16_t skipped;

	aacIcsReset(cce->ics);
	cce->start = bits->position;
	READ(cce->elementInstanceTag, 4);
	READ(cce->indSwCce, 1);
	READ(cce->numCoupledElements, 3);
	gainLists = 0;
	for (c = 0; c <= cce->numCoupledElements; c++) {
		gainLists++;
		READ(cce->ccTargetIsCpe[c], 1);
		READ(cce->ccTargetTagSelect[c], 4);
		if (cce->ccTargetIsCpe[c]) {
			READ(cce->ccLR[c], 2);
			if (cce->ccLR[c] == 3)
				gainLists++;
		}
	}
	READ(cce->ccDomain, 1);
	READ(cce->gainElementSign, 1);
	READ(cce->gainElementScale, 2);
	cce->ics->start = bits->position;
	result = aacIndividualChannelStream(bits, cce->ics, 0);
	if (result)
		return result;
	for (c = 0; c < gainLists - 1; c++) {
		present = &cce->commonGainElementPresent[c];
		if (cce->indSwCce)
			*present = 1;
		else
			READ(*present, 1);
		if (*present) {
			if (aacReadHuffman(bits, &cce->commonGain[c], 0))
				return AAC_STATUS_UNDERFLOW;
			continue;
		}
		/* the band gains are skipped */
		for (g = 0; g < cce->ics->info.numWindowGroups; g++) {
			for (sfb = 0; sfb < cce->ics->info.maxSfb; sfb++) {
				if (cce->ics->section.sfbCb[sfb] &&
				    aacReadHuffman(bits, &skipped, 0))
					return AAC_STATUS_UNDERFLOW;
			}
		}
	}
	cce->end = bits->position;
	return 0;
}

/* individual_channel_stream; the reader is at its global_gain */
int32_t aacIndividualChannelStream(AacBitstream *bits, AacIcs *ics, uint32_t commonWindow)
{
	int32_t result;

	ics->start = bits->position;
	if (aacReadByte(bits, &ics->globalGain, 8))
		return AAC_STATUS_UNDERFLOW;
	if (!commonWindow) {
		result = aacIcsInfo(bits, &ics->info);
		if (result)
			return result;
	}
	result = aacSectionData(bits, &ics->section, &ics->info);
	if (result)
		return result;
	ics->scaleFactorDataStart = bits->position;
	result = aacScaleFactorData(bits, ics);
	if (result)
		return result;
	READ(ics->pulseDataPresent, 1);
	if (ics->pulseDataPresent) {
		result = aacPulseData(bits, ics);
		if (result)
			return result;
	}
	READ(ics->tnsDataPresent, 1);
	if (ics->tnsDataPresent) {
		result = aacTnsData(bits, &ics->tns, ics->info.windowSequence);
		if (result)
			return result;
		aacTnsDecode(bits, &ics->tns, ics->info.windowSequence);
	}
	READ(ics->gainControlDataPresent, 1);
	if (ics->gainControlDataPresent)
		return AAC_STATUS_GAIN_CONTROL;
	ics->spectralDataStart = bits->position;
	return aacSpectralData(bits, ics);
}

int32_t aacIcsInfo(AacBitstream *bits, AacIcsInfo *info)
{
	uint32_t grouping, length, i, sfb, count;

	info->start = bits->position;
	READ(info->reservedBit, 1);
	READ(info->windowSequence, 2);
	READ(info->windowShape, 1);
	if (info->windowSequence == AAC_EIGHT_SHORT_SEQUENCE) {
		READ(info->maxSfb, 4);
		READ(info->scaleFactorGrouping, 7);
		/* a group ends where the next window's bit is clear */
		info->numWindowGroups = 0;
		length = 0;
		grouping = info->scaleFactorGrouping << 1;
		for (i = 0; i < 8; i++) {
			if ((0x80 >> i) & grouping) {
				length = (uint8_t)(length + 1);
			} else {
				info->windowGroupLength[info->numWindowGroups] = length + 1;
				length = 0;
				info->numWindowGroups++;
			}
		}
		return 0;
	}
	READ(info->maxSfb, 6);
	READ(info->predictorDataPresent, 1);
	if (info->predictorDataPresent) {
		READ(info->predictorReset, 1);
		if (info->predictorReset)
			READ(info->predictorResetGroupNumber, 5);
		count = info->maxSfb;
		if (count > bits->tables->predSfbMax[bits->samplingIndex])
			count = bits->tables->predSfbMax[bits->samplingIndex];
		for (sfb = 0; (int32_t)sfb < (int32_t)count; sfb++)
			READ(info->predictionUsed[sfb], 1);
	}
	info->windowGroupLength[0] = 1;
	info->numWindowGroups = 1;
	return 0;
}

int32_t aacSectionData(AacBitstream *bits, AacSectionData *section, const AacIcsInfo *info)
{
	uint32_t sectBits = info->windowSequence == AAC_EIGHT_SHORT_SEQUENCE ? 3 : 5;
	uint8_t escape = (1 << sectBits) - 1, increment;
	int32_t g, k, i = 0, band = 0, start, end, sfb;
	uint8_t *cb, *length;

	section->start = bits->position;
	for (g = 0; g < info->numWindowGroups; g++) {
		for (k = 0; k < info->maxSfb; i++) {
			cb = &section->sectCb[i];
			READ(*cb, 4);
			length = &section->sectLen[i];
			*length = 0;
			if (*cb == AAC_RESERVED_HCB)
				return AAC_STATUS_CODEBOOK;
			if (*cb == AAC_NOISE_HCB && bits->id == 1)
				return AAC_STATUS_CODEBOOK;
			do {
				READ(increment, sectBits);
				*length += increment;
			} while (increment == escape);
			start = k;
			k += *length;
			end = k < info->maxSfb ? k : info->maxSfb;
			if (info->maxSfb < k)
				return AAC_STATUS_SECTION;
			for (sfb = start; sfb < end; sfb++)
				section->sfbCb[band++] = *cb;
		}
	}
	return 0;
}

/*
 * The scale factors, the intensity positions and the noise energies, each
 * differential; the first noise energy is 9 bits.
 */
int32_t aacScaleFactorData(AacBitstream *bits, AacIcs *ics)
{
	int32_t scaleFactor = ics->globalGain, noiseEnergy = ics->globalGain, position = 0;
	int32_t noisePcm = 0, g, sfb, k = 0, value;
	uint16_t code;
	uint8_t cb;

	ics->scaleFactorStart = bits->position;
	for (g = 0; g < ics->info.numWindowGroups; g++) {
		for (sfb = 0; sfb < ics->info.maxSfb; sfb++, k++) {
			cb = ics->section.sfbCb[k];
			if (cb == AAC_ZERO_HCB) {
				value = 0;
			} else if (cb == AAC_NOISE_HCB) {
				if (!noisePcm) {
					if (aacReadBits(bits, &code, 9))
						return AAC_STATUS_UNDERFLOW;
					noisePcm = 1;
					noiseEnergy += code - 256;
				} else {
					if (aacReadHuffman(bits, &code, 0))
						return AAC_STATUS_UNDERFLOW;
					noiseEnergy += code - 60;
				}
				value = noiseEnergy;
			} else {
				if (aacReadHuffman(bits, &code, 0))
					return AAC_STATUS_UNDERFLOW;
				if (ics->section.sfbCb[k] == AAC_INTENSITY_HCB ||
				    ics->section.sfbCb[k] == AAC_INTENSITY_HCB2) {
					position += code - 60;
					value = position;
				} else {
					scaleFactor += code - 60;
					value = scaleFactor;
				}
			}
			ics->scaleFactors[k] = value;
		}
	}
	return 0;
}

int32_t aacPulseData(AacBitstream *bits, AacIcs *ics)
{
	AacPulseData *pulse = &ics->pulse;
	int32_t i;

	pulse->start = bits->position;
	READ(pulse->numberPulse, 2);
	READ(pulse->pulseStartSfb, 6);
	for (i = 0; i <= pulse->numberPulse; i++) {
		if (ics->info.maxSfb < pulse->pulseStartSfb)
			return AAC_STATUS_PULSE_START;
		READ(pulse->pulseOffset[i], 5);
		READ(pulse->pulseAmp[i], 4);
	}
	return 0;
}

int32_t aacTnsData(AacBitstream *bits, AacTnsData *tns, uint32_t windowSequence)
{
	uint8_t nFiltBits, lengthBits, orderBits, coefBits;
	uint8_t *order = tns->order, *compress;
	int32_t windows, w, f, i, filter = 0, coef = 0;

	tns->start = bits->position;
	if (windowSequence == AAC_EIGHT_SHORT_SEQUENCE) {
		windows = 8;
		nFiltBits = 1;
		lengthBits = 4;
		orderBits = 3;
	} else {
		windows = 1;
		nFiltBits = 2;
		lengthBits = 6;
		orderBits = 5;
	}
	for (w = 0; w < windows; w++) {
		READ(tns->nFilt[w], nFiltBits);
		if (tns->nFilt[w])
			READ(tns->coefRes[w], 1);
		for (f = 0; f < tns->nFilt[w]; f++, filter++, order++) {
			READ(tns->length[filter], lengthBits);
			READ(*order, orderBits);
			compress = &tns->coefCompress[filter];
			if (*order > AAC_TNS_MAX_ORDER)
				return AAC_STATUS_TNS_ORDER;
			if (*order) {
				READ(tns->direction[filter], 1);
				READ(*compress, 1);
			}
			coefBits = tns->coefRes[w] - *compress + 3;
			for (i = 0; i < *order; i++)
				READ(tns->coef[coef++], coefBits);
		}
	}
	return 0;
}

/* fill_element: the payload's bytes kept */
int32_t aacFillElement(AacBitstream *bits, AacFillElement *fill)
{
	uint8_t escape;
	int32_t i, count;

	fill->start = bits->position;
	READ16(fill->count, 4);
	if (fill->count == 15) {
		READ(escape, 8);
		fill->count += escape - 1;
	}
	count = fill->count;
	for (i = 0; i < count; i++)
		READ(fill->extensionPayload[i], 8);
	return 0;
}

/* what this decoder leaves out of the header: Main, SSR, LTP, long frames */
int32_t aacAdtsCheck(const AdtsHeader *header)
{
	if (header->layer)
		return AAC_STATUS_LAYER;
	switch (header->profile) {
	case 0:
		return AAC_STATUS_PROFILE_MAIN;
	case 2:
		return AAC_STATUS_PROFILE_SSR;
	case 3:
		return AAC_STATUS_PROFILE_LTP;
	}
	if (header->samplingIndex < 3 || !(header->samplingIndex < 12))
		return AAC_STATUS_SAMPLING_RATE;
	if (!(header->frameLength < 3927))
		return AAC_STATUS_FRAME_LENGTH;
	return 0;
}

/* what an ICS may not use: more bands than the rate has, prediction, gain control */
int32_t aacIcsCheck(const AacBitstream *bits, const AacIcs *ics)
{
	uint32_t isShort = ics->info.windowSequence == AAC_EIGHT_SHORT_SEQUENCE;

	if (bits->tables->numSwb[isShort][bits->samplingIndex] < ics->info.maxSfb)
		return AAC_STATUS_MAX_SFB;
	if (ics->info.predictorDataPresent)
		return AAC_STATUS_PREDICTION;
	if (ics->gainControlDataPresent)
		return AAC_STATUS_GAIN_CONTROL;
	if (ics->pulseDataPresent && ics->info.windowSequence == AAC_EIGHT_SHORT_SEQUENCE)
		return AAC_STATUS_SHORT_PULSE;
	return 0;
}

int32_t aacSceCheck(const AacBitstream *bits, const AacSingleChannelElement *sce)
{
	return aacIcsCheck(bits, sce->ics);
}

int32_t aacCpeCheck(const AacBitstream *bits, const AacChannelPairElement *cpe)
{
	int32_t result = aacIcsCheck(bits, cpe->ics[0]);

	if (result)
		return result;
	return aacIcsCheck(bits, cpe->ics[1]);
}

/* the left channel's spectrum, to the right's */
void aacSpectrumCopy(AacChannelPairElement *cpe)
{
	int32_t i;

	for (i = 0; i < AAC_FRAME_SAMPLES; i++)
		cpe->ics[1]->spectrum[i] = cpe->ics[0]->spectrum[i];
}

/* everything but the spectrum's place cleared, and the spectrum */
int32_t aacIcsReset(AacIcs *ics)
{
	int32_t *spectrum = ics->spectrum;
	uint8_t *byte = (uint8_t *)ics;
	uint32_t i;

	for (i = 0; i < sizeof(*ics); i++)
		byte[i] = 0;
	ics->spectrum = spectrum;
	for (i = 0; i < AAC_FRAME_SAMPLES; i++)
		spectrum[i] = 0;
	return 0;
}
