#include "pamgr_internal.h"
#include <string.h>
#include "qaf_cache.h"
#include <sysmem/cpu.h>
#include <excpmgr/excpmgr_cpu.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>
#include <sysmem/sysroot_pamgr.h>
#include <syslibtrace/syslibtrace.h>
#include <kernel/error.h>
#include <sysmem/dipsw.h>
#include <sysmem/suspend.h>
#include <sysmem/sysmem.h>
#include <processmgr/processmgr.h>
#include <acmgr/acmgr.h>
#include <intrmgr/intrmgr.h>
#include <pamgr/pamgr.h>

int checkDebugPermission(void);
int configurePrimaryTrace(const volatile struct PamgrPrimaryTraceConfig *config);
int configureArmTrace(const volatile struct PamgrArmTraceConfig *config);
int startTrace(unsigned int channel);
int stopTrace(unsigned int channel);

struct PamgrState pamgrState;

/* GPU sampling configuration with two intervals of at least 32. */
int configureGpuTrace(const volatile struct PamgrGpuTraceConfig *config)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	uint32_t mode, control;
	uint16_t interval0, interval1;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	interval1 = config->interval1;
	interval0 = config->interval0;
	if (interval0 <= 31 || interval1 <= 31)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	memcpy(&pamgrState.gpuTraceConfig, (const void *)config, sizeof(*config));
	pfm->traceEventControl |=PFM_EVENT_GPU_TRACE;
	mode = SceSysrootForKernel_403B509E();
	control = config->control;
	interval1 = config->interval1;
	control &= mode <= 0x20 ? 7 : 15;
	pamgrState.pfmRegisters->gpuControl = control;
	pamgrState.pfmRegisters->gpuIntervals =
		(uint32_t)interval1 << 16 | config->interval0;
	pamgrState.pfmRegisters->gpuCounterConfig[0] = config->counterConfig[0];
	pamgrState.pfmRegisters->gpuCounterConfig[1] = config->counterConfig[1];
	pamgrState.pfmRegisters->gpuCounterConfig[2] = config->counterConfig[2];
	pamgrState.pfmRegisters->gpuCounterConfig[3] = config->counterConfig[3];
	pamgrState.pfmRegisters->gpuCounterConfig[4] = config->counterConfig[4];
	pamgrState.pfmRegisters->gpuCounterConfig[5] = config->counterConfig[5];
	pamgrState.pfmRegisters->gpuCounterConfig[6] = config->counterConfig[6];
	pamgrState.pfmRegisters->gpuCounterConfig[7] = config->counterConfig[7];
	pamgrState.pfmRegisters->gpuSampleConfig[0] = config->sampleConfig[0];
	pamgrState.pfmRegisters->gpuSampleConfig[1] = config->sampleConfig[1];
	pamgrState.pfmRegisters->gpuSampleConfig[2] = config->sampleConfig[2];
	pamgrState.pfmRegisters->gpuSampleConfig[3] = config->sampleConfig[3];
	pamgrState.pfmRegisters->gpuSampleConfig[4] = config->sampleConfig[4];
	pamgrState.pfmRegisters->gpuSampleConfig[5] = config->sampleConfig[5];
	pamgrState.pfmRegisters->gpuSampleConfig[6] = config->sampleConfig[6];
	pamgrState.pfmRegisters->gpuSampleConfig[7] = config->sampleConfig[7];
	return 0;
}

