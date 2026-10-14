/*
 * Marvell 88W8787 ("Robin") SoC registers used by the firmware.
 *
 * No Marvell documentation or source names these registers. The names below
 * describe what the firmware does with each one (see
 * agent/namecheck/os0/kd/wlanbt_robin_img_ax.tsv).
 */
#ifndef ROBIN_SOC_H
#define ROBIN_SOC_H

#include <stdint.h>

#define REG32(addr)	(*(volatile uint32_t *)(addr))
#define REG16(addr)	(*(volatile uint16_t *)(addr))
#define REG8(addr)	(*(volatile uint8_t *)(addr))

/* Event sources cascaded through interrupt line 22. */
#define SOC_EVENT_STATUS		REG32(0x8000208c)
#define SOC_EVENT_CONTROL	REG32(0x8000200c)
#define SOC_EVENT_STATE		REG8(0x80000002)
#define SOC_EVENT_LATCH		REG8(0x80000092)
#define SOC_EVENT_SOURCE_ENABLE	REG32(0x80002080)
#define SOC_EVENT_SECONDARY_ENABLE REG32(0x80002088)
#define SOC_EVENT_ENGINE_GATE	REG32(0x80002090)

/* The original firmware stores this hardware base in a single data word. */
extern volatile void *sysEventRegisterBase;
enum {
	SOC_EVENT_CONFIGURATION_STATUS_OFFSET = 0,
	SOC_EVENT_GLOBAL_CONTROL_OFFSET = 4,
	SOC_EVENT_CONFIGURATION_OFFSET = 8,
	SOC_EVENT_TIMING_CONTROL_OFFSET = 0xc,
	SOC_EVENT_COUNTDOWN_CONTROL_OFFSET = 0x10,
	SOC_EVENT_CLOCK_PERIOD_OFFSET = 0x14,
	SOC_EVENT_PRIMARY_TIMING_OFFSET = 0x18,
	SOC_EVENT_FLAGS_OFFSET = 0x1c,
	SOC_EVENT_SECONDARY_FLAGS_OFFSET = 0x20,
	SOC_EVENT_MEASUREMENT_SAMPLE_OFFSET = 0x24,
	SOC_EVENT_MEASUREMENT_UNITS_OFFSET = 0x2c,
	SOC_EVENT_MEASUREMENT_REQUEST_OFFSET = 0x30,
	SOC_EVENT_CLOCK_OFFSET = 0x34,
	SOC_EVENT_HANDSHAKE_STATUS_OFFSET = 0x38,
	SOC_EVENT_SECONDARY_TIMING_OFFSET = 0x3c,
	SOC_EVENT_ENGINE_CONTROL_OFFSET = 0x80,
	SOC_EVENT_ENGINE_CONFIGURATION_OFFSET = 0x84,
	SOC_EVENT_ENGINE_COUNTDOWN_OFFSET = 0x8c,
	SOC_EVENT_ENGINE_COUNTDOWN_RELOAD_OFFSET = 0x90,
	SOC_EVENT_ENGINE_COUNTDOWN_LIMIT_OFFSET = 0x94,
	SOC_EVENT_ENGINE_COUNTDOWN_SETUP_OFFSET = 0x98,
	SOC_EVENT_ENGINE_PRIMARY_TIMING_OFFSET = 0xa0,
	SOC_EVENT_ENGINE_SECONDARY_TIMING_OFFSET = 0xa4,
	SOC_EVENT_SECONDARY_CONTROL_OFFSET = 0xc0,
	SOC_EVENT_SECONDARY_CONFIGURATION_OFFSET = 0xc4,
	SOC_EVENT_TERTIARY_CONTROL_OFFSET = 0x100,
	SOC_EVENT_TERTIARY_CONFIGURATION_OFFSET = 0x104,
};
#define SOC_EVENT_CONFIGURATION_STATUS \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_CONFIGURATION_STATUS_OFFSET)
#define SOC_EVENT_GLOBAL_CONTROL \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_GLOBAL_CONTROL_OFFSET)
#define SOC_EVENT_CONFIGURATION \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_CONFIGURATION_OFFSET)
#define SOC_EVENT_TIMING_CONTROL \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_TIMING_CONTROL_OFFSET)
#define SOC_EVENT_COUNTDOWN_CONTROL \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_COUNTDOWN_CONTROL_OFFSET)
#define SOC_EVENT_CLOCK_PERIOD \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_CLOCK_PERIOD_OFFSET)
#define SOC_EVENT_PRIMARY_TIMING \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_PRIMARY_TIMING_OFFSET)
#define SOC_EVENT_FLAGS \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_FLAGS_OFFSET)
#define SOC_EVENT_SECONDARY_FLAGS \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_SECONDARY_FLAGS_OFFSET)
#define SOC_EVENT_MEASUREMENT_SAMPLE \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_MEASUREMENT_SAMPLE_OFFSET)
#define SOC_EVENT_MEASUREMENT_UNITS \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_MEASUREMENT_UNITS_OFFSET)
#define SOC_EVENT_MEASUREMENT_REQUEST \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_MEASUREMENT_REQUEST_OFFSET)
#define SOC_EVENT_CLOCK \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_CLOCK_OFFSET)
#define SOC_EVENT_HANDSHAKE_STATUS \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_HANDSHAKE_STATUS_OFFSET)
#define SOC_EVENT_SECONDARY_TIMING \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_SECONDARY_TIMING_OFFSET)
#define SOC_EVENT_ENGINE_CONTROL \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_CONTROL_OFFSET)
#define SOC_EVENT_ENGINE_CONFIGURATION \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_CONFIGURATION_OFFSET)
#define SOC_EVENT_ENGINE_COUNTDOWN \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_COUNTDOWN_OFFSET)
#define SOC_EVENT_ENGINE_COUNTDOWN_RELOAD \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_COUNTDOWN_RELOAD_OFFSET)
#define SOC_EVENT_ENGINE_COUNTDOWN_LIMIT \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_COUNTDOWN_LIMIT_OFFSET)
#define SOC_EVENT_ENGINE_COUNTDOWN_SETUP \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_COUNTDOWN_SETUP_OFFSET)
#define SOC_EVENT_ENGINE_PRIMARY_TIMING \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_PRIMARY_TIMING_OFFSET)
#define SOC_EVENT_ENGINE_SECONDARY_TIMING \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_ENGINE_SECONDARY_TIMING_OFFSET)
#define SOC_EVENT_SECONDARY_CONTROL \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_SECONDARY_CONTROL_OFFSET)
#define SOC_EVENT_SECONDARY_CONFIGURATION \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_SECONDARY_CONFIGURATION_OFFSET)
#define SOC_EVENT_TERTIARY_CONTROL \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_TERTIARY_CONTROL_OFFSET)
#define SOC_EVENT_TERTIARY_CONFIGURATION \
	REG32((uintptr_t)sysEventRegisterBase + SOC_EVENT_TERTIARY_CONFIGURATION_OFFSET)

