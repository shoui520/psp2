/*
 * FW2's codec: its commands, the open and the run. A run reads the records the
 * parser wrote and takes each (the parameter sets, the SEI messages, the slice
 * headers and data) until one ends the picture or the stream, or the records
 * run out; then it hands decoded pictures to the ARM side, in output order
 * (bumpPictures) or when they are due (outputOnTime).
 */
#include "codec.h"

/* VIP_CB_CLOCK_ENABLE besides the picture units' */
#define VIP_CLOCK_PREDICTION		0x04	/* the prediction unit's */
#define VIP_CLOCK_UNIT5			0x08

/* how often a reset reads the prediction unit's command before it stops it */
#define PREDICTION_POLLS		0x8000000

/*
 * The interrupts a run takes: the decoder's and the read stream's (5 to 8) at
 * level 1, and 0 to 3 enabled.
 */
#define RUN_INTERRUPTS		(1u << VIP_INTERRUPT_DECODER | 1u << VIP_INTERRUPT_READ_STREAM0 | \
	1u << VIP_INTERRUPT_READ_STREAM1 | 1u << VIP_INTERRUPT_8)
#define RUN_LEVELS		(1u << 4 * VIP_INTERRUPT_DECODER | \
	1u << 4 * VIP_INTERRUPT_READ_STREAM0 | 1u << 4 * VIP_INTERRUPT_READ_STREAM1)
#define RUN_LEVEL_8		(1u << 4 * (VIP_INTERRUPT_8 - 8))
#define RUN_ENABLED		0xf

/* VIP_CODEC_QUERY's answer is in 8 KB */
#define QUERY_SHIFT		13	/* VIP_WORK_ROW_SHIFT */
#define QUERY_MOTION_ROWS	5	/* per macroblock row, without direct_8x8_inference_flag */

/* the open's checks */
#define OUTPUT_RING_MINIMUM	0x110
#define AREA_ADDRESS_SIZE	4	/* a work size of one word: one picture's address */
#define AREA_LIST_LIMIT		0x99	/* below this, the work size is an area list's */
#define WORK_BLOCK_MINIMUM	0x28000
#define WORK_BLOCK_MAXIMUM	0x2000000
#define WORK_BLOCK_ALIGNMENT	0x2000
#define NO_COLOUR		0xffffff00	/* VipFrameInfo.colour */

/* decodeRecords' results: below 1, the run goes on */
#define RUN_CONTINUE		0
#define RUN_PICTURE_END		1
#define RUN_STREAM_END		2
#define RUN_FAILED		3
#define RUN_NO_BUFFER		4
#define RUN_BUSY		5	/* frames given out have not come back */

/* how long a run waits when it skips, or has no records (vipWaitTimer) */
#define SKIP_WAIT		0x1000
#define INPUT_WAIT		0x4000

/* the context's globals */
VipCodecGlobals g_codec VIP_GLOBAL = {
	.options = VIP_CODEC_OPTIONS_CLOSED,
};
VipCodecDecoder g_decoder VIP_GLOBAL;
int8_t g_randomAccess VIP_GLOBAL;
uint8_t g_lastBottomField VIP_GLOBAL;
int8_t g_skipping VIP_GLOBAL;
uint8_t g_late VIP_GLOBAL;
int8_t g_drainState VIP_GLOBAL;
uint64_t g_outputClock VIP_GLOBAL;
uint32_t g_outputClockFraction VIP_GLOBAL;
uint32_t g_seiPresent VIP_GLOBAL;
uint32_t g_userDataArea VIP_GLOBAL;
uint32_t g_userDataSlot VIP_GLOBAL;
VipSps *VIP_GLOBAL g_sps;
VipPps *VIP_GLOBAL g_pps;
uint32_t g_underrunCount VIP_GLOBAL;
uint32_t g_pictureCount VIP_GLOBAL;
uint32_t g_outputCount VIP_GLOBAL;
uint8_t g_outputState VIP_GLOBAL;
uint32_t g_outputStartFrame VIP_GLOBAL;
uint64_t g_delimiterTime VIP_GLOBAL;
uint32_t g_untimedDelimiters VIP_GLOBAL;
VipFrameExtension g_extensions[VIP_CODEC_FRAMES] VIP_GLOBAL;
uint8_t g_vui[VIP_SPS_COUNT][VIP_EXTENSION_VUI_SIZE] VIP_GLOBAL;
VipSeiMessages g_sei VIP_GLOBAL;

/* the parameter sets, past $gp's reach */
static VipSps g_spsArea[VIP_SPS_COUNT] VIP_GLOBAL_FAR;
static VipPps g_ppsArea[VIP_PPS_COUNT] VIP_GLOBAL_FAR;

/* local RAM: kept with the context, and a command's */
VipRingSegment g_sliceSegments[2] VIP_CODEC_SAVED;
VipPictureBuffer *g_currentBuffer VIP_CODEC_SAVED;
uint32_t g_dataStart VIP_CODEC_LOCAL;
uint8_t g_sliceSegmentCount VIP_CODEC_LOCAL;
VipCodecDma g_codecDma VIP_CODEC_LOCAL __attribute__((aligned(8)));

/* The core's reset hook (VIP_RESET_HOOKS): stop the units a run left on. */
void codecResetHardware(void)
{
	uint32_t clock, i;

	if (vipReadControlBus(VIP_CB_CLOCK_REQUEST) & 1) {
		vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 0);
		vipWriteControlBus(VIP_CB_UNIT_ENABLE(1), 0);
		vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 0);
		vipWriteControlBus(VIP_CB_UNIT_ENABLE(3), 0);
		vipWriteControlBus(VIP_CB_UNIT_ENABLE(4), 0);
	}
	clock = vipReadControlBus(VIP_CB_CLOCK_ENABLE);
	if (clock & VIP_CLOCK_UNIT5)
		vipWriteControlBus(VIP_CB_UNIT_ENABLE(5), 0);
	if ((clock & VIP_CLOCK_PREDICTION) && vipReadControlBus(VIP_CB_PREDICTION_ENABLE)) {
		for (i = 0; i <= PREDICTION_POLLS; i++)
			vipReadControlBus(VIP_CB_PREDICTION_COMMAND);
		vipWriteControlBus(VIP_CB_PREDICTION_ENABLE, 0);
	}
}

