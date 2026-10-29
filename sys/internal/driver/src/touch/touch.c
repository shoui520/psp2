/*
 * SceTouch: front and back touch panels, driven through the syscon.
 *
 * Every vblank the configuration (report mode, force sensing) is pushed to
 * the syscon and a sample request is queued; its reply is parsed into one
 * sample per panel, tracked (touch IDs keep their owner region across
 * frames) and copied into the ring of every process.
 * Private function names and many fields remain unknown.
 */
#include <modulemgr/modulemgr.h>
#include <syscon/syscon.h>
#include <sysmem/sysroot.h>
#include "private.h"

#define TOUCH_ERROR_BUSY	((int)0x80350103)	/* name unknown */

#define TOUCH_PORTS		2
#define TOUCH_REGIONS		8
#define TOUCH_RING_SIZE		64
#define TOUCH_MAX_PROCESSES	16

#define TOUCH_VENDOR_ALT	0x800a		/* deviceInfo.vendor of the alternative panel */
#define TOUCH_BARYON_FORCE	0x00090002	/* newer baryons apply force settings */
#define TOUCH_BARYON_INFO2	0x010005ff	/* newer baryons report both panels at once */

/* syscon commands */
#define TOUCH_CMD_SAMPLE_BOTH	0x0300
#define TOUCH_CMD_SAMPLE_FRONT	0x0301
#define TOUCH_CMD_SAMPLE_BACK	0x0302
#define TOUCH_CMD_SET_MODE	0x0381
#define TOUCH_CMD_SET_FORCE	0x0387

/* touchState.state */
#define TOUCH_STATE_SUSPENDED	1
#define TOUCH_STATE_RUNNING	2
#define TOUCH_STATE_SUSPENDING	3
#define TOUCH_STATE_IDLE	4
#define TOUCH_STATE_UPDATING	5

struct touchPort {
	struct touchDeviceInfo deviceInfo;
	unsigned config;			/* mode byte of the set-mode command */
	float densityX, densityY;		/* pixels per mm */
	unsigned char forceMode;		/* applied by the syscon */
	unsigned char forceRequest;		/* to be applied */
	unsigned char forceChange;		/* 1: raise, 2: lower, 3: off */
	unsigned char padding0;
	unsigned allocated, stopped;
	int events[2];
	SceTouchPanelInfo panel;
	struct touchTracking tracking;
	unsigned lastTouchTime, timeout;
	unsigned char forceLevel[2];		/* raised / lowered force setting */
	unsigned char enableState, padding1;
};
struct touchCommand {
	SceSysconPacket packet;
	unsigned short pending, retries;
};
struct touchState {
	int state;
	unsigned activeMask;			/* panels reporting */
	unsigned modeMask;			/* requested by the last set-mode command */
	unsigned idleMask;
	struct touchCommand sample, config;
	unsigned processCount;
	int pids[TOUCH_MAX_PROCESSES];
	unsigned priorityCount;
	int priorities[TOUCH_MAX_PROCESSES];
	struct touchPort ports[TOUCH_PORTS];
	int timer, lock;
	unsigned baryonVersion;
	unsigned priorityEnabled;
	int sysHandler, procHandler;
	unsigned char forceDefault[TOUCH_PORTS];
	unsigned short generation;
	int plsId;
	int (*ctrlCallback)(unsigned, SceTouchData *);
};

_Static_assert(sizeof(struct touchPort) == 0xf4, "port layout");
_Static_assert(sizeof(struct touchCommand) == 0x84, "command layout");
_Static_assert(offsetof(struct touchState, processCount) == 0x118, "state layout");
_Static_assert(offsetof(struct touchState, ports) == 0x1a0, "state ports layout");
_Static_assert(sizeof(struct touchState) == 0x3ac, "state layout");

/* defined last to first: GCC 4.8 places a unit's objects in reverse order, Sony's from the buffer up */
static int s_updateEvent;
static SceSysconPacket s_resetPacket;
static struct touchState touch;
static struct touchSample s_samples[TOUCH_PORTS];
static unsigned char s_sampleBuffer[0x80] __attribute__((aligned(16)));

static int sampleDone(SceSysconPacket *packet, void *argp);
static SceUInt32 sampleAlarm(void *argp);

static int lookupProcess(int pid, void **out, int create) /* name unknown */
{
	int ret;

	*out = NULL;
	ret = sceKernelGetRemoteKPLS(pid, touch.plsId, out, create);
	if (ret < 0)
		*out = NULL;
	return ret;
}

/* ------------------------------------------------------------------ syscon */

/* name unknown: completion of the reset packet */
static int resetDone(SceSysconPacket *packet, void *argp)
{
	touch.state = TOUCH_STATE_RUNNING;
	touch.activeMask = 0x40000000;
	return 0;
}

/* name unknown: SceTouchAlarm1, queued on resume */
static SceUInt32 resetAlarm(void *argp)
{
	s_resetPacket.request.command[0] = 0x90;
	s_resetPacket.request.command[1] = 3;
	s_resetPacket.request.length = 1;
	if (SceSysconForDriver_C2224E82(&s_resetPacket, 0, resetDone, NULL) < 0) {
		touch.state = TOUCH_STATE_RUNNING;
		touch.activeMask = 0x40000000;
	}
	return 0;
}

