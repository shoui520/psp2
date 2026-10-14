/*
 * fw1 runtime: code both cores run from local RAM. The DMA-complete interrupt
 * hands queued requests to the engine; it must not wait on DRAM fetches.
 */
#include "runtime.h"
#include "dma.h"
#include "interrupt.h"

#ifndef VIP_FW2
/* Report a result and state, then sleep until the debugger gives a command. */
VIP_LRAM_TEXT void waitForDebugger(uint32_t result, uint32_t state)
{
	g_status->command = result;
	g_status->state = state;
	do
		mep_halt();
	while (!g_status->command);
}
#endif

/*
 * A request completed (the interrupt, acknowledged on entry): start the one the
 * engine holds, retire the completed one, give the engine the next one waiting,
 * and run the completed one's callback.
 */
VIP_LRAM_TEXT void dmaComplete(uint32_t argument)
{
	const VipDmaRequest *request;
	void (*callback)(void);
	uint32_t count, head;

	(void)argument;
	count = g_dmaQueue.count;
	if (count >= 2)
		vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
	head = g_dmaQueue.head;
	request = &g_dmaQueue.requests[head];
	callback = request->callback;
	if (request->done)
		*request->done = 0;
	head = (head + 1) & (VIP_DMA_QUEUE_LENGTH - 1);
	g_dmaQueue.head = head;
	g_dmaQueue.count = --count;
	if (count >= 2) {
		request = &g_dmaQueue.requests[(head + 1) & (VIP_DMA_QUEUE_LENGTH - 1)];
		vipWriteControlBus(VIP_CB_DMA_DIRECTION, request->control);
		vipWriteControlBus(VIP_CB_DMA_EXTERNAL, request->external);
		vipWriteControlBus(VIP_CB_DMA_LOCAL_LENGTH, request->localLength);
		vipWriteControlBus(VIP_CB_DMA_PARAMETER0, request->parameters[0]);
		vipWriteControlBus(VIP_CB_DMA_PARAMETER1, request->parameters[1]);
	}
	if (callback)
		callback();
}
