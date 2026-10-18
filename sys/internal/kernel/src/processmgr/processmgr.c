/*
 * processmgr.c - processes: creation, start, suspend/resume, termination,
 * resource recovery, process local storage and the process queries
 */

#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include "processmgr_internal.h"
#include <modulemgr/modulemgr_patch.h>
#include <processmgr/processmgr.h>
#include <processmgr/process_info.h>
#include <sysmem/sysroot_processmgr.h>

/* the process private memory (PPM): per slot one 0x4000 byte area for the
 * address space and one 0x1000 byte area for process objects */
typedef struct PPMArea {
	SceUIntPtr cur;
	SceUIntPtr end;
} PPMArea;

typedef struct PPMTable {
	SceUIntPtr base0;			/* SceKernelPPM0: 0x4000 per slot */
	SceUIntPtr base1;			/* SceKernelPPM1: 0x1000 per slot */
	PPMArea slot[PROCESS_MAX];
} PPMTable;

/* a request queued to the process event thread */
typedef struct ProcessRequest {
	ListNode node;
	SceProcessObject *process;
	SceUInt32 request;
} ProcessRequest;

/* thread ids of a process, collected under the vector's lock */
typedef struct ThreadIdVector {
	SceUID id[PROCESS_THREAD_VECTOR_MAX];
	SceKernelSpinlock lock;
} ThreadIdVector;

typedef struct ProcEventStateParam {
	SceSize size;				/* 0x14 */
	SceUID pid;
	SceUInt32 state;
	SceUInt32 request;
	SceUInt32 type;
} ProcEventStateParam;

/* the record k_sceEventLogPut gets for process events */
typedef struct ProcessEventLog {
	int result;
	SceUID pid;
	SceUInt32 type;
	SceSize nameLength;
	char name[12];
} ProcessEventLog;

/* ExitSpawn: the process the next CreateProcess inherits */
typedef struct ExitSpawnState {
	SceSelfAuthInfo authInfo;
	SceUID pid;
	SceProcessObject *process;
} ExitSpawnState;

/* a static probe (the module's NID 0 probes table) */
typedef struct ProcessProbe {
	SceUInt32 *id;
	const char *provider;
	const char *name;
	const void *site;
	void (*volatile *handler)(const struct ProcessProbe *probe, ...);
	SceUInt32 reserved[5];
} ProcessProbe;

typedef struct ProcessProbesInfo {
	SceUInt32 version;
	SceSize count;
} ProcessProbesInfo;

#define PROCESS_PROBE_SECTION	"processmgr_probes"

/*
 * a static probe site: the descriptor goes to the probes table, the handler
 * is called while one is attached
 */
#define PROCESS_PROBE(_name, ...) do {						\
	__label__ site;								\
	static SceUInt32 id;							\
	static void (*volatile handler)(const ProcessProbe *probe, ...);	\
	static const ProcessProbe probe = {					\
		&id, "__proc_", _name, &&site, &handler,			\
	};									\
	static const ProcessProbe *const entry					\
	__attribute__((section(PROCESS_PROBE_SECTION), used)) = &probe;	\
site:										\
	if (handler != NULL)							\
		handler(&probe, ##__VA_ARGS__);					\
} while (0)

SceClass g_processClass;

/* what sysroot keeps of the process manager (sceKernelSysrootSetModulePrivate) */
static struct {
	SceClass *processClass;
	SceProcessObject *kernelProcess;
} g_processmgr;

static SceKernelSpinlock g_localStorageLock;
static void (*g_processDeleteCallback)(SceUID pid, int exitStatus);

static ProcessLocalStorageName g_localStorageName[PROCESS_LOCAL_STORAGE_MAX];
static SceSize g_localStorageSize[PROCESS_LOCAL_STORAGE_MAX];

static SceUInt32 g_processCount;
static SceKernelSpinlock g_processListLock;
static ListNode g_processList;

static SceUInt32 g_ppmUsed;
static PPMTable g_ppm;
static SceKernelSpinlock g_ppmLock;

static ThreadIdVector g_threadIdVector;
static ProcessHwBreakpoints g_kernelPhbp;
static ProcessHwWatchpoints g_kernelPhwp;
static int g_objectHeapLimitsSet;

static SceProcessObject g_kernelProcessObject;

static ListNode g_requestList;
static SceUID g_eventThread;
static SceUID g_eventFlag;
static SceKernelSpinlock g_requestLock;
static SceUInt32 g_requestCount;
static SceUID g_requestSema;
static volatile SceInt32 g_sgiEnter;
static volatile SceInt32 g_sgiLeave;
static SceUInt32 g_sgiScan;
static SceProcessObject *g_sgiProcess;

static ProcessDebugRegPair g_defaultPhwp[4];

static ListNode g_recoveryList;
static SceKernelSpinlock g_recoveryLock;
static SceUID g_recoveryThread;
static SceUID g_recoveryEventFlag;

static ProcessDebugRegPair g_kernelContextBreakpoint;
static SceClass *g_threadClass;
static ThreadIdVector g_excpThreadIdVector;
static ThreadIdVector g_intrThreadIdVector;

static ExitSpawnState g_exitSpawn;
static ProcessDebugRegPair g_defaultPhbp[5];

/* the titles that get other thread manager NIDs */
static const ModulemgrNidPatch g_threadmgrPatch[] = {
	{ 0x4b675d05, 0x97c4a7c4 },	/* sceKernelDelayThread */
	{ 0x9c0180e1, 0x34938242 },	/* sceKernelDelayThreadCB */
};

static const ModulemgrLibraryPatch g_threadmgrLibraryPatch = {
	"SceThreadmgr", 0, ARRAY_SIZE(g_threadmgrPatch), g_threadmgrPatch,
};

static const ModulemgrTitlePatch g_titlePatch[] = {
	{ "PCSG00063", 0, 1, &g_threadmgrLibraryPatch },
	{ "PCSB00144", 0, 1, &g_threadmgrLibraryPatch },
	{ NULL, 0, 0, NULL },
};

/* the static probes (NID 0 variables 0x9318D9DD and 0x8CE938B1) */
extern const ProcessProbe *const __start_processmgr_probes[];
const ProcessProbesInfo module_nid0_9318D9DD = { 1, 5 };

static inline void ListInit(ListNode *head)
{
	head->next = head;
	head->prev = head;
}

static inline void ListRemove(ListNode *node)
{
	node->next->prev = node->prev;
	node->prev->next = node->next;
}

static inline void ListAddTail(ListNode *head, ListNode *node)
{
	node->next = head;
	node->prev = head->prev;
	head->prev = node;
	node->prev->next = node;
}

void *PPMAllocMemory(SceUInt32 slot, SceSize size)
{
	PPMArea *area = &g_ppm.slot[slot];
	SceUIntPtr p = area->cur;

	if (p + size >= area->end)
		return NULL;
	area->cur = (p + size + 0x1f) & ~0x1f;
	return (void *)p;
}

/* name unknown: the process private memory is released with the process */
void PPMFreeMemory(void *slot, void *object)
{
}

/* the user mode addresses _sceKernelRegisterLibkernelAddresses gave */
static void *ProcessGetLibkernelAddr0(SceProcessObject *process)
{
	return process->libkernelAddr[0];
}

static void *ProcessGetLibkernelAddr1(SceProcessObject *process)
{
	return process->libkernelAddr[1];
}

static void *ProcessGetLibkernelAddr2(SceProcessObject *process)
{
	return process->libkernelAddr[2];
}

/* name unknown */
static void PPMFreeSlot(SceUInt32 slot)
{
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&g_ppmLock);
	g_ppmUsed &= ~(1 << (slot & 0xff));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_ppmLock, intr);
}

int SceProcessmgrForDriver_3801D7D6(const char *name, SceSize size)
{
	int intr, i;

	intr = SceCpuForDriver_D32ACE9E(&g_localStorageLock);
	for (i = 2; i != PROCESS_LOCAL_STORAGE_MAX; i++) {
		if (g_localStorageSize[i] == 0) {
			g_localStorageSize[i] = size;
			strncpy(g_localStorageName[i].name, name, 0x1f);
			g_localStorageName[i].name[0x1f] = '\0';
			goto out;
		}
	}
	i = SCE_KERNEL_ERROR_PROCESSMGR_NO_LOCAL_STORAGE;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_localStorageLock, intr);
	return i;
}

/* name unknown: tell the handlers a process starts suspending */
static void ProcessNotifySuspending(SceProcessObject *process, SceUInt32 state, SceUInt32 kind)
{
	ProcEventParam param;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	process->status |= PROCESS_FLAG_SUSPENDING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	param.size = sizeof(param);
	param.state = state;
	param.kind = kind;
	param.type = process->type;
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_SUSPEND, 0x400, &param, NULL, 0);
}

/* name unknown: the SceUIDProcessClass constructor */
static int ProcessConstructor(SceProcessObject *process)
{
	ListNode *node;
	int intr, i;

	process->magic = PROCESS_MAGIC;
	process->phbp = NULL;
	process->phwp = NULL;
	process->contextBreakpoint = NULL;
	process->attr = 0;
	process->entryHeapObject = NULL;
	process->cpuAffinity = 0;
	process->addressSpaceCB = NULL;
	process->exitStatus = 0;
	process->infoParam = 0;
	process->libdb = NULL;
	process->image.sdkVersion = 0;
	process->image.mainThreadName[0] = '\0';
	process->image.mainThreadPriority = 0;
	process->image.mainThreadStackSize = 0;
	process->image.mainThreadAttr = 0;
	process->image.mainThreadCpuAffinityMask = 0;
	process->image.processName[0] = '\0';
	process->lockSema = -1;
	process->eventFlag = -1;
	process->eventSema = -1;
	process->entryHeap = -1;
	process->app = -1;
	process->addressSpace = -1;
	process->pid = -1;
	process->parentPid = -1;
	process->budgetId = -1;
	process->mainThread = -1;
	process->exitThread = -1;
	process->uniqueHeap = -1;
	process->kplsHeap = -1;
	process->cdialogHeap = -1;
	process->toolHeap = -1;
	process->image.processPreloadDisabled = 0;
	process->image.processParam = NULL;
	memset(process->image.mainThreadOptParam, 0, sizeof(process->image.mainThreadOptParam));
	process->stdioFd[0] = -1;
	process->stdioFd[1] = -1;
	process->stdioFd[2] = -1;
	process->stdioFd[3] = -1;
	process->stdioPuid[0] = -1;
	process->stdioPuid[1] = -1;
	process->stdioPuid[2] = -1;
	process->stdioPuid[3] = -1;
	memset(&process->authInfo, 0, sizeof(process->authInfo));
	process->lock = 0;
	process->threadPermission = 0;
	process->status = 0;
	process->libkernelAddr[0] = NULL;
	process->libkernelAddr[1] = NULL;
	process->libkernelAddr[2] = NULL;
	process->libkernelAddr[3] = NULL;
	process->libkernelAddr[4] = NULL;
	process->libkernelAddr[5] = NULL;
	process->syscallTraceFlag = 0;
	process->prevState = 0;
	process->pendingRequests = 0;
	process->reserved2[0] = 0;
	process->reserved2[1] = 0;
	process->reserved2[2] = 0;
	process->localStorageLock = 0;
	process->dtraceFlags = 4;
	for (i = 0; i != PROCESS_LOCAL_STORAGE_MAX; i++)
		process->localStorage[i] = NULL;
	process->powerLock = 0;
	process->powerLockCount = 0;
	process->threadsStopped = 0;
	process->callbackLock = 0;
	process->callbacks = NULL;
	process->preloadThread = 0;
	process->threads.next = NULL;
	process->threads.prev = NULL;
	process->threadCount = 0;
	process->threadPeak = 0;
	process->unlimitedThreadCount = 0;
	process->unlimitedThreadPeak = 0;
	process->msgPipes.next = NULL;
	process->msgPipes.prev = NULL;
	memset(&process->msgPipeLock, 0, sizeof(process->msgPipeLock));
	process->lwMutexCacheLock = 0;
	process->abortCode = 0;
	process->lwMutexCache = NULL;
	process->lwMutexCacheCount = 0;
	process->abortRequested = 0;
	process->exception = NULL;
	process->resourceLock = 0;
	process->uidCount = 0;
	process->uidPeak = 0;
	process->uidUnlimitedCount = 0;
	process->uidUnlimitedPeak = 0;
	process->callbackCount = 0;
	process->callbackPeak = 0;
	process->threadEventCount = 0;
	process->threadEventPeak = 0;
	process->condCount = 0;
	process->condPeak = 0;
	process->eventFlagCount = 0;
	process->eventFlagPeak = 0;
	process->lwCondCount = 0;
	process->lwCondPeak = 0;
	process->lwMutexCount = 0;
	process->lwMutexPeak = 0;
	process->msgPipeCount = 0;
	process->msgPipePeak = 0;
	process->mutexCount = 0;
	process->mutexPeak = 0;
	process->rwLockCount = 0;
	process->rwLockPeak = 0;
	process->workerThread = -1;
	process->abortEventFlag = -1;
	process->abortThread = -1;
	process->exceptionId = -1;
	process->semaCount = 0;
	process->coredumpStack = -1;
	process->semaPeak = 0;
	process->simpleEventCount = 0;
	process->simpleEventPeak = 0;
	process->timerCount = 0;
	process->timerPeak = 0;
	for (i = 0; i != 6; i++)
		process->reserved3[i] = 0;
	process->uidLwCount = 0;
	process->uidLwPeak = 0;
	process->coredumpLock = 0;
	process->coredumpHandler = NULL;
	process->coredumpStackSize = 0;
	process->coredumpArg = NULL;
	process->msgPipeAsyncCount = 0;
	process->msgPipeMemory = 0;
	process->moduleCount[0] = 0;
	process->moduleCount[1] = 0;
	process->limit[0] = 0;
	process->limit[1] = 0;
	for (i = 0; i != 8; i++) {
		process->resource[i].lock = 0;
		process->resource[i].count = 0;
		process->resource[i].peak = 0;
		process->resource[i].limit = -1;
	}
	node = &process->node;
	process->timeLock = 0;
	process->time = 0;
	process->timeRunning = 0;
	process->threadActionCursor = 0;
	ListInit(node);
	memset(process->TitleId, 0, sizeof(process->TitleId));
	process->kplsBlock = -1;
	process->uniqueHeapCB = NULL;
	process->cdialogHeapCB = NULL;
	process->toolMemory = NULL;
	process->objectHeap[0] = NULL;
	process->objectHeap[1] = NULL;
	intr = SceCpuForDriver_D32ACE9E(&g_processListLock);
	ListAddTail(&g_processList, node);
	g_processCount++;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
	return 0;
}

static int DeleteProcess(SceProcessObject *process)
{
	int ret;

	sceKernelSetEventFlag(process->eventFlag, PROCESS_EVF_NOT_DEBUG_SUSPENDED);
	if (process->type != PROCESS_TYPE_SHELL || SceDipswForDriver_A98FC2FD(0xc2) != 0)
		SceModulemgrForKernel_E71530D7(process->pid);
	process->status = (process->status & ~PROCESS_STATE_MASK) | PROCESS_STATE_EXITED;
	sceKernelSetEventFlag(process->eventFlag, PROCESS_EVF_DELETED);
	ret = sceKernelThreadMgrProcessDeleteBeforePUIDClose(process);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 4180,
			"*** error: DeleteProcess() -> sceKernelThreadMgrProcessDeleteBeforePUIDClose()=0x%08X",
			ret);
	return sceGUIDClose(process->pid);
}

/* name unknown: take a free process private memory slot */
static int PPMAllocSlot(void)
{
	SceUInt32 slot;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&g_ppmLock);
	slot = __builtin_ctz(~g_ppmUsed);
	if (slot < PROCESS_MAX) {
		g_ppmUsed |= 1 << slot;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_ppmLock, intr);
		g_ppm.slot[slot].cur = g_ppm.base1 + slot * 0x1000;
		g_ppm.slot[slot].end = g_ppm.base1 + slot * 0x1000 + 0x1000;
		return slot;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_ppmLock, intr);
	return SCE_KERNEL_ERROR_PROCESSMGR_NO_SLOT;
}

static int ProcessThreadSuspend(SceUID thid, SceUInt32 state)
{
	int ret;

	switch (state) {
	case PROCESS_STATE_SUSPENDED:
		ret = SceThreadmgrForKernel_15D6357C(thid, 0xf);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 779, "sceKernelSuspendThread failed 0x%08x\n", ret);
			return ret;
		}
		return 0;
	case 0x11:
		ret = sceKernelDebugSuspendThread(thid, 2);
		break;
	case 0x12:
		ret = sceKernelDebugSuspendThread(thid, 0x1000);
		break;
	case 0x13:
		ret = sceKernelDebugSuspendThread(thid, 0x100);
		break;
	case 0x14:
		ret = sceKernelDebugSuspendThread(thid, 0x200);
		break;
	case 0x15:
		ret = sceKernelDebugSuspendThread(thid, 0x400);
		break;
	default:
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 773, "unexpected status 0x%08x\n", state);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (ret == SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 784, "thread is already suspended (tid 0x%08x)\n", thid);
	} else if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 787, "sceKernelDebugSuspendThread(0x%08x) failed 0x%08x\n",
				     thid, ret);
		return ret;
	}
	return 0;
}

/* free everything the process still holds; deleteFromBudget is 0 when the object is just destroyed */
static int ProcessResourceRecovery(SceProcessObject *process, int destroyed)
{
	ProcessTerminationCallback *callback;
	ProcessBudget *budget;
	int intr, ret, i;

	intr = SceCpuForDriver_D32ACE9E(&process->callbackLock);
	for (callback = process->callbacks; callback != NULL; callback = callback->next)
		sceKernelNotifyCallback(callback->cb, process->pid);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->callbackLock, intr);

	if (process->entryHeap != -1) {
		ret = SceSysmemForKernel_133024A5(process->entryHeap);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2817, "sceUIDEntryHeapCloseAllPUID failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2818);
			return ret;
		}
		ret = SceSysmemForKernel_F6D09BD9(process->entryHeap);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2825, "Delete Entry Heap failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2826);
			return ret;
		}
	}
	ret = sceKernelThreadMgrProcessDeleteAfterPUIDClose(process);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2834,
			"*** error: ProcessResourceRecovery() -> sceKernelThreadMgrProcessDeleteAfterPUIDClose()=0x%08X",
			ret);
	SceSysmemForKernel_CAB15F74(process->pid);
	if (process->coredumpStack > 0) {
		ret = SceSysmemForDriver_009E1C61(process->coredumpStack);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 2842);
		process->coredumpStack = -1;
	}
	if (process->kplsHeap != -1) {
#if FW_DEBUG
		if (SceSblACMgrForKernel_BBA13D9C() != 0) {
			sceKernelVerifyHeap(process->kplsHeap, 0);
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2850, "process KPLS heap verified\n");
		}
#endif
		ret = sceKernelDeleteHeap(process->kplsHeap);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2856, "Delete Heap failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2857);
			return ret;
		}
	}
	if (process->kplsBlock != -1) {
		ret = SceSysmemForDriver_009E1C61(process->kplsBlock);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2865, "Delete Block for heap failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2866);
			return ret;
		}
	}
	if (process->addressSpace != -1) {
		ret = sceKernelAddressSpaceFreeAllMemBlock(process->addressSpace);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2876,
					     "sceKernelAddressSpaceFreeAllMemBlock failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2877);
			return ret;
		}
		ret = SceSysmemForKernel_4A2E421D(process->addressSpace);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2884, "_DeleteAddressSpace failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2885);
			return ret;
		}
	}
	if (process->objectHeap[1] != NULL && process->type != PROCESS_TYPE_MINI_APP) {
		ret = SceSysmemForKernel_2F526AB8(process->objectHeap[1]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2894, "sceKernelDeleteObjectHeap failed 0x%08x\n", ret);
			return ret;
		}
	}
	if (SceDipswForDriver_A98FC2FD(0xe4) != 0 && process->phbp != NULL) {
		ret = sceKernelFreeHeapMemory(process->toolHeap > 0 ? process->toolHeap
								     : process->uniqueHeap,
					       process->phbp);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2907, "sceKernelFreeHeapMemory failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2908);
		}
		process->phbp = NULL;
		process->phwp = NULL;
		process->contextBreakpoint = NULL;
	}
	if (SceDipswForDriver_A98FC2FD(0xd2) != 0) {
		if (process->toolHeap != -1) {
			ret = sceKernelDeleteHeap(process->toolHeap);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 2920, "Delete ToolHeap failed. 0x%08x\n", ret);
				SCE_DBG_ASSERT_FAIL(2921);
				return ret;
			}
		}
#if FW_VERSION >= 0x04000000
		if (process->toolMemory != NULL)
			sceKernelFreeHeapMemory(TOOL_HEAP, process->toolMemory);
#endif
	}
	if (process->cdialogHeap != -1) {
#if FW_DEBUG
		if (SceSblACMgrForKernel_BBA13D9C() != 0) {
			sceKernelVerifyHeap(process->cdialogHeap, 0);
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2935, "process CDialog heap verified\n");
		}
#endif
		ret = sceKernelDeleteHeap(process->cdialogHeap);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2941, "Delete Heap failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2942);
			return ret;
		}
	}
	if (process->msgPipeLock.next != NULL)
		sceKernelFinalizeFastMutex((SceKernelFastMutex *)&process->msgPipeLock);
	if (process->objectHeap[0] != NULL && process->type != PROCESS_TYPE_MINI_APP) {
		ret = SceSysmemForKernel_2F526AB8(process->objectHeap[0]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2959, "sceKernelDeleteObjectHeap failed 0x%08x\n", ret);
			return ret;
		}
	}
	if (process->uniqueHeap != -1) {
#if FW_DEBUG
		if (SceSblACMgrForKernel_BBA13D9C() != 0) {
			sceKernelVerifyHeap(process->uniqueHeap, 0);
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2969, "process unique heap verified\n");
		}
#endif
		ret = sceKernelDeleteHeap(process->uniqueHeap);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2975, "Delete Heap failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2976);
			return ret;
		}
	}
	if (process->eventSema != -1) {
		ret = sceKernelDeleteSema(process->eventSema);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 2988, "sceKernelDeleteSema failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2989);
			return ret;
		}
	}
	if (process->lockSema != -1) {
		ret = sceKernelDeleteSema(process->lockSema);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 2997, "sceKernelDeleteSema failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2998);
			return ret;
		}
	}
	if (process->eventFlag != -1) {
		ret = sceKernelDeleteEventFlag(process->eventFlag);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3006, "sceKernelDeleteEventFlag failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(3007);
			return ret;
		}
	}
	if (destroyed != 1) {
		if (process->budgetId != -1) {
			ret = BudgetIDtoBudgetObj(process->budgetId, &budget);
			if (ret < 0) {
				SCE_DBG_ASSERT_FAIL(3016);
				return ret;
			}
			BudgetLock(budget);
			for (i = 0; i != PROCESS_MAX; i++) {
				if (process->pid == budget->process[i]) {
					budget->process[i] = 0;
					break;
				}
			}
			budget->processCount--;
			BudgetUnlock(budget);
		}
		if (g_processDeleteCallback != NULL)
			g_processDeleteCallback(process->pid, process->exitStatus);
	}
	process->magic = 0;
	PPMFreeSlot(process->slot);
	g_processCount--;
	return 0;
}

/* the SceKernelResourceRecovery thread */
static int sceKernelProcessResourceRecovery(SceSize args, void *argp)
{
	SceProcessObject *process;
	ListNode *node;
	int intr, ret;

	(void)args;
	(void)argp;
	while ((ret = sceKernelWaitEventFlag(g_recoveryEventFlag, 1, 5, NULL, NULL)) >= 0) {
		for (;;) {
			intr = SceCpuForDriver_D32ACE9E(&g_recoveryLock);
			node = g_recoveryList.next;
			if (node == &g_recoveryList)
				break;
			ListRemove(node);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_recoveryLock, intr);
			process = PROCESS_FROM_RECOVERY(node);
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2516, "pProcess %p, uid 0x%08x\n", process, process->pid);
			ret = ProcessResourceRecovery(process, 0);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 2531, "ProcessResourceRecovery failed. 0x%08x\n", ret);
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_recoveryLock, intr);
	}
	SCE_DBG_ASSERT_FAIL(2535);
	return ret;
}

/* the 10 second timer the event thread arms while a process terminates */
static SceUInt32 ProcessEventTimeupHandler(SceUID timer, void *arg)
{
	(void)timer;
	(void)arg;
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 671,
		"process terminate processing has timed out(%d sec). priority change %d => %d\n",
		10, 0x60, 0x3f);
	sceKernelChangeThreadPriority2(g_eventThread, 0x3f);
	return 0;
}

/* name unknown: close the standard I/O OpenStdio opened */
static void CloseStdio(SceProcessObject *process)
{
	SceUID pid;
	int permission;

	permission = sceKernelSetThreadAccessLevel(0x80);
	pid = SceThreadmgrForDriver_0486F239(KERNEL_PID);
	if (process->stdioPuid[0] != -1)
		SceSysmemForDriver_84A4AF5E(process->pid, process->stdioPuid[0]);
	if (process->stdioFd[0] != -1)
		sceIoClose(process->stdioFd[0]);
	if (process->stdioPuid[1] != -1)
		SceSysmemForDriver_84A4AF5E(process->pid, process->stdioPuid[1]);
	if (process->stdioFd[1] != -1)
		sceIoClose(process->stdioFd[1]);
	if (process->stdioPuid[2] != -1)
		SceSysmemForDriver_84A4AF5E(process->pid, process->stdioPuid[2]);
	if (process->stdioFd[2] != -1)
		sceIoClose(process->stdioFd[2]);
	if (process->stdioPuid[3] != -1)
		SceSysmemForDriver_84A4AF5E(process->pid, process->stdioPuid[3]);
	if (process->stdioFd[3] != -1)
		sceIoClose(process->stdioFd[3]);
	SceThreadmgrForDriver_0486F239(pid);
	sceKernelSetThreadAccessLevel(permission);
}

