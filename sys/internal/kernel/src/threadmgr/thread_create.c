/*
 * SceKernelThreadMgr (threadmgr.c): creating a thread. The thread object
 * gets its register and VFP save areas from the process heap and a kernel
 * stack; a user thread also gets a user stack, a user TLS block and a UID
 * in its process, all filled in through temporary kernel mappings. The new
 * thread waits, dormant, for a start.
 */

#include <string.h>
#include <psp2common/kernel/modulemgr.h>
#include <psp2common/kernel/threadmgr.h>
#include "affinity.h"
#include "private.h"
#include "priority_change.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

/* createThread's flags */
#define CREATE_FLAG_USER		0x00000001	/* PLACEHOLDER: name; not read here */
#define CREATE_FLAG_PROCESS		0x00000002	/* the thread is for the process given */

#define THREAD_ATTR_USER		0x80000000	/* runs in user mode */
#define THREAD_ATTR_NO_STACK_FILL	0x10000000	/* PLACEHOLDER: its stacks are filled only with DIPSW 0xd8 */
#define THREAD_ATTR_EXCEPTION_LIST	0x04000000	/* PLACEHOLDER: name; it gets an exception entry */
#define THREAD_ATTR_NO_MODULE_TLS	0x02000000	/* PLACEHOLDER: name; no module TLS image is copied */
#define THREAD_ATTR_IN_SUSPENDED	0x00800000	/* PLACEHOLDER: name; may be created in a suspended process */
#define THREAD_ATTR_UID_ATTR_MASK	0x00070000	/* passed on to its UID */
#define THREAD_ATTR_UNLIMITED		0x00004000	/* not counted against the process's thread limit */
#define THREAD_ATTR_OBJECT_MASK		0x0000a000	/* kept in the object header */

/* the attributes each kind of caller may not give */
#define THREAD_ATTR_ILLEGAL_ROOT	0x87f85fff
#define THREAD_ATTR_ILLEGAL_SYSTEM	0x05785fff
#define THREAD_ATTR_ILLEGAL_KERNEL	0x797f5fff	/* a kernel caller or a shared module */
#define THREAD_ATTR_ILLEGAL_USER	0x7affdfff

/* SceKernelThreadOptParam.attr: the fields given */
#define THREAD_OPT_KSTACK_TYPE		0x01
#define THREAD_OPT_USTACK_TYPE		0x02
#define THREAD_OPT_UTLS_TYPE		0x04
#define THREAD_OPT_USTACK_BLOCK		0x08
#define THREAD_OPT_HANDED_EXCP		0x10
#define THREAD_OPT_ATTR_MASK		0x1f
#define THREAD_HANDED_EXCP_DEFAULT	0x301
#define THREAD_HANDED_EXCP_MASK		0x301



/* priorities */
#define PRIORITY_DEFAULT		0		/* the creator's */
#define PRIORITY_RELATIVE		0x10000000	/* relative to the process type's default */
#define PRIORITY_RELATIVE_BASE		0x100000e0
#define PRIORITY_RELATIVE_RANGE		0x40
#define PRIORITY_RELATIVE_OFFSET	0x100000a0
#define PRIORITY_RELATIVE_OFFSET_SYSTEM	0x10000060
#define PRIORITY_RELATIVE_OFFSET_OLD	0x10000100
#define PRIORITY_USER_LOW		0x40
#define PRIORITY_USER_RANGE_SYSTEM	0x80
#define PRIORITY_USER_RANGE		0xbc
#define PRIORITY_SHARED_LOW		0x80	/* priorities 0x80-0xfb share the ready queues */
#define PRIORITY_SHARED_RANGE		0x7c
#define BOOST_PRIORITY_NONE		0xfe

#define AFFINITY_MASK			0xf
#define AFFINITY_CPU_COUNT		4
#define AFFINITY_SYSTEM_CPU		0x8	/* PLACEHOLDER: name */

#define SDK_VERSION_AFFINITY_CHECK	0x01800000	/* older SDKs: the affinity must fit the priority */
#define SDK_VERSION_DEFAULT_AFFINITY	0x02100000
#define SDK_VERSION_BIG_STACK		0x03000000	/* from here a user stack may not pass 32 MiB */
#define USER_STACK_MAX			0x02000000

#define STACK_PAGE			0x1000
#define STACK_MIN			0x1000
#define KERNEL_STACK_USER		0x1000	/* a user thread's kernel stack */
#define KERNEL_STACK_USER_BIG		0x4000	/* the same with DIPSW 0xd9 */
#define STACK_FILL			0xff

#define DIPSW_STACK_CHECK		0xd8
#define DIPSW_BIG_KERNEL_STACK		0xd9
#define DIPSW_THBP			0xe4

#define MEMBLOCK_TYPE_KERNEL_STACK	0x1020d006	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_KSTACK_UNLIMITED	0x10a0d006	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_KSTACK_USER	0x10c0d006	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_USER_RW		0x0c20d060	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_USER_SHARED	0x0e20d060	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_KERNEL_MIRROR	0x6020d006	/* PLACEHOLDER: name */
#define MEMBLOCK_ATTR_KSTACK		((4 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_HAS_EXTRA_LOW)
#define MEMBLOCK_ATTR_USTACK		((5 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_NO_MAP | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID | \
					 ALLOC_ATTR_HAS_EXTRA_LOW)
#define MEMBLOCK_ATTR_UTLS		((6 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_NO_MAP | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID)
#define MEMBLOCK_ATTR_MIRROR		(ALLOC_ATTR_SHARE_PHYPAGE | \
					 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID)

#define ACCESS_ENTRY_KERNEL		5	/* PLACEHOLDER: ksceKernelIsEqualAccessibleRangeProcBySW modes */
#define ACCESS_ENTRY_USER		0x50

