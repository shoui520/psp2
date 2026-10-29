/* SceCamera: process-aware driver configuration. */

#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <processmgr/processmgr.h>
#include "camera_config.h"
#include <sysmem/sysmem.h>
#include <string.h>
#include <camera.h>
#include <camera/camera.h>

SceUInt32 cameraReservedState[2] = {0};
CameraProcessRegistry cameraProcessRegistry = {0};

int cameraTranslateFrameBuffer(int devnum, CameraProcess *process, int cache)
{
	SceUInt16 format = process->config.wFormat;
	CameraDevice *device = &cameraDriver->device[devnum];
	SceKernelPARange physical;
	SceKernelVARange range;
	int result;

	switch (format) {
	case 1:
	case 2:
	case 6:
		result = cache;
		if (cache) {
			result = 0;
			goto cacheVirtual;
		}
		return result;
	case 4:
	case 5:
		range.addr = (SceUInt32)process->frameBuffer.pvIBase;
		range.size = process->config.sizeIBase;
		result = sceKernelVARangeToPARange(&range, &physical);
		if (result >= 0) {
			if (cache)
				goto cachePhysical;
			goto setPhysical;
		}
		goto failed;
	case 3:
		range.addr = (SceUInt32)process->frameBuffer.pvUBase;
		range.size = process->config.sizeUBase;
		result = sceKernelVARangeToPARange(&range, &physical);
		if (result < 0)
			goto failed;
		if (cache) {
			device->cachedFrameBuffer.paddrUBase = physical.addr;
			range.addr = (SceUInt32)process->frameBuffer.pvVBase;
			range.size = process->config.sizeVBase;
			result = sceKernelVARangeToPARange(&range, &physical);
			if (result < 0)
				goto restore;
			device->cachedFrameBuffer.paddrVBase = physical.addr;
			range.addr = (SceUInt32)process->frameBuffer.pvIBase;
			range.size = process->config.sizeIBase;
			result = sceKernelVARangeToPARange(&range, &physical);
			if (result < 0)
				goto restore;
			goto cachePhysical;
		}
		process->frameBuffer.paddrUBase = physical.addr;
		range.addr = (SceUInt32)process->frameBuffer.pvVBase;
		range.size = process->config.sizeVBase;
		result = sceKernelVARangeToPARange(&range, &physical);
		if (result < 0)
			return result;
		process->frameBuffer.paddrVBase = physical.addr;
		range.addr = (SceUInt32)process->frameBuffer.pvIBase;
		range.size = process->config.sizeIBase;
		result = sceKernelVARangeToPARange(&range, &physical);
		if (result < 0)
			return result;
setPhysical:
		process->frameBuffer.paddrIBase = physical.addr;
		return result;
	}
	result = 0;
	if (!cache) {
		process->frameBuffer.pvIBase = NULL;
		process->frameBuffer.pvUBase = NULL;
		process->frameBuffer.pvVBase = NULL;
		return result;
	}
	device->cachedFrameBuffer.pvIBase = NULL;
	device->cachedFrameBuffer.pvUBase = NULL;
	device->cachedFrameBuffer.pvVBase = NULL;
	goto restore;
cachePhysical:
	device->cachedFrameBuffer.paddrIBase = physical.addr;
cacheVirtual:
	device->cachedFrameBuffer.pvIBase = process->frameBuffer.pvIBase;
	device->cachedFrameBuffer.pvUBase = process->frameBuffer.pvUBase;
	device->cachedFrameBuffer.pvVBase = process->frameBuffer.pvVBase;
restore:
	process->frameBuffer.paddrIBase = device->cachedFrameBuffer.paddrIBase;
	process->frameBuffer.paddrUBase = device->cachedFrameBuffer.paddrUBase;
	process->frameBuffer.paddrVBase = device->cachedFrameBuffer.paddrVBase;
	process->frameBuffer.pvIBase = device->cachedFrameBuffer.pvIBase;
	process->frameBuffer.pvUBase = device->cachedFrameBuffer.pvUBase;
	process->frameBuffer.pvVBase = device->cachedFrameBuffer.pvVBase;
	return result;
failed:
	if (cache)
		goto restore;
	return result;
}

SceUID cameraGetCallerProcessId(void)
{
	int result;

	if (sceKernelIsIntrContext())
		return 0;
	result = sceKernelIsUserModeThread();
	if (result != 0)
		return sceKernelGetProcessId();
	return result;
}

CameraProcess *cameraGetProcess(int devnum, SceUID processId)
{
	void *storage;
	CameraProcess *process = NULL;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey, &storage, 0) >= 0 &&
		    storage != NULL) {
			process = (CameraProcess *)storage + devnum;
			if (processId != process->processId)
				process = NULL;
		}
	}
	return process;
}

int cameraInitializeConfig(void)
{
	memset(cameraDriver, 0, sizeof(*cameraDriver));
	cameraDriver->processLocalStorageKey =
		sceKernelRegisterKPLS("SceCamera",
						  2 * sizeof(CameraProcess));
	cameraReservedState[0] = 0;
	cameraDriver->device[0].activeProcessId = -1;
	cameraDriver->device[1].activeProcessId = -1;
	cameraReservedState[1] = 0;
	cameraProcessRegistry.count = 0;
	cameraProcessRegistry.processId[0] = -1;
	cameraProcessRegistry.processId[1] = -1;
	cameraProcessRegistry.processId[2] = -1;
	cameraProcessRegistry.processId[3] = -1;
	cameraProcessRegistry.processId[4] = -1;
	cameraProcessRegistry.processId[5] = -1;
	cameraProcessRegistry.processId[6] = -1;
	cameraProcessRegistry.processId[7] = -1;
	cameraProcessRegistry.processId[8] = -1;
	cameraProcessRegistry.processId[9] = -1;
	cameraProcessRegistry.processId[10] = -1;
	cameraProcessRegistry.processId[11] = -1;
	cameraProcessRegistry.processId[12] = -1;
	cameraProcessRegistry.processId[13] = -1;
	cameraProcessRegistry.processId[14] = -1;
	cameraProcessRegistry.processId[15] = -1;
	return 0;
}