/* name unknown: completion of a set-mode / set-force command */
static int configDone(SceSysconPacket *packet, void *argp)
{
	struct touchCommand *cmd = argp;
	unsigned intr, mask, changed, time;
	int resend;

	if (!cmd)
		return -1;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	if (cmd->pending != 1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return -1;
	}
	cmd->pending = 0;
	switch (packet->request.command[0] | packet->request.command[1] << 8) {
	case TOUCH_CMD_SET_MODE:
		touch.modeMask = 0;
		mask = (cmd->packet.request.data[2] ? 2 : 0) | (cmd->packet.request.data[0] ? 1 : 0);
		changed = mask ^ touch.activeMask;
		touch.activeMask = mask;
		time = SceThreadmgrForDriver_47F6DE49();
		if ((touch.activeMask & 1) && (changed & 1))
			touch.ports[0].lastTouchTime = time;
		if ((touch.activeMask & 2) && (changed & 2))
			touch.ports[1].lastTouchTime = time;
		resend = 0;
		if (touch.baryonVersion > TOUCH_BARYON_FORCE) {
			changed |= ~touch.activeMask & 3;
			if (changed & 1)
				touch.ports[0].forceRequest = (touch.activeMask & 1) ?
							      touch.ports[0].forceLevel[0] : 0;
			if (changed & 2)
				touch.ports[1].forceRequest = (touch.activeMask & 2) ?
							      touch.ports[1].forceLevel[0] : 0;
			cmd->packet.request.data[1] = touch.ports[0].forceRequest;
			cmd->pending = 1;
			cmd->retries = 0;
			cmd->packet.request.data[2] = 0;
			cmd->packet.request.data[3] = 0;
			cmd->packet.request.data[6] = 0;
			cmd->packet.request.data[7] = 0;
			cmd->packet.request.data[0] = changed & 1;
			cmd->packet.request.data[4] = (changed >> 1) & 1;
			cmd->packet.request.data[5] = touch.ports[1].forceRequest;
			cmd->packet.request.command[0] = TOUCH_CMD_SET_FORCE & 0xff;
			cmd->packet.request.command[1] = TOUCH_CMD_SET_FORCE >> 8;
			cmd->packet.request.length = 9;
			resend = 1;
		}
		break;
	case TOUCH_CMD_SET_FORCE:
		if (cmd->packet.request.data[0])
			touch.ports[0].forceMode = touch.ports[0].forceRequest;
		if (cmd->packet.request.data[4])
			touch.ports[1].forceMode = touch.ports[1].forceRequest;
		resend = 0;
		break;
	default:
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return -1;
	}

	if (touch.state == TOUCH_STATE_UPDATING && touch.activeMask == 0) {
		if (!resend) {
			if (s_updateEvent != -1)
				sceKernelSetEventFlag(s_updateEvent, 1);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return 0;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	} else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		if (!resend)
			return 0;
	}
	if (SceSysconForDriver_C2224E82(&cmd->packet, 0, configDone, cmd) < 0)
		cmd->pending = 0;
	return 0;
}

/* name unknown: device info of both panels from the newer syscon interface */
static int readDeviceInfo(void)
{
	SceKernelTouchpanelDeviceInfo2 info = { { 0 } };
	int retries = 3, ret;

	for (;;) {
		ret = SceSysconForDriver_030D447F(&info);
		if (ret < 0)
			return ret;
		if ((info.front.product && info.rear.product) || retries == 1)
			break;
		retries--;
		sceKernelDelayThread(20000);
	}
	touch.ports[0].deviceInfo.product = info.front.product;
	touch.ports[1].deviceInfo.product = info.rear.product;
	touch.ports[0].deviceInfo.vendor = info.front.vendor;
	touch.ports[0].deviceInfo.firmware = info.front.firmware;
	touch.ports[0].deviceInfo.detail[0] = info.front.detail[0];
	touch.ports[0].deviceInfo.detail[1] = info.front.detail[1];
	touch.ports[1].deviceInfo.vendor = info.rear.vendor;
	touch.ports[1].deviceInfo.firmware = info.rear.firmware;
	touch.ports[1].deviceInfo.detail[0] = info.rear.detail[0];
	touch.ports[1].deviceInfo.detail[1] = info.rear.detail[1];
	return ret;
}

static inline unsigned short swap16(unsigned value)
{
	return (value >> 8 & 0xff) | (value << 8);
}

/* name unknown: device info from the older syscon interface */
static int readDeviceInfoOld(void)
{
	unsigned short info[4] = { 0 };
	SceUInt32 version = 0;
	int retries = 3, ret;

	for (;;) {
		ret = sceSysconGetTouchpanelDeviceInfo(info);
		if (ret < 0)
			return ret;
		if ((info[1] && info[3]) || retries == 1)
			break;
		retries--;
		sceKernelDelayThread(20000);
	}
	touch.ports[0].deviceInfo.product = info[1];
	touch.ports[1].deviceInfo.product = info[3];
	touch.ports[0].deviceInfo.vendor = info[0];
	touch.ports[1].deviceInfo.vendor = info[2];
	if (touch.baryonVersion > TOUCH_BARYON_FORCE) {
		ret = SceSysconForDriver_9B6B8BB9(&version);
		if (ret < 0)
			return ret;
		touch.ports[0].deviceInfo.firmware = swap16(version);
		ret = SceSysconForDriver_C4A61241(&version);
		if (ret >= 0)
			touch.ports[1].deviceInfo.firmware = swap16(version);
	}
	return ret;
}

static int systemEvent(int resume, int event, void *args, void *opt) /* name unknown */
{
	unsigned intr;

	if (resume == 0) {
		switch (event) {
		case 0x400:
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
			touch.state = TOUCH_STATE_SUSPENDING;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return resume;
		case 0x401:
			if (!touch.activeMask && !touch.ports[0].forceMode && !touch.ports[1].forceMode &&
			    !touch.sample.pending && !touch.config.pending &&
			    touch.state != TOUCH_STATE_IDLE) {
				sceSysconCtrlDeviceReset(12, 0);
				touch.state = TOUCH_STATE_IDLE;
			}
			return 0;
		case 0x402:
			return touch.state == TOUCH_STATE_IDLE ? 0 : -1;
		case 0x400c:
			sceKernelDisableSubIntr(8, 5);
			touch.state = TOUCH_STATE_SUSPENDED;
			if (touch.timer != -1) {
				sceKernelCancelAlarm(touch.timer);
				touch.timer = -1;
				return resume;
			}
			return 0;
		}
	} else if (resume == 1) {
		if (event == 0x1000c) {
			touch.activeMask = 0;
			touch.modeMask = 0;
			touch.ports[0].forceChange = 0;
			touch.ports[1].forceChange = 0;
			sceKernelEnableSubIntr(8, 5);
			return 0;
		}
		if (event == 0x100000) {
			sceSysconCtrlDeviceReset(12, resume);
			touch.state = resume;
			SceThreadmgrForDriver_C58DF384("SceTouchAlarm1", 100000, resetAlarm, NULL);
			return 0;
		}
	}
	return 0;
}

