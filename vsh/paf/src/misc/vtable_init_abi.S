/* Native constructors installing the firmware-specific static vtable. */
	.syntax unified
	.thumb
	.text

	.macro PafSetStaticVtable name
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
#if defined(FW_TYPE_CEX)
	movw r1, #0x1600
#else
	movw r1, #0x22b8
#endif
	movt r1, #0x812e
	adds r1, #8
	str r1, [r0]
	bx lr
	.fnend
	.size \name, . - \name
	.endm

	PafSetStaticVtable ScePafMisc_34B5499D
	PafSetStaticVtable ScePafMisc_7D7D1EB2
