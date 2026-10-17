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
	return (SceSysrootSharedMemory *)ksceKernelSysrootGetSharedMemory();
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

uint32_t SceSysrootForDriver_0A52A59C(void);	/* ScePervasiveMisc (0xe3100000) */
int SceSysrootForDriver_0F59B369(SceUInt32 result, const char *msg, int len);
uint32_t SceSysrootForDriver_10788257(void);
int SceSysrootForDriver_1160FCA1(SceUID pid, SceUInt32 *flag);
int SceSysrootForDriver_17DD213C(const char *name, SceSize nameLength, const char *str, SceSize strLength);
int SceSysrootForDriver_1B3FA457(SceUID pid);
int SceSysrootForDriver_1BA0C6C1(SceInt32 kind, SceUInt32 percent);
int SceSysrootForDriver_1D61C837(SceUID pid, SceUID spawnPid, uint64_t time);
int SceSysrootForDriver_217B2871(SceUInt32 result, const char *msg, int len);
int SceSysrootForDriver_26AA237C(void);
int SceSysrootForDriver_3F76D531(SceUID pid);
void SceSysrootForDriver_400B9793(int (*callback)(int arg));
int SceSysrootForDriver_421EFC96(SceUID pid);
int SceSysrootForDriver_47724459(int event);
int SceSysrootForDriver_51F9C118(SceUID pid, SceUID spawnPid, SceUID prevPid, uint64_t time);
int SceSysrootForDriver_56C4C949(SceInt32 kind, SceUInt32 total, const char *msg, int len);
int SceSysrootForDriver_571E5B79(SceUID parentPid, SceUID pid, int flags, void *process, uint64_t time);
int SceSysrootForDriver_582616EC(SceUID pid, SceUID spawnPid, SceUID prevPid, uint64_t time);
int SceSysrootForDriver_591BB490(int magic, void *buffer, SceSize size);
int SceSysrootForDriver_5C86E49B(SceUID pid, SceUInt32 status, SceUInt64 time);
int SceSysrootForDriver_6050A467(SceUID pid);
int SceSysrootForDriver_6219CC14(void *dbgp, void *process, SceUInt32 set, SceUInt32 clear, SceUInt32 *flag);
int SceSysrootForDriver_631141E2(const char *name, SceSize length, SceUInt32 number);
int SceSysrootForDriver_6D44F190(SceUID pid, SceUInt64 time);
int SceSysrootForDriver_70A60BBA(SceInt32 kind, SceUInt32 total, const char *msg, int len);
int SceSysrootForDriver_77CF3FBD(SceUID pid, SceUID spawnPid, SceUInt64 time);
int SceSysrootForDriver_79458ACB(SceInt32 kind, SceUInt32 percent);
int SceSysrootForDriver_7998F5B5(SceInt32 kind, SceUInt32 percent);
int SceSysrootForDriver_8498E23F(SceUID pid, int exitStatus, SceUInt64 time);
int SceSysrootForDriver_89532E2F(void);
int SceSysrootForDriver_97E2FB77(SceInt32 kind, SceUInt32 total, const char *msg, int len);
int SceSysrootForDriver_9A486846(SceInt32 kind, SceUInt32 total, const char *msg, int len);
int SceSysrootForDriver_9AF075D4(SceUID pid);
int SceSysrootForDriver_9BF782CA(SceUInt32 led, SceUInt32 mode, const void *option);
int SceSysrootForDriver_9D058A32(SceUID pid, int exitStatus, uint64_t time);
int SceSysrootForDriver_9E96D990(const char *name, SceSize nameLength, const void *data, SceSize dataSize);
int SceSysrootForDriver_9EC02A41(SceInt32 kind, SceUInt32 percent);
int SceSysrootForDriver_AA8D34EE(SceUID pid, SceUID modid, SceUInt64 time);
int SceSysrootForDriver_B501E861(SceUID pid, uint64_t time);
unsigned int SceSysrootForDriver_BFD8F2A2(void);
int SceSysrootForDriver_CA497324(SceUID pid, SceUID modid, SceUInt64 time);
int SceSysrootForDriver_CAE820B2(SceUID pid, SceUInt32 *sdkVersion);
int SceSysrootForDriver_D03741B8(SceUInt32 result, const char *msg, int len);
int SceSysrootForDriver_D04C7723(SceUInt32 result, const char *msg, int len);
int SceSysrootForDriver_D75D4F37(void *buffer);
int SceSysrootForDriver_E2515A08(void);
int SceSysrootForDriver_E384DD93(SceUInt32 result, const char *msg, int len);
int SceSysrootForDriver_E8EBF179(SceInt32 kind, SceUInt32 total, const char *msg, int len);
int SceSysrootForDriver_F0C91CD3(SceInt32 kind, SceUInt32 percent);
int SceSysrootForDriver_F2BC9E1D(SceUID pid);
int SceSysrootForDriver_F4340469(SceUID parentPid, SceUID pid, SceUInt64 time);
int SceSysrootForDriver_F804F761(void);
struct SysrootPerCpu *SceSysrootForKernel_06182D59(unsigned int cpu);
int SceSysrootForKernel_1247A825(SceUID pid, SceUInt32 state);
int SceSysrootForKernel_1D84C4D4(SceUID pid, SceUInt32 addr, SceUInt32 *fingerprint, void **textBase, char *name);
int SceSysrootForKernel_20D2A0DF(SceUID pid, SceUID modid, uint64_t time);
int SceSysrootForKernel_2A03DFA1(void *out);
int SceSysrootForKernel_377895EB(void);
int SceSysrootForKernel_38751FB7(SceUID pid, SceUInt32 address, SceUInt32 instruction);
uint32_t SceSysrootForKernel_41636522(void);
int SceSysrootForKernel_47724459(int event);
int SceSysrootForKernel_6050A467(SceUID pid);
int SceSysrootForKernel_6D111FA7(int appId, void *out);
SceUID SceSysrootForKernel_7334F1E8(SceUID processId);
int SceSysrootForKernel_73522F65(SceUID pid, SceUInt32 *flag);
int SceSysrootForKernel_7968F091(SceUID pid, void *ptr);
int SceSysrootForKernel_AA8D34EE(SceUID pid, SceUID modid, SceUInt64 time);
void SceSysrootForKernel_B39CD708(int (*callback)(uint32_t nid, const char **name));
int SceSysrootForKernel_C67A1FA9(SceUID pid, SceUID modid, int percent, const char *path);
int SceSysrootForKernel_C81B7E2B(SceUID pid, SceUID modid, uint64_t time);
int SceSysrootForKernel_CA497324(SceUID pid, SceUID modid, SceUInt64 time);
int SceSysrootForKernel_CBD377B7(int (*callback)(void));
uint64_t SceSysrootForKernel_E20F6FC8(void);
int SceSysrootForKernel_ksceKernelSysrootAppMgrSpawnProcess(const char *name, int type, const char *path, SceSize length, const void *args, void *opt, void *out);
uint32_t SceSysrootForKernel_ksceSysrootIsBsodReboot(void);
int SceSysrootForKernel_ksceSysrootIsSafeMode(void);
uint32_t SceSysrootForKernel_ksceSysrootIsUpdateMode(void);
int SceSysrootForKernel_ksceSysrootIsUsbEnumWakeup(void);
int ksceKernelIsColdBoot(void);
/* nonzero when the model has the capability (1: multiple controller ports) */
unsigned int ksceKernelSysrootCheckModelCapability(unsigned int capability);
void *ksceKernelSysrootGetModulePrivate(unsigned int index);
int ksceKernelSysrootGetPUIDEntryHeap(SceUID processId, void **entryHeap);
uint32_t ksceKernelSysrootGetStatus(void);
int ksceKernelSysrootGetThreadAccessLevel(void);
int ksceSysrootGetSelfAuthInfo(SceUID processId, SceSelfAuthInfo *info);
int ksceSysrootUtMgrHasNpTestFlag(void);
void sceKernelApiDeclareTypeInt64(void);
void sceKernelApiDeclareTypeRaw(void);
void sceKernelApiDeclareTypeUInt64(void);
void sceKernelSysrootSetVbaseResetVector(uint32_t value);
