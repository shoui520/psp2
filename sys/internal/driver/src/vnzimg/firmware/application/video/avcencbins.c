/*
 * The AVC encoder's bin coder (0x874448-0x8747e8). While the helper holds
 * the unit, the bins of each macroblock's syntax elements are packed into
 * words by the bitstream unit and stored, a word at a time, into one of two
 * 512-byte buffers; the CABAC engine codes each buffer once it is full,
 * while the next one fills. The unit's registers are kept in the writer
 * between the helper's jobs. And the rest of the header writer's bits:
 * Exp-Golomb codes and the byte alignment.
 */
#include "service.h"

/* the state of the engine's work, CABAC_STATUS bits 8-9 */
#define ENGINE_IDLE		0
#define ENGINE_BUFFER_DONE	1	/* it took the bins' buffer */
#define ENGINE_OUTPUT_FULL	2	/* its output buffer is full: copy it out */
#define ENGINE_BOTH		3

static inline uint32_t engineWait(void)
{
	uint32_t status;

	while ((status = CABAC[CABAC_STATUS]) & 1)
		;
	return status;
}

static inline uint32_t engineWork(uint32_t status)
{
	return (status >> 8) & 3;
}

/* the first buffer, and the unit's registers as a slice's bins begin */
void avcencCabacLoad(VideoAvcEncodeJob *job, VideoAvcWriter *writer)
{
	VideoAvcCabacState *cabac = job->cabacState;

	cabac->last = BINS_BUFFER(0) + BINS_BUFFER_SIZE - 1;
	cabac->engineRunning = 0;
	cabac->bufferBit = 0;
	cabac->left = BINS_BUFFER_SIZE;
	cabac->write = BINS_BUFFER(0);
	cabac->buffer = BINS_BUFFER(0);
	writer->cabacState.state6 = 0;
	writer->cabacState.status = 1;
	writer->cabacState.state4 = 0;
	writer->cabacState.free = 32;
	writer->cabacState.word = 0;
	writer->cabacState.state1 = 0;
	writer->cabacState.control = 0;
}

void avcencCabacSave(VideoAvcWriter *writer)
{
	uint32_t control = VIDEO_DSP_READ(BINS_READ + BINS_CONTROL);
	uint32_t state1 = VIDEO_DSP_READ(BINS_READ + BINS_STATE1);
	uint32_t word = VIDEO_DSP_READ(BINS_READ + BINS_WORD);
	uint32_t free = VIDEO_DSP_READ(BINS_READ + BINS_FREE);
	uint32_t state4 = VIDEO_DSP_READ(BINS_READ + BINS_STATE4);
	uint32_t status = VIDEO_DSP_READ(BINS_READ + BINS_STATUS);
	uint32_t state6 = VIDEO_DSP_READ(BINS_READ + BINS_STATE6);

	writer->cabacState.state6 = state6;
	writer->cabacState.status = status;
	writer->cabacState.state4 = state4;
	writer->cabacState.free = free;
	writer->cabacState.word = word;
	writer->cabacState.state1 = state1;
	writer->cabacState.control = control;
}

void avcencCabacStart(const VideoAvcWriter *writer)
{
	const VideoAvcCabacState *state = &writer->cabacState;

	VIDEO_DSP_WRITE(BINS_WRITE + BINS_CONTROL, state->control);
	VIDEO_DSP_WRITE(BINS_WRITE + BINS_STATE1, state->state1);
	VIDEO_DSP_WRITE(BINS_WRITE + BINS_WORD, state->word);
	VIDEO_DSP_WRITE(BINS_WRITE + BINS_FREE, state->free);
	VIDEO_DSP_WRITE(BINS_WRITE + BINS_STATE4, state->state4);
	VIDEO_DSP_WRITE(BINS_WRITE + BINS_STATUS, state->status);
	VIDEO_DSP_WRITE(BINS_WRITE + BINS_STATE6, state->state6);
}

/* the other buffer filled next, the one the engine does not read */
static __attribute__((noinline, noclone)) void binsNextBuffer(VideoAvcEncodeJob *job)
{
	VideoAvcCabacState *cabac = job->cabacState;
	uint32_t bit = 1 - ((job->unitState & STATE_BINS) >> 9);
	uint32_t buffer;

	cabac->bufferBit = bit;
	buffer = BINS_BUFFER(bit);
	cabac->buffer = buffer;
	cabac->left = BINS_BUFFER_SIZE;
	cabac->write = buffer;
	cabac->last = buffer + BINS_BUFFER_SIZE - 1;
	job->binCount += BINS_BUFFER_SIZE * 8;
	vnzCacheInvalidate(cabac->write, BINS_BUFFER_SIZE);
}

