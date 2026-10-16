/*
 * The AAC decoder's engine interface (0x84d9d8-0x84e888): an instance in
 * the memory given, settings by key (negative keys set), and a frame a
 * decode call.
 */
#include "aac.h"

#define AAC_ERROR_PARAMETER	(-97)
#define AAC_ERROR_FRAME		(-98)
#define AAC_ERROR_COUNT		(-90)
#define AAC_ERROR_EMPTY		(-87)
#define AAC_ERROR_VALUE		(-88)
#define AAC_ERROR_KEY		(-86)

/* the keys; the negative ones set */
#define AAC_KEY_TWO		1
#define AAC_KEY_ONE		2
#define AAC_KEY_INTERFACE_7	3
#define AAC_KEY_MEMORY_SIZE	4
#define AAC_KEY_VERSION		6
#define AAC_KEY_STATUS		9
#define AAC_KEY_FORMAT		21
#define AAC_KEY_OUTPUT		31
#define AAC_KEY_DOWNSAMPLED	45
#define AAC_KEY_INTERFACE	47
#define AAC_KEY_SAMPLING_RATE	200
#define AAC_KEY_CHANNELS	201
#define AAC_KEY_FRAMES		203
#define AAC_KEY_INTERLEAVED	204
#define AAC_KEY_SAMPLES		214
#define AAC_KEY_DOWNMIX		400
#define AAC_KEY_SBR		403
#define AAC_KEY_CONFIGURATION	407
#define AAC_KEY_SBR_STATE	412
#define AAC_KEY_IGNORE_CRC	413
#define AAC_KEY_CRC_ERRORS	1012
#define AAC_KEY_OUT_CHANNELS	16511

/* the stream formats (AAC_KEY_FORMAT) */
#define AAC_STREAM_RAW		122
#define AAC_STREAM_ADTS		128

/* output formats: sample size and byte order */
#define AAC_OUTPUT_16_LITTLE	101
#define AAC_OUTPUT_16_BIG	102
#define AAC_OUTPUT_24_LITTLE	104
#define AAC_OUTPUT_24_BIG	105

#define AAC_DEFAULT_SBR_MODE	2
#define AAC_DEFAULT_UPSAMPLING	2

/* the memory each part takes */
#define CHANNEL_BYTES_PLAIN	3504
#define CHANNEL_BYTES_SBR	10928
#define SPECTRA_BYTES_PLAIN	0x3100
#define SPECTRA_BYTES_SBR	0x8000
#define SBR_WORK_BYTES		0x2000
#define SBR_STATE_BYTES		2000
#define OVERLAP_BYTES		0x800
#define SAMPLES_SBR		2048

static const char aacVersion[] = "";

static int32_t aacCreate(void **handle, void *memory, uint32_t size);
static int32_t aacControl(void *handle, const int32_t *keys, uint32_t count, int32_t *values,
	uint32_t *work);
static int32_t aacDecode(void *handle, const uint8_t **input, uint32_t *inputSize,
	uint32_t count, int16_t **output, uint32_t *outputSize, uint32_t *frames);
static int32_t aacDestroy(void *handle);

static const AudioDecoderEngine aacEngineTable = {
	aacCreate,
	aacControl,
	aacDecode,
	aacDestroy,
};

const AudioDecoderEngine *aacEngine(void)
{
	return &aacEngineTable;
}

static int32_t aacDestroy(void *handle)
{
	AacHandle *aac = handle;

	if (!aac)
		return AAC_ERROR_PARAMETER;
	aac->decoder = NULL;
	return 0;
}

/* the memory an instance of the layout and channels takes; an unknown pair is the largest */
static uint32_t aacMemorySize(uint32_t layout, uint32_t channels)
{
	uint32_t size, perChannel;

	if (layout == AAC_LAYOUT_PLAIN) {
		if (channels != 6 && channels != 2 && channels != 1) {
			channels = 6;
			layout = AAC_LAYOUT_SBR;
		}
	} else if (layout != AAC_LAYOUT_SBR) {
		channels = 6;
		layout = AAC_LAYOUT_SBR;
	} else if (channels != 6 && channels != 2) {
		channels = 6;
	}
	perChannel = (channels * CHANNEL_BYTES_PLAIN + 63) & ~63;
	size = (channels << 11) + 0x6e00 + perChannel;
	if (layout == AAC_LAYOUT_PLAIN)
		return size + SPECTRA_BYTES_PLAIN;
	if (layout == AAC_LAYOUT_SBR)
		return size + 0x8800 + channels * 0x1d00;
	return size;
}

