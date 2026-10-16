/*
 * The MP3 decoder's engine interface (0x84d430-0x84d9d8): an instance in
 * the memory given, settings by key (negative keys set), and a frame a
 * decode call.
 */
#include "mp3.h"

#define MP3_MEMORY_SIZE		(sizeof(Mp3Handle) + sizeof(Mp3Decoder))
#define MP3_ERROR_PARAMETER	(-97)
#define MP3_ERROR_FRAME		(-98)
#define MP3_ERROR_COUNT		(-90)
#define MP3_ERROR_EMPTY		(-87)
#define MP3_ERROR_VALUE		(-88)
#define MP3_ERROR_KEY		(-86)

/* the keys; the negative ones set */
#define MP3_KEY_ZERO		1
#define MP3_KEY_ONE		2
#define MP3_KEY_INTERFACE	3
#define MP3_KEY_MEMORY_SIZE	4
#define MP3_KEY_VERSION		6
#define MP3_KEY_STATUS		9
#define MP3_KEY_FORMAT		31
#define MP3_KEY_SAMPLING_RATE	200
#define MP3_KEY_SAMPLES		202
#define MP3_KEY_FRAMES		203
#define MP3_KEY_INTERLEAVED	204
#define MP3_KEY_SAMPLE_BITS	205
#define MP3_KEY_BITRATE		207
#define MP3_KEY_MODE		600
#define MP3_KEY_VERSION_ID	601
#define MP3_KEY_LAYER		602
#define MP3_KEY_PROTECTION	603
#define MP3_KEY_COPYRIGHT	604
#define MP3_KEY_ORIGINAL	605
#define MP3_KEY_EMPHASIS	606
#define MP3_KEY_CHANNELS	16511

/* output formats: sample size and byte order */
#define MP3_FORMAT_16_LITTLE	101
#define MP3_FORMAT_16_BIG	102
#define MP3_FORMAT_24_LITTLE	104
#define MP3_FORMAT_24_BIG	105

static const char mp3Version[] = "SoCrates MP3 Decoder FIXED-1-16.";

static int32_t mp3Create(void **handle, void *memory, uint32_t size);
static int32_t mp3Control(void *handle, const int32_t *keys, uint32_t count, int32_t *values,
	uint32_t *work);
static int32_t mp3Decode(void *handle, const uint8_t **input, uint32_t *inputSize,
	uint32_t count, int16_t **output, uint32_t *outputSize, uint32_t *frames);
static int32_t mp3Destroy(void *handle);

static const AudioDecoderEngine mp3EngineTable = {
	mp3Create,
	mp3Control,
	mp3Decode,
	mp3Destroy,
	MP3_MEMORY_SIZE,
};

static const uint32_t mp3MemorySize = MP3_MEMORY_SIZE;

const AudioDecoderEngine *mp3Engine(void)
{
	return &mp3EngineTable;
}

static int32_t mp3Create(void **handle, void *memory, uint32_t size)
{
	Mp3Handle *mp3 = memory;
	Mp3Decoder *decoder;

	if (!memory || !handle || size < mp3MemorySize)
		return MP3_ERROR_PARAMETER;
	mp3Copy(mp3, &(Mp3Handle){ 0 }, sizeof(*mp3));
	mp3->decoder = (Mp3Decoder *)(mp3 + 1);
	if (mp3DecoderInit(mp3->decoder))
		return MP3_ERROR_PARAMETER;
	decoder = mp3->decoder;
	if (!decoder->bigEndianOutput) {
		if (decoder->bytesPerSample == 2)
			mp3->format = MP3_FORMAT_16_LITTLE;
	} else if (decoder->bytesPerSample == 2) {
		mp3->format = MP3_FORMAT_16_BIG;
	}
	decoder->input = decoder->inputBuffer;
	mp3->size = size;
	*handle = mp3;
	return 0;
}

static int32_t mp3Destroy(void *handle)
{
	Mp3Handle *mp3 = handle;
	int32_t result;

	if (!mp3)
		return MP3_ERROR_PARAMETER;
	result = mp3DecoderReset(mp3->decoder);
	mp3->reserved2 = 0;
	mp3->decoder = NULL;
	if (result)
		return MP3_ERROR_PARAMETER;
	return 0;
}

