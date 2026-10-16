/*
 * power_entry.S: the last step of sceSysconSetPowerMode (power_tzs.c), which
 * copies this code to the 32 KiB power scratchpad (0x1f000000) and jumps to
 * it with the syscon packet: r0 the packet, r1 its size in bytes. It runs
 * from there with the MMU and caches off, since the DRAM goes into self
 * refresh under it:
 *
 *	1. the CDRAM (SceEmcTop) into self refresh, if CompatRAM is out of reset;
 *	2. each LPDDR2 interface into self refresh, then held in reset with its
 *	   clock stopped;
 *	3. the packet out on the syscon's SPI0, with its request line raised;
 *	4. wait for the syscon to take it, and stay here until it powers down.
 *
 * Only assembly can run with no stack and no memory but the scratchpad.
 * Internal names are unknown. The register bases come from the shared
 * headers; the offsets below are the fields of their structs.
 */
#include <sce/emc_regs.h>
#include <sce/gpio_regs.h>
#include <sce/lpddr2_regs.h>
#include <sce/pervasive_regs.h>
#include <sce/spi_regs.h>

	.syntax unified
	.arch armv7-a
	.thumb

/* SCTLR */
	.equ	SCTLR_M,		0x0001
	.equ	SCTLR_C,		0x0004
	.equ	SCTLR_I,		0x1000

/* ScePervasiveDeviceRegisters, sce/pervasive_regs.h: reset and gate pages */
	.equ	PERVASIVE_COMPAT_RAM,	0x028
	.equ	PERVASIVE_LPDDR2_MAIN,	0x160
	.equ	PERVASIVE_LPDDR2_SUB,	0x164

/* SceEmcRegisters, sce/emc_regs.h */
	.equ	EMC_COMMAND,		0x24

/* SceLpddr2Registers, sce/lpddr2_regs.h: the control words, a nibble per
 * channel, then their status */
	.equ	LPDDR2_CHANNEL,		0x8000
	.equ	LPDDR2_CHANNEL_STATUS,	0x20
	.equ	LPDDR2_SELF_REFRESH,	4	/* a channel's command and status nibble */
	.equ	LPDDR2_MAIN_ALL,	0x4444	/* its four channels in self refresh */
	.equ	LPDDR2_SUB_ALL,		0x44	/* its two */

/* SceGpioRegisters, sce/gpio_regs.h */
	.equ	GPIO_INPUT,		0x04
	.equ	GPIO_OUTPUT_SET,	0x08
	.equ	GPIO_OUTPUT_CLEAR,	0x0c
	.equ	GPIO_INTERRUPT_MASK,	0x1c	/* 5 words */
	.equ	GPIO_INTERRUPT_STATUS,	0x38	/* 5 words */
	.equ	GPIO0_PORT0,		0x01
	.equ	GPIO0_SYSCON_REQUEST,	0x08	/* port 3 */
	.equ	GPIO0_SYSCON_READY,	0x10	/* port 4 */
	.equ	GPIO1_PORT7,		0x80

/* SceSpiRegisters, sce/spi_regs.h: SPI0 is the syscon's */
	.equ	SPI_TRANSMIT,		0x04
	.equ	SPI_CONTROL,		0x08
	.equ	SPI_ENABLE,		0x10

	.section .text.power_entry,"ax",%progbits
	.global	PowerModeEntry
	.type	PowerModeEntry, %function
	.thumb_func
PowerModeEntry:
	cpsid	aif
	dsb	sy
	isb	sy
	mrc	p15, 0, r4, c1, c0, 0		/* MMU and caches off */
	bic	r4, r4, #SCTLR_I
	bic	r4, r4, #SCTLR_C
	bic	r4, r4, #SCTLR_M
	mcr	p15, 0, r4, c1, c0, 0
	isb	sy

	movw	r4, #:lower16:(PERVASIVE_RESET_BASE + PERVASIVE_COMPAT_RAM)
	movt	r4, #:upper16:(PERVASIVE_RESET_BASE + PERVASIVE_COMPAT_RAM)
	ldr	r5, [r4]
	cbnz	r5, 2f				/* the CDRAM is in reset */
	movw	r4, #:lower16:(EMC_TOP_BASE + EMC_COMMAND)
	movt	r4, #:upper16:(EMC_TOP_BASE + EMC_COMMAND)
	ldr	r5, [r4]
	tst.w	r5, #EMC_COMMAND_IN_SELF_REFRESH
	bne	2f
	mov.w	r5, #EMC_COMMAND_SELF_REFRESH
	str	r5, [r4]
	dmb	sy
1:	ldr	r5, [r4]
	tst.w	r5, #EMC_COMMAND_IN_SELF_REFRESH
	beq	1b

