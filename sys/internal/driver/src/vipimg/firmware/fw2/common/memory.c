/*
 * fw2 runtime: copying and zeroing DRAM through the DMA engine, which the core
 * uses to set up a context.
 */
#include "runtime.h"
#include "../../fw1/common/dma.h"

#define COPY_CHUNK	512	/* bytes through local RAM at a time */

/*
 * Queue a request with the engine's two parameters too; wait while the queue
 * is full. *done is set, and cleared when the request completes.
 */
void queueDmaWithParameters(uint32_t control, uint32_t external, uint32_t localLength,
	uint32_t parameter0, uint32_t parameter1, volatile uint32_t *done)
{
	VipDmaRequest *request;
	uint32_t psw;

	for (;;) {
		psw = vipReadControl(VIP_CR_PSW);
		mep_di();
		request = &g_dmaQueue.requests[g_dmaQueue.tail];
		if (g_dmaQueue.count < 2) {
			vipWriteControlBus(VIP_CB_DMA_DIRECTION, control);
			vipWriteControlBus(VIP_CB_DMA_EXTERNAL, external);
			vipWriteControlBus(VIP_CB_DMA_LOCAL_LENGTH, localLength);
			vipWriteControlBus(VIP_CB_DMA_PARAMETER0, parameter0);
			vipWriteControlBus(VIP_CB_DMA_PARAMETER1, parameter1);
			if (!g_dmaQueue.count)
				vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
			break;
		}
		if (g_dmaQueue.count != VIP_DMA_QUEUE_LENGTH) {
			request->control = control;
			request->external = external;
			request->localLength = localLength;
			request->parameters[0] = parameter0;
			request->parameters[1] = parameter1;
			break;
		}
		if (psw & VIP_PSW_IEC)
			mep_ei();
	}
	request->done = done;
	request->callback = NULL;
	g_dmaQueue.count++;
	if (done)
		*done = 1;
	g_dmaQueue.tail = (g_dmaQueue.tail + 1) & (VIP_DMA_QUEUE_LENGTH - 1);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/*
 * Zero size bytes of DRAM: the engine repeats 8 zero bytes over the 8-byte
 * aligned part, the core writes the rest. The original clears (address & 7)
 * bytes at the start, not up to the aligned start, which is the same when the
 * address is aligned, as every caller's is.
 */
void vipZeroDram(uint32_t address, uint32_t size)
{
	uint32_t zeros[4], *pattern;
	volatile uint32_t done = 0;
	uint8_t *bytes = (uint8_t *)address;
	uint32_t head = address & 7, body, i;

	body = size - head;
	if (body >> 3) {
		pattern = (uint32_t *)(((uint32_t)zeros + 7) & ~7u);
		pattern[1] = 0;
		pattern[0] = 0;
		queueDmaWithParameters(VIP_DMA_FILL, (address + 7) & ~7u,
			(uint32_t)pattern << 16 | body >> 3, 8, 0, &done);
	}
	for (i = 0; i < head; i++)
		bytes[i] = 0;
	for (i = body & ~7u; i < size; i++)
		bytes[i] = 0;
	while (done == 1)
		;
}

/*
 * Copy size bytes of DRAM, a word at a time when it is small, else through a
 * buffer in local RAM (on the stack) by DMA.
 */
void vipCopyDram(const uint32_t *source, uint32_t *destination, uint32_t size)
{
	uint32_t buffer[COPY_CHUNK / 4 + 2], *chunk, length, i;
	volatile uint32_t done;

	if (size < 0x3c) {
		for (i = 0; i < size >> 2; i++)
			destination[i] = source[i];
		return;
	}
	chunk = (uint32_t *)(((uint32_t)buffer + 7) & ~7u);
	do {
		length = size & ~7u;
		if (length > COPY_CHUNK)
			length = COPY_CHUNK;
		size -= length;
		queueDmaWithParameters(VIP_DMA_READ, (uint32_t)source,
			(uint32_t)chunk << 16 | length, 0, 0, NULL);
		queueDmaWithParameters(VIP_DMA_WRITE, (uint32_t)destination,
			(uint32_t)chunk << 16 | length, 0, 0, &done);
		destination += length / 4;
		source += length / 4;
		while (done == 1)
			;
	} while (size >= 8);
	if (size)
		*destination = *source;
}
