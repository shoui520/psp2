/*
 * fw1 runtime: the DMA request queue. The engine holds two requests, the one it
 * runs and the next; the others wait here until the DMA-complete interrupt
 * (lram.c) gives them to the engine.
 */
#pragma once

#include "runtime.h"

#define VIP_DMA_QUEUE_LENGTH	8

typedef struct VipDmaRequest {
	uint32_t control;		/* VIP_CB_DMA_DIRECTION */
	uint32_t external;		/* VIP_CB_DMA_EXTERNAL */
	uint32_t localLength;		/* VIP_CB_DMA_LOCAL_LENGTH */
	uint32_t parameters[2];		/* VIP_CB_DMA_PARAMETER0, 1 */
	void (*callback)(void);		/* run when the request completes */
	volatile uint32_t *done;	/* cleared when the request completes */
} VipDmaRequest;

typedef struct VipDmaQueue {
	uint32_t count;			/* requests not completed, the engine's included */
	uint32_t head;			/* the request the engine runs */
	uint32_t tail;			/* where the next one goes */
	VipDmaRequest requests[VIP_DMA_QUEUE_LENGTH];
	uint32_t bankSize[2];		/* the sizes of local RAM banks 0 and 1 */
} VipDmaQueue;

/* a VIP_DMA_LIST request transfers a list of these */
typedef struct VipDmaDescriptor {
	uint32_t external;
	uint32_t localLength;		/* local offset << 16 | length */
} VipDmaDescriptor;

extern VipDmaQueue g_dmaQueue;

void queueDma(uint32_t control, uint32_t external, uint32_t localLength,
	volatile uint32_t *done);
/* PU0's and FW2's parsers' form */
#if defined(VIP_PROGRAM_fw1_pu0) || defined(VIP_PROGRAM_fw2_parser0) || \
	defined(VIP_PROGRAM_fw2_parser1)
int queueListDma(uint32_t control, uint32_t localLength, void (*callback)(void),
	volatile uint32_t *done);
#else
int queueListDma(uint32_t control, uint32_t localLength, uint32_t parameter0,
	uint32_t parameter1, void (*callback)(void), volatile uint32_t *done);
#endif
