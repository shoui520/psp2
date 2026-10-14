/*
 * SceKernelThreadMgr: callbacks (SceUIDCallbackObject). A callback belongs
 * to the thread that created it; notifying it bumps its count and wakes the
 * thread if it waits with callbacks enabled, and the thread runs its pending
 * callbacks (callCallbackAll) before it waits again. A callback runs on the
 * thread in a fresh register context (callCallbackEntry): kernel callbacks
 * on an extended stack, user callbacks on the thread's user stack, returning
 * through returnFromCallback. Callbacks can also be registered on objects of
 * SceUIDRegisterCallbackClass (events), which notify them when set.
 */

#include <string.h>
#include "timer.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/error.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "event.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "callback.c"

#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define THREAD_ATTR_USER		0x80000000
#define WAIT_TYPE_DELAY			0x00000001
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define THREADMGR_ERROR_CALLBACK_NOT_REGISTERED	0x800280a3	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define EVENT_CALLBACK_ALLOC_SIZE	0x20	/* a ThreadmgrEventCallback's heap block */

#define CALLBACK_STACK_MIN_FREE		0x708	/* the kernel stack a callback needs left */
#define EXT_STACK_MEMBLOCK_TYPE		0x1020d006	/* the extended stack cache's blocks */
#define EXT_STACK_SIZE			0x2000
#define EXT_STACK_GUARD			0x1000
#define EXT_STACK_MEMBLOCK_ATTR		0x40400008	/* PLACEHOLDER: SceKernelAllocMemBlockAttr bits */

#define SAVE_ALIGN			64	/* the context save areas' alignment */
#define SAVE_ALIGNED(buffer)		(((uintptr_t)(buffer) + SAVE_ALIGN - 1) & ~(SAVE_ALIGN - 1))
#define CALLBACK_VFP_SAVE_SIZE		0x48	/* d8-d15, FPSCR, FPEXC */

#define PSR_T				0x00000020
#define PSR_KERNEL_CALLBACK		0x0000005f	/* SYS mode, FIQ masked */
#define PSR_USER_CALLBACK		0x00000050	/* USR mode, FIQ masked */
#define CPACR_CP10_CP11_FULL		0x00f00000
#define VFP_EXCEPTION_KERNEL		0x00000087	/* PLACEHOLDER: the kernel callback's VFP exception mask */
#define USER_TLS_VFP_EXCEPTION		0x7f0	/* below the user TLS: the thread's VFP exception mask */

#define TRAP_REQUEST_UNK2000000		0x02000000	/* PLACEHOLDER: name */
#define INTR_EXCEPTION_NO_CALLBACK_STACK	0x200	/* PLACEHOLDER: SceIntrmgrForDriver_C568F145's code */
#define INTR_EXCEPTION_STACK_OVERFLOW	0x201	/* PLACEHOLDER: name */

#define TRACE_CALLBACK_START		9
#define TRACE_CALLBACK_NOTIFY		0xb
#define TRACE_CALLBACK_END		0xe

#define CALLBACK_CLASS()	(s_threadmgrRuntime.manager->callbackClass)
#define THREAD_CLASS()		(s_threadmgrRuntime.manager->threadClass)
#define CALLBACK_HEAD(thread)	((struct ThreadmgrCallbackCB *)&(thread)->callbackNext)
#define CALLBACK_FROM_CB(cb) \
	((struct ThreadmgrCallback *)((uintptr_t)(cb) - offsetof(struct ThreadmgrCallback, cb)))


_Static_assert(sizeof(ThreadmgrCallbackInfo) == 0x44, "SceKernelCallbackInfo size");

/* SceUIDRegisterCallbackObject (0x30): an object callbacks can be registered on */
typedef struct ThreadmgrRegisterCallback {
	uint8_t base[0x28];			/* the waitable object */
	ThreadmgrEventCallback *callbackNext;	/* 0x28 */
	ThreadmgrEventCallback *callbackPrev;
} ThreadmgrRegisterCallback;


extern void SceIntrmgrForKernel_08FDB0CE(struct ThreadRegisterContext *context, uint32_t flags,
					 uintptr_t vfp) __attribute__((noreturn));
extern int SceIntrmgrForKernel_71C5F42F(struct ThreadRegisterContext *context, uintptr_t vfp)
	__attribute__((returns_twice));
extern void SceIntrmgrForDriver_C568F145(uint32_t code);
extern uintptr_t SceSysrootForKernel_AD74BEB3(SceProcessObject *process);
extern int SceSysrootForKernel_7968F091(SceUID pid, void *ptr);
extern int SceThreadmgrForDriver_D74F66BD(void);
extern int ksceGUIDReferObjectWithSubclass(SceUID uid, SceClass *cls, SceObjectBase **object);
extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern const char *ksceGUIDName(SceUID guid);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern SceUID kscePUIDOpenByGUIDWithFlags(SceUID pid, SceUID guid, int flags);
extern int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				       SceSize *count);
extern int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level,
				       SceUID *vector, SceSize num, SceSize *count);
extern int PUIDGetPUIDVectorByGUID(SceUID pid, SceUID guid, SceUInt32 level, SceUID *vector,
				   SceSize num, SceSize *count);
extern int ksceKernelIsEqualAccessibleRangeProcBySW(SceUID pid, uint32_t access, uintptr_t addr,
						    SceSize size);
extern int ksceKernelIsIntrAllowedInCurrentContext(void);
extern int ksceKernelFreeHeapMemory(SceUID heap, void *entry);
extern int ksceKernelGetMemBlockMappedBase(SceUID uid, void **base);
int sceKernelIsUserModeThread(void);
int sceKernelCheckThreadUserStackSelf(void);
int sceKernelTrapThreadAfterSyscall(ThreadSyscallFrame *frame, uint32_t *result,
				    uint32_t request);