/* SGI 6: every other core waits here while _SuspendProcess scans the threads */
static int sceKernelProcessmgrSgiHandler(int code, void *arg)
{
	int intr, ret;

	(void)code;
	(void)arg;
	SceCpuForDriver_5F6A8743(&g_sgiEnter, 1);
	__asm__ volatile("dsb" ::: "memory");
	while (g_sgiEnter != 0)
		__asm__ volatile("wfe");
	if (g_sgiScan == 1) {
		intr = SceCpuForDriver_821FC0EE();
		ret = sceKernelCpuScanThreadForProcessmgr(g_sgiProcess->pid);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 880,
				"error: sceKernelCpuScanThreadForProcessmgr(0x%08X)=0x%08X\n",
				g_sgiProcess->pid, ret);
	}
	SceCpuForDriver_5F6A8743(&g_sgiLeave, 1);
	__asm__ volatile("dsb" ::: "memory");
	return -1;
}

/* the release builds keep this one out of line, the debug build inlines it */
#if FW_DEBUG
#define PROCESSMGR_RELEASE_NOINLINE
#else
#define PROCESSMGR_RELEASE_NOINLINE	__attribute__((noinline))
#endif

/* name unknown: the process of the current thread */
static PROCESSMGR_RELEASE_NOINLINE int CurrentProcess(SceProcessObject **process)
{
	ThreadCB *cb = getCurrentThreadCB();

	if (cb == NULL)
		return SCE_KERNEL_ERROR_PROCESSMGR_UNKNOWN_UID;
	*process = cb->process;
	return 0;
}

static int UIDtoProcess(SceUID pid, SceProcessObject **process)
{
	ThreadCB *cb;
	SceClass *cls;

	if (pid == 0)
		return CurrentProcess(process);
	if (pid == KERNEL_PID) {
		*process = g_processmgr.kernelProcess;
		return 0;
	}
	if (pid == SHELL_DUMMY_PID || pid == NPDRM_DUMMY_PID) {
		cls = SceSysmemForDriver_85336A1C();
	} else {
		cb = getCurrentThreadCB();
		if (cb != NULL && cb->processId == pid) {
			*process = cb->process;
			return 0;
		}
		cls = &g_processClass;
	}
	if (SceSysmemForKernel_0E43E113(pid, cls, process) < 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_UNKNOWN_UID;
	return *process != NULL ? 0 : SCE_KERNEL_ERROR_PROCESSMGR_UNKNOWN_UID;
}

static inline __attribute__((always_inline)) int AssertProcess(SceProcessObject *process)
{
	if (process == NULL) {
		SCE_DBG_ASSERT_FAIL(495);
		return 0;
	}
	if (process->magic != PROCESS_MAGIC) {
		SCE_DBG_ASSERT_FAIL(499);
		return 0;
	}
	return 1;
}

inline __attribute__((always_inline)) SceProcessObject *GetCurrentProcess(void)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcess(0, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3057, "UIDtoProcess(SELF) failed 0x%08x\n", ret);
		return NULL;
	}
	SCE_DBG_ASSERT(AssertProcess(process), 3060);
	return process;
}

/* name unknown: a free slot in the event thread's request permission */
static int PollRequestPermission(void)
{
	return sceKernelPollSema(g_requestSema, 1);
}

static int UIDtoProcessAndRefer(SceUID pid, SceProcessObject **process)
{
	SceProcessObject *p;
	ThreadCB *cb;
	SceClass *cls;

	if (pid == 0) {
		cb = getCurrentThreadCB();
		if (cb == NULL)
			return SCE_KERNEL_ERROR_PROCESSMGR_UNKNOWN_UID;
	} else {
		if (pid == KERNEL_PID) {
			p = g_processmgr.kernelProcess;
			*process = p;
			goto refer;
		}
		if (pid == SHELL_DUMMY_PID || pid == NPDRM_DUMMY_PID) {
			cls = SceSysmemForDriver_85336A1C();
			goto refer_class;
		}
		cb = getCurrentThreadCB();
		if (cb == NULL || cb->processId != pid) {
			cls = &g_processClass;
refer_class:
			if (SceSysmemForDriver_00ED6C14(pid, cls, process) < 0)
				return SCE_KERNEL_ERROR_PROCESSMGR_UNKNOWN_UID;
			return 0;
		}
	}
	p = cb->process;
	*process = p;
refer:
	if (SceSysmemForDriver_00ED6C14(p->pid, &g_processClass, NULL) < 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_UNKNOWN_UID;
	return 0;
}

/* name unknown: the address space of a process */
SceUID SceProcessmgrForKernel_9BC44974(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret >= 0) {
		ret = process->addressSpace;
		sceGUIDReleaseObject(process->pid);
	}
	return ret;
}

int SceProcessmgrForDriver_AF80F39C(SceUID pid, SceUInt32 key, void **addr, int create)
{
	SceProcessObject *process;
	void *storage;
	SceSize size;
	int intr, ret;

	if (key >= PROCESS_LOCAL_STORAGE_MAX)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	storage = process->localStorage[key];
	if (storage != NULL) {
		ret = 0;
	} else if (create == 0) {
		ret = 0;
	} else {
		size = g_localStorageSize[key];
		if (size == 0) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		} else {
			intr = SceCpuForDriver_D32ACE9E(&process->localStorageLock);
			storage = process->localStorage[key];
			if (storage != NULL) {
				ret = 0;
			} else {
				storage = SceSysmemForDriver_7B4CB60A(process->kplsHeap, size);
				if (storage == NULL) {
					ret = SCE_KERNEL_ERROR_NO_FREE_HEAP;
				} else {
					memset(storage, 0, size);
					ret = 0;
					process->localStorage[key] = storage;
				}
			}
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->localStorageLock, intr);
		}
	}
	*addr = storage;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

void *SceProcessmgrForDriver_EE694840(SceUInt32 key)
{
	ThreadCB *cb = getCurrentThreadCB();
	void *storage;

	if (cb == NULL || key >= PROCESS_LOCAL_STORAGE_MAX)
		return NULL;
	storage = cb->process->localStorage[key];
	if (storage == NULL
	    && SceProcessmgrForDriver_AF80F39C(0, key, &storage, 1) < 0)
		return NULL;
	return storage;
}

int sceKernelGetProcessSelfAuthInfo(SceUID pid, SceSelfAuthInfo *authInfo)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6803, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	memcpy(authInfo, &process->authInfo, sizeof(*authInfo));
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: the SDK version of a process */
int SceProcessmgrForDriver_D141C076(SceUID pid, SceUInt32 *sdkVersion)
{
	SceProcessObject *process;
	int ret;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	*sdkVersion = process->image.sdkVersion;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: the name of a process */
static int ProcessGetName(SceUID pid, char *name, SceSize size)
{
	SceProcessObject *process;
	int ret;

	if (name == NULL || size > sizeof(process->TitleId))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret >= 0) {
		strncpy(name, process->TitleId, size);
		sceGUIDReleaseObject(process->pid);
	}
	return ret;
}

static int GetUIDEntryHeap(SceUID pid, void **entryHeap)
{
	SceProcessObject *process;
	ThreadCB *cb;
	int ret;

	if (pid == 0) {
		cb = getCurrentThreadCB();
		if (cb != NULL && (process = cb->process) != NULL)
			goto found;
	} else {
		process = g_processmgr.kernelProcess;
		if (pid == KERNEL_PID)
			goto found;
		cb = getCurrentThreadCB();
		if (cb != NULL && cb->processId == pid && (process = cb->process) != NULL) {
			*entryHeap = process->entryHeapObject;
			return 0;
		}
	}
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5524, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		*entryHeap = NULL;
		return ret;
	}
	*entryHeap = process->entryHeapObject;
	sceGUIDReleaseObject(process->pid);
	return 0;
found:
	*entryHeap = process->entryHeapObject;
	return 0;
}

static void *sceKernelGetObjectHeap(SceUID pid, int kind)
{
	SceProcessObject *process;
	void *heap;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7243, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7244);
		return NULL;
	}
	if (kind == 0) {
		heap = process->objectHeap[0];
		sceGUIDReleaseObject(process->pid);
	} else if (kind == 1) {
		heap = process->objectHeap[1];
		sceGUIDReleaseObject(process->pid);
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7256, "Unknown subbudget type %d\n", kind);
		sceGUIDReleaseObject(process->pid);
		heap = NULL;
	}
	return heap;
}

static int sceKernelPermitProcEvent(SceUID pid)
{
	SceProcessObject *process;
	SceUID sema;
	int ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5375, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	sema = process->eventSema;
	sceGUIDReleaseObject(process->pid);
	ret = sceKernelSignalSema(sema, 1);
	return ret < 0 ? ret : 0;
}

static int sceKernelInhibitProcEvent(SceUID pid)
{
	SceProcessObject *process;
	SceUID sema;
	int ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5346, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	sema = process->eventSema;
	sceGUIDReleaseObject(process->pid);
	ret = sceKernelWaitSema(sema, 1, NULL);
	return ret < 0 ? ret : 0;
}

static void *sceKernelAllocRemoteProcessHeapForSysroot(SceUID pid, SceSize size, void *opt,
						       const void *caller)
{
	SceProcessObject *process;
	void *ptr;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7045, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7046);
		return NULL;
	}
	ptr = SceSysmemForDriver_B415B5A8(process->uniqueHeap, size, opt, caller);
	sceGUIDReleaseObject(process->pid);
	return ptr;
}

int sceKernelFreeRemoteProcessHeap(SceUID pid, void *ptr)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7063, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7064);
		return ret;
	}
	ret = sceKernelFreeHeapMemory(process->uniqueHeap, ptr);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* name unknown: add (unregister 0) or remove a process termination callback */
static int ProcessTerminationCallbackCore(SceUID pid, SceUID cb, int unregister)
{
	ProcessTerminationCallback *callback, *prev;
	SceProcessObject *process;
	SceUInt32 state;
	SceUID guid;
	int intr, ret;

	state = ProcessSyscallEnter();
	if (pid == 0 || cb == 0)
		goto invalid;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		goto out;
	guid = SceSysmemForDriver_45D22597(0, cb);
	if (guid < 0) {
		ret = guid;
		sceGUIDReleaseObject(process->pid);
		goto out;
	}
	ret = sceKernelGetThreadmgrUIDClass(guid);
	if (ret < 0) {
		sceGUIDReleaseObject(process->pid);
		goto out;
	}
	if (ret != 8) {
		sceGUIDReleaseObject(process->pid);
		goto invalid;
	}
	if (unregister == 0) {
		intr = SceCpuForDriver_D32ACE9E(&process->callbackLock);
		callback = SceSysmemForDriver_7B4CB60A(process->uniqueHeap, sizeof(*callback));
		if (callback == NULL) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->callbackLock, intr);
			sceGUIDReleaseObject(process->pid);
			ret = SCE_KERNEL_ERROR_NO_FREE_HEAP;
			goto out;
		}
		callback->next = process->callbacks;
		callback->cb = guid;
		process->callbacks = callback;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->callbackLock, intr);
	} else {
		intr = SceCpuForDriver_D32ACE9E(&process->callbackLock);
		callback = process->callbacks;
		if (callback == NULL)
			goto not_found;
		if (callback->cb == guid) {
			process->callbacks = callback->next;
		} else {
			do {
				prev = callback;
				callback = prev->next;
				if (callback == NULL)
					goto not_found;
			} while (callback->cb != guid);
			prev->next = callback->next;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->callbackLock, intr);
		sceKernelFreeHeapMemory(process->uniqueHeap, callback);
	}
	sceGUIDReleaseObject(process->pid);
	ret = 0;
	goto out;
not_found:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->callbackLock, intr);
	sceGUIDReleaseObject(process->pid);
	ret = SCE_KERNEL_ERROR_PROCESSMGR_NO_CALLBACK;
	goto out;
invalid:
	ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
out:
	ProcessSyscallLeave(state);
	return ret;
}

static int PIDtoAddressSpaceCB(SceUID pid, void **addressSpaceCB)
{
	SceProcessObject *process;
	ThreadCB *cb;
	int ret;

	if (pid == 0) {
		cb = getCurrentThreadCB();
		if (cb != NULL) {
			*addressSpaceCB = cb->addressSpace;
			return 0;
		}
	} else {
		if (pid == KERNEL_PID) {
			*addressSpaceCB = g_processmgr.kernelProcess->addressSpaceCB;
			return 0;
		}
		cb = getCurrentThreadCB();
		if (cb != NULL && cb->processId == pid) {
			*addressSpaceCB = cb->addressSpace;
			return 0;
		}
	}
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		*addressSpaceCB = NULL;
		return ret;
	}
#if FW_DEBUG
	if (process->addressSpaceCB == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5454, "there is no address space\n");
		sceGUIDReleaseObject(process->pid);
		return SCE_KERNEL_ERROR_ERROR;
	}
#endif
	*addressSpaceCB = process->addressSpaceCB;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* the SceUIDProcessClass destructor */
static int Destructor(SceProcessObject *process)
{
	int intr, ret;

	intr = SceCpuForDriver_D32ACE9E(&g_processListLock);
	ListRemove(&process->node);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
	if ((process->status & PROCESS_STATE_MASK) != PROCESS_STATE_NONE) {
		intr = SceCpuForDriver_D32ACE9E(&g_recoveryLock);
		ListAddTail(&g_recoveryList, &process->recoveryNode);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_recoveryLock, intr);
		ret = sceKernelSetEventFlag(g_recoveryEventFlag, 1);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2788,
				"sceKernelSetEventFlag(ResourceRecoveryThread) failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(2789);
		}
		return 0;
	}
	ret = ProcessResourceRecovery(process, 1);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 2777, "ProcessResourceRecovery failed. 0x%08x\n", ret);
	return 0;
}

static int CreateKernelProcessCommon(SceProcessObject *process, SceUID pid)
{
	SceClass *cls;
	int ret;

	process->type = PROCESS_TYPE_KERNEL;
	process->addressSpace = KERNEL_ADDRESS_SPACE;
	ret = SceSysmemForDriver_0FC24464(KERNEL_ADDRESS_SPACE, &process->addressSpaceCB);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 9512, "sceGUIDGetObject(0x%08x) failed\n",
				     process->addressSpace);
	if (pid == KERNEL_PID)
		cls = &g_processClass;
	else
		cls = SceSysmemForDriver_85336A1C();
	ret = SceSysmemForKernel_BF275C5A(pid, cls, 0, process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 9527, "sceGUIDSet failed 0x%08x\n", ret);
		return ret;
	}
	process->pid = pid;
	process->cpuAffinity = 0xf;
	return 0;
}

/* the self authentication of the kernel process and of the dummy processes */
static const SceSelfAuthInfo g_kernelAuthInfo = {
	.programAuthorityId = 0x2808000000000001ULL,
	.capability = {
		0x80, 0x00, 0x00, 0x00, 0xc0, 0x00, 0xf0, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	},
	.attribute = {
		0x80, 0x09, 0x80, 0x03, 0x00, 0x00, 0xc3, 0x00,
		0x00, 0x00, 0x80, 0x09, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	},
};

static const SceSelfAuthInfo g_shellDummyAuthInfo = {
	.programAuthorityId = 0x2f00000000000001ULL,
	.capability = {
		0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	},
	.attribute = {
		0x60, 0x07, 0x40, 0x06, 0x40, 0x00, 0x33, 0x00,
		0x46, 0x02, 0x00, 0x75, 0x40, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	},
};

static const SceSelfAuthInfo g_npdrmDummyAuthInfo = {
	.programAuthorityId = 0x2800000000000001ULL,
	.capability = {
		0x40, 0x00, 0x00, 0x00, 0xc0, 0x00, 0xf0, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	},
	.attribute = {
		0x80, 0x09, 0x80, 0x03, 0x00, 0x00, 0xc3, 0x00,
		0x00, 0x00, 0x20, 0x09, 0x40, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	},
};

static inline __attribute__((always_inline)) SceProcessObject *_CreateProcessRaw(void)
{
	SceProcessObject *process = &g_kernelProcessObject;
	SceKernelHeapCreateOpt opt;

	ProcessConstructor(process);
	opt.size = sizeof(opt);
	opt.attr = HEAP_AUTO_EXTEND;
	process->uniqueHeap = SceSysmemForDriver_9328E0E8("unique: SceKernelProcess", 0x9000, &opt);
	if (process->uniqueHeap < 0) {
		SCE_DBG_ASSERT_FAIL(3085);
		return NULL;
	}
	opt.attr = HEAP_AUTO_EXTEND;
	process->kplsHeap = SceSysmemForDriver_9328E0E8("kpls: SceKernelProcess", 0x7000, &opt);
	if (process->kplsHeap < 0) {
		sceKernelDeleteHeap(process->uniqueHeap);
		SCE_DBG_ASSERT_FAIL(3097);
		return NULL;
	}
	if (SceDipswForDriver_A98FC2FD(0xd2) != 0)
		process->toolHeap = TOOL_HEAP;
	process->entryHeap = 0x10003;
	process->entryHeapObject = SceSysmemForKernel_FA6C098C(0x10003);
	return process;
}

static int CreateKernelProcess(void)
{
	SceProcessObject *process;
	int ret;

	process = _CreateProcessRaw();
	ret = CreateKernelProcessCommon(process, KERNEL_PID);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 9552, "CreateKernelProcessCommon failed 0x%08x\n", ret);
		return ret;
	}
	PROCESS_PROBE("create", process->pid);
	SceSysmemForKernel_5E772E19(process->addressSpace, KERNEL_PID);
	g_processmgr.kernelProcess = process;
	memcpy(&process->authInfo, &g_kernelAuthInfo, sizeof(process->authInfo));
	process->threadPermission = 0x80;
	process->budgetId = BudgetGetKernelBudgetId();
	process->budget = g_kernelBudget;
	if (SceDipswForDriver_A98FC2FD(0xe4) == 0) {
		process->phbp = NULL;
		process->phwp = NULL;
		process->contextBreakpoint = NULL;
	} else {
		g_kernelPhbp.lock = 0;
		process->phbp = &g_kernelPhbp;
		g_kernelPhwp.lock = 0;
		process->phwp = &g_kernelPhwp;
		process->contextBreakpoint = &g_kernelContextBreakpoint;
	}
	SceExcpmgrForKernel_293DFA04(process->phbp);
	SceIntrmgrForKernel_ABEB5A69(process->phbp);
	SceExcpmgrForKernel_E7487AFD(process->phwp);
	SceIntrmgrForKernel_7E3D4560(process->phwp);
	process->time = 0;
	ret = sceKernelThreadMgrProcessCreate(process);
	SCE_DBG_ASSERT_CHECK(ret == 0, 9632);
	SceSysmemForKernel_E8CC2166(process->pid, 5);
	process->status = (process->status & ~PROCESS_STATE_MASK) | PROCESS_STATE_RUNNING;
	return 0;
}

static int CreateDummyProcessWithUID(SceUID pid, SceProcessObject *process,
				     const SceSelfAuthInfo *authInfo, SceUInt32 threadPermission)
{
	int intr, ret;

	ProcessConstructor(process);
	intr = SceCpuForDriver_D32ACE9E(&g_processListLock);
	ListRemove(&process->node);
	g_processCount--;
	ListInit(&process->node);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
	ret = CreateKernelProcessCommon(process, pid);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 9662, "CreateKernelProcessCommon failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_LEVEL(0, 0, 9663);
		return ret;
	}
	memcpy(&process->authInfo, authInfo, sizeof(process->authInfo));
	process->threadPermission = threadPermission;
	SceSysmemForKernel_E8CC2166(process->pid, 7);
	return 0;
}

static int ProcessThreadResume(SceUID thid, SceUInt32 state)
{
	int ret;

	if (state == PROCESS_STATE_SUSPENDED) {
		ret = SceThreadmgrForKernel_490DAF14(thid);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 804, "sceKernelResumeThread(0x%08x) failed 0x%08x\n",
					     thid, ret);
	} else if (state == PROCESS_STATE_DEBUG_SUSPENDED) {
		ret = sceKernelDebugResumeThread(thid, 2);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 812,
					     "sceKernelDebugResumeThread(0x%08x) failed 0x%08x\n", thid, ret);
	} else {
		ret = 0;
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 819, "sceKernelDebugResumeThread(0x%08x): %d\n", thid,
				     SceThreadmgrForDriver_A0B1AB21(thid));
	}
	return ret;
}

/* name unknown: move the mini applications without their own affinity to other cores */
static inline __attribute__((always_inline)) void SetMiniAppCpuAffinity(SceUInt32 cpuAffinity)
{
	SceProcessObject *process;
	ListNode *node;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&g_processListLock);
	for (node = g_processList.next; node != &g_processList; node = node->next) {
		process = PROCESS_FROM_NODE(node);
		if (process->type != PROCESS_TYPE_MINI_APP || process->cpuAffinityOverride != 0)
			continue;
		SceCpuForDriver_BF82DEB2(&process->lock);
		if ((process->status & 0xfb) != 0) {
			process->cpuAffinity = cpuAffinity;
			SceThreadmgrForKernel_16D1961A();
		}
		sceKernelSpinlockLowUnlock(&process->lock);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
}

/* the start argument of the preload thread */
typedef struct ProcessPreloadParam {
	SceProcessObject *process;
	SceSize args;
	void *argp;
	SceUInt32 waitDebugger;
} ProcessPreloadParam;

/* the SceKernelProcessPreload thread: start the preloaded modules, then the main thread */
static int sceKernelProcessPreloadModule(SceSize args, void *argp)
{
	ProcessPreloadParam *param = argp;
	SceProcessObject *process;
	ProcEventStateParam event;
	ProcessBudget *budget;
	int intr, ret;

	if (args != sizeof(*param))
		SCE_PANIC(4617);
	process = param->process;
	if (param->waitDebugger == 0) {
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		process->prevState = 0;
		process->status = (process->status & ~PROCESS_STATE_MASK) | PROCESS_STATE_RUNNING;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	} else {
		ret = sceKernelWaitEventFlag(process->eventFlag, PROCESS_EVF_DEBUG_RESUME, 5, NULL, NULL);
		if (ret < 0)
			goto out;
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		process->prevState = process->status & PROCESS_STATE_MASK;
		if (process->status & PROCESS_FLAG_EXIT) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
			goto out;
		}
		process->status &= ~PROCESS_FLAG_PRELOAD;
		process->status = (process->status & ~PROCESS_STATE_MASK) | PROCESS_STATE_RUNNING;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		ret = ProcessThreadResume(process->mainThread, PROCESS_STATE_DEBUG_SUSPENDED);
		if (ret == (int)SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED)
			SCE_DBG_ASSERT_LEVEL(0, 0, 4651);
		else if (ret < 0)
			SCE_PRINTF_WITH_INFO(0xb, 4653, "ProcessThreadResume failed 0x%08x\n", ret);
	}
	ret = sceKernelStartPreloadingModules(process->pid);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4666, "sceKernelStartPreloadingModules : failed (res=0x%08x)\n",
				     ret);
		goto out;
	}
	ret = BudgetLock(process->budget);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4672, "Lock budget mutex failed. 0x%08x\n", ret);
		goto out;
	}
	if (!(process->attr & PROCESS_ATTR_NOT_ACTIVE))
		process->budget->activeCount++;
	if (process->type == PROCESS_TYPE_GAME) {
		budget = process->budget;
		if (budget->activeCount != 0) {
			event.size = sizeof(event);
			event.pid = process->pid;
			event.state = process->status & PROCESS_STATE_MASK;
			event.request = 0;
			event.type = process->type;
			sceKernelInvokeProcEventHandler(event.pid, PROC_EVENT_PRELOAD, 0, &event, NULL, 0);
			SetMiniAppCpuAffinity(8);
			budget = process->budget;
		}
	} else {
		if (process->type == PROCESS_TYPE_MINI_APP && process->cpuAffinityOverride == 0) {
			ret = BudgetLock(g_gameBudget);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 4736, "Lock budget mutex failed. 0x%08x\n", ret);
				BudgetUnlock(process->budget);
				goto out;
			}
			if (g_gameBudget->activeCount != 0)
				process->cpuAffinity = 8;
			BudgetUnlock(g_gameBudget);
		}
		budget = process->budget;
	}
	BudgetUnlock(budget);
	ret = sceKernelStartThread(process->mainThread, param->args, param->argp);
	if (ret >= 0) {
		sceKernelSetEventFlag(process->eventFlag, PROCESS_EVF_STARTED);
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		process->status &= ~PROCESS_FLAG_STARTING;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	}
out:
	process->preloadThread = 0;
	sceKernelExitDeleteThread(ret);
	return ret;
}

/* name unknown */
static SceUID CreateAddressSpace(SceUID pid, const char *name, AddressSpaceHeapOps *param,
				 SceUIntPtr ppm)
{
	AddressSpaceCreateOpt opt;

	opt.size = sizeof(opt);
	if (param == NULL) {
		opt.flags = 0;
	} else {
		opt.flags = ADDRESS_SPACE_OPT_HEAP;
		opt.heap = param;
	}
	if (ppm != 0) {
		opt.flags |= ADDRESS_SPACE_OPT_L1_TABLE;
		opt.l1Table = (uint32_t *)ppm;
	}
	return sceKernelCreateAddressSpace(pid, name, 0x10000001, &opt);
}