int ScePamgrForDriver_02ACD81C(struct PamgrGpuTraceConfig *config)
{
	if (!(pamgrState.pfmRegisters->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	memcpy(config, &pamgrState.gpuTraceConfig, sizeof(*config));
	return 0;
}

/* Poison the saved configuration, then disable GPU sampling. */
int clearGpuTrace(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	memset(&pamgrState.gpuTraceConfig, 0xcd,
	       sizeof(pamgrState.gpuTraceConfig));
	pfm->traceEventControl &=~PFM_EVENT_GPU_TRACE;
	pamgrState.pfmRegisters->gpuControl = 0;
	pamgrState.pfmRegisters->gpuIntervals = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[0] = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[1] = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[2] = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[3] = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[4] = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[5] = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[6] = 0;
	pamgrState.pfmRegisters->gpuCounterConfig[7] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[0] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[1] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[2] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[3] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[4] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[5] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[6] = 0;
	pamgrState.pfmRegisters->gpuSampleConfig[7] = 0;
	return 0;
}

/* Enable PMU counter trace after validating the hardware mode. */
int configureCounterTrace(const volatile struct PamgrCounterTraceConfig *config)
{
	ScePerfmonRegisters *pfm;
	uint32_t interval, selection, mode, pmcr, flags;
	uint8_t upper, lower;

	if (!(pamgrState.pfmRegisters->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	interval = config->interval;
	if (interval <= 31)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	mode = SceSysrootForKernel_403B509E();
	selection = config->selection;
	if (mode == 0x20 && !(selection & 0x1fff))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pmcr = readPmcr();
	pmcr |= PMCR_E | PMCR_X;
	writePmcr(pmcr);
	pfm = pamgrState.pfmRegisters;
	flags = config->flags;
	pfm->traceEventControl |=PFM_EVENT_COUNTER_TRACE;
	pfm->counterTraceSelection = (selection & 0x1fff) << 16 | interval;
	pfm->counterTraceValues[0] = config->values[0];
	pfm->counterTraceValues[1] = config->values[1];
	pfm->counterTraceValues[2] = config->values[2];
	pfm->counterTraceControl = config->control & 0x100ff;
	upper = config->upperByte;
	lower = config->lowerByte;
	pfm->counterTraceBytes = (uint32_t)upper << 16 | lower;
	pfm->counterTraceFlags = (flags & 0xf) | (flags & 0x80) << 1;
	return 0;
}

/* Disable counter trace and clear all of its configuration. */
int __attribute__((noinline)) clearCounterTrace(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceEventControl &=~PFM_EVENT_COUNTER_TRACE;
	pfm->counterTraceSelection = 0;
	pfm->counterTraceValues[0] = 0;
	pfm->counterTraceValues[1] = 0;
	pfm->counterTraceValues[2] = 0;
	pfm->counterTraceControl = 0;
	pfm->counterTraceBytes = 0;
	pfm->counterTraceFlags = 0;
	return 0;
}

/* User virtual buffer adapter, translation precedes validation. */
int configurePrimaryTraceForUser(
				const struct PamgrPrimaryTraceConfig *config)
{
	struct PamgrPrimaryTraceConfig physicalConfig;
	uintptr_t physical;
	uint32_t virtual;
	int result;

	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	physicalConfig = *config;
	virtual = physicalConfig.base;
	result = sceKernelVAtoPA((const void *)virtual, &physical);
	if (result < 0)
		return result;
	if (SceSysmemForDriver_A7C0D1FC((const void *)virtual) < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pamgrState.traceBufferVirtual = virtual;
	pamgrState.traceBufferPhysical = physical;
	physicalConfig.base = physical;
	return configurePrimaryTrace(&physicalConfig);
}

int sceKernelPaAddCounterTraceForUser(const struct PamgrCounterTraceConfig *config)
{
	int permission, result;

	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	permission = sceKernelSetThreadAccessLevel(PAMGR_ACCESS_LEVEL);
	if (permission < 0) {
		SCE_DBG_PRINTF(0, 2449,
			"[Error]  sceKernelSetThreadAccessLevel(ret = 0x%08x)\n",
			permission);
		return permission;
	}
	result = configureCounterTrace(config);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

/* The separate user entry preserves the driver clear result. */
int clearCounterTraceForUser(void)
{
	return clearCounterTrace();
}

/* Primary trace buffer and callback-record configuration. */
int configurePrimaryTrace(const volatile struct PamgrPrimaryTraceConfig *config)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	uint32_t control, base, size, limit;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if ((config->base & 0x7f) || (config->size & 0x7f) ||
	    (config->limit & 0x7f) || (config->control & 0xfffefff0))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	control = config->control;
	size = config->size;
	limit = config->limit;
	base = config->base;
	pamgrState.primaryTraceConfig.control = control;
	pamgrState.primaryTraceConfig.base = base;
	pamgrState.primaryTraceConfig.size = size;
	pamgrState.primaryTraceConfig.limit = limit;
	pamgrState.primaryTraceConfig.recordMask = config->recordMask;
	pfm->traceBufferControl = config->control & 0x1000f;
	pfm->traceBufferBase = config->base;
	pfm->traceWritePointer = config->base;
	pfm->traceBufferSize = config->size;
	pfm->traceBufferLimit = config->limit;
	pamgrState.traceRecordMask = config->recordMask;
	return 0;
}

int ScePamgrForDriver_606C98CD(struct PamgrPrimaryTraceConfig *config)
{
	if (!(pamgrState.pfmRegisters->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	*config = pamgrState.primaryTraceConfig;
	return 0;
}

int configureArmTraceForUser(
		const struct PamgrArmTraceConfig *config)
{
	struct PamgrArmTraceConfig local;

	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	local = *config;
	if (local.modeAndContext - 1 <= 1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (local.modeAndContext - 5 <= 1)
		local.modeAndContext |= SceCpuForKernel_FD54A91D() << 24;
	return configureArmTrace(&local);
}

/* User entries retain Sony's inlined filter operations. */
int clearArmTraceForUser(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceEventControl &=~PFM_EVENT_ARM_TRACE;
	return 0;
}

int __attribute__((noinline)) setTraceWordFilter(const uint32_t *filter);

int setTraceWordFilterForUser(const uint32_t *filter)
{
	if (filter == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return setTraceWordFilter(filter);
}

int clearTraceWordFilterForUser(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceEventControl &=~PFM_EVENT_WORD_FILTER;
	pfm->traceWordFilter = 0;
	return 0;
}

void ScePamgrForDriver_E4AF5A5D(uint32_t *values)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	values[0] = pfm->busErrorLog[0];
	values[1] = pfm->busErrorLog[1];
}

void ScePamgrForDriver_C0EF725E(uint32_t *values)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	values[0] = pfm->secureBusErrorLog[0];
	values[1] = pfm->secureBusErrorLog[1];
}

/* Debug/PA bus error log resets: the attribute word is cleared. */
void clearBusErrorLog(void)
{
	pamgrState.pfmRegisters->busErrorLog[1] = 0;
}

void clearSecureBusErrorLog(void)
{
	pamgrState.pfmRegisters->secureBusErrorLog[1] = 0;
}

void enableTraceAuxControl0(void)
{
	pamgrState.pfmRegisters->traceAuxControl0 = 1;
}

void enableTraceAuxControl1(void)
{
	pamgrState.pfmRegisters->traceAuxControl1 = 1;
}

void disableTraceAuxControl1(void)
{
	pamgrState.pfmRegisters->traceAuxControl1 = 0;
}

void ScePamgrForDriver_4C4A48AF(uint32_t *controls)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	controls[0] = pfm->traceBitControls[0];
	controls[1] = pfm->traceBitControls[1];
	controls[2] = pfm->traceBitControls[2];
	controls[3] = pfm->traceBitControls[3];
}

/* Each of the four control words accepts only bit 1. */
int setTraceBitControls(const uint32_t *controls)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceBitControls[0] = controls[0] & 2;
	pfm->traceBitControls[1] = controls[1] & 2;
	pfm->traceBitControls[2] = controls[2] & 2;
	pfm->traceBitControls[3] = controls[3] & 2;
	return 0;
}

/* Preserve the original order of all ten device writes. */
int configureTraceSamples(const volatile struct PamgrSampleConfig *config)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->sampleControl0 = config->bank0Control;
	pfm->sampleControl1 = config->bank1Control;
	pfm->traceSampleControls[0] = config->traceControls[0];
	pfm->traceSampleControls[1] = config->traceControls[1];
	pfm->traceSampleControls[2] = config->traceControls[2];
	pfm->traceSampleControls[3] = config->traceControls[3];
	pfm->buffer1SampleControls[0] = config->buffer1Controls[0];
	pfm->buffer1SampleControls[1] = config->buffer1Controls[1];
	pfm->buffer1SampleControls[2] = config->buffer1Controls[2];
	pfm->buffer1SampleControls[3] = config->buffer1Controls[3];
	return 0;
}

unsigned int ScePamgrForDriver_060C4668(void)
{
	return pamgrState.pfmRegisters->sampleStatus;
}

/* Stop tracing while replacing its two configuration words. */
void configureTraceControl(uint32_t config, uint32_t half)
{
	pamgrState.pfmRegisters->traceControl = 0;
	pamgrState.pfmRegisters->traceConfigHalf = (uint16_t)half;
	pamgrState.pfmRegisters->traceConfig = config;
	pamgrState.pfmRegisters->traceControl = PFM_TRACE_ENABLED;
}

/* Channel 1 occupies the upper half of both shared event masks. */
int configureTraceBuffer1(const struct PamgrTraceBufferConfig *config)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	uint16_t first, second;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if ((config->base & 0x7f) || (config->size & 0x7f))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	first = config->eventMask0;
	pfm->traceBuffer1Base = config->base;
	second = config->eventMask1;
	pfm->traceBuffer1WritePointer = config->base;
	pfm->traceBuffer1Size = config->size;
	pfm->traceEventMask0 = (pfm->traceEventMask0 & 0xffff) | (uint32_t)first << 16;
	pfm->traceEventMask1 = (pfm->traceEventMask1 & 0xffff) | (uint32_t)second << 16;
	return 0;
}

/* Record selection is inverted in the low 16 mask bits. */
int setTraceRecordMask(unsigned int mask, uint32_t selection)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (mask == 0)
		pfm->traceEventMask0 = (pfm->traceEventMask0 & 0xffff0000) |
					(uint16_t)~selection;
	else if (mask == 1) {
		pfm->traceEventMask1 = (pfm->traceEventMask1 & 0xffff0000) |
					(uint16_t)~selection;
		pamgrState.traceRecordCallbacksEnabled = (selection & 0x40) != 0;
	} else
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return 0;
}

int ScePamgrForDriver_6005FDAA(unsigned int mask, uint32_t *selection)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (mask == 0)
		*selection = (uint16_t)~pfm->traceEventMask0;
	else if (mask == 1)
		*selection = (uint16_t)~pfm->traceEventMask1;
	else
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return 0;
}

int ScePamgrForDriver_3D299796(unsigned int bank, uint32_t *status)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (bank == 0)
		*status = pfm->registerBank0Status;
	else if (bank == 1)
		*status = pfm->registerBank1Status;
	else
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return 0;
}

