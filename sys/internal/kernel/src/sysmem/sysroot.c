/* Sysroot state, descriptor and shared time services. */
#include <stddef.h>
#include <stdint.h>
#include <sce/kbl_param.h>
#include <sce/timer_regs.h>
#include <psp2kern/kernel/cpu/corelock.h>
#include "fw.h"
#include <psp2kern/coredump.h>
#include <sce/debug.h>
#include <sce/error.h>
#include <sce/threadcb.h>
#include <sce/sysroot_processmgr.h>
#include <sce/sysroot_modulemgr.h>
#include <sce/sysroot_pamgr.h>
#include <sce/sysroot_dfmgr.h>
#include <sce/sysroot_hid.h>
#include <sce/sysroot_bt.h>
#include <sce/sysroot_acmgr.h>
#include <sce/sysroot_process_events.h>
#include <sce/sysroot_dbgp.h>
#include <sce/sysroot_threadmgr.h>
#include <sce/sysroot_init.h>
#include <sce/sysroot_netps.h>
#include <sce/sysroot_appmgr.h>
#include "sysmem_boot.h"
#if SYSMEM_NSKBL
#include <string.h>
#include <sce/kernel_boot_param.h>
#endif


struct SysrootInitCallbackSlot {
	SysrootInitCallback callback;
	void *argument;
};

extern int ksceKernelSpinlockLowLockCpuSuspendIntr(volatile uint32_t *lock);
extern void ksceKernelSpinlockLowUnlockCpuResumeIntr(volatile uint32_t *lock,
						   uint32_t flags);

struct SysrootSelfImage {
	void *data;
	SceSize size;
};

/* Native twelve-byte query record, matching SceKernelSysrootSelfInfo. */
struct SysrootSelfInfo {
	SceSize size;
	void *selfData;
	SceSize selfSize;
};

_Static_assert(sizeof(struct SysrootSelfImage) == 8, "Sysroot SELF image entry size");
_Static_assert(sizeof(struct SysrootSelfInfo) == 12, "Sysroot SELF query size");

/*
 * The debugger's event notifiers (deci4p_sdbgp registers them): each sends
 * the host one event. Exported to the modules that raise them.
 */
struct SysrootDebugNotifiers {
	uint32_t size;
	uint32_t vipRpc;		/* event 40 */
	uint32_t veneziaRpc;		/* event 41 */
	uint32_t syscon;		/* event 52, syscon's logger */
	uint32_t appMgr;		/* event 51 */
	uint32_t power[2];		/* event 49 */
};

struct SysrootWordBlock128 {
	uint32_t words[4];
} __attribute__((may_alias));

/* the debugger's profiling timers (deci4p_sdbgp): all reset, one started, one stopped */
struct SysrootProfiler {
	int (*reset)(uint32_t, uint32_t, uint32_t);
	int (*start)(uint32_t, uint32_t, uint32_t);
	int (*stop)(uint32_t, uint32_t, uint32_t);
};

/* a CPU's block the exception vectors are given (excpmgr); sysmem never looks inside */
struct SysrootPerCpu {
	uint8_t data[0x20];
};

/* Services other modules register in the state. */
typedef int (*SysrootBusErrorHandler)(SceUInt32 status0, SceUInt32 status1,
				      SceUInt32 status2);
typedef SceSize (*SysrootBusErrorGetInfo)(void *buffer, SceSize size);
typedef int (*SysrootCoredumpAddThread)(void *threadInfo);
typedef int (*SysrootUtMgrHasAuthId)(SceUInt64 authId);
typedef int (*SysrootLedSetMode)(SceUInt32 led, SceUInt32 mode, const void *option);
typedef int (*SysrootSysStateEvent)(int event);
typedef int (*SysrootTmcpCommand)(SceUInt32 flags, const char *pathName,
				  const char *cmdLine, const char *workDir, SceUInt32 *id,
				  SceUInt32 *value, SceUID callbackUid, void *status, int async);
typedef int (*SysrootPfsMgrGetMountInfo)(void *mount, void *info);
typedef int (*SysrootCoredumpTrigger)(SceUID pid, SceKernelCoredumpStateUpdateCallback update,
				      SceKernelCoredumpStateFinishCallback finish,
				      SceCoredumpTriggerParam *param);

struct SysrootState {
	void *object;				/* the UID object header */
	void *cls;
	uint32_t size;
	uint32_t magic;
	volatile uint32_t bootHeapLock;
	SceSize bootHeapCapacity;
	uintptr_t bootHeapCursor;
	uint32_t perCpuCount;			/* 0x1c */
	struct SysrootPerCpu *perCpu;		/* 0x20: one per CPU */
	uint32_t bootFlags;
	uint32_t status;
	SceCorelockContext corelock;		/* 0x2c */
	void *modulePrivate[14];
	SceKblParam *kblParam;
	struct SceKernelBootParam *bootArgs;	/* 0x70: cleared after boot */
	SceUInt32 factorySystemSwVersion;
	SceUInt32 pervasiveDword;		/* 0x78: a pervasive register the bootloader read */
	SceUInt32 kermitRevision;		/* 0x7c */
	union {
		uint32_t modelInfo;		/* 0x80 */
		struct {
			uint16_t modelInfoLow;
			uint16_t model;		/* 1 PS Vita, 2 PS TV */
		};
	};
	volatile uint32_t modelCapability;
	volatile SceLongTimerRegisters *sharedTime;
	volatile uint32_t initCallbackLock;
	uint32_t initCallbackBase;
	struct SysrootInitCallbackSlot initCallbacks[9][8];
	int (*threadMgrStartAfterProcess)(void);
	int (*iofilemgrStart)(void);
	uint32_t reserved1;
	void *fallbackPUIDEntryHeap;
	char *memBlockNames[9];		/* 0x2e4 */
	SceUID sysrootUid;			/* 0x308 */
	uint32_t sysrootClass;			/* 0x30c */
	void *fallbackAddressSpaceCB;
	uint32_t intrMMUContext;		/* 0x314 */
	uint32_t vbaseResetVector;
	uint32_t mvbar;
	uint32_t sharedMemory;
	const SysrootAcmgrOps *acmgrOps;
	SysrootAppMgrSpawnProcess appMgrSpawnProcess;
	int (*getShellPid)(void);
	SceUID (*getThreadId)(void);
	int (*isUserModeThread)(void);
	SceUID (*getProcessId)(void);
	uint32_t (*getCurrentProcessCB)(void);
	void *(*getCurrentAddressSpaceCB)(void);
	const SysrootModulemgrOps *modulemgrOps;
	void *(*getCurrentUIDEntryHeapCB)(void);
	int (*returnFromExcpToThread)(void);
	int (*getThreadAccessLevel)(void);
	int (*trapThreadAfterSyscall)(void);
	int (*getCurrentThreadAttr)(uint32_t *attr);
	uint32_t registeredPid;			/* 0x35c: DD7821AA sets, 340575CB returns */
	const SysrootProcessmgrRuntimeOps *processmgrRuntimeOps;
	const SysrootProcessmgrOps *processmgrOps;
	int (*licMgrGetLicenseStatus)(void);
	int (*utMgrHasPsmTestFlag)(void);
	int (*utMgrHasSystemDataFilePlayReadyFlag)(void);
	SysrootBusErrorHandler busErrorHandler;	/* 0x374: deci4p_sdfmgr's, buserror calls it */
	SysrootBusErrorGetInfo busErrorGetInfo;	/* 0x378: buserror's record reader */
	SysrootCoredumpTrigger coredumpTrigger;
	int (*coredumpCancel)(int arg);
	SysrootCoredumpAddThread coredumpAddThread;	/* 0x384: coredump's, for each thread of a faulting process */
	int (*ttyGetLogRaw)(int magic, void *buffer, SceSize size);	/* tty2uart's */
	SysrootUtMgrHasAuthId utMgrHasAuthId;	/* 0x38c: post_ss_mgr's, acmgr asks */
	int (*utMgrGetTrilithiumBuffer)(void *buffer);
	int (*utMgrHasNpTestFlag)(void);
	int (*getSystemSwVersion)(void);
	SysrootLedSetMode ledSetMode;		/* 0x39c: power's ksceLedSetMode */
	const struct SysrootSelfImage *selfImages;
	int (*getNidName)(uint32_t nid, const char **name);
	SysrootSysStateEvent sysStateEvent;	/* 0x3a8: deci4p_(s)dfmgr's, sysstatemgr reports boot */
	int (*sendSystemError)(int type, SceSize size, const char *buffer);	/* deci4p_sdfmgr's */
	const SysrootDfmgrOps *dfmgrOps;
	int (*setSwInfoInt)(const char *name, SceSize length, SceUInt32 number);
	int (*setSwInfoStr)(const char *name, SceSize nameLength, const char *str,
			    SceSize strLength);
	int (*setSwInfoBin)(const char *name, SceSize nameLength, const void *data,
			    SceSize dataSize);
	int (*syncLocalTime)(void);		/* 0x3c0: deci4p_tsmp's */
	const SysrootProcessEventOps *processEventOps;
	const SysrootDbgpOps *dbgpOps;
	SysrootNetHook dbgpNetAttach;		/* 0x3cc: net_ps's debug hooks, for deci4p_dbgp */
	SysrootNetHook dbgpNetDetach;
	SysrootTmcpCommand tmcpCommand;		/* 0x3d4: deci4p_tmcp's command handler */
	void (*debugHook)(void);		/* 0x3d8: deci4p_sdbgp registers NULL */
	SysrootNetHook sdbgpNetAttach;		/* 0x3dc: the same net_ps hooks, for deci4p_sdbgp */
	SysrootNetHook sdbgpNetDetach;
	int (*appMgrGetAppInfoByPid)(int pid, void *out);	/* 0x3e4: appmgr's, for debuggers */
	int (*appMgrGetAppInfoByAppId)(int appId, void *out);
	int (*appMgrGetAppList)(void *out);
	SysrootPfsMgrGetMountInfo pfsMgrGetMountInfo;	/* 0x3f0: pfsmgr's */
	const SysrootBtOps *btOps;
	const SysrootHidOps *hidOps;
	struct SysrootDebugNotifiers *debugNotifiers;	/* 0x3fc */
	struct SysrootProfiler *profiler;		/* 0x400 */
	int (*setBootParameter)(const char *, const char *);
	int (*sendTargetStatus)(uint32_t, uint32_t);
	unsigned int (*getTimebaseFrequency)(void);
	uint64_t (*getTimebaseValue)(void);
	volatile uint32_t *(*getTimebaseRegister)(unsigned int bank, unsigned int index);
	uint32_t magicEnd;
};

/* the AutoClockDown idle context lowio's idle loop uses */
struct SysrootAutoClockDownState {
	uint32_t armBaseClkConfig[2];		/* backup of the ARM base clock registers */
	uint32_t disableCountAndFlags;
};

struct SysrootAutoClockDownState SceSysrootForDriver_4B8C305A = { 0 };
/* PLACEHOLDER: meaning of the secondary native Sysroot word618. */
static uint32_t s_sysrootClass;
extern int SceDebugForKernel_0EDA5174(uint32_t value);