static inline __attribute__((always_inline)) int AllocAddressSpace(SceProcessObject *process,
								    const char *name,
								    SceUInt32 type,
								    AddressSpaceHeapOps *param,
								    SceUIntPtr ppm)
{
	SceUID addressSpace;
	int ret;

	switch (type) {
	case PROCESS_TYPE_GAME:
		addressSpace = CreateAddressSpace(process->pid, name, param, ppm);
		if (addressSpace < 0) {
			ret = addressSpace;
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3130, "CreateAddressSpace failed 0x%08x\n", ret);
			goto failed;
		}
		break;
	case PROCESS_TYPE_MINI_APP:
		addressSpace = CreateAddressSpace(process->pid, name, param, ppm);
		if (addressSpace < 0) {
			ret = addressSpace;
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3139, "CreateAddressSpace failed 0x%08x\n", ret);
			goto failed;
		}
		break;
	case PROCESS_TYPE_SHELL:
		addressSpace = CreateAddressSpace(process->pid, name, param, ppm);
		if (addressSpace < 0) {
			ret = addressSpace;
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3148, "CreateAddressSpace failed 0x%08x\n", ret);
			goto failed;
		}
		break;
	case PROCESS_TYPE_KERNEL:
		ret = -1;
		goto failed;
	default:
		ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
		goto failed;
	}
	process->addressSpace = addressSpace;
	ret = SceSysmemForDriver_0FC24464(addressSpace, &process->addressSpaceCB);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3168, "sceGUIDGetObject failed 0x%08x\n", ret);
	return ret;
failed:
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 3156, "Create address space failed. 0x%08x\n", ret);
	return ret;
}

/* the object heap resource limits per budget type */
typedef struct ObjectHeapLimit {
	SceInt32 limit;
	SceUInt32 reserved;
} ObjectHeapLimit;

static const ObjectHeapLimit g_shellObjectLimit[6] = {
	{ -1, 0 }, { 0x400, 0 }, { -1, 0 }, { 0, 0 }, { 0, 0 }, { 4, 0 },
};

static const ObjectHeapLimit g_shellCDialogObjectLimit[6] = {
	{ -1, 0 }, { 0, 0 }, { -1, 0 }, { 0, 0 }, { 0, 0 }, { 4, 0 },
};

static const ObjectHeapLimit g_miniAppObjectLimit[6] = {
	{ -1, 0 }, { 0x400, 0 }, { -1, 0 }, { 0, 0 }, { 0, 0 }, { 4, 0 },
};

static const ObjectHeapLimit g_miniAppCDialogObjectLimit[6] = {
	{ -1, 0 }, { 0x10, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { 4, 0 },
};

static const ObjectHeapLimit g_gameObjectLimit[6] = {
	{ -1, 0 }, { 0x400, 0 }, { -1, 0 }, { 0, 0 }, { 0, 0 }, { 4, 0 },
};

static const ObjectHeapLimit g_gameCDialogObjectLimit[6] = {
	{ -1, 0 }, { 0x10, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { 4, 0 },
};

static inline __attribute__((always_inline)) const ModulemgrTitlePatch *FindTitlePatch(const char *name)
{
	const ModulemgrTitlePatch *patch;

	for (patch = g_titlePatch; patch->titleId != NULL; patch++) {
		if (strncmp(name, patch->titleId, 0x1f) == 0)
			return patch;
	}
	return NULL;
}

/* open one standard I/O file of a process (index 0..3) */
static inline __attribute__((always_inline)) int OpenStdioFile(SceProcessObject *process,
								const char *path, int index)
{
	SceUID fd, puid;

	fd = sceIoOpen(path, 3, 0);
	if (fd < 0)
		return fd;
	puid = SceSysmemForDriver_BF209859(process->pid, fd);
	if (puid < 0) {
		sceIoClose(fd);
		return puid;
	}
	process->stdioFd[index] = fd;
	process->stdioPuid[index] = puid;
	return 0;
}

static inline __attribute__((always_inline)) int OpenStdio(SceProcessObject *process)
{
	SceUID pid;
	int ret;

	pid = SceThreadmgrForDriver_0486F239(KERNEL_PID);
	ret = OpenStdioFile(process, "tty0:", 0);
	if (ret < 0)
		goto out;
	ret = OpenStdioFile(process, "tty0:", 1);
	if (ret < 0)
		goto out;
	ret = OpenStdioFile(process, "tty1:", 2);
	if (ret < 0)
		goto out;
	if (SceDipswForDriver_A98FC2FD(0xb8) != 0)
		ret = OpenStdioFile(process, "tty7:", 3);
out:
	SceThreadmgrForDriver_0486F239(pid);
	return ret;
}

static int _CreateProcess(const char *name, SceUInt32 type, const char *path, SceUInt32 attr,
			  SceInt32 priority, SceSize stackSize, SceUInt32 cpuAffinity,
			  SceUID parentPid, SceUInt32 kind, SceUID prevPid, const void *klicensee,
			  SceUInt32 limit0, SceUInt32 limit1, SceSize uniqueHeapSize,
			  ProcessBudget *budget, SceUInt16 resolutionFlags, SceUInt16 powerConfigFlags,
			  SceProcessObject **out)
{
	SceProcessObject *process;
	AddressSpaceHeapOps *asParam;
	const ObjectHeapLimit *limit, *cdialogLimit;
	ProcEventCreateParam createParam;
	ProcEventParam failParam;
	ProcessDebugRegBlock *regs;
	SceKernelHeapCreateOpt opt;
	UIDCreateOpt uidOpt;
	const char *threadName, *processName;
	const void *optParam;
	void *objectHeap;
	char heapName[0x20];
	SceUID pid, heap, cdialogHeap, handler;
	SceUInt32 sdkVersion, threadAttr, budgetType, maxSize;
	SceSize toolHeapSize;
	int deleteThreads, cpu, permission, intr, ret, slot, i;

	objectHeap = NULL;
	if (kind == 1 && prevPid != 0) {
		/* ExitSpawn: take over the object KillProcessCommon prepared */
		if (g_exitSpawn.pid != prevPid) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3408,
				"<ExitSpawn> inherited pid(0x%08x) is differ from prev_pid(0x%08x)\n",
				g_exitSpawn.pid, prevPid);
			return SCE_KERNEL_ERROR_PROCESSMGR_INHERIT_PID;
		}
		process = g_exitSpawn.process;
		pid = process->pid;
		memcpy(&process->authInfo, &g_exitSpawn.authInfo, sizeof(process->authInfo));
		process->parentPid = g_exitSpawn.pid;
		SceSysmemForDriver_4CFA4100(pid, name);
		memset(&g_exitSpawn, 0, sizeof(g_exitSpawn));
	} else {
		slot = PPMAllocSlot();
		if (slot < 0)
			return slot;
		process = PPMAllocMemory(slot, sizeof(*process));
		if (process == NULL) {
			PPMFreeSlot(slot);
			return SCE_KERNEL_ERROR_NO_FREE_HEAP;
		}
		memset(process, 0, sizeof(*process));
		uidOpt.flags = UID_CREATE_OPT_OBJECT | UID_CREATE_OPT_ATTR;
		uidOpt.object = (SceObjectBase *)process;
		uidOpt.attr = 0x8000000;
		pid = sceGUIDKernelCreateWithOpt(&g_processClass, name, &uidOpt, &process);
		if (pid < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3451, "Create Process UID failed 0x%08x\n", pid);
			PPMFreeSlot(slot);
			return pid;
		}
		process->slot = slot;
		process->pid = pid;
		if (klicensee == NULL)
			memset(process->authInfo.klicensee, 0, 0x10);
		else
			memcpy(process->authInfo.klicensee, klicensee, 0x10);
		process->parentPid = parentPid;
		process->authInfo.sharedSecret3[0] = 0x10;
	}
	SceKernelUtilsForDriver_875B2A1C(process->random, sizeof(process->random));
	type &= 0x7fffffff;
	deleteThreads = 0;
	SceKernelUtilsForDriver_B55C69B7(process->random);
	process->type = type;
	process->randomLock = 0;

	ret = sceKernelCreateEventFlag("SceKernelProcessmgrProc", 0x9000,
					PROCESS_EVF_NOT_DEBUG_SUSPENDED, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3483, "Create EventFlag failed 0x%08x\n", ret);
		goto failed;
	}
	process->eventFlag = ret;
	ret = sceKernelCreateSema("SceKernelProcessmgrProcLock", 0x8000, 1, 1, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3494, "Create Semaphore failed 0x%08x\n", ret);
		goto failed;
	}
	process->lockSema = ret;
	ret = sceKernelCreateSema("SceKernelProcessmgrEventLock", 0x8000, 4, 4, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3505, "Create Event Semaphore failed 0x%08x\n", ret);
		goto failed;
	}
	process->eventSema = ret;

	asParam = PPMAllocMemory(process->slot, sizeof(*asParam));
	asParam->heap = (void *)process->slot;
	asParam->alloc = (void *(*)(void *, SceSize))PPMAllocMemory;
	asParam->free = PPMFreeMemory;
	ret = AllocAddressSpace(process, name, type, asParam, g_ppm.base0 + process->slot * 0x4000);
	if (ret < 0)
		goto failed;
	ret = SetBudget(type, process->addressSpace, budget);
	if (ret < 0)
		goto failed;

	budgetType = budget->type;
	switch (budgetType) {
	case PROCESS_TYPE_MINI_APP:
		objectHeap = SceSysmemForKernel_791D6205();
		ret = 0;
		break;
	case PROCESS_TYPE_GAME:
	case PROCESS_TYPE_SHELL:
	case PROCESS_TYPE_KERNEL:
		ret = sceKernelCreateObjectHeap(pid, 0, asParam, &objectHeap);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3534, "sceKernelCreateObjectHeap failed 0x%08x\n", ret);
		break;
	default:
		ret = 0;
		SCE_DBG_ASSERT_FAIL(3541);
		break;
	}
	budgetType = budget->type;
	process->objectHeap[0] = objectHeap;

	process->uniqueHeapCB = PPMAllocMemory(process->slot, 0x80);
	opt.size = sizeof(opt);
	if (budgetType == PROCESS_TYPE_MINI_APP) {
		opt.attr = 0;
		if (budget->mainSize > 0x2000000) {
			maxSize = 0x100000;
		} else if (budget->mainSize == 0x2000000) {
			maxSize = 0x80000;
		} else {
			maxSize = 0x40000;
			if (uniqueHeapSize == 0) {
				uniqueHeapSize = 0x1000;
				opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MAX_SIZE;
				opt.maxSize = 0x80000;
			}
		}
		if (uniqueHeapSize == 0) {
			uniqueHeapSize = maxSize;
			opt.attr = HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
		} else if (uniqueHeapSize > maxSize) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
			goto failed;
		} else {
			opt.attr |= HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
		}
		limit = g_miniAppObjectLimit;
		cdialogLimit = g_miniAppCDialogObjectLimit;
		toolHeapSize = 0xf000;
	} else if (budgetType == PROCESS_TYPE_GAME) {
		cdialogLimit = g_gameCDialogObjectLimit;
		limit = g_gameObjectLimit;
		toolHeapSize = 0xf000;
		opt.attr = HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
		uniqueHeapSize = 0x300000;
	} else if (budgetType == PROCESS_TYPE_SHELL) {
		toolHeapSize = 0x7f000;
		uniqueHeapSize = 0x7f000;
		cdialogLimit = g_shellCDialogObjectLimit;
		limit = g_shellObjectLimit;
		opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
	} else if (budgetType == PROCESS_TYPE_KERNEL) {
		opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
		cdialogLimit = NULL;
		toolHeapSize = 0xf000;
		limit = NULL;
		uniqueHeapSize = 0x1000;
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3609, "invalid budget type. 0x%x\n", budgetType);
		SCE_DBG_ASSERT_FAIL(3610);
		cdialogLimit = NULL;
		limit = NULL;
		toolHeapSize = 0xf000;
		opt.attr |= HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
	}
	opt.memtype = 0x10c0d006;
	opt.pid = pid;
	opt.heap = process->uniqueHeapCB;
	snprintf(heapName, sizeof(heapName), "unique: %s", name);
	heap = SceSysmemForDriver_9328E0E8(heapName, uniqueHeapSize, &opt);
	if (heap < 0) {
		ret = heap;
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3626, "Create Heap failed 0x%08x\n", ret);
		goto failed;
	}
	process->uniqueHeap = heap;

	snprintf(heapName, sizeof(heapName), "kpls: %s", name);
	ret = SceSysmemForDriver_C94850C9(heapName, 0x1020d006, 0x40000, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3639, "allocate kpls block failed. 0x%08x\n", ret);
		goto failed;
	}
	process->kplsBlock = ret;
	opt.maxSize = 0;
	opt.memtype = 0;
	opt.pid = 0;
	opt.heap = NULL;
	opt.size = sizeof(opt);
	opt.attr = HEAP_HAS_MEMBLOCK;
	opt.memBlock = ret;
	ret = SceSysmemForDriver_9328E0E8(heapName, 0x40000, &opt);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3654, "Create Heap failed 0x%08x\n", ret);
		goto failed;
	}
	process->kplsHeap = ret;

	if (budget->cdialogPartition == NULL) {
		cdialogHeap = -1;
	} else {
		process->cdialogHeapCB = PPMAllocMemory(process->slot, 0x80);
		opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
		opt.memtype = 0x10a0d006;
		opt.pid = pid;
		opt.heap = process->cdialogHeapCB;
		snprintf(heapName, sizeof(heapName), "CDialog: %s", name);
		cdialogHeap = SceSysmemForDriver_9328E0E8(heapName, 0x1000, &opt);
		if (cdialogHeap < 0) {
			ret = cdialogHeap;
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3678, "Create Heap failed 0x%08x\n", ret);
			goto failed;
		}
		process->cdialogHeap = cdialogHeap;
		switch (budget->type) {
		case PROCESS_TYPE_MINI_APP:
			objectHeap = SceSysmemForKernel_791D6205();
			break;
		case PROCESS_TYPE_GAME:
		case PROCESS_TYPE_SHELL:
		case PROCESS_TYPE_KERNEL:
			ret = sceKernelCreateObjectHeap(pid, 1, asParam, &objectHeap);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 3693,
						     "sceKernelCreateObjectHeap failed 0x%08x\n", ret);
			break;
		default:
			SCE_DBG_ASSERT_FAIL(3700);
			objectHeap = NULL;
			break;
		}
		process->objectHeap[1] = objectHeap;
	}

	if (SceDipswForDriver_A98FC2FD(0xd2) != 0) {
#if FW_VERSION >= 0x04000000
		process->toolMemory = SceSysmemForDriver_7B4CB60A(TOOL_HEAP, 0x80);
		if (process->toolMemory == NULL) {
			ret = SCE_KERNEL_ERROR_NO_FREE_HEAP;
			goto failed;
		}
		opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE | HEAP_HAS_PID | HEAP_HAS_HEAPCB;
#else
		process->toolMemory = PPMAllocMemory(process->slot, 0x80);
		opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE | HEAP_HAS_PID;
		if (process->toolMemory != NULL)
			opt.attr |= HEAP_HAS_HEAPCB;
#endif
		opt.memtype = 0x10f0d006;
		opt.pid = pid;
		opt.heap = process->toolMemory;
		snprintf(heapName, sizeof(heapName), "tool: %s", name);
		ret = SceSysmemForDriver_9328E0E8(heapName, toolHeapSize, &opt);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 3730, "Create Heap failed 0x%08x\n", ret);
			goto failed;
		}
		process->toolHeap = ret;
	}

	if (budget->type != PROCESS_TYPE_MINI_APP) {
		if (process->objectHeap[0] != NULL) {
			ret = SceSysmemForKernel_660DC18A(process->objectHeap[0], pid, heap);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 3742, "sceKernelObjectHeapStart failed 0x%08x\n",
						     ret);
		}
		if (process->objectHeap[1] != NULL) {
			ret = SceSysmemForKernel_660DC18A(process->objectHeap[1], pid, cdialogHeap);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 3751, "sceKernelObjectHeapStart failed 0x%08x\n",
						     ret);
		}
	}
	if (budget->type != PROCESS_TYPE_MINI_APP || g_objectHeapLimitsSet == 0) {
		if (process->objectHeap[0] != NULL)
			sceKernelObjectHeapSetResourceLimitAll(process->objectHeap[0], 6, limit);
		if (process->objectHeap[1] != NULL)
			sceKernelObjectHeapSetResourceLimitAll(process->objectHeap[1], 6, cdialogLimit);
		if (budget->type == PROCESS_TYPE_MINI_APP)
			g_objectHeapLimitsSet = 1;
	}

	ret = SceSysmemForKernel_89BEF058(pid, heap);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3788, "Create Entry Heap failed 0x%08x\n", ret);
		goto failed;
	}
	process->entryHeap = ret;
	process->entryHeapObject = SceSysmemForKernel_FA6C098C(ret);
	process->budget = budget;
	process->budgetId = budget->uid;
	process->attr = attr;
	if (cpuAffinity == 0) {
		switch (budget->type) {
		case PROCESS_TYPE_MINI_APP:
		case PROCESS_TYPE_GAME:
			process->cpuAffinity = 7;
			break;
		case PROCESS_TYPE_SHELL:
			process->cpuAffinity = 8;
			break;
		default:
			SCE_DBG_ASSERT_FAIL(3814);
			goto failed;
		}
		process->cpuAffinityOverride = 0;
	} else {
		process->cpuAffinity = cpuAffinity;
		process->cpuAffinityOverride = cpuAffinity;
	}
	process->limit[0] = limit0;
	process->resolutionFlags = resolutionFlags;
	process->powerConfigFlags = powerConfigFlags;
	process->limit[1] = limit1;
	process->infoParam = 0x7f;
	strncpy(process->TitleId, name, sizeof(process->TitleId));
	if (budget->type == PROCESS_TYPE_GAME) {
		process->resource[0].limit = 0x800;
		process->resource[1].limit = -1;
		process->resource[2].limit = 0;
	} else {
		process->resource[2].limit = 0;
		process->resource[0].limit = -1;
		process->resource[1].limit = -1;
	}
	SceKernelUtilsForDriver_875B2A1C(&process->stackGuard, sizeof(process->stackGuard));

	if (SceDipswForDriver_A98FC2FD(0xe4) != 0) {
		regs = SceSysmemForDriver_7B4CB60A(process->toolHeap > 0 ? process->toolHeap
								       : process->uniqueHeap,
						 sizeof(*regs));
		if (regs == NULL)
			SCE_PANIC(3869);
		memset(regs, 0, sizeof(*regs));
		process->phbp = &regs->bp;
		for (i = 0; i < 5; i++)
			regs->bp.reg[i] = g_defaultPhbp[i];
		regs->bp.lock = 0;
		process->phwp = &regs->wp;
		for (i = 0; i < 4; i++)
			regs->wp.reg[i] = g_defaultPhwp[i];
		regs->wp.lock = 0;
		process->contextBreakpoint = &regs->contextBreakpoint;
	}
	ret = sceKernelThreadMgrProcessCreate(process);
	SCE_DBG_ASSERT_CHECK(ret == 0, 3901);
	PROCESS_PROBE("exec", path);

	ret = sceKernelLoadProcessImage(process->pid, path,
					 ((attr & PROCESS_ATTR_LOAD_FLAG) ? 0x10000 : 0) | ((kind == 1) << 14),
					 &process->authInfo, &process->image,
					 FindTitlePatch(process->TitleId));
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3931,
				     "sceKernelLoadProcessImage [file: %s]: failed (res=0x%08x)\n", path, ret);
		PROCESS_PROBE("exec__failure", ret);
		deleteThreads = 1;
		goto failed;
	}
	{
		const SceUInt8 *capability = process->authInfo.capability;

		process->threadPermission = capability != NULL ? *(const SceUInt32 *)capability : 0;
	}
	PROCESS_PROBE("exec__success");
	if (!(attr & PROCESS_ATTR_PRIORITY)) {
		priority = process->image.mainThreadPriority;
		if (priority < 1)
			priority = 0x10000100;
	}
	if (!(attr & PROCESS_ATTR_STACK_SIZE)) {
		stackSize = process->image.mainThreadStackSize;
		if (stackSize == 0)
			stackSize = 0x40000;
	}
	cpu = process->image.mainThreadCpuAffinityMask;
	if (cpu == 0 && (budget->type != PROCESS_TYPE_MINI_APP || process->cpuAffinityOverride != 0))
		cpu = sceKernelGetDefaultCpuAffinityMask(budget);
	ret = SceProcessmgrForDriver_D141C076(process->pid, &sdkVersion);
	if (ret < 0) {
		deleteThreads = 1;
		SceModulemgrForKernel_E71530D7(process->pid);
		goto failed;
	}
	threadAttr = process->image.mainThreadAttr;
	if (sdkVersion < 0x1650000)
		threadAttr = 0;
	threadName = process->image.mainThreadName;
	if (strnlen(threadName, sizeof(process->image.mainThreadName)) == 0)
		threadName = name;
	optParam = process->image.mainThreadOptParam;
	if (*(const SceSize *)optParam == 0)
		optParam = NULL;
	permission = sceKernelSetThreadAccessLevel(0x80);
	ret = sceKernelGetModuleIdByPid(process->pid);
	if (ret < 0) {
		deleteThreads = 1;
		SceModulemgrForKernel_E71530D7(process->pid);
		goto failed;
	}
	ret = SceThreadmgrForDriver_C8E57BB4(process->pid, threadName, SceModulemgrForKernel_7B302C5D(ret),
					     priority, stackSize, threadAttr | 0x80000000, cpu,
					     (void *)optParam);
	sceKernelSetThreadAccessLevel(permission);
	if (ret < 0) {
		deleteThreads = 1;
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4024, "sceKernelCreateRemoteThread failed. 0x%08x\n", ret);
		SceModulemgrForKernel_E71530D7(process->pid);
		goto failed;
	}
	process->mainThread = ret;
	createParam.size = sizeof(createParam);
	createParam.pid = 0;
	createParam.kind = kind;
	createParam.prevPid = prevPid;
	createParam.type = type;
	ret = sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_CREATE, 0, &createParam,
					       &handler, 0);
	if (ret < 0) {
		deleteThreads = 1;
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4047, "process CREATE handler [%s] failed. 0x%08x\n",
				     SceSysmemForDriver_E655852F(handler), ret);
		memset(&failParam, 0, sizeof(failParam));
		failParam.size = sizeof(failParam);
		failParam.kind = kind;
		failParam.type = process->type;
		sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_EXIT, 0, &failParam,
						 NULL, handler);
		SceModulemgrForKernel_E71530D7(process->pid);
		goto failed;
	}
	if (process->image.processName[0] == '\0') {
		processName = SceSysmemForDriver_E655852F(process->pid);
	} else {
		processName = process->image.processName;
		SceSysmemForDriver_4CFA4100(process->pid, processName);
	}
	snprintf(heapName, sizeof(heapName), "plock: %s", processName);
	SceSysmemForDriver_4CFA4100(process->lockSema, heapName);
	snprintf(heapName, sizeof(heapName), "pevent: %s", processName);
	SceSysmemForDriver_4CFA4100(process->eventFlag, heapName);
	SceSysmemForDriver_4CFA4100(process->eventSema, heapName);
	snprintf(heapName, sizeof(heapName), "pident: %s", processName);
	SceSysmemForDriver_4CFA4100(process->entryHeap, heapName);
	sceKernelAddressSpaceSetName(process->addressSpace, processName);
	ret = OpenStdio(process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4096, "OpenStdio() failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(4097);
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if (attr & PROCESS_ATTR_SUSPENDED)
		process->status = PROCESS_FLAG_PRELOAD_SUSPEND | PROCESS_STATE_CREATED;
	else
		process->status = (process->status & ~PROCESS_STATE_MASK) | PROCESS_STATE_CREATED;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	sceKernelSetEventFlag(process->eventFlag, PROCESS_EVF_CREATED);
	budget->processCount++;
	for (i = 0; i != PROCESS_MAX; i++) {
		if (budget->process[i] == 0) {
			budget->process[i] = process->pid;
			break;
		}
	}
	PROCESS_PROBE("create", process->pid);
	*out = process;
	return 0;

failed:
	if (process->mainThread != -1) {
		i = sceKernelDeleteThread(process->mainThread);
		SCE_DBG_ASSERT_LEVEL(0, i == 0, 4140);
	}
	if (deleteThreads) {
		i = sceKernelThreadMgrProcessDeleteBeforePUIDClose(process);
		if (i < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4147,
				"*** error: _CreateProcess() -> sceKernelThreadMgrProcessDeleteBeforePUIDClose()=0x%08X",
				i);
	}
	sceGUIDClose(pid);
	return ret;
}

