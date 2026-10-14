/*
 * fw1 runtime: what the PU0 and PU1 programs share.
 *
 * Each core runs a few cooperative tasks. A task's code and data are overlays: the
 * scheduler copies them by DMA into local RAM when the task is switched in, and
 * saves the data back to DRAM when it is switched out. A pass over the active
 * schedule (a list of task numbers) is one frame; the ARM side requests the
 * schedule and follows the core through the status block in shared DRAM.
 */
#pragma once

#ifdef VIP_FW2
/* FW2 builds fw1's shared code (DMA, rings, frame stores) on its own runtime */
#include "../../fw2/common/runtime.h"
#else

#include <vip.h>

#if defined(VIP_PROGRAM_fw1_pu0)
#define VIP_CORE		0
#define VIP_TASK_COUNT		5
#else
#define VIP_CORE		1
#define VIP_TASK_COUNT		2
#endif

#define VIP_SCHEDULE_COUNT	4
#define VIP_FRAME_RECORDS	32

/* the stack of the scheduler and of the start code, in local RAM */
#define VIP_SCHEDULER_STACK	(VIP_LOCAL_RAM + 0x2400)

/*
 * where a task's overlays go in local RAM: code after the local RAM code (which
 * starts at VIP_LOCAL_RAM + 0x90; ours is larger than the firmware's, which put
 * the task code at 0x570 and 0x170)
 */
#if defined(VIP_PROGRAM_fw1_pu0)
#define VIP_TASK_CODE		(VIP_LOCAL_RAM + 0x600)
#else
#define VIP_TASK_CODE		(VIP_LOCAL_RAM + 0x230)
#endif
#define VIP_TASK_DATA		(VIP_LOCAL_RAM + 0x2000)

/* the status blocks (VipStatus, one per core), and the debugger's registers */
#define VIP_STATUS_ADDRESS	(VIP_IMAGE + 0xc3580)
#define VIP_STATUS_SIZE		0x20
#define VIP_DEBUG_REGISTERS	(VIP_IMAGE + 0xc3800)

/* what each core writes to its first debugger register at reset */
#define VIP_DEBUG_RESET_VALUE	0x2000daf8

/* per-task and per-frame time statistics, and the trace log, in DRAM */
#define VIP_STATISTICS_ADDRESS	(VIP_IMAGE + 0xe6000)

#ifndef __ASSEMBLER__

/* code and data the core runs from its local RAM (copied in by loadLocalRam) */
#define VIP_LRAM_TEXT		__attribute__((section(".lram.text")))
#define VIP_LRAM_DATA		__attribute__((section(".lram.data")))

/* the local RAM sections: where they are loaded from, where they run, their size */
extern char __lram_text_load[], __lram_text_start[], __lram_text_size[];
extern char __lram_data_load[], __lram_data_start[], __lram_data_size[];

/* a task: its saved context while it is switched out */
typedef struct VipTask {
	uint32_t argument;		/* passed to the entry in $1 */
	uint32_t stack;			/* initial stack, then the saved $sp */
	uint32_t pc;			/* entry, then where to resume */
	uint32_t returnAddress;		/* where the task called vipTaskYield */
	uint32_t psw;
	uint32_t interruptEnable;	/* the task's bits of VIP_CB_INTERRUPT_ENABLE */
	uint32_t started;		/* its data overlay has been loaded */
	uint32_t reserved;
} VipTask;

/* one copy of a task's code overlay: words from DRAM to local RAM */
typedef struct VipOverlayCopy {
	uint32_t *destination;
	const uint32_t *source;
	uint32_t wordCount;
} VipOverlayCopy;

/* a task's overlays */
typedef struct VipTaskOverlay {
	uint32_t copyCount;		/* copies[] at switch-in */
	const VipOverlayCopy *copies;
	uint32_t code;			/* DRAM: code for VIP_TASK_CODE */
	uint32_t codeSize;
	uint32_t data;			/* DRAM: where VIP_TASK_DATA is saved */
	uint32_t dataSize;
	uint32_t initialData;		/* DRAM: data for the first switch-in */
	uint32_t initialDataDestination;
	uint32_t initialDataSize;
} VipTaskOverlay;

/* a set of tasks the ARM side can start: their entries, one per task number */
typedef struct VipTaskSet {
	uint32_t initialize;		/* reset the tasks' context too */
	const uint32_t *entries;
} VipTaskSet;

/* the status block the ARM side reads (its "debug status words") */
typedef struct VipStatus {
	volatile uint32_t state;		/* VIP_STATE_* */
	volatile uint32_t holds;		/* debug points that hold the schedule */
	volatile uint32_t command;		/* VIP_DEBUG_*, or the core's result */
	volatile uint32_t frameCount;
	volatile uint32_t requestedSchedule;
	volatile uint32_t activeSchedule;
	volatile int32_t currentTask;
	volatile uint32_t frameIndex;
} VipStatus;

#define VIP_STATUSES		((VipStatus *)VIP_STATUS_ADDRESS)
#define VIP_STATUS		(&VIP_STATUSES[VIP_CORE])
#define VIP_STATISTICS		((VipStatistic *)VIP_STATISTICS_ADDRESS)

/* VipStatus.state */
#define VIP_STATE_IDLE		0
#define VIP_STATE_RUNNING	1
#define VIP_STATE_PAUSED	2	/* debug points are paused (g_pauseFlags) */
#define VIP_STATE_RESET		3	/* the hardware was reset for the debugger */
#define VIP_STATE_DRAINED	4
#define VIP_STATE_SAVED		5	/* the core's state was saved for the debugger */
#define VIP_STATE_STOPPED	6
#define VIP_STATE_DRAINING	8
#define VIP_STATE_RESUMING	9
#define VIP_STATE_SAVING	10
#define VIP_STATE_RESTORING_1	11
#define VIP_STATE_RESTORING_2	12

