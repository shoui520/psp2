/*
 * SceSysmem sysroot services (SceSysrootForKernel / SceSysrootForDriver), as used so far.
 */

#pragma once

#include <scetypes.h>
#include <authmgr/sblauthmgr.h>
#include <sysmem/sysroot_init.h>
#include <sysmem/sysroot_netps.h>
#include <sysmem/sysroot_appmgr.h>
#include <sysmem/sysroot_acmgr.h>
#include <sysmem/sysroot_bt.h>
#include <sysmem/sysroot_dbgp.h>
#include <sysmem/sysroot_dfmgr.h>
#include <sysmem/sysroot_hid.h>
#include <sysmem/sysroot_modulemgr.h>
#include <sysmem/sysroot_pamgr.h>
#include <sysmem/sysroot_process_events.h>
#include <sysmem/sysroot_processmgr.h>
#include <sysmem/sysroot_threadmgr.h>
#include <systimer/timer_regs.h>
#include <coredump/coredump.h>

/* services other modules register with Sysroot */
typedef int (*SysrootBusErrorHandler)(SceUInt32 status0, SceUInt32 status1,
				      SceUInt32 status2);
typedef SceSize (*SysrootBusErrorGetInfo)(void *buffer, SceSize size);
typedef int (*SysrootCoredumpAddThread)(const struct CoredumpThreadSnapshot *snapshot);
typedef int (*SysrootUtMgrHasAuthId)(SceUInt64 authId);
typedef SceUInt8 (*SysrootUtMgrHasFlag)(void);
typedef int (*SysrootLedSetMode)(SceUInt32 led, SceUInt32 mode, const void *option);
typedef int (*SysrootSysStateEvent)(int event);
typedef int (*SysrootSendSystemError)(int type, SceSize size, const char *buffer);
typedef int (*SysrootSetSwInfoInt)(const char *name, SceSize nameLength, SceUInt32 number);
typedef int (*SysrootSetSwInfoStr)(const char *name, SceSize nameLength, const char *str,
				   SceSize strLength);
typedef int (*SysrootSetSwInfoBin)(const char *name, SceSize nameLength, const void *data,
				   SceSize dataSize);
struct SceVfsMount;
typedef struct ScePfsMountInfo {
	struct SceVfsMount *mount;
	const char *path;
	const char *rootPath;
	const char *assignName;
	SceUInt16 mode;
	SceUInt32 quota;
	SceUInt32 version;
	SceInt32 approvedMax;
	SceUInt64 *approved;
	SceInt32 acidDirCount;
	void *acidDirs[16];
} ScePfsMountInfo;
typedef int (*SysrootPfsMgrGetMountInfo)(struct SceVfsMount *mount, ScePfsMountInfo *info);
typedef int (*SysrootTmcpCommand)(SceUInt32 flags, const char *pathName,
				  const char *cmdLine, const char *workDir, SceUInt32 *id,
				  SceUInt32 *value, SceUID callbackUid, void *status, int async);
typedef int (*SysrootCoredumpTrigger)(SceUID pid, SceKernelCoredumpStateUpdateCallback update,
				      SceKernelCoredumpStateFinishCallback finish,
				      SceCoredumpTriggerParam *param);
typedef int (*SysrootSetBootParameter)(const char *key, SceUInt32 value);
typedef int (*SysrootSendTargetStatus)(SceUInt32 status, SceUInt32 value);
typedef int (*SysrootTtyGetLogRaw)(SceUID pid, void *buffer, int size);

/* sysmem's own, opaque to the modules that pass them */
struct SceKblParam;
struct SysrootDebugNotifiers;
struct SysrootProfiler;

/* a secure module the boot loader read into memory (sysroot's selfImages) */
struct SysrootSelfImage {
	void *data;
	SceSize size;
};

#ifndef __cplusplus
_Static_assert(sizeof(struct SysrootSelfImage) == 8, "Sysroot SELF image entry size");
#endif

#include <buserror/buserror_types.h>

