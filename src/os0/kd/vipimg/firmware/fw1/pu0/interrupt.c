/*
 * PU0: the handlers of its interrupts.
 */
#include "interrupt.h"
#include "stream.h"

/* the interrupt vectors in local RAM of interrupts 5 to 11 */
#define VIP_DECODER_VECTORS	(VIP_LOCAL_RAM + 0x30 + 4 * VIP_INTERRUPT_DECODER)

extern const uint32_t vipDecoderVectors[7];

const VipOverlayCopy g_vectorCopy = {
	(uint32_t *)VIP_DECODER_VECTORS, vipDecoderVectors, 7
};

static void ignoreInterrupt8(uint32_t argument)
{
	(void)argument;
	vipAcknowledgeInterrupt(VIP_INTERRUPT_8);
}

static void ignoreInterrupt11(uint32_t argument)
{
	(void)argument;
	vipAcknowledgeInterrupt(VIP_INTERRUPT_11);
}

/* The ARM side wrote the mailbox: writing the word back acknowledges it. */
void acknowledgeMailbox(uint32_t argument)
{
	(void)argument;
	VIP_MAILBOX->debugSend[1] = VIP_MAILBOX->debugSend[1];
}

void (*g_interruptHandlers[INTERRUPT_SLOTS])(uint32_t argument) VIP_LRAM_DATA = {
	[INTERRUPT_SLOT_DMA] = dmaComplete,
	[INTERRUPT_SLOT_DECODER] = decoderInterrupt,
	[INTERRUPT_SLOT_READ_STREAM] = readStreamInterrupt,
	[INTERRUPT_SLOT_8] = ignoreInterrupt8,
	[INTERRUPT_SLOT_WRITE_STREAM] = writeStreamInterrupt,
	[INTERRUPT_SLOT_11] = ignoreInterrupt11,
	[INTERRUPT_SLOT_MAILBOX] = acknowledgeMailbox,
};
