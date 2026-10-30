/*
 * DTrace's breakpoint action (Sony's dtrace/sce/dtrace_breakpoint.c): a
 * thread that fires breakpoint() in a debugged process is marked, and when it
 * returns to the process, a breakpoint instruction is planted where it goes
 * on. The debugger reports the hit to _removeElement, which puts the original
 * instruction back.
 */
#include <sys/dtrace_impl.h>
#include <deci4p_dbgp/dbgp.h>
#include <dtrace/heap.h>
#include <dtrace/utility.h>
#include <dtrace/sce/dtrace_cpu.h>
#include <dtrace/sce/dtrace_process.h>
#include <dtrace/sce/dtrace_security.h>
#include <excpmgr/excpmgr.h>
#include <kernel/error.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadcb.h>

#define BREAKPOINT_THREADS_MAX	16

#define BKPT_ARM		0xe1200971	/* bkpt #0x91 */
#define BKPT_THUMB		0xbe91		/* bkpt #0x91 */

#define CPSR_THUMB		0x20

/* a planted breakpoint, and the instruction it replaced */
typedef struct Element {
	SceUID pid;
	uintptr_t addr;
	int isThumb;
	uint32_t insn;
	struct Element *next;
} Element;

static struct {
	SceKernelSpinlock listLock;	/* the list */
	SceKernelSpinlock lock;		/* from planting a breakpoint until it is hit */
	Element *list;			/* circular, behind an empty head */
} s_breakpoint;

/* the threads that fired breakpoint() and have not returned to their process */
static kthread_t *s_breakpointThread[BREAKPOINT_THREADS_MAX];

extern uintptr_t dtrace_usdt_getarg_ptr;

static int _removeElement(SceUID pid, SceUID thid, SceUInt32 pc, SceUInt32 sp, SceUInt32 cpsr,
			  SceUInt32 armOpcode, SceUInt16 thumbOpcode, int isThumb)
{
	SceKernelIntrStatus intr;
	Element *prev, *cur, *element;
	int res;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_breakpoint.listLock);
	for (prev = s_breakpoint.list; ; prev = prev->next) {
		if (prev->next == s_breakpoint.list) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_breakpoint.listLock, intr);
			return SCE_KERNEL_ERROR_ERROR;
		}
		if (prev->next->addr == pc)
			break;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_breakpoint.listLock, intr);

	element = prev->next;
	if (element == NULL)
		return SCE_KERNEL_ERROR_ERROR;

	if (element->isThumb == 0) {
		uint32_t insn = element->insn;

		res = sceKernelCopyToUserProcTextDomainForDBGP(element->pid, (void *)pc, &insn, sizeof(insn));
	} else {
		uint16_t insn = element->insn;

		res = sceKernelCopyToUserProcTextDomainForDBGP(element->pid, (void *)pc, &insn, sizeof(insn));
	}
	if (res != 0) {
		sceKernelSpinlockLowUnlock(&s_breakpoint.lock);
		return SCE_KERNEL_ERROR_ERROR;
	}
	sceKernelL1IcacheInvalidateEntireAllCore();

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_breakpoint.listLock);
	if (element == s_breakpoint.list) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_breakpoint.listLock, intr);
		SCE_PANIC_AT(125);
	}

	prev = NULL;
	for (cur = s_breakpoint.list; cur->next != s_breakpoint.list; cur = cur->next) {
		if (prev != NULL && cur == element) {
			prev->next = cur->next;
			dtrace_heap_free(element);
			break;
		}
		prev = cur;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_breakpoint.listLock, intr);

	sceKernelSpinlockLowUnlock(&s_breakpoint.lock);
	return SCE_OK;
}

void dtrace_action_breakpoint_sce(dtrace_ecb_t *ecb)
{
	dtrace_probe_t *probe;
	dtrace_provider_t *prov;
	kthread_t *thread;
	SceUID pid;
	int i;

	thread = dtrace_get_current_thread();
	if (thread == NULL)
		return;
	pid = thread->processId;

	if (dtrace_security_check_internal() != 0 && dtrace_get_process_type(pid) != 0)
		return;

	/* only in a USDT probe, or on the return from a system call */
	probe = ecb->dte_probe;
	prov = probe->dtpr_provider;
	if ((uintptr_t)prov->dtpv_pops.dtps_getargval != dtrace_usdt_getarg_ptr &&
	    (strncmp(prov->dtpv_name, "syscall", 7) != 0 || strncmp(probe->dtpr_name, "return", 7) != 0))
		return;

	if (SceSysrootForDriver_39F1FD07(pid) == 0)
		return;

	for (i = 0; i < BREAKPOINT_THREADS_MAX; i++) {
		if (s_breakpointThread[i] == NULL) {
			s_breakpointThread[i] = dtrace_get_current_thread();
			break;
		}
	}
}