/* the sysroot shared memory (sceKernelSysrootGetSharedMemory), name unknown */
typedef struct SceSysrootSharedMemory {
	SceUInt8 secure[0x1000];		/* its address goes to the secure monitor (intrmgr) */
	char tty[0x1000];			/* the TTY log at a crash or bus error */
	char report[4][0x1000];			/* bsod: each core's crash report */
	BusErrorRecord busError;		/* 0x6000 */
} SceSysrootSharedMemory;

void *sceKernelSysrootGetSharedMemory(void);

/* the SELF images sysroot keeps for the secure modules (SceSysrootForKernel_F10AB792) */
#define SCE_KERNEL_SYSROOT_SELF_INDEX_GCAUTHMGR_SM			0
#define SCE_KERNEL_SYSROOT_SELF_INDEX_RMAUTH_SM			1
#define SCE_KERNEL_SYSROOT_SELF_INDEX_ENCDEC_W_PORTABILITY_SM		2

struct SysrootSelfInfo {
	SceSize size;				/* sizeof(struct SysrootSelfInfo) */
	void *selfData;				/* out */
	SceSize selfSize;			/* out */
};

#ifndef __cplusplus
_Static_assert(sizeof(struct SysrootSelfInfo) == 12, "Sysroot SELF query size");
#endif

/* Manufacturing-mode query in SceSysrootForDriver (NID 0x55392965). */
SceUInt32 SceSysrootForDriver_55392965(void);	/* vitasdk: ksceSysrootUseExternalStorage */

static inline SceSysrootSharedMemory *sysrootSharedMemory(void)
{
	return (SceSysrootSharedMemory *)sceKernelSysrootGetSharedMemory();
}

/* Name unknown: returns the hardware model classification. */
uint32_t sceKernelSysrootGetKermitRevision(void);
/* Name unknown: installs a size0x10 timebase provider, or clears it. */
struct PamgrTimebaseProvider;
void SceSysrootForDriver_C94C76FA(const struct PamgrTimebaseProvider *provider);
void sceKernelSysrootDisableAutoClockDown(void);
void sceKernelSysrootEnableAutoClockDown(void);
void sceKernelSysrootCorelockLock(SceUInt32 core);
void sceKernelSysrootCorelockUnlock(void);

/* name unknown: registers the TMCP command handler (os0:kd/deci4p_tmcp.skprx) */
void SceSysrootForDriver_1834D482(SysrootTmcpCommand callback);

/* name unknown: a 64-bit timestamp */
SceUInt64 SceSysrootForDriver_273EAE53(void);

/* the hardware info word of the boot parameters (vitasdk lacks it) */
uint32_t SceSysrootForDriver_930B1342(void);	/* vitasdk: ksceSysrootGetHardwareInfo */

/* name unknown: returns the descriptor's logger callback address, or zero */
uint32_t SceSysrootForDriver_3AE319DA(void);

/* names unknown: debugger hooks (os0:kd/deci4p_userp.skprx) */
int SceSysrootForDriver_06436EF0(SceUInt32 flags, const char *pathName, const char *cmdLine, const char *workDir,
				 SceUInt32 *id, SceUInt32 *value, SceUID callbackUid, void *status,
				 int async);	/* create host process */
int SceSysrootForDriver_39F1FD07(SceUID pid);		/* is process attached */
int SceSysrootForDriver_7B4A377C(void);				/* enable watchpoint */
int SceSysrootForDriver_65F3D598(void);				/* disable watchpoint */

/* name unknown: registers the per-process tty log reader (os0:kd/bootimage/tty2uart) */
void SceSysrootForDriver_AA770EF7(SysrootTtyGetLogRaw callback);

/* names unknown (os0:kd/buserror.skprx) */
int SceSysrootForKernel_82FC6405(SceSize (*getInfo)(void *buf, SceSize size));	/* registers the bus error record reader */
int SceSysrootForKernel_B27B7530(SceUInt32 status0, SceUInt32 status1, SceUInt32 status2);

/* names unknown (os0:kd/deci4p_sdfctl.skprx): register the host's boot parameter / target status senders */
void SceSysrootForDriver_E541959B(SysrootSetBootParameter callback);
void SceSysrootForDriver_D56A2F3E(SysrootSendTargetStatus callback);