int cameraInitializeProcess(SceUID processId)
{
	void *storage = NULL;
	CameraProcess *process;

	if (sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, &storage, 1) >= 0 &&
	    storage != NULL) {
		process = storage;
		memset(&process[0], 0, sizeof(process[0]));
		process[0].processId = -1;
		memset(&process[1], 0, sizeof(process[1]));
		process[1].processId = -1;
		return 0;
	}
	return (int)0x802e00ff;
}

int cameraRegisterProcess(SceUID processId)
{
	int slot;

	if (cameraProcessRegistry.processId[0] == -1)
		slot = 0;
	else if (cameraProcessRegistry.processId[1] == -1)
		slot = 1;
	else if (cameraProcessRegistry.processId[2] == -1)
		slot = 2;
	else if (cameraProcessRegistry.processId[3] == -1)
		slot = 3;
	else if (cameraProcessRegistry.processId[4] == -1)
		slot = 4;
	else if (cameraProcessRegistry.processId[5] == -1)
		slot = 5;
	else if (cameraProcessRegistry.processId[6] == -1)
		slot = 6;
	else if (cameraProcessRegistry.processId[7] == -1)
		slot = 7;
	else if (cameraProcessRegistry.processId[8] == -1)
		slot = 8;
	else if (cameraProcessRegistry.processId[9] == -1)
		slot = 9;
	else if (cameraProcessRegistry.processId[10] == -1)
		slot = 10;
	else if (cameraProcessRegistry.processId[11] == -1)
		slot = 11;
	else if (cameraProcessRegistry.processId[12] == -1)
		slot = 12;
	else if (cameraProcessRegistry.processId[13] == -1)
		slot = 13;
	else if (cameraProcessRegistry.processId[14] == -1)
		slot = 14;
	else if (cameraProcessRegistry.processId[15] == -1)
		slot = 15;
	else
		return 0x802e000d;

	cameraProcessRegistry.processId[slot] = processId;
	cameraProcessRegistry.count++;
	return 0;
}

int cameraUnregisterProcess(SceUID processId)
{
	int slot;
	unsigned int count;

	if (cameraProcessRegistry.processId[0] == processId)
		slot = 0;
	else if (cameraProcessRegistry.processId[1] == processId)
		slot = 1;
	else if (cameraProcessRegistry.processId[2] == processId)
		slot = 2;
	else if (cameraProcessRegistry.processId[3] == processId)
		slot = 3;
	else if (cameraProcessRegistry.processId[4] == processId)
		slot = 4;
	else if (cameraProcessRegistry.processId[5] == processId)
		slot = 5;
	else if (cameraProcessRegistry.processId[6] == processId)
		slot = 6;
	else if (cameraProcessRegistry.processId[7] == processId)
		slot = 7;
	else if (cameraProcessRegistry.processId[8] == processId)
		slot = 8;
	else if (cameraProcessRegistry.processId[9] == processId)
		slot = 9;
	else if (cameraProcessRegistry.processId[10] == processId)
		slot = 10;
	else if (cameraProcessRegistry.processId[11] == processId)
		slot = 11;
	else if (cameraProcessRegistry.processId[12] == processId)
		slot = 12;
	else if (cameraProcessRegistry.processId[13] == processId)
		slot = 13;
	else if (cameraProcessRegistry.processId[14] == processId)
		slot = 14;
	else {
		if (cameraProcessRegistry.processId[15] != processId)
			return (int)0x802e00ff;
		slot = 15;
	}

	count = cameraProcessRegistry.count;
	cameraProcessRegistry.processId[slot] = -1;
	cameraProcessRegistry.count = count - 1;
	return 0;
}

CameraProcess *cameraGetProcessStorage(SceUID processId, int create)
{
	void *storage = NULL;
	int result;

	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, &storage, create);
	return result < 0 ? NULL : storage;
}

int sceCameraConfigOpen(int devnum, SceCameraInfo *pInfo)
{
	SceUID processId = 0;
	CameraDevice *device;
	CameraDevice *sensor;
	CameraProcess *process;
	void *storage = NULL;
	unsigned int framerate;
	unsigned int timeout;
	int result;

	if (!sceKernelIsIntrContext()) {
		processId = sceKernelIsUserModeThread();
		if (processId != 0)
			processId = sceKernelGetProcessId();
	}
	device = &cameraDriver->device[devnum];
	if (pInfo == NULL)
		return (int)0x802e0000;
	if (processId < 0)
		return processId;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, &storage, 0);
	if (result >= 0 && storage != NULL &&
	    ((CameraProcess *)storage)[devnum].processId == processId)
		return (int)0x802e0003;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, &storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e00ff;
	process = &((CameraProcess *)storage)[devnum];
	if (process->processId != -1)
		return (int)0x802e00ff;
	sensor = &cameraDriver->device[devnum];
	process->processId = processId;
	if (device->activeProcessId == -1)
		device->activeProcessId = processId;
	process->config.wHeight = pInfo->wHeight;
	process->config.sizeThis = pInfo->sizeThis;
	process->wPriority = pInfo->wPriority;
	process->config.wFormat = pInfo->wFormat;
	process->config.wResolution = pInfo->wResolution;
	framerate = pInfo->wFramerate;
	process->config.wFramerate = framerate;
	process->config.wWidth = pInfo->wWidth;
	process->config.wRange = pInfo->wRange;
	process->config.wZoom = 10;
	process->config.wPitch = 0;
	process->config.wBuffer = 0;
	if (pInfo->sizeThis > 0x2b) {
		process->config.wPitch = pInfo->wPitch;
		if (pInfo->wBuffer == 1)
			process->config.wBuffer = 1;
	}
	process->config.brightness = 128;
	process->config.contrast = 32;
	process->config.saturation = 10;
	process->config.sharpness = 1;
	process->config.iso = 1;
	process->config.noiseReduction = 1;
	process->config.reverse = 0;
	process->config.effect = 0;
	process->config.ev = 0;
	process->config.antiflicker = 0;
	process->config.gain = 0;
	process->config.whiteBalance = 0;
	process->config.backlight = 0;
	process->config.nightMode = 0;
	process->config.autoControlHold = 0;
	process->config.exposureCeiling = 0;
	process->config.imageQuality = 0;
	switch (framerate) {
	case 3: timeout = 270000; break;
	case 5: timeout = 204000; break;
	case 7: timeout = 137000; break;
	case 10: timeout = 104000; break;
	case 15: timeout = 70000; break;
	case 20: timeout = 54000; break;
	case 24: timeout = 45000; break;
	case 25: timeout = 44000; break;
	case 30: timeout = 37000; break;
	case 60: timeout = 20000; break;
	case 120: timeout = 12000; break;
	default: timeout = 0; break;
	}
	process->config.frameTimeout = timeout;
	sensor->getSensorRegisters(devnum, process->sensorRegisterValues, 1);
	if (process->config.wBuffer == 0) {
		process->config.sizeIBase = pInfo->sizeIBase;
		process->config.sizeUBase = pInfo->sizeUBase;
		process->config.sizeVBase = pInfo->sizeVBase;
		process->frameBuffer.pvIBase = pInfo->pvIBase;
		process->frameBuffer.pvUBase = pInfo->pvUBase;
		process->frameBuffer.pvVBase = pInfo->pvVBase;
	}
	process->state = 1;
	return 0;
}

int sceCameraConfigStartPid(int devnum, SceUID processId, int force)
{
	CameraDevice *device = &cameraDriver->device[devnum];
	CameraProcess *storage = NULL;
	CameraProcess *process;
	CameraProcess *current;
	CameraFrameBuffer *buffer;
	unsigned int pixels;
	unsigned int chroma;
	int pendingStart = 0;
	int result;
	int index;

	if (processId < 0)
		return processId;
	if (device->activeProcessId == -1)
		device->activeProcessId = processId;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (process->processId != processId)
		return (int)0x802e0004;
	if (!force) {
		if (process->started)
			return (int)0x802e0005;
		if (device->activeProcessId != processId) {
			result = 0;
			if (process->config.wBuffer == 0) {
				result = cameraTranslateFrameBuffer(devnum, process, 0);
				if (result < 0)
					return result;
			}
			if (result == 0) {
				storage = NULL;
				result = sceKernelGetRemoteKPLS(processId,
					cameraDriver->processLocalStorageKey, (void **)&storage, 0);
				if (result < 0 || storage == NULL ||
				    storage[devnum].processId != processId) {
					result = (int)0x802e0004;
				} else {
					current = &storage[devnum];
					if (current->started) {
						result = (int)0x802e0005;
					} else {
						current->state = 2;
						current->started = 1;
						result = 0;
					}
				}
			}
			process->state = 3;
			if (cameraDriver->activeEnabled && cameraDriver->powerEnabled) {
				if (result != 0)
					return result;
				goto pending;
			}
			pendingStart = 1;
		} else if (!cameraDriver->activeEnabled || !cameraDriver->powerEnabled) {
			pendingStart = 1;
		}
	}
	device->config.wFormat = process->config.wFormat;
	device->config.wFramerate = process->config.wFramerate;
	device->config.wResolution = process->config.wResolution;
	device->config.sizeThis = process->config.sizeThis;
	device->config.wZoom = process->config.wZoom;
	device->config.wPitch = 0;
	device->config.wBuffer = 0;
	device->config.frameTimeout = process->config.frameTimeout;
	if (process->config.sizeThis > 0x2b) {
		device->config.wPitch = process->config.wPitch;
		device->config.wBuffer = process->config.wBuffer;
	}
	device->config.contrast = process->config.contrast;
	device->config.saturation = process->config.saturation;
	device->config.wWidth = process->config.wWidth;
	device->config.wHeight = process->config.wHeight;
	device->config.sharpness = process->config.sharpness;
	device->config.wRange = process->config.wRange;
	device->config.reverse = process->config.reverse;
	device->config.brightness = process->config.brightness;
	device->config.ev = process->config.ev;
	device->config.antiflicker = process->config.antiflicker;
	device->config.iso = process->config.iso;
	device->config.effect = process->config.effect;
	device->config.gain = process->config.gain;
	device->config.backlight = process->config.backlight;
	device->config.nightMode = process->config.nightMode;
	device->config.noiseReduction = process->config.noiseReduction;
	device->config.whiteBalance = process->config.whiteBalance;
	device->config.autoControlHold = process->config.autoControlHold;
	device->config.exposureCeiling = process->config.exposureCeiling;
	device->config.imageQuality = process->config.imageQuality;
	device->sensorRegisterWords[0] = process->sensorRegisterWords[0];
	device->sensorRegisterWords[1] = process->sensorRegisterWords[1];
	device->sensorRegisterWords[2] = process->sensorRegisterWords[2];
	device->sensorRegisterWords[3] = process->sensorRegisterWords[3];
	device->sensorRegisterWords[4] = process->sensorRegisterWords[4];
	device->setSensorRegisters(devnum, device->sensorRegisterValues);
	device->chromaAligned = (unsigned int)(device->config.wFormat - 3) <= 2;
	result = 0;
	switch (device->config.wFormat) {
	case 1:
	case 3:
	case 4:
	case 5:
		pixels = device->config.wWidth * device->config.wHeight;
		device->config.sizeIBase = pixels;
		chroma = pixels >> 1;
		if (device->chromaAligned)
			chroma = (((device->config.wWidth >> 1) + 15) & ~15U) *
				 device->config.wHeight;
		device->config.sizeUBase = chroma;
		device->config.sizeVBase = chroma;
		for (index = 0; index < 3; ++index) {
			buffer = &device->frameBuffer[index];
			buffer->pvIBase = cameraFrameBufferBase[devnum][index];
			buffer->pvUBase = (char *)buffer->pvIBase + pixels;
			buffer->pvVBase = (char *)buffer->pvUBase + chroma;
			result = sceKernelVAtoPA(buffer->pvIBase, &buffer->paddrIBase);
			if (result < 0)
				return result;
			result = sceKernelVAtoPA(buffer->pvUBase, &buffer->paddrUBase);
			if (result < 0)
				return result;
			result = sceKernelVAtoPA(buffer->pvVBase, &buffer->paddrVBase);
			if (result < 0)
				return result;
			pixels = device->config.sizeIBase;
			chroma = device->config.sizeUBase;
		}
		break;
	case 2:
	case 6:
		pixels = device->config.wWidth * device->config.wHeight;
		if (device->config.wFormat == 2)
			pixels *= 2;
		device->config.sizeUBase = 0;
		device->config.sizeVBase = 0;
		device->config.sizeIBase = pixels;
		for (index = 0; index < 3; ++index) {
			buffer = &device->frameBuffer[index];
			buffer->pvUBase = NULL;
			buffer->pvVBase = NULL;
			buffer->pvIBase = cameraFrameBufferBase[devnum][index];
			result = sceKernelVAtoPA(buffer->pvIBase, &buffer->paddrIBase);
			if (result < 0)
				return result;
			buffer->paddrUBase = 0;
			buffer->paddrVBase = 0;
		}
		break;
	default:
		device->config.sizeIBase = 0;
		device->config.sizeUBase = 0;
		device->config.sizeVBase = 0;
		memset(device->frameBuffer, 0, sizeof(device->frameBuffer));
		break;
	}
	if (device->config.wBuffer == 0) {
		if (force) {
			cameraDriver->device[devnum].cachedFrameBuffer = process->frameBuffer;
		} else {
			result = cameraTranslateFrameBuffer(devnum, process, 1);
			if (result < 0)
				return result;
		}
	}
	if (pendingStart && result == 0) {
		storage = NULL;
		result = sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey, (void **)&storage, 0);
		if (result < 0 || storage == NULL || storage[devnum].processId != processId) {
			result = (int)0x802e0004;
		} else {
			current = &storage[devnum];
			if (current->started) {
				result = (int)0x802e0005;
			} else {
				current->state = 2;
				current->started = 1;
				result = 0;
			}
		}
	}
	device->pendingProcessId = processId;
	if (pendingStart) {
		if (result != 0)
			return result;
pending:
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0) >= 0 &&
		    storage != NULL && storage[devnum].processId == processId)
			storage[devnum].activeStatus = 2;
		return (int)0x802e000e;
	}
	storage = NULL;
	if (sceKernelGetRemoteKPLS(processId,
		cameraDriver->processLocalStorageKey, (void **)&storage, 0) < 0 ||
	    storage == NULL || storage[devnum].processId != processId)
		return (int)0x802e0004;
	storage[devnum].activeStatus = 0;
	return 0;
}

