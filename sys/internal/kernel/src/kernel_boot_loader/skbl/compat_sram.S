/*
 * The two pieces of code SKBL copies into Compat SRAM (physical 0x1f000000,
 * which Pervasive aliases at address 0 for the ARM cores) for the code that
 * runs there; they run from the copy, not from here.
 *
 * compatSramCoreSync lines the four cores up one at a time. Core 0 takes the
 * lock word, waits for the other three to arrive, and while it alone runs
 * opens LPDDR0 firewall region 3; then each core in turn takes the lock, hands
 * the turn to the next and releases the lock, and all leave once the turn is
 * back at core 0.
 *
 * compatSramResume is the resume image, copied to address 0 with the suspend
 * information after it: the reset vector at 0 jumps to the Thumb code at 0xc,
 * which turns the MMU on with a flat map of Compat SRAM, enters the high
 * alias at 0x80000000 + 0x200 and there restores the CP15 state saved at
 * suspend before it returns to the address saved with it.
 */

#include <buserror/ssx_firewall_regs.h>

	.syntax	unified

/* the lock, turn and arrival count compatSramCoreSync's r0 points to */
#define CORE_SYNC_LOCK		0
#define CORE_SYNC_TURN		4
#define CORE_SYNC_ARRIVED	8

#define REGION3_ADDR_MATCH	(SSX_FIREWALL_REGION(3) + SSX_FIREWALL_REGION_ADDR_MATCH)
#define REGION3_REQ_INFO_PERMISSION (SSX_FIREWALL_REGION(3) + SSX_FIREWALL_REGION_REQ_INFO_PERMISSION)

#define SCTLR_M		0x00000001
#define SCTLR_A		0x00000002
#define SCTLR_C		0x00000004
#define SCTLR_SW	0x00000400
#define SCTLR_Z		0x00000800
#define SCTLR_I		0x00001000
#define SCTLR_V		0x00002000
#define SCTLR_EE	0x02000000
#define SCTLR_TRE	0x10000000
#define SCTLR_AFE	0x20000000
#define SCTLR_TE	0x40000000

/* Compat SRAM seen from the resume image */
#define COMPAT_SRAM			0x1f000000
#define COMPAT_SRAM_HIGH		0x80000000	/* the second alias the MMU map adds */
#define RESUME_PAGE_TABLE		(COMPAT_SRAM + 0x4000)	/* 16 KiB first-level table */
#define RESUME_PAGE_TABLE_SIZE		0x4000
#define RESUME_SECTION_COMPAT_SRAM	0x1f091402	/* 1 MiB section, normal memory */
#define RESUME_SECTION_DEVICE		0xe2090402	/* 1 MiB section, 0xe2000000 */
#define RESUME_TTBR_ATTRIBUTES		0x42
#define RESUME_TTBCR			2
#define RESUME_CONTEXT_ID		0xf7
#define RESUME_COPY_DESTINATION		(COMPAT_SRAM + 0x1008)	/* where the copy goes */
#define RESUME_RETURN_ADDRESS		(COMPAT_SRAM + 0x100c)
#define RESUME_CP15_STATE		(COMPAT_SRAM + 0x1010)
#define RESUME_COPY_SOURCE		(COMPAT_SRAM + 0x1800)	/* its size first */
#define RESUME_POISON			0xdeadbeef

	.section .rodata.compat_sram, "a", %progbits
	.balign	4

	.global	compatSramImage			/* what SKBL copies: data, not a Thumb address */
	.type	compatSramImage, %object
compatSramImage:
	.thumb
	.global	compatSramCoreSync
	.type	compatSramCoreSync, %function
compatSramCoreSync:
	mrc	p15, 0, r1, c0, c0, 5		/* MPIDR */
	ands	r1, r1, #3
	beq	1f