void exitDeleteThread(int status, int intr, int error) __attribute__((noreturn));
void initRegisterContext(struct ThreadRegisterContext *regs);
void initVfpContext(uintptr_t vfp);
void returnFromKernelCallbackEntry(int result) __attribute__((noreturn));
int uidRegisterCallbackClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
	SceClass *parent, SceSize size, SceClassCallback create, SceClassCallback destroy,
	ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate, ThreadmgrReleaseMethod release);

static inline int CallbackUnknownIdError(void)
{
	if (!ThreadmgrInSyscall())
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID;
	return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID | SYSCALL_ERROR_BIT;
}

static inline void unlinkCallback(struct ThreadmgrCallbackCB *cb)
{
	cb->next->prev = cb->prev;
	cb->prev->next = cb->next;
}

/*
 * The end of a callback: back to the context callCallbackEntry saved, which
 * returns from its context save a second time.
 */
void __attribute__((noreturn)) returnFromCallback(struct ThreadCB *thread)
{
	struct ThreadRegisterContext *regs;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 100);
	regs = thread->pRegisterContext;
	regs->callbackContext->pmuserenr = regs->pmuserenr;
	regs->callbackContext->pmcr = regs->pmcr;
	SceIntrmgrForKernel_08FDB0CE(regs->callbackContext, 0, regs->callbackVfp);
}

/* SceUIDCallbackClass's constructor */
static int uidCallback_Constructor(void *object)
{
	memset(&((struct ThreadmgrCallback *)object)->cb, 0, sizeof(struct ThreadmgrCallbackCB));
	return 0;
}

/* SceUIDRegisterCallbackClass's constructor */
static int uidRegisterCallback_Constructor(void *object)
{
	ThreadmgrRegisterCallback *reg = object;

	reg->callbackNext = (ThreadmgrEventCallback *)&reg->callbackNext;
	reg->callbackPrev = (ThreadmgrEventCallback *)&reg->callbackNext;
	return 0;
}

/* SceUIDCallbackClass's destructor: the callback leaves its process's counts */
int uidCallback_Destructor(struct ThreadmgrCallback *callback)
{
	SceProcessObject *process;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2068);
	intr = ksceKernelCpuSuspendIntr();
	pid = callback->ownerPid;
	if (ksceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != THREADMGR_KERNEL_PID) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (callback->cb.attr & THREAD_ATTR_UNLIMITED_OBJECTS)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->callbackCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		ksceGUIDReleaseObject(pid);
	}
	ksceKernelCpuResumeIntr(intr);
	memset(&callback->cb, 0, sizeof(callback->cb));
	return 0;
}

/* the information of a callback, read under its thread's lock */
int getCallbackInfo(SceUID callbackId, ThreadmgrCallbackInfo *info)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (ksceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto release;
	}
	info->callbackId = callbackId;
	strncpy(info->name, ksceGUIDName(callbackId), sizeof(info->name) - 1);
	ret = 0;
	ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	info->attr = callback->cb.attr;
	info->threadId = callback->cb.threadId;
	info->entry = callback->cb.entry;
	info->notifyId = callback->cb.notifyId;
	info->notifyCount = callback->cb.notifyCount;
	info->notifyArg = callback->cb.notifyArg;
	info->common = callback->cb.common;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	ksceGUIDReleaseObject(callback->cb.threadId);
release:
	ksceGUIDReleaseObject(callbackId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* where a kernel callback returns to: its result in waitResult */
void __attribute__((noreturn)) returnFromKernelCallback(int result)
{
	struct ThreadCB *thread;

	ksceKernelCpuSuspendIntr();
	thread = currentThreadContext();
	thread->waitResult = result;
	returnFromCallback(thread);
}

/* the user callback's return (from the process's return stub) */
void __attribute__((noreturn)) _sceKernelExitCallback(int result)
{
	struct ThreadCB *thread;
	int intr;

	intr = ksceKernelCpuSuspendIntr();
	thread = currentThreadContext();
	thread->waitResult = result;
	if (thread->pRegisterContext->callbackContext == NULL) {
		/* not in a callback */
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(0, 0, TRAP_REQUEST_UNK2000000);
		exitDeleteThread(0, intr, SCE_KERNEL_ERROR_ILLEGAL_CONTEXT);
	}
	returnFromCallback(thread);
}

/* a thread's callbacks go when it does */
void deleteAllCallback(struct ThreadCB *thread)
{
	struct ThreadmgrCallbackCB *cb, *next;
	SceUID pid = thread->processId;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 244);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	for (cb = thread->callbackNext; cb != CALLBACK_HEAD(thread); cb = next) {
		next = cb->next;
		cb->deleted = 1;
		unlinkCallback(cb);
		if (cb->puid == -1) {
			ret = ksceGUIDClose(cb->guid);
			SCE_DBG_ASSERT_CHECK(ret >= 0, 265);
		} else {
			ret = kscePUIDClose(pid, cb->puid);
			SCE_DBG_ASSERT_CHECK(ret >= 0, 262);
		}
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
}

/* a callback registration goes back to the heap */
void __attribute__((noinline)) deleteRegisteredCallbackList(ThreadmgrEventCallback *entry)
{
	if (ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, entry) < 0)
		SCE_PANIC(280);
}