_Static_assert(sizeof(struct SysrootState) == 0x41c, "Sysroot state size");
_Static_assert(offsetof(struct SysrootState, modulePrivate) == 0x34,
	       "Sysroot module-private pointer table offset");
_Static_assert(offsetof(struct SysrootState, modelCapability) == 0x84,
	       "Sysroot model capability word offset");
_Static_assert(offsetof(struct SysrootState, processmgrOps) == 0x364,
	       "Sysroot process-manager services offset");
_Static_assert(offsetof(struct SysrootState, getThreadAccessLevel) == 0x350,
	       "Sysroot thread access-level service offset");
_Static_assert(offsetof(struct SysrootAutoClockDownState, disableCountAndFlags) == 8,
	       "Sysroot automatic clock-down counter offset");

_Static_assert(offsetof(struct SysrootState, kblParam) == 0x6c,
	       "Sysroot KBL parameter pointer offset");
_Static_assert(offsetof(struct SysrootState, sharedTime) == 0x88,
	       "Sysroot shared time pointer offset");

/* Native BSS starts at NULL; the module startup caller remains unimplemented. */
#if SYSMEM_NSKBL
static struct SysrootState *s_sysrootState;
#else
static struct SysrootState *volatile s_sysrootState;
#endif

#if SYSMEM_NSKBL
#define SYSROOT_MAGIC1	0xba97f5a1
#define SYSROOT_MAGIC2	0xa008b0c3

#if FW_DEBUG
int AssertSysroot(const struct SysrootState *pSysroot)
{
	if (pSysroot->magic != SYSROOT_MAGIC1) {
		SCE_DBG_PRINTF(0xb, 97, "pSysroot->magic1=0x%08x\n", pSysroot->magic);
		SCE_DBG_ASSERT_FAIL(98);
	}
	if (pSysroot->magicEnd != SYSROOT_MAGIC2) {
		SCE_DBG_PRINTF(0xb, 101, "pSysroot->magic2=0x%08x\n", pSysroot->magicEnd);
		SCE_DBG_ASSERT_FAIL(102);
	}
	return 0;
}
#endif /* FW_DEBUG */
#endif

void *sceKernelSysrootAlloc(SceSize size)
{
	struct SysrootState *state = s_sysrootState;
	uint32_t flags;
	uintptr_t cursor, end;

	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->bootHeapLock);
	cursor = state->bootHeapCursor;
	end = cursor + size;
	if (end < (uintptr_t)state + state->bootHeapCapacity) {
		state->bootHeapCursor = (end + 0x1f) & ~0x1fU;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sysrootState->bootHeapLock, flags);
		if (state->status > 2)
			SCE_DBG_PRINTF(0xb, 144,
				"Allocated after kernel heap ready: ptr=%p,size=%d,lr=%p\n",
				(void *)cursor, size, __builtin_return_address(0));
		return (void *)cursor;
	}
	SCE_DBG_PRINTF(0xb, 130, "vheap=0x%08x + 0x%08x > %p + 0x%08x\n",
		      cursor, size, state, state->bootHeapCapacity);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sysrootState->bootHeapLock, flags);
	SCE_DBG_ASSERT_FAIL(132);
	return NULL;
}

void sceKernelSysrootSetStatus(uint32_t status)
{
	struct SysrootState *state = s_sysrootState;
	uint32_t flags;

	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->bootHeapLock);
	state = s_sysrootState;
	if (state->status < status)
		state->status = status;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->bootHeapLock, flags);
}

#if !SYSMEM_NSKBL
int SceSysrootForKernel_BE1EF51C(const SysrootAcmgrOps *ops)
{
	s_sysrootState->acmgrOps = ops;
	return 0;
}

int ksceKernelSysrootCheckRemapCodeForUser(SceUID pid)
{
	const SysrootAcmgrOps *ops = s_sysrootState->acmgrOps;

	return ops ? ops->checkRemapCodeForUser(pid) : 0;
}

int SceSysrootForDriver_51F9C118(SceUID pid, SceUID spawnPid, SceUID prevPid,
				 uint64_t time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	/* Native dispatch preserves r0 when no table is registered. */
	return ops ? ops->exitSpawn(pid, spawnPid, prevPid, time) : pid;
}

int SceSysrootForDriver_582616EC(SceUID pid, SceUID spawnPid, SceUID prevPid,
				 uint64_t time)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->exitSpawn(pid, spawnPid, prevPid, time) : pid;
}

int ksceKernelSysrootSetProcessHandler(const SysrootProcessEventOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->processEventOps = ops;
	return 0;
}

int SceSysrootForKernel_ksceKernelSysrootRegisterDbgpHandler(const SysrootDbgpOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->dbgpOps = ops;
	return 0;
}

int SceSysrootForDriver_571E5B79(SceUID parentPid, SceUID pid, int flags,
				 void *process, uint64_t time)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	/* Native r3 is overwritten with the callback table before dispatch. */
	(void)process;
	return ops ? ops->processStart(parentPid, pid, flags, time) : parentPid;
}

int SceSysrootForDriver_9D058A32(SceUID pid, int exitStatus, uint64_t time)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->processExit(pid, exitStatus, time) : pid;
}

int SceSysrootForDriver_B501E861(SceUID pid, uint64_t time)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->processKill(pid, time) : pid;
}

int SceSysrootForDriver_C67A1FA9(SceUID pid, SceUID modid, int percent,
				 const char *path)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->loadProgress(pid, modid, percent, path) : pid;
}

int SceSysrootForDriver_C81B7E2B(SceUID pid, SceUID modid, uint64_t time)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->moduleStart(pid, modid, time) : pid;
}

int SceSysrootForDriver_20D2A0DF(SceUID pid, SceUID modid, uint64_t time)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->moduleStop(pid, modid, time) : pid;
}

int SceSysrootForDriver_421EFC96(SceUID pid)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->processQuery(pid) : SCE_ERROR_ERRNO_ENOTSUP;
}

int SceSysrootForDriver_1D61C837(SceUID pid, SceUID spawnPid, uint64_t time)
{
	const SysrootProcessEventOps *ops = s_sysrootState->processEventOps;

	return ops ? ops->processExitToSpawn(pid, spawnPid, time) : pid;
}

/* the current thread's syscall frame (ThreadCB.userFrame), 0 outside a thread */
uint32_t SceSysrootForDriver_44EA3197(void)
{
	const ThreadCB *thread = getCurrentThreadCB();

	return thread ? (uint32_t)thread->userFrame : 0;
}

int SceSysrootForKernel_CBD377B7(int (*callback)(void))
{
	s_sysrootState->iofilemgrStart = callback;
	return 0;
}

int SceSysrootForKernel_DD7821AA(uint32_t value)
{
	s_sysrootState->registeredPid = value;
	return 0;
}

SceSize SceSysrootForKernel_EEB867C0(void)
{
	return sizeof(struct SysrootState);
}

int SceSysrootForDriver_F804F761(void)
{
	return 0;
}
#endif

__attribute__((noinline))
#if !SYSMEM_NSKBL
void InvokeInitCallback(uint32_t phase, uint32_t kind)
{
	struct SysrootState *state;
	uint32_t flags, current;

#if FW_DEBUG
	if (phase > 8) {
		SCE_DBG_ASSERT_FAIL(2735);
		return;
	}
	if (kind > 1) {
		SCE_DBG_ASSERT_FAIL(2741);
		return;
	}
#endif
	state = s_sysrootState;
	if (phase <= state->initCallbackBase) {
		SCE_DBG_ASSERT_FAIL(2747);
		return;
	}
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->initCallbackLock);
	state = s_sysrootState;
	for (current = state->initCallbackBase + 1; current <= phase; current++) {
		for (unsigned int slot = 0; slot < 8; slot++) {
			struct SysrootInitCallbackSlot *entry = &state->initCallbacks[current][slot];

			if (!entry->callback)
				break;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->initCallbackLock, flags);
			/* The native invocation reloads both fields after unlocking. */
			entry->callback(kind, entry->argument);
			state = s_sysrootState;
			flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->initCallbackLock);
			state = s_sysrootState;
		}
		state->initCallbackBase++;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->initCallbackLock, flags);
}

void ksceKernelInvokeInitCallback(uint32_t phase)
{
#if !FW_DEBUG
	if (phase > 8)
		return;
#endif
	InvokeInitCallback(phase, 0);
}

void SceSysrootForDriver_7C2C10E2(uint32_t phase, uint32_t kind)
{
#if !FW_DEBUG
	if (phase > 8 || kind > 1)
		return;
#endif
	InvokeInitCallback(phase, kind);
}
#endif

int sceKernelRegisterInitCallback(SysrootInitCallback callback, uint32_t phase,
				  void *argument, int *result)
{
	struct SysrootInitCallbackSlot entry = { callback, argument };
	struct SysrootState *state;
	uint32_t flags, slot;
	int status;

	if (!callback || phase > 8)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_sysrootState->initCallbackLock);
	state = s_sysrootState;
	if (phase < state->initCallbackBase) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->initCallbackLock, flags);
		status = callback(0, argument);
		if (result)
			*result = status;
		return 0;
	}
	for (slot = 0; slot < 8; slot++) {
		if (!state->initCallbacks[phase][slot].callback) {
			state->initCallbacks[phase][slot] = entry;
			status = 0;
			goto unlock;
		}
	}
	status = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, 0, 2719);
		state = s_sysrootState;
	}
#endif
unlock:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->initCallbackLock, flags);
	return status;
}

#if !SYSMEM_NSKBL
int ksceSysrootGetSelfInfo(uint32_t index, struct SysrootSelfInfo *info)
{
	const struct SysrootSelfImage *images;
	SceSize size;

	if (index > 2)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!info)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	images = s_sysrootState->selfImages;
	if (!images)
		return SCE_ERROR_ERRNO_ENOMEM;
	size = images[index].size;
	if (!size)
		return SCE_ERROR_ERRNO_ENOENT;
	info->selfData = images[index].data;
	info->selfSize = size;
	return 0;
}

void *ksceSysrootGetSessionId(struct SysrootWordBlock128 *sessionId)
{
	/* Native KBL byte arrays are word-aligned and read completely before stores. */
	const struct SysrootWordBlock128 *source =
		(const void *)s_sysrootState->kblParam->session_id;
	uint32_t word0 = source->words[0], word1 = source->words[1];
	uint32_t word2 = source->words[2], word3 = source->words[3];

	sessionId->words[0] = word0;
	sessionId->words[1] = word1;
	sessionId->words[2] = word2;
	sessionId->words[3] = word3;
	return sessionId;
}

int ksceSysrootGetHardwareFlags(struct SysrootWordBlock128 *flags)
{
	const struct SysrootWordBlock128 *source =
		(const void *)s_sysrootState->kblParam->hardware_flags;
	uint32_t word0 = source->words[0], word1 = source->words[1];
	uint32_t word2 = source->words[2], word3 = source->words[3];

	flags->words[0] = word0;
	flags->words[1] = word1;
	flags->words[2] = word2;
	flags->words[3] = word3;
	return 0;
}

int SceSysrootForKernel_ksceSysrootIsUsbEnumWakeup(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	if (kbl->poweroff_factor & SCE_KBL_POWEROFF_USB_ENUM)
		return 1;
	return (kbl->wakeup_factor & 0x7f) == 0x0f;
}

