/* SceDs3 port manager; the original filename is not recorded. */
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


static const uint8_t ds3PortLightbarColors[5][3] = {
	{12, 23, 29},
	{0, 0, 13},
	{13, 0, 0},
	{0, 13, 0},
	{13, 0, 13},
};

static Ds3PortManager ds3PortManager;

int ds3PortManagerThread(SceSize args, void *argp)
{
	Ds3LedState leds;
	uint8_t previousBattery[4] = {0};
	uint8_t battery;
	SceUInt timeout;
	int result, refresh, port, mode;

	for (;;) {
		timeout = 3000000;
		result = sceKernelWaitEventFlag(ds3PortManager.workEvent, 1, 3,
			NULL, &timeout);
		refresh = result != (int)0x80028005;
		if (refresh)
			__builtin_memset(previousBattery, 0, sizeof(previousBattery));
		for (port = 1; port != 5; port++) {
			if (ds3IsDs4Port((uint8_t)port)) {
				if (refresh) {
					leds.lightbarColor[0] = ds3PortLightbarColors[port][0];
					leds.lightbarColor[1] = ds3PortLightbarColors[port][1];
					leds.lightbarColor[2] = ds3PortLightbarColors[port][2];
					ds3QueueLightbarState(port, &leds);
				}
				continue;
			}
			battery = 0;
			if (ds3GetBatteryInfo(port, &battery) != 0)
				continue;
			if (previousBattery[port - 1] != battery) {
				__builtin_memset(&leds, 0, sizeof(leds));
				if (battery <= 5) {
					mode = battery != 0 && battery < 3 ? 3 : 1;
				} else if (battery == 0xee) {
					mode = 2;
				} else if (battery == 0xef) {
					mode = 1;
				} else {
					previousBattery[port - 1] = battery;
					continue;
				}
				ds3BuildLedState((uint8_t)(2 << (port - 1)), mode, &leds);
				ds3QueueLedState(port, &leds);
			}
			previousBattery[port - 1] = battery;
		}
		ds3UsbPollBattery();
		if (ds3IsUsbCharging())
			SceKernelSuspendForDriver_E0489831(1);
	}
}

int ds3InitializePortManager(void)
{
	SceKernelThreadOptParamV1 option = {sizeof(option), 0};
	int result;

	memset(&ds3PortManager, 0, sizeof(ds3PortManager));
	sceKernelInitializeFastMutex((SceKernelFastMutex *)&ds3PortManager.mutex,
		"SceDs3Manager", 0x2000, NULL);
	result = sceKernelCreateEventFlag("SceDs3Manager", 0x1000, 0, NULL);
	ds3PortManager.workEvent = result;
	if (result < 0)
		return result;
	result = sceKernelCreateThread("SceDs3Manager", ds3PortManagerThread,
		0x60, 0x2000, 0x10000000, 0x80000,
		(const SceKernelThreadOptParam *)&option);
	ds3PortManager.workerThread = result;
	if (result < 0)
		return result;
	return sceKernelStartThread(result, 0, NULL);
}

int ds3SignalPortChange(void)
{
	lockFastMutex((SceKernelFastMutex *)&ds3PortManager.mutex);
	sceKernelSetEventFlag(ds3PortManager.workEvent, 1);
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&ds3PortManager.mutex);
	return 0;
}

int ds3AssignPort(uint64_t address, int8_t *port, uint8_t deviceType)
{
	int index;

	if (port == NULL)
		return -1;
	if (ds3PortManager.ports[0].deviceAddress == address)
		index = 0;
	else if (ds3PortManager.ports[1].deviceAddress == address)
		index = 1;
	else if (ds3PortManager.ports[2].deviceAddress == address)
		index = 2;
	else if (ds3PortManager.ports[3].deviceAddress == address)
		index = 3;
	else {
		if (ds3PortManager.ports[0].deviceAddress == 0)
			index = 0;
		else if (ds3PortManager.ports[1].deviceAddress == 0)
			index = 1;
		else if (ds3PortManager.ports[2].deviceAddress == 0)
			index = 2;
		else if (ds3PortManager.ports[3].deviceAddress == 0)
			index = 3;
		else
			return -1;
		ds3PortManager.ports[index].deviceAddress = address;
		ds3PortManager.ports[index].deviceType = deviceType;
	}
	*port = index + 1;
	return 0;
}

int ds3SwapTrackedPort(uint64_t address, uint8_t port)
{
	uint64_t previousAddress;
	unsigned int target = (uint8_t)(port - 1);
	int index;

	if (target > 3)
		return -1;
	if (ds3PortManager.ports[0].deviceAddress == address)
		index = 0;
	else if (ds3PortManager.ports[1].deviceAddress == address)
		index = 1;
	else if (ds3PortManager.ports[2].deviceAddress == address)
		index = 2;
	else if (ds3PortManager.ports[3].deviceAddress == address)
		index = 3;
	else
		return -1;
	previousAddress = ds3PortManager.ports[target].deviceAddress;
	ds3PortManager.ports[target].deviceAddress = address;
	ds3PortManager.ports[index].deviceAddress = previousAddress;
	return 0;
}

int ds3RemoveTrackedAddress(uint64_t address)
{
	if (ds3PortManager.ports[0].deviceAddress == address)
		ds3PortManager.ports[0].deviceAddress = 0;
	if (ds3PortManager.ports[1].deviceAddress == address)
		ds3PortManager.ports[1].deviceAddress = 0;
	if (ds3PortManager.ports[2].deviceAddress == address)
		ds3PortManager.ports[2].deviceAddress = 0;
	if (ds3PortManager.ports[3].deviceAddress == address)
		ds3PortManager.ports[3].deviceAddress = 0;
	return 0;
}