int sceCameraConfigStopPid(int devnum, SceUID processId)
{
	CameraDriver *driver = cameraDriver;
	CameraProcess *storage = NULL;
	CameraProcess *process;
	CameraDevice *device;
	int active;
	int enabled;
	int result;

	if (processId < 0)
		return processId;
	result = sceKernelGetRemoteKPLS(processId,
			driver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL || storage[devnum].processId != processId)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (!process->started)
		return (int)0x802e0006;
	device = &driver->device[devnum];
	active = device->activeProcessId == processId;
	if (!active)
		process->state = 4;
	enabled = cameraDriver->activeEnabled && cameraDriver->powerEnabled;
	if (active || !enabled) {
		process->config.contrast = device->config.contrast;
		process->config.saturation = device->config.saturation;
		process->config.wZoom = device->config.wZoom;
		process->config.sharpness = device->config.sharpness;
		process->config.brightness = device->config.brightness;
		process->config.effect = device->config.effect;
		process->config.reverse = device->config.reverse;
		process->config.antiflicker = device->config.antiflicker;
		process->config.iso = device->config.iso;
		process->config.ev = device->config.ev;
		process->config.whiteBalance = device->config.whiteBalance;
		process->config.nightMode = device->config.nightMode;
		process->config.noiseReduction = device->config.noiseReduction;
		process->config.gain = device->config.gain;
		process->config.backlight = device->config.backlight;
		process->config.autoControlHold = device->config.autoControlHold;
		process->config.exposureCeiling = device->config.exposureCeiling;
		process->config.imageQuality = device->config.imageQuality;
		process->sensorRegisterWords[0] = device->sensorRegisterWords[0];
		process->sensorRegisterWords[1] = device->sensorRegisterWords[1];
		process->sensorRegisterWords[2] = device->sensorRegisterWords[2];
		process->sensorRegisterWords[3] = device->sensorRegisterWords[3];
		process->sensorRegisterWords[4] = device->sensorRegisterWords[4];
		device->pendingProcessId = 0;
		if (active && enabled) {
			storage = NULL;
			if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey, (void **)&storage, 0) >= 0 &&
			    storage != NULL && storage[devnum].processId == processId)
				storage[devnum].activeStatus = 0;
			return 0;
		}
	}
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL || storage[devnum].processId != processId)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (!process->started)
		return (int)0x802e0006;
	process->state = 1;
	process->started = 0;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result >= 0 && storage != NULL) {
		if (storage[devnum].processId != processId)
			return (int)0x802e0006;
		storage[devnum].activeStatus = 0;
	}
	return (int)0x802e000e;
}