/* VipStatus.command: the debugger's commands */
#define VIP_DEBUG_RESET		1
#define VIP_DEBUG_DRAIN		2
#define VIP_DEBUG_RESUME	3
#define VIP_DEBUG_SAVE		4
#define VIP_DEBUG_RESTORE_1	5
#define VIP_DEBUG_RESTORE_2	6

/*
 * g_runMode, and the task set each mode starts: normal running; restarting the
 * tasks from their entries; finishing the frame before a restart.
 */
#define VIP_RUN_NORMAL		0
#define VIP_RUN_RESTART		1
#define VIP_RUN_STOPPING	2

/* the time of each task in a frame (VIP_FRAME_RECORDS of them, round robin) */
typedef struct VipFrameRecord {
	uint32_t frameCount;
	uint32_t taskTime[11];
} VipFrameRecord;

/* a statistic: maximum, wraps, total and count */
typedef struct VipStatistic {
	uint32_t maximum;
	uint32_t wraps;
	uint32_t total;
	uint32_t count;
} VipStatistic;

/* tables of this core's program (pu0/tasks.c, pu1/tasks.c) */
extern const VipTaskSet g_taskSets[3];
extern const uint32_t g_taskIntervals[VIP_TASK_COUNT];
extern uint32_t g_taskStacks[VIP_TASK_COUNT];
extern uint32_t g_taskArguments[VIP_TASK_COUNT];
extern const VipTaskOverlay *g_taskOverlays[VIP_TASK_COUNT];
extern const int32_t *const g_schedules[VIP_SCHEDULE_COUNT];
extern volatile uint32_t g_stopAfterFrame;

/* when the frame and the current task started, by readTime() */
typedef struct VipTimes {
	uint32_t frameStart;
	uint32_t taskStart;
	uint32_t reserved;
} VipTimes;

/* time.c */
extern uint32_t g_timeBase;
extern VipTimes g_times;
extern VipFrameRecord g_frameRecords[VIP_FRAME_RECORDS];
extern uint32_t *g_frameCursor;

/* scheduler state (task.c) */
extern VipTask g_tasks[VIP_TASK_COUNT];
extern VipStatus *g_status;
extern uint32_t g_defaultInterruptEnable;
extern uint32_t g_runMode;
extern int32_t g_currentTaskId;
extern VipTask *g_currentTask;
extern const VipTaskOverlay *g_currentOverlay;

/* start code and context switches (context.S) */
void vipTaskResume(void) __attribute__((noreturn));
void vipTaskYield(void);
void vipRestartOnStack(uint32_t stack, void (*function)(void)) __attribute__((noreturn));
void vipContinueOnStack(uint32_t stack, void (*function)(uint32_t stack))
	__attribute__((noreturn));

/*
 * The core's program: its hardware, for the scheduler and the debugger (PU0's
 * decoder.c, PU1's ...).
 */
void vipIdleHardware(void);
void vipWakeHardware(void);
uint32_t vipHardwareBusy(uint32_t state);
void vipSaveHardware(void);
void vipRestoreHardware(void);
uint32_t vipResetHardware(void);

/* debug.c */
uint32_t needsRestart(void);
void updateDebugger(void);

/* task.c */
void schedulerMain(void) __attribute__((noreturn));
void schedulerTaskYielded(void) __attribute__((noreturn));
void vipHalt(uint32_t result) __attribute__((noreturn));
void saveTaskData(void);

/* the core's interrupt vectors (vectors.S): jmp instructions */
extern const uint32_t vipDmaVector, vipMailboxVector;
extern const VipOverlayCopy g_vectorCopy;	/* the others */

/* interrupt.c */
void installInterruptVectors(void);
void installVectors(void);
void copyVectorCode(void);
void configureInterrupts(void);
void enableInterrupts(void);
void enableDmaInterrupt(void);

/* dma.c */
int waitDmaIdle(void);
void copyByDma(uint32_t chunk, uint32_t external, uint32_t local, uint32_t size);
/* Clear the repeat block registers, RPB, RPE and RPC. */
static inline void clearRepeat(void)
{
	__asm__ __volatile__("stc %0, $rpb\n\tstc %0, $rpe\n\tstc %0, $rpc" : : "r"(0));
}

int readDram(uint32_t external, uint32_t local, uint32_t size);
int writeDram(uint32_t external, uint32_t local, uint32_t size);
void readDramFully(uint32_t external, uint32_t local, uint32_t size);
void writeDramFully(uint32_t external, uint32_t local, uint32_t size);
void initializeDmaQueue(void);
void loadLocalRam(void);

/* time.c */
uint32_t readTime(void);
void delay(uint32_t cycles);
void waitTaskInterval(void);
void waitForTimer(void);
void clearWords(uint32_t *words, uint32_t count);
void clearStatistics(void);
void recordStatistic(uint32_t index, uint32_t value, VipStatistic *statistics);
void recordTaskStatistic(uint32_t index, uint32_t value);
void recordTaskTime(void);
void recordFrameTime(void);
void recordFramePeriod(void);

/* trace.c */
uint32_t getCoreNumber(void);
uint32_t getTraceTask(uint32_t channel);
void writeTrace(uint32_t channel, uint32_t first, uint32_t second);
void initializeTrace(void);

/* lram.c */
void waitForDebugger(uint32_t result, uint32_t state);

#endif /* !__ASSEMBLER__ */
#endif /* !VIP_FW2 */