/* SceUIDRegisterCallbackClass's destructor: its registrations go */
static int uidRegisterCallback_Destructor(void *object)
{
	ThreadmgrRegisterCallback *reg = object;
	ThreadmgrEventCallback *head = (ThreadmgrEventCallback *)&reg->callbackNext;
	ThreadmgrEventCallback *entry, *next;
	int intr;

	intr = ksceKernelCpuSuspendIntr();
	for (entry = reg->callbackNext; entry != head; entry = next) {
		next = entry->next;
		next->prev = entry->prev;
		entry->prev->next = next;
		deleteRegisteredCallbackList(entry);
	}
	ksceKernelCpuResumeIntr(intr);
	__builtin_memset(&reg->callbackNext, 0, 2 * sizeof(reg->callbackNext));
	return 0;
}

/*
 * Runs entry(arg0..arg3) on the thread, in user mode if user, and returns
 * when it has returned (through returnFromCallback, which resumes the
 * context saved here). The outermost callback of a kernel thread gets an
 * extended stack. Called and returns with the thread locked.
 */
int callCallbackEntry(struct ThreadCB *thread, int user, uintptr_t entry,
		      uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
	uint8_t contextBuffer[sizeof(struct ThreadRegisterContext) + SAVE_ALIGN - 4];
	uint8_t vfpBuffer[CALLBACK_VFP_SAVE_SIZE + SAVE_ALIGN - 4];
	SceKernelAllocMemBlockKernelOpt opt;
	struct ThreadRegisterContext *context, *regs;
	ThreadSyscallFrame *frame;
	volatile uint32_t *vfpException;
	uint32_t savedStatus, savedUnk30, savedVfpException, savedUnk100, type, request;
	ThreadSyscallFrame *savedUserFrame;
	uintptr_t vfp, base, stack, savedStackBase, userStack;
	SceSize savedStackSize, stackSize;
	SceUID savedStackMemBlockId, blockId;
	int savedPermission, cacheable, nest, ret, i;
	SceUID savedTlsProcessId;
	void *savedUnk7C;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 329);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 332);
	context = (struct ThreadRegisterContext *)SAVE_ALIGNED(contextBuffer);
	memset(context, 0, sizeof(*context));
	if (thread->pVfpRegisterContext != 0) {
		vfp = SAVE_ALIGNED(vfpBuffer) | 1;
		memset((void *)vfp, 0, CALLBACK_VFP_SAVE_SIZE);
	} else {
		vfp = 0;
	}
	savedStatus = thread->flags & THREAD_FLAG_CALLBACK_SAVED;
	savedUnk30 = thread->kernelStackTop;
	savedUserFrame = thread->userFrame;
	savedVfpException = thread->vfpException;
	savedPermission = thread->permission;
	savedTlsProcessId = thread->tlsProcessId;
	savedUnk100 = thread->ioFlags;
	savedStackBase = thread->pKStack;
	savedStackMemBlockId = thread->kStackMemblkId;
	savedStackSize = thread->kStackSize;
	savedUnk7C = thread->restartWaitMultipleEvents;
	nest = thread->callbackNest;
	if (nest == 1) {
		/* the outermost callback: a stack of its own */
		cacheable = 0;
		stackSize = savedStackSize;
		type = thread->stackMemBlockType;
		if ((thread->currentAttr & THREAD_ATTR_USER) ||
		    (type == EXT_STACK_MEMBLOCK_TYPE && stackSize == EXT_STACK_SIZE)) {
			stackSize = EXT_STACK_SIZE;
			cacheable = 1;
			blockId = popExtendedStackCache((void **)&base);
			if (blockId > 0)
				goto mapped;
			type = EXT_STACK_MEMBLOCK_TYPE;
		} else if (type == EXT_STACK_MEMBLOCK_TYPE && stackSize == 0x1000) {
			blockId = popExtendedStackCache((void **)&base);
			if (blockId > 0) {
				cacheable = 1;
				base += 0x1000;
				goto mapped;
			}
		}
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = EXT_STACK_MEMBLOCK_ATTR;
		opt.extraLow = EXT_STACK_GUARD;
		blockId = ksceKernelAllocMemBlock("SceExt", type, stackSize + opt.extraLow, &opt);
		if (blockId < 0) {
			SCE_DBG_PRINTF(0, 410,
				" k: error: sceKernelAllocMemBlockForKernel(%s, 0x%08X, 0x%08X)=0x%08X\n",
				"SceExt", type, stackSize + opt.extraLow, blockId);
			if (!(thread->currentAttr & THREAD_ATTR_USER))
				SCE_PANIC(415);
			SceIntrmgrForDriver_C568F145(INTR_EXCEPTION_NO_CALLBACK_STACK);
			return blockId;
		}
		ksceKernelGetMemBlockMappedBase(blockId, (void **)&base);
mapped:
		thread->pKStack = base;
		stack = base + stackSize;
		thread->kStackMemblkId = blockId;
		thread->kStackSize = stackSize;
	} else {
		SCE_DBG_ASSERT_PRINTF(nest != 0 || thread->threadEventHandler != NULL,
				      427, "pThreadCB->callbackNest=0x%08X\n", thread->callbackNest);
		blockId = -1;
		cacheable = 0;
		__asm__ volatile("mov %0, sp" : "=r"(stack));
	}

	if (SceIntrmgrForKernel_71C5F42F(context, vfp) != 0) {
		/* back from the callback */
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (thread->callbackNest == 1) {
			if (cacheable == 1) {
				if (thread->kStackSize == 0x1000)
					base -= 0x1000;
				if (pushExtendedStackCache(blockId, (void *)base) >= 0)
					goto restore;
			}
			ret = ksceKernelFreeMemBlock(blockId);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 453, " k: error: sceKernelFreeMemBlock(0x%08X)=0x%08X\n",
					       blockId, ret);
				SCE_PANIC(454);
			}
		}
