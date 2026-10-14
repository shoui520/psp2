/*
 * fw1 runtime: the debugger on the ARM side (see debug.h). The core checks its
 * debug points as it runs, holds or pauses on them, and serves the debugger's
 * commands (VIP_DEBUG_*) between frames, stopping in waitForDebugger for the next.
 */
#include "debug.h"
#include "dma.h"

#define DEBUG_SEMAPHORE_TRIES	10

/* debug points that pause the core (setPauseFlag) */
uint32_t g_pauseFlags;

/* the points hit since they were last disabled, as in VipDebugPointSet.hit */
static uint32_t g_hitDebugPoints[2];

/* whether interrupts were on when the debug semaphores were taken */
static uint32_t g_debugInterrupts;
static uint32_t g_eventInterrupts;

static inline uint32_t pointWord(uint32_t point)
{
	return point & VIP_DEBUG_POINT_CORE1 ? 1 : 0;
}

static inline uint32_t pointBit(uint32_t point)
{
	return 1u << (point & 0x1f);
}

static inline uint32_t pointKind(const VipDebugPoint *record)
{
	return record->state >> VIP_POINT_KIND_SHIFT;
}

static uint32_t isDebugPointEnabled(uint32_t point, const VipDebugPointSet *set)
{
	return set->enabled[pointWord(point)] & pointBit(point);
}

/*
 * The set the point belongs to, if its record is of the kind (VIP_POINT_ANY: any).
 * Only the first set with points enabled is looked at.
 */
static VipDebugPointSet *findDebugPointSet(uint32_t point, uint32_t kind)
{
	VipDebugPointSet *set;

	if (point >= VIP_DEBUG_POINTS)
		return NULL;
	for (set = VIP_DEBUG->pointSets; set; set = set->next) {
		if (!set->enabled[0] && !set->enabled[1])
			continue;
		if (!isDebugPointEnabled(point, set))
			return NULL;
		if (kind != VIP_POINT_ANY && pointKind(set->points[point]) != kind)
			return NULL;
		return set;
	}
	return NULL;
}

VipDebugPoint *findDebugPoint(uint32_t point, uint32_t kind)
{
	VipDebugPointSet *set = findDebugPointSet(point, kind);

	if (!set)
		return NULL;
	return set->points[point];
}

/*
 * The core reached a debug point: mark it hit, and report what the debugger set
 * on it. A point reports its action only once, and only a VIP_POINT_ONCE point
 * with a count of 4 or more keeps it; any other is disabled with result 3.
 * -1 if the debug points are busy.
 */
int checkDebugPoint(uint32_t point, uint32_t *action, uint32_t *argument,
	uint32_t *count)
{
	VipDebugPointSet *set = NULL;
	VipDebugPoint *record;
	uint32_t word = pointWord(point), kind;

	if (takeDebugSemaphore(0))
		return -1;
	if (!(g_hitDebugPoints[word] & pointBit(point)))
		set = findDebugPointSet(point, VIP_POINT_ANY);
	if (!set) {
		*action = 0;
		releaseDebugSemaphore(0);
		return 0;
	}
	record = set->points[point];
	kind = pointKind(record);
	if (kind == VIP_POINT_ONCE)
		record->state = (record->state & 0x3f) | VIP_POINT_HIT << VIP_POINT_KIND_SHIFT;
	set->hit[word] |= pointBit(point);
	g_hitDebugPoints[word] |= pointBit(point);
	*action = record->action;
	*argument = record->argument;
	*count = record->count;
	releaseDebugSemaphore(0);
	if (*action && kind == VIP_POINT_ONCE && *count >= 4)
		return 0;
	*action = 0;
	setDebugPointResult(point, 3);
	disableDebugPoint(point);
	return 0;
}

int getDebugPointValue(uint32_t point, uint32_t which, uint32_t *value)
{
	VipDebugPoint *record = findDebugPoint(point, VIP_POINT_VALUES);

	if (!record)
		return -1;
	*value = record->values[which];
	return 0;
}

int setDebugPointResult(uint32_t point, uint32_t result)
{
	VipDebugPoint *record = findDebugPoint(point, VIP_POINT_ANY);

	if (!record)
		return -1;
	record->result = result;
	return 0;
}

