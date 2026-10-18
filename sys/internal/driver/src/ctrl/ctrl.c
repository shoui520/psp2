/* Controller sampling, per-process rings and remote-control notification. */
#include <sysmem/sysmem.h>
#include <sysmem/uid_guid.h>
#include "ctrl_internal.h"
#include <psp2kern/kernel/processmgr.h>
#include <intrmgr/intrmgr.h>
#include <acmgr/acmgr.h>
#include <processmgr/processmgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysevent.h>
#include <sysmem/procevent.h>
#include <sysmem/sysroot.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>

struct ctrlStorage ctrlStorage = {
	.bdRemoconNotify = 0xff,
	.bdRemoconPendingNotify = 0xff,
};

void ctrlPublishSample(int port, struct ctrlNativeSample *sample, int source);
int ctrlPublishCachedSample(void);
#if FW_VERSION >= 0x04000000
int _sceCtrlBdRMCForceReleaseCallback(SceUID pid);
#else
static inline __attribute__((always_inline))
int _sceCtrlBdRMCForceReleaseCallback(SceUID pid);
#endif

/* Name unknown: the native buffer entries inline this sample conversion. */
static inline __attribute__((always_inline)) void ctrlCopyBufferSample(
		SceCtrlData2 *out, const struct ctrlNativeSample *in,
		const struct ctrlProcessStorage *process, int mode, int negative,
		int extended, int version2)
{
	uint32_t buttons, mapped;

	out->timeStamp = in->sample.timeStamp;
	buttons = in->sample.buttons;
	if (!process->privileged) {
		if (!process->specialAuthority)
			buttons &= ~0x04000000U;
		if (!ctrlStorage.foregroundSelected)
			buttons &= ctrlStorage.maskForNonShell & ~0x10000U;
		else if (buttons & 0x10000)
			buttons &= 0x30000;
		else if (buttons & 0x02000000)
			buttons = (buttons & 0x0402ffff) | 0x10000;
		else if (buttons & 0x80000000) {
			if (extended)
				buttons &= 0x7fffffff;
			else
				buttons = (buttons & 0x30000) | 0x10000;
		}
	} else if (!ctrlStorage.foregroundSelected) {
		buttons &= ctrlStorage.maskForAll;
	}
	mapped = buttons;
	if (!version2) {
		mapped &= 0xfffff0f9;
		if (buttons & 0x400)
			mapped |= 0x100;
		if (buttons & 0x800)
			mapped |= 0x200;
	}
	out->buttons = negative ? ~mapped : mapped;
	if (!mode) {
		out->lx = 0x80;
		out->ly = 0x80;
		out->rx = 0x80;
		out->ry = 0x80;
	} else if (mode == 2) {
		out->lx = in->sample.wideLx;
		out->ly = in->sample.wideLy;
		out->rx = in->sample.wideRx;
		out->ry = in->sample.wideRy;
	} else if (mode == 3) {
		out->lx = in->extended[0];
		out->ly = in->extended[1];
		out->rx = in->extended[2];
		out->ry = in->extended[3];
	} else {
		out->lx = in->sample.lx;
		out->ly = in->sample.ly;
		out->rx = in->sample.rx;
		out->ry = in->sample.ry;
	}
	if (version2) {
		out->up = in->sample.up;
		out->right = in->sample.right;
		out->down = in->sample.down;
		out->left = in->sample.left;
		out->l2 = in->sample.l2;
		out->r2 = in->sample.r2;
		out->l1 = in->sample.l1;
		out->r1 = in->sample.r1;
		out->triangle = in->sample.triangle;
		out->circle = in->sample.circle;
		out->cross = in->sample.cross;
		out->square = in->sample.square;
		out->rsrv[0] = 0;
		out->rsrv[1] = 0;
		out->rsrv[2] = 0;
		out->rsrv[3] = 0;
	} else {
		memset(&out->up, 0, sizeof(*out) - offsetof(SceCtrlData2, up));
	}
}

/* Name unknown: shared inline shape of the positive/negative peek entries. */
static inline __attribute__((always_inline)) int ctrlPeekBuffer(
		int port, SceCtrlData2 *samples, int count, int negative,
		int extended, int version2)
{
	struct ctrlProcessStorage *process = NULL;
	struct ctrlSampleRing *ring;
	SceKernelIntrStatus intr;
	SceUID pid;
	int index, mode, i, result;

	if ((uint32_t)count - 1 >= 64)
		return SCE_CTRL_ERROR_INVALID_ARG;
	if (SceSysrootForDriver_8AA268D6(1)) {
		if ((uint32_t)port > 4)
			return SCE_CTRL_ERROR_INVALID_ARG;
	} else if (port) {
		return SCE_CTRL_ERROR_INVALID_ARG;
	}
	if (!samples)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
			ctrlStorage.processStorageKey, (void **)&process, 0) < 0 ||
	    !process) {
		result = SCE_CTRL_ERROR_PRIV_REQUIRED;
		goto out;
	}
	if (process->mode3Blocked && port == 2 && process->sdkVersion < 0x03100000) {
		result = SCE_CTRL_ERROR_INVALID_ARG;
		goto out;
	}
	mode = extended ? process->samplingModeExt : process->samplingMode;
	ring = process->ports[port].ring;
	index = ring->writeIndex;
	if (count != 64) {
		index -= count;
		if (index < 0)
			index += 64;
	}
	for (i = 0; i < count; i++) {
		ctrlCopyBufferSample(&samples[i], &ring->samples[index], process,
				     mode, negative, extended, version2);
		if (++index == 64)
			index = 0;
	}
	if (process->processState == 3)
		process->processState = 0;
	result = count;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int SceCtrlForDriver_EA1D3A34(int port, SceCtrlData *samples, int count)
{
	return ctrlPeekBuffer(port, (SceCtrlData2 *)samples, count, 0, 0, 0);
}

int SceCtrlForDriver_19895843(int port, SceCtrlData *samples, int count)
{
	return ctrlPeekBuffer(port, (SceCtrlData2 *)samples, count, 1, 0, 0);
}

/* Name unknown: both native read entries inline the same wait/copy sequence. */
static inline __attribute__((always_inline)) int ctrlReadBuffer(
		int port, SceCtrlData2 *samples, int count, int negative,
		int extended, int version2)
{
	struct ctrlProcessStorage *process, *lookup = NULL;
	struct ctrlSampleRing *ring;
	SceKernelIntrStatus intr;
	SceUID pid, event;
	uint32_t eventBit;
	int index, writeIndex, available, mode, i, result;

	if ((uint32_t)count - 1 >= 64)
		return SCE_CTRL_ERROR_INVALID_ARG;
	if (SceSysrootForDriver_8AA268D6(1)) {
		if ((uint32_t)port > 4)
			return SCE_CTRL_ERROR_INVALID_ARG;
	} else if (port) {
		return SCE_CTRL_ERROR_INVALID_ARG;
	}
	if (!samples)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	event = extended ? ctrlStorage.ports[port].negative :
			   ctrlStorage.ports[port].positive;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
			ctrlStorage.processStorageKey, (void **)&lookup, 0) < 0 ||
	    !lookup) {
		result = SCE_CTRL_ERROR_PRIV_REQUIRED;
		goto out;
	}
	process = lookup;
	if (process->mode3Blocked && port == 2 && process->sdkVersion < 0x03100000) {
		result = SCE_CTRL_ERROR_INVALID_ARG;
		goto out;
	}
	eventBit = process->ports[port].eventBit;
	ring = process->ports[port].ring;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	result = sceKernelWaitEventFlag(event, eventBit, SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
	if (result < 0)
		return result;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	sceKernelClearEventFlag(event, ~eventBit);
	lookup = NULL;
	if (SceProcessmgrForDriver_AF80F39C(pid,
			ctrlStorage.processStorageKey, (void **)&lookup, 0) < 0 ||
	    !lookup) {
		result = SCE_CTRL_ERROR_PRIV_REQUIRED;
		goto out;
	}
	/* Native code validates the second lookup but retains the first record/ring. */
	mode = extended ? process->samplingModeExt : process->samplingMode;
	writeIndex = ring->writeIndex;
	index = extended ? ring->readIndexExt : ring->readIndex;
	if (extended)
		ring->readIndexExt = writeIndex;
	else
		ring->readIndex = writeIndex;
	available = writeIndex - index;
	if (available < 0)
		available += 64;
	if (count < available) {
		index = writeIndex - count;
		if (index < 0)
			index += 64;
	} else {
		count = available;
	}
	for (i = 0; i < count; i++) {
		ctrlCopyBufferSample(&samples[i], &ring->samples[index], process,
				     mode, negative, extended, version2);
		if (++index == 64)
			index = 0;
	}
	if (process->processState == 3)
		process->processState = 0;
	result = count;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int SceCtrlForDriver_9B96A1AA(int port, SceCtrlData *samples, int count)
{
	return ctrlReadBuffer(port, (SceCtrlData2 *)samples, count, 0, 0, 0);
}

int SceCtrlForDriver_8D4E0DD1(int port, SceCtrlData *samples, int count)
{
	return ctrlReadBuffer(port, (SceCtrlData2 *)samples, count, 1, 0, 0);
}

int ctrlPeekBufferPositiveExt(int port, SceCtrlData2 *samples, int count)
{
	return ctrlPeekBuffer(port, samples, count, 0, 1, 0);
}

int ctrlReadBufferPositiveExt(int port, SceCtrlData2 *samples, int count)
{
	return ctrlReadBuffer(port, samples, count, 0, 1, 0);
}

int ctrlPeekBufferPositive2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlPeekBuffer(port, samples, count, 0, 0, 1);
}

int ctrlPeekBufferNegative2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlPeekBuffer(port, samples, count, 1, 0, 1);
}

int ctrlReadBufferPositive2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlReadBuffer(port, samples, count, 0, 0, 1);
}

int ctrlReadBufferNegative2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlReadBuffer(port, samples, count, 1, 0, 1);
}

int ctrlPeekBufferPositiveExt2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlPeekBuffer(port, samples, count, 0, 1, 1);
}

int ctrlReadBufferPositiveExt2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlReadBuffer(port, samples, count, 0, 1, 1);
}

/* Name unknown: the syscall entries inline this locked staging-buffer copy. */
static inline __attribute__((always_inline)) int ctrlUserBuffer(
		int port, void *samples, int count, int read, int negative,
		int extended, int version2)
{
	struct ctrlUserBuffer *buffer = &ctrlStorage.userBuffers[read ? 0 : 1];
	uint32_t context = sceSyscallEnter();
	int result, copied, unlocked;

	if (!samples || (uint32_t)count - 1 >= 64) {
		result = SCE_CTRL_ERROR_INVALID_ARG;
		goto out;
	}
	result = sceKernelLockMutex(buffer->mutex, 1, NULL);
	if (result < 0)
		goto out;
	if (extended) {
		if (read) {
			if (version2)
				result = ctrlReadBufferPositiveExt2(port, buffer->samples, count);
			else
				result = ctrlReadBufferPositiveExt(port, buffer->samples, count);
		} else if (version2) {
			result = ctrlPeekBufferPositiveExt2(port, buffer->samples, count);
		} else {
			result = ctrlPeekBufferPositiveExt(port, buffer->samples, count);
		}
	} else if (version2) {
		if (read) {
			if (negative)
				result = ctrlReadBufferNegative2(port, buffer->samples, count);
			else
				result = ctrlReadBufferPositive2(port, buffer->samples, count);
		} else if (negative) {
			result = ctrlPeekBufferNegative2(port, buffer->samples, count);
		} else {
			result = ctrlPeekBufferPositive2(port, buffer->samples, count);
		}
	} else if (read) {
		if (negative)
			result = SceCtrlForDriver_8D4E0DD1(port, (SceCtrlData *)buffer->samples, count);
		else
			result = SceCtrlForDriver_9B96A1AA(port, (SceCtrlData *)buffer->samples, count);
	} else if (negative) {
		result = SceCtrlForDriver_19895843(port, (SceCtrlData *)buffer->samples, count);
	} else {
		result = SceCtrlForDriver_EA1D3A34(port, (SceCtrlData *)buffer->samples, count);
	}
	if (result > 0) {
		copied = SceSysmemForDriver_6D88EF8A(samples, buffer->samples,
					    result * sizeof(buffer->samples[0]));
		if (copied < 0) {
			sceKernelUnlockMutex(buffer->mutex, 1);
			result = copied;
			goto out;
		}
	}
	unlocked = sceKernelUnlockMutex(buffer->mutex, 1);
	if (unlocked < 0)
		result = unlocked;
out:
	sceSyscallLeave(context);
	return result;
}

