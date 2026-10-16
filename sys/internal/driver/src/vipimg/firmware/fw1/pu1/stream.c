/*
 * PU1: refilling the read stream's buffer, from task 0's code overlay.
 *
 * FW2's codec refills it the same way from its code overlay, straight from the
 * slice ring's segments (g_sliceSegments, from g_dataStart on), and its handler
 * is the interrupt's vector itself: it acknowledges the interrupt first.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#define STREAM_TEXT	VIP_CODEC_OVERLAY
#else
#include "stream.h"
#define STREAM_TEXT	VIP_TASK0_TEXT

VipReadStream g_readStream VIP_TASK0_BSS;
#endif

/*
 * Queue a VIP_DMA_LIST request (the list is in local RAM, so the request has no
 * DRAM address); -1 if the queue is full.
 */
STREAM_TEXT int queueListDma(uint32_t control, uint32_t localLength,
	uint32_t parameter0, uint32_t parameter1, void (*callback)(void),
	volatile uint32_t *done)
{
	VipDmaRequest *request;
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	request = &g_dmaQueue.requests[g_dmaQueue.tail];
	if (g_dmaQueue.count < 2) {
		vipWriteControlBus(VIP_CB_DMA_DIRECTION, control);
		vipWriteControlBus(VIP_CB_DMA_LOCAL_LENGTH, localLength);
		vipWriteControlBus(VIP_CB_DMA_PARAMETER0, parameter0);
		vipWriteControlBus(VIP_CB_DMA_PARAMETER1, parameter1);
		if (!g_dmaQueue.count)
			vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
	} else if (g_dmaQueue.count == VIP_DMA_QUEUE_LENGTH) {
		if (psw & VIP_PSW_IEC)
			mep_ei();
		return -1;
	} else {
		request->control = control;
		request->localLength = localLength;
		request->parameters[0] = parameter0;
		request->parameters[1] = parameter1;
	}
	request->callback = callback;
	request->done = done;
	g_dmaQueue.count++;
	if (done)
		*done = 1;
	g_dmaQueue.tail = (g_dmaQueue.tail + 1) & (VIP_DMA_QUEUE_LENGTH - 1);
	if (psw & VIP_PSW_IEC)
		mep_ei();
	return 0;
}

/* tell the hardware a half is ready, and take the other half's interrupt again */
static STREAM_TEXT void readStreamHalf0Ready(void)
{
	uint32_t enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);

	vipWriteControlBus(VIP_CB_SLICE_READY0, 1);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable | 1 << VIP_INTERRUPT_READ_STREAM1);
}

static STREAM_TEXT void readStreamHalf1Ready(void)
{
	uint32_t enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);

	vipWriteControlBus(VIP_CB_SLICE_READY1, 1);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable | 1 << VIP_INTERRUPT_READ_STREAM0);
}

#ifdef VIP_FW2
uint16_t g_streamHalf VIP_CODEC_LOCAL;	/* the half to fill next, by its low bit */

/*
 * The read stream wants a half refilled: list up to a half's worth of the slice
 * data (whole 8-byte units) and queue it. Past the end of the data the half is
 * only marked ready.
 */
STREAM_TEXT void readStreamInterrupt(void)
{
	const VipRingSegment *segment = g_sliceSegments;
	void (*ready)(void);
	uint32_t enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	uint32_t half = g_streamHalf & 1, cursor = g_dataStart, filled = 0, count = 0;
	uint32_t end, length, offset, i;

	if (half)
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
	else
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
	g_streamHalf++;
	for (i = 0; i < g_sliceSegmentCount; i++, segment++) {
		if (cursor < segment->start)
			continue;
		end = segment->start + segment->size;
		if (cursor >= end) {
			g_dataStart = cursor;
			if (half)
				vipWriteControlBus(VIP_CB_SLICE_READY1, 1);
			else
				vipWriteControlBus(VIP_CB_SLICE_READY0, 1);
			return;
		}
		length = VIP_STREAM_HALF - filled;
		if (end - cursor < length) {
			length = end - cursor;
			if (length & 7)
				length += 8 - (length & 7);
		}
		if (half) {
			vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
				enable & ~(1 << VIP_INTERRUPT_READ_STREAM0));
			offset = VIP_STREAM_HALF + filled;
		} else {
			vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
				enable & ~(1 << VIP_INTERRUPT_READ_STREAM1));
			offset = filled;
		}
		filled += length;
		g_streamList[count].external = cursor;
		g_streamList[count].localLength = offset << 16 | length;
		cursor += length;
		/* the ring's data goes on at its start */
		if (cursor == end && i == 0 && g_sliceSegmentCount != 1)
			cursor = g_sliceSegments[1].start;
		count++;
		if (filled >= VIP_STREAM_HALF)
			break;
	}
	g_dataStart = cursor;
	ready = half ? readStreamHalf1Ready : readStreamHalf0Ready;
	queueListDma(VIP_STREAM_DMA,
		(uint32_t)g_streamList << 16 | count * sizeof(VipDmaDescriptor), 0, 0, ready, NULL);
}
#else
/*
 * The read stream wants a half refilled: list up to a half's worth of the stream
 * from its segments (whole 8-byte units) and queue it. Past the end of the stream
 * the half is only marked ready.
 */
STREAM_TEXT void readStreamInterrupt(uint32_t argument)
{
	VipReadStream *stream = &g_readStream;
	const VipRingSegment *segment = stream->segments;
	void (*ready)(void) = NULL;
	uint32_t enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	uint32_t filled = 0, count = 0, i;

	(void)argument;
	for (i = 0; i < stream->segmentCount; i++, segment++) {
		uint32_t end = segment->start + segment->size;
		uint32_t cursor = stream->cursor;
		uint32_t length, offset;

		if (cursor < segment->start)
			continue;
		if (cursor >= end) {
			if (stream->half & 1) {
				vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
				vipWriteControlBus(VIP_CB_SLICE_READY1, 1);
			} else {
				vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
				vipWriteControlBus(VIP_CB_SLICE_READY0, 1);
			}
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
			vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
				enable & ~(1 << VIP_INTERRUPT_READ_STREAM0));
			offset = VIP_READ_BUFFER + VIP_STREAM_HALF + filled;
			ready = readStreamHalf1Ready;
		} else {
			vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
			vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
				enable & ~(1 << VIP_INTERRUPT_READ_STREAM1));
			offset = VIP_READ_BUFFER + filled;
			ready = readStreamHalf0Ready;
		}
		stream->list[count].external = cursor;
		stream->list[count].localLength = offset << 16 | length;
		filled += length;
		stream->cursor = cursor + length;
		if (stream->cursor == segment->start + segment->size &&
		    i + 1 < stream->segmentCount)
			stream->cursor = segment[1].start;
		count = (count + 1) & 0xff;
		if (filled >= VIP_STREAM_HALF)
			break;
	}
	queueListDma(VIP_STREAM_DMA, (uint32_t)stream->list << 16 | count * sizeof(VipDmaDescriptor), 0, 0,
		ready, NULL);
	stream->half++;
}
#endif