0:	ldrex	r1, [r0, #CORE_SYNC_ARRIVED]
	adds	r1, #1
	strex	r2, r1, [r0, #CORE_SYNC_ARRIVED]
	cmp	r2, #0
	bne	0b
	sev
	mrc	p15, 0, r1, c0, c0, 5
	ands	r1, r1, #3
0:	ldr	r2, [r0, #CORE_SYNC_TURN]
	cmp	r1, r2
	it	ne
	wfene
	bne	0b
	isb	sy
0:	ldrex	r2, [r0, #CORE_SYNC_LOCK]
	cmp	r2, #0
	it	ne
	wfene
	bne	0b
	strex	r2, lr, [r0, #CORE_SYNC_LOCK]
	cmp	r2, #0
	bne	0b
	dmb	sy
	b	2f

1:	str.w	lr, [r0, #CORE_SYNC_LOCK]	/* core 0 */
	str	r1, [r0, #CORE_SYNC_TURN]
	dsb	sy
	movw	r1, #:lower16:SSX_FIREWALL_LPDDR0_BASE
	movt	r1, #:upper16:SSX_FIREWALL_LPDDR0_BASE
	ldrd	r2, r3, [r1, #REGION3_ADDR_MATCH]
	dsb	sy
0:	ldr	r1, [r0, #CORE_SYNC_ARRIVED]
	cmp	r1, #3
	it	ne
	wfene
	bne	0b

2:	isb	sy
	mrc	p15, 0, r1, c0, c0, 5
	ands	r1, r1, #3
	bne	3f
	movw	r1, #:lower16:SSX_FIREWALL_LPDDR0_BASE
	movt	r1, #:upper16:SSX_FIREWALL_LPDDR0_BASE
	movw	r2, #0x0061			/* 0x40000000 and up, enabled */
	movt	r2, #0x4000
	mov.w	r3, #0
	strd	r2, r3, [r1, #REGION3_ADDR_MATCH]
	mov.w	r2, #1
	strd	r2, r3, [r1, #REGION3_REQ_INFO_PERMISSION]
	dmb	sy
	ldrd	r2, r3, [r1, #REGION3_REQ_INFO_PERMISSION]
	dsb	sy
3:	isb	sy
	mrc	p15, 0, r1, c0, c0, 5
	adds	r1, #1
	ands	r1, r1, #3
	str	r1, [r0, #CORE_SYNC_TURN]	/* the next core's turn */
	sev
	mov.w	r2, #0
	dmb	sy
	str	r2, [r0, #CORE_SYNC_LOCK]
	dsb	sy
	sev
0:	ldr	r1, [r0, #CORE_SYNC_TURN]
	cmp	r1, #0
	it	ne
	wfene
	bne	0b
	bx	lr
	nop					/* to 32 bytes */
	nop.w
	nop.w
	.size	compatSramCoreSync, . - compatSramCoreSync

	.arm
	.global	compatSramResume
	.type	compatSramResume, %function
compatSramResume:
	cpsid	if
	clrex
	b	resumeJump

	.thumb
resumeStart:
	mov.w	r0, #0				/* SCTLR, everything off */
	bic	r0, r0, #SCTLR_TE
	bic	r0, r0, #SCTLR_TRE
	bic	r0, r0, #SCTLR_AFE
	bic	r0, r0, #SCTLR_EE
	bic	r0, r0, #SCTLR_V
	bic	r0, r0, #SCTLR_I
	bic	r0, r0, #SCTLR_Z
	bic	r0, r0, #SCTLR_SW
	bic	r0, r0, #SCTLR_A
	bic	r0, r0, #SCTLR_C
	bic	r0, r0, #SCTLR_M
	mcr	p15, 0, r0, c1, c0, 0
	mov.w	r0, #0
	mcr	p15, 0, r0, c7, c1, 0		/* ICIALLUIS */
	dsb	sy
	mcr	p15, 0, r0, c7, c1, 6		/* BPIALLIS */
	isb	sy
	mcr	p15, 0, r0, c7, c5, 0		/* ICIALLU */
	dsb	sy
0:	mov.w	r1, #0				/* invalidate the L1 data cache by set/way */
1:	orr.w	r2, r1, r0
	mcr	p15, 0, r2, c7, c6, 2		/* DCISW */
	add.w	r1, r1, #0x40000000
	cmp	r1, #0
	bne	1b
	add.w	r0, r0, #32
	cmp.w	r0, #0x2000
	bne	0b
	dsb	sy
	mcr	p15, 0, r0, c7, c5, 6		/* BPIALL */
	isb	sy
	mcr	p15, 0, r0, c8, c3, 0		/* TLBIALLIS */
	mov.w	r0, #0
	mcr	p15, 0, r0, c7, c5, 0		/* ICIALLU */
	mov.w	r0, #0
	mcr	p15, 0, r0, c8, c7, 0		/* TLBIALL */
	mcr	p15, 0, r0, c2, c0, 2		/* TTBCR */
	mov.w	r0, #0
	mcr	p15, 0, r0, c7, c5, 6		/* BPIALL */
	isb	sy
	mrc	p15, 0, r1, c1, c0, 0
	bic	r1, r1, #SCTLR_A
	mcr	p15, 0, r1, c1, c0, 0

	movw	r0, #:lower16:RESUME_PAGE_TABLE
	movt	r0, #:upper16:RESUME_PAGE_TABLE
	mov.w	r1, #0
	mov.w	r2, #RESUME_PAGE_TABLE_SIZE
	bl	resumeMemset
	movw	r0, #:lower16:RESUME_PAGE_TABLE
	movt	r0, #:upper16:RESUME_PAGE_TABLE
	mov.w	r1, #COMPAT_SRAM >> 20
	movw	r2, #:lower16:RESUME_SECTION_COMPAT_SRAM
	movt	r2, #:upper16:RESUME_SECTION_COMPAT_SRAM
	str.w	r2, [r0, r1, lsl #2]
	mov.w	r1, #RESUME_SECTION_DEVICE >> 20
	movw	r2, #:lower16:RESUME_SECTION_DEVICE
	movt	r2, #:upper16:RESUME_SECTION_DEVICE
	str.w	r2, [r0, r1, lsl #2]
	mov.w	r1, #COMPAT_SRAM_HIGH >> 20
	movw	r2, #:lower16:RESUME_SECTION_COMPAT_SRAM
	movt	r2, #:upper16:RESUME_SECTION_COMPAT_SRAM
	str.w	r2, [r0, r1, lsl #2]
	orrs	r0, r0, #RESUME_TTBR_ATTRIBUTES
	mcr	p15, 0, r0, c2, c0, 0		/* TTBR0 */
	mcr	p15, 0, r0, c2, c0, 1		/* TTBR1 */
	mov.w	r0, #RESUME_TTBCR
	mcr	p15, 0, r0, c2, c0, 2
	mov.w	r0, #0x55555555			/* DACR: all client */
	mcr	p15, 0, r0, c3, c0, 0
	mov.w	r0, #RESUME_CONTEXT_ID
	mcr	p15, 0, r0, c13, c0, 1		/* CONTEXTIDR */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #SCTLR_TE
	orrs	r0, r0, #SCTLR_AFE
	orrs	r0, r0, #0x4000			/* RR */
	bic	r0, r0, #SCTLR_TRE
	bic	r0, r0, #SCTLR_V
	orrs	r0, r0, #SCTLR_I
	orrs	r0, r0, #SCTLR_Z
	bic	r0, r0, #SCTLR_SW
	orrs	r0, r0, #SCTLR_C
	bic	r0, r0, #SCTLR_A
	orrs	r0, r0, #SCTLR_M
	mcr	p15, 0, r0, c1, c0, 0
	mov.w	r1, #0
	mcr	p15, 0, r1, c7, c5, 0		/* ICIALLU */
	movw	r0, #:lower16:(COMPAT_SRAM_HIGH + 0x200 + 1)
	movt	r0, #:upper16:(COMPAT_SRAM_HIGH + 0x200 + 1)
	bx	r0				/* resumeRestore, through the high alias */
	.rept	5				/* to 64 bytes */
	nop.w
	.endr
	.org	compatSramResume + 0x200

resumeRestore:
	movw	r0, #:lower16:RESUME_CP15_STATE
	movt	r0, #:upper16:RESUME_CP15_STATE
	ldmia	r0!, {r4, r5, r6}
	mcr	p15, 0, r4, c1, c0, 0		/* SCTLR */
	mcr	p15, 0, r6, c1, c0, 2		/* CPACR */
	ldmia.w	r0!, {r4-r11}
	mcr	p15, 0, r4, c2, c0, 0		/* TTBR0 */
	mcr	p15, 0, r6, c2, c0, 2		/* TTBCR */
	mcr	p15, 0, r8, c10, c2, 0		/* PRRR */
	mcr	p15, 0, r9, c10, c2, 1		/* NMRR */
	mcr	p15, 0, r10, c12, c0, 0		/* VBAR */
	mcr	p15, 0, r11, c13, c0, 1		/* CONTEXTIDR */
	mov	r1, r5				/* the return values */
	mov	r2, r11
	mov	r3, r7
	ldmia	r0!, {r4, r5, r6}
	mcr	p15, 0, r4, c13, c0, 2		/* TPIDRURW */
	mcr	p15, 0, r5, c13, c0, 3		/* TPIDRURO */
	mcr	p15, 0, r6, c13, c0, 4		/* TPIDRPRW */
	mov	r4, r1
	mov	r5, r2
	mov	r6, r3
	dsb	sy
	isb	sy
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR */
	ands	r0, r0, #15
	bne	0f
	movw	r0, #:lower16:RESUME_COPY_DESTINATION
	movt	r0, #:upper16:RESUME_COPY_DESTINATION
	ldr	r0, [r0]
	movw	r1, #:lower16:RESUME_COPY_SOURCE
	movt	r1, #:upper16:RESUME_COPY_SOURCE
	ldr	r2, [r1]
	bl	resumeMemcpy
	sev
	b	1f
0:	wfe
1:	movw	r0, #:lower16:RESUME_RETURN_ADDRESS
	movt	r0, #:upper16:RESUME_RETURN_ADDRESS
	ldr.w	lr, [r0]
	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	movw	r3, #:lower16:RESUME_POISON
	movt	r3, #:upper16:RESUME_POISON
	mov	r4, r3
	mov	r5, r3
	mov	r6, r3
	mov	r7, r3
	mov	r8, r3
	mov	r9, r3
	mov	r10, r3
	mov	r11, r3
	mov	r12, r3
	bx	lr

/* r2 bytes, a word at a time */
resumeMemcpy:
	mov	r12, r0
0:	ldmia	r1!, {r3}
	stmia	r0!, {r3}
	subs	r2, #4
	bne	0b
	mov	r0, r12
	bx	lr

resumeMemset:
	mov	r12, r0
0:	strb.w	r1, [r0], #1
	subs	r2, #1
	bne	0b
	mov	r0, r12
	bx	lr
	nop					/* to 64 bytes */
	.rept	15
	nop.w
	.endr
	.org	compatSramResume + 0x400

	.arm
resumeJump:					/* the reset vector's target */
	ldr	pc, [pc, #-4]
	.word	resumeStart - compatSramResume + 1
	.size	compatSramResume, . - compatSramResume

	.global	compatSramResumeEnd		/* the end of the copy */
compatSramResumeEnd:

/*
 * The TrustZone module images follow compatSramResumeEnd in SKBL's read-only
 * data: SceSysmem's ARZL archive, then the plain ELFs of SceExcpmgr,
 * SceKernelIntrMgr, SceKernelBusError, SceSblSmsched and SceDriverTzs, end to
 * end. The build makes them from their own folders under slb2:kbl/ and names
 * each file here (CMakeLists.txt: TZS_*_IMAGE; tools/arzl.py packs the archive).
 */
	.global	tzsSysmemArchive, tzsExcpmgrImage, tzsIntrMgrImage, tzsBusErrorImage
	.global	tzsSmschedImage, tzsDriverTzsImage, tzsImagesEnd
tzsSysmemArchive:
	.incbin	TZS_SYSMEM_ARCHIVE
tzsExcpmgrImage:
	.incbin	TZS_EXCPMGR_IMAGE
tzsIntrMgrImage:
	.incbin	TZS_INTRMGR_IMAGE
tzsBusErrorImage:
	.incbin	TZS_BUSERROR_IMAGE
tzsSmschedImage:
	.incbin	TZS_SMSCHED_IMAGE
tzsDriverTzsImage:
	.incbin	TZS_DRIVERTZS_IMAGE
tzsImagesEnd:
