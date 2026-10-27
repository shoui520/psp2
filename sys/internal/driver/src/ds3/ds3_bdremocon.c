/* SceDs3 BD remote; the original filename is not recorded. */
#include "ds3_private.h"
#include <ctrl/ctrl.h>
#include <regmgr/regmgr.h>
#include <bt/bt_registered.h>
#include <bt/bt_event.h>
#include <bt/bt_test.h>
#include <sysmem/debug.h>
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
#include <usbserv/usbserv.h>
#include <display/display.h>
#include <string.h>


static Ds3BdRemocon ds3BdRemocon;

uint32_t ds3BdRemoconReleaseTimer(void *argp)
{
	ds3BdRemocon.releaseTimer = 0;
	_sceCtrlBdRemoconSendMergedNotify2(0, 255, 0);
	return 0;
}

int ds3BdRemoconThread(SceSize args, void *argp)
{
	SceUID callback, event;
	SceUInt32 outBits;
	int result;

	callback = sceKernelCreateCallback("SceBdRemocon", 0,
					    ds3BdRemoconCallback, NULL);
	ds3BdRemocon.callback = callback;
	if (callback < 0)
		return callback;
	result = SceBtForDriver_120AC1F7(callback, 0x460);
	if (result < 0)
		return result;
	sceKernelRegisterSysEventHandler("SceBdRemocon", ds3BdRemoconSysEvent, NULL);
	event = sceKernelCreateEventFlag("SceBdRemocon", 0x1000, 0, NULL);
	if (event < 0)
		return event;
	for (;;)
		sceKernelWaitEventFlagCB(event, 1, 5, &outBits, NULL);
}

int ds3BdRemoconSysEvent(int resume, int eventId, void *args, void *opt)
{
	if (resume == 0 && eventId == 0x20f && ds3BdRemocon.deviceAddress != 0) {
		_sceCtrlBdRemoconSendMergedNotify2(0, 255, 0);
		ds3BdRemocon.inputReady = 0;
		ds3BdRemocon.deviceAddress = 0;
		SceCtrlForDriver_AA36F578(0);
	}
	return 0;
}