restore:
		regs = thread->pRegisterContext;
		regs->callbackVfp = regs->callbackContext->callbackVfp;
		regs->callbackContext = regs->callbackContext->callbackContext;
		thread->kernelStackTop = savedUnk30;
		thread->userFrame = savedUserFrame;
		thread->vfpException = savedVfpException;
		thread->permission = savedPermission;
		thread->tlsProcessId = savedTlsProcessId;
		thread->ioFlags = savedUnk100;
		thread->pKStack = savedStackBase;
		thread->kStackMemblkId = savedStackMemBlockId;
		thread->kStackSize = savedStackSize;
		if (thread->restartWaitMultipleEvents != NULL) {
			SCE_DBG_ASSERT_CHECK((thread->flags & 0xc0000000) != 0, 472);
			SCE_DBG_ASSERT_CHECK(user == 1, 473);
			SceSysrootForKernel_7968F091(thread->processId, thread->restartWaitMultipleEvents);
		}
		thread->restartWaitMultipleEvents = savedUnk7C;
		thread->flags |= savedStatus;
		vfpException = currentVfpException();
		SCE_DBG_ASSERT_CHECK(vfpException != NULL, 480);
		*vfpException = thread->vfpException;
		__asm__ volatile("dsb sy" : : : "memory");
		if (user) {
			ret = ksceKernelCopyToUser((void *)(thread->tls - USER_TLS_VFP_EXCEPTION),
						   &thread->vfpException, sizeof(uint32_t));
			SCE_DBG_ASSERT_CHECK(ret == 0, 486);
		}
		return 0;
	}

	regs = thread->pRegisterContext;
	context->callbackContext = regs->callbackContext;
	context->callbackVfp = regs->callbackVfp;
	regs->callbackVfp = vfp;
	regs->callbackContext = context;
	thread->restartWaitMultipleEvents = NULL;
	thread->ioFlags = 0;
	thread->flags &= ~THREAD_FLAG_CALLBACK_SAVED;
	thread->permission = thread->initPermission;
	thread->tlsProcessId = thread->processId;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	if (user) {
		request = thread->flags & THREAD_FLAG_REQUEST_MASK;
		if (request == 0 && thread->debugSuspendReason != 0)
			request = TRAP_REQUEST_UNK2000000;
		if (request != 0) {
			sceKernelTrapThreadAfterSyscall(0, 0, request);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_RESUMED_WAIT);
		}
		frame = thread->userFrame;
		if (frame != NULL) {
			userStack = frame->sp;
			thread->pRegisterContext->tpidrurw = frame->tpidrurw;
		} else {
			userStack = context->callbackContext->sp;
			thread->pRegisterContext->tpidrurw = context->callbackContext->tpidrurw;
		}
		thread->pRegisterContext->syscallAux = (uintptr_t)frame;
		thread->kernelStackTop = stack;
		thread->userFrame = NULL;
		stack = userStack;
	}
	initRegisterContext(thread->pRegisterContext);
	regs = thread->pRegisterContext;
	regs->full = 1;
	regs->pc = entry & ~1;
	regs->r[0] = arg0;
	regs->r[1] = arg1;
	if (entry & 1)
		regs->cpsr = PSR_T;
	regs->r[2] = arg2;
	regs->r[3] = arg3;
	regs->sp = stack;
	if (!user) {
		regs->lr = (uintptr_t)returnFromKernelCallbackEntry;
		regs->cpsr |= PSR_KERNEL_CALLBACK;
		thread->vfpException = VFP_EXCEPTION_KERNEL;
	} else {
		regs->cpsr |= PSR_USER_CALLBACK;
		regs->lr = SceSysrootForKernel_AD74BEB3(thread->process);
		thread->pRegisterContext->tpidruro = thread->tls;
		thread->vfpException = 0;
		ret = ksceKernelCopyToUser((void *)(thread->tls - USER_TLS_VFP_EXCEPTION),
					   &thread->vfpException, sizeof(uint32_t));
		SCE_DBG_ASSERT_CHECK(ret == 0, 584);
	}
	thread->pRegisterContext->cpacr = CPACR_CP10_CP11_FULL;
	initVfpContext(thread->pVfpRegisterContext);
	regs = thread->pRegisterContext;
	regs->pmccntr = 0;
	for (i = 0; i < 6; i++) {
		regs->pmevent[i].eventType = 0;
		regs->pmevent[i].count = 0;
	}
	SceIntrmgrForKernel_08FDB0CE(thread->pRegisterContext, 0, thread->pVfpRegisterContext);
}

/*
 * Runs the thread's notified callbacks, each once per pass; a callback that
 * returns nonzero is deleted. Returns how many ran.
 */