static uint8_t *align64(uint8_t *address)
{
	uint32_t rest = (uint32_t)address & 63;

	if (rest)
		address += 64 - rest;
	return address;
}

/* the layout whose size is exactly the memory's, largest first; else none */
static uint32_t aacMemoryLayout(uint32_t *layout, uint32_t *channels, uint32_t size)
{
	static const uint32_t candidates[5][2] = {
		{ AAC_LAYOUT_SBR, 6 },
		{ AAC_LAYOUT_SBR, 2 },
		{ AAC_LAYOUT_PLAIN, 6 },
		{ AAC_LAYOUT_PLAIN, 2 },
		{ AAC_LAYOUT_PLAIN, 1 },
	};
	uint32_t i, total;

	for (i = 0; i < 5; i++) {
		*layout = candidates[i][0];
		*channels = candidates[i][1];
		total = aacMemorySize(*layout, *channels);
		if (total == size)
			return total;
	}
	*layout = 0;
	*channels = 0;
	return 0;
}

static void fill(void *destination, int32_t value, int32_t size)
{
	uint8_t *byte = destination;

	while (size-- > 0)
		*byte++ = value;
}

/* the reader at the frame's start, its tables, the channels' overlap cleared */
static void bitstreamInit(AacBitstream *bits, uint32_t channels)
{
	aacTablesInit((AacTables *)bits->tables);
	bits->samplingIndex = 3;
	bits->samplingRates = bits->tables->samplingRates;
	aacOverlapClear(bits, channels);
}

static int32_t aacDecoderInit(AacDecoder *decoder, uint32_t sbrMode, uint32_t upsampling)
{
	uint32_t channels = decoder->numChannels, i;
	AacStream *stream;
	AacBitPosition start;

	if (channels == 0 || !(channels < 7))
		return -1;
	stream = decoder->stream;
	stream->channels = 1;
	stream->frames = 0;
	stream->samplingRate = 48000;
	stream->crcPresent = 0;
	stream->crcErrors = 0;
	stream->pceFound = 0;
	stream->value58 = 2;
	stream->value10 = 24;
	stream->value14 = 32;
	stream->crcWholeCpe = 1;
	stream->crcCheck = 1;
	stream->id = 1;
	stream->value4c = 1;
	stream->channelMode = 0;
	start = stream->syntax.frameStart;
	aacSeek(&stream->bits, &start);
	bitstreamInit(&stream->bits, channels);
	stream->bits.end = 0x1000;
	stream->bits.bytesRead = 0;
	stream->bits.randomSeed = 0;
	stream->bits.position.byte = 0;
	stream->bits.reserved7[0] = 0;
	for (i = 0; i < channels; i++)
		stream->channelState[i] = &decoder->channelStates[i];
	stream->matrixMixdown = 1;
	stream->bits.noise.negativePowers = aacNegativePowers;
	stream->bits.noise.inverseSqrt = aacInverseSqrt;
	stream->bits.noise.seed = 0;
	if (sbrMode == 0 || sbrMode == 1 || sbrMode == 2)
		decoder->sbrMode = sbrMode;
	decoder->sbrUpsampling = upsampling;
	aacSbrInit(channels, decoder);
	decoder->rateChanged = 1;
	decoder->sbrDownsampled = 0;
	decoder->sbrParameter = 0;
	return 0;
}