/*
 * VIP_CODEC_QUERY 0: the work memory so many frames of a picture size need. -1:
 * another query, -2: they do not fit in the most there may be.
 */
static int queryWorkMemory(const uint32_t *arguments, uint32_t *output)
{
	uint32_t rows;

	if (arguments[0])
		return -1;
	rows = workMemoryRows((arguments[1] + 15) >> 4, (arguments[2] + 15) >> 4, arguments[3],
		WORK_BLOCK_MAXIMUM, QUERY_MOTION_ROWS);
	if (!rows)
		return -2;
	*output = rows << QUERY_SHIFT;
	return 0;
}

/* VIP_CODEC_SET_OPTION 0: the option byte, the top of the value */
static int setOption(const uint32_t *arguments)
{
	if (arguments[0])
		return -1;
	g_codec.options = (arguments[1] >> 24 & VIP_CODEC_OPTIONS_SET) | VIP_CODEC_OPTION_BROKEN_LINK;
	return 0;
}

/* Point g_sps and g_pps at this context's parameter sets. */
static void placeParameterSets(void)
{
	g_sps = vipContextAddress(g_spsArea[0]);
	g_pps = vipContextAddress(g_ppsArea[0]);
}

/*
 * VIP_CODEC_OPEN: check the arguments, take the rings, the frame stores and the
 * work memory, and open the decoder. -2: an argument is missing, -3: a ring is
 * not aligned or the output ring too small, -4: the work size is wrong, -5: the
 * area count, -6: the work memory is not aligned.
 */
static int openCodec(const VipCodecOpen *open)
{
	const uint32_t *arguments = (const uint32_t *)open;
	VipFrameExtension *extension = g_extensions;
	uint32_t kind = VIP_WORK_BLOCK, size, count, offset, i;

	for (i = 0; i < 6; i++) {
		if (i != 4 && !arguments[i])	/* the work memory may be 0 */
			return -2;
	}
	if (open->input->start && (open->input->start & 7))
		return -3;
	if (open->records->start && (open->records->start & 7))
		return -3;
	if (open->output->start &&
	    ((open->output->start & 7) || open->output->size < OUTPUT_RING_MINIMUM))
		return -3;

	size = open->workSize;
	if (size < AREA_LIST_LIMIT) {
		if (size < AREA_ADDRESS_SIZE || (size != AREA_ADDRESS_SIZE && (size & 7)))
			return -4;
		kind = VIP_WORK_AREAS;
	} else if (size < WORK_BLOCK_MINIMUM || (size & (WORK_BLOCK_ALIGNMENT - 1)) ||
		   size > WORK_BLOCK_MAXIMUM) {
		return -4;
	}
	count = open->areaCount;
	if (count > VIP_CODEC_AREAS)
		return -5;
	if (kind == VIP_WORK_AREAS &&
	    (!count || (count >= 2 && size < count * sizeof(VipWorkArea))))
		return -5;
	if (open->workMemory & (kind == VIP_WORK_AREAS ? 3 : 0x1f))
		return -6;

	clearCodecUnitState();
	g_lastBottomField = VIP_NO_FIELD;
	g_drainState = kind != VIP_WORK_AREAS;
	g_skipping = 0;
	g_late = 0;
	g_decoder.rings[VIP_RING_SLICES].ring = open->input;
	g_decoder.rings[VIP_RING_SLICES].cursor = VIP_RING_WRITER + 1;
	g_decoder.rings[VIP_RING_RECORDS].ring = open->records;
	g_decoder.rings[VIP_RING_RECORDS].cursor = VIP_RING_WRITER + 1;
	g_decoder.rings[VIP_RING_OUTPUT].ring = open->output;
	g_decoder.rings[VIP_RING_OUTPUT].cursor = VIP_RING_WRITER;
	g_decoder.rings[VIP_RING_USER_DATA].ring = NULL;
	g_decoder.rings[VIP_RING_USER_RECORDS].ring = NULL;
	if (kind == VIP_WORK_AREAS) {
		const VipWorkArea *areas = (const VipWorkArea *)open->workMemory;

		g_decoder.workMemory = (uint32_t)g_decoder.areas;
		if (open->areaCount < 2) {
			g_decoder.areas[0].picture = areas[0].picture;
		} else {
			for (i = 0; i < open->areaCount; i++)
				g_decoder.areas[i] = areas[i];
		}
	} else {
		g_decoder.workMemory = open->workMemory;
	}
	g_userDataArea = open->userData;
	g_userDataSlot = open->userDataSlot;

	/* each frame's user data: a slot for each field */
	vipZeroDram((uint32_t)open->frames, VIP_CODEC_FRAMES * sizeof(VipFrame));
	offset = open->userDataSlot;
	for (i = 0; i < VIP_CODEC_FRAMES; i++, extension++) {
		g_decoder.frames[i].frame = &open->frames[i];
		open->frames[i].info.colour = NO_COLOUR;
		open->frames[i].info.extension = extension;
		extension->userDataOffset = offset;
		offset += 2 * open->userDataSlot;
	}
	g_decoder.workKind = kind;
	g_decoder.workSize = open->workSize;
	g_decoder.areaCount = open->areaCount;
	placeParameterSets();
	return openDecoder(&g_decoder);
}

/* A record the codec has not read, or data. */
static int hasInput(void)
{
	const VipRing *records = g_decoder.rings[VIP_RING_RECORDS].ring;
	const VipRing *slices = g_decoder.rings[VIP_RING_SLICES].ring;

	/* the writer's cursor, and the codec's (the first reader's) */
	if (records->cursors[0].position != records->cursors[1].position)
		return 1;
	return slices->cursors[0].position != slices->cursors[1].position;
}

static int decodeRecords(VipCodecDecoder *decoder, uint32_t picture, uint32_t colocated,
	uint32_t which);
static int flushOutput(void);
static void endOutput(void);
static int outputOnTime(void);