int sceCtrlPeekBufferPositive(int port, SceCtrlData *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 0, 0, 0, 0);
}

int sceCtrlPeekBufferNegative(int port, SceCtrlData *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 0, 1, 0, 0);
}

int sceCtrlReadBufferPositive(int port, SceCtrlData *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 1, 0, 0, 0);
}

int sceCtrlReadBufferNegative(int port, SceCtrlData *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 1, 1, 0, 0);
}

int sceCtrlPeekBufferPositiveExt(int port, SceCtrlData *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 0, 0, 1, 0);
}

int sceCtrlReadBufferPositiveExt(int port, SceCtrlData *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 1, 0, 1, 0);
}

int sceCtrlPeekBufferPositive2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 0, 0, 0, 1);
}

int sceCtrlPeekBufferNegative2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 0, 1, 0, 1);
}

int sceCtrlReadBufferPositive2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 1, 0, 0, 1);
}

int sceCtrlReadBufferNegative2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 1, 1, 0, 1);
}

int sceCtrlPeekBufferPositiveExt2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 0, 0, 1, 1);
}

int sceCtrlReadBufferPositiveExt2(int port, SceCtrlData2 *samples, int count)
{
	return ctrlUserBuffer(port, samples, count, 1, 0, 1, 1);
}

/* Name unknown: Sony inlines this normalized distance for all four axes. */
static inline __attribute__((always_inline)) int ctrlAxisDistance(uint8_t axis)
{
	int distance = (int)axis - 0x80;

	if (distance < 0)
		distance = -distance;
	return distance == 0x7f ? 0x80 : distance;
}