int cameraHasOpenProcess(int devnum)
{
	CameraProcess *storage;
	SceUID processId;
	int index = 0;

	do {
		processId = cameraProcessRegistry.processId[index++];
		if ((unsigned int)processId + 1 != 0 && processId >= 0) {
			storage = NULL;
			if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey, (void **)&storage, 0) >= 0 &&
			    storage != NULL && storage[devnum].processId == processId)
				return 1;
		}
	} while (index != 16);
	return 0;
}

int cameraHasStartedProcess(int devnum, int activeOnly)
{
	CameraProcess *storage;
	SceUID processId;
	int index = 0;

	if (activeOnly) {
		do {
			processId = cameraProcessRegistry.processId[index++];
			if (processId != -1 && processId >= 0) {
				storage = NULL;
				if (sceKernelGetRemoteKPLS(processId,
					cameraDriver->processLocalStorageKey, (void **)&storage, 0) >= 0 &&
				    storage != NULL && storage[devnum].processId == processId &&
				    storage[devnum].started && storage[devnum].activeStatus != 2)
					return 1;
			}
		} while (index != 16);
	} else {
		do {
			processId = cameraProcessRegistry.processId[index++];
			if (processId != -1 && processId >= 0) {
				storage = NULL;
				if (sceKernelGetRemoteKPLS(processId,
					cameraDriver->processLocalStorageKey, (void **)&storage, 0) >= 0 &&
				    storage != NULL && storage[devnum].processId == processId &&
				    storage[devnum].started)
					return 1;
			}
		} while (index != 16);
	}
	return 0;
}

int sceCameraConfigSetStartedPid(int devnum, SceUID processId, int started)
{
	CameraProcess *storage;
	CameraProcess *process;
	int result;

	if (processId < 0)
		return processId;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL || processId != storage[devnum].processId)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (started) {
		if (storage[devnum].started == 0) {
			process->state = 2;
			goto setStarted;
		}
		return (int)0x802e0005;
	}
	if (storage[devnum].started != 0) {
		process->state = 1;
		goto setStarted;
	}
	return (int)0x802e0006;
setStarted:
	storage[devnum].started = started;
	return 0;
}

int sceCameraConfigSetActiveStatusPid(int devnum, SceUID processId, int status)
{
	CameraProcess *storage;
	CameraProcess *process;
	int result;

	if (processId < 0)
		return processId;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result >= 0 && storage != NULL) {
		process = &storage[devnum];
		if (processId == process->processId) {
			process->activeStatus = status;
			return 0;
		}
	}
	return (int)0x802e0004;
}

int sceCameraConfigClosePid(int devnum, SceUID processId)
{
	CameraDriver *driver = cameraDriver;
	CameraProcess *storage;
	CameraProcess *process;
	CameraDevice *device;
	int alternateFrameBuffers;
	SceUID eventFlagId;
	SceSize eventFlagOptionSize;
	int result;

	if (processId < 0)
		return processId;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			driver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (processId != process->processId)
		return (int)0x802e0004;
	alternateFrameBuffers = process->alternateFrameBuffers;
	device = &driver->device[devnum];
	eventFlagId = device->eventFlagId;
	eventFlagOptionSize = device->eventFlagOption.size;
	if (device->activeProcessId == processId)
		device->activeProcessId = -1;
	memset(process, 0, sizeof(*process));
	process->processId = -1;
	device->eventFlagId = eventFlagId;
	device->eventFlagOption.size = eventFlagOptionSize;
	process->alternateFrameBuffers = alternateFrameBuffers;
	process->state = 0;
	return 0;
}

int cameraResetConfig(void)
{
	memset(cameraDriver, 0, sizeof(*cameraDriver));
	cameraDriver->processLocalStorageKey =
		sceKernelRegisterKPLS("SceCamera",
						  2 * sizeof(CameraProcess));
	cameraReservedState[0] = 0;
	cameraDriver->device[0].activeProcessId = -1;
	cameraDriver->device[1].activeProcessId = -1;
	cameraReservedState[1] = 0;
	cameraProcessRegistry.count = 0;
	cameraProcessRegistry.processId[0] = -1;
	cameraProcessRegistry.processId[1] = -1;
	cameraProcessRegistry.processId[2] = -1;
	cameraProcessRegistry.processId[3] = -1;
	cameraProcessRegistry.processId[4] = -1;
	cameraProcessRegistry.processId[5] = -1;
	cameraProcessRegistry.processId[6] = -1;
	cameraProcessRegistry.processId[7] = -1;
	cameraProcessRegistry.processId[8] = -1;
	cameraProcessRegistry.processId[9] = -1;
	cameraProcessRegistry.processId[10] = -1;
	cameraProcessRegistry.processId[11] = -1;
	cameraProcessRegistry.processId[12] = -1;
	cameraProcessRegistry.processId[13] = -1;
	cameraProcessRegistry.processId[14] = -1;
	cameraProcessRegistry.processId[15] = -1;
	return 0;
}

