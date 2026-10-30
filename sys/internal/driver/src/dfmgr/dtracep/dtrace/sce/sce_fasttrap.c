/*
 * The pid provider's hook into the debugger (Sony's dtrace/sce/sce_fasttrap.c):
 * the debugger hands a USDT probe a process hits to the pid provider, with
 * the thread's registers.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/sce/sce_fasttrap.h>
#include <deci4p_dbgp/dbgp.h>
#include <threadmgr/threadmgr.h>

static int fasttrap_sce_usdt_handler(SceUID pid, SceUID thid, SceUInt32 pc, SceUInt32 sp,
				     SceUInt32 cpsr, SceUInt32 armOpcode, SceUInt16 thumbOpcode,
				     int isThumb)
{
	ThreadmgrDebugRegisters regs[2];	/* the user's, then the kernel's */
	int res;

	res = SceThreadmgrForDriver_5022689D(thid, regs);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(40, "failed to get CPU registers from thread 0x%08x (0x%08x)",
				    thid, res);
		return 0;
	}
	if (dtrace_pid_probe_ptr != NULL)
		dtrace_pid_probe_ptr((dtrace_thread_state_t *)&regs[0], isThumb);
	return 0;
}

int fasttrap_sce_init(void)
{
	sceDbgpSetDTraceUsdtHandler(fasttrap_sce_usdt_handler);
	return 0;
}

int fasttrap_sce_fini(void)
{
	sceDbgpSetDTraceUsdtHandler(NULL);
	return 0;
}