int SceSysrootForKernel_ksceSysrootIsSafeMode(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;
	uint32_t wakeupFactor, releasedControls;

	if (kbl->boot_flags[3] != 0xff)
		return 1;
	wakeupFactor = kbl->wakeup_factor & 0x7f;
	if (wakeupFactor != 0x0b) {
		if (wakeupFactor != 4)
			return wakeupFactor == 0x1f || wakeupFactor - 0x18 <= 1;
		if (!kbl->suspendinfo_adr)
			return 0;
	}
	releasedControls = ~kbl->hold_ctrl;
	if (((releasedControls >> 8) & 0x54) != 0x54)
		return 0;
	return (releasedControls & 0xc0) == 0;
}

int ksceKernelSysrootGetThreadAccessLevel(void)
{
	int (*getThreadAccessLevel)(void) = s_sysrootState->getThreadAccessLevel;

	return getThreadAccessLevel ? getThreadAccessLevel() : 0;
}

int SceSysrootForKernel_26458702(const SysrootThreadmgrOps *ops)
{
	struct SysrootState *state;

	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	state = s_sysrootState;
	state->getThreadAccessLevel = ops->getThreadAccessLevel;
	state->getCurrentAddressSpaceCB = ops->getCurrentAddressSpaceCB;
	state->getCurrentUIDEntryHeapCB = ops->getCurrentUIDEntryHeapCB;
	state->returnFromExcpToThread = ops->returnFromExcpToThread;
	state->getThreadId = ops->getThreadId;
	state->isUserModeThread = ops->isUserModeThread;
	state->threadMgrStartAfterProcess = ops->threadMgrStartAfterProcess;
	state->getCurrentThreadAttr = ops->getCurrentThreadAttr;
	state->getProcessId = ops->getProcessId;
	state->getCurrentProcessCB = ops->getCurrentProcessCB;
	state->trapThreadAfterSyscall = ops->trapThreadAfterSyscall;
	return 0;
}

void *ksceKernelSysrootGetCurrentAddressSpaceCB(void)
{
	struct SysrootState *state = s_sysrootState;
	void *(*getCurrentAddressSpaceCB)(void) = state->getCurrentAddressSpaceCB;

	return getCurrentAddressSpaceCB ? getCurrentAddressSpaceCB() :
		state->fallbackAddressSpaceCB;
}

SceUID SceSysrootForKernel_7385CADE(void)
{
	SceUID (*getProcessId)(void) = s_sysrootState->getProcessId;

	return getProcessId ? getProcessId() : SCE_KERNEL_PROCESS_ID;
}

int ksceKernelSysrootIsUserModeThread(void)
{
	int (*isUserModeThread)(void) = s_sysrootState->isUserModeThread;

	return isUserModeThread ? isUserModeThread() : 0;
}

uint32_t sceKernelSysrootGetCurrentProcessCB(void)
{
	uint32_t (*callback)(void) = s_sysrootState->getCurrentProcessCB;

	return callback ? callback() : 0;
}

void *ksceKernelSysrootGetCurrentUIDEntryHeapCB(void)
{
	void *(*getCurrentUIDEntryHeapCB)(void) = s_sysrootState->getCurrentUIDEntryHeapCB;

	return getCurrentUIDEntryHeapCB ? getCurrentUIDEntryHeapCB() : NULL;
}

SceUID SceSysrootForKernel_D441DC34(void)
{
	SceUID (*getThreadId)(void) = s_sysrootState->getThreadId;

	return getThreadId ? getThreadId() : -1;
}

int SysrootModuleStart(const struct SysmemBootContext *context)
{
	struct SysrootState *state = context->sysroot;

	s_sysrootState = state;
	if (state->size != sizeof(*state)) {
		SCE_PRINTF_WITH_INFO(0, 962,
			"Sysroot MISMATCH 0x%08x != 0x%08x (expect)\n",
			state->size, (unsigned int)sizeof(*state));
		state = s_sysrootState;
	}
	s_sysrootClass = state->sysrootClass;
	SceSysrootForDriver_4B8C305A.disableCountAndFlags =
		(state->kblParam->boot_type_indicator_1 & 4) ? 0x14 : 4;
	SceDebugForKernel_0EDA5174((uintptr_t)state->modulePrivate[2]);
	return 0;
}

struct SysrootPerCpu *SceSysrootForKernel_06182D59(unsigned int cpu)
{
	return &s_sysrootState->perCpu[cpu];
}

uint32_t SceSysrootForKernel_085C2BCB(void)
{
	return s_sysrootClass;
}

void *ksceKernelSysrootGetModulePrivate(unsigned int index)
{
	return s_sysrootState->modulePrivate[index];
}
#endif

void sceKernelSysrootSetModulePrivate(unsigned int index, void *data)
{
	s_sysrootState->modulePrivate[index] = data;
}

#if !SYSMEM_NSKBL
void SceSysrootForKernel_5B5EBFB1(volatile SceLongTimerRegisters *time)
{
	s_sysrootState->sharedTime = time;
}

unsigned int ksceKernelSysrootCheckModelCapability(unsigned int capability)
{
	/* the shift by register takes the low byte; 32 and up give 0 */
	return (s_sysrootState->modelCapability >> capability) & 1;
}

int sceKernelCheckModelCapability(unsigned int capability)
{
	switch (capability) {
	case 7:
	case 10:
		return (s_sysrootState->modelCapability >> capability) & 1;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
}
#endif

#if !SYSMEM_NSKBL
/* PLACEHOLDER: unnamed capability/attribute bits in the boot-process template. */
static const SceSelfAuthInfo s_fallbackSelfAuthInfo = {
	.program_authority_id = UINT64_C(0x2808000000000001),
	.capability = {
		[0] = 0x80, [4] = 0xc0, [6] = 0xf0,
		[12] = 0xff, [13] = 0xff, [14] = 0xff, [15] = 0xff,
	},
	.attribute = {
		[0] = 0x80, [1] = 0x09, [2] = 0x80, [3] = 0x03,
		[6] = 0xc3, [10] = 0x80, [11] = 0x09, [12] = 0x80,
		[28] = 0xff, [29] = 0xff, [30] = 0xff, [31] = 0xff,
	},
};
#endif

extern void *memcpy(void *destination, const void *source, size_t size);

#if !SYSMEM_NSKBL
int SceSysrootForKernel_733C243E(const SysrootProcessmgrOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->processmgrOps = ops;
	return 0;
}

int ksceSysrootGetSelfAuthInfo(SceUID processId, SceSelfAuthInfo *info)
{
	const SysrootProcessmgrOps *ops = s_sysrootState->processmgrOps;

	if (ops)
		return ops->getSelfAuthInfo(processId, info);
	memcpy(info, &s_fallbackSelfAuthInfo, sizeof(*info));
	return 0;
}

int ksceKernelSysrootGetPUIDEntryHeap(SceUID processId, void **entryHeap)
{
	struct SysrootState *state = s_sysrootState;
	const SysrootProcessmgrOps *ops = state->processmgrOps;

	if (ops)
		return ops->getUIDEntryHeap(processId, entryHeap);
	*entryHeap = state->fallbackPUIDEntryHeap;
	return 0;
}

SceUID SceSysrootForKernel_7334F1E8(SceUID processId)
{
	const SysrootProcessmgrOps *ops = s_sysrootState->processmgrOps;

	return ops ? ops->getAddressSpace(processId) : SCE_KERNEL_ADDRESS_SPACE_ID;
}

int ksceKernelSysrootGetProcessTitleId(SceUID processId, char *name, SceSize size)
{
	const SysrootProcessmgrOps *ops = s_sysrootState->processmgrOps;

	return ops ? ops->getName(processId, name, size) : SCE_KERNEL_ERROR_ERROR;
}

/* Names unknown; these dispatch the process-manager resource counters. */
int SysrootIncProcessResource(SceUInt32 type)
{
	const SysrootProcessmgrOps *ops = s_sysrootState->processmgrOps;

	return ops ? ops->incResource(type) : 0;
}

int SysrootDecProcessResource(SceUInt32 type)
{
	const SysrootProcessmgrOps *ops = s_sysrootState->processmgrOps;

	return ops ? ops->decResource(type) : 0;
}

int SysrootPIDtoAddressSpaceCB(SceUID processId, void **addressSpaceCB)
{
	struct SysrootState *state = s_sysrootState;
	const SysrootProcessmgrOps *ops = state->processmgrOps;

	if (ops)
		return ops->pidToAddressSpaceCB(processId, addressSpaceCB);
	*addressSpaceCB = state->fallbackAddressSpaceCB;
	return 0;
}

/* Name unknown. The high word brackets each low-word read. */
static uint64_t ReadSysrootTimeStable(void)
{
	volatile SceLongTimerRegisters *time = s_sysrootState->sharedTime;
	uint32_t low, high;

	do {
		high = time->countHi;
		low = time->countLo;
	} while (high != time->countHi);
	return (uint64_t)high << 32 | low;
}

int SceSysrootForKernel_1D8DB3A5(uint64_t *time)
{
	if (!time)
		return (int)SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	*time = ReadSysrootTimeStable();
	return 0;
}

uint64_t SceSysrootForKernel_E20F6FC8(void)
{
	return ReadSysrootTimeStable();
}

uint32_t SceSysrootForKernel_41636522(void)
{
	return s_sysrootState->sharedTime->countLo;
}
#endif

_Static_assert(offsetof(struct SysrootState, sysrootUid) == 0x308,
	       "Sysroot UID offset");
_Static_assert(offsetof(struct SysrootState, debugNotifiers) == 0x3fc,
	       "Sysroot debug notifiers offset");
_Static_assert(offsetof(struct SysrootState, sendTargetStatus) == 0x408,
	       "Sysroot target status callback offset");

#if !SYSMEM_NSKBL
/* the debugger's event notifiers: read by the modules that raise the events */
uint32_t SceSysrootForDriver_483EF108(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	return notifiers ? notifiers->vipRpc : 0;
}

uint32_t SceSysrootForDriver_49843C16(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	return notifiers ? notifiers->veneziaRpc : 0;
}

uint32_t SceSysrootForDriver_3AE319DA(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	return notifiers ? notifiers->syscon : 0;
}

SceUInt32 SceSysrootForDriver_525B24C5(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	return notifiers ? notifiers->appMgr : 0;
}

uint32_t SceSysrootForDriver_10788257(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	return notifiers ? notifiers->power[0] : 0;
}

uint32_t SceSysrootForDriver_0A52A59C(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	return notifiers ? notifiers->power[1] : 0;
}

void SceSysrootForDriver_A6A0A038(uint32_t notifier)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->vipRpc = notifier;
}

void SceSysrootForDriver_47F19DD3(uint32_t notifier)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->veneziaRpc = notifier;
}

void SceSysrootForDriver_709799BB(uint32_t notifier)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->syscon = notifier;
}

void SceSysrootForDriver_1434D90E(uint32_t notifier)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->appMgr = notifier;
}

void SceSysrootForDriver_9CDE3623(uint32_t notifier)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->power[0] = notifier;
}

void SceSysrootForDriver_B0E3E791(uint32_t notifier)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->power[1] = notifier;
}

