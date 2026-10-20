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
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "event.h"
#include "syscall_context.h"
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>

#define EVENT_CALLBACK_ALLOC_SIZE	0x20	/* a ThreadmgrEventCallback's heap block */

#define CALLBACK_STACK_MIN_FREE		0x708	/* the kernel stack a callback needs left */
#define EXT_STACK_MEMBLOCK_TYPE		0x1020d006	/* the extended stack cache's blocks */
#define EXT_STACK_SIZE			0x2000
#define EXT_STACK_GUARD			0x1000
#define EXT_STACK_MEMBLOCK_ATTR		0x40400008	/* PLACEHOLDER: SceKernelAllocMemBlockAttr bits */

#define CALLBACK_VFP_SAVE_SIZE		0x48	/* d8-d15, FPSCR, FPEXC */

#define INTR_EXCEPTION_NO_CALLBACK_STACK	0x200	/* PLACEHOLDER: SceIntrmgrForDriver_C568F145's code */
#define INTR_EXCEPTION_STACK_OVERFLOW	0x201	/* PLACEHOLDER: name */

#define TRACE_CALLBACK_START		9
#define TRACE_CALLBACK_NOTIFY		0xb
#define TRACE_CALLBACK_END		0xe

#define CALLBACK_CLASS()	(s_threadmgrRuntime.manager->callbackClass)
#define CALLBACK_HEAD(thread)	((struct ThreadmgrCallbackCB *)&(thread)->callbackNext)
#define CALLBACK_FROM_CB(cb) \
	((struct ThreadmgrCallback *)((uintptr_t)(cb) - offsetof(struct ThreadmgrCallback, cb)))

/* SceUIDRegisterCallbackObject (0x30): an object callbacks can be registered on */
typedef struct ThreadmgrRegisterCallback {
	uint8_t base[0x28];			/* the waitable object */
	ThreadmgrEventCallback *callbackNext;	/* 0x28 */
	ThreadmgrEventCallback *callbackPrev;
} ThreadmgrRegisterCallback;

void exitDeleteThread(int status, int intr, int error) __attribute__((noreturn));
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
	ThreadmgrIntWord(ThreadmgrCallbackContext(regs)->pmuserenr) = ThreadmgrIntWord(regs->pmuserenr);
	ThreadmgrIntWord(ThreadmgrCallbackContext(regs)->pmcr) = ThreadmgrIntWord(regs->pmcr);
	SceIntrmgrForKernel_08FDB0CE(ThreadmgrCallbackContext(regs), 0, regs->callbackVfp);
	__builtin_unreachable();
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
	intr = SceCpuForDriver_821FC0EE();
	pid = callback->ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != SCE_KERNEL_PROCESS_ID) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (callback->cb.attr & THREAD_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->callbackCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	sceKernelCpuResumeIntr(intr);
	memset(&callback->cb, 0, sizeof(callback->cb));
	return 0;
}

/* the information of a callback, read under its thread's lock */
int getCallbackInfo(SceUID callbackId, SceKernelCallbackInfo *info)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	int intr, ret;

	*info = (SceKernelCallbackInfo){ .size = sizeof(*info) };
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (sceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto release;
	}
	info->callbackId = callbackId;
	strncpy(info->name, SceSysmemForDriver_E655852F(callbackId), sizeof(info->name) - 1);
	ret = 0;
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	info->attr = callback->cb.attr;
	info->threadId = callback->cb.threadId;
	info->callbackFunc = (SceKernelCallbackFunction)callback->cb.entry;
	info->notifyId = callback->cb.notifyId;
	info->notifyCount = callback->cb.notifyCount;
	info->notifyArg = callback->cb.notifyArg;
	info->pCommon = callback->cb.common;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(callback->cb.threadId);
