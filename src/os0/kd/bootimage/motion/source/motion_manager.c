/* Hardware motion lifecycle and asynchronous device completions. */
#include "motion_manager.h"
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/proc_event.h>
#include <string.h>
#include <sce/processmgr.h>
#include <sce/threadmgr.h>
#include <sce/idstorage_imports.h>
#include <sce/idstorage.h>
#include <sce/syscon.h>
#include <sce/sysevent.h>
#include <psp2kern/kernel/sysmem.h>

static SceMotionDevData motionRingSamples[MOTION_RING_SAMPLES];
static SceMotionDevData motionReadBuffer[MOTION_RING_SAMPLES];
static MotionManagerState motion;
static SceUID motionRingOwners[MOTION_RING_SAMPLES];
static uint16_t motionBusy;
static uint16_t motionBusyTicks;
static SceMotionDevData motionSample;
static MotionPacketCompletion motionPacket;
static MotionUpdateState motionUpdate;
static MotionRing motionRing;
static MotionCalibrationBuffer *motionCalibrationBuffer;

void motionSuspendComplete(int result, void *arg)
{
	(void)result;
	(void)arg;
	motion.suspendState |= 0x80000000;
}

void motionClearBusy(int result, void *arg)
{
	(void)result;
	(void)arg;
	motionBusy = 0;
}

void motionPacketComplete(int result, void *arg)
{
	void (*completion)(int, const MotionSensorPacket *,
		MotionSensorPacket *, void *) = motionPacket.completion;
	const MotionSensorPacket *request = motionPacket.request;
	MotionSensorPacket *response = motionPacket.response;
	void *completionArg = motionPacket.arg;
	int intr;

	(void)arg;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	motionPacket.completion = NULL;
	motionPacket.request = NULL;
	motionPacket.response = NULL;
	motionPacket.arg = NULL;
	motionBusy = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	if (completion != NULL)
		completion(result, request, response, completionArg);
}

void motionSamplesComplete(int result, void *arg)
{
	SceMotionDevData *sample = NULL;
	SceMotionDevDataEntry *entry;
	SceUID *owner = NULL;
	float bias[3], delta[3];
	int intr, i;

	(void)result;
	(void)arg;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motionBusy == 1) {
		motionBusy = 0;
		motionRingGetWriteSlot(motion.ring, &sample, &owner);
		if (sample != NULL && owner != NULL && motionSample.time != 0 &&
		    motionSample.count != 0 &&
		    (motion.lastSequence == -1 ||
		     motionSample.entry[0].sequence != (uint16_t)motion.lastSequence)) {
			motionSample.magnCalibrationGeneration =
				motion.currentMagnCalibration.generation;
			motionSample.magnStabilityLevel = motion.magnStabilityLevel;
			memcpy(sample, &motionSample, sizeof(*sample));
			*owner = motion.owner;
			entry = sample->entry;
			for (i = 0; i < sample->count; i++, entry++) {
				if (!(entry->flags & MOTION_ENTRY_GYRO))
					continue;
				if (updateGyroFilter(&motion.filter, entry->gyro,
						     bias, delta)) {
					motion.gyroBias.generation++;
					motion.gyroBias.bias[0] = bias[0];
					motion.gyroBias.bias[1] = bias[1];
					motion.gyroBias.bias[2] = bias[2];
				}
			}
			sample->biasGeneration = motion.gyroBias.generation;
			motion.lastSequence = sample->entry[0].sequence;
			motionRingAdvance(motion.ring);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
}

void motionStopComplete(int result, void *arg)
{
	int intr;

	(void)result;
	(void)arg;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motionBusy != 1) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return;
	}
	motionBusy = 0;
	if (motion.state == 3) {
		ksceKernelSetEventFlag(motionUpdate.event, 1);
	} else if (motion.state == 5) {
		motion.savedState = 2;
		motion.suspendState |= 0x80000000;
	} else {
		motion.state = 2;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
}

void motionIdentityComplete(int result, void *arg)
{
	int intr;

	(void)arg;
	if (result == 0)
		motion.initState = 4;
	motionBusy = 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	switch (motion.state) {
	case 3:
		ksceKernelSetEventFlag(motionUpdate.event, 1);
		break;
	case 4:
		motion.state = 128;
		ksceKernelSetEventFlag(motionUpdate.event, 2);
		break;
	case 5:
		motion.savedState = 128;
		motion.suspendState |= 0x80000000;
		break;
	default:
		motion.state = 128;
		break;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
}

void motionStartComplete(int result, void *arg)
{
	int intr;

	(void)result;
	(void)arg;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motionBusy == 1) {
		motionBusy = 0;
		resetGyroFilter(&motion.filter);
		motion.state = 1;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
}

uint32_t motionInitializeTimer(void *arg)
{
	int result;

	(void)arg;

	motionBusy = 1;
	result = motionReadDeviceInfo(&motion.deviceInfo,
		motionInitializeComplete, NULL);
	if (result < 0)
		motionBusy = 0;
	else
		motion.initState = 0;
	return 0;
}

int motionReadSamples(SceMotionDevData *data, int count, uint32_t *info,
	SceUID pid, int user)
{
	MotionDeviceReader *process;
	SceMotionDevData *sample, *dest, *buffer;
	SceUID *owner;
	uint32_t overrideSampling;
	int intr, i, j, k, n, skip, ret;

	if (data == NULL || (unsigned int)(count - 1) >= MOTION_RING_SAMPLES)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	ret = 0;
	if ((unsigned int)(motion.state - 1) > 1) {
		if (motion.state == 128)
			ret = (int)0x80360006;
		goto out;
	}
	overrideSampling = motion.overrideSampling != 0;
	if (info != NULL) {
		if (user == 1) {
			if (ksceKernelCopyToUser(info, &overrideSampling,
						sizeof(overrideSampling)) < 0)
				goto out;
		} else {
			*info = overrideSampling;
		}
	}
	if (motion.state != 1)
		goto out;
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x803600ff;
	}
	process = &motion.processes[i];
	if (process->stopped)
		goto out;
	n = motionRingCount(motion.ring, process->reader);
	if (count < n) {
		skip = n - count;
		n = count;
	} else if (n > 0) {
		skip = 0;
	} else {
		goto out;
	}
	buffer = user == 1 ? motionReadBuffer : data;
	for (j = 0; j < n; j++) {
		unsigned int wraps;

		sample = NULL;
		owner = NULL;
		ret = motionRingPeek(motion.ring, process->reader, skip + j,
			&sample, &owner);
		if (ret < 0) {
			n = ret;
			break;
		}
		if (sample == NULL || owner == NULL) {
			n = 0;
			break;
		}
		wraps = sample->timestampWraps & 0xf;
		if (process->rebase && sample->count) {
			uint32_t timestamp = sample->entry[0].timestamp;

			if (timestamp >= process->lastTimestamp)
				process->timestampOffset = timestamp - process->lastTimestamp - wraps;
			else
				process->timestampOffset = timestamp - process->lastTimestamp + 0xffff * wraps;
			process->rebase = 0;
		}
		dest = &buffer[j];
		memcpy(dest, sample, sizeof(*dest));
		if (*owner != pid) {
			__builtin_prefetch((void *)((uintptr_t)dest + 728));
			dest->flags |= MOTION_DATA_OTHER_OWNER;
		}
		for (k = 0; k < dest->count; k++) {
			uint32_t timestamp = dest->entry[k].timestamp;

			if (timestamp < process->timestampOffset)
				dest->entry[k].timestamp = (wraps << 16) - process->timestampOffset + timestamp;
			else
				dest->entry[k].timestamp = timestamp - process->timestampOffset;
		}
		SceProcessmgrForDriver_8CB01675(0, &dest->time, sample->time);
	}
	if (j > 0) {
		dest = &buffer[j - 1];
		if (dest->count)
			process->lastTimestamp = dest->entry[dest->count - 1].timestamp;
	}
	motionRingConsume(motion.ring, process->reader);
	if (user == 1 && j > 0 && n > 0) {
		ret = ksceKernelCopyToUser(data, motionReadBuffer,
			n * sizeof(*motionReadBuffer));
		if (ret < 0)
			goto out;
	}
	ret = n;
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return ret;
}

