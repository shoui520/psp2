/*
 * PU0: refilling and draining the stream buffers, from local RAM.
 */
#include "stream.h"

VipStream g_readStream VIP_LRAM_DATA;
VipStream g_writeStream VIP_LRAM_DATA;

/*
 * Queue a VIP_DMA_LIST request (the list is in local RAM, so the request has no
 * DRAM address); -1 if the queue is full.
 */
VIP_LRAM_TEXT int queueListDma(uint32_t control, uint32_t localLength,
	void (*callback)(void), volatile uint32_t *done)
{
	VipDmaRequest *request;
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	request = &g_dmaQueue.requests[g_dmaQueue.tail];
	if (g_dmaQueue.count < 2) {
		vipWriteControlBus(VIP_CB_DMA_DIRECTION, control);
		vipWriteControlBus(VIP_CB_DMA_LOCAL_LENGTH, localLength);
		if (!g_dmaQueue.count)
			vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
	} else if (g_dmaQueue.count == VIP_DMA_QUEUE_LENGTH) {
		if (psw & VIP_PSW_IEC)
			mep_ei();
		return -1;
	} else {
		request->control = control;
		request->localLength = localLength;
	}
	request->done = done;
	request->callback = callback;
	g_dmaQueue.count++;
	if (done)
		*done = 1;
	g_dmaQueue.tail = (g_dmaQueue.tail + 1) & (VIP_DMA_QUEUE_LENGTH - 1);
	if (psw & VIP_PSW_IEC)
		mep_ei();
	return 0;
}

/* tell the hardware a buffer half is ready (DMA callbacks) */
static VIP_LRAM_TEXT void readStreamHalf0Ready(void)
{
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
}

static VIP_LRAM_TEXT void readStreamHalf1Ready(void)
{
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
}

static VIP_LRAM_TEXT void writeStreamHalf0Ready(void)
{
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 1);
}

static VIP_LRAM_TEXT void writeStreamHalf1Ready(void)
{
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 1);
}

/*
 * The read stream wants a half refilled: list up to a half's worth of the stream
 * from its segments (whole 8-byte units) and queue it. Past the end of the stream
 * the interrupt is only acknowledged.
 */
VIP_LRAM_TEXT void readStreamInterrupt(uint32_t argument)
{
	VipStream *stream = &g_readStream;
	const VipRingSegment *segment = stream->segments;
	VipDmaDescriptor *list = NULL;
	void (*ready)(void) = NULL;
	uint32_t filled = 0, count = 0, i;

	(void)argument;
	for (i = 0; i < stream->segmentCount; i++, segment++) {
		uint32_t end = segment->start + segment->size;
		uint32_t cursor = stream->cursor;
		uint32_t length, offset;

		if (cursor < segment->start)
			continue;
		if (cursor >= end) {
			if (stream->half & 1)
				vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
			else
				vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
			stream->half++;
			return;
		}
		length = VIP_STREAM_HALF - filled;
		if (end - cursor < length) {
			length = end - cursor;
			if (length & 7)
				length += 8 - (length & 7);
		}
		if (stream->half & 1) {
			vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
			offset = VIP_READ_BUFFER + VIP_STREAM_HALF + filled;
			ready = readStreamHalf1Ready;
			list = stream->lists[1];
		} else {
			vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
			offset = VIP_READ_BUFFER + filled;
			ready = readStreamHalf0Ready;
			list = stream->lists[0];
		}
		list[count].external = cursor;
		list[count].localLength = offset << 16 | length;
		filled += length;
		stream->cursor = cursor + length;
		if (stream->cursor == segment->start + segment->size &&
		    i + 1 < stream->segmentCount)
			stream->cursor = segment[1].start;
		count = (count + 1) & 0xff;
		if (filled >= VIP_STREAM_HALF)
			break;
	}
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
		(uint32_t)list << 16 | count * sizeof(VipDmaDescriptor), ready, NULL);
	stream->half++;
}

/*
 * The write stream has filled a half: list it out to the stream's segments and
 * queue it. When the stream reaches the end of its last segment, the bitstream
 * unit is reset instead.
 */
VIP_LRAM_TEXT void writeStreamInterrupt(uint32_t argument)
{
	VipStream *stream = &g_writeStream;
	const VipRingSegment *segment = stream->segments;
	VipDmaDescriptor *list;
	void (*ready)(void);
	uint32_t drained = 0, count = 0, i;

	(void)argument;
	if (stream->half & 1) {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_WRITE_STREAM1);
		list = stream->lists[1];
		ready = writeStreamHalf1Ready;
	} else {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_WRITE_STREAM0);
		list = stream->lists[0];
		ready = writeStreamHalf0Ready;
	}
	if (stream->ended) {
		stream->half++;
		vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 1);
		vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 1);
		return;
	}
	for (i = 0; i < stream->segmentCount; i++, segment++) {
		uint32_t cursor = stream->cursor;
		uint32_t length, offset;

		if (cursor < segment->start)
			continue;
		length = VIP_STREAM_HALF - drained;
		if (segment->start + segment->size - cursor < length)
			length = segment->start + segment->size - cursor;
		offset = VIP_WRITE_BUFFER + drained;
		if (stream->half & 1)
			offset += VIP_STREAM_HALF;
		list[count].external = cursor;
		list[count].localLength = offset << 16 | length;
		drained += length;
		stream->cursor = cursor + length;
		if (stream->cursor == segment->start + segment->size) {
			if (i + 1 >= stream->segmentCount) {
				resetBitstream();
				return;
			}
			stream->cursor = segment[1].start;
		}
		count = (count + 1) & 0xff;
		if (drained >= VIP_STREAM_HALF)
			break;
	}
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_WRITE,
		(uint32_t)list << 16 | count * sizeof(VipDmaDescriptor), ready, NULL);
	stream->half++;
}