void SceSysrootForDriver_0ECD711E(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->vipRpc = 0;
}

void SceSysrootForDriver_5B106EB3(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->veneziaRpc = 0;
}

void SceSysrootForDriver_2C9E7797(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->syscon = 0;
}

void SceSysrootForDriver_D2423D6B(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->appMgr = 0;
}

void SceSysrootForDriver_EB19AAF6(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->power[0] = 0;
}

void SceSysrootForDriver_5458BF6D(void)
{
	struct SysrootDebugNotifiers *notifiers = s_sysrootState->debugNotifiers;

	if (notifiers)
		notifiers->power[1] = 0;
}

int SceSysrootForDriver_FF2DD7AB(struct SysrootDebugNotifiers *notifiers)
{
	if (!notifiers || notifiers->size != sizeof(*notifiers))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->debugNotifiers = notifiers;
	return 0;
}

int SceSysrootForKernel_14D81797(struct SysrootProfiler *profiler)
{
	s_sysrootState->profiler = profiler;
	return 0;
}

int SceSysrootForKernel_CD8B5D37(uint32_t a0, uint32_t a1, uint32_t a2)
{
	struct SysrootProfiler *profiler = s_sysrootState->profiler;

	if (!profiler || !profiler->reset)
		return SCE_KERNEL_ERROR_ERROR;
	return profiler->reset(a0, a1, a2);
}

int SceSysrootForKernel_CA8D1A2B(uint32_t a0, uint32_t a1, uint32_t a2)
{
	struct SysrootProfiler *profiler = s_sysrootState->profiler;

	if (!profiler || !profiler->start)
		return SCE_KERNEL_ERROR_ERROR;
	return profiler->start(a0, a1, a2);
}

int SceSysrootForKernel_FF44A7BA(uint32_t a0, uint32_t a1, uint32_t a2)
{
	struct SysrootProfiler *profiler = s_sysrootState->profiler;

	if (!profiler || !profiler->stop)
		return SCE_KERNEL_ERROR_ERROR;
	return profiler->stop(a0, a1, a2);
}

void SceSysrootForDriver_E541959B(int (*callback)(const char *, const char *))
{
	s_sysrootState->setBootParameter = callback;
}

void SceSysrootForDriver_9421B223(void)
{
	s_sysrootState->setBootParameter = NULL;
}

int SceSysrootForDriver_7AAC4EE7(const char *key, const char *value)
{
	int (*callback)(const char *, const char *) = s_sysrootState->setBootParameter;

	return callback ? callback(key, value) : 0;
}

void SceSysrootForDriver_D56A2F3E(int (*callback)(uint32_t, uint32_t))
{
	s_sysrootState->sendTargetStatus = callback;
}

void SceSysrootForDriver_2816A813(void)
{
	s_sysrootState->sendTargetStatus = NULL;
}

int SceSysrootForDriver_39F9C968(uint32_t status, uint32_t value)
{
	int (*callback)(uint32_t, uint32_t) = s_sysrootState->sendTargetStatus;

	return callback ? callback(status, value) : 0;
}

struct SysrootState *ksceSysrootGetSysroot(void)
{
	return s_sysrootState;
}

void ksceKernelSysrootSetSysroot(SceUID uid)
{
	s_sysrootState->sysrootUid = uid;
}

SceKblParam *ksceKernelSysrootGetKblParam(void)
{
	return s_sysrootState->kblParam;
}
#endif

/* PLACEHOLDER: meanings and callback signatures of raw Sysroot state words. */
_Static_assert(offsetof(struct SysrootState, status) == 0x28,
	       "Sysroot word28 offset");
_Static_assert(offsetof(struct SysrootState, bootArgs) == 0x70,
	       "Sysroot word70 offset");
_Static_assert(offsetof(struct SysrootState, factorySystemSwVersion) == 0x74,
	       "Sysroot word74 offset");
_Static_assert(offsetof(struct SysrootState, pervasiveDword) == 0x78,
	       "Sysroot word78 offset");
_Static_assert(offsetof(struct SysrootState, kermitRevision) == 0x7c,
	       "Sysroot word7c offset");
_Static_assert(offsetof(struct SysrootState, modelInfo) == 0x80,
	       "Sysroot word80 offset");
_Static_assert(offsetof(struct SysrootState, fallbackPUIDEntryHeap) == 0x2e0,
	       "Sysroot word2e0 offset");
_Static_assert(offsetof(struct SysrootState, intrMMUContext) == 0x314,
	       "Sysroot word314 offset");
_Static_assert(offsetof(struct SysrootState, vbaseResetVector) == 0x318,
	       "Sysroot word318 offset");
_Static_assert(offsetof(struct SysrootState, sharedMemory) == 0x320,
	       "Sysroot word320 offset");
_Static_assert(offsetof(struct SysrootState, appMgrSpawnProcess) == 0x328,
	       "Sysroot word328 offset");
_Static_assert(offsetof(struct SysrootState, getShellPid) == 0x32c,
	       "Sysroot word32c offset");
_Static_assert(offsetof(struct SysrootState, registeredPid) == 0x35c,
	       "Sysroot word35c offset");
_Static_assert(offsetof(struct SysrootState, licMgrGetLicenseStatus) == 0x368,
	       "Sysroot word368 offset");
_Static_assert(offsetof(struct SysrootState, utMgrHasPsmTestFlag) == 0x36c,
	       "Sysroot word36c offset");
_Static_assert(offsetof(struct SysrootState, utMgrHasSystemDataFilePlayReadyFlag) == 0x370,
	       "Sysroot word370 offset");
_Static_assert(offsetof(struct SysrootState, busErrorHandler) == 0x374,
	       "Sysroot word374 offset");
_Static_assert(offsetof(struct SysrootState, busErrorGetInfo) == 0x378,
	       "Sysroot word378 offset");
_Static_assert(offsetof(struct SysrootState, coredumpTrigger) == 0x37c,
	       "Sysroot word37c offset");
_Static_assert(offsetof(struct SysrootState, coredumpCancel) == 0x380,
	       "Sysroot word380 offset");
_Static_assert(offsetof(struct SysrootState, coredumpAddThread) == 0x384,
	       "Sysroot word384 offset");
_Static_assert(offsetof(struct SysrootState, ttyGetLogRaw) == 0x388,
	       "Sysroot word388 offset");
_Static_assert(offsetof(struct SysrootState, utMgrHasAuthId) == 0x38c,
	       "Sysroot word38c offset");
_Static_assert(offsetof(struct SysrootState, utMgrGetTrilithiumBuffer) == 0x390,
	       "Sysroot word390 offset");
_Static_assert(offsetof(struct SysrootState, utMgrHasNpTestFlag) == 0x394,
	       "Sysroot word394 offset");
_Static_assert(offsetof(struct SysrootState, getSystemSwVersion) == 0x398,
	       "Sysroot word398 offset");
_Static_assert(offsetof(struct SysrootState, ledSetMode) == 0x39c,
	       "Sysroot word39c offset");
_Static_assert(offsetof(struct SysrootState, getNidName) == 0x3a4,
	       "Sysroot word3a4 offset");
_Static_assert(offsetof(struct SysrootState, sysStateEvent) == 0x3a8,
	       "Sysroot word3a8 offset");
_Static_assert(offsetof(struct SysrootState, sendSystemError) == 0x3ac,
	       "Sysroot word3ac offset");
_Static_assert(offsetof(struct SysrootState, setSwInfoInt) == 0x3b4,
	       "Sysroot word3b4 offset");
_Static_assert(offsetof(struct SysrootState, setSwInfoStr) == 0x3b8,
	       "Sysroot word3b8 offset");
_Static_assert(offsetof(struct SysrootState, setSwInfoBin) == 0x3bc,
	       "Sysroot word3bc offset");
_Static_assert(offsetof(struct SysrootState, syncLocalTime) == 0x3c0,
	       "Sysroot word3c0 offset");
_Static_assert(offsetof(struct SysrootState, processEventOps) == 0x3c4,
	       "Sysroot word3c4 offset");
_Static_assert(offsetof(struct SysrootState, dbgpOps) == 0x3c8,
	       "Sysroot word3c8 offset");
_Static_assert(offsetof(struct SysrootState, dbgpNetAttach) == 0x3cc,
	       "Sysroot word3cc offset");
_Static_assert(offsetof(struct SysrootState, dbgpNetDetach) == 0x3d0,
	       "Sysroot word3d0 offset");
_Static_assert(offsetof(struct SysrootState, tmcpCommand) == 0x3d4,
	       "Sysroot word3d4 offset");
_Static_assert(offsetof(struct SysrootState, debugHook) == 0x3d8,
	       "Sysroot word3d8 offset");
_Static_assert(offsetof(struct SysrootState, sdbgpNetAttach) == 0x3dc,
	       "Sysroot word3dc offset");
_Static_assert(offsetof(struct SysrootState, sdbgpNetDetach) == 0x3e0,
	       "Sysroot word3e0 offset");
_Static_assert(offsetof(struct SysrootState, appMgrGetAppInfoByPid) == 0x3e4,
	       "Sysroot word3e4 offset");
_Static_assert(offsetof(struct SysrootState, appMgrGetAppInfoByAppId) == 0x3e8,
	       "Sysroot word3e8 offset");
_Static_assert(offsetof(struct SysrootState, appMgrGetAppList) == 0x3ec,
	       "Sysroot word3ec offset");
_Static_assert(offsetof(struct SysrootState, pfsMgrGetMountInfo) == 0x3f0,
	       "Sysroot word3f0 offset");
_Static_assert(offsetof(struct SysrootState, btOps) == 0x3f4,
	       "Sysroot word3f4 offset");
_Static_assert(offsetof(struct SysrootState, hidOps) == 0x3f8,
	       "Sysroot word3f8 offset");

#if !SYSMEM_NSKBL
void sceKernelSysrootSetVbaseResetVector(uint32_t value)
{
	s_sysrootState->vbaseResetVector = value;
}

uint32_t SceSysrootForKernel_118657C6(void)
{
	return s_sysrootState->intrMMUContext;
}

void SceSysrootForKernel_21F5790B(void (*callback)(void))
{
	s_sysrootState->debugHook = callback;
}

void SceSysrootForDriver_27D30DB2(SysrootNetHook callback)
{
	s_sysrootState->sdbgpNetAttach = callback;
}

void SceSysrootForDriver_2D6B2A79(int (*callback)(int type, SceSize size, const char *buffer))
{
	s_sysrootState->sendSystemError = callback;
}

uint32_t SceSysrootForKernel_340575CB(void)
{
	return s_sysrootState->registeredPid;
}

uint32_t SceSysrootForKernel_403B509E(void)
{
	return s_sysrootState->kermitRevision;
}

void SceSysrootForDriver_42FC9FFF(void)
{
	s_sysrootState->dbgpNetAttach = 0;
}

void SceSysrootForKernel_59B76D23(void *value)
{
	s_sysrootState->fallbackPUIDEntryHeap = value;
}

uint32_t ksceKernelSysrootGetStatus(void)
{
	return s_sysrootState->status;
}

void SceSysrootForKernel_611F17A4(uint32_t value)
{
	s_sysrootState->intrMMUContext = value;
}