/* keys that need an instance */
static int keyNeedsHandle(int32_t key)
{
	switch (key) {
	case MP3_KEY_FORMAT:
	case -MP3_KEY_FORMAT:
	case -MP3_KEY_INTERLEAVED:
	case MP3_KEY_CHANNELS:
	case MP3_KEY_STATUS:
	case MP3_KEY_SAMPLING_RATE:
	case MP3_KEY_SAMPLES:
	case MP3_KEY_FRAMES:
	case MP3_KEY_INTERLEAVED:
	case MP3_KEY_SAMPLE_BITS:
	case MP3_KEY_BITRATE:
		return 1;
	}
	return key >= MP3_KEY_MODE && key <= MP3_KEY_EMPHASIS;
}

static int32_t setFormat(Mp3Handle *mp3, int32_t format)
{
	Mp3Decoder *decoder = mp3->decoder;

	mp3->format = format;
	switch (format) {
	case MP3_FORMAT_24_BIG:
		decoder->bytesPerSample = 3;
		decoder->bigEndianOutput = 1;
		return 0;
	case MP3_FORMAT_24_LITTLE:
		decoder->bytesPerSample = 3;
		decoder->bigEndianOutput = 0;
		return 0;
	case MP3_FORMAT_16_BIG:
		decoder->bytesPerSample = 2;
		decoder->bigEndianOutput = 1;
		return 0;
	case MP3_FORMAT_16_LITTLE:
		decoder->bytesPerSample = 2;
		decoder->bigEndianOutput = 0;
		return 0;
	}
	mp3->format = 0;
	return MP3_ERROR_VALUE;
}

/* one key's value, or its setting */
static int32_t controlKey(Mp3Handle *mp3, int32_t key, int32_t *values, uint32_t i)
{
	Mp3Decoder *decoder = mp3 ? mp3->decoder : NULL;
	char *version;
	const char *from;

	switch (key) {
	case MP3_KEY_CHANNELS:
		values[i] = decoder->header.mode == MP3_MODE_MONO ? 1 : 2;
		return 0;
	case MP3_KEY_FORMAT:
		values[i] = mp3->format;
		return 0;
	case -MP3_KEY_FORMAT:
		return setFormat(mp3, values[i]);
	case -MP3_KEY_INTERLEAVED:
		if (values[i] != 0 && values[i] != 1)
			return MP3_ERROR_VALUE;
		decoder->interleaved = values[i];
		return 0;
	case MP3_KEY_ZERO:
		values[i] = 0;
		return 0;
	case MP3_KEY_ONE:
		values[i] = 1;
		return 0;
	case MP3_KEY_INTERFACE:
		values[i] = 0x30002;
		return 0;
	case MP3_KEY_MEMORY_SIZE:
		values[i] = mp3MemorySize;
		return 0;
	case MP3_KEY_VERSION:
		/* the name, written over the values */
		version = (char *)values;
		if (version) {
			for (from = mp3Version; *from; from++)
				*version++ = *from;
			*version = 0;
		}
		return 0;
	case MP3_KEY_STATUS:
		values[i] = decoder->status;
		if (!decoder->status && decoder->warning)
			values[i] = decoder->warning;
		return 0;
	case MP3_KEY_SAMPLING_RATE:
		values[i] = decoder->header.samplingRate;
		return 0;
	case MP3_KEY_SAMPLES:
		values[i] = decoder->samples;
		return 0;
	case MP3_KEY_FRAMES:
		values[i] = decoder->frames;
		return 0;
	case MP3_KEY_INTERLEAVED:
		values[i] = decoder->interleaved;
		return 0;
	case MP3_KEY_SAMPLE_BITS:
		values[i] = decoder->bytesPerSample * 8;
		return 0;
	case MP3_KEY_BITRATE:
		values[i] = decoder->header.bitrate / 1000;
		return 0;
	case MP3_KEY_MODE:
		values[i] = decoder->header.mode;
		return 0;
	case MP3_KEY_VERSION_ID:
		if (decoder->header.samplingIndex < 3)
			values[i] = 0;
		else if (decoder->header.samplingIndex < 6)
			values[i] = 1;
		else
			values[i] = 2;
		return 0;
	case MP3_KEY_LAYER:
		values[i] = decoder->header.layer;
		return 0;
	case MP3_KEY_PROTECTION:
		values[i] = decoder->header.protection;
		return 0;
	case MP3_KEY_COPYRIGHT:
		values[i] = decoder->header.copyright;
		return 0;
	case MP3_KEY_ORIGINAL:
		values[i] = decoder->header.original;
		return 0;
	case MP3_KEY_EMPHASIS:
		values[i] = decoder->header.emphasis;
		return 0;
	}
	return MP3_ERROR_KEY;
}