/* VIP_CODEC_RUN: decode until a picture or the stream ends, or nothing more can be. */
static int runCodec(const uint32_t *arguments)
{
	uint32_t work = g_decoder.workMemory, clock, pending, level0, level1, clear, enable;
	int result;

	if ((g_codec.options & VIP_CODEC_OPTION_NEED_INPUT) && !hasInput())
		return VIP_CODEC_NO_INPUT;
	if (g_decoder.workKind == VIP_WORK_AREAS)
		work = ((const VipWorkArea *)work)->picture;

	/* the prediction unit's view of the work memory */
	mep_di();
	clock = vipReadControlBus(VIP_CB_CLOCK_ENABLE);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, clock | VIP_CLOCK_PREDICTION);
	vipWriteControlBus(VIP_CB_PREDICTION_SETUP, work >> 24);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, clock | ~VIP_CLOCK_PREDICTION);	/* as it is */
	__asm__ __volatile__("nop\n\tnop\n\tnop");
	mep_ei();

	mep_di();
	pending = vipReadControlBus(VIP_CB_INTERRUPT_PENDING);
	level0 = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL0);
	level1 = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1);
	clear = vipReadControlBus(VIP_CB_INTERRUPT_CLEAR);
	enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING, pending | RUN_INTERRUPTS);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0, level0 | RUN_LEVELS);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0, level1 | RUN_LEVEL_8);	/* as it is */
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, clear & ~RUN_ENABLED);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable | RUN_ENABLED);
	__asm__ __volatile__("nop\n\tnop\n\tnop");
	mep_ei();

	beginDecode(&g_decoder);
	g_listsDone = 0;
	g_colocatedDone = 0;
	do {
		result = decodeRecords(&g_decoder, 0, 0, arguments[2]);
		if (result == RUN_NO_BUFFER) {
			if (g_codec.options & VIP_CODEC_OPTION_ON_TIME)
				result = flushOutput();
			else
				bumpPictures();
		}
	} while (result < 1);

	switch (result) {
	case RUN_PICTURE_END:
		if (g_codec.options & VIP_CODEC_OPTION_ON_TIME)
			outputOnTime();
		else
			bumpPictures();
		return VIP_RESULT_OK;
	case RUN_STREAM_END:
		endOutput();
		g_codec.mode = VIP_CODEC_ENDED;
		return VIP_RESULT_OK;
	case RUN_FAILED:
		endOutput();
		g_codec.mode = VIP_CODEC_ENDED;
		vipSetState(VIP_CODEC_ENDED);
		return VIP_PROGRAM_CONTINUE;
	case RUN_NO_BUFFER:
	case RUN_BUSY:
		return VIP_CODEC_HELD;
	default:
		endOutput();
		vipTraceEvent(VIP_CODEC_TRACE_NOT_PAIRED, g_pictureCount);
		return VIP_RESULT_OK;
	}
}

/* The codec's commands besides the one a mode takes, which are in the wrong mode. */
static int otherCommand(uint32_t kind)
{
	if (kind == VIP_CODEC_OPEN || kind == VIP_CODEC_QUERY || kind == VIP_CODEC_SET_OPTION ||
	    kind == VIP_CODEC_RUN || kind == VIP_CODEC_CLOSE)
		return VIP_CODEC_WRONG_MODE;
	return VIP_CODEC_NO_COMMAND;
}

/* The program's entry (VipProgram.entry): the command, by the codec's mode. */
int codecEntry(uint32_t *output)
{
	const uint32_t *command, *arguments;
	uint32_t kind;
	int result = VIP_RESULT_OK;

	vipSetState(g_codec.mode);
	command = vipGetCommand();
	if (!command)
		return VIP_CODEC_NO_COMMAND;
	kind = command[2];
	arguments = command + 3;
	switch (g_codec.mode) {
	case VIP_CODEC_CLOSED:
		if (kind == VIP_CODEC_OPEN) {
			result = openCodec((const VipCodecOpen *)arguments);
			if (result) {
				*output = result;
				result = VIP_CODEC_FAILED;
				break;
			}
			g_codec.mode = VIP_CODEC_OPENED;
			vipSetState(VIP_CODEC_OPENED);
		} else if (kind == VIP_CODEC_QUERY) {
			if (queryWorkMemory(arguments, output))
				result = VIP_CODEC_FAILED;
		} else if (kind == VIP_CODEC_SET_OPTION) {
			if (setOption(arguments))
				result = VIP_CODEC_FAILED;
		} else if (kind == VIP_CODEC_RUN) {
			result = VIP_CODEC_WRONG_MODE;
		} else {
			result = VIP_CODEC_NO_COMMAND;	/* VIP_CODEC_CLOSE too */
		}
		break;
	case VIP_CODEC_OPENED:
		if (kind == VIP_CODEC_RUN)
			result = runCodec(arguments);
		else
			result = otherCommand(kind);
		break;
	case VIP_CODEC_ENDED:
		if (kind == VIP_CODEC_CLOSE) {
			closeDecoder(&g_decoder);
			g_codec.options = VIP_CODEC_OPTIONS_CLOSED;
			g_codec.mode = VIP_CODEC_CLOSED;
		} else {
			result = otherCommand(kind);
		}
		break;
	default:
		result = VIP_CODEC_BAD_MODE;
		break;
	}
	vipSetState(g_codec.mode);
	return result;
}

/* the picture units' clocks: on while a run uses them */
void enableDecoderClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);
	uint32_t enable, request;

	mep_di();
	enable = vipReadControlBus(VIP_CB_CLOCK_ENABLE);
	request = vipReadControlBus(VIP_CB_CLOCK_REQUEST);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, enable | VIP_CLOCK_PICTURE_UNITS);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST, request | VIP_CLOCK_REQUEST_UNITS);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

void gateDecoderClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);
	uint32_t enable, request;

	mep_di();
	enable = vipReadControlBus(VIP_CB_CLOCK_ENABLE);
	request = vipReadControlBus(VIP_CB_CLOCK_REQUEST);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, enable & ~VIP_CLOCK_PICTURE_UNITS);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST, request & ~VIP_CLOCK_REQUEST_UNITS);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/* Write the parse results readParsed left in local RAM to the context. */
void saveParsed(void *destination, uint32_t size)
{
	volatile uint32_t done = 1;

	g_codecDma.staging[0].external = (uint32_t)destination;
	g_codecDma.staging[0].localLength = (uint32_t)g_parsed << 16 | size;
	queueListDma(VIP_DMA_LIST | VIP_DMA_WRITE, VIP_STAGING_LIST(0), 0, 0, NULL, &done);
	while (done == 1)
		;
}

