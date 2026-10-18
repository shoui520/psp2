/*
 * SceTouch: front and back touch panels, driven through the syscon.
 *
 * Every vblank the configuration (report mode, force sensing) is pushed to
 * the syscon and a sample request is queued; its reply is parsed into one
 * sample per panel, tracked (touch IDs keep their owner region across
 * frames) and copied into the ring of every process.
 * Private function names and many fields remain unknown.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/intrmgr.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/procevent.h>
#include <syscon/syscon.h>
#include <sysmem/sysroot.h>
#include <motion_dummy/motion_dummy.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysevent.h>
#include <intrmgr/syscall.h>
#include <touch_dummy/touch_dummy.h>
#include <touch/touch.h>

#define TOUCH_ERROR(n)		((int)(0x80350000U | (n)))
#define TOUCH_ERROR_BUSY	TOUCH_ERROR(0x103)

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

struct touchOwner {
	int pid;
	unsigned short region, attr;
};
struct touchSample {
	SceTouchData data;
	struct touchOwner owners[SCE_TOUCH_MAX_REPORT];
};
struct touchRegion {
	int active;
	short x, y, width, height;
	unsigned attr;
};
struct touchRing {
	struct touchSample samples[TOUCH_RING_SIZE];
	unsigned readers[2], writeIndex, reserved;
};
struct touchProcessPort {
	unsigned eventBit, reserved;
	struct touchRegion regions[TOUCH_REGIONS];
	struct touchRing ring;
};
struct touchProcess {
	int pid;
	unsigned char sampling[2], force[2];
	unsigned privilege;
	int state;
	unsigned timestamp;
	unsigned short generation;
	unsigned char compat[TOUCH_PORTS];	/* report without force (old SDK / panel) */
	unsigned reserved;
	unsigned sdkVersion;
	struct touchProcessPort ports[TOUCH_PORTS];
};
struct touchNode {
	unsigned char id, reportIndex, padding[2];
	struct touchOwner owner;
	struct touchNode *next;
};
struct touchTracking {
	struct touchNode *added, *active, *free;
	struct touchNode nodes[SCE_TOUCH_MAX_REPORT];
};
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

/* force filter coefficients, per panel */
struct touchForceGain {
	float gain, gainAlt;
	unsigned maxForce, minForce;
};
struct touchForceSpread {
	float scaleNear, scaleNearAlt, slope, slopeAlt;
	unsigned short nearDistance, farDistance;
	unsigned short lowForce, highForce;
};
struct touchForceRange {
	unsigned short low, high;
	float scale;
};
struct touchForceCurve {
	float scale;
	unsigned short low, high;
};
struct touchFilter {
	unsigned enable;
	struct touchForceCurve curve[TOUCH_PORTS];	/* set at init, never read */
	struct touchForceGain gain[TOUCH_PORTS];
	struct touchForceSpread spread[TOUCH_PORTS];
	struct touchForceRange range[TOUCH_PORTS][3];
};

_Static_assert(sizeof(SceTouchData) == 0x90, "touch data layout");
_Static_assert(sizeof(struct touchSample) == 0xd0, "sample layout");
_Static_assert(sizeof(struct touchRing) == 0x3410, "ring layout");
_Static_assert(sizeof(struct touchProcessPort) == 0x3498, "process port layout");
_Static_assert(sizeof(struct touchProcess) == 0x6950, "process layout");
_Static_assert(sizeof(struct touchPort) == 0xf4, "port layout");
_Static_assert(sizeof(struct touchCommand) == 0x84, "command layout");
_Static_assert(offsetof(struct touchState, processCount) == 0x118, "state layout");
_Static_assert(offsetof(struct touchState, ports) == 0x1a0, "state ports layout");
_Static_assert(sizeof(struct touchState) == 0x3ac, "state layout");
_Static_assert(sizeof(struct touchFilter) == 0x94, "filter layout");

static unsigned s_clampCoordinates;
static unsigned char s_sampleBuffer[0x80];
static struct touchSample s_samples[TOUCH_PORTS];
static struct touchState touch;
static SceSysconPacket s_resetPacket;
static int s_updateEvent;
static unsigned s_forceReport;		/* report the raw force sum and spread (debug) */
static unsigned s_forceFilterMode;	/* use the syscon's state for the force filter */
static int s_readMutex;
static SceTouchData s_readBuffer[TOUCH_RING_SIZE];
static int s_peekMutex;
static SceTouchData s_peekBuffer[TOUCH_RING_SIZE];
static struct touchFilter s_filter;

