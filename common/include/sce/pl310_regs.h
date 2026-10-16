/*
 * PL310 (L2C-310) L2 cache controller registers: SceSysmem's L2 maintenance,
 * ScePamgr's event counters ("ScePl310Reg") and the control words the TZS
 * IntrMgr sets for the kernel ("SceL2Cache"); SKBL sets the controller up. Names follow the
 * ARM TRM.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* in the Cortex-A9 MPCore private region */
#define PL310_REG_BASE			0x1a002000

/* ScePl310Registers.auxControl */
#define PL310_AUX_EVENT_MONITOR_BUS	0x00100000

typedef struct ScePl310Registers {
	volatile uint32_t cacheId;		/* 0x000: bits 0-5 the RTL release */
	uint8_t reserved0[0x100 - 0x004];
	volatile uint32_t control;		/* 0x100: bit 0 enables the cache */
	volatile uint32_t auxControl;		/* 0x104 PL310_AUX_* */
	volatile uint32_t tagRamControl;	/* 0x108: tag RAM latencies */
	volatile uint32_t dataRamControl;	/* 0x10c: data RAM latencies */
	uint8_t reserved1a[0x200 - 0x110];
	volatile uint32_t eventCounterControl;	/* 0x200 */
	volatile uint32_t eventCounter1Config;
	volatile uint32_t eventCounter0Config;
	volatile uint32_t eventCounter1Value;
	volatile uint32_t eventCounter0Value;
	uint8_t reserved1[0x730 - 0x214];
	volatile uint32_t cacheSync;		/* 0x730 */
	uint8_t reserved2[0x770 - 0x734];
	volatile uint32_t invalidatePA;		/* 0x770: invalidate line by PA */
	uint8_t reserved3a[0x77c - 0x774];
	volatile uint32_t invalidateWay;	/* 0x77c: one bit per way, busy until clear */
	uint8_t reserved3[0x7b0 - 0x780];
	volatile uint32_t cleanPA;		/* 0x7b0: clean line by PA */
	uint8_t reserved4[0x7f0 - 0x7b4];
	volatile uint32_t cleanInvalidatePA;	/* 0x7f0: clean and invalidate line by PA */
	uint8_t reserved5[0x7fc - 0x7f4];
	volatile uint32_t cleanInvalidateWay;	/* 0x7fc: one bit per way, busy until clear */
	uint8_t reserved6[0xf60 - 0x800];
	volatile uint32_t prefetchControl;	/* 0xf60, r3p0 and later */
	uint8_t reserved7[0xf80 - 0xf64];
	volatile uint32_t powerControl;		/* 0xf80 */
} ScePl310Registers;

_Static_assert(offsetof(ScePl310Registers, auxControl) == 0x104, "PL310 auxiliary control");
_Static_assert(offsetof(ScePl310Registers, eventCounter0Value) == 0x210, "PL310 event counters");
_Static_assert(offsetof(ScePl310Registers, cacheSync) == 0x730, "PL310 cache sync");
_Static_assert(offsetof(ScePl310Registers, cleanInvalidatePA) == 0x7f0, "PL310 clean and invalidate");
_Static_assert(offsetof(ScePl310Registers, dataRamControl) == 0x10c, "PL310 RAM latencies");
_Static_assert(offsetof(ScePl310Registers, invalidateWay) == 0x77c, "PL310 invalidate by way");
_Static_assert(offsetof(ScePl310Registers, cleanInvalidateWay) == 0x7fc, "PL310 clean and invalidate by way");
_Static_assert(offsetof(ScePl310Registers, powerControl) == 0xf80, "PL310 power control");
