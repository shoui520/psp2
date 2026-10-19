#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * GRAB and SonyRegbus: the bridges the compat (Allegrex) and GPU masters
 * reach the system through. Each has a secure and a Non-secure page; the
 * secure kernel's SceDriverTzs maps all four, SceBusError reads their error
 * logs. Names are descriptive except where the henkaku wiki gives them.
 */
#define SONY_REGBUS_BASE	0xe8000000
#define SONY_REGBUS_NS_BASE	0xe8001000
#define GRAB_BASE		0xe8300000
#define GRAB_NS_BASE		0xe8301000
#define GRAB_PAGE_SIZE		0x1000

/* the masters a GRAB port mask selects */
#define GRAB_PORT_COMPAT	0x0c0	/* the Allegrex's two ports */
#define GRAB_PORT_GPU		0xf00	/* the GPU's four ports */
#define GRAB_PORT_ALL		0xfff

#define GRAB_CMP_MAP_COUNT	8	/* banks of the compat address space */
#define GRAB_PORT_CONFIG_COUNT	6	/* ports 6-11: compat 0-1, GPU 0-3 */

typedef struct SceGrabRegisters {
	uint32_t reserved0[2];
	volatile uint32_t errorStatus;		/* 0x008 bit 1 read, bit 2 write error */
	uint32_t reserved1;
	volatile uint32_t writeError[4];	/* 0x010 ID, address, attribute, info */
	volatile uint32_t readError[4];		/* 0x020 */
	uint32_t reserved2[60];
	/* 0x120 secure page: the physical address each compat bank maps to */
	volatile uint32_t cmpMap[GRAB_CMP_MAP_COUNT];
	uint32_t reserved3[48];
	/* 0x200: a set bit stops that port; the busy words show its
	 * transfers still in flight */
	volatile uint32_t readStop;
	volatile uint32_t writeStop;
	volatile uint32_t readBusy;
	volatile uint32_t writeBusy;
	/* 0x210 Non-secure page: set to 1 by SKBL in development mode (dipsw 159) */
	volatile uint32_t developmentMode;
	uint32_t reserved4[251];
	volatile uint32_t portConfig[GRAB_PORT_CONFIG_COUNT][64];	/* 0x600 */
	uint32_t reserved5[256];
} SceGrabRegisters;

_Static_assert(offsetof(SceGrabRegisters, errorStatus) == 0x8, "GRAB error status");
_Static_assert(offsetof(SceGrabRegisters, readError) == 0x20, "GRAB read error log");
_Static_assert(offsetof(SceGrabRegisters, cmpMap) == 0x120, "GRAB compat map");
_Static_assert(offsetof(SceGrabRegisters, readStop) == 0x200, "GRAB port stop");
_Static_assert(offsetof(SceGrabRegisters, writeBusy) == 0x20c, "GRAB port busy");
_Static_assert(offsetof(SceGrabRegisters, developmentMode) == 0x210, "GRAB development mode");
_Static_assert(offsetof(SceGrabRegisters, portConfig) == 0x600, "GRAB port config");
_Static_assert(sizeof(SceGrabRegisters) == GRAB_PAGE_SIZE, "GRAB register page");

typedef struct SceSonyRegbusRegisters {
	/* the access mode of each part, set by monitor call 0x107: two
	 * 2-bit fields per word, both written with the same mode */
	volatile uint32_t accessMode[5];
	uint32_t reserved0[59];
	volatile uint32_t errorControl;		/* 0x100 */
	uint32_t reserved1;
	volatile uint32_t errorStatus;		/* 0x108 bit 0: a bus error occurred */
	uint32_t reserved2;
	volatile uint32_t errorId;		/* 0x110 REGBUS_(SEC)ERRID */
	volatile uint32_t errorAddr;
	volatile uint32_t errorAttr;
	volatile uint32_t errorInfo;
	volatile uint32_t status;		/* 0x120 Non-secure page: bits 0-1 */
	uint32_t reserved3[951];
} SceSonyRegbusRegisters;

_Static_assert(offsetof(SceSonyRegbusRegisters, errorStatus) == 0x108, "regbus error status");
_Static_assert(offsetof(SceSonyRegbusRegisters, errorId) == 0x110, "regbus error log");
_Static_assert(offsetof(SceSonyRegbusRegisters, status) == 0x120, "regbus status");
_Static_assert(sizeof(SceSonyRegbusRegisters) == GRAB_PAGE_SIZE, "regbus register page");

/* the bridges at their physical addresses (the boot loaders, the secure world, the bus error
 * handler) */
#define SONY_REGBUS		((SceSonyRegbusRegisters *)SONY_REGBUS_BASE)
#define SONY_REGBUS_NS		((SceSonyRegbusRegisters *)SONY_REGBUS_NS_BASE)
#define GRAB			((SceGrabRegisters *)GRAB_BASE)
#define GRAB_NS			((SceGrabRegisters *)GRAB_NS_BASE)
