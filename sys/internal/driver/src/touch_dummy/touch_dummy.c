/* SceTouchDummy: physical touch emulation and virtual controller touch.
 * Private names and fields without a recovered SDK name remain unknown.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/procevent.h>
#include <motion_dummy/motion_dummy.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysevent.h>
#include <intrmgr/syscall.h>
#include <touch_dummy/touch_dummy.h>
#include <touch/touch.h>

#define TOUCH_ERROR(n) ((int)(0x80350000U | (n)))

struct touchOwner {
	int pid;
	unsigned short region, attr;
};
struct touchSample {
	SceTouchData data;
	struct touchOwner owners[8];
};
struct touchRegion {
	int active;
	short x, y, width, height;
	unsigned attr;
};
struct touchRing {
	struct touchSample samples[64];
	unsigned readers[2], writeIndex;
};
struct touchProcessPort {
	unsigned eventBit, reserved;
	struct touchRegion regions[8];
	struct touchRing ring;
};
struct touchProcess {
	int pid;
	unsigned char sampling[2], force[2];
	unsigned privilege;
	int state;
	unsigned timestamp;
	unsigned short generation;
	unsigned char padding[6];
	unsigned sdkVersion;
	struct touchProcessPort ports[2];
};
struct touchNode {
	unsigned char id, reportIndex, padding[2];
	struct touchOwner owner;
	struct touchNode *next;
};
struct touchTracking {
	struct touchNode *added, *active, *free;
	struct touchNode nodes[8];
};
struct touchPort {
	unsigned deviceInfo[3], reportMax;
	float densityX, densityY;		/* pixels per mm */
	unsigned char forceMode, forceRequest;
	unsigned short forceChange;
	unsigned allocated, stopped;
	int events[2];
	SceTouchPanelInfo panel;
	struct touchTracking tracking;
	unsigned lastTouchTime, timeout;
	unsigned char forceLevel[2];		/* raised / lowered force setting */
	unsigned char enableState, padding;
};
struct physicalState {
	int state;
	unsigned activeMask, modeMask, idleMask, processCount;
	int pids[16];
	unsigned priorityCount;
	int priorities[16];
	struct touchPort ports[2];
	int timer, lock, priorityEnabled, sysHandler, procHandler;
	unsigned char forceDefault[2];
	unsigned short generation;
	int plsId;
	int (*ctrlCallback)(unsigned, SceTouchData *);
	const TouchVirtualDriver *virtualDriver;
	unsigned reserved;
};
struct virtualRing {
	SceTouchData samples[64];
	unsigned reader, writeIndex;
};
struct virtualProcessPort {
	unsigned eventBit, reserved;
	struct virtualRing ring;
};
struct virtualProcess {
	int pid;
	unsigned reserved;
	int state;
	unsigned timestamp;
	unsigned short generation;
	unsigned char padding[6];
	struct virtualProcessPort ports[4];
};
struct virtualState {
	unsigned processCount;
	int pids[16];
	unsigned priorityCount;
	int priorities[16];
	struct { unsigned allocated, stopped; int event; } ports[4];
	int lock, plsId;
	unsigned short generation, padding;
};
static struct {
	struct touchSample front __attribute__((aligned(8)));
	SceTouchData emulation[2];
	struct physicalState physical;
	struct virtualState virtual;
	struct touchSample back __attribute__((aligned(8)));
	int readMutex;
	unsigned padding0;
	SceTouchData readBuffer[64];
	int peekMutex;
	unsigned padding1;
	SceTouchData peekBuffer[64];
	SceTouchData virtualSamples[4];
	SceTouchData virtualBuffer[64];
} storage;
#define touch storage.physical
#define pad storage.virtual

_Static_assert(offsetof(__typeof__(storage), back) == 0x560, "back buffer layout");
_Static_assert(offsetof(__typeof__(storage), virtualSamples) == 0x4e40, "virtual buffers layout");
_Static_assert(sizeof(SceTouchData) == 0x90, "touch data layout");
_Static_assert(sizeof(struct touchSample) == 0xd0, "sample layout");
_Static_assert(sizeof(struct touchRing) == 0x3410, "ring layout");
_Static_assert(sizeof(struct touchProcessPort) == 0x3498, "process port layout");
_Static_assert(sizeof(struct touchProcess) == 0x6950, "process layout");
_Static_assert(sizeof(struct touchPort) == 0xf4, "port layout");
_Static_assert(sizeof(struct physicalState) == 0x2a8, "physical state layout");
_Static_assert(sizeof(struct virtualState) == 0xc4, "virtual state layout");
_Static_assert(sizeof(struct virtualProcess) == 0x9058, "virtual process layout");

static void copyWords(void *out, const void *in, unsigned bytes) /* name unknown */
{
	unsigned i;

	for (i = 0; i < bytes / 4; i++)
		((unsigned *)out)[i] = ((const unsigned *)in)[i];
}

static int lookupProcess(int pid, int pls, void **out, int create) /* name unknown */
{
	int ret;

	*out = NULL;
	ret = SceProcessmgrForDriver_AF80F39C(pid, pls, out, create);
	if (ret < 0)
		*out = NULL;
	return ret;
}

static unsigned allocateBit(unsigned *allocated) /* name unknown */
{
	unsigned i, bit;

	for (i = 0; i < 32; i++) {
		bit = 1U << i;
		if (!(*allocated & bit)) {
			*allocated |= bit;
			return bit;
		}
	}
	return 0;
}

static int findPid(int *pids, int pid) /* name unknown */
{
	int i;

	for (i = 0; i < 16; i++)
		if (pids[i] == pid)
			return i;
	return -1;
}

static void appendNodes(struct touchNode **list, struct touchNode *nodes) /* name unknown */
{
	while (*list)
		list = &(*list)->next;
	*list = nodes;
}