/* name unknown: the force request of a panel after a raise/lower/off change */
static unsigned char forceForChange(unsigned port)
{
	unsigned char change = touch.ports[port].forceChange;

	touch.ports[port].forceChange = 0;
	switch (change) {
	case 0:
		return touch.ports[port].forceRequest;
	case 2:
		return touch.ports[port].forceLevel[1];
	case 3:
		return 0;
	default:
		return touch.ports[port].forceLevel[0];
	}
}

/* name unknown: vblank: push the configuration and request a sample */
static int vblankHandler(void *subintrArg, void *registerArg, unsigned char priority)
{
	struct touchProcess *process;
	unsigned intr, mask, i, n, sample;
	unsigned char front, back;
	int config = 0, alarm, ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	mask = touch.activeMask;
	if (touch.config.pending) {
		touch.config.retries++;
	} else if (touch.state == TOUCH_STATE_RUNNING) {
		/* panels some process samples */
		mask = 0;
		for (i = 0, n = 0; i < TOUCH_MAX_PROCESSES && n < touch.processCount; i++) {
			if (touch.pids[i] == -1)
				continue;
			if (lookupProcess(touch.pids[i], (void **)&process, 0) == 0 && process &&
			    process->state != 3)
				mask |= process->sampling[0] | process->sampling[1];
			n++;
		}
		if (touch.ports[0].enableState == 2)
			mask &= ~1;
		if (touch.ports[1].enableState == 2)
			mask &= ~2;
		if (touch.activeMask != mask) {
			touch.activeMask &= 3;
			front = back = 0;
			if (mask) {
				switch (mask & 3) {
				case 2:
				case 3:
					back = touch.forceDefault[1];
					break;
				case 1:
					front = touch.forceDefault[0];
					break;
				}
			}
			touch.config.packet.request.data[1] = front | touch.ports[0].config;
			touch.config.packet.request.data[3] = back | touch.ports[1].config;
			touch.modeMask = mask | 0x80000000;
			config = 1;
			touch.config.pending = 1;
			touch.config.retries = 0;
			touch.config.packet.request.command[0] = TOUCH_CMD_SET_MODE & 0xff;
			touch.config.packet.request.command[1] = TOUCH_CMD_SET_MODE >> 8;
			touch.config.packet.request.length = 5;
			touch.config.packet.request.data[0] = mask & 1;
			touch.config.packet.request.data[2] = (mask >> 1) & 1;
			mask = touch.activeMask;
		} else if (touch.baryonVersion > TOUCH_BARYON_FORCE) {
			if (mask & 1)
				touch.ports[0].forceRequest = forceForChange(0);
			if (touch.activeMask & 2)
				touch.ports[1].forceRequest = forceForChange(1);
			mask = touch.activeMask;
			if (touch.ports[0].forceMode != touch.ports[0].forceRequest ||
			    touch.ports[1].forceMode != touch.ports[1].forceRequest) {
				config = 1;
				touch.config.packet.request.data[0] = touch.ports[0].forceMode != touch.ports[0].forceRequest;
				touch.config.packet.request.data[1] = touch.ports[0].forceRequest;
				touch.config.packet.request.data[4] = touch.ports[1].forceMode != touch.ports[1].forceRequest;
				touch.config.packet.request.data[5] = touch.ports[1].forceRequest;
				touch.config.pending = 1;
				touch.config.retries = 0;
				touch.config.packet.request.data[2] = 0;
				touch.config.packet.request.data[3] = 0;
				touch.config.packet.request.data[6] = 0;
				touch.config.packet.request.data[7] = 0;
				touch.config.packet.request.command[0] = TOUCH_CMD_SET_FORCE & 0xff;
				touch.config.packet.request.command[1] = TOUCH_CMD_SET_FORCE >> 8;
				touch.config.packet.request.length = 9;
			}
		}
	} else if (((touch.state & ~2) == TOUCH_STATE_SUSPENDED || touch.state == TOUCH_STATE_UPDATING) &&
		   touch.activeMask) {
		/* stop reporting */
		touch.config.packet.request.data[1] = touch.ports[0].config;
		touch.config.pending = 1;
		touch.config.retries = 0;
		touch.config.packet.request.data[3] = touch.ports[1].config;
		config = 1;
		touch.config.packet.request.data[0] = 0;
		touch.config.packet.request.data[2] = 0;
		touch.config.packet.request.command[0] = TOUCH_CMD_SET_MODE & 0xff;
		touch.config.packet.request.command[1] = TOUCH_CMD_SET_MODE >> 8;
		touch.config.packet.request.length = 5;
		touch.modeMask = 0x80000000;
	}

	if (touch.sample.pending) {
		alarm = 0;
		touch.sample.retries++;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	} else {
		sample = 0;
		if (touch.state == TOUCH_STATE_RUNNING && mask) {
			sample = mask & 3;
			if ((int)touch.modeMask < 0)
				sample &= touch.modeMask;
		}
		if (sample) {
			/* TOUCH_CMD_SAMPLE_BOTH / _FRONT / _BACK */
			touch.sample.packet.request.command[0] = sample == 3 ? 0 : sample == 1 ? 1 : 2;
			alarm = 0;
			touch.sample.pending = 1;
			touch.sample.packet.request.length = 1;
			touch.sample.packet.request.command[1] = 3;
			touch.sample.retries = 0;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			ret = SceSysconForDriver_C2224E82(&touch.sample.packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX, sampleDone, &touch.sample);
			if (ret < 0)
				touch.sample.pending = 0;
		} else {
			alarm = touch.timer == -1;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		}
	}
	if (config && SceSysconForDriver_C2224E82(&touch.config.packet, 0, configDone, &touch.config) < 0)
		touch.config.pending = 0;
	if (alarm) {
		touch.timer = SceThreadmgrForDriver_C58DF384("SceTouchAlarm0", 1000, sampleAlarm, NULL);
		if (touch.timer < 0)
			touch.timer = -1;
	}
	return -1;
}

/* ---------------------------------------------------------------- processes */

static int startProcess(int pid, int event, ProcEventParam *args, void *common) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret;

	if (event != 0x10000)
		return 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		touch.ports[1].stopped &= ~process->ports[1].eventBit;
		touch.ports[0].stopped &= ~process->ports[0].eventBit;
		process->state = 4;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

