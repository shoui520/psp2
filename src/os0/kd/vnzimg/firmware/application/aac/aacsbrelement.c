/*
 * An SBR decoder and its channels: creation and resets (0x851ce8-0x851e3c,
 * 0x85223e-0x8522c4, 0x8523e6-0x85242a, 0x8540dc-0x8541ce).
 */
#include "aac.h"

static AacSbrPrevious *previousFrame(AacDecoder *decoder, int32_t n)
{
	return (AacSbrPrevious *)(decoder->sbrPrevious + n * 240);
}

/* the adjuster's smoothing starts over */
void aacSbrAdjustReset(AacSbrAdjust *adjust)
{
	int32_t i;

	adjust->noiseIndex = 0;
	adjust->noiseExponent = 0;
	for (i = 0; i < 48; i++)
		adjust->history[1][i] = 0;
	adjust->first = 1;
}

/* the element's first channel also builds the band tables */
static int32_t adjustInit(AacSbrAdjust *adjust, AacSbrHeader *header, int32_t index,
	int32_t channel, int32_t *work, AacDecoder *decoder)
{
	int32_t n = index + channel, i;

	for (i = 0; i < 3; i++)
		adjust->sineMask[i] = 0;
	adjust->phaseIndex = 0;
	adjust->history[0] = (int32_t *)decoder->channelBuffer[AAC_BUFFER_ENVELOPE][n];
	adjust->history[1] = (int32_t *)decoder->channelBuffer[AAC_BUFFER_ENVELOPE + 1][n];
	adjust->history[2] = (int32_t *)decoder->channelBuffer[AAC_BUFFER_ENVELOPE + 2][n];
	adjust->previousTransient = -1;
	aacSbrAdjustReset(adjust);
	if (index)
		return 0;
	return aacSbrResetBands(header, work);
}

static void previousReset(AacSbrPrevious *previous, uint8_t timeSlots)
{
	int32_t i;

	for (i = 0; i < 48; i++)
		previous->envelope[i] = 0;
	for (i = 0; i < 5; i++)
		previous->noise[i] = 0;
	for (i = 0; i < 5; i++)
		previous->invfMode[i] = 0;
	previous->lastBorder = timeSlots;
	previous->ampResolution = 0;
	previous->coupling = 0;
}

/* the index'th channel of the SBR decoder of `channel` */
static int32_t elementInit(AacSbrElement *element, AacSbrHeader *header, uint32_t downsampled,
	uint8_t created, int32_t index, int32_t channel, int32_t *work, AacDecoder *decoder)
{
	AacSbrState *state = &element->state;
	AacSbrBands *bands;
	int32_t timeSlots = header->timeSlots;
	int32_t numSlots = header->timeStep * header->timeSlots;
	int32_t n = index + channel, i;

	for (i = 1; i <= 5; i++)
		state->timing[i] = 27;
	element->previous = previousFrame(decoder, n);
	if (adjustInit(&state->adjust, header, index, channel, work, decoder))
		return -1;

	bands = header->bands;
	aacSbrAnalysisInit(&state->analysis, state->timing, numSlots, bands->kx, bands->stop,
		header->status, index, downsampled, channel, decoder);
	aacSbrSynthesisInit(&state->synthesis, state->timing, numSlots, bands->kx, bands->stop,
		header->status, created, index, downsampled, channel, decoder);
	previousReset(element->previous, timeSlots);

	bands = header->bands;
	if (aacSbrHfInit(&state->hf, bands->kx, bands->master, bands->numMaster,
	    state->synthesis.stop, timeSlots, state->analysis.numSlots, bands->noise,
	    bands->numNoise, header->outputRate, index, channel, decoder))
		return -1;

	state->history = (int32_t *)decoder->channelBuffer[AAC_BUFFER_HISTORY][n];
	aacSbrFill((int8_t *)state->history, 0, 3072);
	return 0;
}

/*
 * The SBR decoder of the element at `channel`, its `channels` channels
 * set up for the core's rate; NULL on an error.
 */
