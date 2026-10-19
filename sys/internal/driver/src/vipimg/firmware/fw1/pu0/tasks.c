/*
 * PU0: its tasks and schedules.
 *
 * Task 1 demultiplexes the decoder's input (demux.h); its code overlay holds its
 * hot functions, and its data overlay its stack, buffers and state. Task 2 serves the ARM side's commands and has only its
 * stack. Tasks 0, 3 and 4 are unused.
 */
#include "../common/runtime.h"
#include "demux.h"
#include "decoder.h"

#define TASK_DEMUX		1
#define TASK_COMMAND		2

/* each task's stack: the bottom of the task data window */
#define TASK_STACK_SIZE		(VIP_SCHEDULER_STACK - VIP_TASK_DATA)

/* the local RAM task 1's data overlay covers: up to the local RAM data */
#define TASK_DEMUX_DATA_SIZE	0xc50

/* task 1's overlays as the linker places them (pu0.ld): the data is its buffers */
extern char __task1_text_load[], __task1_text_size[];
extern char __task1_data_load[], __task1_data_start[], __task1_data_size[];

/* where the tasks' data is kept while they are switched out */
static uint64_t g_demuxTaskSave[TASK_DEMUX_DATA_SIZE / 8];
static uint64_t g_commandTaskSave[TASK_STACK_SIZE / 8];

static const uint32_t g_normalEntries[VIP_TASK_COUNT] = {
	[TASK_DEMUX] = (uint32_t)demuxTask,
	[TASK_COMMAND] = (uint32_t)commandTask,
};

static const uint32_t g_restartEntries[VIP_TASK_COUNT];
static const uint32_t g_stoppingEntries[VIP_TASK_COUNT];

const VipTaskSet g_taskSets[3] = {
	[VIP_RUN_NORMAL] = { 0, g_normalEntries },
	[VIP_RUN_RESTART] = { 1, g_restartEntries },
	[VIP_RUN_STOPPING] = { 0, g_stoppingEntries },
};

const uint32_t g_taskIntervals[VIP_TASK_COUNT];

uint32_t g_taskStacks[VIP_TASK_COUNT] = {
	[TASK_DEMUX] = VIP_SCHEDULER_STACK,
	[TASK_COMMAND] = VIP_SCHEDULER_STACK,
};

uint32_t g_taskArguments[VIP_TASK_COUNT] = { 0, 0, 1, 2, 0 };

static const VipTaskOverlay g_demuxOverlay = {
	.code = (uint32_t)__task1_text_load,
	.codeSize = (uint32_t)__task1_text_size,
	.data = (uint32_t)g_demuxTaskSave,
	.dataSize = sizeof(g_demuxTaskSave),
	.initialData = (uint32_t)__task1_data_load,
	.initialDataDestination = (uint32_t)__task1_data_start,
	.initialDataSize = (uint32_t)__task1_data_size,
};

static const VipTaskOverlay g_commandOverlay = {
	.data = (uint32_t)g_commandTaskSave,
	.dataSize = sizeof(g_commandTaskSave),
};

const VipTaskOverlay *g_taskOverlays[VIP_TASK_COUNT] = {
	[TASK_DEMUX] = &g_demuxOverlay,
	[TASK_COMMAND] = &g_commandOverlay,
};

static const int32_t g_schedule0[] = { 3, 2, 1, 2, -1 };
static const int32_t g_schedule1[] = { 2, 1, 2, -1 };
static const int32_t g_schedule2[] = { 3, 1, -1 };
static const int32_t g_schedule3[] = { -1 };

const int32_t *const g_schedules[VIP_SCHEDULE_COUNT] = {
	g_schedule0, g_schedule1, g_schedule2, g_schedule3,
};

volatile uint32_t g_stopAfterFrame;
