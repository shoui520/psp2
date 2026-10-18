/*
 * SceDriverTzs, the secure world's device driver (slb2:kbl/SceDriverTzs).
 *
 * It holds the Pervasive devices the Non-secure kernel may not touch: the
 * secure reset, gate and clock words (monitor calls 0x110-0x114, which
 * SceLowio's ScePervasiveForDriver exports call), the grab and SonyRegbus
 * windows (0x11b-0x121), the CDRAM's controller (0x117-0x119) and the crash
 * dump (0x122). Its ForTZS imports are mapped to the os0 names here, before
 * any declaration.
 */
#pragma once

/* ForTZS imports: names unknown, the os0 function each one stands for */
#define SceSysmemForDriver_C94850C9			SceSysmemForTZS_402EB970
#define SceSysmemForDriver_A841EDDA		SceSysmemForTZS_E1DEDFF4
#define SceSysmemForDriver_009E1C61			SceSysmemForTZS_658EACE3
#define SceDipswForDriver_A98FC2FD			SceDipswForTZS_A98FC2FD
#define SceCpuForDriver_D32ACE9E	SceCpuForTZS_D67A4356
#define sceKernelSpinlockLowUnlockCpuResumeIntr SceCpuForTZS_B8F00FBE
#define SceDebugForDriver_D9703808		SceDebugForTZS_D26EB4BB
#define memset					SceSysclibForTZS_956DB750
#define memcpy					SceSysclibForTZS_BE3AF2EE

/* ForTZS imports named by the henkaku wiki (SceSysmem page) */
#define sceKernelVAtoPAForTZS			SceSysmemForTZS_1DEADF6C
#define sceKernelCpuAtomicOrAndGet32		SceCpuForTZS_75D87321
#define sceKernelDcacheCleanInvalidateRange	SceCpuForTZS_7A5373EB
#define sceKernelDcacheCleanInvalidateAll	SceCpuForTZS_7CCE9480
#define sceKernelSysrootGetModelInfoForTZS	SceSysrootForTZS_D6FBF445	/* the wiki's guess */
#define sceKernelSysrootGetErnieWakeupFactorForTZS SceSysrootForTZS_8E871C6D

#include <fw.h>
#include <stdint.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr_tzs.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <lowio/uart_regs.h>
#include <compat/grab_regs.h>
#include "emc_regs.h"
#include <sysmem/dipsw.h>

/* Sony built the 3.6x secure kernel without its debug output */
#if !FW_SINCE(0x04000000)
#undef SCE_DBG_PRINTF
#define SCE_DBG_PRINTF(...)		((void)0)
#endif

/* the compiler's stack-protector symbols are the ForTZS sysclib ones here */
__asm__(".set __stack_chk_fail, SceSysclibForTZS_E5457B97\n\t"
	".set __stack_chk_guard, SceSysclibForTZS_BA505B15");

int sceKernelVAtoPAForTZS(const void *va, uintptr_t *pa);
int sceKernelCpuAtomicOrAndGet32(volatile int *addr, int value);
void sceKernelDcacheCleanInvalidateRange(const void *addr, unsigned int size);
void sceKernelDcacheCleanInvalidateAll(void);
unsigned int sceKernelSysrootGetModelInfoForTZS(void);
unsigned int sceKernelSysrootGetErnieWakeupFactorForTZS(void);
void SceCpuForTZS_D4E7413D(void);	/* name unknown: cleans the data cache before a core parks */

#define PERVASIVE_ERROR			((int)0x803f0000)
#define ALL_CPUS			0xf	/* an interrupt's target CPUs */

/* the monitor calls this module serves */
#define PERVASIVE_MONCALL_POWER		0x110
#define PERVASIVE_MONCALL_RESET		0x111
#define PERVASIVE_MONCALL_GATE		0x113
#define PERVASIVE_MONCALL_CLOCK		0x114
#define CDRAM_MONCALL_ENABLE		0x117
#define CDRAM_MONCALL_ENTER_SELF_REFRESH	0x118
#define CDRAM_MONCALL_EXIT_SELF_REFRESH	0x119
#define SYSCON_MONCALL_SET_POWER_MODE	0x11a
#define GRAB_MONCALL_STOP		0x11b
#define GRAB_MONCALL_PORT_CONFIG	0x11c
#define GRAB_MONCALL_SET_CMP_MAP	0x11d
#define GRAB_MONCALL_COMPAT		0x11e
#define GRAB_MONCALL_REGBUS_WAIT	0x11f
#define GRAB_MONCALL_GET_CMP_MAP	0x120
#define GRAB_MONCALL_GPU_CLOCK		0x121

/* Sony's DMB and DSB are also compiler barriers here: register pointers
 * held across them are reloaded. */
static inline void PervasiveDmb(void)
{
	__asm__ volatile("dmb sy" : : : "memory");
}

static inline void PervasiveDsb(void)
{
	__asm__ volatile("dsb sy" : : : "memory");
}

extern ScePervasiveMiscRegisters *s_misc;
extern ScePervasiveResetRegisters *s_reset;
extern ScePervasiveGateRegisters *s_gate;
extern ScePervasiveBaseClkRegisters *s_baseClock;

/* pervasive_tzs.c, pervasive_control_tzs.c, pervasive_clock_tzs.c */
unsigned int GetSoCRevision(void);
int ParkOtherCpus(int cleanCache);
int ReleaseOtherCpus(void);
int AllegrexPowerOn(int fast);
int AllegrexStart(void);
int AllegrexPowerOff(void);
int GpuGateEnable(unsigned int mask);
int GpuGateDisable(unsigned int mask);
int DmacGateEnable(int n);
int GpuClockSet(unsigned int core, unsigned int mp);
int PervasiveModuleStart(SceSize arglen, const void *argp);

/* grab.c, emc_tzs.c, power_tzs.c, crashdump_tzs.c: module_start's later stages */
int GrabModuleStart(SceSize arglen, const void *argp);
int EmcModuleStart(SceSize arglen, const void *argp);
int CrashDumpModuleStart(SceSize arglen, const void *argp);
int PowerModeModuleStart(SceSize arglen, const void *argp);