/* the user TLS block: 0x800 bytes of thread words below the TLS, the module TLS above */
#define USER_TLS_OFFSET			0x800
#define USER_TLS_BLOCK_MIN		0x1000
#define USER_TLS_AREA_MAX		0xffffe800
#define USER_TLS_ROUND			(USER_TLS_OFFSET + STACK_PAGE - 1)

#define REGISTER_CONTEXT_ALIGN		0x40
#define EXCEPTION_ENTRY_ALIGN		8
#define DACR_KERNEL			0x15450000	/* the register context's kernelDacr */
#define DACR_USER			0x15455551	/* and its userDacr */
#define REGISTER_POISON			0xdeadbeef
#define USER_FRAME_FPSCR		0x03000000
#define USER_FRAME_UNK40		4		/* PLACEHOLDER: meaning */
#define USER_FRAME_POISON_COUNT		13
#define PROCESS_THREAD_MAX		0x80

#define DEBUG_THREAD_EVENT_CREATE	0
#define TRACE_EVENT_THREAD_CREATE	0

#define SCE_KERNEL_ERROR_ILLEGAL_TYPE		0x8002000c
#define SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE	0x80028024
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION	0x80028026
#define SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED	0x8002802f
#define SCE_KERNEL_ERROR_NO_MEMORY		0x8002710b
#define SCE_KERNEL_ERROR_UID_RL_OVERFLOW_SYSCALL	0xc002450d	/* 0x8002450d | SYSCALL_ERROR_BIT */

typedef void (*ThreadmgrLwpCreateProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
					SceUID threadId, SceUID processId);

/* the words a user thread's TLS block starts with (below the TLS) */
struct ThreadmgrUserTls {
	uint32_t reserved0;
	SceUID threadId;			/* 0x04: its UID in the process */
	uintptr_t stackTop;			/* 0x08 */
	uintptr_t stackBase;			/* 0x0c */
	uint32_t reserved1[2];			/* 0x10: zeroed */
	uint32_t stackGuard[2];			/* 0x18: random, then the process's */
	int priority;				/* 0x20 */
	uint32_t cpuAffinityMask;		/* 0x24: << 16 */
};

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceKernelGetModuleIsSharedByAddr(SceUID pid, const void *addr);
extern int ksceKernelIsEqualAccessibleRangeProcBySW(SceUID pid, uint32_t mode, const void *addr,
						    SceSize size);
extern int ksceSblACMgrIsRootProgram2(SceUID pid);
extern int ksceSblACIsSystemProgram(SceUID pid);
extern int ksceGUIDGetObject(SceUID guid, SceObjectBase **object);
extern void *SceSysmemForKernel_FA6C098C(SceUID uid);		/* the object of a UID */
extern SceUID SceSysrootForKernel_7334F1E8(SceUID pid);
extern int ksceKernelSysrootGetPUIDEntryHeap(SceUID pid, void **heap);
extern int ksceKernelGetMemBlockMappedBase(SceUID uid, void **base);
extern int ksceKernelFreeMemBlock(SceUID uid);
extern int ksceKernelMapMemBlock(SceUID uid);
extern int ksceKernelCheckDipsw(unsigned int bit);
extern SceUID ksceKernelGetModuleIdByPid(SceUID pid);
extern int ksceKernelGetModuleInfo(SceUID pid, SceUID modid, SceKernelModuleInfo *info);
extern int ksceKernelCopyFromUserProc(SceUID pid, void *dst, const void *src, SceSize size);
extern void *ksceKernelAllocHeapMemory2(SceUID pid, SceSize size, SceAllocOpt *opt);
extern int SceSysrootForKernel_7968F091(SceUID pid, void *ptr);
extern SceUID kscePUIDOpenByGUID(SceUID pid, SceUID guid);
extern int kscePUIDClose(SceUID pid, SceUID puid);
extern int ksceMt19937GlobalUninit(void *buffer, SceSize size);	/* PLACEHOLDER: fills it with random bytes */
extern uintptr_t SceSysrootForKernel_5F2CE289(SceProcessObject *process);
int syslibtraceThreadCBInit(struct ThreadCB *thread);
int dtraceThreadCBInit(struct ThreadCB *thread);
void notifyDebugThreadEvent(SceUID processId, SceUID threadId, uint32_t event, int *status);

static uint32_t s_procLwpCreateProbeId;
volatile uintptr_t s_procLwpCreateHandler;
extern const char s_procLwpCreateCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_procLwpCreateDescriptor = {
	.probeId = &s_procLwpCreateProbeId,
	.provider = "__proc_",
	.name = "lwp__create",
	.codeSite = s_procLwpCreateCodeSite,
	.handler = &s_procLwpCreateHandler,
};

#define PRIORITY_FORMAT \
	"***\n***\n***\n*** error: createThread(\"%s\"), illegal priority for user mode thread(%d)\n" \
	"*** This will be forbidden by 2011/04/13 !\n***\n***\n***\n"
#define AFFINITY_FORMAT \
	"*** error: createThread(), CPU affinity mask and priority mismatch. " \
	"name=[%s], mask=0x%08X, priority=%d\n"
#define ALLOC_FORMAT \
	"*** error: createThread(), sceKernelAllocMemBlockForKernel(\"%s\")=0x%08X\n"
#define FREE_USER_TLS_FORMAT "*** free uTLSMemblkId, eRet=0x%08X\n"

/*
 * Creates a thread of process pid (with CREATE_FLAG_PROCESS; otherwise of
 * the caller's process for a user thread, else the kernel's). callerAddr is
 * where a user caller called from. Returns the thread's UID: a syscall gets
 * the UID in its process.
 */
