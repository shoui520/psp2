/*
 * fw1 runtime: DMA between DRAM and local RAM.
 *
 * The engine takes two requests at a time; more wait in a ring of eight, which the
 * DMA-complete interrupt (lram.S) feeds to the engine, clearing each request's done
 * flag as it finishes. A request must not cross the middle of a local RAM bank, so
 * one that would is split in two.
 */
#include "runtime.h"
#include "dma.h"

VipDmaQueue g_dmaQueue VIP_LRAM_DATA;

#ifndef VIP_FW2	/* not in FW2 */
/* Wait for the engine to go idle; non-zero if it did not. */
int waitDmaIdle(void)
{
	uint32_t tries;

	for (tries = 0x3ffff; tries; tries--)
		if (!vipReadControlBus(VIP_CB_DMA_BUSY))
			return 0;
	return 1;
}

/* Copy DRAM to local RAM directly, in pieces that do not cross a chunk boundary. */
void copyByDma(uint32_t chunk, uint32_t external, uint32_t local, uint32_t size)
{
	uint32_t control, length;

	waitDmaIdle();
	control = vipReadControlBus(VIP_CB_DMA_CONTROL);
	vipWriteControlBus(VIP_CB_DMA_CONTROL, 0);
	local &= 0xffff;
	size = (size + 15) & ~15u;
	length = chunk - (local & (chunk - 1));
	while (size) {
		waitDmaIdle();
		if (size < length)
			length = size;
		vipWriteControlBus(VIP_CB_DMA_DIRECTION, VIP_DMA_READ);
		vipWriteControlBus(VIP_CB_DMA_EXTERNAL, external);
		vipWriteControlBus(VIP_CB_DMA_LOCAL_LENGTH, local << 16 | length);
		vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
		external += length;
		local += length;
		size -= length;
		length = chunk;
	}
	vipWriteControlBus(VIP_CB_DMA_CONTROL, control);
}

/* Read DRAM into local RAM, waiting until the data is there. */
void readDramFully(uint32_t external, uint32_t local, uint32_t size)
{
	if (!size)
		return;
	while (readDram(external, local, (size + 7) & ~7u) < 0)
		;
}

/* Write local RAM to DRAM, waiting until it is written. */
void writeDramFully(uint32_t external, uint32_t local, uint32_t size)
{
	if (!size)
		return;
	while (writeDram(external, local, (size + 7) & ~7u) < 0)
		;
}

#endif

void initializeDmaQueue(void)
{
	uint32_t configuration, i;

	g_dmaQueue.count = 0;
	g_dmaQueue.head = 0;
	g_dmaQueue.tail = 0;
	for (i = 0; i < VIP_DMA_QUEUE_LENGTH; i++) {
		VipDmaRequest *request = &g_dmaQueue.requests[i];

		request->control = 0;
		request->external = 0;
		request->localLength = 0;
		request->parameters[0] = 0;
		request->parameters[1] = 0;
		request->callback = NULL;
		request->done = NULL;
	}
	configuration = vipReadControl(VIP_CR_RCFG);
	g_dmaQueue.bankSize[0] = ((configuration & 0x7f0000) >> 16) << 10;
	g_dmaQueue.bankSize[1] = (configuration & 0xff) << 10;
	vipWriteControlBus(VIP_CB_DMA_CONTROL, 0);
	vipWriteControlBus(VIP_CB_DMA_CONTROL, 1);
}

/* Queue a request, or give it to the engine when it has room; wait while the queue is full. */
void queueDma(uint32_t control, uint32_t external, uint32_t localLength,
	volatile uint32_t *done)
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
			if (!g_dmaQueue.count)
				vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
			break;
		}
		if (g_dmaQueue.count != VIP_DMA_QUEUE_LENGTH) {
			request->control = control;
			request->external = external;
			request->localLength = localLength;
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

/* The offset of a local address in its bank, and the size of that bank. */
static inline uint32_t bankOffset(uint32_t local, uint32_t *bankSize)
{
	uint32_t offset = local & 0xffff;

	if (offset < g_dmaQueue.bankSize[0]) {
		*bankSize = g_dmaQueue.bankSize[0];
		return local & (g_dmaQueue.bankSize[0] - 1);
	}
	*bankSize = g_dmaQueue.bankSize[1];
	return offset - (((vipReadControl(VIP_CR_RCFG) >> 12) & 0xf) << 12);
}

static inline int transfer(uint32_t read, uint32_t external, uint32_t local, uint32_t size)
{
	volatile uint32_t done;
	uint32_t bankSize, half, offset, first, control;

	offset = bankOffset(local, &bankSize);
	half = bankSize >> 1;
	if (offset < half && offset + size > half) {
		first = half - offset;
		control = read | ((local & VIP_LOCAL_BANK1) ? VIP_DMA_BANK1 : 0);
		queueDma(control, external, local << 16 | first, NULL);
		size -= first;
		local += first & ~3u;
		external += first & ~3u;
	}
	control = read | ((local & VIP_LOCAL_BANK1) ? VIP_DMA_BANK1 : 0);
	queueDma(control, external, local << 16 | size, &done);
	while (done == 1)
		;
	return 0;
}

int readDram(uint32_t external, uint32_t local, uint32_t size)
{
	return transfer(VIP_DMA_READ, external, local, size);
}

int writeDram(uint32_t external, uint32_t local, uint32_t size)
{
	return transfer(VIP_DMA_WRITE, external, local, size);
}

#ifndef VIP_FW2
/* Copy one of the program's local RAM sections in, if it lies in its 4 KB page. */
static void loadSection(uint32_t load, uint32_t start, uint32_t size, uint32_t page)
{
	page &= ~0xfffu;
	if (size && start >= page && start + size <= page + 0x1000)
		copyByDma(2048, load, start, size);
}

void loadLocalRam(void)
{
	loadSection((uint32_t)__lram_text_load, (uint32_t)__lram_text_start,
		(uint32_t)__lram_text_size, VIP_TASK_CODE);
	loadSection((uint32_t)__lram_data_load, (uint32_t)__lram_data_start,
		(uint32_t)__lram_data_size, VIP_TASK_DATA);
}
#endif
