/* SceTouchDummy: touch emulation for kits without a touch panel, and the kernel API.
 * Private names and fields without a recovered SDK name remain unknown.
 */
#include "private.h"

struct touchStorage storage;

static SceUInt32 noOpAlarm(void *args) /* name unknown */
{
	return 0;
}

static SceUInt32 samplingTimer(void *args);

static int vblankCallback(void *args, void *context, unsigned char priority) /* name unknown */
{
	touch.timer = SceThreadmgrForDriver_C58DF384("SceTouchAlarm0", 1000, samplingTimer, NULL);
	if (touch.timer < 0)
		touch.timer = -1;
	return -1;
}

static int createPhysicalProcess(int pid) /* name unknown */
{
	struct touchProcess *process;
	struct touchProcessPort *port;
	SceTouchPanelInfo *panel;
	unsigned intr, bit;
	int ret, slot, i, r;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 1);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	for (slot = 0; slot < 16; slot++)
		if (touch.pids[slot] == -1)
			break;
	if (slot == 16) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return SCE_TOUCH_ERROR_FATAL;
	}
	touch.pids[slot] = pid;
	touch.processCount++;
	memset(process, 0, sizeof(*process));
	process->pid = pid;
	process->privilege = 0;
	process->state = 0;
	SceProcessmgrForDriver_D141C076(pid, &process->sdkVersion);
	for (i = 0; i < 2; i++) {
		port = &process->ports[i];
		initializeRing(&port->ring);
		bit = allocateBit(&touch.ports[i].allocated);
		port->eventBit = bit;
		touch.ports[i].stopped &= ~bit;
		for (r = 1; r < 8; r++)
			port->regions[r].active = 0;
		panel = &touch.ports[i].panel;
		port->regions[0].active = 1;
		port->regions[0].rect.x = panel->minAaX;
		port->regions[0].rect.y = panel->minAaY;
		port->regions[0].rect.width = panel->maxAaX + 1 - panel->minAaX;
		port->regions[0].rect.height = panel->maxAaY + 1 - panel->minAaY;
		port->regions[0].attr = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

static int systemEvent(int resume, int eventId, void *args, void *opt);
static int createProcess(int pid, ProcEventCreateParam *param, void *common);
static int killProcess(int pid, ProcEventParam *param, void *common);
static int exitProcess(int pid, ProcEventParam *param, void *common);
static int suspendProcess(int pid, int phase, ProcEventParam *param, void *common);
static int resumeProcess(int pid, int phase, ProcEventParam *param, void *common);

static const SceKernelProcEventHandler procHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = createProcess,
	.exit = exitProcess,
	.kill = killProcess,
	.suspend = suspendProcess,
	.resume = resumeProcess,
};

static int initializePhysical(void *args) /* name unknown */
{
	SceTouchPanelInfo *panel;
	unsigned i;

	for (i = 0; i < 2; i++) {
		panel = &touch.ports[i].panel;
		panel->minAaX = 0;
		panel->minAaY = i ? 108 : 0;
		panel->maxAaX = 1919;
		panel->maxAaY = i ? 889 : 1087;
		panel->minDispX = 0;
		panel->minDispY = 0;
		panel->maxDispX = 1919;
		panel->maxDispY = 1087;
		panel->minForce = 1;
		panel->maxForce = 128;
		touch.ports[i].densityX = 17.54f;
		touch.ports[i].densityY = 17.54f;
		touch.ports[i].timeout = 10000000;
		touch.ports[i].forceLevel[0] = 255;
		touch.ports[i].forceLevel[1] = 16;
	}
	touch.timer = -1;
	touch.state = 2;
	touch.sysHandler = sceKernelRegisterSysEventHandler("SceTouch", systemEvent, NULL);
	touch.procHandler = sceKernelRegisterProcEventHandler("SceTouch", &procHandlers, NULL);
	touch.plsId = sceKernelRegisterKPLS("SceTouch", sizeof(struct touchProcess));
	if (touch.plsId >= 0) {
		createPhysicalProcess(0x10005);
		createVirtualProcess(0x10005);
		sceKernelRegisterSubIntrHandler(8, 5, "SceTouchVblank", vblankCallback, NULL);
		sceKernelEnableSubIntr(8, 5);
	}
	return 1;
}

static int createProcess(int pid, ProcEventCreateParam *param, void *common) /* name unknown */
{
	int ret = createPhysicalProcess(pid);

	if (ret < 0)
		return ret;
	return createVirtualProcess(pid);
}

