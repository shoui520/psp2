/*
 * A frame (0x84fa64-0x850e0b): the ADTS header, the raw data block's
 * elements, the channel layout and downmix, SBR, and the CRC over what the
 * elements recorded.
 */
#include "aac.h"

#define CRC_POLYNOMIAL		0x8005
#define CRC_HEADER_BITS		56
#define CRC_ELEMENT_BITS	192
#define CRC_RIGHT_BITS		128
#define ADTS_HEADER_BYTES	9
#define SAMPLES_SBR		2048

/* the channel modes (AacStream.channelMode) */
#define MODE_FROM_PCE		0

/*
 * The channels' order out of the elements' for each mode, and the mode
 * of each ADTS channel_configuration.
 */
static const uint32_t channelOrders[11][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 0, 0, 0, 0, 0, 0 },
	{ 1, 2, 0, 0, 0, 0, 0, 0 },
	{ 1, 0, 2, 3, 0, 0, 0, 0 },
	{ 1, 2, 0, 3, 4, 0, 0, 0 },
	{ 1, 2, 0, 3, 4, 5, 0, 0 },
	{ 1, 3, 0, 2, 4, 5, 6, 7 },
	{ 0, 1, 2, 3, 0, 0, 0, 0 },
	{ 1, 3, 0, 2, 4, 5, 6, 0 },
	{ 1, 2, 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 0, 0, 0, 0, 0, 0 },
};

static const uint32_t configurationModes[8] = { 0, 1, 2, 3, 4, 5, 6, 0 };

#include "aacmixes.h"

uint32_t aacSamplingIndex(uint32_t rate)
{
	switch (rate) {
	case 48000:
		return 3;
	case 44100:
		return 4;
	case 32000:
		return 5;
	case 24000:
		return 6;
	case 22050:
		return 7;
	case 16000:
		return 8;
	case 12000:
		return 9;
	}
	return (uint8_t)((rate == 11025 ? 0 : 1) + 10);
}

/* ADTS: the header found and read; a frame's raw blocks follow */
int32_t aacAdtsFrame(AacStream *stream, uint32_t minimum, uint32_t size, uint32_t channels)
{
	AacBitstream *bits = &stream->bits;
	AdtsHeader *header = &stream->syntax.header;
	uint32_t crcCheck = stream->crcCheck, frames = stream->frames, rate;
	int32_t result;

	result = aacSync(&stream->syntax.frameStart, bits, minimum, size);
	stream->format = result;
	if (result == AAC_FORMAT_ADIF || result == 3)
		return AAC_STATUS_ADIF;
	if (stream->format != AAC_FORMAT_ADTS)
		return AAC_STATUS_NO_SYNC;
	if (size - stream->syntax.frameStart.byte < ADTS_HEADER_BYTES)
		return AAC_STATUS_UNDERFLOW;
	result = aacAdtsHeader(bits, header);
	if (result)
		return result;
	rate = bits->samplingRates[header->samplingIndex];
	if (stream->samplingRate != rate && frames >= 1)
		aacOverlapClear(bits, channels);
	stream->samplingRate = rate;
	if (frames == 0) {
		stream->id = header->id;
		bits->id = header->id;
	}
	result = aacAdtsCheck(header);
	if (result)
		return result;
	stream->crcPresent = 0;
	if (crcCheck && !header->protectionAbsent) {
		stream->crcPresent = 1;
		bits->crc = 0xffff;
		result = aacCrcHeader(bits, header);
		if (result)
			return result;
	}
	stream->rawDataBlock = 0;
	stream->rawDataBlocks = header->rawDataBlocks + 1;
	return 0;
}

/*
 * A raw data block decoded to the output, 32-bit samples a channel after
 * another; then converted in place. sbrFound: SBR was decoded.
 */
/*
 * When SBR fails (inlined at 0x84ff4a): the core's samples scaled as SBR's
 * output would be (3 bits down), the rest of the doubled frame cleared.
 */
static void aacSbrFallback(int32_t *output, int32_t samples)
{
	int32_t i;

	for (i = 0; i < samples; i++)
		output[i] >>= 3;
	for (; i < 2 * AAC_FRAME_SAMPLES; i++)
		output[i] = 0;
}