static int initializeTracking(struct touchTracking *tracking) /* name unknown */
{
	unsigned i;

	memset(tracking, 0, sizeof(*tracking));
	for (i = 0; i < 8; i++)
		appendNodes(&tracking->free, &tracking->nodes[i]);
	return 0;
}

static int updateTracking(struct touchTracking *tracking, SceTouchData *sample) /* name unknown */
{
	struct touchNode *node, *retained = NULL;
	unsigned remaining = sample->reportNum, seen = 0, i;

	appendNodes(&tracking->active, tracking->added);
	tracking->added = NULL;
	if (!remaining) {
		appendNodes(&tracking->free, tracking->active);
		tracking->active = NULL;
		return 0;
	}
	while (tracking->active && remaining) {
		node = tracking->active;
		tracking->active = node->next;
		node->next = NULL;
		for (i = 0; i < sample->reportNum; i++)
			if (sample->report[i].id == node->id)
				break;
		if (i == sample->reportNum) {
			appendNodes(&tracking->free, node);
		} else {
			node->reportIndex = i;
			seen |= (1U << (i & 255)) & 255;
			appendNodes(&retained, node);
			remaining--;
		}
	}
	if (!remaining)
		appendNodes(&tracking->free, tracking->active);
	tracking->active = retained;
	for (i = 0; i < sample->reportNum && remaining; i++) {
		if ((seen >> (i & 255)) & 1)
			continue;
		node = tracking->free;
		if (!node)
			return TOUCH_ERROR(255);
		tracking->free = node->next;
		node->next = NULL;
		node->id = sample->report[i].id;
		node->reportIndex = i;
		node->owner.pid = -1;
		node->owner.region = 0xffff;
		appendNodes(&tracking->added, node);
		remaining--;
	}
	return remaining ? TOUCH_ERROR(255) : 0;
}

static void findOwner(unsigned port, short x, short y, struct touchOwner *owner) /* name unknown */
{
	struct touchProcess *process;
	struct touchRegion *region;
	unsigned i;
	int r;

	owner->pid = -1;
	owner->region = 0xffff;
	if (touch.ports[port].enableState)
		return;
	for (i = 0; i < touch.priorityCount; i++) {
		if (touch.priorities[i] == -1)
			continue;
		if (lookupProcess(touch.priorities[i], touch.plsId, (void **)&process, 0) < 0 ||
		    !process || process->state)
			continue;
		for (r = 7; r >= 0; r--) {
			region = &process->ports[port].regions[r];
			if (region->active && x >= region->x && y >= region->y &&
			    x < region->x + region->width && y < region->y + region->height) {
				owner->pid = touch.priorities[i];
				owner->region = r;
				owner->attr = region->attr;
				return;
			}
		}
	}
}

static void updateOwners(unsigned port, struct touchSample *sample) /* name unknown */
{
	struct touchTracking *tracking = &touch.ports[port].tracking;
	struct touchNode *node;
	struct touchProcess *process;
	struct touchOwner owner;
	SceTouchReport *report;
	unsigned i;

	for (node = tracking->added; node; node = node->next) {
		report = &sample->data.report[node->reportIndex];
		findOwner(port, report->x, report->y, &node->owner);
	}
	for (node = tracking->active; node; node = node->next) {
		if (node->owner.pid == -1)
			continue;
		if (lookupProcess(node->owner.pid, touch.plsId, (void **)&process, 0) < 0 ||
		    !process || process->state == 3)
			continue;
		report = &sample->data.report[node->reportIndex];
		findOwner(port, report->x, report->y, &owner);
		if (owner.pid == -1) {
			report->info = 1;
		} else if (owner.pid != node->owner.pid) {
			report->info = 0x100;
			for (i = 0; i < touch.priorityCount; i++) {
				if (touch.priorities[i] == -1 ||
				    lookupProcess(touch.priorities[i], touch.plsId, (void **)&process, 0) < 0 ||
				    !process || process->state == 3)
					continue;
				if (touch.priorities[i] == node->owner.pid)
					break;
				if (touch.priorities[i] == owner.pid) {
					report->info = 1;
					break;
				}
			}
		} else if (owner.region != node->owner.region) {
			report->info = owner.region > node->owner.region ? 1 : 0x100;
		}
	}
	appendNodes(&tracking->active, tracking->added);
	tracking->added = NULL;
	for (node = tracking->active; node; node = node->next)
		copyWords(&sample->owners[node->reportIndex], &node->owner, 8);
}