/* name unknown: the topmost process region under a point */
static int findOwner(unsigned port, const short *x, const short *y, struct touchOwner *owner)
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
		if (lookupProcess(touch.priorities[i], (void **)&process, 0) < 0 ||
		    !process || process->state)
			continue;
		for (r = TOUCH_REGIONS - 1; r >= 0; r--) {
			region = &process->ports[port].regions[r];
			if (region->active && region->rect.x <= *x && region->rect.y <= *y &&
			    *x < region->rect.x + region->rect.width && *y < region->rect.y + region->rect.height) {
				owner->attr = region->attr;
				owner->region = r;
				owner->pid = touch.priorities[i];
				return i;
			}
		}
	}
	return -1;
}

static inline int regionSet(const struct touchRegion *region)
{
	return region->active && region->rect.x + region->rect.width != 0 && region->rect.y + region->rect.height != 0;
}

/* name unknown: whether the topmost process with a region on the panel samples it */
static int regionTouched(unsigned port)
{
	struct touchProcess *process;
	unsigned i, r;

	for (i = 0; i < touch.priorityCount; i++) {
		if (touch.priorities[i] == -1)
			continue;
		if (lookupProcess(touch.priorities[i], (void **)&process, 0) < 0 ||
		    !process || process->state == 3)
			continue;
		for (r = 0; r < TOUCH_REGIONS; r++)
			if (regionSet(&process->ports[port].regions[r]))
				break;
		if (r == TOUCH_REGIONS)
			continue;
		return ((1U << port) & process->sampling[0]) || ((1U << port) & process->sampling[1]);
	}
	return 0;
}

static int findPid(int pid) /* name unknown */
{
	int i;

	for (i = 0; i < TOUCH_MAX_PROCESSES; i++)
		if (touch.pids[i] == pid)
			return i;
	return -1;
}

static int removeProcess(int pid) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr, i, bit;
	int ret, slot;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		slot = findPid(pid);
		if (!touch.processCount || slot < 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return SCE_TOUCH_ERROR_FATAL;
		}
		touch.processCount--;
		for (i = 0; i < TOUCH_PORTS; i++) {
			bit = process->ports[i].eventBit;
			touch.ports[i].stopped &= ~bit;
			if (i == 0)
				touch.pids[slot] = -1;
			if (touch.ports[i].allocated & bit) {
				sceKernelClearEventFlag(touch.ports[i].events[0], ~bit);
				if (touch.ports[i].allocated & process->ports[i].eventBit)
					sceKernelClearEventFlag(touch.ports[i].events[1],
								 ~process->ports[i].eventBit);
			}
			touch.ports[i].allocated &= ~process->ports[i].eventBit;
		}
		memset(process, 0, sizeof(*process));
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

static int killProcess(int pid, ProcEventParam *args, void *common) /* name unknown */
{
	return removeProcess(pid);
}

static int exitProcess(int pid, ProcEventParam *args, void *common) /* name unknown */
{
	return removeProcess(pid);
}

/* name unknown: owners of new touches, ownership changes of continuing ones */
static int updateOwners(unsigned port, struct touchSample *sample, struct touchTracking *tracking)
{
	struct touchNode *node;
	struct touchProcess *process;
	struct touchOwner owner;
	SceTouchReport *report;
	unsigned i;

	for (node = tracking->added; node; node = node->next) {
		report = &sample->data.report[node->reportIndex];
		findOwner(port, &report->x, &report->y, &node->owner);
	}
	for (node = tracking->active; node; node = node->next) {
		if (node->owner.pid == -1)
			continue;
		if (lookupProcess(node->owner.pid, (void **)&process, 0) < 0 ||
		    !process || process->state == 3)
			continue;
		report = &sample->data.report[node->reportIndex];
		findOwner(port, &report->x, &report->y, &owner);
		if (owner.pid == -1) {
			report->info = 1;
		} else if (owner.pid != node->owner.pid) {
			report->info = 0x100;
			for (i = 0; i < touch.priorityCount; i++) {
				if (lookupProcess(touch.priorities[i], (void **)&process, 0) < 0 ||
				    !process || process->state == 3)
					continue;
				if (touch.priorities[i] == node->owner.pid)
					break;
				if (touch.priorities[i] == owner.pid) {
					report->info = 1;
					break;
				}
			}
		} else if ((short)owner.region != (short)node->owner.region) {
			report->info = (short)owner.region > (short)node->owner.region ? 1 : 0x100;
		}
	}
	if (tracking->added)
		moveNodes(&tracking->added, &tracking->active);
	for (node = tracking->active; node; node = node->next) {
		sample->owners[node->reportIndex].pid = node->owner.pid;
		sample->owners[node->reportIndex].region = node->owner.region;
		sample->owners[node->reportIndex].attr = node->owner.attr;
	}
	return 0;
}