int setDebugPointValue(uint32_t point, uint32_t which, uint32_t value)
{
	VipDebugPoint *record = findDebugPoint(point, VIP_POINT_VALUES);

	if (!record)
		return -1;
	record->values[which] = value;
	return 0;
}

int disableDebugPoint(uint32_t point)
{
	VipDebugPointSet *set = findDebugPointSet(point, VIP_POINT_ANY);
	VipDebugPoint *record;
	uint32_t word = pointWord(point);

	if (!set)
		return -1;
	record = set->points[point];
	if (!record)
		return -1;
	record->state &= 0x3f;
	set->enabled[word] &= ~pointBit(point);
	g_hitDebugPoints[word] &= ~pointBit(point);
	return 0;
}

/* Hold the point's core at the end of its schedule, until it is released. */
int holdSchedule(uint32_t point)
{
	VIP_STATUSES[pointWord(point)].holds |= pointBit(point);
	return 0;
}

int releaseSchedule(uint32_t point)
{
	VIP_STATUSES[pointWord(point)].holds &= ~pointBit(point);
	return 0;
}

int readDebugRegister(uint32_t offset, uint32_t *value)
{
	*value = *(volatile uint32_t *)(VIP_DEBUG_REGISTERS + offset);
	return 0;
}

int writeDebugRegister(uint32_t offset, uint32_t value)
{
	*(volatile uint32_t *)(VIP_DEBUG_REGISTERS + offset) = value;
	return 0;
}

int getDebugArea(uint32_t kind, uint32_t *address)
{
	switch (kind) {
	case 0:
		*address = VIP_DEBUG_AREA_0;
		return 0;
	case 16:
		*address = VIP_DEBUG_AREA_16;
		return 0;
	case 17:
		*address = VIP_DEBUG_AREA_17;
		return 0;
	case 1:
	case 2:
	case 32:
	case 33:
		*address = VIP_DEBUG_AREA_COMMON;
		return 0;
	default:
		*address = 0;
		return -1;
	}
}

/*
 * Empty in this build, a debug build's hook (provisional name); the decode calls
 * it first. An uncalled empty function follows it in the firmware.
 */
void debugHook(void)
{
}

/*
 * Report the result and state, and wait for the debugger's next command. Only the
 * mailbox may interrupt the wait, except after a reset, which leaves every
 * interrupt off until the command comes.
 */
void stopForDebugger(uint32_t result, uint32_t state)
{
	uint32_t enable = 0;

	vipIdleHardware();
	if (state != VIP_STATE_RESET) {
		mep_di();
		enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
		vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 1u << VIP_INTERRUPT_MAILBOX);
		__asm__ volatile ("nop\n\tnop\n\tnop");
		mep_ei();
	}
	waitForDebugger(result, state);
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable);
	__asm__ volatile ("nop\n\tnop\n\tnop");
	mep_ei();
}

/* After a resume or a pause change: running, or paused while any flag is set. */
static void updateRunState(void)
{
	switch (g_status->state) {
	case VIP_STATE_RUNNING:
	case VIP_STATE_PAUSED:
	case VIP_STATE_RESUMING:
	case VIP_STATE_RESTORING_1:
	case VIP_STATE_RESTORING_2:
		g_status->state = g_pauseFlags ? VIP_STATE_PAUSED : VIP_STATE_RUNNING;
		break;
	}
}

void setPauseFlag(uint32_t flag, uint32_t set)
{
	if (set)
		g_pauseFlags |= 1u << (flag & 0x1f);
	else
		g_pauseFlags &= ~(1u << (flag & 0x1f));
	updateRunState();
}

/* Whether the tasks are restarted when they start: the core is idle or running. */
uint32_t needsRestart(void)
{
	uint32_t state = g_status->state;

	return state == VIP_STATE_PAUSED || state == VIP_STATE_RUNNING ||
		state == VIP_STATE_IDLE;
}

/* VIP_DEBUG_RESET: reset the hardware and wait in VIP_STATE_RESET. */
static void resetCommand(void)
{
	g_pauseFlags = 0;
	mep_di();
	stopForDebugger(vipResetHardware() ? 0 : -1, VIP_STATE_RESET);
}

