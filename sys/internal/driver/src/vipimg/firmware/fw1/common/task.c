/*
 * fw1 runtime: the task scheduler.
 *
 * A frame runs the active schedule once: each listed task is switched in (its
 * overlays loaded into local RAM), resumed, and runs until it yields. The frame
 * ends at the end of the schedule, or when the ARM side requests another one; the
 * core then waits out the frame period and starts the next frame.
 */
#include "runtime.h"

#define VIP_TASK_PSW		0x10b	/* a new task: interrupts on */

VipTask g_tasks[VIP_TASK_COUNT];
VipStatus *g_status;
uint32_t g_defaultInterruptEnable;
uint32_t g_runMode;
static const int32_t *g_scheduleCursor;
int32_t g_currentTaskId;
VipTask *g_currentTask;
const VipTaskOverlay *g_currentOverlay;
static uint32_t g_tasksRun;
static int32_t g_nextTaskId;
static uint32_t g_scheduleLength;

/* Load the tasks of the active schedule from a task set; non-zero if any has an entry. */
static uint32_t startTasks(uint32_t initialize, const uint32_t *entries)
{
	const int32_t *schedule = g_schedules[g_status->activeSchedule];
	uint32_t started = 0;
	uint32_t any = 0;
	int32_t id;

	g_scheduleLength = 2;
	for (;;) {
		VipTask *task;
		uint32_t bit;

		id = *schedule++;
		if (id < 0 || id >= VIP_TASK_COUNT)
			return any;
		g_scheduleLength++;
		bit = 1u << id;
		if (started & bit)
			continue;
		started |= bit;
		task = &g_tasks[id];
		task->argument = g_taskArguments[id];
		task->stack = g_taskStacks[id];
		task->pc = entries[id];
		any |= entries[id];
		if (initialize) {
			task->psw = VIP_TASK_PSW;
			task->started = 0;
			task->interruptEnable = g_defaultInterruptEnable;
		}
	}
}

/* Start task set 0 (normal), 1 (restart) or 2 (stopping) and enter its run mode. */
static uint32_t startTaskSet(uint32_t set)
{
	if (set >= 3)
		return 0;
	g_runMode = set;
	if (set == VIP_RUN_NORMAL)
		enableDmaInterrupt();
	else
		enableInterrupts();
	return startTasks(g_taskSets[set].initialize, g_taskSets[set].entries);
}

/* Do the copies a task's code overlay asks for. */
static void copyTaskOverlay(void)
{
	const VipOverlayCopy *copy = g_currentOverlay->copies;
	uint32_t count;

	for (count = g_currentOverlay->copyCount; count; count--, copy++) {
		const uint32_t *source = copy->source;
		uint32_t *destination = copy->destination;
		uint32_t words;

		for (words = copy->wordCount; words; words--)
			*destination++ = *source++;
	}
}

static void resetSchedule(void)
{
	g_scheduleCursor = g_schedules[g_status->activeSchedule];
	g_tasksRun = 0;
}

/* Has the ARM side asked for another schedule that may be taken now? */
static uint32_t scheduleChangeRequested(void)
{
	updateDebugger();
	if (g_status->activeSchedule != g_status->requestedSchedule &&
	    g_runMode == VIP_RUN_NORMAL)
		return g_status->holds == 0;
	return 0;
}

/* Stop the tasks for a new schedule; non-zero if the frame has to end. */
static uint32_t takeScheduleChange(void)
{
	if (!scheduleChangeRequested())
		return 0;
	resetSchedule();
	if (startTaskSet(VIP_RUN_STOPPING))
		return 0;
	if (g_currentTaskId >= 0)
		saveTaskData();
	return 1;
}

/* The next task of the schedule to run, or -1 at its end. */
static int32_t nextTask(void)
{
	for (;;) {
		int32_t id = *g_scheduleCursor++;
		VipTask *task;
		uint32_t bit;

		if (id < 0)
			return id;
		task = &g_tasks[id];
		if (!task->pc)
			return -1;
		if (g_runMode == VIP_RUN_NORMAL)
			return id;
		bit = 1u << id;
		if (g_tasksRun & bit)
			continue;
		if (g_runMode == VIP_RUN_STOPPING && !task->started)
			continue;
		g_tasksRun |= bit;
		return id;
	}
}

/* Bring the switched-in task's code overlay into local RAM. */
static void loadTaskCode(void)
{
	uint32_t code = g_currentOverlay->code;

	if (code && !(code & 7))
		readDramFully(code, VIP_TASK_CODE, g_currentOverlay->codeSize);
}

/*
 * Bring the switched-in task's data into local RAM: its initial data the first
 * time, its saved data after that. The task's stack lives in its data, so the part
 * below the stack pointer, already the new stack, is left alone.
 */
static void loadTaskData(uint32_t stack)
{
	const VipTaskOverlay *overlay = g_currentOverlay;
	uint32_t local, size, skip;

	if (!g_currentTask->started) {
		g_currentTask->started = 1;
		if (overlay->initialDataSize)
			readDramFully(overlay->initialData, overlay->initialDataDestination,
				overlay->initialDataSize);
		return;
	}
	size = overlay->dataSize;
	if (!size)
		return;
	local = VIP_TASK_DATA;
	skip = 0;
	if (stack >= local && local + size >= stack) {
		skip = (stack - local) & ~7u;
		local = stack & ~7u;
	}
	readDramFully(overlay->data + skip, local, size - skip);
}

/* Save the switched-out task's data to DRAM. */
void saveTaskData(void)
{
	const VipTaskOverlay *overlay = g_currentOverlay;
	uint32_t size = overlay->dataSize;

	if (size && overlay->data && !(overlay->data & 7))
		writeDramFully(overlay->data, VIP_TASK_DATA, (size + 7) & ~7u);
}

