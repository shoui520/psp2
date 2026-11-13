/*
 * The AVC encoder's encoder engine thread (0x834588-0x8356b0): a picture
 * the stream thread prepared taken when its encode comes, its job started
 * (its access unit delimiter and parameter sets written into the stream),
 * its slices handed to the helper, and the encode answered with the NAL
 * units made. A picture the helper could not finish, and one the stream
 * thread says to skip, is a skipped picture: every macroblock P_Skip.
 *
 * The engine's memory (640 bytes): its header, the sequence, the stream's
 * information, the job and the writer, each opened while it is used.
 */
#include "service.h"

#define ENCODER_SIZE		640
#define ENCODER_HEADER_SIZE	64
#define SEQUENCE_SIZE		64
#define INFO_SIZE		64
#define ENCODE_JOB_SIZE		256
#define JOB_OFFSET		192
#define WRITER_SIZE		192
#define WRITER_OFFSET		448
#define RATE_SIZE		128
#define BUFFER_BLOCK		256	/* the writer's buffer, and the line the stream's end is read back by */

#define ALIGN64(size)		(((size) + 63) & ~63)
#define ALIGN256(size)		(((size) + 255) & ~255)

/* the engine's states */
#define ENCODER_STOPPED		0
#define ENCODER_IDLE		2	/* waiting for an encode */
#define ENCODER_STARTED		4	/* its job started */
#define ENCODER_ENCODING	5
#define ENCODER_WAITING		6	/* for the stream thread's picture */
#define ENCODER_PICTURE		7	/* the picture's kind to take */
#define ENCODER_HELPER		8	/* the job to hand to the helper */

/* the thread's flags */
#define ENCODER_FLAG_ENDED	0x00000020	/* the stream ended: encodes are answered empty */
#define ENCODER_FLAG_EXIT	0x00008000
#define ENCODER_FLAG_DONE	0x00010000	/* the picture's slices written, or its skipped picture */
#define ENCODER_FLAG_IDR	0x00080000
#define ENCODER_FLAG_KIND	0x00100000
#define ENCODER_FLAG_SKIP	0x00200000
#define ENCODER_FLAG_STARTED	0x00400000	/* the picture's job done */
#define ENCODER_FLAG_HELPER	0x00800000	/* the helper may take it */
#define ENCODER_FLAG_FINISHED	0x01000000	/* how the helper finished it: */
#define ENCODER_FLAG_FAILED	0x06000000
#define ENCODER_FLAG_REPLACE	0x08000000
#define ENCODER_FLAGS		(0x00400000 | ENCODER_FLAG_DONE | ENCODER_FLAG_EXIT | 0x100 | \
				 ENCODER_FLAG_ENDED | 0x4 | 0x1)
#define ENCODER_FLAGS_PICTURE	(ENCODER_FLAG_IDR | ENCODER_FLAG_KIND | ENCODER_FLAG_SKIP)
#define ENCODER_FLAGS_RESULT	(ENCODER_FLAG_FINISHED | ENCODER_FLAG_FAILED | ENCODER_FLAG_REPLACE)

/* the stream thread's flags, from this one */
#define STREAM_FLAG_LOOP	0x00000001
#define STREAM_FLAG_ENCODED	0x10000000
#define STREAM_FLAG_ANSWERED	0x20000000

/* the encode's contextFlags */
#define ENCODE_CABAC		0x02
#define ENCODE_OPTION		0x08

/* NAL unit types */
#define NAL_SLICE		1
#define NAL_SLICE_IDR		5

/* the picture's types in its NAL list */
#define PICTURE_IDR		4
#define PICTURE_NON_IDR		1

/* a Main profile level 3.0 picture's slices: a slice of at least 22 macroblocks */
#define PROFILE_MAIN		77
#define LEVEL_30		30
#define MB_PER_SLICE		22

/* the engine's header */
typedef struct AvcencEncoder {
	int32_t state;
	VideoAvcSequence *sequence;	/* opened raw while the thread runs */
	uint32_t info;			/* a VideoAvcStreamInfo */
	uint32_t job;			/* a VideoAvcEncodeJob */
	uint32_t writer;		/* a VideoAvcWriter */
	uint32_t request;		/* the encode being answered */
	uint32_t reserved;
	int8_t ended;			/* the stream ended: an encode is answered empty */
	int8_t idr;			/* the next picture is an IDR */
	int8_t skip;			/* it is skipped */
	int8_t failed;			/* the last picture was replaced by a skipped one */
	int8_t replaced;		/* and is followed by an IDR */
	int8_t overflow;		/* the last picture overflowed the stream's buffer */
	int8_t option;			/* the slot's */
	uint8_t reserved2;
	VkMessageQueue *encodeQueue;
	VkMessageQueue *serviceQueue;
	VkMessageQueue *wakeQueue;	/* the stream thread's pictures */
	VkMessageQueue *helperQueue;
	VkMessageQueue *innerQueue;
	VkVThreadContext *context;
	uint32_t reserved3;
} AvcencEncoder;

