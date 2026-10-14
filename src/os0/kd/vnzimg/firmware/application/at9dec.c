/*
 * ATRAC9 decoding for the ARM's audio codec (RPCs 128-137). The ARM keeps a
 * 128-byte control block per stream; each call maps it, maps its input and
 * output, runs a job on the stream's VThread context and waits for it.
 */
#include "application.h"
#include "ngs/ngs.h"

#define ERROR_INVALID_ARGUMENT		(-1001)
#define ERROR_INVALID_COUNT		(-1003)
#define ERROR_MEMORY			(-10001)
#define ERROR_WORD_LENGTH		(-3)
#define ERROR_SAMPLING_RATE		(-4)

#define CTRL_SIZE			128
#define DECODER_SIZE			576
#define WORD_LENGTH			16
#define CONTEXT_JOBS			8
#define MAX_BATCH			6
/* the job a batch's decodes signal, and that wakes the caller */
#define BATCH_JOB			7
#define DONE_FLAG			0x80000000

/* SceAudiodecInfoAt9 */
typedef struct At9Info {
	uint32_t size;
	uint32_t configData;
	uint32_t ch;
	uint32_t bitRate;
	uint32_t samplingRate;
	uint32_t superFrameSize;
	uint32_t framesInSuperFrame;
} At9Info;

/* the decoder's state, in memory the ARM gives */
typedef struct At9Decoder {
	uint8_t state[96];
	VkVThreadContext *context;
} At9Decoder;

/* the ARM's control block */
typedef struct At9Ctrl {
	uint32_t reserved;
	uint32_t pEs;			/* mapped while a call runs */
	uint32_t esSize;		/* the bytes given; then the bytes used */
	uint32_t pPcm;
	uint32_t pcmSize;		/* the bytes given; then the bytes written */
	uint32_t wordLength;
	uint32_t reserved2;
	int32_t result;
	At9Decoder *decoder;
	uint32_t ctrlSize;
	uint32_t reserved3[2];
	At9Info info;
	uint8_t *work[2];
	uint32_t reserved4[6];
	uint32_t decoderSize;
	int32_t inputLeft;
	int32_t outputLeft;
	uint32_t reserved5[2];
} At9Ctrl;

static uint32_t memoryOpen(uint32_t address, uint32_t size)
{
	address = enterMemoryAccessProtocol(address, size);
	if (address)
		address = openPublicMemory(address, size);
	return address;
}

static uint32_t memoryClose(uint32_t address, uint32_t size)
{
	address = closePublicMemory(address, size);
	if (address)
		address = leaveMemoryAccessProtocol(address, size);
	return address;
}

/* width bits of the configuration, from bit (big-endian) */
static uint32_t configBits(const uint8_t *config, uint32_t width, uint32_t bit)
{
	uint32_t value;

	config += bit >> 3;
	value = config[0] << 16 | config[1] << 8 | config[2];
	return ((value << (bit & 7)) & 0xffffff) >> (24 - width);
}

static uint32_t configRateIndex(const uint8_t *config)
{
	return configBits(config, 4, 8);
}

static uint32_t configSuperframeBytes(const uint8_t *config)
{
	return configBits(config, 11, 16) + 1;
}

/* the bit rate of a configuration */
static int32_t configBitRate(const uint8_t *config)
{
	int32_t rateIndex = configRateIndex(config);
	int32_t bytes = configSuperframeBytes(config);

	if (rateIndex == 7)
		return (bytes * 3 << 7) / 256;
	if (rateIndex == 4)
		return (bytes * 3 << 6) / 128;
	if (rateIndex == 1)
		return (bytes * 3 << 5) / 64;
	return 0;
}

static void decoderCounters(At9Ctrl *ctrl, At9Decoder *decoder)
{
	ctrl->inputLeft = ngsAt9DecoderInputLeft((struct At9DecoderState *)decoder);
	ctrl->outputLeft = ngsAt9DecoderOutputLeft((struct At9DecoderState *)decoder);
}