static int32_t aacCreate(void **handle, void *memory, uint32_t size)
{
	uint32_t layout = 0, channels = 0, total, perChannel = 0, i, k;
	static const uint32_t bufferSizes[8] = { 192, 192, 48, 512, 512, 1280, 5120, 3072 };
	AacHandle *aac = memory;
	AacDecoder *decoder;
	AacStream *stream;
	uint8_t *next, *buffer;
	AacBitPosition start;

	if (!handle || !memory)
		return AAC_ERROR_PARAMETER;
	total = aacMemoryLayout(&layout, &channels, size);
	if (size < total || !total)
		return AAC_ERROR_PARAMETER;
	if (layout == AAC_LAYOUT_PLAIN)
		perChannel = (channels * CHANNEL_BYTES_PLAIN + 63) & ~63;
	else if (layout == AAC_LAYOUT_SBR)
		perChannel = (channels * CHANNEL_BYTES_SBR + 63) & ~63;

	decoder = (AacDecoder *)align64((uint8_t *)memory + sizeof(*aac));
	fill(decoder, 0, sizeof(*decoder));
	aac->decoder = decoder;
	next = align64((uint8_t *)decoder + sizeof(*decoder));
	decoder->numChannels = channels;
	decoder->layout = layout;
	if (layout == AAC_LAYOUT_SBR) {
		fill(next, 0, SPECTRA_BYTES_SBR);
		decoder->spectra = (int32_t *)next;
		next += SPECTRA_BYTES_SBR;
		decoder->sbrWork[0] = decoder->spectra;
		decoder->sbrWork[1] = decoder->sbrWork[0] + SBR_WORK_BYTES / 4;
		decoder->sbrWork[2] = decoder->sbrWork[1] + SBR_WORK_BYTES / 4;
		decoder->savedChannel = decoder->sbrWork[2] + SBR_WORK_BYTES / 4;
	} else {
		fill(next, 0, SPECTRA_BYTES_PLAIN);
		decoder->spectra = (int32_t *)next;
		next += SPECTRA_BYTES_PLAIN;
		decoder->sbrWork[0] = NULL;
		decoder->sbrWork[1] = NULL;
		decoder->sbrWork[2] = NULL;
		decoder->savedChannel = NULL;
	}

	/* each channel's buffers, those SBR needs only with it */
	fill(next, 0, perChannel);
	decoder->channelMemory = next;
	decoder->channelMemoryEnd = decoder->channelMemory;
	buffer = decoder->channelMemoryEnd;
	for (k = 0; k < 8; k++) {
		if (layout != AAC_LAYOUT_SBR && k >= 3 && k < 7)
			continue;
		for (i = 0; i < channels; i++) {
			decoder->channelBuffer[k][i] = buffer;
			buffer += bufferSizes[k];
		}
	}

	stream = (AacStream *)align64(next + perChannel);
	decoder->stream = stream;
	fill(stream, 0, sizeof(*stream));
	next = align64((uint8_t *)stream + sizeof(*stream));
	if (layout == AAC_LAYOUT_SBR) {
		decoder->sbrState = next;
		fill(decoder->sbrState, 0, SBR_STATE_BYTES);
		next += SBR_STATE_BYTES;
	}
	next = align64(next);
	fill(next, 0, sizeof(AacIcs));
	stream->syntax.ics[0] = (AacIcs *)next;
	next = align64(next + sizeof(AacIcs));
	fill(next, 0, sizeof(AacIcs));
	stream->syntax.ics[1] = (AacIcs *)next;
	next = align64(next + sizeof(AacIcs));
	fill(next, 0, sizeof(AacTables));
	stream->bits.tables = (AacTables *)next;
	next = align64(next + sizeof(AacTables));
	stream->syntax.sce.ics = stream->syntax.ics[0];
	stream->syntax.cpe.ics[0] = stream->syntax.ics[0];
	stream->syntax.cpe.ics[1] = stream->syntax.ics[1];
	fill(next, 0, sizeof(AacCouplingChannelElement));
	stream->syntax.cce = (AacCouplingChannelElement *)next;
	stream->syntax.cce->ics = stream->syntax.ics[0];
	stream->syntax.sce.ics->spectrum = decoder->spectra;
	stream->syntax.cpe.ics[0]->spectrum = decoder->spectra;
	stream->syntax.cpe.ics[1]->spectrum = decoder->spectra + AAC_FRAME_SAMPLES;
	next += sizeof(AacCouplingChannelElement);
	fill(next, 0, channels << 11);
	next = align64(next);
	for (i = 0; i < channels; i++) {
		stream->bits.overlap[i] = (int32_t *)next;
		next += OVERLAP_BYTES;
	}
	if ((uint8_t *)memory + total < next)
		return AAC_ERROR_PARAMETER;

	start = stream->syntax.frameStart;
	aacSeek(&stream->bits, &start);
	bitstreamInit(&stream->bits, channels);
	aacDecoderInit(decoder, AAC_DEFAULT_SBR_MODE, AAC_DEFAULT_UPSAMPLING);
	aac->size = size;
	*handle = aac;
	return 0;
}