/* Sources cascaded through interrupt line 30; inverted acknowledgement. */
#define SOC_SOURCE_IRQ_ENABLE	REG32(0x80002190)
#define SOC_SOURCE_IRQ_SECONDARY_ENABLE REG32(0x80002198)
#define SOC_PENDING_EVENT_STATUS REG32(0x8000a7ac)
#define SOC_MAC_EVENT_CONTROL	REG32(0x8000a700)
#define SOC_MAC_REGISTER_BASE	0x8000a000u
/* MAC register file; the offsets are the hardware's, no names are known. */
#define MAC_REG(offset)		REG32(SOC_MAC_REGISTER_BASE + (offset))
#define SOC_MAC_EVENT_PATH_CONTROL REG32(0x8000a7e0)
#define SOC_MAC_EVENT_PRIMARY_PATTERN REG32(0x8000a78c)
#define SOC_MAC_EVENT_CONTROL_PATTERN REG32(0x8000a7e4)
#define SOC_MAC_EVENT_PRIMARY_CONFIGURATION REG32(0x8000a7e8)
#define SOC_MAC_EVENT_SECONDARY_CONFIGURATION REG32(0x8000a7ec)
#define SOC_MAC_EVENT_SECONDARY_PATTERN REG32(0x8000a790)
#define SOC_MAC_EVENT_PRIMARY_MASK REG32(0x8000a79c)
#define SOC_MAC_EVENT_SECONDARY_MASK REG32(0x8000a7a0)
#define SOC_EVENT_PATH_CONTROL	REG32(0x80002134)
#define SOC_SOURCE_IRQ_STATUS	REG32(0x8000219c)
#define SOC_PENDING_EVENT_CONTROL REG32(0x8000a70c)

/*
 * Interrupt controller (0x80002800). One bit per line, 32 lines.
 * Handlers mask their line on entry and unmask it on exit.
 */
#define SOC_IRQ_STATUS		REG32(0x80002800)	/* pending lines */
#define SOC_IRQ_ENABLE_SET	REG32(0x80002808)	/* write 1: unmask; read: unmasked lines */
#define SOC_IRQ_ENABLE_CLEAR	REG32(0x8000280c)	/* write 1: mask */
#define SOC_IRQ_LINE1_ACK	REG32(0x80002810)	/* written 0 before line 1 is serviced */
#define SOC_IRQ_RAW_STATUS	REG32(0x80002900)	/* raw sources; bit 23 = fatal line */