static void at9InitJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	At9Ctrl *ctrl = (At9Ctrl *)argument;
	At9Decoder *decoder = ctrl->decoder;
	int32_t rateIndex, channelConfig, result;

	if (ctrl->wordLength != WORD_LENGTH) {
		result = ERROR_WORD_LENGTH;
		goto out;
	}
	result = ngsAt9ConfigInfo(&ctrl->info.configData, &rateIndex, &channelConfig,
		&ctrl->info.superFrameSize, &ctrl->info.framesInSuperFrame);
	if (result < 0)
		goto out;
	result = ngsAt9ChannelCount(channelConfig);
	if (result < 0)
		goto out;
	if (rateIndex == 7) {
		ctrl->info.samplingRate = 48000;
	} else if (rateIndex == 4) {
		ctrl->info.samplingRate = 24000;
	} else if (rateIndex == 1) {
		ctrl->info.samplingRate = 12000;
	} else {
		result = ERROR_SAMPLING_RATE;
		goto out;
	}
	result = ngsAt9DecoderInit((struct At9DecoderState *)decoder, ctrl->info.ch, ctrl->work, &ctrl->info.configData, 1);
	if (result < 0)
		goto out;
	decoderCounters(ctrl, decoder);
out:
	ctrl->result = result;
}

/* one superframe; the result is the channel count */
static void at9DecodeJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	At9Ctrl *ctrl = (At9Ctrl *)argument;
	At9Decoder *decoder = ctrl->decoder;
	int32_t result, samples;

	ctrl->pcmSize = 0;
	result = ngsAt9DecodeSuperframe((struct At9DecoderState *)decoder, (const uint8_t *)ctrl->pEs, &ctrl->esSize,
		(int16_t *)ctrl->pPcm);
	if (result < 0)
		goto out;
	result = ngsAt9DecoderSamples((struct At9DecoderState *)decoder);
	if (result < 0)
		goto out;
	samples = result;
	result = ngsAt9DecoderChannels((struct At9DecoderState *)decoder);
	if (result < 0)
		goto out;
	ctrl->pcmSize = samples * result * 2;
	decoderCounters(ctrl, decoder);
out:
	ctrl->result = result;
}

/* count superframes */
static void at9DecodeManyJob(uint32_t argument, uint32_t count, uint32_t argument3,
	uint32_t argument4)
{
	At9Ctrl *ctrl = (At9Ctrl *)argument;
	At9Decoder *decoder = ctrl->decoder;
	const uint8_t *es = (const uint8_t *)ctrl->pEs;
	uint8_t *pcm = (uint8_t *)ctrl->pPcm;
	int32_t result, samples, channels, i;
	uint32_t used, bytes;

	ctrl->esSize = 0;
	ctrl->pcmSize = 0;
	result = ngsAt9DecoderSamples((struct At9DecoderState *)decoder);
	if (result < 0)
		goto out;
	samples = result;
	result = ngsAt9DecoderChannels((struct At9DecoderState *)decoder);
	if (result < 0)
		goto out;
	channels = result;
	for (i = 0; i < (int32_t)count; i++) {
		result = ngsAt9DecodeSuperframe((struct At9DecoderState *)decoder, es, &used, (int16_t *)pcm);
		if (result < 0)
			goto out;
		bytes = samples * channels * 2;
		es += used;
		pcm += bytes;
		ctrl->esSize += used;
		ctrl->pcmSize += bytes;
	}
	decoderCounters(ctrl, decoder);
out:
	ctrl->result = result;
}