/* keys that need an instance */
static int keyNeedsHandle(int32_t key)
{
	switch (key) {
	case AAC_KEY_SBR:
	case AAC_KEY_IGNORE_CRC:
	case -AAC_KEY_IGNORE_CRC:
	case AAC_KEY_SBR_STATE:
	case AAC_KEY_STATUS:
	case AAC_KEY_CONFIGURATION:
	case -AAC_KEY_CONFIGURATION:
	case AAC_KEY_OUTPUT:
	case -AAC_KEY_OUTPUT:
	case AAC_KEY_INTERLEAVED:
	case -AAC_KEY_INTERLEAVED:
	case AAC_KEY_DOWNSAMPLED:
	case -AAC_KEY_DOWNSAMPLED:
	case AAC_KEY_DOWNMIX:
	case -AAC_KEY_DOWNMIX:
	case AAC_KEY_CHANNELS:
	case -AAC_KEY_CHANNELS:
	case AAC_KEY_SAMPLING_RATE:
	case -AAC_KEY_SAMPLING_RATE:
	case AAC_KEY_FORMAT:
	case -AAC_KEY_FORMAT:
		return 1;
	}
	return 0;
}

static int32_t setOutput(AacHandle *aac, int32_t format)
{
	AacDecoder *decoder = aac->decoder;

	switch (format) {
	case AAC_OUTPUT_24_BIG:
		decoder->bigEndian = 1;
		decoder->bytesPerSample = 3;
		decoder->reserved4 = 0;
		decoder->outputBytes = 3;
		break;
	case AAC_OUTPUT_24_LITTLE:
		decoder->bigEndian = 0;
		decoder->bytesPerSample = 3;
		decoder->reserved4 = 0;
		decoder->outputBytes = 3;
		break;
	case AAC_OUTPUT_16_BIG:
		decoder->bigEndian = 1;
		decoder->bytesPerSample = 2;
		decoder->reserved4 = 0;
		decoder->outputBytes = 2;
		break;
	case AAC_OUTPUT_16_LITTLE:
		decoder->bigEndian = 0;
		decoder->bytesPerSample = 2;
		decoder->reserved4 = 0;
		decoder->outputBytes = 2;
		break;
	default:
		return AAC_ERROR_VALUE;
	}
	aac->format = format;
	return 0;
}

static int isFlag(int32_t value)
{
	return value == 0 || value == 1;
}

static void versionCopy(char *destination)
{
	const char *from;

	if (!destination)
		return;
	for (from = aacVersion; *from; from++)
		*destination++ = *from;
	*destination = 0;
}