/* thread manager errors a whole-process operation skips */
static inline __attribute__((always_inline)) int ThreadGone(int ret)
{
	return ret == (int)SCE_KERNEL_ERROR_INVALID_UID
	    || ret == (int)SCE_KERNEL_ERROR_DORMANT
	    || ret == (int)SCE_KERNEL_ERROR_DELETED
	    || ret == (int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
}

static int sceKernelGetThreadIdVector(SceProcessObject *process, SceUID *vector, SceSize *count)
{
	ThreadCB *thread;
	ListNode *node;
	SceSize n;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	node = process->threads.next;
	if (node == &process->threads) {
		n = 0;
		*count = 0;
	} else {
		n = 0;
		do {
			thread = THREAD_FROM_PROCESS_LINK(node);
			n++;
			node = node->next;
			*vector++ = thread->threadId;
			if (node == &process->threads) {
				*count = n;
				if (n == PROCESS_THREAD_VECTOR_MAX)
					goto full;
				goto done;
			}
		} while (n != PROCESS_THREAD_VECTOR_MAX);
		*count = PROCESS_THREAD_VECTOR_MAX;
full:
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		return SCE_KERNEL_ERROR_UID_RL_OVERFLOW;
	}
done:
	SCE_DBG_ASSERT_CHECK(n <= process->threadCount + process->unlimitedThreadCount, 848);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	return 0;
}

/* run func(tid, 0x11) on every thread of the process, starting at *cursor */
static SceSize ProcessAllThreadAction(SceProcessObject *process,
				      int (*func)(SceUID thid, SceUInt32 state), SceUInt32 *cursor)
{
	ThreadIdVector *vector;
	SceSize count, i;
	SceUInt32 index;
	int ret;

	if (SceExcpmgrForKernel_B615A7DA() != 0)
		vector = &g_excpThreadIdVector;
	else if (SceIntrmgrForDriver_182EE3E3() != 0)
		vector = &g_intrThreadIdVector;
	else
		vector = &g_threadIdVector;
	SceCpuForDriver_BF82DEB2(&vector->lock);
	ret = sceKernelGetThreadIdVector(process, vector->id, &count);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5924, "sceKernelGetThreadIdVector failed. 0x%08x\n", ret);
	if (cursor == NULL) {
		index = 0;
	} else {
		index = *cursor;
		if (index >= count) {
			index = 0;
			*cursor = 0;
		}
	}
	for (i = 0; i < count; i++, index++) {
		if (index == count)
			index = 0;
		ret = func(vector->id[index], 0x11);
		if (!ThreadGone(ret) && ret != (int)SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED
		    && ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 5954, "<func> failed 0x%08x\n", ret);
			goto out;
		}
	}
	if (cursor != NULL)
		(*cursor)++;
out:
	sceKernelSpinlockLowUnlock(&vector->lock);
	return count;
}

static int _SuspendAllThread(SceProcessObject *process)
{
	SceUInt32 total;
	int intr, ret;

	do {
		ret = ProcessAllThreadAction(process, ProcessThreadSuspend, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6097, "ProcessAllThreadAction failed 0x%08x\n", ret);
			return ret;
		}
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		total = process->unlimitedThreadCount + process->threadCount;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	} while ((SceUInt32)ret != total);
	return 0;
}

static inline void ProcessEventLogInit(ProcessEventLog *log, SceProcessObject *process)
{
	log->result = 0;
	log->pid = process->pid;
	log->type = process->type;
	log->nameLength = 10;
	memset(log->name, 0, sizeof(log->name));
	strncpy(log->name, process->TitleId, 10);
}

/* stop every thread of a process (kind 0x10: kill, 0x40: exit spawn) */
static int _SuspendProcess(SceProcessObject *process, SceUInt32 state, SceUInt32 kind)
{
	ProcEventParam param;
	ProcessEventLog log;
	ProcEventStateParam event;
	SceKernelThreadInfo info;
	ProcessBudget *budget;
	SceUID handler;
	SceSize count, i;
	SceUInt32 n;
	int stopThreads, intr, ret, type;
#if FW_DEBUG
	int retry;
#endif

	param.size = sizeof(param);
	param.state = state;
	param.kind = kind;
	param.type = process->type;
	log.result = 0;
	log.pid = process->pid;
	log.type = process->type;
	log.nameLength = 10;
	memset(log.name, 0, sizeof(log.name));
	strncpy(log.name, process->TitleId, 10);
	g_sgiScan = 0;
	g_sgiProcess = process;
	for (type = 0x100; type != 0x110; type++)
		sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_SUSPEND, type, &param, NULL, 0);
	intr = SceCpuForDriver_821FC0EE();
	stopThreads = kind != 0x40 && kind != 0x10;
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_SUSPEND, 0x200, &param, NULL, 0);
	sceKernelCpuResumeIntr(intr);
#if FW_DEBUG
	retry = 0;
#endif
	do {
		handler = 0;
		sceKernelDelayThread(5000);
		sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_SUSPEND, 0x201, &param, NULL, 0);
		intr = SceCpuForDriver_821FC0EE();
		g_sgiEnter = 0;
		g_sgiLeave = 0;
		sceKernelGenerateSoftIntr(6, 0, 7);
		while (g_sgiEnter != 3)
			;
		ret = sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_SUSPEND, 0x202, &param,
						       &handler, 0);
		if (stopThreads && ret >= 0) {
			process->threadsStopped = 1;
			g_sgiScan = 1;
			n = sceKernelCpuScanThreadForProcessmgr(process->pid);
			if ((int)n < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 1033,
					"error: sceKernelCpuScanThreadForProcessmgr(0x%08X)=0x%08X\n",
					process->pid, n);
		}
		g_sgiEnter = 0;
		__asm__ volatile("dsb" ::: "memory");
		while (g_sgiLeave != 3)
			__asm__ volatile("wfe");
		sceKernelCpuResumeIntr(intr);
#if FW_DEBUG
		if (ret < 0 && ++retry % 1000 == 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1054, "count %d, Busy handler %s, error code 0x%08x\n",
					     retry, SceSysmemForDriver_E655852F(handler), ret);
#endif
	} while (ret < 0);
	if (stopThreads) {
		SceCpuForDriver_BF82DEB2(&g_threadIdVector.lock);
		ret = sceKernelGetThreadIdVector(process, g_threadIdVector.id, &count);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1068, "sceKernelGetThreadIdVector failed. 0x%08x\n", ret);
		for (i = 0; i < count; i++) {
			ret = SceThreadmgrForKernel_15D6357C(g_threadIdVector.id[i], 0xf);
			if (!ThreadGone(ret) && ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1079,
					"sceKernelSuspendThread failed 0x%08x, tid 0x%08x\n", ret,
					g_threadIdVector.id[i]);
		}
		sceKernelSpinlockLowUnlock(&g_threadIdVector.lock);
	}
	while (process->status & PROCESS_FLAG_DEBUG_SUSPENDING)
		sceKernelDelayThread(100);
	info.size = sizeof(info);
	SceCpuForDriver_BF82DEB2(&g_threadIdVector.lock);
	ret = sceKernelGetThreadIdVector(process, g_threadIdVector.id, &count);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1101, "sceKernelGetThreadIdVector failed. 0x%08x\n", ret);
	for (i = 0; i < count; i++) {
		for (n = 1;; n++) {
			ret = sceKernelGetThreadInfo(g_threadIdVector.id[i], &info);
			if (ThreadGone(ret))
				break;
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1150,
					"sceKernelGetThreadInfo failed 0x%08x, tid 0x%08x\n", ret,
					g_threadIdVector.id[i]);
				SCE_PANIC(1151);
			}
			if (info.status == SCE_KERNEL_THREAD_STATUS_SUSPENDED ||
			    info.status == SCE_KERNEL_THREAD_STATUS_DORMANT)
				break;
#if FW_DEBUG
			if (n % 50000 == 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1190,
					"(WARNING): thread \"%s\"(0x%08x) has not been suspended yet\n",
					SceSysmemForDriver_E655852F(g_threadIdVector.id[i]), g_threadIdVector.id[i]);
#endif
			sceKernelDelayThread(100);
		}
	}
	sceKernelSpinlockLowUnlock(&g_threadIdVector.lock);
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_SUSPEND, 0x800, &param, NULL, 0);
	SceThreadmgrForKernel_7425185E(process);
	intr = SceCpuForDriver_D32ACE9E(&process->timeLock);
	process->timeRunning = 0;
	process->time = sceKernelGetSystemTimeWide() - process->time;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->timeLock, intr);
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	process->prevState = 0;
	process->status = (process->status & ~(PROCESS_FLAG_SUSPENDING | PROCESS_STATE_MASK))
			| PROCESS_STATE_SUSPENDED;
	sceKernelClearEventFlag(process->eventFlag, ~PROCESS_EVF_RESUMED);
	sceKernelSetEventFlag(process->eventFlag, PROCESS_EVF_SUSPENDED);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_SUSPEND, 0x1000, &param, NULL, 0);
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 1239, "Process [%s] is SUSPENDED!\n", SceSysmemForDriver_E655852F(process->pid));
	ret = BudgetLock(process->budget);
	if (ret < 0) {
		log.result = ret;
		k_sceEventLogPut(0x2711, 4, 0, &log, sizeof(log));
		SCE_DBG_ASSERT_FAIL(1246);
		return ret;
	}
	budget = process->budget;
	if (!(process->attr & PROCESS_ATTR_NOT_ACTIVE))
		budget->activeCount--;
	if (process->type == PROCESS_TYPE_GAME && budget->activeCount == 0) {
		SetMiniAppCpuAffinity(7);
		event.size = sizeof(event);
		event.pid = process->pid;
		event.state = process->status & PROCESS_STATE_MASK;
		event.request = kind;
		event.type = process->type;
		sceKernelInvokeProcEventHandler(event.pid, PROC_EVENT_SUSPEND_DONE, 0, &event, NULL, 0);
		budget = process->budget;
	}
	BudgetUnlock(budget);
	k_sceEventLogPut(0x2711, 4, 0, &log, sizeof(log));
	return 0;
}

/* stop and delete the threads of a process, then the process itself */
static int _KillProcess(SceProcessObject *process, SceUInt32 state, SceUInt32 kind)
{
	ProcEventParam param;
	ProcessEventLog log;
	SceSize count, i;
	SceUID timer;
	int ret;

	ProcessEventLogInit(&log, process);
	if ((process->status & PROCESS_STATE_MASK) == PROCESS_STATE_RUNNING) {
		log.result = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		k_sceEventLogPut(0x2711, 3, 0, &log, sizeof(log));
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1824, "illegal process status. 0x%08x\n",
				     process->status & PROCESS_STATE_MASK);
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
	}
	timer = SceThreadmgrForDriver_C58DF384("SceProcessEvent", 10000000, ProcessEventTimeupHandler, NULL);
	if (timer < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1847, "set alarm failed. 0x%08x\n", timer);
	sceKernelChangeThreadPriority2(0, 0x60);
	param.size = sizeof(param);
	param.state = state;
	param.kind = kind;
	param.type = process->type;
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_KILL, 0, &param, NULL, 0);
	SceCpuForDriver_BF82DEB2(&g_threadIdVector.lock);
	ret = sceKernelGetThreadIdVector(process, g_threadIdVector.id, &count);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1874, "sceKernelGetThreadIdVector failed. 0x%08x\n", ret);
	for (i = 0; i < count; i++) {
		ret = SceThreadmgrForKernel_9928DFBA(g_threadIdVector.id[i], 0xf);
		if (ret != (int)SCE_KERNEL_ERROR_DORMANT) {
			if (ThreadGone(ret))
				continue;
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1894,
					"sceKernelStopThread failed 0x%08x, tid 0x%08x\n", ret,
					g_threadIdVector.id[i]);
				SCE_DBG_ASSERT_FAIL(1895);
			}
		}
		sceKernelWaitThreadEnd(g_threadIdVector.id[i], NULL, NULL);
		ret = sceKernelDeleteThread(g_threadIdVector.id[i]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1901, "sceKernelDeleteThread failed 0x%08x, tid 0x%08x\n",
					     ret, g_threadIdVector.id[i]);
			SCE_DBG_ASSERT_FAIL(1902);
		}
	}
	sceKernelSpinlockLowUnlock(&g_threadIdVector.lock);
	ret = DeleteProcess(process);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1910, "Delete process(0x%08x) failed 0x%08x\n",
				     process->pid, ret);
	sceKernelChangeThreadPriority2(0, 0x3f);
	sceKernelCancelAlarm(timer);
	k_sceEventLogPut(0x2711, 3, 0, &log, sizeof(log));
	return 0;
}

/* name unknown: queue a request for the process event thread */
static int ProcessSendRequest(SceProcessObject *process, SceUInt32 request)
{
	ProcessRequest *req;
	int intr;

	req = SceSysmemForKernel_85571907(sizeof(*req));
	if (req == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ListInit(&req->node);
	req->process = process;
	req->request = request;
	if (process->pendingRequests == 0 && PollRequestPermission() < 0) {
		SceSysmemForKernel_4233C16D(req);
		return SCE_KERNEL_ERROR_PROCESSMGR_NO_REQUEST;
	}
	intr = SceCpuForDriver_D32ACE9E(&g_requestLock);
	ListAddTail(&g_requestList, &req->node);
	g_requestCount++;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_requestLock, intr);
	process->pendingRequests++;
	return sceKernelSetEventFlag(g_eventFlag, 1);
}

/* kind 0: exit, 1: force kill, 2: exit spawn */
static int KillProcessCommon(SceUID pid, int kind)
{
	SceProcessObject *process, *spawn;
	UIDCreateOpt uidOpt;
	SceUID spawnPid;
	int intr, ret, slot;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5149, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	pid = process->pid;
	if (process->type == PROCESS_TYPE_KERNEL) {
		sceGUIDReleaseObject(pid);
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if ((process->status & PROCESS_STATE_MASK) == PROCESS_STATE_EXITED
	    || (process->status & PROCESS_STATE_MASK) == PROCESS_STATE_NONE) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		sceGUIDReleaseObject(process->pid);
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	if (kind == 1) {
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		if (process->status & PROCESS_FLAG_FORCE_KILL) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		} else if (process->pendingRequests == 0) {
			ret = ProcessSendRequest(process, PROCESS_REQUEST_FORCE_KILL);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 5194, "sending kill event failed 0x%08x\n", ret);
			else
				process->status |= PROCESS_FLAG_EXIT | PROCESS_FLAG_FORCE_KILL;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		} else {
			process->status |= PROCESS_FLAG_EXIT | PROCESS_FLAG_FORCE_KILL;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			sceKernelSetEventFlag(g_eventFlag, 1);
			ret = 0;
		}
		sceGUIDReleaseObject(process->pid);
		return ret;
	}
	if (kind == 2) {
		slot = PPMAllocSlot();
		if (slot < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 5217,
				"ExitSpawn: process number limit exceeded. num of processes %d\n",
				g_processCount);
			sceGUIDReleaseObject(process->pid);
			return slot;
		}
		spawn = PPMAllocMemory(slot, sizeof(*spawn));
		if (spawn == NULL) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 5224, "PPMAllocMemory failed\n");
			PPMFreeSlot(slot);
			sceGUIDReleaseObject(process->pid);
			return SCE_KERNEL_ERROR_NO_FREE_HEAP;
		}
		memset(spawn, 0, sizeof(*spawn));
		uidOpt.flags = UID_CREATE_OPT_OBJECT | UID_CREATE_OPT_ATTR;
		uidOpt.object = (SceObjectBase *)spawn;
		uidOpt.attr = 0x8000000;
		spawnPid = sceGUIDKernelCreateWithOpt(&g_processClass, "SceKernelProcessExitSpawnTemp",
						       &uidOpt, &spawn);
		if (spawnPid < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 5240, "Create Process UID failed 0x%08x\n", spawnPid);
			PPMFreeSlot(slot);
			sceGUIDReleaseObject(process->pid);
			return spawnPid;
		}
		spawn->slot = slot;
		spawn->pid = spawnPid;
		g_exitSpawn.process = spawn;
		g_exitSpawn.pid = pid;
		memcpy(&g_exitSpawn.authInfo, &process->authInfo, sizeof(g_exitSpawn.authInfo));
	} else if (kind == 0) {
		spawnPid = 0;
	} else {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if ((process->status & (PROCESS_FLAG_PRELOAD | PROCESS_FLAG_EXIT | PROCESS_FLAG_FORCE_SUSPEND))) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		if (kind == 2 && spawnPid > 0) {
			memset(&g_exitSpawn, 0, sizeof(g_exitSpawn));
			sceGUIDClose(spawnPid);
			PPMFreeSlot(g_exitSpawn.process->slot);
		}
	} else {
		ret = ProcessSendRequest(process, kind == 2 ? PROCESS_REQUEST_EXIT_SPAWN
							    : PROCESS_REQUEST_KILL);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 5277, "send kill event failed 0x%08x\n", ret);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			if (kind == 2 && spawnPid > 0) {
				memset(&g_exitSpawn, 0, sizeof(g_exitSpawn));
				sceGUIDClose(spawnPid);
				PPMFreeSlot(g_exitSpawn.process->slot);
			}
		} else {
			process->status |= PROCESS_FLAG_EXIT;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		}
	}
out:
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* name unknown: wait until the resumed threads of one group left the suspended state */
static inline __attribute__((always_inline)) void WaitThreadGroup(SceUID *thid, int count,
								   SceUInt32 flag)
{
	struct ThreadmgrThreadObject *thread;
	SceUID id;
	int remaining, i;

	remaining = count;
	do {
		sceKernelDelayThread(5000);
		for (i = 0; i != count; i++) {
			id = thid[i];
			if (id == -1)
				continue;
			if (SceSysmemForDriver_00ED6C14(id, g_threadClass, &thread) < 0) {
				thid[i] = -1;
				remaining--;
				continue;
			}
			if (!(thread->context.flags & flag)) {
				remaining--;
				thid[i] = -1;
			}
			sceGUIDReleaseObject(id);
		}
	} while (remaining > 0);
}

static inline __attribute__((always_inline)) void _ResumeProcess(SceProcessObject *process,
								  SceUInt32 state)
{
	ThreadCB *thread;
	ProcEventStateParam event;
	ProcEventParam param;
	ProcessEventLog log;
	ListNode *node;
	ProcessBudget *budget;
	SceUID *thid, *groupB, *rest, handler;
	int count, na, nb, nc, intr, ret, type, i;
#if FW_DEBUG
	int retry;
#endif

	ProcessEventLogInit(&log, process);
	ret = BudgetLock(process->budget);
	if (ret < 0) {
		log.result = ret;
		k_sceEventLogPut(0x2711, 5, 0, &log, sizeof(log));
		SCE_DBG_ASSERT_FAIL(1340);
		return;
	}
	budget = process->budget;
	type = process->type;
	if (!(process->attr & PROCESS_ATTR_NOT_ACTIVE))
		budget->activeCount++;
	if (type == PROCESS_TYPE_GAME && budget->activeCount != 0) {
		event.size = sizeof(event);
		event.pid = process->pid;
		event.state = process->status & PROCESS_STATE_MASK;
		event.request = 0;
		event.type = type;
		sceKernelInvokeProcEventHandler(event.pid, PROC_EVENT_PRELOAD, 0, &event, NULL, 0);
		SetMiniAppCpuAffinity(8);
		budget = process->budget;
	}
	BudgetUnlock(budget);
	param.size = sizeof(param);
	param.state = state;
	param.kind = 0;
	param.type = process->type;
	for (type = 0x10000; type != 0x10010; type++)
		sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_RESUME, type, &param, NULL, 0);
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_RESUME, 0x20000, &param, NULL, 0);
#if FW_DEBUG
	retry = 0;
#endif
	for (;;) {
		handler = 0;
		sceKernelDelayThread(5000);
		ret = sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_RESUME, 0x20001, &param,
						       &handler, 0);
		if (ret >= 0)
			break;
#if FW_DEBUG
		if (++retry % 1000 == 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1452, "count %d, Busy handler %s, error code 0x%08x\n",
					     retry, SceSysmemForDriver_E655852F(handler), ret);
#endif
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	process->prevState = 0;
	process->status = (process->status & ~PROCESS_STATE_MASK) | PROCESS_STATE_RUNNING;
	sceKernelClearEventFlag(process->eventFlag, ~PROCESS_EVF_SUSPENDED);
	sceKernelSetEventFlag(process->eventFlag, PROCESS_EVF_RESUMED);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_RESUME, 0x40000, &param, NULL, 0);
	intr = SceCpuForDriver_D32ACE9E(&process->timeLock);
	process->timeRunning = 1;
	process->time = sceKernelGetSystemTimeWide() - process->time;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->timeLock, intr);
	if (process->libkernelAddr[4] != NULL) {
		ret = sceKernelCopyToUserProc(process->pid, process->libkernelAddr[4], &process->time,
					       sizeof(process->time));
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1485, "sceKernelCopyToUserProc failed 0x%08x\n", ret);
			log.result = ret;
			k_sceEventLogPut(0x2711, 5, 0, &log, sizeof(log));
			SCE_DBG_ASSERT_FAIL(1489);
			return;
		}
	}
	SceThreadmgrForKernel_89C17825(process);
	process->threadsStopped = 0;
	count = process->unlimitedThreadCount + process->threadCount;
	thid = SceSysmemForDriver_7B4CB60A(process->uniqueHeap, count * 3 * sizeof(*thid));
	if (thid == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1518, "sceKernelAllocHeapMemory failed %p\n", NULL);
		return;
	}
	groupB = thid + count;
	rest = thid + count * 2;
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	node = process->threads.next;
	if (node == &process->threads) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	} else {
		na = 0;
		nb = 0;
		nc = 0;
		do {
			thread = THREAD_FROM_PROCESS_LINK(node);
			if (thread->flags & THREAD_FLAG_COND_KEEP_MUTEX)
				thid[na++] = thread->threadId;
			else if (thread->flags & THREAD_FLAG_LW_COND_KEEP_MUTEX)
				groupB[nb++] = thread->threadId;
			else
				rest[nc++] = thread->threadId;
			node = node->next;
		} while (node != &process->threads);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		if (na != 0) {
			for (i = 0; i != na; i++) {
				ret = ProcessThreadResume(thid[i], PROCESS_STATE_SUSPENDED);
				if (ret != (int)SCE_KERNEL_ERROR_DELETED
				    && ret != (int)SCE_KERNEL_ERROR_DORMANT && ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 1551,
						"ProcessThreadResume failed 0x%08x, tid 0x%08x\n", ret, thid[i]);
			}
			WaitThreadGroup(thid, na, THREAD_FLAG_COND_KEEP_MUTEX);
		}
		if (nb != 0) {
			for (i = 0; i != nb; i++) {
				ret = ProcessThreadResume(groupB[i], PROCESS_STATE_SUSPENDED);
				if (ret != (int)SCE_KERNEL_ERROR_DELETED
				    && ret != (int)SCE_KERNEL_ERROR_DORMANT && ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 1592,
						"ProcessThreadResume failed 0x%08x, tid 0x%08x\n", ret, groupB[i]);
			}
			WaitThreadGroup(groupB, nb, THREAD_FLAG_LW_COND_KEEP_MUTEX);
		}
		for (i = 0; i != nc; i++) {
			ret = ProcessThreadResume(rest[i], PROCESS_STATE_SUSPENDED);
			if (!ThreadGone(ret) && ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1635,
					"ProcessThreadResume failed 0x%08x, tid 0x%08x, i=%d\n", ret, rest[i], i);
				SCE_DBG_ASSERT_FAIL(1636);
			}
		}
	}
	ret = sceKernelFreeHeapMemory(process->uniqueHeap, thid);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1643, "sceKernelAllocHeapMemory failed(0x%08X) %p\n", ret, thid);
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 1647, "Process [%s] is RESUMED!\n", SceSysmemForDriver_E655852F(process->pid));
	k_sceEventLogPut(0x2711, 5, 0, &log, sizeof(log));
	if (SceSysrootForDriver_6050A467(process->pid) != 0)
		SceSysrootForDriver_3F76D531(process->pid);
}

static inline __attribute__((always_inline)) void _ExitProcess(SceProcessObject *process,
								SceUInt32 state)
{
	ProcEventParam param;
	ProcessEventLog log;
	SceSize count, i;
	SceUID timer;
	int ret;

	ProcessEventLogInit(&log, process);
	if ((process->status & PROCESS_STATE_MASK) == PROCESS_STATE_RUNNING) {
		log.result = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		k_sceEventLogPut(0x2711, 2, 0, &log, sizeof(log));
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1683, "illegal process status. 0x%08x\n",
				     process->status & PROCESS_STATE_MASK);
		return;
	}
	timer = SceThreadmgrForDriver_C58DF384("SceProcessEvent", 10000000, ProcessEventTimeupHandler, NULL);
	if (timer < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1706, "set alarm failed. 0x%08x\n", timer);
	sceKernelChangeThreadPriority2(0, 0x60);
	param.size = sizeof(param);
	param.state = state;
	param.kind = 0;
	param.type = process->type;
	sceKernelInvokeProcEventHandler(process->pid, PROC_EVENT_EXIT, 0, &param, NULL, 0);
	SceCpuForDriver_BF82DEB2(&g_threadIdVector.lock);
	ret = sceKernelGetThreadIdVector(process, g_threadIdVector.id, &count);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1733, "sceKernelGetThreadIdVector failed. 0x%08x\n", ret);
	for (i = 0; i < count; i++) {
		ret = SceThreadmgrForKernel_9928DFBA(g_threadIdVector.id[i], 0xf);
		if (ret != (int)SCE_KERNEL_ERROR_DORMANT) {
			if (ThreadGone(ret))
				continue;
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1753,
					"sceKernelStopThread failed 0x%08x, tid 0x%08x\n", ret,
					g_threadIdVector.id[i]);
				SCE_DBG_ASSERT_FAIL(1754);
			}
		}
		sceKernelWaitThreadEnd(g_threadIdVector.id[i], NULL, NULL);
		ret = sceKernelDeleteThread(g_threadIdVector.id[i]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1760, "sceKernelDeleteThread failed 0x%08x, tid 0x%08x\n",
					     ret, g_threadIdVector.id[i]);
			SCE_DBG_ASSERT_FAIL(1761);
		}
	}
	sceKernelSpinlockLowUnlock(&g_threadIdVector.lock);
	ret = DeleteProcess(process);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1769, "Delete process(0x%08x) failed 0x%08x\n",
				     process->pid, ret);
	sceKernelChangeThreadPriority2(0, 0x3f);
	sceKernelCancelAlarm(timer);
	k_sceEventLogPut(0x2711, 2, 0, &log, sizeof(log));
}

