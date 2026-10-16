/* SceCamera driver initialization and control. */

#include "skeleton.h"
#include "sce/sysmem.h"
#include "camera_config.h"
#include "sce/intrmgr.h"
#include "sce/sysevent.h"
#include "sce/power.h"
#include "sce/clockgen.h"
#include "sce/csi.h"
#include "sce/idstorage.h"
#include "sce/processmgr.h"
#include <psp2kern/power.h>
#include <string.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2/camera.h>
#include "sce/camera.h"
#include "sce/iftu.h"
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/syscon.h>
#include <psp2kern/lowio/gpio.h>
#include <sce/compat_regs.h>

int cameraInitializeConfig(void);
int sceCameraConfigOpen(int devnum, SceCameraInfo *pInfo);
int cameraResetConfig(void);
SceUID cameraGetCallerProcessId(void);
int cameraSetProcessRangesLocked(int devnum, SceUID processId, int locked);
int cameraAdvanceProcessFrame(int devnum, SceUID processId);
int cameraIsProcessActive(int devnum, SceUID processId);
int cameraGetProcessSuspendState(int devnum, SceUID processId, int *state);
int cameraSetProcessSuspendState(int devnum, SceUID processId, int state);
int cameraSetProcessRequestedSuspendState(int devnum, SceUID processId, int state);
CameraProcess *cameraGetProcessStorage(SceUID processId, int create);
int cameraInitializeProcess(SceUID processId);
int cameraHasStartedProcess(int devnum, int activeOnly);
int cameraIsProcessStarted(int devnum, SceUID processId);
int cameraRestoreProcessSensorRegisters(int devnum);
int sceCameraConfigStopPid(int devnum, SceUID processId);
int sceCameraConfigSetStartedPid(int devnum, SceUID processId, int started);
CameraProcess *cameraGetProcess(int devnum, SceUID processId);
int sceCameraConfigClosePid(int devnum, SceUID processId);
int cameraHasOtherProcess(int devnum, SceUID processId);
int cameraHasOpenProcess(int devnum);
int cameraUnregisterProcess(SceUID processId);
int cameraRegisterProcess(SceUID processId);
int cameraGetProcessDeviceStatus(SceUID processId);
int cameraSetProcessState(SceUID processId, int devnum, int state);
int cameraGetProcessRequestedSuspendState(int devnum, SceUID processId, int *state);
int cameraGetProcessAlternateFrameBuffers(int devnum, SceUID processId, int *alternate);
int sceCameraConfigStartPid(int devnum, SceUID processId, int force);
int sceCameraConfigSetActiveStatusPid(int devnum, SceUID processId, int status);
int sceCameraConfigAttribute(int devnum, int checkFormat);
int cameraCheckActiveProcess(SceUID processId, int devnum, int currentProcess);
int sceCameraConfigGetAttribute(int devnum, int attribute, int *pValue);
int sceCameraConfigRead(int devnum, SceCameraReadV2 *pRead);
SceUInt64 sceCameraConfigGetProcPrevFrame(int devnum, SceUID processId);
SceUInt64 sceCameraConfigGetProcFrame(int devnum, SceUID processId);
SceUInt64 sceCameraConfigGetProcTimestamp(int devnum, SceUID processId);
int sceCameraConfigSetProcPrevFrame(int devnum, SceUID processId, SceUInt64 frame);
int sceCameraConfigSetAttribute(int devnum, int attribute, int value, int flags, int updateSensor);
int cameraSetProcessAlternateFrameBuffers(int devnum, SceUID processId, int alternate);
int cameraSubIntrHandler(void *subintrArg, void *registerArg, unsigned char intrPriority);
int cameraDriverThread(SceSize args, void *argp);
int cameraSysEventHandler(int resume, int eventid, void *args, void *opt);
int cameraProcessCreateEvent(SceUID pid, SceProcEventInvokeParam2 *param, int arg);
int cameraProcessExitEvent(SceUID pid, SceProcEventInvokeParam1 *param, int arg);
int cameraProcessKillEvent(SceUID pid, SceProcEventInvokeParam1 *param, int arg);
int cameraProcessStopEvent(SceUID pid, int eventType, SceProcEventInvokeParam1 *param, int arg);
int cameraProcessStartEvent(SceUID pid, int eventType, SceProcEventInvokeParam1 *param, int arg);

int cameraInitializeOV7735(int devnum);
int cameraResetOV7735(int devnum);
int cameraSetFormatOV7735(int devnum, int format);
int cameraSetResolutionOV7735(int devnum, int resolution, int framerate);
int cameraSetFramerateOV7735(int devnum, int framerate, int resolution);
int cameraSetRangeOV7735(int devnum, int range);
int cameraGetAttributeOV7735(int devnum, int attribute, int *value, int flags, void *option);
int cameraSetAttributeOV7735(int devnum, int attribute, int value, int flags, void *option);
int cameraWriteRegisterOV7735(int devnum, SceUInt16 address, SceUInt8 value);
int cameraReadRegisterOV7735(int devnum, SceUInt16 address, SceUInt8 *value);
int cameraWriteRegistersOV7735(int devnum, const void *registers, unsigned int count);
int cameraSetPowerStateOV7735(int devnum, int enabled);
int cameraGetCsiFormatOV7735(int devnum, int framerate, int format);
int sceCameraGetExposureTimeOV7735(int devnum, int framerate, unsigned int *exposure);
int cameraGetRaw8FormatOV7735(int devnum, int format, unsigned int *value);
int cameraHasColorChangeOV7735(int devnum);
int cameraRecoverOV7735(int devnum);
int cameraGetSensorRegistersOV7735(int devnum, SceUInt8 *values, int defaults);
int cameraSetSensorRegistersOV7735(int devnum, const SceUInt8 *values);
int cameraSetControlModeOV7735(int devnum, int command, int mode);
int cameraGetProbeResultOV7735(int devnum, int command, SceUInt16 *result);
int cameraProbeOV7735(int devnum);

CameraDriver *cameraDriver = &cameraDriverStorage;
SceUID cameraSramMemBlockId = -1;
SceUID cameraFrameBufferMemBlockId = -1;

static const SceIftuCscParams cameraRgbToYuvCsc[4] = {
	{
		.post_add_0 = 0x40,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3af,
		.post_clamp_min_0 = 0x40,
		.post_clamp_max_1_2 = 0x3c3,
		.post_clamp_min_1_2 = 0x40,
		.ctm = {{0x83, 0x102, 0x32}, {0xfb4, 0xf6c, 0xe0}, {0xe0, 0xf44, 0xfdc}},
	},
	{
		.post_add_0 = 0x40,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3af,
		.post_clamp_min_0 = 0x40,
		.post_clamp_max_1_2 = 0x3c3,
		.post_clamp_min_1_2 = 0x40,
		.ctm = {{0x5d, 0x13a, 0x1f}, {0xfcd, 0xf53, 0xe0}, {0xe0, 0xf34, 0xfec}},
	},
	{
		.post_add_0 = 0x0,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3ff,
		.post_clamp_min_0 = 0x0,
		.post_clamp_max_1_2 = 0x3ff,
		.post_clamp_min_1_2 = 0x0,
		.ctm = {{0x99, 0x12c, 0x3a}, {0xfaa, 0xf57, 0x100}, {0x100, 0xf2a, 0xfd7}},
	},
	{
		.post_add_0 = 0x0,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3ff,
		.post_clamp_min_0 = 0x0,
		.post_clamp_max_1_2 = 0x3ff,
		.post_clamp_min_1_2 = 0x0,
		.ctm = {{0x6c, 0x16e, 0x24}, {0xfc6, 0xf3b, 0x100}, {0x100, 0xf18, 0xfe9}},
	},
};

static const SceIftuCscParams cameraYuvToRgbCsc[4] = {
	{
		.post_add_0 = 0x40,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3ff,
		.post_clamp_min_0 = 0x0,
		.post_clamp_max_1_2 = 0x0,
		.post_clamp_min_1_2 = 0x0,
		.ctm = {{0x254, 0x0, 0x331}, {0x254, 0xf38, 0xe60}, {0x254, 0x408, 0x0}},
	},
	{
		.post_add_0 = 0x40,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3ff,
		.post_clamp_min_0 = 0x0,
		.post_clamp_max_1_2 = 0x0,
		.post_clamp_min_1_2 = 0x0,
		.ctm = {{0x254, 0x0, 0x395}, {0x254, 0xf93, 0xef0}, {0x254, 0x439, 0x0}},
	},
	{
		.post_add_0 = 0x0,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3ff,
		.post_clamp_min_0 = 0x0,
		.post_clamp_max_1_2 = 0x0,
		.post_clamp_min_1_2 = 0x0,
		.ctm = {{0x200, 0x0, 0x2cd}, {0x200, 0xf50, 0xe93}, {0x200, 0x38b, 0x0}},
	},
	{
		.post_add_0 = 0x0,
		.post_add_1_2 = 0x202,
		.post_clamp_max_0 = 0x3ff,
		.post_clamp_min_0 = 0x0,
		.post_clamp_max_1_2 = 0x0,
		.post_clamp_min_1_2 = 0x0,
		.ctm = {{0x200, 0x0, 0x326}, {0x200, 0xfa1, 0xf11}, {0x200, 0x3b6, 0x0}},
	},
};

static const SceProcEventHandler cameraProcessEventHandler = {
	.size = sizeof(SceProcEventHandler),
	.create = cameraProcessCreateEvent,
	.exit = cameraProcessExitEvent,
	.kill = cameraProcessKillEvent,
	.stop = cameraProcessStopEvent,
	.start = cameraProcessStartEvent,
	.switch_process = NULL,
};

int sce_camera_init(void)
{
	CameraDriver *driver;
	SceUInt32 hardwareInfo;
	int result;

	if (cameraDriver->initialized)
		return (int)0x802e0001;
	cameraInitializeConfig();
	driver = cameraDriver;
	driver->spinlock = 0;
	driver->hardwareInfo = ksceSysconGetHardwareInfo();
	driver->device[0].mutexId = ksceKernelCreateMutex("SceCameraDev0", 0, 0, NULL);
	cameraDriver->device[1].mutexId = ksceKernelCreateMutex("SceCameraDev1", 0, 0, NULL);
	hardwareInfo = cameraDriver->hardwareInfo;
	if ((hardwareInfo & 0xdf0000) == 0x100000 ||
	    (hardwareInfo & 0xff0000) == 0x310000 ||
	    (hardwareInfo & 0xfe0000) == 0x400000) {
		ksceKernelRegisterSubIntrHandler(166, 0, "SceCif0Camera",
				cameraSubIntrHandler, &cameraDriver->device[1]);
		ksceKernelRegisterSubIntrHandler(168, 0, "SceCif1Camera",
				cameraSubIntrHandler, &cameraDriver->device[0]);
		cameraDriver->device[0].eventFlagId = ksceKernelCreateEventFlag(
				"SceCif1", 0x1000, 0, &cameraDriver->device[0].eventFlagOption);
		cameraDriver->device[1].eventFlagId = ksceKernelCreateEventFlag(
				"SceCif0", 0x1000, 0, &cameraDriver->device[1].eventFlagOption);
	} else {
		ksceKernelRegisterSubIntrHandler(166, 0, "SceCif0Camera",
				cameraSubIntrHandler, &cameraDriver->device[0]);
		ksceKernelRegisterSubIntrHandler(168, 0, "SceCif1Camera",
				cameraSubIntrHandler, &cameraDriver->device[1]);
		cameraDriver->device[0].eventFlagId = ksceKernelCreateEventFlag(
				"SceCif0", 0x1000, 0, &cameraDriver->device[0].eventFlagOption);
		cameraDriver->device[1].eventFlagId = ksceKernelCreateEventFlag(
				"SceCif1", 0x1000, 0, &cameraDriver->device[1].eventFlagOption);
	}
	cameraDriver->eventFlagId = ksceKernelCreateEventFlag("SceCameraDrv", 0x1000,
			0, &cameraDriver->eventFlagOption);
	driver = cameraDriver;
	driver->threadOption.size = sizeof(driver->threadOption);
	driver->threadId = ksceKernelCreateThread("SceCameraDrv", cameraDriverThread,
			30, 0x2000, 0x10000000, 0x80000,
			(const SceKernelThreadOptParam *)&driver->threadOption);
	result = cameraDriver->threadId;
	if (result >= 0) {
		ksceKernelEnableSubIntr(166, 0);
		ksceKernelEnableSubIntr(168, 0);
		ksceKernelRegisterSysEventHandler("SceCamera", cameraSysEventHandler, NULL);
		ksceKernelRegisterProcEventHandler("SceCamera", &cameraProcessEventHandler, 0);
		cameraDriver->mutexId = ksceKernelCreateMutex("SceCameraDrv", 0, 0, NULL);
		result = ksceKernelStartThread(cameraDriver->threadId, 0, NULL);
		if (result == 0) {
			cameraDriver->powerEnabled = 1;
			cameraDriver->activeEnabled = 1;
		}
	}
	return result;
}

