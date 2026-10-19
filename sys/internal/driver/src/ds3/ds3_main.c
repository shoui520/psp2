/* SceDs3 USB attachment and controller management. */
#include "ds3_private.h"
#include <ctrl/ctrl.h>
#include <regmgr/regmgr.h>
#include <bt/bt_registered.h>
#include <bt/bt_event.h>
#include <bt/bt_test.h>
#include <sysmem/debug.h>
#include <udcd/udcd_imports.h>
#include <threadmgr/threadmgr.h>
#include <motion/motion_dev.h>
#include <touch_dummy/touch_dummy.h>
#include <touch/touch.h>
#include <sysmem/sysmem.h>
#include <syscon/syscon.h>
#include <sysmem/cpu.h>
#include <sysmem/suspend.h>
#include <sysmem/procevent.h>
#include <sysmem/sysroot.h>
#include <threadmgr/fast_mutex_object.h>
#include <usbserv/usbserv.h>
#include <display/display.h>
#include <string.h>

int ds3ReadSensorData(int port, MotionDriverSample *data);
int ds3GetGyroCalibData(int port, SceMotionDevGyroCalibData *data);
int ds3GetAccCalibData(int port, SceMotionDevAccCalibData *data);
int ds3GetControllerType(int port, uint8_t *type);
int ds3GetCalibrationData(int port, SceMotionDevDs3CalibData *data);
int ds3SetAuxiliaryMotor(int port, int level);
int ds3ReadTouchData(int port, TouchVirtualSampleRequest *request);
int ds3CreateProcess(SceUID pid, ProcEventCreateParam *params, void *common);
int ds3ExitProcess(SceUID pid, ProcEventParam *params, void *common);
int ds3KillProcess(SceUID pid, ProcEventParam *params, void *common);
int ds3StopProcess(SceUID pid, int eventType, ProcEventParam *params, void *common);
int ds3StartProcess(SceUID pid, int eventType, ProcEventParam *params, void *common);
int ds3ReadButtons(int port, struct ctrlNativeSample *output);
int ds3SetActuator(int port, const SceCtrlActuator *state);
int ds3DisconnectController(int port);
int ds3SetIdleTimeout(int seconds);
int ds3GetControllerPortMask(void);
int ds3SwapPorts(int port, int destinationPort);
int ds3DisconnectAll(void);
int ds3GetControllerPortInfo(SceCtrlPortInfo *info);
int ds3SetLightbar(int port, uint8_t red, uint8_t green, uint8_t blue);
int ds3ResetLightbar(int port);
int ds3ForegroundChanged(SceUID pid);
int ds3SetSingleControllerMode(int enabled);

static const SceCtrlVirtualControllerDriver ds3ControllerDriver = {
	.readButtons = ds3ReadButtons,
	.setActuator = ds3SetActuator,
	.getBatteryInfo = ds3GetBatteryInfo,
	.disconnect = ds3DisconnectController,
	.setTurnOffInterval = ds3SetIdleTimeout,
	.getActiveControllerPort = ds3GetControllerPortMask,
	.changePortAssign = ds3SwapPorts,
	.displayOff = ds3DisconnectAll,
	.getControllerPortInfo = ds3GetControllerPortInfo,
	.setLightBar = ds3SetLightbar,
	.resetLightBar = ds3ResetLightbar,
	.foregroundChanged = ds3ForegroundChanged,
	.singleControllerMode = ds3SetSingleControllerMode,
};

static const SceKernelProcEventHandler ds3ProcessHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = ds3CreateProcess,
	.exit = ds3ExitProcess,
	.kill = ds3KillProcess,
	.suspend = ds3StopProcess,
	.resume = ds3StartProcess,
};

static const uint8_t ds3DefaultLightbarColors[5][3] = {
	{12, 23, 29},
	{0, 0, 13},
	{13, 0, 0},
	{0, 13, 0},
	{13, 0, 13},
};

static const TouchVirtualDriver ds3TouchDriver = {
	.getSample = ds3ReadTouchData,
};

static const MotionVirtualDriver ds3MotionDriver = {
	.getSample = ds3ReadSensorData,
	.getGyroCalibData = ds3GetGyroCalibData,
	.getAccCalibData = ds3GetAccCalibData,
	.getControllerType = ds3GetControllerType,
	.getDs3CalibData = ds3GetCalibrationData,
	.setGyroFeedBack = ds3SetAuxiliaryMotor,
};

static uint8_t ds3UnassignedLocalAddress[6];
static Ds3State ds3State;

int ds3GetControllerPortMask(void)
{
	int mask = 0;

	if ((ds3State.devices[0].bluetoothState == 2 ||
		ds3State.devices[0].usbState == 2) &&
		ds3State.devices[0].port != -1)
		mask |= 1 << ds3State.devices[0].port;
	if ((ds3State.devices[1].bluetoothState == 2 ||
		ds3State.devices[1].usbState == 2) &&
		ds3State.devices[1].port != -1)
		mask |= 1 << ds3State.devices[1].port;
	if ((ds3State.devices[2].bluetoothState == 2 ||
		ds3State.devices[2].usbState == 2) &&
		ds3State.devices[2].port != -1)
		mask |= 1 << ds3State.devices[2].port;
	if ((ds3State.devices[3].bluetoothState == 2 ||
		ds3State.devices[3].usbState == 2) &&
		ds3State.devices[3].port != -1)
		mask |= 1 << ds3State.devices[3].port;
	if ((ds3State.devices[4].bluetoothState == 2 ||
		ds3State.devices[4].usbState == 2) &&
		ds3State.devices[4].port != -1)
		mask |= 1 << ds3State.devices[4].port;
	if ((ds3State.devices[5].bluetoothState == 2 ||
		ds3State.devices[5].usbState == 2) &&
		ds3State.devices[5].port != -1)
		mask |= 1 << ds3State.devices[5].port;
	if ((ds3State.devices[6].bluetoothState == 2 ||
		ds3State.devices[6].usbState == 2) &&
		ds3State.devices[6].port != -1)
		mask |= 1 << ds3State.devices[6].port;
	if ((ds3State.devices[7].bluetoothState == 2 ||
		ds3State.devices[7].usbState == 2) &&
		ds3State.devices[7].port != -1)
		mask |= 1 << ds3State.devices[7].port;
	return mask;
}

int ds3CreateProcess(SceUID pid, ProcEventCreateParam *params, void *common)
{
	return 0;
}

int ds3StartProcess(SceUID pid, int eventType, ProcEventParam *params, void *common)
{
	return 0;
}