int callCallbackAll(struct ThreadCB *thread)
{
	struct ThreadmgrCallbackCB *cb, *next;
	struct ThreadmgrCallback *outer;
	ThreadmgrTraceHook hook;
	uint32_t notifyId, count, arg;
	int ran, ret, err;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 640);
	if ((currentThreadContext()->currentAttr & THREAD_ATTR_USER) &&
	    !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_CALLBACKS);
	ran = 0;
	for (cb = thread->callbackNext; cb != CALLBACK_HEAD(thread); cb = next) {
		if (cb->deleted || (int)cb->notifyCount <= 0) {
			next = cb->next;
			continue;
		}
		if ((uint32_t)SceThreadmgrForDriver_D74F66BD() < CALLBACK_STACK_MIN_FREE) {
			/* no stack left to run it on */
			if (!(currentThreadContext()->currentAttr & THREAD_ATTR_USER))
				SCE_PANIC(678);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			SceIntrmgrForDriver_C568F145(INTR_EXCEPTION_STACK_OVERFLOW);
			sceKernelTrapThreadAfterSyscall(0, 0, TRAP_REQUEST_UNK2000000);
			exitDeleteThread(0, 0, SCE_KERNEL_ERROR_ILLEGAL_CONTEXT);
		}
		count = cb->notifyCount;
		arg = cb->notifyArg;
		cb->notifyCount = 0;
		cb->notifyArg = 0;
		outer = thread->currentCallback;
		notifyId = cb->notifyId;
		cb->notifyId = 0;
		thread->callbackNest++;
		thread->currentCallback = CALLBACK_FROM_CB(cb);
		cb->running++;
		hook = s_threadmgrRuntime.threadTraceHook;
		if (hook != NULL)
			hook(TRACE_CALLBACK_START, 8, thread->processId, thread->threadId, cb->guid,
			     cb->entry, notifyId, count, arg, cb->common);
		ret = callCallbackEntry(thread, cb->user, cb->entry, notifyId, count, arg,
					(uint32_t)cb->common);
		SCE_DBG_ASSERT_CHECK(thread->lock != 0, 717);
		hook = s_threadmgrRuntime.threadTraceHook;
		if (hook != NULL)
			hook(TRACE_CALLBACK_END, 3, thread->processId, thread->threadId,
			     thread->waitResult);
		ran++;
		next = cb->next;
		cb->running--;
		thread->callbackNest--;
		thread->currentCallback = outer;
		if (!cb->deleted && thread->waitResult != 0)
			cb->deleted = 1;
		if (cb->deleted && cb->running == 0) {
			/* deleted while it ran, or it asked to be */
			unlinkCallback(cb);
			if (cb->puid == -1) {
				err = ksceGUIDClose(cb->guid);
				SCE_DBG_ASSERT_CHECK(err >= 0, 744);
			} else {
				err = kscePUIDClose(thread->processId, cb->puid);
				SCE_DBG_ASSERT_CHECK(err >= 0, 741);
			}
		}
		if (ret < 0)
			break;
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return ran;
}

/* a process's callbacks go when it exits */
int callbackProcessExit(SceProcessObject *process)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	SceUID *vector, guid;
	SceSize got = 0;
	int num, i, intr, ret;

	num = SceSysmemForKernel_08C05493(process->pid, CALLBACK_CLASS(), 0x50000, NULL, 0, NULL);
	if (num < 0)
		return 0;
	if (num == 0) {
		vector = NULL;
	} else {
		do {
			vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
							   num * sizeof(SceUID));
			SCE_DBG_ASSERT_CHECK(vector != NULL, 788);
			num = SceSysmemForKernel_08C05493(process->pid, CALLBACK_CLASS(), 5, vector,
							  num, &got);
			if ((int)got >= num)
				break;
			ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		} while (num != 0);
	}
	for (i = 0; i < (int)got; i++) {
		intr = ksceKernelCpuSuspendIntr();
		guid = kscePUIDtoGUID(process->pid, vector[i]);
		if (guid < 0) {
			SCE_DBG_PRINTF(0xb, 810, "()->scePUIDtoGUID(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], guid);
			SCE_PANIC(811);
		}
		ret = ksceGUIDReferObjectWithClass(guid, CALLBACK_CLASS(),
						   (SceObjectBase **)&callback);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 816, "()->sceGUIDReferObjectWithClass(0x%08X)=0x%08X\n",
				       guid, ret);
			SCE_PANIC(817);
		}
		ret = ksceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
						   (SceObjectBase **)&object);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 822,
				"()->sceGUIDReferObjectWithClass(threadId:0x%08X)=0x%08X\n",
				callback->cb.threadId, ret);
			ksceGUIDReleaseObject(guid);
			ksceKernelCpuResumeIntr(intr);
			continue;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (callback->cb.deleted) {
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			ksceGUIDReleaseObject(callback->cb.threadId);
			ksceGUIDReleaseObject(guid);
			ksceKernelCpuResumeIntr(intr);
			continue;
		}
		callback->cb.deleted = 1;
		if (callback->cb.running == 0) {
			unlinkCallback(&callback->cb);
			ret = kscePUIDClose(process->pid, vector[i]);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 846,
					"()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
					process->pid, vector[i], ret);
				SCE_PANIC(847);
			}
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ret = ksceGUIDReleaseObject(callback->cb.threadId);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 856, "()->sceGUIDReleaseObject(threadId:0x%08X)=0x%08X\n",
				       callback->cb.threadId, ret);
			SCE_PANIC(857);
		}
		ret = ksceGUIDReleaseObject(guid);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 862, "()->sceGUIDReleaseObject(0x%08X)=0x%08X\n", guid,
				       ret);
			SCE_PANIC(863);
		}
		ksceKernelCpuResumeIntr(intr);
	}
	if (vector != NULL) {
		ret = ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 872);
	}
	return 0;
}