/* the record of an ExitSpawn */
typedef struct ProcessExitSpawnLog {
	ProcessEventLog process;
	SceUID spawnPid;
	SceUInt32 reserved;
	SceSize spawnNameLength;
	char spawnName[12];
} ProcessExitSpawnLog;

/* name unknown: drop every queued request of a process; the next one to handle */
/* (returns with the request lock held) */
static inline __attribute__((always_inline)) ListNode *PurgeRequests(SceProcessObject *process,
									     int *intr)
{
	ListNode *node, *next, *first;
	ProcessRequest *req;

	*intr = SceCpuForDriver_D32ACE9E(&g_requestLock);
	if (g_requestList.next == &g_requestList) {
		sceKernelSignalSema(g_requestSema, 1);
		return &g_requestList;
	}
	first = NULL;
	for (node = g_requestList.next; node != &g_requestList; node = next) {
		next = node->next;
		req = (ProcessRequest *)node;
		if (req->process == process) {
			g_requestCount--;
			ListRemove(node);
			SceSysmemForKernel_4233C16D(req);
		} else if (first == NULL) {
			first = node;
		}
	}
	if (first == NULL)
		first = &g_requestList;
	sceKernelSignalSema(g_requestSema, 1);
	return first;
}

/* the SceKernelProcessEvent thread: carries out the queued process requests */
static int sceKernelProcessEventThread(SceSize args, void *argp)
{
	SceProcessObject *process;
	ProcessExitSpawnLog spawnLog;
	ListNode busy, *node, *n;
	ProcessRequest *req;
	SceUInt32 status, state, request, kind;
	SceUInt64 now;
	SceSize count, i;
	SceUID pid;
	int exitStatus, intr, ret;

	(void)args;
	(void)argp;
	for (;;) {
		ret = sceKernelWaitEventFlag(g_eventFlag, 1, 5, NULL, NULL);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(1964);
			return ret;
		}
		ListInit(&busy);
		intr = SceCpuForDriver_D32ACE9E(&g_requestLock);
		node = g_requestList.next;
next_request:
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_requestLock, intr);
		if (node == &g_requestList)
			continue;
		req = (ProcessRequest *)node;
		process = req->process;
		if (process->status & (PROCESS_FLAG_FORCE_KILL | PROCESS_FLAG_FORCE_SUSPEND)) {
			if (process->status & PROCESS_FLAG_FORCE_KILL) {
				request = PROCESS_REQUEST_FORCE_KILL;
				kind = 0x10;
				SCE_DBG_PRINTF_LEVEL(2, 0xb, 1997, "force term flag was set\n");
			} else {
				request = PROCESS_REQUEST_FORCE_SUSPEND;
				kind = 0x40;
				SCE_DBG_PRINTF_LEVEL(2, 0xb, 2001, "force suspend flag was set\n");
			}
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			state = process->status & PROCESS_STATE_MASK;
			if (state == PROCESS_STATE_RUNNING
			    || ((process->status & 0x10) && process->prevState == PROCESS_STATE_RUNNING)) {
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
				if (!(process->status & PROCESS_FLAG_SUSPENDING))
					ProcessNotifySuspending(process, state, kind);
			} else {
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			}
			goto dispatch;
		}
		for (n = busy.next; n != &busy; n = n->next) {
			if (process == PROCESS_FROM_RECOVERY(n)) {
				intr = SceCpuForDriver_D32ACE9E(&g_requestLock);
				node = node->next;
				goto next_request;
			}
		}
		ret = sceKernelWaitSema(process->lockSema, 1, NULL);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(2041);
			goto make_busy;
		}
		request = req->request;
		status = process->status;
		state = status & PROCESS_STATE_MASK;
		switch (request) {
		case PROCESS_REQUEST_EXIT:
		case PROCESS_REQUEST_KILL:
		case PROCESS_REQUEST_SUSPEND:
		case PROCESS_REQUEST_EXIT_SPAWN:
			if (status & 0x10)
				goto unlock_busy;
			if (process->status & PROCESS_FLAG_SUSPENDING) {
				kind = status & 0x10;
			} else if (state == PROCESS_STATE_RUNNING) {
				kind = request == PROCESS_REQUEST_EXIT_SPAWN ? 0x20 : 0;
				ProcessNotifySuspending(process, PROCESS_STATE_RUNNING, kind);
			} else {
				kind = 0;
			}
			break;
		case PROCESS_REQUEST_RESUME:
			if (status & 0x10)
				goto unlock_busy;
			kind = status & 0x10;
			break;
		default:
			kind = 0;
			break;
		}
		ret = sceKernelWaitSema(process->eventSema, 4, NULL);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(2091);
		} else {
			intr = SceCpuForDriver_D32ACE9E(&process->powerLock);
			if ((int)process->powerLockCount < 1) {
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->powerLock, intr);
				goto dispatch;
			}
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2099, "Process(0x%08x) PowerLock is acquired. num %d\n",
					     process->pid, process->powerLockCount);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->powerLock, intr);
			sceKernelSignalSema(process->eventSema, 4);
		}
unlock_busy:
		sceKernelSignalSema(process->lockSema, 1);
make_busy:
		intr = SceCpuForDriver_D32ACE9E(&g_requestLock);
		ListAddTail(&busy, &process->recoveryNode);
		node = node->next;
		goto next_request;

dispatch:
		if (request == PROCESS_REQUEST_RESUME) {
			if (state == PROCESS_STATE_RUNNING) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 2344, "process [%s] is already resumed.\n",
						     SceSysmemForDriver_E655852F(process->pid));
				goto signal_event;
			}
			sceKernelSignalSema(process->eventSema, 4);
			_ResumeProcess(process, state);
			goto done;
		}
		switch (request) {
		case PROCESS_REQUEST_SUSPEND:
			if (state == PROCESS_STATE_SUSPENDED) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 2116, "process [%s] is already suspended.\n",
						     SceSysmemForDriver_E655852F(process->pid));
			} else {
				ret = _SuspendProcess(process, state, 0);
				if (ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 2124, "process(%s) suspend failed. 0x%08x\n",
							     SceSysmemForDriver_E655852F(process->pid), ret);
				else if (SceSysrootForDriver_6050A467(process->pid) != 0)
					SceSysrootForDriver_F2BC9E1D(process->pid);
			}
			goto signal_event;
		case PROCESS_REQUEST_EXIT:
			if (state == PROCESS_STATE_RUNNING) {
				ret = _SuspendProcess(process, PROCESS_STATE_RUNNING, 0);
				if (ret < 0) {
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 2144, "process(%s) suspend failed. 0x%08x\n",
							     SceSysmemForDriver_E655852F(process->pid), ret);
					goto signal_event;
				}
			}
			CloseStdio(process);
			pid = process->pid;
			exitStatus = process->exitStatus;
			_ExitProcess(process, state);
			if (SceDipswForDriver_A98FC2FD(0xc2) != 0) {
				now = sceKernelGetSystemTimeWide();
				SceSysrootForDriver_8498E23F(pid, exitStatus, now);
				SceSysrootForDriver_9D058A32(pid, exitStatus, now);
			}
			sceKernelSignalSema(g_requestSema, 1);
			goto remove;
		case PROCESS_REQUEST_KILL:
		case PROCESS_REQUEST_EXIT_SPAWN:
			if (state == PROCESS_STATE_RUNNING) {
				ret = _SuspendProcess(process, PROCESS_STATE_RUNNING, kind);
				if (ret < 0) {
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 2179, "process(%s) suspend failed. 0x%08x\n",
							     SceSysmemForDriver_E655852F(process->pid), ret);
					goto signal_event;
				}
			}
			CloseStdio(process);
			pid = process->pid;
			if (request == PROCESS_REQUEST_EXIT_SPAWN) {
				memset(&spawnLog, 0, sizeof(spawnLog));
				spawnLog.process.pid = pid;
				spawnLog.process.nameLength = 10;
				strncpy(spawnLog.process.name, process->TitleId, 10);
				spawnLog.spawnPid = g_exitSpawn.process->pid;
				spawnLog.spawnNameLength = 10;
				strncpy(spawnLog.spawnName, g_exitSpawn.process->TitleId, 10);
				_KillProcess(process, state, kind);
				if (SceDipswForDriver_A98FC2FD(0xc2) != 0) {
					SCE_DBG_ASSERT_CHECK(g_exitSpawn.pid == pid, 2210);
					now = sceKernelGetSystemTimeWide();
					SceSysrootForDriver_77CF3FBD(pid, g_exitSpawn.process->pid, now);
					SceSysrootForDriver_1D61C837(pid, g_exitSpawn.process->pid, now);
				}
			} else {
				_KillProcess(process, state, kind);
			}
			if (SceDipswForDriver_A98FC2FD(0xc2) != 0 && state != PROCESS_STATE_CREATED) {
				now = sceKernelGetSystemTimeWide();
				SceSysrootForDriver_6D44F190(pid, now);
				SceSysrootForDriver_B501E861(pid, now);
			}
			if (request == PROCESS_REQUEST_EXIT_SPAWN)
				k_sceEventLogPut(0x2711, 6, 0, &spawnLog, sizeof(spawnLog));
			sceKernelSignalSema(g_requestSema, 1);
			goto remove;
		case PROCESS_REQUEST_FORCE_SUSPEND:
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			goto force_suspend;
		case PROCESS_REQUEST_FORCE_KILL:
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			goto force_kill;
		default:
			SCE_DBG_ASSERT_FAIL(2454);
			goto signal_event;
		}

signal_event:
		sceKernelSignalSema(process->eventSema, 4);
done:
		sceKernelSignalSema(process->lockSema, 1);
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		if (process->status & PROCESS_FLAG_FORCE_KILL)
			goto force_kill;
		if (process->status & PROCESS_FLAG_FORCE_SUSPEND)
			goto force_suspend;
		if (--process->pendingRequests == 0)
			sceKernelSignalSema(g_requestSema, 1);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
remove:
		intr = SceCpuForDriver_D32ACE9E(&g_requestLock);
		n = node->next;
		g_requestCount--;
		ListRemove(node);
		SceSysmemForKernel_4233C16D(node);
		node = n;
		goto next_request;

force_suspend:
		for (;;) {
			status = process->status;
			if ((status & PROCESS_STATE_MASK) != PROCESS_STATE_RUNNING
			    && (!(process->status & 0x10) || process->prevState != PROCESS_STATE_RUNNING))
				goto force_check_kill;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			SceCpuForDriver_BF82DEB2(&g_threadIdVector.lock);
			ret = sceKernelGetThreadIdVector(process, g_threadIdVector.id, &count);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 2381, "sceKernelGetThreadIdVector failed. 0x%08x\n",
						     ret);
			for (i = 0; i < count; i++) {
				ret = SceThreadmgrForKernel_15D6357C(g_threadIdVector.id[i], 0xf);
				if (!ThreadGone(ret) && ret < 0) {
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 2394,
						"sceKernelSuspendThread failed 0x%08x, tid 0x%08x\n", ret,
						g_threadIdVector.id[i]);
					SCE_DBG_ASSERT_FAIL(2395);
					goto force_suspend_retry;
				}
			}
			sceKernelSpinlockLowUnlock(&g_threadIdVector.lock);
			ret = _SuspendProcess(process, status & PROCESS_STATE_MASK, 0x40);
			if (ret >= 0)
				break;
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 2406, "process(%s) suspend failed. 0x%08x\n",
					     SceSysmemForDriver_E655852F(process->pid), ret);
force_suspend_retry:
			sceKernelSpinlockLowUnlock(&g_threadIdVector.lock);
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
		}
		if (SceSysrootForDriver_6050A467(process->pid) != 0)
			SceSysrootForDriver_F2BC9E1D(process->pid);
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
force_check_kill:
		if (process->status & PROCESS_FLAG_FORCE_KILL)
			goto force_kill;
		process->pendingRequests = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		node = PurgeRequests(process, &intr);
		goto next_request;

force_kill:
		for (;;) {
			state = process->status & PROCESS_STATE_MASK;
			if (state != PROCESS_STATE_RUNNING
			    && (!(process->status & 0x10) || process->prevState != PROCESS_STATE_RUNNING))
				break;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			SceCpuForDriver_BF82DEB2(&g_threadIdVector.lock);
			ret = sceKernelGetThreadIdVector(process, g_threadIdVector.id, &count);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 2264, "sceKernelGetThreadIdVector failed. 0x%08x\n",
						     ret);
			for (i = 0; i < count; i++) {
				ret = SceThreadmgrForKernel_9928DFBA(g_threadIdVector.id[i], 0xf);
				if (!ThreadGone(ret) && ret < 0) {
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 2277,
						"sceKernelStopThread failed 0x%08x, tid 0x%08x\n", ret,
						g_threadIdVector.id[i]);
					SCE_DBG_ASSERT_FAIL(2278);
					break;
				}
			}
			sceKernelSpinlockLowUnlock(&g_threadIdVector.lock);
			if (i < count) {
				intr = SceCpuForDriver_D32ACE9E(&process->lock);
				continue;
			}
			ret = _SuspendProcess(process, state, 0x10);
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			if (ret >= 0)
				break;
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 2290, "process(%s) suspend failed. 0x%08x\n",
					     SceSysmemForDriver_E655852F(process->pid), ret);
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		CloseStdio(process);
		pid = process->pid;
		_KillProcess(process, state, 0x10);
		if (SceDipswForDriver_A98FC2FD(0xc2) != 0 && state != PROCESS_STATE_CREATED) {
			now = sceKernelGetSystemTimeWide();
			SceSysrootForDriver_6D44F190(pid, now);
			SceSysrootForDriver_B501E861(pid, now);
		}
		node = PurgeRequests(process, &intr);
		goto next_request;
	}
}

/* suspend a process for the debugger (state 0x11..0x15) */
int sceKernelProcessDebugSuspend(SceUID pid, SceUInt32 state)
{
	SceProcessObject *process = NULL;
	SceUInt32 prev;
	int intr, ret;

	ret = UIDtoProcess(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6162, "UIDtoProcess(pid 0x%08X) failed 0x%08x\n", pid, ret);
		return ret;
	}
	if (process->type == PROCESS_TYPE_KERNEL)
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
	if (!(state & 0x10))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (SceExcpmgrForKernel_B615A7DA() != 0) {
		/* in an exception: no waiting for the process lock */
		do {
			prev = process->status & PROCESS_STATE_MASK;
#if FW_DEBUG
			if (prev == PROCESS_STATE_EXITED)
				return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
#endif
			if (prev != PROCESS_STATE_RUNNING) {
				SCE_DBG_PRINTF_LEVEL(2, 0xb, 6268, "invalid status 0x%08x\n", prev);
				return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
			}
		} while (process->status & PROCESS_FLAG_RESUMING);
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		if (process->status & (PROCESS_FLAG_FORCE_KILL | PROCESS_FLAG_FORCE_SUSPEND))
			return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		ret = sceKernelClearEventFlag(process->eventFlag, ~PROCESS_EVF_NOT_DEBUG_SUSPENDED);
		if (ret < 0)
			return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		process->status |= PROCESS_FLAG_DEBUG_SUSPENDING;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		ret = _SuspendAllThread(process);
		if (ret < 0) {
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			process->status &= ~PROCESS_FLAG_DEBUG_SUSPENDING;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		} else {
			SceCpuForDriver_BF82DEB2(&process->lock);
			process->prevState = PROCESS_STATE_RUNNING;
			process->status = (process->status & ~PROCESS_STATE_MASK) | (state & 0xff);
			process->status &= ~PROCESS_FLAG_DEBUG_SUSPENDING;
			sceKernelSpinlockLowUnlock(&process->lock);
		}
		return ret;
	}
	if (getCurrentThreadCB() == NULL) {
		ret = sceKernelPollSema(process->lockSema, 1);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6191, "process lock failed. 0x%08x\n", ret);
			return ret;
		}
	} else {
		ret = sceKernelWaitSema(process->lockSema, 1, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6184, "process lock failed. 0x%08x\n", ret);
			return ret;
		}
	}
	prev = process->status & PROCESS_STATE_MASK;
	if ((prev == PROCESS_STATE_RUNNING
	     && !(process->status & (PROCESS_FLAG_FORCE_KILL | PROCESS_FLAG_FORCE_SUSPEND)))
	    || prev == PROCESS_STATE_SUSPENDED) {
		if (!(process->status & PROCESS_FLAG_PRELOAD)) {
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			ret = sceKernelClearEventFlag(process->eventFlag, ~PROCESS_EVF_NOT_DEBUG_SUSPENDED);
			if (ret < 0) {
#if FW_DEBUG
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
				sceKernelSignalSema(process->lockSema, 1);
#else
				sceKernelSignalSema(process->lockSema, 1);
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
#endif
				return ret;
			}
			process->status |= PROCESS_FLAG_DEBUG_SUSPENDING;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			ret = _SuspendAllThread(process);
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			if (ret >= 0) {
				process->prevState = prev;
				process->status = (process->status & ~PROCESS_STATE_MASK) | (state & 0xff);
			}
			process->status &= ~PROCESS_FLAG_DEBUG_SUSPENDING;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			sceKernelSignalSema(process->lockSema, 1);
			return ret;
		}
	}
	sceKernelSignalSema(process->lockSema, 1);
	return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
}

/* free/alloc in the unique heap of the current process */
int SceProcessmgrForDriver_FC2A424E(void *ptr)
{
	SceProcessObject *process = GetCurrentProcess();

	return sceKernelFreeHeapMemory(process->uniqueHeap, ptr);
}

static void *ProcessAllocCurrentHeapMemory(SceSize size, void *opt, const void *caller)
{
	SceProcessObject *process = GetCurrentProcess();

	return SceSysmemForDriver_B415B5A8(process->uniqueHeap, size, opt, caller);
}

static int DecProcessResource(SceUInt32 type)
{
	SceProcessObject *process = GetCurrentProcess();
	ProcessResource *resource;
	int intr;

	if (process == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6916, "There is no current ProcessCB: 0x%08x\n", type);
		return SCE_KERNEL_ERROR_ERROR;
	}
	resource = &process->resource[type];
	intr = SceCpuForDriver_D32ACE9E(&resource->lock);
	if (resource->count != 0)
		resource->count--;
	else
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6908, "Resource underflow: type=0x%08x\n", type);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&resource->lock, intr);
	return 0;
}

static inline __attribute__((always_inline)) void ShowProcessResource(const char *title,
								       SceUInt32 type,
								       ProcessResource *resource)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 6826, "%s: %d: %5d[0x%08x] MAX %5d[0x%08x] LIMIT %5d[0x%08x]\n",
			     title, type, resource->count, resource->count, resource->peak, resource->peak,
			     resource->limit, resource->limit);
}

static int IncProcessResource(SceUInt32 type)
{
	SceProcessObject *process;
	ProcessResource *resource;
	SceUInt32 count;
	int intr, ret;

	if (type > 2) {
		SCE_PRINTF_WITH_INFO(0xb, 6840, "%s:type=0x%08x\n", "IncProcessResource", type);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	process = GetCurrentProcess();
	if (process == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6886, "There is no current ProcessCB: type=0x%08x\n", type);
		return 0;
	}
	resource = &process->resource[type];
	intr = SceCpuForDriver_D32ACE9E(&resource->lock);
	count = ++resource->count;
	if (count > resource->peak) {
#if FW_DEBUG
		if (type == 0 && (float)resource->limit * 0.75f < (float)resource->peak)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6859,
				"%p:UPDATE MAX for type=0x%08x, C=0x%08x/M=0x%08x/L=0x%08x\n", process, 0,
				count, resource->peak, resource->limit);
#endif
		resource->peak = count;
	}
	if (count > resource->limit) {
		SCE_PRINTF_WITH_INFO(0xb, 6878, "%s:type=0x%08x, INC FAILED 0x%08x > 0x%08x\n",
				     "IncProcessResource", type, count, resource->limit);
		ret = SCE_KERNEL_ERROR_ERROR;
		ShowProcessResource("INC FAIL:", type, resource);
		resource->count--;
	} else {
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&resource->lock, intr);
	return ret;
}

static int ResumeProcessCommon(SceUID pid)
{
	SceProcessObject *process, *current;
	int intr, ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6319, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	pid = process->pid;
	if (process->type == PROCESS_TYPE_KERNEL) {
		ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
		goto out;
	}
	current = GetCurrentProcess();
	if (current != NULL && current->pid == pid) {
		ret = 0x80024001;
		goto out;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if ((process->status & PROCESS_STATE_MASK) == PROCESS_STATE_EXITED
	    || (process->status & PROCESS_STATE_MASK) == PROCESS_STATE_CREATED
	    || (process->status & PROCESS_STATE_MASK) == PROCESS_STATE_NONE
	    || (process->status & (PROCESS_FLAG_PRELOAD | PROCESS_FLAG_FORCE_SUSPEND | PROCESS_FLAG_EXIT))) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
	} else {
		ret = ProcessSendRequest(process, PROCESS_REQUEST_RESUME);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6357, "send resume event failed 0x%08x\n", ret);
		else
			ret = 0;
	}
out:
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* flags 1: wait until suspended, 2: forced */
static int SuspendProcessCommon(SceUID pid, SceUInt32 flags, SceUInt32 *timeout)
{
	SceProcessObject *process, *current;
	SceUInt32 request;
	int intr, ret;

	if (flags & ~3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5983, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	pid = process->pid;
	if (process->type == PROCESS_TYPE_KERNEL) {
		ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
		goto out;
	}
	if (flags & 1) {
		current = GetCurrentProcess();
		if (current == NULL || current->pid == pid) {
			ret = 0x80024001;
			goto out;
		}
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if ((process->status & PROCESS_STATE_MASK) == PROCESS_STATE_EXITED
	    || (process->status & PROCESS_STATE_MASK) == PROCESS_STATE_CREATED
	    || (process->status & PROCESS_STATE_MASK) == PROCESS_STATE_NONE
	    || (process->status & (PROCESS_FLAG_PRELOAD | PROCESS_FLAG_FORCE_SUSPEND)))
		goto invalid;
	if (flags & 2) {
		process->status |= PROCESS_FLAG_FORCE_SUSPEND;
		request = PROCESS_REQUEST_FORCE_SUSPEND;
		if (process->pendingRequests != 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			sceKernelSetEventFlag(g_eventFlag, 1);
			ret = 0;
			goto out;
		}
	} else {
		if (process->status & PROCESS_FLAG_EXIT)
			goto invalid;
		request = PROCESS_REQUEST_SUSPEND;
	}
	ret = ProcessSendRequest(process, request);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6041, "send suspend event failed 0x%08x\n", ret);
		process->status &= ~PROCESS_FLAG_FORCE_SUSPEND;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		goto out;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	if (flags & 1) {
		ret = sceKernelWaitEventFlag(process->eventFlag, PROCESS_EVF_SUSPENDED, 4, NULL, timeout);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6055, "sceKernelWaitEventFlag failed. 0x%08x\n", ret);
		else
			ret = 0;
	} else {
		ret = 0;
	}
	goto out;
invalid:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
out:
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* SceSysrootForDriver_73522F65's output */
#define PROCESS_DEBUG_WAIT		0x2	/* name unknown */
#define PROCESS_DEBUG_WAIT_SUSPENDED	0x4	/* name unknown */

static int StartProcessCommon(SceUID pid, int waitDebugger, SceSize args, void *argp,
			      SceUInt32 flags)
{
	SceProcessObject *process;
	ProcessPreloadParam param;
	ProcessEventLog log;
	SceUInt32 status, debugFlags;
	SceUInt64 now;
	SceUID thread;
	int debug, exitStatus, intr, ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4790, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	pid = process->pid;
	ProcessEventLogInit(&log, process);
	ret = sceKernelWaitSema(process->lockSema, 1, NULL);
	if (ret < 0) {
		log.result = ret;
		k_sceEventLogPut(0x2711, 1, 0, &log, sizeof(log));
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4810, "process lock failed. 0x%08x\n", ret);
		sceGUIDReleaseObject(process->pid);
		return ret;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if ((process->status & PROCESS_STATE_MASK) != PROCESS_STATE_CREATED
	    || (process->status & (PROCESS_FLAG_STARTING | PROCESS_FLAG_EXIT))) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		goto failed;
	}
	process->status |= PROCESS_FLAG_STARTING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	debug = 0;
	if (SceSblACMgrForKernel_49509A83(pid) != 0) {
		debugFlags = 0;
		SceSysrootForDriver_73522F65(process->parentPid, &debugFlags);
		if (process->attr & 0x20000) {
			if (debugFlags & PROCESS_DEBUG_WAIT_SUSPENDED) {
				flags |= 0x20000;
				debug = 1;
			}
		} else if (waitDebugger < 0 || (debugFlags & PROCESS_DEBUG_WAIT)) {
			flags |= 0x20000;
			debug = 1;
		}
	}
	thread = sceKernelCreateThread("SceKernelProcessPreload", sceKernelProcessPreloadModule, 0x3f,
					0x2000, 0, 0x80000, NULL);
	ret = thread;
	if (thread < 0)
		goto failed;
	process->preloadThread = thread;
	intr = SceCpuForDriver_D32ACE9E(&process->timeLock);
	process->time = sceKernelGetSystemTimeWide();
	process->timeRunning = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->timeLock, intr);
	ret = sceKernelLoadPreloadingModules(pid, &process->image, flags);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4880, "sceKernelLoadPreloadingModules : failed (res=0x%08x)\n",
				     ret);
		goto delete_thread;
	}
	if (debug) {
		status = process->status;
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4892, "suspend process thread\n");
		ret = ProcessThreadSuspend(process->mainThread, PROCESS_STATE_DEBUG_SUSPENDED);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 4896, "suspend main thread failed 0x%08x\n", ret);
			goto delete_thread;
		}
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		process->status = PROCESS_FLAG_PRELOAD | PROCESS_STATE_DEBUG_SUSPENDED;
		process->prevState = PROCESS_STATE_RUNNING;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		sceKernelSignalSema(process->lockSema, 1);
		if (!(status & PROCESS_FLAG_PRELOAD_SUSPEND)) {
			now = sceKernelGetSystemTimeWide();
			SceSysrootForDriver_571E5B79(process->parentPid, pid, 0, process, now);
			SceSysrootForDriver_F4340469(process->parentPid, pid, now);
		}
		memset(&param, 0, sizeof(param));
		param.process = process;
		param.waitDebugger = 1;
		param.args = args;
		param.argp = argp;
		ret = sceKernelStartThread(thread, sizeof(param), &param);
		if (ret >= 0)
			goto done;
		goto delete_thread;
	}
	if (!(process->status & PROCESS_FLAG_PRELOAD_SUSPEND)) {
		now = sceKernelGetSystemTimeWide();
		SceSysrootForDriver_571E5B79(process->parentPid, pid, 0, process, now);
		if (SceSysrootForDriver_6050A467(process->parentPid) != 0
		    && SceSblACMgrForKernel_49509A83(pid) != 0)
			SceSysrootForDriver_F4340469(process->parentPid, pid, now);
	}
	memset(&param, 0, sizeof(param));
	param.process = process;
	param.args = args;
	param.argp = argp;
	ret = sceKernelStartThread(thread, sizeof(param), &param);
	if (ret < 0)
		goto delete_thread;
	ret = sceKernelWaitThreadEnd(thread, &exitStatus, NULL);
	if (ret < 0) {
		if (ret == (int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID) {
			ret = 0;
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 4956, "sceKernelWaitThreadEnd failed. 0x%08x\n", ret);
			SCE_DBG_ASSERT_LEVEL(0, 0, 4957);
		}
	} else {
		sceKernelDeleteThread(thread);
		ret = exitStatus;
	}
	sceKernelSignalSema(process->lockSema, 1);