void dtrace_breakpoint_sce_init(void)
{
	if (s_breakpoint.list == NULL) {
		s_breakpoint.list = dtrace_heap_zalloc(sizeof(Element));
		s_breakpoint.list->next = s_breakpoint.list;
	}
	sceDbgpSetDTraceBreakpointHandler(_removeElement);
}

void dtrace_breakpoint_sce_fin(void)
{
}

void dtrace_action_sysbreak_sce(dtrace_ecb_t *ecb)
{
	dtrace_probe_t *probe;

	if (dtrace_security_check_internal() != 0)
		return;

	probe = ecb->dte_probe;
	SceDebugForDriver_391B74B7("%s:%s:%s dtrace sysbreak() cpu=%d tid=0x%08x\n",
				   probe->dtpr_mod, probe->dtpr_func, probe->dtpr_name,
				   dtrace_processor_id(), dtrace_get_current_thread_puid());
	__asm__ volatile("bkpt #0x92");
}

/* add a breakpoint behind the list's head */
static inline void addElement(Element *element)
{
	SceKernelIntrStatus intr;
	Element *next;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_breakpoint.listLock);
	next = s_breakpoint.list->next;
	s_breakpoint.list->next = element;
	element->next = next;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_breakpoint.listLock, intr);
}

void dtrace_do_breakpoint(void)
{
	SceExcpmgrContext *exc;
	ThreadSyscallFrame *frame;
	kthread_t *thread, *current;
	Element *element;
	uintptr_t pc;
	SceUID pid;
	uint16_t thumbInsn;
	uint32_t armInsn, insn;
	union {
		uint16_t thumb;
		uint32_t arm;
	} bkpt;
	int isThumb;
	int i;

	thread = dtrace_get_current_thread();
	for (i = 0; i < BREAKPOINT_THREADS_MAX; i++) {
		if (thread == s_breakpointThread[i])
			break;
	}
	if (i == BREAKPOINT_THREADS_MAX)
		return;

	thread = dtrace_get_current_thread();
	if (thread == NULL)
		return;
	pid = thread->processId;

	current = dtrace_get_current_thread();
	for (i = 0; i < BREAKPOINT_THREADS_MAX; i++) {
		if (current == s_breakpointThread[i]) {
			s_breakpointThread[i] = NULL;
			break;
		}
	}

	/* where the thread goes on in its process */
	frame = thread->userFrame;
	if (frame != NULL) {
		pc = frame->pc;
		isThumb = frame->cpsr & CPSR_THUMB;
	} else {
		if (SceExcpmgrForKernel_B615A7DA() == 0)
			return;
		exc = sceKernelGetExcpStackBottom(dtrace_processor_id());
		if (exc == NULL)
			return;
		exc--;
		isThumb = exc->cpsr & CPSR_THUMB;
		if (isThumb)
			pc = exc->pc + 1;
		else
			pc = exc->pc + 4;
	}

	sceKernelSpinlockLowLock(&s_breakpoint.lock);
	if (isThumb) {
		pc += 1;
		bkpt.thumb = BKPT_THUMB;
		if (sceKernelCopyFromUserProc(pid, &thumbInsn, (const void *)pc, sizeof(thumbInsn)) != 0 ||
		    sceKernelCopyToUserProcTextDomainForDBGP(pid, (void *)pc, &bkpt, sizeof(bkpt.thumb)) != 0)
			goto fail;

		insn = thumbInsn;
		element = dtrace_heap_zalloc(sizeof(*element));
		if (element != NULL) {
			element->pid = pid;
			element->addr = pc;
			element->isThumb = 1;
			element->insn = insn;
			addElement(element);
		}
	} else {
		bkpt.arm = BKPT_ARM;
		if (sceKernelCopyFromUserProc(pid, &armInsn, (const void *)pc, sizeof(armInsn)) != 0 ||
		    sceKernelCopyToUserProcTextDomainForDBGP(pid, (void *)pc, &bkpt, sizeof(bkpt.arm)) != 0)
			goto fail;

		insn = armInsn;
		element = dtrace_heap_zalloc(sizeof(*element));
		if (element != NULL) {
			element->pid = pid;
			element->addr = pc;
			element->isThumb = isThumb;
			element->insn = insn;
			addElement(element);
		}
	}
	sceKernelL1IcacheInvalidateEntireAllCore();
	return;

fail:
	sceKernelSpinlockLowUnlock(&s_breakpoint.lock);
}