int32_t aacRawDataBlock(AacDecoder *decoder, int32_t *sbrFound, int32_t *samples,
	int32_t *channels, int32_t *rate, int32_t interleaved, int32_t *output)
{
	int32_t *outputs[AAC_MAX_CHANNELS] = { 0 };
	AacStream *stream;
	AacBitstream *bits;
	int32_t result, crcResult, status, sbrPresent, mixError, ch, count, i;
	uint32_t numChannels = decoder->numChannels, sbrCreated, sbrDone, copy, bitsOut;
	int8_t *elementChannels;
	int32_t *sbrWork, *sbrState;
	const int32_t *mix;

	decoder->sbrActive = 0;
	decoder->sbrStatus = 2;
	decoder->parametricStereo = 0;
	if (numChannels == 0 || !(numChannels < 7))
		return -1;
	if (decoder->layout != AAC_LAYOUT_PLAIN && decoder->layout != AAC_LAYOUT_SBR)
		return -1;
	sbrWork = decoder->sbrWork[2];
	*sbrFound = 0;
	for (i = 0; (uint32_t)i < numChannels; i++)
		aacSbrChannelReset(decoder->sbrChannels[i]);
	stream = decoder->stream;
	bits = &stream->bits;
	result = aacElements(decoder, &sbrPresent);
	if (result && result != AAC_STATUS_CHANNEL_CHANGE)
		return result;

	crcResult = 0;
	if (stream->crcPresent && stream->rawDataBlock + 1 == stream->rawDataBlocks &&
	    !decoder->ignoreCrc)
		crcResult = aacCrcCompare(bits, &stream->syntax.header);
	mix = NULL;
	status = crcResult | result;
	if (!status) {
		mixError = 0;
		mix = aacChannelConfig(stream->channels, stream, &stream->syntax, NULL, &mixError);
		switch (stream->channelMode) {
		case 11:
			decoder->outputMode = 0;
			break;
		case 1:
			decoder->outputMode = 1;
			break;
		case 2:
			decoder->outputMode = 2;
			break;
		case 3:
			decoder->outputMode = 3;
			break;
		case 10:
			decoder->outputMode = 4;
			break;
		case 4:
			decoder->outputMode = 5;
			break;
		case 8:
			decoder->outputMode = 6;
			break;
		case 5:
		case 6:
			decoder->outputMode = 7;
			break;
		}
		for (i = 0; i < stream->channels; i++)
			outputs[i] = bits->output[i];
		for (i = 0; i < stream->channels; i++)
			bits->output[i] = outputs[stream->channelMap[i]];
		status |= mixError;
	}
	if (status)
		aacOverlapClear(bits, numChannels);

	elementChannels = stream->elementChannels;
	*channels = stream->channels;
	*rate = stream->samplingRate;
	*samples = AAC_FRAME_SAMPLES;
	if (sbrPresent) {
		decoder->sbrStatus = 5;
		sbrState = (int32_t *)decoder->sbrState;
		decoder->sbrActive = 1;
		if (decoder->sbrMode == 1 || (decoder->sbrMode == 2 && sbrPresent)) {
			for (ch = 0; ch < *channels; ch += elementChannels[ch]) {
				count = elementChannels[ch];
				if (decoder->sbr[ch] && status != AAC_STATUS_CHANNEL_CHANGE)
					continue;
				if (*channels == 1)
					count = 2;
				if (decoder->sbrUpsampling == 2)
					sbrCreated = (uint8_t)*channels == 1;
				else
					sbrCreated = decoder->sbrUpsampling == 1;
				decoder->sbr[ch] = aacSbrCreate(*rate, *samples, decoder->sbrDownsampled,
					sbrCreated, sbrState, count, ch, sbrWork, decoder);
			}
		}

		copy = 1;
		sbrDone = 1;
		for (ch = 0; ch < *channels; ch += count) {
			count = elementChannels[ch];
			if (*rate * 2 != aacSbrRate(&decoder->sbr[ch]->header))
				aacSbrSetRate(&decoder->sbr[ch]->header, *rate, *samples);
			if (!decoder->sbr[ch])
				continue;
			/* a pair's right channel next to its left, out of the left's way */
			if (count == 2) {
				for (i = 0; i < *samples; i++)
					output[ch * *samples * 2 + *samples + i] =
						output[(ch * *samples + *samples) * 2 + i];
			}
			if (ch == 0) {
				for (i = 0; i < AAC_FRAME_SAMPLES; i++)
					decoder->savedChannel[i] = output[*samples * 2 + i];
			}
			if (ch == 1) {
				for (i = 0; i < AAC_FRAME_SAMPLES; i++)
					output[*samples * 2 + i] = decoder->savedChannel[i];
			}
			result = aacSbrDecode(decoder->sbr[ch], decoder->sbrChannels[ch],
				output + ch * *samples * 2, &count, decoder->sbrDownsampled,
				decoder->sbrParameter, 1, sbrWork, count - 1);
			i = aacSbrCheck(&decoder->sbr[ch]->header, 4);
			if (result) {
				/* the core's output alone, scaled as SBR's would be */
				if (i)
					decoder->sbrStatus = 29;
				decoder->sbr[ch] = NULL;
				if (count == 1)
					aacSbrFallback(output + ch * *samples * 2, *samples);
				if (count == 2) {
					aacSbrFallback(output + ch * *samples * 2, *samples);
					aacSbrFallback(output + (ch * *samples + *samples) * 2, *samples);
				}
				continue;
			}
			if (i) {
				decoder->parametricStereo = 1;
				decoder->outputMode = 2;
				decoder->sbrStatus = 29;
			}
			if (*channels == 1)
				*channels = count;
			*sbrFound = 1;
			if (!decoder->sbrDownsampled)
				copy = 0;
			sbrDone = 0;
		}
		if (!decoder->sbrDownsampled && *sbrFound) {
			*samples <<= 1;
			*rate <<= 1;
		}
		/* channels SBR left at 2048 apart, closed up */
		if (copy == 1) {
			for (ch = 1; ch < *channels; ch++) {
				for (i = 0; i < *samples; i++)
					output[ch * *samples + i] = output[ch * *samples * 2 + i];
			}
		}
		if (sbrDone) {
			for (i = 0; i < *channels * *samples; i++)
				output[i] <<= 1;
		}
	}

	bitsOut = decoder->bytesPerSample == 2 ? 16 : 24;
	result = stream->downmix;
	aacOutput(output, *samples, *channels, result, bitsOut, interleaved, decoder, sbrPresent,
		mix);
	if (result == 1)
		*channels = 2;
	if (crcResult)
		stream->crcErrors++;
	stream->rawDataBlock++;
	if (stream->rawDataBlocks == stream->rawDataBlock)
		stream->frames++;
	return status;
}