done:
	k_sceEventLogPut(0x2711, 1, 0, &log, sizeof(log));
	sceGUIDReleaseObject(process->pid);
	return ret;
delete_thread:
	sceKernelDeleteThread(thread);
failed:
	log.result = ret;
	k_sceEventLogPut(0x2711, 1, 0, &log, sizeof(log));
	sceKernelSignalSema(process->lockSema, 1);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

SceClass *SceProcessmgrForKernel_98AE4BC8(void)
{
	return &g_processClass;
}

/* name unknown: hold off / allow the process requests (all 16 permissions) */
int SceProcessmgrForKernel_8321B827(SceUInt32 *timeout)
{
	return sceKernelWaitSema(g_requestSema, PROCESS_MAX, timeout);
}

int SceProcessmgrForKernel_D5DF895D(void)
{
	return sceKernelSignalSema(g_requestSema, PROCESS_MAX);
}

#define SCE_KERNEL_ERROR_PROCESS_LIMIT		0x80010082	/* name unknown */
#define SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY	0x80022004	/* name unknown */
#define SCE_KERNEL_ERROR_CANNOT_SELF		0x80024001	/* name unknown */
#define SCE_KERNEL_ERROR_SEMA_ZERO		0x80028102	/* name unknown */

static int g_firstCreate;

SceUID sceKernelCreateProcess(const char *name, SceUInt32 type, const char *path,
			       const SceKernelProcessOpt *opt)
{
#if FW_DEBUG
	const char budgetName[4][8] = { "KERNEL", "GAME", "MINIAPP", "SYS" };
	const char *typeName;
#endif
	SceProcessObject *process;
	ProcessBudget *budget;
	const void *klicensee;
	SceUInt32 attr, cpuAffinity, kind, limit0, limit1, resolutionFlags, powerConfigFlags;
	SceSize stackSize, uniqueHeapSize;
	SceInt32 priority;
	SceUID parentPid, prevPid, budgetId;
	int ret;

	parentPid = sceKernelGetProcessId();
	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (path == NULL || name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (g_processCount > 15)
		return SCE_KERNEL_ERROR_PROCESS_LIMIT;
	attr = type & 0x1f000000;
	if (attr != PROCESS_TYPE_MINI_APP && attr != PROCESS_TYPE_SHELL && attr != PROCESS_TYPE_GAME)
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
	if (opt == NULL) {
		powerConfigFlags = 3;
		limit1 = 10;
		priority = 0x10000100;
		budgetId = 2;
		limit0 = 0x40;
		stackSize = 0x40000;
		attr = 0;
		resolutionFlags = 0;
		kind = 0;
		cpuAffinity = 0;
		prevPid = 0;
		uniqueHeapSize = 0;
		klicensee = NULL;
	} else {
		if (opt->size != sizeof(*opt))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
		attr = opt->flags;
		priority = opt->priority;
		stackSize = opt->stackSize;
		budgetId = (attr & 0x4000) ? opt->budgetId : 2;
		cpuAffinity = 0;
		if ((attr & 0x1000) && (cpuAffinity = opt->cpuAffinity) != 0) {
			if (cpuAffinity & 0xfff0fff0)
				return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY;
			if (cpuAffinity & 0xf0000) {
				if (cpuAffinity & 0xf)
					return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY;
				cpuAffinity = (SceInt32)cpuAffinity >> 16;
			}
		}
		if (attr & 0x10000)
			parentPid = opt->parentPid;
		if (attr & 0x20000) {
			kind = opt->kind;
			if (kind > 1)
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			prevPid = opt->prevPid;
		} else {
			kind = 0;
			prevPid = 0;
		}
		klicensee = (attr & 0x40000) ? opt->klicensee : NULL;
		limit0 = (attr & 0x80000) ? opt->limit0 : 0x40;
		limit1 = (attr & 0x100000) ? opt->limit1 : 10;
		if (attr & 0x200000) {
			uniqueHeapSize = opt->uniqueHeapSize;
			if (uniqueHeapSize == 0)
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		} else {
			uniqueHeapSize = 0;
		}
		if (attr & 0x800000) {
			resolutionFlags = opt->resolutionFlags;
			if (resolutionFlags & ~3)
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		} else {
			resolutionFlags = 0;
		}
		if (attr & 0x1000000) {
			powerConfigFlags = opt->powerConfigFlags;
			if (powerConfigFlags & ~3)
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		} else {
			powerConfigFlags = 3;
		}
	}
	if (g_firstCreate != 0) {
		sceKernelSysrootSetStatus(0x100);
		SceSysrootForDriver_93CD44CD(8);
		g_firstCreate = 0;
	}
	ret = BudgetIDtoBudgetObj(budgetId, &budget);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4491, "BudgetIDtoBudgetObj(0x%08x) failed. 0x%08x\n",
				     budgetId, ret);
		return ret;
	}
	ret = BudgetLock(budget);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4497, "Lock budget mutex failed. 0x%08x\n", ret);
		return ret;
	}
#if FW_DEBUG
	switch (budget->type) {
	case PROCESS_TYPE_MINI_APP:
		typeName = budgetName[2];
		break;
	case PROCESS_TYPE_GAME:
		typeName = budgetName[1];
		break;
	case PROCESS_TYPE_SHELL:
		typeName = budgetName[3];
		break;
	case PROCESS_TYPE_KERNEL:
		typeName = budgetName[0];
		break;
	default:
		BudgetUnlock(budget);
		SCE_DBG_ASSERT_FAIL(4526);
		typeName = NULL;
		break;
	}
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 4530, "Budget type: [%s] (budget id: 0x%08x)\n", typeName, budgetId);
#endif
	if (budget->processCount >= PROCESS_MAX) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4535, "ERROR: Budget has %d processes\n",
				     budget->processCount);
		BudgetUnlock(budget);
		return SCE_KERNEL_ERROR_PROCESSMGR_TOO_MANY_PROCESSES;
	}
	if ((type ^ budget->type) & 0x1f000000) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4543, "budget type is different with process type\n");
		BudgetUnlock(budget);
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
	}
	if (cpuAffinity != 0 && (cpuAffinity &= 0xf) == 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4552, "cpu affinity is invalid. 0x%x\n", 0);
		BudgetUnlock(budget);
		return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY;
	}
	ret = sceKernelWaitSema(g_requestSema, 1, NULL);
	if (ret < 0) {
		BudgetUnlock(budget);
		return ret;
	}
	ret = _CreateProcess(name, type, path, attr, priority, stackSize, cpuAffinity, parentPid, kind,
			     prevPid, klicensee, limit0, limit1, uniqueHeapSize, budget, resolutionFlags, powerConfigFlags,
			     &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 4584, "_CreateProcess(%s) failed. 0x%08x\n", name, ret);
		sceKernelSignalSema(g_requestSema, 1);
		BudgetUnlock(budget);
		return ret;
	}
	sceKernelSignalSema(g_requestSema, 1);
	BudgetUnlock(budget);
	return process->pid;
}

/* name unknown: start a process (no load flags) */
int SceProcessmgrForKernel_5137B783(SceUID pid, int waitDebugger, SceSize args, void *argp)
{
	int ret;

	sceKernelWaitSema(g_requestSema, 1, NULL);
	SceKernelSuspendForDriver_4DF40893(0);
	ret = StartProcessCommon(pid, waitDebugger, args, argp, 0);
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelSignalSema(g_requestSema, 1);
	return ret;
}

int sceKernelStartProcessExt(SceUID pid, int waitDebugger, SceSize args, void *argp, SceUInt32 flags)
{
	int ret;

	SCE_DBG_ASSERT_LEVEL(0, (flags & ~3) == 0, 5005);
	sceKernelWaitSema(g_requestSema, 1, NULL);
	SceKernelSuspendForDriver_4DF40893(0);
	ret = StartProcessCommon(pid, waitDebugger, args, argp, flags);
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelSignalSema(g_requestSema, 1);
	return ret;
}

int sceKernelExitProcess(int status)
{
	SceProcessObject *process;
	int intr, ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0) {
		SCE_DBG_ASSERT_FAIL(5024);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	process = GetCurrentProcess();
	ret = SceSysmemForDriver_0F5C84B7(process->pid, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5032, "sceGUIDReferObject failed 0x%08x\n", ret);
		SCE_PANIC(5033);
	}
	if (process->type != PROCESS_TYPE_KERNEL) {
		for (;;) {
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			if (process->status & PROCESS_FLAG_EXIT) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 5047,
					"process termination has been already requested. pid 0x%08x\n",
					process->pid);
				break;
			}
			ret = ProcessSendRequest(process, PROCESS_REQUEST_EXIT);
			if (ret == 0) {
				process->exitStatus = status;
				process->status |= PROCESS_FLAG_EXIT;
				process->exitThread = sceKernelGetThreadId();
				break;
			}
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 5058, "send exit event failed 0x%08x\n", ret);
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
				sceKernelDelayThread(1000000);
			}
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
		sceGUIDReleaseObject(process->pid);
		sceKernelExitThread(status);
	}
	sceGUIDReleaseObject(process->pid);
	SCE_PANIC(5039);
}

int sceKernelKillProcess(SceUID pid, int kind)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 5304, "pid 0x%08x, option %d\n", pid, kind);
	if (SceIntrmgrForDriver_182EE3E3() == 0 && SceExcpmgrForKernel_B615A7DA() == 0)
		return KillProcessCommon(pid, kind);
	return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

/* name unknown */
static SceUID SpawnProcessCommon(const char *name, SceUInt32 type, const char *path, SceSize args,
				 void *argp, const SceKernelProcessOpt *opt, SceUInt32 flags)
{
	SceUID pid;
	int ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	pid = sceKernelCreateProcess(name, type & 0x7fffffff, path, opt);
	if (pid >= 0) {
		ret = sceKernelStartProcessExt(pid, type & 0x80000000, args, argp, flags);
		if (ret < 0) {
			sceKernelKillProcess(pid, 0);
			return ret;
		}
	}
	return pid;
}

/* name unknown: spawn a process (no load flags) */
SceUID SceProcessmgrForKernel_1E5B38E5(const char *name, SceUInt32 type, const char *path,
				       SceSize args, void *argp, const SceKernelProcessOpt *opt)
{
	return SpawnProcessCommon(name, type, path, args, argp, opt, 0);
}

SceUID sceKernelSpawnProcessExt(const char *name, SceUInt32 type, const char *path, SceSize args,
				void *argp, const SceKernelProcessOpt *opt, SceUInt32 flags)
{
	SCE_DBG_ASSERT_LEVEL(0, (flags & ~3) == 0, 5134);
	return SpawnProcessCommon(name, type, path, args, argp, opt, flags);
}

int sceKernelKillProcessForDebugger(SceUID pid, int kind)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 5316, "pid 0x%08x, option %d\n", pid, kind);
	return KillProcessCommon(pid, kind);
}

int sceKernelProcessAssocApp(SceUID pid, SceUID app)
{
	SceProcessObject *process;
	int ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5403, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	process->app = app;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int sceKernelGetProcessType(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5540, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	ret = process->type;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int sceKernelChangeProcessCpuAffinity(SceUID pid, SceUInt32 cpuAffinity)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5555, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	if (cpuAffinity & 0xfff0fff0) {
		sceGUIDReleaseObject(process->pid);
		SCE_DBG_ASSERT_FAIL(5563);
		return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY;
	}
	if (cpuAffinity & 0xf0000) {
		if (cpuAffinity & 0xf) {
			sceGUIDReleaseObject(process->pid);
			SCE_DBG_ASSERT_FAIL(5570);
			return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY;
		}
		cpuAffinity >>= 16;
	}
	process->cpuAffinity = cpuAffinity;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int sceKernelSetProcessStatus(SceUID pid, SceUInt32 status)
{
	SceProcessObject *process;
	int intr, ret;

	if (!(status & 0x10))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5598, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	process->status = (status & PROCESS_STATE_MASK) | (process->status & ~PROCESS_STATE_MASK);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int sceKernelGetProcessStatus(SceUID pid, SceUInt32 *status)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5617, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	*status = process->status & PROCESS_STATE_MASK;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int sceKernelIsProcessSuspending(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5641, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	ret = (process->status & PROCESS_FLAG_SUSPENDING) != 0;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int sceKernelGetProcessInfo(SceUID pid, SceKernelProcessInfo *info)
{
	SceProcessObject *process;
	const char *name;
	SceUID modid;
	int ret;

	if (info == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5664, "pInfo is SCE_NULL\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (info->size != sizeof(*info)) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5669, "pInfo->size 0x%08x != 0x%08x(Expect)\n", info->size,
				     sizeof(*info));
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5675, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	pid = process->pid;
	name = SceSysmemForDriver_E655852F(pid);
	info->pid = process->pid;
	info->addressSpace = process->addressSpace;
	info->attr = process->attr;
	info->parentPid = process->parentPid;
	info->budgetId = process->budgetId;
	info->cpuAffinity = process->cpuAffinity;
	info->status = process->status & ~0xf0000;
	info->param = process->infoParam;
	sceKernelGetLibraryDBFlags(pid, &info->libraryDBFlags);
	modid = sceKernelGetModuleIdByPid(pid);
	if (modid < 0) {
		info->mainModule = -1;
		info->entryPoint = (void *)-1;
	} else {
		info->mainModule = modid;
		info->entryPoint = SceModulemgrForKernel_7B302C5D(modid);
	}
	info->type = process->type;
	if (strnlen(process->image.processName, sizeof(process->image.processName)) != 0)
		strncpy(info->name, process->image.processName, sizeof(info->name));
	else if (name != NULL)
		strncpy(info->name, name, sizeof(info->name));
	else
		info->name[0] = '\0';
	info->entryHeap = process->entryHeap;
	info->abortThread = process->abortThread;
	info->abortCode = process->abortCode;
	info->objectHeap[0] = process->objectHeap[0];
	info->objectHeap[1] = process->objectHeap[1];
	memcpy(info->localStorage, process->localStorage, sizeof(info->localStorage));
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: list the processes (kind 0: info without the kernel, 1: info, 2: pids) */
static int ProcessGetList(int level, void *buf, SceSize size, SceSize *count, int kind)
{
	SceKernelProcessInfo *info;
	ListNode *node;
	SceUID *pids, pid;
	SceSize max, n, i;
	int intr, ret;

	if (count == NULL || kind < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (kind < 2)
		max = size / sizeof(*info);
	else if (kind == 2)
		max = size / sizeof(*pids);
	else
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	i = 0;
	n = 0;
	info = buf;
	pids = buf;
	intr = SceCpuForDriver_D32ACE9E(&g_processListLock);
	for (node = g_processList.next; node != &g_processList; node = node->next) {
		pid = PROCESS_FROM_NODE(node)->pid;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
		if (SceSysmemForDriver_00ED6C14(pid, &g_processClass, NULL) >= 0) {
			if (SceSysmemForKernel_4E923101(pid) <= level && i < max) {
				if (kind == 0) {
					if (PROCESS_FROM_NODE(node)->pid == KERNEL_PID)
						goto release;
					goto info;
				} else if (kind == 1) {
info:
					info->size = sizeof(*info);
					ret = sceKernelGetProcessInfo(pid, info);
					if (ret < 0) {
						sceGUIDReleaseObject(pid);
						*count = 0;
						return ret;
					}
					info++;
				} else if (kind == 2) {
					pids[n] = pid;
				}
				n++;
			}
release:
			sceGUIDReleaseObject(pid);
		}
		i++;
		intr = SceCpuForDriver_D32ACE9E(&g_processListLock);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
	*count = n;
	return i;
}

/* names unknown: the process lists */
int SceProcessmgrForDriver_E8005AFC(int level, void *buf, SceSize size, SceSize *count)
{
	return ProcessGetList(level, buf, size, count, 0);
}

int SceProcessmgrForDriver_BA0A06A4(int level, void *buf, SceSize size, SceSize *count)
{
	return ProcessGetList(level, buf, size, count, 1);
}

int SceProcessmgrForKernel_EFFF2975(int level, void *buf, SceSize size, SceSize *count)
{
	return ProcessGetList(level, buf, size, count, 2);
}

SceUID sceKernelGetProcessParentId(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5858, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	ret = process->parentPid;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

SceUID sceKernelGetProcessApp(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5874, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	ret = process->app;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

SceUID sceKernelGetProcessMainThreadForDebugger(SceUID pid)
{
	SceProcessObject *process = NULL;
	int ret;

	ret = UIDtoProcess(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 5892, "UIDtoProcess(pid 0x%08X) failed 0x%08x\n", pid, ret);
		return ret;
	}
	return process->mainThread;
}

int sceKernelSuspendProcess(SceUID pid, SceUInt32 flags, SceUInt32 *timeout)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 6069, "(pid 0x%08x, option 0x%08x)\n", pid, flags);
	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	return SuspendProcessCommon(pid, flags, timeout);
}

int sceKernelSuspendProcessForDebugger(SceUID pid)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 6081, "(pid 0x%08x)\n", pid);
	return SuspendProcessCommon(pid, 0, NULL);
}

/* name unknown: is any process debug-suspended */
int SceProcessmgrForDriver_C715591F(void)
{
	ListNode *node;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&g_processListLock);
	for (node = g_processList.next; node != &g_processList; node = node->next) {
		if (PROCESS_FROM_NODE(node)->status & 0x10) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
			return 1;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_processListLock, intr);
	return 0;
}

int sceKernelResumeProcess(SceUID pid)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 6371, "(pid 0x%08x)\n", pid);
	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	return ResumeProcessCommon(pid);
}

int sceKernelResumeProcessForDebugger(SceUID pid)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 6384, "(pid 0x%08x)\n", pid);
	return ResumeProcessCommon(pid);
}

/* name unknown: leave a debugger suspend, back to the state before it */
static inline __attribute__((always_inline)) int DebugResumeThreads(SceProcessObject *process,
								     SceUInt32 state,
								     SceUInt32 *cursor)
{
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	process->status = (process->status & ~PROCESS_STATE_MASK) | state;
	process->status |= PROCESS_FLAG_RESUMING;
	process->prevState = process->status & PROCESS_STATE_MASK;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	return ProcessAllThreadAction(process, ProcessThreadResume, cursor);
}

static inline __attribute__((always_inline)) void ClearResuming(SceProcessObject *process)
{
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	process->status &= ~PROCESS_FLAG_RESUMING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
}

int sceKernelProcessDebugResume(SceUID pid, SceUInt32 reserved)
{
	SceProcessObject *process = NULL;
	int intr, ret;

	if (SceExcpmgrForKernel_B615A7DA() > 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	ret = UIDtoProcess(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6406, "UIDtoProcess(pid 0x%08X) failed 0x%08x\n", pid, ret);
		return ret;
	}
	ret = sceKernelPollSema(process->lockSema, 1);
	if (ret == 0) {
		if (!(process->status & 0x10)) {
			sceKernelSignalSema(process->lockSema, 1);
			return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		}
		if (process->prevState == PROCESS_STATE_RUNNING) {
			if (process->status & PROCESS_FLAG_PRELOAD) {
				/* not started yet: let the preload thread go on */
				sceKernelSignalSema(process->lockSema, 1);
				intr = SceCpuForDriver_D32ACE9E(&process->lock);
				if ((process->status & PROCESS_STATE_MASK) == PROCESS_STATE_DEBUG_SUSPENDED
				    && !(process->status & (PROCESS_FLAG_FORCE_SUSPEND | PROCESS_FLAG_EXIT))) {
					sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
					return sceKernelSetEventFlag(process->eventFlag,
								      PROCESS_EVF_DEBUG_RESUME);
				}
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
				return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
			}
			intr = SceCpuForDriver_D32ACE9E(&process->lock);
			if (process->status & (PROCESS_FLAG_FORCE_KILL | PROCESS_FLAG_FORCE_SUSPEND)) {
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
				sceKernelSignalSema(process->lockSema, 1);
				return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
			}
			process->status = (process->status & ~PROCESS_STATE_MASK) | PROCESS_STATE_RUNNING;
			process->status |= PROCESS_FLAG_RESUMING;
			process->prevState = process->status & PROCESS_STATE_MASK;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
			ret = ProcessAllThreadAction(process, ProcessThreadResume,
						     &process->threadActionCursor);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 6440, "ProcessAllThreadAction failed 0x%08x\n", ret);
				sceKernelSignalSema(process->lockSema, 1);
				return ret;
			}
			ClearResuming(process);
			ret = sceKernelSetEventFlag(process->eventFlag, PROCESS_EVF_NOT_DEBUG_SUSPENDED);
			if (ret < 0) {
				sceKernelSignalSema(process->lockSema, 1);
				return ret;
			}
		} else if (process->prevState == PROCESS_STATE_SUSPENDED) {
			ret = DebugResumeThreads(process, PROCESS_STATE_SUSPENDED, NULL);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 6494, "ProcessAllThreadAction failed 0x%08x\n", ret);
				sceKernelSignalSema(process->lockSema, 1);
				return ret;
			}
			ClearResuming(process);
		} else {
			sceKernelSignalSema(process->lockSema, 1);
			return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		}
		sceKernelSignalSema(process->lockSema, 1);
	} else if (ret == (int)SCE_KERNEL_ERROR_SEMA_ZERO) {
		/* the process lock is held: the event thread works on it */
		if (!(process->status & 0x10))
			return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		if (process->prevState != PROCESS_STATE_RUNNING) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6519, "invalid process status: prev_status 0x%08x\n",
					     process->prevState);
			return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		}
		if (process->status & PROCESS_FLAG_PRELOAD) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6524,
					     "invalid process status: process havn't started yet\n");
			return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		}
		ret = DebugResumeThreads(process, PROCESS_STATE_RUNNING, &process->threadActionCursor);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 6538, "ProcessAllThreadAction failed 0x%08x\n", ret);
			return ret;
		}
		ClearResuming(process);
	} else if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6546, "process lock failed. 0x%08x\n", ret);
		return ret;
	}
	if (process->pendingRequests != 0)
		sceKernelSetEventFlag(g_eventFlag, 1);
	return 0;
}

int sceKernelWaitProcessEnd(SceUID pid, int *exitStatus, SceUInt32 *timeout)
{
	SceProcessObject *process;
	int ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6573, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	ret = sceKernelWaitEventFlag(process->eventFlag, PROCESS_EVF_DELETED, 0, NULL, timeout);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6583, "sceKernelWaitEventFlag failed. 0x%08x\n", ret);
		sceGUIDReleaseObject(process->pid);
		return ret;
	}
	if (exitStatus != NULL)
		*exitStatus = process->exitStatus;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int sceKernelWaitProcessEventCompletion(SceUID pid, SceUInt32 event, SceUInt32 *timeout)
{
	SceProcessObject *process;
	int ret;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (event != PROCESS_EVF_STARTED && event != PROCESS_EVF_SUSPENDED
	    && event != PROCESS_EVF_RESUMED && event != PROCESS_EVF_DELETED)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6618, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	ret = sceKernelWaitEventFlag(process->eventFlag, event, 1, NULL, timeout);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6628, "sceKernelWaitEventFlag failed. 0x%08x\n", ret);
		sceGUIDReleaseObject(process->pid);
		return ret;
	}
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int SceProcessmgrForDriver_C5CF15ED(SceUInt32 key, ProcessLocalStorageInfo *info)
{
	if (key >= PROCESS_LOCAL_STORAGE_MAX || info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	info->name = g_localStorageName[key];
	info->storageSize = g_localStorageSize[key];
	return 0;
}

void *sceKernelGetProcessLibdb(SceUID pid)
{
	SceProcessObject *process;
	void *libdb;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6957, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return NULL;
	}
	libdb = process->libdb;
	sceGUIDReleaseObject(process->pid);
	return libdb;
}