/* each key's result in work; the last error is returned */
static int32_t mp3Control(void *handle, const int32_t *keys, uint32_t count, int32_t *values,
	uint32_t *work)
{
	Mp3Handle *mp3 = handle;
	int32_t result = 0;
	uint32_t i;

	if (!keys || !values || !work)
		return MP3_ERROR_PARAMETER;
	if ((int32_t)count < 1)
		return MP3_ERROR_EMPTY;
	for (i = 0; i < count; i++) {
		work[i] = 0;
		if (keyNeedsHandle(keys[i]) && !mp3) {
			result = MP3_ERROR_PARAMETER;
			work[i] = result;
		}
		if (work[i])
			continue;
		work[i] = controlKey(mp3, keys[i], values, i);
		if (work[i])
			result = work[i];
	}
	return result;
}

/* one frame from input; output is one interleaved buffer or one a channel */
static int32_t mp3Decode(void *handle, const uint8_t **input, uint32_t *inputSize,
	uint32_t count, int16_t **output, uint32_t *outputSize, uint32_t *frames)
{
	Mp3Handle *mp3 = handle;
	Mp3Decoder *decoder;
	uint32_t bytes;
	int32_t result;

	if (!mp3 || !input || !inputSize || !*input || !output || !outputSize || !*output ||
	    !*outputSize)
		return MP3_ERROR_PARAMETER;
	if (mp3->decoder->interleaved != 1) {
		if (!output[1] || !outputSize[1] || *frames < 2)
			return MP3_ERROR_PARAMETER;
	} else if ((int32_t)*frames < 1) {
		return MP3_ERROR_PARAMETER;
	}
	if ((int32_t)count < 1)
		return MP3_ERROR_COUNT;

	decoder = mp3->decoder;
	decoder->bitPosition = 0;
	decoder->bitEnd = *inputSize * 8 - 1;
	mp3->decoder->output = (uint8_t **)output;
	mp3->decoder->outputSizes[0] = outputSize[0];
	mp3->decoder->outputSizes[1] = outputSize[1];
	mp3->decoder->outputSize = mp3->decoder->outputSizes;
	decoder->output[0] = (uint8_t *)output[0];
	if (!decoder->interleaved)
		decoder->output[1] = (uint8_t *)output[1];
	decoder->synthesis = &decoder->synthesisState;
	decoder->outputSize = decoder->outputSizes;
	decoder->input = decoder->inputBuffer;
	mp3Copy(decoder->input, *input, *inputSize);
	result = mp3DecodeFrame(decoder);
	mp3->decoder->input = mp3->decoder->inputBuffer;
	mp3->decoder->status = result;
	*inputSize = mp3->decoder->bitsUsed >> 3;

	bytes = decoder->bytesPerSample;
	if (mp3->decoder->interleaved == 1) {
		outputSize[0] = bytes * decoder->channels * decoder->samples;
		*frames = decoder->channels ? 1 : 0;
	} else if (decoder->channels == 2) {
		outputSize[0] = bytes * decoder->samples;
		outputSize[1] = bytes * decoder->samples;
		*frames = 2;
	} else if (decoder->channels == 1) {
		outputSize[0] = bytes * decoder->samples;
		outputSize[1] = 0;
		*frames = 1;
	} else {
		*frames = 0;
		outputSize[1] = 0;
		outputSize[0] = 0;
	}
	return result ? MP3_ERROR_FRAME : 0;
}
