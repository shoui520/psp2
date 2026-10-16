#pragma once

/*
 * SceEmcTop: the external memory controller of the CDRAM (henkaku wiki,
 * Physical Memory). Only the secure kernel's SceDriverTzs touches it; the
 * Non-secure SceLowio asks for it through monitor calls 0x117-0x119. Names
 * are descriptive: what SceDriverTzs writes and waits for.
 */
#define EMC_TOP_BASE		0xe8200000
#define EMC_TOP_SIZE		0x1000

/* command: a command runs while START is set; DATA holds its argument */
#define EMC_COMMAND_START		0x01
#define EMC_COMMAND_LAST		0x02	/* set with the last initialization command */
#define EMC_COMMAND_SELF_REFRESH	0x10	/* request self refresh */
#define EMC_COMMAND_IN_SELF_REFRESH	0x20	/* the CDRAM is in self refresh */
#define EMC_COMMAND_KEEP		0x30	/* bits a new command keeps */

#define EMC_INTR_EVENT			0x01

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

typedef struct SceEmcRegisters {
	volatile uint32_t control;		/* 0x000 */
	volatile uint32_t timing[7];		/* 0x004 */
	uint32_t reserved0;
	volatile uint32_t command;		/* 0x024 EMC_COMMAND_* */
	volatile uint32_t commandData;		/* 0x028 */
	volatile uint32_t refresh;		/* 0x02c */
	uint32_t reserved1[2];
	/* 0x038: set at enable, switched by the controller's interrupt */
	volatile uint32_t eventTiming[2];
	uint32_t reserved2[124];
	volatile uint32_t intrEnable;		/* 0x230 */
	volatile uint32_t intrMode;		/* 0x234 */
	uint32_t reserved3[2];
	volatile uint32_t intrStatus;		/* 0x240 EMC_INTR_* */
	volatile uint32_t intrClear;		/* 0x244 */
	uint32_t reserved4[878];
} SceEmcRegisters;

_Static_assert(offsetof(SceEmcRegisters, command) == 0x24, "EMC command");
_Static_assert(offsetof(SceEmcRegisters, refresh) == 0x2c, "EMC refresh");
_Static_assert(offsetof(SceEmcRegisters, eventTiming) == 0x38, "EMC event timing");
_Static_assert(offsetof(SceEmcRegisters, intrEnable) == 0x230, "EMC interrupt enable");
_Static_assert(offsetof(SceEmcRegisters, intrClear) == 0x244, "EMC interrupt clear");
_Static_assert(sizeof(SceEmcRegisters) == EMC_TOP_SIZE, "EMC register page");

#endif /* __ASSEMBLER__ */