/* VIP_DEBUG_DRAIN: let the hardware finish, and wait in VIP_STATE_DRAINED. */
static void drainCommand(void)
{
	g_status->state = VIP_STATE_DRAINING;
	while (vipHardwareBusy(VIP_STATE_DRAINING))
		;
	stopForDebugger(0, VIP_STATE_DRAINED);
}

static void resumeCommand(void)
{
	g_status->state = VIP_STATE_RESUMING;
	g_status->command = 0;
	updateRunState();
}

/*
 * VIP_DEBUG_SAVE: once the DMA queue and the hardware are idle and no task is
 * switched in, save and reset the hardware and wait in VIP_STATE_SAVED. Non-zero
 * if the core has to get there first; the command stays for the next call.
 */
static uint32_t saveCommand(void)
{
	g_status->state = VIP_STATE_SAVING;
	if (g_dmaQueue.count || vipHardwareBusy(VIP_STATE_SAVING))
		return 1;
	if (g_currentTaskId >= 0) {
		saveTaskData();
		return 1;
	}
	mep_di();
	vipSaveHardware();
	stopForDebugger(vipResetHardware() ? 0 : -1, VIP_STATE_SAVED);
	return 0;
}

/* VIP_DEBUG_RESTORE_1, _2: restore the saved hardware and run on. */
static void restoreCommand(uint32_t state)
{
	g_status->state = state;
	mep_di();
	vipRestoreHardware();
	vipIdleHardware();
	mep_ei();
	g_status->command = 0;
	updateRunState();
}

/* Serve the debugger's commands until it has none. */
void updateDebugger(void)
{
	while (g_status->command) {
		mep_di();
		vipWakeHardware();
		mep_ei();
		switch (g_status->command) {
		case VIP_DEBUG_RESET:
			resetCommand();
			break;
		case VIP_DEBUG_DRAIN:
			drainCommand();
			break;
		case VIP_DEBUG_RESUME:
			resumeCommand();
			break;
		case VIP_DEBUG_SAVE:
			if (saveCommand())
				return;
			break;
		case VIP_DEBUG_RESTORE_1:
			restoreCommand(VIP_STATE_RESTORING_1);
			break;
		case VIP_DEBUG_RESTORE_2:
			restoreCommand(VIP_STATE_RESTORING_2);
			break;
		default:
			g_status->command = 0;
			break;
		}
	}
}

/*
 * Post events to the debugger. Those it does not keep back are delivered by a
 * mailbox interrupt and cleared; the others wait in events.
 */
int notifyDebugger(uint32_t mode, uint32_t events)
{
	uint32_t pending;

	if (mode)
		return -1;
	g_eventInterrupts = vipReadControl(VIP_CR_PSW) & VIP_PSW_IEC;
	mep_di();
	while (*VIP_DEBUG->eventSemaphore)
		__asm__ volatile ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop");
	events |= VIP_DEBUG->events;
	VIP_DEBUG->events = events;
	pending = events & ~VIP_DEBUG->eventMask;
	if (pending) {
		VIP_DEBUG->events &= VIP_DEBUG->eventMask;
		VIP_MAILBOX->debugReceive[1] = pending;
	}
	*VIP_DEBUG->eventSemaphore = 0;
	if (g_eventInterrupts)
		mep_ei();
	return 0;
}

/*
 * Take the debug points' semaphore, with interrupts off until it is released;
 * -1 if it stays taken. Reading the semaphore takes it when it reads 0.
 */
int takeDebugSemaphore(uint32_t mode)
{
	uint32_t tries;

	if (mode)
		return -1;
	g_debugInterrupts = vipReadControl(VIP_CR_PSW) & VIP_PSW_IEC;
	mep_di();
	for (tries = 0; *VIP_DEBUG->semaphore; tries++) {
		if (tries >= DEBUG_SEMAPHORE_TRIES) {
			if (g_debugInterrupts)
				mep_ei();
			return -1;
		}
		__asm__ volatile ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop");
	}
	return 0;
}

void releaseDebugSemaphore(uint32_t mode)
{
	if (mode)
		return;
	*VIP_DEBUG->semaphore = 0;
	if (g_debugInterrupts)
		mep_ei();
}