void SceSysrootForKernel_642123EB(void)
{
	s_sysrootState->bootArgs = 0;
}

void SceSysrootForKernel_ksceSysrootRegisterLicMgrGetLicenseStatus(int (*callback)(void))
{
	s_sysrootState->licMgrGetLicenseStatus = callback;
}

void SceSysrootForKernel_ksceKernelSysrootUnregisterDbgpHandler(void)
{
	s_sysrootState->dbgpOps = NULL;
}

void SceSysrootForDriver_82FC6405(SysrootBusErrorGetInfo callback)
{
	s_sysrootState->busErrorGetInfo = callback;
}

void SceSysrootForDriver_8747D415(SysrootBusErrorHandler callback)
{
	s_sysrootState->busErrorHandler = callback;
}

void SceSysrootForDriver_9A8871F1(SysrootAppMgrSpawnProcess callback)
{
	s_sysrootState->appMgrSpawnProcess = callback;
}

void SceSysrootForDriver_A799F22A(SysrootNetHook callback)
{
	s_sysrootState->dbgpNetDetach = callback;
}

void SceSysrootForKernel_B39CD708(int (*callback)(uint32_t nid, const char **name))
{
	s_sysrootState->getNidName = callback;
}

void SceSysrootForDriver_C5EAF5F7(SysrootSysStateEvent callback)
{
	s_sysrootState->sysStateEvent = callback;
}

void SceSysrootForDriver_C728BC61(void)
{
	s_sysrootState->sdbgpNetAttach = 0;
}

uint32_t ksceKernelSysrootGetSharedMemory(void)
{
	return s_sysrootState->sharedMemory;
}

void SceSysrootForKernel_CC119171(SysrootPfsMgrGetMountInfo callback)
{
	s_sysrootState->pfsMgrGetMountInfo = callback;
}
#endif

uint32_t ksceKernelSysrootGetVbaseResetVector(void)
{
	return s_sysrootState->vbaseResetVector;
}

#if !SYSMEM_NSKBL
void SceSysrootForDriver_D02281A8(void)
{
	s_sysrootState->sdbgpNetDetach = 0;
}

uint32_t ksceSysrootGetFactorySystemSwVersion(void)
{
	return s_sysrootState->factorySystemSwVersion;
}

void SceSysrootForDriver_D4457D4F(SysrootNetHook callback)
{
	s_sysrootState->sdbgpNetDetach = callback;
}

void SceSysrootForDriver_D7207203(void)
{
	s_sysrootState->dbgpNetDetach = 0;
}

void SceSysrootForDriver_ECADF8C3(SysrootNetHook callback)
{
	s_sysrootState->dbgpNetAttach = callback;
}
#endif

void sceKernelSysrootSetSharedMemory(uint32_t value)
{
	s_sysrootState->sharedMemory = value;
}

#if !SYSMEM_NSKBL
uint32_t SceSysrootForKernel_FFD6E24D(void)
{
	return s_sysrootState->pervasiveDword;
}

void SceSysrootForDriver_05CD0DD9(int (*callback)(const char *name, SceSize nameLength,
						  const void *data, SceSize dataSize))
{
	s_sysrootState->setSwInfoBin = callback;
}

void SceSysrootForDriver_1834D482(SysrootTmcpCommand callback)
{
	s_sysrootState->tmcpCommand = callback;
}

void SceSysrootForDriver_1FFD2AFA(int (*callback)(const char *name, SceSize length,
						  SceUInt32 number))
{
	s_sysrootState->setSwInfoInt = callback;
}

void SceSysrootForDriver_246AAAB3(SysrootLedSetMode callback)
{
	s_sysrootState->ledSetMode = callback;
}

void SceSysrootForDriver_2BE874EF(int (*callback)(int appId, void *out))
{
	s_sysrootState->appMgrGetAppInfoByPid = callback;
}

void ksceKernelSysrootSetGetSystemSwVersionFunc(int (*callback)(void))
{
	s_sysrootState->getSystemSwVersion = callback;
}

void SceSysrootForDriver_35875119(int (*callback)(void *buffer))
{
	s_sysrootState->utMgrGetTrilithiumBuffer = callback;
}

void SceSysrootForDriver_3B19B06B(int (*callback)(int appId, void *out))
{
	s_sysrootState->appMgrGetAppInfoByAppId = callback;
}

void SceSysrootForDriver_400B9793(int (*callback)(int arg))
{
	s_sysrootState->coredumpCancel = callback;
}

uint32_t SceSysrootForDriver_4D98B15B(void)
{
	return s_sysrootState->modelInfo;
}

void SceSysrootForDriver_5A7FFDC1(void)
{
	s_sysrootState->processEventOps = NULL;
}

void SceSysrootForDriver_70AD47A9(int (*callback)(void *out))
{
	s_sysrootState->appMgrGetAppList = callback;
}

void SceSysrootForDriver_7C0BE393(int (*callback)(const char *name, SceSize nameLength,
						  const char *str, SceSize strLength))
{
	s_sysrootState->setSwInfoStr = callback;
}

void SceSysrootForDriver_9DCD0894(SysrootCoredumpAddThread callback)
{
	s_sysrootState->coredumpAddThread = callback;
}

void SceSysrootForDriver_A12C9950(SysrootUtMgrHasAuthId callback)
{
	s_sysrootState->utMgrHasAuthId = callback;
}

void SceSysrootForDriver_A7A01DF0(void)
{
	s_sysrootState->tmcpCommand = 0;
}

void SceSysrootForDriver_A85FF3B2(void)
{
	s_sysrootState->hidOps = NULL;
}

void SceSysrootForDriver_AA770EF7(int (*callback)(int magic, void *buffer, SceSize size))
{
	s_sysrootState->ttyGetLogRaw = callback;
}

void ksceKernelSysrootRegisterCoredumpTrigger(SysrootCoredumpTrigger callback)
{
	s_sysrootState->coredumpTrigger = callback;
}

void SceSysrootForDriver_E06C14FD(void)
{
	s_sysrootState->btOps = NULL;
}

int SceSysrootForDriver_5A4A7F7E(const SysrootBtOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->btOps = ops;
	return 0;
}

int SceSysrootForDriver_951C7943(uint64_t deviceAddress, void *buffer,
			       int maxCount)
{
	const SysrootBtOps *ops = s_sysrootState->btOps;

	if (!ops)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->getRegisteredInfo(deviceAddress, buffer, maxCount);
}

int SceSysrootForDriver_D1E54BF4(uint64_t deviceAddress)
{
	const SysrootBtOps *ops = s_sysrootState->btOps;

	if (!ops)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->getConnectingInfo(deviceAddress);
}

void SceSysrootForDriver_E25D2FD5(int (*callback)(void))
{
	s_sysrootState->utMgrHasPsmTestFlag = callback;
}

void SceSysrootForDriver_E2E88E3E(int (*callback)(void))
{
	s_sysrootState->utMgrHasSystemDataFilePlayReadyFlag = callback;
}

void SceSysrootForDriver_E9B4221F(int (*callback)(void))
{
	s_sysrootState->getShellPid = callback;
}

int ksceSysrootGetNidName(uint32_t nid, const char **name)
{
	int (*callback)(uint32_t, const char **) = s_sysrootState->getNidName;

	if (!callback)
		return SCE_ERROR_ERRNO_ENOSYS;
	return callback(nid, name);
}

int ksceKernelSysrootIofilemgrStart(void)
{
	return s_sysrootState->iofilemgrStart();
}

int ksceKernelSysrootGetShellPid(void)
{
	int (*callback)(void) = s_sysrootState->getShellPid;

	return callback ? callback() : -1;
}

int ksceKernelSysrootGetSystemSwVersion(void)
{
	int (*callback)(void) = s_sysrootState->getSystemSwVersion;

	return callback ? callback() : -1;
}

void SceSysrootForDriver_ED688AEE(int (*callback)(void))
{
	s_sysrootState->syncLocalTime = callback;
}

void ksceSysrootUtMgrSetNpTestFlag(int (*callback)(void))
{
	s_sysrootState->utMgrHasNpTestFlag = callback;
}
#endif

_Static_assert(offsetof(SceKblParam, boot_flags) == 0x30,
	       "KBL boot_flags offset");
_Static_assert(offsetof(SceKblParam, boot_type_indicator_1) == 0x6c,
	       "KBL boot_type_indicator_1 offset");
_Static_assert(offsetof(SceKblParam, poweroff_factor) == 0xc0,
	       "KBL poweroff_factor offset");
_Static_assert(offsetof(SceKblParam, wakeup_factor) == 0xc4,
	       "KBL wakeup_factor offset");
_Static_assert(offsetof(SceKblParam, hardware_info) == 0xd4,
	       "KBL hardware_info offset");

#if !SYSMEM_NSKBL
uint32_t ksceSysrootGetWakeupFactor(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return kbl->wakeup_factor;
}

uint32_t SceSysrootForKernel_ksceSysrootIsBsodReboot(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return (kbl->wakeup_factor & 0x7f) == 0x17;
}

uint32_t ksceSysrootIsNonRemovableCardMode(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return (kbl->boot_flags[5] & 1) ^ 1;
}

uint32_t ksceSysrootIsManufacturingMode(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return (kbl->boot_type_indicator_1 >> 2) & 1;
}

uint32_t SceSysrootForDriver_7918D44E(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return (kbl->boot_type_indicator_1 >> 16) & 1;
}

uint32_t SceSysrootForKernel_ksceKernelIsSomeBootMode(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return (kbl->boot_type_indicator_1 >> 19) & 1;
}

uint32_t SceSysrootForKernel_ksceSysrootIsExternalBootMode(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return kbl->boot_type_indicator_1 & 1;
}

/* name unknown */
uint32_t SysrootGetBootTypeIndicator2(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return kbl->boot_type_indicator_2;
}

uint32_t SceSysrootForKernel_AB3CC7D0(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return kbl->poweroff_factor;
}

uint32_t SceSysrootForDriver_AE7A8F1D(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return kbl->boot_flags[1] != 0xff;
}

uint32_t SceSysrootForKernel_ksceSysrootIsUpdateMode(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return kbl->boot_flags[0] != 0xff;
}

uint32_t SceSysrootForDriver_E4EA1960(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return (kbl->wakeup_factor & 0x7f) == 1;
}

uint32_t ksceSysrootGetHardwareInfo(void)
{
	const SceKblParam *kbl = s_sysrootState->kblParam;

	return kbl->hardware_info;
}

int SceSysrootForKernel_B171CC2D(const SysrootModulemgrOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->modulemgrOps = ops;
	return 0;
}

void SceSysrootForKernel_0A12227B(SceUID pid, SceUInt32 addr)
{
	const SysrootModulemgrOps *ops = s_sysrootState->modulemgrOps;

	if (ops)
		ops->showCallerInfo(pid, addr);
}

int ksceSysrootGetModuleInfoForPid(SceUID pid, SceUInt32 addr,
				struct ModulemgrStubInfo *info)
{
	const SysrootModulemgrOps *ops = s_sysrootState->modulemgrOps;

	return ops ? ops->getStubInfo(pid, addr, info) : (int)SCE_ERROR_ERRNO_ENOSYS;
}