int ctrlSampleComplete(SceSysconPacket *packet, void *arg)
{
	struct ctrlNativeSample samples[5];
	struct ctrlNativeSample *sample;
	uint32_t previous[5], buttons, changed, events;
	uint32_t command, first, second, third, fourth;
	SceKernelIntrStatus intr;
	int port, source, threshold, key;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	for (port = 0; port < 5; port++) {
		sample = &samples[port];
		memset(sample, 0, sizeof(*sample));
		sample->sample.lx = 0x80;
		sample->sample.ly = 0x80;
		sample->sample.rx = 0x80;
		sample->sample.ry = 0x80;
		sample->sample.wideLx = 0x80;
		sample->sample.wideLy = 0x80;
		sample->sample.wideRx = 0x80;
		sample->sample.wideRy = 0x80;
		sample->extended[0] = 0x80;
		sample->extended[1] = 0x80;
		sample->extended[2] = 0x80;
		sample->extended[3] = 0x80;
	}
	ctrlStorage.samplePending = 0;
	if (!ctrlStorage.systemEventState) {
		ctrlPublishCachedSample();
		goto out;
	}
	previous[0] = ctrlStorage.cachedButtons[0];
	sample = &samples[0];
	command = packet->tx[0] | packet->tx[1] << 8;
	if (command == 0x104 || command - 0x100U <= 1) {
		first = packet->rx[4];
		second = packet->rx[5];
		third = packet->rx[6];
		fourth = packet->rx[7];
		buttons = (second & 0x40) << 24 | (fourth & 2) << 28 |
			  (second & 9) | (third & 3) << 20 |
			  (fourth & 0xc) << 16 | (second & 0x30) << 12 |
			  (first & 0xf0) << 8 | (second & 6) << 9 |
			  (uint8_t)(first << 4);
		sample->sample.buttons = buttons ^ 0x4037fcf9;
		if (command == 0x101 || command == 0x104) {
			sample->sample.lx = packet->rx[8];
			sample->sample.ly = packet->rx[9];
			sample->sample.rx = packet->rx[10];
			sample->sample.ry = packet->rx[11];
			if (command == 0x104) {
				sample->sample.wideLx = packet->rx[12];
				sample->sample.wideLy = packet->rx[13];
				sample->sample.wideRx = packet->rx[14];
				sample->sample.wideRy = packet->rx[15];
			} else {
				sample->sample.wideLx = sample->sample.lx;
				sample->sample.wideLy = sample->sample.ly;
				sample->sample.wideRx = sample->sample.rx;
				sample->sample.wideRy = sample->sample.ry;
			}
			sample->extended[0] = sample->sample.wideLx;
			sample->extended[1] = sample->sample.wideLy;
			sample->extended[2] = sample->sample.wideRx;
			sample->extended[3] = sample->sample.wideRy;
		}
	} else {
		sample->sample.buttons = ctrlStorage.cachedButtons[0];
		sample->sample.lx = ctrlStorage.cachedLx;
		sample->sample.ly = ctrlStorage.cachedLy;
		sample->sample.rx = ctrlStorage.cachedRx;
		sample->sample.ry = ctrlStorage.cachedRy;
		sample->sample.wideLx = ctrlStorage.cachedUp;
		sample->sample.wideLy = ctrlStorage.cachedRight;
		sample->sample.wideRx = ctrlStorage.cachedDown;
		sample->sample.wideRy = ctrlStorage.cachedLeft;
	}
	ctrlStorage.cachedButtons[0] = sample->sample.buttons;
	ctrlStorage.cachedLx = sample->sample.lx;
	ctrlStorage.cachedLy = sample->sample.ly;
	ctrlStorage.cachedRx = sample->sample.rx;
	ctrlStorage.cachedRy = sample->sample.ry;
	ctrlStorage.cachedUp = sample->sample.wideLx;
	ctrlStorage.cachedRight = sample->sample.wideLy;
	ctrlStorage.cachedDown = sample->sample.wideRx;
	ctrlStorage.cachedLeft = sample->sample.wideRy;
	source = SceSysrootForDriver_8AA268D6(1) ? 1 : 0;
	for (port = source ? 4 : 0; port >= 0; port--) {
		if (port)
			previous[port] = ctrlStorage.cachedButtons[port];
		sample = &samples[port];
		ctrlPublishSample(port, sample, source);
		if (ctrlStorage.powerTickPending)
			continue;
		buttons = ctrlStorage.cachedButtons[port];
		changed = buttons ^ previous[port];
		key = buttons & 0x20000 ? 2 : 0;
		threshold = ctrlStorage.idleCancelThresholds[key / 2];
		events = (buttons & ctrlStorage.idleCancelKeys[key + 1]) |
			 (changed & ctrlStorage.idleCancelKeys[key]);
		if ((events & ctrlStorage.idleCancelMask) ||
		    threshold < ctrlAxisDistance(sample->sample.lx) ||
		    threshold < ctrlAxisDistance(sample->sample.ly) ||
		    threshold < ctrlAxisDistance(sample->sample.rx) ||
		    threshold < ctrlAxisDistance(sample->sample.ry))
			ctrlStorage.powerTickPending = 1;
	}
	for (port = 0;
	     port < (SceSysrootForDriver_8AA268D6(1) ? 5 : 1);
	     port++) {
		events = ctrlStorage.ports[port].active & ~ctrlStorage.ports[port].blocked;
		sceKernelSetEventFlag(ctrlStorage.ports[port].positive, events);
		sceKernelSetEventFlag(ctrlStorage.ports[port].negative, events);
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

static const uint32_t ctrlButtonMasks[14] = {
	0x400, 0x800, 0x100, 0x200, 0x2, 0x4, 0x10,
	0x20, 0x40, 0x80, 0x1000, 0x2000, 0x4000, 0x8000,
};

struct ctrlStick {
	uint8_t x;
	uint8_t y;
};

struct ctrlSampleAxes {
	struct ctrlStick left;
	struct ctrlStick right;
	struct ctrlStick pressureLeft;
	struct ctrlStick pressureRight;
};

/* Names unknown: native publication inlines remapping and axis processing. */
static inline __attribute__((always_inline)) void ctrlRemapSample(
		struct ctrlNativeSample *sample)
{
	uint32_t original, buttons, source, target;
	uint8_t triangle = 0, circle = 0, temporary;
	int i;

	if (!ctrlStorage.buttonRemapEnabled || ctrlStorage.safeMode)
		return;
	original = sample->sample.buttons;
	buttons = original & 0xffff0009;
	for (i = 0; i < 14; i++) {
		source = ctrlButtonMasks[i];
		if (!(original & source))
			continue;
		target = ctrlButtonMasks[ctrlStorage.remapping.buttons[i]];
		buttons |= target;
		if (target == 0x100) {
			if (source == 0x100)
				triangle = sample->sample.l2;
			else if (source == 0x200)
				triangle = sample->sample.r2;
			else
				triangle = 0xff;
		} else if (target == 0x200) {
			if (source == 0x200)
				circle = sample->sample.r2;
			else if (source == 0x100)
				circle = sample->sample.l2;
			else
				circle = 0xff;
		}
	}
	sample->sample.buttons = buttons;
	sample->sample.l2 = triangle;
	sample->sample.r2 = circle;
	if (!ctrlStorage.remapping.swapAnalogSticks)
		return;
	temporary = sample->sample.lx;
	sample->sample.lx = sample->sample.rx;
	sample->sample.rx = temporary;
	temporary = sample->sample.ly;
	sample->sample.ly = sample->sample.ry;
	sample->sample.ry = temporary;
	temporary = sample->sample.wideLx;
	sample->sample.wideLx = sample->sample.wideRx;
	sample->sample.wideRx = temporary;
	temporary = sample->sample.wideLy;
	sample->sample.wideLy = sample->sample.wideRy;
	sample->sample.wideRy = temporary;
	temporary = sample->extended[0];
	sample->extended[0] = sample->extended[2];
	sample->extended[2] = temporary;
	temporary = sample->extended[1];
	sample->extended[1] = sample->extended[3];
	sample->extended[3] = temporary;
}

static inline __attribute__((always_inline)) uint8_t ctrlSaturateAxis(int value)
{
	if (value < 0)
		return 0;
	return value > 255 ? 255 : value;
}

static inline __attribute__((always_inline)) void ctrlCheckStick(
		struct ctrlStick *stick, unsigned int index, uint64_t now)
{
	struct ctrlAnalogStickCheck *check = &ctrlStorage.analogStickChecks[index];

	if (ctrlStorage.analogStickCheckMode) {
		stick->x = stick->x <= 0x80 ? 0x60 : 0xa0;
		stick->y = stick->y <= 0x80 ? 0x60 : 0xa0;
		return;
	}
	if (stick->x < 0x60 || stick->x > 0xa0 ||
	    stick->y < 0x60 || stick->y > 0xa0) {
		check->triggered = 0;
		check->timestamp = now;
		return;
	}
	if (!check->triggered || now - check->timestamp > 2000000) {
		check->triggered = check->triggered < 4 ? check->triggered + 1 : 1;
		check->timestamp = now;
	}
	switch (check->triggered) {
	case 1:
		stick->x = 0xa0;
		stick->y = 0x60;
		break;
	case 2:
		stick->x = 0xa0;
		stick->y = 0xa0;
		break;
	case 3:
		stick->x = 0x60;
		stick->y = 0xa0;
		break;
	case 4:
		stick->x = 0x60;
		stick->y = 0x60;
		break;
	}
}

static inline __attribute__((always_inline)) void ctrlStoreAxes(
		struct ctrlNativeSample *sample, const struct ctrlSampleAxes *axes)
{
	sample->sample.lx = axes->left.x;
	sample->sample.ly = axes->left.y;
	sample->sample.rx = axes->right.x;
	sample->sample.ry = axes->right.y;
	sample->sample.wideLx = axes->pressureLeft.x;
	sample->sample.wideLy = axes->pressureLeft.y;
	sample->sample.wideRx = axes->pressureRight.x;
	sample->sample.wideRy = axes->pressureRight.y;
}

static inline __attribute__((always_inline)) void ctrlNeutralAxes(
		struct ctrlNativeSample *sample)
{
	sample->sample.lx = 0x80;
	sample->sample.ly = 0x80;
	sample->sample.rx = 0x80;
	sample->sample.ry = 0x80;
	sample->sample.wideLx = 0x80;
	sample->sample.wideLy = 0x80;
	sample->sample.wideRx = 0x80;
	sample->sample.wideRy = 0x80;
	sample->extended[0] = 0x80;
	sample->extended[1] = 0x80;
	sample->extended[2] = 0x80;
	sample->extended[3] = 0x80;
}

void ctrlPublishSample(int port, struct ctrlNativeSample *sample, int source)
{
	struct ctrlSampleAxes baseline, checked;
	struct ctrlEmulation *emulation;
	struct ctrlProcessStorage *process;
	struct ctrlSampleRing *ring;
	struct ctrlNativeSample *out;
	struct ctrlRapidFireEntry *rapid;
	const SceCtrlVirtualControllerDriver *driver = ctrlStorage.virtualDriver;
	SceUID active;
	uint64_t now;
	uint32_t userButtons, kernelButtons, userEmulation = 0, kernelEmulation = 0;
	uint32_t buttons, previous, changed, rapidButtons, phase, remaining;
	int leftX = 0, leftY = 0, rightX = 0, rightY = 0;
	int pressureLeftX = 0, pressureLeftY = 0, pressureRightX = 0, pressureRightY = 0;
	int i, index, intercept = 0;

	if (!driver) {
		ctrlRemapSample(sample);
	} else if (!port) {
		sample->sample.lx = ctrlStorage.accumulatedAxes[0].analog;
		sample->sample.ly = ctrlStorage.accumulatedAxes[1].analog;
		sample->sample.rx = ctrlStorage.accumulatedAxes[2].analog;
		sample->sample.ry = ctrlStorage.accumulatedAxes[3].analog;
		sample->sample.wideLx = ctrlStorage.accumulatedAxes[0].pressure;
		sample->sample.wideLy = ctrlStorage.accumulatedAxes[1].pressure;
		sample->sample.wideRx = ctrlStorage.accumulatedAxes[2].pressure;
		sample->sample.wideRy = ctrlStorage.accumulatedAxes[3].pressure;
		for (i = 0; i < 4; i++) {
			sample->extended[i] = ctrlStorage.accumulatedAxes[i].extended;
			ctrlStorage.accumulatedAxes[i].analog = 0x80;
			ctrlStorage.accumulatedAxes[i].pressure = 0x80;
			ctrlStorage.accumulatedAxes[i].extended = 0x80;
		}
		sample->sample.buttons = ctrlStorage.accumulatedButtons |
			ctrlStorage.bdRemoconButtons | (sample->sample.buttons & 0x40000000);
		ctrlStorage.accumulatedButtons = 0;
	} else {
		if (driver->readButtons(port, sample) < 1) {
			sample->sample.buttons = 0;
			ctrlNeutralAxes(sample);
		} else if (port == 1) {
			ctrlRemapSample(sample);
			ctrlStorage.accumulatedAxes[0].analog = sample->sample.lx;
			ctrlStorage.accumulatedAxes[1].analog = sample->sample.ly;
			ctrlStorage.accumulatedAxes[2].analog = sample->sample.rx;
			ctrlStorage.accumulatedAxes[3].analog = sample->sample.ry;
			ctrlStorage.accumulatedAxes[0].pressure = sample->sample.wideLx;
			ctrlStorage.accumulatedAxes[1].pressure = sample->sample.wideLy;
			ctrlStorage.accumulatedAxes[2].pressure = sample->sample.wideRx;
			ctrlStorage.accumulatedAxes[3].pressure = sample->sample.wideRy;
			for (i = 0; i < 4; i++)
				ctrlStorage.accumulatedAxes[i].extended = sample->extended[i];
			ctrlStorage.accumulatedButtons |= sample->sample.buttons;
		} else {
			ctrlStorage.accumulatedButtons |= sample->sample.buttons & 0x10000;
		}
	}
	ctrlStorage.cachedButtons[port] = sample->sample.buttons;
	if (ctrlStorage.ctrlpCallback)
		ctrlStorage.ctrlpCallback(port, sample, ctrlStorage.ctrlpMode);
	now = sceKernelGetSystemTimeWide();
	for (i = 0; i < 4; i++) {
		emulation = &ctrlStorage.emulation[port][i];
		if ((int32_t)emulation->analogDuration <= 0)
			continue;
		emulation->analogDuration--;
		leftX += emulation->userLx - 0x80;
		leftY += emulation->userLy - 0x80;
		rightX += emulation->userRx - 0x80;
		rightY += emulation->userRy - 0x80;
		pressureLeftX += emulation->kernelLx - 0x80;
		pressureLeftY += emulation->kernelLy - 0x80;
		pressureRightX += emulation->kernelRx - 0x80;
		pressureRightY += emulation->kernelRy - 0x80;
	}
	baseline.left.x = ctrlSaturateAxis(leftX + sample->sample.lx);
	baseline.left.y = ctrlSaturateAxis(leftY + sample->sample.ly);
	baseline.right.x = ctrlSaturateAxis(rightX + sample->sample.rx);
	baseline.right.y = ctrlSaturateAxis(rightY + sample->sample.ry);
	baseline.pressureLeft.x = ctrlSaturateAxis(pressureLeftX + sample->sample.wideLx);
	baseline.pressureLeft.y = ctrlSaturateAxis(pressureLeftY + sample->sample.wideLy);
	baseline.pressureRight.x = ctrlSaturateAxis(pressureRightX + sample->sample.wideRx);
	baseline.pressureRight.y = ctrlSaturateAxis(pressureRightY + sample->sample.wideRy);
	checked = baseline;
	if (!port && !source && ctrlStorage.analogStickCheckTarget) {
		if (ctrlStorage.analogStickCheckTarget != 2) {
			ctrlCheckStick(&checked.right, 0, now);
			ctrlCheckStick(&checked.pressureRight, 2, now);
		}
		if (ctrlStorage.analogStickCheckTarget != 1) {
			ctrlCheckStick(&checked.left, 1, now);
			ctrlCheckStick(&checked.pressureLeft, 3, now);
		}
	}
	kernelButtons = sample->sample.buttons & ctrlStorage.maskForAll;
	if (kernelButtons & 0x20000)
		kernelButtons &= 0x700e0000;
	for (i = 0; i < 4; i++) {
		emulation = &ctrlStorage.emulation[port][i];
		if ((int32_t)emulation->buttonDuration <= 0)
			continue;
		emulation->buttonDuration--;
		userEmulation |= emulation->userButtons;
		kernelEmulation |= emulation->kernelButtons;
		if (!emulation->buttonDuration) {
			emulation->userButtons = 0;
			emulation->kernelButtons = 0;
		}
	}
	if (!port) {
		buttons = kernelEmulation | sample->sample.buttons;
		previous = ctrlStorage.notificationButtons;
		changed = buttons ^ previous;
		ctrlStorage.notificationButtons = buttons;
		for (i = 0; i < 4; i++) {
			struct ctrlButtonChangeCallback *callback =
				&ctrlStorage.buttonChangeCallbacks[i];

			if ((changed & callback->mask) && callback->callback)
				callback->callback(buttons, previous, callback->arg);
		}
	}
	sample->sample.buttons &= ~0x10000U;
	userButtons = sample->sample.buttons & ctrlStorage.maskForNonShell &
		      ctrlStorage.maskForAll;
	if (userButtons & 0x20000)
		userButtons &= 0x30000;
	userButtons |= userEmulation;
	if (!port) {
		if (!userButtons && !ctrlStorage.otherPortButtons) {
			process = NULL;
			if (ctrlStorage.activeProcess != -1 &&
			    SceProcessmgrForDriver_AF80F39C(ctrlStorage.activeProcess,
				ctrlStorage.processStorageKey, (void **)&process, 0) >= 0 &&
			    process && process->buttonInterceptPending) {
				process->buttonInterceptPending = 0;
				process->buttonIntercept = process->pendingButtonIntercept;
				if (process->pendingButtonIntercept == 1)
					_sceCtrlBdRMCForceReleaseCallback(ctrlStorage.activeProcess);
			}
			if (ctrlStorage.activeProcessChangePending) {
				ctrlStorage.activeProcess = ctrlStorage.pendingActiveProcess;
				ctrlStorage.activeProcessChangePending = 0;
			}
		}
		ctrlStorage.otherPortButtons = 0;
	} else if (userButtons) {
		ctrlStorage.otherPortButtons |= userButtons;
	}
	active = ctrlStorage.activeProcess;
	process = NULL;
	if (active == -1 || SceProcessmgrForDriver_AF80F39C(active,
		ctrlStorage.processStorageKey, (void **)&process, 0) < 0 ||
	    !process || process->processState == 2) {
		active = -1;
	} else {
		intercept = process->buttonIntercept;
	}
	if (intercept != 1) {
		rapidButtons = userButtons;
		for (i = 15; i >= 0; i--) {
			rapid = &ctrlStorage.rapidFire[port][i];
			if (rapid->owner == -1)
				continue;
			process = NULL;
			if (SceProcessmgrForDriver_AF80F39C(rapid->owner,
				ctrlStorage.processStorageKey, (void **)&process, 0) < 0 ||
			    !process || ((rapid->owner != ctrlStorage.activeProcess ||
			    process->processState == 2) && rapid->owner != SCE_KERNEL_PROCESS_ID) ||
			    !rapid->mask)
				continue;
			if ((rapid->mask & userButtons) != rapid->trigger) {
				rapid->counter = 0;
				continue;
			}
			phase = rapid->counter >> 6;
			remaining = rapid->counter & 0x3f;
			if (remaining)
				remaining--;
			if (!remaining && !phase) {
				remaining = rapid->delay;
				phase = 3;
				if (!remaining) {
					remaining = rapid->make;
					phase = 2;
				}
			} else {
				if (phase == 2)
					rapidButtons |= rapid->target;
				else
					rapidButtons &= ~rapid->target;
				if (!remaining) {
					phase ^= 1;
					remaining = phase & 1 ? rapid->breakCount : rapid->make;
				}
			}
			rapid->counter = remaining | phase << 6;
		}
		userButtons = rapidButtons & ctrlStorage.maskForNonShell & ctrlStorage.maskForAll;
		if (userButtons & 0x20000)
			userButtons &= 0x30000;
	}
	for (i = 0; i < 16; i++) {
		if (ctrlStorage.processes[i] == -1)
			continue;
		process = NULL;
		if (SceProcessmgrForDriver_AF80F39C(ctrlStorage.processes[i],
			ctrlStorage.processStorageKey, (void **)&process, 0) < 0 ||
		    !process || process->processState == 2)
			continue;
		ring = process->ports[port].ring;
		out = &ring->samples[ring->writeIndex];
		out->sample.up = sample->sample.up;
		out->sample.right = sample->sample.right;
		out->sample.down = sample->sample.down;
		out->sample.left = sample->sample.left;
		out->sample.l2 = sample->sample.l2;
		out->sample.r2 = sample->sample.r2;
		out->sample.l1 = sample->sample.l1;
		out->sample.r1 = sample->sample.r1;
		for (index = 0; index < 4; index++) {
			out->sample.pressure[8 + index] = sample->sample.pressure[8 + index];
			out->extended[index] = sample->extended[index];
		}
		if (process->privileged) {
			ctrlStoreAxes(out, &baseline);
			buttons = kernelButtons | kernelEmulation;
		} else {
			ctrlStoreAxes(out, &checked);
			if (ctrlStorage.processes[i] == active) {
				buttons = userButtons;
				if (ctrlStorage.activeProcessChangePending || (process->buttonInterceptPending &&
				    process->pendingButtonIntercept == 1) ||
				    (process->processState & ~2U) == 1)
					buttons |= 0x02000000;
				if (intercept == 1)
					buttons |= 0x80000000;
				if (ctrlStorage.ctrlpProcess == (uint32_t)active && ctrlStorage.ctrlpMode == 1) {
					buttons &= 0x20000;
					ctrlNeutralAxes(out);
				}
			} else {
				buttons = userButtons | 0x10000;
				if (ctrlStorage.activeProcessChangePending && ctrlStorage.processes[i] ==
				    (SceUID)ctrlStorage.pendingActiveProcess) {
					buttons = userButtons & 0x30000;
					if ((process->processState & ~2U) == 1)
						buttons |= 0x10000;
				}
				if (ctrlStorage.foregroundSelected && !process->activation) {
					buttons = 0;
					if (process->systemProgram)
						ctrlNeutralAxes(out);
				}
			}
		}
		out->sample.buttons = buttons;
		out->sample.timeStamp = now;
		SceProcessmgrForDriver_8CB01675(ctrlStorage.processes[i],
					     &out->sample.timeStamp, now);
		out->reserved[0] = 0;
		out->reserved[1] = 0;
		out->reserved[2] = 0;
		out->reserved[3] = 0;
		ring = process->ports[port].ring;
		index = ((int32_t)ring->writeIndex + 1) % 64;
		ring->writeIndex = index;
		if (index == (int32_t)ring->readIndex)
			ring->readIndex = (index + 1) % 64;
		if (index == (int32_t)ring->readIndexExt)
			ring->readIndexExt = (index + 1) % 64;
	}
}

int SceCtrlForDriver_B26C2231(SceUID pid)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.foregroundSelected = 1;
	if (ctrlStorage.activeProcess != pid) {
		ctrlStorage.activeProcessChangePending = 1;
		ctrlStorage.pendingActiveProcess = pid;
		if (SceProcessmgrForDriver_AF80F39C(pid,
		    ctrlStorage.processStorageKey, (void **)&process, 0) >= 0 &&
		    process && !process->activation)
			process->activation = 1;
	} else if (ctrlStorage.activeProcessChangePending) {
		ctrlStorage.activeProcessChangePending = 0;
	}
	if (ctrlStorage.virtualDriver)
		ctrlStorage.virtualDriver->foregroundChanged(pid);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

/* Names unknown: kernel side of the corresponding user APIs. */
int ctrlIsMultiControllerSupported(void)
{
	return SceSysrootForDriver_8AA268D6(1);
}

int ctrlGetWirelessControllerInfo(SceCtrlWirelessControllerInfo *info)
{
	SceCtrlPortInfo ports;
	unsigned int index;
	int result = 0;

	if (!info)
		return SCE_CTRL_ERROR_INVALID_ARG;
	memset(info, 0, sizeof(*info));
	for (index = 1; index < 5; index++) {
		info->connected[index - 1] = 0;
		if (ctrlStorage.virtualDriver) {
			result = ctrlStorage.virtualDriver->getControllerPortInfo(&ports);
			if (ports.port[index] == 4 || ports.port[index] == 8)
				info->connected[index - 1] = 1;
		}
	}
	return result;
}

int ctrlSetButtonRemappingInfo(const struct ctrlButtonRemappingInfo *info)
{
	SceKernelIntrStatus intr;
	uint32_t used = 0, button;
	unsigned int index;
	int result = 0;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (!info) {
		ctrlStorage.buttonRemapEnabled = 0;
		goto out;
	}
	for (index = 0; index < 14; index++) {
		button = ctrlButtonMasks[info->buttons[index]];
		if (used & button) {
			result = SCE_CTRL_ERROR_INVALID_ARG;
			goto out;
		}
		used |= button;
	}
	__builtin_memcpy(&ctrlStorage.remapping, info, sizeof(*info));
	ctrlStorage.buttonRemapEnabled = 1;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

/* Names unknown: forwarding entries used by the corresponding user APIs. */
int ctrlSetActuator(int port, const SceCtrlActuator *state)
{
	if (!ctrlStorage.vibrationEnabled)
		return 0;
	if (!ctrlStorage.virtualDriver)
		return SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->setActuator(port, state);
}

int ctrlSetLightBar(int port, uint8_t red, uint8_t green, uint8_t blue)
{
	if (!ctrlStorage.virtualDriver)
		return SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->setLightBar(port, red, green, blue);
}

int ctrlResetLightBar(int port)
{
	if (!ctrlStorage.virtualDriver)
		return SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->resetLightBar(port);
}

int ctrlSingleControllerMode(int port)
{
	if (!ctrlStorage.virtualDriver)
		return SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->singleControllerMode(port);
}

int ctrlGetBatteryInfo(int port, uint8_t *battery)
{
	if (!ctrlStorage.virtualDriver)
		return SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->getBatteryInfo(port, battery);
}

int ctrlDisconnect(int port)
{
	if (!ctrlStorage.virtualDriver)
		return SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->disconnect(port);
}

int SceCtrlForDriver_69E45836(void)
{
	if (!ctrlStorage.virtualDriver)
		return SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->displayOff();
}

int SceCtrlForDriver_E170883E(SceUID callback)
{
	SceKernelIntrStatus intr;
	int result, index;

	result = SceThreadmgrForDriver_0892D8DF(callback);
	if (result < 0)
		return result;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	for (index = 0; index < 16; index++) {
		if (ctrlStorage.bdRemoconCallbacks[index] == -1) {
			ctrlStorage.bdRemoconCallbacks[index] = callback;
			ctrlStorage.bdRemoconCallbackOwners[index] = sceKernelGetProcessId();
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_9DAB51C4(SceUID callback)
{
	SceKernelIntrStatus intr;
	int result, index;

	result = SceThreadmgrForDriver_0892D8DF(callback);
	if (result < 0)
		return result;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	for (index = 0; index < 16; index++) {
		if (ctrlStorage.bdRemoconCallbacks[index] == callback) {
			ctrlStorage.bdRemoconCallbacks[index] = -1;
			ctrlStorage.bdRemoconCallbackOwners[index] = -1;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

/* Names unknown: kernel side of the corresponding user callback APIs. */
int ctrlRegisterBdRMCCallback(SceUID callback)
{
	SceKernelIntrStatus intr;
	int result, index;

	result = SceThreadmgrForDriver_0892D8DF(callback);
	if (result < 0)
		return SCE_CTRL_ERROR_FATAL;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	for (index = 0; index < 16; index++) {
		if (ctrlStorage.remoconReleaseCallbacks[index] == -1) {
			ctrlStorage.remoconReleaseCallbacks[index] = callback;
			ctrlStorage.remoconReleaseOwners[index] = sceKernelGetProcessId();
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return index == 16 ? (int)SCE_CTRL_ERROR_FATAL : result;
}

int ctrlUnregisterBdRMCCallback(SceUID callback)
{
	SceKernelIntrStatus intr;
	int result, index;

	result = SceThreadmgrForDriver_0892D8DF(callback);
	if (result < 0)
		return SCE_CTRL_ERROR_FATAL;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	for (index = 0; index < 16; index++) {
		if (ctrlStorage.remoconReleaseCallbacks[index] == callback) {
			ctrlStorage.remoconReleaseCallbacks[index] = -1;
			ctrlStorage.remoconReleaseOwners[index] = -1;
			ctrlStorage.remoconReleaseStates[index] = 0xff;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return index == 16 ? (int)SCE_CTRL_ERROR_FATAL : result;
}

int ctrlGetProcessStatus(SceUID pid, int *status)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	int result = SCE_CTRL_ERROR_PRIV_REQUIRED;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
		ctrlStorage.processStorageKey, (void **)&process, 0) >= 0 && process) {
		*status = process->buttonIntercept == 1;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int SceCtrlForDriver_DA07E970(SceUID *pid)
{
	if (!pid)
		return SCE_CTRL_ERROR_INVALID_ARG;
	*pid = ctrlStorage.activeProcess;
	return 0;
}

int SceCtrlForDriver_5B708A15(SceUID pid, int mode)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.ctrlpProcess = mode ? pid : -1;
	ctrlStorage.ctrlpMode = mode;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int sceCtrlSetCtrlpCallback(int (*callback)(SceUInt32 port, struct ctrlNativeSample *sample,
					    int mode))
{
	SceKernelIntrStatus intr;

	if (!callback)
		return SCE_CTRL_ERROR_INVALID_ARG;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.ctrlpCallback = callback;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_FF9D4DD4(uint32_t mask,
		void (*callback)(uint32_t, uint32_t, void *), void *arg,
		uint32_t *buttons)
{
	struct ctrlButtonChangeCallback *entry;
	SceKernelIntrStatus intr;
	int index;

	if (!callback)
		return SCE_CTRL_ERROR_INVALID_ARG;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	for (index = 0; index < 4; index++) {
		entry = &ctrlStorage.buttonChangeCallbacks[index];
		if (!entry->callback) {
			entry->mask = mask;
			entry->callback = callback;
			entry->arg = arg;
			break;
		}
	}
	if (index == 4)
		index = SCE_CTRL_ERROR_FATAL;
	if (buttons)
		*buttons = ctrlStorage.notificationButtons;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return index;
}

int SceCtrlForDriver_4CCB2153(unsigned int index)
{
	struct ctrlButtonChangeCallback *entry;

	if (index > 3)
		return SCE_CTRL_ERROR_INVALID_ARG;
	entry = &ctrlStorage.buttonChangeCallbacks[index];
	entry->mask = 0;
	entry->callback = NULL;
	entry->arg = NULL;
	return 0;
}

int SceCtrlForDriver_ED138A26(int port, int index)
{
	struct ctrlRapidFireEntry *entry;
	SceUID pid;

	if (port || (unsigned int)index > 15)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	entry = &ctrlStorage.rapidFire[0][index];
	if (entry->owner == pid) {
		entry->mask = 0;
		entry->owner = -1;
	}
	return 0;
}

int SceCtrlForDriver_A248CDAE(int port, int index, const SceCtrlRapidFireRule *rule)
{
	struct ctrlRapidFireEntry *entry;
	SceKernelIntrStatus intr;
	uint32_t mask, trigger, target, breakCount, delay;
	SceUID pid;

	if (port || (unsigned int)index > 15 || !rule)
		return SCE_CTRL_ERROR_INVALID_ARG;
	if ((rule->uiDelay | rule->uiMake | rule->uiBreak) - 1U > 0x3e)
		return SCE_CTRL_ERROR_INVALID_ARG;
	mask = rule->uiMask;
	trigger = rule->uiTrigger;
	target = rule->uiTarget;
	if (!SceSysrootForDriver_8AA268D6(1)) {
		if (mask & 0x100)
			mask = (mask & ~0x100) | 0x400;
		if (mask & 0x200)
			mask = (mask & ~0x200) | 0x800;
		if (trigger & 0x100)
			trigger = (trigger & ~0x100) | 0x400;
		if (trigger & 0x200)
			trigger = (trigger & ~0x200) | 0x800;
		if (target & 0x100)
			target = (target & ~0x100) | 0x400;
		if (target & 0x200)
			target = (target & ~0x200) | 0x800;
	}
	if (!SceIntrmgrForDriver_182EE3E3() && sceKernelIsUserModeThread() &&
	    ((mask | trigger | target) & (~ctrlStorage.maskForNonShell | 0x20000)))
		return SCE_CTRL_ERROR_PRIV_REQUIRED;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	pid = sceKernelGetProcessId();
	entry = &ctrlStorage.rapidFire[0][index];
	entry->make = rule->uiMake;
	breakCount = rule->uiBreak;
	entry->owner = pid;
	entry->mask = mask;
	entry->breakCount = breakCount;
	delay = rule->uiDelay;
	entry->trigger = trigger;
	entry->target = target;
	entry->counter = 0;
	entry->delay = delay;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_06AA5E1A(int intercept)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	SceUID pid;
	int result = SCE_CTRL_ERROR_FATAL;

	if ((unsigned int)intercept > 1)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
		ctrlStorage.processStorageKey, (void **)&process, 0) < 0 || !process)
		goto out;
	if (process->buttonIntercept != intercept) {
		if (ctrlStorage.activeProcess == pid) {
			process->pendingButtonIntercept = intercept;
			process->buttonInterceptPending = 1;
		} else {
			process->buttonIntercept = intercept;
		}
	} else if (process->buttonInterceptPending) {
		process->buttonInterceptPending = 0;
	}
	result = 0;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int SceCtrlForDriver_6BEB1CD7(int *intercept)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	SceUID pid;
	int result = SCE_CTRL_ERROR_FATAL;

	if (!intercept)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
		ctrlStorage.processStorageKey, (void **)&process, 0) >= 0 && process) {
		*intercept = process->buttonIntercept == 1;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int SceCtrlForDriver_C8CE3E43(int enabled)
{
	ctrlStorage.vblankSampling = enabled;
	return 0;
}

int SceCtrlForDriver_1E750326(unsigned int port, unsigned char slot,
			       unsigned int userButtons, unsigned int kernelButtons,
			       unsigned int duration)
{
	SceKernelIntrStatus intr;
	struct ctrlEmulation *emulation;

	if (port || slot > 3)
		return SCE_CTRL_ERROR_INVALID_ARG;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	emulation = &ctrlStorage.emulation[0][slot];
	emulation->userButtons = userButtons;
	emulation->kernelButtons = kernelButtons;
	emulation->buttonDuration = duration;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_06577FE8(unsigned int port, unsigned char slot,
			      unsigned char userLx, unsigned char userLy,
			      unsigned char userRx, unsigned char userRy,
			      unsigned char kernelLx, unsigned char kernelLy,
			      unsigned char kernelRx, unsigned char kernelRy,
			      unsigned int duration)
{
	SceKernelIntrStatus intr;
	struct ctrlEmulation *emulation;

	if (port || slot > 3)
		return SCE_CTRL_ERROR_INVALID_ARG;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	emulation = &ctrlStorage.emulation[0][slot];
	emulation->userLx = userLx;
	emulation->userLy = userLy;
	emulation->userRx = userRx;
	emulation->userRy = userRy;
	emulation->kernelLx = kernelLx;
	emulation->kernelLy = kernelLy;
	emulation->kernelRx = kernelRx;
	emulation->kernelRy = kernelRy;
	emulation->analogDuration = duration;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_0A679512(uint32_t key0, uint32_t key1,
			   uint32_t key2, uint32_t key3)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.idleCancelKeys[0] = key0;
	ctrlStorage.idleCancelKeys[1] = key1;
	ctrlStorage.idleCancelKeys[2] = key2;
	ctrlStorage.idleCancelKeys[3] = key3;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_30801AE9(uint32_t *key0, uint32_t *key1,
			   uint32_t *key2, uint32_t *key3)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (key0)
		*key0 = ctrlStorage.idleCancelKeys[0];
	if (key1)
		*key1 = ctrlStorage.idleCancelKeys[1];
	if (key2)
		*key2 = ctrlStorage.idleCancelKeys[2];
	if (key3)
		*key3 = ctrlStorage.idleCancelKeys[3];
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_345E74FD(int threshold0, int threshold1)
{
	SceKernelIntrStatus intr;

	if (threshold0 < -1 || threshold0 > 128 ||
	    threshold1 < -1 || threshold1 > 128)
		return SCE_CTRL_ERROR_INVALID_ARG;
	if (threshold0 == -1)
		threshold0 = 0x81;
	if (threshold1 == -1)
		threshold1 = 0x81;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.idleCancelThresholds[0] = threshold0;
	ctrlStorage.idleCancelThresholds[1] = threshold1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_B9402CEF(int *threshold0, int *threshold1)
{
	SceKernelIntrStatus intr;
	int value;

	if (!threshold0 || !threshold1)
		return SCE_CTRL_ERROR_INVALID_ARG;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	value = ctrlStorage.idleCancelThresholds[0];
	*threshold0 = value == 0x81 ? -1 : value;
	value = ctrlStorage.idleCancelThresholds[1];
	*threshold1 = value == 0x81 ? -1 : value;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

/* Name unknown: publishes the cached physical-controller state. */
int ctrlPublishCachedSample(void)
{
	struct ctrlNativeSample sample;
	uint32_t buttons = ctrlStorage.cachedButtons[0];
	unsigned int port;
	uint32_t events;

	if (!ctrlStorage.systemEventState)
		buttons &= ~ctrlStorage.maskForNonShell | 0x20000;
	sample.sample.buttons = buttons;
	sample.sample.lx = ctrlStorage.cachedLx;
	sample.sample.ly = ctrlStorage.cachedLy;
	sample.sample.rx = ctrlStorage.cachedRx;
	sample.sample.ry = ctrlStorage.cachedRy;
	sample.sample.wideLx = ctrlStorage.cachedUp;
	sample.sample.wideLy = ctrlStorage.cachedRight;
	sample.sample.wideRx = ctrlStorage.cachedDown;
	sample.sample.wideRy = ctrlStorage.cachedLeft;
	sample.extended[0] = sample.sample.wideLx;
	sample.extended[1] = sample.sample.wideLy;
	sample.extended[2] = sample.sample.wideRx;
	sample.extended[3] = sample.sample.wideRy;
	ctrlPublishSample(0, &sample, 0);
	for (port = 0;
	     port < (SceSysrootForDriver_8AA268D6(1) ? 5U : 1U);
	     port++) {
		events = ctrlStorage.ports[port].active &
			 ~ctrlStorage.ports[port].blocked;
		sceKernelSetEventFlag(ctrlStorage.ports[port].positive, events);
		sceKernelSetEventFlag(ctrlStorage.ports[port].negative, events);
	}
	return 0;
}

SceUInt32 ctrlAlarmCallback(void *arg)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlPublishCachedSample();
	ctrlStorage.samplingTimer = -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int ctrlSamplingModeComplete(SceSysconPacket *packet, void *arg)
{
	SceKernelIntrStatus intr;
	int result = -1;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.samplingModePending = 0;
	if ((packet->tx[0] | packet->tx[1] << 8) == 0x180) {
		ctrlStorage.samplingMode = packet->tx[3] ? 2 : 0;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int ctrlVblankCallback(void *arg, void *context, unsigned char priority)
{
	SceKernelIntrStatus intr;
	SceSysconPacket *sample = NULL;
	SceSysconPacket *mode = NULL;
	uint32_t requestedMode;
	uint8_t modeCommand;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (!ctrlStorage.samplePending && ctrlStorage.vblankSampling == 1) {
		sample = &ctrlStorage.samplePacket;
		sample->tx[0] = 0;
		if (ctrlStorage.samplingMode - 1U < 3)
			sample->tx[0] = ctrlStorage.baryonVersion < 0x90202 ? 1 : 4;
		sample->tx[1] = 1;
		sample->tx[2] = 1;
		ctrlStorage.samplePending = ctrlStorage.vblankSampling;
	} else if (ctrlStorage.samplingTimer == -1) {
		ctrlStorage.samplingTimer = SceThreadmgrForDriver_C58DF384("SceCtrlAlarm0",
					700, ctrlAlarmCallback, NULL);
	}
	if (!ctrlStorage.samplingModePending) {
		requestedMode = ctrlStorage.requestedSamplingMode &&
				!ctrlStorage.updateMode && !ctrlStorage.safeMode ? 2 : 0;
		if (ctrlStorage.samplingMode != requestedMode) {
			ctrlStorage.samplingModePending = 1;
			modeCommand = 0;
			if (requestedMode)
				modeCommand = ctrlStorage.baryonVersion < 0x90202 ? 1 : 3;
			mode = &ctrlStorage.samplingModePacket;
			mode->tx[0] = 0x80;
			mode->tx[1] = 1;
			mode->tx[2] = 2;
			mode->tx[3] = modeCommand;
		}
	}
	if (ctrlStorage.powerTickPending) {
		ctrlStorage.powerTickPending = 0;
		SceKernelSuspendForDriver_E0489831(0);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	if (sample && SceSysconForDriver_C2224E82(sample, 0, ctrlSampleComplete, NULL) < 0)
		ctrlStorage.samplePending = 0;
	if (mode && SceSysconForDriver_C2224E82(mode, 0, ctrlSamplingModeComplete, NULL) < 0)
		ctrlStorage.samplingModePending = 0;
	return -1;
}

/* Name unknown: handles sampling shutdown and resume notifications. */
int ctrlSystemEventCallback(int category, int event, void *args, void *opt)
{
	if (category == 1) {
		if (event == SYSEVENT_RESUME_DEVICE(12)) {
			ctrlStorage.systemEventState = 1;
			sceKernelEnableSubIntr(8, 3);
		}
		return 0;
	}
	if (category != 0)
		return 0;
	switch (event) {
	case SYSEVENT_SUSPEND_DEVICE(12):
		sceKernelDisableSubIntr(8, 3);
		ctrlStorage.samplingMode = 0;
		if (ctrlStorage.samplingTimer != -1) {
			sceKernelCancelAlarm(ctrlStorage.samplingTimer);
			ctrlStorage.samplingTimer = -1;
		}
		break;
	case SYSEVENT_SUSPEND_IDLE_ENTER:
		ctrlStorage.systemEventState = 0;
		break;
	case SYSEVENT_SUSPEND_IDLE_QUERY:
		if (ctrlStorage.systemEventState || ctrlStorage.pendingState)
			return -1;
		break;
	}
	return 0;
}

int ctrlCreateProcess(SceUID pid)
{
	struct ctrlProcessStorage *process = NULL;
	struct ctrlSampleRing *ring;
	SceSelfAuthInfo auth;
	SceKernelIntrStatus intr;
	uint32_t eventBit;
	unsigned int port, bit, slot;
	int result = SCE_CTRL_ERROR_FATAL;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
		ctrlStorage.processStorageKey, (void **)&process, 1) < 0 || !process)
		goto unlock;
	process->processId = pid;
	process->processState = 0;
	process->activation = 0;
	SceProcessmgrForDriver_D141C076(pid, &process->sdkVersion);
	process->mode3Blocked = sceSblACMgrIsGameProgram(pid);
	for (port = 0;
	     port < (SceSysrootForDriver_8AA268D6(1) ? 5U : 1U);
	     port++) {
		eventBit = 0;
		for (bit = 0; bit < 16; bit++) {
			if (!(ctrlStorage.ports[port].active & (1U << bit))) {
				eventBit = 1U << bit;
				ctrlStorage.ports[port].active |= eventBit;
				break;
			}
		}
		process->ports[port].eventBit = eventBit;
		ctrlStorage.ports[port].blocked &= ~eventBit;
		ring = SceSysmemForDriver_7B4CB60A(ctrlStorage.sampleHeap, sizeof(*ring));
		process->ports[port].ring = ring;
		ring->writeIndex = 0;
		ring->readIndex = 0;
		ring->readIndexExt = 0;
	}
	process->privileged = SceSblACMgrForDriver_991FDC15(pid) ||
		sceSblACMgrIsKernelProgram(pid) || SceSblACMgrForDriver_2E992B02(pid);
	sceKernelGetProcessSelfAuthInfo(pid, &auth);
	process->specialAuthority = auth.programAuthorityId == 0x2800000000000013ULL ||
		(uint64_t)(auth.programAuthorityId - 0x2800000000007009ULL) < 2 ||
		(uint64_t)(auth.programAuthorityId - 0x220000101cd2000cULL) < 2;
	process->systemProgram = sceSblACMgrIsSystemProgram(pid) != 0;
	process->samplingMode = 0;
	process->samplingModeExt = 0;
	process->buttonInterceptPending = 0;
	process->buttonIntercept = 0;
	process->pendingButtonIntercept = 0;
	for (port = 0;
	     port < (SceSysrootForDriver_8AA268D6(1) ? 5U : 1U);
	     port++) {
		sceKernelClearEventFlag(ctrlStorage.ports[port].positive,
					 ~process->ports[port].eventBit);
		sceKernelClearEventFlag(ctrlStorage.ports[port].negative,
					 ~process->ports[port].eventBit);
	}
	for (slot = 0; slot < 16; slot++) {
		if (ctrlStorage.processes[slot] == -1) {
			ctrlStorage.processes[slot] = pid;
			ctrlStorage.processCount++;
			result = 0;
			break;
		}
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int ctrlDestroyProcess(SceUID pid)
{
	struct ctrlProcessStorage *process = NULL;
	struct ctrlRapidFireEntry *entry;
	SceKernelIntrStatus intr;
	unsigned int port, slot;
	int capability;
	int result = SCE_CTRL_ERROR_FATAL;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (ctrlStorage.activeProcess == pid)
		ctrlStorage.activeProcess = -1;
	if (SceProcessmgrForDriver_AF80F39C(pid,
		ctrlStorage.processStorageKey, (void **)&process, 0) < 0 || !process)
		goto unlock;
	for (port = 0; ; port++) {
		capability = SceSysrootForDriver_8AA268D6(1);
		if (port >= (capability ? 5U : 1U))
			break;
		sceKernelFreeHeapMemory(ctrlStorage.sampleHeap,
			process->ports[port].ring);
	}
	if (pid != -1) {
		for (port = 0; port < 5; port++) {
			for (slot = 0; slot < 16; slot++) {
				entry = &ctrlStorage.rapidFire[port][slot];
				if (entry->owner != pid)
					continue;
				entry->owner = -1;
				entry->mask = 0;
				entry->trigger = 0;
				entry->target = 0;
				entry->make = 0;
				entry->breakCount = 0;
				entry->delay = 0;
				entry->counter = 0;
			}
		}
		capability = SceSysrootForDriver_8AA268D6(1);
	}
	for (port = 0; port < (capability ? 5U : 1U); port++) {
		sceKernelClearEventFlag(ctrlStorage.ports[port].positive,
					 ~process->ports[port].eventBit);
		sceKernelClearEventFlag(ctrlStorage.ports[port].negative,
					 ~process->ports[port].eventBit);
		ctrlStorage.ports[port].blocked &= ~process->ports[port].eventBit;
		ctrlStorage.ports[port].active &= ~process->ports[port].eventBit;
		capability = SceSysrootForDriver_8AA268D6(1);
	}
	for (slot = 0; slot < 16; slot++) {
		if (ctrlStorage.processes[slot] == pid) {
			ctrlStorage.processes[slot] = -1;
			ctrlStorage.processCount--;
			result = 0;
			break;
		}
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

/* Names unknown: process lifecycle registration callbacks. */
int ctrlCreateProcessCallback(SceUID pid, ProcEventCreateParam *args, int flags)
{
	if (pid < 0)
		return pid;
	return ctrlCreateProcess(pid);
}

int ctrlKillProcessCallback(SceUID pid, ProcEventParam *args, int flags)
{
	if (pid < 0)
		return pid;
	return ctrlDestroyProcess(pid);
}

int ctrlExitProcessCallback(SceUID pid, ProcEventParam *args, int flags)
{
	if (pid < 0)
		return pid;
	return ctrlDestroyProcess(pid);
}

int ctrlActivateProcess(SceUID pid, int event, ProcEventParam *args, int flags)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	unsigned int port;
	int result = SCE_CTRL_ERROR_FATAL;

	if (event != 0x10000)
		return 0;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
		ctrlStorage.processStorageKey, (void **)&process, 0) < 0 || !process)
		goto unlock;
	if (process->samplingMode - 1U <= 2)
		ctrlStorage.requestedSamplingMode++;
	if (process->samplingModeExt - 1U <= 2)
		ctrlStorage.requestedSamplingMode++;
	process->processState = 3;
	for (port = 0;
	     port < (SceSysrootForDriver_8AA268D6(1) ? 5U : 1U);
	     port++)
		ctrlStorage.ports[port].blocked &= ~process->ports[port].eventBit;
	result = 0;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

#if FW_VERSION < 0x04000000
static inline __attribute__((always_inline))
#endif
int _sceCtrlBdRMCForceReleaseCallback(SceUID pid)
{
	uint32_t notify = 0xff;
	uint32_t mode;
	unsigned int slot;
	int result = 0;

	SCE_DBG_ASSERT_CHECK(ctrlStorage.lock != 0, 3021);
	if (pid == -1)
		return 0;
	for (slot = 0; slot < 16; slot++) {
		if (ctrlStorage.remoconReleaseCallbacks[slot] == -1 ||
		    ctrlStorage.remoconReleaseOwners[slot] != pid ||
		    (ctrlStorage.remoconReleaseStates[slot] & 0xf0000000) != 0x10000000)
			continue;
		mode = ctrlStorage.remoconReleaseStates[slot] & 0x0f000000;
		if (mode == 0x1000000 || mode == 0x2000000)
			notify |= mode;
		result = sceKernelNotifyCallback(ctrlStorage.remoconReleaseCallbacks[slot],
						 notify);
		ctrlStorage.remoconReleaseStates[slot] = notify;
		if (result < 0) {
			ctrlStorage.remoconReleaseCallbacks[slot] = -1;
			ctrlStorage.remoconReleaseOwners[slot] = -1;
			result = 0;
		}
	}
	return result;
}

int ctrlProcessEvent(SceUID pid, int event, ProcEventParam *args, int flags)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	uint32_t requestedMode;
	unsigned int port;
	int result = SCE_CTRL_ERROR_FATAL;

	if (event != 0x200 && event != 0x800)
		return 0;
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid,
		ctrlStorage.processStorageKey, (void **)&process, 0) < 0 || !process)
		goto unlock;
	if (event == 0x200) {
		process->processState = 1;
		_sceCtrlBdRMCForceReleaseCallback(pid);
	} else {
		for (port = 0;
		     port < (SceSysrootForDriver_8AA268D6(1) ? 5U : 1U);
		     port++)
			ctrlStorage.ports[port].blocked |= process->ports[port].eventBit;
		process->processState = 2;
		requestedMode = ctrlStorage.requestedSamplingMode;
		if (process->samplingMode - 1U <= 2 && requestedMode)
			requestedMode--;
		if (process->samplingModeExt - 1U <= 2 && requestedMode)
			requestedMode--;
		ctrlStorage.requestedSamplingMode = requestedMode;
	}
	result = 0;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return result;
}

int _sceCtrlBdRemoconSendNotify(uint32_t notify)
{
	struct ctrlProcessStorage *process = NULL;
	SceUID owner;
	unsigned int slot;

#if FW_DEBUG
	if (SceDebugForDriver_374B7868() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ctrlStorage.lock != 0, 2834);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, ctrlStorage.lock != 0,
				SCE_DBGINFO_FUNC(3062, "_sceCtrlGetBdRMCOwner"),
				__builtin_return_address(0));
	}
#endif
	owner = ctrlStorage.activeProcess;
	if (owner == -1 || SceProcessmgrForDriver_AF80F39C(owner,
		ctrlStorage.processStorageKey, (void **)&process, 0) < 0 || !process ||
	    process->processState == 2 || process->buttonIntercept == 1)
		return 0;
	for (slot = 0; slot < 16; slot++) {
		if (ctrlStorage.remoconReleaseCallbacks[slot] == -1 ||
		    ctrlStorage.remoconReleaseOwners[slot] != owner ||
		    !((ctrlStorage.remoconReleaseStates[slot] ^ notify) >> 28))
			continue;
		if (sceKernelNotifyCallback(ctrlStorage.remoconReleaseCallbacks[slot],
					     notify) < 0) {
			ctrlStorage.remoconReleaseCallbacks[slot] = -1;
			ctrlStorage.remoconReleaseOwners[slot] = -1;
			ctrlStorage.remoconReleaseStates[slot] = 0xff;
		} else {
			ctrlStorage.remoconReleaseStates[slot] = notify;
		}
	}
	return 0;
}

int _sceCtrlBdRemoconSendMergedNotify(const SceHdmiBdRemoconState *state)
{
	SceKernelIntrStatus intr;
	uint32_t notify, current, buttons;
	int result;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	buttons = state->buttons;
	notify = state->state | state->stateExt;
	ctrlStorage.bdRemoconButtons = buttons;
	SCE_DBG_ASSERT_CHECK(ctrlStorage.lock != 0, 2879);
	if ((notify & 0xfff) == 0x43)
		goto unlock;
	current = ctrlStorage.bdRemoconNotify;
	if ((current & 0xf0000000) == 0x10000000) {
		ctrlStorage.bdRemoconPendingNotify = notify;
		if ((notify & 0xf0000000) == 0x10000000)
			result = _sceCtrlBdRemoconSendNotify(0x10000ff);
		else
			result = _sceCtrlBdRemoconSendNotify(current | 0x1000000);
		if (result < 0)
			SCE_DBG_ASSERT_FAIL(2924);
	} else if ((ctrlStorage.bdRemoconPendingNotify ^ notify) >> 28) {
		notify |= 0x2000000;
		result = _sceCtrlBdRemoconSendNotify(notify);
		if (result >= 0)
			ctrlStorage.bdRemoconPendingNotify = notify;
		else
			SCE_DBG_ASSERT_FAIL(2951);
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int _sceCtrlBdRemoconSendMergedNotify2(uint32_t buttons, uint32_t state,
				    int connected)
{
	SceKernelIntrStatus intr;
	uint32_t notify, current;
	int result;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.bdRemoconButtons = buttons;
	notify = state | (connected == 1 ? 0x10000000 : 0);
#if FW_DEBUG
	if (SceDebugForDriver_374B7868() > 0)
		SceDebugForDriver_FD753E7A(1, ctrlStorage.lock != 0,
			SCE_DBGINFO_FUNC(2879, "_sceCtrlBdRemoconSendMergedNotify"),
			__builtin_return_address(0));
#endif
	if ((notify & 0xfff) == 0x43)
		goto unlock;
	current = ctrlStorage.bdRemoconPendingNotify;
	if ((current & 0xf0000000) == 0x10000000) {
		ctrlStorage.bdRemoconNotify = notify;
		if ((notify & 0xf0000000) == 0x10000000)
			result = _sceCtrlBdRemoconSendNotify(0x20000ff);
		else
			result = _sceCtrlBdRemoconSendNotify(current | 0x2000000);
#if FW_DEBUG
		if (result < 0 && SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(2924, "_sceCtrlBdRemoconSendMergedNotify"),
				__builtin_return_address(0));
#endif
	} else if ((ctrlStorage.bdRemoconNotify ^ notify) >> 28) {
		notify |= 0x1000000;
		result = _sceCtrlBdRemoconSendNotify(notify);
		if (result >= 0)
			ctrlStorage.bdRemoconNotify = notify;
#if FW_DEBUG
		else if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(2951, "_sceCtrlBdRemoconSendMergedNotify"),
				__builtin_return_address(0));
#endif
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceHdmiForDriver_05174BC2(int (*callback)(const SceHdmiBdRemoconState *state))
	__attribute__((weak));

/* Name unknown: initializes the controller's kernel sampling state. */
int ctrlInitialize(void)
{
	static const char *const eventNames[5] = {
		"SceCtrl0", "SceCtrl1", "SceCtrl2", "SceCtrl3", "SceCtrl4"
	};
	static const char *const extendedEventNames[5] = {
		"SceCtrlExt0", "SceCtrlExt1", "SceCtrlExt2", "SceCtrlExt3", "SceCtrlExt4"
	};
	static const SceKernelProcEventHandler events = {
		.size = sizeof(events),
		.create = ctrlCreateProcessCallback,
		.exit = ctrlExitProcessCallback,
		.kill = ctrlKillProcessCallback,
		.suspend = ctrlProcessEvent,
		.resume = ctrlActivateProcess,
	};
	unsigned int port, slot;
	SceSize heapSize;
	int result;

	memset(&ctrlStorage.lock, 0, offsetof(struct ctrlStorage, userBuffers) -
				     offsetof(struct ctrlStorage, lock));
	ctrlStorage.vblankSampling = 1;
	ctrlStorage.systemEventState = 1;
	ctrlStorage.cachedLx = 0x80;
	ctrlStorage.cachedLy = 0x80;
	ctrlStorage.cachedRx = 0x80;
	ctrlStorage.cachedRy = 0x80;
	ctrlStorage.cachedUp = 0x80;
	ctrlStorage.cachedRight = 0x80;
	ctrlStorage.cachedDown = 0x80;
	ctrlStorage.cachedLeft = 0x80;
	for (slot = 0; slot < 4; slot++)
		ctrlStorage.accumulatedAxes[slot].analog = 0x80;
	for (port = 0;
	     port < (SceSysrootForDriver_8AA268D6(1) ? 5U : 1U);
	     port++) {
		ctrlStorage.ports[port].active = 0;
		ctrlStorage.ports[port].blocked = 0;
		ctrlStorage.ports[port].positive =
			sceKernelCreateEventFlag(eventNames[port], 0x9000, 0, NULL);
		ctrlStorage.ports[port].negative =
			sceKernelCreateEventFlag(extendedEventNames[port], 0x9000, 0, NULL);
	}
	ctrlStorage.systemEvents = sceKernelRegisterSysEventHandler("SceCtrl",
						ctrlSystemEventCallback, NULL);
	ctrlStorage.processEvents = sceKernelRegisterProcEventHandler("SceCtrl", &events, 0);
	SceSysconForDriver_901D6CD4(0);
	ctrlStorage.maskForAll = 0x643fffff;
	ctrlStorage.maskForNonShell = 0x403ffff;
	ctrlStorage.idleCancelKeys[0] = 0x343fffff;
	ctrlStorage.idleCancelKeys[2] = 0x700e0000;
	ctrlStorage.idleCancelThresholds[0] = 0x20;
	ctrlStorage.idleCancelThresholds[1] = 0x20;
	ctrlStorage.idleCancelMask = 0x743fffff;
	ctrlStorage.idleCancelKeys[1] = 0x431ffff;
	ctrlStorage.idleCancelKeys[3] = 0;
	ctrlStorage.baryonVersion = SceSysconForDriver_FF86F4C5();
	ctrlStorage.foregroundSelected = 0;
	ctrlStorage.activeProcessChangePending = 0;
	ctrlStorage.ctrlpMode = 0;
	ctrlStorage.samplingTimer = -1;
	ctrlStorage.activeProcess = -1;
	ctrlStorage.pendingActiveProcess = -1;
	ctrlStorage.ctrlpProcess = -1;
	ctrlStorage.updateMode = SceSysrootForDriver_B0E1FC67();
	ctrlStorage.safeMode = SceSysrootForDriver_834439A7();
	ctrlStorage.processCount = 0;
	memset(ctrlStorage.processes, 0xff, sizeof(ctrlStorage.processes));
	ctrlStorage.lock = 0;
	result = SceProcessmgrForDriver_3801D7D6("SceCtrl", sizeof(struct ctrlProcessStorage));
	ctrlStorage.processStorageKey = result;
	if (result < 0)
		return result;
	heapSize = SceSysrootForDriver_8AA268D6(1) ? 0x32500 : 0xa100;
	ctrlStorage.sampleHeap = SceSysmemForDriver_9328E0E8("SceCtrlHeap", heapSize, NULL);
	ctrlCreateProcess(SCE_KERNEL_PROCESS_ID);
	if (SceHdmiForDriver_05174BC2) {
		result = SceHdmiForDriver_05174BC2(_sceCtrlBdRemoconSendMergedNotify);
		if (result < 0)
			return result;
	} else {
		ctrlStorage.hdmiRemoconAvailable = 0;
	}
	memset(ctrlStorage.bdRemoconCallbacks, 0xff, sizeof(ctrlStorage.bdRemoconCallbacks));
	memset(ctrlStorage.bdRemoconCallbackOwners, 0xff,
	       sizeof(ctrlStorage.bdRemoconCallbackOwners));
	memset(ctrlStorage.remoconReleaseCallbacks, 0xff,
	       sizeof(ctrlStorage.remoconReleaseCallbacks));
	memset(ctrlStorage.remoconReleaseOwners, 0xff, sizeof(ctrlStorage.remoconReleaseOwners));
	for (slot = 0; slot < 16; slot++)
		ctrlStorage.remoconReleaseStates[slot] = 0xff;
	sceKernelRegisterSubIntrHandler(8, 3, "SceCtrlVblank", ctrlVblankCallback, NULL);
	sceKernelEnableSubIntr(8, 3);
	return 0;
}

/* Name unknown: creates the two user-copy buffer mutexes. */
int ctrlInitializeUserBuffers(void)
{
	ctrlStorage.userBuffers[0].mutex = -1;
	ctrlStorage.userBuffers[1].mutex = -1;
	ctrlStorage.userBuffers[0].mutex = sceKernelCreateMutex("SceCtrl1", 0, 0, NULL);
	if (ctrlStorage.userBuffers[0].mutex < 0)
		goto fail;
	ctrlStorage.userBuffers[1].mutex = sceKernelCreateMutex("SceCtrl2", 0, 0, NULL);
	if (ctrlStorage.userBuffers[1].mutex >= 0)
		return 0;
	if (ctrlStorage.userBuffers[0].mutex >= 0)
		sceKernelDeleteMutex(ctrlStorage.userBuffers[0].mutex);
fail:
	if (ctrlStorage.userBuffers[1].mutex >= 0)
		sceKernelDeleteMutex(ctrlStorage.userBuffers[1].mutex);
	return SCE_CTRL_ERROR_FATAL;
}

int module_start(SceSize argSize, void *args)
{
	ctrlInitialize();
	ctrlInitializeUserBuffers();
	return 0;
}

int SceCtrlForDriver_03777131(int clearMask, int setMask)
{
	ctrlStorage.maskForNonShell =
		(ctrlStorage.maskForNonShell & ~clearMask) | setMask;
	return 0;
}

int SceCtrlForDriver_12C13A72(int clearMask, int setMask)
{
	uint32_t mask = (ctrlStorage.maskForAll & ~clearMask) | setMask;

	ctrlStorage.maskForAll = mask;
	ctrlStorage.idleCancelMask = mask;
	return 0;
}

int SceCtrlForDriver_B25BD8FD(uint32_t *mask)
{
	if (!mask)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	*mask = ctrlStorage.maskForNonShell;
	return 0;
}

int SceCtrlForDriver_46ABDDE8(uint32_t *mask)
{
	if (!mask)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	*mask = ctrlStorage.maskForAll;
	return 0;
}

int SceCtrlForDriver_18823EEA(const SceCtrlVirtualControllerDriver *driver)
{
	if (!driver)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	ctrlStorage.virtualDriver = driver;
	return 0;
}

int SceCtrlForDriver_4D8FE670(uint32_t enabled)
{
	ctrlStorage.vibrationEnabled = enabled;
	return 0;
}

int SceCtrlForDriver_ED8F0129(int port)
{
	if (!ctrlStorage.virtualDriver)
		return (int)SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->setTurnOffInterval(port);
}

int SceCtrlForDriver_FA88D837(void)
{
	if (!ctrlStorage.virtualDriver)
		return (int)SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->getActiveControllerPort();
}

int SceCtrlForDriver_F11D0D30(SceCtrlPortInfo *info)
{
	if (!info)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	if (ctrlStorage.virtualDriver)
		return ctrlStorage.virtualDriver->getControllerPortInfo(info);
	memset(info, 0, sizeof(*info));
	info->port[0] = 1;
	return 0;
}

int SceCtrlForDriver_5B849B22(void)
{
	return ctrlStorage.bdRemoconConnected;
}

int SceCtrlForDriver_53381364(int port1, int port2)
{
	if (!ctrlStorage.virtualDriver)
		return (int)SCE_CTRL_ERROR_NOT_SUPPORTED;
	return ctrlStorage.virtualDriver->changePortAssign(port1, port2);
}

int SceCtrlForDriver_6E183B9B(int notification)
{
	unsigned int slot;

	for (slot = 0; slot < 16; slot++) {
		if (ctrlStorage.bdRemoconCallbacks[slot] != -1 &&
		    ctrlStorage.bdRemoconCallbackOwners[slot] != -1 &&
		    sceKernelNotifyCallback(ctrlStorage.bdRemoconCallbacks[slot],
				     notification) < 0)
			ctrlStorage.bdRemoconCallbacks[slot] = -1;
	}
	return 0;
}

/* Internal names unknown; the user exports identify these operations. */
int ctrlSetAnalogStickCheckTarget(uint32_t target)
{
	unsigned int stick;
	uint64_t now;

	if (target >= 4)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	if (ctrlStorage.analogStickCheckTarget != target) {
		now = sceKernelGetSystemTimeWide();
		for (stick = 0; stick < 4; stick++)
			ctrlStorage.analogStickChecks[stick].timestamp = now;
		for (stick = 0; stick < 4; stick++)
			ctrlStorage.analogStickChecks[stick].triggered = 0;
	}
	ctrlStorage.analogStickCheckTarget = target;
	return 0;
}

int ctrlGetAnalogStickCheckTarget(uint32_t *target)
{
	if (!target)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	*target = ctrlStorage.analogStickCheckTarget;
	return 0;
}

int ctrlSetAnalogStickCheckMode(uint32_t mode)
{
	unsigned int stick;
	uint64_t now;

	if (mode >= 2)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	ctrlStorage.analogStickCheckMode = mode;
	if (!mode) {
		now = sceKernelGetSystemTimeWide();
		for (stick = 0; stick < 4; stick++)
			ctrlStorage.analogStickChecks[stick].triggered = 0;
		for (stick = 0; stick < 4; stick++)
			ctrlStorage.analogStickChecks[stick].timestamp = now;
	}
	return 0;
}

int ctrlGetAnalogStickCheckMode(uint32_t *mode)
{
	if (!mode)
		return (int)SCE_CTRL_ERROR_INVALID_ARG;
	*mode = ctrlStorage.analogStickCheckMode;
	return 0;
}

int SceCtrlForDriver_AA36F578(uint32_t connected)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	ctrlStorage.bdRemoconConnected = connected;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int SceCtrlForDriver_80F5E418(int mode)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	SceUID pid;
	int previous;

	if ((uint32_t)mode > 3)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid, ctrlStorage.processStorageKey,
						     (void **)&process, 0) < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
		return SCE_CTRL_ERROR_PRIV_REQUIRED;
	}
	if (mode == 3 && process->mode3Blocked) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
		return SCE_CTRL_ERROR_INVALID_ARG;
	}
	previous = process->samplingMode;
	process->samplingMode = mode;
	if (mode != previous) {
		if (mode >= 1 && mode <= 3)
			ctrlStorage.requestedSamplingMode++;
		else if (ctrlStorage.requestedSamplingMode != 0)
			ctrlStorage.requestedSamplingMode--;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return previous;
}

/* Name unknown: the extended sampling-mode slot has the same update policy. */
int ctrlSetSamplingModeExt(int mode)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	SceUID pid;
	int previous;

	if ((uint32_t)mode > 3)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid, ctrlStorage.processStorageKey,
						     (void **)&process, 0) < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
		return SCE_CTRL_ERROR_PRIV_REQUIRED;
	}
	if (mode == 3 && process->mode3Blocked) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
		return SCE_CTRL_ERROR_INVALID_ARG;
	}
	previous = process->samplingModeExt;
	process->samplingModeExt = mode;
	if (mode != previous) {
		if (mode >= 1 && mode <= 3)
			ctrlStorage.requestedSamplingMode++;
		else if (ctrlStorage.requestedSamplingMode != 0)
			ctrlStorage.requestedSamplingMode--;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return previous;
}

int SceCtrlForDriver_145B13BC(int *mode)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	SceUID pid;

	if (!mode)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid, ctrlStorage.processStorageKey,
						     (void **)&process, 0) < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
		return SCE_CTRL_ERROR_PRIV_REQUIRED;
	}
	*mode = process->samplingMode;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

/* Name unknown: read the separate extended sampling-mode slot. */
int ctrlGetSamplingModeExt(int *mode)
{
	struct ctrlProcessStorage *process = NULL;
	SceKernelIntrStatus intr;
	SceUID pid;

	if (!mode)
		return SCE_CTRL_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_D32ACE9E(&ctrlStorage.lock);
	if (SceProcessmgrForDriver_AF80F39C(pid, ctrlStorage.processStorageKey,
						     (void **)&process, 0) < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
		return SCE_CTRL_ERROR_PRIV_REQUIRED;
	}
	*mode = process->samplingModeExt;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrlStorage.lock, intr);
	return 0;
}

int sceCtrlSetSamplingMode(int mode)
{
	uint32_t context = sceSyscallEnter();
	int result = SceCtrlForDriver_80F5E418(mode);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetSamplingModeExt(int mode)
{
	uint32_t context = sceSyscallEnter();
	int result = ctrlSetSamplingModeExt(mode);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetSamplingMode(int *mode)
{
	uint32_t context = sceSyscallEnter();
	int value = 0;
	int result = SCE_CTRL_ERROR_INVALID_ARG;

	if (mode) {
		result = SceCtrlForDriver_145B13BC(&value);
		if (!result)
			result = SceSysmemForDriver_6D88EF8A(mode, &value, sizeof(value));
	}
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetSamplingModeExt(int *mode)
{
	uint32_t context = sceSyscallEnter();
	int value = 0;
	int result = SCE_CTRL_ERROR_INVALID_ARG;

	if (mode) {
		result = ctrlGetSamplingModeExt(&value);
		if (!result)
			result = SceSysmemForDriver_6D88EF8A(mode, &value, sizeof(value));
	}
	sceSyscallLeave(context);
	return result;
}

int sceCtrlClearRapidFire(int port, int index)
{
	uint32_t context = sceSyscallEnter();
	int result = SceCtrlForDriver_ED138A26(port, index);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetButtonIntercept(int intercept)
{
	uint32_t context = sceSyscallEnter();
	int result = SceCtrlForDriver_06AA5E1A(intercept);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetLightBar(int port, uint8_t red, uint8_t green, uint8_t blue)
{
	uint32_t context = sceSyscallEnter();
	int result = ctrlSetLightBar(port, red, green, blue);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlResetLightBar(int port)
{
	uint32_t context = sceSyscallEnter();
	int result = ctrlResetLightBar(port);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlDisconnect(int port)
{
	uint32_t context = sceSyscallEnter();
	int result = ctrlDisconnect(port);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlSingleControllerMode(int port)
{
	uint32_t context = sceSyscallEnter();
	int result = ctrlSingleControllerMode(port);

	sceSyscallLeave(context);
	return result;
}

int sceCtrlIsMultiControllerSupported(void)
{
	uint32_t context = sceSyscallEnter();
	int result = ctrlIsMultiControllerSupported();

	sceSyscallLeave(context);
	return result;
}

int sceCtrlRegisterBdRMCCallback(SceUID callback)
{
	SceUID guid = SceSysmemForDriver_45D22597(0, callback);
	uint32_t context;
	int result;

	if (guid < 0)
		return guid;
	context = sceSyscallEnter();
	result = ctrlRegisterBdRMCCallback(guid);
	sceSyscallLeave(context);
	return result;
}

int sceCtrlUnregisterBdRMCCallback(SceUID callback)
{
	SceUID guid = SceSysmemForDriver_45D22597(0, callback);
	uint32_t context;
	int result;

	if (guid < 0)
		return guid;
	context = sceSyscallEnter();
	result = ctrlUnregisterBdRMCCallback(guid);
	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetAnalogStickCheckTarget(uint32_t target)
{
	uint32_t context = sceSyscallEnter();
	int result = SCE_CTRL_ERROR_PRIV_REQUIRED;

	if (sceSblACMgrIsSystemProgram(sceKernelGetProcessId()))
		result = ctrlSetAnalogStickCheckTarget(target);
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetAnalogStickCheckTarget(uint32_t *target)
{
	uint32_t context, local;
	int result = SCE_CTRL_ERROR_PRIV_REQUIRED;

	if (!target)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	if (sceSblACMgrIsSystemProgram(sceKernelGetProcessId())) {
		result = ctrlGetAnalogStickCheckTarget(&local);
		if (!result)
			result = SceSysmemForDriver_6D88EF8A(target, &local, sizeof(local));
	}
	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetAnalogStickCheckMode(uint32_t mode)
{
	uint32_t context = sceSyscallEnter();
	int result = SCE_CTRL_ERROR_PRIV_REQUIRED;

	if (sceSblACMgrIsSystemProgram(sceKernelGetProcessId()))
		result = ctrlSetAnalogStickCheckMode(mode);
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetAnalogStickCheckMode(uint32_t *mode)
{
	uint32_t context, local;
	int result = SCE_CTRL_ERROR_PRIV_REQUIRED;

	if (!mode)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	if (sceSblACMgrIsSystemProgram(sceKernelGetProcessId())) {
		result = ctrlGetAnalogStickCheckMode(&local);
		if (!result)
			result = SceSysmemForDriver_6D88EF8A(mode, &local, sizeof(local));
	}
	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetRapidFire(int port, int index, const SceCtrlRapidFireRule *rule)
{
	SceCtrlRapidFireRule local;
	uint32_t context;
	int result;

	if (!rule)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	result = SceSysmemForDriver_BC996A7A(&local, rule, sizeof(local));
	if (!result)
		result = SceCtrlForDriver_A248CDAE(port, index, &local);
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetButtonIntercept(int *intercept)
{
	uint32_t context;
	int local, result;

	if (!intercept)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	result = SceCtrlForDriver_6BEB1CD7(&local);
	if (!result)
		result = SceSysmemForDriver_6D88EF8A(intercept, &local, sizeof(local));
	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetActuator(int port, const SceCtrlActuator *state)
{
	SceCtrlActuator local;
	uint32_t context;
	int result;

	if (!state)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	result = SceSysmemForDriver_BC996A7A(&local, state, sizeof(local));
	if (!result)
		result = ctrlSetActuator(port, &local);
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetBatteryInfo(int port, uint8_t *battery)
{
	uint32_t context;
	uint8_t local;
	int result;

	if (!battery)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	result = ctrlGetBatteryInfo(port, &local);
	if (!result)
		result = SceSysmemForDriver_6D88EF8A(battery, &local, sizeof(local));
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetControllerPortInfo(SceCtrlPortInfo *info)
{
	SceCtrlPortInfo local;
	uint32_t context;
	int result;

	if (!info)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	result = SceCtrlForDriver_F11D0D30(&local);
	if (!result)
		result = SceSysmemForDriver_6D88EF8A(info, &local, sizeof(local));
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetProcessStatus(SceUID pid, int *status)
{
	uint32_t context;
	int local, result;

	if (!status)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	result = ctrlGetProcessStatus(pid, &local);
	if (!result)
		result = SceSysmemForDriver_6D88EF8A(status, &local, sizeof(local));
	sceSyscallLeave(context);
	return result;
}

int sceCtrlGetWirelessControllerInfo(SceCtrlWirelessControllerInfo *info)
{
	SceCtrlWirelessControllerInfo local;
	uint32_t context;
	int result;

	if (!info)
		return SCE_CTRL_ERROR_INVALID_ARG;
	context = sceSyscallEnter();
	result = ctrlGetWirelessControllerInfo(&local);
	if (!result)
		result = SceSysmemForDriver_6D88EF8A(info, &local, sizeof(local));
	sceSyscallLeave(context);
	return result;
}

int sceCtrlSetButtonRemappingInfo(const struct ctrlButtonRemappingInfo *info)
{
	struct ctrlButtonRemappingInfo local;
	uint32_t context = sceSyscallEnter();
	int result;

	if (info) {
		result = SceSysmemForDriver_BC996A7A(&local, info, sizeof(local));
		if (result)
			goto out;
		info = &local;
	}
	result = ctrlSetButtonRemappingInfo(info);
out:
	sceSyscallLeave(context);
	return result;
}