void cameraRequestActiveState(int active, void *argument)
{
	SceKernelIntrStatus intr;
	CameraDriver *driver;
	CameraDriver *unlockDriver;

	(void)argument;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	driver = cameraDriver;
	if (driver->activeEnabled == active) {
		unlockDriver = driver;
		goto out;
	}
	if (active) {
		driver->activeEventState = 3;
		ksceKernelSetEventFlag(driver->eventFlagId, 0x10000);
		unlockDriver = cameraDriver;
		goto out;
	}
	driver->activeEventState = 1;
	ksceKernelSetEventFlag(driver->eventFlagId, 0x8000);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	return;
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&unlockDriver->spinlock, intr);
}

int cameraPowerEventHandler(int eventId, int eventCount, int flags, void *argument)
{
	SceKernelIntrStatus intr;

	(void)argument;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	if (flags & 0x800000) {
		cameraDriver->powerEventState = 3;
		ksceKernelSetEventFlag(cameraDriver->eventFlagId, 0x4000);
	} else if (flags & 0x400000) {
		cameraDriver->powerEventState = 1;
		ksceKernelSetEventFlag(cameraDriver->eventFlagId, 0x2000);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	return 0;
}

int cameraUnlockFrameBufferRanges(int devnum, CameraProcess *process,
				 SceCameraReadV2 *read, int explicitProcess,
				 SceUID processId)
{
	SceUID callerProcessId = cameraGetCallerProcessId();
	SceSize sizeIBase, sizeUBase, sizeVBase;
	void *pvIBase, *pvUBase, *pvVBase;
	int result;

	if (process->config.wBuffer == 1) {
		if (read == NULL)
			return (int)0x802e00ff;
		sizeIBase = read->sizeIBase;
		sizeUBase = read->sizeUBase;
		sizeVBase = read->sizeVBase;
		pvIBase = read->pvIBase;
		pvUBase = read->pvUBase;
		pvVBase = read->pvVBase;
	} else {
		sizeIBase = process->config.sizeIBase;
		sizeUBase = process->config.sizeUBase;
		sizeVBase = process->config.sizeVBase;
		pvIBase = process->frameBuffer.pvIBase;
		pvUBase = process->frameBuffer.pvUBase;
		pvVBase = process->frameBuffer.pvVBase;
	}
	switch (process->config.wFormat) {
	case 1:
	case 2:
	case 6:
		break;
	case 3:
		result = 0;
		if (pvIBase != NULL && sizeIBase != 0) {
			if (explicitProcess)
				result = ksceKernelUnlockRangeProc(processId, pvIBase, sizeIBase);
			else
				result = ksceKernelUnlockRangeWithMode(0x30, pvIBase, sizeIBase);
		}
		if (pvUBase != NULL && sizeUBase != 0) {
			if (explicitProcess)
				result = ksceKernelUnlockRangeProc(processId, pvUBase, sizeUBase);
			else
				result = ksceKernelUnlockRangeWithMode(0x30, pvUBase, sizeUBase);
		}
		if (pvVBase != NULL && sizeVBase != 0) {
			pvIBase = pvVBase;
			sizeIBase = sizeVBase;
			goto unlock;
		}
		if (result != 0)
			return result;
		break;
	case 4:
	case 5:
		if (pvIBase == NULL || sizeIBase == 0)
			break;
	unlock:
		if (explicitProcess)
			result = ksceKernelUnlockRangeProc(processId, pvIBase, sizeIBase);
		else
			result = ksceKernelUnlockRangeWithMode(0x30, pvIBase, sizeIBase);
		if (result != 0)
			return result;
		break;
	default:
		return (int)0x802e0007;
	}
	if (explicitProcess)
		result = cameraSetProcessRangesLocked(devnum, processId, 0);
	else
		result = cameraSetProcessRangesLocked(devnum, callerProcessId, 0);
	if (result < 0)
		return (int)0x802e00ff;
	return result;
}

int cameraBindSensor(int devnum)
{
	CameraDevice *device = &cameraDriver->device[devnum];
	int sensorType = device->sensorType;

	device->devnum = devnum;
	if (sensorType == 0) {
		cameraSetPowerStateOV7735(devnum, 1);
		if (cameraProbeOV7735(devnum) == 0)
			device->sensorType = 2;
		cameraSetPowerStateOV7735(devnum, 0);
		sensorType = device->sensorType;
		if (sensorType == 0)
			return (int)0x802e0010;
	}
	switch (sensorType) {
	case 2:
		device->initSensor = cameraInitializeOV7735;
		device->resetSensor = cameraResetOV7735;
		device->setFormat = cameraSetFormatOV7735;
		device->setResolution = cameraSetResolutionOV7735;
		device->setFramerate = cameraSetFramerateOV7735;
		device->setRange = cameraSetRangeOV7735;
		device->getAttribute = cameraGetAttributeOV7735;
		device->setAttribute = cameraSetAttributeOV7735;
		device->writeRegister = cameraWriteRegisterOV7735;
		device->readRegister = cameraReadRegisterOV7735;
		device->writeRegisters = cameraWriteRegistersOV7735;
		device->setPowerState = cameraSetPowerStateOV7735;
		device->getCsiFormat = cameraGetCsiFormatOV7735;
		device->getExposure = sceCameraGetExposureTimeOV7735;
		device->getRaw8Format = cameraGetRaw8FormatOV7735;
		device->hasColorChange = cameraHasColorChangeOV7735;
		device->recoverSensor = cameraRecoverOV7735;
		device->getSensorRegisters = cameraGetSensorRegistersOV7735;
		device->setSensorRegisters = cameraSetSensorRegistersOV7735;
		return 0;
	default:
		return 0;
	}
}

int cameraCsiStart(int devnum)
{
	CsiTransferConfig transfer;
	CameraDevice *device = &cameraDriver->device[devnum];
	SceUInt32 hardwareInfo = cameraDriver->hardwareInfo;
	unsigned int width;
	unsigned int head;
	unsigned int mode;
	int result;

	if ((hardwareInfo & 0xdf0000) == 0x100000 ||
	    (hardwareInfo & 0xff0000) == 0x310000 ||
	    (hardwareInfo & 0xfe0000) == 0x400000)
		head = 1 - devnum;
	else
		head = devnum;
	SceCsiForDriver_B508822B(head, device->config.wWidth, device->config.wHeight);
	transfer.chromaAddress[1][0] = device->frameBuffer[1].paddrUBase;
	transfer.chromaAddress[1][1] = device->frameBuffer[1].paddrVBase;
	transfer.chromaAddress[0][0] = device->frameBuffer[0].paddrUBase;
	transfer.chromaAddress[0][1] = device->frameBuffer[0].paddrVBase;
	transfer.iAddress[0] = device->frameBuffer[0].paddrIBase;
	transfer.iAddress[2] = device->frameBuffer[2].paddrIBase;
	transfer.iAddress[1] = device->frameBuffer[1].paddrIBase;
	transfer.chromaAddress[2][1] = device->frameBuffer[2].paddrVBase;
	transfer.chromaAddress[2][0] = device->frameBuffer[2].paddrUBase;
	width = device->config.wWidth;
	transfer.iPitch = device->config.wFormat == 2 ? width * 2 : width;
	width >>= 1;
	if (device->chromaAligned)
		width = (width + 15) & ~15u;
	transfer.chromaPitch = width;
	SceCsiForDriver_10545393(head, &transfer);
	switch (device->config.wFormat) {
	case 1:
	case 3:
	case 4:
	case 5:
		mode = 16;
		break;
	case 2:
		mode = 17;
		break;
	case 6:
		mode = 48;
		break;
	default:
		return (int)0x802e0007;
	}
	SceCsiForDriver_2AFEA1B0(head, mode);
	result = device->getCsiFormat(devnum, device->config.wFramerate,
			device->config.wFormat);
	result = SceCsiForDriver_FC165297(head, result);
	device->irqFrameCounter = 0;
	return result;
}

int cameraSubIntrHandler(void *subintrArg, void *registerArg,
		unsigned char intrPriority)
{
	CameraDevice *device = registerArg;
	unsigned int frameIndex = (unsigned int)subintrArg - 1;
	unsigned int eventBits;
	int intrStatus;
	int devnum;
	int frameInterval;
	SceUInt64 timestamp;

	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	if (frameIndex <= 2) {
		devnum = device != &cameraDriver->device[0] &&
			device == &cameraDriver->device[1];
		device->irqFrameCounter++;
		if (device->waitingForReverseFrame) {
			device->waitingForReverseFrame = 0;
			ksceKernelSetEventFlag(device->eventFlagId, 2);
		} else {
			ksceKernelPollEventFlag(device->eventFlagId, 7, 1, &eventBits);
			if (eventBits & 4) {
				ksceKernelSetEventFlag(device->eventFlagId, 1);
			} else {
				cameraPreviousFrameTimestamp[devnum] = device->timestamp;
				if (cameraAdvanceProcessFrame(devnum, device->activeProcessId) >= 0) {
					device->frame++;
					timestamp = sceKernelGetSystemTimeWide();
					device->timestamp = timestamp;
					cameraLastFrameTimestamp[devnum] = timestamp;
					frameInterval = (unsigned int)timestamp -
						(unsigned int)cameraPreviousFrameTimestamp[devnum];
					if (frameInterval > 0)
						cameraMeasuredFramerate[devnum] = 1000000000 / frameInterval;
					device->irqFrameIndex = frameIndex;
					ksceKernelSetEventFlag(device->eventFlagId, 1);
				}
			}
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	return 0;
}

int cameraLockFrameBufferRanges(int devnum, CameraProcess *process,
		SceCameraReadV2 *read)
{
	SceUID processId = cameraGetCallerProcessId();
	SceSize sizeIBase, sizeUBase, sizeVBase;
	void *pvIBase, *pvUBase, *pvVBase;
	unsigned int formatMask;
	int result = 0;

	if (process->config.wBuffer == 1) {
		if (read == NULL)
			return (int)0x802e00ff;
		sizeIBase = read->sizeIBase;
		sizeUBase = read->sizeUBase;
		sizeVBase = read->sizeVBase;
		pvIBase = read->pvIBase;
		pvUBase = read->pvUBase;
		pvVBase = read->pvVBase;
	} else {
		sizeIBase = process->config.sizeIBase;
		sizeUBase = process->config.sizeUBase;
		sizeVBase = process->config.sizeVBase;
		pvIBase = process->frameBuffer.pvIBase;
		pvUBase = process->frameBuffer.pvUBase;
		pvVBase = process->frameBuffer.pvVBase;
	}
	if ((SceUInt16)(process->config.wFormat - 1) > 5)
		return (int)0x802e0007;
	formatMask = 1U << (process->config.wFormat - 1);
	if (formatMask & 0x23)
		goto locked;
	if (formatMask & 4) {
		if (pvIBase != NULL && sizeIBase != 0) {
			result = ksceKernelLockRangeWithMode(0x30, pvIBase, sizeIBase);
			if (result < 0)
				return result;
		}
		if (pvUBase != NULL && sizeUBase != 0) {
			result = ksceKernelLockRangeWithMode(0x30, pvUBase, sizeUBase);
			if (result < 0)
				goto unlockI;
		}
		if (pvVBase != NULL && sizeVBase != 0) {
			result = ksceKernelLockRangeWithMode(0x30, pvVBase, sizeVBase);
			if (result < 0)
				goto unlockIU;
		}
		if (result != 0)
			return result;
	} else if (formatMask & 0x18) {
		if (pvIBase != NULL && sizeIBase != 0) {
			result = ksceKernelLockRangeWithMode(0x30, pvIBase, sizeIBase);
			if (result != 0)
				return result;
		}
	} else {
		return (int)0x802e0007;
	}
locked:
	result = cameraSetProcessRangesLocked(devnum, processId, 1);
	if (result < 0)
		return (int)0x802e00ff;
	return result;
unlockIU:
	if (pvIBase != NULL && sizeIBase != 0)
		ksceKernelUnlockRangeWithMode(0x30, pvIBase, sizeIBase);
	if (pvUBase != NULL && sizeUBase != 0)
		ksceKernelUnlockRangeWithMode(0x30, pvUBase, sizeUBase);
	return result;
unlockI:
	if (pvIBase != NULL && sizeIBase != 0)
		ksceKernelUnlockRangeWithMode(0x30, pvIBase, sizeIBase);
	return result;
}

int cameraSetBusClockFrequency(int frequency)
{
	SceUInt32 timeout = 0xffffffff;
	CameraDriver *driver;
	int result;

	result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout);
	if (result < 0)
		return result;
	timeout = 0xffffffff;
	result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout);
	if (result < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		return result;
	}
	result = kscePowerSetCameraBusClockFrequency(frequency);
	driver = cameraDriver;
	if (result >= 0)
		driver->busClockFast = frequency == 166;
	ksceKernelUnlockMutex(driver->device[0].mutexId, 1);
	ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	return result;
}

int cameraSetSensorClockEnabled(int enabled)
{
	SceUInt32 timeout = 0xffffffff;
	int result;

	result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout);
	if (result < 0)
		return result;
	timeout = 0xffffffff;
	result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout);
	if (result < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		return result;
	}
	if (enabled) {
		result = SceClockgenForDriver_06663ECA();
		sceKernelDelayThread(1500);
	} else {
		result = SceClockgenForDriver_82F39378();
	}
	if (result >= 0)
		cameraDriver->clockEnabled = enabled;
	ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
	ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	return result;
}