release:
	sceGUIDReleaseObject(callbackId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* where a kernel callback returns to: its result in waitResult */
void __attribute__((noreturn)) returnFromKernelCallback(int result)
{
	struct ThreadCB *thread;

	SceCpuForDriver_821FC0EE();
	thread = currentThreadContext();
	thread->waitResult = result;
	returnFromCallback(thread);
}

/* the user callback's return (from the process's return stub) */
void __attribute__((noreturn)) _sceKernelExitCallback(int result)
{
	struct ThreadCB *thread;
	int intr;

	intr = SceCpuForDriver_821FC0EE();
	thread = currentThreadContext();
	thread->waitResult = result;
	if (__builtin_expect(thread->pRegisterContext->callbackContext == NULL, 1)) {
		/* not in a callback */
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(0, 0, TRAP_REQUEST_DEBUG_SUSPEND);
		exitDeleteThread(0, intr, SCE_KERNEL_ERROR_ILLEGAL_CONTEXT);
	}
	returnFromCallback(thread);
}

/* a thread's callbacks go when it does */
void deleteAllCallback(struct ThreadCB *thread)
{
	struct ThreadmgrCallbackCB *cb, *next;
	SceUID pid = thread->processId;
	SceUID puid, guid;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 244);
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	for (cb = thread->callbackNext; cb != CALLBACK_HEAD(thread); cb = next) {
		next = cb->next;
		puid = cb->puid;
		cb->deleted = 1;
		cb->next->prev = cb->prev;
		guid = cb->guid;
		cb->prev->next = cb->next;
		if (puid == -1) {
			ret = sceGUIDClose(guid);
			SCE_DBG_ASSERT_CHECK(ret >= 0, 265);
		} else {
			ret = SceSysmemForDriver_84A4AF5E(pid, puid);
			SCE_DBG_ASSERT_CHECK(ret >= 0, 262);
		}
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
}

/* a callback registration goes back to the heap */
void __attribute__((noinline)) deleteRegisteredCallbackList(ThreadmgrEventCallback *entry)
{
	if (sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, entry) < 0)
		SCE_PANIC(280);
}