static AvcencEncoder *encoderBegin(uint32_t memory);
static uint32_t encoderWait(AvcencEncoder *encoder);
static void encoderEnd(AvcencEncoder *encoder);
static void pictureAnswer(AvcencEncoder *encoder);
static void pictureRate(AvcencEncoder *encoder);
static void pictureSkip(AvcencEncoder *encoder);
static void pictureFailed(AvcencEncoder *encoder);
static void pictureReplaced(AvcencEncoder *encoder);
static void streamRestart(AvcencEncoder *encoder);
static void jobStart(uint32_t sequence, uint32_t info, uint32_t job, uint32_t unused);
static void jobSkip(uint32_t sequence, uint32_t info, uint32_t job, uint32_t newFrame);
static void streamOpen(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job);
static void writeHeaders(VideoAvcSequence *sequence, const VideoAvcStreamInfo *info,
	VideoAvcEncodeJob *job);
static void writeSkipPicture(VideoAvcSequence *sequence, const VideoAvcStreamInfo *info,
	VideoAvcEncodeJob *job);

/* a job set on the engine's context, until the context takes it */
static inline void jobSet(const AvcencEncoder *encoder, VkJobFunction function, uint32_t flags)
{
	const VideoAvcSequence *sequence = encoder->sequence;
	uint32_t index = sequence->index * 8, busy;
	VkVThreadContext *locked;

	do {
		locked = vkVThreadContextLock(encoder->context);
		busy = vkVThreadJobSetNotify(locked, index, 0, function, (uint32_t)encoder->sequence,
			encoder->info, encoder->job, 0, sequence->thread, flags);
		vkVThreadContextUnlock(locked);
	} while (busy);
}

/* 0x834588: the engine's thread, until it is stopped */
void avcencEncoderThread(uint32_t memory, uint32_t codec)
{
	AvcencEncoder *encoder = encoderBegin(memory);
	VideoAvcSequence *sequence = encoder->sequence;
	VkMessageQueue *encodeQueue = encoder->encodeQueue;
	VkMessageQueue *serviceQueue = encoder->serviceQueue;
	VkMessageQueue *helperQueue = encoder->helperQueue;
	VkMessageQueue *wakeQueue = encoder->wakeQueue;
	VideoEngineRequest *request;
	uint32_t flags, kind;

	do {
		flags = encoderWait(encoder);
		if (encoder->state == ENCODER_WAITING) {
			if (vkMessageReceive(wakeQueue, (uint32_t)encoder)) {
				sequence = encoder->sequence;
				encoder->state = ENCODER_PICTURE;
			} else if (flags & ENCODER_FLAG_ENDED) {
				encoder->ended = 1;
				encoder->state = ENCODER_IDLE;
			}
		}
		if (encoder->state == ENCODER_PICTURE) {
			kind = vnzThreadWaitFlags(ENCODER_FLAGS_PICTURE);
			encoder->idr = (kind & ENCODER_FLAG_IDR) ||
				(encoder->failed && sequence->idrAfterFailure) || encoder->replaced;
			encoder->state = ENCODER_IDLE;
			encoder->skip = (kind & ENCODER_FLAG_SKIP) != 0;
		}
		if (encoder->state == ENCODER_IDLE &&
		    vkMessageReceive(encodeQueue, (uint32_t)encoder)) {
			if (encoder->ended) {
				request = (VideoEngineRequest *)encoder->request;
				request->status = 0;
				request->esSize = 0;
				request->numOfPictures = 0;
				vkMessageSend(serviceQueue, videoServicePicturesDone, encoder->request);
				streamRestart(encoder);
				encoder->state = ENCODER_WAITING;
			} else if (encoder->skip) {
				pictureSkip(encoder);
				encoder->state = ENCODER_ENCODING;
			} else {
				encoder->state = ENCODER_STARTED;
			}
		}
		if (encoder->state == ENCODER_STARTED && (flags & ENCODER_FLAG_STARTED))
			encoder->state = ENCODER_HELPER;
		if (encoder->state == ENCODER_HELPER) {
			vnzThreadWaitFlags(ENCODER_FLAG_HELPER);
			vkMessageSend(helperQueue, videoHelperStart, encoder->request);
			encoder->state = ENCODER_ENCODING;
		}
		if (encoder->state != ENCODER_ENCODING || !(flags & ENCODER_FLAG_DONE))
			continue;
		pictureRate(encoder);
		vnzThreadSetFlags(sequence->thread2, STREAM_FLAG_ENCODED);
		flags = vnzThreadWaitFlags(ENCODER_FLAGS_RESULT);
		if (flags & ENCODER_FLAG_FAILED) {
			pictureFailed(encoder);
			encoder->failed = 1;
		} else {
			encoder->failed = 0;
		}
		encoder->replaced = 0;
		if (flags & ENCODER_FLAG_REPLACE) {
			pictureReplaced(encoder);
			encoder->replaced = 1;
			encoder->failed = 1;
		}
		pictureAnswer(encoder);
		vkMessageSend(serviceQueue, videoServicePicturesDone, encoder->request);
		vnzThreadSetFlags(sequence->thread, STREAM_FLAG_LOOP);
		vnzThreadSetFlags(sequence->thread2, STREAM_FLAG_ANSWERED);
		encoder->state = ENCODER_WAITING;
	} while (encoder->state != ENCODER_STOPPED);
	encoderEnd(encoder);
}