void motionResponseComplete(int result, void *arg)
{
	void (*completion)(int, const MotionSensorResponse *,
		MotionSensorResponse *, void *);
	const MotionSensorResponse *request;
	MotionSensorResponse *response;
	void *completionArg;
	int intr;

	(void)arg;
	if ((result != 0) | (motionUpdate.response == NULL))
		goto complete;
	if (motionUpdate.response->type == 0) {
		result = motionRequestResponse(NULL, motionUpdate.response,
			motionResponseComplete, NULL);
		if (result >= 0)
			return;
	}
complete:
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	completion = motionUpdate.completion;
	motionBusy = 0;
	request = motionUpdate.request;
	response = motionUpdate.response;
	motionUpdate.request = NULL;
	motionUpdate.response = NULL;
	completionArg = motionUpdate.arg;
	motionUpdate.completion = NULL;
	motionUpdate.arg = NULL;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	if (completion != NULL)
		completion(result, request, response, completionArg);
}

int motionInitializeMagnCalibration(int count,
	const SceMotionDevCalibrationData *records, SceMotionDevMagnCalibData *calibration)
{
	unsigned int present = 0;
	int i;

	memset(calibration, 0, sizeof(*calibration));
	for (i = 0; i != count; i++) {
		const SceMotionDevCalibrationData *record = &records[i];
		unsigned int lane;
		float x, y, z;
		union { uint32_t integer; float real; } value;

		if (record->attributes[0] != 2 ||
		    (record->attributes[1] & 0xfd) != 4)
			continue;
		if (record->attributes[2] != 0) {
			x = (float)record->parameters[1];
			y = (float)record->parameters[2];
			z = (float)record->parameters[3];
		} else {
			value.integer = record->parameters[1];
			x = value.real;
			value.integer = record->parameters[2];
			y = value.real;
			value.integer = record->parameters[3];
			z = value.real;
		}
		lane = record->kind & 1;
		if (record->attributes[1] == 4) {
			calibration->parameters[0][lane][0] = x;
			calibration->parameters[0][lane][1] = y;
			calibration->parameters[0][lane][2] = z;
			if (lane == 0)
				present |= 1;
		} else if (record->attributes[1] == 6) {
			calibration->parameters[1][lane][0] = x;
			calibration->parameters[1][lane][1] = y;
			calibration->parameters[1][lane][2] = z;
			if (lane == 0)
				present |= 2;
		}
	}
	if ((present & 3) == 3)
		calibration->generation = 1;
	return 0;
}