SceUID createThread(SceUID pid, const char *name, uintptr_t entry, int initPriority,
		    SceSize stackSize, uint32_t attr, uint32_t cpuAffinityMask,
		    const SceKernelThreadOptParam *option, uint32_t flags, const void *callerAddr)
{
	struct ThreadCB *self, *thread;
	struct ThreadmgrThreadObject *object;
	struct ThreadRegisterContext *regs;
	ThreadSyscallFrame *frame;
	struct ThreadmgrExceptionEntry *exception;
	ProcessBudget *type;
	SceProcessObject *process;
	struct ThreadmgrUserTls *userTls;
	UIDCreateOpt uidOpt;
	SceKernelAllocMemBlockKernelOpt opt;
	SceKernelModuleInfo info;
	SceKernelThreadOptParam param;
	SceKernelProcessContext *context;
	SceAllocOpt heapOpt;
	ThreadmgrLwpCreateProbe lwpCreate;
	ThreadmgrTraceHook trace;
	SceUID ret, uid, puid, moduleId, stackMirror, tlsMirror;
	SceUInt32 sdkVersion, defaultMask;
	SceSize tlsSize;
	uint32_t saved, access, memType, handedExceptions, *stackMirrorBase;
	void *tlsBase;
	int inSyscall, shared, priority, intr, n, cpus, err;
	unsigned int cpu;

	stackMirrorBase = NULL;
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	inSyscall = ThreadmgrInSyscall();
	shared = 0;
	if (inSyscall)
		shared = ksceKernelGetModuleIsSharedByAddr(currentThreadContext()->processId,
							   callerAddr);
	self = currentThreadContext();
	if (self == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 3164, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (currentThreadContext()->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF(0, 3172,
			"*** error: illegal context(thread event handler). [%s]\n", name);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	access = (int)attr < 0 ? ACCESS_ENTRY_USER : ACCESS_ENTRY_KERNEL;
	err = ksceKernelIsEqualAccessibleRangeProcBySW(pid, access, (const void *)entry, 4);
	if (err != 0) {
		SCE_DBG_PRINTF(0, 3189, "*** error: createThread(%s), "
			"sceKernelIsEqualAccessibleRangeProcBySW(0x%08x, 0x%08x, %p): error 0x%08X\n",
			name, pid, access, entry, err);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}

	/* the attributes this caller may give */
	if (ksceSblACMgrIsRootProgram2(pid)) {
		if (attr & THREAD_ATTR_ILLEGAL_ROOT) {
			SCE_DBG_PRINTF(0, 3206, " k: error: createThread(%s), attr=0x%08X\n",
				       name, attr);
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (ksceSblACIsSystemProgram(pid)) {
		if (attr & THREAD_ATTR_ILLEGAL_SYSTEM) {
			SCE_DBG_PRINTF(0, 3224, " k: error: createThread(%s), attr=0x%08X\n",
				       name, attr);
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (!inSyscall || shared) {
		if (attr & THREAD_ATTR_ILLEGAL_KERNEL) {
			SCE_DBG_PRINTF(0, 3238, " k: error: createThread(%s), attr=0x%08X\n",
				       name, attr);
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & THREAD_ATTR_ILLEGAL_USER) {
		SCE_DBG_PRINTF(0, 3249, " k: error: createThread(%s), attr=0x%08X\n", name, attr);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}

	/* the options (one that does not fit is ignored) */
	param = (SceKernelThreadOptParam){ 0 };
	handedExceptions = THREAD_HANDED_EXCP_DEFAULT;
	if (option != NULL && option->size <= sizeof(param) &&
	    (option->attr & ~THREAD_OPT_ATTR_MASK) == 0) {
#if FW_DEBUG
		__memcpy_chk(&param, option, option->size, sizeof(param));
#else
		memcpy(&param, option, option->size);
#endif
		if ((int)attr < 0 && (param.attr & THREAD_OPT_KSTACK_TYPE)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
		if ((param.attr & THREAD_OPT_USTACK_TYPE) &&
		    param.uStackMemType != MEMBLOCK_TYPE_USER_RW) {
			if (param.uStackMemType != MEMBLOCK_TYPE_USER_SHARED || (!shared && inSyscall)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
				goto restore;
			}
			attr |= THREAD_ATTR_UNLIMITED;
		}
		if ((param.attr & THREAD_OPT_UTLS_TYPE) &&
		    param.uTLSMemType != MEMBLOCK_TYPE_USER_RW &&
		    (param.uTLSMemType != MEMBLOCK_TYPE_USER_SHARED || (!shared && inSyscall))) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
			goto restore;
		}
		if ((param.attr & THREAD_OPT_USTACK_BLOCK) &&
		    (inSyscall || (int)param.uStackMemid < 0)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
		if (param.attr & THREAD_OPT_HANDED_EXCP) {
			handedExceptions = param.data_0x18;
			if (handedExceptions & ~THREAD_HANDED_EXCP_MASK) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
				goto restore;
			}
		}
	}

	/* the process it is for */
	if (!(flags & CREATE_FLAG_PROCESS)) {
		if ((int)attr < 0 && (self = currentThreadContext()) != NULL)
			pid = self->processId;
		else
			pid = THREADMGR_KERNEL_PID;
	}
	if (pid == THREADMGR_KERNEL_PID)
		sdkVersion = SDK_VERSION_KERNEL;
	else if (SceSysrootForDriver_CAE820B2(pid, &sdkVersion) < 0)
		sdkVersion = 0;
	process = SceSysmemForKernel_FA6C098C(pid);
	if (process == NULL) {
		ret = SCE_KERNEL_ERROR_INVALID_UID;
		goto restore;
	}
	ret = ksceGUIDGetObject(process->budgetId, (SceObjectBase **)&type);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 3344,
			" k: error: createThread(%s)=0x%08X, pProcess->uidBudget=0x%08X\n",
			name, ret, process->budgetId);
		goto restore;
	}

	/* its priority */
	priority = initPriority;
	if ((unsigned int)initPriority - 1 >= BOOST_PRIORITY_NONE) {
		if (initPriority == PRIORITY_DEFAULT) {
			priority = currentThreadContext()->currentPriority;
		} else if (!(initPriority & PRIORITY_RELATIVE)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto restore;
		} else if ((unsigned int)initPriority - PRIORITY_RELATIVE_BASE >=
			   PRIORITY_RELATIVE_RANGE) {
			priority = initPriority - PRIORITY_RELATIVE_OFFSET_OLD;
			SCE_DBG_PRINTF(0, 3355, "***\n***\n***\n*** error: createThread(\"%s\"), "
				"illegal priority offset(%d)\n*** This will be forbidden by 2011/04/13 !\n"
				"***\n***\n***\n", name, priority);
			ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto restore;
		} else {
			switch (type->type) {
			case PROCESS_TYPE_GAME:
				if (!(attr & THREAD_ATTR_UNLIMITED) ||
				    !(cpuAffinityMask & AFFINITY_SYSTEM_CPU)) {
					priority = initPriority - PRIORITY_RELATIVE_OFFSET_SYSTEM;
					break;
				}
				/* fall through */
			case PROCESS_TYPE_MINI_APP:
			case PROCESS_TYPE_SHELL:
			case PROCESS_TYPE_KERNEL:
				priority = initPriority - PRIORITY_RELATIVE_OFFSET;
				break;
			default:
				priority = initPriority - PRIORITY_RELATIVE_OFFSET;
				SCE_DBG_PRINTF(0, 3375,
					"*** warning: unknown process type(0x%08X), [%s] priority %d\n",
					type->type, name, priority);
				break;
			}
		}
	}
	if ((int)attr < 0) {
		if (type->type == PROCESS_TYPE_GAME) {
			if ((unsigned int)priority - PRIORITY_USER_LOW >= PRIORITY_USER_RANGE_SYSTEM) {
				SCE_DBG_PRINTF(0, 3388, PRIORITY_FORMAT, name, priority);
				ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
				goto restore;
			}
		} else if ((unsigned int)priority - PRIORITY_USER_LOW >= PRIORITY_USER_RANGE) {
			SCE_DBG_PRINTF(0, 3394, PRIORITY_FORMAT, name, priority);
			ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto restore;
		}
	}

	/* its CPU affinity */
	ret = checkCpuAffinityMaskPhy(cpuAffinityMask, attr, process);
	if (ret < 0)
		goto restore;
	if (sdkVersion < SDK_VERSION_AFFINITY_CHECK && s_threadmgrRuntime.manager->testMode == 0) {
		if ((unsigned int)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			/* a shared priority: on the process type's CPUs */
			if (cpuAffinityMask != 0) {
				defaultMask = sceKernelGetDefaultCpuAffinityMask(type);
				if ((defaultMask & cpuAffinityMask) !=
				    sceKernelGetDefaultCpuAffinityMask(type)) {
					SCE_PRINTF_WITH_INFO(0, SCE_LINE(3407, 3413), AFFINITY_FORMAT,
							     name, cpuAffinityMask, priority);
					ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
					goto restore;
				}
			}
		} else if (cpuAffinityMask != 0) {
			/* a CPU's own priority: on one CPU */
			cpus = 0;
			for (cpu = 0; cpu != AFFINITY_CPU_COUNT; cpu++) {
				if (((int)cpuAffinityMask >> cpu) & 1)
					cpus++;
			}
			if (cpus > 1) {
				if (strncmp(name, "ScePsp2SwuCui", 0x1f) != 0) {
					SCE_PRINTF_WITH_INFO(0, SCE_LINE(3425, 3431), AFFINITY_FORMAT,
							     name, cpuAffinityMask, priority);
					ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
					goto restore;
				}
				SCE_DBG_PRINTF(0, 3437, "=== [ScePsp2SwuCui]: initPriority=%d, "
					"cpuAffinityMask=0x%08X, accepted to keep compatibility.\n",
					priority, cpuAffinityMask);
			}
		}
	}

	/* its stack size */
	if ((int)attr < 0 && stackSize > USER_STACK_MAX && sdkVersion >= SDK_VERSION_BIG_STACK) {
		SCE_DBG_PRINTF(0, 3447, "*** error: createThread(), name=[%s], stackSize=0x%08X\n",
			       name, stackSize);
		ret = SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
		goto restore;
	}
	stackSize = ((stackSize + STACK_PAGE - 1) / STACK_PAGE) * STACK_PAGE;
	ret = SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	if (stackSize == 0)
		goto restore;
	ret = ksceKernelProcessGetContext(pid, &context);
	if (ret < 0)
		goto restore;

	/* the object, handed over locked */
	uidOpt = (UIDCreateOpt){
		.flags = UID_CREATE_OPT_ATTR,
		.attr = attr & THREAD_ATTR_UID_ATTR_MASK,
	};
	if (pid != THREADMGR_KERNEL_PID) {
		uidOpt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		uidOpt.pid = pid;
		if (attr & THREAD_ATTR_UNLIMITED)
			uidOpt.heapKind = 1;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->lock);
	uid = ksceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->threadClass, name,
					  (SceGUIDKernelCreateOpt *)&uidOpt,
					  (SceObjectBase **)&object);
	ret = uid;
	if (uid < 0)
		goto unlock_process;
	thread = &object->context;
	object->ownerPid = pid;
	object->attributes = attr & THREAD_ATTR_OBJECT_MASK;
	thread->boostPriority = BOOST_PRIORITY_NONE;
	thread->threadId = uid;
	thread->initAttr = attr;
	thread->currentAttr = attr;
	thread->changeCpuCount = -1;
	thread->initPriority = priority;
	thread->basePriority = priority;
	thread->currentPriority = priority;
	thread->waitId = -1;
	thread->currentCpuId = -1;
	thread->lastExecutedCpuId = -1;
	thread->delayId = -1;
	thread->uStackMemblkId = -1;
	thread->kStackMemblkId = -1;
	thread->userTlsMemBlockId = -1;
	thread->entry = entry;
	thread->processId = pid;
	thread->process = process;
	thread->magic = THREAD_CB_MAGIC;
	thread->syslibtraceState = NULL;
	if (s_threadmgrRuntime.manager->syslibtraceThreadCBEnable)
		syslibtraceThreadCBInit(thread);
	thread->dtraceState = NULL;
	thread->dtraceLock = 0;
	if (s_threadmgrRuntime.manager->dtraceThreadCBEnable)
		dtraceThreadCBInit(thread);
	thread->addressSpace = SceSysmemForKernel_FA6C098C(SceSysrootForKernel_7334F1E8(pid));
	ret = ksceKernelSysrootGetPUIDEntryHeap(pid, &thread->pUIDEntryHeap);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 3539, "*** error: createThread(), sceKernelSysrootGetUIDEntryHeap"
			"(0x%08X)=0x%08X, pThreadCB->pUIDEntryHeap=%p\n",
			pid, ret, thread->pUIDEntryHeap);
		goto fail_kernel_stack;
	}
	thread->initCpuAffinityMask = cpuAffinityMask & AFFINITY_MASK;
	thread->currentCpuAffinityMask = cpuAffinityMask & AFFINITY_MASK;
	thread->initPermission = process->threadPermission;
	thread->permission = process->threadPermission;
	thread->tlsProcessId = pid;
	thread->ioFlags = 0;

	/* its kernel stack */
	memset(&opt, 0, sizeof(opt));
	opt.attr = MEMBLOCK_ATTR_KSTACK;
	opt.size = sizeof(opt);
	if ((int)attr < 0) {
		thread->kStackSize = ksceKernelCheckDipsw(DIPSW_BIG_KERNEL_STACK) ?
			KERNEL_STACK_USER_BIG : KERNEL_STACK_USER;
		thread->stackMemBlockType = (attr & THREAD_ATTR_UNLIMITED) ?
			MEMBLOCK_TYPE_KSTACK_UNLIMITED : MEMBLOCK_TYPE_KSTACK_USER;
		opt.attr |= SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID;
		opt.pid = pid;
	} else {
		if (stackSize < STACK_MIN) {
			stackSize = STACK_MIN;
			SCE_DBG_PRINTF(0, 3581, "warning: specified stack size for [%s] is less "
				       "than 8KiB.(resized)\n", name);
		}
		thread->kStackSize = stackSize;
		if ((param.attr & THREAD_OPT_KSTACK_TYPE) && param.kStackMemType != 0)
			thread->stackMemBlockType = param.kStackMemType;
		else
			thread->stackMemBlockType = MEMBLOCK_TYPE_KERNEL_STACK;
	}
	opt.extraLow = STACK_PAGE;	/* a guard page below */
	thread->kStackMemblkId = ksceKernelAllocMemBlock(name, thread->stackMemBlockType,
		thread->kStackSize + STACK_PAGE, &opt);
	if (thread->kStackMemblkId < 0) {
		SCE_DBG_PRINTF(0, 3604, "*** error: createThread(), "
			"sceKernelAllocMemBlockForKernel(\"kStack: %s\")=0x%08X\n",
			name, thread->kStackMemblkId);
		ret = thread->kStackMemblkId;
		goto fail_kernel_stack;
	}
	ksceKernelGetMemBlockMappedBase(thread->kStackMemblkId, (void **)&thread->pKStack);

	if ((int)attr >= 0) {
		tlsSize = 0;
		moduleId = -1;
		tlsMirror = -1;
		thread->uStackMemblkId = -1;
		stackMirror = -1;
		puid = -1;
		thread->userTlsMemBlockId = -1;
		thread->userThreadId = -1;
		thread->vfpException = 0x87;	/* PLACEHOLDER: meaning */
		goto contexts;
	}

	/* a user thread: its user stack, seen by the kernel through a mirror */
	if (param.attr & THREAD_OPT_USTACK_BLOCK) {
		thread->uStackSize = stackSize;
		thread->uStackMemblkId = param.uStackMemid;
	} else {
		memType = (param.attr & THREAD_OPT_USTACK_TYPE) ?
			param.uStackMemType : MEMBLOCK_TYPE_USER_RW;
		memset(&opt, 0, sizeof(opt));
		thread->uStackSize = stackSize;
		opt.size = sizeof(opt);
		opt.attr = MEMBLOCK_ATTR_USTACK;
		opt.extraLow = STACK_PAGE;	/* a guard page below */
		opt.pid = pid;
		thread->uStackMemblkId = ksceKernelAllocMemBlock(name, memType,
			stackSize + STACK_PAGE, &opt);
		if (thread->uStackMemblkId < 0) {
			SCE_DBG_PRINTF(0, 3637, "*** error: createThread(), "
				"sceKernelAllocMemBlockForKernel(\"uStack: %s\")=0x%08X\n",
				name, thread->uStackMemblkId);
			ret = thread->uStackMemblkId;
			goto fail_user_stack;
		}
	}
	ksceKernelGetMemBlockMappedBase(thread->uStackMemblkId,
					(void **)&thread->pUStack);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = MEMBLOCK_ATTR_MIRROR;
	opt.mirror_blockid = thread->uStackMemblkId;
	stackMirror = ksceKernelAllocMemBlock("UserStackKVA", MEMBLOCK_TYPE_KERNEL_MIRROR, 0,
					      &opt);
	if (stackMirror < 0) {
		SCE_DBG_PRINTF(0, 3667, ALLOC_FORMAT, "UserStackKVA", stackMirror);
		ret = stackMirror;
		goto fail_stack_mirror;
	}
	ksceKernelGetMemBlockMappedBase(stackMirror, (void **)&stackMirrorBase);

	/* its TLS block: the thread words, then the main module's TLS */
	moduleId = -1;
	tlsSize = USER_TLS_BLOCK_MIN;
	if (!(attr & THREAD_ATTR_NO_MODULE_TLS)) {
		moduleId = ksceKernelGetModuleIdByPid(pid);
		if (ksceGUIDReferObject(moduleId, NULL) < 0) {
			moduleId = -1;
			tlsSize = USER_TLS_BLOCK_MIN;
		} else {
			memset(&info, 0, sizeof(info));
			info.size = sizeof(info);
			if (ksceKernelGetModuleInfo(pid, moduleId, &info) < 0 ||
			    info.tlsAreaSize > USER_TLS_AREA_MAX)
				tlsSize = USER_TLS_BLOCK_MIN;
			else
				tlsSize = (info.tlsAreaSize + USER_TLS_ROUND) & ~(STACK_PAGE - 1);
		}
	}
	memType = (param.attr & THREAD_OPT_UTLS_TYPE) ? param.uTLSMemType : MEMBLOCK_TYPE_USER_RW;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = MEMBLOCK_ATTR_UTLS;
	opt.pid = pid;
	thread->userTlsMemBlockId = ksceKernelAllocMemBlock(name, memType, tlsSize,
		&opt);
	if (thread->userTlsMemBlockId < 0) {
		SCE_DBG_PRINTF(0, 3720, "*** error: createThread(), "
			"sceKernelAllocMemBlockForKernel(\"uTLS: %s\")=0x%08X\n",
			name, thread->userTlsMemBlockId);
		ret = thread->userTlsMemBlockId;
		thread->userTlsMemBlockId = -1;
		thread->tls = 0;
		goto fail_tls;
	}
	ksceKernelGetMemBlockVBase(thread->userTlsMemBlockId, &tlsBase);
	thread->tls = (uintptr_t)tlsBase + USER_TLS_OFFSET;
	memset(&opt, 0, sizeof(opt));
	opt.attr = MEMBLOCK_ATTR_MIRROR;
	opt.mirror_blockid = thread->userTlsMemBlockId;
	opt.size = sizeof(opt);
	tlsMirror = ksceKernelAllocMemBlock("UserTLSKVA", MEMBLOCK_TYPE_KERNEL_MIRROR, 0,
					    &opt);
	if (tlsMirror < 0) {
		SCE_DBG_PRINTF(0, 3744, ALLOC_FORMAT, "UserTLSKVA", thread->userTlsMemBlockId);
		ret = tlsMirror;
		goto fail_tls_mirror;
	}
	ksceKernelGetMemBlockVBase(tlsMirror, (void **)&userTls);

	if (attr & THREAD_ATTR_EXCEPTION_LIST) {
		memset(&heapOpt, 0, sizeof(heapOpt));
		heapOpt.size = sizeof(heapOpt);
		heapOpt.align = EXCEPTION_ENTRY_ALIGN;
		exception = ksceKernelAllocHeapMemory2(pid, 0x28, &heapOpt);
		thread->exceptionEntry = exception;
		if (exception == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			SCE_DBG_PRINTF(0, 3760, "*** error: createThread(), "
				"sceKernelSysrootAllocRemoteProcessHeap(0x%08X)=%p, pExceptionList\n",
				pid, NULL);
			goto fail_exception;
		}
		memset(exception, 0, 0x28);
		thread->exceptionEntry->thread = thread;
		thread->handedExceptions = handedExceptions;
	}
	thread->vfpException = 0;
	puid = kscePUIDOpenByGUID(pid, uid);
	if (puid < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 3773);
		ret = puid;
		goto fail_puid;
	}
	thread->userThreadId = puid;

contexts:
	/* its register and VFP save areas */
	memset(&heapOpt, 0, sizeof(heapOpt));
	heapOpt.size = sizeof(heapOpt);
	heapOpt.align = REGISTER_CONTEXT_ALIGN;
	thread->pRegisterContext = ksceKernelAllocHeapMemory2(pid, sizeof(*thread->pRegisterContext), &heapOpt);
	if (thread->pRegisterContext == NULL) {
		SCE_DBG_PRINTF(0, 3795, "*** error: createThread(), "
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X)=%p, pRegisterContext\n",
			pid, NULL);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto fail_registers;
	}
	memset(thread->pRegisterContext, 0, sizeof(*thread->pRegisterContext));
	regs = thread->pRegisterContext;
	regs->ttbr1 = context->TTBR1;
	regs->kernelDacr = DACR_KERNEL;
	regs->contextidr = (uint8_t)context->CONTEXTIDR | uid << 8;
	if ((int)thread->currentAttr < 0) {
		/* the syscall frame its first return to user mode leaves through */
		regs->userDacr = DACR_USER;
		thread->kernelStackTop = thread->pKStack + thread->kStackSize;
		frame = (ThreadSyscallFrame *)thread->kernelStackTop - 1;
		for (n = 0; n < USER_FRAME_POISON_COUNT; n++)
			frame->r[n] = REGISTER_POISON;
		frame->syscallAux = 0;
		frame->entry = REGISTER_POISON;
		frame->fpscr = USER_FRAME_FPSCR;
		frame->apiDeclare = USER_FRAME_UNK40;
		frame->sp = thread->uStackSize + thread->pUStack;
		frame->lr = SceSysrootForKernel_5F2CE289(thread->process);
		frame->pc = entry & ~1;
		thread->userFrame = frame;
	}
	memset(&heapOpt, 0, sizeof(heapOpt));
	heapOpt.size = sizeof(heapOpt);
	heapOpt.align = REGISTER_CONTEXT_ALIGN;
	thread->pVfpRegisterContext = (uintptr_t)ksceKernelAllocHeapMemory2(pid,
		sizeof(struct ThreadmgrVfpContext), &heapOpt);
	if (thread->pVfpRegisterContext == 0) {
		SCE_DBG_PRINTF(0, 3854, "*** error: createThread(), "
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X)=%p, pVfpRegisterContext\n",
			pid, NULL);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto fail_vfp;
	}
	if (ksceKernelCheckDipsw(DIPSW_THBP)) {
		thread->thbp[0] = thread->process->contextBreakpoint->value;
		thread->thbp[1] = thread->process->contextBreakpoint->control;
	}

	/* dormant */
	thread->exitStatus = SCE_KERNEL_ERROR_DORMANT;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			 SCE_KERNEL_THREAD_STATUS_DORMANT;
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->dormantNext;
	thread->prev = s_threadmgrRuntime.manager->dormantPrev;
	s_threadmgrRuntime.manager->dormantPrev = (uintptr_t)thread;
	((struct ThreadCB *)thread->prev)->next = (uintptr_t)thread;
	ksceKernelSpinlockLowUnlock(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	if ((int)thread->currentAttr < 0) {
		/* a user thread of a process the debugger holds */
		if (!(flags & CREATE_FLAG_PROCESS)) {
			if (process->status & (PROCESS_FLAG_DEBUG_SUSPENDING | PROCESS_STATE_DEBUG)) {
				thread->debugSuspendReason |= DEBUG_SUSPEND_PROCESS;
				thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
			}
		} else if (((process->status & (PROCESS_FLAG_DEBUG_SUSPENDING | PROCESS_STATE_DEBUG)) ||
			    (process->status & PROCESS_STATE_MASK) == PROCESS_STATE_SUSPENDED) &&
			   !(attr & THREAD_ATTR_IN_SUSPENDED)) {
			ret = SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED;
			goto fail_vfp_free;
		}
	}

	/* counted and listed in its process */
	if (!(attr & THREAD_ATTR_UNLIMITED)) {
		if ((unsigned int)process->threadCount >= PROCESS_THREAD_MAX) {
			ret = SCE_KERNEL_ERROR_UID_RL_OVERFLOW_SYSCALL;
			goto fail_vfp_free;
		}
		if ((uint32_t)++process->threadCount > process->threadPeak)
			process->threadPeak = process->threadCount;
	} else {
		if ((uint32_t)++process->unlimitedThreadCount > process->unlimitedThreadPeak)
			process->unlimitedThreadPeak = process->unlimitedThreadCount;
	}
	thread->processLink.next = &process->threads;
	thread->processLink.prev = process->threads.prev;
	process->threads.prev = &thread->processLink;
	thread->processLink.prev->next = &thread->processLink;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);

	/* its stacks: filled, with its UID at the bottom; a user thread's TLS words */
	if ((int)attr < 0) {
		if (!(attr & THREAD_ATTR_NO_STACK_FILL) || ksceKernelCheckDipsw(DIPSW_STACK_CHECK)) {
			memset((void *)thread->pKStack, STACK_FILL,
			       thread->kStackSize - sizeof(ThreadSyscallFrame));
			memset(stackMirrorBase, STACK_FILL, thread->uStackSize);
		}
		*(SceUID *)thread->pKStack = thread->threadId;
		*stackMirrorBase = uid;
		memset(userTls, 0, tlsSize);
		userTls->threadId = puid;
		userTls->stackBase = thread->pUStack;
		userTls->reserved1[0] = 0;
		userTls->reserved1[1] = 0;
		userTls->stackTop = thread->pUStack + thread->uStackSize;
		userTls->priority = thread->initPriority;
		defaultMask = thread->initCpuAffinityMask;
		if (defaultMask == 0 && sdkVersion < SDK_VERSION_DEFAULT_AFFINITY)
			defaultMask = process->cpuAffinity;
		userTls->cpuAffinityMask = defaultMask << 16;
		ksceMt19937GlobalUninit(&userTls->stackGuard[0], sizeof(userTls->stackGuard[0]));
		userTls->stackGuard[1] =
			thread->process->stackGuard;
		if (!(attr & THREAD_ATTR_NO_MODULE_TLS) && info.tlsInitSize != 0) {
			ret = ksceKernelCopyFromUserProc(pid, (uint8_t *)userTls + USER_TLS_OFFSET,
							 info.tlsInit, info.tlsInitSize);
			if (ret < 0) {
				SCE_DBG_ASSERT_LEVEL(0, 0, 3978);
				intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
					&process->lock);
				ksceKernelSpinlockLowLock(&object->waitLock);
				ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
				thread->processLink.next->prev = thread->processLink.prev;
				thread->processLink.prev->next = thread->processLink.next;
				if (!(attr & THREAD_ATTR_UNLIMITED))
					process->threadCount--;
				else
					process->unlimitedThreadCount--;
				goto fail_vfp_free;
			}
		}
	} else {
		if (!(attr & THREAD_ATTR_NO_STACK_FILL) || ksceKernelCheckDipsw(DIPSW_STACK_CHECK))
			memset((void *)thread->pKStack, STACK_FILL, thread->kStackSize);
		*(SceUID *)thread->pKStack = thread->threadId;
	}

	/* the mirrors go; the thread is announced */
	intr = ksceKernelCpuSuspendIntr();
	if (moduleId != -1)
		ksceGUIDReleaseObject(moduleId);
	if ((int)attr < 0) {
		if (ksceKernelMapMemBlock(thread->uStackMemblkId) < 0)
			SCE_PANIC(SCE_LINE(3997, 4003));
		if (ksceKernelFreeMemBlock(stackMirror) < 0)
			SCE_PANIC(SCE_LINE(4002, 4008));
		if (ksceKernelMapMemBlock(thread->userTlsMemBlockId) < 0)
			SCE_PANIC(SCE_LINE(4007, 4013));
		if (ksceKernelFreeMemBlock(tlsMirror) < 0)
			SCE_PANIC(SCE_LINE(4012, 4018));
	}
	notifyDebugThreadEvent(pid, puid, DEBUG_THREAD_EVENT_CREATE, NULL);
	/* the DTrace probe site __proc_:lwp__create */
	__asm__ volatile(".global s_procLwpCreateCodeSite\ns_procLwpCreateCodeSite:");
	lwpCreate = (ThreadmgrLwpCreateProbe)s_procLwpCreateHandler;
	if (lwpCreate != NULL)
		lwpCreate(&s_procLwpCreateDescriptor, (int)attr < 0 ? puid : uid, pid);
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_THREAD_CREATE, 3, entry, uid, pid);
	ret = inSyscall ? puid : uid;
	ksceKernelCpuResumeIntr(intr);
	goto restore;

	/* undone in reverse */
