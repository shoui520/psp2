/*
 * Venezia firmware (os0:kd/vnzimg.skprx): the machine.
 *
 * Venezia is eight MeP-c5 cores (ids 48 to 55, `$id >> 16`) with the IVC2
 * coprocessor, little-endian. The image is loaded at physical 0x40800000 and
 * the cores run it at 0x800000. Memory is reached through windows onto the
 * physical address space:
 *
 *	0x18000000-0xc0000000	physical, through the core's own caches
 *	0xc0000000-0xe0000000	"public": physical 0x18000000-0x28000000 and
 *				0x40000000-0x50000000, through the shared L2 cache
 *	0xf0000000-0xf8000000	uncached: physical 0x1e000000-0x24000000 and
 *				0x40000000-0x42000000
 *
 * (henkakuwiki Venezia; the window arithmetic is kernel/address.c.)
 */
#pragma once

#define VNZ_IMAGE		0x800000	/* where the cores run the image */
#define VNZ_IMAGE_PHYSICAL	0x40800000	/* where it is loaded */
#define VNZ_CORES		8
#define VNZ_FIRST_CORE_ID	48

/* the image's addresses in the other windows */
#define VNZ_PHYSICAL_OF(address)	((address) - VNZ_IMAGE + VNZ_IMAGE_PHYSICAL)
#define VNZ_PUBLIC_OF(address)		(VNZ_PHYSICAL_OF(address) + 0x90000000)

/* per-core hardware, through the core's local bus */
#define VNZ_DRAM		0x18000000	/* the start of DRAM's physical window */
#define VNZ_SEMAPHORES		0x4fd000	/* read: take (0 when taken), write 0: give */
#define VNZ_FREE_RUNNING_TIMER	0x4fd080
#define VNZ_DOORBELLS		0x4f1000	/* 0x20 per core: ack, mask, ring */
#define VNZ_DOORBELLS2		0x4f1800
#define VNZ_LOCAL_RAM		0x4e0000	/* 8 KB of code and data, each core's own */
#define VNZ_DATA_RAM		0x330000	/* the data cache's tags */
#define VNZ_L2_CONTROL		0x4f0000
#define VNZ_DMA_FLUSH		0x4b0080

/* control registers */
#define VNZ_CR_PC	0
#define VNZ_CR_LP	1
#define VNZ_CR_SAR	2
#define VNZ_CR_RPB	4
#define VNZ_CR_RPE	5
#define VNZ_CR_RPC	6
#define VNZ_CR_HI	7
#define VNZ_CR_LO	8
#define VNZ_CR_MB0	12
#define VNZ_CR_ME0	13
#define VNZ_CR_MB1	14
#define VNZ_CR_ME1	15
#define VNZ_CR_PSW	16
#define VNZ_CR_ID	17
#define VNZ_CR_TMP	18
#define VNZ_CR_EPC	19
#define VNZ_CR_EXC	20
#define VNZ_CR_CFG	21
#define VNZ_CR_VID	22
#define VNZ_CR_NPC	23
#define VNZ_CR_DBG	24
#define VNZ_CR_DEPC	25
#define VNZ_CR_OPT	26
#define VNZ_CR_RCFG	27
#define VNZ_CR_CCFG	28
#define VNZ_CR_RES30	30
#define VNZ_CR_RES31	31

#define VNZ_PSW_IEC	0x001	/* interrupts enabled (current) */
#define VNZ_PSW_IEP	0x100	/* interrupts enabled (previous) */

#define VNZ_CFG_DCACHE	0x1	/* the data cache is on */

/* the interrupt controller, on the control bus */
#define VNZ_CB_INTERRUPT_STATUS		0x0000	/* bits 3-7: the interrupt taken */
#define VNZ_CB_INTERRUPT_CLEAR		0x0001	/* write ~(1 << interrupt) */
#define VNZ_CB_INTERRUPT_ENABLE		0x0002
#define VNZ_CB_INTERRUPT_EDGE		0x0003
#define VNZ_CB_INTERRUPT_LEVEL0		0x0004	/* four bits per interrupt, eight per register */
#define VNZ_CB_INTERRUPT_LEVEL1		0x0005
#define VNZ_CB_INTERRUPT_LEVEL2		0x0006
#define VNZ_CB_INTERRUPT_LEVEL3		0x0007

#define VNZ_INTERRUPTS			24
#define VNZ_INTERRUPT_DOORBELL		17	/* another core rang this one's doorbell */

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>
#include <intrinsics.h>

/* the control-bus address and the control-register number must be constants */
#define vnzReadControlBus(address) ({				\
	long _value;						\
	mep_ldcb(&_value, (address));				\
	(uint32_t)_value;					\
})

#define vnzWriteControlBus(address, value) mep_stcb((long)(value), (address))

#define vnzReadControl(reg) ({					\
	long _value;						\
	mep_ldc(&_value, (reg));				\
	(uint32_t)_value;					\
})

#define vnzWriteControl(reg, value) mep_stc((long)(value), (reg))

#define vnzRegister(address)	(*(volatile uint32_t *)(address))

/* this core's index, 0 to 7 */
static inline uint32_t vnzCoreIndex(void)
{
	return ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) - VNZ_FIRST_CORE_ID;
}

/* this core's id, 48 to 55 */
static inline uint32_t vnzCoreId(void)
{
	return ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 0xff;
}

/* test and set a byte: returns what it was */
static inline uint32_t vnzTestAndSet(volatile uint8_t *lock)
{
	long value;

	mep_tas(&value, (long *)lock);
	return value;
}

/* a data-cache operation on the line holding address (MeP CACHE) */
#define VNZ_CACHE_FLUSH			0	/* before the cores share a line */
#define VNZ_CACHE_WRITEBACK		2
#define VNZ_CACHE_INVALIDATE		5
#define VNZ_CACHE_PURGE			6	/* write back and invalidate */
#define vnzCacheLine(operation, address) mep_cache((operation), (long *)(address))
#define vnzPrefetch(address)		mep_pref(0, (long *)(address))

static inline void vnzDisableInterrupts(void)
{
	mep_di();
}

static inline void vnzEnableInterrupts(void)
{
	mep_ei();
}

/* the C library's pieces (lib/string.c) */
void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);
int abs(int value);

#endif /* !__ASSEMBLER__ */