/* one key's value, or its setting */
static int32_t controlKey(AacHandle *aac, int32_t key, int32_t *values, uint32_t *i,
	uint32_t *work)
{
	AacDecoder *decoder = aac ? aac->decoder : NULL;
	int32_t value = values[*i];
	uint32_t index;

	switch (key) {
	case AAC_KEY_SAMPLES:
		values[*i] = decoder->samples;
		return 0;
	case AAC_KEY_OUT_CHANNELS:
		if (decoder->parametricStereo == 1)
			values[*i] = 2;
		else
			values[*i] = decoder->channels;
		return 0;
	case AAC_KEY_CRC_ERRORS:
		values[*i] = decoder->stream->crcErrors;
		return 0;
	case AAC_KEY_FRAMES:
		values[*i] = decoder->stream->frames;
		return 0;
	case AAC_KEY_SBR:
		if (!aac->decoder || !aac->decoder->stream->frames || aac->decoder->lastError) {
			values[*i] = 0;
			return 0;
		}
		if (!decoder->sbrActive)
			values[*i] = 2;
		else if (decoder->sbrActive == 1)
			values[*i] = 3;
		return 0;
	case AAC_KEY_INTERFACE:
		values[*i] = 0x20002;
		return 0;
	case AAC_KEY_IGNORE_CRC:
		values[*i] = decoder->ignoreCrc;
		return 0;
	case -AAC_KEY_IGNORE_CRC:
		if (!isFlag(value))
			return AAC_ERROR_VALUE;
		decoder->ignoreCrc = value;
		return 0;
	case AAC_KEY_CONFIGURATION:
		values[*i] = decoder->stream->syntax.header.channelConfiguration;
		return 0;
	case -AAC_KEY_CONFIGURATION:
		if (value < 0 || value >= 8)
			return AAC_ERROR_VALUE;
		decoder->stream->syntax.header.channelConfiguration = value;
		return 0;
	case AAC_KEY_OUTPUT:
		values[*i] = aac->format;
		return 0;
	case -AAC_KEY_OUTPUT:
		return setOutput(aac, value);
	case AAC_KEY_SBR_STATE:
		values[(*i)++] = decoder->sbrStatus;
		values[(*i)++] = decoder->outputMode;
		values[(*i)++] = decoder->value111;
		values[*i] = decoder->sbrActive;
		return 0;
	case AAC_KEY_INTERLEAVED:
		values[*i] = decoder->interleaved;
		return 0;
	case -AAC_KEY_INTERLEAVED:
		if (!isFlag(value))
			return AAC_ERROR_VALUE;
		decoder->interleaved = value;
		return 0;
	case AAC_KEY_DOWNSAMPLED:
		values[*i] = decoder->sbrDownsampled;
		return 0;
	case -AAC_KEY_DOWNSAMPLED:
		if (!isFlag(value))
			return AAC_ERROR_VALUE;
		decoder->sbrDownsampled = value;
		return 0;
	case AAC_KEY_DOWNMIX:
		values[*i] = decoder->stream->downmix;
		return 0;
	case -AAC_KEY_DOWNMIX:
		if (!isFlag(value))
			return AAC_ERROR_VALUE;
		decoder->stream->downmix = value;
		return 0;
	case -AAC_KEY_CHANNELS:
		if (value < 1 || value >= 7)
			return AAC_ERROR_VALUE;
		decoder->channels = value;
		return 0;
	case AAC_KEY_SAMPLING_RATE:
		values[*i] = decoder->samplingRate;
		return 0;
	case -AAC_KEY_SAMPLING_RATE:
		for (index = 0; index < AAC_SAMPLING_RATES; index++) {
			if (value == (int32_t)decoder->stream->bits.tables->samplingRates[index])
				break;
		}
		if (index < 3 || index >= AAC_SAMPLING_RATES)
			return AAC_ERROR_VALUE;
		decoder->samplingRate = value;
		return 0;
	case AAC_KEY_FORMAT:
		if (decoder->rawFormat == 1)
			values[*i] = AAC_STREAM_RAW;
		else if (decoder->rawFormat == 0)
			values[*i] = AAC_STREAM_ADTS;
		return 0;
	case -AAC_KEY_FORMAT:
		if (value == AAC_STREAM_RAW)
			decoder->rawFormat = 1;
		else if (value == AAC_STREAM_ADTS)
			decoder->rawFormat = 0;
		else
			return AAC_ERROR_VALUE;
		return 0;
	case AAC_KEY_TWO:
		values[*i] = 2;
		return 0;
	case AAC_KEY_ONE:
		values[*i] = 1;
		return 0;
	case AAC_KEY_INTERFACE_7:
		values[*i] = 0x70000;
		return 0;
	case AAC_KEY_MEMORY_SIZE:
		values[*i] = aacMemorySize(value & 0xffff0000, value & 0xffff);
		work[*i] = 0;
		return 0;
	case AAC_KEY_VERSION:
		versionCopy((char *)values);
		return 0;
	case AAC_KEY_STATUS:
		values[*i] = decoder->lastError;
		return 0;
	}
	return AAC_ERROR_KEY;
}

/* each key's result in work; the last error is returned */
static int32_t aacControl(void *handle, const int32_t *keys, uint32_t count, int32_t *values,
	uint32_t *work)
{
	AacHandle *aac = handle;
	int32_t result = 0;
	uint32_t i;

	if (!keys)
		return AAC_ERROR_PARAMETER;
	if ((int32_t)count < 1)
		return AAC_ERROR_EMPTY;
	if (!values || !work)
		return AAC_ERROR_PARAMETER;
	for (i = 0; i < count; i++) {
		work[i] = 0;
		if (keyNeedsHandle(keys[i]) && !aac) {
			result = AAC_ERROR_PARAMETER;
			work[i] = result;
		}
		if (work[i])
			continue;
		work[i] = controlKey(aac, keys[i], values, &i, work);
		if (work[i])
			result = work[i];
	}
	return result;
}

/*
 * Frames from input: ADTS ones each with their header, or one raw data
 * block; outputSize and frames say what came out.
 */