fail_vfp_free:
	err = SceSysrootForKernel_7968F091(pid, (void *)thread->pVfpRegisterContext);
	SCE_DBG_PRINTF(0, 4058, "*** free pVfpRegisterContext, eRet=0x%08X\n", err);
	thread->pVfpRegisterContext = 0;
fail_vfp:
	err = SceSysrootForKernel_7968F091(pid, thread->pRegisterContext);
	SCE_DBG_PRINTF(0, 4062, "*** free pRegisterContext, eRet=0x%08X\n", err);
	thread->pRegisterContext = NULL;
fail_registers:
	if ((int)attr < 0) {
		err = kscePUIDClose(pid, puid);
		SCE_DBG_PRINTF(0, 4067, "*** scePUIDClose, eRet=0x%08X\n", err);
		if (err < 0)
			SCE_PANIC(SCE_LINE(4063, 4069));
	}
fail_puid:
	if (attr & THREAD_ATTR_EXCEPTION_LIST) {
		err = SceSysrootForKernel_7968F091(pid, thread->exceptionEntry);
		SCE_DBG_PRINTF(0, 4075, "*** free pExceptionList, eRet=0x%08X\n", err);
		thread->exceptionEntry = NULL;
	}
	if ((int)attr < 0) {
fail_exception:
		err = ksceKernelFreeMemBlock(tlsMirror);
		SCE_DBG_PRINTF(0, 4081, "*** free uTLSTempId, eRet=0x%08X\n", err);
	}
