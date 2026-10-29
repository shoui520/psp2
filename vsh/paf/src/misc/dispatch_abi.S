/* Original PAF object-owned virtual dispatch wrappers. */
	.syntax unified
	.thumb
	.text

	.macro PafMiscDispatch name, slot
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, lr}
	ldr r0, [r0, #0x2c]
	ldr r3, [r0]
	ldr r3, [r3, #\slot]
	blx r3
	pop {r4, pc}
	.fnend
	.size \name, . - \name
	.endm

	PafMiscDispatch ScePafMisc_7E7BC632, 0
	PafMiscDispatch ScePafMisc_E56F76BF, 4
	PafMiscDispatch ScePafMisc_B8A95C4E, 8
	PafMiscDispatch ScePafMisc_12D2FAA7, 12