/* names unknown (os0:kd/intrmgr.skprx) */
void *SceSysrootForKernel_118657C6(void);
void sceKernelSysrootSetModulePrivate(unsigned int index, void *data);
int sceKernelSysrootTrapThreadAfterSyscall(struct ThreadSyscallFrame *frame, SceUInt32 *result,
					   SceUInt32 request);
void *sceKernelSysrootGetVbaseResetVector(void);
int SceSysrootForDriver_4373AC96(void);	/* as vitasdk declares it */

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
int SceSysrootForDriver_591BB490(SceUID pid, void *buffer, int size);
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
/* the instruction a software breakpoint replaced: 9CFF80F9 reads it (0: there is one), 38751FB7 replaces it */
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
/* the same function, SceSysrootForKernel's export */
int SceSysrootForKernel_sceKernelSysrootAppMgrSpawnProcess(const char *name, int type, const char *path, SceSize length, const void *args, void *opt, void *out);
int SceSysrootForKernel_4373AC96(void);	/* vitasdk: ksceSysrootIsBsodReboot */
/* the same function, SceSysrootForKernel's export */
int SceSysrootForKernel_sceKernelSysrootIsSafeMode(void);
int SceSysrootForKernel_B0E1FC67(void);	/* vitasdk: ksceSysrootIsUpdateMode */
int SceSysrootForKernel_79C9AE10(void);	/* vitasdk: ksceSysrootIsUsbEnumWakeup */
int sceKernelIsColdBoot(void);
/* nonzero when the model has the capability (1: multiple controller ports) */
/* vitasdk: ksceKernelSysrootCheckModelCapability */
unsigned int SceSysrootForDriver_8AA268D6(unsigned int capability);
void *sceKernelSysrootGetModulePrivate(unsigned int index);
int sceKernelSysrootGetUIDEntryHeap(SceUID processId, void **entryHeap);
uint32_t sceKernelSysrootGetStatus(void);
int sceKernelSysrootGetThreadAccessLevel(void);
/* vitasdk: ksceSysrootGetSelfAuthInfo */
int SceSysrootForKernel_4F0A4066(SceUID processId, SceSelfAuthInfo *info);
int SceSysrootForDriver_A43599E9(void);	/* vitasdk: ksceSysrootUtMgrHasNpTestFlag */
void sceKernelApiDeclareTypeInt64(void);
void sceKernelApiDeclareTypeRaw(void);
void sceKernelApiDeclareTypeUInt64(void);
void sceKernelSysrootSetVbaseResetVector(void *vector);

