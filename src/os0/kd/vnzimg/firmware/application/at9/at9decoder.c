/*
 * The ATRAC9 decoder's interface (0x8809d8-0x8817b0, 0x881830-0x882770): the
 * configuration, opening and resetting a decoder, and decoding a frame
 * (or skipping it, which keeps the state in step) from the bytes of a
 * superframe. A call takes the next frame of the superframe; `used` is the
 * bytes it took.
 */
#include "at9.h"
#include "../ngs/ngs.h"

/* samples a channel in a frame, by sample rate index */
static const uint16_t frameSamples[16] = {
	64, 64, 128, 128, 128, 256, 256, 256, 64, 64, 128, 128, 128, 256, 256, 256,
};

/* the channel configurations: channels, blocks and the blocks' types */
static const int8_t channelConfigs[8][4] = {
	{ 1, 1, AT9_BLOCK_MONO, 0 },
	{ 2, 2, AT9_BLOCK_MONO, AT9_BLOCK_MONO },
	{ 2, 1, AT9_BLOCK_STEREO, 0 },
};

#define CONFIG_CHANNELS		0
#define CONFIG_BLOCKS		1
#define CONFIG_TYPES		2

/* 0x881cc0 */
static int32_t checkRateIndex(int32_t rateIndex)
{
	return rateIndex >= 0 && rateIndex < 8 ? 0 : AT9_ERROR_RATE_INDEX;
}

/* 0x881d68 */
static int32_t checkChannelConfig(int32_t channelConfig)
{
	if (channelConfig == 0 || channelConfig == 2 || channelConfig == 1)
		return 0;
	return AT9_ERROR_CHANNEL_CONFIG;
}

/* 0x881cf0 */
static int32_t checkSuperframeBytes(int32_t frames, int32_t bytes)
{
	if (bytes < 0 || bytes > (frames << 8) || bytes >= 1025)
		return AT9_ERROR_FRAME_BYTES;
	return 0;
}

/* 0x881d98 */
static int32_t checkFrames(int32_t frames)
{
	if (frames == 1 || frames == 2 || frames == 4)
		return 0;
	return AT9_ERROR_FRAMES;
}

/* 0x881740: samples a channel in a frame, or the error */
int32_t at9FrameSamples(int32_t rateIndex)
{
	int32_t error = checkRateIndex(rateIndex);

	return error ? error : frameSamples[rateIndex];
}

/* 0x8817b0: channels in the configuration, or the error */
int32_t ngsAt9ChannelCount(int32_t channelConfig)
{
	int32_t error = checkChannelConfig(channelConfig);

	return error ? error : channelConfigs[channelConfig][CONFIG_CHANNELS];
}

/* 0x881548 and 0x881538 */
int32_t ngsAt9DecoderSamples(At9DecoderState *decoder)
{
	return at9FrameSamples(decoder->rateIndex);
}

int32_t ngsAt9DecoderChannels(At9DecoderState *decoder)
{
	return ngsAt9ChannelCount(decoder->channelConfig);
}

/* 0x8814e8 and 0x881510: bytes and frames of the superframe still to come */
int32_t ngsAt9DecoderInputLeft(At9DecoderState *decoder)
{
	return decoder->superframeBytes - decoder->consumed;
}

int32_t ngsAt9DecoderOutputLeft(At9DecoderState *decoder)
{
	return decoder->framesPerSuperframe - decoder->frameIndex;
}

/*
 * 0x881558: the four bytes of configuration: 0xfe, the sample rate index
 * (4 bits), the channel configuration (3), a reserved bit, the bytes of
 * a frame less one (11), the frames of a superframe as a power of two
 * (2) and 3 more bits.
 */
int32_t ngsAt9ConfigInfo(const uint32_t *config, int32_t *rateIndex, int32_t *channelConfig,
	uint32_t *superframeBytes, uint32_t *framesPerSuperframe)
{
	At9Reader reader = { (const uint8_t *)config, 0 };
	int32_t error, frameBytes;
	uint32_t frames;

	if (at9Read(&reader, 8) != AT9_FRAME_SYNC)
		return AT9_ERROR_SYNC;
	*rateIndex = at9Read(&reader, 4);
	*channelConfig = at9Read(&reader, 3);
	if (at9ReadBit(&reader))
		return AT9_ERROR_RESERVED;
	frameBytes = at9ReadWide(&reader, 11) + 1;
	frames = 1 << at9Read(&reader, 2);
	*framesPerSuperframe = frames;
	error = checkFrames(frames);
	if (error)
		return error;
	*superframeBytes = frames * frameBytes;
	at9Read(&reader, 3);
	return 0;
}