volatile uint32_t *ScePamgrForDriver_328C006F(unsigned int bank, unsigned int index)
{
	if (index > 15)
		return NULL;
	if (bank == 0)
		return &pamgrState.pfmRegisters->registerBank0[index];
	if (bank == 1)
		return &pamgrState.pfmRegisters->registerBank1[index];
	return NULL;
}

int __attribute__((noinline)) ScePamgrForDriver_16A24732(
			unsigned int bank, unsigned int index, uint32_t value)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (bank > 1 || index > 15)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (bank == 0)
		pfm->registerBank0[index] = value;
	else
		pfm->registerBank1[index] = value;
	return 0;
}

uint32_t ScePamgrForDriver_80CA3B39(void)
{
	return pamgrState.pfmRegisters->traceBuffer1WritePointer;
}

/* The second trace buffer has the same 128-byte alignment. */
int setTraceBuffer1WritePointer(uint32_t pointer)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (pointer & 0x7f)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pfm->traceBuffer1WritePointer = pointer;
	return 0;
}

/* Independent event-filter control bits 0, 1 and 2. */
int clearArmTrace(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceEventControl &=~PFM_EVENT_ARM_TRACE;
	return 0;
}

int setTracePackedFilter(const struct PamgrPackedFilter *filter)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	uint16_t upper, lower;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	lower = filter->lower;
	if (lower == 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	upper = filter->upper;
	pfm->traceEventControl |=PFM_EVENT_PACKED_FILTER;
	pfm->tracePackedFilter = ((uint32_t)(upper & 0xfff) << 16) | lower;
	return 0;
}

int clearTracePackedFilter(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceEventControl &=~PFM_EVENT_PACKED_FILTER;
	pfm->tracePackedFilter = 0;
	return 0;
}

int setTraceWordFilter(const uint32_t *filter)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceEventControl |=PFM_EVENT_WORD_FILTER;
	pfm->traceWordFilter = *filter & 0x7fff007f;
	return 0;
}

int clearTraceWordFilter(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	pfm->traceEventControl &=~PFM_EVENT_WORD_FILTER;
	pfm->traceWordFilter = 0;
	return 0;
}

static inline __attribute__((always_inline)) int shouldWriteSyscallTrace(uint32_t flags[4])
{
	if (SceProcessmgrForDriver_54D469E2(&flags[SceCpuId()]) != 0)
		return 0;
	if (flags[SceCpuId()] & 2)
		return (flags[SceCpuId()] & 1) != 0;
	if (checkDebugPermission() != 0 &&
	    SceSblACMgrForKernel_49509A83(sceKernelGetProcessId()) == 0) {
		sceKernelSetSyscallTraceFlag(0, 2);
		return 0;
	}
	sceKernelSetSyscallTraceFlag(0, 3);
	return 1;
}

void sceKernelPaSetSyscallLeave(ThreadSyscallFrame *frame, uint32_t *results)
{
	uint32_t flags[4];

#if FW_DEBUG
	if (SceDebugForDriver_374B7868() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, (readCpsr() & PSR_I_BIT) != 0, 1662);
	}
#endif
	if (!shouldWriteSyscallTrace(flags))
		return;
	pamgrState.pfmRegisters->traceFifo = 0x05100000;
	pamgrState.pfmRegisters->traceFifo = frame->r[12];
	pamgrState.pfmRegisters->traceFifo = results[0];
	pamgrState.pfmRegisters->traceFifo = results[1];
}

void sceKernelPaSetSyscallEnter(ThreadSyscallFrame *frame)
{
	uint32_t flags[4], nid;

#if FW_DEBUG
	if (SceDebugForDriver_374B7868() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, (readCpsr() & PSR_I_BIT) != 0, 1617);
	}
#endif
	if (!shouldWriteSyscallTrace(flags))
		return;
	pamgrState.pfmRegisters->traceFifo = 0x041c0000;
	SceSyslibtraceForKernel_7CC73CDA(frame->r[12], &nid);
	pamgrState.pfmRegisters->traceFifo = ~(nid + 0x162b09c8U);
	pamgrState.pfmRegisters->traceFifo = frame->r[12];
	pamgrState.pfmRegisters->traceFifo = frame->r[0];
	pamgrState.pfmRegisters->traceFifo = frame->r[1];
	pamgrState.pfmRegisters->traceFifo = frame->r[2];
	pamgrState.pfmRegisters->traceFifo = frame->r[3];
}

/* The observer may change the callback before it is loaded. */
void dispatchSyscallLeave(ThreadSyscallFrame *frame, uint32_t *results)
{
	if (pamgrState.syscallLeaveObserver)
		pamgrState.syscallLeaveObserver(frame, results);
	if (pamgrState.syscallLeaveCallback)
		pamgrState.syscallLeaveCallback(frame, results);
}

void dispatchSyscallEnter(ThreadSyscallFrame *frame)
{
	if (pamgrState.syscallEnterObserver)
		pamgrState.syscallEnterObserver(frame);
	if (pamgrState.syscallEnterCallback)
		pamgrState.syscallEnterCallback(frame);
}