int sceKernelSysrootGetShellPid(void);
void SceSysrootForDriver_05CD0DD9(SysrootSetSwInfoBin callback);
void SceSysrootForDriver_0ECD711E(void);
/* vitasdk: ksceKernelSysrootSetProcessHandler */
int SceSysrootForDriver_0F07C3FC(const SysrootProcessEventOps *ops);
void SceSysrootForDriver_1434D90E(uint32_t notifier);
void SceSysrootForDriver_1FFD2AFA(SysrootSetSwInfoInt callback);
int SceSysrootForDriver_20D2A0DF(SceUID pid, SceUID modid, uint64_t time);
void SceSysrootForDriver_246AAAB3(SysrootLedSetMode callback);
int sceKernelSysrootDbgpSuspendProcessAndWaitResume(SceUID pid, SceUID modid, SceUInt32 stopReason, SceUInt64 time);
void SceSysrootForDriver_2816A813(void);
void SceSysrootForDriver_2C9E7797(void);
void SceSysrootForDriver_2D6B2A79(SysrootSendSystemError callback);
/* vitasdk: ksceKernelSysrootSetGetSystemSwVersionFunc */
void SceSysrootForDriver_3276086B(int (*callback)(void));
void SceSysrootForDriver_35875119(int (*callback)(void *buffer));
int SceSysrootForDriver_377895EB(void);
int SceSysrootForDriver_38751FB7(SceUID pid, SceUInt32 address, SceUInt32 instruction);
int SceSysrootForDriver_3999F917(const SysrootDbgpOps *ops);
int SceSysrootForDriver_39F9C968(SceUInt32 status, SceUInt32 value);
int sceKernelSysrootAppMgrSpawnProcess(const char *name, int type, const char *path, SceSize length, const void *args, void *opt, void *out);
void SceSysrootForDriver_42FC9FFF(void);
uint32_t SceSysrootForDriver_44EA3197(void);
/* vitasdk: ksceSysrootGetHardwareFlags */
int SceSysrootForDriver_46E72428(void *flags);
void SceSysrootForDriver_47F19DD3(uint32_t notifier);
uint32_t SceSysrootForDriver_483EF108(void);
uint32_t SceSysrootForDriver_49843C16(void);
uint32_t SceSysrootForDriver_4D98B15B(void);
uint32_t SceSysrootForDriver_50FE3B4D(void);	/* vitasdk: ksceSysrootUseInternalStorage */
void SceSysrootForDriver_5458BF6D(void);
int SceSysrootForDriver_56D85EB0(SceUInt64 authId);
int SceSysrootForDriver_579CC758(const SysrootDfmgrOps *ops);
int SceSysrootForDriver_5A4A7F7E(const SysrootBtOps *ops);
void SceSysrootForDriver_5A7FFDC1(void);
void SceSysrootForDriver_5B106EB3(void);
volatile uint32_t *SceSysrootForDriver_6655F48C(unsigned int bank, unsigned int index);
int SceSysrootForDriver_67AAB627(void);	/* vitasdk: ksceKernelSysrootGetSystemSwVersion */
void SceSysrootForDriver_709799BB(uint32_t notifier);
void SceSysrootForDriver_71DB83A2(int (*callback)(void));
int SceSysrootForDriver_73522F65(SceUID pid, SceUInt32 *flag);
uint32_t SceSysrootForDriver_7918D44E(void);
int SceSysrootForDriver_79C9AE10(void);
int SceSysrootForDriver_7AAC4EE7(const char *key, SceUInt32 value);
void SceSysrootForDriver_7B7ED1E9(void);
uint32_t SceSysrootForDriver_7B7F8171(void);
void SceSysrootForDriver_7C0BE393(SysrootSetSwInfoStr callback);
void SceSysrootForDriver_7C2C10E2(uint32_t phase, uint32_t kind);
void SceSysrootForDriver_82FC6405(SysrootBusErrorGetInfo callback);
int sceKernelSysrootIsSafeMode(void);
#if SYSMEM_NSKBL
void SysrootSetSelfImages(const struct SysrootSelfImage *images);
#endif
int SceSysrootForDriver_84C0BE05(SceUID pid);
void SceSysrootForDriver_8747D415(SysrootBusErrorHandler callback);
int SceSysrootForDriver_89D19090(void);
void SceSysrootForDriver_93CD44CD(uint32_t phase);	/* vitasdk: ksceKernelInvokeInitCallback */
void SceSysrootForDriver_9421B223(void);
int SceSysrootForDriver_951C7943(uint64_t deviceAddress, void *buffer, int maxCount);
void SceSysrootForDriver_9CDE3623(uint32_t notifier);
int SceSysrootForDriver_9CFF80F9(SceUID pid, void *address, SceUInt32 *opcode);
void SceSysrootForDriver_9DCD0894(SysrootCoredumpAddThread callback);
void SceSysrootForDriver_A12C9950(SysrootUtMgrHasAuthId callback);
void SceSysrootForDriver_A6A0A038(uint32_t notifier);
void SceSysrootForDriver_A7A01DF0(void);
void SceSysrootForDriver_A85FF3B2(void);
uint32_t SceSysrootForDriver_AE7A8F1D(void);
int SceSysrootForDriver_B0E1FC67(void);
void SceSysrootForDriver_B0E3E791(uint32_t notifier);
int SceSysrootForDriver_B27B7530(SceUInt32 status0, SceUInt32 status1, SceUInt32 status2);
void SceSysrootForDriver_C5EAF5F7(SysrootSysStateEvent callback);
int SceSysrootForDriver_C67A1FA9(SceUID pid, SceUID modid, int percent, const char *path);
void SceSysrootForDriver_C728BC61(void);
int SceSysrootForDriver_C81B7E2B(SceUID pid, SceUID modid, uint64_t time);
int SceSysrootForDriver_CC7A0E63(int type, SceSize size, const char *buffer);
int SceSysrootForDriver_CD4B84F7(void *buffer, SceSize size);
void SceSysrootForDriver_D02281A8(void);
int SceSysrootForDriver_D1E54BF4(uint64_t deviceAddress);
void SceSysrootForDriver_D2423D6B(void);
void SceSysrootForDriver_D7207203(void);
int SceSysrootForDriver_DABD4A47(void *threadInfo);
int SceSysrootForDriver_DC651D22(void *buffer, SceSize size, SceUInt32 *count);
void SceSysrootForDriver_E06C14FD(void);
void SceSysrootForDriver_E25D2FD5(SysrootUtMgrHasFlag callback);
void SceSysrootForDriver_E2E88E3E(SysrootUtMgrHasFlag callback);
uint32_t SceSysrootForDriver_E4EA1960(void);
void SceSysrootForDriver_EB19AAF6(void);
int SceSysrootForDriver_ED22CF8F(void *buffer, SceSize size, SceUInt32 *count);
void SceSysrootForDriver_ED688AEE(int (*callback)(void));
/* vitasdk: ksceSysrootUtMgrSetNpTestFlag */
void SceSysrootForDriver_EE5D6CE9(SysrootUtMgrHasFlag callback);
int SceSysrootForDriver_F404026C(void);
int SceSysrootForDriver_FF2DD7AB(struct SysrootDebugNotifiers *notifiers);
uint32_t SceSysrootForKernel_085C2BCB(void);
void SceSysrootForKernel_0A12227B(SceUID pid, SceUInt32 addr);
/* vitasdk: ksceSysrootGetNidName */
int SceSysrootForKernel_0B79E220(uint32_t nid, const char **name);
void SceSysrootForKernel_0DF574A9(void);
int SceSysrootForKernel_14D81797(struct SysrootProfiler *profiler);
int SceSysrootForKernel_1D8DB3A5(uint64_t *time);
void SceSysrootForKernel_21F5790B(void (*callback)(void));
int SceSysrootForKernel_sceKernelSysrootDbgpSuspendProcessAndWaitResume(SceUID pid, SceUID modid, SceUInt32 stopReason, SceUInt64 time);
int SceSysrootForKernel_26458702(const SysrootThreadmgrOps *ops);
void SceSysrootForKernel_27D30DB2(SysrootNetHook callback);
void SceSysrootForKernel_2D6B2A79(SysrootSendSystemError callback);
void *SceSysrootForKernel_2F75C1DC(SceSize size, void *opt);
uint32_t sceKernelSysrootGetErnieWakeupFactor(void);
uint32_t SceSysrootForKernel_340575CB(void);
void sceKernelSysrootSetSysroot(SceUID uid);
/* vitasdk: ksceKernelSysrootRegisterDbgpHandler */
int SceSysrootForKernel_3999F917(const SysrootDbgpOps *ops);
struct SysrootState *sceKernelSysrootGetSysroot(void);
int SceSysrootForKernel_3F76D531(SceUID pid);
int SceSysrootForKernel_40F28DC6(void *interface);
void SceSysrootForKernel_42FC9FFF(void);
int SceSysrootForKernel_4F57A13F(void *mount, void *info);
uint32_t SceSysrootForKernel_50FE3B4D(void);	/* vitasdk: ksceSysrootIsNonRemovableCardMode */
int SceSysrootForKernel_55392965(void);	/* vitasdk: ksceSysrootIsManufacturingMode */
int SceSysrootForKernel_571E5B79(SceUID parentPid, SceUID pid, int flags, void *process, uint64_t time);
void SceSysrootForKernel_59B76D23(void *value);
void SceSysrootForKernel_5B5EBFB1(volatile SceLongTimerRegisters *time);
int SceSysrootForKernel_5C86E49B(SceUID pid, SceUInt32 status, SceUInt64 time);
void *SceSysrootForKernel_5F2CE289(struct SceProcessObject *process);
void SceSysrootForKernel_611F17A4(void *context);
void *sceKernelSysrootGetCurrentAddressSpaceCB(void);
void SceSysrootForKernel_642123EB(void);
int SceSysrootForKernel_6C036B53(void *interface);
int SceSysrootForKernel_6D44F190(SceUID pid, SceUInt64 time);
/* vitasdk: ksceSysrootRegisterLicMgrGetLicenseStatus */
void SceSysrootForKernel_71DB83A2(int (*callback)(void));
int SceSysrootForKernel_733C243E(const SysrootProcessmgrOps *ops);
SceUID SceSysrootForKernel_7385CADE(void);
int SceSysrootForKernel_77CF3FBD(SceUID pid, SceUID spawnPid, SceUInt64 time);
uint32_t SceSysrootForKernel_7918D44E(void);
/* vitasdk: ksceKernelSysrootUnregisterDbgpHandler */
void SceSysrootForKernel_7B7ED1E9(void);
uint32_t SceSysrootForKernel_7B7F8171(void);	/* vitasdk: ksceKernelIsSomeBootMode */
int sceKernelSysrootIsUserModeThread(void);
/* vitasdk: ksceSysrootGetSessionId */
void *SceSysrootForKernel_84783B71(void *sessionId);
int SceSysrootForKernel_8498E23F(SceUID pid, int exitStatus, SceUInt64 time);
int SceSysrootForKernel_84C0BE05(SceUID pid);
void SceSysrootForKernel_8747D415(SysrootBusErrorHandler callback);
int SceSysrootForKernel_89532E2F(void);
int SceSysrootForKernel_89D19090(void);	/* vitasdk: ksceSysrootIsExternalBootMode */
void SceSysrootForKernel_9A8871F1(SysrootAppMgrSpawnProcess callback);
int SceSysrootForKernel_9CFF80F9(SceUID pid, void *address, SceUInt32 *opcode);
int SceSysrootForKernel_9D058A32(SceUID pid, int exitStatus, uint64_t time);
void SceSysrootForKernel_A799F22A(SysrootNetHook callback);
int SceSysrootForKernel_A9510EFE(void *interface);
uint32_t SceSysrootForKernel_AB3CC7D0(void);
void *SceSysrootForKernel_AD74BEB3(struct SceProcessObject *process);
int SceSysrootForKernel_AE2132FD(void *ptr);
uint32_t SceSysrootForKernel_AE7A8F1D(void);
int SceSysrootForKernel_B171CC2D(const SysrootModulemgrOps *ops);
void *sceKernelSysrootGetCurrentUIDEntryHeapCB(void);
int SceSysrootForKernel_B501E861(SceUID pid, uint64_t time);
int SceSysrootForKernel_BF82931F(int appId, void *out);
void SceSysrootForKernel_C5EAF5F7(SysrootSysStateEvent callback);
void SceSysrootForKernel_C728BC61(void);
int SceSysrootForKernel_CA8D1A2B(uint32_t a0, uint32_t a1, uint32_t a2);
void SceSysrootForKernel_CC119171(SysrootPfsMgrGetMountInfo callback);
int SceSysrootForKernel_CC7A0E63(int type, SceSize size, const char *buffer);
int SceSysrootForKernel_CCB8BB5E(void *interface);
int SceSysrootForKernel_CD4B84F7(void *buffer, SceSize size);
int SceSysrootForKernel_CD8B5D37(uint32_t a0, uint32_t a1, uint32_t a2);
void SceSysrootForKernel_D02281A8(void);
int SceSysrootForKernel_D29BCA77(const SysrootProcessmgrRuntimeOps *ops);
void *sceKernelSysrootAllocRemoteProcessHeap(SceUID pid, SceSize size, void *opt);
uint32_t SceSysrootForKernel_D3872270(void);	/* vitasdk: ksceSysrootGetFactorySystemSwVersion */
SceUID SceSysrootForKernel_D441DC34(void);
void SceSysrootForKernel_D4457D4F(SysrootNetHook callback);
void SceSysrootForKernel_D7207203(void);
int SceSysrootForKernel_DD7821AA(uint32_t value);
uint32_t SceSysrootForKernel_E4EA1960(void);
/* vitasdk: ksceKernelSysrootGetProcessTitleId */
int SceSysrootForKernel_EC3124A3(SceUID processId, char *name, SceSize size);
void SceSysrootForKernel_ECADF8C3(SysrootNetHook callback);
SceSize SceSysrootForKernel_EEB867C0(void);
/* vitasdk: ksceSysrootGetSelfInfo */
int SceSysrootForKernel_F10AB792(uint32_t index, struct SysrootSelfInfo *info);
int SceSysrootForKernel_F2BC9E1D(SceUID pid);
int SceSysrootForKernel_F4340469(SceUID parentPid, SceUID pid, SceUInt64 time);
int sceKernelSysrootIofilemgrStart(void);
/* vitasdk: ksceKernelSysrootCheckRemapCodeForUser */
int SceSysrootForKernel_F8769E86(SceUID pid);
int SceSysrootForKernel_FF44A7BA(uint32_t a0, uint32_t a1, uint32_t a2);
/* vitasdk: ksceSysrootGetModuleInfoForPid */
int SceSysrootForKernel_FF9F80FF(SceUID pid, SceUInt32 addr, struct ModulemgrStubInfo *info);
uint32_t SceSysrootForKernel_FFD6E24D(void);
void sceKernelApiDeclareErrorMsg(uint32_t message);
void sceKernelApiDeclareTypeInt32(void);
void sceKernelApiDeclareTypeUInt32(void);
int sceKernelSysrootBacktrace(SceUID thid, struct SceKernelBacktraceFrameForKernel *frames, SceSize size, SceSize *count, SceUInt32 flags);
int sceKernelSysrootCoredumpCancel(int arg);
int sceKernelSysrootCoredumpTrigger(SceUID pid,
	SceKernelCoredumpStateUpdateCallback update,
	SceKernelCoredumpStateFinishCallback finish,
	SceCoredumpTriggerParam *param);
