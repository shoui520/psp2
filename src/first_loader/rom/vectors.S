/*
 * The Cmep's vector table, at the vector base 0x40000: reset, and 44 vectors that
 * all end in park, a jump to itself.
 */

	.section .vectors, "ax"
	.global	vectors
vectors:
	jmp	reset
	.rept	43
	jmp	park
	.endr

	.global	park
park:		/* the 45th vector */
	jmp	park
