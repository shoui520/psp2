/*
 * SceKernelIntrMgr, secure-world build (slb2:kbl/SceKernelIntrMgr).
 *
 * Sony built it from the os0 intrmgr.c with TrustZone-only parts (the
 * monitor call table, the L2 cache controller and kernel-area services), plus
 * dmacmgr_tzs.c, usleep.c and access_mode.c. It calls the ForTZS libraries of
 * the secure kernel.
 */
#pragma once

/* the ForTZS imports under their os0 names, before any declaration */
#include <sysmem/debug_tzs.h>

#include <fw.h>
#include <scetypes.h>
#include <sysmem/sysmem.h>
#include <sysmem/utils.h>
#include <sysmem/pmmgr.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <intrmgr/intrmgr.h>
#include <intrmgr/intrmgr_tzs.h>
#include "../intrmgr_state.h"
#include <sysmem/dipsw.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <excpmgr/excpmgr.h>

/* SKBL builds this module into the secure kernel boot loader (INTRMGR_SKBL) */

/* intrmgr.c */
extern IntrState g_intrState;
extern volatile GicCpuInterface *volatile g_intrCpuInterface;
extern SceKernelMonitorCallHandler *g_moncallTable;
extern SceUID g_moncallTableUid;

int IllegalContext(void);

/* intrmgr_entry.S */
extern SceExcpmgrHandler monitorFiqEntry, monitorSmcEntry;
void enableInterrupts(void);

/* monitor calls of dmacmgr_tzs.c (names unknown) */
#define DMAC_MONCALL_SET_ACCESS0	0x168
#define DMAC_MONCALL_SET_ACCESS1	0x169

/* dmacmgr_tzs.c */
int initDmacSecure(void);

/* usleep.c */
int InitUsleepTimer(int remap);

/* access_mode.c */
int RegisterAccessModeMonitorCall(void);