/* the engine given the buffer just filled */
static inline void binsGive(VideoAvcEncodeJob *job, const VideoAvcCabacState *cabac)
{
	job->unitState = (job->unitState & ~STATE_BINS) | ((cabac->bufferBit & 1) << 9);
	avcencCabacResume(job);
}

/* a buffer full while the engine works: its output copied until it takes the buffer */
static __attribute__((noinline, noclone)) uint32_t binsWait(VideoAvcEncodeJob *job)
{
	VideoAvcCabacState *cabac = job->cabacState;
	uint32_t status = engineWait();

	while (engineWork(status) == ENGINE_OUTPUT_FULL) {
		avcencCabacResume(job);
		avcencCabacCopyBlock(job);
		status = engineWait();
	}
	if (engineWork(status) == ENGINE_BUFFER_DONE) {
		binsGive(job, cabac);
		binsNextBuffer(job);
	} else if (engineWork(status) == ENGINE_BOTH) {
		binsGive(job, cabac);
		avcencCabacCopyBlock(job);
		binsNextBuffer(job);
	}
	return engineWork(status);
}

/* the buffer stored, and given to the engine: the first one starts it */
static inline uint32_t binsFlush(VideoAvcEncodeJob *job, VideoAvcCabacState *cabac)
{
	vnzCacheFlush(cabac->buffer, BINS_BUFFER_SIZE);
	mep_syncm();
	if (cabac->engineRunning)
		return binsWait(job);
	binsGive(job, cabac);
	cabac->engineRunning = 1;
	binsNextBuffer(job);
	return ENGINE_BUFFER_DONE;
}

/* count bins of value, the last bin last */
uint32_t avcencBinsPut(VideoAvcEncodeJob *job, uint32_t value, int32_t count)
{
	VideoAvcCabacState *cabac;
	uint32_t *write;

	VIDEO_DSP(BINS_PUT, value, count);
	if (!value)
		return ENGINE_BUFFER_DONE;
	cabac = job->cabacState;
	write = (uint32_t *)cabac->write;
	*write = VIDEO_DSP_READ(BINS_READ + BINS_WORD);
	cabac->write = (uint32_t)(write + 1);
	cabac->left -= 4;
	if (cabac->left)
		return ENGINE_BUFFER_DONE;
	return binsFlush(job, cabac);
}

/* the last word, and every buffer coded: the engine's state then */
uint32_t avcencBinsFinish(VideoAvcEncodeJob *job)
{
	VideoAvcCabacState *cabac = job->cabacState;
	uint32_t zero = 0, *write;
	uint32_t status, work;

	VIDEO_DSP(BINS_END, zero, zero);
	write = (uint32_t *)cabac->write;
	*write = VIDEO_DSP_READ(BINS_READ + BINS_WORD);
	cabac->write = (uint32_t)(write + 1);
	cabac->left -= 4;
	job->binCount += (BINS_BUFFER_SIZE - cabac->left) * 8;
	binsFlush(job, cabac);
	for (;;) {
		status = engineWait();
		if (engineWork(status) != ENGINE_OUTPUT_FULL)
			break;
		avcencCabacResume(job);
		avcencCabacCopyBlock(job);
	}
	work = engineWork(status);
	while (work == ENGINE_BUFFER_DONE) {
		avcencCabacResume(job);
		status = engineWait();
		work = engineWork(status);
	}
	return work;
}

uint32_t avcencSwap32(uint32_t word)
{
	return VIDEO_DSP_GET(BINS_SWAP, word);
}

uint32_t avcencBitsAligned(const VideoAvcWriter *writer)
{
	return writer->bitPosition == 0;
}

/* ue(v): as many zeros as the bits after value + 1's first, then value + 1 */
void avcencBitsUe(VideoAvcWriter *writer, uint32_t value)
{
	uint32_t code = value + 1;
	int32_t length = 31 - vnzLeadingZeros(code);

	if (length < 16) {
		avcencBitsPut(writer, length * 2 + 1, code);
		return;
	}
	avcencBitsPut(writer, length, 0);
	avcencBitsPut(writer, length + 1, code);
}

/* se(v): positive values to the odd codes, the others to the even */
void avcencBitsSe(VideoAvcWriter *writer, int32_t value)
{
	if (value > 0)
		avcencBitsUe(writer, value * 2 - 1);
	else
		avcencBitsUe(writer, -(value * 2));
}