/* 0x881b58 */
static int32_t setup(At9DecoderState *decoder, int32_t rateIndex, int32_t channelConfig,
	int32_t superframeBytes, int32_t frames, int32_t wordLength)
{
	int32_t error;

	error = checkRateIndex(rateIndex);
	if (error)
		return error;
	error = checkChannelConfig(channelConfig);
	if (error)
		return error;
	error = checkSuperframeBytes(frames, superframeBytes);
	if (error)
		return error;
	error = checkFrames(frames);
	if (error)
		return error;
	if (wordLength != 16)
		return AT9_ERROR_WORD_LENGTH_CONFIG;
	decoder->wordLength = wordLength;
	decoder->framesPerSuperframe = frames;
	decoder->superframeBytes = superframeBytes;
	decoder->channelConfig = channelConfig;
	decoder->rateIndex = rateIndex;
	return 0;
}

/* 0x881830: the decoder for a configuration; its channels' state is in work[] */
static int32_t open(At9DecoderState *decoder, int32_t channelConfig, uint8_t **work,
	int32_t rateIndex, int32_t superframeBytes, int32_t frames, int32_t wordLength,
	int32_t physical)
{
	int32_t error, channels, block, channel, next;

	if (frames < 0 || frames >= 5 || frames == 3)
		return AT9_ERROR_FLAGS;
	if (wordLength != 16)
		return AT9_ERROR_WORD_LENGTH;
	memset(decoder, 0, sizeof(*decoder));
	decoder->physical = physical;
	error = setup(decoder, rateIndex, channelConfig, superframeBytes, frames, wordLength);
	if (error < 0)
		return error;
	decoder->frameIndex = 0;
	decoder->consumed = 0;
	channels = ngsAt9ChannelCount(channelConfig);
	if (channels < 0)
		return channels;
	for (channel = 0; channel < channels; channel++)
		memset(work[channel], 0, sizeof(At9Work));

	next = 0;
	for (block = 0; block < channelConfigs[decoder->channelConfig][CONFIG_BLOCKS]; block++) {
		int32_t type = channelConfigs[decoder->channelConfig][CONFIG_TYPES + block];

		for (channel = 0; channel < at9ChannelsOfBlock(type); channel++)
			decoder->block[block].work[channel] = work[next++];
	}
	return error;
}

/* 0x8809d8 */
int32_t ngsAt9DecoderInit(At9DecoderState *decoder, int32_t channels, uint8_t **work,
	const uint32_t *config, int32_t physical)
{
	int32_t rateIndex, channelConfig;
	uint32_t superframeBytes, frames;
	int32_t error;

	error = ngsAt9ConfigInfo(config, &rateIndex, &channelConfig, &superframeBytes, &frames);
	if (error < 0)
		return error;
	if (ngsAt9ChannelCount(channelConfig) != channels)
		return -512;
	return open(decoder, channelConfig, work, rateIndex, superframeBytes, frames, 16, physical);
}

/* 0x881338: to the start of a superframe, the state of the channels forgotten */
void ngsAt9DecoderReset(At9DecoderState *decoder)
{
	int32_t blocks, block, channel;

	decoder->frameIndex = 0;
	decoder->consumed = 0;
	blocks = channelConfigs[decoder->channelConfig][CONFIG_BLOCKS];
	for (block = 0; block < blocks; block++) {
		int32_t type = channelConfigs[decoder->channelConfig][CONFIG_TYPES + block];

		decoder->block[block].quantUnitsPrevious = 0;
		for (channel = 0; channel < at9ChannelsOfBlock(type); channel++)
			memset(decoder->block[block].work[channel], 0, 96);
	}
}

/*
 * 0x881dc8: one frame's blocks read, nothing synthesized: the state of the
 * channels stays in step. Returns the bytes the frame took in *used.
 */
