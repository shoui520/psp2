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
#define SceSysmemForDriver_C94850C9			SceSysmemForTZS_402EB970
#define SceSysmemForDriver_A841EDDA		SceSysmemForTZS_E1DEDFF4
#define SceDipswForDriver_A98FC2FD			SceDipswForTZS_ksceKernelCheckDipsw
#define SceCpuForDriver_BF82DEB2		SceCpuForTZS_71FD9AB5
#define sceKernelSpinlockLowUnlock		SceCpuForTZS_CD98416C
#define SceCpuForDriver_D32ACE9E	SceCpuForTZS_D67A4356
#define sceKernelSpinlockLowUnlockCpuResumeIntr SceCpuForTZS_B8F00FBE
#define SceDebugForDriver_D9703808		SceDebugForTZS_D26EB4BB
#define SceDebugForDriver_374B7868		SceDebugForTZS_F851BA4F
#define SceDebugForDriver_FD753E7A			SceDebugForTZS_618F89E6
#define memset					SceSysclibForTZS_956DB750
#define ScePmMgrForDriver_2AC815A2			ScePmMgrForTZS_CF554E69
#define SceDebugForDriver_35A35322			SceDebugForTZS_2F9B8AA8
#define SceSysmemForDriver_009E1C61			SceSysmemForTZS_658EACE3

#include <fw.h>
#include <psp2kern/types.h>
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

/* Sony built the 3.6x secure kernel without its debug output */
#if !FW_SINCE(0x04000000)
#undef SCE_DBG_PRINTF
#define SCE_DBG_PRINTF(...)		((void)0)
#undef SCE_DBG_PRINTF_LEVEL
#define SCE_DBG_PRINTF_LEVEL(...)	((void)0)
#undef SCE_DBG_ASSERT_FAIL
#define SCE_DBG_ASSERT_FAIL(...)	((void)0)
#endif

/* the compiler's stack-protector symbols are the ForTZS sysclib ones here */
__asm__(".set __stack_chk_fail, SceSysclibForTZS_E5457B97\n\t"
	".set __stack_chk_guard, SceSysclibForTZS_BA505B15");

/* ForTZS imports named by the henkaku wiki's SceSysmem and SceExcpmgr pages */
#define sceKernelAllocPartitionMemBlockForTZS	SceSysmemForTZS_0028E26C
#define sceKernelAddressSpaceUnmapForTZS	SceSysmemForTZS_31273DC7
#define sceKernelNameHeapInsertForTZS		SceSysmemForTZS_42AD34AB
#define sceKernelNameHeapDeleteForTZS		SceSysmemForTZS_F459D09D
#define sceKernelSysrootGetSharedMemoryForTZS	SceSysrootForTZS_1C96096F
#define sceKernelSysrootSetSharedMemoryForTZS	SceSysrootForTZS_45EEB2E0
#define sceKernelSysrootSetModulePrivateForTZS	SceSysrootForTZS_EA572BD9
#define sceKernelL1DcacheCleanInvalidateAll	SceCpuForTZS_0A15B41C
#define sceKernelCpuWaitL2Cache			SceCpuForTZS_2B6403F8
#define sceKernelPleFlushRequest		SceCpuForTZS_C4137AED
#define sceKernelRegisterMonitorEntryForTZS	sceKernelRegisterMonitorEntry

SceUID sceKernelAllocPartitionMemBlockForTZS(SceUID partition, const char *name, SceUInt32 type,
					     SceSize size,
					     const SceKernelAllocMemBlockKernelOpt *opt);
int sceKernelAddressSpaceUnmapForTZS(SceUID addressSpace, SceUInt32 flags, SceUInt32 type,
				     void *vaddr, SceSize size);
int sceKernelNameHeapInsertForTZS(const char *name, SceUInt32 flags, char **out);
int sceKernelNameHeapDeleteForTZS(char *name, SceUInt32 flags);
void *sceKernelSysrootGetSharedMemoryForTZS(void);
void sceKernelSysrootSetSharedMemoryForTZS(void *base);
int sceKernelSysrootSetModulePrivateForTZS(unsigned int slot, void *data);
void sceKernelL1DcacheCleanInvalidateAll(void);
int sceKernelCpuWaitL2Cache(void);
void sceKernelPleFlushRequest(void);
int SceCpuForTZS_9E4C0D0D(void);			/* before an L2 control change */
int sceKernelRegisterMonitorEntryForTZS(int vector, void *entry);

/* SKBL builds this module into the secure kernel boot loader (INTRMGR_SKBL) */

/* intrmgr.c */
extern IntrState g_intrState;
#define g_intrHandlers (g_intrState.handlers)
#define g_intrCpu (g_intrState.cpu)
extern volatile GicCpuInterface *volatile g_intrCpuInterface;
extern SceKernelMonitorCallHandler *g_moncallTable;
extern SceUID g_moncallTableUid;

int IllegalContext(void);
int sceKernelSetIntrTargetCpu(unsigned int intrCode, unsigned int target);

/* intrmgr_entry.S */
extern char monitorFiqEntry[], monitorSmcEntry[];
void enableInterrupts(void);

/* monitor calls of dmacmgr_tzs.c (names unknown) */
#define DMAC_MONCALL_SET_ACCESS0	0x168
#define DMAC_MONCALL_SET_ACCESS1	0x169

/* dmacmgr_tzs.c */
int initDmacSecure(void);

/* usleep.c */
void sceKernelUsleep(SceUInt32 usec);
int InitUsleepTimer(int remap);

/* access_mode.c */
int RegisterAccessModeMonitorCall(void);