int ds3BdRemoconCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	SceBtEvent events[2], *event;
	SceBtRegisteredInfo info;
	SceBtHidRequest *request;
	const uint8_t *report;
	uint64_t address;
	uint32_t buttons, state, connected;
	int count, index, result, timer;

	do {
		do {
			count = SceBtForDriver_5ABB9A9D(events, 2);
		} while (count == (int)0x802f0c04);
		if (count <= 0)
			break;
		for (event = events; event < events + count; event++) {
			address = ((uint64_t)event->mac1 << 32) | event->mac0;
			__builtin_prefetch(event + 1);
			if (event->id == 6) {
				if (ds3BdRemocon.deviceAddress != address ||
				    ds3BdRemocon.deviceAddress == 0)
					continue;
				_sceCtrlBdRemoconSendMergedNotify2(0, 255, 0);
				ds3BdRemocon.inputReady = 0;
				ds3BdRemocon.deviceAddress = 0;
				SceCtrlForDriver_AA36F578(0);
			} else if (event->id == 5) {
				memset(&info, 0, sizeof(info));
				result = SceBtForDriver_F86D25E2(address, &info, 1);
				if (result >= 0) {
					if (info.vid != 0x054c || info.pid != 0x0306)
						continue;
					ds3BdRemocon.deviceAddress = address;
					SceCtrlForDriver_AA36F578(1);
				}
				sceKernelPowerTick(0);
				ds3BdRemocon.state = 0;
				ds3BdRemocon.connected = 0;
				ds3BdRemocon.buttons = 0;
				index = ds3BdRemocon.inputIndex;
				request = &ds3BdRemocon.inputRequests[index];
				memset(request, 0, sizeof(*request));
				request->buffer = ds3BdRemocon.inputReports[index];
				request->type = 0;
				request->reportId = 0;
				request->length = 64;
				request->argument = 0;
				SceBtForDriver_F9DCEC77((uint32_t)address, address >> 32, request);
				index = ds3BdRemocon.inputIndex ^ 1;
				ds3BdRemocon.inputIndex = index;
				request = &ds3BdRemocon.inputRequests[index];
				memset(request, 0, sizeof(*request));
				request->buffer = ds3BdRemocon.inputReports[index];
				request->type = 0;
				request->reportId = 0;
				request->length = 64;
				request->argument = 0;
				SceBtForDriver_F9DCEC77((uint32_t)address, address >> 32, request);
				ds3BdRemocon.inputIndex ^= 1;
			} else if (event->id == 10) {
				if (ds3BdRemocon.deviceAddress != address)
					continue;
				report = ds3BdRemocon.inputReports[ds3BdRemocon.inputIndex];
				buttons = report[1] | (report[2] << 8) |
					((report[3] & 1) << 16);
				if (ds3BdRemocon.buttonAssign == 0)
					buttons |= (report[3] & 8) << 10;
				else if (ds3BdRemocon.buttonAssign == 1)
					buttons |= (report[3] & 8) << 11;
				state = report[4];
				connected = report[10];
				if (ds3BdRemocon.inputReady != 0) {
					if (ds3BdRemocon.releaseTimer != 0) {
						sceKernelCancelAlarm(ds3BdRemocon.releaseTimer);
						ds3BdRemocon.releaseTimer = 0;
					}
					if (state == 255 && connected == 1) {
						state = 255;
						connected = 0;
					}
				} else {
					ds3BdRemocon.inputReady = 1;
					if (ScePowerForDriver_394DE492() == 0) {
						timer = SceThreadmgrForDriver_C58DF384("SceBDRmcPS", 40000,
									       ds3BdRemoconReleaseTimer, NULL);
						if (timer >= 0)
							ds3BdRemocon.releaseTimer = timer;
						buttons = 0x10000;
						state = 255;
						connected = 0;
					} else if (state == 255 && connected == 1) {
						state = 255;
						connected = 0;
					}
				}
				if (ds3BdRemocon.buttons != buttons ||
				    ds3BdRemocon.state != state ||
				    ds3BdRemocon.connected != connected) {
					if (sceDisplayGetVcountInternal(1) == ds3BdRemocon.lastVcount) {
						sceDisplayWaitVblankStart();
						sceDisplayWaitVblankStart();
					}
					_sceCtrlBdRemoconSendMergedNotify2(buttons, state, connected);
					ds3BdRemocon.lastVcount = sceDisplayGetVcountInternal(1);
				}
				ds3BdRemocon.state = state;
				ds3BdRemocon.connected = connected;
				ds3BdRemocon.buttons = buttons;
				index = ds3BdRemocon.inputIndex;
				request = &ds3BdRemocon.inputRequests[index];
				memset(request, 0, sizeof(*request));
				request->type = 0;
				request->reportId = 0;
				request->length = 64;
				request->argument = 0;
				request->buffer = ds3BdRemocon.inputReports[index];
				SceBtForDriver_F9DCEC77((uint32_t)address, address >> 32, request);
				ds3BdRemocon.inputIndex ^= 1;
			}
		}
	} while (count == 2);
	return 0;
}

int ds3InitializeBdRemocon(void)
{
	SceKernelThreadOptParamV1 option;
	int buttonAssign = 0;
	int result;

	memset(&ds3BdRemocon, 0, sizeof(ds3BdRemocon));
	option.size = sizeof(option);
	option.attr = 0;
	if (sceRegMgrSystemParamGetInt(0x00229142, &buttonAssign) >= 0) {
		if (buttonAssign == 0)
			ds3BdRemocon.buttonAssign = 0;
		else if (buttonAssign == 1)
			ds3BdRemocon.buttonAssign = 1;
	}
	result = sceKernelCreateThread("SceBdRemocon", ds3BdRemoconThread,
		60, 0x1000, 0x10000000, 0x80000,
		(const SceKernelThreadOptParam *)&option);
	ds3BdRemocon.workerThread = result;
	if (result < 0)
		return result;
	result = sceKernelStartThread(result, 0, NULL);
	if (result < 0)
		return result;
	return 0;
}

int ds3DisconnectTrackedDevice(void)
{
	uint64_t address = ds3BdRemocon.deviceAddress;

	if (address != 0)
		SceBtForDriver_50710281((uint32_t)address, address >> 32);
	return 0;
}