int motionStopProcess(SceUID pid, int event, SceProcEventInvokeParam1 *param, int flags)
{
	MotionDeviceReader *process;
	int intr, i;

	(void)param;
	(void)flags;
	if (event != 0x800)
		return 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else
		i = MOTION_RING_READERS;
	if (i != MOTION_RING_READERS) {
		process = &motion.processes[i];
		process->stopped = 1;
		process->rebase = process->sampling != 0;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

uint32_t motionSampleTimer(void *arg)
{
	int period = motion.period;
	int intr, mode, i, result;

	(void)arg;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	mode = motion.overrideSampling;
	if (mode == 0) {
		for (i = 0; i < MOTION_RING_READERS; i++) {
			MotionDeviceReader *process = &motion.processes[i];
			int sampling;

			if (process->pid == -1 || process->stopped == 1)
				continue;
			sampling = process->sampling;
			if (sampling == 2)
				sampling = 0;
			mode |= sampling;
		}
		if (mode != 1)
			mode = mode == 3 ? 0 : 3;
	} else {
		mode = 3;
	}
	if (motionBusy == 1) {
		motionBusyTicks++;
		goto out;
	}
	switch (motion.state) {
	case 1:
		if (mode == 3)
			goto stop;
		motionBusyTicks = 0;
		motion.mode = mode;
		motionBusy = 1;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		if (motion.samplingDisabled)
			return period;
		result = motionReadDeviceSamples(&motionSample, (uint8_t)mode,
			motionSamplesComplete, &motionSample);
		break;
	case 2:
		if (mode == 3) {
			period = 0;
			motion.mode = 3;
			motion.timer = -1;
			motion.lastSequence = -1;
			goto out;
		}
		motionBusy = 1;
		motion.mode = mode;
		motionBusyTicks = 0;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		if (motion.samplingDisabled)
			return period;
		result = motionStartDevice((uint8_t)mode, motionStartComplete, NULL);
		break;
	case 3:
	case 5:
		period = 0;
		motion.timer = -1;
		motion.lastSequence = -1;
		motion.mode = 3;
stop:
		motionBusy = 1;
		motionBusyTicks = 0;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		if (motion.samplingDisabled)
			return period;
		result = motionStopDevice(motionStopComplete, NULL);
		break;
	default:
		goto out;
	}
	if (result < 0)
		motionBusy = 0;
	return period;
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return period;
}

int motionCreateProcess(SceUID pid, SceProcEventInvokeParam2 *param, int flags)
{
	MotionDeviceReader *process;
	int intr, i;

	(void)param;
	(void)flags;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == -1)
		i = 0;
	else if (motion.processes[1].pid == -1)
		i = 1;
	else if (motion.processes[2].pid == -1)
		i = 2;
	else if (motion.processes[3].pid == -1)
		i = 3;
	else if (motion.processes[4].pid == -1)
		i = 4;
	else if (motion.processes[5].pid == -1)
		i = 5;
	else if (motion.processes[6].pid == -1)
		i = 6;
	else if (motion.processes[7].pid == -1)
		i = 7;
	else if (motion.processes[8].pid == -1)
		i = 8;
	else if (motion.processes[9].pid == -1)
		i = 9;
	else if (motion.processes[10].pid == -1)
		i = 10;
	else if (motion.processes[11].pid == -1)
		i = 11;
	else if (motion.processes[12].pid == -1)
		i = 12;
	else if (motion.processes[13].pid == -1)
		i = 13;
	else if (motion.processes[14].pid == -1)
		i = 14;
	else {
		if (motion.processes[15].pid != -1) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return (int)0x80360005;
		}
		i = 15;
	}
	process = &motion.processes[i];
	memset(process, 0, sizeof(*process));
	process->pid = pid;
	process->reader = motionRingOpenReader(motion.ring);
	process->stopped = 0;
	process->sampling = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionExitProcess(SceUID pid, SceProcEventInvokeParam1 *param, int flags)
{
	MotionDeviceReader *process;
	int intr, i;

	(void)param;
	(void)flags;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else
		i = MOTION_RING_READERS;
	if (i == MOTION_RING_READERS) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	process = &motion.processes[i];
	motionRingCloseReader(motion.ring, process->reader);
	if (motion.owner == process->pid)
		motion.owner = -1;
	process->pid = -1;
	process->reader = -1;
	process->stopped = 0;
	process->sampling = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionKillProcess(SceUID pid, SceProcEventInvokeParam1 *param, int flags)
{
	MotionDeviceReader *process;
	int intr, i;

	(void)param;
	(void)flags;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else
		i = MOTION_RING_READERS;
	if (i == MOTION_RING_READERS) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	process = &motion.processes[i];
	motionRingCloseReader(motion.ring, process->reader);
	if (motion.owner == process->pid)
		motion.owner = -1;
	process->pid = -1;
	process->reader = -1;
	process->stopped = 0;
	process->sampling = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionStartProcess(SceUID pid, int event, SceProcEventInvokeParam1 *param,
	int flags);
int motionSysEvent(int resume, int event, void *arg, void *param);

static const SceProcEventHandler motionProcessHandlers = {
	.size = sizeof(SceProcEventHandler),
	.create = motionCreateProcess,
	.exit = motionExitProcess,
	.kill = motionKillProcess,
	.stop = motionStopProcess,
	.start = motionStartProcess,
	.switch_process = NULL,
};

int motionInitializeManager(void *arg)
{
	DeviceLocation location;
	MotionDeviceReader *process;
	int intr, i, result;

	(void)arg;
	ksceSysconWaitInitialized();
	motion.baryonVersion = ksceSysconGetBaryonVersion();
	location = (DeviceLocation){0};
	result = ksceIdStorageLookup(0x114, 32, &location, sizeof(location));
	if (result < 0) {
		location.type = 16;
		if ((ksceSysconGetHardwareInfo() & 0xff0000) < 0x800000) {
			location.x = -32;
			location.y = 12;
		} else {
			location.x = 13;
			location.y = 19;
		}
		location.z = -8;
	}
	if (location.type == 16) {
		{
			float value = (float)location.x;

			if (value > 300.0f)
				value = 300.0f;
			else if (value < -300.0f)
				value = -300.0f;
			motion.deviceLocation.sensorPosition[0][0] = value;
		}
		{
			float value = (float)location.y;

			if (value > 300.0f)
				value = 300.0f;
			else if (value < -300.0f)
				value = -300.0f;
			motion.deviceLocation.sensorPosition[0][1] = value;
		}
		{
			float value = (float)location.z;

			if (value > 300.0f)
				value = 300.0f;
			else if (value < -300.0f)
				value = -300.0f;
			motion.deviceLocation.sensorPosition[0][2] = value;
		}
	}
	location = (DeviceLocation){0};
	result = ksceIdStorageLookup(0x114, 48, &location, sizeof(location));
	if (result < 0) {
		location.type = 17;
		if ((ksceSysconGetHardwareInfo() & 0xff0000) < 0x800000) {
			location.x = -33;
			location.y = 6;
		} else {
			location.x = 13;
			location.y = 19;
		}
		location.z = -8;
	}
	if (location.type == 17) {
		{
			float value = (float)location.x;

			if (value > 300.0f)
				value = 300.0f;
			else if (value < -300.0f)
				value = -300.0f;
			motion.deviceLocation.sensorPosition[1][0] = value;
		}
		{
			float value = (float)location.y;

			if (value > 300.0f)
				value = 300.0f;
			else if (value < -300.0f)
				value = -300.0f;
			motion.deviceLocation.sensorPosition[1][1] = value;
		}
		{
			float value = (float)location.z;

			if (value > 300.0f)
				value = 300.0f;
			else if (value < -300.0f)
				value = -300.0f;
			motion.deviceLocation.sensorPosition[1][2] = value;
		}
	}
	location.type = 0;
	motion.allowGyroControl = ksceIdStorageLookup(0x114, 256,
		&location.type, sizeof(location.type)) < 0 || location.type != 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == -1)
		i = 0;
	else if (motion.processes[1].pid == -1)
		i = 1;
	else if (motion.processes[2].pid == -1)
		i = 2;
	else if (motion.processes[3].pid == -1)
		i = 3;
	else if (motion.processes[4].pid == -1)
		i = 4;
	else if (motion.processes[5].pid == -1)
		i = 5;
	else if (motion.processes[6].pid == -1)
		i = 6;
	else if (motion.processes[7].pid == -1)
		i = 7;
	else if (motion.processes[8].pid == -1)
		i = 8;
	else if (motion.processes[9].pid == -1)
		i = 9;
	else if (motion.processes[10].pid == -1)
		i = 10;
	else if (motion.processes[11].pid == -1)
		i = 11;
	else if (motion.processes[12].pid == -1)
		i = 12;
	else if (motion.processes[13].pid == -1)
		i = 13;
	else if (motion.processes[14].pid == -1)
		i = 14;
	else if (motion.processes[15].pid == -1)
		i = 15;
	else
		i = MOTION_RING_READERS;
	if (i != MOTION_RING_READERS) {
		process = &motion.processes[i];
		memset(process, 0, sizeof(*process));
		process->pid = 0x10005;
		process->reader = motionRingOpenReader(motion.ring);
		process->stopped = 0;
		process->sampling = 0;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	if (motionInitializeDevice() >= 0) {
		motion.sysEventHandler = ksceKernelRegisterSysEventHandler(
			"SceMotion", motionSysEvent, NULL);
		motion.procEventHandler = ksceKernelRegisterProcEventHandler(
			"SceMotion", &motionProcessHandlers, 0);
		ksceKernelRegisterTimer("SceMotionAlarm0", 80000,
			motionInitializeTimer, NULL);
	}
	return 1;
}

void motionResumeComplete(int result, void *arg)
{
	unsigned int mode;
	uint32_t control;
	int i;

	(void)result;
	(void)arg;
	switch ((uint8_t)motion.suspendState) {
	case 0:
		motion.suspendState = 0x40000001;
		control = 1;
		if ((unsigned int)motion.deviceInfo.firmwareVersion - 0x1010 < 0x2f0 &&
		    motion.allowGyroControl) {
			if (motion.deviceInfo.attributes[0] <= 0x20)
				control = 6;
		}
		motionControlDevice(control, motionResumeComplete, NULL);
		break;
	case 1:
		motion.suspendState = 0x40000002;
		motionStopDevice(motionResumeComplete, NULL);
		break;
	case 2:
		motion.state = motion.savedState;
		mode = motion.overrideSampling;
		if (mode != 0) {
			mode = 3;
		} else {
			for (i = 0; i < MOTION_RING_READERS; i++) {
				MotionDeviceReader *process = &motion.processes[i];
				int sampling;

				if (process->pid == -1 || process->stopped == 1)
					continue;
				sampling = process->sampling;
				if (sampling == 2)
					sampling = 0;
				mode |= sampling;
			}
			if (mode != 1)
				mode = mode == 3 ? 0 : 3;
		}
		if (motion.timer == -1 && motion.savedState == 2 && mode <= 1)
			motion.timer = ksceKernelRegisterTimer("SceMotionAlarm", motion.period,
				motionSampleTimer, NULL);
		motion.suspendState = 16;
		break;
	default:
		break;
	}
}

int motionStartProcess(SceUID pid, int event, SceProcEventInvokeParam1 *param, int flags)
{
	MotionDeviceReader *process;
	unsigned int mode;
	int intr, i;

	(void)param;
	(void)flags;
	if (event != 0x10000)
		return 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else
		i = MOTION_RING_READERS;
	if (i == MOTION_RING_READERS)
		goto out;
	process = &motion.processes[i];
	motionRingResetReader(motion.ring, process->reader);
	mode = motion.overrideSampling;
	process->stopped = 0;
	if (mode != 0) {
		mode = 3;
	} else {
		for (i = 0; i < MOTION_RING_READERS; i++) {
			int sampling;

			process = &motion.processes[i];
			if (process->pid == -1 || process->stopped == 1)
				continue;
			sampling = process->sampling;
			if (sampling == 2)
				sampling = 0;
			mode |= sampling;
		}
		if (mode != 1)
			mode = mode == 3 ? 0 : 3;
	}
	if (motion.timer == -1 && motion.state == 2 && mode <= 1)
		motion.timer = ksceKernelRegisterTimer("SceMotionAlarm", motion.period,
			motionSampleTimer, NULL);
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

void motionInitializeComplete(int result, void *arg)
{
	MotionCalibrationBuffer *buffer;
	const SceMotionDevCalibrationData *record;
	unsigned int mode, generation;
	int intr, i;

	(void)arg;
	motionBusy = 0;
	if (result != 0) {
		if (motion.initState == 0) {
			motionBusy = 1;
			if (motionRequestIdentity(&motion.deviceInfo,
				motionIdentityComplete, NULL) >= 0)
				return;
		} else if (motion.initState == 3) {
			if (motionCalibrationBuffer != NULL)
				motionCalibrationBuffer->size = 0;
			goto stage3;
		}
		goto fail;
	}
	switch (motion.initState) {
	case 0:
		buffer = motionAllocHeap(sizeof(*buffer));
		motionCalibrationBuffer = buffer;
		if (buffer == NULL) {
			motionBusy = 0;
			goto failRecords;
		}
		motionBusy = 1;
		if (motionReadDeviceCalibration(0, buffer,
			motionInitializeComplete, NULL) != 0)
			goto fail;
		motion.initState = 1;
		return;
	case 1:
		buffer = motionCalibrationBuffer;
		if (buffer->size == 0)
			goto fail;
		if (motionDecodeCalibrationHeader(buffer->data, buffer->size,
			&motion.calibrationHeaders[0]) < 0 ||
		    (unsigned int)motion.calibrationHeaders[0].recordCount - 1 > 24)
			goto fail;
		motion.calibrationRecords[0] = motionAllocHeap(
			sizeof(SceMotionDevCalibrationData) * motion.calibrationHeaders[0].recordCount);
		if (motion.calibrationRecords[0] == NULL ||
		    motionDecodeCalibrationRecords(buffer->data + 12, buffer->size - 12,
			&motion.calibrationHeaders[0], motion.calibrationRecords[0],
			motion.calibrationHeaders[0].recordCount) < 0)
			goto fail;
		memset(&motion.gyroCalibration, 0, sizeof(motion.gyroCalibration));
		motion.gyroBias.generation = 0;
		generation = 0;
		for (record = motion.calibrationRecords[0], i = 0;
		     i != motion.calibrationHeaders[0].recordCount; i++, record++) {
			unsigned int channel = record->attributes[1];
			float x, y, z, reference;
			MotionCalibPoint *point;
			union { uint32_t integer; float real; } value;

			if (channel > 2 && (channel != 4 || record->attributes[0] != 1))
				continue;
			if (record->attributes[2] != 0) {
				x = (float)record->parameters[1];
				y = (float)record->parameters[2];
				z = (float)record->parameters[3];
			} else {
				value.integer = record->parameters[1];
				x = value.real;
				value.integer = record->parameters[2];
				y = value.real;
				value.integer = record->parameters[3];
				z = value.real;
			}
			if (channel == 4) {
				motion.gyroCalibration.zeroRate[0] = x;
				motion.gyroCalibration.zeroRate[1] = y;
				motion.gyroCalibration.zeroRate[2] = z;
				motion.gyroBias.bias[0] = x;
				generation++;
				motion.gyroBias.bias[1] = y;
				motion.gyroBias.bias[2] = z;
				continue;
			}
			value.integer = record->parameters[0];
			reference = value.real;
			if (record->attributes[0] == 1) {
				point = reference >= 0 ? &motion.gyroCalibration.positive[channel] :
					&motion.gyroCalibration.negative[channel];
			} else if (record->attributes[0] == 0) {
				point = reference >= 0 ? &motion.accCalibration.positive[channel] :
					&motion.accCalibration.negative[channel];
			} else {
				continue;
			}
			point->reference = reference;
			point->value[0] = x;
			point->value[1] = y;
			point->value[2] = z;
		}
		motion.gyroBias.generation = generation;
		initGyroFilter(&motion.filter, &motion.gyroCalibration, 0);
		motionBusy = 1;
		if (motionReadDeviceCalibration(1, motionCalibrationBuffer,
			motionInitializeComplete, NULL) < 0)
			goto fail;
		motion.initState = 2;
		return;
	case 2:
		buffer = motionCalibrationBuffer;
		if (buffer->size == 0)
			goto fail;
		if (motionDecodeCalibrationHeader(buffer->data, buffer->size,
			&motion.calibrationHeaders[1]) < 0 ||
		    (unsigned int)motion.calibrationHeaders[1].recordCount - 1 > 24)
			goto fail;
		motion.calibrationRecords[1] = motionAllocHeap(
			sizeof(SceMotionDevCalibrationData) * motion.calibrationHeaders[1].recordCount);
		if (motion.calibrationRecords[1] == NULL ||
		    motionDecodeCalibrationRecords(buffer->data + 12, buffer->size - 12,
			&motion.calibrationHeaders[1], motion.calibrationRecords[1],
			motion.calibrationHeaders[1].recordCount) < 0)
			goto fail;
		motionInitializeMagnCalibration(motion.calibrationHeaders[1].recordCount,
			motion.calibrationRecords[1], &motion.factoryMagnCalibration);
		motionBusy = 1;
		if (motionReadDeviceCalibration(2, motionCalibrationBuffer,
			motionInitializeComplete, NULL) < 0)
			goto fail;
		motion.initState = 3;
		return;
	case 3:
	stage3:
		buffer = motionCalibrationBuffer;
		if (buffer->size != 0) {
			if (motionDecodeCalibrationHeader(buffer->data, buffer->size,
				&motion.calibrationHeaders[2]) < 0 ||
			    (unsigned int)motion.calibrationHeaders[2].recordCount - 1 > 24 ||
			    (motion.calibrationRecords[2] = motionAllocHeap(
				sizeof(SceMotionDevCalibrationData) * motion.calibrationHeaders[2].recordCount)) == NULL ||
			    motionDecodeCalibrationRecords(buffer->data + 12, buffer->size - 12,
				&motion.calibrationHeaders[2], motion.calibrationRecords[2],
				motion.calibrationHeaders[2].recordCount) < 0)
				motion.calibrationHeaders[2].recordCount = 0;
		}
		if (motionCalibrationBuffer != NULL) {
			motionFreeHeap(motionCalibrationBuffer);
			motionCalibrationBuffer = NULL;
		}
		if (motion.calibrationHeaders[2].recordCount > 3)
			motionInitializeMagnCalibration(motion.calibrationHeaders[2].recordCount,
				motion.calibrationRecords[2], &motion.currentMagnCalibration);
		else
			motionInitializeMagnCalibration(motion.calibrationHeaders[1].recordCount,
				motion.calibrationRecords[1], &motion.currentMagnCalibration);
		mode = 1;
		motionBusy = 1;
		if ((unsigned int)motion.deviceInfo.firmwareVersion - 0x1010 < 0x2f0 &&
		    motion.allowGyroControl != 0)
			mode = motion.deviceInfo.attributes[0] > 0x20 ? 1 : 6;
		if (motionControlDevice(mode, motionInitializeComplete, NULL) < 0)
			goto fail;
		motion.initState = 5;
		return;
	case 5:
		motionBusy = 1;
		if (motionStopDevice(motionInitializeComplete, NULL) < 0)
			goto fail;
		motion.initState = 6;
		return;
	case 6:
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
		if (motion.state == 3) {
			ksceKernelSetEventFlag(motionUpdate.event, 1);
			goto unlock;
		}
		if (motion.state == 5) {
			motion.savedState = 2;
			motion.suspendState |= 0x80000000;
			goto unlock;
		}
		if (motion.state == 4)
			ksceKernelSetEventFlag(motionUpdate.event, 2);
		mode = motion.overrideSampling;
		motion.state = 2;
		if (mode != 0) {
			motion.mode = 3;
			goto unlock;
		}
		for (i = 0; i < MOTION_RING_READERS; i++) {
			MotionDeviceReader *process = &motion.processes[i];
			int sampling;

			if (process->pid == -1 || process->stopped == 1)
				continue;
			sampling = process->sampling;
			if (sampling == 2)
				sampling = 0;
			mode |= sampling;
		}
		if (mode != 1)
			mode = mode == 3 ? 0 : 3;
		motion.mode = mode;
		mode = 0;
		for (i = 0; i < MOTION_RING_READERS; i++) {
			MotionDeviceReader *process = &motion.processes[i];
			int sampling;

			if (process->pid == -1 || process->stopped == 1)
				continue;
			sampling = process->sampling;
			if (sampling == 2)
				sampling = 0;
			mode |= sampling;
		}
		if ((mode & ~2u) == 1 && motion.timer == -1)
			motion.timer = ksceKernelRegisterTimer("SceMotionAlarm", motion.period,
				motionSampleTimer, NULL);
		goto unlock;
	default:
		goto fail;
	}
fail:
	motionBusy = 0;
	if (motionCalibrationBuffer != NULL) {
		motionFreeHeap(motionCalibrationBuffer);
		motionCalibrationBuffer = NULL;
	}
failRecords:
	if (motion.calibrationRecords[0] != NULL) {
		motion.calibrationHeaders[0] = (SceMotionDevCalibrationHeader){0};
		motionFreeHeap(motion.calibrationRecords[0]);
		motion.calibrationRecords[0] = NULL;
	}
	if (motion.calibrationRecords[1] != NULL) {
		motion.calibrationHeaders[1] = (SceMotionDevCalibrationHeader){0};
		motionFreeHeap(motion.calibrationRecords[1]);
		motion.calibrationRecords[1] = NULL;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	switch (motion.state) {
	case 3:
		ksceKernelSetEventFlag(motionUpdate.event, 1);
		break;
	case 4:
		motion.state = 128;
		ksceKernelSetEventFlag(motionUpdate.event, 2);
		break;
	case 5:
		motion.savedState = 128;
		motion.suspendState |= 0x80000000;
		break;
	default:
		motion.state = 128;
		break;
	}
unlock:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
}

int motionSysEvent(int resume, int event, void *arg, void *param)
{
	int intr, i;
	int activeLow;

	(void)arg;
	(void)param;
	if (resume == 0) {
		if (event == 0x401) {
			intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
			if (motion.suspendState == 16) {
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
				return 0;
			}
			if ((int32_t)motion.suspendState < 0) {
				if ((uint8_t)motion.suspendState <= 1) {
					if (motionBusy != 1) {
						motion.suspendState = 0x40000002;
						if (motion.processes[0].pid != -1 && motion.processes[0].sampling != 0)
							motion.processes[0].rebase = 1;
						if (motion.processes[1].pid != -1 && motion.processes[1].sampling != 0)
							motion.processes[1].rebase = 1;
						if (motion.processes[2].pid != -1 && motion.processes[2].sampling != 0)
							motion.processes[2].rebase = 1;
						if (motion.processes[3].pid != -1 && motion.processes[3].sampling != 0)
							motion.processes[3].rebase = 1;
						if (motion.processes[4].pid != -1 && motion.processes[4].sampling != 0)
							motion.processes[4].rebase = 1;
						if (motion.processes[5].pid != -1 && motion.processes[5].sampling != 0)
							motion.processes[5].rebase = 1;
						if (motion.processes[6].pid != -1 && motion.processes[6].sampling != 0)
							motion.processes[6].rebase = 1;
						if (motion.processes[7].pid != -1 && motion.processes[7].sampling != 0)
							motion.processes[7].rebase = 1;
						if (motion.processes[8].pid != -1 && motion.processes[8].sampling != 0)
							motion.processes[8].rebase = 1;
						if (motion.processes[9].pid != -1 && motion.processes[9].sampling != 0)
							motion.processes[9].rebase = 1;
						if (motion.processes[10].pid != -1 && motion.processes[10].sampling != 0)
							motion.processes[10].rebase = 1;
						if (motion.processes[11].pid != -1 && motion.processes[11].sampling != 0)
							motion.processes[11].rebase = 1;
						if (motion.processes[12].pid != -1 && motion.processes[12].sampling != 0)
							motion.processes[12].rebase = 1;
						if (motion.processes[13].pid != -1 && motion.processes[13].sampling != 0)
							motion.processes[13].rebase = 1;
						if (motion.processes[14].pid != -1 && motion.processes[14].sampling != 0)
							motion.processes[14].rebase = 1;
						if (motion.processes[15].pid != -1 && motion.processes[15].sampling != 0)
							motion.processes[15].rebase = 1;
						motionSuspendDeviceAsync(motionSuspendComplete, NULL);
					}
				} else if ((uint8_t)motion.suspendState == 2) {
					motion.suspendState = 16;
				}
			}
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return 0;
		}
		if (event == 0x402)
			return motion.suspendState == 16 ? 0 : -1;
		if (event != 0x400)
			return 0;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
		motion.savedState = motion.state;
		switch (motion.state) {
		case 0:
			motion.suspendState = 0x40000000;
			break;
		case 1:
		case 4:
			motion.suspendState = 0x40000001;
			break;
		default:
			motion.suspendState = 0x40000002;
			if (motion.processes[0].pid != -1 && motion.processes[0].sampling != 0)
				motion.processes[0].rebase = 1;
			if (motion.processes[1].pid != -1 && motion.processes[1].sampling != 0)
				motion.processes[1].rebase = 1;
			if (motion.processes[2].pid != -1 && motion.processes[2].sampling != 0)
				motion.processes[2].rebase = 1;
			if (motion.processes[3].pid != -1 && motion.processes[3].sampling != 0)
				motion.processes[3].rebase = 1;
			if (motion.processes[4].pid != -1 && motion.processes[4].sampling != 0)
				motion.processes[4].rebase = 1;
			if (motion.processes[5].pid != -1 && motion.processes[5].sampling != 0)
				motion.processes[5].rebase = 1;
			if (motion.processes[6].pid != -1 && motion.processes[6].sampling != 0)
				motion.processes[6].rebase = 1;
			if (motion.processes[7].pid != -1 && motion.processes[7].sampling != 0)
				motion.processes[7].rebase = 1;
			if (motion.processes[8].pid != -1 && motion.processes[8].sampling != 0)
				motion.processes[8].rebase = 1;
			if (motion.processes[9].pid != -1 && motion.processes[9].sampling != 0)
				motion.processes[9].rebase = 1;
			if (motion.processes[10].pid != -1 && motion.processes[10].sampling != 0)
				motion.processes[10].rebase = 1;
			if (motion.processes[11].pid != -1 && motion.processes[11].sampling != 0)
				motion.processes[11].rebase = 1;
			if (motion.processes[12].pid != -1 && motion.processes[12].sampling != 0)
				motion.processes[12].rebase = 1;
			if (motion.processes[13].pid != -1 && motion.processes[13].sampling != 0)
				motion.processes[13].rebase = 1;
			if (motion.processes[14].pid != -1 && motion.processes[14].sampling != 0)
				motion.processes[14].rebase = 1;
			if (motion.processes[15].pid != -1 && motion.processes[15].sampling != 0)
				motion.processes[15].rebase = 1;
			motionSuspendDeviceAsync(motionSuspendComplete, NULL);
			break;
		}
		motion.state = 5;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	if (resume != 1)
		return 0;
	if (event != 0x100000) {
		if (event == 0x100002)
			return motion.suspendState == 16 ? 0 : -1;
		return 0;
	}
	motion.state = 6;
	motion.suspendState = 0x40000000;
	activeLow = motion.savedState == 4;
	for (i = 0; i < MOTION_RING_READERS; i++) {
		if (motion.processes[i].pid != -1)
			motionRingResetReader(motion.ring, motion.processes[i].reader);
	}
	motionResumeDeviceAsync(activeLow, motionResumeComplete, NULL);
	return 0;
}

int SceMotionDevForDriver_77BA3A04(void)
{
	return motion.deviceInfo.firmwareVersion < 0x2000;
}

int motionUseWideGyroFilter(void)
{
	return motion.deviceInfo.hardwareInfo >= 0x3000;
}

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	memset(&motion, 0, sizeof(motion));
	memset(&motionSample, 0, sizeof(motionSample));
	motion.period = 8000;
	motion.allowGyroControl = 1;
	motion.lock = 0;
	motion.timer = -1;
	motion.lastSequence = -1;
	motion.owner = -1;
	motion.processes[0].pid = -1;
	motion.processes[0].reader = -1;
	motion.processes[1].pid = -1;
	motion.processes[1].reader = -1;
	motion.processes[2].pid = -1;
	motion.processes[2].reader = -1;
	motion.processes[3].pid = -1;
	motion.processes[3].reader = -1;
	motion.processes[4].pid = -1;
	motion.processes[4].reader = -1;
	motion.processes[5].pid = -1;
	motion.processes[5].reader = -1;
	motion.processes[6].pid = -1;
	motion.processes[6].reader = -1;
	motion.processes[7].pid = -1;
	motion.processes[7].reader = -1;
	motion.processes[8].pid = -1;
	motion.processes[8].reader = -1;
	motion.processes[9].pid = -1;
	motion.processes[9].reader = -1;
	motion.processes[10].pid = -1;
	motion.processes[10].reader = -1;
	motion.processes[11].pid = -1;
	motion.processes[11].reader = -1;
	motion.savedState = 2;
	motion.mode = 255;
	motion.processes[12].pid = -1;
	motion.processes[12].reader = -1;
	motion.processes[13].pid = -1;
	motion.processes[13].reader = -1;
	motion.processes[14].pid = -1;
	motion.processes[14].reader = -1;
	motion.processes[15].pid = -1;
	motion.processes[15].reader = -1;

	if (motionCreateHeap() >= 0) {
		motion.ring = &motionRing;
		motionRingInit(&motionRing, motionRingSamples, motionRingOwners,
			MOTION_RING_SAMPLES);
		clearGyroFilter(&motion.filter);
	}
	ksceKernelEnqueueWorkQueue(0x10023, "SceMotionInit", motionInitializeManager, NULL);
	return 0;
}

int ksceMotionDevIsReady(void)
{
	if ((unsigned int)motion.state - 1 <= 1)
		return 1;
	if (motion.state != 128)
		return 0;
	return (int)0x80360006;
}

int ksceMotionDevGetDeviceInfo(SceMotionDevDeviceInfo *info)
{
	int intr;

	if (info == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if ((unsigned int)motion.state - 1 > 1) {
		if (motion.state != 128) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return (int)0x80360004;
		}
		if (motion.initState == 0) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return (int)0x80360006;
		}
	}
	*info = motion.deviceInfo;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int ksceMotionDevGetCalibrationHeader(unsigned int block, SceMotionDevCalibrationHeader *header)
{
	int intr;

	if (block > 1)
		return (int)0x80360001;
	if (header == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if ((unsigned int)motion.state - 1 <= 1) {
		*header = motion.calibrationHeaders[block];
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	if (motion.state == 128) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360006;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return (int)0x80360004;
}

int ksceMotionDevGetCalibrationData(unsigned int block, SceMotionDevCalibrationData *data,
	unsigned int count)
{
	unsigned int i, available;
	int intr;

	if (block > 1)
		return (int)0x80360001;
	if (data == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if ((unsigned int)motion.state - 1 <= 1) {
		available = motion.calibrationHeaders[block].recordCount;
		if (available >= count)
			available = count;
		for (i = 0; i != available; i++)
			data[i] = motion.calibrationRecords[block][i];
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return available;
	}
	if (motion.state == 128) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360006;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return (int)0x80360004;
}

int ksceMotionDevRead(SceMotionDevData *data, int count, unsigned int *info)
{
	SceUID pid = sceKernelGetProcessId();

	return motionReadSamples(data, count, info, pid, 0);
}

int motionRead(SceMotionDevData *data, int count, unsigned int *info)
{
	SceUID pid = sceKernelGetProcessId();

	return motionReadSamples(data, count, info, pid, 1);
}

int motionReadForMagnCalib(SceMotionDevData *data, int count)
{
	return motionReadSamples(data, count, NULL, 0x10005, 1);
}

int ksceMotionDevSetSamplingMode(unsigned int mode)
{
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if ((unsigned int)motion.state - 1 > 1) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360004;
	}
	if (motion.state != 2) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360003;
	}
	if (motionBusy != 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360003;
	}
	motionBusy = 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return motionControlDevice((uint8_t)mode, motionClearBusy, NULL);
}

int ksceMotionDevSamplingStart(void)
{
	MotionDeviceReader *process;
	SceUID pid;
	unsigned int mode;
	int intr, i;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if ((unsigned int)motion.state - 1 > 1) {
		if (motion.state == 128) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return (int)0x80360006;
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360004;
	}
	pid = sceKernelGetProcessId();
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x803600ff;
	}
	process = &motion.processes[i];
	if ((process->sampling & 1) != 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	motionRingResetReader(motion.ring, process->reader);
	mode = motion.overrideSampling;
	process->sampling |= 1;
	if (mode != 0) {
		mode = 3;
	} else {
		for (i = 0; i < MOTION_RING_READERS; i++) {
			int sampling;

			process = &motion.processes[i];
			if (process->pid == -1 || process->stopped == 1)
				continue;
			sampling = process->sampling;
			if (sampling == 2)
				sampling = 0;
			mode |= sampling;
		}
		if (mode != 1)
			mode = mode == 3 ? 0 : 3;
	}
	if (motion.timer == -1 && motion.state == 2 && mode <= 1)
		motion.timer = ksceKernelRegisterTimer("SceMotionAlarm", motion.period,
			motionSampleTimer, NULL);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int ksceMotionDevSamplingStop(void)
{
	SceUID pid = sceKernelGetProcessId();
	int intr, i;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	motion.processes[i].sampling &= ~1u;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionMagnSamplingStart(void)
{
	MotionDeviceReader *process;
	SceUID pid;
	int intr, i;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if ((unsigned int)motion.state - 1 <= 1) {
		pid = sceKernelGetProcessId();
		if (motion.processes[0].pid == pid)
			i = 0;
		else if (motion.processes[1].pid == pid)
			i = 1;
		else if (motion.processes[2].pid == pid)
			i = 2;
		else if (motion.processes[3].pid == pid)
			i = 3;
		else if (motion.processes[4].pid == pid)
			i = 4;
		else if (motion.processes[5].pid == pid)
			i = 5;
		else if (motion.processes[6].pid == pid)
			i = 6;
		else if (motion.processes[7].pid == pid)
			i = 7;
		else if (motion.processes[8].pid == pid)
			i = 8;
		else if (motion.processes[9].pid == pid)
			i = 9;
		else if (motion.processes[10].pid == pid)
			i = 10;
		else if (motion.processes[11].pid == pid)
			i = 11;
		else if (motion.processes[12].pid == pid)
			i = 12;
		else if (motion.processes[13].pid == pid)
			i = 13;
		else if (motion.processes[14].pid == pid)
			i = 14;
		else if (motion.processes[15].pid == pid)
			i = 15;
		else {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return (int)0x803600ff;
		}
		process = &motion.processes[i];
		if ((process->sampling & 2) == 0)
			process->sampling |= 2;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	if (motion.state == 128) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360006;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return (int)0x80360004;
}

int motionMagnSamplingStop(void)
{
	SceUID pid = sceKernelGetProcessId();
	int intr, i;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.processes[0].pid == pid)
		i = 0;
	else if (motion.processes[1].pid == pid)
		i = 1;
	else if (motion.processes[2].pid == pid)
		i = 2;
	else if (motion.processes[3].pid == pid)
		i = 3;
	else if (motion.processes[4].pid == pid)
		i = 4;
	else if (motion.processes[5].pid == pid)
		i = 5;
	else if (motion.processes[6].pid == pid)
		i = 6;
	else if (motion.processes[7].pid == pid)
		i = 7;
	else if (motion.processes[8].pid == pid)
		i = 8;
	else if (motion.processes[9].pid == pid)
		i = 9;
	else if (motion.processes[10].pid == pid)
		i = 10;
	else if (motion.processes[11].pid == pid)
		i = 11;
	else if (motion.processes[12].pid == pid)
		i = 12;
	else if (motion.processes[13].pid == pid)
		i = 13;
	else if (motion.processes[14].pid == pid)
		i = 14;
	else if (motion.processes[15].pid == pid)
		i = 15;
	else {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	motion.processes[i].sampling &= ~2u;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionUpdateMagnCalibData(const SceMotionDevMagnCalibData *data, uint32_t *tag)
{
	int intr, group, value, axis;

	if (tag == NULL || data == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	for (group = 0; group < 2; group++)
		for (value = 0; value < 2; value++)
			for (axis = 0; axis < 3; axis++)
				motion.currentMagnCalibration.parameters[group][value][axis] =
					data->parameters[group][value][axis];
	motion.currentMagnCalibration.generation++;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	*tag = motion.currentMagnCalibration.generation;
	return 0;
}

int motionGetCurrentMagnCalibData(SceMotionDevMagnCalibData *data)
{
	int intr, group, value, axis;

	if (data == NULL)
		return (int)0x80360001;
	if (motion.currentMagnCalibration.generation == 0)
		return (int)0x80360105;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	data->generation = motion.currentMagnCalibration.generation;
	for (value = 0; value < 2; value++)
		for (group = 0; group < 2; group++)
			for (axis = 0; axis < 3; axis++)
				data->parameters[group][value][axis] =
					motion.currentMagnCalibration.parameters[group][value][axis];
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetFactoryMagnCalibData(SceMotionDevMagnCalibData *data)
{
	int intr, group, value, axis;

	if (data == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	data->generation = motion.factoryMagnCalibration.generation;
	for (value = 0; value < 2; value++)
		for (group = 0; group < 2; group++)
			for (axis = 0; axis < 3; axis++)
				data->parameters[group][value][axis] =
					motion.factoryMagnCalibration.parameters[group][value][axis];
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionUpdateMagnStabilityLevel(uint32_t level)
{
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	motion.magnStabilityLevel = level;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetCurrentMagnStabilityLevel(uint32_t *level)
{
	int intr;

	if (level == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	*level = motion.magnStabilityLevel;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetGyroCalibData(SceMotionDevGyroCalibData *data)
{
	int intr;

	if (data == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	memcpy(data, &motion.gyroCalibration, sizeof(*data));
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetAccCalibData(SceMotionDevAccCalibData *data)
{
	int intr;

	if (data == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	memcpy(data, &motion.accCalibration, sizeof(*data));
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetGyroBias(SceMotionDevGyroBiasData *bias)
{
	int intr;

	if (bias == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	*bias = motion.gyroBias;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetDeviceLocation(SceMotionDevDeviceLocation *location)
{
	int intr;

	if (location == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	*location = motion.deviceLocation;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

uint32_t motionGetMeasMode(uint8_t *mode)
{
	uint32_t result;

	if (mode == NULL)
		return 0x80360001;
	result = motion.mode;
	if (result != 0) {
		if (result == 1)
			*mode = 3;
		else
			*mode = 0;
		return 0;
	}
	*mode = 7;
	return result;
}

int ksceMotionDevNoiseFilterIsAvailable(void)
{
	return (unsigned int)motion.deviceInfo.firmwareVersion - 0x1010 < 0x1f0;
}

int motionGetControllerType(int port, uint8_t *type)
{
	*type = 1;
	return 0;
}

int SceMotionDevForDriver_22CD6DCA(SceUID owner)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	motion.owner = owner;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int SceMotionDevForDriver_3CD0CE14(SceUID *owner)
{
	int intr;

	if (owner == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	*owner = motion.owner;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int SceMotionDevForDriver_0808D77D(unsigned int overrideSampling)
{
	int intr, i;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	motion.overrideSampling = overrideSampling;
	if (overrideSampling == 0) {
		for (i = 0; i < MOTION_RING_READERS; i++) {
			MotionDeviceReader *process = &motion.processes[i];
			int sampling;

			if (process->pid == -1 || process->stopped == 1)
				continue;
			sampling = process->sampling;
			if (sampling == 2)
				sampling = 0;
			overrideSampling |= sampling;
		}
		if (overrideSampling != 1)
			overrideSampling = overrideSampling == 3 ? 0 : 3;
		if (motion.timer == -1 && motion.state == 2 && overrideSampling <= 1)
			motion.timer = ksceKernelRegisterTimer("SceMotionAlarm", motion.period,
				motionSampleTimer, NULL);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int SceMotionDevForDriver_FD1C528D(void)
{
	SceUInt timeout;
	int result, intr, state;

	memset(&motionUpdate, 0, sizeof(motionUpdate));
	result = ksceKernelCreateEventFlag("SceMotionUpdate", 0x1000, 0, NULL);
	motionUpdate.event = result;
	if (result < 0)
		return result;
	SceKernelSuspendForDriver_4DF40893(0);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.timer == -1 && (motion.state == 2 || motion.state == 128))
		motion.state = 4;
	else
		motion.state = 3;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	state = motion.state;
	result = 0;
	if (state == 3) {
		timeout = 5000000;
		result = ksceKernelWaitEventFlag(motionUpdate.event, 1, 5, NULL, &timeout);
		if (result == 0)
			motion.state = 4;
		state = motion.state;
	}
	if (state == 4) {
		motionSetActivePolarity(1);
		motionReleaseReset(1);
		ksceKernelDelayThread(80000);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int SceMotionDevForDriver_D7DA3DA7(void)
{
	if (motionUpdate.event != -1) {
		ksceKernelDeleteEventFlag(motionUpdate.event);
		motionUpdate.event = -1;
	}
	return 0;
}

int SceMotionDevForDriver_A75976EE(void)
{
	SceUInt timeout = 5000000;
	int result;

	SceKernelSuspendForDriver_4DF40893(0);
	if (motion.state != 4) {
		SceKernelSuspendForDriver_2BB92967(0);
		return (int)0x80360002;
	}
	if (motion.calibrationRecords[0] != NULL) {
		motion.calibrationHeaders[0] = (SceMotionDevCalibrationHeader){0};
		motionFreeHeap(motion.calibrationRecords[0]);
		motion.calibrationRecords[0] = NULL;
	}
	if (motion.calibrationRecords[1] != NULL) {
		motion.calibrationHeaders[1] = (SceMotionDevCalibrationHeader){0};
		motionFreeHeap(motion.calibrationRecords[1]);
		motion.calibrationRecords[1] = NULL;
	}
	motion.deviceInfo = (SceMotionDevDeviceInfo){0};
	motionSetActivePolarity(0);
	motionReleaseReset(1);
	ksceKernelRegisterTimer("SceMotionAlarm5", 80000, motionInitializeTimer, NULL);
	result = ksceKernelWaitEventFlag(motionUpdate.event, 2, 5, NULL, &timeout);
	if (result >= 0 && motion.state == 128)
		result = (int)0x80360006;
	if (motionUpdate.event != -1) {
		ksceKernelDeleteEventFlag(motionUpdate.event);
		motionUpdate.event = -1;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int SceMotionDevForDriver_09918DAE(
	void (*completion)(int, const MotionSensorResponse *, MotionSensorResponse *, void *),
	const MotionSensorResponse *request, MotionSensorResponse *response, void *arg)
{
	int intr, result;

	if (request == NULL || completion == NULL || response == NULL)
		return (int)0x80360001;
	if ((int8_t)request->type >= 0)
		return (int)0x80360002;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.state != 4) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360002;
	}
	if (motionBusy != 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360003;
	}
	if ((uint8_t)(request->type + 123) <= 2 &&
	    (motion.deviceInfo.hardwareInfo >> 8) == 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360104;
	}
	motionUpdate.response = response;
	motionUpdate.arg = arg;
	motionUpdate.completion = completion;
	motionBusy = 1;
	motionUpdate.request = request;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	result = motionRequestResponse(request, response, motionResponseComplete, NULL);
	if (result != 0)
		motionBusy = 0;
	return result;
}

int SceMotionDevForDriver_DB89D1BF(
	void (*completion)(int, const MotionSensorPacket *, MotionSensorPacket *, void *),
	const MotionSensorPacket *request, MotionSensorPacket *response, void *arg)
{
	int intr, result;

	if (request == NULL || completion == NULL || response == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.state != 2 || motionPacket.completion != NULL || motionBusy != 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return (int)0x80360003;
	}
	motionPacket.completion = completion;
	motionPacket.request = request;
	motionPacket.response = response;
	motionPacket.arg = arg;
	motionBusy = 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	result = motionRequestPacket(motionPacket.request, motionPacket.response,
		motionPacketComplete, NULL);
	if (result != 0)
		motionBusy = 0;
	return result;
}

int SceMotionDevForDriver_FE6ECA41(uint32_t *available)
{
	if (available == NULL)
		return (int)0x80360001;
	if ((unsigned int)motion.state - 1 > 1)
		return (int)0x80360004;
	*available = 1;
	return 0;
}

int SceMotionDevForDriver_85E3C678(void)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	if (motion.state == 2) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return motionPowerOn(0);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return (int)0x80360003;
}

int SceMotionDevForDriver_031AF7A1(void)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	if (motion.state == 2) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return motionSuspendDevice();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return (int)0x80360003;
}

int ksceMotionDevGetEvaInfo(void *statistics)
{
	int intr, result;

	if (statistics == NULL)
		return (int)0x80360001;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	result = motionGetDeviceStatistics(statistics);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return result;
}

int ksceMotionDevRegisterVirtualMotionDriver(const struct MotionVirtualDriver *driver)
{
	return 0;
}