/*
 * Copy the user data the picture collected (at the start of the user data area)
 * to the frame's slot for the field, through the staging buffer.
 */
static void copyUserData(const VipFrameExtension *extension, uint32_t field)
{
	uint32_t source = g_userDataArea, destination, size, chunk;
	volatile uint32_t done;

	if (!source)
		return;
	size = (g_sei.userData & VIP_USER_DATA_SIZE) << 3;
	if (!size)
		return;
	destination = extension->userDataOffset;
	if (field)
		destination += g_userDataSlot;
	destination += source;
	do {
		chunk = size < VIP_STAGING_SIZE ? size : VIP_STAGING_SIZE;
		size -= chunk;
		g_codecDma.staging[0].external = source;
		g_codecDma.staging[1].external = destination;
		g_codecDma.staging[0].localLength = VIP_STAGING << 16 | chunk;
		g_codecDma.staging[1].localLength = VIP_STAGING << 16 | chunk;
		source += chunk;
		destination += chunk;
		queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ, VIP_STAGING_LIST(0),
			0, 0, NULL, NULL);
		queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_WRITE, VIP_STAGING_LIST(1),
			0, 0, NULL, &done);
		while (done == 1)
			;
	} while (size);
}

/*
 * Copy the field's SEI messages (g_sei) and its SPS's VUI to the frame's
 * extension, through the staging buffer; the user data is taken.
 */
static void copyExtensionData(void *sei, void *vui, const void *spsVui)
{
	volatile uint32_t done;

	g_codecDma.staging[0].external = (uint32_t)&g_sei;
	g_codecDma.staging[0].localLength = VIP_STAGING << 16 | sizeof(VipSeiMessages);
	g_codecDma.staging[1].external = (uint32_t)spsVui;
	g_codecDma.staging[1].localLength = (VIP_STAGING + sizeof(VipSeiMessages)) << 16 |
		VIP_EXTENSION_VUI_SIZE;
	g_codecDma.staging[2].external = (uint32_t)sei;
	g_codecDma.staging[2].localLength = g_codecDma.staging[0].localLength;
	g_codecDma.staging[3].external = (uint32_t)vui;
	g_codecDma.staging[3].localLength = g_codecDma.staging[1].localLength;
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
		VIP_STAGING_LIST(0) + sizeof(VipDmaDescriptor), 0, 0, NULL, NULL);
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_WRITE,
		VIP_STAGING_LIST(2) + sizeof(VipDmaDescriptor), 0, 0, NULL, &done);
	while (done == 1)
		;
	g_sei.userData = 0;
}

/* -1 while a frame given to the ARM side has not come back. */
static int framesOutstanding(void)
{
	uint32_t count = g_dpbCount, i;

	for (i = 0; i < count; i++) {
		VipPictureBuffer *buffer = g_dpbByPoc[i];

		if (buffer->idle && !getFrameAvailable(buffer->frame))
			return -1;
	}
	return 0;
}

/*
 * No picture buffer comes free: once every frame is back, the DPB's pictures go
 * unshown and the decoding waits for a random access point again. RUN_NO_BUFFER
 * while frames are out.
 */
static int flushOutput(void)
{
	uint32_t count, i;

	if (framesOutstanding())
		return RUN_NO_BUFFER;
	count = g_dpbCount;
	for (i = 0; i < count; i++) {
		VipPictureBuffer *buffer = g_dpbByPoc[i];

		buffer->reference = VIP_REFERENCE_NONE;
		buffer->topReference = VIP_REFERENCE_NONE;
		buffer->bottomReference = VIP_REFERENCE_NONE;
		buffer->idle = 1;
	}
	g_referenceFrameCount = 0;
	g_late = 0;
	resetUnits();
	g_outputState = VIP_OUTPUT_WAITING;
	g_generation++;
	vipTraceEvent(VIP_CODEC_TRACE_RESTART, g_pictureCount);
	vipNotifyEvents(VIP_CODEC_EVENT_RESTART);
	return 0;
}

/*
 * Give a decoded picture's frame to the ARM side, and its record to the output
 * ring (dpb.c outputPicture): it leaves the DPB.
 * After a random access the output starts at the picture the stream named: one
 * before it is dropped (-1).
 */
int handOutPicture(VipPictureBuffer *buffer)
{
	uint32_t state = g_outputState;

	if (state < VIP_OUTPUT_RUNNING) {
		if (g_outputStartFrame != buffer->frameNum) {
			buffer->idle = 1;
			return -1;
		}
		g_outputState = VIP_OUTPUT_RUNNING;
		setFrameAvailable(buffer->frame, 0);
		outputPicture(buffer);
		return 0;
	}
	setFrameAvailable(buffer->frame, 0);
	outputPicture(buffer);
	if (state == VIP_OUTPUT_RESUMING)
		g_outputState = g_outputStartFrame == buffer->frameNum ?
			VIP_OUTPUT_RUNNING : VIP_OUTPUT_STARTING;
	return 0;
}

/* the records that end a run */
static inline int endsRun(uint32_t type)
{
	return type == VIP_PARSED_PICTURE_END || type == VIP_PARSED_STREAM_END ||
		type == VIP_PARSED_START;
}

/*
 * Drop the records before the next one that ends a run (or the last), and their
 * data. 0 if there was one, -1 if not or there were no records.
 */
static int skipToPictureEnd(void)
{
	VipRingHandle *records = g_ringHandles[VIP_RING_RECORDS];
	VipRingSegment segments[2];
	const VipRing *ring;
	VipRecord *record, *end, *found = NULL;
	uint32_t count, i;
	int result = -1;

	segments[1].size = 0;
	getRingReadable(records, segments, 2);
	count = (segments[0].size + segments[1].size) / sizeof(VipRecord);
	if (!count)
		return -1;
	ring = records->ring;
	end = (VipRecord *)(ring->start + ring->size);
	record = (VipRecord *)segments[0].start;
	for (i = 0; i < count; i++, record++) {
		if (record == end)
			record = (VipRecord *)ring->start;
		found = record;
		if (endsRun(record->header >> VIP_RECORD_TYPE_SHIFT)) {
			result = 0;
			break;
		}
	}
	if (found) {
		setRingReader(g_ringHandles[VIP_RING_RECORDS], (uint32_t)found);
		setRingReader(g_ringHandles[VIP_RING_SLICES], found->position);
	}
	return result;
}