/*
 * The channel layout (AacStream.channelMode) from the header, the program
 * config or the count, the channels reordered for it, and the downmix's
 * gains when one is asked for.
 */
const int32_t *aacChannelConfig(int32_t channels, AacStream *stream, AacSyntax *syntax,
	const int32_t *mix, int32_t *error)
{
	uint32_t mapped[AAC_MAX_CHANNELS] = { 0 };
	int32_t mode = 0, i;

	for (i = 0; i < stream->channels; i++)
		stream->channelMap[i] = i;
	mode = syntax->header.channelConfiguration;
	if (mode)
		mode = configurationModes[stream->channels];
	if (!mode) {
		if (stream->pceFound) {
			*error = aacPceChannelMap(&mode, channels, stream, syntax);
			if (*error)
				return mix;
		} else {
			switch (stream->channels) {
			case 2:
				mode = 11;
				break;
			case 3:
				mode = 10;
				break;
			case 4:
				mode = 8;
				break;
			case 1:
			case 5:
			case 6:
				mode = stream->channels;
				break;
			}
		}
	}
	if (stream->channelMode != (uint32_t)mode && stream->frames >= 1)
		*error = AAC_STATUS_CHANNEL_CHANGE;
	if (mode >= 1) {
		for (i = 0; i < stream->channels; i++)
			mapped[i] = stream->channelMap[i];
		for (i = 0; i < stream->channels; i++)
			stream->channelMap[i] = mapped[channelOrders[mode - 1][i]];
	}

	mix = NULL;
	if (stream->downmix) {
		switch (mode) {
		case 1:
			mix = stereoMixes[0][0];
			break;
		case 2:
			mix = stereoMixes[1][0];
			break;
		case 3:
			mix = stereoMixes[2][0];
			break;
		case 4:
			mix = stereoMixes[4][0];
			break;
		case 5:
		case 6:
			if (stream->matrixMixdown) {
				if (stream->pceFound && syntax->pce.matrixMixdownIdxPresent) {
					if (!syntax->pce.pseudoSurroundEnable || stream->surroundMix)
						mix = matrixMixes[syntax->pce.matrixMixdownIdx][0];
					else
						mix = pseudoSurroundMixes[syntax->pce.matrixMixdownIdx][0];
				} else {
					mix = defaultMix[0];
				}
			} else if (syntax->pce.matrixMixdownIdxPresent) {
				mix = matrixMixes[syntax->pce.matrixMixdownIdx][0];
			} else {
				mix = defaultMix[0];
			}
			break;
		case 11:
			mix = dualMonoMixes[stream->dualMono][0];
			break;
		default:
			stream->downmix = 0;
			mix = NULL;
			break;
		}
	}
	stream->channelMode = mode;
	return mix;
}