/* 0x834754: the memory laid out and opened, the stream's state cleared */
static AvcencEncoder *encoderBegin(uint32_t memory)
{
	AvcencEncoder *header = (AvcencEncoder *)memory, *encoder;
	VideoAvcStreamInfo *info;
	VideoAvcSequence *sequence;
	VideoAvcWriter *writer;

	header->sequence = (VideoAvcSequence *)(memory + ENCODER_HEADER_SIZE);
	header->info = memory + ENCODER_HEADER_SIZE + SEQUENCE_SIZE;
	header->job = memory + JOB_OFFSET;
	header->writer = memory + WRITER_OFFSET;
	encoder = (AvcencEncoder *)vnzPublicOpenPrivate(memory, ENCODER_HEADER_SIZE);
	writer = (VideoAvcWriter *)encoder->writer;
	info = (VideoAvcStreamInfo *)encoder->info;
	sequence = encoder->sequence;
	sequence->thread = vnzThreadCurrent();
	sequence->initialized = 0;
	info->writer = (uint32_t)writer;
	writer->previousFrameNum = 0;
	writer->frameNum = 0;
	writer->idrPicId = 0;
	writer->length = 0;
	writer->publicBase = 0;
	writer->noUnit = 0;
	writer->pictures = 0;
	encoder->sequence = (VideoAvcSequence *)vnzPublicOpenRaw((uint32_t)encoder->sequence,
		SEQUENCE_SIZE);
	encoder->state = ENCODER_WAITING;
	encoder->replaced = 0;
	encoder->failed = 0;
	encoder->ended = 0;
	return encoder;
}

/* 0x8347e2 */
static void encoderEnd(AvcencEncoder *encoder)
{
	vnzPublicCloseRaw((uint32_t)encoder->sequence, SEQUENCE_SIZE);
	vnzPublicClosePrivate((uint32_t)encoder, ENCODER_HEADER_SIZE);
}

/* 0x834804: the thread's flags; an exit stops the engine */
static uint32_t encoderWait(AvcencEncoder *encoder)
{
	uint32_t flags = vnzThreadWaitFlags(ENCODER_FLAGS);

	if (flags & ENCODER_FLAG_EXIT)
		encoder->state = ENCODER_STOPPED;
	return flags;
}

/*
 * 0x83482a: a message on the inner queue: the stream thread's sequence
 * settings: the rate's, the frame number's length (to cover the IDR
 * interval), the frame rate, the NAL list
 */
