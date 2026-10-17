/*
 * VIP firmware: definitions shared by every program in os0:kd/vipimg.skprx.
 *
 * The VIP ("Venezia Image Processing", the H.264 decoder) runs big-endian Toshiba MeP
 * cores, PU0 and PU1. Each core has its own local RAM at 0x200000 and sees the
 * firmware image in DRAM at 0x800000 (its first 0x3000 bytes, the boot block, also
 * at 0). The ARM side (os0:kd/bootimage/vnz_wrapper) talks to the cores through the
 * pervasive mailbox and through shared structures at fixed places in the image.
 *
 * Built with GCC 4.5 for MeP (-meb); the control bus, control registers and
 * interrupt masking are reached through the compiler's MeP intrinsics.
 */
#pragma once

#define VIP_LOCAL_RAM		0x200000	/* each core's own */
#define VIP_IMAGE		0x800000	/* the firmware image in DRAM */
#define VIP_MAILBOX_BASE	0xe3106000	/* the pervasive mailbox (lowio/mailbox_regs.h) */

/* control registers (ldc/stc), by MeP register number */
#define VIP_CR_LP	1
#define VIP_CR_SAR	2
#define VIP_CR_RPB	4
#define VIP_CR_RPE	5
#define VIP_CR_RPC	6
#define VIP_CR_HI	7
#define VIP_CR_LO	8
#define VIP_CR_PSW	16
#define VIP_CR_ID	17
#define VIP_CR_EPC	19
#define VIP_CR_EXC	20
#define VIP_CR_CFG	21
#define VIP_CR_NPC	23
#define VIP_CR_OPT	26
#define VIP_CR_RCFG	27
#define VIP_CR_CCFG	28

/* processor status word bits */
#define VIP_PSW_IEC	0x001	/* interrupts enabled (current) */
#define VIP_PSW_UMC	0x002
#define VIP_PSW_IEP	0x100	/* interrupts enabled (previous) */

/* control bus registers (ldcb/stcb): the interrupt controller */
#define VIP_CB_INTERRUPT_STATUS	0x0000
#define VIP_CB_INTERRUPT_CLEAR	0x0001	/* write ~(1 << interrupt) */
#define VIP_CB_INTERRUPT_ENABLE	0x0002
#define VIP_CB_INTERRUPT_PENDING	0x0003
#define VIP_CB_INTERRUPT_LEVEL0	0x0004
#define VIP_CB_INTERRUPT_LEVEL1	0x0005
#define VIP_CB_INTERRUPT_LEVEL2	0x0006
#define VIP_CB_INTERRUPT_LEVEL3	0x0007

#define VIP_CB_CACHE_CONTROL	0x0800

/* DMA between DRAM and local RAM */
#define VIP_CB_DMA_BUSY	0x1000	/* write 1: start */
#define VIP_CB_DMA_CONTROL	0x1001
#define VIP_CB_DMA_DIRECTION	0x1002
#define VIP_CB_DMA_EXTERNAL	0x1003	/* DRAM address */
#define VIP_CB_DMA_LOCAL_LENGTH	0x1004	/* local address << 16 | length */
#define VIP_CB_DMA_PARAMETER0	0x1005
#define VIP_CB_DMA_PARAMETER1	0x1006

/* VIP_CB_DMA_DIRECTION */
#define VIP_DMA_WRITE		0x000	/* local RAM to DRAM */
#define VIP_DMA_READ		0x001	/* DRAM to local RAM */
#define VIP_DMA_BANK1		0x002	/* the local address is in bank 1 */
#define VIP_DMA_EXTERNAL_LOCAL	0x004	/* the external address is in local RAM bank 1 */
#define VIP_DMA_FILL		0x200	/* repeat the source (VIP_CB_DMA_PARAMETER0 bytes; inferred) */
#define VIP_DMA_LIST		0x800	/* the local address is a list of VipDmaDescriptor */

/* local addresses with this bit are in local RAM bank 1 */
#define VIP_LOCAL_BANK1		0x400000