uint32_t sceKernelSysrootGetCurrentProcessCB(void);
int sceKernelSysrootGetCurrentThreadAttr(uint32_t *attr);
void *sceKernelSysrootGetProcessCBExitDeleteThread(struct SceProcessObject *process);
int sceKernelSysrootPrintBacktrace(SceUID pid, const struct SceKernelBacktraceFrameForKernel *frames, SceSize count, SceUInt32 start);
int sceKernelSysrootProcessmgrStart2(void);
void sceKernelSysrootReturnFromExcpToThread(void *context, void *thread);
void sceKernelSysrootSetSharedMemory(void *base);
struct SceKblParam *sceKernelSysrootGetKblParam(void);
/* vitasdk: ksceKernelSysrootRegisterCoredumpTrigger */
void SceSysrootForDriver_DD473B05(SysrootCoredumpTrigger callback);
void sceKernelSysrootSetStatus(uint32_t status);
int sceKernelSysrootThreadMgrStartAfterProcess(void);

/* the secure kernel's SceSysrootForTZS (the names of 0.990's import stubs) */
unsigned int sceKernelSysrootGetErnieWakeupFactor(void);
unsigned int *sceKernelSysrootGetVbaseMonitorVector(void);	/* the monitor vector page */
const unsigned int *sceKernelSysrootGetKernelMMUContext(void);
unsigned int SceSysrootForTZS_317E7556(unsigned int cpu);	/* a core's block */
unsigned int SceSysrootForTZS_D6FBF445(void);	/* the model (the henkaku wiki's guess) */
void *SceSysrootForTZS_CC36643A(void *sessionId);	/* SceSysrootForKernel_84783B71 */
void *SceSysrootForTZS_32546BDB(void *key);	/* the KBL's coredump session key, 32 bytes */