/* elements of a kind, in the program config's order, onto their channels */
static int32_t channelCount(const uint8_t *isCpe, uint32_t count)
{
	int32_t channels = 0;
	uint32_t i;

	for (i = 0; i < count; i++)
		channels += isCpe[i] ? 2 : 1;
	return channels;
}

int32_t aacPceChannelMap(int32_t *mode, int32_t channels, AacStream *stream,
	AacSyntax *syntax)
{
	const AacProgramConfig *pce = &syntax->pce;
	int32_t front, side, back, lfe, total, frontCenter, backCenter, n = 0;
	const AacElement *element;
	uint32_t i, k;

	front = channelCount(pce->frontElementIsCpe, pce->numFrontChannelElements);
	frontCenter = (pce->numFrontChannelElements * 2 - front) & 1;
	side = channelCount(pce->sideElementIsCpe, pce->numSideChannelElements);
	back = channelCount(pce->backElementIsCpe, pce->numBackChannelElements);
	backCenter = (pce->numBackChannelElements * 2 - back) & 1;
	lfe = pce->numLfeChannelElements;
	total = front + side + back + lfe;
	if (channels < total || !total)
		return 16;
	switch (total) {
	case 1:
		*mode = 1;
		break;
	case 2:
		*mode = pce->numFrontChannelElements == (uint32_t)front ? 11 : 2;
		break;
	case 3:
		if (frontCenter && !backCenter && !lfe)
			*mode = 3;
		if (!frontCenter && backCenter && !lfe)
			*mode = 10;
		break;
	case 4:
		if (frontCenter && backCenter && !lfe)
			*mode = 4;
		else if (!frontCenter && !backCenter && !lfe)
			*mode = 8;
		break;
	case 5:
		if (frontCenter && !backCenter && !lfe)
			*mode = 5;
		break;
	case 6:
		if (frontCenter && !backCenter && lfe)
			*mode = 6;
		break;
	}

#define MAP(count, isCpe, tags) \
	for (i = 0; i < pce->count; i++) { \
		for (k = 0, element = syntax->elements; k < stream->numElements; k++, element++) { \
			if (pce->isCpe[i] != element->id || pce->tags[i] != element->elementInstanceTag) \
				continue; \
			stream->channelMap[n++] = element->channel; \
			if (pce->isCpe[i]) \
				stream->channelMap[n++] = element->channel + 1; \
		} \
		if (pce->count == i) \
			return 16; \
	}
	MAP(numFrontChannelElements, frontElementIsCpe, frontElementTagSelect);
	MAP(numSideChannelElements, sideElementIsCpe, sideElementTagSelect);
	MAP(numBackChannelElements, backElementIsCpe, backElementTagSelect);
#undef MAP
	for (i = 0; i < pce->numLfeChannelElements; i++) {
		for (k = 0, element = syntax->elements; k < stream->numElements; k++, element++) {
			if (element->id == AAC_ID_LFE &&
			    pce->lfeElementTagSelect[i] == element->elementInstanceTag)
				stream->channelMap[n++] = element->channel;
		}
		if (pce->numLfeChannelElements == i)
			return 16;
	}
	return 0;
}

