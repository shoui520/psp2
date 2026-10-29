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

/* The two full native initializers share an exact Thumb instruction body. */
	.macro PafInitStaticObject name
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, lr}
	movs r1, #0
	str r1, [r0, #0x18]
#if defined(FW_TYPE_CEX)
	movw r2, #0x1600
#else
	movw r2, #0x22b8
#endif
	str r1, [r0, #0x1c]
	movt r2, #0x812e
	str r1, [r0, #0x20]
	adds r2, #8
	str r1, [r0, #0x24]
	movs.w r3, #-1
	str r1, [r0, #0x28]
	str r1, [r0, #0x2c]
	str r1, [r0, #0x30]
	str r1, [r0, #0x34]
	str r1, [r0, #0x38]
	str r1, [r0, #0x3c]
	str r2, [r0]
	str r3, [r0, #4]
	movs r4, #1
	str r3, [r0, #8]
	str r1, [r0, #0xc]
	str r1, [r0, #0x10]
	str r1, [r0, #0x14]
	str r1, [r0, #0x40]
	str r1, [r0, #0x44]
	str r1, [r0, #0x48]
	str r1, [r0, #0x4c]
	strh.w r1, [r0, #0x50]
	strh.w r1, [r0, #0x52]
	str r4, [r0, #0x54]
	pop {r4, pc}
	.fnend
	.size \name, . - \name
	.endm

	PafInitStaticObject ScePafMisc_2A230DDC
	PafInitStaticObject ScePafMisc_9057D20F
