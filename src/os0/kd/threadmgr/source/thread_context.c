/* SceKernelThreadMgr: saved-context initialization and exit setup. */
#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"

#define THREADMGR_CONTEXT_DEFINE
#include "thread_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define REGISTER_POISON			0xdeadbeef
#define VFP_POISON_LOW			0x7f80dead	/* a signalling NaN in each single */
#define VFP_POISON_HIGH			0x7ff8dead
#define FPSCR_DEFAULT			0x03000000	/* DN | FZ */
#define FPEXC_EN			0x40000000
#define CPSR_MODE_SYS_MASKED		0xdf	/* SYS mode, IRQ and FIQ masked */
#define CPSR_MODE_SYS			0x1f
#define CPSR_THUMB			0x20
#define CPACR_VFP_FULL			0x00f00000	/* cp10 and cp11: full access */
#define THREAD_EXIT_MODE			0x87	/* PLACEHOLDER: thread +0x14 of a thread set up to exit */
#define SCE_KERNEL_ERROR_THREAD_STOPPED	0x8002802c

void __attribute__((noreturn)) exitDeleteThread(int delete, int intr, int status);
void __attribute__((noreturn)) _sceKernelExitCallback(int result);
int ksceKernelExitThread(int status);

/* a fresh VFP state: every register a NaN, default FPSCR, VFP enabled */
void initVfpContext(uintptr_t vfp)
{
	struct ThreadmgrVfpContext *context = (struct ThreadmgrVfpContext *)vfp;
	uint32_t *reg = context->s;

	memset(context, 0, sizeof(*context));
	while (reg != context->s + 64) {
		reg[0] = VFP_POISON_LOW;
		reg[1] = VFP_POISON_HIGH;
		reg += 2;
	}
	context->fpscr = FPSCR_DEFAULT;
	context->fpexc = FPEXC_EN;
}

/*
 * PLACEHOLDER: name. Points a stopped thread's saved context at its end:
 * exitDeleteThread, or _sceKernelExitCallback when it is in a callback. A
 * user thread gets a syscall frame on its kernel stack first, as if it had
 * called sceKernelExitThread.
 */
void setupThreadExit(struct ThreadCB *thread, uint32_t request)
{
	struct ThreadRegisterContext *regs;
	ThreadSyscallFrame *frame;
	SceProcessObject *process;
	uintptr_t vfp, entry;

	if ((int)thread->currentAttr < 0) {
		uint32_t *dest;
		const uint32_t *source;

		frame = (ThreadSyscallFrame *)(thread->kernelStackTop - sizeof(*frame));
		regs = thread->pRegisterContext;
		dest = frame->r;
		source = regs->r;
		do {
			*dest++ = *source++;
		} while (dest != frame->r + 13);
		vfp = thread->pVfpRegisterContext;
		frame->syscallAux = regs->syscallAux;
		frame->entry = (uintptr_t)ksceKernelExitThread;
		if (vfp != 0)
			frame->fpscr = ((struct ThreadmgrVfpContext *)vfp)->fpscr;
		frame->apiDeclare = 0;
		frame->tpidrurw = regs->tpidrurw;
		frame->sp = regs->sp;
		frame->lr = regs->lr;
		frame->pc = regs->pc;
		frame->cpsr = regs->cpsr;
		thread->userFrame = frame;
	} else {
		regs = thread->pRegisterContext;
		frame = NULL;
		vfp = thread->pVfpRegisterContext;
	}
	initRegisterContext(regs);
	if (thread->callbackNest < 1 && thread->threadEventHandler == NULL) {
		entry = (uintptr_t)exitDeleteThread;
		regs->r[0] = (request & THREAD_FLAG_REQUEST_EXIT_DELETE) != 0;
		regs->r[1] = 0;
		regs->r[2] = SCE_KERNEL_ERROR_THREAD_STOPPED;
		regs->cpsr = CPSR_MODE_SYS_MASKED;
	} else {
		entry = (uintptr_t)_sceKernelExitCallback;
		regs->r[0] = 0;
		regs->cpsr = CPSR_MODE_SYS;
	}
	thread->vfpException = THREAD_EXIT_MODE;
	regs->full = 1;
	if ((int)thread->currentAttr < 0) {
		regs->tpidruro = 1;
		regs->sp = (uintptr_t)frame;
	} else {
		regs->sp = thread->initSp;
	}
	regs->pc = entry & ~1;
	if (entry & 1)
		regs->cpsr |= CPSR_THUMB;
	regs->cpacr = CPACR_VFP_FULL;
	regs->lr = REGISTER_POISON;
	initVfpContext(vfp);
	process = thread->process;
	thread->pRegisterContext->pmuserenr = process->defaultPmuserenr;
	thread->pRegisterContext->pmcr = process->defaultPmcr;
	thread->ioFlags = 0;
	thread->permission = thread->initPermission;
	thread->tlsProcessId = thread->processId;
}