static int systemEvent(int resume, int eventId, void *args, void *opt) /* name unknown */
{
	unsigned intr;

	if (resume == 0) {
		switch (eventId) {
		case 0x400:
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
			touch.state = 3;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			break;
		case 0x401:
			if (!touch.activeMask && !touch.ports[0].forceMode && !touch.ports[1].forceMode &&
			    touch.state != 4)
				touch.state = 4;
			break;
		case 0x402:
			return touch.state == 4 ? 0 : -1;
		case 0x400c:
			sceKernelDisableSubIntr(8, 5);
			touch.state = 1;
			if (touch.timer != -1) {
				sceKernelCancelAlarm(touch.timer);
				touch.timer = -1;
			}
			break;
		}
	} else if (resume == 1) {
		if (eventId == 0x1000c) {
			touch.activeMask = 0;
			touch.modeMask = 0;
			touch.ports[0].forceChange = 0;
			touch.ports[1].forceChange = 0;
			sceKernelEnableSubIntr(8, 5);
		} else if (eventId == 0x100000) {
			touch.state = 1;
			SceThreadmgrForDriver_C58DF384("SceTouchAlarm1", 100000, noOpAlarm, NULL);
		}
	}
	return 0;
}

static int resumeProcess(int pid, int phase, ProcEventParam *param, void *common) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret;

	if (phase != 0x10000)
		return 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret < 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	if (process) {
		touch.ports[0].stopped &= ~process->ports[0].eventBit;
		touch.ports[1].stopped &= ~process->ports[1].eventBit;
		process->state = 4;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return resumeVirtualProcess(pid);
}

static int findOwner(unsigned port, const short *x, const short *y, struct touchOwner *owner) /* name unknown */
{
	struct touchProcess *process;
	struct touchRegion *region;
	unsigned i;
	int r;

	owner->region = 0xffff;
	owner->pid = -1;
	if (touch.ports[port].enableState)
		return -1;
	for (i = 0; i < touch.priorityCount; i++) {
		if (touch.priorities[i] == -1)
			continue;
		if (lookupProcess(touch.priorities[i], touch.plsId, (void **)&process, 0) < 0)
			continue;
		if (!process || process->state)
			continue;
		for (r = 7; r >= 0; r--) {
			region = &process->ports[port].regions[r];
			if (region->active && region->rect.x <= *x && region->rect.y <= *y &&
			    region->rect.x + region->rect.width > *x &&
			    region->rect.y + region->rect.height > *y) {
				owner->attr = region->attr;
				owner->region = r;
				owner->pid = touch.priorities[i];
				return i;
			}
		}
	}
	return -1;
}

static int removePhysicalProcess(int pid) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr, bit;
	int ret, slot, i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	if (!touch.processCount)
		goto fatal;
	for (slot = 0; slot < 16; slot++)
		if (touch.pids[slot] == pid)
			break;
	if (slot == 16)
		goto fatal;
	touch.processCount--;
	touch.pids[slot] = -1;
	for (i = 0; i < 2; i++) {
		bit = process->ports[i].eventBit;
		touch.ports[i].stopped &= ~bit;
		if (touch.ports[i].allocated & bit) {
			sceKernelClearEventFlag(touch.ports[i].events[0], ~bit);
			bit = process->ports[i].eventBit;
			if (touch.ports[i].allocated & bit)
				sceKernelClearEventFlag(touch.ports[i].events[1], ~bit);
		}
		touch.ports[i].allocated &= ~process->ports[i].eventBit;
	}
	memset(process, 0, sizeof(*process));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;

fatal:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return SCE_TOUCH_ERROR_FATAL;
}

static int killProcess(int pid, ProcEventParam *param, void *common) /* name unknown */
{
	int ret = removePhysicalProcess(pid);

	if (ret < 0)
		return ret;
	return removeVirtualProcess(pid);
}

static int exitProcess(int pid, ProcEventParam *param, void *common) /* name unknown */
{
	int ret = removePhysicalProcess(pid);

	if (ret < 0)
		return ret;
	return removeVirtualProcess(pid);
}