int SceSysrootForKernel_1D84C4D4(SceUID pid, SceUInt32 addr,
			      SceUInt32 *fingerprint, void **textBase, char *name)
{
	const SysrootModulemgrOps *ops = s_sysrootState->modulemgrOps;

	return ops ? ops->getModuleInfoMinByAddr(pid, addr, fingerprint, textBase, name) :
		(int)SCE_ERROR_ERRNO_ENOSYS;
}

int sceKernelSysrootBacktrace(SceUID thid,
			      struct SceKernelBacktraceFrameForKernel *frames,
			      SceSize size, SceSize *count, SceUInt32 flags)
{
	const SysrootModulemgrOps *ops = s_sysrootState->modulemgrOps;

	return ops ? ops->backtrace(thid, frames, size, count, flags) : 0;
}

int sceKernelSysrootPrintBacktrace(SceUID pid,
			      const struct SceKernelBacktraceFrameForKernel *frames,
			      SceSize count, SceUInt32 start)
{
	const SysrootModulemgrOps *ops = s_sysrootState->modulemgrOps;

	return ops ? ops->printBacktrace(pid, frames, count, start) : 0;
}

void SceSysrootForDriver_C94C76FA(const struct PamgrTimebaseProvider *provider)
{
	struct SysrootState *state;

	if (provider && provider->size == sizeof(*provider)) {
		unsigned int (*getFrequency)(void) = provider->getFrequency;
		uint64_t (*getValue)(void) = provider->getValue;
		volatile uint32_t *(*getRegister)(unsigned int, unsigned int) =
			provider->getRegister;

		state = s_sysrootState;
		state->getTimebaseFrequency = getFrequency;
		state->getTimebaseValue = getValue;
		state->getTimebaseRegister = getRegister;
	} else {
		state = s_sysrootState;
		state->getTimebaseFrequency = NULL;
		state->getTimebaseValue = NULL;
		state->getTimebaseRegister = NULL;
	}
}

unsigned int SceSysrootForDriver_BFD8F2A2(void)
{
	unsigned int (*getFrequency)(void) = s_sysrootState->getTimebaseFrequency;

	return getFrequency ? getFrequency() : 0;
}

uint64_t SceSysrootForDriver_273EAE53(void)
{
	uint64_t (*getValue)(void) = s_sysrootState->getTimebaseValue;

	return getValue ? getValue() : 0;
}

volatile uint32_t *SceSysrootForDriver_6655F48C(unsigned int bank, unsigned int index)
{
	volatile uint32_t *(*getRegister)(unsigned int, unsigned int) =
		s_sysrootState->getTimebaseRegister;

	return getRegister ? getRegister(bank, index) : NULL;
}
#endif

extern uint32_t ksceKernelAtomicGetAndAdd32(uint32_t *address, uint32_t value);

#if !SYSMEM_NSKBL
void sceKernelSysrootEnableAutoClockDown(void)
{
	uint32_t previous = ksceKernelAtomicGetAndAdd32(
		&SceSysrootForDriver_4B8C305A.disableCountAndFlags, (uint32_t)-0x10);

	SCE_DBG_ASSERT_CHECK((int32_t)previous > 0x10, 1002);
}

void sceKernelSysrootDisableAutoClockDown(void)
{
	uint32_t previous = ksceKernelAtomicGetAndAdd32(
		&SceSysrootForDriver_4B8C305A.disableCountAndFlags, 0x10);

	SCE_DBG_ASSERT_CHECK(previous < 0xf0000000U, 1011);
}

int SceSysrootForKernel_D29BCA77(const SysrootProcessmgrRuntimeOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->processmgrRuntimeOps = ops;
	return 0;
}

void *SceSysrootForKernel_5F2CE289(struct SceProcessObject *process)
{
	return s_sysrootState->processmgrRuntimeOps->getLibkernelAddr0(process);
}

void *SceSysrootForKernel_AD74BEB3(struct SceProcessObject *process)
{
	return s_sysrootState->processmgrRuntimeOps->getLibkernelAddr2(process);
}

int SceSysrootForKernel_1247A825(SceUID pid, SceUInt32 state)
{
	return s_sysrootState->processmgrRuntimeOps->debugSuspend(pid, state);
}

void *SceSysrootForKernel_2F75C1DC(SceSize size, void *opt)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;
	/* Native dispatch supplies caller LR in r2; the registered ABI uses r0/r1. */
	typedef void *(*SysrootAllocCurrentHeapCall)(SceSize, void *, const void *);

	return ops ? ((SysrootAllocCurrentHeapCall)ops->allocCurrentHeap)(
		size, opt, __builtin_return_address(0)) : NULL;
}

int SceSysrootForKernel_AE2132FD(void *ptr)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	return ops ? ops->freeCurrentHeap(ptr) : 0;
}

int SceSysrootForDriver_9AF075D4(SceUID pid)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	return ops ? ops->inhibitProcEvent(pid) : 0;
}

int SceSysrootForDriver_1B3FA457(SceUID pid)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	return ops ? ops->permitProcEvent(pid) : 0;
}

int SceSysrootForDriver_CAE820B2(SceUID pid, SceUInt32 *sdkVersion)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	if (ops)
		return ops->getSdkVersion(pid, sdkVersion);
	*sdkVersion = 0;
	return SCE_KERNEL_ERROR_ERROR;
}

int ksceKernelSysrootProcessmgrStart2(void)
{
	int result = s_sysrootState->processmgrRuntimeOps->moduleStart2();

	if (result < 0)
		ksceKernelPanic(SCE_DBGINFO_FUNC(579, "sceKernelSysrootProcessmgrStart2"),
				__builtin_return_address(0));
	return result;
}

void *sceKernelSysrootGetProcessCBExitDeleteThread(struct SceProcessObject *process)
{
	SCE_DBG_ASSERT_LEVEL(0, s_sysrootState->processmgrRuntimeOps != NULL, 643);
	return s_sysrootState->processmgrRuntimeOps->getLibkernelAddr1(process);
}

/* Original private dispatcher names are unknown. */
void *SysrootGetProcessObjectHeap(SceUID pid, int kind)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	return ops ? ops->getObjectHeap(pid, kind) : NULL;
}

int SysrootCreateProcessLocalStorage(const char *name, SceSize size)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	return ops ? ops->createLocalStorage(name, size) : (int)SCE_KERNEL_ERROR_ERROR;
}

void *SysrootGetProcessLocalStorageAddr(SceUInt32 key)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	return ops ? ops->getLocalStorageAddr(key) : NULL;
}

int SysrootGetProcessLocalStorageAddrForPid(SceUID pid, SceUInt32 key,
					 void **addr, int create)
{
	const SysrootProcessmgrRuntimeOps *ops = s_sysrootState->processmgrRuntimeOps;

	return ops ? ops->getLocalStorageAddrForPid(pid, key, addr, create) :
		(int)SCE_KERNEL_ERROR_ERROR;
}
#endif

extern void *ksceKernelAllocHeapMemoryWithOpt(SceUID heap, SceSize size,
					  void *opt, const void *caller);
extern int ksceKernelFree(void *ptr);

#if !SYSMEM_NSKBL
void *ksceKernelAllocHeapMemory2(SceUID pid, SceSize size, void *opt)
{
	const SysrootProcessmgrRuntimeOps *ops;
	/* Native dispatch preserves caller LR in r3 on both allocation routes. */
	typedef void *(*SysrootAllocRemoteHeapCall)(SceUID, SceSize, void *, const void *);

	if (pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_PROCESS_ID ||
	    pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		return ksceKernelAllocHeapMemoryWithOpt(SCE_KERNEL_HEAP_ID, size, opt,
						      __builtin_return_address(0));
	ops = s_sysrootState->processmgrRuntimeOps;
	return ops ? ((SysrootAllocRemoteHeapCall)ops->allocRemoteHeap)(
		pid, size, opt, __builtin_return_address(0)) : NULL;
}

int SceSysrootForKernel_7968F091(SceUID pid, void *ptr)
{
	const SysrootProcessmgrRuntimeOps *ops;

	if (pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_PROCESS_ID ||
	    pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		return ksceKernelFree(ptr);
	ops = s_sysrootState->processmgrRuntimeOps;
	return ops ? ops->freeRemoteHeap(pid, ptr) : 0;
}

int SceSysrootForDriver_579CC758(const SysrootDfmgrOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->dfmgrOps = ops;
	return 0;
}

int SceSysrootForDriver_9A486846(SceInt32 kind, SceUInt32 total,
			      const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->updateProgressStarted(kind, total, msg, len) : 0;
}

int SceSysrootForDriver_9EC02A41(SceInt32 kind, SceUInt32 percent)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->updateProgressOngoing(kind, percent) : 0;
}

int SceSysrootForDriver_217B2871(SceUInt32 result, const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->updateFinished(result, msg, len) : 0;
}

int SceSysrootForDriver_70A60BBA(SceInt32 kind, SceUInt32 total,
			      const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->installProgressStarted(kind, total, msg, len) : 0;
}

int SceSysrootForDriver_79458ACB(SceInt32 kind, SceUInt32 percent)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->installProgressOngoing(kind, percent) : 0;
}

int SceSysrootForDriver_D03741B8(SceUInt32 result, const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->installFinished(result, msg, len) : 0;
}

int SceSysrootForDriver_56C4C949(SceInt32 kind, SceUInt32 total,
			      const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->uninstallProgressStarted(kind, total, msg, len) : 0;
}

int SceSysrootForDriver_7998F5B5(SceInt32 kind, SceUInt32 percent)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->uninstallProgressOngoing(kind, percent) : 0;
}

int SceSysrootForDriver_0F59B369(SceUInt32 result, const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->uninstallFinished(result, msg, len) : 0;
}

int SceSysrootForDriver_97E2FB77(SceInt32 kind, SceUInt32 total,
			      const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->saveDataInstallProgressStarted(kind, total, msg, len) : 0;
}

int SceSysrootForDriver_F0C91CD3(SceInt32 kind, SceUInt32 percent)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->saveDataInstallProgressOngoing(kind, percent) : 0;
}

int SceSysrootForDriver_E384DD93(SceUInt32 result, const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->saveDataInstallFinished(result, msg, len) : 0;
}

int SceSysrootForDriver_E8EBF179(SceInt32 kind, SceUInt32 total,
			      const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->saveDataFormatProgressStarted(kind, total, msg, len) : 0;
}

int SceSysrootForDriver_1BA0C6C1(SceInt32 kind, SceUInt32 percent)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->saveDataFormatProgressOngoing(kind, percent) : 0;
}

int SceSysrootForDriver_D04C7723(SceUInt32 result, const char *msg, int len)
{
	const SysrootDfmgrOps *ops = s_sysrootState->dfmgrOps;

	return ops ? ops->saveDataFormatFinished(result, msg, len) : 0;
}

int SceSysrootForDriver_34281746(const SysrootHidOps *ops)
{
	if (!ops || ops->size != sizeof(*ops))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_sysrootState->hidOps = ops;
	return 0;
}

int SceSysrootForDriver_ED22CF8F(void *buffer, SceSize size, SceUInt32 *count)
{
	const SysrootHidOps *ops = s_sysrootState->hidOps;

	return ops ? ops->dumpDevices(buffer, size, count) : (int)SCE_ERROR_ERRNO_ENOTSUP;
}