/* A picture's (or first field's) frame gets its display information and SPS's. */
static void publishFrameInfo(VipPictureBuffer *buffer)
{
	VipFrameHandle *frame = buffer->frame;
	const VipSps *sps = g_currentSps;
	VipFrameInfo info = g_frameInfo;
	uint32_t level;
	uint8_t type;

	setFrameInfo(frame, &info);
	setFrameChromaLocTop(frame, sps->chromaSampleLocTypeTopField);
	setFrameChromaLocBottom(frame, sps->chromaSampleLocTypeBottomField);
	setFrameNumUnitsInTick(frame, sps->numUnitsInTick);
	setFrameTimeScale(frame, sps->timeScale);
	setFrameProfile(frame, sps->profileIdc);
	level = sps->levelIdc;
	setFrameLevel(frame, level);
	if (level < 31 && sps->picWidthInMbsMinus1 >= 76)
		g_seiPresent |= VIP_SEI_WIDE_LOW_LEVEL;
	setFrameAspect(frame, 16, sps->sarWidth, sps->sarHeight);
	if (!g_sliceHeader.fieldPic) {
		getFrameType(frame, &type);
		setFramePictureType(frame, type);
	}
}

/* A second field's: the flags, and its picture type by slice_type. */
static void publishSecondField(VipPictureBuffer *buffer)
{
	VipFrameHandle *frame = buffer->frame;
	uint32_t type;

	frame->frame->info.flags = g_frameInfo.flags;
	switch (g_sliceType) {	/* the cases past VIP_SLICE_TYPE_I as they are */
	case 2:
	case 7:
		type = g_sliceHeader.idr ? VIP_FRAME_TYPE_IDR : VIP_FRAME_TYPE_I;
		break;
	case 1:
	case 6:
		type = VIP_FRAME_TYPE_B;
		break;
	case 0:
	case 5:
		type = VIP_FRAME_TYPE_P;
		break;
	default:
		type = 0;
		break;
	}
	setFramePictureType(frame, type);
}

/* The field's SEI messages, user data and its SPS's VUI go to its frame's extension. */
static void keepExtension(VipPictureBuffer *buffer, uint32_t field)
{
	VipFrameExtension *extension = buffer->frame->frame->info.extension;

	extension->seiPresent = g_seiPresent | (uint8_t)buffer->ptsValid;
	copyUserData(extension, field);
	copyExtensionData(extension->sei[field], extension->vui,
		g_vui[g_currentPps->spsId]);
}

/*
 * A picture's last slice is decoded: mark it and keep what came with it. After
 * a first field the units wait for the second.
 */
void finishPicture(void)
{
	VipPictureBuffer *buffer = g_currentBuffer;
	uint32_t fieldPic = g_sliceHeader.fieldPic;

	if (!buffer) {
		vipTraceEvent(VIP_CODEC_TRACE_NO_PICTURE, g_pictureCount);
		if (fieldPic && !g_secondFieldPending)
			return;
		g_secondFieldPending = 0;
		g_seiPresent = 0;
		g_pictureCount++;
		return;
	}
	g_lastBottomField = fieldPic ? g_sliceHeader.bottomField : VIP_NO_FIELD;
	enableDecoderClocks();
	if (fieldPic && !g_secondFieldPending) {
		if (markReference()) {
			vipTraceEvent(VIP_CODEC_TRACE_MARKING, g_pictureCount);
			vipNotifyEvents(VIP_CODEC_EVENT_MARKING);
		} else {
			keepExtension(buffer, 0);
		}
		resetUnit0();
		g_secondFieldPending = 1;
		return;
	}
	if (markReference()) {
		vipTraceEvent(VIP_CODEC_TRACE_MARKING, g_pictureCount);
		vipNotifyEvents(VIP_CODEC_EVENT_MARKING);
	} else {
		keepExtension(buffer, fieldPic);
	}
	resetUnits();
	g_secondFieldPending = 0;
	g_seiPresent = 0;
	g_currentBuffer = NULL;
	g_pictureCount++;
}

/* An access unit delimiter's time, its DTS else its PTS. */
static void takeDelimiterTime(uint32_t flags, uint32_t dtsLow, uint32_t ptsLow)
{
	if (flags & VIP_TIME_DTS) {
		g_delimiterTime = recordDts(flags, dtsLow);
	} else if (flags & VIP_TIME_PTS) {
		g_delimiterTime = recordPts(flags, ptsLow);
	} else {
		g_untimedDelimiters++;
		return;
	}
	g_untimedDelimiters = 0;
}

/* The ARM side asked for a reset: drop the pictures and all that is parsed. */
static void checkReset(void)
{
	VipRingHandle *records = g_ringHandles[VIP_RING_RECORDS], *slices;

	if (!getRingReaderState0(records))
		return;
	g_generation++;
	resetDecoding();
	slices = g_ringHandles[VIP_RING_SLICES];
	while (ringSkipToWriter(records))
		;
	while (ringSkipToWriter(slices))
		;
	setRingReaderState0(records, 0);
}

/* the counters the ARM side reads */
static inline void publishCounters(void)
{
	g_codec.counters.pictures = g_pictureCount - g_codec.counters.skippedPictures;
	g_codec.counters.underruns = g_underrunCount;
}

/* A field whose pair did not come: it is concealed, and marked if it can be. */
static void dropUnpairedField(void)
{
	VipPictureBuffer *buffer = g_currentBuffer;

	vipTraceEvent(VIP_CODEC_TRACE_NOT_PAIRED, g_pictureCount);
	vipNotifyEvents(VIP_CODEC_EVENT_NOT_PAIRED);
	if (buffer) {
		buffer->frame->frame->info.flags |= 1u << VIP_FRAME_UNPAIRED;
		concealPicture();
		if (markReference()) {
			vipTraceEvent(VIP_CODEC_TRACE_MARKING, g_pictureCount);
			vipNotifyEvents(VIP_CODEC_EVENT_MARKING);
		}
	}
	resetUnits();
	g_secondFieldPending = 0;
	g_currentBuffer = NULL;
	g_seiPresent = (g_seiPresent & VIP_SEI_FIRST_FIELDS) >> 1;
	g_late = 0;
}