fail_tls_mirror:
	err = ksceKernelFreeMemBlock(thread->userTlsMemBlockId);
	SCE_DBG_PRINTF(0, 4085, FREE_USER_TLS_FORMAT, err);
	thread->userTlsMemBlockId = -1;
	thread->tls = 0;
	if ((int)attr < 0) {
fail_tls:
		err = ksceKernelFreeMemBlock(stackMirror);
		SCE_DBG_PRINTF(0, 4091, "*** free uStackTempId, eRet=0x%08X\n", err);
		if (moduleId != -1)
			ksceGUIDReleaseObject(moduleId);
	}
fail_stack_mirror:
	err = ksceKernelFreeMemBlock(thread->uStackMemblkId);
	SCE_DBG_PRINTF(0, 4099, "*** free uStackMemblkId, eRet=0x%08X\n", err);
fail_user_stack:
	thread->uStackMemblkId = -1;
	thread->pUStack = 0;
	err = ksceKernelFreeMemBlock(thread->kStackMemblkId);
	SCE_DBG_PRINTF(0, 4104, "*** free kStackMemblkId, eRet=0x%08X\n", err);
fail_kernel_stack:
	thread->kStackMemblkId = -1;
	thread->pKStack = 0;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			 SCE_KERNEL_THREAD_STATUS_DELETED;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	if (ksceGUIDClose(uid) < 0)
		SCE_PANIC(SCE_LINE(4107, 4113));