int SceSysrootForDriver_DC651D22(void *buffer, SceSize size, SceUInt32 *count)
{
	const SysrootHidOps *ops = s_sysrootState->hidOps;

	return ops ? ops->dumpLists(buffer, size, count) : (int)SCE_ERROR_ERRNO_ENOTSUP;
}
#endif

/*
 * Calls into the services other modules registered. Most return
 * SCE_ERROR_ERRNO_ENOTSUP when nothing is registered; where the
 * original has no fallback value it hands back its first argument unchanged.
 */

#if !SYSMEM_NSKBL
void SceSysrootForKernel_0DF574A9(void)
{
	void (*callback)(void) = s_sysrootState->debugHook;

	if (callback != NULL)
		callback();
}

int SceSysrootForKernel_ksceKernelSysrootDbgpSuspendProcessAndWaitResume(SceUID pid,
	SceUID modid, SceUInt32 stopReason, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->suspendProcessAndWaitResume(pid, modid, stopReason, time);
}

int SceSysrootForKernel_2A03DFA1(void *out)
{
	int (*callback)(void *) = s_sysrootState->appMgrGetAppList;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(out);
}

/* sceKernelSysrootLicMgrGetLicenseStatus */
int SceSysrootForDriver_377895EB(void)
{
	int (*callback)(void) = s_sysrootState->licMgrGetLicenseStatus;

	if (callback == NULL)
		return -1;
	return callback();
}

int SceSysrootForDriver_38751FB7(SceUID pid, SceUInt32 address, SceUInt32 instruction)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->codeModified(pid, address, instruction);
}

int SceSysrootForKernel_ksceKernelSysrootAppMgrSpawnProcess(const char *name, int type,
	const char *path, SceSize length, const void *args, void *opt, void *out)
{
	return s_sysrootState->appMgrSpawnProcess(name, type, path, length, args, opt, out);
}

int SceSysrootForDriver_3F76D531(SceUID pid)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->processResume(pid);
}

int SceSysrootForDriver_40F28DC6(void *interface)
{
	SysrootNetHook callback = s_sysrootState->sdbgpNetDetach;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(interface);
}

int SceSysrootForDriver_47724459(int event)
{
	SysrootSysStateEvent callback = s_sysrootState->sysStateEvent;

	if (callback == NULL)
		return event;
	return callback(event);
}

int SceSysrootForKernel_4F57A13F(void *mount, void *info)
{
	SysrootPfsMgrGetMountInfo callback = s_sysrootState->pfsMgrGetMountInfo;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(mount, info);
}

int SceSysrootForDriver_5C86E49B(SceUID pid, SceUInt32 status, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->processSpontaneousExit(pid, status, time);
}

int SceSysrootForDriver_6050A467(SceUID pid)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->isDebugTarget(pid);
}

int SceSysrootForDriver_6C036B53(void *interface)
{
	SysrootNetHook callback = s_sysrootState->dbgpNetAttach;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(interface);
}

int SceSysrootForKernel_6D111FA7(int appId, void *out)
{
	int (*callback)(int, void *) = s_sysrootState->appMgrGetAppInfoByAppId;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(appId, out);
}

int SceSysrootForDriver_6D44F190(SceUID pid, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->processKill(pid, time);
}

/* sceKernelSysrootGetPrxDebugFlag */
int SceSysrootForDriver_73522F65(SceUID pid, SceUInt32 *flag)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->getPrxDebugFlag(pid, flag);
}

int SceSysrootForDriver_77CF3FBD(SceUID pid, SceUID spawnPid, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->processExitToSpawn(pid, spawnPid, time);
}

int SceSysrootForDriver_8498E23F(SceUID pid, int exitStatus, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->processExit(pid, exitStatus, time);
}

int SceSysrootForDriver_84C0BE05(SceUID pid)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->appAbort(pid);
}

int SceSysrootForDriver_89532E2F(void)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->isPsButtonCaptured();
}

int sceKernelSysrootTrapThreadAfterSyscall(void)
{
	return s_sysrootState->trapThreadAfterSyscall();
}

/* sceKernelSysrootSWBkptGetOriginalOpcode */
int SceSysrootForDriver_9CFF80F9(SceUID pid, void *address, SceUInt32 *opcode)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->swBkptGetOriginalOpcode(pid, address, opcode);
}

int SceSysrootForDriver_A9510EFE(void *interface)
{
	SysrootNetHook callback = s_sysrootState->dbgpNetDetach;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(interface);
}

int SceSysrootForDriver_AA8D34EE(SceUID pid, SceUID modid, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->moduleStop(pid, modid, time);
}

int SceSysrootForDriver_B27B7530(SceUInt32 status0, SceUInt32 status1, SceUInt32 status2)
{
	SysrootBusErrorHandler callback = s_sysrootState->busErrorHandler;

	if (callback == NULL)
		return status0;
	return callback(status0, status1, status2);
}

int SceSysrootForKernel_BF82931F(int appId, void *out)
{
	int (*callback)(int, void *) = s_sysrootState->appMgrGetAppInfoByPid;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(appId, out);
}

int SceSysrootForDriver_CA497324(SceUID pid, SceUID modid, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->moduleStart(pid, modid, time);
}

int SceSysrootForDriver_CC7A0E63(int type, SceSize size, const char *buffer)
{
	int (*callback)(int, SceSize, const char *) = s_sysrootState->sendSystemError;

	if (callback == NULL)
		return type;
	return callback(type, size, buffer);
}

int SceSysrootForDriver_CCB8BB5E(void *interface)
{
	SysrootNetHook callback = s_sysrootState->sdbgpNetAttach;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(interface);
}

int SceSysrootForDriver_CD4B84F7(void *buffer, SceSize size)
{
	SysrootBusErrorGetInfo callback = s_sysrootState->busErrorGetInfo;

	if (callback == NULL)
		return 0;
	return callback(buffer, size);
}

/* sceKernelSysrootReturnFromExcpToThread */
int sceKernelSysrootReturnFromExcpToThread(void)
{
	return s_sysrootState->returnFromExcpToThread();
}

int SceSysrootForDriver_F2BC9E1D(SceUID pid)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return pid;
	return ops->processSuspend(pid);
}

int SceSysrootForDriver_F4340469(SceUID parentPid, SceUID pid, SceUInt64 time)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return parentPid;
	return ops->processStart(parentPid, pid, time);
}

int SceSysrootForDriver_06436EF0(SceUInt32 flags, const char *pathName,
	const char *cmdLine, const char *workDir, SceUInt32 *id, SceUInt32 *value,
	SceUID callbackUid, void *status, int async)
{
	SysrootTmcpCommand callback = s_sysrootState->tmcpCommand;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return callback(flags, pathName, cmdLine, workDir, id, value, callbackUid, status,
			async);
}

int SceSysrootForDriver_1160FCA1(SceUID pid, SceUInt32 *flag)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->updateUlobjmgrFlagForLibfiber(pid, flag);
}

/* sceKernelSysrootSetSwInfoStr */
int SceSysrootForDriver_17DD213C(const char *name, SceSize nameLength, const char *str,
				 SceSize strLength)
{
	int (*callback)(const char *, SceSize, const char *, SceSize) =
		s_sysrootState->setSwInfoStr;

	if (callback == NULL)
		return 0;
	return callback(name, nameLength, str, strLength);
}

/* sceKernelSysrootSblUtMgrHasPSMTestFlag (henkaku wiki) */
int SceSysrootForDriver_26AA237C(void)
{
	int (*callback)(void) = s_sysrootState->utMgrHasPsmTestFlag;

	if (callback == NULL)
		return 0;
	return callback();
}

int SceSysrootForDriver_39F1FD07(SceUID pid)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return 0;
	return ops->isProcessAttached(pid);
}

int SceSysrootForDriver_56D85EB0(SceUInt64 authId)
{
	SysrootUtMgrHasAuthId callback = s_sysrootState->utMgrHasAuthId;

	if (callback == NULL)
		return 0;
	return callback(authId);
}

int SceSysrootForDriver_591BB490(int magic, void *buffer, SceSize size)
{
	int (*callback)(int, void *, SceSize) = s_sysrootState->ttyGetLogRaw;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOENT;
	return callback(magic, buffer, size);
}

int SceSysrootForDriver_6219CC14(void *dbgp, void *process, SceUInt32 set, SceUInt32 clear,
	SceUInt32 *flag)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return SCE_ERROR_ERRNO_ENOTSUP;
	return ops->updateUlobjmgrFlagForDbgp(dbgp, process, set, clear, flag);
}

/* sceKernelSysrootSetSwInfoInt */
int SceSysrootForDriver_631141E2(const char *name, SceSize length, SceUInt32 number)
{
	int (*callback)(const char *, SceSize, SceUInt32) = s_sysrootState->setSwInfoInt;

	if (callback == NULL)
		return 0;
	return callback(name, length, number);
}

int SceSysrootForDriver_65F3D598(void)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return 0;
	return ops->disableWatchpoint(ops);
}

int SceSysrootForDriver_7B4A377C(void)
{
	const SysrootDbgpOps *ops = s_sysrootState->dbgpOps;

	if (ops == NULL)
		return 0;
	return ops->enableWatchpoint(ops);
}

/* sceKernelSysrootCoredumpCancel */
int sceKernelSysrootCoredumpCancel(int arg)
{
	return s_sysrootState->coredumpCancel(arg);
}

int SceSysrootForDriver_9BF782CA(SceUInt32 led, SceUInt32 mode, const void *option)
{
	SysrootLedSetMode callback = s_sysrootState->ledSetMode;

	if (callback == NULL)
		return 0;
	return callback(led, mode, option);
}

/* sceKernelSysrootSetSwInfoBin */
int SceSysrootForDriver_9E96D990(const char *name, SceSize nameLength, const void *data,
				 SceSize dataSize)
{
	int (*callback)(const char *, SceSize, const void *, SceSize) =
		s_sysrootState->setSwInfoBin;

	if (callback == NULL)
		return 0;
	return callback(name, nameLength, data, dataSize);
}

int ksceSysrootUtMgrHasNpTestFlag(void)
{
	int (*callback)(void) = s_sysrootState->utMgrHasNpTestFlag;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOENT;
	return callback();
}

int ksceKernelSysrootCoredumpTrigger(SceUID pid,
	SceKernelCoredumpStateUpdateCallback update,
	SceKernelCoredumpStateFinishCallback finish,
	SceCoredumpTriggerParam *param)
{
	return s_sysrootState->coredumpTrigger(pid, update, finish, param);
}

/* sceKernelSysrootUtMgrGetTrilithiumBuffer */
int SceSysrootForDriver_D75D4F37(void *buffer)
{
	int (*callback)(void *) = s_sysrootState->utMgrGetTrilithiumBuffer;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOENT;
	return callback(buffer);
}

int SceSysrootForDriver_DABD4A47(void *threadInfo)
{
	SysrootCoredumpAddThread callback = s_sysrootState->coredumpAddThread;

	if (callback == NULL)
		return SCE_ERROR_ERRNO_ENOENT;
	return callback(threadInfo);
}

