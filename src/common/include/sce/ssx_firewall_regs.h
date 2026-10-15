#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * The SonicsSX interconnect's protection mechanism (firewall) in front of a
 * target agent (henkaku wiki, Memory System: "Protection Mechanism"; the
 * register names are the OMAP35x TRM's L3 PM ones the wiki refers to).
 * Each register is 64 bits wide.
 */
#define SSX_FIREWALL_SPAD32K_BASE	0xec060000	/* ScePowerScratchPad32KiB */
#define SSX_FIREWALL_LPDDR0_BASE	0xec060400

typedef struct SceSsxFirewallRegion {
	volatile uint64_t reqInfoPermission;	/* the initiators allowed in */
	volatile uint64_t readPermission;
	volatile uint64_t writePermission;
	volatile uint64_t addrMatch;		/* regions 1 and up */
} SceSsxFirewallRegion;

typedef struct SceSsxFirewallRegisters {
	uint32_t reserved0[8];
	volatile uint64_t errorLog;		/* 0x20 */
	volatile uint64_t control;
	volatile uint64_t errorClearSingle;
	volatile uint64_t errorClearMulti;
	uint32_t reserved1[2];
	SceSsxFirewallRegion region[8];		/* 0x48 */
} SceSsxFirewallRegisters;

_Static_assert(offsetof(SceSsxFirewallRegisters, errorLog) == 0x20, "firewall error log");
_Static_assert(offsetof(SceSsxFirewallRegisters, region) == 0x48, "firewall region 0");
_Static_assert(offsetof(SceSsxFirewallRegisters, region[1].reqInfoPermission) == 0x68,
	"firewall region 1");