/* interrupt numbers: bits of the VIP_CB_INTERRUPT_* registers */
#define VIP_INTERRUPT_DMA		0	/* a DMA request completed */
#define VIP_INTERRUPT_MAILBOX		1	/* the ARM side wrote the mailbox */
#define VIP_INTERRUPT_DECODER		5
#define VIP_INTERRUPT_READ_STREAM0	6	/* the read stream wants buffer half 0 */
#define VIP_INTERRUPT_READ_STREAM1	7
#define VIP_INTERRUPT_8			8
#define VIP_INTERRUPT_WRITE_STREAM0	9	/* the write stream has filled half 0 */
#define VIP_INTERRUPT_WRITE_STREAM1	10
#define VIP_INTERRUPT_11		11

/* interval timer: counts down from VIP_CB_TIMER_COUNT */
#define VIP_CB_TIMER_ENABLE	0x4000
#define VIP_CB_TIMER_MODE	0x4001
#define VIP_CB_TIMER_CONTROL	0x4002
#define VIP_CB_TIMER_RELOAD	0x4003
#define VIP_CB_TIMER_COUNT	0x4004
#define VIP_CB_TIMER_INTERRUPT	0x4005	/* 1: interrupt at VIP_CB_TIMER_COMPARE */
#define VIP_CB_TIMER_COMPARE	0x4007

/* the stream unit; the reset also stops VIP_CB_SPARE_CONTROL, which nothing else uses */
#define VIP_CB_STREAM_CONTROL	0x6000	/* 0: stopped; PU0 runs it with 2 */
#define VIP_CB_STREAM_SETUP	0x6010	/* the first of its settings */
#define VIP_CB_SPARE_CONTROL	0x8000

/*
 * the units' enables: writing 0 then 1 resets a unit. The bitstream unit is
 * unit 0 of the codec's picture units.
 */
#define VIP_CB_PREDICTION_ENABLE	0x5000
#define VIP_CB_DIVIDER_ENABLE		0x6200
#define VIP_CB_UNIT_ENABLE(unit)	(0x7000 + 0x100 * (unit))
#define VIP_CB_BITSTREAM_ENABLE		VIP_CB_UNIT_ENABLE(0)
#define VIP_CB_SCANNER_CONTROL		0x9000	/* the start code scanner */

/* clocks of the units */
#define VIP_CB_CLOCK_ENABLE	0x6800
#define VIP_CB_CLOCK_REQUEST	0x6801

#define VIP_CLOCK_CORE		0x1	/* off while the debugger holds the core */
#define VIP_CLOCK_SHARED	0x2	/* the bitstream unit and the scanner */
#define VIP_CLOCK_DEMUX		0x4	/* the scanner */

/* VIP_CB_CLOCK_REQUEST */
#define VIP_CLOCK_REQUEST_DECODER	0x1	/* the decoder needs VIP_CLOCK_SHARED */

/* the stream buffers: write 1 when a half has been filled or emptied by DMA */
#define VIP_CB_READ_STREAM_READY0	0x701c
#define VIP_CB_READ_STREAM_READY1	0x701d
#define VIP_CB_WRITE_STREAM_READY0	0x701e
#define VIP_CB_WRITE_STREAM_READY1	0x701f

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>
#include <intrinsics.h>

/* GCC 4.5 has no _Static_assert; the shared sce/ headers use it. */
#ifndef __cplusplus
#if __GNUC__ == 4 && __GNUC_MINOR__ < 6
#define VIP_CAT2(a, b) a##b
#define VIP_CAT(a, b) VIP_CAT2(a, b)
#define _Static_assert(condition, message) \
	typedef char VIP_CAT(vipStaticAssert, __LINE__)[(condition) ? 1 : -1]
#endif
#endif

#include <lowio/mailbox_regs.h>

#define VIP_MAILBOX ((ScePervasiveMailboxRegisters *)VIP_MAILBOX_BASE)

/* the control-bus address and the control-register number must be constants */
#define vipReadControlBus(address) ({				\
	long _value;						\
	mep_ldcb(&_value, (address));				\
	(uint32_t)_value;					\
})

#define vipWriteControlBus(address, value) mep_stcb((long)(value), (address))

#define vipReadControl(reg) ({					\
	long _value;						\
	mep_ldc(&_value, (reg));				\
	(uint32_t)_value;					\
})

#define vipWriteControl(reg, value) mep_stc((long)(value), (reg))

#define vipAcknowledgeInterrupt(interrupt) \
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, ~(1u << (interrupt)))

/* the C library's pieces the codecs build in (fw1/pu1/memory.c) */
void *memcpy(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);

#endif /* !__ASSEMBLER__ */
