/*
 * The codec's interrupt vectors, interrupts 4 to 8, at the start of its code
 * overlay (the runtime's vectors before them hold 0 to 3). Both halves of the read
 * stream go to readStreamInterrupt, which tells them apart; its entry saves what
 * the C handler does not (the firmware's handler is an interrupt function).
 */
#include "../common/runtime.h"

	.section .overlay.vectors, "ax"
	.global	vipCodecVectors
vipCodecVectors:
	jmp	vipTimerEntry		/* 4 */
	jmp	interrupt5		/* 5: the decoder */
	jmp	readStreamEntry		/* 6, 7: the read stream */
	jmp	readStreamEntry
	jmp	interrupt8

	.section .overlay.program, "ax"
	interruptEntry readStreamEntry, readStreamInterrupt