int sceKernelSetProcessLibdb(SceUID pid, void *libdb)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 6975, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	process->libdb = libdb;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: allocate in the unique heap of the current process */
void *sceKernelAllocOwnProcessHeap(SceSize size, void *opt)
{
	SceProcessObject *process = GetCurrentProcess();

	return SceSysmemForDriver_B415B5A8(process->uniqueHeap, size, opt,
					   __builtin_return_address(0));
}

#define SCE_KERNEL_ERROR_NO_HEAP		0x80020004	/* name unknown */
#define SCE_KERNEL_ERROR_NO_CDIALOG_HEAP	0x8001000e	/* name unknown */
#define SCE_KERNEL_ERROR_MODULE_COUNT_OVER	0x8002450d	/* name unknown */
#define SCE_KERNEL_ERROR_MODULE_COUNT_UNDER	0x8002000f	/* name unknown */

void *sceKernelAllocRemoteProcessHeap(SceUID pid, SceSize size, void *opt)
{
	SceProcessObject *process;
	void *ptr;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7026, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7027);
		return NULL;
	}
	ptr = SceSysmemForDriver_B415B5A8(process->uniqueHeap, size, opt,
					  __builtin_return_address(0));
	sceGUIDReleaseObject(process->pid);
	return ptr;
}

/* names unknown: the CDialog heap of the current process */
void *SceProcessmgrForDriver_3B33FFBA(SceSize size, void *opt)
{
	SceProcessObject *process = GetCurrentProcess();

	if (process->cdialogHeap == -1)
		return NULL;
	return SceSysmemForDriver_B415B5A8(process->cdialogHeap, size, opt,
					   __builtin_return_address(0));
}

int SceProcessmgrForDriver_11BEFBBE(void *ptr)
{
	SceProcessObject *process = GetCurrentProcess();

	if (process->cdialogHeap == -1)
		return SCE_KERNEL_ERROR_NO_CDIALOG_HEAP;
	return sceKernelFreeHeapMemory(process->cdialogHeap, ptr);
}

static inline int ProcessIsKernelLike(SceUID pid)
{
	return pid == SHELL_DUMMY_PID || (pid & ~0x10) == KERNEL_PID;
}

void *sceKernelAllocRemoteCDialogHeap(SceUID pid, SceSize size, void *opt)
{
	SceProcessObject *process;
	void *ptr;
	int ret;

	if (ProcessIsKernelLike(pid))
		return SceSysmemForDriver_B415B5A8(KERNEL_HEAP, size, opt,
						   __builtin_return_address(0));
	ptr = NULL;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7117, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7118);
		return ptr;
	}
	if (process->cdialogHeap == -1) {
		sceGUIDReleaseObject(process->pid);
		return ptr;
	}
	ptr = SceSysmemForDriver_B415B5A8(process->cdialogHeap, size, opt,
					  __builtin_return_address(0));
	sceGUIDReleaseObject(process->pid);
	return ptr;
}

int sceKernelFreeRemoteCDialogHeap(SceUID pid, void *ptr)
{
	SceProcessObject *process;
	int ret;

	if (ProcessIsKernelLike(pid))
		return SceSysmemForKernel_4233C16D(ptr);
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7148, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7149);
		return ret;
	}
	if (process->cdialogHeap == -1) {
		sceGUIDReleaseObject(process->pid);
		return SCE_KERNEL_ERROR_NO_HEAP;
	}
	ret = sceKernelFreeHeapMemory(process->cdialogHeap, ptr);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

void *sceKernelAllocRemoteToolHeap(SceUID pid, SceSize size)
{
	SceProcessObject *process;
	void *ptr;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xd2) == 0)
		return NULL;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7186, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7187);
		return NULL;
	}
	if (process->toolHeap == -1) {
		sceGUIDReleaseObject(process->pid);
		return NULL;
	}
	ptr = SceSysmemForDriver_B415B5A8(process->toolHeap, size, NULL,
					  __builtin_return_address(0));
	sceGUIDReleaseObject(process->pid);
	return ptr;
}

int sceKernelFreeRemoteToolHeap(SceUID pid, void *ptr)
{
	SceProcessObject *process;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xd2) == 0)
		return SCE_KERNEL_ERROR_NO_HEAP;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7215, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		SCE_DBG_ASSERT_FAIL(7216);
		return ret;
	}
	if (process->toolHeap == -1) {
		sceGUIDReleaseObject(process->pid);
		return SCE_KERNEL_ERROR_NO_HEAP;
	}
	ret = sceKernelFreeHeapMemory(process->toolHeap, ptr);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int sceKernelSetPMUSERENR(SceUID pid, SceUInt32 value)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7269,
				     "UIDtoProcessAndRefer(processId 0x%08x) failed 0x%08x\n", pid, ret);
		return ret;
	}
	ret = sceKernelCopyToUserProc(process->pid, process->libkernelAddr[5], &value, sizeof(value));
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7275, "sceKernelSetPMUSERENR failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(7276);
	} else {
		ret = 0;
	}
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int sceKernelSetSyscallTraceFlag(SceUID pid, SceUInt32 flag)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7293,
				     "UIDtoProcessAndRefer(processId 0x%08x) failed 0x%08x\n", pid, ret);
		return ret;
	}
	process->syscallTraceFlag = flag;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: the system call trace flag of the current process */
int SceProcessmgrForDriver_54D469E2(SceUInt32 *flag)
{
	ThreadCB *cb = getCurrentThreadCB();
	SceProcessObject *process;

	if (cb != NULL) {
		*flag = cb->process->syscallTraceFlag;
		return 0;
	}
	process = SceSysmemForKernel_FA6C098C(KERNEL_PID);
	*flag = process->syscallTraceFlag;
	return 0;
}

int sceKernelGetProcessResourceLimit(SceUID pid, SceKernelProcessResourceLimit *limit)
{
	SceProcessObject *process;
	int ret;

	if (limit->size != sizeof(*limit))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 7332, "UIDtoProcessAndRefer(pid 0x%08x) failed 0x%08x\n",
				     pid, ret);
		return ret;
	}
	limit->limit0 = process->limit[0];
	limit->limit1 = process->limit[1];
	limit->budgetType = process->budget->type;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: tell sysroot about an ExitSpawn */
void SceProcessmgrForKernel_344DDE0B(SceUID pid, SceUID prevPid)
{
	SceUInt64 now = sceKernelGetSystemTimeWide();
	SceUID spawnPid;

	spawnPid = g_exitSpawn.process == NULL ? 0 : g_exitSpawn.process->pid;
	SceSysrootForDriver_51F9C118(pid, spawnPid, prevPid, now);
	SceSysrootForDriver_582616EC(pid, spawnPid, prevPid, now);
}

/* names unknown: the hardware breakpoint/watchpoint defaults for new processes */
int SceProcessmgrForKernel_1197DB8A(SceUInt32 index, SceUInt32 value, SceUInt32 control)
{
	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index >= ARRAY_SIZE(g_defaultPhbp))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	g_defaultPhbp[index].value = value;
	g_defaultPhbp[index].control = control;
	return 0;
}

int SceProcessmgrForKernel_6DEC45C3(SceUInt32 index, SceUInt32 *value, SceUInt32 *control)
{
	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index >= ARRAY_SIZE(g_defaultPhbp))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (value == NULL || control == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	*value = g_defaultPhbp[index].value;
	*control = g_defaultPhbp[index].control;
	return 0;
}

/* name unknown: reload the debug registers on every core running the process */
static inline __attribute__((always_inline)) int ProcessUpdateDebugRegs(SceUID pid)
{
	SceKernelThreadRunStatus info;
	int ret, i;

	if (pid == KERNEL_PID) {
		SceThreadmgrForKernel_8D53CA9B(0);
		SceThreadmgrForKernel_8D53CA9B(1);
		SceThreadmgrForKernel_8D53CA9B(2);
		SceThreadmgrForKernel_8D53CA9B(3);
		return 0;
	}
	info.size = sizeof(info);
	ret = SceThreadmgrForDriver_33E85E9E(&info);
	if (ret < 0)
		return ret;
	for (i = 0; i < 4; i++) {
		if (info.cpuInfo[i].processId == pid)
			SceThreadmgrForKernel_8D53CA9B(i);
	}
	return 0;
}

int sceKernelSetPHBP(SceUID pid, SceUInt32 index, SceUInt32 value, SceUInt32 control)
{
	ProcessHwBreakpoints *phbp;
	SceProcessObject *process;
	int intr, ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index > 4)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	pid = process->pid;
	phbp = process->phbp;
	SCE_DBG_ASSERT_CHECK(phbp != NULL, 7477);
	intr = SceCpuForDriver_D32ACE9E(&phbp->lock);
	phbp->reg[index].value = value;
	phbp->reg[index].control = control;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&phbp->lock, intr);
	ret = ProcessUpdateDebugRegs(pid);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(7496);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int sceKernelGetPHBP(SceUID pid, SceUInt32 index, SceUInt32 *value, SceUInt32 *control)
{
	ProcessHwBreakpoints *phbp;
	SceProcessObject *process;
	int intr, ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index >= 5)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (value == NULL || control == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	phbp = process->phbp;
	SCE_DBG_ASSERT_CHECK(phbp != NULL, 7559);
	intr = SceCpuForDriver_D32ACE9E(&phbp->lock);
	*value = phbp->reg[index].value;
	*control = phbp->reg[index].control;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&phbp->lock, intr);
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* names unknown: the abort handler registers (index 5) */
int SceProcessmgrForKernel_FC80A606(SceUID pid, int index, SceUInt32 value, SceUInt32 control)
{
	SceProcessObject *process;
	ProcessDebugRegPair *regs;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index != 5)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	regs = process->contextBreakpoint;
	regs->value = value;
	regs->control = control;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int SceProcessmgrForKernel_E025F5A9(SceUID pid, int index, SceUInt32 *value, SceUInt32 *control)
{
	SceProcessObject *process;
	ProcessDebugRegPair *regs;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index != 5)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (value == NULL || control == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	regs = process->contextBreakpoint;
	*value = regs->value;
	*control = regs->control;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int SceProcessmgrForKernel_DDE282CF(SceUInt32 index, SceUInt32 value, SceUInt32 control)
{
	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index >= ARRAY_SIZE(g_defaultPhwp))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	g_defaultPhwp[index].value = value;
	g_defaultPhwp[index].control = control;
	return 0;
}

int SceProcessmgrForKernel_9FA66773(SceUInt32 index, SceUInt32 *value, SceUInt32 *control)
{
	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index >= ARRAY_SIZE(g_defaultPhwp))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (value == NULL || control == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	*value = g_defaultPhwp[index].value;
	*control = g_defaultPhwp[index].control;
	return 0;
}