static int32_t parseFrame(At9DecoderState *decoder, const uint8_t *data, int32_t *used)
{
	At9Channel channels[AT9_MAX_CHANNELS];
	int32_t blocks, block, channel, error = 0;

	at9ReaderStart(decoder, data);
	blocks = channelConfigs[decoder->channelConfig][CONFIG_BLOCKS];
	for (block = 0; block < blocks; block++) {
		At9Block *current = &decoder->block[block];
		int32_t type = channelConfigs[decoder->channelConfig][CONFIG_TYPES + block];

		if (decoder->physical)
			for (channel = 0; channel < at9ChannelsOfBlock(type); channel++)
				current->work[channel] = (uint8_t *)vnzPublicOpenPrivate(
					(uint32_t)current->work[channel], sizeof(At9Work));
		error = at9DecodeBlock(decoder, current, channels, type);
		if (decoder->physical)
			for (channel = 0; channel < at9ChannelsOfBlock(type); channel++)
				current->work[channel] = (uint8_t *)vnzPublicClosePrivate(
					(uint32_t)current->work[channel], sizeof(At9Work));
		if (error < 0)
			return error;
	}
	error = at9ReaderEnd(decoder);
	if (error < 0)
		return error;
	*used = at9ReaderBytes(decoder);
	decoder->frameIndex++;
	decoder->consumed += *used;
	if (decoder->framesPerSuperframe == decoder->frameIndex) {
		decoder->consumed = 0;
		decoder->frameIndex = 0;
	}
	return error;
}

/*
 * 0x882118: one frame decoded to 16-bit PCM, `count` samples of the frame
 * from `skip`: each block's channels are dequantized, put through the
 * inverse MDCT with the previous frame's overlap and written out.
 */
static int32_t decodeFrame(At9DecoderState *decoder, const uint8_t *data, int16_t *pcm, int32_t *used,
	int32_t skip, int32_t count)
{
	At9Channel channels[AT9_MAX_CHANNELS];
	int32_t samples, channelCount, blocks, block, channel, type, first = 0, error = 0;
	int32_t bytes = decoder->wordLength >> 3;

	samples = at9FrameSamples(decoder->rateIndex);
	if (samples < 0)
		return samples;
	if (skip < 0 || count < 0 || samples < skip + count)
		return AT9_ERROR_RANGE;
	if (bytes != 2)
		return AT9_ERROR_WORD_LENGTH_CONFIG;
	channelCount = ngsAt9ChannelCount(decoder->channelConfig);
	if (channelCount < 0)
		return channelCount;

	/*
	 * The original loads the coprocessor's permutation selectors CCR8 to
	 * CCR10 (0xba987654, 0xba983210, 0xfedc7654) here (0x882220-0x882260) for
	 * its kernels; the C kernels need none.
	 */
	at9ReaderStart(decoder, data);
	blocks = channelConfigs[decoder->channelConfig][CONFIG_BLOCKS];
	for (block = 0; block < blocks; block++) {
		At9Block *current = &decoder->block[block];

		type = channelConfigs[decoder->channelConfig][CONFIG_TYPES + block];
		if (decoder->physical)
			for (channel = 0; channel < at9ChannelsOfBlock(type); channel++)
				current->work[channel] = (uint8_t *)vnzPublicOpenPrivate(
					(uint32_t)current->work[channel], sizeof(At9Work));
		error = at9DecodeBlock(decoder, current, channels + first, type);
		for (channel = 0; channel < at9ChannelsOfBlock(type); channel++) {
			At9Channel *scratch = &channels[first + channel];
			At9Work *work = (At9Work *)current->work[channel];

			if (decoder->rateIndex == AT9_RATE_48000)
				at9Imdct256(scratch, work);
			else if (decoder->rateIndex == AT9_RATE_24000)
				at9Imdct128(scratch, work);
			else
				at9Imdct64(scratch, work);
			if (decoder->physical)
				current->work[channel] = (uint8_t *)vnzPublicClosePrivate(
					(uint32_t)work, sizeof(At9Work));
		}
		if (error < 0)
			return error;
		first += at9ChannelsOfBlock(type);
	}

	if (decoder->physical && count > 0)
		pcm = (int16_t *)openPrivateMemory((uint32_t)pcm, count * bytes * channelCount);
	at9Output(decoder->channelConfig, pcm, channels, skip, count);
	if (decoder->physical && count > 0)
		pcm = (int16_t *)closePrivateMemory((uint32_t)pcm, count * bytes * channelCount);

	error = at9ReaderEnd(decoder);
	if (error < 0)
		return error;
	*used = at9ReaderBytes(decoder);
	decoder->frameIndex++;
	decoder->consumed += *used;
	if (decoder->framesPerSuperframe == decoder->frameIndex) {
		decoder->consumed = 0;
		decoder->frameIndex = 0;
	}
	return error;
}