/* the elements up to ID_END, each checked, CRC'd and reconstructed */
int32_t aacElements(AacDecoder *decoder, int32_t *sbrPresent)
{
	AacStream *stream = decoder->stream;
	AacBitstream *bits = &stream->bits;
	AacSyntax *syntax = &stream->syntax;
	uint32_t layout = decoder->layout, numChannels = decoder->numChannels;
	int32_t *spectra = (int32_t *)decoder->spectra;
	int16_t channelsBefore = stream->channels;
	uint32_t pceFound = stream->pceFound, frames = stream->frames;
	uint32_t value14 = stream->value14, crcWholeCpe = stream->crcWholeCpe;
	uint32_t crcPresent = stream->crcPresent;
	int32_t counts[AAC_MAX_CHANNELS], result, n;
	uint32_t ch = 0, numElements = 0, i;
	uint8_t id, previous;
	AacElement *element;

	*sbrPresent = 0;
	for (i = 0; i < numChannels; i++)
		aacSbrChannelReset(decoder->sbrChannels[i]);
	result = aacReadByte(bits, &id, 3);
	if (result)
		return result;
	for (i = 0; i < numChannels; i++)
		stream->channelFlags[i] = 0;
	for (i = 0; i < numChannels; i++)
		counts[i] = 0;
	previous = AAC_ID_END;
	while (id != AAC_ID_END) {
		switch (id) {
		case AAC_ID_SCE:
		case AAC_ID_LFE:
			if (id == AAC_ID_LFE)
				decoder->value111 = 1;
			if (!(ch < numChannels))
				return AAC_STATUS_CHANNELS;
			counts[ch] = 1;
			if (id == AAC_ID_LFE)
				result = aacLfeChannelElement(bits, &syntax->sce);
			else
				result = aacSingleChannelElement(bits, &syntax->sce);
			if (result)
				return result;
			result = aacSceCheck(bits, &syntax->sce);
			if (result)
				return result;
			if (crcPresent) {
				if (id == AAC_ID_LFE)
					result = aacCrcLfe(bits, &syntax->sce);
				else
					result = aacCrcSce(bits, &syntax->sce);
				if (result)
					return result;
			}
			aacSceReconstruct(bits, &syntax->sce, ch, value14, spectra);
			element = &syntax->elements[numElements++];
			element->id = id;
			element->elementInstanceTag = syntax->sce.elementInstanceTag;
			element->channel = ch;
			ch++;
			break;
		case AAC_ID_CPE:
			if (numChannels < ch + 2)
				return AAC_STATUS_CHANNELS;
			counts[ch] = 2;
			result = aacChannelPairElement(bits, &syntax->cpe);
			if (result)
				return result;
			result = aacCpeCheck(bits, &syntax->cpe);
			if (result)
				return result;
			if (crcPresent) {
				result = aacCrcCpe(bits, &syntax->cpe, crcWholeCpe);
				if (result)
					return result;
			}
			aacCpeReconstruct(bits, &syntax->cpe, ch, value14, spectra);
			element = &syntax->elements[numElements++];
			element->id = id;
			element->elementInstanceTag = syntax->cpe.elementInstanceTag;
			element->channel = ch;
			ch += 2;
			break;
		case AAC_ID_CCE:
			result = aacCouplingChannelElement(bits, syntax->cce);
			if (result)
				return result;
			if (crcPresent && aacCrcCce(bits, syntax->cce))
				return -1;
			break;
		case AAC_ID_PCE:
			result = aacProgramConfig(bits, &syntax->pce);
			if (result)
				return result;
			if (crcPresent) {
				result = aacCrcPce(bits, &syntax->pce);
				if (result)
					return result;
			}
			pceFound = 1;
			break;
		case AAC_ID_DSE:
			result = aacDataStreamElement(bits, &syntax->dse);
			if (result)
				return result;
			if (crcPresent) {
				result = aacCrcDse(bits, &syntax->dse);
				if (result)
					return result;
			}
			break;
		case AAC_ID_FIL:
			/* SBR's extension follows the element it extends */
			if (layout == AAC_LAYOUT_SBR)
				result = aacSbrExtension(bits,
					decoder->sbrChannels[ch - (previous == AAC_ID_CPE ? 2 : 1)],
					previous);
			else
				result = aacFillElement(bits, &syntax->fill);
			if (result)
				return result;
			break;
		default:
			return AAC_STATUS_ELEMENT;
		}
		previous = id;
		if (aacReadByte(bits, &id, 3))
			return AAC_STATUS_UNDERFLOW;
	}
	aacByteAlign(bits);
	if (ch == 0)
		return AAC_STATUS_NO_CHANNELS;
	if (frames && channelsBefore != (int32_t)ch) {
		decoder->channels = ch;
		stream->channels = ch;
	}
	stream->numElements = numElements;
	n = (int16_t)ch;
	stream->pceFound = pceFound;
	stream->channels = n;
	for (i = 0; (int32_t)i < (int32_t)ch; i += counts[i]) {
		stream->elementChannels[i] = counts[i];
		if (aacSbrPresent(decoder->sbrChannels[i]))
			*sbrPresent = 1;
	}
	if (frames && n != (int32_t)ch)
		return AAC_STATUS_CHANNEL_CHANGE;
	return 0;
}

int32_t aacCrcCompare(const AacBitstream *bits, const AdtsHeader *header)
{
	if (header->crc != bits->crc)
		return AAC_STATUS_CRC;
	return 0;
}

