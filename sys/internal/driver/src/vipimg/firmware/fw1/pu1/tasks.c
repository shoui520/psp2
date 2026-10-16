/*
 * PU1: its tasks and schedules.
 *
 * Task 0 decodes the pictures (codec.h); its code overlay holds the read stream's
 * interrupt code, and its data overlay its stack and state. Task 1 is unused.
 */
#include "../common/runtime.h"

#define TASK_DECODER		0

/*
 * The most data task 0's overlay saves (its stack and state, up to the local RAM
 * data, which pu1.ld places after them: __task0_save_size)
 */
#define TASK_DECODER_DATA_SIZE	0xf00

void decoderTask(uint32_t argument);

/* task 0's overlays as the linker places them (pu1.ld) */
extern char __task0_text_load[], __task0_text_size[];
extern char __task0_data_load[], __task0_data_start[], __task0_data_size[];
extern char __task0_save_size[];

/* where the task's data is kept while it is switched out */
static uint64_t g_decoderTaskSave[TASK_DECODER_DATA_SIZE / 8];

static const uint32_t g_normalEntries[VIP_TASK_COUNT] = {
	[TASK_DECODER] = (uint32_t)decoderTask,
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
	[TASK_DECODER] = VIP_SCHEDULER_STACK,
};

/* the debug points the tasks answer to */
uint32_t g_taskArguments[VIP_TASK_COUNT] = { 0x20, 0x21 };

static const VipTaskOverlay g_decoderOverlay = {
	.code = (uint32_t)__task0_text_load,
	.codeSize = (uint32_t)__task0_text_size,
	.data = (uint32_t)g_decoderTaskSave,
	.dataSize = (uint32_t)__task0_save_size,
	.initialData = (uint32_t)__task0_data_load,
	.initialDataDestination = (uint32_t)__task0_data_start,
	.initialDataSize = (uint32_t)__task0_data_size,
};

const VipTaskOverlay *g_taskOverlays[VIP_TASK_COUNT] = {
	[TASK_DECODER] = &g_decoderOverlay,
};

static const int32_t g_schedule0[] = { 0, 1, -1 };
static const int32_t g_schedule1[] = { 0, -1 };
static const int32_t g_schedule2[] = { 1, -1 };
static const int32_t g_schedule3[] = { -1 };

const int32_t *const g_schedules[VIP_SCHEDULE_COUNT] = {
	g_schedule0, g_schedule1, g_schedule2, g_schedule3,
};

volatile uint32_t g_stopAfterFrame;
