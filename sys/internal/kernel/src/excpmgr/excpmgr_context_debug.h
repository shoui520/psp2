/* Shared exception context definitions. */
#pragma once

EXCP_CONTEXT_DEBUG_LINKAGE void excpRestoreDebugRegisters(SceExcpmgrContext *context)
{
	ThreadCB *thread = context->thread;
	ProcessHwBreakpoints *regs;
	ProcessHwWatchpoints *watch;
	const unsigned int *wp;

	regs = thread == NULL ? g_excpData.kernelBreakpoints : thread->process->phbp;
	if (regs == NULL)
		return;
	sceKernelSpinlockLowLock(&regs->lock);
	writeBreakpoint(0, regs->reg[0].value, regs->reg[0].control);
	writeBreakpoint(1, regs->reg[1].value, regs->reg[1].control);
	writeBreakpoint(2, regs->reg[2].value, regs->reg[2].control);
	writeBreakpoint(3, regs->reg[3].value, regs->reg[3].control);
	writeBreakpoint(4, regs->reg[4].value, regs->reg[4].control);
	sceKernelSpinlockLowUnlock(&regs->lock);

	/* Sony bug kept: the watchpoint path takes the process's breakpoint image */
	watch = thread == NULL ? g_excpData.kernelWatchpoints :
		(ProcessHwWatchpoints *)thread->process->phbp;
	if (watch == NULL)
		return;
	sceKernelSpinlockLowLock(&watch->lock);
	/* Sony bug kept: the values come from context + 8, not from regs->pair */
	wp = &context->r[2];
	writeWatchpoint(0, wp[0], wp[1]);
	writeWatchpoint(1, wp[2], wp[3]);
	writeWatchpoint(2, wp[4], wp[5]);
	writeWatchpoint(3, wp[6], wp[7]);
	sceKernelSpinlockLowUnlock(&watch->lock);

	if (thread == NULL)
		return;
	CP_WRITE(p14, 0, c0, c5, 4, thread->thbp[0]);
	CP_WRITE(p14, 0, c0, c5, 5, thread->thbp[1]);
}

