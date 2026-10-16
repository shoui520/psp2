#pragma once

/*
 * The SonicsSX interconnect's protection mechanism (firewall) in front of a
 * target agent, and its register target (henkaku wiki, Memory System: "Protection Mechanism"; the
 * register names are the OMAP35x TRM's L3 PM ones the wiki refers to).
 * Each register is 64 bits wide.
 */
#define SSX_FIREWALL_SPAD32K_BASE	0xec060000	/* ScePowerScratchPad32KiB */
#define SSX_FIREWALL_LPDDR0_BASE	0xec060400	/* the main LPDDR2 */
#define SSX_FIREWALL_LPDDR1_BASE	0xec560800	/* the TOOL units' LPDDR2, on the Debug Bus */

/* for assembly: region n's registers (SceSsxFirewallRegisters.region[n]) */
#define SSX_FIREWALL_REGION(n)			(0x40 + (n) * 0x20)
#define SSX_FIREWALL_REGION_ADDR_MATCH		0x00
#define SSX_FIREWALL_REGION_REQ_INFO_PERMISSION	0x08

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

typedef struct SceSsxFirewallRegion {
	volatile uint64_t addrMatch;		/* regions 1 and up; region 0 is the default */
	volatile uint64_t reqInfoPermission;	/* the initiators allowed in */
	volatile uint64_t readPermission;
	volatile uint64_t writePermission;
} SceSsxFirewallRegion;

typedef struct SceSsxFirewallRegisters {
	uint32_t reserved0[8];
	volatile uint64_t errorLog;		/* 0x20 */
	volatile uint64_t control;
	volatile uint64_t errorClearSingle;
	volatile uint64_t errorClearMulti;
	SceSsxFirewallRegion region[8];		/* 0x40 */
} SceSsxFirewallRegisters;

_Static_assert(offsetof(SceSsxFirewallRegisters, errorLog) == 0x20, "firewall error log");
_Static_assert(offsetof(SceSsxFirewallRegisters, region[0].reqInfoPermission) == 0x48,
	"firewall region 0");
_Static_assert(offsetof(SceSsxFirewallRegisters, region[1].addrMatch) == 0x60,
	"firewall region 1");
_Static_assert(offsetof(SceSsxFirewallRegisters, region[2].reqInfoPermission) == 0x88,
	"firewall region 2");
_Static_assert(offsetof(SceSsxFirewallRegisters, region[3].reqInfoPermission) ==
	SSX_FIREWALL_REGION(3) + SSX_FIREWALL_REGION_REQ_INFO_PERMISSION, "firewall region offsets");

/*
 * Each interconnect's register target: the configuration of the interconnect
 * itself (henkaku wiki, Memory System: "Register Target"), at 0x70000 in its
 * 1 MiB window. Each register is 64 bits wide.
 */
#define SSX_MAIN_XBAR		0
#define SSX_CENTER_XBAR		1
#define SSX_VIDEO_XBAR		2
#define SSX_IFTU_XBAR		3
#define SSX_VIP_XBAR		4
#define SSX_DEBUG_BUS		5
#define SSX_DMAC_XBAR		6

/* each crossbar's 1 MiB window */
#define SSX_WINDOW_SIZE			0x100000
#define SSX_XBAR_BASE(xbar)		(0xec000000 + (xbar) * SSX_WINDOW_SIZE)

#define SSX_REGISTER_TARGET_BASE(xbar)	(SSX_XBAR_BASE(xbar) + 0x70000)

/*
 * The buses behind the crossbars, each with its own 1 MiB window (henkaku
 * wiki, Physical Memory): 0xed000000 and up, then 0xee000000 and up.
 */
#define SSX_IO_SLAVE_BUS		0x00
#define SSX_IO_MASTER_BUS		0x01
#define SSX_PERVASIVE_BUS		0x02
#define SSX_MISC_IO_BUS			0x03
#define SSX_NORTH_IO_MASTER_BUS		0x04
#define SSX_AUDIO_BUS			0x05
#define SSX_NORTH_IO_SLAVE_BUS		0x06
#define SSX_SRC_BUS			0x07
#define SSX_GPU_REGISTER_BUS		0x08
#define SSX_IFTU_REGISTER_BUS		0x09
#define SSX_CAMERA_SLAVE_BUS		0x10
#define SSX_CAMERA_MASTER_BUS		0x11
#define SSX_USB_SLAVE_BUS		0x12
#define SSX_USB_MASTER_BUS		0x13
#define SSX_SUBLCD_SLAVE_BUS		0x14
#define SSX_SUBLCD_MASTER_BUS		0x15
#define SSX_CAMERA_REGISTER_BUS		0x16

#define SSX_BUS_BASE(bus)		(0xed000000 + (bus) * SSX_WINDOW_SIZE)

typedef struct SceSsxRegisterTarget {
	volatile uint64_t component;
	uint32_t reserved0[2];
	volatile uint64_t network;		/* 0x10: the interconnect's ID in bits 63:48 */
	uint32_t reserved1[22];
	volatile uint64_t initIdReadback;	/* 0x70 */
	volatile uint64_t networkControl;	/* 0x78: TIMEOUT_BASE in bits 10:8 */
} SceSsxRegisterTarget;

_Static_assert(offsetof(SceSsxRegisterTarget, network) == 0x10, "register target network");
_Static_assert(offsetof(SceSsxRegisterTarget, networkControl) == 0x78,
	"register target network control");

#endif /* __ASSEMBLER__ */
