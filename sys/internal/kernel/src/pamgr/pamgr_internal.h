#ifndef PAMGR_INTERNAL_H
#define PAMGR_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <psp2kern/kernel/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <intrmgr/intrmgr_syscall.h>
#include <sysmem/pl310_regs.h>
#include <pamgr/perfmon_regs.h>
#include <pamgr/coresight_regs.h>
#include <sysmem/cp15.h>
#include <pamgr/pamgr_trace.h>
#include <sysmem/sysroot.h>

typedef void (*PamgrTraceInterruptCallback)(uint32_t status, void *arg);
typedef void (*PamgrSyscallEnterCallback)(ThreadSyscallFrame *frame);
typedef void (*PamgrSyscallLeaveCallback)(ThreadSyscallFrame *frame,
					 uint32_t *results);

/* Counter trace configuration, with observed width constraints. */
struct PamgrCounterTraceConfig {
	uint16_t selection;
	uint16_t interval;
	uint32_t values[3];
	uint32_t control;
	uint8_t upperByte;
	uint8_t lowerByte;
	uint16_t flags;
};

/* Two bank controls and two groups of four hardware words. */
struct PamgrSampleConfig {
	uint32_t bank0Control;
	uint32_t bank1Control;
	uint32_t traceControls[4];
	uint32_t buffer1Controls[4];
};

/* ETMLAR unlock key, ETMCR/ETMSR programming bit */
/* arm_pmon.c: the Cortex-A9 performance monitor, per thread */
#define ARM_PMON_COUNTERS		6
#define ARM_PMON_COUNTER_MASK		((1U << ARM_PMON_COUNTERS) - 1)
#define ARM_PMON_CYCLE_COUNTER		31	/* the counter number of PMCCNTR */
#define ARM_PMON_ENABLE_ALL		(PMCNTEN_C | ARM_PMON_COUNTER_MASK)
#define ARM_PMON_PMCR			(PMCR_E | PMCR_X)

/* armPmonExecAllThread's operations */
#define ARM_PMON_OP_RESET		1
#define ARM_PMON_OP_SET_EVENT		2
#define ARM_PMON_OP_START		3
#define ARM_PMON_OP_STOP		4
#define ARM_PMON_OP_SET_COUNTER		5

/* sceKernelSetThreadAccessLevel's level while pamgr works for a user call */
#define PAMGR_ACCESS_LEVEL		0x80

#define CORESIGHT_LOCK_KEY		0xc5acce55
#define ETM_CONTROL_PROGRAMMING		0x400
#define ETM_STATUS_PROGRAMMING		0x2

/* PTM events: a resource, or a function of two (ETMv3 event encoding) */
#define ETM_RESOURCE_COUNTER_ZERO(n)	(0x40 + (n))	/* counter n + 1 reached zero */
#define ETM_RESOURCE_CONTEXT_ID(n)	(0x58 + (n))	/* context ID comparator n + 1 */
#define ETM_RESOURCE_NON_SECURE		0x6d
#define ETM_RESOURCE_ALWAYS		0x6f
#define ETM_EVENT_NOT(a)		((1 << 14) | (a))
#define ETM_EVENT_AND(a, b)		((2 << 14) | ((b) << 7) | (a))
#define ETM_EVENT_NEVER			ETM_EVENT_NOT(ETM_RESOURCE_ALWAYS)

/* ETMCR: branch broadcast, timestamps, the return stack */
#define ETM_CONTROL_BRANCH_OUTPUT	0x00000100
#define ETM_CONTROL_TIMESTAMP		0x10000000
#define ETM_CONTROL_RETURN_STACK	0x20000000
#define ETM_TRACE_ENABLE_EXCLUDE	0x01000000	/* ETMTECR1: exclude no range */

/* TPIU: the 32-bit port; FFCR formatting, manual flush, stop on flush; FFSR */
#define TPIU_PORT_SIZE_32		0x80000000
#define TPIU_FORMATTER_ENABLE		0x0001
#define TPIU_FORMATTER_FLUSH		0x0040
#define TPIU_FORMATTER_STOP_ON_FLUSH	0x1000
#define TPIU_FORMATTER_STOPPED		0x2

/* funnel: the slave ports of the four PTMs and the ITM */
#define FUNNEL_PORTS			0x17