/* Set the state the ARM side sees once the core is up. */
static void reportStarted(void)
{
	uint32_t state = g_status->state;

	if (state == VIP_STATE_SAVED) {
		if (g_status->command == VIP_DEBUG_RESTORE_1)
			state = VIP_STATE_RESTORING_1;
		else if (g_status->command == VIP_DEBUG_RESTORE_2)
			state = VIP_STATE_RESTORING_2;
		else
			state = VIP_STATE_STOPPED;
	} else if (state == VIP_STATE_RESET || state == VIP_STATE_IDLE) {
		state = VIP_STATE_RUNNING;
	} else {
		state = VIP_STATE_STOPPED;
	}
	g_status->state = state;
}

/* Stop the core for good, leaving a result for the ARM side. */
void vipHalt(uint32_t result)
{
	g_status->command = result;
	mep_di();
	for (;;)
		mep_halt();
}

static void restartTimer(void)
{
	vipWriteControlBus(VIP_CB_TIMER_CONTROL, 0);
	vipWriteControlBus(VIP_CB_TIMER_COUNT, 0xffffffff);
	vipWriteControlBus(VIP_CB_TIMER_CONTROL, 1);
}

static void runFrames(void) __attribute__((noreturn));
static void schedule(uint32_t newFrame) __attribute__((noreturn));

/* Run the switched-in task, on its own stack. */
static void runCurrentTask(void) __attribute__((noreturn));
static void runCurrentTask(void)
{
	g_times.taskStart = readTime();
	vipTaskResume();
}

static void switchInTask(uint32_t stack) __attribute__((noreturn));
static void switchInTask(uint32_t stack)
{
	loadTaskData(stack);
	runCurrentTask();
}

/* End a frame: wait out its period and count it. */
static void endFrame(void)
{
	if (g_runMode == VIP_RUN_STOPPING) {
		g_runMode = VIP_RUN_RESTART;
		vipRestartOnStack(VIP_SCHEDULER_STACK, runFrames);
	}
	if (g_runMode == VIP_RUN_RESTART)
		startTaskSet(VIP_RUN_NORMAL);
	waitForTimer();
	recordFrameTime();
	recordFramePeriod();
	if (!g_stopAfterFrame) {
		g_status->frameCount++;
		restartTimer();
	}
}

static void startFrame(void)
{
	VipFrameRecord *record;

	resetSchedule();
	g_currentTaskId = -1;
	g_times.frameStart = readTime();
	record = &g_frameRecords[g_status->frameIndex & (VIP_FRAME_RECORDS - 1)];
	g_frameCursor = record->taskTime;
	record->frameCount = g_status->frameCount;
}

/* Switch to the next task of the frame, ending frames until there is one. */
static void schedule(uint32_t newFrame)
{
	if (newFrame)
		startFrame();
	for (;;) {
		int32_t id;

		if (takeScheduleChange()) {
			g_currentTaskId = -1;
			endFrame();
			startFrame();
			continue;
		}
		id = nextTask();
		g_nextTaskId = id;
		if (g_currentTaskId >= 0 && id != g_currentTaskId)
			saveTaskData();
		if (g_nextTaskId < 0) {
			g_currentTaskId = -1;
			endFrame();
			startFrame();
			continue;
		}
		if (g_nextTaskId == g_currentTaskId)
			runCurrentTask();
		g_currentTaskId = g_nextTaskId;
		g_currentTask = &g_tasks[g_currentTaskId];
		g_currentOverlay = g_taskOverlays[g_currentTaskId];
		copyTaskOverlay();
		loadTaskCode();
		vipContinueOnStack(g_currentTask->stack & ~7u, switchInTask);
	}
}

/* A task yielded (vipTaskYield): account for it and go on with the frame. */
void schedulerTaskYielded(void)
{
	if (g_runMode == VIP_RUN_NORMAL)
		enableDmaInterrupt();
	waitTaskInterval();
	recordTaskTime();
	schedule(0);
}

/* The frame loop, on a fresh scheduler stack. */
static void runFrames(void)
{
	installInterruptVectors();
	g_status->activeSchedule = g_status->requestedSchedule;
	if (g_runMode == VIP_RUN_RESTART && !startTaskSet(VIP_RUN_RESTART))
		startTaskSet(VIP_RUN_NORMAL);
	restartTimer();
	schedule(1);
}

/* The program, after the start code: set the core up and run frames. */
void schedulerMain(void)
{
	installVectors();
	loadLocalRam();
	copyVectorCode();
	configureInterrupts();
	g_status = VIP_STATUS;
	if (needsRestart()) {
		g_runMode = VIP_RUN_RESTART;
		g_status->command = 0;
	}
	g_status->activeSchedule = 0;
	g_status->currentTask = -1;
	g_status->frameCount = 0;
	g_timeBase = 0;
	initializeDmaQueue();
	initializeTrace();
	enableDmaInterrupt();
	mep_ei();
	g_defaultInterruptEnable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	clearStatistics();
	vipWriteControlBus(VIP_CB_TIMER_ENABLE, 0);
	vipWriteControlBus(VIP_CB_TIMER_RELOAD, 0);
	vipWriteControlBus(VIP_CB_TIMER_ENABLE, 1);
	vipWriteControlBus(VIP_CB_TIMER_COUNT, 0xffffffff);
	vipWriteControlBus(VIP_CB_TIMER_CONTROL, 1);
	vipIdleHardware();
	reportStarted();
	vipRestartOnStack(VIP_SCHEDULER_STACK, runFrames);
}