static const float s_forceMax = 255.0f;

static int sampleDone(SceSysconPacket *packet, void *argp);
static int sampleAlarm(void *argp);

static int lookupProcess(int pid, void **out, int create) /* name unknown */
{
	int ret;

	*out = NULL;
	ret = SceProcessmgrForDriver_AF80F39C(pid, touch.plsId, out, create);
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
static int resetAlarm(void *argp)
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
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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

/* syscon's report of one panel (SceSysconForDriver_030D447F) */
struct touchSysconPanel {
	unsigned short vendor, product, firmware;
	unsigned char detail[2];
	unsigned short reserved;
};

/* name unknown: device info of both panels from the newer syscon interface */
static int readDeviceInfo(void)
{
	struct touchSysconPanel info[TOUCH_PORTS] = { 0 };
	int retries = 3, ret;

	for (;;) {
		ret = SceSysconForDriver_030D447F(info);
		if (ret < 0)
			return ret;
		if ((info[0].product && info[1].product) || retries == 1)
			break;
		retries--;
		sceKernelDelayThread(20000);
	}
	touch.ports[0].deviceInfo.product = info[0].product;
	touch.ports[1].deviceInfo.product = info[1].product;
	touch.ports[0].deviceInfo.vendor = info[0].vendor;
	touch.ports[0].deviceInfo.firmware = info[0].firmware;
	touch.ports[0].deviceInfo.detail[0] = info[0].detail[0];
	touch.ports[0].deviceInfo.detail[1] = info[0].detail[1];
	touch.ports[1].deviceInfo.vendor = info[1].vendor;
	touch.ports[1].deviceInfo.firmware = info[1].firmware;
	touch.ports[1].deviceInfo.detail[0] = info[1].detail[0];
	touch.ports[1].deviceInfo.detail[1] = info[1].detail[1];
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
		ret = SceSysconForDriver_F492E69E(info);
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
			intr = SceCpuForDriver_D32ACE9E(&touch.lock);
			touch.state = TOUCH_STATE_SUSPENDING;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return resume;
		case 0x401:
			if (!touch.activeMask && !touch.ports[0].forceMode && !touch.ports[1].forceMode &&
			    !touch.sample.pending && !touch.config.pending &&
			    touch.state != TOUCH_STATE_IDLE) {
				SceSysconForDriver_40FF3898(12, 0);
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
			SceSysconForDriver_40FF3898(12, resume);
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

	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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

/* name unknown: rings */
static struct touchSample *ringWriteSlot(struct touchRing *ring)
{
	return &ring->samples[ring->writeIndex];
}

/* name unknown */
static void ringAdvance(struct touchRing *ring)
{
	int next = ((int)ring->writeIndex + 1) % TOUCH_RING_SIZE;

	ring->writeIndex = next;
	if (next == (int)ring->readers[0])
		ring->readers[0] = (next + 1) % TOUCH_RING_SIZE;
	if (next == (int)ring->readers[1])
		ring->readers[1] = (next + 1) % TOUCH_RING_SIZE;
}

/* name unknown */
static struct touchSample *ringLastSample(struct touchRing *ring)
{
	if (ring->writeIndex == 0)
		return &ring->samples[TOUCH_RING_SIZE - 1];
	return &ring->samples[ring->writeIndex - 1];
}

/* name unknown */
static int ringReset(struct touchRing *ring)
{
	ring->writeIndex = 0;
	ring->readers[0] = 0;
	ring->readers[1] = 0;
	return 0;
}

/* name unknown: a reader catches up with the writer */
static int ringSyncReader(struct touchRing *ring, int kind)
{
	if (kind >= 2)
		return TOUCH_ERROR(1);
	ring->readers[kind] = ring->writeIndex;
	return 0;
}

static int startProcess(int pid, int event, ProcEventParam *args, int opt) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret;

	if (event != 0x10000)
		return 0;
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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
			if (region->active && region->x <= *x && region->y <= *y &&
			    *x < region->x + region->width && *y < region->y + region->height) {
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
	return region->active && region->x + region->width != 0 && region->y + region->height != 0;
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

	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		slot = findPid(pid);
		if (!touch.processCount || slot < 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return TOUCH_ERROR(0xff);
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

static int killProcess(int pid, ProcEventParam *args, int opt) /* name unknown */
{
	return removeProcess(pid);
}

static int exitProcess(int pid, ProcEventParam *args, int opt) /* name unknown */
{
	return removeProcess(pid);
}

/* name unknown: append list *from to *to */
static int moveNodes(struct touchNode **from, struct touchNode **to)
{
	struct touchNode **tail = to;

	while (*tail)
		tail = &(*tail)->next;
	*tail = *from;
	*from = NULL;
	return 0;
}

static void appendNodes(struct touchNode **list, struct touchNode *nodes) /* name unknown */
{
	while (*list)
		list = &(*list)->next;
	*list = nodes;
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

static int stopProcess(int pid, int event, ProcEventParam *args, int opt) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret;

	if (event == 0x202) {
		intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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
		intr = SceCpuForDriver_D32ACE9E(&touch.lock);
		ret = lookupProcess(pid, (void **)&process, 0);
		if (ret >= 0 && process) {
			touch.ports[1].stopped |= process->ports[1].eventBit;
			touch.ports[0].stopped |= process->ports[0].eventBit;
			process->state = 3;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
			return 0;
		}
	} else if (event == 0x200) {
		intr = SceCpuForDriver_D32ACE9E(&touch.lock);
		ret = lookupProcess(pid, (void **)&process, 0);
		if (ret >= 0 && process) {
			int front = ringLastSample(&process->ports[0].ring)->data.reportNum;
			int back = ringLastSample(&process->ports[1].ring)->data.reportNum;

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

	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 1);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return ret;
	}
	slot = findPid(-1);
	if (slot < 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
		return TOUCH_ERROR(0xff);
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
		ringReset(&port->ring);
		bit = allocateBit(&touch.ports[i].allocated);
		port->eventBit = bit;
		touch.ports[i].stopped &= ~bit;
		for (r = 1; r < TOUCH_REGIONS; r++)
			port->regions[r].active = 0;
		panel = &touch.ports[i].panel;
		port->regions[0].active = 1;
		port->regions[0].x = panel->minAaX;
		port->regions[0].y = panel->minAaY;
		port->regions[0].width = panel->maxAaX + 1 - panel->minAaX;
		port->regions[0].attr = 0;
		port->regions[0].height = panel->maxAaY + 1 - panel->minAaY;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

static int createProcessEvent(int pid, ProcEventCreateParam *args, int opt) /* name unknown */
{
	return createProcess(pid);
}

static SceKernelProcEventHandler procHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = createProcessEvent,
	.exit = exitProcess,
	.kill = killProcess,
	.suspend = stopProcess,
	.resume = startProcess,
};

/* ------------------------------------------------------------------ sampling */

static int parseSamples(unsigned kinds, const unsigned char *buffer, unsigned size,
			SceTouchData *front, SceTouchData *back);
static int initializeTracking(struct touchTracking *tracking);
static int updateTracking(struct touchTracking *tracking, SceTouchData *sample);

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
		SceKernelSuspendForDriver_E0489831(0);

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
		front = ringWriteSlot(&process->ports[0].ring);
		back = ringWriteSlot(&process->ports[1].ring);
		copySample(touch.pids[i], process, front, &s_samples[0]);
		copySample(touch.pids[i], process, back, &s_samples[1]);
		processed++;
		ringAdvance(&process->ports[0].ring);
		ringAdvance(&process->ports[1].ring);
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
static int sampleAlarm(void *argp)
{
	unsigned intr = SceCpuForDriver_D32ACE9E(&touch.lock);

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
	unsigned intr = SceCpuForDriver_D32ACE9E(&touch.lock), kinds;
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
static int isAltPanel(unsigned port)
{
	return touch.ports[port].deviceInfo.vendor == TOUCH_VENDOR_ALT;
}

static void initializeFilter(void);
static void setForceFilterMode(unsigned mode);

/* name unknown: SceTouchInit work queue item */
static int initialize(void *argp)
{
	SceTouchPanelInfo *front = &touch.ports[0].panel, *back = &touch.ports[1].panel;
	unsigned hw;

	SceSysconForDriver_55DF1C9B();
	touch.baryonVersion = SceSysconForDriver_FF86F4C5();
	if (SceSysconForDriver_40FF3898(12, 0) < 0)
		return 1;
	sceKernelDelayThread(1000);
	if (SceSysconForDriver_40FF3898(12, 1) < 0)
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
	touch.procHandler = sceKernelRegisterProcEventHandler("SceTouch", &procHandlers, 0);
	touch.plsId = SceProcessmgrForDriver_3801D7D6("SceTouch", sizeof(struct touchProcess));
	if (touch.plsId < 0)
		return 1;
	createProcess(0x10005);
	sceKernelRegisterSubIntrHandler(8, 5, "SceTouchVblank", vblankHandler, NULL);
	sceKernelEnableSubIntr(8, 5);
	return 1;
}

static int initializeMutexes(void) /* name unknown */
{
	s_peekMutex = -1;
	s_readMutex = -1;
	s_readMutex = sceKernelCreateMutex("SceTouch1", 0, 0, NULL);
	if (s_readMutex >= 0) {
		s_peekMutex = sceKernelCreateMutex("SceTouch2", 0, 0, NULL);
		if (s_peekMutex >= 0)
			return 0;
		if (s_readMutex >= 0)
			sceKernelDeleteMutex(s_readMutex);
	}
	if (s_peekMutex >= 0)
		sceKernelDeleteMutex(s_peekMutex);
	return TOUCH_ERROR(0xff);
}

int module_start(SceSize args, void *argp)
{
	memset(&touch, 0, sizeof(touch));
	memset(s_sampleBuffer, 0, sizeof(s_sampleBuffer));
	memset(touch.pids, 0xff, sizeof(touch.pids));
	memset(touch.priorities, 0xff, sizeof(touch.priorities));
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
	return initializeMutexes() < 0 ? SCE_KERNEL_START_FAILED : SCE_KERNEL_START_SUCCESS;
}

/* ------------------------------------------------------------- sample parsing */

/* one report of the syscon's sample reply */
#define TOUCH_RAW_REPORT	6
#define TOUCH_RAW_FRONT		6	/* reports per panel in the reply */
#define TOUCH_RAW_BACK		4
#define TOUCH_RAW_PANEL		0x3c	/* bytes per panel */
#define TOUCH_RAW_INVALID	0x80	/* id byte of an unused report */

static inline unsigned rawX(const unsigned char *raw)
{
	return raw[1] + ((raw[2] & 7) << 8);
}

static inline unsigned rawY(const unsigned char *raw)
{
	return raw[3] + ((raw[4] & 7) << 8);
}

static inline void clearReserved(SceTouchReport *report)
{
	unsigned i;

	for (i = 0; i < sizeof(report->rsv); i++)
		report->rsv[i] = 0;
}

/* name unknown: raw force sum and spread (s_forceReport) into reports[0].reserved */
static inline void reportForceInfo(const unsigned char *panel, unsigned count, SceTouchData *out)
{
	unsigned short sum = (panel[0x34] + (panel[0x35] << 8)) << 5;
	unsigned short spread = 0;
	unsigned first, last;

	if (count) {
		first = panel[0x30] + ((panel[0x31] & 0xf) << 8);
		last = (panel[0x32] << 4) + (panel[0x31] >> 4);
		if (last < first)
			last += 0x1000;
		spread = last - first;
	}
	out->report[0].rsv[0] = sum;
	out->report[0].rsv[2] = spread;
	out->report[0].rsv[1] = sum >> 8;
	out->report[0].rsv[3] = spread >> 8;
}

static void forceSpread(unsigned port, SceTouchData *data, unsigned flags);
static void forceRange(unsigned port, SceTouchData *data, unsigned flags);

/* name unknown: the syscon's sample reply into one sample per panel */
static int parseSamples(unsigned kinds, const unsigned char *buffer, unsigned size,
			SceTouchData *front, SceTouchData *back)
{
	SceTouchReport *report;
	const unsigned char *raw;
	unsigned n, x, y;
	uint64_t time;
	int flags, busy;

	if (!front || !back)
		return TOUCH_ERROR(1);
	time = sceKernelGetSystemTimeWide();
	back->status = 0;
	back->reportNum = 0;
	front->timeStamp = time;
	front->status = 0;
	front->reportNum = 0;
	back->timeStamp = time;

	if (kinds & 1) {
		if (!buffer || size < TOUCH_RAW_PANEL)
			return TOUCH_ERROR(1);
		n = 0;
		for (raw = buffer; raw != buffer + TOUCH_RAW_FRONT * TOUCH_RAW_REPORT;
		     raw += TOUCH_RAW_REPORT) {
			if (raw[0] & TOUCH_RAW_INVALID)
				continue;
			report = &front->report[n];
			report->id = raw[0];
			x = rawX(raw);
			y = rawY(raw);
			if (s_clampCoordinates) {
				if (x >= 1919)
					x = 1919;
				if (y >= 1087)
					y = 1087;
			}
			report->x = x;
			n++;
			report->y = y;
			report->force = raw[5];
			report->info = 0;
			clearReserved(report);
		}
		if (s_forceReport)
			reportForceInfo(buffer, n, front);
		front->reportNum = n;
		if (!(kinds & 2))
			goto filter;
		buffer += TOUCH_RAW_PANEL;
		size -= TOUCH_RAW_PANEL;
	} else if (!(kinds & 2)) {
		goto filter;
	}

	if (!buffer || size < TOUCH_RAW_PANEL)
		return TOUCH_ERROR(1);
	n = 0;
	back->status = 0;
	for (raw = buffer; raw != buffer + TOUCH_RAW_BACK * TOUCH_RAW_REPORT; raw += TOUCH_RAW_REPORT) {
		if (raw[0] & TOUCH_RAW_INVALID)
			continue;
		report = &back->report[n];
		report->id = raw[0];
		x = rawX(raw);
		y = rawY(raw);
		if (s_clampCoordinates) {
			/* the back panel covers display rows 108..889 */
			report->x = x < 1919 ? x : 1919;
			report->y = y > 781 ? 889 : y + 108;
		} else {
			report->x = x;
			report->y = y;
		}
		n++;
		report->force = raw[5];
		report->info = 0;
		clearReserved(report);
	}
	if (s_forceReport)
		reportForceInfo(buffer, n, back);
	back->reportNum = n;

filter:
	if (!isAltPanel(0))
		return 0;
	if (!s_forceFilterMode) {
		forceRange(0, front, 2);
		forceRange(1, back, 2);
		return 0;
	}
	busy = SceSysconForDriver_142D5E82();
	if (SceSysconForDriver_9DA2A5AB() == 0) {
		flags = busy != 0;
		forceSpread(0, front, flags);
		forceSpread(1, back, flags);
	} else {
		flags = 2;
	}
	forceRange(0, front, flags);
	forceRange(1, back, flags);
	return 0;
}

/* ------------------------------------------------------------- force filter */

/*
 * name unknown: raise each touch's force by the pressure spread from the
 * other touches on the panel, then apply the panel's gain curve.
 */
static void forceSpread(unsigned port, SceTouchData *data, unsigned flags)
{
	const struct touchForceSpread *spread = &s_filter.spread[port];
	const struct touchForceGain *gain = &s_filter.gain[port];
	float boost[SCE_TOUCH_MAX_REPORT], scaleNear, slope, g, range, w, out, fs, fu;
	unsigned n = data->reportNum, i, j, d, f, sum, high;
	SceTouchReport *report;
	int diff;

	if (!s_filter.enable)
		return;
	if (n < 2) {
		if (n == 1) {
			f = data->report[0].force;
			g = flags & 1 ? gain->gainAlt : gain->gain;
			range = (float)(gain->maxForce - gain->minForce);
			out = (float)f + (float)(f - gain->minForce) / range * g * (float)(int)f;
			data->report[0].force = out > s_forceMax ? 255 : (unsigned char)(unsigned)out;
		}
		return;
	}

	scaleNear = flags & 1 ? spread->scaleNearAlt : spread->scaleNear;
	slope = flags & 1 ? spread->slopeAlt : spread->slope;
	for (i = 0; i < n; i++) {
		boost[i] = 0.0f;
		for (j = 0; j < n; j++) {
			if (i == j)
				continue;
			diff = data->report[i].y - data->report[j].y;
			if (diff < 0)
				diff = -diff;
			d = (unsigned short)diff;
			if (d < spread->nearDistance)
				w = scaleNear;
			else if (d < spread->farDistance)
				w = (float)(int)(spread->farDistance - diff) * slope;
			else
				continue;
			if (!(w > 0.0f))
				continue;
			f = data->report[j].force;
			if (f <= spread->lowForce)
				continue;
			high = spread->highForce;
			w *= (float)(f <= high ? f - spread->lowForce : high - spread->lowForce) /
			     (float)(int)(high - spread->lowForce);
			if (w > boost[i])
				boost[i] = w;
		}
	}

	g = flags & 1 ? gain->gainAlt : gain->gain;
	range = (float)(gain->maxForce - gain->minForce);
	for (i = 0; i < n; i++) {
		report = &data->report[i];
		if (boost[i] > 0.0f) {
			sum = report->force + (unsigned)(boost[i] * (float)(int)report->force);
			if (sum <= 255) {
				report->force = sum;
				fs = (float)(int)sum;
				fu = (float)sum;
			} else {
				report->force = 255;
				sum = 255;
				fu = s_forceMax;
				fs = fu;
			}
		} else {
			sum = report->force;
			fs = (float)(int)sum;
			fu = (float)sum;
		}
		out = fu + (float)(sum - gain->minForce) / range * g * fs;
		report->force = out > s_forceMax ? 255 : (unsigned char)(unsigned)out;
	}
}

/* name unknown: scale each touch's force into the panel's reporting range */
static void forceRange(unsigned port, SceTouchData *data, unsigned flags)
{
	const struct touchForceRange *range;
	unsigned n, i, f;
	unsigned char low, high;

	if (!s_filter.enable)
		return;
	if (flags & 2)
		range = &s_filter.range[port][2];
	else if (flags & 1)
		range = &s_filter.range[port][1];
	else
		range = &s_filter.range[port][0];
	n = data->reportNum;
	if (!n)
		return;
	low = range->low;
	high = range->high;
	for (i = 0; i < n; i++) {
		f = (unsigned char)(unsigned)((float)(int)data->report[i].force * range->scale);
		if (f >= high)
			f = high;
		data->report[i].force = low < f ? (unsigned char)(f - low) : 1;
	}
}

/* name unknown: the force filter's coefficients */
static void initializeFilter(void)
{
	s_filter.enable = 1;
	s_filter.curve[0].scale = 0.08f;
	s_filter.curve[0].low = 0x3c;
	s_filter.curve[0].high = 0xa0;
	s_filter.curve[1].scale = 0.05f;
	s_filter.curve[1].low = 0x3c;
	s_filter.curve[1].high = 0xaa;
	s_filter.gain[0] = (struct touchForceGain){ 0.6f, 0.18f, 0xff, 0 };
	s_filter.gain[1] = (struct touchForceGain){ 0.6f, 0.25f, 0xff, 0 };
	s_filter.spread[0] = (struct touchForceSpread){ 0.45f, 0.1f, 0.45f / 280, 0.1f / 280,
							 70, 350, 52, 170 };
	s_filter.spread[1] = (struct touchForceSpread){ 0.45f, 0.15f, 0.45f / 190, 0.15f / 190,
							 40, 230, 40, 120 };
	s_filter.range[0][0] = (struct touchForceRange){ 0x40, 0xc0, 0.85f };
	s_filter.range[0][1] = (struct touchForceRange){ 0x40, 0xc0, 0.8f };
	s_filter.range[0][2] = (struct touchForceRange){ 0x40, 0xc0, 0.76f };
	s_filter.range[1][0] = (struct touchForceRange){ 0x38, 0xb8, 0.9f };
	s_filter.range[1][1] = (struct touchForceRange){ 0x38, 0xb8, 0.9f };
	s_filter.range[1][2] = (struct touchForceRange){ 0x40, 0xc0, 0.8f };
}

/* names unknown: debug switches (SceTouchForDriver_062A11A2 / _845E2115) */
static void setForceReport(unsigned enable)
{
	s_forceReport = enable;
}

static void setForceFilterMode(unsigned mode)
{
	s_forceFilterMode = mode;
}

static void setClampCoordinates(unsigned clamp)
{
	s_clampCoordinates = clamp;
}

static void setFilterEnable(unsigned enable)
{
	s_filter.enable = enable;
}

/* ------------------------------------------------------- tracking, ring reads */

static void copyWords(void *out, const void *in, unsigned bytes) /* name unknown */
{
	unsigned i;

	for (i = 0; i < bytes / 4; i++)
		((unsigned *)out)[i] = ((const unsigned *)in)[i];
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


/* ------------------------------------------------------------ driver exports */

/* name unknown: read (wait for the next frame) or peek a process' ring */
static inline int readPhysical(unsigned port, SceTouchData *out, unsigned count,
			       unsigned regions, unsigned kind, int peek)
{
	struct touchProcess *process;
	unsigned intr, bit, base, flags;
	int ret, pid;

	if (port >= TOUCH_PORTS || !out || count > TOUCH_RING_SIZE || (regions & 0xffffff00))
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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

/* name unknown: SceTouchForDriver_9A91F624 of the Ext sampling */
static int readRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 1, 0);
}

/* name unknown */
static int peekRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions)
{
	return readPhysical(port, out, count, regions, 1, 1);
}

/* name unknown: virtual touch devices are not supported, nothing to read */
static int readVirtual(void)
{
	return 0;
}

/* name unknown */
static int peekVirtual(void)
{
	return 0;
}

static int setSampling(unsigned port, unsigned state, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr, old, bit;
	int ret, pid;

	if (port >= TOUCH_PORTS || state >= 2)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		old = (process->sampling[kind] >> port) & 1;
		if (old != state) {
			if (state) {
				ringSyncReader(&process->ports[port].ring, kind);
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

/* name unknown */
static int setSamplingExt(unsigned port, unsigned state)
{
	return setSampling(port, state, 1);
}

static int getSampling(unsigned port, unsigned *state, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= TOUCH_PORTS || !state)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
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

/* name unknown */
static int getSamplingExt(unsigned port, unsigned *state)
{
	return getSampling(port, state, 1);
}

static int setForce(unsigned port, unsigned enable, unsigned kind) /* name unknown */
{
	struct touchProcess *process;
	unsigned intr;
	int ret, pid;

	if (port >= TOUCH_PORTS)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
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

/* name unknown */
static int enableTouchForceExt(unsigned port)
{
	return setForce(port, 1, 1);
}

int SceTouchForDriver_CDE79889(unsigned port)
{
	return setForce(port, 0, 0);
}

/* name unknown */
static int disableTouchForceExt(unsigned port)
{
	return setForce(port, 0, 1);
}

int SceTouchForDriver_328E38B0(unsigned port, int enable)
{
	struct touchTracking *tracking;
	unsigned intr;

	if (port >= TOUCH_PORTS)
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

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS || !rect || !validRegion(port, rect, 1))
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
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

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
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

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
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

	if (port >= TOUCH_PORTS || region >= TOUCH_REGIONS || enable >= 2)
		return TOUCH_ERROR(1);
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	ret = lookupProcess(pid, (void **)&process, 0);
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

/*
 * Front-to-back process order for region ownership. Sony's copy loop has
 * no bound; its vectorised form stops after 16 entries (plus a stray tail
 * for counts above 16 that are not multiples of 4).
 */
int SceTouchForDriver_3F3FBF69(const TouchProcessPriorities *priorities)
{
	unsigned intr, i;

	if (!priorities)
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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

int SceTouchForDriver_937DB4C0(unsigned port, SceTouchPanelInfo *out)
{
	if (port >= TOUCH_PORTS || !out)
		return TOUCH_ERROR(1);
	copyWords(out, &touch.ports[port].panel, sizeof(*out));
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
	ret = lookupProcess(pid, (void **)&process, 0);
	if (ret >= 0 && process) {
		process->privilege = privilege;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return ret;
}

/* the regions of every process with a priority, for one panel */
struct touchProcessInfo {
	unsigned count;
	struct {
		int pid;
		unsigned privilege;
		struct touchRegion regions[TOUCH_REGIONS];
	} processes[TOUCH_MAX_PROCESSES];
};

_Static_assert(sizeof(struct touchProcessInfo) == 0x884, "process info layout");

int SceTouchForDriver_4DF3A37C(unsigned port, struct touchProcessInfo *out)
{
	struct touchProcess *process;
	unsigned intr, i;

	if (port >= TOUCH_PORTS || !out)
		return TOUCH_ERROR(1);
	memset(out, 0, sizeof(*out));
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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

int SceTouchForDriver_EBF4C86A(unsigned port, int enable)
{
	if (port >= TOUCH_PORTS)
		return TOUCH_ERROR(1);
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
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
	touch.ports[port].timeout = timeout;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&touch.lock, intr);
	return 0;
}

int SceTouchForDriver_FD2E0278(unsigned port, struct touchDeviceInfo *out)
{
	struct touchDeviceInfo *info;
	unsigned char *clear;
	unsigned intr, n;

	if (port >= TOUCH_PORTS || !out)
		return TOUCH_ERROR(1);
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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
int SceTouchForDriver_DDCF5145(void)
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
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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
	intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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
int SceTouchForDriver_4E6C4558(unsigned panels)
{
	SceTouchPanelInfo *front = &touch.ports[0].panel, *back = &touch.ports[1].panel;
	unsigned intr, devices;
	int ret;

	if (touch.state != TOUCH_STATE_UPDATING)
		return TOUCH_ERROR_BUSY;
	SceKernelSuspendForDriver_4DF40893(0);
	devices = (panels & 1 ? 8 : 0) | (panels & 2 ? 4 : 0);
	ret = SceSysconForDriver_40FF3898(devices, 0);
	if (ret >= 0) {
		sceKernelDelayThread(1000);
		ret = SceSysconForDriver_40FF3898(devices, 1);
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
				intr = SceCpuForDriver_D32ACE9E(&touch.lock);
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

/* ------------------------------------------------------------ user syscalls */

/* the kernel read behind a user read */
static inline int kernelRead(unsigned port, SceTouchData *buffer, unsigned count,
			     unsigned regions, unsigned kind, int peek)
{
	if (kind)
		return peek ? peekRegionExt(port, buffer, count, regions) :
			      readRegionExt(port, buffer, count, regions);
	if (regions)
		return peek ? SceTouchForDriver_9B3F7207(port, buffer, count, regions) :
			      SceTouchForDriver_9A91F624(port, buffer, count, regions);
	return peek ? SceTouchForDriver_BAD1960B(port, buffer, count) : SceTouchForDriver_70C8AACE(port, buffer, count);
}

/* regions 0: the plain read (all regions) */
static int userRead(unsigned port, void *out, unsigned count, unsigned regions,
		    unsigned kind, int peek) /* name unknown */
{
	SceTouchData *buffer = peek || kind ? s_peekBuffer : s_readBuffer;
	int mutex = peek || kind ? s_peekMutex : s_readMutex;
	unsigned t = sceSyscallEnter();
	int ret, copied, unlocked;

	if (!out || count - 1 >= TOUCH_RING_SIZE || port >= TOUCH_PORTS) {
		ret = TOUCH_ERROR(1);
	} else {
		ret = sceKernelLockMutex(mutex, 1, NULL);
		if (ret >= 0) {
			ret = kernelRead(port, buffer, count, regions, kind, peek);
			if (ret > 0) {
				copied = SceSysmemForDriver_6D88EF8A(out, buffer, ret * sizeof(SceTouchData));
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
	return userRead(port, out, count, 0, 0, 0);
}

int sceTouchPeek(SceUInt32 port, SceTouchData *out, SceUInt32 count)
{
	return userRead(port, out, count, 0, 0, 1);
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
	int ret = SceTouchForDriver_1247257A(port, state);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetSamplingStateExt(unsigned port, unsigned state)
{
	unsigned t = sceSyscallEnter();
	int ret = setSamplingExt(port, state);

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
	ret = kind ? getSamplingExt(port, &value) : SceTouchForDriver_174D0333(port, &value);
	if (ret >= 0)
		ret = SceSysmemForDriver_6D88EF8A(out, &value, 4);
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

int sceTouchEnableTouchForce(SceUInt32 port)
{
	unsigned t = sceSyscallEnter();
	int ret = SceTouchForDriver_E41B4BF8(port);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchEnableTouchForceExt(unsigned port)
{
	unsigned t = sceSyscallEnter();
	int ret = enableTouchForceExt(port);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchDisableTouchForce(SceUInt32 port)
{
	unsigned t = sceSyscallEnter();
	int ret = SceTouchForDriver_CDE79889(port);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchDisableTouchForceExt(unsigned port)
{
	unsigned t = sceSyscallEnter();
	int ret = disableTouchForceExt(port);

	sceSyscallLeave(t);
	return ret;
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
		ret = SceSysmemForDriver_6D88EF8A(out, &info, sizeof(info));
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
			ret = SceSysmemForDriver_6D88EF8A(front, &values[0], 4);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(back, &values[1], 4);
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
			ret = SceSysmemForDriver_6D88EF8A(frontX, &values[0], 4);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(frontY, &values[1], 4);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(backX, &values[2], 4);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(backY, &values[3], 4);
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
	ret = SceSysmemForDriver_BC996A7A(copy, rect, 8);
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

int sceTouchGetProcessInfo(unsigned port, void *out)
{
	struct touchProcessInfo info;
	unsigned t = sceSyscallEnter();
	int ret = TOUCH_ERROR(1);

	if (out) {
		ret = SceTouchForDriver_4DF3A37C(port, &info);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(out, &info, sizeof(info));
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
			ret = SceSysmemForDriver_6D88EF8A(out, &info, sizeof(info));
	}
	sceSyscallLeave(t);
	return ret;
}


int sceTouchRead2(unsigned device, unsigned port, void *out, unsigned count)
{
	unsigned t;
	int ret;

	if (!device)
		return userRead(port, out, count, 0, 0, 0);
	t = sceSyscallEnter();
	ret = readVirtual();
	sceSyscallLeave(t);
	return ret;
}

int sceTouchPeek2(unsigned device, unsigned port, void *out, unsigned count)
{
	unsigned t;
	int ret;

	if (!device)
		return userRead(port, out, count, 0, 0, 1);
	t = sceSyscallEnter();
	ret = peekVirtual();
	sceSyscallLeave(t);
	return ret;
}
