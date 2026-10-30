/*
 * The CPU DTrace runs on (Sony's dtrace/sce/dtrace_cpu.c): interrupt state,
 * and cross calls run on every CPU through the thread manager.
 */
#include <sys/dtrace_impl.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>

static void dtrace_sync_func(void)
{
}

int dtrace_is_interrupt_context(void)
{
	return sceKernelIsIntrContext() != 0;
}

dtrace_icookie_t dtrace_interrupt_disable(void)
{
	return sceKernelCpuSuspendIntr();
}

void dtrace_interrupt_enable(dtrace_icookie_t cookie)
{
	sceKernelCpuResumeIntr(cookie);
}

void dtrace_xcall(processorid_t cpu, dtrace_xcall_t func, void *arg)
{
	int res;

	if (cpu == DTRACE_CPUALL) {
		for (cpu = 0; cpu < NCPU; cpu++) {
			res = SceThreadmgrForKernel_DF59DF99(cpu, func, arg);
			if (res != 0)
				DTRACE_LOG_ERROR_AT(43, "%s failed calling CPU%d (0x%08x)", __FUNCTION__, cpu, res);
		}
	} else {
		res = SceThreadmgrForKernel_DF59DF99(cpu, func, arg);
		if (res != 0)
			DTRACE_LOG_ERROR_AT(50, "%s failed calling CPU%d (0x%08x)", __FUNCTION__, cpu, res);
	}
}

void dtrace_sync(void)
{
	dtrace_xcall(DTRACE_CPUALL, (dtrace_xcall_t)dtrace_sync_func, NULL);
}
