/*
 * The LPDDR2 interface controllers: CH0 (LPDDR2MAIN, DDRIF0) and CH1
 * (LPDDR2SUB, DDRIF1, the development kits' second DRAM bank). The secure
 * boot loader programs them in boot(); SKBL 0.990 names the CH1 sequence
 * EnableLpddr2Sub. The register blocks are named after the henkaku wiki's
 * Physical Memory page; no source names the registers inside them, so the
 * names below are descriptive and the controller's parameter words stay
 * an indexed table.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define LPDDR2_MAIN_REG_BASE		0xe6000000
#define LPDDR2_SUB_REG_BASE		0xe5880000

/* control[]: the controller's 64-bit-spaced words, low half used */
#define LPDDR2_CONTROL_COUNT		0x200
/* the initialisation command, once per rank; the status reports it done */
#define LPDDR2_CONTROL_COMMAND(rank)	(rank)
#define LPDDR2_CONTROL_STATUS		4

/* commands and the status each one ends in */
#define LPDDR2_COMMAND_RESET		5
#define LPDDR2_STATUS_RESET_DONE	0x44
#define LPDDR2_COMMAND_INIT		1
#define LPDDR2_STATUS_INIT_DONE		0x11

/* the control words that hold the secure bus error log */
#define LPDDR2_CONTROL_SECURE_BUS_ERROR_ATTRIBUTE	0x74
#define LPDDR2_CONTROL_SECURE_BUS_ERROR_ADDRESS		0x75

typedef struct SceLpddr2ControlWord {
	volatile uint32_t value;
	uint32_t reserved;
} SceLpddr2ControlWord;

typedef struct SceLpddr2Registers {
	volatile uint32_t config;			/* 0x0000 */
	uint32_t reserved0[0x23];
	/* CH0: written once by SKBL after the timers are out of reset */
	volatile uint32_t ioConfig[2];			/* 0x0090 */
	uint32_t reserved1[0x1fda];
	SceLpddr2ControlWord control[LPDDR2_CONTROL_COUNT];	/* 0x8000 */
	uint32_t reserved2[0xc00];
	volatile uint32_t busErrorAttribute;		/* 0xc000 */
	uint32_t reserved3;
	volatile uint32_t busErrorAddress;		/* 0xc008 */
	uint32_t reserved4[0xffd];
} SceLpddr2Registers;

_Static_assert(offsetof(SceLpddr2Registers, ioConfig) == 0x90, "LPDDR2 I/O config");
_Static_assert(offsetof(SceLpddr2Registers, control) == 0x8000, "LPDDR2 control words");
_Static_assert(offsetof(SceLpddr2Registers, busErrorAddress) == 0xc008, "LPDDR2 bus error");
_Static_assert(sizeof(SceLpddr2Registers) == 0x10000, "LPDDR2 register block");
