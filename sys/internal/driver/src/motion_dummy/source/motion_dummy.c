/*
 * SceMotionDevDummy: processes, the sampling timer and the kernel interface.
 */
#include <string.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/proc_event.h>
#include <sce/sysevent.h>
#include <sce/motion_dummy.h>
#include "fw.h"
#include "motion_dummy.h"

/* 4.00 is built with _FORTIFY_SOURCE: memset into an object of known size is checked */
#if FW_SINCE(0x04000000)
#undef memset
#define memset(dst, c, n) __builtin___memset_chk(dst, c, n, __builtin_object_size(dst, 0))
#endif

#define SAMPLING_PERIOD		8000	/* microseconds */

/* SceKernelSuspendForDriver sys events */
#define SYS_EVENT_SUSPEND	0x400
#define SYS_EVENT_RESUME	0x100000

/* SceProcEventForDriver stop and start events */
#define PROC_EVENT_STOP		0x800
#define PROC_EVENT_START	0x10000

typedef struct MotionRingStorage {
	MotionRing ring;
	SceMotionDevData samples[MOTION_RING_SAMPLES];
	SceUID owners[MOTION_RING_SAMPLES];
} MotionRingStorage;

static MotionRingStorage rings[MOTION_PORT_MAX];
static SceMotionDevData readBuffer[MOTION_RING_SAMPLES];
static MotionState motion;
static MotionDriverSample driverSample;

static inline int findProcess(SceUID pid)
{
	int i;

	for (i = 0; i < MOTION_PROCESS_MAX; i++)
		if (motion.processes[i].pid == pid)
			return i;
	return -1;
}

static inline void registerTimer(void);
static int sampleTimer(void *argp);

/* fetch the calibration of the controller now on @port */
static int updateCalibration(int port)
{
	int ret = 0;

	if (motion.driver == NULL)
		return 0;
	motion.driver->getControllerType(port, &motion.controllers[port - 1].type);
	if (motion.controllers[port - 1].type == MOTION_CONTROLLER_DS3) {
		ret = motion.driver->getDs3CalibData(port,
						     &motion.controllers[port - 1].ds3Calibration);
	} else if (motion.controllers[port - 1].type == MOTION_CONTROLLER_DS4) {
		ret = motion.driver->getGyroCalibData(port,
						      &motion.controllers[port - 1].gyroCalibration);
		if (ret < 0)
			return ret;
		ret = motion.driver->getAccCalibData(port,
						     &motion.controllers[port - 1].accCalibration);
		if (ret < 0)
			return ret;
		motion.controllers[port - 1].gyroBias.generation++;
		motion.controllers[port - 1].gyroBias.bias[0] =
			motion.controllers[port - 1].gyroCalibration.zeroRate[0];
		motion.controllers[port - 1].gyroBias.bias[1] =
			motion.controllers[port - 1].gyroCalibration.zeroRate[1];
		motion.controllers[port - 1].gyroBias.bias[2] =
			motion.controllers[port - 1].gyroCalibration.zeroRate[2];
	}
	return ret;
}