/* SceUIDRegisterCallbackClass's destructor: its registrations go */
static int uidRegisterCallback_Destructor(void *object)
{
	ThreadmgrRegisterCallback *reg = object;
	ThreadmgrEventCallback *head = (ThreadmgrEventCallback *)&reg->callbackNext;
	ThreadmgrEventCallback *entry, *next;
	int intr;

	intr = SceCpuForDriver_821FC0EE();
	for (entry = reg->callbackNext; entry != head; entry = next) {
		next = entry->next;
		next->prev = entry->prev;
		entry->prev->next = next;
		deleteRegisteredCallbackList(entry);
	}
	sceKernelCpuResumeIntr(intr);
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
		blockId = sceKernelAllocMemBlockForKernel("SceExt", type, stackSize + opt.extraLow, &opt);
		if (blockId < 0) {
			SCE_DBG_PRINTF(0, 410,
				" k: error: sceKernelAllocMemBlockForKernel(%s, 0x%08X, 0x%08X)=0x%08X\n",
				"SceExt", type, stackSize + opt.extraLow, blockId);
			if (!(thread->currentAttr & THREAD_ATTR_USER))
				SCE_PANIC(415);
			SceIntrmgrForDriver_C568F145(INTR_EXCEPTION_NO_CALLBACK_STACK);
			return blockId;
		}
		sceKernelGetMemBlockMappedBase(blockId, (void **)&base);
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
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (thread->callbackNest == 1) {
			if (cacheable == 1) {
				if (thread->kStackSize == 0x1000)
					base -= 0x1000;
				if (pushExtendedStackCache(blockId, (void *)base) >= 0)
					goto restore;
			}
			ret = sceKernelFreeMemBlock(blockId);
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
			ret = sceKernelCopyToUser((void *)(thread->tls - USER_TLS_VFP_EXCEPTION),
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
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	if (user) {
		request = thread->flags & THREAD_FLAG_REQUEST_MASK;
		if (request == 0 && thread->debugSuspendReason != 0)
			request = TRAP_REQUEST_DEBUG_SUSPEND;
		if (request != 0) {
			sceKernelTrapThreadAfterSyscall(0, 0, request);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_RESUMED_WAIT);
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
		regs->lr = (uint32_t)SceSysrootForKernel_AD74BEB3(thread->process);
		thread->pRegisterContext->tpidruro = thread->tls;
		thread->vfpException = 0;
		ret = sceKernelCopyToUser((void *)(thread->tls - USER_TLS_VFP_EXCEPTION),
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
	__builtin_unreachable();
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
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_CALLBACKS);
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
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			SceIntrmgrForDriver_C568F145(INTR_EXCEPTION_STACK_OVERFLOW);
			sceKernelTrapThreadAfterSyscall(0, 0, TRAP_REQUEST_DEBUG_SUSPEND);
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
		if (__builtin_expect(hook != NULL, 0))
			hook(TRACE_CALLBACK_START, 8, thread->processId, thread->threadId, cb->guid,
			     cb->entry, notifyId, count, arg, cb->common);
		ret = callCallbackEntry(thread, cb->user, cb->entry, notifyId, count, arg,
					(uint32_t)cb->common);
		SCE_DBG_ASSERT_CHECK(thread->lock != 0, 717);
		hook = s_threadmgrRuntime.threadTraceHook;
		if (__builtin_expect(hook != NULL, 0))
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
				err = sceGUIDClose(cb->guid);
				SCE_DBG_ASSERT_CHECK(err >= 0, 744);
			} else {
				err = SceSysmemForDriver_84A4AF5E(thread->processId, cb->puid);
				SCE_DBG_ASSERT_CHECK(err >= 0, 741);
			}
		}
		if (ret < 0)
			break;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return ran;
}

/* a process's callbacks go when it exits */
int callbackProcessExit(SceProcessObject *process)
{
	SceSize got = 0;
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	SceUID *vector, guid;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid, CALLBACK_CLASS(), 0x50000, NULL, 0, NULL);
	if (num < 0)
		return 0;
	if (num == 0) {
		vector = NULL;
	} else {
		do {
			vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
							   num * sizeof(SceUID));
			SCE_DBG_ASSERT_CHECK(vector != NULL, 788);
			num = scePUIDGetUIDVectorByClass(process->pid, CALLBACK_CLASS(), 5, vector,
							  num, &got);
			if ((int)got >= num)
				break;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		} while (num != 0);
	}
	for (i = 0; i < (int)got; i++) {
		intr = SceCpuForDriver_821FC0EE();
		guid = SceSysmemForDriver_45D22597(process->pid, vector[i]);
		if (guid < 0) {
			SCE_DBG_PRINTF(0xb, 810, "()->scePUIDtoGUID(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], guid);
			SCE_PANIC(811);
		}
		ret = sceGUIDReferObjectWithClass(guid, CALLBACK_CLASS(),
						   (SceObjectBase **)&callback);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 816, "()->sceGUIDReferObjectWithClass(0x%08X)=0x%08X\n",
				       guid, ret);
			SCE_PANIC(817);
		}
		ret = sceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
						   (SceObjectBase **)&object);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 822,
				"()->sceGUIDReferObjectWithClass(threadId:0x%08X)=0x%08X\n",
				callback->cb.threadId, ret);
			sceGUIDReleaseObject(guid);
			sceKernelCpuResumeIntr(intr);
			continue;
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (callback->cb.deleted) {
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(callback->cb.threadId);
			sceGUIDReleaseObject(guid);
			sceKernelCpuResumeIntr(intr);
			continue;
		}
		callback->cb.deleted = 1;
		if (callback->cb.running == 0) {
			unlinkCallback(&callback->cb);
			ret = SceSysmemForDriver_84A4AF5E(process->pid, vector[i]);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 846,
					"()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
					process->pid, vector[i], ret);
				SCE_PANIC(847);
			}
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ret = sceGUIDReleaseObject(callback->cb.threadId);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 856, "()->sceGUIDReleaseObject(threadId:0x%08X)=0x%08X\n",
				       callback->cb.threadId, ret);
			SCE_PANIC(857);
		}
		ret = sceGUIDReleaseObject(guid);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 862, "()->sceGUIDReleaseObject(0x%08X)=0x%08X\n", guid,
				       ret);
			SCE_PANIC(863);
		}
		sceKernelCpuResumeIntr(intr);
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 872);
	}
	return 0;
}

SceUID sceKernelCreateCallback(const char *name, SceUInt32 attr, SceKernelCallbackFunction entry,
			       void *common)
{
	UIDCreateOpt opt;
	struct ThreadCB *thread;
	SceProcessObject *process;
	struct ThreadmgrCallback *callback;
	SceObjectBase *pObject;
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
	ret = sceKernelIsEqualAccessibleRangeProcBySW(thread->process->pid,
						       access, (const void *)entry, 4);
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
		if (thread->currentAttr & THREAD_ATTR_UNLIMITED)
			limit = THREAD_ATTR_UNLIMITED;
		opt.flags |= UID_CREATE_OPT_PROCESS_HEAP;
		opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED) != 0;
	}
	process = NULL;
