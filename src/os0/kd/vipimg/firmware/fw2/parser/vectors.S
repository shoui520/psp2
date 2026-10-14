/*
 * The parsers' interrupt vectors, interrupts 4 to 12, at the start of a parser's
 * code overlay (the runtime's vectors before them hold 0 to 3), and the entry of
 * the interrupts whose handler is C (parserInterrupt).
 */
#include "../common/runtime.h"

	.section .overlay.vectors, "ax"
	.global	vipParserVectors
vipParserVectors:
	jmp	vipTimerEntry		/* 4 */
	jmp	parserInterruptEntry	/* 5: the bitstream unit */
	jmp	parserInterruptEntry	/* 6, 7: the read stream */
	jmp	parserInterruptEntry
	jmp	interrupt8
	jmp	parserInterruptEntry	/* 9, 10: the write stream */
	jmp	parserInterruptEntry
	jmp	interrupt11
	jmp	parserInterruptEntry	/* 12: the scanner */

	.section .overlay.program, "ax"
	interruptEntry parserInterruptEntry, parserInterrupt