/* how long a register handshake may take, in PFM timebase ticks */
#define PAMGR_TIMEOUT			0x10000000

/* a CoreSight PTM (Cortex-A9 program trace macrocell): ETMCR, ETMTRIGGER,
 * ETMSR, ETMTEEVR, ETMTECR1, the counters, ETMCIDCVR/CMR, ETMSYNCFR,
 * ETMTSEVR, ETMTRACEIDR, ETMLAR/LSR */
struct PamgrPtmRegisters {
	volatile uint32_t control;
	uint8_t reserved0[4];
	volatile uint32_t triggerEvent;
	uint8_t reserved1[4];
	volatile uint32_t status;
	uint8_t reserved2[0x0c];
	volatile uint32_t traceEnableEvent;
	volatile uint32_t traceEnableControl1;
	uint8_t reserved3[0x118];
	volatile uint32_t counter1ReloadValue;
	volatile uint32_t counter2ReloadValue;
	uint8_t reserved4[8];
	volatile uint32_t counter1EnableEvent;
	volatile uint32_t counter2EnableEvent;
	uint8_t reserved5[8];
	volatile uint32_t counter1ReloadEvent;
	volatile uint32_t counter2ReloadEvent;
	uint8_t reserved6[8];
	volatile uint32_t counter1Value;
	volatile uint32_t counter2Value;
	uint8_t reserved7[0x38];
	volatile uint32_t contextIdComparatorValue;
	uint8_t reserved8[8];
	volatile uint32_t contextIdComparatorMask;
	uint8_t reserved9[0x20];
	volatile uint32_t syncFrequency;
	uint8_t reserved10[0x14];
	volatile uint32_t timestampEvent;
	uint8_t reserved11[4];
	volatile uint32_t traceId;
	uint8_t reserved12[0xdac];
	volatile uint32_t lockAccess;
	volatile uint32_t lockStatus;
};

struct PamgrCoreSightRegisters {
	volatile uint32_t control;
	uint8_t reserved[0xfac];
	volatile uint32_t lockAccess;
	volatile uint32_t lockStatus;
};

struct PamgrCoreSightMapping {
	struct PamgrCoreSightRegisters *registers;
	SceUID memBlock;
};

struct PamgrPtmMapping {
	struct PamgrPtmRegisters *registers;
	SceUID memBlock;
};

/* the CoreSight TPIU ("SceTpiuReg"): port sizes, the formatter and flush */
struct PamgrTpiuRegisters {
	volatile uint32_t supportedPortSize;
	volatile uint32_t currentPortSize;
	uint8_t reserved0[0x2f8];
	volatile uint32_t formatterStatus;
	volatile uint32_t formatterControl;
	uint8_t reserved1[0xca8];
	volatile uint32_t lockAccess;
	volatile uint32_t lockStatus;
};

struct PamgrArmTraceConfig {
	uint32_t control;
	uint32_t modeAndContext;
	uint16_t timestampPeriod;	/* counter 1 reload: a timestamp each time it expires */
	uint16_t counter2Period;	/* counter 2 reload */
};

/* Packed filter halves, upper limited to 12 bits by hardware. */
struct PamgrPackedFilter {
	uint16_t upper;
	uint16_t lower;
};

struct PamgrTraceBufferConfig {
	uint32_t base;
	uint32_t size;
	uint16_t eventMask0;
	uint16_t eventMask1;
};

/* Remaining storage is kept at its current listing offsets until its users
 * have been converted and the field meanings recovered. */