/* count bits of value, high first */
uint16_t aacCrcUpdate(uint16_t *crc, uint32_t value, int32_t count)
{
	uint16_t data = (uint16_t)value << (16 - count);
	int32_t i;

	for (i = 0; i < count; i++) {
		uint16_t feedback = (*crc ^ data) & 0x8000;

		data <<= 1;
		*crc <<= 1;
		*crc ^= feedback ? CRC_POLYNOMIAL : 0;
	}
	return *crc;
}

/* count bits from the stream, then zeros up to total */
static uint8_t crcBits(AacBitstream *bits, int32_t count, int32_t total)
{
	int32_t done = 0, chunk;
	uint8_t value;

	while (done < count) {
		chunk = count - done < 8 ? count - done : 8;
		if ((uint8_t)aacReadByte(bits, &value, chunk))
			return (uint8_t)-1;
		aacCrcUpdate(&bits->crc, value, chunk);
		done += chunk;
	}
	while (done < total) {
		chunk = total - done < 8 ? total - done : 8;
		aacCrcUpdate(&bits->crc, 0, chunk);
		done += chunk;
	}
	return 0;
}

/* the bits between two places */
static int32_t bitsBetween(const AacBitPosition *start, const AacBitPosition *end)
{
	return end->byte * 8 - (start->byte * 8 - start->bits) - end->bits;
}

/* the reader is put back where it was after each */
int32_t aacCrcHeader(AacBitstream *bits, const AdtsHeader *header)
{
	AacBitPosition saved = bits->position, start = header->start;
	int32_t result;

	result = aacSeek(bits, &start);
	if (result)
		return result;
	result = crcBits(bits, CRC_HEADER_BITS, CRC_HEADER_BITS);
	if (result)
		return result;
	return aacSeek(bits, &saved);
}

static int32_t crcElement(AacBitstream *bits, const AacBitPosition *start,
	const AacBitPosition *end, int32_t limit)
{
	AacBitPosition saved = bits->position, from = *start;
	int32_t count;

	if (aacSeek(bits, &from) == -1)
		return -1;
	count = bitsBetween(start, end);
	if (limit) {
		if (count > limit)
			count = limit;
		if (crcBits(bits, count, limit) == (uint8_t)-1)
			return -1;
	} else if (crcBits(bits, count, count) == (uint8_t)-1) {
		return -1;
	}
	if (aacSeek(bits, &saved) == -1)
		return -1;
	return 0;
}

int32_t aacCrcSce(AacBitstream *bits, const AacSingleChannelElement *sce)
{
	return crcElement(bits, &sce->start, &sce->end, CRC_ELEMENT_BITS);
}

/* the element to its right channel's start (or all of it), then the right channel's */
int32_t aacCrcCpe(AacBitstream *bits, const AacChannelPairElement *cpe, uint32_t whole)
{
	AacBitPosition saved = bits->position, from = cpe->start;
	int32_t count;

	if (aacSeek(bits, &from) == -1)
		return -1;
	if (whole)
		count = bitsBetween(&cpe->start, &cpe->end);
	else
		count = bitsBetween(&cpe->start, &cpe->ics[1]->start);
	if (count > CRC_ELEMENT_BITS)
		count = CRC_ELEMENT_BITS;
	if (crcBits(bits, count, CRC_ELEMENT_BITS) == (uint8_t)-1)
		return -1;
	from = cpe->ics[1]->start;
	if (aacSeek(bits, &from) == -1)
		return -1;
	count = bitsBetween(&cpe->ics[1]->start, &cpe->end);
	if (count > CRC_RIGHT_BITS)
		count = CRC_RIGHT_BITS;
	if (crcBits(bits, count, CRC_RIGHT_BITS) == (uint8_t)-1)
		return -1;
	if (aacSeek(bits, &saved) == -1)
		return -1;
	return 0;
}

int32_t aacCrcCce(AacBitstream *bits, const AacCouplingChannelElement *cce)
{
	return crcElement(bits, &cce->start, &cce->end, CRC_ELEMENT_BITS);
}

int32_t aacCrcPce(AacBitstream *bits, const AacProgramConfig *pce)
{
	return crcElement(bits, &pce->start, &pce->end, 0);
}

int32_t aacCrcDse(AacBitstream *bits, const AacDataStreamElement *dse)
{
	return crcElement(bits, &dse->start, &dse->end, 0);
}

int32_t aacCrcLfe(AacBitstream *bits, const AacSingleChannelElement *lfe)
{
	return aacCrcSce(bits, lfe);
}