int cameraWaitForDeviceReads(void)
{
	CameraDriver *driver;
	SceUInt32 timeout;
	int intrStatus;

	for (;;) {
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		driver = cameraDriver;
		if (!driver->device[0].readInProgress && !driver->device[1].readInProgress)
			break;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
		timeout = 8000;
		ksceKernelClearEventFlag(cameraDriver->eventFlagId, ~0x60000U);
		ksceKernelWaitEventFlagCB(cameraDriver->eventFlagId, 0x60000, 5, NULL, &timeout);
	}
	driver->device[0].busy = 1;
	driver->device[1].busy = 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
	return 1;
}

int cameraProcessStartEvent(SceUID pid, int eventType,
		SceProcEventInvokeParam1 *param, int arg)
{
	int intrStatus;
	int state;
	int result = pid;

	switch (eventType) {
	case 0x20000:
		if (pid >= 0) {
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			cameraDriver->pendingSuspendProcessId = pid;
			ksceKernelSetEventFlag(cameraDriver->eventFlagId, 0x20);
			cameraSetProcessRequestedSuspendState(0, pid, 3);
			result = 0;
			cameraSetProcessRequestedSuspendState(1, pid, 3);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		}
		break;
	case 0x20001:
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		if (cameraIsProcessActive(0, pid))
			cameraGetProcessSuspendState(0, pid, &state);
		if (cameraIsProcessActive(1, pid))
			cameraGetProcessSuspendState(1, pid, &state);
		result = 0;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		break;
	default:
		result = 0;
		break;
	}
	return result;
}

int cameraProcessCreateEvent(SceUID pid, SceProcEventInvokeParam2 *param, int arg)
{
	SceUInt32 timeout = 3000000;
	int intrStatus;
	int result = pid;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) >= 0) {
		if (pid >= 0) {
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			if (cameraGetProcessStorage(pid, 0) == NULL) {
				result = cameraInitializeProcess(pid);
				if (result == 0)
					result = cameraRegisterProcess(pid);
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			} else {
				result = (int)0x802e00ff;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			}
		}
		ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	} else {
		result = (int)0x802e000a;
	}
	return result;
}

int cameraSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	CameraDriver *driver;
	SceUInt32 timeout;
	unsigned int eventBits;
	int requestedState;
	int intrStatus;
	int result = resume;

	if (resume != 0) {
		if (resume != 1)
			return 0;
		if (eventid != 0x100000) {
			if (eventid != 0x100002)
				return 0;
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			driver = cameraDriver;
			if (driver->device[0].suspendState == 0)
				result = driver->device[1].suspendState != 0 ? (int)0x802e0013 : 0;
			else
				result = (int)0x802e0013;
			goto unlock;
		}
		result = 0;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		driver = cameraDriver;
		requestedState = 3;
		eventBits = 0x400;
		driver->device[0].requestedSuspendState = 3;
	} else {
		if (eventid == 0x401) {
			timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
				result = (int)0x802e000a;
			else
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			return result;
		}
		if (eventid == 0x402) {
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			driver = cameraDriver;
			if (driver->device[0].suspendState == 2)
				result = driver->device[1].suspendState == 2 ? 0 : (int)0x802e0013;
			else
				result = (int)0x802e0013;
			goto unlock;
		}
		if (eventid != 0x400)
			return result;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		driver = cameraDriver;
		eventBits = 0x100;
		requestedState = 1;
		driver->device[0].suspendState = 1;
		driver->device[0].requestedSuspendState = 1;
		driver->device[1].suspendState = 1;
	}
	driver->device[1].requestedSuspendState = requestedState;
	ksceKernelSetEventFlag(driver->eventFlagId, eventBits);
	driver = cameraDriver;
unlock:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
	return result;
}

int cameraProcessStopEvent(SceUID pid, int eventType,
		SceProcEventInvokeParam1 *param, int arg)
{
	union {
		SceUInt32 timeout;
		int state;
	} wait;
	CameraDriver *driver;
	int intrStatus;
	int devnum;
	int status;

	switch (eventType) {
	case 0x201:
		if (pid < 0)
			break;
		wait.timeout = 3000000;
		if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &wait.timeout) < 0) {
			pid = (int)0x802e000a;
			break;
		}
		status = 0;
		for (devnum = 0; devnum != 2; devnum++) {
			driver = cameraDriver;
			if (cameraIsProcessActive(devnum, pid) &&
			    cameraGetProcessSuspendState(devnum, pid, &wait.state) == 0 &&
			    wait.state != 2 && driver->device[devnum].suspendState != 2)
				status = (int)0x802e0013;
		}
		pid = status;
		ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
		break;
	case 0x202:
		if (pid < 0)
			break;
		devnum = 0;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		for (;;) {
			driver = cameraDriver;
			if (cameraIsProcessActive(devnum, pid) &&
			    cameraGetProcessSuspendState(devnum, pid, &wait.state) == 0 &&
			    wait.state != 2 && driver->device[devnum].suspendState != 2) {
				pid = (int)0x802e0013;
				break;
			}
			if (++devnum == 2) {
				pid = 0;
				break;
			}
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		break;
	case 0x200:
		if (pid < 0)
			break;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		cameraSetProcessSuspendState(0, pid, 1);
		cameraSetProcessRequestedSuspendState(0, pid, 1);
		cameraSetProcessSuspendState(1, pid, 1);
		cameraSetProcessRequestedSuspendState(1, pid, 1);
		cameraDriver->pendingSuspendProcessId = pid;
		pid = 0;
		ksceKernelSetEventFlag(cameraDriver->eventFlagId, 8);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		break;
	default:
		pid = 0;
		break;
	}
	return pid;
}

int cameraTerminate(void)
{
	CameraDriver *driver;
	SceUInt32 hardwareInfo;
	SceUInt32 timeout;
	unsigned int head;
	int result;

	if (!cameraDriver->initialized)
		return (int)0x802e0002;
	ksceKernelDeleteMutex(cameraDriver->mutexId);
	ksceKernelDeleteEventFlag(cameraDriver->device[0].eventFlagId);
	ksceKernelDeleteEventFlag(cameraDriver->device[1].eventFlagId);
	ksceKernelReleaseSubIntrHandler(166, 0);
	ksceKernelReleaseSubIntrHandler(168, 0);
	driver = cameraDriver;
	hardwareInfo = driver->hardwareInfo;
	head = (hardwareInfo & 0xdf0000) == 0x100000 ||
	       (hardwareInfo & 0xff0000) == 0x310000 ||
	       (hardwareInfo & 0xfe0000) == 0x400000;
	if (driver->busClockFast) {
		ksceKernelClearEventFlag(driver->device[0].eventFlagId, ~3u);
		SceCsiForDriver_A1D1805D(head);
		driver->device[0].setPowerState(0, 0);
		SceCsiForDriver_4AE39F26(head);
		driver->device[0].irqFrameCounter = 0;
	}
	driver = cameraDriver;
	hardwareInfo = driver->hardwareInfo;
	head = (hardwareInfo & 0xdf0000) != 0x100000 &&
	       (hardwareInfo & 0xff0000) != 0x310000 &&
	       (hardwareInfo & 0xfe0000) != 0x400000;
	if (driver->busClockFast) {
		ksceKernelClearEventFlag(driver->device[1].eventFlagId, ~3u);
		SceCsiForDriver_A1D1805D(head);
		driver->device[1].setPowerState(1, 0);
		SceCsiForDriver_4AE39F26(head);
		driver->device[1].irqFrameCounter = 0;
	}
	ksceKernelFreeMemBlock(cameraFrameBufferMemBlockId);
	cameraFrameBufferMemBlockId = -1;
	ksceKernelFreeMemBlock(cameraSramMemBlockId);
	cameraFrameBufferBase[0][0] = NULL;
	cameraFrameBufferBase[0][1] = NULL;
	cameraFrameBufferBase[0][2] = NULL;
	cameraFrameBufferBase[1][0] = NULL;
	cameraFrameBufferBase[1][1] = NULL;
	cameraFrameBufferBase[1][2] = NULL;
	cameraAlternateFrameBufferBase[0][0] = NULL;
	cameraAlternateFrameBufferBase[0][1] = NULL;
	cameraAlternateFrameBufferBase[0][2] = NULL;
	cameraAlternateFrameBufferBase[1][0] = NULL;
	cameraAlternateFrameBufferBase[1][1] = NULL;
	cameraAlternateFrameBufferBase[1][2] = NULL;
	cameraSramFrameBufferBase[0][0] = NULL;
	cameraSramFrameBufferBase[0][1] = NULL;
	cameraSramFrameBufferBase[0][2] = NULL;
	cameraSramFrameBufferBase[1][0] = NULL;
	cameraSramFrameBufferBase[1][1] = NULL;
	cameraSramFrameBufferBase[1][2] = NULL;
	cameraSramMemBlockId = -1;
	timeout = ~0u;
	result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout);
	if (result < 0)
		return result;
	timeout = ~0u;
	result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout);
	if (result < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		return result;
	}
	result = SceClockgenForDriver_82F39378();
	driver = cameraDriver;
	if (result < 0) {
		ksceKernelUnlockMutex(driver->device[0].mutexId, 1);
		ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
		return result;
	}
	driver->clockEnabled = 0;
	ksceKernelUnlockMutex(driver->device[0].mutexId, 1);
	ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	ksceKernelSetEventFlag(cameraDriver->eventFlagId, 0x100000);
	sceKernelDelayThread(100000);
	ksceKernelDeleteEventFlag(cameraDriver->eventFlagId);
	ksceKernelDeleteMutex(cameraDriver->device[0].mutexId);
	ksceKernelDeleteMutex(cameraDriver->device[1].mutexId);
	return cameraResetConfig();
}

