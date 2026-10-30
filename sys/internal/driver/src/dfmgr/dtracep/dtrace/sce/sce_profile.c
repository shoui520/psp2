/*
 * The profile provider's probe firing (Sony's dtrace/sce/sce_profile.c): the
 * interrupted PC, kernel or user, as the probe's arguments.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/utility.h>
#include <dtrace/sce/dtrace_security.h>
#include <dtrace/sce/dtrace_thread.h>
#include <dtrace/sce/sce_profile.h>
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadcb.h>

/* the CPSR's mode field (ARM's names) */
#define CPSR_MODE_MASK	0x1f
#define CPSR_MODE_USR	0x10

void profile_fire_probe(dtrace_id_t id, hrtime_t late, int frames)
{
	dtrace_intr_frame_t *frame;
	ThreadRegisterContext *ctx;
	uintptr_t pc = 0;
	uint32_t cpsr;
	SceUID pid;
	int intr;

	pid = dtrace_get_current_process_id();
	dtrace_get_current_thread_id();
	intr = sceKernelIsIntrContext();
	if (intr > 0) {
		frame = SceIntrmgrForKernel_01911612(dtrace_processor_id(), intr);
		if (frame != NULL) {
			frame--;
			if (frame->thread != NULL) {
				ctx = frame->thread->pRegisterContext;
				pc = ctx->pc;
				cpsr = ctx->cpsr;
			} else {
				pc = frame->pc;
				cpsr = frame->cpsr;
			}
			if ((cpsr & CPSR_MODE_MASK) == CPSR_MODE_USR) {
				if (dtrace_security_check_access_process_state(pid) != 0)
					pc = -1;
				dtrace_probe(id, 0, pc, late, 0, 0);
				return;
			}
		}
	}

	if (dtrace_security_check_internal() != 0)
		pc = -1;
	dtrace_probe(id, pc, 0, late, 0, 0);
}
