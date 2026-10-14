/* Secure monitor services used by ScePervasive. */
.syntax unified
.thumb
.arch armv7-a
.arch_extension sec
.text

.macro SERVICE name, number
	.global \name
	.type \name, %function
	.thumb_func
\name:
	movw ip, #\number
	smc #0
	bx lr
	.size \name, .-\name
.endm

SERVICE Smc117, 0x117
SERVICE Smc110, 0x110
SERVICE Smc111, 0x111
SERVICE Smc113, 0x113
SERVICE Smc114, 0x114
SERVICE Smc11B, 0x11b
SERVICE Smc11C, 0x11c
SERVICE Smc11D, 0x11d
SERVICE Smc11E, 0x11e
SERVICE Smc11F, 0x11f
SERVICE Smc121, 0x121
SERVICE SceCdramForDriver_2D728EBF, 0x118
SERVICE SceCdramForDriver_0657FC1B, 0x119
.section .note.GNU-stack,"",%progbits