int cameraResumeDevice(int devnum)
{
	union {
		SceUInt32 timeout;
		CameraAttributeOption attribute;
	} option;
	CameraDriver *driver;
	CameraDevice *device;
	CameraDevice *attributeDevice;
	SceUInt32 eventPattern;
	int intrStatus;
	int started;
	int result;

	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	started = cameraHasStartedProcess(devnum, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (!started)
		return 0;
	driver = cameraDriver;
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
	started = cameraIsProcessStarted(devnum, cameraDriver->activeProcessId);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (!started)
		return 0;
	device = &driver->device[devnum];
	ksceKernelClearEventFlag(device->eventFlagId, ~3u);
	result = cameraSetSensorClockEnabled(1);
	if (result < 0)
		return result;
	option.timeout = ~0u;
	result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &option.timeout);
	if (result < 0)
		return result;
	option.timeout = ~0u;
	result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &option.timeout);
	if (result < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		return result;
	}
	result = kscePowerSetCameraBusClockFrequency(166);
	if (result < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
		return result;
	}
	cameraDriver->busClockFast = 1;
	ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
	ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	result = cameraCsiStart(devnum);
	if (result < 0)
		return result;
	if (!cameraDriver->gpioPowerEnabled) {
		ksceGpioSetPortMode(0, 9, 0);
		ksceGpioPortClear(0, 9);
		sceKernelDelayThread(1000);
		ksceGpioPortSet(0, 9);
		cameraDriver->gpioPowerEnabled = 1;
	}
	result = ksceGpioSetPortMode(0, devnum ? 11 : 10, 0);
	if (result < 0)
		return result;
	device->setPowerState(devnum, 1);
	result = device->resetSensor(devnum);
	if (result < 0)
		return result;
	result = device->initSensor(devnum);
	if (result < 0)
		return result;
	result = device->setFormat(devnum, device->config.wFormat);
	if (result < 0)
		return result;
	result = device->setResolution(devnum, device->config.wResolution,
			device->config.wFramerate);
	if (result < 0)
		return result;
	result = device->setFramerate(devnum, device->config.wFramerate,
			device->config.wResolution);
	if (result < 0)
		return result;
	result = device->setRange(devnum, device->config.wRange);
	if (result < 0)
		return result;
	attributeDevice = &cameraDriver->device[devnum];
	option.attribute.wRange = attributeDevice->config.wRange;
	option.attribute.deferRegisterUpdate = 1;
	option.attribute.updateDevice = 1;
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = cameraRestoreProcessSensorRegisters(devnum);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (result < 0)
		return result;
	if (attributeDevice->config.brightness != 128 &&
	    (result = attributeDevice->setAttribute(devnum, 11,
		attributeDevice->config.brightness, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.contrast != 32 &&
	    (result = attributeDevice->setAttribute(devnum, 12,
		attributeDevice->config.contrast, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.saturation != 10 &&
	    (result = attributeDevice->setAttribute(devnum, 10,
		attributeDevice->config.saturation, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.sharpness != 1 &&
	    (result = attributeDevice->setAttribute(devnum, 13,
		attributeDevice->config.sharpness, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.reverse &&
	    (result = attributeDevice->setAttribute(devnum, 14,
		attributeDevice->config.reverse, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.effect &&
	    (result = attributeDevice->setAttribute(devnum, 15,
		attributeDevice->config.effect, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.ev &&
	    (result = attributeDevice->setAttribute(devnum, 16,
		attributeDevice->config.ev, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.wZoom != 10 &&
	    (result = attributeDevice->setAttribute(devnum, 17,
		attributeDevice->config.wZoom, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.antiflicker &&
	    (result = attributeDevice->setAttribute(devnum, 18,
		attributeDevice->config.antiflicker, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.iso != 1 &&
	    (result = attributeDevice->setAttribute(devnum, 20,
		attributeDevice->config.iso, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.whiteBalance &&
	    (result = attributeDevice->setAttribute(devnum, 22,
		attributeDevice->config.whiteBalance, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.backlight &&
	    (result = attributeDevice->setAttribute(devnum, 23,
		attributeDevice->config.backlight, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.nightMode &&
	    (result = attributeDevice->setAttribute(devnum, 24,
		attributeDevice->config.nightMode, 0, &option.attribute)) < 0)
		return result;
	if (attributeDevice->config.noiseReduction != 1)
		attributeDevice->setAttribute(devnum, 25,
			attributeDevice->config.noiseReduction, 0, &option.attribute);
	if (attributeDevice->config.exposureCeiling)
		attributeDevice->setAttribute(devnum, 27,
			attributeDevice->config.exposureCeiling, 0, &option.attribute);
	if (attributeDevice->config.imageQuality)
		attributeDevice->setAttribute(devnum, 28,
			attributeDevice->config.imageQuality, 0, &option.attribute);
	result = attributeDevice->setAttribute(devnum, 240, 0, 0, &option.attribute);
	if (result < 0)
		return result;
	attributeDevice = &cameraDriver->device[devnum];
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	attributeDevice->waitingForReverseFrame = 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	option.timeout = device->config.frameTimeout;
	ksceKernelWaitEventFlag(device->eventFlagId, 3, 1, &eventPattern, &option.timeout);
	return result;
}

int cameraResumeDevices(int eventType)
{
	CameraDriver *driver;
	CameraDevice *device;
	int enabled;
	int devnum;
	int intrStatus;
	int result = 0;

	if (eventType == 1)
		enabled = cameraDriver->activeEnabled != 0;
	else if (eventType == 2)
		enabled = cameraDriver->powerEnabled != 0;
	else
		enabled = 1;
	driver = cameraDriver;
	for (devnum = 0; devnum != 2; devnum++) {
		device = &driver->device[devnum];
		if (driver->suspendState != 2) {
			if (driver->activeProcessId == device->pendingProcessId &&
			    enabled && (result = cameraResumeDevice(devnum),
					driver = cameraDriver, result < 0)) {
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
				ksceKernelClearEventFlag(device->eventFlagId, ~4u);
				driver = cameraDriver;
				device->busy = 0;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
				driver = cameraDriver;
			} else {
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
				ksceKernelClearEventFlag(device->eventFlagId, ~4u);
				driver = cameraDriver;
				device->suspendState = 0;
				device->busy = 0;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
				driver = cameraDriver;
			}
		}
	}
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
	driver = cameraDriver;
	if (eventType == 1) {
		if (driver->powerEventState == 1) {
			ksceKernelSetEventFlag(driver->eventFlagId, 0x2000);
			driver = cameraDriver;
		}
	} else if (eventType == 2 && driver->activeEventState == 1) {
		ksceKernelSetEventFlag(driver->eventFlagId, 0x8000);
		driver = cameraDriver;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
	return result;
}

int cameraStopProcess(int devnum, SceUID processId)
{
	CameraDriver *driver;
	CameraDevice *device;
	SceUInt32 hardwareInfo;
	SceUInt32 timeout;
	unsigned int head;
	int otherDevice;
	int intrStatus;
	int started;
	int result;

	if (devnum & ~1)
		return (int)0x802e0000;
	if (!cameraDriver->initialized)
		return (int)0x802e0002;
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = sceCameraConfigStopPid(devnum, processId);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (result)
		return result == (int)0x802e000e ? 0 : result;
	hardwareInfo = cameraDriver->hardwareInfo;
	if ((hardwareInfo & 0xdf0000) == 0x100000 ||
	    (hardwareInfo & 0xff0000) == 0x310000 ||
	    (hardwareInfo & 0xfe0000) == 0x400000) {
		otherDevice = 1 - devnum;
		head = otherDevice;
	} else {
		head = devnum;
		otherDevice = 1 - devnum;
	}
	driver = cameraDriver;
	if (driver->busClockFast) {
		device = &driver->device[devnum];
		ksceKernelClearEventFlag(device->eventFlagId, ~3u);
		SceCsiForDriver_A1D1805D(head);
		device->setPowerState(devnum, 0);
		SceCsiForDriver_4AE39F26(head);
		driver = cameraDriver;
		device->irqFrameCounter = 0;
	}
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
	started = cameraHasStartedProcess(otherDevice, 1);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (started)
		goto setStopped;
	timeout = ~0u;
	result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout);
	if (result < 0)
		return result;
	timeout = ~0u;
	result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout);
	if (result < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		return result;
	}
	result = kscePowerSetCameraBusClockFrequency(0);
	if (result < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
		return result;
	}
	driver = cameraDriver;
	driver->busClockFast = 0;
	ksceKernelUnlockMutex(driver->device[0].mutexId, 1);
	ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	timeout = ~0u;
	if (ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout) >= 0) {
		timeout = ~0u;
		if (ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout) < 0) {
			ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		} else {
			result = SceClockgenForDriver_82F39378();
			if (result >= 0)
				cameraDriver->clockEnabled = 0;
			ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
			ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
		}
	}
setStopped:
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = sceCameraConfigSetStartedPid(devnum, processId, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	return result;
}

int cameraReleaseProcess(SceUID processId)
{
	CameraDriver *driver;
	CameraDevice *device;
	CameraProcess *process;
	int devnum;
	int intrStatus;
	int otherProcess;
	int result;

	if (processId < 0)
		return processId;
	driver = cameraDriver;
	for (devnum = 0; devnum != 2; devnum++) {
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
		if (cameraIsProcessActive(devnum, processId)) {
			device = &cameraDriver->device[devnum];
			ksceKernelClearEventFlag(device->eventFlagId, ~7u);
			ksceKernelSetEventFlag(device->eventFlagId, 4);
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		process = cameraGetProcess(devnum, processId);
		if (process && process->config.wBuffer == 0 && process->rangesLocked)
			cameraUnlockFrameBufferRanges(devnum, process, NULL, 1, processId);
		driver = cameraDriver;
		if (driver->initialized) {
			cameraStopProcess(devnum, processId);
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			if (sceCameraConfigClosePid(devnum, processId) < 0) {
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			} else {
				otherProcess = cameraHasOtherProcess(devnum, processId);
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				if (!otherProcess) {
					intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
					cameraHasOpenProcess(1 - devnum);
					ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				}
			}
			driver = cameraDriver;
		}
	}
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
	if (cameraGetProcessStorage(processId, 0)) {
		result = cameraUnregisterProcess(processId);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		return result;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	return (int)0x802e00ff;
}

int cameraProcessKillEvent(SceUID pid, SceProcEventInvokeParam1 *param, int arg)
{
	SceUInt32 timeout = 3000000;
	int result;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	result = cameraReleaseProcess(pid);
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int cameraProcessExitEvent(SceUID pid, SceProcEventInvokeParam1 *param, int arg)
{
	SceUInt32 timeout = 3000000;
	int result;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	result = cameraReleaseProcess(pid);
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int cameraSuspendDevices(int eventType)
{
	CameraDriver *driver;
	CameraDevice *device;
	SceUInt32 hardwareInfo;
	SceUInt32 timeout;
	int enabled;
	int intrStatus;
	int devnum;
	int head;
	int startedFront;
	int startedBack;
	int result;

	if (eventType == 1)
		enabled = cameraDriver->activeEnabled != 0;
	else if (eventType == 2)
		enabled = cameraDriver->powerEnabled != 0;
	else
		enabled = 1;
	driver = cameraDriver;
	for (devnum = 0; devnum != 2; devnum++) {
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
		device = &driver->device[devnum];
		device->suspendState = 1;
		result = cameraHasStartedProcess(devnum, 0);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		if (enabled && result) {
			ksceKernelClearEventFlag(device->eventFlagId, ~7u);
			ksceKernelSetEventFlag(device->eventFlagId, 4);
			driver = cameraDriver;
			if (driver->activeProcessId == device->pendingProcessId) {
				hardwareInfo = driver->hardwareInfo;
				head = devnum;
				if ((hardwareInfo & 0xdf0000) == 0x100000 ||
				    (hardwareInfo & 0xff0000) == 0x310000 ||
				    (hardwareInfo & 0xfe0000) == 0x400000)
					head = 1 - devnum;
				result = driver->busClockFast;
				if (result) {
					device = &driver->device[devnum];
					result = 0;
					ksceKernelClearEventFlag(device->eventFlagId, ~3u);
					SceCsiForDriver_A1D1805D(head);
					device->setPowerState(devnum, 0);
					SceCsiForDriver_4AE39F26(head);
					driver = cameraDriver;
					device->irqFrameCounter = 0;
				}
			}
		} else {
			driver = cameraDriver;
		}
	}
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
	startedFront = cameraHasStartedProcess(0, 1);
	startedBack = cameraHasStartedProcess(1, 1);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if ((!startedFront || !startedBack) && enabled) {
		cameraSetBusClockFrequency(0);
		timeout = 0xffffffff;
		result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout);
		if (result >= 0) {
			timeout = 0xffffffff;
			result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout);
			if (result < 0) {
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
			} else {
				result = SceClockgenForDriver_82F39378();
				if (result >= 0)
					cameraDriver->clockEnabled = 0;
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
				ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
			}
		}
	}
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	driver = cameraDriver;
	driver->device[0].suspendState = 2;
	driver->device[1].suspendState = 2;
	driver->device[0].busy = 0;
	driver->device[1].busy = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	driver = cameraDriver;
	if (eventType == 1) {
		if (driver->powerEventState == 3) {
			ksceKernelSetEventFlag(driver->eventFlagId, 0x4000);
			driver = cameraDriver;
		}
	} else if (eventType == 2 && driver->activeEventState == 3) {
		ksceKernelSetEventFlag(driver->eventFlagId, 0x10000);
		driver = cameraDriver;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
	return result;
}

int cameraDriverThread(SceSize args, void *argp)
{
	SceKernelAllocMemBlockKernelOpt frameOption;
	SceKernelAllocMemBlockKernelOpt sramOption;
	union {
		SceUInt32 timeout;
		int state;
		DeviceLocation location;
	} work;
	CameraDriver *driver;
	CameraDevice *device;
	void *(*frameBuffers)[3];
	SceUID processId;
	SceUID previousProcessId;
	SceUInt32 events;
	SceUInt32 hardwareInfo;
	unsigned int deviceFlags;
	int devnum;
	int head;
	int intrStatus;
	int result;
	int failed = 0;
	int requested;
	int processStatus;
	int hasProcessStatus;
	float x;
	float y;
	float z;

	if (cameraSetSensorClockEnabled(1) < 0)
		goto registerCallback;
	cameraDriver->gpioPowerEnabled = 0;
	for (devnum = 0; devnum != 2; devnum++) {
		if (!cameraDriver->gpioPowerEnabled) {
			ksceGpioSetPortMode(0, 9, 0);
			ksceGpioPortClear(0, 9);
			sceKernelDelayThread(1000);
			ksceGpioPortSet(0, 9);
			cameraDriver->gpioPowerEnabled = 1;
		}
		if (ksceGpioSetPortMode(0, devnum ? 11 : 10, 0) >= 0) {
			cameraDriver->device[devnum].sensorType = 0;
			if (cameraBindSensor(devnum) < 0)
				failed = 1;
		}
	}
	work.timeout = 0xffffffff;
	if (ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &work.timeout) < 0)
		goto registerCallback;
	work.timeout = 0xffffffff;
	if (ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &work.timeout) < 0) {
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		goto registerCallback;
	}
	result = SceClockgenForDriver_82F39378();
	driver = cameraDriver;
	if (result >= 0)
		driver->clockEnabled = 0;
	ksceKernelUnlockMutex(driver->device[0].mutexId, 1);
	ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	if (result < 0 || failed)
		goto registerCallback;
	memset(&frameOption, 0, sizeof(frameOption));
	frameOption.size = sizeof(frameOption);
	frameOption.alignment = 4096;
	frameOption.attr = 0x200000;
	cameraFrameBufferMemBlockId = ksceKernelAllocMemBlock("SceCamera", 0x10208006,
							   3686400, &frameOption);
	if (cameraFrameBufferMemBlockId < 0 ||
	    ksceKernelGetMemBlockBase(cameraFrameBufferMemBlockId,
				     &cameraAlternateFrameBufferBase[0][0]) < 0)
		goto registerCallback;
	cameraAlternateFrameBufferBase[0][1] = (char *)cameraAlternateFrameBufferBase[0][0] + 614400;
	cameraAlternateFrameBufferBase[1][2] = (char *)cameraAlternateFrameBufferBase[0][0] + 3072000;
	cameraAlternateFrameBufferBase[0][2] = (char *)cameraAlternateFrameBufferBase[0][0] + 1228800;
	cameraAlternateFrameBufferBase[1][0] = (char *)cameraAlternateFrameBufferBase[0][0] + 1843200;
	cameraAlternateFrameBufferBase[1][1] = (char *)cameraAlternateFrameBufferBase[0][0] + 2457600;
	memset(&sramOption, 0, sizeof(sramOption));
	sramOption.size = sizeof(sramOption);
	sramOption.attr = 2;
	sramOption.paddr = COMPAT_SRAM_BASE;
	cameraSramMemBlockId = ksceKernelAllocMemBlock("SceCameraSRAM", 0x20108006,
							 1843200, &sramOption);
	if (cameraSramMemBlockId < 0 ||
	    ksceKernelGetMemBlockBase(cameraSramMemBlockId, &cameraSramFrameBufferBase[0][0]) < 0)
		goto registerCallback;
	cameraSramFrameBufferBase[0][1] = (char *)cameraSramFrameBufferBase[0][0] + 614400;
	cameraSramFrameBufferBase[0][2] = (char *)cameraSramFrameBufferBase[0][0] + 1228800;
	cameraSramFrameBufferBase[1][0] = cameraSramFrameBufferBase[0][0];
	cameraSramFrameBufferBase[1][1] = cameraSramFrameBufferBase[0][1];
	cameraSramFrameBufferBase[1][2] = cameraSramFrameBufferBase[0][2];
	__builtin_memcpy(cameraFrameBufferBase, cameraSramFrameBufferBase, sizeof(cameraSramFrameBufferBase));
	memset(&work.location, 0, sizeof(work.location));
	driver = cameraDriver;
	for (devnum = 0; devnum != 2; devnum++) {
		if (ksceIdStorageLookup(0x114, 16 * devnum, &work.location, 16) < 0) {
			hardwareInfo = ksceSysconGetHardwareInfo();
			if (devnum) {
				work.location.type = 1;
				work.location.x = 0;
				work.location.y = 32;
				work.location.z = -12;
			} else {
				work.location.type = 0;
				if ((hardwareInfo & 0xff0000) < 0x800000) {
					work.location.x = 63;
					work.location.y = 21;
				} else {
					work.location.x = 66;
					work.location.y = 24;
				}
				work.location.z = -2;
			}
		}
		x = (float)work.location.x;
		if (x > 300.0f)
			x = 300.0f;
		else if (x < -300.0f)
			x = -300.0f;
		y = (float)work.location.y;
		if (y > 300.0f)
			y = 300.0f;
		else if (y < -300.0f)
			y = -300.0f;
		z = (float)work.location.z;
		if (z > 300.0f)
			z = 300.0f;
		else if (z < -300.0f)
			z = -300.0f;
		if (work.location.type == 0) {
			driver->location[0].x = x;
			driver->location[0].y = y;
			driver->location[0].z = z;
		} else if (work.location.type == 1) {
			driver->location[1].x = x;
			driver->location[1].y = y;
			driver->location[1].z = z;
		}
	}
	cameraDriver->initialized = 1;
registerCallback:
	driver = cameraDriver;
	result = ksceKernelCreateCallback("SceCameraDrv", 0, cameraPowerEventHandler, NULL);
	driver->callbackId = result;
	result = cameraDriver->callbackId;
	if (result < 0)
		return result;
	scePowerRegisterCallback(result);
	ScePowerForDriver_FE35B73D(cameraRequestActiveState, NULL);
	for (;;) {
		ksceKernelWaitEventFlagCB(cameraDriver->eventFlagId, 0x11ffff, 5, &events, NULL);
		if (events & 0x100000)
			return sceKernelExitThread(0);
		if (!(events & 0x1fffc)) {
			driver = cameraDriver;
			if ((events & 1) && driver->i2cEnabled && driver->clockEnabled) {
				if (driver->device[0].hasColorChange(0) > 0)
					driver->device[0].recoverSensor(0);
				driver = cameraDriver;
			}
			if ((events & 2) && driver->i2cEnabled && driver->clockEnabled) {
				if (driver->device[1].hasColorChange(1) > 0)
					driver->device[1].recoverSensor(1);
			}
			continue;
		}
		if (events & 8) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				for (devnum = 0; devnum != 2; devnum++) {
					driver = cameraDriver;
					device = &driver->device[devnum];
					if (driver->activeEnabled && driver->powerEnabled) {
						intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
						result = cameraIsProcessActive(devnum, cameraDriver->pendingSuspendProcessId);
						ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
						if (result) {
							ksceKernelClearEventFlag(device->eventFlagId, ~7u);
							ksceKernelSetEventFlag(device->eventFlagId, 4);
							driver = cameraDriver;
							hardwareInfo = driver->hardwareInfo;
							head = devnum;
							if ((hardwareInfo & 0xdf0000) == 0x100000 ||
							    (hardwareInfo & 0xff0000) == 0x310000 ||
							    (hardwareInfo & 0xfe0000) == 0x400000)
								head = 1 - devnum;
							if (driver->busClockFast) {
								CameraDevice *activeDevice = &driver->device[devnum];
								ksceKernelClearEventFlag(activeDevice->eventFlagId, ~3u);
								SceCsiForDriver_A1D1805D(head);
								activeDevice->setPowerState(devnum, 0);
								SceCsiForDriver_4AE39F26(head);
								activeDevice->irqFrameCounter = 0;
							}
							if (!cameraHasStartedProcess(1 - devnum, 1)) {
								work.timeout = 0xffffffff;
								if (ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &work.timeout) < 0)
									goto suspendProcessDone;
								work.timeout = 0xffffffff;
								if (ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &work.timeout) < 0)
									goto suspendProcessUnlockFront;
								if (kscePowerSetCameraBusClockFrequency(0) < 0)
									goto suspendProcessUnlockBoth;
								cameraDriver->busClockFast = 0;
								ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
								ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
								work.timeout = 0xffffffff;
								if (ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &work.timeout) < 0)
									goto suspendProcessDone;
								work.timeout = 0xffffffff;
								if (ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &work.timeout) < 0)
									goto suspendProcessUnlockFront;
								if (SceClockgenForDriver_82F39378() < 0)
									goto suspendProcessUnlockBoth;
								cameraDriver->clockEnabled = 0;
								ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
								ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
							}
						}
					}
					intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
					cameraSetProcessSuspendState(devnum, cameraDriver->pendingSuspendProcessId, 2);
					device->busy = 0;
					ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				}
				goto suspendProcessDone;
suspendProcessUnlockBoth:
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
				ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
				goto suspendProcessDone;
suspendProcessUnlockFront:
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
suspendProcessDone:
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x20) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				requested = 0;
				work.state = 0;
				for (devnum = 0; devnum != 2; devnum++) {
					driver = cameraDriver;
					device = &driver->device[devnum];
					if (driver->activeEnabled && driver->powerEnabled &&
					    driver->activeProcessId != device->activeProcessId) {
						intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
						result = cameraIsProcessActive(devnum, cameraDriver->pendingSuspendProcessId);
						ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
						if (result) {
							intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
							result = sceCameraConfigStartPid(devnum, cameraDriver->pendingSuspendProcessId, 1);
							ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
							if (result >= 0)
								cameraResumeDevice(devnum);
						}
					}
					intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
					ksceKernelClearEventFlag(device->eventFlagId, ~4u);
					cameraSetProcessSuspendState(devnum, cameraDriver->pendingSuspendProcessId, 0);
					device->busy = 0;
					if (cameraGetProcessRequestedSuspendState(devnum, cameraDriver->pendingSuspendProcessId,
										 &work.state) == 0 && work.state == 1)
						requested = 1;
					ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				}
				if (requested)
					ksceKernelSetEventFlag(cameraDriver->eventFlagId, 8);
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x100) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				for (devnum = 0; devnum != 2; devnum++) {
					driver = cameraDriver;
					device = &driver->device[devnum];
					if (driver->activeEnabled && driver->powerEnabled) {
						intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
						result = cameraHasStartedProcess(devnum, 0);
						ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
						if (result) {
							ksceKernelClearEventFlag(device->eventFlagId, ~7u);
							ksceKernelSetEventFlag(device->eventFlagId, 4);
							driver = cameraDriver;
							hardwareInfo = driver->hardwareInfo;
							head = devnum;
							if ((hardwareInfo & 0xdf0000) == 0x100000 ||
							    (hardwareInfo & 0xff0000) == 0x310000 ||
							    (hardwareInfo & 0xfe0000) == 0x400000)
								head = 1 - devnum;
							if (driver->busClockFast) {
								CameraDevice *activeDevice = &driver->device[devnum];
								ksceKernelClearEventFlag(activeDevice->eventFlagId, ~3u);
								SceCsiForDriver_A1D1805D(head);
								activeDevice->setPowerState(devnum, 0);
								SceCsiForDriver_4AE39F26(head);
								activeDevice->irqFrameCounter = 0;
							}
						}
					}
					ksceGpioSetPortMode(0, devnum ? 11 : 10, 0);
					ksceGpioPortClear(0, devnum ? 11 : 10);
					if (devnum == 1 && cameraDriver->gpioPowerEnabled) {
						ksceGpioPortClear(0, 9);
						cameraDriver->gpioPowerEnabled = 0;
					}
					intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
					device->busy = 0;
					device->suspendState = 2;
					ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				}
				work.timeout = 0xffffffff;
				if (ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &work.timeout) < 0)
					goto suspendDevicesDone;
				work.timeout = 0xffffffff;
				if (ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &work.timeout) < 0)
					goto suspendDevicesUnlockFront;
				if (kscePowerSetCameraBusClockFrequency(0) < 0)
					goto suspendDevicesUnlockBoth;
				cameraDriver->busClockFast = 0;
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
				ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
				work.timeout = 0xffffffff;
				if (ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &work.timeout) < 0)
					goto suspendDevicesDone;
				work.timeout = 0xffffffff;
				if (ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &work.timeout) < 0)
					goto suspendDevicesUnlockFront;
				if (SceClockgenForDriver_82F39378() < 0)
					goto suspendDevicesUnlockBoth;
				cameraDriver->clockEnabled = 0;
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
				ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
				cameraDriver->suspendState = 2;
				goto suspendDevicesDone;
suspendDevicesUnlockBoth:
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
				ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
				goto suspendDevicesDone;
suspendDevicesUnlockFront:
				ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
suspendDevicesDone:
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x400) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				requested = 0;
				for (devnum = 0; devnum != 2; devnum++) {
					driver = cameraDriver;
					device = &driver->device[devnum];
					if (!driver->gpioPowerEnabled) {
						ksceGpioSetPortMode(0, 9, 0);
						ksceGpioPortClear(0, 9);
						sceKernelDelayThread(1000);
						ksceGpioPortSet(0, 9);
						cameraDriver->gpioPowerEnabled = 1;
					}
					ksceGpioSetPortMode(0, devnum ? 11 : 10, 0);
					ksceGpioSetPortMode(0, devnum ? 11 : 10, 0);
					ksceGpioPortSet(0, devnum ? 11 : 10);
					driver = cameraDriver;
					if (driver->activeEnabled && driver->powerEnabled && cameraResumeDevice(devnum) < 0)
						continue;
					intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
					ksceKernelClearEventFlag(device->eventFlagId, ~4u);
					device->suspendState = 0;
					device->busy = 0;
					if (device->requestedSuspendState == 1)
						requested = 1;
					ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				}
				if (requested)
					ksceKernelSetEventFlag(cameraDriver->eventFlagId, 0x100);
				cameraDriver->suspendState = 0;
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x1000) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				processId = cameraDriver->activeProcessId;
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
				processStatus = cameraGetProcessDeviceStatus(processId);
				hasProcessStatus = processStatus > 0;
				cameraDriver->processSwitchInProgress = 1;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				if (hasProcessStatus) {
					for (devnum = 0; devnum != 2; devnum++, processStatus <<= 8) {
						driver = cameraDriver;
						device = &driver->device[devnum];
						deviceFlags = (unsigned int)processStatus >> 8 & 0xff;
						previousProcessId = device->activeProcessId;
						device->activeProcessId = processId;
						if (!(deviceFlags & 4))
							goto switchedDevice;
						if (deviceFlags & 0x20) {
							if (driver->activeEnabled && driver->powerEnabled) {
								hardwareInfo = driver->hardwareInfo;
								head = devnum;
								if ((hardwareInfo & 0xdf0000) == 0x100000 ||
								    (hardwareInfo & 0xff0000) == 0x310000 ||
								    (hardwareInfo & 0xfe0000) == 0x400000)
									head = 1 - devnum;
								if (driver->busClockFast) {
									ksceKernelClearEventFlag(device->eventFlagId, ~3u);
									SceCsiForDriver_A1D1805D(head);
									device->setPowerState(devnum, 0);
									SceCsiForDriver_4AE39F26(head);
									device->irqFrameCounter = 0;
								}
							}
							intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
							cameraSetProcessState(processId, devnum, 1);
							ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
							goto switchedDevice;
						}
						if (!(deviceFlags & 0x1a))
							goto switchedDevice;
						if (driver->activeEnabled && driver->powerEnabled) {
							hardwareInfo = driver->hardwareInfo;
							head = devnum;
							if ((hardwareInfo & 0xdf0000) == 0x100000 ||
							    (hardwareInfo & 0xff0000) == 0x310000 ||
							    (hardwareInfo & 0xfe0000) == 0x400000)
								head = 1 - devnum;
							if (driver->busClockFast) {
								ksceKernelClearEventFlag(device->eventFlagId, ~3u);
								SceCsiForDriver_A1D1805D(head);
								device->setPowerState(devnum, 0);
								SceCsiForDriver_4AE39F26(head);
								device->irqFrameCounter = 0;
							}
							if (!cameraHasStartedProcess(1 - devnum, 1)) {
								cameraSetBusClockFrequency(0);
								work.timeout = 0xffffffff;
								if (ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &work.timeout) >= 0) {
									work.timeout = 0xffffffff;
									if (ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &work.timeout) >= 0) {
										if (SceClockgenForDriver_82F39378() >= 0)
											cameraDriver->clockEnabled = 0;
										ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
										ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
									} else {
										ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
									}
								}
							}
						}
						intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
						sceCameraConfigSetActiveStatusPid(devnum, previousProcessId, 2);
						ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
						work.state = 0;
						if (cameraGetProcessAlternateFrameBuffers(0, processId, &work.state) >= 0 && work.state)
							frameBuffers = cameraAlternateFrameBufferBase;
						else
							frameBuffers = cameraSramFrameBufferBase;
						__builtin_memcpy(cameraFrameBufferBase, frameBuffers, sizeof(cameraSramFrameBufferBase));
						if (deviceFlags & 2) {
							intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
							result = sceCameraConfigStartPid(devnum, processId, 1);
							ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
							if (result < 0)
								goto switchedDevice;
							driver = cameraDriver;
							if (driver->activeEnabled && driver->powerEnabled && cameraResumeDevice(devnum) < 0)
								goto switchedDevice;
							intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
							if (deviceFlags & 0x10)
								cameraSetProcessState(processId, devnum, 2);
							sceCameraConfigSetActiveStatusPid(devnum, processId, 0);
							ksceKernelClearEventFlag(device->eventFlagId, ~4u);
							device->pendingProcessId = processId;
							ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
						}
switchedDevice:
						intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
						device->busy = 0;
						ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
					}
				}
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
				driver = cameraDriver;
				driver->processSwitchInProgress = 0;
				if (hasProcessStatus && driver->queuedActiveProcessId > 0) {
					driver->activeProcessId = driver->queuedActiveProcessId;
					ksceKernelSetEventFlag(driver->eventFlagId, 0x1000);
					driver = cameraDriver;
					driver->queuedActiveProcessId = -1;
				}
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&driver->spinlock, intrStatus);
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x2000) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
				cameraDriver->powerEnabled = 0;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				cameraSuspendDevices(1);
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x4000) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				cameraResumeDevices(1);
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
				cameraDriver->powerEnabled = 1;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x8000) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
				cameraDriver->activeEnabled = 0;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				cameraSuspendDevices(2);
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
		if (events & 0x10000) {
			cameraWaitForDeviceReads();
			work.timeout = 3000000;
			if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) >= 0) {
				cameraResumeDevices(2);
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
				cameraDriver->activeEnabled = 1;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			}
		}
	}
}

int sce_camera_set_attribute(int devnum, int attribute, int value, int flags)
{
	CameraAttributeOption option;
	CameraDevice *device;
	SceUID processId;
	int intrStatus;
	int result;

	processId = cameraGetCallerProcessId();
	if ((devnum & ~1u) != 0)
		return (int)0x802e0000;
	device = &cameraDriver->device[devnum];
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = sceCameraConfigAttribute(devnum, 1);
	if (result < 0) {
		if (result == (int)0x802e000e) {
			option.deferRegisterUpdate = 0;
			option.updateDevice = 0;
			option.process = cameraGetProcess(devnum, processId);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			result = device->setAttribute(devnum, attribute, value, flags, &option);
			if (result < 0)
				return result;
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			result = sceCameraConfigSetAttribute(devnum, attribute, value, flags, 0);
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		return result;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	option.wRange = device->config.wRange;
	option.deferRegisterUpdate = 0;
	option.updateDevice = 1;
	result = device->setAttribute(devnum, attribute, value, flags, &option);
	if (result < 0)
		return result;
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = sceCameraConfigSetAttribute(devnum, attribute, value, flags, 1);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (result < 0)
		return result;
	if (attribute == 14) {
		device = &cameraDriver->device[devnum];
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		device->waitingForReverseFrame = 1;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	}
	return 0;
}

int module_start(SceSize arglen, const void *argp)
{
	return (unsigned int)sce_camera_init() >> 31;
}

int cameraSetAlternateFrameBuffers(unsigned int mode)
{
	SceUInt32 callerContext;
	SceUInt32 timeout;
	SceUID processId;
	int result;

	ENTER_SYSCALL(callerContext);
	processId = cameraGetCallerProcessId();
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(callerContext);
		return (int)0x802e000a;
	}
	if (mode > 1) {
		result = (int)0x802e0000;
	} else {
		if (cameraSetProcessAlternateFrameBuffers(0, processId, mode) >= 0 && mode)
			__builtin_memcpy(cameraFrameBufferBase, cameraAlternateFrameBufferBase, sizeof(cameraAlternateFrameBufferBase));
		else
			__builtin_memcpy(cameraFrameBufferBase, cameraSramFrameBufferBase, sizeof(cameraSramFrameBufferBase));
		result = 0;
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(callerContext);
	return result;
}

int SceCameraForDriver_D259707C(SceUID processId, unsigned int mode)
{
	SceUInt32 timeout = 3000000;
	void *(*frameBuffers)[3];
	int result;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	if (mode > 1) {
		result = (int)0x802e0000;
	} else {
		if (cameraSetProcessAlternateFrameBuffers(0, processId, mode) >= 0 && mode)
			frameBuffers = cameraAlternateFrameBufferBase;
		else
			frameBuffers = cameraSramFrameBufferBase;
		__builtin_memcpy(cameraFrameBufferBase, frameBuffers, sizeof(cameraSramFrameBufferBase));
		result = 0;
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int SceCameraForDriver_45EB69C9(void)
{
	return sce_camera_init();
}

int SceCameraForDriver_F7A0C99F(void)
{
	return cameraTerminate();
}

int SceCameraForDriver_E487870A(int devnum, SceCameraInfo *pInfo)
{
	SceUInt32 context;
	SceUInt32 timeout;
	SceUID processId;
	CameraDriver *driver;
	CameraProcess *process;
	SceKernelIntrStatus intr;
	unsigned int resolution, framerate;
	SceUInt32 result;
	int status;

	ENTER_SYSCALL(context);
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	processId = cameraGetCallerProcessId();
	result = devnum & ~1u;
	if (result != 0 || pInfo == NULL || pInfo->size - 40 > 8) {
		result = (int)0x802e0000;
		goto out;
	}
	driver = cameraDriver;
	if (!driver->initialized) {
		result = (int)0x802e0002;
		goto out;
	}
	if (pInfo->priority != 0 || pInfo->buffer > 1u) {
		result = (int)0x802e0000;
		goto out;
	}
	if ((unsigned int)(pInfo->format - 1) > 5) {
		result = (int)0x802e0007;
		goto out;
	}
	resolution = pInfo->resolution;
	switch (resolution) {
	case 1:
		break;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 8:
		if (pInfo->format == 6) {
			result = (int)0x802e0000;
			goto out;
		}
		break;
	default:
		result = (int)0x802e0008;
		goto out;
	}
	framerate = pInfo->framerate;
	if (framerate == 20)
		goto setSize;
	if (framerate <= 20) {
		if (framerate != 7) {
			if (framerate > 7) {
				if (framerate != 10 && framerate != 15)
					goto badFramerate;
			} else if (framerate != 3 && framerate != 5) {
				goto badFramerate;
			}
		}
	} else if (framerate != 30) {
		if (framerate > 30) {
			if (framerate != 60) {
				unsigned int highSpeedResolution = (SceUInt16)(resolution - 2);

				if (framerate != 120 || highSpeedResolution > 3 ||
				    !((1u << highSpeedResolution) & 0xb))
					goto badFramerate;
			}
			if (driver->device[devnum].sensorType == 1)
				goto badFramerate;
		} else if (framerate - 24 > 1) {
			goto badFramerate;
		}
	}
setSize:
	switch (resolution - 1) {
	case 0:
		pInfo->width = 640;
		pInfo->height = 480;
		break;
	case 1:
		pInfo->width = 320;
		pInfo->height = 240;
		break;
	case 2:
		pInfo->width = 160;
		pInfo->height = 120;
		break;
	case 3:
		pInfo->width = 352;
		pInfo->height = 288;
		break;
	case 4:
		pInfo->width = 176;
		pInfo->height = 144;
		break;
	case 5:
		pInfo->width = 480;
		pInfo->height = 272;
		break;
	case 7:
		pInfo->width = 640;
		pInfo->height = 360;
		break;
	default:
		pInfo->width = 0;
		pInfo->height = 0;
		break;
	}
	if (pInfo->range > 1u) {
		result = (int)0x802e0011;
		goto out;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
	status = sceCameraConfigOpen(devnum, pInfo);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	if (status < 0) {
		result = status;
		goto out;
	}
	process = cameraGetProcess(devnum, processId);
	if (process == NULL) {
		result = (int)0x802e0000;
		goto out;
	}
	if (process->config.wBuffer == 0) {
		timeout = 0;
		status = SceProcessmgrForDriver_D141C076(processId, &timeout);
		if (status < 0) {
			result = status;
			goto out;
		}
		if (timeout >= 0x1800000) {
			status = cameraLockFrameBufferRanges(devnum, process, NULL);
			if (status < 0) {
				result = status;
				goto out;
			}
		}
	}
	ksceKernelClearEventFlag(driver->device[devnum].eventFlagId, ~7u);
	goto out;
badFramerate:
	result = (int)0x802e0009;
out:
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_0E9B4B9E(int devnum)
{
	SceUInt32 context;
	SceUInt32 timeout;
	SceUID processId;
	CameraProcess *process;
	SceKernelIntrStatus intr;
	int result, hasOtherProcess;

	ENTER_SYSCALL(context);
	processId = cameraGetCallerProcessId();
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	result = devnum & ~1u;
	if (result != 0) {
		result = (int)0x802e0000;
		goto out;
	}
	if (cameraDriver->initialized == 0) {
		result = (int)0x802e0002;
		goto out;
	}
	cameraStopProcess(devnum, processId);
	process = cameraGetProcess(devnum, processId);
	if (process == NULL) {
		result = (int)0x802e0004;
		goto out;
	}
	if (process->config.wBuffer == 0) {
		timeout = 0;
		result = SceProcessmgrForDriver_D141C076(processId, &timeout);
		if (result < 0)
			goto out;
		if (timeout >= 0x1800000 || process->rangesLocked != 0) {
			result = cameraUnlockFrameBufferRanges(devnum, process, NULL, 0, 0);
			if (result < 0)
				goto out;
		}
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = sceCameraConfigClosePid(devnum, processId);
	if (result < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
		goto out;
	}
	hasOtherProcess = cameraHasOtherProcess(devnum, processId);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	if (hasOtherProcess == 0) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		cameraHasOpenProcess(1 - devnum);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	}
	result = 0;
out:
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_5338BE86(int devnum)
{
	SceUInt32 context, timeout, eventPattern;
	SceUID processId;
	CameraProcess *process;
	CameraDriver *driver;
	CameraDevice *device;
	SceKernelIntrStatus intr;
	void *(*frameBuffers)[3];
	int result, started, hasStartedOther;

	ENTER_SYSCALL(context);
	processId = cameraGetCallerProcessId();
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	process = cameraGetProcess(devnum, processId);
	if (process == NULL) {
		result = (int)0x802e0004;
		goto earlyError;
	}
	if (process->config.wBuffer == 0) {
		timeout = 0;
		result = SceProcessmgrForDriver_D141C076(processId, &timeout);
		if (result < 0)
			goto earlyError;
		if (timeout < 0x1800000) {
			result = cameraLockFrameBufferRanges(devnum, process, NULL);
			if (result < 0)
				goto earlyError;
		}
	}
	result = devnum & ~1u;
	if (result != 0) {
		result = (int)0x802e0000;
		goto cleanup;
	}
	driver = cameraDriver;
	if (driver->initialized == 0) {
		result = (int)0x802e0002;
		goto cleanup;
	}
	if (cameraAllowSimultaneousDevices == 0) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		if (cameraHasStartedProcess(1 - devnum, 1) != 0 ||
		    cameraIsProcessStarted(1 - devnum, processId) != 0) {
			result = (int)0x802e0012;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
			goto cleanup;
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
		timeout = 0;
		if (cameraGetProcessAlternateFrameBuffers(0, processId, (int *)&timeout) >= 0 && timeout != 0)
			frameBuffers = cameraAlternateFrameBufferBase;
		else
			frameBuffers = cameraSramFrameBufferBase;
		__builtin_memcpy(cameraFrameBufferBase, frameBuffers, sizeof(cameraSramFrameBufferBase));
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	started = sceCameraConfigStartPid(devnum, processId, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	if (started < 0) {
		if (started == (int)0x802e000e)
			goto out;
		result = started;
		goto cleanup;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	hasStartedOther = cameraHasStartedProcess(1 - devnum, 1);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	if (hasStartedOther == 0) {
		result = cameraSetSensorClockEnabled(1);
		if (result < 0)
			goto cleanup;
		timeout = -1;
		result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout);
		if (result < 0)
			goto cleanup;
		timeout = -1;
		result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout);
		if (result < 0) {
			ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
			goto cleanup;
		}
		result = kscePowerSetCameraBusClockFrequency(166);
		if (result < 0) {
			ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
			ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
			goto cleanup;
		}
		cameraDriver->busClockFast = 1;
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	}
	result = cameraCsiStart(devnum);
	if (result < 0)
		goto cleanup;
	if (cameraDriver->gpioPowerEnabled == 0) {
		ksceGpioSetPortMode(0, 9, 0);
		ksceGpioPortClear(0, 9);
		sceKernelDelayThread(1000);
		ksceGpioPortSet(0, 9);
		cameraDriver->gpioPowerEnabled = 1;
	}
	result = ksceGpioSetPortMode(0, devnum != 0 ? 11 : 10, 0);
	if (result < 0)
		goto cleanup;
	device = &driver->device[devnum];
	device->setPowerState(devnum, 1);
	result = device->resetSensor(devnum);
	if (result < 0)
		goto cleanup;
	result = device->initSensor(devnum);
	if (result < 0)
		goto cleanup;
	result = device->setFormat(devnum, device->config.wFormat);
	if (result < 0)
		goto cleanup;
	result = device->setResolution(devnum, device->config.wResolution, device->config.wFramerate);
	if (result < 0)
		goto cleanup;
	result = device->setFramerate(devnum, device->config.wFramerate, device->config.wResolution);
	if (result < 0)
		goto cleanup;
	result = device->setRange(devnum, device->config.wRange);
	if (result < 0)
		goto cleanup;
	device = &cameraDriver->device[devnum];
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	device->waitingForReverseFrame = 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = sceCameraConfigSetStartedPid(devnum, processId, 1);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intr);
	timeout = driver->device[devnum].config.frameTimeout;
	ksceKernelWaitEventFlag(driver->device[devnum].eventFlagId, 3, 1, &eventPattern, &timeout);
	if (result >= 0)
		goto out;
cleanup:
	process = cameraGetProcess(devnum, processId);
	if (process != NULL && process->config.wBuffer == 0) {
		timeout = 0;
		if (SceProcessmgrForDriver_D141C076(processId, &timeout) >= 0 && timeout < 0x1800000)
			cameraUnlockFrameBufferRanges(devnum, process, NULL, 0, 0);
	}
out:
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
earlyError:
	EXIT_SYSCALL(context);
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int SceCameraForDriver_8C3354E5(int devnum)
{
	SceUInt32 context, timeout;
	SceUID processId;
	CameraProcess *process;
	int result;

	ENTER_SYSCALL(context);
	processId = cameraGetCallerProcessId();
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	result = cameraStopProcess(devnum, processId);
	if (result < 0) {
		process = cameraGetProcess(devnum, processId);
		if (process != NULL && process->config.wBuffer == 0) {
			timeout = 0;
			if (SceProcessmgrForDriver_D141C076(processId, &timeout) >= 0 && timeout < 0x1800000)
				cameraUnlockFrameBufferRanges(devnum, process, NULL, 0, 0);
		}
	} else {
		process = cameraGetProcess(devnum, processId);
		if (process != NULL) {
			result = 0;
			if (process->config.wBuffer == 0) {
				timeout = 0;
				result = SceProcessmgrForDriver_D141C076(processId, &timeout);
				if (result >= 0 && timeout < 0x1800000)
					result = cameraUnlockFrameBufferRanges(devnum, process, NULL, 0, 0);
			}
		} else {
			result = (int)0x802e0000;
		}
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_2D12FDF3(int devnum, SceCameraReadV2 *read)
{
	SceUInt32 context, timeout, events, raw8Format;
	SceUInt32 exposureTimeAndGap[2];
	SceUID processId;
	CameraProcess *process;
	CameraDevice *device, *currentDevice;
	CameraFrameBuffer *frame;
	SceIftuConvParams conversion;
	SceIftuPlaneState source;
	SceIftuFrameBuf destination;
	SceUInt64 previousFrame, currentFrame, timestamp;
	SceSize imageSize, chromaSize;
	int intrStatus, result, sampleExposure, sampleColor, reverseFrame, delay, yuv420;

	ENTER_SYSCALL(context);
	processId = cameraGetCallerProcessId();
	if ((devnum & ~1u) != 0 || read == NULL ||
	    (unsigned int)read->dwMode > 1 ||
	    (unsigned int)read->dwExposureTimeMode > 1 || read->sizeThis - 24 > 48) {
		result = (int)0x802e0000;
		goto out;
	}
	device = &cameraDriver->device[devnum];
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	if (!cameraDriver->initialized) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		result = (int)0x802e0002;
		goto out;
	}
	process = cameraGetProcess(devnum, processId);
	if (process == NULL) {
		result = (int)0x802e0004;
		goto out;
	}
	if (process->config.wBuffer == 1) {
		result = cameraLockFrameBufferRanges(devnum, process, read);
		if (result < 0)
			goto finishRead;
	}
	result = sceCameraConfigRead(devnum, read);
	if (result < 0 && result != -2) {
		switch (result) {
		case (int)0x802e0015:
			read->dwStatus = -3;
			break;
		case (int)0x802e0014:
			read->dwStatus = -2;
			break;
		case (int)0x802e000e:
			read->dwStatus = -1;
			break;
		default:
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			if (process->config.wBuffer == 1)
				cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
			goto out;
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		result = 0;
		if (process->config.wBuffer == 1)
			result = cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
		sceKernelDelayThread(2000);
		goto out;
	}
	cameraDriver->device[devnum].readInProgress = 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		if (process->config.wBuffer == 1)
			cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
		result = (int)0x802e000a;
		goto finishRead;
	}
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	read->dwStatus = device->irqFrameCounter < 11 ? 2 : 0;
	previousFrame = sceCameraConfigGetProcPrevFrame(devnum, processId);
	currentFrame = sceCameraConfigGetProcFrame(devnum, processId);
	ksceKernelClearEventFlag(device->eventFlagId, ~3u);
	reverseFrame = device->waitingForReverseFrame;
	sampleExposure = 0;
	timestamp = device->timestamp;
	if (read->dwExposureTimeMode == 1 &&
	    (device->lastExposureSampleTime == 0 || read->dwStatus == 2 ||
	     timestamp - device->lastExposureSampleTime > 1000000)) {
		device->lastExposureSampleTime = timestamp;
		sampleExposure = 1;
	}
	sampleColor = 0;
	if (device->lastColorCountSampleTime == 0 ||
	    timestamp - device->lastColorCountSampleTime > 1000000) {
		device->lastColorCountSampleTime = timestamp;
		sampleColor = 1;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (read->dwExposureTimeMode == 1) {
		result = sampleExposure;
		if (sampleExposure) {
			exposureTimeAndGap[0] = 0;
			exposureTimeAndGap[1] = 0;
			currentDevice = &cameraDriver->device[devnum];
			result = currentDevice->getExposure(devnum, currentDevice->config.wFramerate,
					exposureTimeAndGap);
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			currentDevice->exposureTime = exposureTimeAndGap[0];
			currentDevice->exposureGap = exposureTimeAndGap[1];
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			if (result < 0) {
				intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
				cameraDriver->device[devnum].readInProgress = 0;
				ksceKernelSetEventFlag(cameraDriver->eventFlagId,
						devnum ? 0x40000 : 0x20000);
				ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
				if (process->config.wBuffer == 1)
					cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
				goto unlock;
			}
		}
		read->dwExposureTime = device->exposureTime;
		read->dwExposureTimeGap = device->exposureGap;
	} else {
		result = 0;
	}
	if (sampleColor) {
		ksceKernelSetEventFlag(cameraDriver->eventFlagId, devnum ? 2 : 1);
		result = 0;
	}
	if (read->sizeThis > 47 && previousFrame == currentFrame) {
		if (read->dwMode != 0) {
			if (read->dwMode != 1)
				goto transferFrame;
			read->dwStatus = 1;
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			read->qwFrame = device->frame;
			read->qwTimestamp = sceCameraConfigGetProcTimestamp(devnum, processId);
			cameraDriver->device[devnum].readInProgress = 0;
			ksceKernelSetEventFlag(cameraDriver->eventFlagId, devnum ? 0x40000 : 0x20000);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			result = 0;
			if (process->config.wBuffer == 1)
				result = cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
			ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			sceKernelDelayThread(2000);
			goto out;
		}
		timeout = cameraDriver->device[devnum].config.frameTimeout;
		if (reverseFrame) {
			ksceKernelWaitEventFlag(cameraDriver->device[devnum].eventFlagId, 2, 1,
					&events, &timeout);
			ksceKernelClearEventFlag(cameraDriver->device[devnum].eventFlagId, ~2u);
		}
		result = ksceKernelWaitEventFlag(cameraDriver->device[devnum].eventFlagId,
				7, 1, &events, &timeout);
		if ((events & 4) != 0 || result < 0) {
			intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
			delay = 0;
			if ((events & 4) != 0 || result == -1) {
				delay = 2000;
				read->dwStatus = 1;
				result = 0;
			} else if (result == (int)0x80028005) {
				read->dwStatus = 1;
				result = 0;
			}
			cameraDriver->device[devnum].readInProgress = 0;
			ksceKernelSetEventFlag(cameraDriver->eventFlagId, devnum ? 0x40000 : 0x20000);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			if (process->config.wBuffer == 1)
				cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
			ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
			if (delay)
				sceKernelDelayThread(delay);
			goto out;
		}
	}
transferFrame:
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	frame = &device->frameBuffer[device->irqFrameIndex];
	read->qwTimestamp = sceCameraConfigGetProcTimestamp(devnum, processId);
	read->qwFrame = sceCameraConfigGetProcFrame(devnum, processId);
	sceCameraConfigSetProcPrevFrame(devnum, processId, read->qwFrame);
	device->previousReadFrame = device->frame;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (device->config.wFormat < 3 || device->config.wFormat > 5) {
		if (device->config.sizeIBase && device->cachedFrameBuffer.pvIBase) {
			if (device->isUserThread)
				result = ksceKernelCopyToUser(device->cachedFrameBuffer.pvIBase,
						frame->pvIBase, device->config.sizeIBase);
			else
				memcpy(device->cachedFrameBuffer.pvIBase, frame->pvIBase,
						device->config.sizeIBase);
		}
		if (device->config.sizeUBase && device->cachedFrameBuffer.pvUBase) {
			if (device->isUserThread)
				result = ksceKernelCopyToUser(device->cachedFrameBuffer.pvUBase,
						frame->pvUBase, device->config.sizeUBase);
			else
				memcpy(device->cachedFrameBuffer.pvUBase, frame->pvUBase,
						device->config.sizeUBase);
		}
		if (device->config.sizeVBase && device->cachedFrameBuffer.pvVBase) {
			if (device->isUserThread)
				result = ksceKernelCopyToUser(device->cachedFrameBuffer.pvVBase,
						frame->pvVBase, device->config.sizeVBase);
			else
				memcpy(device->cachedFrameBuffer.pvVBase, frame->pvVBase,
						device->config.sizeVBase);
		}
	} else {
		yuv420 = device->config.wFormat == 3;
		source.fb.pixelformat = SCE_IFTU_PIXELFORMAT_YUV422;
		source.fb.width = device->config.wWidth;
		source.fb.height = device->config.wHeight;
		source.fb.leftover_stride = 0;
		source.fb.leftover_align = ((SceUInt8)(source.fb.width >> 1) + 7) & 8;
		source.fb.paddr0 = frame->paddrIBase;
		source.fb.paddr1 = frame->paddrUBase;
		source.fb.paddr2 = frame->paddrVBase;
		source.scaleX = 0x10000;
		source.scaleY = 0x10000;
		source.dst_x = 0;
		source.dst_y = 0;
		source.sampleX = 0;
		source.sampleY = 0;
		source.vtop_padding = 0;
		source.vbot_padding = 0;
		source.hleft_padding = 0;
		source.hright_padding = 0;
		destination.width = source.fb.width;
		destination.height = source.fb.height;
		destination.leftover_stride = 0;
		if (device->config.sizeThis > 43)
			destination.leftover_stride = device->config.wPitch;
		destination.paddr0 = device->cachedFrameBuffer.paddrIBase;
		if (yuv420) {
			destination.pixelformat = SCE_IFTU_PIXELFORMAT_YUV420;
			destination.leftover_align = source.fb.leftover_align;
			destination.paddr1 = device->cachedFrameBuffer.paddrUBase;
			destination.paddr2 = device->cachedFrameBuffer.paddrVBase;
			imageSize = 640 * source.fb.height;
			chromaSize = 320 * (source.fb.height >> 1);
		} else {
			destination.pixelformat = device->config.wFormat == 4 ?
					SCE_IFTU_PIXELFORMAT_RGBX8888 : SCE_IFTU_PIXELFORMAT_BGRX8888;
			imageSize = 2560 * source.fb.height;
			chromaSize = 0;
		}
		if (device->cachedFrameBuffer.pvIBase && imageSize &&
		    ksceKernelGetMemBlockMemtypeByAddr(device->cachedFrameBuffer.pvIBase) != 0x8000)
			ksceKernelDcacheCleanInvalidateRange(device->cachedFrameBuffer.pvIBase, imageSize);
		if (yuv420) {
			if (device->cachedFrameBuffer.pvUBase && chromaSize &&
			    ksceKernelGetMemBlockMemtypeByAddr(device->cachedFrameBuffer.pvUBase) != 0x8000)
				ksceKernelDcacheCleanInvalidateRange(device->cachedFrameBuffer.pvUBase, chromaSize);
			if (device->cachedFrameBuffer.pvVBase && chromaSize &&
			    ksceKernelGetMemBlockMemtypeByAddr(device->cachedFrameBuffer.pvVBase) != 0x8000)
				ksceKernelDcacheCleanInvalidateRange(device->cachedFrameBuffer.pvVBase, chromaSize);
		}
		conversion.size = sizeof(conversion);
		conversion.filterMode = 1;
		conversion.csc_params1 = (SceIftuCscParams *)&cameraRgbToYuvCsc;
		conversion.csc_params2 = (SceIftuCscParams *)&cameraYuvToRgbCsc;
		conversion.csc_control = 0;
		conversion.backgroundColor[0] = 0;
		conversion.backgroundColor[1] = 0;
		conversion.backgroundColor[2] = 0;
		conversion.alpha = 255;
		conversion.fieldMode = 0;
		result = ksceIftuCsc(&destination, &source, &conversion);
		if (yuv420) {
			if (device->cachedFrameBuffer.pvIBase && imageSize &&
			    ksceKernelGetMemBlockMemtypeByAddr(device->cachedFrameBuffer.pvIBase) != 0x8000)
				ksceKernelDcacheCleanInvalidateRange(device->cachedFrameBuffer.pvIBase, imageSize);
			if (device->cachedFrameBuffer.pvUBase && chromaSize &&
			    ksceKernelGetMemBlockMemtypeByAddr(device->cachedFrameBuffer.pvUBase) != 0x8000)
				ksceKernelDcacheCleanInvalidateRange(device->cachedFrameBuffer.pvUBase, chromaSize);
			if (device->cachedFrameBuffer.pvVBase && chromaSize &&
			    ksceKernelGetMemBlockMemtypeByAddr(device->cachedFrameBuffer.pvVBase) != 0x8000)
				ksceKernelDcacheCleanInvalidateRange(device->cachedFrameBuffer.pvVBase, chromaSize);
		} else if (device->cachedFrameBuffer.pvIBase && imageSize &&
			   ksceKernelGetMemBlockMemtypeByAddr(device->cachedFrameBuffer.pvIBase) != 0x8000) {
			ksceKernelDcacheInvalidateRange(device->cachedFrameBuffer.pvIBase, imageSize);
		}
		if (process->config.wBuffer == 1) {
			if (result < 0)
				cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
			else
				result = cameraUnlockFrameBufferRanges(devnum, process, read, 0, 0);
		}
	}
	raw8Format = 0;
	currentDevice = &cameraDriver->device[devnum];
	if (currentDevice->config.wFormat == 6) {
		if (currentDevice->getRaw8Format(devnum, currentDevice->config.reverse, &raw8Format) != 0)
			currentDevice->raw8Format = 0;
		else
			currentDevice->raw8Format = raw8Format;
	} else {
		currentDevice->raw8Format = 0;
	}
	read->dwRaw8Format = device->raw8Format;
finishRead:
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	cameraDriver->device[devnum].readInProgress = 0;
	ksceKernelSetEventFlag(cameraDriver->eventFlagId, devnum ? 0x40000 : 0x20000);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
unlock:
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
out:
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_C2E71BE8(int devnum, int attribute, int *pValue, int flags)
{
	SceUInt32 context;
	union {
		SceUInt32 timeout;
		CameraAttributeOption option;
	} work;
	CameraDevice *device;
	int intrStatus, result;

	ENTER_SYSCALL(context);
	work.timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	if (attribute == 25 || (devnum & ~1u) != 0 || pValue == NULL) {
		result = (int)0x802e0000;
	} else {
		device = &cameraDriver->device[devnum];
		work.option.wRange = device->config.wRange;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		result = sceCameraConfigAttribute(devnum, 0);
		if (result < 0) {
			if (result == (int)0x802e000e)
				result = sceCameraConfigGetAttribute(devnum, attribute, pValue);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		} else {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			result = device->getAttribute(devnum, attribute, pValue, flags, &work.option);
			result &= result >> 31;
		}
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}
int SceCameraForDriver_FE3C170C(int devnum, int attribute, int *pValue, int flags)
{
	SceUInt32 context;
	union {
		SceUInt32 timeout;
		CameraAttributeOption option;
	} work;
	CameraDevice *device;
	int intrStatus, result;

	ENTER_SYSCALL(context);
	work.timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &work.timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	if ((attribute != 13 && attribute != 25) || (devnum & ~1u) != 0 || pValue == NULL) {
		result = (int)0x802e0000;
	} else {
		device = &cameraDriver->device[devnum];
		work.option.wRange = device->config.wRange;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		result = sceCameraConfigAttribute(devnum, 0);
		if (result < 0) {
			if (result == (int)0x802e000e)
				result = sceCameraConfigGetAttribute(devnum, attribute, pValue);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		} else {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			result = device->getAttribute(devnum, attribute, pValue, flags, &work.option);
			result &= result >> 31;
		}
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_D56A12BD(int devnum, int attribute, int value, int flags)
{
	SceUInt32 context, timeout;
	int result;

	ENTER_SYSCALL(context);
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	if (attribute == 25 || (attribute == 13 && value == 0xf000))
		result = (int)0x802e0000;
	else
		result = sce_camera_set_attribute(devnum, attribute, value, flags);
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_B312CE90(int devnum, int attribute, int value, int flags)
{
	SceUInt32 context, timeout;
	int result;

	ENTER_SYSCALL(context);
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	if (attribute == 13 || attribute == 25)
		result = sce_camera_set_attribute(devnum, attribute, value, flags);
	else
		result = (int)0x802e0000;
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_E81EADE1(SceUID processId)
{
	int intrStatus;

	if (processId < 0)
		return processId;
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	if (cameraDriver->processSwitchInProgress != 0)
		cameraDriver->queuedActiveProcessId = processId;
	else
		cameraDriver->activeProcessId = processId;
	ksceKernelSetEventFlag(cameraDriver->eventFlagId, 0x1000);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	return 0;
}

int SceCameraForDriver_93477709(int devnum, SceUInt16 address, SceUInt8 value)
{
	SceUInt32 timeout = 3000000;
	CameraDriver *driver;
	int result, intrStatus;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	if ((devnum & ~1u) != 0) {
		result = (int)0x802e0000;
	} else {
		driver = cameraDriver;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		result = sceCameraConfigAttribute(devnum, 0);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		if (result >= 0) {
			result = driver->device[devnum].writeRegister(devnum, address, value);
			result &= result >> 31;
		}
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int SceCameraForDriver_B241F11F(int devnum, SceUInt16 address, SceUInt8 *value)
{
	SceUInt32 timeout = 3000000;
	CameraDriver *driver;
	int result, intrStatus;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	if ((devnum & ~1u) != 0) {
		result = (int)0x802e0000;
	} else {
		driver = cameraDriver;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		result = sceCameraConfigAttribute(devnum, 0);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		if (result >= 0) {
			result = driver->device[devnum].readRegister(devnum, address, value);
			result &= result >> 31;
		}
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int SceCameraForDriver_36C164BE(int devnum, const void *registers, unsigned int count)
{
	SceUInt32 timeout = 3000000;
	CameraDriver *driver;
	int result, intrStatus;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	if ((devnum & ~1u) != 0) {
		result = (int)0x802e0000;
	} else {
		driver = cameraDriver;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		result = sceCameraConfigAttribute(devnum, 0);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		if (result >= 0) {
			result = driver->device[devnum].writeRegisters(devnum, registers, count);
			result &= result >> 31;
		}
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int SceCameraForDriver_6443EEDB(int devnum)
{
	SceUInt32 timeout = 3000000;
	CameraDevice *device;
	CameraDriver *driver;
	int result, intrStatus, started;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	if ((devnum & ~1u) != 0) {
		result = (int)0x802e0000;
		goto out;
	}
	device = &cameraDriver->device[devnum];
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	result = sceCameraConfigAttribute(devnum, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (result >= 0)
		result = device->sensorType;
	if (result != (int)0x802e0004)
		goto out;
	driver = cameraDriver;
	driver->hardwareInfo = ksceSysconGetHardwareInfo();
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&driver->spinlock);
	started = cameraHasStartedProcess(devnum, 0) || cameraHasStartedProcess(1 - devnum, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (!started) {
		result = cameraSetSensorClockEnabled(1);
		if (result < 0)
			goto out;
	}
	if (!cameraDriver->gpioPowerEnabled) {
		ksceGpioSetPortMode(0, 9, 0);
		ksceGpioPortClear(0, 9);
		sceKernelDelayThread(1000);
		ksceGpioPortSet(0, 9);
		cameraDriver->gpioPowerEnabled = 1;
	}
	result = ksceGpioSetPortMode(0, devnum ? 11 : 10, 0);
	if (result < 0)
		goto out;
	result = cameraBindSensor(devnum);
	if (result < 0)
		goto out;
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	started = cameraHasStartedProcess(devnum, 0) || cameraHasStartedProcess(1 - devnum, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	if (!started) {
		timeout = ~0u;
		result = ksceKernelLockMutex(cameraDriver->device[0].mutexId, 1, &timeout);
		if (result < 0)
			goto out;
		timeout = ~0u;
		result = ksceKernelLockMutex(cameraDriver->device[1].mutexId, 1, &timeout);
		if (result < 0) {
			ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
			goto out;
		}
		result = SceClockgenForDriver_82F39378();
		if (result < 0) {
			ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
			ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
			goto out;
		}
		cameraDriver->clockEnabled = 0;
		ksceKernelUnlockMutex(cameraDriver->device[0].mutexId, 1);
		ksceKernelUnlockMutex(cameraDriver->device[1].mutexId, 1);
	}
	result = driver->device[devnum].sensorType;
out:
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int SceCameraForDriver_3590BB9B(int devnum, int enabled)
{
	SceUInt32 timeout = 3000000;
	CameraDriver *driver;
	int result, intrStatus;

	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0)
		return (int)0x802e000a;
	if ((devnum & ~1u) != 0) {
		result = (int)0x802e0000;
	} else {
		driver = cameraDriver;
		intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
		result = sceCameraConfigAttribute(devnum, 0);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
		if (result >= 0) {
			result = driver->device[devnum].setPowerState(devnum, enabled);
			result &= result >> 31;
		}
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	return result;
}

int SceCameraForDriver_DFBBA264(int devnum, int *framerate)
{
	if ((devnum & ~1u) != 0)
		return (int)0x802e0000;
	*framerate = cameraMeasuredFramerate[devnum];
	return 0;
}

int SceCameraForDriver_22E70D83(int devnum)
{
	SceUInt32 context;
	int intrStatus, result;

	ENTER_SYSCALL(context);
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	if ((devnum & ~1u) != 0) {
		result = (int)0x802e0000;
	} else if (cameraDriver->initialized) {
		result = cameraCheckActiveProcess(-1, devnum, 1);
		if (result == (int)0x802e000e)
			result = 0;
		else if (result == 0)
			result = 1;
	} else {
		result = (int)0x802e0002;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	EXIT_SYSCALL(context);
	return result;
}

int ksceCameraIsActive(SceUID processId, int *devices)
{
	int intrStatus, active;

	if (devices == NULL)
		return (int)0x802e0000;
	intrStatus = ksceKernelSpinlockLowLockCpuSuspendIntr(&cameraDriver->spinlock);
	active = 0;
	if (cameraDriver->initialized) {
		active = cameraCheckActiveProcess(processId, 0, 0) == 0;
		if (cameraDriver->initialized && cameraCheckActiveProcess(processId, 1, 0) == 0) {
			*devices = active | 2;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
			return 1;
		}
	}
	*devices = active;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&cameraDriver->spinlock, intrStatus);
	return active;
}

int SceCameraForDriver_EE3D4B1C(int devnum, SceFVector3 *pLocation)
{
	SceUInt32 context, timeout;
	const SceFVector3 *location;
	int result;

	ENTER_SYSCALL(context);
	timeout = 3000000;
	if (ksceKernelLockMutex(cameraDriver->mutexId, 1, &timeout) < 0) {
		EXIT_SYSCALL(context);
		return (int)0x802e000a;
	}
	result = 0;
	if ((devnum & ~1u) != 0 || pLocation == NULL) {
		result = (int)0x802e0000;
	} else if (cameraDriver->initialized) {
		location = &cameraDriver->location[devnum];
		pLocation->y = location->y;
		pLocation->x = location->x;
		pLocation->z = location->z;
	} else {
		result = (int)0x802e0002;
	}
	ksceKernelUnlockMutex(cameraDriver->mutexId, 1);
	EXIT_SYSCALL(context);
	return result;
}

int SceCameraForDriver_22D51107(int enabled)
{
	cameraAllowSimultaneousDevices = enabled;
	return enabled;
}

int SceCameraForDriver_96C63737(int devnum, int command, int mode)
{
	if ((devnum & ~1u) != 0 || (mode != 0 && (unsigned int)(mode - 3) > 1) || command != 2)
		return (int)0x802e0000;
	return cameraSetControlModeOV7735(devnum, command, mode);
}

int SceCameraForDriver_D0810455(int devnum, int command, SceUInt16 *result)
{
	if ((devnum & ~1u) != 0)
		return (int)0x802e0000;
	__builtin_memset(result, 0, 4 * sizeof(*result));
	if (command != 2)
		return (int)0x802e0000;
	return cameraGetProbeResultOV7735(devnum, command, result);
}