int sceCameraConfigRead(int devnum, SceCameraRead *pRead)
{
	CameraDriver *driver;
	CameraProcess *storage;
	CameraProcess *process;
	CameraDevice *device;
	SceUID processId;
	int result, isUserThread, wBuffer;

	if (sceKernelIsIntrContext()) {
		driver = cameraDriver;
		processId = 0;
		device = &driver->device[devnum];
	} else {
		processId = sceKernelIsUserModeThread();
		if (processId) {
			processId = sceKernelGetProcessId();
			driver = cameraDriver;
			if (processId < 0)
				return processId;
			device = &driver->device[devnum];
		} else {
			driver = cameraDriver;
			device = &driver->device[devnum];
		}
	}
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			driver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (processId != process->processId)
		return (int)0x802e0004;
	if (!process->started)
		return (int)0x802e0006;
	if (device->activeProcessId != processId)
		return (int)0x802e000e;
	if (!cameraDriver->activeEnabled)
		return (int)0x802e0015;
	if (!cameraDriver->powerEnabled)
		return (int)0x802e0014;
	if (device->busy)
		return (int)0x802e000e;
	isUserThread = sceKernelIsUserModeThread();
	wBuffer = device->config.wBuffer;
	device->isUserThread = isUserThread;
	result = 0;
	if (wBuffer == 1 && pRead->sizeThis > 0x2f) {
		void *pvIBase = pRead->pvIBase;
		void *pvUBase = pRead->pvUBase;
		SceSize sizeIBase = pRead->sizeIBase;
		SceSize sizeUBase = pRead->sizeUBase;
		SceSize sizeVBase = pRead->sizeVBase;

		process->frameBuffer.pvUBase = pvUBase;
		process->config.sizeIBase = sizeIBase;
		process->config.sizeUBase = sizeUBase;
		process->config.sizeVBase = sizeVBase;
		process->frameBuffer.pvVBase = pRead->pvVBase;
		process->frameBuffer.pvIBase = pvIBase;
		result = cameraTranslateFrameBuffer(devnum, process, 1);
		if (result < 0)
			return result;
	}
	if (process->suspendState || device->suspendState)
		return -2;
	return result;
}

int sceCameraConfigAttribute(int devnum, int checkFormat)
{
	CameraDriver *driver;
	CameraProcess *storage;
	CameraProcess *process;
	CameraDevice *device;
	SceUID processId;
	int result;

	if (sceKernelIsIntrContext()) {
		driver = cameraDriver;
		processId = 0;
		device = &driver->device[devnum];
	} else {
		processId = sceKernelIsUserModeThread();
		if (processId) {
			processId = sceKernelGetProcessId();
			driver = cameraDriver;
			if (processId < 0)
				return processId;
			device = &driver->device[devnum];
		} else {
			driver = cameraDriver;
			device = &driver->device[devnum];
		}
	}
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			driver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (processId != process->processId)
		return (int)0x802e0004;
	if (!process->started)
		return (int)0x802e0006;
	if (checkFormat && process->config.wFormat == 6)
		return (int)0x802e000c;
	if (device->activeProcessId != processId || !cameraDriver->activeEnabled)
		return (int)0x802e000e;
	return cameraDriver->powerEnabled ? 0 : (int)0x802e000e;
}

int cameraCheckActiveProcess(SceUID processId, int devnum, int currentProcess)
{
	CameraDriver *driver;
	CameraProcess *storage;
	CameraProcess *process;
	int result;

	if (sceKernelIsIntrContext()) {
		result = 0;
	} else {
		result = sceKernelIsUserModeThread();
		if (result)
			result = sceKernelGetProcessId();
	}
	if (!currentProcess)
		result = processId;
	processId = result;
	driver = cameraDriver;
	if (processId < 0)
		return processId;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			driver->processLocalStorageKey, (void **)&storage, 0);
	if (result >= 0 && storage != NULL &&
	    processId == storage[devnum].processId) {
		process = &storage[devnum];
		if (process->started) {
			result = driver->device[devnum].activeProcessId;
			return result == processId ? 0 : (int)0x802e000e;
		}
		return (int)0x802e0006;
	}
	return (int)0x802e0004;
}

int sceCameraConfigGetAttribute(int devnum, int attribute, int *pValue)
{
	CameraProcess *storage;
	CameraProcess *process;
	SceUID processId;
	int result;

	if (sceKernelIsIntrContext() ||
	    !sceKernelIsUserModeThread()) {
		processId = 0;
	} else {
		processId = sceKernelGetProcessId();
		if (processId < 0)
			return processId;
	}
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (processId != process->processId)
		return (int)0x802e0004;
	switch (attribute) {
	case 10:
		*pValue = process->config.saturation;
		return 0;
	case 11:
		*pValue = process->config.brightness;
		return 0;
	case 12:
		*pValue = process->config.contrast;
		return 0;
	case 13:
		*pValue = process->config.sharpness;
		return 0;
	case 14:
		*pValue = process->config.reverse;
		return 0;
	case 15:
		*pValue = process->config.effect;
		return 0;
	case 16:
		*pValue = process->config.ev;
		return 0;
	case 17:
		*pValue = process->config.wZoom;
		return 0;
	case 18:
		*pValue = process->config.antiflicker;
		return 0;
	case 20:
		*pValue = process->config.iso;
		return 0;
	case 22:
		*pValue = process->config.whiteBalance;
		return 0;
	case 23:
		*pValue = process->config.backlight;
		return 0;
	case 24:
		*pValue = process->config.nightMode;
		return 0;
	case 25:
		*pValue = process->config.noiseReduction;
		return 0;
	case 26:
		*pValue = process->config.autoControlHold;
		return 0;
	case 27:
		*pValue = process->config.exposureCeiling;
		return 0;
	case 28:
		*pValue = process->config.imageQuality;
		return 0;
	default:
		return (int)0x802e000c;
	}
}

