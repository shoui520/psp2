/*
 * PU0: the read and write streams. The decoder hardware reads its input from, and
 * writes its output to, a 1 KB buffer per stream in local RAM bank 1, in two
 * 512-byte halves. When it is done with a half it raises the stream's interrupt for
 * that half; the handler refills (or drains) the half by DMA from the stream's
 * DRAM segments, and tells the hardware when the half is ready.
 */
#pragma once

#include "../common/dma.h"
#include "../common/ring.h"
#include "bitstream.h"

typedef struct VipStream {
	uint32_t cursor;		/* the next DRAM address to transfer */
	VipRingSegment segments[VIP_STREAM_SEGMENTS];	/* in stream order */
	uint8_t segmentCount;
	uint8_t ended;			/* write stream: its data is no longer wanted */
	uint16_t half;			/* the half to transfer next, by its low bit */
	VipDmaDescriptor lists[2][VIP_STREAM_SEGMENTS];	/* each half's DMA list */
} VipStream;

extern VipStream g_readStream;
extern VipStream g_writeStream;

void readStreamInterrupt(uint32_t argument);
void writeStreamInterrupt(uint32_t argument);

/*
 * decoder.c: stop the bitstream unit and start it over (the write stream reached
 * the end of its last segment: what does not fit is dropped)
 */
void resetBitstream(void);
