/*
 * dtracep_module.c: the DTrace module's start and stop. Start builds DTrace
 * up from its heap to the DECI4P channels; stop takes it down in reverse.
 * DTrace starts only when DIP switch 194 enables user DECI.
 */
#include <sys/dtrace.h>
#include <sys/taskq.h>
#include <dtrace/heap.h>
#include <dtrace/logging.h>
#include <dtrace/sce/dtrace_bookmark.h>
#include <dtrace/sce/dtrace_hrtime.h>
#include <dtrace/sce/sce_fasttrap.h>
#include <kernel/modulemgr.h>
#include <sysmem/debug.h>
#include <sysmem/dipsw.h>
#include <sceerror.h>
#include "dtrace_client.h"
#include "dtracep.h"
#include "dtracep_module.h"
#include "global_timer.h"

#define DTRACE_CPUS		4
#define DTRACE_PROBE_STACK_ALIGN	0x1000

void *dtraceProbeStack[DTRACE_CPUS];
SceSize dtraceProbeStackSize[DTRACE_CPUS];

static int _dtraceModuleStart(void)
{
	int res, i;

	res = dtrace_heap_init();
	if (res != 0)
		DTRACE_LOG_ERROR_AT(67, "failed to create dtrace heap (0x%08x)", res);

	for (i = 0; i < DTRACE_CPUS; i++) {
		dtraceProbeStack[i] = dtrace_heap_alloc_aligned(DTRACE_PROBE_STACK_SIZE,
								DTRACE_PROBE_STACK_ALIGN);
		if (dtraceProbeStack[i] == NULL) {
			DTRACE_LOG_ERROR_AT(74, "failed to init stack for dtrace (0x%08x)",
					    SCE_ERROR_ERRNO_ENOMEM);
			SCE_PANIC(75);
		}
		dtraceProbeStackSize[i] = DTRACE_PROBE_STACK_SIZE;
	}

	res = dtraceClientInit();
	if (res != 0)
		DTRACE_LOG_ERROR_AT(80, "failed to create dtrace client list (0x%08x)", res);

	res = fasttrap_sce_init();
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(87, "failed to initialize dtrace breakpoint (0x%08x)", res);
		return res;
	}
	res = dtrace_bookmark_init();
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(95, "failed to initialize dtrace pa (0x%08x", res);
		return res;
	}
	res = GlobalTimerInit();
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(103, "failed to initialize global timer (0x%08x)", res);
		return res;
	}

	taskq_init();
	dtrace_hrtime_init();
	dtrace_init();
	res = dtracepInit();
	if (res < 0)
		return res;

	return 0;
}

static int _dtraceModuleStop(void)
{
	int res, i;

	res = dtracepExit();
	if (res < 0)
		DTRACE_LOG_ERROR_AT(189, "dtracep failed to exit (0x%08x)", res);

	dtrace_exit();
	dtrace_hrtime_exit();
	taskq_exit();
	if (GlobalTimerFini() != 0)
		DTRACE_LOG_ERROR_AT(216, "failed to exit global timer");

	res = dtrace_bookmark_fini();
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(223, "failed to exit dtrace pa (0x%08x)", res);
		return res;
	}
	res = fasttrap_sce_fini();
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(231, "failed to exit dtrace breakpoint (0x%08x)", res);
		return res;
	}
	res = dtraceClientFini();
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(239, "failed to exit dtrace client (0x%08x)", res);
		return res;
	}

	for (i = 0; i < DTRACE_CPUS; i++)
		dtrace_heap_free(dtraceProbeStack[i]);

	res = dtrace_heap_fini();
	if (res != 0)
		DTRACE_LOG_ERROR_AT(252, "failed to destroy dtrace heap: 0x%08x", res);

	return 0;
}

int dtraceModuleStart(SceSize args, const void *argp)
{
	int res;

	if (!sceKernelCheckDipsw(194)) {
		SCE_DBG_PRINTF(11, 280,
			"dtrace module start failed: disabled by dipsw; user DECI disabled\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}

	res = _dtraceModuleStart();
	if (res != 0)
		DTRACE_LOG_ERROR_AT(293, "module start failed (0x%08x)", res);

	return res;
}

int module_stop(SceSize args, const void *argp)
{
	int res;

	res = _dtraceModuleStop();
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(315, "deci4p_dtracep failed to stop (0x%08x)", res);
		return SCE_KERNEL_STOP_FAIL;
	}

	return SCE_KERNEL_STOP_SUCCESS;
}