/* sceKernelSysrootSblUtMgrHasSystemDataFilePlayReadyFlag (henkaku wiki) */
int SceSysrootForDriver_E2515A08(void)
{
	int (*callback)(void) = s_sysrootState->utMgrHasSystemDataFilePlayReadyFlag;

	if (callback == NULL)
		return 0;
	return callback();
}

int SceSysrootForDriver_F404026C(void)
{
	int (*callback)(void) = s_sysrootState->syncLocalTime;

	if (callback == NULL)
		return 0;
	return callback();
}
#endif

/* the result type of the API call in progress, for the debugger */
#define SYSROOT_API_TYPE_INT32		0
#define SYSROOT_API_TYPE_INT64		1
#define SYSROOT_API_TYPE_UINT32		2
#define SYSROOT_API_TYPE_UINT64		3
#define SYSROOT_API_TYPE_RAW		4
#define SYSROOT_API_TYPE_ERROR_MSG	0x80000000

#define SYSROOT_API_DECLARE(_type, _line) do {				\
	ThreadCB *thread = getCurrentThreadCB();			\
									\
	if (thread != NULL && thread->userFrame != NULL)		\
		thread->userFrame->apiDeclare = (_type);		\
	else								\
		SCE_DBG_ASSERT_FAIL(_line);				\
} while (0)

#if !SYSMEM_NSKBL
void sceKernelApiDeclareTypeInt32(void)
{
	SYSROOT_API_DECLARE(SYSROOT_API_TYPE_INT32, 2818);
}

void sceKernelApiDeclareTypeInt64(void)
{
	SYSROOT_API_DECLARE(SYSROOT_API_TYPE_INT64, 2831);
}

void sceKernelApiDeclareTypeUInt32(void)
{
	SYSROOT_API_DECLARE(SYSROOT_API_TYPE_UINT32, 2844);
}

void sceKernelApiDeclareTypeUInt64(void)
{
	SYSROOT_API_DECLARE(SYSROOT_API_TYPE_UINT64, 2857);
}

void sceKernelApiDeclareTypeRaw(void)
{
	SYSROOT_API_DECLARE(SYSROOT_API_TYPE_RAW, 2870);
}

/* the error message's id, flagged */
void sceKernelApiDeclareErrorMsg(uint32_t message)
{
	SYSROOT_API_DECLARE(SYSROOT_API_TYPE_ERROR_MSG | message, 2883);
}

int sceKernelGetModelForCDialog(void)
{
	return s_sysrootState->model << 16;
}

int sceKernelGetModel(void)
{
	return s_sysrootState->model << 16;
}

int sceKernelIsPSVitaTV(void)
{
	return s_sysrootState->model == 2;
}
#endif

/* bootFlags: set when the system resumed rather than booted */
#define SYSROOT_BOOT_FLAG_WARM		0x10

#if !SYSMEM_NSKBL
int ksceKernelIsColdBoot(void)
{
	return !(s_sysrootState->bootFlags & SYSROOT_BOOT_FLAG_WARM);
}

/* the current thread's attributes, from the thread manager */
int sceKernelSysrootGetCurrentThreadAttr(uint32_t *attr)
{
#if FW_DEBUG
	int (*callback)(uint32_t *) = s_sysrootState->getCurrentThreadAttr;

	if (callback != NULL)
		return callback(attr);
	ksceKernelAssertLevel(0, callback != NULL, SCE_DBGINFO(490), __builtin_return_address(0));
#endif
	return s_sysrootState->getCurrentThreadAttr(attr);
}

int sceKernelSysrootThreadMgrStartAfterProcess(void)
{
	int ret = s_sysrootState->threadMgrStartAfterProcess();

	if (ret < 0)
		SCE_PANIC(546);
	return ret;
}
#endif

/* the boot-time lock that orders the cores */
void sceKernelSysrootCorelockLock(SceUInt32 core)
{
	ksceKernelCorelockLock(&s_sysrootState->corelock, core);
}

void sceKernelSysrootCorelockUnlock(void)
{
	ksceKernelCorelockUnlock(&s_sysrootState->corelock);
}

#if !SYSMEM_NSKBL
/* name unknown: the names of the boot memblocks (sysroot.c); 4 is also the default */
const char *SysrootGetMemBlockName(unsigned int index)
{
	switch (index) {
	case 0:
		return s_sysrootState->memBlockNames[0];
	case 1:
		return s_sysrootState->memBlockNames[1];
	case 2:
		return s_sysrootState->memBlockNames[2];
	case 3:
		return s_sysrootState->memBlockNames[3];
	case 5:
		return s_sysrootState->memBlockNames[5];
	case 6:
		return s_sysrootState->memBlockNames[6];
	case 7:
		return s_sysrootState->memBlockNames[7];
	case 8:
		return s_sysrootState->memBlockNames[8];
	case 4:
	default:
		return s_sysrootState->memBlockNames[4];
	}
}
#endif

#if SYSMEM_NSKBL
extern int ksceKernelCpuId(void);
extern int InitializeDipsw(SceDipsw *dipsw);

#define SCE_KBL_PARAM_MAGIC	0xcbac03aa
#define SYSROOT_CPU_COUNT	4
#define SYSROOT_NAME_SIZE	32

/* NSKBL: core 0 builds the sysroot in the block SKBL mapped for it */
int SetupCore0(SceKernelBootParam *param)
{
	struct SysrootState *state;
	SceKblParam *kblParam;
	void *block;
	int cpu;

	cpu = ksceKernelCpuId();
	ksceKernelCorelockLock(param->corelock, 0);
	if (cpu == 0) {
		s_sysrootState = param->pSysroot;
		SCE_DBG_ASSERT_CHECK(s_sysrootState == (void *)param->sysroot.vbase, 1024);
		state = s_sysrootState;
		__builtin_memset(state, 0, param->sysroot.psize);
		state->bootHeapCapacity = param->sysroot.psize;
		state->bootHeapCursor = ((uintptr_t)state + sizeof(*state) + 0x1f) & ~0x1fU;
		state->magic = SYSROOT_MAGIC1;
		state->size = sizeof(*state);
		state->magicEnd = SYSROOT_MAGIC2;
		state->bootArgs = param;
		state->bootHeapLock = 0;
#if FW_DEBUG
		AssertSysroot(state);
#endif

		state = s_sysrootState;
		state->perCpuCount = SYSROOT_CPU_COUNT;
		state->perCpu = sceKernelSysrootAlloc(SYSROOT_CPU_COUNT * sizeof(struct SysrootPerCpu));
		block = sceKernelSysrootAlloc(0x2c);
		s_sysrootState->sysrootClass = (uint32_t)block;
		s_sysrootClass = (uint32_t)block;
		sceKernelSysrootSetStatus(1);
		ksceKernelCorelockInitialize(&s_sysrootState->corelock);

		state = s_sysrootState;
		state->reserved1 = 0;
		state->fallbackPUIDEntryHeap = NULL;
		state->memBlockNames[0] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[0], "SceSysrootFixedHeap", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[1] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[1], "SceSysrootHeap", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[2] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[2], "SceSysrootL2", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[3] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[3], "SceSysrootNPL2", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[4] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[4], "SceSysrootKBL", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[5] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[5], "SceSysrootSimpleMemBlock4K", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[6] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[6], "SceSysrootSimpleMemBlock8K8K", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[7] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[7], "SceSysrootSimpleMemBlock16K", SYSROOT_NAME_SIZE);
		s_sysrootState->memBlockNames[8] = sceKernelSysrootAlloc(SYSROOT_NAME_SIZE);
		__builtin_strncpy(s_sysrootState->memBlockNames[8], "SceSysrootSimpleMemBlock64K", SYSROOT_NAME_SIZE);
		s_sysrootState->fallbackAddressSpaceCB = NULL;

		SCE_DBG_ASSERT_CHECK(param->kblParam->magic == SCE_KBL_PARAM_MAGIC, 1094);
		state = s_sysrootState;
		block = sceKernelSysrootAlloc(sizeof(SceKblParam));
		kblParam = param->kblParam;
		state->kblParam = block;
		__builtin_memcpy(s_sysrootState->kblParam, kblParam, sizeof(SceKblParam));
		InitializeDipsw(&s_sysrootState->kblParam->dipsw);

		state = s_sysrootState;
		state->registeredPid = -1;
		state->intrMMUContext = 0;
		state->sharedMemory = 0;
		state->acmgrOps = NULL;
		state->modulemgrOps = NULL;
		state->getNidName = NULL;
		state->processmgrRuntimeOps = NULL;
		state->processmgrOps = NULL;
		state->isUserModeThread = NULL;
		for (int i = 0; i < 14; i++)
			state->modulePrivate[i] = NULL;
		state->threadMgrStartAfterProcess = NULL;
		state->getThreadId = NULL;
		state->getProcessId = NULL;
		state->getCurrentProcessCB = NULL;
		state->getCurrentAddressSpaceCB = NULL;
		state->getCurrentUIDEntryHeapCB = NULL;
		state->returnFromExcpToThread = NULL;
		state->getThreadAccessLevel = NULL;
		state->trapThreadAfterSyscall = NULL;
		state->getCurrentThreadAttr = NULL;
		state->busErrorHandler = NULL;
		state->sysStateEvent = NULL;
		state->sendSystemError = NULL;
		state->dfmgrOps = NULL;
		state->processEventOps = NULL;
		state->iofilemgrStart = NULL;
		state->appMgrSpawnProcess = NULL;
		state->getShellPid = NULL;
		state->coredumpTrigger = NULL;
		state->coredumpCancel = NULL;
		state->licMgrGetLicenseStatus = NULL;
		state->utMgrHasPsmTestFlag = NULL;
		state->utMgrHasSystemDataFilePlayReadyFlag = NULL;
		state->utMgrHasAuthId = NULL;
		state->utMgrGetTrilithiumBuffer = NULL;
		state->utMgrHasNpTestFlag = NULL;
		state->getSystemSwVersion = NULL;
		state->ledSetMode = NULL;
		state->selfImages = NULL;
		state->dbgpOps = NULL;
		state->dbgpNetAttach = NULL;
		state->dbgpNetDetach = NULL;
		state->tmcpCommand = NULL;
		state->sdbgpNetAttach = NULL;
		state->sdbgpNetDetach = NULL;
		state->appMgrGetAppInfoByPid = NULL;
		state->appMgrGetAppInfoByAppId = NULL;
		state->appMgrGetAppList = NULL;
		state->pfsMgrGetMountInfo = NULL;
		state->debugNotifiers = NULL;
		state->profiler = NULL;
		state->getTimebaseValue = NULL;
		state->getTimebaseRegister = NULL;
		state->sharedTime = (volatile SceLongTimerRegisters *)TIMER_LONG_REG_BASE(5);
		state->initCallbackLock = 0;
		state->initCallbackBase = 0;
		state->modelInfo = param->hardwareDependent[0];
		state->modelCapability = param->hardwareDependent[1];
		state->factorySystemSwVersion = param->socRevision;
		state->pervasiveDword = param->socCacheStatus;
		state->kermitRevision = param->kermitRevision;
#if FW_DEBUG
		AssertSysroot(state);
#endif
	}
	ksceKernelCorelockUnlock(param->corelock);
	return 0;
}
#endif