2:	movw	r4, #:lower16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_MAIN)
	movt	r4, #:upper16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_MAIN)
	ldr	r5, [r4]
	cbnz	r5, 4f
	mov.w	r4, #LPDDR2_MAIN_REG_BASE
	adds.w	r4, r4, #LPDDR2_CHANNEL
	mov.w	r5, #LPDDR2_SELF_REFRESH
	str	r5, [r4]
	str	r5, [r4, #0x08]
	str	r5, [r4, #0x10]
	str	r5, [r4, #0x18]
	dmb	sy
	movw	r5, #LPDDR2_MAIN_ALL
	movt	r5, #0
3:	ldr	r6, [r4, #LPDDR2_CHANNEL_STATUS]
	cmp	r5, r6
	bne	3b
	movw	r4, #:lower16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_MAIN)
	movt	r4, #:upper16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_MAIN)
	mov.w	r5, #1
	str	r5, [r4]
	movw	r4, #:lower16:(PERVASIVE_GATE_BASE + PERVASIVE_LPDDR2_MAIN)
	movt	r4, #:upper16:(PERVASIVE_GATE_BASE + PERVASIVE_LPDDR2_MAIN)
	mov.w	r5, #0
	str	r5, [r4]

4:	movw	r4, #:lower16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_SUB)
	movt	r4, #:upper16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_SUB)
	ldr	r5, [r4]
	cbnz	r5, 6f
	movw	r4, #:lower16:LPDDR2_SUB_REG_BASE
	movt	r4, #:upper16:LPDDR2_SUB_REG_BASE
	adds.w	r4, r4, #LPDDR2_CHANNEL
	mov.w	r5, #LPDDR2_SELF_REFRESH
	str	r5, [r4]
	str	r5, [r4, #0x08]
	dmb	sy
	movw	r5, #LPDDR2_SUB_ALL
	movt	r5, #0
5:	ldr	r6, [r4, #LPDDR2_CHANNEL_STATUS]
	cmp	r5, r6
	bne	5b
	movw	r4, #:lower16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_SUB)
	movt	r4, #:upper16:(PERVASIVE_RESET_BASE + PERVASIVE_LPDDR2_SUB)
	mov.w	r5, #1
	str	r5, [r4]
	movw	r4, #:lower16:(PERVASIVE_GATE_BASE + PERVASIVE_LPDDR2_SUB)
	movt	r4, #:upper16:(PERVASIVE_GATE_BASE + PERVASIVE_LPDDR2_SUB)
	mov.w	r5, #0
	str	r5, [r4]

6:	movw	r4, #:lower16:GPIO1_REG_BASE	/* request the syscon */
	movt	r4, #:upper16:GPIO1_REG_BASE
	mov.w	r5, #GPIO1_PORT7
	str	r5, [r4, #GPIO_OUTPUT_CLEAR]
	movw	r4, #:lower16:GPIO0_REG_BASE
	movt	r4, #:upper16:GPIO0_REG_BASE
	mov.w	r5, #GPIO0_PORT0
	str	r5, [r4, #GPIO_OUTPUT_CLEAR]
	mov.w	r5, #GPIO0_SYSCON_REQUEST
	str	r5, [r4, #GPIO_OUTPUT_CLEAR]
	dmb	sy
	ldr	r5, [r4, #GPIO_INPUT]
	dsb	sy

	movw	r4, #:lower16:SPI_REG_BASE(0)	/* the packet, a halfword at a time */
	movt	r4, #:upper16:SPI_REG_BASE(0)
7:	ldrb	r5, [r0], #1
	ldrb	r6, [r0], #1
	bfi	r5, r6, #8, #8
	str	r5, [r4, #SPI_TRANSMIT]
	subs.w	r1, r1, #2
	bne	7b
	mov.w	r5, #0
	str	r5, [r4, #SPI_CONTROL]
	mov.w	r5, #1
	str	r5, [r4, #SPI_ENABLE]
	b	8f

8:	movw	r4, #:lower16:GPIO0_REG_BASE
	movt	r4, #:upper16:GPIO0_REG_BASE
	mvn	r5, #0				/* mask and clear every GPIO0 interrupt */
	str	r5, [r4, #GPIO_INTERRUPT_MASK]
	str	r5, [r4, #GPIO_INTERRUPT_MASK + 0x04]
	str	r5, [r4, #GPIO_INTERRUPT_MASK + 0x08]
	str	r5, [r4, #GPIO_INTERRUPT_MASK + 0x0c]
	str	r5, [r4, #GPIO_INTERRUPT_MASK + 0x10]
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS]
	str	r5, [r4, #GPIO_INTERRUPT_STATUS]
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x04]
	str	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x04]
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x08]
	str	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x08]
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x0c]
	str	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x0c]
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x10]
	str	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x10]
9:	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS]	/* until the ready line is clear */
	ldr	r6, [r4, #GPIO_INTERRUPT_STATUS + 0x04]
	orrs	r6, r5
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x08]
	orrs	r6, r5
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x0c]
	orrs	r6, r5
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x10]
	orrs	r6, r5
	ands	r6, r6, #GPIO0_SYSCON_READY
	bne	9b
	mov.w	r5, #GPIO0_SYSCON_REQUEST
	str	r5, [r4, #GPIO_OUTPUT_SET]
	dmb	sy
10:	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS]	/* until the syscon answers */
	ldr	r6, [r4, #GPIO_INTERRUPT_STATUS + 0x04]
	orrs	r6, r5
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x08]
	orrs	r6, r5
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x0c]
	orrs	r6, r5
	ldr	r5, [r4, #GPIO_INTERRUPT_STATUS + 0x10]
	orrs	r6, r5
	ands	r6, r6, #GPIO0_SYSCON_READY
	beq	10b
	mov.w	r5, #GPIO0_SYSCON_REQUEST
	str	r5, [r4, #GPIO_OUTPUT_CLEAR]
	dmb	sy
	ldr	r5, [r4, #GPIO_INPUT]
	dsb	sy
11:	b	11b
	.size	PowerModeEntry, . - PowerModeEntry