/* skip samples, then decode count, a superframe at a time */
static void at9DecodeRangeJob(uint32_t argument, uint32_t skip, uint32_t count,
	uint32_t argument4)
{
	At9Ctrl *ctrl = (At9Ctrl *)argument;
	At9Decoder *decoder = ctrl->decoder;
	const uint8_t *es = (const uint8_t *)ctrl->pEs;
	uint8_t *pcm = (uint8_t *)ctrl->pPcm;
	uint32_t samples, channels, used, skipped, taken, bytes;
	int32_t result;

	ctrl->esSize = 0;
	ctrl->pcmSize = 0;
	result = ngsAt9DecoderSamples((struct At9DecoderState *)decoder);
	if (result < 0)
		goto out;
	samples = result;
	result = ngsAt9DecoderChannels((struct At9DecoderState *)decoder);
	if (result < 0)
		goto out;
	channels = result;

	while (skip + count != 0) {
		if (skip >= samples * 2) {
			/* the original does not check this result */
			result = ngsAt9SkipSuperframe((struct At9DecoderState *)decoder, es, &used);
			taken = 0;
			skip -= samples;
		} else {
			skipped = min(skip, samples);
			taken = samples - skipped;
			if (count < taken)
				taken = count;
			result = ngsAt9DecodeRange((struct At9DecoderState *)decoder, es, &used, (int16_t *)pcm, skipped, taken);
			if (result < 0)
				goto out;
			count -= taken;
			skip -= skipped;
		}
		bytes = taken * channels * 2;
		es += used;
		pcm += bytes;
		ctrl->esSize += used;
		ctrl->pcmSize += bytes;
	}
	decoderCounters(ctrl, decoder);
out:
	ctrl->result = result;
}

static void at9ResetJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	At9Ctrl *ctrl = (At9Ctrl *)argument;

	ngsAt9DecoderReset((struct At9DecoderState *)ctrl->decoder);
	decoderCounters(ctrl, ctrl->decoder);
	ctrl->result = 0;
}

/* a batch's decode, signalling the batch job */
static void at9BatchJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	VkVThreadContext *context = (VkVThreadContext *)argument2;

	at9DecodeJob(argument, 0, 0, 0);
	context = vkVThreadContextLock(context);
	vkVThreadJobSignal(context, BATCH_JOB);
	vkVThreadContextUnlock(context);
}

static void at9BatchDoneJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
}

/* the job on the stream's context, waited for */
static void at9Run(At9Decoder *decoder, VkJobFunction job, At9Ctrl *ctrl, uint32_t argument2,
	uint32_t argument3)
{
	VkVThreadContext *context = vkVThreadContextLock(decoder->context);

	vkVThreadJobSetNotify(context, 0, 0, job, (uint32_t)ctrl, argument2, argument3, 0,
		vnzThreadCurrent(), DONE_FLAG);
	vkVThreadContextUnlock(context);
	vnzThreadWaitFlags(DONE_FLAG);
}

/* RPC 128: the sizes the ARM allocates */
int32_t rpcAt9Sizes(uint32_t ctrlAddress)
{
	At9Ctrl *ctrl;

	if (ctrlAddress == 0)
		return ERROR_INVALID_ARGUMENT;
	ctrl = (At9Ctrl *)memoryOpen(ctrlAddress, CTRL_SIZE);
	if (ctrl == NULL)
		return ERROR_MEMORY;
	ctrl->ctrlSize = CTRL_SIZE;
	ctrl->decoderSize = DECODER_SIZE;
	if (memoryClose((uint32_t)ctrl, CTRL_SIZE) == 0)
		return ERROR_MEMORY;
	return 0;
}

/* RPC 129: a decoder for the stream, on a context of the resource */
int32_t rpcAt9Init(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	int32_t result;
	At9Ctrl *ctrl;

	if (ctrlAddress == 0)
		return ERROR_INVALID_ARGUMENT;
	if (resource == NULL)
		return ERROR_MEMORY;
	ctrl = (At9Ctrl *)memoryOpen(ctrlAddress, CTRL_SIZE);
	if (ctrl == NULL)
		return ERROR_MEMORY;
	ctrl->info.bitRate = configBitRate((const uint8_t *)&ctrl->info.configData);
	ctrl->decoder->context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	if (ctrl->decoder->context == NULL) {
		result = ERROR_MEMORY;
	} else {
		at9Run(ctrl->decoder, at9InitJob, ctrl, 0, 0);
		result = ctrl->result;
	}
	if (memoryClose((uint32_t)ctrl, CTRL_SIZE) == 0 && result == 0)
		result = ERROR_MEMORY;
	return result;
}