static int32_t aacDecode(void *handle, const uint8_t **input, uint32_t *inputSize,
	uint32_t count, int16_t **output, uint32_t *outputSize, uint32_t *frames)
{
	AacHandle *aac = handle;
	int32_t samples = 0, channels = 0, rate = 0, sbrFound, result = 0, status;
	uint32_t numChannels, layout, size, headerBytes = 0, i;
	uint8_t interleaved;
	int32_t ch;
	AacDecoder *decoder;
	AacStream *stream;
	AacBitstream *bits;
	const uint8_t *data;
	int32_t *pcm;

	if (!input || !*input)
		return AAC_ERROR_PARAMETER;
	data = *input;
	if (!output || !outputSize || !*output || !aac || count >= 2 || !frames ||
	    (int32_t)*frames < 1 || (int32_t)*outputSize < 1)
		return AAC_ERROR_PARAMETER;
	pcm = (int32_t *)*output;
	if ((int32_t)count < 1 || (int32_t)*inputSize < 1)
		return AAC_ERROR_COUNT;
	size = *inputSize;

	decoder = aac->decoder;
	numChannels = decoder->numChannels;
	layout = decoder->layout;
	decoder->lastError = 0;
	interleaved = decoder->interleaved;
	stream = decoder->stream;
	stream->bits.cursor = data;
	stream->bits.buffer = stream->bits.cursor;
	bits = &stream->bits;
	for (i = 0; i < numChannels; i++)
		bits->output[i] = pcm + i * (layout == AAC_LAYOUT_PLAIN ? AAC_FRAME_SAMPLES :
			SAMPLES_SBR);
	aacBitstreamReset(bits);

	if (decoder->rawFormat == 0) {
		bits->end = 9;
		status = aacAdtsFrame(stream, 9, size, numChannels);
		headerBytes = stream->syntax.frameStart.byte;
		if (status) {
			*outputSize = 0;
			if (status == AAC_STATUS_NO_SYNC)
				*inputSize = headerBytes - 1;
			decoder->lastError = status;
			aacOverlapClear(bits, numChannels);
			return AAC_ERROR_FRAME;
		}
		if (stream->rawDataBlocks >= 2) {
			decoder->lastError = AAC_STATUS_RAW_BLOCKS;
			return AAC_ERROR_FRAME;
		}
		if (headerBytes && size < stream->syntax.header.frameLength + headerBytes) {
			*inputSize = headerBytes - 1;
			*outputSize = 0;
			decoder->lastError = AAC_STATUS_UNDERFLOW;
			return AAC_ERROR_FRAME;
		}
		bits->end = stream->syntax.header.frameLength + headerBytes;
		if (decoder->sbrDownsampled == 1) {
			stream->samplingRate = decoder->samplingRate;
			bits->samplingIndex = aacSamplingIndex(decoder->samplingRate);
		}
	} else {
		stream->format = AAC_FORMAT_RAW;
		stream->channels = decoder->channels;
		stream->samplingRate = decoder->samplingRate;
		stream->rawDataBlocks = 1;
		stream->rawDataBlock = 0;
		stream->id = 0;
		bits->id = 0;
		if (decoder->rateChanged) {
			decoder->rateChanged = 0;
			bits->samplingIndex = aacSamplingIndex(decoder->samplingRate);
		}
		bits->end = size;
	}

	for (i = 0; i < stream->rawDataBlocks; i++) {
		status = aacRawDataBlock(decoder, &sbrFound, &samples, &channels, &rate,
			interleaved, pcm);
		if (status) {
			result = AAC_ERROR_FRAME;
			decoder->lastError = status;
		}
		if (result && status != AAC_STATUS_CHANNEL_CHANGE) {
			aacOverlapClear(&decoder->stream->bits, numChannels);
			return result;
		}
		*inputSize = stream->syntax.header.frameLength + headerBytes;
		if (decoder->interleaved) {
			*outputSize = samples * channels * decoder->bytesPerSample;
		} else {
			for (ch = 0; ch < channels; ch = (uint8_t)(ch + 1))
				outputSize[ch] = decoder->bytesPerSample * samples;
		}
		*frames = decoder->interleaved ? 1 : channels;
	}
	if (decoder->rawFormat == 0) {
		decoder->channels = channels;
		decoder->samplingRate = rate;
	} else {
		bits->end = bits->position.byte;
		*inputSize = bits->position.byte;
	}
	decoder->samples = samples;
	return result;
}