static int stopProcess(int pid, int event, ProcEventParam *args, void *common) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret;

	if (event == 0x202) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		ret = lookupProcess(pid, (void **)&process, 0);
		if (ret >= 0 && process) {
			if (process->state == 2)
				ret = 0;
			else if (process->generation == touch.generation &&
				 (unsigned)(SceThreadmgrForDriver_47F6DE49() - process->timestamp) < 34001)
				ret = -1;
			else {
				process->state = 2;
				ret = 0;
			}
		}
	} else if (event == 0x800) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		ret = lookupProcess(pid, (void **)&process, 0);
		if (ret >= 0 && process) {
			touch.ports[1].stopped |= process->ports[1].eventBit;
			touch.ports[0].stopped |= process->ports[0].eventBit;
			process->state = 3;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return 0;
		}
	} else if (event == 0x200) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		ret = lookupProcess(pid, (void **)&process, 0);
		if (ret >= 0 && process) {
			int front = lastRingSample(&process->ports[0].ring)->data.reportNum;
			int back = lastRingSample(&process->ports[1].ring)->data.reportNum;

			process->state = front || back ? 1 : 2;
			process->timestamp = SceThreadmgrForDriver_47F6DE49();
			process->generation = touch.generation;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return 0;
		}
	} else {
		return 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
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

static int createProcess(int pid) /* name unknown */
{
	struct touchProcess *process;
	struct touchProcessPort *port;
	SceTouchPanelInfo *panel;
	char titleId[0x20];
	unsigned intr, i, r, bit;
	int ret, slot;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 1);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	slot = findPid(-1);
	if (slot < 0) {
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
	if (process->sdkVersion < 0x2500000) {
		process->compat[0] = touch.ports[0].deviceInfo.vendor == TOUCH_VENDOR_ALT;
		if (touch.ports[1].deviceInfo.vendor == TOUCH_VENDOR_ALT)
			process->compat[1] = 1;
		else
			goto title;
	} else {
		process->compat[0] = 0;
		if (touch.ports[1].deviceInfo.vendor == TOUCH_VENDOR_ALT) {
			process->compat[1] = 0;
		} else {
title:
			if (SceSysrootForKernel_EC3124A3(pid, titleId, sizeof(titleId)) >= 0 &&
			    strncmp(titleId, "NPXS10007", sizeof(titleId)) == 0)
				process->compat[1] = 1;
			else
				process->compat[1] = 0;
		}
	}
	for (i = 0; i < TOUCH_PORTS; i++) {
		port = &process->ports[i];
		initializeRing(&port->ring);
		bit = allocateBit(&touch.ports[i].allocated);
		port->eventBit = bit;
		touch.ports[i].stopped &= ~bit;
		for (r = 1; r < TOUCH_REGIONS; r++)
			port->regions[r].active = 0;
		panel = &touch.ports[i].panel;
		port->regions[0].active = 1;
		port->regions[0].rect.x = panel->minAaX;
		port->regions[0].rect.y = panel->minAaY;
		port->regions[0].rect.width = panel->maxAaX + 1 - panel->minAaX;
		port->regions[0].attr = 0;
		port->regions[0].rect.height = panel->maxAaY + 1 - panel->minAaY;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

static int createProcessEvent(int pid, ProcEventCreateParam *args, void *common) /* name unknown */
{
	return createProcess(pid);
}

static const SceKernelProcEventHandler procHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = createProcessEvent,
	.exit = exitProcess,
	.kill = killProcess,
	.suspend = stopProcess,
	.resume = startProcess,
};

/* ------------------------------------------------------------------ sampling */


/* name unknown: a raise / lower request of a panel's force sensing */
static __attribute__((noclone)) void checkForceChange(unsigned port, const struct touchSample *sample)
{
	struct touchPort *p = &touch.ports[port];

	if (p->forceRequest != p->forceMode || !((touch.activeMask >> port) & 1))
		return;
	if ((unsigned)((unsigned)sample->data.timeStamp - p->lastTouchTime) < p->timeout) {
		if (p->forceLevel[0] != p->forceRequest && sample->data.reportNum &&
		    regionTouched(port) && ((touch.activeMask >> port) & 1))
			p->forceChange = 1;
	} else if (p->forceLevel[1] != p->forceRequest) {
		p->forceChange = 2;
	}
}

/* name unknown: one panel's sample into a process ring slot */
static void copySample(int pid, const struct touchProcess *process, struct touchSample *dest,
		       const struct touchSample *sample)
{
	unsigned r;

	SceProcessmgrForDriver_8CB01675(pid, &dest->data.timeStamp, sample->data.timeStamp);
	dest->data.status = sample->data.status;
	if (process->state)
		dest->data.status = sample->data.status | 0x10000;
	dest->data.reportNum = sample->data.reportNum;
	for (r = 0; r < sample->data.reportNum && r < SCE_TOUCH_MAX_REPORT; r++) {
		dest->data.report[r] = sample->data.report[r];
		dest->owners[r] = sample->owners[r];
	}
}

/* name unknown: the syscon's sample reply into every process ring */
static int processSamples(unsigned kinds, const unsigned char *buffer, unsigned size)
{
	struct touchProcess *process;
	struct touchSample *front, *back;
	unsigned i, processed;
	int ret, idle;

	if (parseSamples(kinds, buffer, size, &s_samples[0].data, &s_samples[1].data) != 0)
		return -1;
	if (touch.ctrlCallback) {
		touch.ctrlCallback(0, &s_samples[0].data);
		touch.ctrlCallback(1, &s_samples[1].data);
	}
	if (s_samples[0].data.reportNum)
		touch.ports[0].lastTouchTime = s_samples[0].data.timeStamp;
	if (s_samples[1].data.reportNum)
		touch.ports[1].lastTouchTime = s_samples[1].data.timeStamp;
	checkForceChange(0, &s_samples[0]);
	checkForceChange(1, &s_samples[1]);

	idle = (touch.idleMask & 1) && s_samples[0].data.reportNum && !touch.ports[0].enableState;
	if (((touch.idleMask & 2) && s_samples[1].data.reportNum && !touch.ports[1].enableState) || idle)
		sceKernelPowerTick(0);

	updateTracking(&touch.ports[0].tracking, &s_samples[0].data);
	updateTracking(&touch.ports[1].tracking, &s_samples[1].data);
	updateOwners(0, &s_samples[0], &touch.ports[0].tracking);
	updateOwners(1, &s_samples[1], &touch.ports[1].tracking);

	processed = 0;
	for (i = 0; i < TOUCH_MAX_PROCESSES && processed < touch.processCount; i++) {
		if (touch.pids[i] == -1)
			continue;
		ret = lookupProcess(touch.pids[i], (void **)&process, 0);
		if (ret != 0 || process->state == 3)
			continue;
		front = nextRingSample(&process->ports[0].ring);
		back = nextRingSample(&process->ports[1].ring);
		copySample(touch.pids[i], process, front, &s_samples[0]);
		copySample(touch.pids[i], process, back, &s_samples[1]);
		processed++;
		advanceRing(&process->ports[0].ring);
		advanceRing(&process->ports[1].ring);
	}
	sceKernelSetEventFlag(touch.ports[0].events[0], touch.ports[0].allocated & ~touch.ports[0].stopped);
	sceKernelSetEventFlag(touch.ports[0].events[1], touch.ports[0].allocated & ~touch.ports[0].stopped);
	sceKernelSetEventFlag(touch.ports[1].events[0], touch.ports[1].allocated & ~touch.ports[1].stopped);
	sceKernelSetEventFlag(touch.ports[1].events[1], touch.ports[1].allocated & ~touch.ports[1].stopped);
	return 0;
}

static inline void endFrame(void)
{
	if (touch.ports[0].enableState == 1)
		touch.ports[0].enableState = 2;
	touch.generation++;
	if (touch.ports[1].enableState == 1)
		touch.ports[1].enableState = 2;
}

/* name unknown: SceTouchAlarm0, an empty frame while no panel reports */
static SceUInt32 sampleAlarm(void *argp)
{
	unsigned intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);

	processSamples(0, NULL, 0);
	endFrame();
	touch.timer = -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

/* name unknown: completion of a sample request */
static int sampleDone(SceSysconPacket *packet, void *argp)
{
	struct touchCommand *cmd = argp;
	unsigned intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock), kinds;
	int ret = -1;

	if (cmd && cmd->pending == 1) {
		cmd->pending = 0;
		switch (packet->request.command[0] | packet->request.command[1] << 8) {
		case TOUCH_CMD_SAMPLE_FRONT:
			kinds = 1;
			break;
		case TOUCH_CMD_SAMPLE_BACK:
			kinds = 2;
			break;
		case TOUCH_CMD_SAMPLE_BOTH:
			kinds = 3;
			break;
		default:
			kinds = 0;
			break;
		}
		if (kinds) {
			ret = 0;
			processSamples(kinds, packet->externalRx, packet->externalRxSize);
		}
	}
	endFrame();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

/* name unknown */
int isAltPanel(unsigned port)
{
	return touch.ports[port].deviceInfo.vendor == TOUCH_VENDOR_ALT;
}


/* name unknown: SceTouchInit work queue item */
static int initialize(void *argp)
{
	SceTouchPanelInfo *front = &touch.ports[0].panel, *back = &touch.ports[1].panel;
	unsigned hw;

	SceSysconForDriver_55DF1C9B();
	touch.baryonVersion = SceSysconForDriver_FF86F4C5();
	if (sceSysconCtrlDeviceReset(12, 0) < 0)
		return 1;
	sceKernelDelayThread(1000);
	if (sceSysconCtrlDeviceReset(12, 1) < 0)
		return 1;
	sceKernelDelayThread(100000);
	if (touch.baryonVersion > TOUCH_BARYON_INFO2)
		readDeviceInfo();
	else
		readDeviceInfoOld();

	touch.forceDefault[0] = 0xe0;
	touch.forceDefault[1] = touch.ports[1].deviceInfo.vendor == TOUCH_VENDOR_ALT ? 0xd0 : 0xb0;
	if (touch.baryonVersion > TOUCH_BARYON_FORCE) {
		touch.ports[0].forceMode = 0;
		touch.ports[1].forceMode = 0;
		touch.ports[0].forceRequest = 0;
		touch.ports[1].forceRequest = 0;
		SceSysconForDriver_1546A141(0, 0);
	}
	front->minAaX = 0;
	front->minAaY = 0;
	front->minDispX = 0;
	front->minDispY = 0;
	back->minAaX = 0;
	back->minDispX = 0;
	back->minDispY = 0;
	front->maxAaX = 1919;
	front->maxDispX = 1919;
	back->maxAaX = 1919;
	back->maxDispX = 1919;
	front->maxAaY = 1087;
	front->maxDispY = 1087;
	back->maxDispY = 1087;
	front->minForce = 1;
	back->minForce = 1;
	front->maxForce = 128;
	back->maxForce = 128;
	back->minAaY = 108;
	back->maxAaY = 889;

	hw = SceSysconForDriver_CBD6D8BC() & 0xff0000;
	if (hw == 0x300000 || hw == 0x310000) {
		touch.ports[0].densityX = 22.0f;
		touch.ports[0].densityY = 22.0f;
		touch.ports[1].densityX = 22.0f;
		touch.ports[1].densityY = 22.0f;
	} else {
		touch.ports[0].densityX = 17.54f;
		touch.ports[0].densityY = 17.54f;
		touch.ports[1].densityX = 17.54f;
		touch.ports[1].densityY = touch.ports[1].deviceInfo.vendor == TOUCH_VENDOR_ALT ?
					  17.54f : 24.23f;
	}
	initializeFilter();
	if (SceSysconForDriver_CBD6D8BC() & 0x10000)
		setForceFilterMode(0);
	else
		setForceFilterMode(1);

	touch.ports[0].timeout = 10000000;
	touch.ports[1].timeout = 10000000;
	touch.timer = -1;
	touch.ports[0].forceLevel[0] = 0xff;
	touch.ports[1].forceLevel[0] = 0xff;
	touch.ports[0].forceLevel[1] = 0x10;
	touch.ports[1].forceLevel[1] = 0x10;
	touch.state = TOUCH_STATE_RUNNING;
	touch.sysHandler = sceKernelRegisterSysEventHandler("SceTouch", systemEvent, NULL);
	touch.procHandler = sceKernelRegisterProcEventHandler("SceTouch", &procHandlers, NULL);
	touch.plsId = sceKernelRegisterKPLS("SceTouch", sizeof(struct touchProcess));
	if (touch.plsId < 0)
		return 1;
	createProcess(0x10005);
	sceKernelRegisterSubIntrHandler(8, 5, "SceTouchVblank", vblankHandler, NULL);
	sceKernelEnableSubIntr(8, 5);
	return 1;
}

int module_start(SceSize args, void *argp)
{
	memset(&touch, 0, sizeof(touch));
	memset(s_sampleBuffer, 0, sizeof(s_sampleBuffer));
	memset(touch.pids, -1, sizeof(touch.pids));
	memset(touch.priorities, -1, sizeof(touch.priorities));
	touch.ports[0].config = 6;
	touch.ports[1].config = 4;
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
	touch.sample.packet.externalRx = s_sampleBuffer;
	touch.sample.packet.externalRxSize = sizeof(s_sampleBuffer);
	touch.lock = 0;
	sceKernelScheduleWorkTask(0x10023, "SceTouchInit", initialize, NULL);
	return initializeUser() < 0 ? SCE_KERNEL_START_FAILED : SCE_KERNEL_START_SUCCESS;
}

/* ------------------------------------------------------- tracking, ring reads */

static void copyWords(void *out, const void *in, unsigned bytes) /* name unknown */
{
	unsigned i;

	for (i = 0; i < bytes / 4; i++)
		((unsigned *)out)[i] = ((const unsigned *)in)[i];
}


/* ------------------------------------------------------------ driver exports */

/* name unknown: read (wait for the next frame) or peek a process' ring */
static inline int readPhysical(unsigned port, SceTouchData *out, unsigned count,
			       unsigned regions, unsigned kind, int peek)
{
	struct touchProcess *process;
	unsigned intr, bit, base, flags;
	int ret, pid;

	if (port >= TOUCH_PORTS || !out || count > TOUCH_RING_SIZE || (regions & 0xffffff00))
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	base = (process->sampling[kind] >> port) & 1 ? 0x80000000 : 0;
	if ((process->force[kind] >> port) & 1)
		base |= 0x40;
	flags = base | (peek ? 2 : 1);
	if (!peek) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		bit = process->ports[port].eventBit;
		if (!(bit & touch.ports[port].allocated))
			return SCE_TOUCH_ERROR_INVALID_ARG;
		ret = sceKernelWaitEventFlag(touch.ports[port].events[kind], bit,
					      SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
		if (ret < 0)
			return ret;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
		bit = process->ports[port].eventBit;
		if (bit & touch.ports[port].allocated)
			sceKernelClearEventFlag(touch.ports[port].events[kind], ~bit);
	}
	if (touch.priorityEnabled)
		flags = base | (peek ? 2 : 1) | (process->privilege ? 0x30 : 0x10);
	if (port == 0 ? process->compat[0] : process->compat[1])
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

/* name unknown: sceTouchReadRegion of the Ext sampling */
int readRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 1, 0);
}

/* name unknown */
int peekRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 1, 1);
}

