/*
 * The Power Scratchpad (ScePowerScratchPad32KiB, henkaku wiki Physical Memory):
 * 32 KiB of SRAM that stays powered through a suspend. Code that must run with
 * the DRAM in self refresh, or before it is back, runs from here; Pervasive
 * aliases it at address 0 for the ARM cores. Its firewall is
 * SSX_FIREWALL_SPAD32K_BASE (sce/ssx_firewall_regs.h).
 */
#pragma once

#define SPAD32K_BASE	0x1f000000
#define SPAD32K_SIZE	0x8000