/* Tracing callbacks share the IntrMgr hooks with observers. */
void setSyscallCallbacks(PamgrSyscallEnterCallback enter,
			 PamgrSyscallLeaveCallback leave)
{
	PamgrSyscallEnterCallback observer = pamgrState.syscallEnterObserver;

	pamgrState.syscallEnterCallback = enter;
	pamgrState.syscallLeaveCallback = leave;
	if (observer == NULL && pamgrState.syscallLeaveObserver == NULL)
		SceIntrmgrForKernel_217AC350(dispatchSyscallEnter, dispatchSyscallLeave);
}

void clearSyscallTraceCallbacks(void)
{
	SceThreadmgrForKernel_A3975A5A(NULL);
	SceThreadmgrForKernel_F885ECCA(NULL);
	SceThreadmgrForKernel_9605DF7D(NULL);
	setSyscallCallbacks(NULL, NULL);
}

void ScePamgrForDriver_A9DD2CB8(PamgrSyscallEnterCallback enter,
			      PamgrSyscallLeaveCallback leave)
{
	PamgrSyscallEnterCallback callback = pamgrState.syscallEnterCallback;

	pamgrState.syscallEnterObserver = enter;
	pamgrState.syscallLeaveObserver = leave;
	if (callback == NULL && pamgrState.syscallLeaveCallback == NULL)
		SceIntrmgrForKernel_217AC350(dispatchSyscallEnter, dispatchSyscallLeave);
}

int ScePamgrForDriver_7F3814A4(uint32_t *values)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	values[0] = pfm->gpuSampledData[0];
	values[1] = pfm->gpuSampledData[1];
	values[2] = pfm->gpuSampledData[2];
	values[3] = pfm->gpuSampledData[3];
	return 0;
}

/* Distinct user entries add the user-pointer NULL gate. */
int configureGpuTraceForUser(
				const struct PamgrGpuTraceConfig *config)
{
	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return configureGpuTrace(config);
}

int clearGpuTraceForUser(void)
{
	return clearGpuTrace();
}

/* User readback adds a NULL gate around the inlined reads. */
int getGpuSampledDataForUser(uint32_t *values)
{
	ScePerfmonRegisters *pfm;

	if (values == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pfm = pamgrState.pfmRegisters;
	values[0] = pfm->gpuSampledData[0];
	values[1] = pfm->gpuSampledData[1];
	values[2] = pfm->gpuSampledData[2];
	values[3] = pfm->gpuSampledData[3];
	return 0;
}

unsigned int sceKernelPaGetWritePointer(void)
{
	sceKernelApiDeclareTypeUInt32();
	return pamgrState.traceBufferVirtual - pamgrState.traceBufferPhysical +
	       pamgrState.pfmRegisters->traceWritePointer;
}

int sceKernelPaInsertBookmark(unsigned int bank, unsigned int index, uint32_t value)
{
	return ScePamgrForDriver_16A24732(bank, index, value);
}

uint32_t ScePamgrForDriver_019BD60F(unsigned int buffer)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	uint32_t status = pfm->traceStatus;

	if (buffer == 0) {
		if (status & 0x10000)
			return 0;
		if (status & 8)
			return pfm->traceBufferSize;
		return pfm->traceWritePointer - pfm->traceBufferBase;
	}
	if (buffer == 1) {
		if (status & 0x20000)
			return 0;
		if (status & 0x10)
			return pfm->traceBuffer1Size;
		return pfm->traceBuffer1WritePointer - pfm->traceBuffer1Base;
	}
	return 0;
}

uint32_t sceKernelPaGetTraceBufferSize(unsigned int buffer)
{
	return ScePamgrForDriver_019BD60F(buffer);
}

/* Request the trace control state and wait for acknowledgement. */
int __attribute__((noinline)) waitTraceControl(struct PamgrPtmRegisters *trace)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	uint32_t start;

	trace->control |= ETM_CONTROL_PROGRAMMING;
	start = pfm->timebaseLow;
	for (;;) {
		if (trace->status & ETM_STATUS_PROGRAMMING)
			return 0;
		if ((uint32_t)(pfm->timebaseLow - start) > PAMGR_TIMEOUT)
			return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
	}
}

/* Record kinds 1 and 2 encode two words and packed header fields;
 * kind03 accepts its complete lower header bits from the caller. */
void writeTraceRecord02(uint32_t first, uint32_t second, uint16_t field16,
			uint8_t field8)
{
	pamgrState.pfmRegisters->traceFifo =
		0x020c0000 | (uint16_t)(field16 << 8) | field8;
	pamgrState.pfmRegisters->traceFifo = first;
	pamgrState.pfmRegisters->traceFifo = second;
}

void writeTraceRecord03(uint32_t first, uint32_t second, uint32_t header)
{
	pamgrState.pfmRegisters->traceFifo = 0x030c0000 | header;
	pamgrState.pfmRegisters->traceFifo = first;
	pamgrState.pfmRegisters->traceFifo = second;
}

void writeTraceRecord01(uint32_t first, uint32_t second, uint16_t field16,
			uint8_t field8)
{
	pamgrState.pfmRegisters->traceFifo =
		0x010c0000 | (uint16_t)(field16 << 8) | field8;
	pamgrState.pfmRegisters->traceFifo = first;
	pamgrState.pfmRegisters->traceFifo = second;
}

/* Shared QA permission check used before enabling tracing. */
int checkDebugPermission(void)
{
	return QafCheckCachedDebugPermission(&pamgrState.debugPermission);
}

/* Sony inlined this stable high/low timebase read in both APIs. */
static inline __attribute__((always_inline)) uint64_t readPfmTimebase(void)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	uint32_t high, low;

	do {
		high = pfm->timebaseHigh;
		low = pfm->timebaseLow;
	} while (high != pfm->timebaseHigh);
	return (uint64_t)high << 32 | low;
}

uint64_t ScePamgrForDriver_FE9BFD27(void)
{
	return readPfmTimebase();
}

unsigned int ScePamgrForDriver_EB70556B(void)
{
	return 333;
}

unsigned int ScePamgrForDriver_FB46C3CD(void)
{
	return pamgrState.pfmRegisters->traceWritePointer;
}

/* Change the aligned hardware trace write pointer. */
int setTraceWritePointer(uint32_t pointer)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (pointer & 0x7f)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pfm->traceWritePointer = pointer;
	return 0;
}

/* Distinct driver/user ABI entries select the trace channel. */
int startPrimaryTrace(void)
{
	return startTrace(0);
}

int startSecondaryTrace(void)
{
	return startTrace(1);
}