AacSbr *aacSbrCreate(uint32_t rate, uint32_t samples, uint32_t downsampled,
	uint32_t upsampling, int32_t *state, int32_t channels, int32_t channel, int32_t *work,
	AacDecoder *decoder)
{
	AacSbr *sbr = &decoder->sbrData[channel];
	int32_t i;

	sbr->numChannels = channels;
	sbr->header.bands = &decoder->sbrBands[channel];
	aacSbrSetRate(&sbr->header, rate, samples);
	for (i = 0; i < (int32_t)sbr->numChannels; i++)
		if (elementInit(&sbr->element[i], &sbr->header, downsampled, upsampling, i,
		    channel, work, decoder))
			return NULL;
	if ((int32_t)sbr->numChannels >= 2 &&
	    aacPsInit(&sbr->ps, sbr->element[1].state.history, state,
	    sbr->element[1].state.analysis.analysisBuffer))
		return NULL;

	sbr->work[1] = decoder->sbrWork[0];
	if (channels == 1)
		sbr->work[0] = decoder->sbrWork[0];
	sbr->work[2] = decoder->sbrWork[1];
	return sbr;
}

/* every channel resets for new bands; nonzero if one could not */
static int32_t resetChannels(AacSbr *sbr, AacSbrHeader *header, int32_t *work, uint8_t created)
{
	int32_t result = 0, i;

	for (i = 0; i < (int32_t)sbr->numChannels; i++)
		result |= aacSbrStateReset(&sbr->element[i].state, header,
			sbr->element[i].previous, work, created);
	return result;
}

/*
 * 0x8541d0: One frame of SBR on the core's output: the channel's payload, if any,
 * is parsed, then each channel is synthesised at the output rate. With
 * parametric stereo a mono element comes out as two channels. Returns 2
 * when the core's output must be used alone.
 */