unlock_process:
	ksceKernelSpinlockLowUnlock(&process->lock);
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* ksceKernelCreateThread: cpuAffinityMask may name the CPUs in bits 16-19 */
SceUID ksceKernelCreateThread(const char *name, uintptr_t entry, int initPriority,
			      SceSize stackSize, uint32_t attr, uint32_t cpuAffinityMask,
			      const SceKernelThreadOptParam *option)
{
	if (cpuAffinityMask & ~0xf000f)
		return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	if (cpuAffinityMask & 0xf0000) {
		if (cpuAffinityMask & 0xf)
			return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		cpuAffinityMask = (int)cpuAffinityMask >> 16;
	}
	return createThread(0, name, entry, initPriority, stackSize, attr, cpuAffinityMask,
			    option, attr >> 31, NULL);
}

/* SceThreadmgrForDriver_C8E57BB4: a user thread of process pid */
SceUID SceThreadmgrForDriver_C8E57BB4(SceUID pid, const char *name, uintptr_t entry,
				      int initPriority, SceSize stackSize, uint32_t attr,
				      uint32_t cpuAffinityMask,
				      const SceKernelThreadOptParam *option)
{
	SceUID ret;

	if (pid == THREADMGR_KERNEL_PID || pid == 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = ksceGUIDReferObjectWithClass(pid,
		s_threadmgrRuntime.manager->processClass, NULL);
	if (ret < 0)
		return ret;
	if (cpuAffinityMask & ~0xf000f) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	} else if ((cpuAffinityMask & 0xf0000) && (cpuAffinityMask & 0xf)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	} else {
		if (cpuAffinityMask & 0xf0000)
			cpuAffinityMask = (int)cpuAffinityMask >> 16;
		ret = createThread(pid, name, entry, initPriority, stackSize,
				   attr | THREAD_ATTR_USER, cpuAffinityMask, option,
				   CREATE_FLAG_PROCESS, NULL);
	}
	ksceGUIDReleaseObject(pid);
	return ret;
}