static int updateOwners(unsigned port, struct touchSample *sample, struct touchTracking *tracking) /* name unknown */
{
	struct touchProcess *process;
	struct touchOwner owner;
	SceTouchReport *report;
	struct touchNode *node;
	unsigned i, info;

	for (node = tracking->added; node; node = node->next) {
		report = &sample->data.report[node->reportIndex];
		findOwner(port, &report->x, &report->y, &node->owner);
	}
	for (node = tracking->active; node; node = node->next) {
		if (node->owner.pid == -1)
			continue;
		if (lookupProcess(node->owner.pid, touch.plsId, (void **)&process, 0) < 0)
			continue;
		if (!process || process->state == 3)
			continue;
		report = &sample->data.report[node->reportIndex];
		findOwner(port, &report->x, &report->y, &owner);
		if (owner.pid == -1) {
			report->info = 1;
			continue;
		}
		if (owner.pid == node->owner.pid) {
			if ((short)node->owner.region == (short)owner.region)
				continue;
			report->info = (short)node->owner.region < (short)owner.region ? 1 : 0x100;
			continue;
		}
		if (node->owner.pid == -1)
			continue;
		info = 0x100;
		for (i = 0; i < touch.priorityCount; i++) {
			if (lookupProcess(touch.priorities[i], touch.plsId, (void **)&process, 0) < 0)
				continue;
			if (!process || process->state == 3)
				continue;
			if (touch.priorities[i] == node->owner.pid)
				break;
			if (touch.priorities[i] == owner.pid) {
				info = 1;
				break;
			}
		}
		report->info = info;
	}
	if (tracking->added)
		moveNodes(&tracking->added, &tracking->active);
	for (node = tracking->active; node; node = node->next)
		sample->owners[node->reportIndex] = node->owner;
	return 0;
}

static void copySample(struct touchSample *dest, const struct touchSample *src, unsigned state) /* name unknown */
{
	unsigned r;

	dest->data.status = src->data.status;
	if (state)
		dest->data.status = src->data.status | 0x10000;
	dest->data.reportNum = src->data.reportNum;
	for (r = 0; r < src->data.reportNum && r < 8; r++) {
		dest->data.report[r] = src->data.report[r];
		dest->owners[r] = src->owners[r];
	}
}