/* The picture cannot be decoded: the records up to its end go. */
static void skipPicture(void)
{
	resetUnits();
	gateDecoderClocks();
	g_skipping = skipToPictureEnd();
	g_currentBuffer = NULL;
	vipTraceEvent(VIP_CODEC_TRACE_NO_BUFFER, g_pictureCount);
	g_secondFieldPending = 0;
	g_codec.counters.skippedPictures++;
	g_late = 0;
}

/*
 * The picture starts: its buffer (picture and colocated, if not 0, replace its
 * memory), its frame's display information, and its times. which is the run's
 * VIP_CODEC_RUN_*, flags the record's VIP_TIME_*.
 */
static void beginPicture(const VipRecord *record, uint32_t picture, uint32_t colocated,
	uint32_t which, uint32_t flags)
{
	VipPictureBuffer *buffer = g_currentBuffer;
	uint64_t fallback;

	if (picture) {
		buffer->picture = picture;
		buffer->colocated = colocated;
		buffer->frame->frame->picture = picture;
	}
	if (startPicture(!!(g_codec.options & VIP_CODEC_OPTION_PICTURE)))
		vipNotifyEvents(VIP_CODEC_EVENT_PICTURE);
	if (g_sliceHeader.fieldPic && g_secondFieldPending) {
		updateSecondFieldInfo();
		publishSecondField(g_currentBuffer);
	} else {
		updateFrameInfo();
		if (which == VIP_CODEC_RUN_REFERENCES_ONLY &&
		    !(g_frameInfo.flags & 1u << VIP_FRAME_NAL_REFERENCE))
			g_frameInfo.flags |= 1u << VIP_FRAME_SKIPPED;
		publishFrameInfo(g_currentBuffer);
	}

	/* without times, the picture's are the output clock's */
	fallback = g_outputClock >> 1;
	takePictureTime(0, 0, flags, record->pts, record->dts, &fallback);
	if (g_secondFieldPending && !g_bufferingPeriodTaken)
		return;
	if (!(flags & (VIP_TIME_PTS | VIP_TIME_DTS)) && !g_currentSps->timeScale)
		return;
	g_outputClock = g_currentBuffer->decodeTime << 1;
	g_outputClockFraction = 0;
}

/*
 * The run proper: take the records whose data is complete (it ends where the
 * next record's starts) until one ends the picture or the stream, then give back
 * the records taken and their data. A record that cannot be taken yet stays for
 * the next run. picture and colocated are for beginPicture, which for the run's
 * VIP_CODEC_RUN_*. RUN_*, or -1 when there was nothing to take.
 */