#if FW_DEBUG
	if (!ThreadmgrInSyscall()) {
		intr = SceCpuForDriver_821FC0EE();
	} else {
		process = thread->process;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
#else
	intr = SceCpuForDriver_821FC0EE();
	if (ThreadmgrInSyscall()) {
		process = thread->process;
		sceKernelSpinlockLowLock(&process->resourceLock);
#endif
		if (limit & THREAD_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
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
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = sceGUIDKernelCreateWithOpt(CALLBACK_CLASS(), name, &opt,
					  &pObject);
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 997, "sceKernelCreateCallback",
				    " error: sceGUIDKernelCreate(0x%08X)\n", uid);
		goto unaccount;
	}
	callback = (struct ThreadmgrCallback *)pObject;
	callback->ownerPid = ThreadmgrInSyscall() ? thread->processId : SCE_KERNEL_PROCESS_ID;
	callback->cb.guid = uid;
	callback->cb.attr = limit;
	callback->cb.entry = (uintptr_t)entry;
	callback->cb.common = common;
	callback->cb.threadId = thread->threadId;
	callback->cb.user = user;
	callback->cb.running = 0;
	if (ThreadmgrInSyscall()) {
		callback->cb.puid = scePUIDOpenByGUIDWithFlags(thread->processId, uid, 1);
		uid = ((struct ThreadmgrCallback *)pObject)->cb.puid;
		if (uid < 0) {
			SCE_DBG_ASSERT_LEVEL_FUNC(0, 0, 1016, "sceKernelCreateCallback");
			goto unaccount;
		}
	} else {
		callback->cb.puid = -1;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	callback = (struct ThreadmgrCallback *)pObject;
	callback->cb.next = CALLBACK_HEAD(thread);
	callback->cb.prev = CALLBACK_HEAD(thread)->prev;
	CALLBACK_HEAD(thread)->prev = &callback->cb;
	callback->cb.prev->next = &callback->cb;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	goto resume;
unaccount:
	if (ThreadmgrInSyscall()) {
		sceKernelSpinlockLowLock(&process->resourceLock);
		if (limit & THREAD_ATTR_UNLIMITED)
			process->uidUnlimitedCount--;
		else
			process->uidCount--;
		process->callbackCount--;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int sceKernelDeleteCallback(SceUID callbackId)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1067, "sceKernelDeleteCallback",
					   "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (sceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto release;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (callback->cb.deleted) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		goto releaseThread;
	}
	callback->cb.deleted = 1;
	if (callback->cb.running == 0) {
		/* not running: it goes now, else when its last call ends */
		unlinkCallback(&callback->cb);
		if (callback->cb.puid == -1) {
			ret = sceGUIDClose(callback->cb.guid);
			SCE_DBG_ASSERT_CHECK_FUNC(ret >= 0, 1123, "sceKernelDeleteCallback");
		} else {
			ret = SceSysmemForDriver_84A4AF5E(object->context.processId, callback->cb.puid);
			SCE_DBG_ASSERT_CHECK_FUNC(ret >= 0, 1115, "sceKernelDeleteCallback");
		}
		if (ret < 0) {
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			goto releaseThread;
		}
	}
	ret = 0;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
releaseThread:
	sceGUIDReleaseObject(callback->cb.threadId);
release:
	sceGUIDReleaseObject(callbackId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* counts a notification of the callback and wakes its thread if it waits with callbacks */
int sceKernelNotifyCallback(SceUID callbackId, int arg)
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
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (sceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		sceGUIDReleaseObject(callbackId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	for (;;) {
		waitObject = NULL;
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		callback->cb.notifyId = -1;
		callback->cb.notifyCount++;
		callback->cb.notifyArg = arg;
		if (!(object->context.status & SCE_KERNEL_THREAD_STATUS_WAITING) ||
		    !(object->context.flags & THREAD_FLAG_WAKE_PENDING)) {
			waitId = -1;
			wake = 0;
			SceCpuForDriver_BC248C30((SceInt32 *)&object->context.flags, THREAD_FLAG_CALLBACKS);
			break;
		}
		if (object->context.waitType == SCE_KERNEL_WAITTYPE_DELAY) {
			/* a delay: the thread leaves the timed waits */
			SceCpuForDriver_BC248C30((SceInt32 *)&object->context.flags, THREAD_FLAG_CALLBACKS);
			waitId = -1;
			self = (struct ThreadmgrWaitNode *)&object->context;
			sceKernelSpinlockLowLock(
				&s_threadmgrRuntime.manager->timedWaitLock);
			self->next->prev = self->prev;
			self->prev->next = self->next;
			sceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->timedWaitLock);
			releaseThreadWaitState(&object->context, 0, SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
			wake = distributeThread(selectExecCpu(&object->context), &object->context);
			break;
		}
		waitId = object->context.waitId;
		ret = sceGUIDReferObjectWithSubclass(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&waitObject);
		if (ret < 0) {
			SCE_DBG_ASSERT_PRINTF_FUNC(waitId == -1 || waitId == WAIT_ID_MULTIPLE_EVENTS,
				1262, "sceKernelNotifyCallback",
				"*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X\n",
				waitId, ret);
			waitObject = NULL;
		} else if (SceCpuForDriver_5AC9D394(&waitObject->lock) < 0) {
			/* lock order: drop the thread and start over */
			callback->cb.notifyCount--;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(waitId);
			continue;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&object->context.flags, THREAD_FLAG_CALLBACKS);
		wake = wakeupAndDistributeThread(&object->context, EVENT_WAIT_QUEUE(waitObject),
						 SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
		break;
	}
	hook = s_threadmgrRuntime.threadTraceHook;
	if (hook != NULL)
		hook(TRACE_CALLBACK_NOTIFY, 3, callbackId, callback->cb.notifyCount, arg);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	if (waitObject != NULL) {
		wake |= revalidateWaitState(EVENT_WAIT_QUEUE(waitObject));
		sceKernelSpinlockLowUnlock(&waitObject->lock);
	}
	sceGUIDReleaseObject(callback->cb.threadId);
	if (waitObject != NULL)
		sceGUIDReleaseObject(waitId);
	sceGUIDReleaseObject(callbackId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
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
		intr = SceCpuForDriver_821FC0EE();
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		if (currentCpuState()->disabledDispatch == 0) {
			thread = currentThreadContext();
			ret = 0;
			if (thread->flags & THREAD_FLAG_CALLBACKS)
				ret = callCallbackAll(thread);
		}
		sceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_C040EC1C(SceUID callbackId)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (sceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		ret = 0;
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		callback->cb.notifyId = 0;
		callback->cb.notifyCount = 0;
		callback->cb.notifyArg = 0;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(callback->cb.threadId);
	}
	sceGUIDReleaseObject(callbackId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_0892D8DF(SceUID callbackId)
{
	struct ThreadmgrCallback *callback;
	struct ThreadmgrThreadObject *object;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
					 (SceObjectBase **)&callback) < 0) {
		ret = CallbackUnknownIdError();
		goto resume;
	}
	if (sceGUIDReferObjectWithClass(callback->cb.threadId, THREAD_CLASS(),
					 (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		ret = callback->cb.notifyCount;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(callback->cb.threadId);
	}
	sceGUIDReleaseObject(callbackId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_FDC044AC: ksceKernelGetCallbackInfo; a syscall sees its own ids */
int SceThreadmgrForDriver_FDC044AC(SceUID callbackId, SceKernelCallbackInfo *info)
{
	SceKernelCallbackInfo local;
	struct ThreadmgrThreadObject *object;
	SceUID puid;
	SceObjectBase *pObject;
	SceUID threadId;
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
				if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(),
						&pObject) < 0) {
					local.threadId = -1;
				} else {
					object = (struct ThreadmgrThreadObject *)pObject;
					local.threadId = object->context.userThreadId;
					sceGUIDReleaseObject(threadId);
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
int SceThreadmgrForDriver_767CA30C(int level, SceKernelCallbackInfo *infos, SceSize size,
				   SceSize *count)
{
	SceSize got = 0;
	SceKernelCallbackInfo info;
	SceUID *vector;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(CALLBACK_CLASS(), level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = sceGUIDGetUIDVectorByClass(CALLBACK_CLASS(), level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
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
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
end:
	return ret;
}

/* SceThreadmgrForDriver_FF8DA217: the same for a process's callback PUIDs, in its ids */
int SceThreadmgrForDriver_FF8DA217(SceUID pid, int level, SceKernelCallbackInfo *infos,
				   SceSize size, SceSize *count)
{
	SceSize got = 0;
	SceKernelCallbackInfo info;
	SceUID puid;
	SceObjectBase *pObject;
	SceSize found;
	struct ThreadmgrThreadObject *object;
	SceUID *vector, threadId;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = scePUIDGetUIDVectorByClass(pid, CALLBACK_CLASS(), level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = scePUIDGetUIDVectorByClass(pid, CALLBACK_CLASS(), level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getCallbackInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.callbackId = vector[i];
		threadId = info.threadId;
		if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(),
						 &pObject) < 0) {
			info.threadId = -1;
		} else {
			object = (struct ThreadmgrThreadObject *)pObject;
			info.threadId = object->context.userThreadId;
			sceGUIDReleaseObject(threadId);
		}
		if ((int)info.notifyId > 0) {
			if (PUIDGetPUIDVectorByGUID(pid, info.notifyId, level, &puid, 4, &found) < 0 ||
			    found == 0)
				info.notifyId = -1;
			else
				info.notifyId = puid;
		}
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
end:
	return ret;
}

/* registers the callback on the object (an event): notified with notifyId when it is set */
int sceKernelRegisterCallbackToEventForDriver(SceUID eventId, SceUID callbackId, uint32_t notifyId)
{
	ThreadmgrEventCallback *entry, *pos, *head;
	struct ThreadmgrCallback *callback;
	ThreadmgrEvent *event;
	SceAllocOpt opt;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(callbackId, CALLBACK_CLASS(),
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
	if (sceGUIDReferObjectWithSubclass(eventId,
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
	sceKernelSpinlockLowLock(&event->lock);
	opt.size = 0x14;
	opt.align = 0x40;
	entry = sceKernelAllocHeapMemoryWithOption(s_threadmgrRuntime.manager->heapId,
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
	sceKernelSpinlockLowUnlock(&event->lock);
	sceGUIDReleaseObject(eventId);
release:
	sceGUIDReleaseObject(callbackId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId)
{
	ThreadmgrEventCallback *entry, *head;
	ThreadmgrEvent *event;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(eventId,
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
	sceKernelSpinlockLowLock(&event->lock);
	ret = SCE_KERNEL_ERROR_CALLBACK_NOT_REGISTERED;
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
	sceKernelSpinlockLowUnlock(&event->lock);
	sceGUIDReleaseObject(eventId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelUnregisterCallbackFromEventAll(SceUID eventId)
{
	ThreadmgrEventCallback *entry;
	ThreadmgrEvent *event;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(eventId,
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
	sceKernelSpinlockLowLock(&event->lock);
	for (entry = EVENT_CALLBACK_HEAD(event)->next; entry != EVENT_CALLBACK_HEAD(event);
	     entry = entry->next) {
		entry->next->prev = entry->prev;
		entry->prev->next = entry->next;
		deleteRegisteredCallbackList(entry);
	}
	ret = 0;
	sceKernelSpinlockLowUnlock(&event->lock);
	sceGUIDReleaseObject(eventId);
resume:
	sceKernelCpuResumeIntr(intr);
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

	cls = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, 0x2c);
	if (cls == NULL)
		SCE_PANIC(184);
	if (sceUIDClassInitClass(cls, "SceUIDCallbackClass", SceSysmemForDriver_85336A1C(), 0x80,
				  uidCallback_Constructor,
				  (SceClassCallback)uidCallback_Destructor) < 0)
		SCE_PANIC_FUNC(SCE_LINE(2040, 2035), "uidCallbackClassInit");
	s_threadmgrRuntime.manager->callbackClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 197, " sizeof(SceUIDCallbackObject)=%d\n",
			     sizeof(struct ThreadmgrCallback));
	registerClass = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
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
