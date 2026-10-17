/*
 * SceSysmem sysroot services (SceSysrootForKernel / SceSysrootForDriver), as used so far.
 */

#pragma once

#include <psp2kern/types.h>
#include <sysmem/sysroot_init.h>
#include <sysmem/sysroot_netps.h>
#include <sysmem/sysroot_appmgr.h>

#include <buserror/buserror_types.h>

/* the sysroot shared memory (ksceKernelSysrootGetSharedMemory), name unknown */
typedef struct SceSysrootSharedMemory {
	SceUInt8 secure[0x1000];		/* its address goes to the secure monitor (intrmgr) */
	char tty[0x1000];			/* the TTY log at a crash or bus error */
	char report[4][0x1000];			/* bsod: each core's crash report */
	BusErrorRecord busError;		/* 0x6000 */
} SceSysrootSharedMemory;

void *ksceKernelSysrootGetSharedMemory(void);

/* Manufacturing-mode query in SceSysrootForDriver (NID 0x55392965). */
SceUInt32 SceSysrootForDriver_ksceSysrootIsManufacturingMode(void);

static inline SceSysrootSharedMemory *sysrootSharedMemory(void)
{
	return ksceKernelSysrootGetSharedMemory();
}

/* Name unknown: returns the hardware model classification. */
SceUInt32 SceSysrootForKernel_403B509E(void);
/* Name unknown: installs a size0x10 timebase provider, or clears it. */
struct PamgrTimebaseProvider;
void SceSysrootForDriver_C94C76FA(const struct PamgrTimebaseProvider *provider);
void sceKernelSysrootDisableAutoClockDown(void);
void sceKernelSysrootEnableAutoClockDown(void);
int sceKernelSysrootCorelockLock(SceUInt32 core);
void sceKernelSysrootCorelockUnlock(void);

/* name unknown: registers the TMCP command handler (os0:kd/deci4p_tmcp.skprx) */
int SceSysrootForDriver_1834D482(void *handler);

/* name unknown: a 64-bit timestamp */
SceUInt64 SceSysrootForDriver_273EAE53(void);

/* the hardware info word of the boot parameters (vitasdk lacks it) */
SceUInt32 ksceSysrootGetHardwareInfo(void);

/* name unknown: returns the descriptor's logger callback address, or zero */
SceUInt32 SceSysrootForDriver_3AE319DA(void);

/* names unknown: debugger hooks (os0:kd/deci4p_userp.skprx) */
int SceSysrootForDriver_06436EF0(SceUInt32 flags, const char *pathName, const char *cmdLine, const char *workDir,
				 void *out, void *out2, SceUID callback, void *status, int async);	/* create host process */
int SceSysrootForDriver_39F1FD07(SceUID pid);		/* is process attached */
int SceSysrootForDriver_7B4A377C(void);				/* enable watchpoint */
int SceSysrootForDriver_65F3D598(void);				/* disable watchpoint */

/* name unknown: registers the per-process tty log reader (os0:kd/bootimage/tty2uart) */
int SceSysrootForDriver_AA770EF7(int (*getLogRaw)(SceUID pid, void *buf, int size));

/* names unknown (os0:kd/buserror.skprx) */
int SceSysrootForKernel_82FC6405(SceSize (*getInfo)(void *buf, SceSize size));	/* registers the bus error record reader */
int SceSysrootForKernel_B27B7530(SceUInt32 status0, SceUInt32 status1, SceUInt32 status2);

/* names unknown (os0:kd/deci4p_sdfctl.skprx): register the host's boot parameter / target status senders */
int SceSysrootForDriver_E541959B(int (*setBootParameter)(const char *key, SceUInt32 value));
int SceSysrootForDriver_D56A2F3E(int (*sendTargetStatus)(SceUInt32 status));

/* names unknown (os0:kd/intrmgr.skprx) */
void *SceSysrootForKernel_118657C6(void);
int sceKernelSysrootSetModulePrivate(unsigned int slot, void *value);
int sceKernelSysrootTrapThreadAfterSyscall(void *args, void *results, unsigned int flags);
void *ksceKernelSysrootGetVbaseResetVector(void);
int ksceSysrootIsBsodReboot(void);	/* as vitasdk declares it */