int stopPrimaryTrace(void)
{
	return stopTrace(0);
}

int stopSecondaryTrace(void)
{
	return stopTrace(1);
}

unsigned int ScePamgrForDriver_3ACF4316(void)
{
	return pamgrState.pfmRegisters->traceStatus;
}

unsigned int ScePamgrForDriver_D4E85C87(void)
{
	return pamgrState.pfmRegisters->traceInterruptStatus;
}

/* Write only the supported trace interrupt state bits. */
int setTraceInterruptStatus(uint32_t value)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (value & 0xfffefffc)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pfm->traceInterruptStatus = value;
	return 0;
}

/* Hardware trace interrupt and callback registration. */
int traceInterruptHandler(int code, void *arg)
{
	(void)arg;
	if (code == 0xf4) {
		if (pamgrState.traceInterruptCallback != NULL)
			pamgrState.traceInterruptCallback(
				pamgrState.pfmRegisters->traceInterruptStatus,
				pamgrState.traceInterruptArg);
		pamgrState.pfmRegisters->traceInterruptStatus = 0x10003;
	}
	return -1;
}

int unregisterTraceInterruptCallback(void)
{
	int result;

	if (pamgrState.traceInterruptCallback == NULL)
		return SCE_KERNEL_ERROR_PA_ERROR;
	result = sceKernelDisableIntr(0xf4);
	pamgrState.traceInterruptCallback = NULL;
	pamgrState.traceInterruptArg = NULL;
	return result;
}

int registerTraceInterruptCallback(PamgrTraceInterruptCallback callback, void *arg)
{
	int result;

	if (pamgrState.traceInterruptCallback != NULL)
		return SCE_KERNEL_ERROR_PA_ERROR;
	pamgrState.traceInterruptCallback = callback;
	pamgrState.traceInterruptArg = arg;
	pamgrState.pfmRegisters->traceInterruptStatus = 0x10003;
	result = sceKernelEnableIntr(0xf4);
	if (result < 0)
		unregisterTraceInterruptCallback();
	return result;
}

int startPrimaryTraceForUser(void)
{
	return startTrace(0);
}

int stopPrimaryTraceForUser(void)
{
	return stopTrace(0);
}

unsigned int sceKernelPaGetTraceBufferStatus(void)
{
	sceKernelApiDeclareTypeUInt32();
	return pamgrState.pfmRegisters->traceStatus;
}

/* User bookmark-channel mask forwards to the common setter. */
int setTraceRecordMaskForUser(unsigned int mask, uint32_t selection)
{
	return setTraceRecordMask(mask, selection);
}

uint64_t sceKernelPaGetTimebaseValue(void)
{
	sceKernelApiDeclareTypeUInt64();
	return readPfmTimebase();
}

unsigned int sceKernelPaGetTimebaseFrequency(void)
{
	sceKernelApiDeclareTypeUInt32();
	return 333;
}

unsigned int sceKernelPaGetIoBaseAddress(void)
{
	sceKernelApiDeclareTypeUInt32();
	return 0x80000000;
}

/* Start one trace channel and synchronize its control blocks. */
int startTrace(unsigned int channel)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	struct PamgrPtmRegisters *trace;
	uint32_t start, callbacks;
	unsigned int i;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (channel == 0) {
		if (pfm->traceBufferControl & 4)
			return 0;
		pfm->traceBufferEnable = 1;
	} else {
		pfm->traceBuffer1Enable = 1;
		if (!pamgrState.traceRecordCallbacksEnabled) {
			clearSyscallTraceCallbacks();
		} else {
			if (pamgrState.traceRecordMask & 2)
				SceThreadmgrForKernel_A3975A5A(writeTraceRecord02);
			if (pamgrState.traceRecordMask & 4)
				SceThreadmgrForKernel_F885ECCA(writeTraceRecord03);
			if (pamgrState.traceRecordMask & 1)
				SceThreadmgrForKernel_9605DF7D(writeTraceRecord01);
			callbacks = pamgrState.traceRecordMask & 0x18;
			if (callbacks == 0x18)
				setSyscallCallbacks(sceKernelPaSetSyscallEnter,
						    sceKernelPaSetSyscallLeave);
			else if (callbacks & 8)
				setSyscallCallbacks(sceKernelPaSetSyscallEnter, NULL);
			else if (callbacks & 0x10)
				setSyscallCallbacks(NULL, sceKernelPaSetSyscallLeave);
		}
		if (pamgrState.traceCallbackTransition == 1)
			pamgrState.traceCallbackTransition = 0;
	}
	pfm = pamgrState.pfmRegisters;
	if (pfm->traceEventControl & PFM_EVENT_ARM_TRACE) {
		for (i = 0; i < 4; i++) {
			trace = pamgrState.ptm[i].registers;
			trace->control &= ~ETM_CONTROL_PROGRAMMING;
			start = pfm->timebaseLow;
			while (trace->status & ETM_STATUS_PROGRAMMING) {
				if ((uint32_t)(pfm->timebaseLow - start) > PAMGR_TIMEOUT)
					return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
			}
		}
	}
	start = pfm->timebaseLow;
	for (;;) {
		if ((pfm->traceStatus & 0x10000) ||
		    (pfm->traceStatus & 0x20000) || (pfm->traceStatus & 1))
			return 0;
		if ((uint32_t)(pfm->timebaseLow - start) > PAMGR_TIMEOUT)
			return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
	}
}