int32_t aacSbrDecode(AacSbr *sbr, AacSbrChannel *channel, int32_t *output,
	int32_t *channels, uint32_t downsampled, uint32_t parameter, uint32_t one, int32_t *work,
	int32_t pair)
{
	AacSbrHeader *header = &sbr->header;
	AacSbrHeader saved;
	AacSbrBits *bits;
	AacSbrFrame *left, *right;
	int32_t frameSize = header->frameSize;
	uint8_t created = sbr->element[pair].state.synthesis.created;
	int8_t initialSync = header->syncState;
	int32_t wasPair = header->mode == 2, wasPs = header->mode == 4;
	int32_t ok = one, isPair = 0, changed = 0, result;
	uint32_t count;

	bits = (AacSbrBits *)work;
	work += sizeof(*bits) / 4;
	if (sbr->numChannels == 2)
		sbr->work[0] = output + frameSize * 2;
	left = (AacSbrFrame *)sbr->work[0];
	right = (AacSbrFrame *)sbr->work[1];

	if (!channel) {
		if (header->syncState == 2) {
			if (ok)
				header->syncState = 1;
			else
				header->error = 1;
		}
		isPair = *channels == 2;
	} else {
		header->previousError = header->error;
		if (channel->element == 1)
			isPair = 1;
		else if (channel->element != 0)
			ok = 0;
		aacSbrBitsInit(bits, channel->data, channel->present * 8);
		aacSbrReadBits(bits, 4);
		if (!ok)
			goto error;

		if (channel->extensionType == EXT_SBR_DATA_CRC) {
			count = channel->present * 8 - 14;
			ok = (int32_t)count >= 0;
			if (ok)
				ok = aacSbrCrcCheck(bits, count, work);
		}
		result = aacSbrReadBits(bits, 1);	/* bs_header_flag */
		if (!ok)
			goto error;
		if (result) {
			aacSbrCopy((int8_t *)&saved, (const int8_t *)header, 32);
			result = aacSbrHeaderParse(header, bits, (uint8_t)channel->element,
				(AacSbrHeader *)work);
			if (result == 3)
				return 2;
			if (result == 1) {
				changed = aacSbrResetBands(header, work);
				if (changed) {
					/* back to the bands that worked */
					aacSbrCopy((int8_t *)header, (const int8_t *)&saved, 32);
					changed = aacSbrResetBands(header, work);
				} else {
					changed = resetChannels(sbr, header, work, created);
				}
				if (!changed)
					header->syncState = 2;
			}
		}

		if (header->syncState == 0) {
			if (changed)
				return 2;
			/* no header yet: the core's band at twice its rate */
			aacSbrSetRate(header, header->outputRate >> 1, frameSize);
			aacSbrResetBands(header, work);
			header->bands->kx = 32;
			header->bands->stop = 32;
			resetChannels(sbr, header, work, created);
			header->syncState = 1;
		}

		if (header->syncState == 2) {
			if (isPair) {
				ok = aacSbrChannelPairElement(header, left, right, bits,
					(uint8_t *)work);
			} else {
				sbr->ps.disabled = parameter != 0;
				ok = aacSbrSingleChannelElement(header, left, &sbr->ps, bits,
					(uint8_t *)work);
			}
			if (bits->remaining < 0 || bits->remaining >= 8)
				ok = 0;
		}
		if (!ok) {
error:
			header->error = 1;
		}
	}

	if (header->syncState == 0 || (!initialSync && !ok)) {
		header->syncState = 0;
		return 2;
	}
	if (!ok || header->syncState != 2 || header->error) {
		isPair = *channels == 2;
		if (isPair)
			header->mode = 2;
		else
			left->coupling = 0;
	} else if (!isPair) {
		left->coupling = 0;
	}
	if (isPair && sbr->numChannels == 1)
		return 2;
	if (header->mode == 4 && sbr->numChannels == 1)
		return 2;

	if (header->syncState == 2) {
		aacSbrDecodeData(header, left, sbr->element[0].previous, isPair ? right : NULL,
			isPair ? sbr->element[1].previous : NULL, work);
		if (header->mode == 4)
			aacPsDecode(&sbr->ps);
	}
	if (!initialSync && header->error) {
		header->syncState = 0;
		return 2;
	}

	/* from parametric stereo back to a pair: the right synthesis carries on from the left */
	if (sbr->numChannels == 2 && !wasPair && !wasPs && isPair) {
		AacSbrQmf *qmf = &sbr->element[0].state.synthesis;

		aacSbrCopy((int8_t *)sbr->element[1].state.synthesis.synthesisBuffer,
			(const int8_t *)qmf->synthesisBuffer, (qmf->bufferSize - qmf->numBands) * 4);
	}

	aacSbrChannelDecode(&sbr->element[0].state, output, sbr->work[0], sbr->work[1],
		sbr->work[2], header, left, sbr->element[0].previous, header->syncState == 2,
		&sbr->ps, &sbr->element[1].state.synthesis, sbr->element[1].state.timing,
		downsampled, work, created);

	if (!parameter && isPair) {
		aacSbrCopy((int8_t *)output, (const int8_t *)sbr->work[0], 4096);
		aacSbrCopy((int8_t *)left, (const int8_t *)right, sizeof(*left));
		aacSbrCopy((int8_t *)sbr->work[1], (const int8_t *)(sbr->work[0] + frameSize), 4096);
		aacSbrChannelDecode(&sbr->element[1].state, output + frameSize, sbr->work[0],
			sbr->work[1], sbr->work[2], header, left, sbr->element[1].previous,
			header->syncState == 2, NULL, NULL, NULL, downsampled, work, created);
		aacSbrCopy((int8_t *)(output + frameSize), (const int8_t *)sbr->work[1], 4096);
		return 0;
	}
	if (header->mode == 4) {
		*channels = 2;
		return 0;
	}
	aacSbrCopy((int8_t *)output, (const int8_t *)sbr->work[0], 8192);
	return 0;
}