void avcencEncoderSequence(uint32_t memory, uint32_t address)
{
	const AvcencEncoder *header = (const AvcencEncoder *)memory;
	const AvcencSequence *settings;
	VideoAvcSequence *sequence;
	VideoAvcStreamInfo *info;
	VideoAvcWriter *writer;
	uint32_t bits;

	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(header->writer, WRITER_SIZE);
	info = (VideoAvcStreamInfo *)vnzPublicOpenPrivate(header->info, INFO_SIZE);
	sequence = (VideoAvcSequence *)vnzPublicCloseRaw((uint32_t)header->sequence,
		SEQUENCE_SIZE);
	settings = (const AvcencSequence *)vnzPublicOpenProtected(address, REQUEST_SIZE);
	info->interval = settings->pictureInterval;
	info->bitRate = settings->pictureBitRate ? settings->pictureMaxBitRate : (uint32_t)-1;
	if (settings->numUnitsInTick && settings->timeScale) {
		sequence->numUnitsInTick = settings->numUnitsInTick;
		sequence->timeScale = settings->timeScale;
		writer->frameRate = settings->timeScale / (settings->numUnitsInTick * 2);
	}
	for (bits = 8; settings->pictureIdrInterval >> bits; bits = (uint8_t)(bits + 1))
		;
	writer->log2MaxFrameNum = bits;
	info->nals = settings->nalTable;
	info->settings = vnzPublicCloseProtected((uint32_t)settings, REQUEST_SIZE);
	vnzPublicOpenRaw((uint32_t)sequence, SEQUENCE_SIZE);
	vnzPublicClosePrivate((uint32_t)info, INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
}

/*
 * 0x834904: an encode: the request filled for the helper (the stream's
 * buffer, the job), and the picture's job started unless it is skipped
 */
void avcencEncoderEncode(uint32_t memory, uint32_t requestAddress)
{
	AvcencEncoder *encoder = (AvcencEncoder *)memory;
	const VideoAvcSequence *sequence = encoder->sequence;
	VideoEngineRequest *request;
	VideoAvcStreamInfo *info;

	encoder->request = requestAddress;
	request = (VideoEngineRequest *)vnzPublicOpenPrivate(requestAddress, REQUEST_SIZE);
	info = (VideoAvcStreamInfo *)vnzPublicOpenPrivate(encoder->info, INFO_SIZE);
	if (encoder->ended) {
		vnzPublicClosePrivate((uint32_t)info, INFO_SIZE);
		vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
		return;
	}
	request->id = sequence->id;
	request->thread = sequence->thread;
	request->queue = encoder->innerQueue;
	request->stream = encoder->info;
	request->contextFlags = 0;
	request->job = encoder->job;
	if (info->cabac)
		request->contextFlags |= ENCODE_CABAC;
	if (encoder->option)
		request->contextFlags |= ENCODE_OPTION;
	info->buffer = request->buffer;
	info->size = request->bufferSize - 512;
	info->idr = encoder->idr;
	vnzPublicClosePrivate((uint32_t)info, INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	if (!encoder->skip)
		jobSet(encoder, jobStart, ENCODER_FLAG_STARTED);
}

/* the stream written made visible to the client: written back from L2, its last line read again */
static inline const volatile uint8_t *streamFlush(uint32_t base, uint32_t size)
{
	uint32_t end = base + ALIGN256(size);

	vnzL2Writeback(base, ALIGN256(size));
	vnzL2Invalidate(end - BUFFER_BLOCK, BUFFER_BLOCK);
	return (const volatile uint8_t *)(end - BUFFER_BLOCK);
}

/*
 * 0x834a0c: the encode answered: the NAL units' place in the stream's
 * buffer, the picture's size; the next picture's numbers
 */
static void pictureAnswer(AvcencEncoder *encoder)
{
	const VideoAvcStreamInfo *info;
	const volatile uint8_t *last;
	VideoEngineRequest *request;
	VideoAvcWriter *writer;

	request = (VideoEngineRequest *)vnzPublicOpenPrivate(encoder->request, REQUEST_SIZE);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(encoder->info, INFO_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(encoder->writer, WRITER_SIZE);
	last = streamFlush(writer->publicBase, writer->length);
	request->esOffset = writer->publicBase - info->buffer;
	request->esSize = writer->length;
	request->status = writer->noUnit < 0x8000 ? 0 : writer->noUnit;
	((VideoAvcEncodeResult *)writer->commandResult)->picOrderCnt = writer->frameNum * 2;
	request->pictures = writer->commandResult;
	request->horizontal = writer->widthInMacroblocks * 16;
	request->numOfPictures = 1;
	request->vertical = writer->heightInMacroblocks * 16;
	writer->noUnit = 0;
	writer->pictures++;
	writer->previousFrameNum = writer->frameNum;
	writer->frameNum++;
	writer->picOrderCnt += 2;
	writer->picOrderCntOutput = writer->picOrderCnt;
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	(void)*last;
}

/*
 * 0x834b12: what the picture took told to the rate control (its slices'
 * bits and the rest, all of them when it overflowed); an IDR picture
 * flagged when the period before it took more than the bit rate
 */
static void pictureRate(AvcencEncoder *encoder)
{
	const VideoAvcStreamInfo *info;
	AvcencSequence *settings;
	VideoAvcNalList *nals;
	VideoAvcWriter *writer;
	uint32_t slices, other, i, type;
	int8_t overflow;

	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(encoder->info, INFO_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(encoder->writer, WRITER_SIZE);
	settings = (AvcencSequence *)vnzPublicOpenPrivate(info->settings, RATE_SIZE);
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	if (!writer->noUnit) {
		slices = 0;
		other = 0;
		for (i = 0; i < nals->count; i++) {
			type = nals->type[i];
			if (type == NAL_SLICE || type == NAL_SLICE_IDR)
				slices += nals->size[i];
			else
				other += nals->size[i];
		}
		overflow = 0;
	} else {
		overflow = 1;
		slices = 0x0fffffff;
		other = 0x0fffffff;
	}
	encoder->overflow = overflow;
	settings->headerBits = other * 8;
	settings->sliceBits = slices * 8;
	settings->skipSliceBits = info->skipSliceBytes * 8;
	settings->skipHeaderBits = info->skipHeaderBytes * 8;
	settings->overflow = encoder->overflow;
	if (info->idr) {
		if (writer->pictures && writer->gopPictures * writer->gopByteLimit <
		    writer->frameRate * writer->gopBytes)
			nals->pictureFlags |= VIDEO_AVC_PICTURE_OVER_RATE;
		writer->gopBytes = slices + other;
		writer->gopPictures = 1;
		writer->gopByteLimit = info->bitRate >> 3;
	} else {
		writer->gopBytes += slices + other;
		writer->gopPictures++;
	}
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
	vnzPublicClosePrivate((uint32_t)settings, RATE_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
}

/* 0x834c50: a skipped picture's job, in place of the helper */
static void pictureSkip(AvcencEncoder *encoder)
{
	jobSet(encoder, jobSkip, ENCODER_FLAG_DONE);
}

/*
 * 0x834cb6: the helper failed the picture: it is rewritten as a skipped
 * picture when an IDR is to follow (or it overflowed), and flagged
 */
static void pictureFailed(AvcencEncoder *encoder)
{
	const VideoAvcSequence *sequence = encoder->sequence;
	const VideoAvcStreamInfo *info;
	VideoAvcNalList *nals;

	if (sequence->idrAfterFailure || encoder->overflow)
		jobSkip((uint32_t)sequence, encoder->info, encoder->job, 1);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(encoder->info, INFO_SIZE);
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	nals->pictureFlags |= sequence->idrAfterFailure || encoder->overflow ?
		VIDEO_AVC_PICTURE_FAILED : VIDEO_AVC_PICTURE_SKIPPED;
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
}

/* 0x834d2c: the picture replaced by a skipped one, an IDR to follow */
static void pictureReplaced(AvcencEncoder *encoder)
{
	const VideoAvcStreamInfo *info;
	const VideoAvcWriter *writer;
	VideoAvcNalList *nals;

	jobSkip((uint32_t)encoder->sequence, encoder->info, encoder->job, 1);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(encoder->info, INFO_SIZE);
	writer = (const VideoAvcWriter *)vnzPublicOpenPrivate(encoder->writer, WRITER_SIZE);
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	nals->pictureFlags |= writer->pictures ? VIDEO_AVC_PICTURE_REPLACED :
		VIDEO_AVC_PICTURE_REPLACED | VIDEO_AVC_PICTURE_FIRST;
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
}

/* 0x834da4: the stream started again after its end */
static void streamRestart(AvcencEncoder *encoder)
{
	VideoAvcWriter *writer;
	uint32_t start;

	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(encoder->writer, WRITER_SIZE);
	start = writer->picOrderCntStart;
	writer->pictures = 0;
	writer->picOrderCnt = start;
	writer->picOrderCntOutput = start;
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	encoder->ended = 0;
}

/* 0x834de0: the engine's memory */
uint32_t avcencQueryContextMemory(void)
{
	return ENCODER_SIZE;
}

/*
 * 0x834de6: the stream thread's memory for `count` frames: each frame's
 * luma, chroma and the rows' data, then the rows' buffers and tables
 */
uint32_t avcencQueryStreamMemory(uint32_t count, uint32_t horizontal, uint32_t vertical)
{
	int32_t width = (horizontal + 15) & ~15, height = (vertical + 15) & ~15;
	int32_t row = ALIGN64((width + 64) / 4);
	int32_t frame, size;

	frame = width * height + height * ((width + 31) & ~31) / 2 + height * row / 4;
	size = ALIGN256(frame) * (count + 1) + row * 6 + (height >> 4) * (width >> 4) * 128 +
		1024 + ALIGN64((width >> 6) * 2) * (height >> 6);
	return ALIGN256(size);
}

/* a frame's macroblock data: 192 bytes a macroblock, half that with four reference frames */
static inline int32_t frameData(uint32_t numOfRefFrames, uint32_t horizontal, uint32_t vertical)
{
	int32_t size = (int32_t)(((horizontal + 15) & ~15) * ((vertical + 15) & ~15)) / 256 *
		384 >> 1;

	if (numOfRefFrames == 4)
		size >>= 1;
	return size;
}

/* 0x834e7c: the reference frames' memory */
uint32_t avcencQueryFrameMemory(uint32_t numOfRefFrames, uint32_t horizontal, uint32_t vertical)
{
	return ALIGN256(frameData(numOfRefFrames, horizontal, vertical)) * 2 + 512;
}

/* 0x834eb4: a frame's */
uint32_t avcencQueryFrames(uint32_t numOfRefFrames, uint32_t horizontal, uint32_t vertical)
{
	return ALIGN256(frameData(numOfRefFrames, horizontal, vertical) + 112);
}

/* each NAL unit's bytes, the slices' and the rest */
static inline void nalBytes(const VideoAvcNalList *nals, uint32_t *slices, uint32_t *other)
{
	uint32_t i, type;

	*slices = 0;
	*other = 0;
	for (i = 0; i < nals->count; i++) {
		type = nals->type[i];
		if (type == NAL_SLICE || type == NAL_SLICE_IDR)
			*slices += nals->size[i];
		else
			*other += nals->size[i];
	}
}

/*
 * 0x834eea: the engine's memory laid out for the encoder's slot: its
 * queues, the sequence from the settings; a skipped picture written once
 * to know its size, then the parameter sets for the client
 */
void avcencEncoderInitialize(uint32_t memory, VideoService *service, int32_t index)
{
	const VideoServiceSlot *slot = &service->encoders[index - service->numOfDecoders];
	const AvcencSequence *settings;
	VideoAvcStreamInfo *info;
	VideoAvcSequence *sequence;
	VideoAvcEncodeJob *job;
	VideoAvcWriter *writer;
	AvcencEncoder *encoder;
	const volatile uint8_t *last;
	uint32_t width, height, frameRate, mbWidth, mbHeight, slices, rows, rest, i;
	uint32_t sliceBytes, otherBytes;

	encoder = (AvcencEncoder *)vnzPublicOpenPrivate(memory, ENCODER_SIZE);
	vnzCacheInvalidateInside((uint32_t)encoder, ENCODER_SIZE);
	sequence = (VideoAvcSequence *)((uint32_t)encoder + ENCODER_HEADER_SIZE);
	info = (VideoAvcStreamInfo *)((uint32_t)encoder + ENCODER_HEADER_SIZE + SEQUENCE_SIZE);
	job = (VideoAvcEncodeJob *)((uint32_t)encoder + JOB_OFFSET);
	writer = (VideoAvcWriter *)((uint32_t)encoder + WRITER_OFFSET);
	encoder->encodeQueue = slot->decodeQueue;
	encoder->serviceQueue = service->engineQueue2;
	encoder->wakeQueue = slot->wakeQueue;
	encoder->innerQueue = slot->innerQueue;
	encoder->helperQueue = service->helperQueue;
	encoder->context = service->encoderContext;
	encoder->option = slot->option;
	sequence->index = index;
	sequence->id = slot->id;
	sequence->thread2 = slot->thread2;
	sequence->commandSize = service->commandSize;
	settings = (const AvcencSequence *)service->encoderOption;
	width = (settings->horizontal + 15) & ~15;
	height = (settings->vertical + 15) & ~15;
	sequence->height = height;
	sequence->width = width;
	sequence->profileIdc = settings->profile;
	sequence->levelIdc = settings->level;
	sequence->parameterSets = settings->mode;
	sequence->numUnitsInTick = settings->numUnitsInTick;
	sequence->timeScale = settings->timeScale;
	sequence->vclBitRate = settings->bitRate;
	sequence->nalBitRate = settings->maxBitRate;
	sequence->vclCpbSize = settings->cpbSize;
	sequence->nalCpbSize = settings->maxCpbSize;
	sequence->idrAfterFailure = settings->option;
	sequence->aspectRatioIdc = settings->vuiFlag;
	sequence->sarWidth = settings->vui[0];
	sequence->sarHeight = settings->vui[1];
	sequence->videoSignalPresent = settings->option3Present;
	sequence->videoFormat = settings->option3[0];
	sequence->videoFullRange = settings->option3[1];
	sequence->colourDescriptionPresent = settings->option3[2];
	sequence->colourPrimaries = settings->option3[3];
	sequence->transferCharacteristics = settings->option3[4];
	sequence->matrixCoefficients = settings->option3[5];
	sequence->cropBottom = 0;
	sequence->cropRight = 0;
	sequence->frameCropping = 0;
	if (settings->horizontal != width) {
		sequence->frameCropping = 1;
		sequence->cropRight = (int32_t)(16 - (settings->horizontal & 15)) >> 1;
	}
	if (settings->vertical != height) {
		sequence->frameCropping = 1;
		sequence->cropBottom = (int32_t)(16 - (settings->vertical & 15)) >> 1;
	}
	frameRate = settings->timeScale / (settings->numUnitsInTick * 2);
	mbWidth = (int32_t)width >> 4;
	mbHeight = (int32_t)height >> 4;
	writer->heightInMacroblocks = mbHeight;
	writer->widthInMacroblocks = mbWidth;
	writer->picStructPresent = 0;
	writer->frameNum = 0;
	writer->length = 0;
	writer->publicBase = 0;
	writer->pictures = 0;
	writer->log2MaxFrameNum = 8;
	writer->dpbOutputDelayLength = 24;
	writer->cpbRemovalDelayLength = 24;
	writer->frameRate = frameRate;
	writer->picOrderCntOutput = settings->option4;
	writer->picOrderCnt = settings->option4;
	writer->picOrderCntStart = settings->option4;
	slices = settings->numOfSlices;
	if (settings->profile == PROFILE_MAIN && settings->level == LEVEL_30 &&
	    (int32_t)(writer->widthInMacroblocks * mbHeight) / MB_PER_SLICE <= (int32_t)slices)
		slices = (int32_t)(writer->widthInMacroblocks * mbHeight) / MB_PER_SLICE;
	rows = (int32_t)mbHeight / (int32_t)slices;
	rest = (int32_t)mbHeight % (int32_t)slices;
	for (i = 0; (int32_t)i < (int32_t)slices; i++)
		info->groupRows[i] = rows + ((int32_t)i < (int32_t)rest);
	info->groups = slices;
	info->reserved3 = 0;
	info->size = BUFFER_BLOCK;
	info->buffer = settings->work;
	info->cabac = settings->entropyCoding;
	info->idr = 1;
	info->noVui = settings->fixed;
	job->stream = writer;
	job->nals = (VideoAvcNalList *)settings->streamTable;
	job->nals->count = 0;
	job->coder = (VideoAvcCabacCoder *)job->reserved2;
	writeSkipPicture(sequence, info, job);
	nalBytes((const VideoAvcNalList *)settings->streamTable, &sliceBytes, &otherBytes);
	info->skipHeaderBytes = otherBytes;
	info->skipSliceBytes = sliceBytes;
	info->size = BUFFER_BLOCK;
	info->buffer = settings->work;
	job->stream = writer;
	job->nals = (VideoAvcNalList *)settings->streamTable;
	job->nals->count = 0;
	sequence->parameterSets = 1;
	sequence->initialized = 1;
	writeHeaders(sequence, info, job);
	last = streamFlush(writer->publicBase, info->size);
	sequence->parameterSets = settings->mode;
	vnzPublicClosePrivate((uint32_t)encoder, ENCODER_SIZE);
	(void)*last;
}

/*
 * 0x835208, a job: a picture's started: its NAL list cleared, its access
 * unit delimiter and parameter sets written; for the stream's first
 * picture the writer's layout of the helper's jobs
 */
static void jobStart(uint32_t sequenceAddress, uint32_t infoAddress, uint32_t jobAddress,
	uint32_t unused)
{
	VideoAvcSequence *sequence = (VideoAvcSequence *)sequenceAddress;
	const VideoAvcStreamInfo *info;
	VideoAvcEncodeJob *job;
	VideoAvcWriter *writer;
	VideoAvcNalList *nals;
	uint32_t width, height, rows, half, jobs, group, done;

	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(infoAddress, INFO_SIZE);
	job = (VideoAvcEncodeJob *)vnzPublicOpenPrivate(jobAddress, ENCODE_JOB_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(info->writer, WRITER_SIZE);
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	job->stream = writer;
	job->nals = nals;
	nals->count = 0;
	nals->pictureFlags = 0;
	nals->reserved = 0;
	nals->pictureType = info->idr ? PICTURE_IDR : PICTURE_NON_IDR;
	nals->picOrderCnt = writer->picOrderCnt;
	nals->picOrderCntOutput = writer->picOrderCntOutput;
	writeHeaders(sequence, info, job);
	height = writer->heightInMacroblocks;
	width = writer->widthInMacroblocks;
	if (!writer->pictures) {
		rows = (int32_t)ALIGN64(sequence->commandSize * 136) / (int32_t)ALIGN64(width * 136);
		half = (int32_t)(height + 1) / 2;
		if ((int32_t)rows >= (int32_t)half)
			rows = half;
		if (height >= 33 && (int32_t)rows > 8)
			rows = 8;
		writer->rowsPerJob = rows;
		writer->unitParameters[2] = width;
		writer->unitParameters[1] = width * 16;
		writer->macroblocks = width * height;
		writer->unitParameters[3] = width * 8;
		writer->unitParameters[0] = ALIGN64(width * 16);
		writer->unitParameters[4] = width * 128;
		writer->unitParameters[5] = ALIGN64(width * 17 * 32);
		writer->unitParameters[6] = ALIGN64(width * 136);
		writer->mode = width < 40 ? 6 : 8;
		jobs = 0;
		for (group = 0; (int32_t)group < info->groups; group++)
			for (done = 0; (int32_t)done < (uint8_t)info->groupRows[group];
			    done += rows)
				jobs++;
		writer->jobs = jobs;
	}
	writer->group = 0;
	writer->row = 0;
	writer->reserved5 = 0;
	writer->firstMacroblock = 0;
	writer->newGroup = 1;
	writer->rowsLeft = info->groupRows[0];
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicClosePrivate((uint32_t)job, ENCODE_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
}

/* 0x8353c0, a job (or called): the picture written as a skipped one, a new frame number given */
static void jobSkip(uint32_t sequence, uint32_t infoAddress, uint32_t jobAddress,
	uint32_t newFrame)
{
	const VideoAvcStreamInfo *info;
	VideoAvcEncodeJob *job;
	VideoAvcWriter *writer;
	VideoAvcNalList *nals;

	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(infoAddress, INFO_SIZE);
	job = (VideoAvcEncodeJob *)vnzPublicOpenPrivate(jobAddress, ENCODE_JOB_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(info->writer, WRITER_SIZE);
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	job->stream = writer;
	job->nals = nals;
	if (newFrame)
		writer->frameNum = (uint8_t)(writer->previousFrameNum + 1);
	writeSkipPicture((VideoAvcSequence *)sequence, info, job);
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicClosePrivate((uint32_t)job, ENCODE_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
}

/* the writer set at a place in the stream's buffer, its first block opened */
static inline void streamAt(VideoAvcEncodeJob *job, VideoAvcWriter *writer, uint32_t base)
{
	writer->publicBase = base;
	writer->publicPosition = base;
	writer->offset = base;
	writer->length = 0;
	writer->buffer = vnzPublicOpenPrivate(base, BUFFER_BLOCK);
	writer->bufferSize = BUFFER_BLOCK;
	job->writer = writer;
}

/* 0x835460: the picture's stream: the buffer's half the last one did not use */
static void streamOpen(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job)
{
	VideoAvcWriter *writer = job->stream;

	if (info->buffer == writer->publicBase)
		streamAt(job, writer, info->buffer + info->size / 2);
	else
		streamAt(job, writer, info->buffer);
}

/* 0x8354b2: the stream after the buffer's end */
void avcencStreamOpenEnd(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job)
{
	streamAt(job, job->stream, info->size + info->buffer);
}

/*
 * 0x8354f2: the stream reopened where it stopped, from its line: for CABAC
 * all of the half left, else a block
 */
void avcencStreamBegin(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job)
{
	VideoAvcWriter *writer = job->stream;
	uint32_t size;

	writer->offset = writer->publicPosition & ~63;
	if (info->cabac)
		size = info->size / 2 - (writer->offset - writer->publicBase);
	else
		size = BUFFER_BLOCK;
	writer->buffer = vnzPublicOpenPrivate(writer->offset, size);
	writer->bufferSize = size;
	job->writer = writer;
}

/* 0x835546: the stream closed where the writer stopped, its partial byte kept */
void avcencStreamEnd(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job)
{
	const VideoAvcWriter *writer = job->writer;
	VideoAvcWriter *stream = job->stream;

	stream->pendingBits = *(const int8_t *)writer->position;
	vnzPublicClosePrivate(stream->buffer, stream->bufferSize);
	stream->publicPosition = stream->offset + (writer->position - writer->start);
}

/*
 * 0x83557e: a picture's first NAL units: its access unit delimiter, and
 * the parameter sets before the first picture or each IDR
 */
static void writeHeaders(VideoAvcSequence *sequence, const VideoAvcStreamInfo *info,
	VideoAvcEncodeJob *job)
{
	const VideoAvcWriter *writer = job->stream;

	streamOpen(info, job);
	avcencWriterBegin(info, job, 0);
	avcencWriteAud(info, job, 0);
	if ((sequence->parameterSets == 1 && !writer->pictures) ||
	    (sequence->parameterSets == 2 && info->idr)) {
		avcencWriteSps(sequence, info, job);
		avcencWritePps(sequence, info, job);
	}
	avcencStreamEnd(info, job);
}

/*
 * 0x8355f2: a skipped picture: its delimiter, its picture timing SEI and
 * a slice of P_Skip macroblocks per group of rows
 */
static void writeSkipPicture(VideoAvcSequence *sequence, const VideoAvcStreamInfo *info,
	VideoAvcEncodeJob *job)
{
	VideoAvcNalList *nals = job->nals;
	const VideoAvcWriter *writer = job->stream;
	uint32_t group, firstMb;

	nals->pictureFlags = 0;
	nals->reserved = 0;
	nals->pictureType = PICTURE_NON_IDR;
	nals->count = 0;
	nals->picOrderCnt = writer->picOrderCnt;
	nals->picOrderCntOutput = writer->picOrderCntOutput;
	streamOpen(info, job);
	avcencWriterBegin(info, job, 0);
	avcencWriteAud(info, job, 1);
	job->sei[3] = 0;
	job->sei[2] = 0;
	job->sei[1] = 1;
	job->sei[0] = 0;
	if (!info->noVui)
		avcencWriteSei(info, job);
	firstMb = 0;
	for (group = 0; (int32_t)group < info->groups; group++) {
		job->skipRun = (uint8_t)info->groupRows[group] * writer->widthInMacroblocks;
		avcencWriteSkipSlice(info, job, firstMb);
		firstMb += job->skipRun;
	}
	avcencStreamEnd(info, job);
}