SceUID ksceKernelCreateCallback(const char *name, SceUInt32 attr, uintptr_t entry, void *common)
{
	UIDCreateOpt opt;
	struct ThreadCB *thread;
	SceProcessObject *process;
	struct ThreadmgrCallback *callback;
	uint32_t saved, access, limit;
	int user, intr, ret;
	SceUID uid;

	user = sceKernelIsUserModeThread();
	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 906, "sceKernelCreateCallback",
					   "*** error: illegal context\n");
		uid = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (attr != 0) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	access = user ? 0x50 : 5;
	ret = ksceKernelIsEqualAccessibleRangeProcBySW(thread->process->pid,
						       access, entry, 4);
	if (ret != 0) {
		SCE_DBG_PRINTF_FUNC(0, 928, "sceKernelCreateCallback",
			"*** error: sceKernelCreateCallback(%s) -> "
			"sceKernelIsEqualAccessibleRangeProcBySW(0x%08x, %08x, %p): error 0x%08X\n",
			name, thread->process->pid, access, (void *)entry,
			ret);
		uid = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	opt = (UIDCreateOpt){ .flags = UID_CREATE_OPT_ATTR };
	limit = 0;
	if (ThreadmgrInSyscall()) {
		opt.pid = thread->processId;
		if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
			limit = THREAD_ATTR_UNLIMITED_OBJECTS;
		opt.flags |= UID_CREATE_OPT_PROCESS_HEAP;
		opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
	}
	process = NULL;
#if FW_DEBUG
	if (!ThreadmgrInSyscall()) {
		intr = ksceKernelCpuSuspendIntr();
	} else {
		process = thread->process;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
#else
	intr = ksceKernelCpuSuspendIntr();
	if (ThreadmgrInSyscall()) {
		process = thread->process;
		ksceKernelSpinlockLowLock(&process->resourceLock);
#endif
		if (limit & THREAD_ATTR_UNLIMITED_OBJECTS) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF_FUNC(0xb, 966, "sceKernelCreateCallback",
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->callbackCount > process->callbackPeak)
			process->callbackPeak = process->callbackCount;
		ksceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = ksceGUIDKernelCreateWithOpt(CALLBACK_CLASS(), name, (SceGUIDKernelCreateOpt *)&opt,
					  (SceObjectBase **)&callback);
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 997, "sceKernelCreateCallback",
				    " error: sceGUIDKernelCreate(0x%08X)\n", uid);
		goto unaccount;
	}
	callback->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
	callback->cb.guid = uid;
	callback->cb.attr = limit;
	callback->cb.entry = entry;
	callback->cb.common = common;
	callback->cb.threadId = thread->threadId;
	callback->cb.user = user;
	callback->cb.running = 0;
	if (ThreadmgrInSyscall()) {
		callback->cb.puid = kscePUIDOpenByGUIDWithFlags(thread->processId, uid, 1);
		uid = callback->cb.puid;
		if (uid < 0) {
			SCE_DBG_ASSERT_LEVEL_FUNC(0, 0, 1016, "sceKernelCreateCallback");
			goto unaccount;
		}
	} else {
		callback->cb.puid = -1;
	}
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	callback->cb.next = CALLBACK_HEAD(thread);
	callback->cb.prev = thread->callbackPrev;
	thread->callbackPrev = &callback->cb;
	callback->cb.prev->next = &callback->cb;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	goto resume;
unaccount:
	if (ThreadmgrInSyscall()) {
		ksceKernelSpinlockLowLock(&process->resourceLock);
		if (limit & THREAD_ATTR_UNLIMITED_OBJECTS)
			process->uidUnlimitedCount--;
		else
			process->uidCount--;
		process->callbackCount--;
		ksceKernelSpinlockLowUnlock(&process->resourceLock);
	}
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int ksceKernelDeleteCallback(SceUID callbackId)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1067, "sceKernelDeleteCallback",
					   "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (ksceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto release;
	}
	ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (callback->cb.deleted) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		goto releaseThread;
	}
	callback->cb.deleted = 1;
	if (callback->cb.running == 0) {
		/* not running: it goes now, else when its last call ends */
		unlinkCallback(&callback->cb);
		if (callback->cb.puid == -1) {
			ret = ksceGUIDClose(callback->cb.guid);
			SCE_DBG_ASSERT_CHECK_FUNC(ret >= 0, 1123, "sceKernelDeleteCallback");
		} else {
			ret = kscePUIDClose(object->context.processId, callback->cb.puid);
			SCE_DBG_ASSERT_CHECK_FUNC(ret >= 0, 1115, "sceKernelDeleteCallback");
		}
		if (ret < 0) {
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			goto releaseThread;
		}
	}
	ret = 0;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
releaseThread:
	ksceGUIDReleaseObject(callback->cb.threadId);
