/*
 * PU1: the read stream. The decoder reads the slice data through a 1 KB buffer in
 * local RAM bank 1, in two 512-byte halves, as PU0's bitstream unit does
 * (pu0/stream.h). When it wants a half refilled it raises that half's interrupt;
 * the handler lists up to a half's worth of the stream from its DRAM segments,
 * disables the other half's interrupt, and queues the DMA, whose callback tells
 * the hardware the half is ready and enables the other half's interrupt again, so
 * the halves fill in order.
 */
#pragma once

#include "../common/dma.h"
#include "../common/ring.h"
#include "units.h"

/* task 0, the decoder: its code overlay at VIP_TASK_CODE, its data at VIP_TASK_DATA */
#define VIP_TASK0_TEXT		__attribute__((section(".task0.text")))
#define VIP_TASK0_DATA		__attribute__((section(".task0.data")))
#define VIP_TASK0_BSS		__attribute__((section(".task0.bss")))

typedef struct VipReadStream {
	VipRingSegment segments[VIP_STREAM_SEGMENTS];	/* in stream order */
	uint32_t cursor;		/* the next DRAM address to read */
	uint16_t half;			/* the half to fill next, by its low bit */
	uint8_t segmentCount;
	VipDmaDescriptor list[VIP_STREAM_SEGMENTS];
} VipReadStream;

extern VipReadStream g_readStream;

void readStreamInterrupt(uint32_t argument);