static int createPhysicalProcess(int pid) /* name unknown */
{
	struct touchProcess *process;
	SceTouchPanelInfo *panel;
	struct touchProcessPort *port;
	unsigned intr = SceCpuForDriver_D32ACE9E(&touch.lock), i;
	int ret = lookupProcess(pid, touch.plsId, (void **)&process, 1), slot;

	if (ret >= 0 && process) {
		slot = findPid(touch.pids, -1);
		if (slot < 0) {
			ret = TOUCH_ERROR(255);
		} else {
			touch.pids[slot] = pid;
			touch.processCount++;
			memset(process, 0, sizeof(*process));
			process->pid = pid;
			SceProcessmgrForDriver_D141C076(pid, &process->sdkVersion);
			for (i = 0; i < 2; i++) {
				port = &process->ports[i];
				port->ring.readers[0] = 0;
				port->ring.readers[1] = 0;
				port->ring.writeIndex = 0;
				port->eventBit = allocateBit(&touch.ports[i].allocated);
				touch.ports[i].stopped &= ~port->eventBit;
				panel = &touch.ports[i].panel;
				port->regions[0].active = 1;
				port->regions[0].x = panel->minAaX;
				port->regions[0].y = panel->minAaY;
				port->regions[0].width = panel->maxAaX + 1 - panel->minAaX;
				port->regions[0].height = panel->maxAaY + 1 - panel->minAaY;
			}
			ret = 0;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

static int createVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr = SceCpuForDriver_D32ACE9E(&pad.lock), i;
	int ret = lookupProcess(pid, pad.plsId, (void **)&process, 1), slot;

	if (ret >= 0 && process) {
		slot = findPid(pad.pids, -1);
		if (slot < 0) {
			ret = TOUCH_ERROR(255);
		} else {
			pad.pids[slot] = pid;
			pad.processCount++;
			memset(process, 0, sizeof(*process));
			process->pid = pid;
			for (i = 0; i < 4; i++) {
				process->ports[i].ring.reader = 0;
				process->ports[i].ring.writeIndex = 0;
				process->ports[i].eventBit = allocateBit(&pad.ports[i].allocated);
				pad.ports[i].stopped &= ~process->ports[i].eventBit;
			}
			ret = 0;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return ret;
}

static int createProcess(int pid, ProcEventCreateParam *args, void *common) /* name unknown */
{
	int ret = createPhysicalProcess(pid);

	if (ret >= 0)
		ret = createVirtualProcess(pid);
	return ret;
}

static int removePhysicalProcess(int pid) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr = SceCpuForDriver_D32ACE9E(&touch.lock), i, bit;
	int ret = lookupProcess(pid, touch.plsId, (void **)&process, 0), slot;

	if (ret >= 0 && process) {
		slot = findPid(touch.pids, pid);
		if (!touch.processCount || slot < 0) {
			ret = TOUCH_ERROR(255);
		} else {
			touch.processCount--;
			touch.pids[slot] = -1;
			for (i = 0; i < 2; i++) {
				bit = process->ports[i].eventBit;
				touch.ports[i].stopped &= ~bit;
				if (touch.ports[i].allocated & bit)
					sceKernelClearEventFlag(touch.ports[i].events[0], ~bit);
				if (touch.ports[i].allocated & bit)
					sceKernelClearEventFlag(touch.ports[i].events[1], ~bit);
				touch.ports[i].allocated &= ~bit;
			}
			memset(process, 0, sizeof(*process));
			ret = 0;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

static int removeVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr = SceCpuForDriver_D32ACE9E(&pad.lock), i, bit;
	int ret = lookupProcess(pid, pad.plsId, (void **)&process, 0), slot;

	if (ret >= 0 && process) {
		slot = findPid(pad.pids, pid);
		if (!pad.processCount || slot < 0) {
			ret = TOUCH_ERROR(255);
		} else {
			pad.processCount--;
			pad.pids[slot] = -1;
			for (i = 0; i < 4; i++) {
				bit = process->ports[i].eventBit;
				pad.ports[i].stopped &= ~bit;
				if (pad.ports[i].allocated & bit)
					sceKernelClearEventFlag(pad.ports[i].event, ~bit);
				pad.ports[i].allocated &= ~bit;
			}
			memset(process, 0, sizeof(*process));
			ret = 0;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return ret;
}

static int removeProcess(int pid, ProcEventParam *args, void *common) /* name unknown */
{
	int ret = removePhysicalProcess(pid);

	if (ret >= 0)
		ret = removeVirtualProcess(pid);
	return ret;
}

static int virtualProcessEvent(int pid, unsigned event) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr = SceCpuForDriver_D32ACE9E(&pad.lock), i;
	int ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);

	if (ret >= 0 && process) {
		if (event == 0x200) {
			process->state =
				process->ports[0].ring.samples[(process->ports[0].ring.writeIndex - 1) & 63].reportNum ||
				process->ports[1].ring.samples[(process->ports[1].ring.writeIndex - 1) & 63].reportNum ? 1 : 2;
			process->timestamp = SceThreadmgrForDriver_47F6DE49();
			process->generation = pad.generation;
		} else if (event == 0x202) {
			if (process->state != 2) {
				if (process->generation == pad.generation &&
				    (unsigned)(SceThreadmgrForDriver_47F6DE49() - process->timestamp) < 34001)
					ret = -1;
				else
					process->state = 2;
			}
		} else {
			for (i = 0; i < 4; i++) {
				if (event == 0x800)
					pad.ports[i].stopped |= process->ports[i].eventBit;
				else
					pad.ports[i].stopped &= ~process->ports[i].eventBit;
			}
			process->state = event == 0x800 ? 3 : 4;
		}
		if (ret >= 0)
			ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return ret;
}

static int processEvent(int pid, unsigned event) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr = SceCpuForDriver_D32ACE9E(&touch.lock), i;
	int ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);

	if (ret >= 0 && process) {
		if (event == 0x200) {
			process->state =
				process->ports[0].ring.samples[(process->ports[0].ring.writeIndex - 1) & 63].data.reportNum ||
				process->ports[1].ring.samples[(process->ports[1].ring.writeIndex - 1) & 63].data.reportNum ? 1 : 2;
			process->timestamp = SceThreadmgrForDriver_47F6DE49();
			process->generation = touch.generation;
		} else if (event == 0x202) {
			ret = 0;
			if (process->state != 2) {
				if (process->generation == touch.generation &&
				    (unsigned)(SceThreadmgrForDriver_47F6DE49() - process->timestamp) < 34001)
					ret = -1;
				else
					process->state = 2;
			}
		} else {
			for (i = 0; i < 2; i++) {
				if (event == 0x800)
					touch.ports[i].stopped |= process->ports[i].eventBit;
				else
					touch.ports[i].stopped &= ~process->ports[i].eventBit;
			}
			process->state = event == 0x800 ? 3 : 4;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	if (ret >= 0)
		ret = virtualProcessEvent(pid, event);
	return ret;
}

static int stopProcess(int pid, int event, ProcEventParam *args, void *common) /* name unknown */
{
	if (event != 0x200 && event != 0x202 && event != 0x800)
		return 0;
	return processEvent(pid, event);
}

static int startProcess(int pid, int event, ProcEventParam *args, void *common) /* name unknown */
{
	if (event != 0x10000)
		return 0;
	return processEvent(pid, event);
}

static int sampleVirtual(TouchVirtualSampleCallback driver) /* name unknown */
{
	struct virtualProcess *process;
	struct virtualRing *ring;
	SceTouchData *sample, *dest;
	TouchVirtualSampleRequest args = { 0 };
	unsigned i, j, processed, r;
	int ret;

	for (i = 0; i < 4; i++) {
		memset(&storage.virtualSamples[i], 0, 0x90);
		args.samples[i] = &storage.virtualSamples[i];
	}
	for (i = 0; i < 4; i++) {
		ret = driver(i + 1, &args);
		if (ret < 0) {
			SceDebugForDriver_391B74B7("[Err] %x\n", ret);
			return -1;
		}
		if (!ret)
			continue;
		sample = &storage.virtualSamples[0];
		if (args.samples[0]->reportNum)
			SceKernelSuspendForDriver_E0489831(0);
		processed = 0;
		for (j = 0; j < 16 && processed < pad.processCount; j++) {
			if (pad.pids[j] == -1)
				continue;
			ret = lookupProcess(pad.pids[j], pad.plsId, (void **)&process, 0);
			if (ret != 0 || process->state == 3)
				continue;
			ring = &process->ports[i].ring;
			dest = &ring->samples[ring->writeIndex];
			SceProcessmgrForDriver_8CB01675(pad.pids[j], &dest->timeStamp,
				sample->timeStamp);
			dest->status = sample->status | (process->state ? 0x10000 : 0);
			dest->reportNum = sample->reportNum;
			for (r = 0; r < sample->reportNum; r++)
				copyWords(&dest->report[r], &sample->report[r], 16);
			ring->writeIndex = (ring->writeIndex + 1) & 63;
			if (ring->reader == ring->writeIndex)
				ring->reader = (ring->reader + 1) & 63;
			processed++;
		}
	}
	for (i = 0; i < 4; i++)
		sceKernelSetEventFlag(pad.ports[i].event, pad.ports[i].allocated & ~pad.ports[i].stopped);
	return 0;
}

static SceUInt32 samplingTimer(void *args) /* name unknown */
{
	struct touchProcess *process;
	struct touchSample *samples[2] = { &storage.front, &storage.back }, *dest;
	struct touchRing *ring;
	unsigned intr = SceCpuForDriver_D32ACE9E(&touch.lock), i, j, r, processed = 0;
	uint64_t time;
	int ret;

	if (touch.virtualDriver)
		sampleVirtual(touch.virtualDriver->getSample);
	memcpy(&storage.front, &storage.emulation[0], sizeof(SceTouchData));
	memcpy(&storage.back, &storage.emulation[1], sizeof(SceTouchData));
	time = sceKernelGetSystemTimeWide();
	storage.front.data.timeStamp = time;
	storage.back.data.timeStamp = time;
	if (touch.ctrlCallback) {
		touch.ctrlCallback(0, &storage.front.data);
		touch.ctrlCallback(1, &storage.back.data);
	}
	for (i = 0; i < 2; i++)
		if (samples[i]->data.reportNum)
			touch.ports[i].lastTouchTime = samples[i]->data.timeStamp;
	if (((touch.idleMask & 1) && storage.front.data.reportNum && !touch.ports[0].enableState) ||
	    ((touch.idleMask & 2) && storage.back.data.reportNum && !touch.ports[1].enableState))
		SceKernelSuspendForDriver_E0489831(0);
	updateTracking(&touch.ports[0].tracking, &storage.front.data);
	updateTracking(&touch.ports[1].tracking, &storage.back.data);
	updateOwners(0, &storage.front);
	updateOwners(1, &storage.back);
	for (i = 0; i < 16 && processed < touch.processCount; i++) {
		if (touch.pids[i] == -1)
			continue;
		ret = lookupProcess(touch.pids[i], touch.plsId, (void **)&process, 0);
		if (ret != 0 || process->state == 3)
			continue;
		for (j = 0; j < 2; j++) {
			ring = &process->ports[j].ring;
			dest = &ring->samples[ring->writeIndex];
			SceProcessmgrForDriver_8CB01675(touch.pids[i], &dest->data.timeStamp,
				samples[j]->data.timeStamp);
			dest->data.status = samples[j]->data.status | (process->state ? 0x10000 : 0);
			dest->data.reportNum = samples[j]->data.reportNum;
			for (r = 0; r < samples[j]->data.reportNum && r < 8; r++) {
				copyWords(&dest->data.report[r], &samples[j]->data.report[r], 16);
				copyWords(&dest->owners[r], &samples[j]->owners[r], 8);
			}
		}
		for (j = 0; j < 2; j++) {
			ring = &process->ports[j].ring;
			ring->writeIndex = (ring->writeIndex + 1) & 63;
			for (r = 0; r < 2; r++)
				if (ring->readers[r] == ring->writeIndex)
					ring->readers[r] = (ring->readers[r] + 1) & 63;
		}
		processed++;
	}
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			sceKernelSetEventFlag(touch.ports[i].events[j], touch.ports[i].allocated & ~touch.ports[i].stopped);
	touch.generation++;
	for (i = 0; i < 2; i++)
		if (touch.ports[i].enableState == 1)
			touch.ports[i].enableState = 2;
	touch.timer = -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

static SceUInt32 noOpAlarm(void *args) /* name unknown */
{
	return 0;
}

static int vblankCallback(void *args, void *context, unsigned char priority) /* name unknown */
{
	touch.timer = SceThreadmgrForDriver_C58DF384("SceTouchAlarm0", 1000, samplingTimer, NULL);
	if (touch.timer < 0)
		touch.timer = -1;
	return -1;
}

static int systemEvent(int resume, int event, void *args, void *opt) /* name unknown */
{
	unsigned intr;

	if (!resume) {
		switch (event) {
		case 0x400:
			intr = SceCpuForDriver_D32ACE9E(&touch.lock);
			touch.state = 3;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			break;
		case 0x401:
			if (!touch.activeMask && !touch.ports[0].forceMode && !touch.ports[1].forceMode && touch.state != 4)
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
		if (event == 0x1000c) {
			touch.activeMask = 0;
			touch.modeMask = 0;
			touch.ports[0].forceChange = 0;
			touch.ports[1].forceChange = 0;
			sceKernelEnableSubIntr(8, 5);
		} else if (event == 0x100000) {
			touch.state = 1;
			SceThreadmgrForDriver_C58DF384("SceTouchAlarm1", 100000, noOpAlarm, NULL);
		}
	}
	return 0;
}

static SceKernelProcEventHandler procHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = createProcess,
	.exit = removeProcess,
	.kill = removeProcess,
	.suspend = stopProcess,
	.resume = startProcess,
};

static int initializePhysical(void *args) /* name unknown */
{
	unsigned i;
	SceTouchPanelInfo *panel;

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

int module_start(unsigned arglen, void *argp)
{
	static const char *names[] = { "SceTouchPad1", "SceTouchPad2", "SceTouchPad3", "SceTouchPad4" };
	unsigned i;

	memset(storage.emulation, 0, sizeof(storage.emulation) + sizeof(storage.physical));
	memset(touch.pids, 255, sizeof(touch.pids));
	memset(touch.priorities, 255, sizeof(touch.priorities));
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
	memset(&pad, 0, sizeof(pad));
	memset(pad.pids, 255, 0x40);
	memset(pad.priorities, 255, 0x40);
	for (i = 0; i < 4; i++) {
		pad.ports[i].allocated = 0;
		pad.ports[i].stopped = 0;
		pad.ports[i].event = sceKernelCreateEventFlag(names[i], 0x9000, 0, NULL);
		if (pad.ports[i].event < 0)
			break;
	}
	pad.lock = 0;
	pad.plsId = sceKernelRegisterKPLS("SceTouchPad", sizeof(struct virtualProcess));
	if (pad.plsId < 0)
		return 2;
	storage.readMutex = -1;
	storage.peekMutex = -1;
	storage.readMutex = sceKernelCreateMutex("SceTouch1", 0, 0, NULL);
	if (storage.readMutex >= 0) {
		storage.peekMutex = sceKernelCreateMutex("SceTouch2", 0, 0, NULL);
		if (storage.peekMutex >= 0)
			return 0;
		if (storage.readMutex >= 0)
			sceKernelDeleteMutex(storage.readMutex);
	}
	if (storage.peekMutex >= 0)
		sceKernelDeleteMutex(storage.peekMutex);
	return 2;
}

static unsigned regionBit(unsigned region) /* name unknown */
{
	region &= 255;
	return region < 32 ? 1U << region : 0;
}

static int readRing(struct touchRing *ring, unsigned kind, SceTouchData *out,
		    unsigned count, int pid, unsigned regions, unsigned flags) /* name unknown */
{
	struct touchSample *sample;
	SceTouchReport *report;
	struct touchOwner *owner;
	unsigned available, index = ring->writeIndex, i, j, reports;
	int owned;

	if (!out || kind > 1)
		return TOUCH_ERROR(1);
	available = flags & 1 ? (ring->writeIndex - ring->readers[kind]) & 63 : 64;
	if (!available)
		return 0;
	if (count > available)
		count = available;
	for (i = count; i > 0; i--) {
		index = (index - 1) & 63;
		sample = &ring->samples[index];
		out[i - 1].timeStamp = sample->data.timeStamp;
		out[i - 1].status = sample->data.status;
		out[i - 1].reportNum = 0;
		if (!(flags & 0x80000000))
			continue;
		reports = sample->data.reportNum;
#if !FW_DEBUG
		if ((flags & 0xf0) == 0xd0 && reports > 8)
			reports = 8;
#endif
		for (j = 0; j < reports; j++) {
			owner = &sample->owners[j];
			owned = owner->pid == pid && (regionBit(owner->region) & regions);
			if ((flags & 16) && !owned && (!(flags & 32) || (owner->attr & 1)))
				continue;
			report = &out[i - 1].report[out[i - 1].reportNum++];
			copyWords(report, &sample->data.report[j], 16);
			if (!(flags & 16))
				report->info = 0;
			else if (!owned)
				report->info = owner->attr & 1;
			if (!(flags & 64))
				report->force = 0;
			else if (!(flags & 128))
				report->force = 128;
		}
	}
	if (flags & 1)
		ring->readers[kind] = ring->writeIndex;
	return count;
}

static int readPhysical(unsigned port, SceTouchData *out, unsigned count,
			unsigned regions, unsigned kind, int peek) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr, bit, flags;
	int pid, ret;

	if (port >= 2 || !out || count > 64 || regions & 0xffffff00)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		if (!peek) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			bit = process->ports[port].eventBit;
			if (!(bit & touch.ports[port].allocated))
				return TOUCH_ERROR(1);
			ret = sceKernelWaitEventFlag(touch.ports[port].events[kind], bit,
						      SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
			if (ret < 0)
				return ret;
			intr = SceCpuForDriver_D32ACE9E(&touch.lock);
			bit = process->ports[port].eventBit;
			if (bit & touch.ports[port].allocated)
				sceKernelClearEventFlag(touch.ports[port].events[kind], ~bit);
		}
		flags = peek ? 2 : 1;
		if (touch.priorityEnabled)
			flags |= process->privilege ? 48 : 16;
		if ((process->sampling[kind] >> port) & 1)
			flags |= 0x80000000;
		if ((process->force[kind] >> port) & 1)
			flags |= 64;
		if (process->sdkVersion < 0x2500000)
			flags |= 128;
		ret = readRing(&process->ports[port].ring, kind, out, count, pid, regions, flags);
		if (process->state == 4)
			process->state = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_70C8AACE(unsigned port, SceTouchData *out, unsigned count)
{
	return readPhysical(port, out, count, 1, 0, 0);
}

int SceTouchForDriver_BAD1960B(unsigned port, SceTouchData *out, unsigned count)
{
	return readPhysical(port, out, count, 1, 0, 1);
}

int SceTouchForDriver_9A91F624(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 0, 0);
}

int SceTouchForDriver_9B3F7207(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 0, 1);
}

static int setSampling(unsigned port, unsigned state, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr, old, bit;
	int ret, pid;

	if (port >= 2 || state >= 2)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		old = (process->sampling[kind] >> port) & 1;
		if (old != state) {
			if (state) {
				process->ports[port].ring.readers[kind] = process->ports[port].ring.writeIndex;
				bit = process->ports[port].eventBit;
				if (bit & touch.ports[port].allocated)
					sceKernelClearEventFlag(touch.ports[port].events[kind], ~bit);
			}
			process->sampling[kind] = (process->sampling[kind] & ~(1U << port)) | (state << port);
		}
		process->state = 0;
		ret = old;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_1247257A(unsigned port, unsigned state)
{
	return setSampling(port, state, 0);
}

static int getSampling(unsigned port, unsigned *state, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= 2 || !state)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		*state = (process->sampling[kind] >> port) & 1;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_174D0333(unsigned port, unsigned *state)
{
	return getSampling(port, state, 0);
}

static int setForce(unsigned port, unsigned enable, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= 2)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		process->force[kind] = (process->force[kind] & ~(1U << port)) | (enable << port);
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_E41B4BF8(unsigned port)
{
	return setForce(port, 1, 0);
}

int SceTouchForDriver_CDE79889(unsigned port)
{
	return setForce(port, 0, 0);
}

int SceTouchForDriver_328E38B0(unsigned port, int enable)
{
	unsigned intr;
	struct touchTracking *tracking;

	if (port >= 2)
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	tracking = &touch.ports[port].tracking;
	if (enable)
		touch.ports[port].enableState = 0;
	else if (!touch.ports[port].enableState)
		touch.ports[port].enableState = tracking->added || tracking->active ? 1 : 2;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

static int validRegion(unsigned port, const short *rect, int positive) /* name unknown */
{
	SceTouchPanelInfo *panel = &touch.ports[port].panel;

	return rect[0] >= panel->minAaX && rect[1] >= panel->minAaY &&
		(!positive || (rect[2] > 0 && rect[3] > 0)) &&
		rect[0] + rect[2] - 1 <= panel->maxAaX &&
		rect[1] + rect[3] - 1 <= panel->maxAaY;
}

int SceTouchForDriver_7985B164(unsigned port, unsigned region, const short *rect, unsigned attr)
{
	struct touchProcess *process;
	struct touchRegion *dest;
	unsigned intr;
	int ret, pid;

	if (port >= 2 || region >= 8 || !rect || !validRegion(port, rect, 1))
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		dest->active = 1;
		copyWords(&dest->x, rect, 8);
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
	unsigned intr, i;
	int ret, pid;

	if (port >= 2 || region >= 8)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		dest->active = 0;
		dest->attr = 0;
		for (i = 0; i < 8; i++)
			((unsigned char *)&dest->x)[i] = 0;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_10A998DE(unsigned port, unsigned region, unsigned attr)
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= 2 || region >= 8)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		process->ports[port].regions[region].attr = attr;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_C7F0DAA0(unsigned port, unsigned region, unsigned enable)
{
	struct touchProcess *process;
	struct touchRegion *dest;
	unsigned intr;
	int ret, pid;

	if (port >= 2 || region >= 8 || enable >= 2)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		if (enable && !validRegion(port, &dest->x, 0))
			ret = TOUCH_ERROR(1);
		else
			dest->active = enable;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_3F3FBF69(const TouchProcessPriorities *priorities)
{
	unsigned intr, i;

	if (!priorities)
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	touch.priorityCount = priorities->count;
	touch.priorityEnabled = 1;
	for (i = 0; i < touch.priorityCount && i < 16; i++)
		touch.priorities[i] = priorities->processIds[i];
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchSetCtrlpCallback(int (*callback)(unsigned, SceTouchData *))
{
	unsigned intr;

	if (!callback)
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	touch.ctrlCallback = callback;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int SceTouchForDriver_03C3AC7C(const TouchVirtualDriver *driver)
{
	if (!driver)
		return TOUCH_ERROR(1);
	touch.virtualDriver = driver;
	return 0;
}

void SceTouchForDriver_845E2115(void)
{
}

void SceTouchForDriver_062A11A2(void)
{
}

int SceTouchForDriver_937DB4C0(unsigned port, SceTouchPanelInfo *out)
{
	if (port >= 2 || !out)
		return TOUCH_ERROR(1);
	copyWords(out, &touch.ports[port].panel, 0x30);
	return 0;
}

static int getPixelDensity(float *front, float *back) /* name unknown */
{
	if (!front || !back)
		return TOUCH_ERROR(1);
	*front = touch.ports[0].densityX;
	*back = touch.ports[1].densityX;
	return 0;
}

static int getPixelDensity2(float *frontX, float *frontY, float *backX, float *backY) /* name unknown */
{
	if (!frontX || !frontY || !backX || !backY)
		return TOUCH_ERROR(1);
	*frontX = touch.ports[0].densityX;
	*frontY = touch.ports[0].densityY;
	*backX = touch.ports[1].densityX;
	*backY = touch.ports[1].densityY;
	return 0;
}

static int setPrivilege(unsigned privilege) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, touch.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		process->privilege = privilege;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int SceTouchForDriver_4DF3A37C(unsigned port, void *out, unsigned unk)
{
	return 0;
}

int SceTouchForDriver_EBF4C86A(unsigned port, int enable)
{
	if (port >= 2)
		return TOUCH_ERROR(1);
	if (enable)
		touch.idleMask |= 1U << port;
	else
		touch.idleMask &= ~(1U << port);
	return 0;
}

int SceTouchForDriver_384F6C68(unsigned port, unsigned timeout)
{
	unsigned intr;

	if (port >= 2)
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	touch.ports[port].timeout = timeout;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int SceTouchForDriver_FD2E0278(unsigned port, struct touchDeviceInfo *out)
{
	unsigned intr, i;

	if (port >= 2 || !out)
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	for (i = 0; i < 12; i++)
		((unsigned char *)out)[i] = 0;
	copyWords(out, touch.ports[port].deviceInfo, 8);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int SceTouchForDriver_63DCF2FE(void)
{
	return touch.state != 0;
}

int SceTouchForDriver_A32529B8(const SceTouchData *front, const SceTouchData *back)
{
	unsigned intr = SceCpuForDriver_D32ACE9E(&touch.lock);

	memcpy(&storage.emulation[0], front, 0x90);
	memcpy(&storage.emulation[1], back, 0x90);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int SceTouchForDriver_9C8CCB66(void)
{
	return 0;
}

int SceTouchForDriver_DDCF5145(void)
{
	return 0;
}

int SceTouchForDriver_423F26D3(void)
{
	return 0;
}

int SceTouchForDriver_4E6C4558(unsigned panels)
{
	return 0;
}

static int userRead(unsigned port, void *out, unsigned count, unsigned regions,
		    unsigned kind, int peek) /* name unknown */
{
	SceTouchData *buffer = peek || kind ? storage.peekBuffer : storage.readBuffer;
	int mutex = peek || kind ? storage.peekMutex : storage.readMutex;
	unsigned t = sceSyscallEnter();
	int ret, copied, unlocked;

	if (!out || count - 1 >= 64 || port >= 2) {
		ret = TOUCH_ERROR(1);
	} else {
		ret = sceKernelLockMutex(mutex, 1, NULL);
		if (ret >= 0) {
			ret = readPhysical(port, buffer, count, regions, kind, peek);
			if (ret > 0) {
				copied = sceKernelCopyToUser(out, buffer, ret * 0x90);
				if (copied < 0) {
					sceKernelUnlockMutex(mutex, 1);
					sceSyscallLeave(t);
					return copied;
				}
			}
			unlocked = sceKernelUnlockMutex(mutex, 1);
			if (unlocked < 0)
				ret = unlocked;
		}
	}
	sceSyscallLeave(t);
	return ret;
}

int sceTouchRead(SceUInt32 port, SceTouchData *out, SceUInt32 count)
{
	return userRead(port, out, count, 1, 0, 0);
}

int sceTouchPeek(SceUInt32 port, SceTouchData *out, SceUInt32 count)
{
	return userRead(port, out, count, 1, 0, 1);
}

int sceTouchReadRegion(unsigned port, void *out, unsigned count, unsigned regions)
{
	return userRead(port, out, count, regions, 0, 0);
}

int sceTouchPeekRegion(unsigned port, void *out, unsigned count, unsigned regions)
{
	return userRead(port, out, count, regions, 0, 1);
}

int sceTouchReadRegionExt(unsigned port, void *out, unsigned count, unsigned regions)
{
	return userRead(port, out, count, regions, 1, 0);
}

int sceTouchPeekRegionExt(unsigned port, void *out, unsigned count, unsigned regions)
{
	return userRead(port, out, count, regions, 1, 1);
}

int sceTouchSetSamplingState(SceUInt32 port, SceUInt32 state)
{
	unsigned t = sceSyscallEnter();
	int ret = setSampling(port, state, 0);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetSamplingStateExt(unsigned port, unsigned state)
{
	unsigned t = sceSyscallEnter();
	int ret = setSampling(port, state, 1);

	sceSyscallLeave(t);
	return ret;
}

static int userGetSampling(unsigned port, void *out, unsigned kind) /* name unknown */
{
	unsigned value = 0, t;
	int ret;

	if (!out)
		return TOUCH_ERROR(1);
	t = sceSyscallEnter();
	ret = getSampling(port, &value, kind);
	if (ret >= 0)
		ret = sceKernelCopyToUser(out, &value, 4);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetSamplingState(SceUInt32 port, SceUInt32 *out)
{
	return userGetSampling(port, out, 0);
}

int sceTouchGetSamplingStateExt(unsigned port, void *out)
{
	return userGetSampling(port, out, 1);
}

static int userSetForce(unsigned port, unsigned enable, unsigned kind) /* name unknown */
{
	unsigned t = sceSyscallEnter();
	int ret = setForce(port, enable, kind);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchEnableTouchForce(SceUInt32 port)
{
	return userSetForce(port, 1, 0);
}

int sceTouchEnableTouchForceExt(unsigned port)
{
	return userSetForce(port, 1, 1);
}

int sceTouchDisableTouchForce(SceUInt32 port)
{
	return userSetForce(port, 0, 0);
}

int sceTouchDisableTouchForceExt(unsigned port)
{
	return userSetForce(port, 0, 1);
}

int sceTouchGetPanelInfo(SceUInt32 port, SceTouchPanelInfo *out)
{
	SceTouchPanelInfo info;
	unsigned t;
	int ret;

	if (!out)
		return TOUCH_ERROR(1);
	t = sceSyscallEnter();
	ret = SceTouchForDriver_937DB4C0(port, &info);
	if (ret >= 0)
		ret = sceKernelCopyToUser(out, &info, 0x30);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetPixelDensity(void *front, void *back)
{
	float values[2] = { 0 };
	unsigned t = sceSyscallEnter();
	int ret = TOUCH_ERROR(1);

	if (front && back) {
		ret = getPixelDensity(&values[0], &values[1]);
		if (ret >= 0)
			ret = sceKernelCopyToUser(front, &values[0], 4);
		if (ret >= 0)
			ret = sceKernelCopyToUser(back, &values[1], 4);
	}
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetPixelDensity2(void *frontX, void *frontY, void *backX, void *backY)
{
	float values[4] = { 0 };
	unsigned t = sceSyscallEnter();
	int ret = TOUCH_ERROR(1);

	if (frontX && frontY && backX && backY) {
		ret = getPixelDensity2(&values[0], &values[1], &values[2], &values[3]);
		if (ret >= 0)
			ret = sceKernelCopyToUser(frontX, &values[0], 4);
		if (ret >= 0)
			ret = sceKernelCopyToUser(frontY, &values[1], 4);
		if (ret >= 0)
			ret = sceKernelCopyToUser(backX, &values[2], 4);
		if (ret >= 0)
			ret = sceKernelCopyToUser(backY, &values[3], 4);
	}
	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetRegion(unsigned port, unsigned region, const void *rect, unsigned attr)
{
	short copy[4];
	unsigned t;
	int ret;

	if (!rect)
		return TOUCH_ERROR(1);
	t = sceSyscallEnter();
	ret = sceKernelCopyFromUser(copy, rect, 8);
	if (!ret)
		ret = SceTouchForDriver_7985B164(port, region, copy, attr);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchClearRegion(unsigned port, unsigned region)
{
	unsigned t = sceSyscallEnter();
	int ret = SceTouchForDriver_E1C5D95F(port, region);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchActivateRegion(unsigned port, unsigned region, unsigned enable)
{
	unsigned t = sceSyscallEnter();
	int ret = SceTouchForDriver_C7F0DAA0(port, region, enable);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetRegionAttr(unsigned port, unsigned region, unsigned attr)
{
	unsigned t = sceSyscallEnter();
	int ret = SceTouchForDriver_10A998DE(port, region, attr);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetProcessPrivilege(unsigned privilege)
{
	unsigned t = sceSyscallEnter();
	int ret = TOUCH_ERROR(2);

	if (sceSblACMgrIsSystemProgram(0))
		ret = setPrivilege(privilege);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetProcessInfo(unsigned port, void *out, unsigned unk)
{
	/* SceTouchForDriver_4DF3A37C fills nothing: the copy hands out stack contents */
	unsigned char info[0x884];
	unsigned t = sceSyscallEnter();
	int ret = TOUCH_ERROR(1);

	if (out) {
		ret = SceTouchForDriver_4DF3A37C(port, info, unk);
		if (ret >= 0)
			ret = sceKernelCopyToUser(out, info, sizeof(info));
	}
	sceSyscallLeave(t);
	return ret;
}

int sceTouchEnableIdleTimerCancelSetting(unsigned port, int enable)
{
	unsigned t = sceSyscallEnter();
	int ret = TOUCH_ERROR(2);

	if (sceSblACMgrIsSystemProgram(0))
		ret = SceTouchForDriver_EBF4C86A(port, enable);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetDeviceInfo(unsigned port, void *out)
{
	struct touchDeviceInfo info;
	unsigned t = sceSyscallEnter();
	int ret = TOUCH_ERROR(2);

	if (sceSblACMgrIsSystemProgram(0)) {
		ret = SceTouchForDriver_FD2E0278(port, &info);
		if (ret >= 0)
			ret = sceKernelCopyToUser(out, &info, 12);
	}
	sceSyscallLeave(t);
	return ret;
}

static int readVirtual(unsigned device, void *out, unsigned count, int peek) /* name unknown */
{
	struct virtualProcess *process;
	struct virtualRing *ring;
	SceTouchData *sample, *dest;
	unsigned intr, available, index, i, j, bit;
	int ret, copied, pid;

	if (!out || count > 64 || device - 1 >= 4)
		return TOUCH_ERROR(1);
	device--;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);
	if (ret >= 0 && process) {
		if (!peek) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
			bit = process->ports[device].eventBit;
			if (!(bit & pad.ports[device].allocated))
				return TOUCH_ERROR(1);
			ret = sceKernelWaitEventFlag(pad.ports[device].event, bit,
						      SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
			if (ret < 0)
				return ret;
			intr = SceCpuForDriver_D32ACE9E(&pad.lock);
			bit = process->ports[device].eventBit;
			if (bit & pad.ports[device].allocated)
				sceKernelClearEventFlag(pad.ports[device].event, ~bit);
		}
		ring = &process->ports[device].ring;
		available = peek ? 64 : (ring->writeIndex - ring->reader) & 63;
		if (count > available)
			count = available;
		index = ring->writeIndex;
		for (i = count; i > 0; i--) {
			index = (index - 1) & 63;
			sample = &ring->samples[index];
			dest = &storage.virtualBuffer[i - 1];
			dest->timeStamp = sample->timeStamp;
			dest->status = sample->status;
			dest->reportNum = 0;
			for (j = 0; j < sample->reportNum; j++) {
				copyWords(&dest->report[j], &sample->report[j], 16);
				dest->report[j].force = 0;
				dest->report[j].info = 0;
				dest->reportNum++;
			}
		}
		if (!peek && available)
			ring->reader = ring->writeIndex;
		copied = sceKernelCopyToUser(out, storage.virtualBuffer, count * 0x90);
		if (copied > 0)
			SceDebugForDriver_391B74B7("error!\n");
		if (process->state == 4)
			process->state = 0;
		ret = count;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return ret;
}

int sceTouchRead2(unsigned device, unsigned port, void *out, unsigned count)
{
	unsigned t;
	int ret;

	if (!device)
		return userRead(port, out, count, 1, 0, 0);
	t = sceSyscallEnter();
	ret = readVirtual(device, out, count, 0);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchPeek2(unsigned device, unsigned port, void *out, unsigned count)
{
	unsigned t;
	int ret;

	if (!device)
		return userRead(port, out, count, 1, 0, 1);
	t = sceSyscallEnter();
	ret = readVirtual(device, out, count, 1);
	sceSyscallLeave(t);
	return ret;
}