int sceKernelSetPHWP(SceUID pid, SceUInt32 index, SceUInt32 value, SceUInt32 control)
{
	ProcessHwWatchpoints *phwp;
	SceProcessObject *process;
	int intr, ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index > 3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	pid = process->pid;
	phwp = process->phwp;
	SCE_DBG_ASSERT_CHECK(phwp != NULL, 7786);
	intr = SceCpuForDriver_D32ACE9E(&phwp->lock);
	phwp->reg[index].value = value;
	phwp->reg[index].control = control;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&phwp->lock, intr);
	ret = ProcessUpdateDebugRegs(pid);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(7811);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int sceKernelGetPHWP(SceUID pid, SceUInt32 index, SceUInt32 *value, SceUInt32 *control)
{
	ProcessHwWatchpoints *phwp;
	SceProcessObject *process;
	int intr, ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index >= 4)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (value == NULL || control == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	phwp = process->phwp;
	SCE_DBG_ASSERT_CHECK(phwp != NULL, 7875);
	intr = SceCpuForDriver_D32ACE9E(&phwp->lock);
	*value = phwp->reg[index].value;
	*control = phwp->reg[index].control;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&phwp->lock, intr);
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int sceKernelEnablePHWP(SceUID pid, SceUInt32 index)
{
	ProcessHwWatchpoints *phwp;
	SceProcessObject *process;
	int intr, ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index > 3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	pid = process->pid;
	phwp = process->phwp;
	SCE_DBG_ASSERT_CHECK(phwp != NULL, 7928);
	intr = SceCpuForDriver_D32ACE9E(&phwp->lock);
	phwp->reg[index].control |= 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&phwp->lock, intr);
	ret = ProcessUpdateDebugRegs(pid);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(7952);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int sceKernelDisablePHWP(SceUID pid, SceUInt32 index)
{
	ProcessHwWatchpoints *phwp;
	SceProcessObject *process;
	int intr, ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	if (index > 3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	pid = process->pid;
	phwp = process->phwp;
	SCE_DBG_ASSERT_CHECK(phwp != NULL, 8010);
	intr = SceCpuForDriver_D32ACE9E(&phwp->lock);
	phwp->reg[index].control &= ~1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&phwp->lock, intr);
	ret = ProcessUpdateDebugRegs(pid);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(8034);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* sceKernelSetInitialPMUSERENR / its getter (names from SceSysmodule's messages) */
int SceProcessmgrForDriver_B1C3EFCA(SceUID pid, SceUInt32 value)
{
	SceProcessObject *process;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	process->defaultPmuserenr = value;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int SceProcessmgrForDriver_1C70501E(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	ret = process->defaultPmuserenr;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* sceKernelSetInitialPMCR / its getter */
int SceProcessmgrForDriver_61B9B6FA(SceUID pid, SceUInt32 value)
{
	SceProcessObject *process;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	process->defaultPmcr = value;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int SceProcessmgrForDriver_594319C7(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	if (SceDipswForDriver_A98FC2FD(0xe4) == 0)
		return SCE_KERNEL_ERROR_PROCESSMGR_NOT_TOOL;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	ret = process->defaultPmcr;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int SceProcessmgrForDriver_C074EB31(SceUID pid, SceUInt64 *time)
{
	SceProcessObject *process;
	int intr, ret;

	if (time == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_D32ACE9E(&process->timeLock);
	if (process->timeRunning == 0)
		*time = process->time;
	else
		*time = sceKernelGetSystemTimeWide() - process->time;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->timeLock, intr);
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int sceKernelGetRemoteProcessTime(SceUID pid, SceUInt64 *time)
{
	SceUInt64 value;
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	ret = SceProcessmgrForDriver_C074EB31(pid, &value);
	if (ret >= 0)
		ret = SceSysmemForDriver_6D88EF8A(time, &value, sizeof(value));
	ProcessSyscallLeave(state);
	return ret;
}

int SceProcessmgrForDriver_EC283166(SceUInt64 *time)
{
	ThreadCB *cb = getCurrentThreadCB();
	SceProcessObject *process;

	if (time == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (cb == NULL)
		process = SceSysmemForKernel_FA6C098C(KERNEL_PID);
	else
		process = cb->process;
	*time = sceKernelGetSystemTimeWide() - process->time;
	return 0;
}

int sceKernelGetProcessTimeCore(SceUInt64 *time)
{
	SceUInt64 value;
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	ret = SceProcessmgrForDriver_EC283166(&value);
	if (ret >= 0)
		ret = SceSysmemForDriver_6D88EF8A(time, &value, sizeof(value));
	ProcessSyscallLeave(state);
	return ret;
}

SceUInt32 SceProcessmgrForDriver_02179E12(void)
{
	ThreadCB *cb = getCurrentThreadCB();
	SceProcessObject *process;

	if (cb != NULL) {
		process = cb->process;
		return SceThreadmgrForDriver_47F6DE49() - (SceUInt32)process->time;
	}
	process = SceSysmemForKernel_FA6C098C(KERNEL_PID);
	return SceThreadmgrForDriver_47F6DE49() - (SceUInt32)process->time;
}

SceUInt32 sceKernelGetProcessTimeLowCore(void)
{
	SceUInt32 state, ret;

	state = ProcessSyscallEnter();
	sceKernelApiDeclareTypeUInt32();
	ret = SceProcessmgrForDriver_02179E12();
	ProcessSyscallLeave(state);
	return ret;
}

SceUInt64 SceProcessmgrForDriver_82D94BE9(void)
{
	ThreadCB *cb = getCurrentThreadCB();
	SceProcessObject *process;

	if (cb == NULL)
		process = SceSysmemForKernel_FA6C098C(KERNEL_PID);
	else
		process = cb->process;
	return sceKernelGetSystemTimeWide() - process->time;
}

SceUInt64 sceKernelGetProcessTimeWideCore(void)
{
	SceUInt32 state;
	SceUInt64 ret;

	state = ProcessSyscallEnter();
	sceKernelApiDeclareTypeUInt64();
	ret = SceProcessmgrForDriver_82D94BE9();
	ProcessSyscallLeave(state);
	return ret;
}

/* name unknown: the start (or run) time of a process */
int SceProcessmgrForDriver_5BDA978F(SceUID pid, SceUInt64 *time)
{
	SceProcessObject *process;
	int ret;

	if (time == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (pid == 0)
		pid = sceKernelGetProcessId();
	ret = SceSysmemForDriver_00ED6C14(pid, &g_processClass, &process);
	if (ret < 0)
		return ret;
	*time = process->time;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: a time stamp relative to the process run time */
int SceProcessmgrForDriver_8CB01675(SceUID pid, SceUInt64 *out, SceUInt64 time)
{
	SceProcessObject *process;
	int intr, ret;

	if (out == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (pid == 0)
		pid = sceKernelGetProcessId();
	ret = SceSysmemForDriver_00ED6C14(pid, &g_processClass, &process);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_D32ACE9E(&process->timeLock);
	if (process->timeRunning == 0)
		*out = time + process->time - sceKernelGetSystemTimeWide();
	else
		*out = time - process->time;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->timeLock, intr);
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* name unknown: the callback for deleted processes */
int SceProcessmgrForDriver_5E882B60(void (*callback)(SceUID pid, int exitStatus))
{
	if (callback == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	g_processDeleteCallback = callback;
	return 0;
}

/* names unknown: count the modules (kind 1: kernel modules) a process loaded */
int SceProcessmgrForKernel_8B4086AF(SceUID pid, int kind)
{
	SceProcessObject *process;
	int intr, ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	if (process->type == PROCESS_TYPE_KERNEL
	    || (process->type == PROCESS_TYPE_SHELL && kind == 1)) {
		sceGUIDReleaseObject(process->pid);
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if (process->type != PROCESS_TYPE_SHELL && kind == 1) {
		if (process->moduleCount[1] >= 0x20) {
			ret = SCE_KERNEL_ERROR_MODULE_COUNT_OVER;
		} else {
			process->moduleCount[1]++;
			ret = 0;
		}
	} else if (process->moduleCount[0] < 0x80) {
		ret = 0;
		process->moduleCount[0]++;
	} else {
		ret = SCE_KERNEL_ERROR_MODULE_COUNT_OVER;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

int SceProcessmgrForKernel_F160B4D8(SceUID pid, int kind)
{
	SceProcessObject *process;
	int intr, ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	if (process->type == PROCESS_TYPE_KERNEL
	    || (process->type == PROCESS_TYPE_SHELL && kind == 1)) {
		sceGUIDReleaseObject(process->pid);
		return SCE_KERNEL_ERROR_PROCESSMGR_INVALID_TYPE;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	if (process->type != PROCESS_TYPE_SHELL && kind == 1) {
		if (process->moduleCount[1] == 0) {
			ret = SCE_KERNEL_ERROR_MODULE_COUNT_UNDER;
		} else {
			process->moduleCount[1]--;
			ret = 0;
		}
	} else if (process->moduleCount[0] == 0) {
		ret = SCE_KERNEL_ERROR_MODULE_COUNT_UNDER;
	} else {
		ret = 0;
		process->moduleCount[0]--;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* the user mode standard I/O and process parameter of the current process */
SceUID sceKernelGetStdin(void)
{
	return GetCurrentProcess()->stdioPuid[0];
}

SceUID sceKernelGetStdout(void)
{
	return GetCurrentProcess()->stdioPuid[1];
}

SceUID sceKernelGetStderr(void)
{
	return GetCurrentProcess()->stdioPuid[2];
}

SceUID sceKernelGetExtraTty(void)
{
	sceKernelApiDeclareTypeUInt32();
	return GetCurrentProcess()->stdioPuid[3];
}

void *sceKernelGetProcessParam(void)
{
	sceKernelApiDeclareTypeUInt32();
	return GetCurrentProcess()->image.processParam;
}

/* name unknown: wait until the debugger lets a process go on */
int SceProcessmgrForKernel_BC110987(SceUID pid, SceUInt32 event)
{
	SceProcessObject *process;
	int intr, ret;

	if (event != PROCESS_EVF_NOT_DEBUG_SUSPENDED)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	ret = sceKernelWaitEventFlag(process->eventFlag, PROCESS_EVF_NOT_DEBUG_SUSPENDED, 1, NULL, NULL);
	if (ret >= 0) {
		intr = SceCpuForDriver_D32ACE9E(&process->lock);
		if (process->status & PROCESS_FLAG_EXIT)
			ret = SCE_KERNEL_ERROR_PROCESSMGR_INVALID_STATUS;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	}
	sceGUIDReleaseObject(process->pid);
	return ret;
}

/* names unknown: the two 16-bit process settings of sceKernelCreateProcess */
int SceProcessmgrForDriver_E1A67C86(SceUID pid, SceUInt32 *value)
{
	SceProcessObject *process;
	int ret;

	if (value == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	*value = process->resolutionFlags;
	sceGUIDReleaseObject(process->pid);
	return 0;
}

int SceProcessmgrForDriver_879153D9(SceUID pid, SceUInt32 kind)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	if ((kind & 0xffff0000) == 0x10000)
		ret = process->powerConfigFlags & 1;
	else if ((kind & 0xffff0000) == 0x20000)
		ret = (process->powerConfigFlags >> 1) & 1;
	else
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	sceGUIDReleaseObject(process->pid);
	return ret;
}

SceUID sceKernelGetCurrentProcess(void)
{
	SceUInt32 state;
	SceUID pid;

	state = ProcessSyscallEnter();
	pid = sceKernelGetProcessId();
	ProcessSyscallLeave(state);
	return pid;
}

void _sceKernelExitProcessForUser(int status)
{
	SceUInt32 state;

	state = ProcessSyscallEnter();
	sceKernelExitProcess(status);
	ProcessSyscallLeave(state);
	SCE_PANIC(8820);
}

int _sceKernelRegisterLibkernelAddressesForUser(const LibkernelAddresses *uaddr)
{
	SceProcessObject *process;
	LibkernelAddresses addr;
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	ret = SceSysmemForDriver_BC996A7A(&addr, uaddr, sizeof(addr));
	if (ret < 0) {
		ProcessSyscallLeave(state);
		return ret;
	}
	if (addr.size != sizeof(addr)) {
		ProcessSyscallLeave(state);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	process = GetCurrentProcess();
	if (process->libkernelAddr[0] == NULL)
		process->libkernelAddr[0] = addr.addr[0];
	if (process->libkernelAddr[1] == NULL)
		process->libkernelAddr[1] = addr.addr[1];
	if (process->libkernelAddr[2] == NULL)
		process->libkernelAddr[2] = addr.addr[2];
	if (process->libkernelAddr[3] == NULL)
		process->libkernelAddr[3] = addr.addr[3];
	if (process->libkernelAddr[4] == NULL && (process->libkernelAddr[4] = addr.addr[4]) != NULL
	    && (ret = SceSysmemForDriver_6D88EF8A(addr.addr[4], &process->time, sizeof(process->time))) < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 8862, "sceKernelCopyToUser failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(8863);
	} else if (process->libkernelAddr[5] == NULL) {
		process->libkernelAddr[5] = addr.addr[5];
	}
	ProcessSyscallLeave(state);
	return 0;
}

int _sceKernelGetTimer5RegForUser(void *uaddr)
{
	SceProcessObject *process;
	SceUInt32 state;
	void *base;
	int ret;

	state = ProcessSyscallEnter();
	process = GetCurrentProcess();
	ret = SceSysmemForKernel_FA1A73DD(process->addressSpace, &base);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 8890, "sceKernelAddressSpaceGetTimerVbase failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(8891);
		ProcessSyscallLeave(state);
		return ret;
	}
	ret = SceSysmemForDriver_6D88EF8A(uaddr, &base, sizeof(base));
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 8898, "sceKernelCopyToUser failed 0x%08x\n", ret);
		ProcessSyscallLeave(state);
		return ret;
	}
	ProcessSyscallLeave(state);
	return 0;
}

int sceKernelPowerLock(int type)
{
	SceProcessObject *process;
	SceUInt32 state;
	int intr, ret;

	state = ProcessSyscallEnter();
	process = GetCurrentProcess();
	ret = sceKernelWaitSema(process->eventSema, 1, NULL);
	if (ret < 0) {
		ProcessSyscallLeave(state);
		return ret;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->powerLock);
	process->powerLockCount++;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->powerLock, intr);
	sceKernelSignalSema(process->eventSema, 1);
	SceKernelSuspendForDriver_254525F8(type);
	ProcessSyscallLeave(state);
	return 0;
}

int sceKernelProcessPowerUnlockForUser(int type)
{
	SceProcessObject *process;
	SceUInt32 state;
	int intr, ret;
#if FW_DEBUG
	char buf[0x200];
	SceSize len;
	SceUID threadId;
#endif

	state = ProcessSyscallEnter();
	process = GetCurrentProcess();
	intr = SceCpuForDriver_D32ACE9E(&process->powerLock);
	if (process->powerLockCount == 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->powerLock, intr);
#if FW_DEBUG
		threadId = sceKernelGetThreadId();
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(8964),
			"*** error: sceKernelPowerUnlock() nest borken 0x%08X [%s]\n",
			threadId, SceSysmemForDriver_E655852F(threadId));
		memset(buf, 0, sizeof(buf));
		if (sceKernelSysrootBacktrace(0, buf, sizeof(buf), &len, 3) >= 0)
			sceKernelSysrootPrintBacktrace(KERNEL_PID, buf, len, 0);
		memset(buf, 0, sizeof(buf));
		if (sceKernelSysrootBacktrace(0, buf, sizeof(buf), &len, 2) >= 0)
			sceKernelSysrootPrintBacktrace(0, buf, len, 0);
#endif
		ProcessSyscallLeave(state);
		return 0;
	}
	process->powerLockCount--;
	SceKernelSuspendForDriver_230495ED(type);
	if (process->powerLockCount == 0 && process->pendingRequests != 0) {
		ret = sceKernelSetEventFlag(g_eventFlag, 1);
		if (ret < 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->powerLock, intr);
			ProcessSyscallLeave(state);
			return ret;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->powerLock, intr);
	ProcessSyscallLeave(state);
	return 0;
}

int sceKernelRegisterProcessTerminationCallback(SceUID pid, SceUID cb)
{
	return ProcessTerminationCallbackCore(pid, cb, 0);
}

int sceKernelUnregisterProcessTerminationCallback(SceUID pid, SceUID cb)
{
	return ProcessTerminationCallbackCore(pid, cb, 1);
}

int sceKernelPowerTick(int type)
{
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	ret = SceKernelSuspendForDriver_E0489831(type);
	ProcessSyscallLeave(state);
	return ret;
}

int sceKernelGetProcessName(char *name, SceSize size)
{
	SceProcessObject *process;
	SceUInt32 state;
	int ret;

	if (name == NULL || size > 0x20)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	state = ProcessSyscallEnter();
	process = GetCurrentProcess();
	ret = SceSysmemForDriver_80BD6FEB(name, SceSysmemForDriver_E655852F(process->pid), size);
	ProcessSyscallLeave(state);
	return ret;
}

int sceKernelGetProcessTitleId(char *titleId, SceSize size)
{
	SceProcessObject *process;
	SceUInt32 state;
	int ret;

	if (titleId == NULL || size > 0x20)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	state = ProcessSyscallEnter();
	process = GetCurrentProcess();
	ret = SceSysmemForDriver_80BD6FEB(titleId, process->TitleId, size);
	ProcessSyscallLeave(state);
	return ret;
}

SceUInt32 sceKernelLibcClock(void)
{
	SceUInt32 state, ret;

	state = ProcessSyscallEnter();
	sceKernelApiDeclareTypeUInt32();
	ret = ProcessmgrLibcClock();
	ProcessSyscallLeave(state);
	return ret;
}

SceKernelTime sceKernelLibcTime(SceKernelTime *ut)
{
	SceKernelTime t;
	SceUInt32 state;

	state = ProcessSyscallEnter();
	sceKernelApiDeclareTypeUInt32();
	t = SceProcessmgrForKernel_B0E076D6(&t);
	if (ut != NULL && SceSysmemForDriver_6D88EF8A(ut, &t, sizeof(t)) < 0)
		t = (SceKernelTime)-1;
	ProcessSyscallLeave(state);
	return t;
}

int sceKernelLibcGettimeofday(ProcessTimeval *utv, ProcessTimezone *utz)
{
	ProcessTimeval tv;
	ProcessTimezone tz;
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	ret = SceProcessmgrForKernel_014F5113(&tv, &tz);
	if (ret == 0 && (utv == NULL || (ret = SceSysmemForDriver_6D88EF8A(utv, &tv, sizeof(tv))) >= 0)
	    && utz != NULL)
		ret = SceSysmemForDriver_6D88EF8A(utz, &tz, sizeof(tz));
	ProcessSyscallLeave(state);
	return ret;
}

int sceKernelLibcGmtime_r(const SceKernelTime *ut, ProcessTm *utm)
{
	const SceKernelTime *pt;
	SceKernelTime t;
	SceUInt32 state;
	ProcessTm tm;
	int ret;

	state = ProcessSyscallEnter();
	pt = NULL;
	if (ut != NULL) {
		ret = SceSysmemForDriver_BC996A7A(&t, ut, sizeof(t));
		pt = &t;
		if (ret < 0)
			goto out;
	}
	ret = ProcessmgrLibcGmtime_r(pt, &tm);
	if (ret >= 0 && utm != NULL)
		ret = SceSysmemForDriver_6D88EF8A(utm, &tm, sizeof(tm));
out:
	ProcessSyscallLeave(state);
	return ret;
}

int sceKernelLibcLocaltime_r(const SceKernelTime *ut, ProcessTm *utm)
{
	const SceKernelTime *pt;
	SceKernelTime t;
	SceUInt32 state;
	ProcessTm tm;
	int ret;

	state = ProcessSyscallEnter();
	pt = NULL;
	if (ut != NULL) {
		ret = SceSysmemForDriver_BC996A7A(&t, ut, sizeof(t));
		pt = &t;
		if (ret < 0)
			goto out;
	}
	ret = ProcessmgrLibcLocaltime_r(pt, &tm);
	if (ret >= 0 && utm != NULL)
		ret = SceSysmemForDriver_6D88EF8A(utm, &tm, sizeof(tm));
out:
	ProcessSyscallLeave(state);
	return ret;
}

int sceKernelLibcMktime(ProcessTm *utm, SceKernelTime *ut, SceUInt64 *uwide)
{
	SceKernelTime t;
	SceUInt64 wide;
	SceUInt32 state;
	ProcessTm tm;
	int ret;

	state = ProcessSyscallEnter();
	if (utm == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SceSysmemForDriver_BC996A7A(&tm, utm, sizeof(tm));
	if (ret >= 0 && (ret = ProcessmgrLibcMktime(&tm, &t, &wide)) >= 0
	    && (ret = SceSysmemForDriver_6D88EF8A(utm, &tm, sizeof(tm))) >= 0
	    && (ut == NULL || (ret = SceSysmemForDriver_6D88EF8A(ut, &t, sizeof(t))) >= 0)
	    && uwide != NULL)
		ret = SceSysmemForDriver_6D88EF8A(uwide, &wide, sizeof(wide));
	ProcessSyscallLeave(state);
	return ret;
}

int sceKernelCallAbortHandlerForUser(SceUInt32 code, const void *opt)
{
	SceProcessObject *process;
	SceUInt32 state;
	SceUID pid, sema;
	int ret;

	state = ProcessSyscallEnter();
	if (opt != NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 9376, "invalid pOpt address. %p\n", opt);
		ProcessSyscallLeave(state);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	process = GetCurrentProcess();
	pid = process->pid;
	if ((SceDipswForDriver_A98FC2FD(0x9f) == 0 || SceSysrootForDriver_6050A467(pid) == 1)
	    && (SceDipswForDriver_A98FC2FD(0x9f) != 0 || SceSblACMgrForKernel_sceSblACMgrIsGameProgram(pid) != 0)) {
		if (SceDipswForDriver_A98FC2FD(0x9f) == 0 || SceSysrootForDriver_6050A467(pid) == 0) {
			if (SceCpuForDriver_CDA96E81((SceInt32 *)&process->abortRequested, 0, 1) == 0) {
				process->abortCode = code;
				process->abortThread = sceKernelGetThreadId();
				ret = sceKernelSetEventFlag(process->abortEventFlag, 1);
				if (ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0, 9427,
						"*** error: sceKernelCallAbortHandlerForUser() -> sceKernelSetEventFlag(0x%08X)=0x%08X\n",
						process->abortEventFlag, ret);
			}
			sema = sceKernelCreateSema("SceKernelCallAbortHandler", 0x8000, 0, 1, NULL);
			if (sema < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 9433,
					"*** error: sceKernelCallAbortHandlerForUser() -> sceKernelCreateSema()=0x%08X\n",
					sema);
				ProcessSyscallLeave(state);
				return sema;
			}
			ret = sceKernelWaitSema(sema, 1, NULL);
			if ((SceUInt32)ret + 0x7ffd7fd4 > 1)
				SCE_DBG_PRINTF_LEVEL(1, 0, 9439,
					"*** error: sceKernelCallAbortHandlerForUser() -> sceKernelWaitSema()=0x%08X\n",
					ret);
			ret = sceKernelDeleteSema(sema);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 9443,
					"*** error: sceKernelCallAbortHandlerForUser() -> sceKernelDeleteSema()=0x%08X\n",
					ret);
		} else if (SceCpuForDriver_CDA96E81((SceInt32 *)&process->abortRequested, 0, 1) == 0) {
			SceSysrootForDriver_5C86E49B(pid, code, sceKernelGetSystemTimeWide());
		}
	}
	ProcessSyscallLeave(state);
	return 0;
}

int sceKernelCDialogSessionClose(void)
{
	SceProcessObject *process;
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	process = GetCurrentProcess();
	ret = SceSysmemForDriver_0F5C84B7(process->pid, NULL);
	if (ret < 0)
		return ret;
	if (process->cdialogHeap == -1) {
		ret = SCE_KERNEL_ERROR_NO_HEAP;
	} else {
		ret = sceKernelShrinkHeap(process->cdialogHeap);
		if (ret >= 0) {
			sceGUIDReleaseObject(process->pid);
			ProcessSyscallLeave(state);
			return 0;
		}
	}
	sceGUIDReleaseObject(process->pid);
	ProcessSyscallLeave(state);
	return ret;
}

#define SCE_KERNEL_ERROR_ILLEGAL_LEASE_LIMIT	0x80024b0b	/* name unknown */

int sceKernelCDialogSetLeaseLimit(SceUInt32 limit)
{
	SceProcessObject *process;
	ProcessResource *resource;
	int intr, ret;

	if (limit > 0x800)
		return SCE_KERNEL_ERROR_ILLEGAL_LEASE_LIMIT;
	process = GetCurrentProcess();
	resource = &process->resource[2];
	intr = SceCpuForDriver_D32ACE9E(&resource->lock);
	if (limit < resource->count) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_LEASE_LIMIT;
	} else {
		resource->limit = limit;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&resource->lock, intr);
	return ret;
}

SceProcessObject *sceKernelUIDtoProcess(SceUID pid)
{
	SceProcessObject *process;
	int ret;

	ret = UIDtoProcess(pid, &process);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 9490, "UIDtoProcess failed 0x%08x\n", ret);
		process = NULL;
	}
	return process;
}

/* the coredump handler of the current process */
#define SCE_KERNEL_ERROR_COREDUMP_NO_HANDLER	0x800a0001	/* name unknown */
#define SCE_KERNEL_ERROR_COREDUMP_HANDLER_SET	0x800a0002	/* name unknown */

int sceKernelRegisterCoredumpHandler(void *handler, SceSize stackSize, void *arg)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceProcessObject *process;
	SceUInt32 state;
	SceUID block;
	int intr, ret;

	process = GetCurrentProcess();
	state = ProcessSyscallEnter();
	ret = SceSysmemForDriver_F4AD89D8(process->pid, 0x50, handler, 4);
	if (ret != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 9979, "error: sceKernelIsEqualAccessibleRangeProcBySW()=0x%08X\n",
				     ret);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto out;
	}
	if (stackSize < 0x1000) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto out;
	}
	if (arg != NULL && (ret = SceSysmemForDriver_9C78064C(0x10, arg, 4)) != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 9996, "error: sceKernelIsAccessibleRange()=0x%08X\n", ret);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto out;
	}
	memset(&opt, 0, sizeof(opt));
	opt.pid = process->pid;
	opt.size = sizeof(opt);
	opt.attr = ALLOC_ATTR_NO_CLEAR | ALLOC_ATTR_NO_MAP | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID |
		   ALLOC_ATTR_HAS_EXTRA_LOW;
	opt.alignment = 0x1000;
	block = SceSysmemForDriver_C94850C9("uStack: SceKernelCoredumpHandle", 0xc20d060, stackSize + 0x1000,
					&opt);
	if (block < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 10014, "error: sceKernelAllocMemBlockForKernel()=0x%08X\n\n",
				     block);
		ret = block;
		goto out;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->coredumpLock);
	if (process->coredumpHandler == NULL) {
		process->coredumpStack = block;
		process->coredumpHandler = handler;
		process->coredumpStackSize = stackSize;
		process->coredumpArg = arg;
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_COREDUMP_HANDLER_SET;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->coredumpLock, intr);
out:
	ProcessSyscallLeave(state);
	return ret;
}

/* name unknown: unregister the coredump handler of the current process */
int SceProcessmgrForDriver_33B3D026(void)
{
	SceProcessObject *process;
	SceUInt32 state;
	SceUID block;
	int intr, ret;

	process = GetCurrentProcess();
	state = ProcessSyscallEnter();
	if (getCurrentThreadCB()->currentAttr & THREAD_ATTR_COREDUMP_HANDLER) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else {
		intr = SceCpuForDriver_D32ACE9E(&process->coredumpLock);
		if (process->coredumpHandler == NULL) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->coredumpLock, intr);
			ret = SCE_KERNEL_ERROR_COREDUMP_NO_HANDLER;
		} else {
			block = process->coredumpStack;
			process->coredumpHandler = NULL;
			process->coredumpStackSize = 0;
			process->coredumpArg = NULL;
			if (block < 1) {
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->coredumpLock, intr);
				ret = 0;
			} else {
				process->coredumpStack = -1;
				sceKernelSpinlockLowUnlockCpuResumeIntr(&process->coredumpLock, intr);
				ret = SceSysmemForDriver_009E1C61(block);
			}
		}
	}
	ProcessSyscallLeave(state);
	return ret;
}

/* the start argument of the coredump handler thread */
typedef struct ProcessCoredumpParam {
	void *handler;
	void *arg;
} ProcessCoredumpParam;

SceUID sceKernelCallCoredumpHandler(SceUID pid)
{
	SceKernelThreadOptParam opt;
	ProcessCoredumpParam param;
	SceProcessObject *process;
	void *thread;
	SceUInt32 state;
	SceUID thid;
	int ret;

	state = ProcessSyscallEnter();
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		goto out;
	pid = process->pid;
	if (process->coredumpHandler == NULL) {
		ret = SCE_KERNEL_ERROR_COREDUMP_NO_HANDLER;
		goto release;
	}
	opt.uStackMemid = process->coredumpStack;
	opt.kStackMemType = 0;
	opt.uStackMemType = 0;
	opt.uTLSMemType = 0;
	opt.handedExceptions = 0;
	opt.size = sizeof(opt);
	opt.attr = 8;			/* uStackMemid is given */
	thid = SceThreadmgrForDriver_C8E57BB4(pid, "SceKernelCoredumpHandler", process->libkernelAddr[3],
					      0x40, process->coredumpStackSize, 0x800000, 0, &opt);
	if (thid < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 10146,
			"error: sceKernelCreateRemoteThread(0x%08X, %s, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, %p)=0x%08X\n",
			pid, "SceKernelCoredumpHandler", process->libkernelAddr[3], 0x40,
			process->coredumpStackSize, 0, 0, &opt, thid);
		ret = thid;
		goto release;
	}
	ret = SceSysmemForKernel_0E43E113(thid, g_threadClass, &thread);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(10151);
		sceKernelDeleteThread(ret);
		goto release;
	}
	param.handler = process->coredumpHandler;
	process->coredumpStack = -1;
	param.arg = process->coredumpArg;
	ret = sceKernelStartThread(thid, sizeof(param), &param);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 10166, "error: sceKernelStartThread(0x%08X, 0x%08X, %p)=0x%08X\n",
				     thid, sizeof(param), &param, ret);
		sceKernelDeleteThread(ret);
		goto release;
	}
	ret = thid;
release:
	sceGUIDReleaseObject(process->pid);
out:
	ProcessSyscallLeave(state);
	return ret;
}

/* name unknown: has the process a coredump handler */
int SceProcessmgrForDriver_A1CF74FA(SceUID pid)
{
	SceProcessObject *process;
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret >= 0) {
		if (process->coredumpHandler != NULL)
			ret = 1;
		sceGUIDReleaseObject(process->pid);
	}
	ProcessSyscallLeave(state);
	return ret;
}

/* name unknown: random numbers from the process's random state */
int SceProcessmgrForDriver_2CEB1C7A(SceUID pid, SceUInt32 *buf, SceSize size)
{
	SceProcessObject *process;
	SceUInt32 *p;
	int intr, ret;

	if (((SceUIntPtr)buf | size) & 3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = UIDtoProcessAndRefer(pid, &process);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_D32ACE9E(&process->randomLock);
	for (p = buf; size != 0 && (SceSize)((char *)p - (char *)buf) < size; p++)
		*p = SceKernelUtilsForDriver_335AF34D(process->random);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->randomLock, intr);
	sceGUIDReleaseObject(process->pid);
	return 0;
}

/* the process manager functions sysroot calls (SceSysrootForKernel_733C243E) */
static const SysrootProcessmgrOps g_sysrootOps2 = {
	0x20,
	GetUIDEntryHeap,
	SceProcessmgrForKernel_9BC44974,
	PIDtoAddressSpaceCB,
	sceKernelGetProcessSelfAuthInfo,
	ProcessGetName,
	IncProcessResource,
	DecProcessResource,
};

/* the second half of the module start, called by sysroot once threads run */
static int ProcessmgrModuleStart2(void)
{
	int ret;

	SceSysrootForKernel_733C243E(&g_sysrootOps2);
	ret = sceKernelCreateEventFlag("SceKernelProcessmgrProcEvent", 0x1000, 0, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9751);
		return ret;
	}
	g_eventFlag = ret;
	ret = sceKernelCreateSema("SceKernelProcRequestPermission", 0x8000, PROCESS_MAX, PROCESS_MAX,
				   NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9762);
		return ret;
	}
	ListInit(&g_requestList);
	g_requestLock = 0;
	g_requestCount = 0;
	g_requestSema = ret;
	ret = sceKernelRegisterIntrHandler(6, "SceKernelProcessmgr", 0, sceKernelProcessmgrSgiHandler,
					    NULL, 0x50, 0xf, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9782);
		return ret;
	}
	ret = sceKernelCreateThread("SceKernelProcessEvent", sceKernelProcessEventThread, 0x3f, 0x2000,
				     0, 0x80000, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9796);
		return ret;
	}
	g_eventThread = ret;
	ret = sceKernelStartThread(ret, 0, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9803);
		return ret;
	}
	ListInit(&g_recoveryList);
	g_recoveryLock = 0;
	ret = sceKernelCreateEventFlag("SceKernelProcResourceRecovery", 0x1000, 0, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9817);
		return ret;
	}
	g_recoveryEventFlag = ret;
	ret = sceKernelCreateThread("SceKernelResourceRecovery", sceKernelProcessResourceRecovery, 0xe7,
				     0x2000, 0, 0x80000, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9831);
		return ret;
	}
	g_recoveryThread = ret;
	ret = sceKernelStartThread(ret, 0, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9838);
		return ret;
	}
	g_defaultPhbp[0].value = 0;
	g_defaultPhbp[0].control = 0;
	g_defaultPhbp[1].value = 0;
	g_defaultPhbp[1].control = 0;
	g_defaultPhbp[2].value = 0;
	g_defaultPhwp[0].value = 0;
	g_defaultPhwp[0].control = 0;
	g_defaultPhwp[1].value = 0;
	g_defaultPhwp[1].control = 0;
	g_defaultPhbp[2].control = 0;
	g_defaultPhbp[3].value = 0;
	g_defaultPhwp[2].value = 0;
	g_defaultPhbp[3].control = 0;
	g_defaultPhwp[2].control = 0;
	g_defaultPhbp[4].value = 0;
	g_defaultPhwp[3].value = 0;
	g_defaultPhbp[4].control = 0;
	g_defaultPhwp[3].control = 0;
	return 0;
}

/* the process manager functions sysroot calls (SceSysrootForKernel_D29BCA77) */
static const struct {
	SceSize size;
	void *(*getLibkernelAddr0)(SceProcessObject *process);
	void *(*getLibkernelAddr1)(SceProcessObject *process);
	void *(*getLibkernelAddr2)(SceProcessObject *process);
	void *(*allocCurrentHeap)(SceSize size, void *opt, const void *caller);
	int (*freeCurrentHeap)(void *ptr);
	void *(*allocRemoteHeap)(SceUID pid, SceSize size, void *opt, const void *caller);
	int (*freeRemoteHeap)(SceUID pid, void *ptr);
	int (*moduleStart2)(void);
	int (*inhibitProcEvent)(SceUID pid);
	int (*permitProcEvent)(SceUID pid);
	void *(*getObjectHeap)(SceUID pid, int kind);
	int (*createLocalStorage)(const char *name, SceSize size);
	void *(*getLocalStorageAddr)(SceUInt32 key);
	int (*getLocalStorageAddrForPid)(SceUID pid, SceUInt32 key, void **addr, int create);
	int (*debugSuspend)(SceUID pid, SceUInt32 state);
	int (*getSdkVersion)(SceUID pid, SceUInt32 *sdkVersion);
} g_sysrootOps = {
	0x44,
	ProcessGetLibkernelAddr0,
	ProcessGetLibkernelAddr1,
	ProcessGetLibkernelAddr2,
	ProcessAllocCurrentHeapMemory,
	SceProcessmgrForDriver_FC2A424E,
	sceKernelAllocRemoteProcessHeapForSysroot,
	sceKernelFreeRemoteProcessHeap,
	ProcessmgrModuleStart2,
	sceKernelInhibitProcEvent,
	sceKernelPermitProcEvent,
	sceKernelGetObjectHeap,
	SceProcessmgrForDriver_3801D7D6,
	SceProcessmgrForDriver_EE694840,
	SceProcessmgrForDriver_AF80F39C,
	sceKernelProcessDebugSuspend,
	SceProcessmgrForDriver_D141C076,
};

static SceUID g_ppmBlock[2];
static SceProcessObject g_shellDummyProcess;
static SceProcessObject g_npdrmDummyProcess;

static inline __attribute__((always_inline)) void PPMInit(void)
{
	g_ppmLock = 0;
	g_ppmUsed = 0;
	g_ppmBlock[0] = SceSysmemForDriver_C94850C9("SceKernelPPM0", 0x1020d004, 0x40000, NULL);
	if (g_ppmBlock[0] < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 426, "sceKernelAllocMemBlockForKernel failed 0x%08x\n",
				     g_ppmBlock[0]);
		return;
	}
	if (SceSysmemForDriver_A841EDDA(g_ppmBlock[0], &g_ppm.base0) < 0)
		g_ppm.base0 = 0;
	g_ppmBlock[1] = SceSysmemForDriver_C94850C9("SceKernelPPM1", 0x1020d006, 0x10000, NULL);
	if (g_ppmBlock[1] < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 439, "sceKernelAllocMemBlockForKernel failed 0x%08x\n",
				     g_ppmBlock[1]);
		return;
	}
	if (SceSysmemForDriver_A841EDDA(g_ppmBlock[1], &g_ppm.base1) < 0)
		g_ppm.base1 = 0;
}

static inline __attribute__((always_inline)) int ProcessmgrModuleStart(void)
{
	int ret;

	g_processmgr.processClass = &g_processClass;
	SceThreadmgrForKernel_59D1D5B0(&g_processClass);
	ListInit(&g_processList);
	g_processListLock = 0;
	sceKernelSysrootSetModulePrivate(7, &g_processmgr);
	ret = CreateDefaultBudget();
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9889);
		return ret;
	}
	SCE_DBG_ASSERT_CHECK(1, 9699);
	ret = SceSysmemForDriver_61317102(&g_processClass, "SceUIDProcessClass", SceSysmemForDriver_85336A1C(),
				    sizeof(SceProcessObject), (SceClassCallback)ProcessConstructor,
				    (SceClassCallback)Destructor);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(9703);
	} else {
		PPMInit();
		if (CreateKernelProcess() < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 9711, "CreateKernelProcess failed\n");
		} else if (SceDipswForDriver_A98FC2FD(0xc2) != 0) {
			CreateDummyProcessWithUID(SHELL_DUMMY_PID, &g_shellDummyProcess,
						  &g_shellDummyAuthInfo, 0x20);
			CreateDummyProcessWithUID(NPDRM_DUMMY_PID, &g_npdrmDummyProcess,
						  &g_npdrmDummyAuthInfo, 0x40);
		}
	}
	SceSysrootForKernel_D29BCA77(&g_sysrootOps);
	sceKernelSysmemModuleStartAfterProcessmgr();
	g_firstCreate = 1;
	memset(&g_exitSpawn, 0, sizeof(g_exitSpawn));
	g_threadIdVector.lock = 0;
	g_intrThreadIdVector.lock = 0;
	g_excpThreadIdVector.lock = 0;
	g_threadClass = SceThreadmgrForKernel_565BD2DA();
	return 0;
}

int StartCore0Phase1(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	return ProcessmgrModuleStart();
}
