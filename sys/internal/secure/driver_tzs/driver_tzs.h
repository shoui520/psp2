/*
 * SceDriverTzs, the secure world's device driver (slb2:kbl/SceDriverTzs).
 *
 * It holds the Pervasive devices the Non-secure kernel may not touch: the
 * secure reset, gate and clock words (monitor calls 0x110-0x114, which
 * SceLowio's ScePervasiveForDriver exports call), the grab and SonyRegbus
 * windows (0x11b-0x121), the CDRAM's controller (0x117-0x119) and the crash
 * dump (0x122).
 */
#pragma once

/* the ForTZS imports under their os0 names, before any declaration */
#include <sysmem/debug_tzs.h>

#include <fw.h>
#include <stdint.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr_tzs.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <lowio/uart_regs.h>
#include <compat/grab_regs.h>
#include "emc_regs.h"
#include <sysmem/dipsw.h>

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
#define CRASHDUMP_MONCALL		0x122

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

/* grab.c, power_tzs.c: what the crash dump calls */
int GrabStopMonitorCall(unsigned int mask, int restart);
int sceSysconSetPowerMode(unsigned int mode, unsigned int argument);

/* grab.c, emc_tzs.c, power_tzs.c, crashdump_tzs.c: module_start's later stages */
int GrabModuleStart(SceSize arglen, const void *argp);
int EmcModuleStart(SceSize arglen, const void *argp);
int CrashDumpModuleStart(SceSize arglen, const void *argp);
int PowerModeModuleStart(SceSize arglen, const void *argp);