static int decodeRecords(VipCodecDecoder *decoder, uint32_t picture, uint32_t colocated,
	uint32_t which)
{
	VipRingSegment segments[2];
	const VipRing *ring;
	const VipRecord *record, *current, *end;
	uint32_t count, taken = 0, header, type, flags, bits, padding, start, next, i;
	int result = RUN_CONTINUE, status;
	VipPictureBuffer *buffer;
	uint32_t timeScale;

	checkReset();
	g_codec.counters.outputs = g_outputCount;
	publishCounters();
	if (g_skipping) {
		vipWaitTimer(SKIP_WAIT);
		g_skipping = skipToPictureEnd();
		return -1;
	}
	if (g_drainState == VIP_DRAIN_LAYOUT) {
		if (framesOutstanding())
			return RUN_BUSY;
		g_drainState = VIP_DRAIN_NONE;
	}

	segments[1].size = 0;
	getRingReadable(g_ringHandles[VIP_RING_RECORDS], segments, 2);
	if (segments[0].size < sizeof(VipRecord)) {
		vipWaitTimer(INPUT_WAIT);
		return -1;
	}
	count = (segments[0].size + segments[1].size) / sizeof(VipRecord);
	record = (const VipRecord *)segments[0].start;
	ring = g_ringHandles[VIP_RING_RECORDS]->ring;
	end = (const VipRecord *)(ring->start + ring->size);
	next = record->position;
	header = record->header;
	if (count == 1 && !endsRun(header >> VIP_RECORD_TYPE_SHIFT))
		return RUN_CONTINUE;
	g_sliceSegmentCount = getRingReadable(g_ringHandles[VIP_RING_SLICES], g_sliceSegments, 2);

	for (i = 0; i < count; i++) {
		current = record++;
		if (record == end)
			record = (const VipRecord *)ring->start;
		start = next ? next : current->position;
		g_dataStart = start;
		type = header >> VIP_RECORD_TYPE_SHIFT;
		flags = header >> VIP_RECORD_FLAGS_SHIFT & VIP_TIME_FLAGS;
		bits = 0;
		if (!endsRun(type)) {
			if (i == count - 1)
				break;		/* where its data ends is not known yet */
			next = record->position;
			header = record->header;
			padding = 0;
			if (header >> VIP_RECORD_TYPE_SHIFT == VIP_PARSED_PADDING)
				padding = header >> VIP_PADDING_BITS_SHIFT & 7;
			if (next < start)
				bits = getRingDistance(g_ringHandles[VIP_RING_SLICES], start, next);
			else
				bits = next - start;
			bits = bits * 8 + padding;
		}
		taken++;

		switch (type) {
		case VIP_PARSED_CABAC_DATA:
		case VIP_PARSED_CAVLC_DATA:
			buffer = g_currentBuffer;
			if (!buffer) {
				status = VIP_CODEC_TRACE_NO_PICTURE;
			} else if (g_frameInfo.flags & 1u << VIP_FRAME_SKIPPED) {
				status = 0;
			} else {
				enableDecoderClocks();
				status = decodeSlice(bits);
			}
			while (setRingReader(g_ringHandles[VIP_RING_SLICES], next))
				;
			next = 0;
			if (status >= 0)
				break;
			g_codec.counters.sliceErrors++;
			vipTraceEvent(status, g_pictureCount);
			if (buffer)
				setFrameDamaged(g_currentBuffer->frame, 1);
			vipNotifyEvents(VIP_CODEC_EVENT_SLICE_ERROR);
			break;

		case VIP_PARSED_SLICE_HEADER:
			if (bits && g_currentBuffer) {
				enableDecoderClocks();
				if (takeSliceHeader() >= 0)
					break;
			}
			g_codec.counters.headerErrors++;
			vipTraceEvent(VIP_CODEC_TRACE_NO_DATA | 2, g_pictureCount);
			vipNotifyEvents(VIP_CODEC_EVENT_HEADER_ERROR);
			break;

		case VIP_PARSED_PICTURE_HEADER:
			enableDecoderClocks();
			initializeUnits();
			if (bits) {
				status = takePictureHeader(decoder->workKind,
					g_codec.options >> VIP_CODEC_OPTION_MODE_SHIFT, g_drainState,
					decoder->areaCount);
			} else {
				g_codec.counters.headerErrors++;
				vipTraceEvent(VIP_CODEC_TRACE_NO_DATA | 2, g_pictureCount);
				vipNotifyEvents(VIP_CODEC_EVENT_PICTURE_ERROR);
				status = VIP_PICTURE_NO_PARAMETERS;
			}
			switch (status) {
			case VIP_PICTURE_TOO_LARGE:
			case VIP_PICTURE_TOO_MANY_FRAMES:
				vipTraceEvent(VIP_CODEC_TRACE_BAD_PICTURE, g_pictureCount);
				result = RUN_FAILED;
				goto keepRecord;
			case VIP_PICTURE_NOT_PAIRED:
				dropUnpairedField();
				goto keepRecord;
			case VIP_PICTURE_DRAIN:
				outputAllPictures();
				vipTraceEvent(VIP_CODEC_TRACE_LAID_OUT, g_pictureCount);
				vipNotifyEvents(VIP_CODEC_EVENT_NEW_LAYOUT);
				g_late = 0;
				g_drainState = VIP_DRAIN_LAYOUT;
				goto keepRecord;
			case VIP_PICTURE_LAID_OUT:
				g_drainState = decoder->workKind != VIP_WORK_AREAS;
				break;
			}
			if (g_outputState == VIP_OUTPUT_WAITING || status < 0) {
				skipPicture();
				return RUN_CONTINUE;
			}
			status = allocatePictureBuffer();
			if (!status && decoder->workKind != VIP_WORK_AREAS)
				status = takeMotionData();
			if (status < 0) {
				if (g_referenceFrameCount == g_dpbCount) {
					/* every picture is a reference: none can go */
					result = RUN_FAILED;
					vipTraceEvent(VIP_CODEC_TRACE_RESTART, g_pictureCount);
					vipNotifyEvents(VIP_CODEC_EVENT_RESTART);
				} else {
					vipTraceEvent(VIP_CODEC_TRACE_BUFFER_TIMEOUT, g_pictureCount);
					result = RUN_NO_BUFFER;
				}
				goto keepRecord;
			}
			beginPicture(current, picture, colocated, which, flags);
			break;

		case VIP_PARSED_PICTURE_END:
			next = g_dataStart;
			result = RUN_PICTURE_END;
			finishPicture();
			goto done;

		case VIP_PARSED_SPS:
			if (bits) {
				takeSps();
				break;
			}
			g_codec.counters.spsErrors++;
			vipTraceEvent(VIP_CODEC_TRACE_NO_DATA | 0, g_pictureCount);
			vipNotifyEvents(VIP_CODEC_EVENT_SPS_ERROR);
			break;

		case VIP_PARSED_PPS:
			if (bits) {
				takePps();
				break;
			}
			g_codec.counters.ppsErrors++;
			vipTraceEvent(VIP_CODEC_TRACE_NO_DATA | 1, g_pictureCount);
			vipNotifyEvents(VIP_CODEC_EVENT_PPS_ERROR);
			break;

		case VIP_PARSED_DELIMITER:
			takeDelimiterTime(flags, current->dts, current->pts);
			break;

		case VIP_PARSED_VUI:
			timeScale = g_currentSps->timeScale;
			takeVui();
			g_seiPresent |= VIP_SEI_VUI;
			saveParsed(g_vui[g_currentPps->spsId], VIP_EXTENSION_VUI_SIZE);
			if (g_currentSps->timeScale != timeScale)
				rescaleClock(timeScale, g_currentSps->timeScale);
			break;

		case VIP_PARSED_STREAM_END:
		case VIP_PARSED_START:
			/* a first field still waiting for its pair is concealed */
			next = g_dataStart;
			if (!g_secondFieldPending || g_lastBottomField != g_sliceHeader.bottomField) {
				if (g_currentBuffer)
					finishPicture();
				if (!g_secondFieldPending) {
					result = RUN_STREAM_END;
					goto done;
				}
			}
			enableDecoderClocks();
			vipTraceEvent(VIP_CODEC_TRACE_NOT_PAIRED, g_pictureCount);
			vipNotifyEvents(VIP_CODEC_EVENT_NOT_PAIRED);
			if (g_currentBuffer) {
				g_currentBuffer->frame->frame->info.flags |= 1u << VIP_FRAME_UNPAIRED;
				concealPicture();
				finishPicture();
			}
			result = RUN_STREAM_END;
			goto done;

		case VIP_PARSED_PICTURE_TIMING:
			takePictureTiming();
			g_seiPresent |= VIP_SEI_PICTURE_TIMING << g_secondFieldPending;
			saveParsed(g_sei.pictureTiming, sizeof(g_sei.pictureTiming));
			break;

		case VIP_PARSED_RECOVERY_POINT:
			takeRecoveryPoint(!!(g_codec.options & VIP_CODEC_OPTION_BROKEN_LINK));
			g_seiPresent |= VIP_SEI_RECOVERY_POINT << g_secondFieldPending;
			saveParsed(g_sei.recoveryPoint, sizeof(g_sei.recoveryPoint));
			break;

		case VIP_PARSED_BUFFERING_PERIOD:
			takeBufferingPeriod();
			g_seiPresent |= VIP_SEI_BUFFERING_PERIOD << g_secondFieldPending;
			saveParsed(g_sei.bufferingPeriod, sizeof(g_sei.bufferingPeriod));
			break;

		case VIP_PARSED_USER_DATA:
		case VIP_PARSED_USER_DATA_UUID:
			if (!(bits >> 3) || !g_userDataArea)
				break;
			g_seiPresent |= VIP_SEI_USER_DATA << g_secondFieldPending;
			enableDecoderClocks();
			takeUserData(&g_sei.userData, type != VIP_PARSED_USER_DATA_UUID, bits >> 3);
			break;

		case VIP_PARSED_PAN_SCAN:
			takePanScan();
			g_seiPresent |= VIP_SEI_PAN_SCAN << g_secondFieldPending;
			saveParsed(g_sei.panScan, sizeof(g_sei.panScan));
			break;

		case VIP_PARSED_FRAME_PACKING:
			takeFramePacking();
			g_seiPresent |= VIP_SEI_FRAME_PACKING << g_secondFieldPending;
			saveParsed(g_sei.framePacking, sizeof(g_sei.framePacking));
			break;

		case VIP_PARSED_PADDING:
		case VIP_PARSED_SEI:
		case VIP_PARSED_SEI_OTHER:
		case VIP_PARSED_RESERVED:
		case VIP_PARSED_END_SEQUENCE:
		case VIP_PARSED_END_STREAM:
		case VIP_PARSED_OTHER:
			break;

		default:
			vipTraceEvent(VIP_CODEC_TRACE_UNKNOWN_RECORD, g_pictureCount);
			result = RUN_FAILED;
			goto done;
		}
		gateDecoderClocks();
	}
	goto done;

keepRecord:
	/* the record is read again by the next run, its data too */
	taken--;
	next = start;
done:
	gateDecoderClocks();
	while (advanceRingReader(g_ringHandles[VIP_RING_RECORDS], taken * sizeof(VipRecord)))
		;
	if (next) {
		while (setRingReader(g_ringHandles[VIP_RING_SLICES], next))
			;
	}
	publishCounters();
	return result;
}

