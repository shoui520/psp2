/* SceTouchDummy: the virtual touch pads a virtual touch driver feeds (ports 1 to 4).
 * Private names without a recovered SDK name remain unknown.
 */
#include "private.h"

/* the driver's samples of the four pads, and the reads before they are copied out */
struct virtualBuffers {
	SceTouchData samples[4];
	SceTouchData read[64];
};

static struct virtualBuffers virtualBuffers;

static const char *const padEventNames[4] = {
	"SceTouchPad1", "SceTouchPad2", "SceTouchPad3", "SceTouchPad4",
};

int readVirtual(unsigned port, SceTouchData *out, int count, unsigned flags) /* name unknown */
{
	struct virtualProcess *process;
	struct virtualRing *ring;
	SceTouchData *sample, *data;
	unsigned intr, bit;
	int ret, pid, available, consume, index, i, j;

	if (!out || (unsigned)count > 64)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	port--;
	if (port > 3)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	pid = sceKernelGetProcessId();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return ret;
	}
	ring = &process->ports[port].ring;
	if (flags & 1) {
		consume = 0;
		available = 64;
	} else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		if (!(process->ports[port].eventBit & pad.ports[port].allocated))
			return SCE_TOUCH_ERROR_INVALID_ARG;
		ret = sceKernelWaitEventFlag(pad.ports[port].event, process->ports[port].eventBit,
					     SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
		if (ret < 0)
			return ret;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
		bit = process->ports[port].eventBit;
		if (bit & pad.ports[port].allocated)
			sceKernelClearEventFlag(pad.ports[port].event, ~bit);
		if (ring->reader > ring->writeIndex)
			available = ring->writeIndex + 64 - ring->reader;
		else
			available = ring->writeIndex - ring->reader;
		consume = 1;
	}
	if (available) {
		if (available >= count)
			available = count;
		index = ring->writeIndex;
		for (i = available - 1; i >= 0; i--) {
			index = index ? index - 1 : 63;
			sample = &ring->samples[index];
			data = &virtualBuffers.read[i];
			data->timeStamp = sample->timeStamp;
			data->reportNum = 0;
			data->status = sample->status;
			for (j = 0; j < (int)sample->reportNum; j++) {
				data->report[data->reportNum] = sample->report[j];
				data->report[data->reportNum].force = 0;
				data->report[data->reportNum].info = 0;
				data->reportNum++;
			}
		}
		if (consume)
			ring->reader = ring->writeIndex;
	}
	if (sceKernelCopyToUser(out, virtualBuffers.read, available * sizeof(SceTouchData)) > 0)
		SceDebugForDriver_391B74B7("error!\n");
	if (process->state == 4)
		process->state = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return available;
}

int createVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr, bit;
	int ret, slot, i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 1);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return ret;
	}
	for (slot = 0; slot < 16; slot++)
		if (pad.pids[slot] == -1)
			break;
	if (slot == 16) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return SCE_TOUCH_ERROR_FATAL;
	}
	pad.pids[slot] = pid;
	pad.processCount++;
	memset(process, 0, sizeof(*process));
	process->pid = pid;
	process->state = 0;
	for (i = 0; i < 4; i++) {
		process->ports[i].ring.reader = 0;
		process->ports[i].ring.writeIndex = 0;
		bit = allocateBit(&pad.ports[i].allocated);
		process->ports[i].eventBit = bit;
		pad.ports[i].stopped &= ~bit;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return 0;
}

int removeVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr, bit;
	int ret, slot, i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return ret;
	}
	if (!pad.processCount)
		goto fatal;
	for (slot = 0; slot < 16; slot++)
		if (pad.pids[slot] == pid)
			break;
	if (slot == 16)
		goto fatal;
	pad.processCount--;
	pad.pids[slot] = -1;
	for (i = 0; i < 4; i++) {
		bit = process->ports[i].eventBit;
		pad.ports[i].stopped &= ~bit;
		if (pad.ports[i].allocated & bit)
			sceKernelClearEventFlag(pad.ports[i].event, ~bit);
		pad.ports[i].allocated &= ~process->ports[i].eventBit;
	}
	memset(process, 0, sizeof(*process));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return 0;

fatal:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return SCE_TOUCH_ERROR_FATAL;
}

static SceTouchData *lastVirtualSample(struct virtualRing *ring) /* name unknown */
{
	return &ring->samples[ring->writeIndex ? ring->writeIndex - 1 : 63];
}

int suspendVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr;
	int ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return ret;
	}
	if (lastVirtualSample(&process->ports[0].ring)->reportNum |
	    lastVirtualSample(&process->ports[1].ring)->reportNum)
		process->state = 1;
	else
		process->state = 2;
	process->timestamp = SceThreadmgrForDriver_47F6DE49();
	process->generation = pad.generation;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return 0;
}

int checkVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr;
	int ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return ret;
	}
	ret = 0;
	if (process->state != 2) {
		if (process->generation == pad.generation &&
		    SceThreadmgrForDriver_47F6DE49() - process->timestamp <= 34000)
			ret = -1;
		else
			process->state = 2;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return ret;
}

int stopVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr;
	int ret, i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return ret;
	}
	for (i = 0; i < 4; i++)
		pad.ports[i].stopped |= process->ports[i].eventBit;
	process->state = 3;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return 0;
}

int resumeVirtualProcess(int pid) /* name unknown */
{
	struct virtualProcess *process;
	unsigned intr;
	int ret, i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pad.lock);
	ret = lookupProcess(pid, pad.plsId, (void **)&process, 0);
	if (ret < 0 || !process) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
		return ret;
	}
	for (i = 0; i < 4; i++)
		pad.ports[i].stopped &= ~process->ports[i].eventBit;
	process->state = 4;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pad.lock, intr);
	return 0;
}

int initializeVirtual(void) /* name unknown */
{
	int i;

	memset(&pad, 0, sizeof(pad));
	memset(pad.pids, -1, sizeof(pad.pids));
	memset(pad.priorities, -1, sizeof(pad.priorities));
	for (i = 0; i < 4; i++) {
		pad.ports[i].allocated = 0;
		pad.ports[i].stopped = 0;
		pad.ports[i].event = sceKernelCreateEventFlag(padEventNames[i], 0x9000, 0, NULL);
		if (pad.ports[i].event < 0)
			break;
	}
	pad.lock = 0;
	pad.plsId = sceKernelRegisterKPLS("SceTouchPad", sizeof(struct virtualProcess));
	return pad.plsId < 0 ? pad.plsId : 0;
}

int sampleVirtual(TouchVirtualSampleCallback driver) /* name unknown */
{
	TouchVirtualSampleRequest request = { 0 };
	SceTouchData *sample = &virtualBuffers.samples[0], *data;
	struct virtualProcess *process;
	struct virtualRing *ring;
	unsigned processed, reports, r;
	int port, ret, i;

	for (i = 0; i < 4; i++) {
		memset(&virtualBuffers.samples[i], 0, sizeof(SceTouchData));
		request.samples[i] = &virtualBuffers.samples[i];
	}
	for (port = 1; port != 5; port++) {
		ret = driver(port, &request);
		if (ret < 0) {
			SceDebugForDriver_391B74B7("[Err] %x\n", ret);
			return -1;
		}
		if (!ret)
			continue;
		if (request.samples[0]->reportNum)
			sceKernelPowerTick(0);
		processed = 0;
		for (i = 0; i < 16; i++) {
			if (processed >= pad.processCount)
				break;
			if (pad.pids[i] == -1)
				continue;
			ret = lookupProcess(pad.pids[i], pad.plsId, (void **)&process, 0);
			if (ret < 0 || ret != 0 || process->state == 3)
				continue;
			ring = &process->ports[port - 1].ring;
			data = &ring->samples[ring->writeIndex];
			SceProcessmgrForDriver_8CB01675(pad.pids[i], &data->timeStamp, sample->timeStamp);
			data->status = sample->status;
			if (process->state)
				data->status = sample->status | 0x10000;
			reports = sample->reportNum;
			data->reportNum = reports;
			for (r = 0; r != reports; r++)
				data->report[r] = sample->report[r];
			ring->writeIndex = (ring->writeIndex + 1) % 64;
			if (ring->writeIndex == ring->reader)
				ring->reader = (ring->writeIndex + 1) % 64;
			processed++;
		}
	}
	for (i = 0; i < 4; i++)
		sceKernelSetEventFlag(pad.ports[i].event, pad.ports[i].allocated & ~pad.ports[i].stopped);
	return 0;
}

int readVirtualWait(unsigned device, unsigned port, SceTouchData *out, unsigned count) /* name unknown */
{
	return readVirtual(device, out, count, 0);
}

int readVirtualPeek(unsigned device, unsigned port, SceTouchData *out, unsigned count) /* name unknown */
{
	return readVirtual(device, out, count, 1);
}