int ds3GetControllerPortInfo(SceCtrlPortInfo *info)
{
	int result, port, index;
	Ds3Device *device;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	for (port = 0; port < 5; port++) {
		info->port[port] = SCE_CTRL_TYPE_UNPAIRED;
		if (port == ds3State.devices[0].port &&
			(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
			index = 0;
		else if (port == ds3State.devices[1].port &&
			(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
			index = 1;
		else if (port == ds3State.devices[2].port &&
			(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
			index = 2;
		else if (port == ds3State.devices[3].port &&
			(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
			index = 3;
		else if (port == ds3State.devices[4].port &&
			(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
			index = 4;
		else if (port == ds3State.devices[5].port &&
			(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
			index = 5;
		else if (port == ds3State.devices[6].port &&
			(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
			index = 6;
		else if (port == ds3State.devices[7].port &&
			(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
			index = 7;
		else
			continue;
		device = &ds3State.devices[index];
		if (device->bluetoothState == 2 || device->usbState == 2) {
			if (device->idProduct == 0x0268)
				info->port[port] = SCE_CTRL_TYPE_DS3;
			else if (device->idProduct == 0x05c4)
				info->port[port] = SCE_CTRL_TYPE_DS4;
		}
	}
	info->port[0] = SCE_CTRL_TYPE_VIRT;
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3GetBatteryInfo(int port, uint8_t *battery)
{
	int result, index;

	if (battery == NULL)
		return (int)0x80340001;
	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
		return (int)0x80340020;
	}
	*battery = ds3State.devices[index].batteryInfo;
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3ForegroundChanged(SceUID pid)
{
	ds3State.foregroundProcess = pid;
	sceKernelSetEventFlag(ds3State.workEvent, 0x20);
	return 0;
}

int ds3SetActuator(int port, const SceCtrlActuator *state)
{
	Ds3Device *device;
	int result, index;
	uint8_t pendingWork, largeMotor;
	uint64_t timestamp;

	if (state == NULL)
		return (int)0x80340001;
	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
		return (int)0x80340020;
	}
	device = &ds3State.devices[index];
	if (device->idProduct == 0x0268)
		device->actuatorState.smallMotor = state->small != 0;
	else if (device->idProduct == 0x05c4)
		device->actuatorState.smallMotor = state->small;
	timestamp = sceKernelGetSystemTimeWide();
	pendingWork = device->pendingWork;
	device->actuatorState.timestamp = timestamp;
	largeMotor = state->large;
	device->pendingWork = pendingWork | 2;
	device->actuatorState.largeMotor = largeMotor;
	sceKernelSetEventFlag(ds3State.workEvent, 1);
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3DisconnectController(int port)
{
	int result, index;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
		return (int)0x80340020;
	}
	ds3State.devices[index].pendingWork = 8;
	sceKernelSetEventFlag(ds3State.workEvent, 1);
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3SetAuxiliaryMotor(int port, int level)
{
	Ds3Device *device;
	int index;
	SceUInt32 intr;

	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
		return (int)0x80340020;
	}
	device = &ds3State.devices[index];
	if ((unsigned int)device->auxiliaryMotorReport - 6 <= 1) {
		device->pendingWork = 16;
		device->actuatorState.auxiliaryMotor = level;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
		sceKernelSetEventFlag(ds3State.workEvent, 1);
	} else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	}
	return 0;
}

int ds3GetControllerType(int port, uint8_t *type)
{
	int index;
	SceUInt32 intr;

	if (type == NULL)
		return -1;
	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	*type = SCE_CTRL_TYPE_UNPAIRED;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		goto unlock;
	if (ds3State.devices[index].idProduct == 0x0268)
		*type = SCE_CTRL_TYPE_DS3;
	else if (ds3State.devices[index].idProduct == 0x05c4)
		*type = SCE_CTRL_TYPE_DS4;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	return 0;
}

int ds3GetCalibrationData(int port, SceMotionDevDs3CalibData *data)
{
	Ds3Device *device;
	uint16_t last, calibration[8];
	int index, result = -1;
	SceUInt32 intr;

	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		goto unlock;
	device = &ds3State.devices[index];
	if (device->inputReady == 0)
		goto unlock;
	result = ds3State.safeMode;
	if (result != 0) {
		result = -1;
		goto unlock;
	}
	last = device->ds3Calibration[8];
	calibration[0] = device->ds3Calibration[0];
	calibration[1] = device->ds3Calibration[1];
	calibration[2] = device->ds3Calibration[2];
	calibration[3] = device->ds3Calibration[3];
	calibration[4] = device->ds3Calibration[4];
	calibration[5] = device->ds3Calibration[5];
	calibration[6] = device->ds3Calibration[6];
	calibration[7] = device->ds3Calibration[7];
	data->value[0] = calibration[0];
	data->value[1] = calibration[1];
	data->value[2] = calibration[2];
	data->value[3] = calibration[3];
	data->value[4] = calibration[4];
	data->value[5] = calibration[5];
	data->value[6] = calibration[6];
	data->value[7] = calibration[7];
	data->value[8] = last;
	data->value[9] = device->auxiliaryMotorLevel;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	return result;
}

int ds3GetAccCalibData(int port, SceMotionDevAccCalibData *data)
{
	Ds3Device *device;
	uint16_t positiveX, positiveY, positiveZ, negativeX, negativeY, negativeZ;
	float middleX, middleY, middleZ;
	int index, result = -1;
	SceUInt32 intr;

	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (ds3State.safeMode != 0)
		goto unlock;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		goto unlock;
	device = &ds3State.devices[index];
	if (device->inputReady == 0 || device->idProduct == 0x0268)
		goto unlock;
	negativeY = device->ds4Calibration[15];
	result = 0;
	negativeZ = device->ds4Calibration[16];
	negativeX = device->ds4Calibration[14];
	positiveY = device->ds4Calibration[12];
	positiveX = device->ds4Calibration[11];
	positiveZ = device->ds4Calibration[13];
	middleY = (float)((uint16_t)(((int16_t)negativeY + (int16_t)positiveY) / 2) ^ 0x8000u);
	middleZ = (float)((uint16_t)(((int16_t)negativeZ + (int16_t)positiveZ) / 2) ^ 0x8000u);
	middleX = (float)((uint16_t)(((int16_t)negativeX + (int16_t)positiveX) / 2) ^ 0x8000u);
	data->positive[0].value[1] = middleY;
	data->positive[2].value[1] = middleY;
	data->negative[0].value[1] = middleY;
	data->negative[2].value[1] = middleY;
	data->positive[0].value[2] = middleZ;
	data->positive[1].value[2] = middleZ;
	data->negative[0].value[2] = middleZ;
	data->negative[1].value[2] = middleZ;
	data->positive[1].value[0] = middleX;
	data->positive[2].value[0] = middleX;
	data->negative[1].value[0] = middleX;
	data->negative[2].value[0] = middleX;
	data->positive[0].value[0] = (float)(positiveX ^ 0x8000u);
	data->positive[1].value[1] = (float)(positiveY ^ 0x8000u);
	data->positive[2].value[2] = (float)(positiveZ ^ 0x8000u);
	data->negative[0].value[0] = (float)(negativeX ^ 0x8000u);
	data->negative[1].value[1] = (float)(negativeY ^ 0x8000u);
	data->negative[2].value[2] = (float)(negativeZ ^ 0x8000u);
	data->positive[0].reference = 1.0f;
	data->positive[1].reference = 1.0f;
	data->positive[2].reference = 1.0f;
	data->negative[0].reference = -1.0f;
	data->negative[1].reference = -1.0f;
	data->negative[2].reference = -1.0f;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	return result;
}

int ds3GetGyroCalibData(int port, SceMotionDevGyroCalibData *data)
{
	Ds3Device *device;
	float zeroX, zeroY, zeroZ, positiveRate, negativeRate;
	float positiveX, positiveZ, negativeX, negativeY;
	uint32_t positiveY;
	int index, result = -1;
	SceUInt32 intr;

	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (ds3State.safeMode != 0)
		goto unlock;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		goto unlock;
	device = &ds3State.devices[index];
	if (device->inputReady == 0)
		goto unlock;
	result = 0;
	if (device->idProduct == 0x0268)
		goto unlock;
	zeroZ = (float)((uint16_t)device->ds4Calibration[2] ^ 0x8000u);
	positiveY = (uint16_t)device->ds4Calibration[4] ^ 0x8000u;
	zeroY = (float)((uint16_t)device->ds4Calibration[1] ^ 0x8000u);
	zeroX = (float)((uint16_t)device->ds4Calibration[0] ^ 0x8000u);
	negativeRate = (float)(int16_t)-device->ds4Calibration[10];
	positiveRate = (float)device->ds4Calibration[9];
	positiveX = (float)((uint16_t)device->ds4Calibration[3] ^ 0x8000u);
	positiveZ = (float)((uint16_t)device->ds4Calibration[5] ^ 0x8000u);
	negativeX = (float)((uint16_t)device->ds4Calibration[6] ^ 0x8000u);
	negativeY = (float)((uint16_t)device->ds4Calibration[7] ^ 0x8000u);
	data->zeroRate[1] = zeroY;
	data->positive[0].value[1] = zeroY;
	data->positive[2].value[1] = zeroY;
	data->negative[0].value[1] = zeroY;
	data->positive[1].value[1] = (float)positiveY;
	data->negative[0].reference = negativeRate;
	data->negative[1].reference = negativeRate;
	data->zeroRate[0] = zeroX;
	data->positive[1].value[0] = zeroX;
	data->positive[2].value[0] = zeroX;
	data->negative[1].value[0] = zeroX;
	data->zeroRate[2] = zeroZ;
	data->positive[0].value[2] = zeroZ;
	data->positive[1].value[2] = zeroZ;
	data->negative[0].value[2] = zeroZ;
	data->negative[1].value[2] = zeroZ;
	data->positive[0].value[0] = positiveX;
	data->positive[2].value[2] = positiveZ;
	data->negative[0].value[0] = negativeX;
	data->negative[1].value[1] = negativeY;
	data->positive[0].reference = positiveRate;
	data->positive[1].reference = positiveRate;
	data->positive[2].reference = positiveRate;
	data->negative[2].reference = negativeRate;
	data->negative[2].value[1] = zeroY;
	data->negative[2].value[0] = zeroX;
	data->negative[2].value[2] = (float)((uint16_t)device->ds4Calibration[8] ^ 0x8000u);
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	return result;
}

int ds3SetSingleControllerMode(int enabled)
{
	SceUInt32 intr;

	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	ds3State.singleControllerMode = enabled;
	if (enabled != 0)
		ds3State.singleControllerProcess = sceKernelGetProcessId();
	else
		ds3State.singleControllerProcess = -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	return 0;
}

int ds3ResetLightbar(int port)
{
	Ds3ProcessColors *process = NULL;
	Ds3Device *device;
	SceUID pid;
	int result, index;
	uint8_t pendingWork;

	if ((unsigned int)port - 1u > 3)
		return (int)0x80340001;
	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	pid = sceKernelGetProcessId();
	if (pid == ds3State.processColors[0].pid)
		process = &ds3State.processColors[0];
	else if (pid == ds3State.processColors[1].pid)
		process = &ds3State.processColors[1];
	else if (pid == ds3State.processColors[2].pid)
		process = &ds3State.processColors[2];
	else if (pid == ds3State.processColors[3].pid)
		process = &ds3State.processColors[3];
	else if (pid == ds3State.processColors[4].pid)
		process = &ds3State.processColors[4];
	else if (pid == ds3State.processColors[5].pid)
		process = &ds3State.processColors[5];
	else if (pid == ds3State.processColors[6].pid)
		process = &ds3State.processColors[6];
	else if (pid == ds3State.processColors[7].pid)
		process = &ds3State.processColors[7];
	else {
		if (ds3State.processColors[0].pid == -1)
			process = &ds3State.processColors[0];
		else if (ds3State.processColors[1].pid == -1)
			process = &ds3State.processColors[1];
		else if (ds3State.processColors[2].pid == -1)
			process = &ds3State.processColors[2];
		else if (ds3State.processColors[3].pid == -1)
			process = &ds3State.processColors[3];
		else if (ds3State.processColors[4].pid == -1)
			process = &ds3State.processColors[4];
		else if (ds3State.processColors[5].pid == -1)
			process = &ds3State.processColors[5];
		else if (ds3State.processColors[6].pid == -1)
			process = &ds3State.processColors[6];
		else if (ds3State.processColors[7].pid == -1)
			process = &ds3State.processColors[7];
		if (process != NULL)
			process->pid = pid;
	}
	if (process != NULL) {
		process->ports[port - 1].color[0] = 0;
		process->ports[port - 1].color[1] = 0;
		process->ports[port - 1].color[2] = 0;
		process->ports[port - 1].enabled = 0;
	}
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
		return 0;
	}
	device = &ds3State.devices[index];
	if (device->idProduct == 0x05c4) {
		pendingWork = device->pendingWork;
		device->ledState.lightbarColor[0] = ds3DefaultLightbarColors[port][0];
		device->ledState.lightbarColor[1] = ds3DefaultLightbarColors[port][1];
		device->ledState.lightbarColor[2] = ds3DefaultLightbarColors[port][2];
		device->pendingWork = pendingWork | 1;
		sceKernelSetEventFlag(ds3State.workEvent, 1);
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return result;
}

int ds3SetLightbar(int port, uint8_t red, uint8_t green, uint8_t blue)
{
	Ds3ProcessColors *process = NULL;
	Ds3Device *device;
	SceUID pid;
	int result, index;
	uint8_t pendingWork;

	if ((unsigned int)port - 1u > 3)
		return (int)0x80340001;
	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	pid = sceKernelGetProcessId();
	if (pid == ds3State.processColors[0].pid)
		process = &ds3State.processColors[0];
	else if (pid == ds3State.processColors[1].pid)
		process = &ds3State.processColors[1];
	else if (pid == ds3State.processColors[2].pid)
		process = &ds3State.processColors[2];
	else if (pid == ds3State.processColors[3].pid)
		process = &ds3State.processColors[3];
	else if (pid == ds3State.processColors[4].pid)
		process = &ds3State.processColors[4];
	else if (pid == ds3State.processColors[5].pid)
		process = &ds3State.processColors[5];
	else if (pid == ds3State.processColors[6].pid)
		process = &ds3State.processColors[6];
	else if (pid == ds3State.processColors[7].pid)
		process = &ds3State.processColors[7];
	else {
		if (ds3State.processColors[0].pid == -1)
			process = &ds3State.processColors[0];
		else if (ds3State.processColors[1].pid == -1)
			process = &ds3State.processColors[1];
		else if (ds3State.processColors[2].pid == -1)
			process = &ds3State.processColors[2];
		else if (ds3State.processColors[3].pid == -1)
			process = &ds3State.processColors[3];
		else if (ds3State.processColors[4].pid == -1)
			process = &ds3State.processColors[4];
		else if (ds3State.processColors[5].pid == -1)
			process = &ds3State.processColors[5];
		else if (ds3State.processColors[6].pid == -1)
			process = &ds3State.processColors[6];
		else if (ds3State.processColors[7].pid == -1)
			process = &ds3State.processColors[7];
		if (process != NULL)
			process->pid = pid;
	}
	if (process != NULL) {
		process->ports[port - 1].color[0] = red;
		process->ports[port - 1].color[1] = green;
		process->ports[port - 1].color[2] = blue;
		process->ports[port - 1].enabled = 1;
	}
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
		return 0;
	}
	device = &ds3State.devices[index];
	if (device->idProduct == 0x05c4) {
		pendingWork = device->pendingWork;
		device->ledState.lightbarColor[0] = red;
		device->ledState.lightbarColor[1] = green;
		device->ledState.lightbarColor[2] = blue;
		device->pendingWork = pendingWork | 1;
		sceKernelSetEventFlag(ds3State.workEvent, 1);
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return result;
}

int ds3DisconnectAll(void)
{
	sceKernelClearEventFlag(ds3State.workEvent, ~0x10u);
	sceKernelSetEventFlag(ds3State.workEvent, 0x10);
	ds3DisconnectTrackedDevice();
	return 0;
}

int ds3SwapPorts(int port, int destinationPort)
{
	Ds3Device *first, *second;
	int index, result;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		index = -1;
	first = index < 0 ? NULL : &ds3State.devices[index];
	if (destinationPort == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (destinationPort == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (destinationPort == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (destinationPort == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (destinationPort == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (destinationPort == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (destinationPort == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (destinationPort == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		index = -1;
	second = index < 0 ? NULL : &ds3State.devices[index];
	if (first != NULL) {
		result = ds3SwapTrackedPort(first->deviceAddress, destinationPort);
		if (result == 0) {
			first->port = destinationPort;
			if (second != NULL)
				second->port = port;
			ds3SignalPortChange();
		}
	} else {
		result = (int)0x80340020;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return result;
}

int ds3ReadSensorData(int port, MotionDriverSample *data)
{
	Ds3Device *device;
	const uint8_t *report;
	uint16_t sequence;
	SceUInt32 intr;
	int index;

	if (data == NULL)
		return -1;
	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	memset(data, 0, sizeof(*data));
	if (ds3State.safeMode != 0)
		goto unlock;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		goto unlock;
	device = &ds3State.devices[index];
	if (device->inputReady == 0)
		goto unlock;
	report = device->inputRequests[device->inputIndex ^ 1]->buffer;
	data->identity = device->deviceAddress;
	if (device->idProduct == 0x0268) {
		sequence = device->sampleSequence;
		device->sampleSequence = sequence + 1;
		data->timestamp = sequence;
		data->accel[0] = report[42] | (report[41] << 8);
		data->accel[1] = report[44] | (report[43] << 8);
		data->accel[2] = report[46] | (report[45] << 8);
		data->gyro[0] = report[48] | (report[47] << 8);
	} else if (device->idProduct == 0x05c4) {
		data->timestamp = report[10] | (report[11] << 8);
		data->gyro[0] = report[13] | (report[14] << 8);
		data->gyro[1] = report[15] | (report[16] << 8);
		data->gyro[2] = report[17] | (report[18] << 8);
		data->accel[0] = report[19] | (report[20] << 8);
		data->accel[1] = report[21] | (report[22] << 8);
		data->accel[2] = report[23] | (report[24] << 8);
		data->status = report[12];
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	return 0;
}

int ds3SetIdleTimeout(int seconds)
{
	uint64_t timestamp;
	int result;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	ds3State.idleTimeout = (uint64_t)(unsigned int)seconds * 1000000;
	timestamp = sceKernelGetSystemTimeWide();
	if (ds3State.devices[0].bluetoothState == 2)
		ds3State.devices[0].lastActivityTime = timestamp;
	if (ds3State.devices[1].bluetoothState == 2)
		ds3State.devices[1].lastActivityTime = timestamp;
	if (ds3State.devices[2].bluetoothState == 2)
		ds3State.devices[2].lastActivityTime = timestamp;
	if (ds3State.devices[3].bluetoothState == 2)
		ds3State.devices[3].lastActivityTime = timestamp;
	if (ds3State.devices[4].bluetoothState == 2)
		ds3State.devices[4].lastActivityTime = timestamp;
	if (ds3State.devices[5].bluetoothState == 2)
		ds3State.devices[5].lastActivityTime = timestamp;
	if (ds3State.devices[6].bluetoothState == 2)
		ds3State.devices[6].lastActivityTime = timestamp;
	if (ds3State.devices[7].bluetoothState == 2)
		ds3State.devices[7].lastActivityTime = timestamp;
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3IsSupportedBluetoothDevice(uint64_t address, uint16_t *productId)
{
	SceBtRegisteredInfo info;
	uint16_t product;

	memset(&info, 0, sizeof(info));
	if (SceBtForDriver_F86D25E2(address, &info, 1) < 0)
		return 0;
	product = info.pid;
	*productId = product;
	if (info.vid != 0x054c)
		return 0;
	if (product == 0x0268 || product == 0x042f || product == 0x05c4)
		return 1;
	return 0;
}

int ds3ReadTouchData(int port, TouchVirtualSampleRequest *request)
{
	Ds3Device *device;
	SceTouchData *sample;
	SceTouchData *emptySample;
	const uint8_t *report;
	const uint8_t *packet;
	uint64_t timestamp;
	SceUInt32 intr;
	unsigned int count;
	unsigned int x;
	unsigned int firstCount;
	unsigned int secondIndex;
	unsigned int reportCount;
	int index;
	int result = 0;

	intr = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (ds3State.safeMode != 0)
		goto unlock;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		goto empty;
	device = &ds3State.devices[index];
	if (device->idProduct == 0x0268 || device->inputReady == 0)
		goto empty;
	timestamp = sceKernelGetSystemTimeWide();
	device->lastTouchTime = timestamp;
	report = device->inputRequests[device->inputIndex ^ 1]->buffer;
	count = report[33];
	sample = request->samples[0];
	sample->timeStamp = timestamp;
	if (count == 0) {
		sample->reportNum = 0;
		request->sampleCount = 1;
		result = 1;
		goto unlock;
	}
	packet = report + 34 + 9 * (count - 1);
	if ((packet[1] & 0x80) == 0) {
		sample->report[0].id = packet[1];
		x = packet[2] | ((packet[3] & 0xf) << 8);
		sample = request->samples[0];
		sample->report[0].x = x;
		sample->report[0].y = (packet[3] >> 4) | (packet[4] << 4);
		firstCount = 1;
		secondIndex = 1;
		reportCount = 2;
	} else {
		firstCount = 0;
		secondIndex = 0;
		reportCount = 1;
	}
	if ((packet[5] & 0x80) != 0) {
		reportCount = firstCount;
	} else {
		sample->report[secondIndex].id = packet[5];
		x = packet[6] | ((packet[7] & 0xf) << 8);
		sample = request->samples[0];
		sample->report[secondIndex].x = x;
		sample->report[secondIndex].y = (packet[7] >> 4) | (packet[8] << 4);
	}
	sample->reportNum = reportCount;
	request->sampleCount = count;
	result = count;
	goto unlock;
empty:
	sample = request->samples[0];
	timestamp = sceKernelGetSystemTimeWide();
	emptySample = request->samples[0];
	sample->timeStamp = timestamp;
	emptySample->reportNum = 0;
	request->sampleCount = 1;
	result = 1;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, intr);
	return result;
}

int ds3ExitProcess(SceUID pid, ProcEventParam *params, void *common)
{
	int result;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (pid == ds3State.processColors[0].pid) {
		ds3State.processColors[0].pid = -1;
		ds3State.processColors[0].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[0].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[0].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[0].ports[3] = (Ds3LightbarOverride){{0}, 0};
	}
	if (pid == ds3State.processColors[1].pid) {
		ds3State.processColors[1].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].pid = -1;
	}
	if (pid == ds3State.processColors[2].pid) {
		ds3State.processColors[2].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].pid = -1;
	}
	if (pid == ds3State.processColors[3].pid) {
		ds3State.processColors[3].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].pid = -1;
	}
	if (pid == ds3State.processColors[4].pid) {
		ds3State.processColors[4].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].pid = -1;
	}
	if (pid == ds3State.processColors[5].pid) {
		ds3State.processColors[5].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].pid = -1;
	}
	if (pid == ds3State.processColors[6].pid) {
		ds3State.processColors[6].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].pid = -1;
	}
	if (pid == ds3State.processColors[7].pid) {
		ds3State.processColors[7].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].pid = -1;
	}
	if (pid == ds3State.singleControllerProcess && ds3State.singleControllerMode != 0) {
		ds3State.singleControllerProcess = -1;
		ds3State.singleControllerMode = 0;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return result;
}

int ds3KillProcess(SceUID pid, ProcEventParam *params, void *common)
{
	int result;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (pid == ds3State.processColors[0].pid) {
		ds3State.processColors[0].pid = -1;
		ds3State.processColors[0].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[0].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[0].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[0].ports[3] = (Ds3LightbarOverride){{0}, 0};
	}
	if (pid == ds3State.processColors[1].pid) {
		ds3State.processColors[1].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[1].pid = -1;
	}
	if (pid == ds3State.processColors[2].pid) {
		ds3State.processColors[2].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[2].pid = -1;
	}
	if (pid == ds3State.processColors[3].pid) {
		ds3State.processColors[3].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[3].pid = -1;
	}
	if (pid == ds3State.processColors[4].pid) {
		ds3State.processColors[4].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[4].pid = -1;
	}
	if (pid == ds3State.processColors[5].pid) {
		ds3State.processColors[5].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[5].pid = -1;
	}
	if (pid == ds3State.processColors[6].pid) {
		ds3State.processColors[6].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[6].pid = -1;
	}
	if (pid == ds3State.processColors[7].pid) {
		ds3State.processColors[7].ports[0] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].ports[1] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].ports[2] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].ports[3] = (Ds3LightbarOverride){{0}, 0};
		ds3State.processColors[7].pid = -1;
	}
	if (pid == ds3State.singleControllerProcess && ds3State.singleControllerMode != 0) {
		ds3State.singleControllerProcess = -1;
		ds3State.singleControllerMode = 0;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return result;
}

int ds3AllocateRequests(Ds3Device *controller)
{
	SceAllocOpt option;
	SceBtHidRequest *request;
	void *buffer;
	int index;

	for (index = 0; index < 2; index++) {
		request = sceKernelAllocHeapMemory(ds3State.heap, sizeof(*request));
		if (request != NULL)
			memset(request, 0, sizeof(*request));
		controller->inputRequests[index] = request;
		option.size = sizeof(option);
		option.align = 64;
		buffer = SceSysmemForDriver_49D4DD9B(ds3State.heap, 64, &option);
		if (buffer != NULL)
			memset(buffer, 0, 64);
		request->buffer = buffer;
	}
	request = sceKernelAllocHeapMemory(ds3State.heap, sizeof(*request));
	if (request != NULL)
		memset(request, 0, sizeof(*request));
	controller->outputRequests[0] = request;
	option.align = 64;
	option.size = sizeof(option);
	buffer = SceSysmemForDriver_49D4DD9B(ds3State.heap, 64, &option);
	if (buffer != NULL)
		memset(buffer, 0, 64);
	request->buffer = buffer;
	return 0;
}

int ds3AllocateBluetoothRequests(Ds3Device *controller)
{
	SceBtHidRequest *request;
	void *buffer;
	int index;

	for (index = 0; index < 2; index++) {
		request = sceKernelAllocHeapMemory(ds3State.heap, sizeof(*request));
		if (request != NULL)
			memset(request, 0, sizeof(*request));
		controller->inputRequests[index] = request;
		buffer = sceKernelAllocHeapMemory(ds3State.heap, 80);
		if (buffer != NULL)
			memset(buffer, 0, 80);
		request->buffer = buffer;
	}
	for (index = 0; index < 2; index++) {
		request = sceKernelAllocHeapMemory(ds3State.heap, sizeof(*request));
		if (request != NULL)
			memset(request, 0, sizeof(*request));
		controller->outputRequests[index] = request;
		buffer = sceKernelAllocHeapMemory(ds3State.heap, 80);
		if (buffer != NULL)
			memset(buffer, 0, 80);
		request->buffer = buffer;
	}
	return 0;
}

int ds3BluetoothEnableReports(Ds3Device *controller)
{
	int interruptState;
	int result;
	unsigned int index;

	interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (controller->outputPending > 1) {
		index = controller->outputIndex;
	} else {
		controller->outputPending++;
		index = (controller->outputIndex + 1) & 1;
		controller->outputIndex = index;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
	controller->outputRequests[index]->type = 3;
	controller->outputRequests[index]->reportId = 0;
	controller->outputRequests[index]->length = 4;
	controller->outputRequests[index]->argument = 0;
	memset(controller->outputRequests[index]->buffer, 0, 80);
	((uint8_t *)controller->outputRequests[index]->buffer)[0] = 0xf4;
	((uint8_t *)controller->outputRequests[index]->buffer)[1] = 0x42;
	((uint8_t *)controller->outputRequests[index]->buffer)[2] = 0x0a;
	((uint8_t *)controller->outputRequests[index]->buffer)[3] = 0;
	if (controller->outputPending != 1)
		return 0;
	result = SceBtForDriver_F9DCEC77((uint32_t)controller->deviceAddress,
		controller->deviceAddress >> 32, controller->outputRequests[index]);
	return result < 0 ? result : 0;
}

int ds3BluetoothSetDs4Actuators(Ds3Device *controller,
	const uint8_t *smallMotor, const uint8_t *largeMotor)
{
	int interruptState;
	int result;
	unsigned int index;
	uint8_t *report;
	uint8_t small;

	interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (controller->outputPending > 1) {
		index = controller->outputIndex;
	} else {
		index = controller->outputIndex;
		controller->outputPending++;
		index = (index + 1) & 1;
		controller->outputIndex = index;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
	report = controller->outputRequests[index]->buffer;
	memset(report, 0, 80);
	report[1] = 3;
	report[0] = 17;
	small = *smallMotor;
	report[4] = small;
	report[5] = *largeMotor;
	report[6] = controller->ledState.lightbarColor[0];
	report[7] = controller->ledState.lightbarColor[1];
	report[8] = controller->ledState.lightbarColor[2];
	controller->actuatorState.smallMotor = small;
	controller->actuatorState.largeMotor = report[5];
	controller->outputRequests[index]->type = 1;
	controller->outputRequests[index]->reportId = 17;
	controller->outputRequests[index]->length = 76;
	controller->outputRequests[index]->argument = 0;
	if (controller->outputPending != 1)
		return 0;
	result = SceBtForDriver_F9DCEC77((uint32_t)controller->deviceAddress,
		controller->deviceAddress >> 32, controller->outputRequests[index]);
	return result < 0 ? result : 0;
}

int ds3BluetoothSetDs4Lightbar(Ds3Device *controller, const Ds3LedState *leds)
{
	int interruptState;
	int result;
	unsigned int index;
	uint8_t *report;
	uint8_t red;

	interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (controller->outputPending > 1) {
		index = controller->outputIndex;
	} else {
		controller->outputPending++;
		index = (controller->outputIndex + 1) & 1;
		controller->outputIndex = index;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
	report = controller->outputRequests[index]->buffer;
	memset(report, 0, 80);
	report[0] = 17;
	report[1] = 3;
	report[2] = 0;
	report[3] = 0;
	report[4] = controller->actuatorState.smallMotor;
	report[5] = controller->actuatorState.largeMotor;
	red = leds->lightbarColor[0];
	report[6] = red;
	report[7] = leds->lightbarColor[1];
	report[8] = leds->lightbarColor[2];
	controller->ledState.lightbarColor[0] = red;
	controller->ledState.lightbarColor[1] = report[7];
	controller->ledState.lightbarColor[2] = report[8];
	controller->outputRequests[index]->type = 1;
	controller->outputRequests[index]->reportId = 17;
	controller->outputRequests[index]->length = 76;
	controller->outputRequests[index]->argument = 0;
	if (controller->outputPending != 1)
		return 0;
	result = SceBtForDriver_F9DCEC77((uint32_t)controller->deviceAddress,
		controller->deviceAddress >> 32, controller->outputRequests[index]);
	return result < 0 ? result : 0;
}

int ds3BluetoothSendControl(Ds3Device *controller, uint8_t control)
{
	int interruptState;
	int result;
	unsigned int index;

	interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (controller->outputPending > 1) {
		index = controller->outputIndex;
	} else {
		controller->outputPending++;
		index = (controller->outputIndex + 1) & 1;
		controller->outputIndex = index;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
	controller->outputRequests[index]->type = 3;
	controller->outputRequests[index]->reportId = 0;
	controller->outputRequests[index]->length = 48;
	controller->outputRequests[index]->argument = 0;
	memset(controller->outputRequests[index]->buffer, 0, 80);
	((uint8_t *)controller->outputRequests[index]->buffer)[0] = 0xef;
	((uint8_t *)controller->outputRequests[index]->buffer)[5] = 3;
	((uint8_t *)controller->outputRequests[index]->buffer)[6] = 1;
	((uint8_t *)controller->outputRequests[index]->buffer)[7] = control;
	result = SceBtForDriver_F9DCEC77((uint32_t)controller->deviceAddress,
		controller->deviceAddress >> 32, controller->outputRequests[index]);
	return result < 0 ? result : 0;
}

int ds3StopProcess(SceUID pid, int eventType, ProcEventParam *params, void *common)
{
	Ds3Device *device;
	uint64_t timestamp;
	uint8_t pendingWork;
	int port;
	int index;

	if (eventType != 0x201)
		return 0;
	for (port = 1; port != 5; port++) {
		if (lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex) < 0)
			continue;
		if (port == ds3State.devices[0].port &&
			(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
			index = 0;
		else if (port == ds3State.devices[1].port &&
			(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
			index = 1;
		else if (port == ds3State.devices[2].port &&
			(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
			index = 2;
		else if (port == ds3State.devices[3].port &&
			(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
			index = 3;
		else if (port == ds3State.devices[4].port &&
			(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
			index = 4;
		else if (port == ds3State.devices[5].port &&
			(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
			index = 5;
		else if (port == ds3State.devices[6].port &&
			(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
			index = 6;
		else if (port == ds3State.devices[7].port &&
			(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
			index = 7;
		else
			goto unlock;
		device = &ds3State.devices[index];
		if (device->idProduct == 0x0268 || device->idProduct == 0x05c4)
			device->actuatorState.smallMotor = 0;
		timestamp = sceKernelGetSystemTimeWide();
		pendingWork = device->pendingWork;
		device->actuatorState.timestamp = timestamp;
		device->actuatorState.largeMotor = 0;
		device->pendingWork = pendingWork | 2;
		sceKernelSetEventFlag(ds3State.workEvent, 1);
unlock:
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	}
	if (pid == ds3State.singleControllerProcess && ds3State.singleControllerMode != 0) {
		ds3State.singleControllerProcess = -1;
		ds3State.singleControllerMode = 0;
	}
	return 0;
}

int ds3BluetoothSetDs3Actuators(Ds3Device *controller,
	const uint8_t *smallMotor, const uint8_t *largeMotor)
{
	int interruptState;
	int result;
	unsigned int index;
	uint8_t *report;
	uint8_t mask;

	interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (controller->outputPending > 1) {
		index = controller->outputIndex;
	} else {
		controller->outputPending++;
		index = (controller->outputIndex + 1) & 1;
		controller->outputIndex = index;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
	mask = controller->ledState.mask;
	report = controller->outputRequests[index]->buffer;
	report[10] = mask;
	report[26] = controller->ledState.patterns[0].period;
	report[27] = controller->ledState.patterns[0].duration >> 8;
	report[28] = controller->ledState.patterns[0].duration;
	report[29] = controller->ledState.patterns[0].offTime;
	report[30] = controller->ledState.patterns[0].onTime;
	report[21] = controller->ledState.patterns[1].period;
	report[22] = controller->ledState.patterns[1].duration >> 8;
	report[23] = controller->ledState.patterns[1].duration;
	report[24] = controller->ledState.patterns[1].offTime;
	report[25] = controller->ledState.patterns[1].onTime;
	report[16] = controller->ledState.patterns[2].period;
	report[17] = controller->ledState.patterns[2].duration >> 8;
	report[18] = controller->ledState.patterns[2].duration;
	report[19] = controller->ledState.patterns[2].offTime;
	report[20] = controller->ledState.patterns[2].onTime;
	report[11] = controller->ledState.patterns[3].period;
	report[12] = controller->ledState.patterns[3].duration >> 8;
	report[13] = controller->ledState.patterns[3].duration;
	report[14] = controller->ledState.patterns[3].offTime;
	report[15] = controller->ledState.patterns[3].onTime;
	report[2] = 0x96;
	report[3] = controller->actuatorState.smallMotor;
	if (controller->auxiliaryMotorReport == 6) {
		report[4] = 0xff;
		report[5] = controller->actuatorState.auxiliaryMotor;
	} else {
		report[4] = 0x96;
		report[5] = controller->actuatorState.largeMotor;
	}
	if (controller->auxiliaryMotorReport == 7) {
		report[6] = 0xff;
		report[7] = controller->actuatorState.auxiliaryMotor;
	}
	report[0] = 1;
	report[1] = 0;
	report[3] = *smallMotor;
	report[5] = *largeMotor;
	controller->ledState.mask = mask;
	controller->ledState.patterns[0].period = report[26];
	controller->ledState.patterns[0].duration = (report[27] << 8) | report[28];
	controller->ledState.patterns[0].offTime = report[29];
	controller->ledState.patterns[0].onTime = report[30];
	controller->ledState.patterns[1].period = report[21];
	controller->ledState.patterns[1].duration = (report[22] << 8) | report[23];
	controller->ledState.patterns[1].offTime = report[24];
	controller->ledState.patterns[1].onTime = report[25];
	controller->ledState.patterns[2].period = report[16];
	controller->ledState.patterns[2].duration = (report[17] << 8) | report[18];
	controller->ledState.patterns[2].offTime = report[19];
	controller->ledState.patterns[2].onTime = report[20];
	controller->ledState.patterns[3].period = report[11];
	controller->ledState.patterns[3].duration = (report[12] << 8) | report[13];
	controller->ledState.patterns[3].offTime = report[14];
	controller->ledState.patterns[3].onTime = report[15];
	controller->actuatorState.smallMotor = report[3];
	if (controller->auxiliaryMotorReport == 6) {
		controller->actuatorState.auxiliaryMotor = report[5];
	} else {
		controller->actuatorState.largeMotor = report[5];
		if (controller->auxiliaryMotorReport == 7)
			controller->actuatorState.auxiliaryMotor = report[7];
	}
	controller->outputRequests[index]->type = 1;
	controller->outputRequests[index]->reportId = 0;
	controller->outputRequests[index]->length = 49;
	controller->outputRequests[index]->argument = 0;
	if (controller->outputPending != 1)
		return 0;
	result = SceBtForDriver_F9DCEC77((uint32_t)controller->deviceAddress,
		controller->deviceAddress >> 32, controller->outputRequests[index]);
	return result < 0 ? result : 0;
}

int ds3SysEventHandler(int resume, int eventId, void *args, void *opt)
{
	Ds3Device *device;
	SceBtHidRequest *request;
	SceUInt timeout;
	int result;
	int index;
	int requestIndex;

	if (resume != 0)
		return 0;
	if (eventId != 0x20f)
		return 0;
	timeout = 5000000;
	ds3State.suspending = 1;
	sceKernelClearEventFlag(ds3State.workEvent, ~2U);
	sceKernelClearEventFlag(ds3State.workEvent, ~4U);
	sceKernelClearEventFlag(ds3State.workEvent, ~8U);
	sceKernelSetEventFlag(ds3State.workEvent, 2);
	result = sceKernelWaitEventFlag(ds3State.workEvent, 8, 5, NULL, &timeout);
	if (result == (int)0x80028005) {
		result = lockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
		if (result < 0)
			return result;
		for (index = 0; index < DS3_CONTROLLERS; index++) {
			device = &ds3State.devices[index];
			__builtin_prefetch((void *)((uintptr_t)device + 2 * sizeof(*device)));
			if (device->deviceAddress != 0) {
				for (requestIndex = 0; requestIndex < 2; requestIndex++) {
					request = device->inputRequests[requestIndex];
					if (request->buffer != NULL) {
						sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
						request = device->inputRequests[requestIndex];
					}
					request->buffer = NULL;
					sceKernelFreeHeapMemory(ds3State.heap, request);
					device->inputRequests[requestIndex] = NULL;
				}
				for (requestIndex = 0; requestIndex < 2; requestIndex++) {
					request = device->outputRequests[requestIndex];
					if (request->buffer != NULL) {
						sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
						request = device->outputRequests[requestIndex];
					}
					request->buffer = NULL;
					sceKernelFreeHeapMemory(ds3State.heap, request);
					device->outputRequests[requestIndex] = NULL;
				}
				ds3RemoveTrackedAddress(device->deviceAddress);
			}
			__builtin_prefetch((void *)((uintptr_t)device + 2 * sizeof(*device) +
				offsetof(Ds3Device, usbDeviceId)));
			memset(device, 0, sizeof(*device));
			device->usbDeviceId = DS3_USB_DEVICE_UNUSED;
			device->port = -1;
		}
		ds3State.bluetoothDeviceCount = 0;
		ds3State.pairedUsbDeviceCount = 0;
		ds3State.usbDeviceCount = 0;
		ds3State.activeDeviceCount = 0;
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	}
	ds3State.suspending = 0;
	return 0;
}

int ds3FreeUsbDevice(int deviceId)
{
	Ds3Device *device;
	SceBtHidRequest *request;
	int result;
	int index;
	int requestIndex;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	if (result < 0)
		return result;
	if (deviceId == ds3State.devices[0].usbDeviceId)
		index = 0;
	else if (deviceId == ds3State.devices[1].usbDeviceId)
		index = 1;
	else if (deviceId == ds3State.devices[2].usbDeviceId)
		index = 2;
	else if (deviceId == ds3State.devices[3].usbDeviceId)
		index = 3;
	else if (deviceId == ds3State.devices[4].usbDeviceId)
		index = 4;
	else if (deviceId == ds3State.devices[5].usbDeviceId)
		index = 5;
	else if (deviceId == ds3State.devices[6].usbDeviceId)
		index = 6;
	else if (deviceId == ds3State.devices[7].usbDeviceId)
		index = 7;
	else
		goto unlock;
	device = &ds3State.devices[index];
	device->usbDeviceId = DS3_USB_DEVICE_UNUSED;
	if (ds3State.ds4UsbEnabled != 0 && device->idProduct == 0x05c4)
		ds3RemoveTrackedAddress(device->deviceAddress);
	for (requestIndex = 0; requestIndex < 2; requestIndex++) {
		request = device->inputRequests[requestIndex];
		if (request->buffer != NULL) {
			sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
			request = device->inputRequests[requestIndex];
		}
		request->buffer = NULL;
		sceKernelFreeHeapMemory(ds3State.heap, request);
		device->inputRequests[requestIndex] = NULL;
	}
	request = device->outputRequests[0];
	if (request->buffer != NULL) {
		sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
		request = device->outputRequests[0];
	}
	request->buffer = NULL;
	sceKernelFreeHeapMemory(ds3State.heap, request);
	device->outputRequests[0] = NULL;
	if (ds3State.usbDeviceCount != 0)
		ds3State.usbDeviceCount--;
	if (ds3State.pairedUsbDeviceCount != 0)
		ds3State.pairedUsbDeviceCount--;
	if (ds3State.activeDeviceCount != 0 && device->transport != 1)
		ds3State.activeDeviceCount--;
	memset(device, 0, sizeof(*device));
	device->usbDeviceId = DS3_USB_DEVICE_UNUSED;
	device->port = -1;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	return 0;
}

int ds3FreeBluetoothDevice(uint64_t address)
{
	Ds3Device *device;
	SceBtHidRequest *request;
	int result;
	int index;
	int requestIndex;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	if (result < 0)
		return result;
	if (address == ds3State.devices[0].deviceAddress &&
		(unsigned int)ds3State.devices[0].transport - 3 <= 1)
		index = 0;
	else if (address == ds3State.devices[1].deviceAddress &&
		(unsigned int)ds3State.devices[1].transport - 3 <= 1)
		index = 1;
	else if (address == ds3State.devices[2].deviceAddress &&
		(unsigned int)ds3State.devices[2].transport - 3 <= 1)
		index = 2;
	else if (address == ds3State.devices[3].deviceAddress &&
		(unsigned int)ds3State.devices[3].transport - 3 <= 1)
		index = 3;
	else if (address == ds3State.devices[4].deviceAddress &&
		(unsigned int)ds3State.devices[4].transport - 3 <= 1)
		index = 4;
	else if (address == ds3State.devices[5].deviceAddress &&
		(unsigned int)ds3State.devices[5].transport - 3 <= 1)
		index = 5;
	else if (address == ds3State.devices[6].deviceAddress &&
		(unsigned int)ds3State.devices[6].transport - 3 <= 1)
		index = 6;
	else if (address == ds3State.devices[7].deviceAddress &&
		(unsigned int)ds3State.devices[7].transport - 3 <= 1)
		index = 7;
	else
		goto unlock;
	device = &ds3State.devices[index];
	for (requestIndex = 0; requestIndex < 2; requestIndex++) {
		request = device->inputRequests[requestIndex];
		if (request->buffer != NULL) {
			sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
			request = device->inputRequests[requestIndex];
		}
		request->buffer = NULL;
		sceKernelFreeHeapMemory(ds3State.heap, request);
		device->inputRequests[requestIndex] = NULL;
	}
	for (requestIndex = 0; requestIndex < 2; requestIndex++) {
		request = device->outputRequests[requestIndex];
		if (request->buffer != NULL) {
			sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
			request = device->outputRequests[requestIndex];
		}
		request->buffer = NULL;
		sceKernelFreeHeapMemory(ds3State.heap, request);
		device->outputRequests[requestIndex] = NULL;
	}
	if (ds3State.bluetoothDeviceCount != 0)
		ds3State.bluetoothDeviceCount--;
	if (ds3State.activeDeviceCount != 0 && device->transport != 4)
		ds3State.activeDeviceCount--;
	memset(device, 0, sizeof(*device));
	device->usbDeviceId = DS3_USB_DEVICE_UNUSED;
	device->port = -1;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	return 0;
}

#define DS3_WRITE_LED_PATTERN(_device, _report, _pattern, _position) do { \
	(_report)[(_position)] = (_device)->ledState.patterns[(_pattern)].period; \
	(_report)[(_position) + 1] = (_device)->ledState.patterns[(_pattern)].duration >> 8; \
	(_report)[(_position) + 2] = (_device)->ledState.patterns[(_pattern)].duration; \
	(_report)[(_position) + 3] = (_device)->ledState.patterns[(_pattern)].offTime; \
	(_report)[(_position) + 4] = (_device)->ledState.patterns[(_pattern)].onTime; \
} while (0)

#define DS3_READ_LED_PATTERN(_device, _report, _pattern, _position) do { \
	(_device)->ledState.patterns[(_pattern)].period = (_report)[(_position)]; \
	(_device)->ledState.patterns[(_pattern)].duration = \
		(_report)[(_position) + 2] | (_report)[(_position) + 1] << 8; \
	(_device)->ledState.patterns[(_pattern)].offTime = (_report)[(_position) + 3]; \
	(_device)->ledState.patterns[(_pattern)].onTime = (_report)[(_position) + 4]; \
} while (0)

int _sceDs3WorkerThread(SceSize args, void *argp)
{
	Ds3Device *device;
	Ds3ProcessColors *process;
	Ds3LedState leds;
	uint64_t localAddress = 0, address, now;
	uint32_t events, nowLow;
	SceUInt timeout;
	SceUInt32 interruptState;
	uint8_t *report, pending, mask, onTime, motor, auxiliaryMotorReport;
	int result, retries, ds4Usb, index, output, port, processIndex;

	ds3State.safeMode = SceSysrootForDriver_834439A7();
	if (!ds3State.safeMode) {
		result = sceKernelCreateCallback("SceDs3", 0, _sceDs3BtCb, NULL);
		ds3State.bluetoothCallback = result;
		if (result < 0)
			return result;
		result = SceBtForDriver_120AC1F7(result, 0x7fffffff);
		if (result < 0)
			return result;
		if (!ds3State.safeMode && !SceSysrootForDriver_B0E1FC67()) {
			SceBtForDriver_83781649(0, &localAddress, sizeof(localAddress));
			sceKernelDelayThread(100000);
			for (retries = 100; localAddress == 0 && retries != 0; retries--) {
				SceBtForDriver_83781649(0, &localAddress, sizeof(localAddress));
				sceKernelDelayThread(100000);
			}
		}
	}
	ds3State.localAddress[0] = localAddress >> 40;
	ds3State.localAddress[1] = localAddress >> 32;
	ds3State.localAddress[2] = localAddress >> 24;
	ds3State.localAddress[3] = localAddress >> 16;
	ds3State.localAddress[4] = localAddress >> 8;
	ds3State.localAddress[5] = localAddress;
	if (ds3State.safeMode || SceSysrootForDriver_B0E1FC67() ||
		memcmp(ds3State.localAddress, ds3UnassignedLocalAddress, 6) == 0)
		ds3State.ds4UsbEnabled = 1;
	if ((SceSysconForDriver_CBD6D8BC() & 0x40) &&
		sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "dualshock4_usb", &ds4Usb) == 0 && ds4Usb)
		ds3State.ds4UsbEnabled = 1;
	ds3UsbRegister();
	ds3InitializePortManager();
	ds3InitializeBdRemocon();
	for (;;) {
		timeout = 500000;
		result = SceThreadmgrForDriver_8A35F714(ds3State.workEvent, 55, 1, &events, &timeout);
		if (result < 0)
			goto checkTimeouts;
		result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
#if FW_VERSION >= 0x04000000
		if (result < 0)
			SCE_DBG_ASSERT_FAIL(552);
#endif
		if (events & 1) {
			sceKernelClearEventFlag(ds3State.workEvent, ~1u);
			for (index = 0; index != 8; index++) {
				device = &ds3State.devices[index];
				pending = device->pendingWork;
				if (pending & 1) {
					pending &= ~1u;
					device->pendingWork = pending;
					if (device->usbState == 2) {
						if (device->idProduct == 0x0268)
							ds3UsbSetLeds(device, &device->ledState);
						else if (device->idProduct == 0x05c4)
							ds3UsbSetLightbar(device, &device->ledState);
						pending = device->pendingWork;
					} else if (device->bluetoothState == 2) {
						if (device->idProduct == 0x0268) {
							interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
							if (device->outputPending <= 1) {
								device->outputPending++;
								device->outputIndex = (device->outputIndex + 1) & 1;
							}
							output = device->outputIndex;
							sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
							report = device->outputRequests[output]->buffer;
							memset(report, 0, 80);
							report[0] = 1;
							report[10] = device->ledState.mask;
							DS3_WRITE_LED_PATTERN(device, report, 0, 26);
							DS3_WRITE_LED_PATTERN(device, report, 1, 21);
							DS3_WRITE_LED_PATTERN(device, report, 2, 16);
							report[11] = device->ledState.patterns[3].period;
							report[12] = device->ledState.patterns[3].duration >> 8;
							report[13] = device->ledState.patterns[3].duration;
							report[14] = device->ledState.patterns[3].offTime;
							onTime = device->ledState.patterns[3].onTime;
							report[2] = 0x96;
							report[15] = onTime;
							report[3] = device->actuatorState.smallMotor;
							if (device->auxiliaryMotorReport == 6) {
								report[4] = 255;
								motor = device->actuatorState.auxiliaryMotor;
							} else {
								report[4] = 0x96;
								motor = device->actuatorState.largeMotor;
							}
							report[5] = motor;
							if (device->auxiliaryMotorReport == 7) {
								report[6] = 255;
								report[7] = device->actuatorState.auxiliaryMotor;
							}
							mask = device->ledState.mask;
							report[10] = mask;
							DS3_WRITE_LED_PATTERN(device, report, 0, 26);
							DS3_WRITE_LED_PATTERN(device, report, 1, 21);
							DS3_WRITE_LED_PATTERN(device, report, 2, 16);
							DS3_WRITE_LED_PATTERN(device, report, 3, 11);
							device->ledState.mask = mask;
							DS3_READ_LED_PATTERN(device, report, 0, 26);
							DS3_READ_LED_PATTERN(device, report, 1, 21);
							device->ledState.patterns[2].period = report[16];
							device->ledState.patterns[2].duration = report[18] | report[17] << 8;
							device->ledState.patterns[2].offTime = report[19];
							auxiliaryMotorReport = device->auxiliaryMotorReport;
							device->ledState.patterns[2].onTime = report[20];
							DS3_READ_LED_PATTERN(device, report, 3, 11);
							device->actuatorState.smallMotor = report[3];
							if (auxiliaryMotorReport == 6)
								device->actuatorState.auxiliaryMotor = report[5];
							else {
								device->actuatorState.largeMotor = report[5];
								if (auxiliaryMotorReport == 7)
									device->actuatorState.auxiliaryMotor = report[7];
							}
							device->outputRequests[output]->type = 1;
							device->outputRequests[output]->reportId = 0;
							device->outputRequests[output]->length = 49;
							device->outputRequests[output]->argument = 0;
							if (device->outputPending == 1)
								SceBtForDriver_F9DCEC77((uint32_t)device->deviceAddress,
									device->deviceAddress >> 32, device->outputRequests[output]);
						} else if (device->idProduct == 0x05c4)
							ds3BluetoothSetDs4Lightbar(device, &device->ledState);
						pending = device->pendingWork;
					}
				}
				if (pending & 2) {
					pending &= ~2u;
					device->pendingWork = pending;
					if (device->usbState == 2) {
						if (device->idProduct == 0x0268)
							ds3UsbSetActuators(device, &device->actuatorState);
						else if (device->idProduct == 0x05c4)
							ds3UsbSetDs4Actuators(device, &device->actuatorState);
						pending = device->pendingWork;
					} else if (device->bluetoothState == 2) {
						if (device->idProduct == 0x0268 && device->auxiliaryMotorReport != 6)
							ds3BluetoothSetDs3Actuators(device, &device->actuatorState.smallMotor,
								&device->actuatorState.largeMotor);
						else if (device->idProduct == 0x05c4)
							ds3BluetoothSetDs4Actuators(device, &device->actuatorState.smallMotor,
								&device->actuatorState.largeMotor);
						pending = device->pendingWork;
					}
				}
				if (pending & 8) {
					device->pendingWork = pending & ~8u;
					if (device->usbState == 2) {
						device->usbState = 4;
						if (device->idProduct == 0x0268) {
							__builtin_memset(&leds, 0, sizeof(leds));
							device->actuatorState.largeMotor = 0;
							device->actuatorState.smallMotor = 0;
							device->setupStage = 51;
							leds.mask = 30;
							leds.patterns[0].period = 255;
							leds.patterns[1].period = 255;
							leds.patterns[2].period = 255;
							leds.patterns[3].period = 255;
							leds.patterns[0].duration = 10000;
							leds.patterns[1].duration = 10000;
							leds.patterns[2].duration = 10000;
							leds.patterns[3].duration = 10000;
							ds3UsbSetLeds(device, &leds);
						} else if (device->idProduct == 0x05c4) {
							__builtin_memset(&leds, 0, sizeof(leds));
							device->actuatorState.largeMotor = 0;
							device->actuatorState.smallMotor = 0;
							device->setupStage = 20;
							ds3UsbSetLightbar(device, &leds);
						}
					} else if (device->bluetoothState == 2) {
						address = device->deviceAddress;
						device->bluetoothState = 4;
						SceBtForDriver_50710281((uint32_t)address, address >> 32);
					}
				} else if (pending & 16) {
					device->pendingWork = pending & ~16u;
					if (device->idProduct != 0x0268)
						continue;
					if (device->usbState == 2) {
						ds3UsbSetAuxiliaryMotor(device);
					} else if (device->bluetoothState == 2) {
						interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
						if (device->outputPending <= 1) {
							device->outputPending++;
							device->outputIndex = (device->outputIndex + 1) & 1;
						}
						output = device->outputIndex;
						sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
						mask = device->ledState.mask;
						report = device->outputRequests[output]->buffer;
						report[10] = mask;
						DS3_WRITE_LED_PATTERN(device, report, 0, 26);
						DS3_WRITE_LED_PATTERN(device, report, 1, 21);
						DS3_WRITE_LED_PATTERN(device, report, 2, 16);
						report[11] = device->ledState.patterns[3].period;
						report[12] = device->ledState.patterns[3].duration >> 8;
						report[13] = device->ledState.patterns[3].duration;
						report[14] = device->ledState.patterns[3].offTime;
						onTime = device->ledState.patterns[3].onTime;
						report[2] = 0x96;
						report[15] = onTime;
						report[3] = device->actuatorState.smallMotor;
						if (device->auxiliaryMotorReport == 6) {
							report[4] = 255;
							motor = device->actuatorState.auxiliaryMotor;
						} else {
							report[4] = 0x96;
							motor = device->actuatorState.largeMotor;
						}
						report[5] = motor;
						if (device->auxiliaryMotorReport == 7) {
							report[6] = 255;
							report[7] = device->actuatorState.auxiliaryMotor;
						}
						report[0] = 1;
						report[1] = 0;
						auxiliaryMotorReport = device->auxiliaryMotorReport;
						if (auxiliaryMotorReport == 6) {
							report[4] = 255;
							report[5] = device->actuatorState.auxiliaryMotor;
							auxiliaryMotorReport = device->auxiliaryMotorReport;
						} else if (auxiliaryMotorReport == 7) {
							report[6] = 255;
							report[7] = device->actuatorState.auxiliaryMotor;
							auxiliaryMotorReport = device->auxiliaryMotorReport;
						}
						device->ledState.mask = mask;
						DS3_READ_LED_PATTERN(device, report, 0, 26);
						DS3_READ_LED_PATTERN(device, report, 1, 21);
						DS3_READ_LED_PATTERN(device, report, 2, 16);
						DS3_READ_LED_PATTERN(device, report, 3, 11);
						device->actuatorState.smallMotor = report[3];
						if (auxiliaryMotorReport == 6)
							device->actuatorState.auxiliaryMotor = report[5];
						else {
							device->actuatorState.largeMotor = report[5];
							if (auxiliaryMotorReport == 7)
								device->actuatorState.auxiliaryMotor = report[7];
						}
						device->outputRequests[output]->type = 1;
						device->outputRequests[output]->reportId = 0;
						device->outputRequests[output]->length = 49;
						device->outputRequests[output]->argument = 0;
						SceBtForDriver_F9DCEC77((uint32_t)device->deviceAddress,
							device->deviceAddress >> 32, device->outputRequests[output]);
					}
				}
			}
		}
		if (events & 2) {
			sceKernelClearEventFlag(ds3State.workEvent, ~2u);
			for (index = 0; index != 8; index++) {
				device = &ds3State.devices[index];
				if (device->usbState == 2) {
					device->usbState = 4;
					if (device->idProduct == 0x0268) {
						ds3UsbSetDs3Control(device, 11);
						ds3RemoveTrackedAddress(device->deviceAddress);
						ds3FreeUsbDevice(device->usbDeviceId);
					} else if (device->idProduct == 0x05c4) {
						__builtin_memset(&leds, 0, sizeof(leds));
						ds3UsbSetLightbar(device, &leds);
						ds3RemoveTrackedAddress(device->deviceAddress);
						ds3FreeUsbDevice(device->usbDeviceId);
						device->inputReady = 0;
					}
				} else if (device->usbState <= 2) {
					if (device->bluetoothState == 2) {
						device->bluetoothState = 4;
						SceBtForDriver_50710281((uint32_t)device->deviceAddress,
							device->deviceAddress >> 32);
					}
				} else {
					ds3FreeUsbDevice(device->usbDeviceId);
				}
			}
			sceKernelSetEventFlag(ds3State.workEvent, 4);
		}
		if ((events & 4) && ds3State.activeDeviceCount == 0) {
			sceKernelClearEventFlag(ds3State.workEvent, ~4u);
			sceKernelSetEventFlag(ds3State.workEvent, 8);
		}
		if (events & 16) {
			sceKernelClearEventFlag(ds3State.workEvent, ~16u);
			for (index = 0; index != 8; index++) {
				device = &ds3State.devices[index];
				if (device->bluetoothState == 2) {
					device->bluetoothState = 4;
					SceBtForDriver_50710281((uint32_t)device->deviceAddress,
						device->deviceAddress >> 32);
					sceKernelDelayThread(150000);
				}
			}
		}
		if (events & 32) {
			sceKernelClearEventFlag(ds3State.workEvent, ~32u);
			for (processIndex = 0; processIndex != 8; processIndex++)
				if (ds3State.processColors[processIndex].pid == ds3State.foregroundProcess)
					break;
			if (processIndex == 8) {
				for (processIndex = 0; processIndex != 8; processIndex++)
					if (ds3State.processColors[processIndex].pid == -1)
						break;
				if (processIndex == 8)
					goto unlock;
				ds3State.processColors[processIndex].pid = ds3State.foregroundProcess;
			}
			process = &ds3State.processColors[processIndex];
			for (port = 1; port != 5; port++) {
				for (index = 0; index != 8; index++) {
					device = &ds3State.devices[index];
					if (device->port == port &&
						(device->bluetoothState == 2 || device->usbState == 2))
						break;
				}
				if (index == 8 || device->idProduct != 0x05c4)
					continue;
				if (process->ports[port - 1].enabled) {
					leds.lightbarColor[0] = process->ports[port - 1].color[0];
					leds.lightbarColor[1] = process->ports[port - 1].color[1];
					leds.lightbarColor[2] = process->ports[port - 1].color[2];
				} else {
					leds.lightbarColor[0] = ds3DefaultLightbarColors[port][0];
					leds.lightbarColor[1] = ds3DefaultLightbarColors[port][1];
					leds.lightbarColor[2] = ds3DefaultLightbarColors[port][2];
				}
				if (device->usbState == 2)
					ds3UsbSetLightbar(device, &leds);
				else if (device->bluetoothState == 2)
					ds3BluetoothSetDs4Lightbar(device, &leds);
			}
		}
unlock:
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
checkTimeouts:
		if (ds3State.idleTimeout != 0) {
			now = sceKernelGetSystemTimeWide();
			for (index = 0; index != 8; index++) {
				device = &ds3State.devices[index];
				if (device->bluetoothState != 2)
					continue;
				if (device->activityFlag) {
					device->activityFlag = 0;
					device->lastActivityTime = now;
				} else if (ds3State.idleTimeout < now - device->lastActivityTime) {
					address = device->deviceAddress;
					device->bluetoothState = 4;
					SceBtForDriver_50710281((uint32_t)address, address >> 32);
				}
			}
		}
		nowLow = sceKernelGetSystemTimeWide();
		for (index = 0; index != 8; index++) {
			device = &ds3State.devices[index];
			if ((device->bluetoothState != 2 && device->usbState != 2) ||
				(int32_t)(nowLow - (uint32_t)device->actuatorState.timestamp) <= 3000000 ||
				(device->actuatorState.smallMotor == 0 && device->actuatorState.largeMotor == 0))
				continue;
			device->actuatorState.smallMotor = 0;
			device->actuatorState.largeMotor = 0;
			if (device->usbState == 2) {
				if (device->idProduct == 0x0268)
					ds3UsbSetActuators(device, &device->actuatorState);
				else if (device->idProduct == 0x05c4)
					ds3UsbSetDs4Actuators(device, &device->actuatorState);
			} else if (device->bluetoothState == 2) {
				if (device->idProduct == 0x0268 && device->auxiliaryMotorReport != 6)
					ds3BluetoothSetDs3Actuators(device, &device->actuatorState.smallMotor,
						&device->actuatorState.largeMotor);
				else if (device->idProduct == 0x05c4)
					ds3BluetoothSetDs4Actuators(device, &device->actuatorState.smallMotor,
						&device->actuatorState.largeMotor);
			}
		}
	}
}

#undef DS3_WRITE_LED_PATTERN
#undef DS3_READ_LED_PATTERN

int _sceDs3BtCb(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	SceBtEvent events[2];
	SceBtRegisteredInfo registeredInfo;
	uint8_t firmwareInfo[48];
	Ds3Device *device;
	SceBtHidRequest *request;
	uint8_t *report;
	uint64_t address;
	unsigned int interruptState;
	unsigned int requestIndex;
	unsigned int reportId, reportLength;
	int result, count, eventIndex, index, item;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(745);
#endif
readEvents:
	do {
		count = SceBtForDriver_5ABB9A9D(events, 2);
	} while (count == (int)0x802f0c04);
	if (count <= 0)
		goto unlock;
	for (eventIndex = 0; eventIndex < count; eventIndex++) {
		SceBtEvent *event = &events[eventIndex];

		address = event->mac0 | ((uint64_t)event->mac1 << 32);
		switch (event->id) {
		case 5:
			result = lockFastMutex(
				(SceKernelFastMutex *)&ds3State.registrationMutex);
			if (result < 0)
				goto rejectConnection;
			memset(&registeredInfo, 0, sizeof(registeredInfo));
			if (SceBtForDriver_F86D25E2(address, &registeredInfo, 1) < 0 ||
				registeredInfo.vid != 0x054c ||
				(registeredInfo.pid != 0x0268 && registeredInfo.pid != 0x042f &&
				 registeredInfo.pid != 0x05c4)) {
				result = -101;
				goto unlockRegistration;
			}
			if (ds3State.singleControllerMode != 0) {
				if (ds3State.activeDeviceCount != 0 ||
					ds3State.bluetoothDeviceCount > 3) {
					result = -1;
					goto unlockRegistration;
				}
			} else {
				if (ds3State.activeDeviceCount > 3) {
					result = -100;
					goto unlockRegistration;
				}
				if (ds3State.bluetoothDeviceCount > 3) {
					result = -1;
					goto unlockRegistration;
				}
			}
			if (ds3State.devices[0].deviceAddress == 0 &&
				ds3State.devices[0].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 0;
			else if (ds3State.devices[1].deviceAddress == 0 &&
				ds3State.devices[1].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 1;
			else if (ds3State.devices[2].deviceAddress == 0 &&
				ds3State.devices[2].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 2;
			else if (ds3State.devices[3].deviceAddress == 0 &&
				ds3State.devices[3].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 3;
			else if (ds3State.devices[4].deviceAddress == 0 &&
				ds3State.devices[4].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 4;
			else if (ds3State.devices[5].deviceAddress == 0 &&
				ds3State.devices[5].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 5;
			else if (ds3State.devices[6].deviceAddress == 0 &&
				ds3State.devices[6].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 6;
			else if (ds3State.devices[7].deviceAddress == 0 &&
				ds3State.devices[7].usbDeviceId == DS3_USB_DEVICE_UNUSED)
				index = 7;
			else {
				result = -100;
				goto unlockRegistration;
			}
			device = &ds3State.devices[index];
			memset(device, 0, sizeof(*device));
			ds3AllocateBluetoothRequests(device);
			device->deviceAddress = address;
			device->port = index;
			device->transport = 3;
			device->idProduct = registeredInfo.pid;
			ds3State.bluetoothDeviceCount++;
			ds3State.activeDeviceCount++;
			sceKernelUnlockFastMutex(
				(SceKernelFastMutex *)&ds3State.registrationMutex);
			address = event->mac0 | ((uint64_t)event->mac1 << 32);
			for (index = 0; index < 8; index++) {
				device = &ds3State.devices[index];
				if (device->deviceAddress == address && device->transport == 3)
					break;
			}
			result = 0;
			if (index == 8)
				break;
			device->bluetoothState = 1;
			if (device->idProduct == 0x0268) {
				device->setupStage = 0;
				reportId = 1;
				reportLength = 50;
				goto featureRequest;
			}
			if (device->idProduct == 0x05c4) {
				if (ds3State.ds4UsbEnabled != 0)
					SceBtForDriver_50710281(address, address >> 32);
				device->setupStage = 0;
				reportId = 6;
				reportLength = 53;
				goto featureRequest;
			}
			break;
unlockRegistration:
			sceKernelUnlockFastMutex(
				(SceKernelFastMutex *)&ds3State.registrationMutex);
rejectConnection:
			if (result != -101) {
				if (result == -100)
					SceCtrlForDriver_6E183B9B((int)0x80340101);
				SceBtForDriver_50710281(event->mac0, event->mac1);
			}
			break;
		case 6:
			for (index = 0; index < 8; index++) {
				device = &ds3State.devices[index];
				if (device->deviceAddress == address &&
					(unsigned int)device->transport - 3 <= 1)
					break;
			}
			if (index == 8)
				break;
			if (device->idProduct == 0x0268) {
				ds3FreeBluetoothDevice(address);
				address = event->mac0 | ((uint64_t)event->mac1 << 32);
				if (address != ds3State.devices[0].deviceAddress &&
					address != ds3State.devices[1].deviceAddress &&
					address != ds3State.devices[2].deviceAddress &&
					address != ds3State.devices[3].deviceAddress &&
					address != ds3State.devices[4].deviceAddress &&
					address != ds3State.devices[5].deviceAddress &&
					address != ds3State.devices[6].deviceAddress &&
					address != ds3State.devices[7].deviceAddress)
					ds3RemoveTrackedAddress(address);
			} else if (device->idProduct == 0x05c4) {
				ds3RemoveTrackedAddress(address);
				address = event->mac0 | ((uint64_t)event->mac1 << 32);
				ds3FreeBluetoothDevice(address);
			}
			break;
		case 10:
			for (index = 0; index < 8; index++) {
				device = &ds3State.devices[index];
				if (device->deviceAddress == address && device->transport == 3)
					break;
			}
			if (index == 8)
				break;
			if (device->inputReady == 0) {
				device->inputReady = 1;
				device->bluetoothState = 2;
				device->lastActivityTime = sceKernelGetSystemTimeWide();
				if (device->idProduct == 0x0268)
					result = ds3AssignPort(device->deviceAddress, &device->port, 1);
				else if (device->idProduct == 0x05c4)
					result = ds3AssignPort(device->deviceAddress, &device->port, 2);
				if (result < 0)
					device->port = -1;
				else {
					ds3SignalPortChange();
					SceKernelSuspendForDriver_E0489831(0);
				}
			}
			address = event->mac0 | ((uint64_t)event->mac1 << 32);
			goto inputRequest;
		case 11:
		case 12:
			for (index = 0; index < 8; index++) {
				device = &ds3State.devices[index];
				if (device->deviceAddress == address && device->transport == 3)
					break;
			}
			if (index == 8 || device->outputPending == 0)
				break;
			device->outputPending--;
			if (device->outputPending != 0) {
				result = SceBtForDriver_F9DCEC77(device->deviceAddress,
					device->deviceAddress >> 32,
					device->outputRequests[device->outputIndex]);
				break;
			}
			if (device->idProduct == 0x0268) {
				if (event->status == 1) {
					device->setupRetries++;
					if (device->setupRetries > 10)
						break;
					goto retryDs3;
				}
				switch (device->setupStage) {
				case 0:
					report = device->outputRequests[device->outputIndex]->buffer;
					for (item = 0; item < report[37]; item++) {
						if ((unsigned int)report[38 + item] - 6 <= 1)
							device->auxiliaryMotorReport = report[38 + item];
					}
					device->setupStage = 1;
					goto retryDs3;
				case 1:
					device->setupStage = 2;
					ds3BluetoothSendControl(device, 0xa0);
					break;
				case 2:
					device->setupStage = 3;
					goto retryDs3;
				case 3:
					report = device->outputRequests[device->outputIndex]->buffer;
					device->ds3Calibration[0] = (report[17] << 8) | report[18];
					device->ds3Calibration[1] = (report[19] << 8) | report[20];
					device->ds3Calibration[3] = (report[21] << 8) | report[22];
					device->ds3Calibration[4] = (report[23] << 8) | report[24];
					device->ds3Calibration[6] = (report[25] << 8) | report[26];
					device->ds3Calibration[7] = (report[27] << 8) | report[28];
					device->ds3Calibration[8] = (report[29] << 8) | report[30];
					device->auxiliaryMotorLevel = report[32];
					device->setupStage = 4;
					ds3BluetoothSendControl(device, 0xb0);
					break;
				case 4:
					device->setupStage = 5;
					goto retryDs3;
				case 5:
					report = device->outputRequests[device->outputIndex]->buffer;
					device->ds3Calibration[2] = (report[17] << 8) | report[18];
					device->ds3Calibration[5] = (report[19] << 8) | report[20];
					device->setupStage = 6;
					goto retryDs3;
				case 6:
					report = device->outputRequests[device->outputIndex]->buffer;
					device->deviceSubtype = report[1];
					device->deviceType = report[2];
					if ((unsigned int)device->deviceType - 1 <= 1) {
						SceBtForDriver_50710281(device->deviceAddress,
							device->deviceAddress >> 32);
						device->setupStage = 0;
						break;
					}
					device->setupStage = 7;
					interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
					requestIndex = device->outputIndex;
					if (device->outputPending <= 1) {
						device->outputPending++;
						requestIndex = (requestIndex + 1) & 1;
						device->outputIndex = requestIndex;
					}
					sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
					request = device->outputRequests[requestIndex];
					report = request->buffer;
					report[0] = 1;
					report[1] = 0;
					report[3] = 0;
					if (device->auxiliaryMotorReport == 6) {
						report[4] = 255;
						report[5] = device->auxiliaryMotorLevel;
					} else if (device->auxiliaryMotorReport == 7) {
						report[6] = 255;
						report[7] = device->auxiliaryMotorLevel;
					}
					report[27] = report[22] = 39;
					report[28] = report[23] = 16;
					report[29] = report[30] = 0;
					report[26] = report[21] = 255;
					report[17] = report[12] = 39;
					report[18] = report[13] = 16;
					report[24] = report[25] = 0;
					report[19] = report[20] = 0;
					report[14] = report[15] = 0;
					report[10] = 30;
					report[16] = report[11] = 255;
					device->ledState.mask = 30;
					for (item = 0; item < 4; item++) {
						int position = 26 - item * 5;

						device->ledState.patterns[item].period = report[position];
						device->ledState.patterns[item].duration =
							(report[position + 1] << 8) | report[position + 2];
						device->ledState.patterns[item].offTime = report[position + 3];
						device->ledState.patterns[item].onTime = report[position + 4];
					}
					device->actuatorState.smallMotor = report[3];
					if (device->auxiliaryMotorReport == 6)
						device->actuatorState.auxiliaryMotor = report[5];
					else {
						device->actuatorState.largeMotor = report[5];
						if (device->auxiliaryMotorReport == 7)
							device->actuatorState.auxiliaryMotor = report[7];
					}
					request->type = 1;
					request->reportId = 0;
					request->argument = 0;
					request->length = 49;
					SceBtForDriver_F9DCEC77(device->deviceAddress,
						device->deviceAddress >> 32, request);
					break;
				case 7:
					device->setupStage = 8;
					goto retryDs3;
				case 8:
					report = device->outputRequests[device->outputIndex]->buffer;
					device->connectionStatus = report[1];
					device->batteryInfo = report[7];
					device->setupStage = 31;
					ds3BluetoothEnableReports(device);
					break;
				case 31:
					device->setupStage = 32;
					address = device->deviceAddress;
					goto inputRequest;
				}
				break;
retryDs3:
				switch (device->setupStage) {
				case 0: reportId = 1; reportLength = 50; break;
				case 1: reportId = 0xf2; reportLength = 18; break;
				case 2: ds3BluetoothSendControl(device, 0xa0); goto nextEvent;
				case 3:
				case 5: reportId = 0xef; reportLength = 48; break;
				case 4: ds3BluetoothSendControl(device, 0xb0); goto nextEvent;
				case 6: reportId = 0xf8; reportLength = 6; break;
				case 8: reportId = 0xf7; reportLength = 12; break;
				case 31: ds3BluetoothEnableReports(device); goto nextEvent;
				default: goto nextEvent;
				}
				goto featureRequest;
			}
			if (device->idProduct != 0x05c4)
				break;
			if (event->status == 1) {
				device->setupRetries++;
				if (device->setupRetries > 10)
					break;
				if (device->setupStage == 0) {
					reportId = 6;
					reportLength = 53;
					goto featureRequest;
				}
				if (device->setupStage != 1)
					break;
			} else if (device->setupStage == 0) {
				report = device->outputRequests[device->outputIndex]->buffer;
				memcpy(firmwareInfo, report + 1, sizeof(firmwareInfo));
				device->setupStage = 1;
			} else if (device->setupStage == 1) {
				memset(device->ds4Calibration, 0, sizeof(device->ds4Calibration));
				report = device->outputRequests[device->outputIndex]->buffer;
				for (item = 0; item < 12; item++)
					device->ds4Calibration[item] =
						report[1 + item * 2] | (report[2 + item * 2] << 8);
				device->ds4Calibration[12] = report[27] | (report[28] << 8);
				device->ds4Calibration[13] = report[31] | (report[32] << 8);
				device->ds4Calibration[14] = report[25] | (report[26] << 8);
				device->ds4Calibration[15] = report[29] | (report[30] << 8);
				device->ds4Calibration[16] = report[33] | (report[34] << 8);
				device->ds4Calibration[17] = report[35] | (report[36] << 8);
				device->setupStage = 2;
				address = device->deviceAddress;
				goto inputRequest;
			} else
				break;
			reportId = 5;
			reportLength = 41;
featureRequest:
			interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
			requestIndex = device->outputIndex;
			if (device->outputPending <= 1) {
				device->outputPending++;
				requestIndex = (requestIndex + 1) & 1;
				device->outputIndex = requestIndex;
			}
			sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
			request = device->outputRequests[requestIndex];
			request->type = 2;
			request->reportId = reportId;
			request->length = reportLength;
			request->argument = 0;
			if (device->outputPending == 1)
				SceBtForDriver_F9DCEC77(device->deviceAddress,
					device->deviceAddress >> 32, request);
			break;
inputRequest:
			for (index = 0; index < 8; index++) {
				device = &ds3State.devices[index];
				if (device->deviceAddress == address && device->transport == 3)
					break;
			}
			if (index == 8)
				break;
			interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
			requestIndex = device->inputIndex ^ 1;
			device->inputIndex = requestIndex;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
			request = device->inputRequests[requestIndex];
			request->type = 0;
			request->reportId = 0;
			request->argument = 0;
			request->length = 80;
			SceBtForDriver_F9DCEC77(device->deviceAddress,
				device->deviceAddress >> 32, request);
			break;
		}
nextEvent:
		;
	}
	if (count == 2)
		goto readEvents;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3ReadButtons(int port, struct ctrlNativeSample *output)
{
	uint8_t report[80];
	Ds3Device *device;
	struct ctrlNativeData *sample = &output->sample;
	unsigned int interruptState;
	uint32_t previousButtons;
	int centered[4], analog[4], wide[4];
	int result, index, axis, leftX;

	if ((unsigned int)port > 7)
		return -1;
	interruptState = SceCpuForDriver_D32ACE9E(&ds3State.lock);
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		result = 0;
		goto unlock;
	}
	device = &ds3State.devices[index];
	memset(report, 0, sizeof(report));
	if (device->idProduct == 0x0268) {
		if (device->transport == 2) {
			result = ds3UsbReadInput(port, (Ds3UsbInputReport *)report);
			if (result <= 0)
				goto unlock;
			leftX = report[6];
		} else if (device->transport == 3) {
			if (ds3State.safeMode != 0 || device->inputReady == 0) {
				result = 0;
				goto unlock;
			}
			memcpy(report, device->inputRequests[device->inputIndex ^ 1]->buffer,
				sizeof(report));
			leftX = report[6];
			if (report[6] == 0 && report[7] == 0) {
				leftX = report[8];
				if (report[8] != 0)
					leftX = report[7];
				else if (report[9] == 0) {
					report[9] = report[8] = report[7] = report[6] = 128;
					leftX = 128;
				}
			}
			device->batteryInfo = report[30];
		} else {
			result = 0;
			goto unlock;
		}
		if (device->powerStateChecked == 0) {
			device->powerStateChecked = 1;
			if (ScePowerForDriver_394DE492() == 0)
				report[4] |= 1;
			leftX = report[6];
		}
		output->extended[2] = report[8];
		sample->wideRx = report[8];
		output->extended[0] = leftX;
		sample->wideLx = leftX;
		output->extended[1] = report[7];
		sample->wideLy = report[7];
		output->extended[3] = report[9];
		sample->wideRy = report[9];
		sample->buttons |= report[2] | (report[3] << 8) | ((report[4] & 1) << 16);
		centered[0] = leftX - 128;
		centered[1] = report[7] - 128;
		centered[2] = report[8] - 128;
		centered[3] = report[9] - 128;
		for (axis = 0; axis < 4; axis++) {
			analog[axis] = 80 * centered[axis] / 66 + 128;
			if (analog[axis] < 0)
				analog[axis] = 0;
			else if (analog[axis] > 255)
				analog[axis] = 255;
		}
		sample->lx = analog[0];
		sample->rx = analog[2];
		sample->ry = analog[3];
		sample->ly = analog[1];
		sample->l2 = report[18];
		sample->r2 = report[19];
		sample->up = sample->right = sample->down = sample->left = 0;
		sample->l1 = sample->r1 = 0;
		sample->triangle = sample->circle = sample->cross = sample->square = 0;
		previousButtons = device->previousButtons;
		device->buttons = sample->buttons;
		device->previousButtons = sample->buttons;
		if (sample->buttons != previousButtons)
			device->activityFlag = 1;
		for (axis = 0; axis < 4; axis++) {
			if (centered[axis] > 37 || centered[axis] < -37)
				device->activityFlag = 1;
		}
		result = 40;
		goto unlock;
	}
	if (device->idProduct != 0x05c4) {
		result = 0;
		goto unlock;
	}
	if (device->transport == 2) {
		result = ds3UsbReadInput(port, (Ds3UsbInputReport *)report);
		if (report[0] != 1) {
			result = 0;
			goto unlock;
		}
	} else if (device->transport == 3) {
		if (ds3State.safeMode != 0) {
			result = 0;
			for (axis = 0; axis < 4; axis++) {
				centered[axis] = -128;
				analog[axis] = wide[axis] = 0;
			}
			goto ds4Buttons;
		}
		if (device->inputReady == 0) {
			result = 0;
			goto unlock;
		}
		memcpy(report, device->inputRequests[device->inputIndex ^ 1]->buffer,
			sizeof(report));
		if (report[0] != 0x11) {
			result = 0;
			goto unlock;
		}
		switch (report[30] & 31) {
		case 0: device->batteryInfo = 2; break;
		case 1:
		case 2: device->batteryInfo = 3; break;
		case 3:
		case 4:
		case 5:
		case 6: device->batteryInfo = 4; break;
		case 7:
		case 8:
		case 9:
		case 10: device->batteryInfo = 5; break;
		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26: device->batteryInfo = 0xee; break;
		case 27: device->batteryInfo = 0xef; break;
		case 30: device->batteryInfo = 0xf0; break;
		case 31: device->batteryInfo = 0xf1; break;
		default: device->batteryInfo = 0; break;
		}
		if (device->powerStateChecked == 0) {
			device->powerStateChecked = 1;
			if (ScePowerForDriver_394DE492() == 0)
				report[7] |= 1;
		}
		result = 40;
	} else {
		result = 0;
		goto unlock;
	}
	for (axis = 0; axis < 4; axis++) {
		centered[axis] = report[axis + 1] - 128;
		analog[axis] = 111 * centered[axis] / 77 + 128;
		wide[axis] = 44 * centered[axis] / 37 + 128;
		if (analog[axis] < 0)
			analog[axis] = 0;
		else if (analog[axis] > 255)
			analog[axis] = 255;
		if (wide[axis] < 0)
			wide[axis] = 0;
		else if (wide[axis] > 255)
			wide[axis] = 255;
	}
ds4Buttons:
	switch (report[5] & 15) {
	case 0: sample->buttons = SCE_CTRL_UP; break;
	case 1: sample->buttons = SCE_CTRL_UP | SCE_CTRL_RIGHT; break;
	case 2: sample->buttons = SCE_CTRL_RIGHT; break;
	case 3: sample->buttons = SCE_CTRL_RIGHT | SCE_CTRL_DOWN; break;
	case 4: sample->buttons = SCE_CTRL_DOWN; break;
	case 5: sample->buttons = SCE_CTRL_DOWN | SCE_CTRL_LEFT; break;
	case 6: sample->buttons = SCE_CTRL_LEFT; break;
	case 7: sample->buttons = SCE_CTRL_LEFT | SCE_CTRL_UP; break;
	default: sample->buttons = 0; break;
	}
	if (report[5] & 0x10)
		sample->buttons |= SCE_CTRL_SQUARE;
	if (report[5] & 0x20)
		sample->buttons |= SCE_CTRL_CROSS;
	if (report[5] & 0x40)
		sample->buttons |= SCE_CTRL_CIRCLE;
	if (report[5] & 0x80)
		sample->buttons |= SCE_CTRL_TRIANGLE;
	if (report[6] & 1)
		sample->buttons |= 0x400;
	if (report[6] & 2)
		sample->buttons |= 0x800;
	if (report[6] & 4)
		sample->buttons |= 0x100;
	if (report[6] & 8)
		sample->buttons |= 0x200;
	if (report[6] & 0x10)
		sample->buttons |= 1;
	if (report[6] & 0x20)
		sample->buttons |= 8;
	if (report[6] & 0x40)
		sample->buttons |= 2;
	if (report[6] & 0x80)
		sample->buttons |= 4;
	if (report[7] & 2)
		sample->buttons |= 0x04000000;
	if (report[7] & 1)
		sample->buttons |= 0x10000;
	sample->ly = analog[1];
	sample->rx = analog[2];
	sample->ry = analog[3];
	sample->wideLy = wide[1];
	sample->lx = analog[0];
	sample->wideRx = wide[2];
	sample->r2 = report[9];
	sample->wideRy = wide[3];
	sample->wideLx = wide[0];
	output->extended[2] = report[3];
	output->extended[3] = report[4];
	output->extended[0] = report[1];
	output->extended[1] = report[2];
	sample->l2 = report[8];
	sample->up = sample->right = sample->down = sample->left = 0;
	sample->l1 = sample->r1 = 0;
	sample->triangle = sample->circle = sample->cross = sample->square = 0;
	if (device->transport == 3) {
		previousButtons = device->previousButtons;
		device->buttons = sample->buttons;
		device->previousButtons = sample->buttons;
		if (sample->buttons != previousButtons)
			device->activityFlag = 1;
		for (axis = 0; axis < 4; axis++) {
			if (centered[axis] > 13 || centered[axis] < -13)
				device->activityFlag = 1;
		}
		if ((report[35] & 0x80) == 0)
			device->activityFlag = 1;
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3State.lock, interruptState);
	return result;
}

int module_start(SceSize args, void *argp)
{
	SceKernelHeapCreateOpt heapOptions = {
		.size = sizeof(SceKernelHeapCreateOpt),
		.flags = HEAP_HAS_MAX_SIZE,
		.maxSize = 0x10000,
	};
	SceKernelThreadOptParamV1 threadOptions = {
		.size = sizeof(SceKernelThreadOptParamV1),
		.attr = 0,
	};
	int result;

	memset(&ds3State, 0, sizeof(ds3State));
	ds3State.foregroundProcess = -1;
	ds3State.singleControllerProcess = -1;
	ds3State.devices[0].port = -1;
	ds3State.devices[1].port = -1;
	ds3State.devices[2].port = -1;
	ds3State.devices[3].port = -1;
	ds3State.devices[4].port = -1;
	ds3State.devices[5].port = -1;
	ds3State.devices[6].port = -1;
	ds3State.devices[7].port = -1;
	ds3State.devices[0].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.devices[1].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.devices[2].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.devices[3].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.devices[4].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.devices[5].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.devices[6].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.devices[7].usbDeviceId = DS3_USB_DEVICE_UNUSED;
	ds3State.lock = 0;
	sceKernelInitializeFastMutex((SceKernelFastMutex *)&ds3State.stateMutex,
		"SceDs3", 0x2000, NULL);
	sceKernelInitializeFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex,
		"SceDs3Register", 0x2000, NULL);
	result = sceKernelCreateEventFlag("SceDs3Event", 0x1000, 0, NULL);
	ds3State.workEvent = result;
	if (result < 0)
		return result;
	ds3State.heap = sceKernelCreateHeap("SceDs3", 0x1000, &heapOptions);
	result = sceKernelCreateThread("SceDs3Worker", _sceDs3WorkerThread,
		60, 0x2000, 0x10000000, 0x80000,
		(const SceKernelThreadOptParam *)&threadOptions);
	ds3State.workerThread = result;
	if (result < 0)
		return result;
	result = sceKernelStartThread(result, 0, NULL);
	if (result < 0)
		return result;
	SceCtrlForDriver_18823EEA(&ds3ControllerDriver);
	SceTouchForDriver_03C3AC7C(&ds3TouchDriver);
	SceMotionDevForDriver_7FD06731(&ds3MotionDriver);
	sceKernelRegisterSysEventHandler("SceDs3", ds3SysEventHandler, NULL);
	ds3State.processColors[0].pid = -1;
	ds3State.processColors[1].pid = -1;
	ds3State.processColors[2].pid = -1;
	ds3State.processColors[3].pid = -1;
	ds3State.processColors[4].pid = -1;
	ds3State.processColors[5].pid = -1;
	ds3State.processColors[6].pid = -1;
	ds3State.processColors[7].pid = -1;
	sceKernelRegisterProcEventHandler("SceDs", &ds3ProcessHandler, NULL);
	return 0;
}

Ds3Device *ds3FindUsbDevice(int deviceId)
{
	int index;

	if (ds3State.usbDeviceCount == 0)
		return NULL;
	if (deviceId == ds3State.devices[0].usbDeviceId)
		index = 0;
	else if (deviceId == ds3State.devices[1].usbDeviceId)
		index = 1;
	else if (deviceId == ds3State.devices[2].usbDeviceId)
		index = 2;
	else if (deviceId == ds3State.devices[3].usbDeviceId)
		index = 3;
	else if (deviceId == ds3State.devices[4].usbDeviceId)
		index = 4;
	else if (deviceId == ds3State.devices[5].usbDeviceId)
		index = 5;
	else if (deviceId == ds3State.devices[6].usbDeviceId)
		index = 6;
	else if (deviceId == ds3State.devices[7].usbDeviceId)
		index = 7;
	else
		return NULL;
	return &ds3State.devices[index];
}

Ds3Device *ds3FindBtDevice(uint64_t address)
{
	int index;

	if (address == ds3State.devices[0].deviceAddress &&
		ds3State.devices[0].transport == 3)
		index = 0;
	else if (address == ds3State.devices[1].deviceAddress &&
		ds3State.devices[1].transport == 3)
		index = 1;
	else if (address == ds3State.devices[2].deviceAddress &&
		ds3State.devices[2].transport == 3)
		index = 2;
	else if (address == ds3State.devices[3].deviceAddress &&
		ds3State.devices[3].transport == 3)
		index = 3;
	else if (address == ds3State.devices[4].deviceAddress &&
		ds3State.devices[4].transport == 3)
		index = 4;
	else if (address == ds3State.devices[5].deviceAddress &&
		ds3State.devices[5].transport == 3)
		index = 5;
	else if (address == ds3State.devices[6].deviceAddress &&
		ds3State.devices[6].transport == 3)
		index = 6;
	else if (address == ds3State.devices[7].deviceAddress &&
		ds3State.devices[7].transport == 3)
		index = 7;
	else
		return NULL;
	return &ds3State.devices[index];
}

Ds3Device *ds3FindDeviceByPort(int port)
{
	int index;

	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		return NULL;
	return &ds3State.devices[index];
}

int ds3AllocateDevice(int transport, int deviceId, uint64_t address)
{
	Ds3Device *device;
	uint16_t productId;
	int result, index;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	if (result < 0)
		return result;
	if (transport == 3) {
		if (ds3IsSupportedBluetoothDevice(address, &productId) == 0) {
			result = -101;
			goto unlock;
		}
		if (ds3State.singleControllerMode != 0) {
			if (ds3State.activeDeviceCount != 0) {
				result = -1;
				goto unlock;
			}
		} else if (ds3State.activeDeviceCount > 3) {
			result = -100;
			goto unlock;
		}
		if (ds3State.bluetoothDeviceCount > 3) {
			result = -1;
			goto unlock;
		}
	} else if (transport == 1) {
		if (ds3State.usbDeviceCount > 3) {
			result = -1;
			goto unlock;
		}
	} else if (transport == 2) {
		if (ds3State.singleControllerMode != 0) {
			if (ds3State.activeDeviceCount != 0) {
				result = -1;
				goto unlock;
			}
		} else if (ds3State.activeDeviceCount > 3) {
			result = -100;
			goto unlock;
		}
		if (ds3State.pairedUsbDeviceCount > 3) {
			result = -1;
			goto unlock;
		}
		if (deviceId == ds3State.devices[0].usbDeviceId)
			index = 0;
		else if (deviceId == ds3State.devices[1].usbDeviceId)
			index = 1;
		else if (deviceId == ds3State.devices[2].usbDeviceId)
			index = 2;
		else if (deviceId == ds3State.devices[3].usbDeviceId)
			index = 3;
		else if (deviceId == ds3State.devices[4].usbDeviceId)
			index = 4;
		else if (deviceId == ds3State.devices[5].usbDeviceId)
			index = 5;
		else if (deviceId == ds3State.devices[6].usbDeviceId)
			index = 6;
		else if (deviceId == ds3State.devices[7].usbDeviceId)
			index = 7;
		else {
			result = -1;
			goto unlock;
		}
		ds3State.pairedUsbDeviceCount++;
		device = &ds3State.devices[index];
		ds3State.activeDeviceCount++;
		device->deviceAddress = address;
		device->transport = 2;
		result = 0;
		goto unlock;
	} else {
		result = 0;
		goto unlock;
	}
	if (ds3State.devices[0].deviceAddress == 0 &&
		ds3State.devices[0].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 0;
	else if (ds3State.devices[1].deviceAddress == 0 &&
		ds3State.devices[1].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 1;
	else if (ds3State.devices[2].deviceAddress == 0 &&
		ds3State.devices[2].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 2;
	else if (ds3State.devices[3].deviceAddress == 0 &&
		ds3State.devices[3].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 3;
	else if (ds3State.devices[4].deviceAddress == 0 &&
		ds3State.devices[4].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 4;
	else if (ds3State.devices[5].deviceAddress == 0 &&
		ds3State.devices[5].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 5;
	else if (ds3State.devices[6].deviceAddress == 0 &&
		ds3State.devices[6].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 6;
	else if (ds3State.devices[7].deviceAddress == 0 &&
		ds3State.devices[7].usbDeviceId == DS3_USB_DEVICE_UNUSED)
		index = 7;
	else {
		result = transport == 3 ? -100 : -1;
		goto unlock;
	}
	device = &ds3State.devices[index];
	memset(device, 0, sizeof(*device));
	if (transport == 1) {
		ds3AllocateRequests(device);
		device->usbDeviceId = deviceId;
		device->port = index;
		device->transport = 1;
		ds3State.usbDeviceCount++;
	} else {
		ds3AllocateBluetoothRequests(device);
		device->deviceAddress = address;
		device->port = index;
		device->transport = 3;
		ds3State.bluetoothDeviceCount++;
		device->idProduct = productId;
		ds3State.activeDeviceCount++;
	}
	result = 0;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	return result;
}

int ds3ReleaseDevice(int transport, int deviceId, uint64_t address)
{
	Ds3Device *device;
	SceBtHidRequest *request;
	int result, index, requestIndex;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	if (result < 0)
		return result;
	if (transport == 3) {
		if (address == ds3State.devices[0].deviceAddress &&
			(unsigned int)ds3State.devices[0].transport - 3 <= 1)
			index = 0;
		else if (address == ds3State.devices[1].deviceAddress &&
			(unsigned int)ds3State.devices[1].transport - 3 <= 1)
			index = 1;
		else if (address == ds3State.devices[2].deviceAddress &&
			(unsigned int)ds3State.devices[2].transport - 3 <= 1)
			index = 2;
		else if (address == ds3State.devices[3].deviceAddress &&
			(unsigned int)ds3State.devices[3].transport - 3 <= 1)
			index = 3;
		else if (address == ds3State.devices[4].deviceAddress &&
			(unsigned int)ds3State.devices[4].transport - 3 <= 1)
			index = 4;
		else if (address == ds3State.devices[5].deviceAddress &&
			(unsigned int)ds3State.devices[5].transport - 3 <= 1)
			index = 5;
		else if (address == ds3State.devices[6].deviceAddress &&
			(unsigned int)ds3State.devices[6].transport - 3 <= 1)
			index = 6;
		else if (address == ds3State.devices[7].deviceAddress &&
			(unsigned int)ds3State.devices[7].transport - 3 <= 1)
			index = 7;
		else
			goto unlock;
		device = &ds3State.devices[index];
		for (requestIndex = 0; requestIndex < 2; requestIndex++) {
			request = device->inputRequests[requestIndex];
			if (request->buffer != NULL) {
				sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
				request = device->inputRequests[requestIndex];
			}
			request->buffer = NULL;
			sceKernelFreeHeapMemory(ds3State.heap, request);
			device->inputRequests[requestIndex] = NULL;
		}
		for (requestIndex = 0; requestIndex < 2; requestIndex++) {
			request = device->outputRequests[requestIndex];
			if (request->buffer != NULL) {
				sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
				request = device->outputRequests[requestIndex];
			}
			request->buffer = NULL;
			sceKernelFreeHeapMemory(ds3State.heap, request);
			device->outputRequests[requestIndex] = NULL;
		}
		if (ds3State.bluetoothDeviceCount != 0)
			ds3State.bluetoothDeviceCount--;
		if (ds3State.activeDeviceCount != 0 && device->transport != 4)
			ds3State.activeDeviceCount--;
		memset(device, 0, sizeof(*device));
		device->usbDeviceId = DS3_USB_DEVICE_UNUSED;
		device->port = -1;
		goto unlock;
	}
	if (transport != 2)
		goto unlock;
	if (deviceId == ds3State.devices[0].usbDeviceId)
		index = 0;
	else if (deviceId == ds3State.devices[1].usbDeviceId)
		index = 1;
	else if (deviceId == ds3State.devices[2].usbDeviceId)
		index = 2;
	else if (deviceId == ds3State.devices[3].usbDeviceId)
		index = 3;
	else if (deviceId == ds3State.devices[4].usbDeviceId)
		index = 4;
	else if (deviceId == ds3State.devices[5].usbDeviceId)
		index = 5;
	else if (deviceId == ds3State.devices[6].usbDeviceId)
		index = 6;
	else if (deviceId == ds3State.devices[7].usbDeviceId)
		index = 7;
	else
		goto unlock;
	device = &ds3State.devices[index];
	device->usbDeviceId = DS3_USB_DEVICE_UNUSED;
	if (ds3State.ds4UsbEnabled != 0 && device->idProduct == 0x05c4)
		ds3RemoveTrackedAddress(device->deviceAddress);
	for (requestIndex = 0; requestIndex < 2; requestIndex++) {
		request = device->inputRequests[requestIndex];
		if (request->buffer != NULL) {
			sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
			request = device->inputRequests[requestIndex];
		}
		request->buffer = NULL;
		sceKernelFreeHeapMemory(ds3State.heap, request);
		device->inputRequests[requestIndex] = NULL;
	}
	request = device->outputRequests[0];
	if (request->buffer != NULL) {
		sceKernelFreeHeapMemory(ds3State.heap, request->buffer);
		request = device->outputRequests[0];
	}
	request->buffer = NULL;
	sceKernelFreeHeapMemory(ds3State.heap, request);
	device->outputRequests[0] = NULL;
	if (ds3State.usbDeviceCount != 0)
		ds3State.usbDeviceCount--;
	if (ds3State.pairedUsbDeviceCount != 0)
		ds3State.pairedUsbDeviceCount--;
	if (ds3State.activeDeviceCount != 0 && device->transport != 1)
		ds3State.activeDeviceCount--;
	memset(device, 0, sizeof(*device));
	device->usbDeviceId = DS3_USB_DEVICE_UNUSED;
	device->port = -1;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	return 0;
}

int ds3DetachBluetoothTransport(uint64_t address)
{
	int result, index;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	if (result < 0)
		return result;
	if (address == ds3State.devices[0].deviceAddress &&
		ds3State.devices[0].transport == 3)
		index = 0;
	else if (address == ds3State.devices[1].deviceAddress &&
		ds3State.devices[1].transport == 3)
		index = 1;
	else if (address == ds3State.devices[2].deviceAddress &&
		ds3State.devices[2].transport == 3)
		index = 2;
	else if (address == ds3State.devices[3].deviceAddress &&
		ds3State.devices[3].transport == 3)
		index = 3;
	else if (address == ds3State.devices[4].deviceAddress &&
		ds3State.devices[4].transport == 3)
		index = 4;
	else if (address == ds3State.devices[5].deviceAddress &&
		ds3State.devices[5].transport == 3)
		index = 5;
	else if (address == ds3State.devices[6].deviceAddress &&
		ds3State.devices[6].transport == 3)
		index = 6;
	else if (address == ds3State.devices[7].deviceAddress &&
		ds3State.devices[7].transport == 3)
		index = 7;
	else
		goto unlock;
	ds3State.devices[index].transport = 4;
	if (ds3State.activeDeviceCount != 0)
		ds3State.activeDeviceCount--;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	return 0;
}

int ds3UnpairUsbDevice(int deviceId)
{
	int result, index;

	result = lockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	if (result < 0)
		return result;
	if (deviceId == ds3State.devices[0].usbDeviceId)
		index = 0;
	else if (deviceId == ds3State.devices[1].usbDeviceId)
		index = 1;
	else if (deviceId == ds3State.devices[2].usbDeviceId)
		index = 2;
	else if (deviceId == ds3State.devices[3].usbDeviceId)
		index = 3;
	else if (deviceId == ds3State.devices[4].usbDeviceId)
		index = 4;
	else if (deviceId == ds3State.devices[5].usbDeviceId)
		index = 5;
	else if (deviceId == ds3State.devices[6].usbDeviceId)
		index = 6;
	else if (deviceId == ds3State.devices[7].usbDeviceId)
		index = 7;
	else
		goto unlock;
	ds3State.devices[index].transport = 1;
	if (ds3State.pairedUsbDeviceCount != 0)
		ds3State.pairedUsbDeviceCount--;
	if (ds3State.activeDeviceCount != 0)
		ds3State.activeDeviceCount--;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.registrationMutex);
	return 0;
}

int ds3BuildLedState(int mask, int mode, Ds3LedState *leds)
{
	uint16_t duration = 0;
	uint8_t period = 0, offTime = 0, onTime = 0;

	if (leds == NULL)
		return mask;
	leds->mask = mask;
	if (mask & 2) {
		switch (mode) {
		case 0: duration = 10000; period = 255; offTime = 0; onTime = 0; break;
		case 1: duration = 10000; period = 255; offTime = 0; onTime = 255; break;
		case 2: duration = 10000; period = 255; offTime = 50; onTime = 50; break;
		case 3: duration = 1000; period = 255; offTime = 125; onTime = 125; break;
		default: duration = 0; period = 0; offTime = 0; onTime = 0; break;
		}
		leds->patterns[0].period = period;
		leds->patterns[0].offTime = offTime;
		leds->patterns[0].onTime = onTime;
		leds->patterns[0].duration = duration;
	}
	if (mask & 4) {
		switch (mode) {
		case 0: duration = 10000; period = 255; offTime = 0; onTime = 0; break;
		case 1: duration = 10000; period = 255; offTime = 0; onTime = 255; break;
		case 2: duration = 10000; period = 255; offTime = 50; onTime = 50; break;
		case 3: duration = 1000; period = 255; offTime = 125; onTime = 125; break;
		}
		leds->patterns[1].period = period;
		leds->patterns[1].offTime = offTime;
		leds->patterns[1].onTime = onTime;
		leds->patterns[1].duration = duration;
	}
	if (mask & 8) {
		switch (mode) {
		case 0: duration = 10000; period = 255; offTime = 0; onTime = 0; break;
		case 1: duration = 10000; period = 255; offTime = 0; onTime = 255; break;
		case 2: duration = 10000; period = 255; offTime = 50; onTime = 50; break;
		case 3: duration = 1000; period = 255; offTime = 125; onTime = 125; break;
		}
		leds->patterns[2].period = period;
		leds->patterns[2].offTime = offTime;
		leds->patterns[2].onTime = onTime;
		leds->patterns[2].duration = duration;
	}
	if (mask & 16) {
		switch (mode) {
		case 0: duration = 10000; period = 255; offTime = 0; onTime = 0; break;
		case 1: duration = 10000; period = 255; offTime = 0; onTime = 255; break;
		case 2: duration = 10000; period = 255; offTime = 50; onTime = 50; break;
		case 3: duration = 1000; period = 255; offTime = 125; onTime = 125; break;
		}
		leds->patterns[3].period = period;
		leds->patterns[3].offTime = offTime;
		leds->patterns[3].onTime = onTime;
		leds->patterns[3].duration = duration;
	}
	return (int)((uint32_t)mask << 27);
}

Ds3Device *ds3UpdateUsbOutputState(Ds3Device *controller, const uint8_t *report)
{
	uint8_t offTime, auxiliaryMotorReport;

	controller->ledState.mask = report[9];
	controller->ledState.patterns[0].period = report[25];
	controller->ledState.patterns[0].duration = report[27] | report[26] << 8;
	controller->ledState.patterns[0].offTime = report[28];
	controller->ledState.patterns[0].onTime = report[29];
	controller->ledState.patterns[1].period = report[20];
	controller->ledState.patterns[1].duration = report[22] | report[21] << 8;
	controller->ledState.patterns[1].offTime = report[23];
	controller->ledState.patterns[1].onTime = report[24];
	controller->ledState.patterns[2].period = report[15];
	controller->ledState.patterns[2].duration = report[17] | report[16] << 8;
	controller->ledState.patterns[2].offTime = report[18];
	controller->ledState.patterns[2].onTime = report[19];
	controller->ledState.patterns[3].period = report[10];
	controller->ledState.patterns[3].duration = report[12] | report[11] << 8;
	offTime = report[13];
	auxiliaryMotorReport = controller->auxiliaryMotorReport;
	controller->ledState.patterns[3].offTime = offTime;
	controller->ledState.patterns[3].onTime = report[14];
	controller->actuatorState.smallMotor = report[2];
	if (auxiliaryMotorReport == 6) {
		controller->actuatorState.auxiliaryMotor = report[4];
	} else {
		controller->actuatorState.largeMotor = report[4];
		if (auxiliaryMotorReport == 7)
			controller->actuatorState.auxiliaryMotor = report[6];
	}
	return controller;
}

Ds3Device *ds3BuildUsbOutputReport(Ds3Device *controller, uint8_t *report)
{
	uint8_t onTime, motor;

	report[9] = controller->ledState.mask;
	report[25] = controller->ledState.patterns[0].period;
	report[26] = controller->ledState.patterns[0].duration >> 8;
	report[27] = controller->ledState.patterns[0].duration;
	report[28] = controller->ledState.patterns[0].offTime;
	report[29] = controller->ledState.patterns[0].onTime;
	report[20] = controller->ledState.patterns[1].period;
	report[21] = controller->ledState.patterns[1].duration >> 8;
	report[22] = controller->ledState.patterns[1].duration;
	report[23] = controller->ledState.patterns[1].offTime;
	report[24] = controller->ledState.patterns[1].onTime;
	report[15] = controller->ledState.patterns[2].period;
	report[16] = controller->ledState.patterns[2].duration >> 8;
	report[17] = controller->ledState.patterns[2].duration;
	report[18] = controller->ledState.patterns[2].offTime;
	report[19] = controller->ledState.patterns[2].onTime;
	report[10] = controller->ledState.patterns[3].period;
	report[11] = controller->ledState.patterns[3].duration >> 8;
	report[12] = controller->ledState.patterns[3].duration;
	report[13] = controller->ledState.patterns[3].offTime;
	onTime = controller->ledState.patterns[3].onTime;
	report[1] = 0x96;
	report[14] = onTime;
	report[2] = controller->actuatorState.smallMotor;
	if (controller->auxiliaryMotorReport == 6) {
		report[3] = 255;
		motor = controller->actuatorState.auxiliaryMotor;
	} else {
		report[3] = 0x96;
		motor = controller->actuatorState.largeMotor;
	}
	report[4] = motor;
	if (controller->auxiliaryMotorReport == 7) {
		report[5] = 255;
		report[6] = controller->actuatorState.auxiliaryMotor;
	}
	return controller;
}

Ds3Device *ds3UpdateBatteryInfo(Ds3Device *controller, uint8_t battery)
{
	switch (battery & 31) {
	case 0:
		controller->batteryInfo = 2;
		return controller;
	case 1:
	case 2:
		controller->batteryInfo = 3;
		return controller;
	case 3:
	case 4:
	case 5:
	case 6:
		controller->batteryInfo = 4;
		return controller;
	case 7:
	case 8:
	case 9:
	case 10:
		controller->batteryInfo = 5;
		return controller;
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 26:
		controller->batteryInfo = 0xee;
		return controller;
	case 27:
		controller->batteryInfo = 0xef;
		return controller;
	case 30:
		controller->batteryInfo = 0xf0;
		return controller;
	case 31:
		controller->batteryInfo = 0xf1;
		return controller;
	default:
		controller->batteryInfo = 0;
		return controller;
	}
}

int ds3QueueLedState(int port, const Ds3LedState *state)
{
	Ds3Device *device;
	int index, result;

	if (state == NULL)
		return (int)0x80340001;
	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
		return (int)0x80340020;
	}
	device = &ds3State.devices[index];
	device->ledState = *state;
	device->pendingWork |= 1;
	sceKernelSetEventFlag(ds3State.workEvent, 1);
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3QueueLightbarState(int port, const Ds3LedState *state)
{
	Ds3Device *device;
	Ds3LightbarOverride *override;
	uint8_t red, green, blue;
	int index, processIndex, result;

	if (state == NULL)
		return (int)0x80340001;
	result = lockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	if (result < 0)
		return result;
	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
		return (int)0x80340020;
	}
	device = &ds3State.devices[index];
	if (device->idProduct != 0x05C4)
		goto unlock;
	if (ds3State.foregroundProcess == ds3State.processColors[0].pid)
		processIndex = 0;
	else if (ds3State.foregroundProcess == ds3State.processColors[1].pid)
		processIndex = 1;
	else if (ds3State.foregroundProcess == ds3State.processColors[2].pid)
		processIndex = 2;
	else if (ds3State.foregroundProcess == ds3State.processColors[3].pid)
		processIndex = 3;
	else if (ds3State.foregroundProcess == ds3State.processColors[4].pid)
		processIndex = 4;
	else if (ds3State.foregroundProcess == ds3State.processColors[5].pid)
		processIndex = 5;
	else if (ds3State.foregroundProcess == ds3State.processColors[6].pid)
		processIndex = 6;
	else if (ds3State.foregroundProcess == ds3State.processColors[7].pid)
		processIndex = 7;
	else {
		if (ds3State.processColors[0].pid == -1)
			processIndex = 0;
		else if (ds3State.processColors[1].pid == -1)
			processIndex = 1;
		else if (ds3State.processColors[2].pid == -1)
			processIndex = 2;
		else if (ds3State.processColors[3].pid == -1)
			processIndex = 3;
		else if (ds3State.processColors[4].pid == -1)
			processIndex = 4;
		else if (ds3State.processColors[5].pid == -1)
			processIndex = 5;
		else if (ds3State.processColors[6].pid == -1)
			processIndex = 6;
		else if (ds3State.processColors[7].pid == -1)
			processIndex = 7;
		else
			goto useState;
		ds3State.processColors[processIndex].pid = ds3State.foregroundProcess;
	}
	override = &ds3State.processColors[processIndex].ports[port - 1];
	if (override->enabled) {
		device->ledState.lightbarColor[0] = override->color[0];
		device->ledState.lightbarColor[1] = override->color[1];
		device->ledState.lightbarColor[2] = override->color[2];
		goto queue;
	}
useState:
	red = state->lightbarColor[0];
	green = state->lightbarColor[1];
	blue = state->lightbarColor[2];
	device->ledState.lightbarColor[0] = red;
	device->ledState.lightbarColor[1] = green;
	device->ledState.lightbarColor[2] = blue;
queue:
	device->pendingWork |= 1;
	sceKernelSetEventFlag(ds3State.workEvent, 1);
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3State.stateMutex);
	return 0;
}

int ds3GetLocalAddress(uint8_t *address)
{
	if (address == NULL)
		return -1;
	__builtin_memcpy(address, ds3State.localAddress, sizeof(ds3State.localAddress));
	return 0;
}

int ds3IsUsbCharging(void)
{
	return (ds3State.devices[0].usbState > 1 &&
			ds3State.devices[0].batteryInfo == 0xee)
		|| (ds3State.devices[1].usbState > 1 &&
			ds3State.devices[1].batteryInfo == 0xee)
		|| (ds3State.devices[2].usbState > 1 &&
			ds3State.devices[2].batteryInfo == 0xee)
		|| (ds3State.devices[3].usbState > 1 &&
			ds3State.devices[3].batteryInfo == 0xee)
		|| (ds3State.devices[4].usbState > 1 &&
			ds3State.devices[4].batteryInfo == 0xee)
		|| (ds3State.devices[5].usbState > 1 &&
			ds3State.devices[5].batteryInfo == 0xee)
		|| (ds3State.devices[6].usbState > 1 &&
			ds3State.devices[6].batteryInfo == 0xee)
		|| (ds3State.devices[7].usbState > 1 &&
			ds3State.devices[7].batteryInfo == 0xee);
}

int ds3IsDs4Port(int port)
{
	int index;

	if (port == ds3State.devices[0].port &&
		(ds3State.devices[0].bluetoothState == 2 || ds3State.devices[0].usbState == 2))
		index = 0;
	else if (port == ds3State.devices[1].port &&
		(ds3State.devices[1].bluetoothState == 2 || ds3State.devices[1].usbState == 2))
		index = 1;
	else if (port == ds3State.devices[2].port &&
		(ds3State.devices[2].bluetoothState == 2 || ds3State.devices[2].usbState == 2))
		index = 2;
	else if (port == ds3State.devices[3].port &&
		(ds3State.devices[3].bluetoothState == 2 || ds3State.devices[3].usbState == 2))
		index = 3;
	else if (port == ds3State.devices[4].port &&
		(ds3State.devices[4].bluetoothState == 2 || ds3State.devices[4].usbState == 2))
		index = 4;
	else if (port == ds3State.devices[5].port &&
		(ds3State.devices[5].bluetoothState == 2 || ds3State.devices[5].usbState == 2))
		index = 5;
	else if (port == ds3State.devices[6].port &&
		(ds3State.devices[6].bluetoothState == 2 || ds3State.devices[6].usbState == 2))
		index = 6;
	else if (port == ds3State.devices[7].port &&
		(ds3State.devices[7].bluetoothState == 2 || ds3State.devices[7].usbState == 2))
		index = 7;
	else
		return 0;
	return ds3State.devices[index].idProduct == 0x05C4;
}

int ds3IsDs4UsbEnabled(void)
{
	return ds3State.ds4UsbEnabled;
}

int ds3IsSuspending(void)
{
	return ds3State.suspending;
}

int ds3IsSafeMode(void)
{
	return ds3State.safeMode;
}

