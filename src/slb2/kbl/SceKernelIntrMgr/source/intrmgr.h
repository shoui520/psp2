/*
 * SceKernelIntrMgr, secure-world build (slb2:kbl/SceKernelIntrMgr).
 *
 * Sony built it from the os0 intrmgr.c with TrustZone-only parts (the
 * monitor call table, the L2 cache controller and kernel-area services), plus
 * dmacmgr_tzs.c, usleep.c and access_mode.c. It calls the ForTZS libraries of
 * the secure kernel; their imports are mapped to the os0 names here, before any
 * declaration.
 */
#pragma once

/* ForTZS imports: names unknown, the os0 function each one stands for */
#define ksceKernelAllocMemBlock			SceSysmemForTZS_402EB970
#define ksceKernelGetMemBlockBase		SceSysmemForTZS_E1DEDFF4
#define ksceKernelCheckDipsw			SceDipswForTZS_ksceKernelCheckDipsw
#define ksceKernelPrintfLevelWithInfo		SceDebugForTZS_D26EB4BB
#define ksceKernelGetAssertLevel		SceDebugForTZS_F851BA4F
#define ksceKernelAssertLevel			SceDebugForTZS_618F89E6
#define memset					SceSysclibForTZS_956DB750

#include <psp2kern/types.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/utils.h>
#include "sce/debug.h"
#include "sce/error.h"
#include "sce/intrmgr_tzs.h"

/* the compiler's stack-protector symbols are the ForTZS sysclib ones here */
__asm__(".set __stack_chk_fail, SceSysclibForTZS_E5457B97\n\t"
	".set __stack_chk_guard, SceSysclibForTZS_BA505B15");

int ksceKernelCheckDipsw(unsigned int number);

/* monitor calls of dmacmgr_tzs.c (names unknown) */
#define DMAC_MONCALL_SET_ACCESS0	0x168
#define DMAC_MONCALL_SET_ACCESS1	0x169

/* dmacmgr_tzs.c */
int initDmacSecure(void);
