/*
 * The PS Vita parts of the DTrace framework (Sony's dtrace/sce/dtrace_sce.c):
 * error probes, SDT probes run on the CPU's own probe stack, the kernel
 * modules offered to providers, and the communication processor's values.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/heap.h>
#include <dtrace/sce/dtrace_process.h>
#include <dtrace/utility.h>
#include <intrmgr/intrmgr.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/cpu.h>
#include "../../dtracep_module.h"
#include "../../dtracep_pmep.h"

/* the communication processor's values, one word per cpch variable */
#define DTRACE_CP_VALUES_SIZE	0x120

typedef struct dtrace_sdt_args {
	const sdt_probedesc_t *probeDesc;
	uintptr_t arg0;
	uintptr_t arg1;
	uintptr_t arg2;
	uintptr_t arg3;
	uintptr_t arg4;
} dtrace_sdt_args_t;

extern dtrace_id_t dtrace_probeid_error;

/* the hooks the providers set: fasttrap's process hooks, the pid provider */
void (*dtrace_fasttrap_fork_ptr)(proc_t *, proc_t *);
int (*dtrace_pid_probe_ptr)(dtrace_thread_state_t *, boolean_t thumb);
int (*dtrace_return_probe_ptr)(dtrace_thread_state_t *, boolean_t thumb);
void (*dtrace_fasttrap_exec_ptr)(proc_t *);
void (*dtrace_fasttrap_exit_ptr)(proc_t *);

static uint32_t *s_cpValues;

void dtrace_probe_error(dtrace_state_t *state, dtrace_epid_t epid, int which, int fault,
			int fltoffs, uint64_t illval)
{
	state->dts_arg_error_illval = illval;
	dtrace_probe(dtrace_probeid_error, (uintptr_t)state, epid, which, fault, fltoffs);
}

int dtrace_istoxic_memory_range(uintptr_t kaddr, size_t size)
{
	return dtrace_heap_contains((const void *)kaddr, size);
}

/* runs on the probe stack; dtraceChangeStack's result is not used */
static void dtrace_probe_sdt_func(dtrace_sdt_args_t *args)
{
	dtrace_probe_sdt(args->probeDesc, args->arg0, args->arg1, args->arg2, args->arg3,
			 args->arg4);
}

void dtrace_probes_for_provider(dtrace_provider_t *prv)
{
	SceSize num;
	SceModuleCB *mod;
	SceUID *list;
	int res, i;

	res = sceKernelGetModuleList(DTRACE_KERNEL_PROCESS, 0x80, 0, NULL, NULL);
	num = res;
	if (res < 0)
		return;
	list = dtrace_heap_alloc(res * sizeof(*list));
	if (list == NULL) {
		DTRACE_LOG_ERROR_AT(34, "failed to allocate memory for module list");
		return;
	}
	if (sceKernelGetModuleList(DTRACE_KERNEL_PROCESS, 0x80, 1, list, &num) < 0)
		return;
	for (i = 0; i < (int)num; i++) {
		if (SceModulemgrForKernel_37512E29(list[i], &mod) >= 0)
			prv->dtpv_pops.dtps_provide_module(prv->dtpv_arg, (struct modctl *)mod);
	}
	dtrace_heap_free(list);
}

uint64_t dtrace_get_cp_value(SceInt32 var)
{
	if ((uint32_t)(var - DIF_VAR_CPCH0) > DIF_VAR_CPCH7 - DIF_VAR_CPCH0)
		return 0;
	return s_cpValues[var - DIF_VAR_CPCH0];
}

void dtrace_connect_cp(void)
{
	s_cpValues = dtrace_heap_alloc(DTRACE_CP_VALUES_SIZE);
	sceDtracepPmepStartPolling(s_cpValues);
}

void dtrace_disconnect_cp(void)
{
	sceDtracepPmepStopPolling();
	if (s_cpValues != NULL) {
		dtrace_heap_free(s_cpValues);
		s_cpValues = NULL;
	}
}

void dtrace_probe_sdt_sce(const sdt_probedesc_t *probeDesc, uintptr_t arg0, uintptr_t arg1,
			  uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
	dtrace_sdt_args_t args;
	processorid_t cpu;
	int intr;

	args.probeDesc = probeDesc;
	args.arg0 = arg0;
	args.arg1 = arg1;
	args.arg2 = arg2;
	args.arg3 = arg3;
	args.arg4 = arg4;
	intr = sceKernelCpuSuspendIntr();
	cpu = dtrace_processor_id();
	dtraceChangeStack(&args, (char *)dtraceProbeStack[cpu] + dtraceProbeStackSize[cpu],
			  (SceInt64 (*)(void *))dtrace_probe_sdt_func);
	sceKernelCpuResumeIntr(intr);
}