struct PamgrState {
	PamgrSyscallLeaveCallback syscallLeaveCallback;
	PamgrSyscallLeaveCallback syscallLeaveObserver;
	ScePl310Registers *pl310Registers;
	PamgrSyscallEnterCallback syscallEnterCallback;
	PamgrSyscallEnterCallback syscallEnterObserver;
	uint32_t savedTraceControl;
	ScePerfmonRegisters *pfmRegisters;
	SceUID pfmMemBlock;
	uint32_t savedTraceBufferControlBits;
	volatile uint32_t savedTraceConfigHalf;
	uint32_t savedBuffer1SampleControl3;
	volatile uint32_t savedTraceConfig;
	uint32_t savedTraceEventMask0;
	uint32_t savedTraceBufferBase;
	uint32_t savedTraceEventMask1;
	ScePl310Registers *mappedPl310Registers;
	SceUID pl310MemBlock;
	int traceCallbackTransition;
	uint32_t traceRecordMask;
	uint32_t savedSampleControl0;
	struct PamgrCoreSightMapping pmu[4];
	struct PamgrGpuTraceConfig gpuTraceConfig;
	struct PamgrPtmMapping ptm[4];
#if FW_VERSION >= 0x04000000
	uint32_t savedTraceSampleControls[3];
	uint32_t savedTraceSampleControl3;
	uint32_t savedTraceBuffer1Size;
	struct PamgrCoreSightMapping dbg[4];
	int debugPermission;
	uint32_t savedTraceBufferSize;
	uint32_t savedTraceBuffer1WritePointer;
	uint32_t traceBufferPhysical;
	struct PamgrTpiuRegisters *tpiuRegisters;
	SceUID tpiuMemBlock;
	uint32_t savedTraceBufferLimit;
	struct PamgrCoreSightMapping itm;
	uint32_t traceBufferVirtual;
	uint32_t savedPmuControl;
#else
	uint32_t savedTraceSampleControls[3];
	struct PamgrCoreSightMapping funnel;
	uint32_t savedTraceBuffer1Size;
	int debugPermission;
	struct PamgrCoreSightMapping dbg[4];
	uint32_t savedTraceBufferSize;
	uint32_t savedTraceBuffer1WritePointer;
	uint32_t traceBufferPhysical;
	struct PamgrTpiuRegisters *tpiuRegisters;
	SceUID tpiuMemBlock;
	uint32_t savedTraceBufferLimit;
	struct PamgrCoreSightMapping itm;
	uint32_t savedPmuControl;
#endif
	volatile struct PamgrPrimaryTraceConfig primaryTraceConfig;
#if FW_VERSION >= 0x04000000
	uint32_t savedBuffer1SampleControls[3];
	uint32_t savedTraceBuffer1Base;
	struct PamgrCoreSightMapping funnel;
#else
	uint32_t savedBuffer1SampleControls[3];
	uint32_t savedTraceBuffer1Base;
	uint32_t traceBufferVirtual;
	uint32_t savedTraceSampleControl3;
#endif
	PamgrTraceInterruptCallback traceInterruptCallback;
	uint32_t savedSampleControl1;
	int resumePrimaryTrace;
	int resumeSecondaryTrace;
	uint32_t savedTraceWritePointer;
	void *traceInterruptArg;
	int traceRecordCallbacksEnabled;
	struct PamgrCoreSightMapping cti[4];
	SceKernelFastMutex registrationMutex;
	uint32_t registrationId;
	SceUID registrationProcess;
	uint32_t sharedKeyFirst;
	uint32_t sharedKeySecond;
	int registrationShared;
	uint8_t reserved[4];
};

_Static_assert(sizeof(struct PamgrState) == 0x1f8, "Pamgr storage size");
_Static_assert(offsetof(struct PamgrState, pfmRegisters) == 0x18,
	       "Pamgr PFM pointer");
_Static_assert(offsetof(struct PamgrState, traceInterruptCallback) == 0x164,
	       "Pamgr trace interrupt callback");
_Static_assert(offsetof(struct PamgrState, primaryTraceConfig) == 0x138,
	       "Pamgr primary trace configuration");
_Static_assert(offsetof(struct PamgrState, traceRecordMask) == 0x48,
	       "Pamgr trace callback record mask");
_Static_assert(offsetof(struct PamgrState, traceInterruptArg) == 0x178,
	       "Pamgr trace interrupt argument");
_Static_assert(offsetof(struct PamgrState, registrationMutex) == 0x1a0,
	       "Pamgr registration mutex");
_Static_assert(offsetof(struct PamgrState, registrationId) == 0x1e0,
	       "Pamgr registration identifier");
_Static_assert(offsetof(ScePerfmonRegisters, timebaseLow) == 0x88,
	       "PFM timebase registers");
_Static_assert(offsetof(ScePerfmonRegisters, traceFifo) == 0x1018,
	       "PFM trace FIFO");
_Static_assert(offsetof(ScePerfmonRegisters, traceWritePointer) == 0x1088,
	       "PFM trace write pointer");
_Static_assert(offsetof(ScePerfmonRegisters, traceControl) == 0x10bc,
	       "PFM trace control");

extern struct PamgrState pamgrState;

#endif