int sceCameraConfigSetAttribute(int devnum, int attribute, int value, int flags,
			      int updateSensor)
{
	CameraProcess *storage;
	CameraProcess *process;
	CameraDevice *device;
	SceUID processId;
	int result;

	if (sceKernelIsIntrContext()) {
		processId = 0;
		device = &cameraDriver->device[devnum];
	} else {
		processId = sceKernelIsUserModeThread();
		if (processId) {
			processId = sceKernelGetProcessId();
			if (processId < 0)
				return processId;
		}
		device = &cameraDriver->device[devnum];
	}
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (processId != process->processId)
		return (int)0x802e0004;
	switch (attribute) {
	case 10: process->config.saturation = value; break;
	case 11: process->config.brightness = value; break;
	case 12: process->config.contrast = value; break;
	case 13: process->config.sharpness = value; break;
	case 14: process->config.reverse = value; break;
	case 15: process->config.effect = value; break;
	case 16: process->config.ev = value; break;
	case 17: process->config.wZoom = value; break;
	case 18: process->config.antiflicker = value; break;
	case 20: process->config.iso = value; break;
	case 22: process->config.whiteBalance = value; break;
	case 23: process->config.backlight = value; break;
	case 24: {
		unsigned int timeout, timeoutOne, timeoutTwo, timeoutThree;

		process->config.nightMode = value;
		result = 0;
		switch (process->config.wFramerate) {
		case 3:
			timeout = 270000; timeoutOne = 1080000;
			timeoutTwo = 810000; timeoutThree = 540000;
			break;
		case 5:
			timeout = 204000; timeoutOne = 816000;
			timeoutTwo = 612000; timeoutThree = 408000;
			break;
		case 7:
			timeout = 137000; timeoutOne = 548000;
			timeoutTwo = 411000; timeoutThree = 274000;
			break;
		case 10:
			timeout = 104000; timeoutOne = 416000;
			timeoutTwo = 312000; timeoutThree = 208000;
			break;
		case 15:
			timeout = 70000; timeoutOne = 280000;
			timeoutTwo = 210000; timeoutThree = 140000;
			break;
		case 20:
			timeout = 54000; timeoutOne = 216000;
			timeoutTwo = 162000; timeoutThree = 108000;
			break;
		case 24:
			timeout = 45000; timeoutOne = 180000;
			timeoutTwo = 135000; timeoutThree = 90000;
			break;
		case 25:
			timeout = 44000; timeoutOne = 176000;
			timeoutTwo = 132000; timeoutThree = 88000;
			break;
		case 30:
			timeout = 37000; timeoutOne = 148000;
			timeoutTwo = 111000; timeoutThree = 74000;
			break;
		case 60:
			timeout = 20000; timeoutOne = 80000;
			timeoutTwo = 60000; timeoutThree = 40000;
			break;
		case 120:
			timeout = 12000; timeoutOne = 48000;
			timeoutTwo = 36000; timeoutThree = 24000;
			break;
		default:
			timeout = timeoutOne = timeoutTwo = timeoutThree = 0;
			result = (int)0x802e0009;
			break;
		}
		switch (value) {
		case 0: break;
		case 1: timeout = timeoutOne; break;
		case 2: timeout = timeoutTwo; break;
		case 3: timeout = timeoutThree; break;
		default: timeout = result; break;
		}
		process->config.frameTimeout = timeout;
		if (updateSensor)
			device->config.frameTimeout = timeout;
		break;
	}
	case 25: process->config.noiseReduction = value; break;
	case 26: process->config.autoControlHold = value; break;
	case 27: process->config.exposureCeiling = value; break;
	case 28: process->config.imageQuality = value; break;
	default: return (int)0x802e000c;
	}
	if (updateSensor) {
		SceUInt32 *sensorWords = process->sensorRegisterWords;
		SceUInt32 sensorWord0, sensorWord1, sensorWord2, sensorWord3;

		device->getSensorRegisters(devnum, (SceUInt8 *)sensorWords, 0);
		sensorWord0 = sensorWords[0];
		sensorWord1 = sensorWords[1];
		sensorWord2 = sensorWords[2];
		sensorWord3 = sensorWords[3];
		device->sensorRegisterWords[0] = sensorWord0;
		device->sensorRegisterWords[1] = sensorWord1;
		device->sensorRegisterWords[2] = sensorWord2;
		device->sensorRegisterWords[3] = sensorWord3;
		device->sensorRegisterWords[4] = sensorWords[4];
	}
	return 0;
}

int cameraGetProcessDeviceStatus(SceUID processId)
{
	CameraProcess *storage;
	CameraProcess *process;
	CameraDevice *device;
	SceUID activeProcessId;
	int status = 0;
	int devnum = 0;
	int flags;

	do {
		flags = status << 8;
		device = &cameraDriver->device[devnum];
		if (processId >= 0) {
			storage = NULL;
			if (sceKernelGetRemoteKPLS(processId,
					cameraDriver->processLocalStorageKey,
					(void **)&storage, 0) >= 0 && storage != NULL) {
				process = &storage[devnum];
				if (processId == process->processId) {
					if (process->state == 3)
						flags |= 0x10;
					else if (process->state == 4)
						flags |= 0x20;
					if (process->started || process->activeStatus == 2)
						flags |= 2;
				}
			}
		}
		activeProcessId = device->activeProcessId;
		if (activeProcessId == processId) {
			status = flags | 1;
		} else {
			status = flags | 4;
			if (activeProcessId >= 0) {
				storage = NULL;
				if (sceKernelGetRemoteKPLS(activeProcessId,
						cameraDriver->processLocalStorageKey,
						(void **)&storage, 0) >= 0 &&
				    storage != NULL &&
				    activeProcessId == storage[devnum].processId &&
				    storage[devnum].started)
					status = flags | 0xc;
			}
		}
		devnum++;
	} while (devnum != 2);
	return status;
}

int cameraSetProcessState(SceUID processId, int devnum, int state)
{
	CameraProcess *storage;
	CameraProcess *process;
	int result;

	if (processId < 0)
		return processId;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL)
		return (int)0x802e0004;
	process = &storage[devnum];
	if (processId != process->processId)
		return (int)0x802e0004;
	process->state = state;
	return 0;
}

