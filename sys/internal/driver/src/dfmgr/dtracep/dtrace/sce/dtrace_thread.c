/*
 * What DTrace asks about threads (Sony's dtrace/sce/dtrace_thread.c): the
 * thread manager's DTrace state, the current thread's user registers, name
 * and kind.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/utility.h>
#include <dtrace/sce/dtrace_process.h>
#include <dtrace/sce/dtrace_security.h>
#include <dtrace/sce/dtrace_thread.h>
#include <acmgr/acmgr.h>
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <sysmem/uid_guid.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/threadmgr.h>

int dtrace_thread_state_init(void)
{
	int res;

	res = sceKernelSetDTraceThreadCBEnable(1);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(29, "failed to enabled dtrace thread state (0x%08x)", res);
	return res;
}

int dtrace_thread_state_exit(void)
{
	int res;

	res = sceKernelSetDTraceThreadCBEnable(0);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(40, "failed to disable dtrace thread state (0x%08x)", res);
	return res;
}

uint64_t dtrace_getreg_thread(kthread_t *thread, uint_t reg)
{
	ThreadSyscallFrame *frame;
	ThreadRegisterContext *ctx;
	SceExcpmgrContext *exc;
	kthread_t *cur;
	dtrace_intr_frame_t *intrFrame;
	int intr, excp;

	if ((thread->currentAttr & THREAD_ATTR_USER) == 0 || thread != dtrace_get_current_thread())
		return 0;
	if (reg > REG_PC) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return 0;
	}

	cur = dtrace_get_current_thread();
	intr = sceKernelIsIntrContext();
	excp = SceExcpmgrForKernel_B615A7DA();
	if (cur == NULL || (cur->currentAttr & THREAD_ATTR_USER) == 0)
		return 0;

	frame = cur->userFrame;
	if (frame != NULL) {
		if (reg <= 12)
			return frame->r[reg];
		if (reg == REG_SP)
			return frame->sp;
		if (reg == REG_LR)
			return frame->lr;
		return frame->pc;
	}
	if (excp != 0) {
		exc = sceKernelGetExcpStackBottom(dtrace_processor_id());
		if (exc == NULL)
			return 0;
		exc--;
		if (reg <= 12)
			return exc->r[reg];
		if (reg == REG_SP)
			return exc->sp;
		if (reg == REG_LR)
			return exc->lr;
		return exc->pc;
	}
	if (intr == 0)
		return 0;
	intrFrame = SceIntrmgrForKernel_01911612(dtrace_processor_id(), intr);
	if (intrFrame == NULL)
		return 0;
	cur = intrFrame[-1].thread;
	if (cur == NULL)
		return 0;
	ctx = cur->pRegisterContext;
	if (reg <= 12)
		return ctx->r[reg];
	if (reg == REG_SP)
		return ctx->sp;
	if (reg == REG_LR)
		return ctx->lr;
	return ctx->pc;
}

void dtrace_get_current_thread_name(char *name, unsigned int size)
{
	const char *guidName = NULL;
	SceUID thid;

	thid = dtrace_get_current_thread_id();
	if (thid < 0) {
		if (name[0] == '\0')
			snprintf(name, size, "unknown");
	} else {
		if (dtrace_security_check_access_thread(thid) == 0 &&
		    sceGUIDGetName(thid, &guidName) == 0) {
			if (guidName != NULL)
				strncpy(name, guidName, size);
			else
				name[0] = '\0';
		}
		if (name[0] == '\0')
			snprintf(name, size, "0x%08x", thid);
	}
	name[size - 1] = '\0';
}

int dtrace_get_current_thread_type(void)
{
	kthread_t *thread;

	thread = dtrace_get_current_thread();
	if (thread->processId == DTRACE_KERNEL_PROCESS) {
		switch (thread->threadId) {
		case SCE_KERNEL_IDLE_THREAD_0_ID:
		case SCE_KERNEL_IDLE_THREAD_1_ID:
		case SCE_KERNEL_IDLE_THREAD_2_ID:
		case SCE_KERNEL_IDLE_THREAD_3_ID:
			return DT_THREAD_KERNEL_IDLE;
		default:
			return DT_THREAD_KERNEL;
		}
	}
	if (sceSblACMgrIsGameProgram(thread->processId))
		return DT_THREAD_GAME;
	return sceSblACMgrIsSystemProgram(thread->processId) ? DT_THREAD_SYSTEM : DT_THREAD_UNKNOWN;
}