static int stopProcess(SceUID pid, int event, SceProcEventInvokeParam1 *param, int flags)
{
	MotionProcess *process;
	int intr, i;

	if (event != PROC_EVENT_STOP)
		return 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	i = findProcess(pid);
	if (i < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	process = &motion.processes[i];
	process->stopped = 1;
	process->rebase = process->sampling != 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionReadPort(int port, SceMotionDevData *data, int count, SceUInt32 *info)
{
	SceUID pid = sceKernelGetProcessId();
	MotionProcess *process;
	MotionRing *ring;
	SceMotionDevData *sample, *dest;
	SceUID *owner;
	SceUInt32 zero;
	unsigned int reader;
	int intr, i, j, k, n, skip, ret;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX || data == NULL ||
	    (unsigned int)(count - 1) >= MOTION_RING_SAMPLES)
		return MOTION_ERROR_INVALID_ARG;
	zero = 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (info != NULL && ksceKernelCopyToUser(info, &zero, sizeof(zero)) < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	if (motion.state != MOTION_STATE_SAMPLING) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	i = findProcess(pid);
	if (i < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return MOTION_ERROR_NOT_FOUND;
	}
	process = &motion.processes[i];
	if (process->stopped) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	ring = motion.controllers[port - 1].ring;
	reader = process->readers[port - 1];
	n = motionRingCount(ring, reader);
	if (count < n) {
		skip = n - count;
		n = count;
	} else if (n > 0) {
		skip = 0;
	} else {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	for (j = 0; j < n; j++) {
		unsigned int wraps;

		ret = motionRingPeek(ring, reader, skip + j, &sample, &owner);
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
		dest = &readBuffer[j];
		memcpy(dest, sample, sizeof(*dest));
		if (*owner != pid)
			dest->flags |= MOTION_DATA_OTHER_OWNER;
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
		dest = &readBuffer[j - 1];
		if (dest->count)
			process->lastTimestamp = dest->entry[dest->count - 1].timestamp;
	}
	motionRingConsume(ring, reader);
	if (n > 0) {
		ret = ksceKernelCopyToUser(data, readBuffer, n * sizeof(*readBuffer));
		if (ret < 0)
			n = ret;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return n;
}

/* the timer handler: one sample of every sampling port; returns the next period */
static int sampleTimer(void *argp)
{
	SceMotionDevData *sample = NULL;
	SceUID *owner;
	float bias[3], delta[3];
	unsigned int period = motion.period;
	int intr, active = 0, i, port;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (motion.state != MOTION_STATE_STOPPED)
		for (i = 0; i < MOTION_PROCESS_MAX; i++)
			if (motion.processes[i].pid != -1 && !motion.processes[i].stopped &&
			    motion.processes[i].sampling == 1)
				active = 1;
	if (!active) {
		period = 0;
		motion.timer = -1;
	} else if (motion.driver != NULL) {
		for (port = 1; port <= MOTION_PORT_MAX; port++) {
			MotionController *controller = &motion.controllers[port - 1];

			if (!((motion.activePorts >> port) & 1))
				continue;
			motion.driver->getControllerType(port, &controller->type);
			if (controller->type == 0)
				continue;
			if (motion.driver->getSample(port, &driverSample) < 0)
				continue;
			if (driverSample.timestamp == controller->timestamp)
				continue;
			if (controller->type == MOTION_CONTROLLER_DS3) {
				MotionDs3Report *report = &controller->ds3Report;

				if (controller->identity != driverSample.identity) {
					controller->identity = driverSample.identity;
					updateCalibration(port);
					if (driverSample.identity != 0)
						controller->generation++;
				}
				report->generation = controller->generation & 0xf;
				report->accel[0] = driverSample.accel[0];
				report->accel[1] = driverSample.accel[1];
				report->accel[2] = driverSample.accel[2];
				report->gyro = driverSample.gyro[0];
			} else if (controller->type == MOTION_CONTROLLER_DS4) {
				SceMotionDevDataEntry *entry;

				motionRingGetWriteSlot(controller->ring, &sample, &owner);
				if (controller->identity != driverSample.identity) {
					controller->identity = driverSample.identity;
					updateCalibration(port);
					initGyroFilter(&controller->filter, &controller->gyroCalibration, 1);
					if (driverSample.identity != 0)
						controller->generation++;
				}
				sample->time = sceKernelGetSystemTimeWide();
				sample->count = 1;
				sample->generation = controller->generation & 0xf;
				entry = &sample->entry[0];
				entry->timestamp = driverSample.timestamp;
				entry->sequence = controller->sequence++;
				entry->flags = MOTION_ENTRY_GYRO | MOTION_ENTRY_ACCEL | MOTION_ENTRY_STATUS;
				/* the driver's values are unsigned, centred on 0x8000 */
				entry->gyro[0] = driverSample.gyro[0] - 0x8000;
				entry->gyro[1] = driverSample.gyro[1] - 0x8000;
				entry->gyro[2] = driverSample.gyro[2] - 0x8000;
				entry->accel[0] = driverSample.accel[0] - 0x8000;
				entry->accel[1] = driverSample.accel[1] - 0x8000;
				entry->accel[2] = driverSample.accel[2] - 0x8000;
				entry->status = driverSample.status;
				*owner = motion.owner;
				for (i = 0; i < sample->count; i++) {
					if (!(sample->entry[i].flags & MOTION_ENTRY_GYRO))
						continue;
					if (updateGyroFilter(&controller->filter, sample->entry[i].gyro,
							     bias, delta)) {
						controller->gyroBias.bias[2] = bias[2];
						controller->gyroBias.bias[0] = bias[0];
						controller->gyroBias.bias[1] = bias[1];
						controller->gyroBias.generation++;
					}
				}
				sample->biasGeneration = controller->gyroBias.generation;
				controller->timestamp = sample->entry[0].timestamp;
				motionRingAdvance(controller->ring);
			}
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return period;
}

static inline void removeProcess(SceUID pid)
{
	MotionProcess *process;
	int intr, i, port;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	i = findProcess(pid);
	if (i < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return;
	}
	process = &motion.processes[i];
	for (port = 0; port < MOTION_PORT_MAX; port++)
		motionRingCloseReader(motion.controllers[port].ring, process->readers[port]);
	if (motion.owner == process->pid)
		motion.owner = -1;
	process->pid = -1;
	process->readers[0] = -1;
	process->readers[1] = -1;
	process->stopped = 0;
	process->sampling = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
}

static int exitProcess(SceUID pid, SceProcEventInvokeParam1 *param, int flags)
{
	removeProcess(pid);
	return 0;
}

static int sysEvent(int resume, int eventid, void *args, void *opt)
{
	int intr, i, port;

	if (resume == 0) {
		if (eventid == SYS_EVENT_SUSPEND) {
			intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
			motion.savedState = motion.state;
			motion.state = MOTION_STATE_SUSPENDED;
			for (i = 0; i < MOTION_PROCESS_MAX; i++)
				if (motion.processes[i].pid != -1)
					motion.processes[i].rebase = 1;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		}
	} else if (resume == 1 && eventid == SYS_EVENT_RESUME) {
		motion.state = MOTION_STATE_RESUMING;
		for (i = 0; i < MOTION_PROCESS_MAX; i++)
			if (motion.processes[i].pid != -1)
				for (port = 0; port < MOTION_PORT_MAX; port++)
					motionRingResetReader(motion.controllers[port].ring,
							      motion.processes[i].readers[port]);
		motion.state = motion.savedState;
		registerTimer();
	}
	return 0;
}

static int startProcess(SceUID pid, int event, SceProcEventInvokeParam1 *param, int flags)
{
	MotionProcess *process;
	int intr, i, port;

	if (event != PROC_EVENT_START)
		return 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	i = findProcess(pid);
	if (i < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	process = &motion.processes[i];
	for (port = 0; port < MOTION_PORT_MAX; port++)
		motionRingResetReader(motion.controllers[port].ring, process->readers[port]);
	process->stopped = 0;
	registerTimer();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

static inline int addProcess(SceUID pid)
{
	MotionProcess *process;
	int intr, i, port;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	i = findProcess(-1);
	if (i < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return MOTION_ERROR_NO_RESOURCE;
	}
	process = &motion.processes[i];
	memset(process, 0, sizeof(*process));
	process->pid = pid;
	for (port = 0; port < MOTION_PORT_MAX; port++)
		process->readers[port] = motionRingOpenReader(motion.controllers[port].ring);
	process->stopped = 0;
	process->sampling = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

static int createProcess(SceUID pid, SceProcEventInvokeParam2 *param, int flags)
{
	return addProcess(pid);
}

static int killProcess(SceUID pid, SceProcEventInvokeParam1 *param, int flags)
{
	removeProcess(pid);
	return 0;
}

static inline void registerTimer(void)
{
	if (motion.timer == -1)
		motion.timer = ksceKernelRegisterTimer("SceMotionAlarm", motion.period,
						       sampleTimer, NULL);
}

int motionRead2(int port, SceMotionDevData *data, int count, SceUInt32 *info)
{
	return motionReadPort(port, data, count, info);
}

int motionRead3(int port, MotionDs3Report *report, int count, SceUInt32 *info)
{
	SceUID pid = sceKernelGetProcessId();
	SceUInt32 zero;
	int intr, i, ret;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX || report == NULL ||
	    (unsigned int)(count - 1) >= MOTION_RING_SAMPLES)
		return MOTION_ERROR_INVALID_ARG;
	zero = 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	if (info != NULL && ksceKernelCopyToUser(info, &zero, sizeof(zero)) < 0) {
		ret = 0;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	} else if (motion.state != MOTION_STATE_SAMPLING) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		ret = 0;
	} else {
		i = findProcess(pid);
		if (i < 0) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return MOTION_ERROR_NOT_FOUND;
		}
		if (motion.processes[i].stopped) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			ret = 0;
		} else {
			ret = ksceKernelCopyToUser(report, &motion.controllers[port - 1].ds3Report,
						   sizeof(*report));
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			if (ret >= 0)
				ret = 1;
		}
	}
	return ret;
}

int SceMotionDevForDriver_77BA3A04(void)
{
	return 0;
}

int motionUseWideGyroFilter(void)
{
	return 0;
}

static const SceProcEventHandler processEventHandler = {
	.size = sizeof(SceProcEventHandler),
	.create = createProcess,
	.exit = exitProcess,
	.kill = killProcess,
	.stop = stopProcess,
	.start = startProcess,
};

int module_start(SceSize argc, const void *args)
{
	int i, port;

	memset(&motion, 0, sizeof(motion));
	driverSample = (MotionDriverSample){ 0 };
	motion.savedState = MOTION_STATE_STOPPED;
	motion.period = SAMPLING_PERIOD;
	motion.lock = 0;
	motion.timer = -1;
	motion.owner = -1;
	for (i = 0; i < MOTION_PROCESS_MAX; i++) {
		motion.processes[i].pid = -1;
		motion.processes[i].readers[0] = -1;
		motion.processes[i].readers[1] = -1;
	}
	for (port = 0; port < MOTION_PORT_MAX; port++) {
		motion.controllers[port].ring = &rings[port].ring;
		motionRingInit(&rings[port].ring, rings[port].samples, rings[port].owners,
			       MOTION_RING_SAMPLES);
		clearGyroFilter(&motion.controllers[port].filter);
	}
	motion.sysEventHandler = ksceKernelRegisterSysEventHandler("SceMotion", sysEvent, NULL);
	motion.procEventHandler = ksceKernelRegisterProcEventHandler("SceMotion",
								     &processEventHandler, 0);
	addProcess(sceKernelGetProcessId());
	return SCE_KERNEL_START_SUCCESS;
}

int ksceMotionDevIsReady(void)
{
	return 1;
}

int ksceMotionDevGetDeviceInfo(SceMotionDevDeviceInfo *info)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(info, 0, MOTION_DEVICE_INFO_SIZE);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int ksceMotionDevGetCalibrationHeader(SceUInt32 blockId, uint8_t *header)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	int i;

	for (i = 0; i < MOTION_CALIBRATION_HEADER_SIZE; i++)
		header[i] = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int ksceMotionDevGetCalibrationData(SceUInt32 blockId, void *data, SceUInt32 count)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(data, 0, count * MOTION_CALIBRATION_DATA_SIZE);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int ksceMotionDevRead(SceMotionDevData *data, int count, SceUInt32 *info)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(data, 0, count * sizeof(*data));
	*info = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return count;
}

int ksceMotionDevSetSamplingMode(SceUInt32 mode)
{
	return 0;
}

/* the built-in sensors: always zero */
int motionRead(SceMotionDevData *data, int count, SceUInt32 *info)
{
	SceSize size = count * sizeof(*data);
	SceUInt32 zero;
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	memset(readBuffer, 0, size);
	if (ksceKernelCopyToUser(data, readBuffer, size) >= 0) {
		zero = 0;
		if (ksceKernelCopyToUser(info, &zero, sizeof(zero)) >= 0) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
			return count;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int ksceMotionDevSamplingStart(void)
{
	return 0;
}

int motionReadForMagnCalib(SceMotionDevData *data, int count)
{
	SceSize size = count * sizeof(*data);
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	memset(readBuffer, 0, size);
	if (ksceKernelCopyToUser(data, readBuffer, size) < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return count;
}

int ksceMotionDevSamplingStop(void)
{
	return 0;
}

int motionSamplingStart2(int port)
{
	MotionProcess *process;
	int intr, i;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX)
		return MOTION_ERROR_INVALID_ARG;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	i = findProcess(sceKernelGetProcessId());
	if (i < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return MOTION_ERROR_NOT_FOUND;
	}
	motion.activePorts |= 1 << port;
	process = &motion.processes[i];
	process->sampling = 1;
	resetGyroFilter(&motion.controllers[port - 1].filter);
	motion.state = MOTION_STATE_SAMPLING;
	motionRingResetReader(motion.controllers[port - 1].ring, process->readers[port - 1]);
	updateCalibration(port);
	registerTimer();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionSamplingStop2(int port)
{
	int intr, i;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX)
		return MOTION_ERROR_INVALID_ARG;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	i = findProcess(sceKernelGetProcessId());
	if (i < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
		return 0;
	}
	motion.activePorts &= ~(1 << port);
	if (motion.activePorts == 0) {
		motion.processes[i].sampling = 0;
		motion.state = MOTION_STATE_STOPPED;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionMagnSamplingStart(void)
{
	return 0;
}

int motionMagnSamplingStop(void)
{
	return 0;
}

int motionUpdateMagnCalibData(const void *data, SceUInt32 *tag)
{
	*tag = 0;
	return 0;
}

int motionGetCurrentMagnCalibData(void *data)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(data, 0, MOTION_MAGN_CALIB_DATA_SIZE);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetFactoryMagnCalibData(void *data)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(data, 0, MOTION_MAGN_CALIB_DATA_SIZE);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionUpdateMagnStabilityLevel(void)
{
	return 0;
}

int motionGetCurrentMagnStabilityLevel(SceUInt32 *level)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	uint8_t *p = (uint8_t *)level;
	int i;

	for (i = 0; i < sizeof(*level); i++)
		*p++ = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

/* an ideal sensor: unit output for unit input on each axis */
int motionGetGyroCalibData(SceMotionDevGyroCalibData *data)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(data, 0, sizeof(*data));
	data->positive[0].value[0] = 1.0f;
	data->positive[1].value[1] = 1.0f;
	data->positive[2].value[2] = 1.0f;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetAccCalibData(SceMotionDevAccCalibData *data)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(data, 0, sizeof(*data));
	data->positive[0].value[0] = 1.0f;
	data->positive[1].value[1] = 1.0f;
	data->positive[2].value[2] = 1.0f;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetGyroBias(SceMotionDevGyroBiasData *bias)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(bias, 0, sizeof(*bias));
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetDeviceLocation(void *location)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	memset(location, 0, MOTION_DEVICE_LOCATION_SIZE);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetMeasMode(void *info)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	uint8_t *p = info;
	int i;

	for (i = 0; i < MOTION_MODE_INFO_SIZE; i++)
		p[i] = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int ksceMotionDevNoiseFilterIsAvailable(void)
{
	return 0;
}

int motionGetControllerType(int port, uint8_t *type)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	*type = motion.controllers[port - 1].type;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionSetGyroFeedBack(int port, int feedback)
{
	int intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);

	motion.driver->setGyroFeedBack(port, feedback);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
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
		return MOTION_ERROR_INVALID_ARG;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	*owner = motion.owner;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int SceMotionDevForDriver_0808D77D(void)
{
	return 0;
}

int SceMotionDevForDriver_FD1C528D(void)
{
	return 0;
}

int SceMotionDevForDriver_D7DA3DA7(void)
{
	return 0;
}

int SceMotionDevForDriver_A75976EE(void)
{
	return 0;
}

int SceMotionDevForDriver_09918DAE(void)
{
	return 0;
}

int SceMotionDevForDriver_DB89D1BF(void)
{
	return 0;
}

int SceMotionDevForDriver_FE6ECA41(void)
{
	return 0;
}

int SceMotionDevForDriver_85E3C678(void)
{
	return 0;
}

int SceMotionDevForDriver_031AF7A1(void)
{
	return 0;
}

int ksceMotionDevGetEvaInfo(void)
{
	return 0;
}

int ksceMotionDevRegisterVirtualMotionDriver(const MotionVirtualDriver *driver)
{
	if (driver == NULL)
		return MOTION_ERROR_INVALID_ARG;
	motion.driver = driver;
	return 0;
}

int motionGetGyroCalibData2(int port, SceMotionDevGyroCalibData *data)
{
	int intr;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX || data == NULL)
		return MOTION_ERROR_INVALID_ARG;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	memcpy(data, &motion.controllers[port - 1].gyroCalibration, sizeof(*data));
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetAccCalibData2(int port, SceMotionDevAccCalibData *data)
{
	int intr;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX || data == NULL)
		return MOTION_ERROR_INVALID_ARG;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	memcpy(data, &motion.controllers[port - 1].accCalibration, sizeof(*data));
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetGyroBias2(int port, SceMotionDevGyroBiasData *bias)
{
	int intr;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX || bias == NULL)
		return MOTION_ERROR_INVALID_ARG;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	*bias = motion.controllers[port - 1].gyroBias;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}

int motionGetDs3CalibData(int port, SceMotionDevDs3CalibData *data)
{
	int intr;

	if ((unsigned int)(port - 1) >= MOTION_PORT_MAX || data == NULL)
		return MOTION_ERROR_INVALID_ARG;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&motion.lock);
	*data = motion.controllers[port - 1].ds3Calibration;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&motion.lock, intr);
	return 0;
}