/* name unknown: virtual touch devices are not supported, nothing to read */
int readVirtualWait(unsigned device, unsigned port, SceTouchData *out, unsigned count)
{
	return 0;
}

/* name unknown */
int readVirtualPeek(unsigned device, unsigned port, SceTouchData *out, unsigned count)
{
	return 0;
}

static int setSampling(unsigned port, unsigned state, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr, old, bit;
	int ret, pid;

	if (port >= TOUCH_PORTS || state >= 2)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		old = (process->sampling[kind] >> port) & 1;
		if (old != state) {
			if (state) {
				resetRingReader(&process->ports[port].ring, kind);
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

int sceTouchSetSamplingState(unsigned port, unsigned state)
{
	return setSampling(port, state, 0);
}

/* name unknown */
int setSamplingStateExt(unsigned port, unsigned state)
{
	return setSampling(port, state, 1);
}

static int getSampling(unsigned port, unsigned *state, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= TOUCH_PORTS || !state)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		*state = (process->sampling[kind] >> port) & 1;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int sceTouchGetSamplingState(unsigned port, unsigned *state)
{
	return getSampling(port, state, 0);
}

/* name unknown */
int getSamplingStateExt(unsigned port, unsigned *state)
{
	return getSampling(port, state, 1);
}

static int setForce(unsigned port, unsigned enable, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= TOUCH_PORTS)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		process->force[kind] = (process->force[kind] & ~(1U << port)) | (enable << port);
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int sceTouchEnableTouchForce(unsigned port)
{
	return setForce(port, 1, 0);
}

/* name unknown */
int enableTouchForceExt(unsigned port)
{
	return setForce(port, 1, 1);
}

int sceTouchDisableTouchForce(unsigned port)
{
	return setForce(port, 0, 0);
}

/* name unknown */
int disableTouchForceExt(unsigned port)
{
	return setForce(port, 0, 1);
}

int SceTouchForDriver_328E38B0(unsigned port, int enable)
{
	struct touchTracking *tracking;
	unsigned intr;

	if (port >= TOUCH_PORTS)
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

static int validRegion(unsigned port, const short *rect, int positive) /* name unknown */
{
	SceTouchPanelInfo *panel = &touch.ports[port].panel;

	return rect[0] >= panel->minAaX && rect[1] >= panel->minAaY &&
		(!positive || (rect[2] > 0 && rect[3] > 0)) &&
		rect[0] + rect[2] - 1 <= panel->maxAaX &&
		rect[1] + rect[3] - 1 <= panel->maxAaY;
}

int sceTouchSetRegion(unsigned port, unsigned region, const short *rect, unsigned attr)
{
	struct touchProcess *process;
	struct touchRegion *dest;
	unsigned intr;
	int ret, pid;

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS || !rect || !validRegion(port, rect, 1))
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		dest->active = 1;
		copyWords(&dest->rect.x, rect, 8);
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

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		dest->active = 0;
		dest->attr = 0;
		for (i = 0; i < 8; i++)
			((unsigned char *)&dest->rect.x)[i] = 0;
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

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
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
	unsigned intr;
	int ret, pid;

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS || enable >= 2)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		dest = &process->ports[port].regions[region];
		if (enable && !validRegion(port, &dest->rect.x, 0))
			ret = SCE_TOUCH_ERROR_INVALID_ARG;
		else
			dest->active = enable;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

/*
 * Front-to-back process order for region ownership. Sony's copy loop has
 * no bound; its vectorised form stops after 16 entries (plus a stray tail
 * for counts above 16 that are not multiples of 4).
 */
int SceTouchForDriver_3F3FBF69(const TouchProcessPriorities *priorities)
{
	unsigned intr, i;

	if (!priorities)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	touch.priorityCount = priorities->count;
	touch.priorityEnabled = 1;
	for (i = 0; i < touch.priorityCount && i < TOUCH_MAX_PROCESSES; i++)
		touch.priorities[i] = priorities->processIds[i];
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
	return 0;
}

/* name unknown: debug: clamp coordinates and the force filter on/off */
void SceTouchForDriver_845E2115(unsigned enable)
{
	setClampCoordinates(enable);
	setFilterEnable(enable);
}

int sceTouchGetPanelInfo(unsigned port, SceTouchPanelInfo *out)
{
	if (port >= TOUCH_PORTS || !out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	copyWords(out, &touch.ports[port].panel, sizeof(*out));
	return 0;
}

int getPixelDensity(float *front, float *back) /* name unknown */
{
	if (!front || !back)
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
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		process->privilege = privilege;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

int sceTouchGetProcessInfo(unsigned port, struct touchProcessInfo *out)
{
	struct touchProcess *process;
	unsigned intr, i;

	if (port >= TOUCH_PORTS || !out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	memset(out, 0, sizeof(*out));
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	for (i = 0; i < touch.priorityCount; i++) {
		out->processes[i].pid = touch.priorities[i];
		if (lookupProcess(touch.priorities[i], (void **)&process, 0) < 0 || !process)
			continue;
		out->processes[i].privilege = process->privilege;
		copyWords(out->processes[i].regions, process->ports[port].regions,
			  sizeof(out->processes[i].regions));
	}
	out->count = touch.priorityCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchEnableIdleTimerCancelSetting(unsigned port, int enable)
{
	if (port >= TOUCH_PORTS)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (enable)
		touch.idleMask |= 1U << port;
	else
		touch.idleMask &= ~(1U << port);
	return 0;
}

/* name unknown: debug: report the raw force sum and spread */
void SceTouchForDriver_062A11A2(unsigned enable)
{
	setForceReport(enable);
}

/* name unknown: the raised / lowered force sensing levels of a panel */
int SceTouchForDriver_9C8CCB66(unsigned port, unsigned char raised, unsigned char lowered)
{
	unsigned intr;

	if (port >= TOUCH_PORTS)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	touch.ports[port].forceLevel[0] = raised;
	touch.ports[port].forceLevel[1] = lowered;
	if ((1U << port) & touch.activeMask) {
		touch.ports[port].forceChange = 1;
		touch.ports[port].lastTouchTime = SceThreadmgrForDriver_47F6DE49();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

/* name unknown: idle time after which the force sensing is lowered */
int SceTouchForDriver_384F6C68(unsigned port, unsigned timeout)
{
	unsigned intr;

	if (port >= TOUCH_PORTS)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	touch.ports[port].timeout = timeout;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int sceTouchGetDeviceInfo(unsigned port, struct touchDeviceInfo *out)
{
	struct touchDeviceInfo *info;
	unsigned char *clear;
	unsigned intr, n;

	if (port >= TOUCH_PORTS || !out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	clear = (unsigned char *)out;
	for (n = sizeof(*out); n; n--)
		*clear++ = 0;
	info = &touch.ports[port].deviceInfo;
	out->vendor = info->vendor;
	out->product = info->product;
	out->firmware = info->firmware;
	out->detail[0] = info->detail[0];
	out->detail[1] = info->detail[1];
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

/* name unknown: the driver is initialised */
int SceTouchForDriver_63DCF2FE(void)
{
	return touch.state != 0;
}

int SceTouchForDriver_A32529B8(const SceTouchData *front, const SceTouchData *back)
{
	return 0;
}

/* name unknown: enter the panel firmware update state */
int sceTouchUpdateInit(void)
{
	SceUInt32 timeout = 5000000;
	unsigned intr;
	int ret;

	s_updateEvent = 0;
	s_updateEvent = sceKernelCreateEventFlag("SceTouchUpdate", SCE_KERNEL_EVF_ATTR_MULTI, 0,
						  NULL);
	if (s_updateEvent < 0) {
		s_updateEvent = -1;
		return -1;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	touch.state = TOUCH_STATE_UPDATING;
	if (touch.activeMask == 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		ret = 0;
	} else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		ret = sceKernelWaitEventFlag(s_updateEvent, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, &timeout);
		if (ret < 0) {
			sceKernelDeleteEventFlag(s_updateEvent);
			s_updateEvent = -1;
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

/* name unknown: leave the update state */
int SceTouchForDriver_423F26D3(void)
{
	unsigned intr;

	if (touch.state != TOUCH_STATE_UPDATING)
		return TOUCH_ERROR_BUSY;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
	touch.state = TOUCH_STATE_RUNNING;
	touch.activeMask = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	if (s_updateEvent != -1) {
		sceKernelDeleteEventFlag(s_updateEvent);
		s_updateEvent = -1;
	}
	return 0;
}

static inline void clearDeviceInfo(struct touchDeviceInfo *info)
{
	info->vendor = 0;
	info->product = 0;
	info->firmware = 0;
	info->detail[0] = 0;
	info->detail[1] = 0;
	info->reserved = 0;
}

/* name unknown: reset the panels (bit 0: front, bit 1: back) and re-read their info */
int sceTouchUpdateRestart(unsigned panels)
{
	SceTouchPanelInfo *front = &touch.ports[0].panel, *back = &touch.ports[1].panel;
	unsigned intr, devices;
	int ret;

	if (touch.state != TOUCH_STATE_UPDATING)
		return TOUCH_ERROR_BUSY;
	SceKernelSuspendForDriver_4DF40893(0);
	devices = (panels & 1 ? 8 : 0) | (panels & 2 ? 4 : 0);
	ret = sceSysconCtrlDeviceReset(devices, 0);
	if (ret >= 0) {
		sceKernelDelayThread(1000);
		ret = sceSysconCtrlDeviceReset(devices, 1);
		if (ret >= 0) {
			sceKernelDelayThread(100000);
			SceKernelSuspendForDriver_2BB92967(0);
			clearDeviceInfo(&touch.ports[0].deviceInfo);
			clearDeviceInfo(&touch.ports[1].deviceInfo);
			if (touch.baryonVersion > TOUCH_BARYON_INFO2)
				ret = readDeviceInfo();
			else
				ret = readDeviceInfoOld();
			if (ret >= 0) {
				intr = sceKernelSpinlockLowLockCpuSuspendIntr(&touch.lock);
				back->maxAaY = 889;
				front->minAaX = 0;
				front->minAaY = 0;
				front->minDispX = 0;
				front->minDispY = 0;
				back->minAaX = 0;
				back->minDispX = 0;
				back->minDispY = 0;
				front->maxAaX = 1919;
				front->maxDispX = 1919;
				back->maxAaX = 1919;
				back->maxDispX = 1919;
				front->maxAaY = 1087;
				front->maxDispY = 1087;
				back->maxDispY = 1087;
				front->minForce = 1;
				back->minForce = 1;
				front->maxForce = 128;
				back->maxForce = 128;
				back->minAaY = 108;
				initializeFilter();
				sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			}
			return ret;
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}