release:
	ksceGUIDReleaseObject(callbackId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* counts a notification of the callback and wakes its thread if it waits with callbacks */
int ksceKernelNotifyCallback(SceUID callbackId, int arg)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrWaitNode *self;
	ThreadmgrEvent *waitObject;
	ThreadmgrTraceHook hook;
	SceUID waitId;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (ksceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ksceGUIDReleaseObject(callbackId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	for (;;) {
		waitObject = NULL;
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		callback->cb.notifyId = -1;
		callback->cb.notifyCount++;
		callback->cb.notifyArg = arg;
		if (!(object->context.status & SCE_KERNEL_THREAD_STATUS_WAITING) ||
		    !(object->context.flags & THREAD_FLAG_WAKE_PENDING)) {
			waitId = -1;
			wake = 0;
			ksceKernelAtomicOrAndGet32((SceInt32 *)&object->context.flags, THREAD_FLAG_CALLBACKS);
			break;
		}
		if (object->context.waitType == WAIT_TYPE_DELAY) {
			/* a delay: the thread leaves the timed waits */
			ksceKernelAtomicOrAndGet32((SceInt32 *)&object->context.flags, THREAD_FLAG_CALLBACKS);
			waitId = -1;
			self = (struct ThreadmgrWaitNode *)&object->context;
			ksceKernelSpinlockLowLock(
				&s_threadmgrRuntime.manager->timedWaitLock);
			self->next->prev = self->prev;
			self->prev->next = self->next;
			ksceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->timedWaitLock);
			releaseThreadWaitState(&object->context, 0, THREADMGR_ERROR_WAIT_CANCEL);
			wake = distributeThread(selectExecCpu(&object->context), &object->context);
			break;
		}
		waitId = object->context.waitId;
		ret = ksceGUIDReferObjectWithSubclass(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&waitObject);
		if (ret < 0) {
			SCE_DBG_ASSERT_PRINTF_FUNC(waitId == -1 || waitId == WAIT_ID_MULTIPLE_EVENTS,
				1262, "sceKernelNotifyCallback",
				"*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X\n",
				waitId, ret);
			waitObject = NULL;
		} else if (ksceKernelSpinlockLowTrylock(&waitObject->lock) < 0) {
			/* lock order: drop the thread and start over */
			callback->cb.notifyCount--;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			ksceGUIDReleaseObject(waitId);
			continue;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&object->context.flags, THREAD_FLAG_CALLBACKS);
		wake = wakeupAndDistributeThread(&object->context, EVENT_WAIT_QUEUE(waitObject),
						 THREADMGR_ERROR_WAIT_CANCEL);
		break;
	}
	hook = s_threadmgrRuntime.threadTraceHook;
	if (hook != NULL)
		hook(TRACE_CALLBACK_NOTIFY, 3, callbackId, callback->cb.notifyCount, arg);
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	if (waitObject != NULL) {
		wake |= revalidateWaitState(EVENT_WAIT_QUEUE(waitObject));
		ksceKernelSpinlockLowUnlock(&waitObject->lock);
	}
	ksceGUIDReleaseObject(callback->cb.threadId);
	if (waitObject != NULL)
		ksceGUIDReleaseObject(waitId);
	ksceGUIDReleaseObject(callbackId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ret = 0;
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* runs the current thread's pending callbacks; returns how many ran */
int sceKernelCheckCallback(void)
{
	struct ThreadCB *thread;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 1328, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else {
		intr = ksceKernelCpuSuspendIntr();
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		if (currentCpuState()->disabledDispatch == 0) {
			thread = currentThreadContext();
			ret = 0;
			if (thread->flags & THREAD_FLAG_CALLBACKS)
				ret = callCallbackAll(thread);
		}
		ksceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelCancelCallback(SceUID callbackId)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (ksceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		ret = 0;
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		callback->cb.notifyId = 0;
		callback->cb.notifyCount = 0;
		callback->cb.notifyArg = 0;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceGUIDReleaseObject(callback->cb.threadId);
	}
	ksceGUIDReleaseObject(callbackId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelGetCallbackCount(SceUID callbackId)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (ksceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		ret = callback->cb.notifyCount;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceGUIDReleaseObject(callback->cb.threadId);
	}
	ksceGUIDReleaseObject(callbackId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_FDC044AC: ksceKernelGetCallbackInfo; a syscall sees its own ids */
int SceThreadmgrForDriver_FDC044AC(SceUID callbackId, ThreadmgrCallbackInfo *info)
{
	ThreadmgrCallbackInfo local;
	struct ThreadmgrThreadObject *object;
	SceUID threadId, puid;
	SceSize size, got;
	uint32_t saved;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getCallbackInfo(callbackId, &local);
		if (ret >= 0) {
			if (ThreadmgrInSyscall()) {
				threadId = local.threadId;
				if (ksceGUIDReferObjectWithClass(threadId, THREAD_CLASS(),
						(SceObjectBase **)&object) < 0) {
					local.threadId = -1;
				} else {
					local.threadId = object->context.userThreadId;
					ksceGUIDReleaseObject(threadId);
				}
				if (local.notifyId != 0) {
					if (PUIDGetPUIDVectorByGUID(0, local.notifyId, 3, &puid, 4,
								    &got) < 0 || got == 0)
						local.notifyId = -1;
					else
						local.notifyId = puid;
				}
			}
			size = info->size;
			if (size > sizeof(local))
				size = sizeof(local);
			memcpy(info, &local, size);
		}
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_767CA30C: the information of every callback visible at level */
int SceThreadmgrForDriver_767CA30C(int level, ThreadmgrCallbackInfo *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrCallbackInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(CALLBACK_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(CALLBACK_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getCallbackInfo(vector[i], &info) != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* SceThreadmgrForDriver_FF8DA217: the same for a process's callback PUIDs, in its ids */
int SceThreadmgrForDriver_FF8DA217(SceUID pid, int level, ThreadmgrCallbackInfo *infos,
				   SceSize size, SceSize *count)
{
	ThreadmgrCallbackInfo info;
	struct ThreadmgrThreadObject *object;
	SceUID *vector, threadId, puid;
	SceSize got = 0, found;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid, CALLBACK_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid, CALLBACK_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getCallbackInfo(kscePUIDtoGUID(pid, vector[i]), &info) != 0)
			continue;
		info.callbackId = vector[i];
		threadId = info.threadId;
		if (ksceGUIDReferObjectWithClass(threadId, THREAD_CLASS(),
						 (SceObjectBase **)&object) < 0) {
			info.threadId = -1;
		} else {
			info.threadId = object->context.userThreadId;
			ksceGUIDReleaseObject(threadId);
		}
		if ((int)info.notifyId > 0 &&
		    (PUIDGetPUIDVectorByGUID(pid, info.notifyId, level, &puid, 4, &found) < 0 ||
		     (info.notifyId = puid, found == 0)))
			info.notifyId = -1;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* registers the callback on the object (an event): notified with notifyId when it is set */
int ksceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId, uint32_t notifyId)
{
	ThreadmgrEventCallback *entry, *pos, *head;
	struct ThreadmgrCallback *callback;
	ThreadmgrEvent *event;
	SceAllocOpt opt;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1842, "sceKernelRegisterCallbackToEventForDriver",
				"*** error: unknown callback id. callbackId=0x%08x\n", callbackId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID;
		}
		goto resume;
	}
	if (ksceGUIDReferObjectWithSubclass(eventId,
			s_threadmgrRuntime.manager->registerCallbackClass,
			(SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1855, "sceKernelRegisterCallbackToEventForDriver",
				"*** error: unknown event id. eventId=0x%08x\n", eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto release;
	}
	ksceKernelSpinlockLowLock(&event->lock);
	opt.size = 0x14;
	opt.align = 0x40;
	entry = ksceKernelAllocHeapMemoryWithOption(s_threadmgrRuntime.manager->heapId,
						    EVENT_CALLBACK_ALLOC_SIZE, &opt);
	if (entry == NULL) {
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
	} else {
		entry->callbackId = callbackId;
		entry->notifyId = notifyId;
		entry->threadId = callback->cb.threadId;
		entry->notifiedCpus = 0;
		/* in callback id order */
		head = EVENT_CALLBACK_HEAD(event);
		for (pos = head->next; pos != head && (uint32_t)pos->callbackId <= (uint32_t)callbackId;
		     pos = pos->next)
			;
		entry->next = pos;
		entry->prev = pos->prev;
		pos->prev = entry;
		entry->prev->next = entry;
		ret = 0;
	}
	ksceKernelSpinlockLowUnlock(&event->lock);
	ksceGUIDReleaseObject(eventId);
release:
	ksceGUIDReleaseObject(callbackId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId)
{
	ThreadmgrEventCallback *entry, *head;
	ThreadmgrEvent *event;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithSubclass(eventId,
			s_threadmgrRuntime.manager->registerCallbackClass,
			(SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1924, "sceKernelUnregisterCallbackFromEvent",
				"*** error: unknown event id. eventId=0x%08x\n", eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&event->lock);
	ret = THREADMGR_ERROR_CALLBACK_NOT_REGISTERED;
	head = EVENT_CALLBACK_HEAD(event);
	for (entry = head->next; entry != head; entry = entry->next) {
		if (entry->callbackId == callbackId) {
			ret = 0;
			entry->next->prev = entry->prev;
			entry->prev->next = entry->next;
			deleteRegisteredCallbackList(entry);
			break;
		}
	}
	ksceKernelSpinlockLowUnlock(&event->lock);
	ksceGUIDReleaseObject(eventId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelUnregisterCallbackFromEventAll(SceUID eventId)
{
	ThreadmgrEventCallback *entry, *head;
	ThreadmgrEvent *event;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithSubclass(eventId,
			s_threadmgrRuntime.manager->registerCallbackClass,
			(SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1981, "sceKernelUnregisterCallbackFromEventAll",
				"*** error: unknown event id. eventId=0x%08x\n", eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&event->lock);
	head = EVENT_CALLBACK_HEAD(event);
	for (entry = head->next; entry != head; entry = entry->next) {
		entry->next->prev = entry->prev;
		entry->prev->next = entry->next;
		deleteRegisteredCallbackList(entry);
	}
	ret = 0;
	ksceKernelSpinlockLowUnlock(&event->lock);
	ksceGUIDReleaseObject(eventId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* a class whose objects callbacks can be registered on (events derive from it) */
int __attribute__((noinline)) uidRegisterCallbackClassInit(struct ThreadmgrWaitableClass *cls,
	const char *name, SceClass *parent, SceSize size, SceClassCallback create,
	SceClassCallback destroy, ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate, ThreadmgrReleaseMethod release)
{
	int ret;

	ret = uidExtendedClassInit(cls, name, parent, size, create, destroy, cancelWait,
				   revalidate, release);
	if (ret < 0)
		SCE_PANIC(SCE_LINE(2130, 2121));
	return ret;
}

/* SceUIDCallbackClass and SceUIDRegisterCallbackClass */
void startCallback(void)
{
	struct ThreadmgrWaitableClass *registerClass;
	SceClass *cls;

	cls = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, 0x2c);
	if (cls == NULL)
		SCE_PANIC(184);
	if (ksceUIDClassInitClass(cls, "SceUIDCallbackClass", ksceKernelGetUIDClass(), 0x80,
				  uidCallback_Constructor,
				  (SceClassCallback)uidCallback_Destructor) < 0)
		SCE_PANIC_FUNC(SCE_LINE(2040, 2035), "uidCallbackClassInit");
	s_threadmgrRuntime.manager->callbackClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 197, " sizeof(SceUIDCallbackObject)=%d\n",
			     sizeof(struct ThreadmgrCallback));
	registerClass = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						  sizeof(*registerClass));
	if (registerClass == NULL)
		SCE_PANIC(202);
	if (uidRegisterCallbackClassInit(registerClass, "SceUIDRegisterCallbackClass",
			s_threadmgrRuntime.manager->waitableClass,
			sizeof(ThreadmgrRegisterCallback), uidRegisterCallback_Constructor,
			uidRegisterCallback_Destructor, uidDefaultCancelWait,
			uidDefault_RevalidateMethod,
			uidDefault_ReleaseMethod) < 0)
		SCE_PANIC(215);
	s_threadmgrRuntime.manager->registerCallbackClass = (SceClass *)registerClass;
	SCE_DBG_PRINTF_LEVEL(2, 0, 218, " sizeof(SceUIDRegisterCallbackObject)=%d\n",
			     sizeof(ThreadmgrRegisterCallback));
}