/* Stop tracing after flushing the TPIU. */
int stopTrace(unsigned int channel)
{
	ScePerfmonRegisters *pfm = pamgrState.pfmRegisters;
	struct PamgrTpiuRegisters *tpiu;
	uint32_t start;
	unsigned int i;
	int result;

	if (!(pfm->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	if (channel == 0 && (pfm->traceBufferControl & 8))
		return 0;
	if (pfm->traceEventControl & PFM_EVENT_ARM_TRACE) {
		for (i = 0; i < 4; i++) {
			result = waitTraceControl(pamgrState.ptm[i].registers);
			if (result != 0)
				return result;
		}
		pamgrState.tpiuRegisters->formatterControl =
			TPIU_FORMATTER_STOP_ON_FLUSH | TPIU_FORMATTER_ENABLE;
		pamgrState.tpiuRegisters->formatterControl =
			TPIU_FORMATTER_STOP_ON_FLUSH | TPIU_FORMATTER_FLUSH | TPIU_FORMATTER_ENABLE;
		pfm = pamgrState.pfmRegisters;
		tpiu = pamgrState.tpiuRegisters;
		start = pfm->timebaseLow;
		while (!(tpiu->formatterStatus & TPIU_FORMATTER_STOPPED)) {
			if ((uint32_t)(pfm->timebaseLow - start) > PAMGR_TIMEOUT)
				return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
		}
	}
	if (channel == 0) {
		pamgrState.pfmRegisters->traceBufferEnable = 0;
	} else {
		if (pamgrState.traceRecordCallbacksEnabled)
			clearSyscallTraceCallbacks();
		if (pamgrState.traceCallbackTransition == 0)
			pamgrState.traceRecordCallbacksEnabled = 0;
		pamgrState.pfmRegisters->traceBuffer1Enable = 0;
	}
	pfm = pamgrState.pfmRegisters;
	start = pfm->timebaseLow;
	for (;;) {
		if ((pfm->traceStatus & 0x10000) ||
		    (pfm->traceStatus & 0x20000) || !(pfm->traceStatus & 1))
			return 0;
		/* The original only checks elapsed time on hardware mode0x20. */
		if (SceSysrootForKernel_403B509E() == 0x20 &&
		    (uint32_t)(pfm->timebaseLow - start) > PAMGR_TIMEOUT)
			return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
	}
}

/* Unlock the CoreSight blocks and configure four ARM traces. */
int configureArmTrace(const volatile struct PamgrArmTraceConfig *config)
{
	struct PamgrCoreSightRegisters *block;
	struct PamgrPtmRegisters *trace;
	struct PamgrTpiuRegisters *tpiu;
	uint32_t control, mode;
	uint16_t timestampPeriod, counter2Period;
	unsigned int i;
	int result;

	if (!(pamgrState.pfmRegisters->traceControl & PFM_TRACE_ENABLED))
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	control = config->control;
	if ((control & (ETM_CONTROL_RETURN_STACK | ETM_CONTROL_BRANCH_OUTPUT)) ==
	    (ETM_CONTROL_RETURN_STACK | ETM_CONTROL_BRANCH_OUTPUT) ||
	    (control & 0xcfff2eff) ||
	    (uint32_t)((config->modeAndContext & 0xffffff) - 1) > 5)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < 4; i++) {
		block = pamgrState.dbg[i].registers;
		block->lockAccess = CORESIGHT_LOCK_KEY;
		if (block->lockStatus != 1)
			return -1;
		block = pamgrState.pmu[i].registers;
		block->lockAccess = CORESIGHT_LOCK_KEY;
		if (block->lockStatus != 1)
			return -1;
		block = pamgrState.cti[i].registers;
		block->lockAccess = CORESIGHT_LOCK_KEY;
		if (block->lockStatus != 1)
			return -1;
		trace = pamgrState.ptm[i].registers;
		trace->lockAccess = CORESIGHT_LOCK_KEY;
		if (trace->lockStatus != 1)
			return -1;
	}
	tpiu = pamgrState.tpiuRegisters;
	tpiu->lockAccess = CORESIGHT_LOCK_KEY;
	if (tpiu->lockStatus != 1)
		return -1;
	block = pamgrState.funnel.registers;
	block->lockAccess = CORESIGHT_LOCK_KEY;
	if (block->lockStatus != 1)
		return -1;
	if (!(tpiu->supportedPortSize & TPIU_PORT_SIZE_32))
		return SCE_KERNEL_ERROR_UNSUP;
	tpiu->currentPortSize = TPIU_PORT_SIZE_32;
	pamgrState.tpiuRegisters->formatterControl = TPIU_FORMATTER_ENABLE;
	pamgrState.funnel.registers->control = FUNNEL_PORTS;
	for (i = 0; i < 4; i++) {
		trace = pamgrState.ptm[i].registers;
		trace->control = 0;
		result = waitTraceControl(trace);
		if (result != 0)
			return result;
		control = config->control;
		if (control & ETM_CONTROL_TIMESTAMP)
			timestampPeriod = config->timestampPeriod;
		trace->control |= config->control;
		mode = config->modeAndContext;
		if (control & ETM_CONTROL_TIMESTAMP) {
			trace->counter1ReloadValue = timestampPeriod;
			trace->counter1Value = timestampPeriod;
			trace->counter1ReloadEvent = ETM_RESOURCE_COUNTER_ZERO(0);
			trace->counter1EnableEvent = ETM_RESOURCE_ALWAYS;
			trace->timestampEvent = ETM_RESOURCE_COUNTER_ZERO(0);
		} else {
			trace->timestampEvent = ETM_EVENT_NEVER;
		}
		switch (mode & 0xffffff) {
		case 1:
			trace->traceEnableEvent = ETM_RESOURCE_ALWAYS;
			break;
		case 2:
			trace->traceEnableEvent = ETM_RESOURCE_COUNTER_ZERO(1);
			break;
		case 3:
			trace->traceEnableEvent = ETM_RESOURCE_NON_SECURE;
			break;
		case 4:
			trace->traceEnableEvent = ETM_EVENT_AND(ETM_RESOURCE_COUNTER_ZERO(1),
								ETM_RESOURCE_NON_SECURE);
			break;
		case 5:
			trace->traceEnableEvent = ETM_RESOURCE_CONTEXT_ID(0);
			break;
		case 6:
			trace->traceEnableEvent = ETM_EVENT_AND(ETM_RESOURCE_COUNTER_ZERO(1),
								ETM_RESOURCE_CONTEXT_ID(0));
			break;
		}
		counter2Period = config->counter2Period;
		trace->contextIdComparatorValue = mode >> 24;
		trace->contextIdComparatorMask = 0xffffff00;
		trace->counter2ReloadValue = counter2Period;
		trace->counter2Value = counter2Period;
		trace->counter2ReloadEvent = ETM_RESOURCE_COUNTER_ZERO(1);
		trace->counter2EnableEvent = ETM_RESOURCE_ALWAYS;
		trace->traceEnableControl1 = ETM_TRACE_ENABLE_EXCLUDE;
		trace->triggerEvent = ETM_EVENT_NEVER;
		trace->syncFrequency = 0x400;
		trace->traceId = i + 1;
	}
	pamgrState.pfmRegisters->traceEventControl |=PFM_EVENT_ARM_TRACE;
	return 0;
}

int resumeTraceHardware(int value, void *args)
{
	ScePerfmonRegisters *pfm;
	int primary, secondary;
	uint32_t control;

	if (sceKernelCpuId() != 0)
		return 0;
	control = pamgrState.savedPmuControl;
	/* PMCR is a privileged CP15 register. */
	writePmcr(control);
	pfm = pamgrState.pfmRegisters;
	if (!(pfm->traceControl & PFM_TRACE_ENABLED)) {
		pfm->traceConfigHalf = pamgrState.savedTraceConfigHalf;
		pfm->traceConfig = pamgrState.savedTraceConfig;
		pfm->traceControl = PFM_TRACE_ENABLED;
	}
	primary = pamgrState.resumePrimaryTrace;
	secondary = pamgrState.resumeSecondaryTrace;
	if (primary != 1 && secondary != 1) {
		pfm->traceEventMask0 = 0xffff;
		pfm->traceEventMask1 = 0xffff;
		return 0;
	}
	pfm->traceBufferControl = pamgrState.savedTraceBufferControlBits |
				  pamgrState.savedTraceControl;
	pfm->traceBufferBase = pamgrState.savedTraceBufferBase;
	pfm->traceWritePointer = pamgrState.savedTraceWritePointer;
	pfm->traceBufferSize = pamgrState.savedTraceBufferSize;
	pfm->traceBufferLimit = pamgrState.savedTraceBufferLimit;
	pfm->traceBuffer1Base = pamgrState.savedTraceBuffer1Base;
	pfm->traceBuffer1WritePointer = pamgrState.savedTraceBuffer1WritePointer;
	pfm->traceBuffer1Size = pamgrState.savedTraceBuffer1Size;
	pfm->traceEventMask0 = pamgrState.savedTraceEventMask0;
	pfm->traceEventMask1 = pamgrState.savedTraceEventMask1;
	pfm->sampleControl0 = pamgrState.savedSampleControl0;
	pfm->sampleControl1 = pamgrState.savedSampleControl1;
	pfm->traceSampleControls[0] = pamgrState.savedTraceSampleControls[0];
	pfm->traceSampleControls[1] = pamgrState.savedTraceSampleControls[1];
	pfm->traceSampleControls[2] = pamgrState.savedTraceSampleControls[2];
	pfm->traceSampleControls[3] = pamgrState.savedTraceSampleControl3;
	pfm->buffer1SampleControls[0] = pamgrState.savedBuffer1SampleControls[0];
	pfm->buffer1SampleControls[1] = pamgrState.savedBuffer1SampleControls[1];
	pfm->buffer1SampleControls[2] = pamgrState.savedBuffer1SampleControls[2];
	pfm->buffer1SampleControls[3] = pamgrState.savedBuffer1SampleControl3;
	if (secondary == 1) {
		pamgrState.resumeSecondaryTrace = 0;
		startTrace(1);
		primary = pamgrState.resumePrimaryTrace;
	}
	if (primary == 1) {
		pamgrState.resumePrimaryTrace = 0;
		startTrace(0);
	}
	pfm = pamgrState.pfmRegisters;
	if (pfm->traceStatus & 0x10000) {
		pfm->traceInterruptStatus |= 0x10000;
		pfm->traceBufferEnable |= 1;
	}
	return 0;
}

int suspendTraceHardware(int value, void *args)
{
	ScePerfmonRegisters *pfm;
	uint32_t high, low, started, control;

	if (sceKernelCpuId() != 0)
		return 0;
	pfm = pamgrState.pfmRegisters;
	if (pfm->traceStatus & 1) {
		if ((pfm->traceEventMask0 & 0xffff) != 0xffff) {
			control = pfm->traceBufferControl;
			low = pfm->traceBufferControl;
			pamgrState.resumePrimaryTrace = 1;
			pamgrState.savedTraceBufferControlBits = control & 8;
			pfm->traceBufferControl = low & ~8;
			stopTrace(0);
			pfm = pamgrState.pfmRegisters;
		}
		if ((pfm->traceEventMask1 & 0xffff) != 0xffff) {
			pamgrState.resumeSecondaryTrace = 1;
			if (pamgrState.traceRecordCallbacksEnabled == 1)
				pamgrState.traceCallbackTransition = 1;
			stopTrace(1);
			pfm = pamgrState.pfmRegisters;
		}
		pamgrState.savedTraceBufferBase = pfm->traceBufferBase;
		pamgrState.savedTraceWritePointer = pfm->traceWritePointer;
		pamgrState.savedTraceBufferSize = pfm->traceBufferSize;
		pamgrState.savedTraceBuffer1Base = pfm->traceBuffer1Base;
		pamgrState.savedTraceBuffer1WritePointer = pfm->traceBuffer1WritePointer;
		pamgrState.savedTraceBuffer1Size = pfm->traceBuffer1Size;
		pamgrState.savedTraceControl = pfm->traceBufferControl;
		pamgrState.savedTraceBufferLimit = pfm->traceBufferLimit;
		pamgrState.savedTraceEventMask0 = pfm->traceEventMask0;
		pamgrState.savedTraceEventMask1 = pfm->traceEventMask1;
		pamgrState.savedSampleControl0 = pfm->sampleControl0;
		pamgrState.savedSampleControl1 = pfm->sampleControl1;
		pamgrState.savedTraceSampleControls[0] = pfm->traceSampleControls[0];
		pamgrState.savedTraceSampleControls[1] = pfm->traceSampleControls[1];
		pamgrState.savedTraceSampleControls[2] = pfm->traceSampleControls[2];
		pamgrState.savedTraceSampleControl3 = pfm->traceSampleControls[3];
		pamgrState.savedBuffer1SampleControls[0] = pfm->buffer1SampleControls[0];
		pamgrState.savedBuffer1SampleControls[1] = pfm->buffer1SampleControls[1];
		pamgrState.savedBuffer1SampleControls[2] = pfm->buffer1SampleControls[2];
		pamgrState.savedBuffer1SampleControl3 = pfm->buffer1SampleControls[3];
	}
	high = pfm->timebaseHigh;
	low = pfm->timebaseLow;
	started = pfm->timebaseLow;
	pfm->traceControl = 0;
	/* The original saves timebase words and restores them to config registers. */
	pamgrState.savedTraceConfigHalf = high;
	pamgrState.savedTraceConfig = low;
	while (pfm->traceControl & PFM_TRACE_ENABLED) {
		if ((uint32_t)(pfm->timebaseLow - started) > PAMGR_TIMEOUT)
			return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
	}
	/* PMCR is a privileged CP15 register. */
	control = readPmcr();
	pamgrState.savedPmuControl = control;
	return 0;
}

int initializeTraceHardware(void)
{
	static const char *const ptmNames[4] = {
		"ScePtm0Reg", "ScePtm1Reg", "ScePtm2Reg", "ScePtm3Reg"
	};
	static const char *const dbgNames[4] = {
		"SceDbg0Reg", "SceDbg1Reg", "SceDbg2Reg", "SceDbg3Reg"
	};
	static const char *const pmuNames[4] = {
		"ScePmu0Reg", "ScePmu1Reg", "ScePmu2Reg", "ScePmu3Reg"
	};
	static const char *const ctiNames[4] = {
		"SceCti0Reg", "SceCti1Reg", "SceCti2Reg", "SceCti3Reg"
	};
	static const struct PamgrTimebaseProvider provider = {
		sizeof(provider), ScePamgrForDriver_EB70556B,
		ScePamgrForDriver_FE9BFD27, ScePamgrForDriver_328C006F
	};
	SceKernelAllocMemBlockKernelOpt opt;
	ScePerfmonRegisters *pfm;
	SceUID block;
	void *base;
	unsigned int i;
	int result, affinity;
	uint32_t control = ARM_PMON_PMCR;

	for (i = 0; i < 4; i++) {
		pamgrState.ptm[i].registers = NULL;
		pamgrState.ptm[i].memBlock = 0;
		pamgrState.dbg[i].registers = NULL;
		pamgrState.dbg[i].memBlock = 0;
		pamgrState.pmu[i].registers = NULL;
		pamgrState.pmu[i].memBlock = 0;
		pamgrState.cti[i].registers = NULL;
		pamgrState.cti[i].memBlock = 0;
	}
	pamgrState.pfmRegisters = NULL;
	pamgrState.pfmMemBlock = 0;
	pamgrState.tpiuRegisters = NULL;
	pamgrState.tpiuMemBlock = 0;
	pamgrState.funnel.registers = NULL;
	pamgrState.funnel.memBlock = 0;
	pamgrState.itm.registers = NULL;
	pamgrState.itm.memBlock = 0;
	pamgrState.mappedPl310Registers = NULL;
	pamgrState.pl310MemBlock = 0;
	memset((void *)&pamgrState.primaryTraceConfig, 0xcd,
	       sizeof(pamgrState.primaryTraceConfig));
	memset(&pamgrState.gpuTraceConfig, 0xcd, sizeof(pamgrState.gpuTraceConfig));
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = PERFMON_REG_BASE;
	block = sceKernelAllocMemBlockForKernel("ScePfmReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x2000, &opt);
	if (block < 0)
		return block;
	pamgrState.pfmMemBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	pamgrState.pfmRegisters = base;
	for (i = 0; i < 4; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 2;
		opt.paddr = CORESIGHT_PTM_BASE(i);
		block = sceKernelAllocMemBlockForKernel(ptmNames[i], SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
		if (block < 0)
			return block;
		pamgrState.ptm[i].memBlock = block;
		sceKernelGetMemBlockBase(block, &base);
		pamgrState.ptm[i].registers = base;
	}
	for (i = 0; i < 4; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 2;
		opt.paddr = CORESIGHT_DBG_BASE(i);
		block = sceKernelAllocMemBlockForKernel(dbgNames[i], SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
		if (block < 0)
			return block;
		pamgrState.dbg[i].memBlock = block;
		sceKernelGetMemBlockBase(block, &base);
		pamgrState.dbg[i].registers = base;
	}
	for (i = 0; i < 4; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 2;
		opt.paddr = CORESIGHT_PMU_BASE(i);
		block = sceKernelAllocMemBlockForKernel(pmuNames[i], SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
		if (block < 0)
			return block;
		pamgrState.pmu[i].memBlock = block;
		sceKernelGetMemBlockBase(block, &base);
		pamgrState.pmu[i].registers = base;
	}
	for (i = 0; i < 4; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 2;
		opt.paddr = CORESIGHT_CTI_BASE(i);
		block = sceKernelAllocMemBlockForKernel(ctiNames[i], SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
		if (block < 0)
			return block;
		pamgrState.cti[i].memBlock = block;
		sceKernelGetMemBlockBase(block, &base);
		pamgrState.cti[i].registers = base;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = CORESIGHT_TPIU_BASE;
	block = sceKernelAllocMemBlockForKernel("SceTpiuReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (block < 0)
		return block;
	pamgrState.tpiuMemBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	pamgrState.tpiuRegisters = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = CORESIGHT_FUNNEL_BASE;
	block = sceKernelAllocMemBlockForKernel("SceFunnelReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (block < 0)
		return block;
	pamgrState.funnel.memBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	pamgrState.funnel.registers = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = CORESIGHT_ITM_BASE;
	block = sceKernelAllocMemBlockForKernel("SceItmReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (block < 0)
		return block;
	pamgrState.itm.memBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	pamgrState.itm.registers = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = PL310_REG_BASE;
	block = sceKernelAllocMemBlockForKernel("ScePl310Reg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (block < 0)
		return block;
	pamgrState.pl310MemBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	pamgrState.pl310Registers = base;
	pamgrState.mappedPl310Registers = base;
	pfm = pamgrState.pfmRegisters;
	if (pfm->traceControl & PFM_TRACE_ENABLED)
		return SCE_KERNEL_ERROR_ALREADY_REGISTERED;
	result = sceKernelRegisterIntrHandler(0xf4, "SceKernelPaPmoni", 0,
					     traceInterruptHandler, NULL, 0xd0, 0xf, NULL);
	if (result < 0)
		return result;
	pfm = pamgrState.pfmRegisters;
	pfm->traceEventMask0 = 0xffff;
	pfm->traceEventMask1 = 0xffff;
	pfm->traceConfigHalf = 0;
	pfm->traceConfig = 0;
	pfm->traceControl = PFM_TRACE_ENABLED;
	affinity = sceKernelGetThreadCpuAffinityMask(0);
	for (i = 0; i < 4; i++) {
		sceKernelChangeThreadCpuAffinityMask(0, 1 << i);
		/* Each core's PMCR requires an actual privileged CP15 write. */
		writePmcr(control);
	}
	sceKernelChangeThreadCpuAffinityMask(0, affinity);
	SceSysrootForDriver_C94C76FA(&provider);
	SceKernelSuspendForDriver_CE7A2207(0xb, suspendTraceHardware, NULL);
	SceKernelSuspendForDriver_105C5752(0xb, resumeTraceHardware, NULL);
	return 0;
}

int module_start(void)
{
	if (SceDipswForDriver_A98FC2FD(0xd4) == 0)
		return 1;
	if (initializeTraceHardware() < 0)
		return 2;
	if (sceKernelPaInitPamgrWrapper() < 0)
		return 2;
	pamgrState.syscallLeaveObserver = NULL;
	pamgrState.syscallEnterObserver = NULL;
	pamgrState.syscallEnterCallback = NULL;
	pamgrState.syscallLeaveCallback = NULL;
	return 0;
}