/*
 * C.4.5.3 as FW2 has it: with the DPB full, hand out the pictures up to the first
 * that is no reference, in output order; with pic_order_cnt_type 2 the first not
 * handed out. 1 while the DPB is not full, -1 if none could go.
 */
int bumpPictures(void)
{
	uint32_t count = g_dpbCount, i, j;

	if (g_sliceHeader.fieldPic && g_secondFieldPending)
		return -1;
	if (g_currentSps->picOrderCntType == 2) {
		for (i = 0; i < count; i++) {
			if (!g_dpbByPoc[i]->idle) {
				handOutPicture(g_dpbByPoc[i]);
				return 0;
			}
		}
		return -1;
	}
	if (count < (uint32_t)g_bufferCount - 1)
		return 1;
	for (i = 0; i < count; i++) {
		VipPictureBuffer *buffer = g_dpbByPoc[i];

		if (buffer->topReference || buffer->bottomReference)
			continue;
		for (j = 0; j < i; j++) {
			if (!g_dpbByPoc[j]->idle)
				handOutPicture(g_dpbByPoc[j]);
		}
		if (!g_dpbByPoc[i]->idle)
			handOutPicture(g_dpbByPoc[i]);
		return 0;
	}
	return -1;
}

/* Hand out every picture not yet handed out, in output order; -1 if none was. */
int outputAllPictures(void)
{
	uint32_t count = g_bufferCount, i;
	int result = -1;

	for (i = 0; i < count; i++) {
		VipPictureBuffer *buffer = g_dpbByPoc[i];

		if (buffer && !buffer->idle) {
			handOutPicture(buffer);
			result = 0;
		}
	}
	return result;
}

/* The stream ended: the rest go, and the DPB is empty. */
static void endOutput(void)
{
	outputAllPictures();
	g_codec.counters.outputs = g_outputCount;
	g_dpbCount = 0;
}

/* a frame's duration by its MPEG frame_rate_code, in 720 kHz */
static uint32_t frameDuration(VipFrameHandle *frame)
{
	uint8_t rate, repeat;

	getFrameRate(frame, &rate);
	switch (rate) {
	case 1:
		return 30030;
	case 2:
		return 30000;
	case 3:
		return 28800;
	case 4:
		getFrameRepeatFirstField(frame, &repeat);
		return repeat ? 36036 : 24024;
	case 5:
		getFrameRepeatFirstField(frame, &repeat);
		return repeat ? 36000 : 24000;
	case 6:
		return 14400;
	case 7:
		return 12012;
	case 8:
		return 12000;
	default:
		return 24024;
	}
}

#define CLOCK_SIGN		((uint64_t)1 << 32)	/* of a 33-bit difference */
#define CLOCK_MASK		0x3ffffffffull		/* the output clock: 34 bits at 180 kHz */
#define CLOCK_FRACTION		4			/* a 180 kHz tick in 720 kHz */

/*
 * VIP_CODEC_OPTION_ON_TIME: hand out the first picture not yet handed out once
 * it is due by the output clock (or is a random access point, or the output is
 * late), and move the clock on by its duration. -1 if none can go yet.
 */
static int outputOnTime(void)
{
	VipPictureBuffer *buffer;
	uint32_t count, i, duration, fraction;
	uint8_t randomAccess, late;

	if (g_sliceHeader.fieldPic && g_secondFieldPending)
		return -1;
	buffer = g_dpbByPoc[0];
	count = g_dpbCount;
	for (i = 0; i < count; i++) {
		buffer = g_dpbByPoc[i];
		if (!buffer->idle)
			break;
	}
	if (i == count)
		return -1;
	getFrameRandomAccess(buffer->frame, &randomAccess);
	if (randomAccess) {
		g_late = 0;
		late = 0;
	} else {
		late = g_late;
	}
	if (!late && buffer->ptsValid) {
		uint64_t difference = (g_outputClock >> 1) - buffer->presentationTime;
		int32_t high = (int32_t)(difference >> 32);

		if (!(difference & CLOCK_SIGN)) {
			if (high >= 0 && high < 0x10000)
				g_late = 1;
		} else if (high < 0 && high >= -0xffff) {
			return -1;	/* not due yet */
		}
	}
	duration = frameDuration(buffer->frame);
	fraction = g_outputClockFraction + (duration & (CLOCK_FRACTION - 1));
	g_outputClock += duration >> 2;
	if (fraction & CLOCK_FRACTION) {
		fraction &= ~CLOCK_FRACTION;
		g_outputClock++;
	}
	g_outputClock &= CLOCK_MASK;
	g_outputClockFraction = fraction;
	handOutPicture(buffer);
	return 0;
}