/* RPC 130 */
int32_t rpcAt9Release(uint32_t ctrlAddress)
{
	int32_t result = 0;
	At9Ctrl *ctrl;

	if (ctrlAddress == 0)
		return ERROR_INVALID_ARGUMENT;
	ctrl = (At9Ctrl *)memoryOpen(ctrlAddress, CTRL_SIZE);
	if (ctrl == NULL)
		return ERROR_MEMORY;
	if (ctrl->decoder->context == NULL) {
		result = ERROR_MEMORY;
	} else {
		vkVThreadContextDelete(ctrl->decoder->context);
		ctrl->outputLeft = 0;
		ctrl->inputLeft = 0;
	}
	if (memoryClose((uint32_t)ctrl, CTRL_SIZE) == 0 && result == 0)
		result = ERROR_MEMORY;
	return result;
}

/* a job with the stream's input and output mapped (output only when given, if optional) */
static int32_t at9Call(uint32_t ctrlAddress, VkJobFunction job, uint32_t argument2,
	uint32_t argument3, uint32_t outputOptional)
{
	uint32_t esSize, pcmSize;
	int32_t result;
	At9Ctrl *ctrl;

	if (ctrlAddress == 0)
		return ERROR_INVALID_ARGUMENT;
	ctrl = (At9Ctrl *)memoryOpen(ctrlAddress, CTRL_SIZE);
	if (ctrl == NULL)
		return ERROR_MEMORY;
	if (ctrl->decoder->context == NULL) {
		result = ERROR_MEMORY;
		goto close;
	}
	esSize = ctrl->esSize;
	ctrl->pEs = memoryOpen(ctrl->pEs, esSize);
	if (ctrl->pEs == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	pcmSize = ctrl->pcmSize;
	if (!outputOptional || pcmSize) {
		ctrl->pPcm = memoryOpen(ctrl->pPcm, pcmSize);
		if (ctrl->pPcm == 0) {
			result = ERROR_MEMORY;
			goto closeEs;
		}
	}
	at9Run(ctrl->decoder, job, ctrl, argument2, argument3);
	result = ctrl->result;
	if (!outputOptional || pcmSize) {
		ctrl->pPcm = memoryClose(ctrl->pPcm, pcmSize);
		if (ctrl->pPcm == 0 && result == 0)
			result = ERROR_MEMORY;
	}
closeEs:
	ctrl->pEs = memoryClose(ctrl->pEs, esSize);
	if (ctrl->pEs == 0 && result == 0)
		result = ERROR_MEMORY;
close:
	if (memoryClose((uint32_t)ctrl, CTRL_SIZE) == 0 && result == 0)
		result = ERROR_MEMORY;
	return result;
}

/* RPC 132: one superframe */
int32_t rpcAt9Decode(uint32_t ctrlAddress)
{
	return at9Call(ctrlAddress, at9DecodeJob, 0, 0, 0);
}

/* RPC 134 */
int32_t rpcAt9Reset(uint32_t ctrlAddress)
{
	int32_t result;
	At9Ctrl *ctrl;

	if (ctrlAddress == 0)
		return ERROR_INVALID_ARGUMENT;
	ctrl = (At9Ctrl *)memoryOpen(ctrlAddress, CTRL_SIZE);
	if (ctrl == NULL)
		return ERROR_MEMORY;
	if (ctrl->decoder->context == NULL) {
		result = ERROR_MEMORY;
	} else {
		at9Run(ctrl->decoder, at9ResetJob, ctrl, 0, 0);
		result = ctrl->result;
	}
	if (memoryClose((uint32_t)ctrl, CTRL_SIZE) == 0 && result == 0)
		result = ERROR_MEMORY;
	return result;
}

/* RPC 135: count superframes */
int32_t rpcAt9DecodeMany(uint32_t ctrlAddress, uint32_t count)
{
	return at9Call(ctrlAddress, at9DecodeManyJob, count, 0, 0);
}

/* RPC 137: skip samples, then decode count */
int32_t rpcAt9DecodeRange(uint32_t ctrlAddress, uint32_t skip, uint32_t count)
{
	return at9Call(ctrlAddress, at9DecodeRangeJob, skip, count, 1);
}

/* the input (or output) of each stream of a batch mapped; on a failure none */
static int32_t batchOpen(At9Ctrl *ctrls, uint32_t *sizes, uint32_t count, uint32_t output);

static int32_t batchClose(At9Ctrl *ctrls, const uint32_t *sizes, uint32_t count, uint32_t output)
{
	int32_t result = 0;
	uint32_t i, *address;

	for (i = 0; i < count; i++) {
		address = output ? &ctrls[i].pPcm : &ctrls[i].pEs;
		*address = memoryClose(*address, sizes[i]);
		if (*address == 0 && result == 0)
			result = ERROR_MEMORY;
	}
	return result;
}

static int32_t batchOpen(At9Ctrl *ctrls, uint32_t *sizes, uint32_t count, uint32_t output)
{
	uint32_t i, *address;

	for (i = 0; i < count; i++) {
		address = output ? &ctrls[i].pPcm : &ctrls[i].pEs;
		sizes[i] = output ? ctrls[i].pcmSize : ctrls[i].esSize;
		*address = memoryOpen(*address, sizes[i]);
		if (*address == 0) {
			batchClose(ctrls, sizes, i, output);
			return ERROR_MEMORY;
		}
	}
	return 0;
}

/* RPC 136: a superframe for each of count streams, decoded in parallel */
int32_t rpcAt9DecodeBatch(uint32_t ctrlsAddress, uint32_t count)
{
	uint32_t esSizes[MAX_BATCH], pcmSizes[MAX_BATCH], size, i;
	VkVThreadContext *context = NULL, *locked;
	int32_t result, closed;
	At9Ctrl *ctrls;

	if (ctrlsAddress == 0)
		return ERROR_INVALID_ARGUMENT;
	if (count == 0 || count >= MAX_BATCH + 1)
		return ERROR_INVALID_COUNT;
	size = count * CTRL_SIZE;
	ctrls = (At9Ctrl *)memoryOpen(ctrlsAddress, size);
	if (ctrls == NULL)
		return ERROR_MEMORY;

	result = 0;
	for (i = 0; i < count; i++) {
		context = ctrls[i].decoder->context;
		if (context == NULL) {
			result = ERROR_MEMORY;
			goto close;
		}
	}
	/* all run on the last stream's context */
	result = batchOpen(ctrls, esSizes, count, 0);
	if (result < 0)
		goto close;
	result = batchOpen(ctrls, pcmSizes, count, 1);
	if (result < 0)
		goto closeEs;

	locked = vkVThreadContextLock(context);
	for (i = 0; i < count; i++)
		vkVThreadJobSet(locked, i, 0, at9BatchJob, (uint32_t)&ctrls[i], (uint32_t)context, 0, 0);
	vkVThreadJobSetNotify(locked, BATCH_JOB, count, at9BatchDoneJob, 0, 0, 0, 0,
		vnzThreadCurrent(), DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	for (i = 0; i < count; i++)
		if (result == 0)
			result = ctrls[i].result;

	closed = batchClose(ctrls, pcmSizes, count, 1);
	if (closed < 0 && result == 0)
		result = closed;
closeEs:
	closed = batchClose(ctrls, esSizes, count, 0);
	if (closed < 0 && result == 0)
		result = closed;
close:
	if (memoryClose((uint32_t)ctrls, size) == 0 && result == 0)
		result = ERROR_MEMORY;
	return result;
}