/* the decoder and the bytes of the frame, in reach: physical addresses are opened first */
static int32_t withMemory(At9DecoderState *decoder, const uint8_t *data, int32_t *used,
	int16_t *pcm, int32_t skip, int32_t count, int32_t parse)
{
	int32_t size = ngsAt9DecoderInputLeft(decoder) + 2, result;

	if (decoder->physical) {
		decoder = (At9DecoderState *)vnzPublicOpenPrivate((uint32_t)decoder, sizeof(*decoder));
		data = (const uint8_t *)openProtectedMemory((uint32_t)data, size);
	}
	if (parse)
		result = parseFrame(decoder, data, used);
	else
		result = decodeFrame(decoder, data, pcm, used, skip, count);
	if (decoder->physical) {
		closeProtectedMemory((uint32_t)data, size);
		vnzPublicClosePrivate((uint32_t)decoder, sizeof(*decoder));
	}
	return result;
}

/* 0x880b28: the next frame read, not decoded */
int32_t ngsAt9Skip(At9DecoderState *decoder, const uint8_t *data, int32_t *used)
{
	return withMemory(decoder, data, used, NULL, 0, 0, 1);
}

/* 0x880d68: samples from skip, count of them */
static int32_t decodeMapped(At9DecoderState *decoder, const uint8_t *data, int32_t *used,
	int16_t *pcm, int32_t skip, int32_t count)
{
	return withMemory(decoder, data, used, pcm, skip, count, 0);
}

/* 0x880c80: the next frame in full */
int32_t ngsAt9Decode(At9DecoderState *decoder, const uint8_t *data, int32_t *used, int16_t *pcm)
{
	int32_t samples = at9FrameSamples(decoder->rateIndex);

	if (samples < 0)
		return samples;
	return decodeMapped(decoder, data, used, pcm, 0, samples);
}

/*
 * The next three work on a copy of the bytes the frame may use (the
 * superframe's rest), made on the stack.
 */
#define COPY_SIZE		1056

/* 0x880f00 */
int32_t ngsAt9SkipSuperframe(At9DecoderState *decoder, const uint8_t *data, uint32_t *used)
{
	uint8_t copy[COPY_SIZE];
	int32_t size = ngsAt9DecoderInputLeft(decoder), result;

	if (decoder->physical) {
		decoder = (At9DecoderState *)vnzPublicOpenPrivate((uint32_t)decoder, sizeof(*decoder));
		data = (const uint8_t *)openProtectedMemory((uint32_t)data, size);
	}
	memcpy(copy, data, size);
	result = parseFrame(decoder, copy, (int32_t *)used);
	if (decoder->physical) {
		closeProtectedMemory((uint32_t)data, size);
		vnzPublicClosePrivate((uint32_t)decoder, sizeof(*decoder));
	}
	return result;
}

/* 0x881170 */
int32_t ngsAt9DecodeRange(At9DecoderState *decoder, const uint8_t *data, uint32_t *used,
	int16_t *pcm, uint32_t skip, uint32_t count)
{
	uint8_t copy[COPY_SIZE];
	int32_t size = ngsAt9DecoderInputLeft(decoder), result;

	if (decoder->physical) {
		decoder = (At9DecoderState *)vnzPublicOpenPrivate((uint32_t)decoder, sizeof(*decoder));
		data = (const uint8_t *)openProtectedMemory((uint32_t)data, size);
	}
	memcpy(copy, data, size);
	result = decodeFrame(decoder, copy, pcm, (int32_t *)used, skip, count);
	if (decoder->physical) {
		closeProtectedMemory((uint32_t)data, size);
		vnzPublicClosePrivate((uint32_t)decoder, sizeof(*decoder));
	}
	return result;
}

/* 0x881088 */
int32_t ngsAt9DecodeSuperframe(At9DecoderState *decoder, const uint8_t *data, uint32_t *used,
	int16_t *pcm)
{
	int32_t samples = at9FrameSamples(decoder->rateIndex);

	if (samples < 0)
		return samples;
	return ngsAt9DecodeRange(decoder, data, used, pcm, 0, samples);
}
