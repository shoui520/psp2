/*
 * PU0 task 2: the decoder's front end. The ARM side opens it through debug point 1
 * with the four rings of point 0: the elementary stream and its records, which the
 * demultiplexer (task 1) writes, and the parsed stream and its records, which go
 * on to PU1.
 *
 * Each record is one NAL unit. The decoder copies the unit into local RAM bank 1
 * (the read stream) and has the bitstream unit (control bus 0x7000) parse it: the
 * parameter sets and slice headers by hardware, with the parts the hardware leaves
 * (VUI, SEI, the slice header's reference list changes) read bit by bit here. The
 * results, and the slice data the unit decodes, go out through the write stream to
 * the parsed stream, each with a record (VipParsedRecord). The work is a chain of
 * callbacks: a DMA completion or the unit's interrupt runs the next step.
 */
#pragma once

#include "stream.h"
#include "demux.h"

#define VIP_DECODER_CHANNELS	1

/* the decoder's counters, among the statistics in DRAM, for the debugger */
typedef struct VipDecoderCounters {
	uint32_t pictures;
	uint32_t reserved0[3];
	uint32_t starved;
	uint32_t overflows;
	uint32_t reserved1[6];
	uint32_t sliceDataErrors;
	uint32_t otherErrors;
} VipDecoderCounters;

#define VIP_DECODER_COUNTERS \
	((volatile VipDecoderCounters *)(VIP_STATISTICS_ADDRESS + 0x300))

/* the rings, by debug point 0's values: VipRing addresses */
#define VIP_DECODER_RINGS		4

extern volatile uint32_t g_decoderState;

/* clock.c */
void enableDecoderClocks(void);
void gateDecoderClocks(void);