static SceUInt32 samplingTimer(void *args) /* name unknown */
{
	struct touchSample *front, *back;
	struct touchProcess *process;
	unsigned intr, processed;
	int i, ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	if (touch.virtualDriver)
		sampleVirtual(touch.virtualDriver->getSample);
	memcpy(&storage.front, &storage.emulation[0], sizeof(SceTouchData));
	memcpy(&storage.back, &storage.emulation[1], sizeof(SceTouchData));
	storage.front.data.timeStamp = sceKernelGetSystemTimeWide();
	storage.back.data.timeStamp = storage.front.data.timeStamp;
	if (touch.ctrlCallback) {
		touch.ctrlCallback(0, &storage.front.data);
		touch.ctrlCallback(1, &storage.back.data);
	}
	if (storage.front.data.reportNum)
		touch.ports[0].lastTouchTime = storage.front.data.timeStamp;
	if (storage.back.data.reportNum)
		touch.ports[1].lastTouchTime = storage.back.data.timeStamp;
	if (((touch.idleMask & 1) && storage.front.data.reportNum && !touch.ports[0].enableState) ||
	    ((touch.idleMask & 2) && storage.back.data.reportNum && !touch.ports[1].enableState))
		sceKernelPowerTick(0);
	updateTracking(&touch.ports[0].tracking, &storage.front.data);
	updateTracking(&touch.ports[1].tracking, &storage.back.data);
	updateOwners(0, &storage.front, &touch.ports[0].tracking);
	updateOwners(1, &storage.back, &touch.ports[1].tracking);
	processed = 0;
	for (i = 0; i < 16; i++) {
		if (processed >= touch.processCount)
			break;
		if (touch.pids[i] == -1)
			continue;
		ret = lookupProcess(touch.pids[i], touch.plsId, (void **)&process, 0);
		if (ret < 0 || ret != 0 || process->state == 3)
			continue;
		front = nextRingSample(&process->ports[0].ring);
		back = nextRingSample(&process->ports[1].ring);
		SceProcessmgrForDriver_8CB01675(touch.pids[i], &front->data.timeStamp,
						storage.front.data.timeStamp);
		copySample(front, &storage.front, process->state);
		SceProcessmgrForDriver_8CB01675(touch.pids[i], &back->data.timeStamp,
						storage.back.data.timeStamp);
		copySample(back, &storage.back, process->state);
		processed++;
		advanceRing(&process->ports[0].ring);
		advanceRing(&process->ports[1].ring);
	}
	sceKernelSetEventFlag(touch.ports[0].events[0], touch.ports[0].allocated & ~touch.ports[0].stopped);
	sceKernelSetEventFlag(touch.ports[0].events[1], touch.ports[0].allocated & ~touch.ports[0].stopped);
	sceKernelSetEventFlag(touch.ports[1].events[0], touch.ports[1].allocated & ~touch.ports[1].stopped);
	sceKernelSetEventFlag(touch.ports[1].events[1], touch.ports[1].allocated & ~touch.ports[1].stopped);
	touch.generation++;
	if (touch.ports[0].enableState == 1)
		touch.ports[0].enableState = 2;
	if (touch.ports[1].enableState == 1)
		touch.ports[1].enableState = 2;
	touch.timer = -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

static int suspendProcess(int pid, int phase, ProcEventParam *param, void *common) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret;

	switch (phase) {
	case 0x200:
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
		if (ret < 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return ret;
		}
		if (process) {
			if (lastRingSample(&process->ports[0].ring)->data.reportNum |
			    lastRingSample(&process->ports[1].ring)->data.reportNum)
				process->state = 1;
			else
				process->state = 2;
			process->timestamp = SceThreadmgrForDriver_47F6DE49();
			process->generation = touch.generation;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return suspendVirtualProcess(pid);
	case 0x202:
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
		if (ret >= 0 && process) {
			if (process->state == 2)
				ret = 0;
			else if (process->generation == touch.generation &&
				 SceThreadmgrForDriver_47F6DE49() - process->timestamp <= 34000)
				ret = -1;
			else {
				ret = 0;
				process->state = 2;
			}
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		if (ret >= 0)
			ret = checkVirtualProcess(pid);
		return ret;
	case 0x800:
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
		if (ret < 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return ret;
		}
		if (process) {
			touch.ports[0].stopped |= process->ports[0].eventBit;
			touch.ports[1].stopped |= process->ports[1].eventBit;
			process->state = 3;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return stopVirtualProcess(pid);
	default:
		return 0;
	}
}

int module_start(SceSize arglen, const void *argp)
{
	memset(storage.emulation, 0, sizeof(storage.emulation) + sizeof(storage.physical));
	memset(touch.pids, -1, sizeof(touch.pids));
	memset(touch.priorities, -1, sizeof(touch.priorities));
	touch.ports[0].reportMax = 6;
	touch.ports[1].reportMax = 4;
	touch.idleMask = 3;
	touch.ports[0].events[0] = sceKernelCreateEventFlag("SceTouchFront", 0x9000, 0, NULL);
	if (touch.ports[0].events[0] >= 0)
		touch.ports[0].events[1] = sceKernelCreateEventFlag("SceTouchFrontExt", 0x9000, 0, NULL);
	touch.ports[1].allocated = 0;
	touch.ports[1].stopped = 0;
	touch.ports[1].events[0] = sceKernelCreateEventFlag("SceTouchBack", 0x9000, 0, NULL);
	if (touch.ports[1].events[0] >= 0)
		touch.ports[1].events[1] = sceKernelCreateEventFlag("SceTouchBackExt", 0x9000, 0, NULL);
	initializeTracking(&touch.ports[0].tracking);
	initializeTracking(&touch.ports[1].tracking);
	touch.lock = 0;
	sceKernelScheduleWorkTask(0x10023, "SceTouchInit", initializePhysical, NULL);
	if (initializeVirtual() < 0 || initializeUser() < 0)
		return 2;
	return 0;
}

/* the reads of the kernel API, each with its own copy of this */
static inline __attribute__((always_inline)) int readPhysical(unsigned port, SceTouchData *out,
	unsigned count, unsigned regions, int kind, int peek) /* name unknown */
{
	struct touchProcess *process = NULL;
	struct touchProcessPort *state;
	unsigned intr, flags, bit;
	int ret, pid;

	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (!out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (count > 64 || (regions & 0xffffff00))
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	state = &process->ports[port];
	flags = (process->sampling[kind] >> port) & 1 ? 0x80000000 : 0;
	if ((process->force[kind] >> port) & 1)
		flags |= 0x40;
	if (!peek) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		if (!(state->eventBit & touch.ports[port].allocated))
			return SCE_TOUCH_ERROR_INVALID_ARG;
		ret = sceKernelWaitEventFlag(touch.ports[port].events[kind], state->eventBit,
					     SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
		if (ret < 0)
			return ret;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		bit = state->eventBit;
		if (bit & touch.ports[port].allocated)
			sceKernelClearEventFlag(touch.ports[port].events[kind], ~bit);
	}
	if (touch.priorityEnabled)
		flags |= process->privilege ? 0x30 : 0x10;
	flags |= peek ? 2 : 1;
	if (process->sdkVersion < 0x2500000)
		flags |= 0x80;
	ret = readRing(&process->ports[port].ring, kind, out, count, process->pid, regions, flags);
	if (process->state == 4)
		process->state = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int sceTouchRead(unsigned port, SceTouchData *out, unsigned count)
{
	return readPhysical(port, out, count, 1, 0, 0);
}

int sceTouchPeek(unsigned port, SceTouchData *out, unsigned count)
{
	return readPhysical(port, out, count, 1, 0, 1);
}

int sceTouchReadRegion(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 0, 0);
}

int sceTouchPeekRegion(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 0, 1);
}

int readRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions) /* name unknown */
{
	return readPhysical(port, out, count, regions, 1, 0);
}

int peekRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions) /* name unknown */
{
	return readPhysical(port, out, count, regions, 1, 1);
}

static inline __attribute__((always_inline)) int setSampling(unsigned port, unsigned state,
	int kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned char *sampling;
	unsigned intr, old, bit;
	int ret, pid;

	if (port > 1 || state > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	sampling = &process->sampling[kind];
	old = (*sampling >> port) & 1;
	if (state != old) {
		if (state == 1) {
			resetRingReader(&process->ports[port].ring, kind);
			bit = process->ports[port].eventBit;
			if (bit & touch.ports[port].allocated)
				sceKernelClearEventFlag(touch.ports[port].events[kind], ~bit);
		}
		*sampling = (state << port) | (*sampling & ~(1 << port));
	}
	process->state = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return old;
}

int sceTouchSetSamplingState(unsigned port, unsigned state)
{
	return setSampling(port, state, 0);
}

int setSamplingStateExt(unsigned port, unsigned state) /* name unknown */
{
	return setSampling(port, state, 1);
}

int SceTouchForDriver_328E38B0(unsigned port, int enable)
{
	unsigned intr;
	struct touchTracking *tracking;

	if (port >= 2)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	tracking = &touch.ports[port].tracking;
	if (enable)
		touch.ports[port].enableState = 0;
	else if (!touch.ports[port].enableState)
		touch.ports[port].enableState = tracking->added || tracking->active ? 1 : 2;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

static inline __attribute__((always_inline)) int getSampling(unsigned port, unsigned *state,
	int kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (!state)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	*state = (process->sampling[kind] >> port) & 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchGetSamplingState(unsigned port, unsigned *state)
{
	return getSampling(port, state, 0);
}

int getSamplingStateExt(unsigned port, unsigned *state) /* name unknown */
{
	return getSampling(port, state, 1);
}

static inline __attribute__((always_inline)) int setForce(unsigned port, unsigned enable,
	int kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	process->force[kind] = (enable << port) | (process->force[kind] & ~(1 << port));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchEnableTouchForce(unsigned port)
{
	return setForce(port, 1, 0);
}

int enableTouchForceExt(unsigned port) /* name unknown */
{
	return setForce(port, 1, 1);
}

int sceTouchDisableTouchForce(unsigned port)
{
	return setForce(port, 0, 0);
}

int disableTouchForceExt(unsigned port) /* name unknown */
{
	return setForce(port, 0, 1);
}

static int validRegion(unsigned port, const struct touchRect *rect) /* name unknown */
{
	SceTouchPanelInfo *panel = &touch.ports[port].panel;

	return rect->x >= panel->minAaX && rect->y >= panel->minAaY &&
		rect->width > 0 && rect->height > 0 &&
		rect->x + rect->width - 1 <= panel->maxAaX &&
		rect->y + rect->height - 1 <= panel->maxAaY;
}

int sceTouchSetRegion(unsigned port, unsigned region, const short *rect, unsigned attr)
{
	const struct touchRect *area = (const struct touchRect *)rect;
	struct touchProcess *process;
	struct touchRegion *dest;
	unsigned intr;
	int ret, pid;

	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (region > 7)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (!area || !validRegion(port, area))
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		__builtin_memcpy(&dest->rect, area, sizeof(dest->rect));
		dest->active = 1;
		dest->attr = attr;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_E1C5D95F(unsigned port, unsigned region)
{
	struct touchProcess *process;
	struct touchRegion *dest;
	unsigned intr;
	int ret, pid;

	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (region > 7)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		dest->active = 0;
		dest->attr = 0;
		__builtin_memset(&dest->rect, 0, sizeof(dest->rect));
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int sceTouchSetRegionAttr(unsigned port, unsigned region, unsigned attr)
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= 2 || region >= 8)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		process->ports[port].regions[region].attr = attr;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int sceTouchActivateRegion(unsigned port, unsigned region, unsigned enable)
{
	struct touchProcess *process;
	struct touchRegion *dest;
	SceTouchPanelInfo *panel;
	unsigned intr;
	int ret, pid;

	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (region > 7)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (enable > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		if (enable == 1) {
			panel = &touch.ports[port].panel;
			if (dest->rect.x + dest->rect.width - 1 > panel->maxAaX ||
			    dest->rect.y + dest->rect.height - 1 > panel->maxAaY ||
			    panel->minAaX > dest->rect.x || panel->minAaY > dest->rect.y)
				ret = SCE_TOUCH_ERROR_INVALID_ARG;
			else
				dest->active = 1;
		} else {
			dest->active = 0;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_3F3FBF69(const TouchProcessPriorities *priorities)
{
	unsigned intr, count, i;

	if (!priorities)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	count = priorities->count;
	touch.priorityEnabled = 1;
	for (i = 0; i < count; i++)
		touch.priorities[i] = priorities->processIds[i];
	touch.priorityCount = count;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchSetCtrlpCallback(int (*callback)(unsigned, SceTouchData *))
{
	unsigned intr;

	if (!callback)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	touch.ctrlCallback = callback;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int SceTouchForDriver_03C3AC7C(const TouchVirtualDriver *driver)
{
	if (!driver)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	touch.virtualDriver = driver;
	return 0;
}

void SceTouchForDriver_845E2115(void)
{
}

int sceTouchGetPanelInfo(unsigned port, SceTouchPanelInfo *out)
{
	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (!out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	*out = touch.ports[port].panel;
	return 0;
}

int getPixelDensity(float *front, float *back) /* name unknown */
{
	if (!back || !front)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	*front = touch.ports[0].densityX;
	*back = touch.ports[1].densityX;
	return 0;
}

int getPixelDensity2(float *frontX, float *frontY, float *backX, float *backY) /* name unknown */
{
	if (!frontX || !frontY || !backX || !backY)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	*frontX = touch.ports[0].densityX;
	*frontY = touch.ports[0].densityY;
	*backX = touch.ports[1].densityX;
	*backY = touch.ports[1].densityY;
	return 0;
}

int setProcessPrivilege(unsigned privilege) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	process->privilege = privilege;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchGetProcessInfo(unsigned port, void *out)
{
	return 0;
}

int sceTouchEnableIdleTimerCancelSetting(unsigned port, int enable)
{
	if (port >= 2)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (enable)
		touch.idleMask |= 1U << port;
	else
		touch.idleMask &= ~(1U << port);
	return 0;
}

void SceTouchForDriver_062A11A2(void)
{
}

int SceTouchForDriver_9C8CCB66(void)
{
	return 0;
}

int SceTouchForDriver_384F6C68(unsigned port, unsigned timeout)
{
	unsigned intr;

	if (port >= 2)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	touch.ports[port].timeout = timeout;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchGetDeviceInfo(unsigned port, struct touchDeviceInfo *out)
{
	struct touchDeviceInfo *info;
	unsigned short vendor, product, firmware;
	unsigned char detail[2];
	unsigned intr;

	if (port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (!out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	__builtin_memset(out, 0, sizeof(*out));
	info = &touch.ports[port].deviceInfo;
	vendor = info->vendor;
	product = info->product;
	firmware = info->firmware;
	detail[0] = info->detail[0];
	detail[1] = info->detail[1];
	out->vendor = vendor;
	out->product = product;
	out->firmware = firmware;
	out->detail[0] = detail[0];
	out->detail[1] = detail[1];
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int SceTouchForDriver_63DCF2FE(void)
{
	return touch.state != 0;
}

int SceTouchForDriver_A32529B8(const SceTouchData *front, const SceTouchData *back)
{
	unsigned intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);

	memcpy(&storage.emulation[0], front, sizeof(SceTouchData));
	memcpy(&storage.emulation[1], back, sizeof(SceTouchData));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchUpdateInit(void)
{
	return 0;
}

int SceTouchForDriver_423F26D3(void)
{
	return 0;
}

int sceTouchUpdateRestart(unsigned panels)
{
	return 0;
}
