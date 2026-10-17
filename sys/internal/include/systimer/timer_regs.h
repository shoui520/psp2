/*
 * The SoC timers, one 0x1000-byte page each: the GT at 0xe20b0000, six
 * long-range timers (SceLT0-5, 64-bit count) from 0xe20b1000 and eight word
 * timers (SceWT0-7, 32-bit count) from 0xe20b7000. SceSystimer owns LT0-4 and
 * WT0-6; ThreadMgr runs its CPU timer on LT5 (1 MHz), which Sysmem also maps
 * read-only into every process; the TZS IntrMgr's usleep uses WT7. Names after
 * SceSystimer's use and the henkaku wiki's Hardware Timers page.
 */
#pragma once

#include <stddef.h>
#include <psp2common/types.h>

#define TIMER_LONG_REG_BASE(n)		(0xe20b1000 + (n) * 0x1000)
#define TIMER_WORD_REG_BASE(n)		(0xe20b7000 + (n) * 0x1000)
/* TMBERR (0.920's name): the timer block's bus error words */
#define TIMER_BUS_ERROR_REG_BASE	0xe20bf000
#define TIMER_REG_SIZE			0x1000

/* control: bit 0 runs the count, bits 23:20 the input clock, 31:24 the prescaler */
#define TIMER_CONTROL_ENABLE		0x01
/* intr: write 3 to clear a pending interrupt */
#define TIMER_INTR_CLEAR		3

typedef struct SceLongTimerRegisters {
	union {
		volatile SceUInt64 count;
		struct {
			volatile SceUInt32 countLo;
			volatile SceUInt32 countHi;
		};
	};
	union {					/* interrupts when count reaches it */
		volatile SceUInt64 compare;
		struct {
			volatile SceUInt32 compareLo;
			volatile SceUInt32 compareHi;
		};
	};
	union {
		volatile SceUInt64 value;
		struct {
			volatile SceUInt32 valueLo;
			volatile SceUInt32 valueHi;
		};
	};
	volatile SceUInt32 intr;
	volatile SceUInt32 control;
} SceLongTimerRegisters;

typedef struct SceWordTimerRegisters {
	volatile SceUInt32 compare;		/* interrupts when count reaches it */
	volatile SceUInt32 count;
	volatile SceUInt32 control;
	volatile SceUInt32 value;
	volatile SceUInt32 reserved;
	volatile SceUInt32 intr;
} SceWordTimerRegisters;

#ifndef __cplusplus
_Static_assert(offsetof(SceLongTimerRegisters, compare) == 0x8, "LT compare");
_Static_assert(offsetof(SceLongTimerRegisters, intr) == 0x18, "LT interrupt");
_Static_assert(offsetof(SceLongTimerRegisters, control) == 0x1c, "LT control");
_Static_assert(offsetof(SceWordTimerRegisters, control) == 0x8, "WT control");
_Static_assert(offsetof(SceWordTimerRegisters, intr) == 0x14, "WT interrupt");
#endif

typedef struct SceTimerBusErrorRegisters {
	volatile uint32_t busErrorAddress;
	volatile uint32_t busErrorAttribute;
	volatile uint32_t secureBusErrorAddress;
	volatile uint32_t secureBusErrorAttribute;
} SceTimerBusErrorRegisters;
