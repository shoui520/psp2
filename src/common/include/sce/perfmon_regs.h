/*
 * The Perfmon block (physical 0xe50d0000, "the Perfmon registers" in
 * SceGpuEs4Init's messages; PFM in ScePamgr): two register banks, the
 * timebase, and the trace and counter units ScePamgr drives. The first page
 * is the user one (Sysmem maps it into processes as SceUserIOPA, the GPU
 * maps it for tools), the second the kernel one. Names follow ScePamgr's use.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define PERFMON_REG_BASE		0xe50d0000
#define PERFMON_PAGE_SIZE		0x1000

/* ScePerfmonRegisters.traceControl */
#define PFM_TRACE_ENABLED		0x01

/* ScePerfmonRegisters.traceEventControl: the trace sources and filters switched on */
#define PFM_EVENT_ARM_TRACE		0x01
#define PFM_EVENT_PACKED_FILTER		0x02
#define PFM_EVENT_WORD_FILTER		0x04
#define PFM_EVENT_GPU_TRACE		0x08
#define PFM_EVENT_COUNTER_TRACE		0x10

typedef struct ScePerfmonRegisters {
	volatile uint32_t registerBank0[16];
	volatile uint32_t registerBank0Status;
	uint8_t reserved0[0x3c];
	volatile uint32_t traceBufferEnable;
	volatile uint32_t traceStatus;
	volatile uint32_t timebaseLow;
	volatile uint32_t timebaseHigh;
	uint8_t reserved1[0xf70];
	union {
		volatile uint32_t registerBank1[16];
		struct {
			uint8_t reserved2[0x18];
			volatile uint32_t traceFifo;
			uint8_t reserved3[0x24];
		};
	};
	volatile uint32_t registerBank1Status;
	volatile uint32_t sampleControl0;
	volatile uint32_t traceEventMask0;
	volatile uint32_t sampleControl1;
	volatile uint32_t traceEventMask1;
	uint8_t reserved4[0x2c];
	volatile uint32_t traceEventControl;
	volatile uint32_t traceBufferBase;
	volatile uint32_t traceWritePointer;
	volatile uint32_t traceBufferSize;
	volatile uint32_t traceBufferLimit;
	volatile uint32_t traceBufferControl;
	volatile uint32_t traceInterruptStatus;
	volatile uint32_t traceBitControls[4];
	volatile uint32_t traceSampleControls[4];
	volatile uint32_t traceControl;
	volatile uint32_t traceConfig;
	volatile uint32_t traceConfigHalf;
	volatile uint32_t counterTraceBytes;
	volatile uint32_t traceAuxControl0;
	volatile uint32_t tracePackedFilter;
	volatile uint32_t counterTraceSelection;
	volatile uint32_t traceWordFilter;
	volatile uint32_t counterTraceControl;
	volatile uint32_t counterTraceValues[3];
	volatile uint32_t traceAuxControl1;
	/* 0x10f0: Debug/PA's bus error log, address and attribute */
	volatile uint32_t busErrorLog[2];
	volatile uint32_t secureBusErrorLog[2];
	volatile uint32_t traceBuffer1Enable;
	volatile uint32_t traceBuffer1Base;
	volatile uint32_t traceBuffer1WritePointer;
	volatile uint32_t traceBuffer1Size;
	volatile uint32_t buffer1SampleControls[4];
	volatile uint32_t sampleStatus;
	volatile uint32_t counterTraceFlags;
	uint8_t reserved5[0x18];
	volatile uint32_t gpuCounterConfig[8];
	volatile uint32_t gpuSampledData[4];
	volatile uint32_t gpuSampleConfig[8];
	volatile uint32_t gpuControl;
	volatile uint32_t gpuIntervals;
} ScePerfmonRegisters;

_Static_assert(offsetof(ScePerfmonRegisters, timebaseLow) == 0x88, "Perfmon timebase");
_Static_assert(offsetof(ScePerfmonRegisters, registerBank1) == 0x1000, "Perfmon kernel page");
_Static_assert(offsetof(ScePerfmonRegisters, busErrorLog) == 0x10f0, "Perfmon Debug/PA bus error");