#define SOC_IRQ_LINE_COUNT	32

#define SOC_CHIP_REVISION	REG32(0x80002018)	/* low byte: silicon revision */
/* Baseband version words, one per byte of the version string. */
#define BBP_VERSION_REGISTERS	((volatile uint32_t *)0x90002000)
/* Baseband register file: one byte-wide register per word. */
#define BBP_REGISTER(n)		REG32(0x90002000u + 4u * (n))

/*
 * Serial register bus to the radio (0x90000800). An access writes the
 * register number to the address register and then moves one byte through
 * the data register; the status register tells when the bus is busy.
 */
#define RFBUS_CONTROL		REG16(0x90000800)
#define RFBUS_CONFIGURATION	REG16(0x90000804)
#define RFBUS_STATUS		REG16(0x9000081c)
#define RFBUS_DATA		REG8(0x90000824)
#define RFBUS_ADDRESS		REG16(0x90000828)
#define RFBUS_STATUS_BUSY	0x1c7
#define SOC_IRQ_LINE_FATAL	23

/*
 * Indirect register window (0x80002074/0x80002078): write an index, then
 * read or write the data register. Bit 31 of the index selects a write.
 */
#define SOC_INDIRECT_INDEX	REG32(0x80002074)
#define SOC_INDIRECT_DATA	REG32(0x80002078)
#define SOC_INDIRECT_WRITE	0x80000000u

/* General-purpose timer block (0x80003000), four timers. */
#define SOC_TIMER1_RELOAD	REG32(0x80003004)
#define SOC_TIMER2_RELOAD	REG32(0x80003008)
#define SOC_TIMER3_RELOAD	REG32(0x8000300c)
#define SOC_TIMER_CONTROL	REG32(0x80003010)
#define SOC_TIMER1_COUNTER	REG32(0x80003018)
#define SOC_TIMER2_COUNTER	REG32(0x8000301c)
#define SOC_TIMER3_COUNTER	REG32(0x80003020)
#define SOC_TIMER_INT_STATUS	REG32(0x80003024)	/* bit n = timer n expired; written ~bit to clear */
#define SOC_TIMER_INT_ENABLE	REG32(0x80003028)	/* bit n = timer n interrupt enabled */

/* Baseband processor event latches serviced on lines 15 and 16. */
#define BBP_EVENT_A		REG32(0x8000e840)
#define BBP_EVENT_B		REG32(0x8000e844)
#define BBP_SECONDARY_COMMAND	REG32(0x8000e804)
/*
 * Copy engines A and B (0x8000e800 and 0x8000e804 ...): length, source,
 * destination and configuration words, and the control word they share with
 * the baseband event latches above (bit 14: busy).
 */
#define DMA_A_LENGTH		REG32(0x8000e800)
#define DMA_B_LENGTH		REG32(0x8000e804)
#define DMA_A_SOURCE		REG32(0x8000e810)
#define DMA_B_SOURCE		REG32(0x8000e814)
#define DMA_A_DESTINATION	REG32(0x8000e820)
#define DMA_B_DESTINATION	REG32(0x8000e824)
#define DMA_A_CONFIG		REG32(0x8000e830)
#define DMA_B_CONFIG		REG32(0x8000e834)
#define DMA_A_CONTROL		BBP_EVENT_A
#define DMA_B_CONTROL		BBP_EVENT_B
#define DMA_CONTROL_BUSY	0x4000
#define DMA_CONTROL_RESET	0x100000

#define BBP_EVENT_A_STATUS	REG32(0x8000e8a0)
#define BBP_EVENT_B_STATUS	REG32(0x8000e8a4)

/* Mask ROM patch unit (0x80003800): four word comparators. */
#define ROM_PATCH_ADDR(n)	REG32(0x80003800 + 4 * (n))	/* ROM address | enable */
#define ROM_PATCH_DATA(n)	REG32(0x80003880 + 4 * (n))	/* replacement word */
#define ROM_PATCH_HIT		REG32(0x800038b0)	/* bits 4:0 = slot of the trapping patch */
#define ROM_PATCH_ENABLE	REG32(0x800038b8)
#define ROM_PATCH_ARM_INSN	REG32(0x800038bc)	/* opcode used for ARM traps */
#define ROM_PATCH_THUMB_INSN	REG32(0x800038c0)	/* opcode used for Thumb traps */

#endif /* ROBIN_SOC_H */
