/*
 * PU1: the handlers of its interrupts.
 */
#include "interrupt.h"
#include "stream.h"

/* the interrupt vectors in local RAM of interrupts 5 to 8 */
#define VIP_STREAM_VECTORS	(VIP_LOCAL_RAM + 0x30 + 4 * VIP_INTERRUPT_DECODER)

extern const uint32_t vipStreamVectors[4];

const VipOverlayCopy g_vectorCopy = {
	(uint32_t *)VIP_STREAM_VECTORS, vipStreamVectors, 4
};

/* what the ARM side last wrote to the mailbox (nothing reads it) */
static uint32_t g_mailboxValue;

/* The ARM side wrote the mailbox: writing the word back acknowledges it. */
void acknowledgeMailbox(uint32_t argument)
{
	(void)argument;
	VIP_MAILBOX->debugSend[0] = VIP_MAILBOX->debugSend[0];
	g_mailboxValue |= VIP_MAILBOX->debugSend[0];
}

void (*g_interruptHandlers[INTERRUPT_SLOTS])(uint32_t argument) VIP_LRAM_DATA = {
	[INTERRUPT_SLOT_DMA] = dmaComplete,
	[INTERRUPT_SLOT_READ_STREAM] = readStreamInterrupt,
	[INTERRUPT_SLOT_MAILBOX] = acknowledgeMailbox,
};
