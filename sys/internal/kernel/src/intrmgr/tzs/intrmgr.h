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

/* ForTZS imports: the os0 name of each one's function */
#define SceDipswForDriver_A98FC2FD			sceKernelCheckDipsw
#define SceCpuForDriver_BF82DEB2		sceKernelSpinlockLowLock
#define SceCpuForDriver_D32ACE9E	sceKernelSpinlockLowLockCpuSuspendIntr
#define SceDebugForDriver_D9703808		SceDebugForTZS_D26EB4BB
#define SceDebugForDriver_374B7868		SceDebugForTZS_F851BA4F
#define SceDebugForDriver_FD753E7A			SceDebugForTZS_618F89E6
#define ScePmMgrForDriver_2AC815A2			sceSblPmMgrGetProductMode
#define SceDebugForDriver_35A35322			sceKernelAssert
#define SceSysmemForDriver_009E1C61			sceKernelFreeMemBlock

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

/* Sony built the 3.6x secure kernel without its debug output */
#if !FW_SINCE(0x04000000)
#undef SCE_DBG_PRINTF
#define SCE_DBG_PRINTF(...)		((void)0)
#undef SCE_DBG_PRINTF_LEVEL
#define SCE_DBG_PRINTF_LEVEL(...)	((void)0)
#undef SCE_DBG_ASSERT_FAIL
#define SCE_DBG_ASSERT_FAIL(...)	((void)0)
#endif

/* SKBL builds this module into the secure kernel boot loader (INTRMGR_SKBL) */

/* intrmgr.c */
extern IntrState g_intrState;
#define g_intrHandlers (g_intrState.handlers)
#define g_intrCpu (g_intrState.cpu)
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