int cameraHasOtherProcess(int devnum, SceUID processId)
{
	CameraProcess *storage;
	SceUID registeredProcessId;
	unsigned int index = 0;

	do {
		registeredProcessId = cameraProcessRegistry.processId[index++];
		if (registeredProcessId != -1 && registeredProcessId >= 0) {
			storage = NULL;
			if (sceKernelGetRemoteKPLS(registeredProcessId,
					cameraDriver->processLocalStorageKey,
					(void **)&storage, 0) >= 0 &&
			    storage != NULL &&
			    registeredProcessId == storage[devnum].processId &&
			    registeredProcessId != processId)
				return 1;
		}
	} while (index != 16);
	return 0;
}

int cameraIsProcessActive(int devnum, SceUID processId)
{
	CameraDriver *driver = cameraDriver;
	CameraProcess *storage;
	int result;

	if (processId < 0)
		return 0;
	storage = NULL;
	result = sceKernelGetRemoteKPLS(processId,
			driver->processLocalStorageKey, (void **)&storage, 0);
	if (result < 0 || storage == NULL ||
	    processId != storage[devnum].processId)
		return 0;
	result = storage[devnum].started;
	if (result)
		result = processId == driver->device[devnum].activeProcessId;
	return result;
}

int cameraIsProcessStarted(int devnum, SceUID processId)
{
	CameraProcess *storage;

	if (processId < 0)
		goto failed;
	storage = NULL;
	if (sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey, (void **)&storage, 0) < 0 ||
	    storage == NULL || processId != storage[devnum].processId)
		goto failed;
	return storage[devnum].started != 0;
failed:
	return 0;
}

int cameraSetProcessSuspendState(int devnum, SceUID processId, int state)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				process->suspendState = state;
				return 0;
			}
		}
	}
	return -1;
}

int cameraGetProcessSuspendState(int devnum, SceUID processId, int *state)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				*state = process->suspendState;
				return 0;
			}
		}
	}
	return -1;
}

int cameraSetProcessRequestedSuspendState(int devnum, SceUID processId, int state)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				process->requestedSuspendState = state;
				return 0;
			}
		}
	}
	return -1;
}

int cameraGetProcessRequestedSuspendState(int devnum, SceUID processId, int *state)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				*state = process->requestedSuspendState;
				return 0;
			}
		}
	}
	return -1;
}

int cameraAdvanceProcessFrame(int devnum, SceUID processId)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				process->frame++;
				return 0;
			}
		}
	}
	return (int)0x802e0004;
}

int sceCameraConfigSetProcPrevFrame(int devnum, SceUID processId, SceUInt64 frame)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				process->previousFrame = frame;
				return 0;
			}
		}
	}
	return (int)0x802e0004;
}

int cameraRestoreProcessSensorRegisters(int devnum)
{
	CameraDevice *device = &cameraDriver->device[devnum];
	SceUID processId = device->activeProcessId;
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				device->sensorRegisterWords[0] = process->sensorRegisterWords[0];
				device->sensorRegisterWords[1] = process->sensorRegisterWords[1];
				device->sensorRegisterWords[2] = process->sensorRegisterWords[2];
				device->sensorRegisterWords[3] = process->sensorRegisterWords[3];
				device->sensorRegisterWords[4] = process->sensorRegisterWords[4];
				device->setSensorRegisters(devnum, device->sensorRegisterValues);
				return 0;
			}
		}
	}
	return (int)0x802e0004;
}

SceUInt64 sceCameraConfigGetProcFrame(int devnum, SceUID processId)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId)
				return process->frame;
		}
	}
	return 0;
}

SceUInt64 sceCameraConfigGetProcPrevFrame(int devnum, SceUID processId)
{
	CameraProcess *storage;
	CameraProcess *process;
	SceUInt64 frame;

	if (processId < 0)
		goto fail;
	storage = NULL;
	if (sceKernelGetRemoteKPLS(processId,
			cameraDriver->processLocalStorageKey,
			(void **)&storage, 0) < 0 || storage == NULL)
		goto fail;
	process = &storage[devnum];
	if (processId != process->processId)
		goto fail;
	frame = process->previousFrame;
	goto out;
fail:
	frame = 0;
out:
	return frame;
}

SceUInt64 sceCameraConfigGetProcTimestamp(int devnum, SceUID processId)
{
	CameraDriver *driver = cameraDriver;
	CameraProcess *storage;
	CameraProcess *process;
	SceUInt64 timestamp;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				driver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				timestamp = process->timestamp;
				SceProcessmgrForDriver_8CB01675(processId, &process->timestamp,
						driver->device[devnum].timestamp);
				if (process->timestamp < timestamp) {
					process->timestamp = timestamp;
					return timestamp;
				}
				return process->timestamp;
			}
		}
	}
	return 0;
}

int cameraSetProcessAlternateFrameBuffers(int devnum, SceUID processId, int alternate)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (process != NULL) {
				process->alternateFrameBuffers = alternate;
				return 0;
			}
		}
	}
	return -1;
}

int cameraGetProcessAlternateFrameBuffers(int devnum, SceUID processId, int *alternate)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (process != NULL) {
				*alternate = process->alternateFrameBuffers;
				return 0;
			}
		}
	}
	return -1;
}

int cameraSetProcessRangesLocked(int devnum, SceUID processId, int locked)
{
	CameraProcess *storage;
	CameraProcess *process;

	if (processId >= 0) {
		storage = NULL;
		if (sceKernelGetRemoteKPLS(processId,
				cameraDriver->processLocalStorageKey,
				(void **)&storage, 0) >= 0 && storage != NULL) {
			process = &storage[devnum];
			if (processId == process->processId) {
				process->rangesLocked = locked;
				return 0;
			}
		}
	}
	return -1;
}

