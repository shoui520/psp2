/*
 * Copy engines A and B. Engine A moves frame data (forward or backward, so
 * overlapping moves inside a frame are safe) and waits for completion;
 * engine B only starts a copy and calls a completion hook. Each routine
 * first offers the call to a ROM-patch override. The filename is inferred.
 */
#include <stddef.h>
#include "robin/dma.h"
#include "robin/patch.h"
#include "robin/soc.h"
#include "robin/threadx.h"

static ROBIN_PATCH_SLOT(wlanDmaMoveForwardOverride);
static ROBIN_PATCH_SLOT(wlanDmaMoveBackwardOverride);
WlanDmaCompletionHook wlanDmaCompletionHook __attribute__((section(".itcm.rw"))) = NULL;

#define DMA_START_FORWARD	0x1bc0
#define DMA_START_BACKWARD	0x1a14
#define DMA_START_BACKWARD_BYTES 0x1b54

/* Copies length bytes upwards from source to destination. */
void wlanDmaMoveForward(uint8_t *destination, const uint8_t *source, uint32_t length)
{
	unsigned int posture;

	if (wlanDmaMoveForwardOverride((uintptr_t)destination, (uintptr_t)source, length, 0))
		return;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	while (DMA_A_CONTROL & DMA_CONTROL_BUSY)
		;
	DMA_A_CONTROL = DMA_CONTROL_RESET;
	DMA_A_SOURCE = (uintptr_t)source;
	DMA_A_DESTINATION = (uintptr_t)destination;
	DMA_A_CONFIG = 0;
	DMA_A_LENGTH = length;
	DMA_A_CONTROL |= DMA_START_FORWARD;
	do {
		while (DMA_A_CONTROL & DMA_CONTROL_BUSY)
			;
	} while (DMA_A_LENGTH != 0);
	tx_interrupt_control(posture);
}

/*
 * Copies length bytes from the end downwards. Up to three bytes at the end are
 * moved by the CPU to align the engine's first word, the bulk goes in 32-byte
 * units, and the remainder is moved last.
 */
void wlanDmaMoveBackward(uint8_t *destination, const uint8_t *source, uint32_t length)
{
	const uint8_t *sourceEnd;
	uint8_t *destinationEnd;
	unsigned int posture;
	uint32_t unit;
	uint32_t control;
	uint32_t head;
	uint32_t tail;
	uint32_t i;

	if (wlanDmaMoveBackwardOverride((uintptr_t)destination, (uintptr_t)source, length, 0))
		return;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	while (DMA_A_CONTROL & DMA_CONTROL_BUSY)
		;
	DMA_A_CONTROL = DMA_CONTROL_RESET;
	sourceEnd = source + length;
	destinationEnd = destination + length;
	if (length >= 0x20) {
		unit = 0x20;
		control = 0x1c0;
		head = (uintptr_t)sourceEnd & 3;
		tail = (length - head) & 0x1f;
	} else {
		unit = 1;
		control = 0x140;
		head = 0;
		tail = 0;
	}
	for (i = 1; i <= head; ++i)
		destinationEnd[-(int)i] = sourceEnd[-(int)i];
	DMA_A_SOURCE = (uintptr_t)sourceEnd - head - unit;
	DMA_A_DESTINATION = (uintptr_t)destinationEnd - head - unit;
	DMA_A_CONFIG = 0;
	DMA_A_LENGTH = length - tail - head;
	DMA_A_CONTROL |= control | DMA_START_BACKWARD;
	while (DMA_A_CONTROL & DMA_CONTROL_BUSY)
		;
	if (tail != 0) {
		DMA_A_CONTROL = DMA_CONTROL_RESET;
		DMA_A_SOURCE = (uintptr_t)source + tail - 1;
		DMA_A_DESTINATION = (uintptr_t)destination + tail - 1;
		DMA_A_CONFIG = 0;
		DMA_A_LENGTH = tail;
		DMA_A_CONTROL |= DMA_START_BACKWARD_BYTES;
		do {
			while (DMA_A_CONTROL & DMA_CONTROL_BUSY)
				;
		} while (DMA_A_LENGTH != 0);
	}
	tx_interrupt_control(posture);
}

/* Starts a copy on engine B and tells the completion hook. */
void wlanDmaStartCopyB(uint8_t *destination, const uint8_t *source, uint32_t length)
{
	unsigned int posture = tx_interrupt_control(TX_INT_DISABLE);

	while (DMA_B_CONTROL & DMA_CONTROL_BUSY)
		;
	DMA_B_CONTROL = DMA_CONTROL_RESET;
	DMA_B_SOURCE = (uintptr_t)source;
	DMA_B_DESTINATION = (uintptr_t)destination;
	DMA_B_CONFIG = 0;
	DMA_B_LENGTH = length;
	DMA_B_CONTROL |= DMA_START_FORWARD;
	wlanDmaCompletionHook();
	tx_interrupt_control(posture);
}
